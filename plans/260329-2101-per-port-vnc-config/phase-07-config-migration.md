# Phase 7: Migration — Old Config to Per-Port Format

**Priority:** MEDIUM — Backward compatibility
**Status:** Pending
**Depends on:** Phase 3 (new per-port storage format defined)
**Estimated effort:** Small

## Context Links

- [Plan overview](plan.md)
- [Phase 3: Configurator per-port storage](phase-03-configurator-per-port-storage.md)
- `server-config-lib/Configurator.cpp` lines 140-220 — `load(SettingsManager*)` ordering
- `server-config-lib/Configurator.cpp` lines 223-295 — `savePortMappingContainer`/`loadPortMappingContainer` (old format)
- `server-config-lib/Configurator.cpp` lines 892-979 — `saveWinAuthConfig`/`loadWinAuthConfig` (global auth)
- `server-config-lib/Configurator.cpp` lines 1008-1044 — `loadPortConfig` (new format detection)

## Overview

Handle seamless migration from old config format (global auth + ExtraPorts) to new per-port config format. On first load of an old config:
1. Detect absence of `PortCount` key (= old format)
2. Build `PortConfig` entries from global settings + old ExtraPorts
3. Save in new format on next `save()`

Also handle:
- Mixed format (partial migration)
- Registry → INI migration path
- Clean upgrade path (no data loss)

## Key Insights

- Current `loadPortConfig()` already returns `true` silently when `PortCount` key is missing (line 1013-1016) — new format not found, no error
- Global auth settings loaded by `loadWinAuthConfig()` into `ServerConfig` global fields
- Old `ExtraPorts` loaded by `loadPortMappingContainer()` into `m_portMappings`
- Old `RfbPort` loaded by `loadServerConfig()` into `m_rfbPort`
- Migration logic should run AFTER all old-format loading completes
- Migration creates PortConfigs from: main port (global auth) + extra ports (global auth)

## Migration Logic

### Detection
```
IF PortCount key exists → new format, skip migration
IF PortCount key absent → old format, run migration
```

### Migration Steps
```
1. Read global auth settings (already loaded into ServerConfig):
   - m_authMode, m_primaryPassword, m_readonlyPassword
   - m_groupRules, m_defaultWinAuthPermissions
   - m_useAuthentication
   - m_accessControlContainer

2. Build PortConfig for main port:
   pc0.port = m_rfbPort (default 5900)
   pc0.rect = m_mainPortMapping.getRect()
   pc0.devicePath = m_mainPortMapping.getDevicePath()
   pc0.authMode = m_authMode
   pc0.primaryPassword = m_primaryPassword
   pc0.readonlyPassword = m_readonlyPassword
   pc0.useAuthentication = m_useAuthentication
   pc0.groupRules = m_groupRules
   pc0.defaultWinAuthPermissions = m_defaultWinAuthPermissions
   pc0.ipAccessRules = m_accessControlContainer (copy)

3. For each extra port in m_portMappings:
   pcN.port = extraPort.getPort()
   pcN.rect = extraPort.getRect()
   pcN.devicePath = extraPort.getDevicePath()
   // Copy same global auth to each extra port
   pcN.authMode = m_authMode
   pcN.primaryPassword = m_primaryPassword
   pcN.readonlyPassword = m_readonlyPassword
   pcN.useAuthentication = m_useAuthentication
   pcN.groupRules = m_groupRules
   pcN.defaultWinAuthPermissions = m_defaultWinAuthPermissions
   pcN.ipAccessRules = m_accessControlContainer (copy)

4. Set all port configs:
   config->setAllPortConfigs({pc0, pc1, ...})

5. Mark migration done (PortCount will be written on next save)
```

## Requirements

### Functional
- Old config (no PortCount) auto-migrates to per-port format on load
- All ports get identical auth settings (cloned from global) — user can differentiate later via UI
- Migration is transparent — server starts normally
- Next `save()` writes new format (PortCount + Port0.AuthMode, etc.)
- Old keys (ExtraPorts, AuthMode, etc.) still written for backward compat with older TightVNC versions

### Non-functional
- Migration runs once per config load — no repeated migration
- No data loss — all settings preserved
- Migration is logged (info-level)

## Files to Modify

### 1. `server-config-lib/Configurator.cpp`

**Add migration method** (~50 lines):
```cpp
void Configurator::migrateToPerPortConfig(ServerConfig *config)
{
  // Check if already migrated (m_portConfigs non-empty from loadPortConfig)
  if (config->getPortConfigCount() > 0) {
    return;  // Already has per-port config
  }

  // Build port configs from global settings
  std::vector<PortConfig> portConfigs;

  // Main port
  PortConfig mainPc;
  mainPc.setPort(config->getRfbPort());
  PortMapping mainPm = config->getMainPortMapping();
  mainPc.setRect(mainPm.getRect());
  mainPc.setDevicePath(mainPm.getDevicePath().getString());

  // Clone global auth settings
  copyGlobalAuthToPortConfig(config, &mainPc);
  portConfigs.push_back(mainPc);

  // Extra ports
  AutoLock l(config);
  PortMappingContainer *extras = config->getPortMappingContainer();
  for (size_t i = 0; i < extras->count(); i++) {
    const PortMapping *pm = extras->at(i);
    PortConfig pc;
    pc.setPort(pm->getPort());
    pc.setRect(pm->getRect());
    pc.setDevicePath(pm->getDevicePath().getString());
    copyGlobalAuthToPortConfig(config, &pc);
    portConfigs.push_back(pc);
  }

  config->setAllPortConfigs(portConfigs);
}
```

**Add helper** (~20 lines):
```cpp
void Configurator::copyGlobalAuthToPortConfig(ServerConfig *config,
                                               PortConfig *pc)
{
  pc->setAuthMode(config->getAuthMode());
  pc->setUseAuthentication(config->isUsingAuthentication());
  pc->setDefaultWinAuthPermissions(config->getDefaultWinAuthPermissions());
  pc->setGroupRules(config->getGroupRules());

  unsigned char pass[ServerConfig::VNC_PASSWORD_SIZE];
  if (config->hasPrimaryPassword()) {
    config->getPrimaryPassword(pass);
    pc->setPrimaryPassword(pass);
    SecureZeroMemory(pass, sizeof(pass));
  }
  if (config->hasReadOnlyPassword()) {
    config->getReadOnlyPassword(pass);
    pc->setReadOnlyPassword(pass);
    SecureZeroMemory(pass, sizeof(pass));
  }

  // Copy IP access rules
  IpAccessControl *globalIp = config->getAccessControl();
  IpAccessControl *portIp = pc->getIpAccessControl();
  for (size_t i = 0; i < globalIp->size(); i++) {
    portIp->push_back(new IpAccessRule(*(*globalIp)[i]));
  }
}
```

**Call migration in `load(SettingsManager*)`** — after all old-format loading:
```cpp
bool Configurator::load(SettingsManager *sm)
{
  // ... existing loading code ...

  // Load old format
  loadServerConfig(sm, &m_serverConfig);
  loadPortMappingContainer(sm, m_serverConfig.getPortMappingContainer());
  loadWinAuthConfig(sm, &m_serverConfig);

  // Load new format (may find nothing if old config)
  loadPortConfig(sm, &m_serverConfig);

  // Migrate if needed
  migrateToPerPortConfig(&m_serverConfig);

  // ... rest of load ...
}
```

### 2. `server-config-lib/Configurator.h`

**Add declarations:**
```cpp
  void migrateToPerPortConfig(ServerConfig *config);
  void copyGlobalAuthToPortConfig(ServerConfig *config, PortConfig *pc);
```

## Implementation Steps

1. Add `migrateToPerPortConfig()` and `copyGlobalAuthToPortConfig()` declarations to `Configurator.h`
2. Implement `copyGlobalAuthToPortConfig()` — clone global auth fields to a PortConfig
3. Implement `migrateToPerPortConfig()` — detect old format, build PortConfig vector
4. Call migration after all old-format loading in `load(SettingsManager*)`
5. Verify: load old INI → migration runs → save writes new format
6. Verify: load new INI → migration skipped → per-port config preserved
7. Build and verify

## Todo List

- [ ] Add method declarations to Configurator.h
- [ ] Implement copyGlobalAuthToPortConfig()
- [ ] Implement migrateToPerPortConfig()
- [ ] Call migration in load() after old-format loading
- [ ] Test: old INI → new format migration
- [ ] Test: new INI → no migration needed
- [ ] Test: empty config (no ports) → default port created
- [ ] Build verification (0 errors)

## Success Criteria

- Old config (pre-per-port) loads successfully with migration
- All ports get correct auth settings from global config
- After save+reload, per-port config persists without re-migration
- New config format detected and used without migration
- No data loss during migration
- Build: 0 errors

## Risk Assessment

- **MEDIUM:** IpAccessControl deep copy — `IpAccessRule` objects are heap-allocated in the container. Must use proper copy constructor or clone.
- **LOW:** Password copy is straightforward 8-byte memcpy
- **LOW:** Migration logic is simple — one-time transform
- **LOW:** Old keys still written alongside new keys — no backward compat break

## Security Considerations

- Password buffers zeroed with `SecureZeroMemory` after copying to PortConfig
- Migration doesn't expose passwords — they remain encrypted in same format
- No new attack surface — migration is internal to Configurator

## Edge Cases

1. **No ports configured:** Create default PortConfig with port 5900 + global auth
2. **Only extra ports, no main port:** Unlikely but handle — create from first extra port
3. **Config file corrupted mid-migration:** `loadPortConfig()` returns true silently → migration runs on next load
4. **Multiple migrations:** `migrateToPerPortConfig()` checks `getPortConfigCount() > 0` — idempotent
