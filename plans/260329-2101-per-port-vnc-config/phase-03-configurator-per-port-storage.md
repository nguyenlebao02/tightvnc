# Phase 3: Configurator Load/Save Per-Port Config

**Priority:** HIGH — Persistence layer for per-port settings
**Status:** Pending
**Depends on:** Phase 2 (unified ServerConfig port storage)
**Estimated effort:** Medium

## Context Links

- [Plan overview](plan.md)
- [Phase 2: Unify port management](phase-02-unify-port-management.md)
- `server-config-lib/Configurator.cpp` lines 981-1044 (existing savePortConfig/loadPortConfig)
- `server-config-lib/Configurator.cpp` lines 892-979 (existing saveWinAuthConfig/loadWinAuthConfig)

## Overview

Extend the Configurator to save/load per-port auth, password, group rules, and IP access settings. The existing `savePortConfig`/`loadPortConfig` methods already save port display info (Port0, Port1, etc.) — we extend this to include auth settings per port.

## Key Insights

- Current `savePortConfig()` saves: `PortCount=N`, `Port0=5900:1920x1080+0+0|\\.\DISPLAY1`, etc.
- Current `saveWinAuthConfig()` saves global: `AuthMode`, `DefaultWinAuthPermissions`, `WinAuthGroupRules`
- New format: each port gets its own INI section `[port.0]`, `[port.1]`, etc.
- OR: keep flat keys with port index prefix: `Port0.AuthMode`, `Port0.PrimaryPassword`, etc.
- Decision: **Use indexed flat keys** — simpler, matches existing SettingsManager API which reads from a single INI section

## INI Format Design

### Current (Global)
```ini
[server]
RfbPort=5900
AuthMode=2
PrimaryPassword=...
WinAuthGroupRules=BUILTIN\Admins:31:100
ExtraPorts=5901:1920x1080+0+0|\\.\DISPLAY1
```

### New (Per-Port)
```ini
[server]
; Global settings (kept for backward compat + truly server-wide settings)
HttpPort=5800
FileTransfersEnabled=1
PollingInterval=1000
; ... other global settings ...

; Per-port config (new format)
PortCount=2

; Port 0 — display config
Port0=5900:0x0+0+0
Port0.AuthMode=2
Port0.UseAuth=1
Port0.PrimaryPassword=F92FA58FE7324C29
Port0.ReadOnlyPassword=
Port0.DefaultWinAuthPerms=31
Port0.WinAuthGroupRules=BUILTIN\Administrators:31:100,BUILTIN\Users:3:50
Port0.IpAccessRules=ALLOW 192.168.1.0/255.255.255.0

; Port 1 — different display, different auth
Port1=5901:1920x1080+0+0|\\.\DISPLAY2
Port1.AuthMode=0
Port1.UseAuth=1
Port1.PrimaryPassword=A1B2C3D4E5F6A7B8
Port1.ReadOnlyPassword=
Port1.DefaultWinAuthPerms=1
Port1.WinAuthGroupRules=
Port1.IpAccessRules=
```

### Registry Format (Same Keys)
```
HKLM\Software\TightVNC\Server
  PortCount          (DWORD) = 2
  Port0              (SZ)    = "5900:0x0+0+0"
  Port0.AuthMode     (DWORD) = 2
  Port0.UseAuth      (DWORD) = 1
  Port0.PrimaryPassword (BINARY) = ...
  ...
```

## Requirements

### Functional
- Save all `PortConfig` fields per port using indexed keys
- Load per-port config; if new format not found, fall back to global config (migration in Phase 7)
- Passwords stored as hex-encoded strings (INI) or binary (Registry) — match existing pattern
- IP access rules stored as comma-separated per-port string

### Non-functional
- `SettingsManager` abstraction handles both INI and Registry transparently
- Loading partial config (some keys missing) uses defaults — never crashes

## Files to Modify

### 1. `server-config-lib/Configurator.cpp`

**Replace `savePortConfig()`** (~60 lines):
```cpp
bool Configurator::savePortConfig(SettingsManager *sm)
{
  AutoLock l(&m_serverConfig);
  std::vector<PortConfig> allPorts = m_serverConfig.getAllPortConfigs();
  UINT portCount = (UINT)allPorts.size();

  sm->setUINT(_T("PortCount"), portCount);

  for (UINT i = 0; i < portCount; i++) {
    const PortConfig &pc = allPorts[i];
    StringStorage prefix;
    prefix.format(_T("Port%u"), i);

    // Display config (port:rect|device)
    PortMapping pm = pc.toPortMapping();
    StringStorage pmStr;
    pm.toString(&pmStr);
    sm->setString(prefix.getString(), pmStr.getString());

    // Auth mode
    StringStorage key;
    key.format(_T("Port%u.AuthMode"), i);
    sm->setUINT(key.getString(), (UINT)pc.getAuthMode());

    key.format(_T("Port%u.UseAuth"), i);
    sm->setUINT(key.getString(), pc.isUsingAuthentication() ? 1 : 0);

    // Passwords (hex-encoded)
    key.format(_T("Port%u.PrimaryPassword"), i);
    savePasswordHex(sm, key.getString(), pc, /*primary=*/true);

    key.format(_T("Port%u.ReadOnlyPassword"), i);
    savePasswordHex(sm, key.getString(), pc, /*primary=*/false);

    // Win auth
    key.format(_T("Port%u.DefaultWinAuthPerms"), i);
    sm->setUINT(key.getString(), (UINT)pc.getDefaultWinAuthPermissions());

    key.format(_T("Port%u.WinAuthGroupRules"), i);
    savePortGroupRules(sm, key.getString(), pc.getGroupRules());

    // IP access
    key.format(_T("Port%u.IpAccessRules"), i);
    savePortIpRules(sm, key.getString(), pc.getIpAccessControl());
  }
  return true;
}
```

**Replace `loadPortConfig()`** (~80 lines):
```cpp
bool Configurator::loadPortConfig(SettingsManager *sm, ServerConfig *config)
{
  UINT portCount = 0;
  if (!sm->getUINT(_T("PortCount"), &portCount)) {
    return true;  // No new format — Phase 7 migration handles this
  }

  std::vector<PortConfig> allPorts;
  for (UINT i = 0; i < portCount; i++) {
    PortConfig pc;
    StringStorage prefix;
    prefix.format(_T("Port%u"), i);

    // Load display config
    StringStorage pmStr;
    if (sm->getString(prefix.getString(), &pmStr)) {
      PortMapping pm;
      if (PortMapping::parse(pmStr.getString(), &pm)) {
        pc.fromPortMapping(pm);
      }
    }

    // Load auth mode
    StringStorage key;
    key.format(_T("Port%u.AuthMode"), i);
    UINT uVal;
    if (sm->getUINT(key.getString(), &uVal) && uVal <= 2) {
      pc.setAuthMode((ServerConfig::AuthMode)uVal);
    }

    // Load UseAuth
    key.format(_T("Port%u.UseAuth"), i);
    if (sm->getUINT(key.getString(), &uVal)) {
      pc.setUseAuthentication(uVal != 0);
    }

    // Load passwords, group rules, IP rules...
    // (similar pattern for each field)

    allPorts.push_back(pc);
  }

  if (!allPorts.empty()) {
    config->setAllPortConfigs(allPorts);
  }
  return true;
}
```

**Add helper methods** (~40 lines each):
- `savePasswordHex()` — encode 8-byte password as hex string
- `loadPasswordHex()` — decode hex string to 8-byte password
- `savePortGroupRules()` — serialize vector<GroupPermissionRule> to comma-separated string
- `loadPortGroupRules()` — deserialize comma-separated string to vector
- `savePortIpRules()` — serialize IpAccessControl to comma-separated string
- `loadPortIpRules()` — deserialize comma-separated string to IpAccessControl

### 2. `server-config-lib/Configurator.h`

**Add private helper declarations:**
```cpp
  // Per-port config helpers
  void savePasswordHex(SettingsManager *sm, const TCHAR *key,
                       const PortConfig &pc, bool primary);
  void loadPasswordHex(SettingsManager *sm, const TCHAR *key,
                       PortConfig *pc, bool primary);
  void savePortGroupRules(SettingsManager *sm, const TCHAR *key,
                          const std::vector<GroupPermissionRule> &rules);
  bool loadPortGroupRules(SettingsManager *sm, const TCHAR *key,
                          std::vector<GroupPermissionRule> *rules);
  void savePortIpRules(SettingsManager *sm, const TCHAR *key,
                       const IpAccessControl *rules);
  bool loadPortIpRules(SettingsManager *sm, const TCHAR *key,
                       IpAccessControl *rules);
```

## Implementation Steps

1. Add helper method declarations to `Configurator.h`
2. Implement `savePasswordHex()` / `loadPasswordHex()` — hex encode/decode 8-byte passwords
3. Implement `savePortGroupRules()` / `loadPortGroupRules()` — reuse existing comma-separated pattern from `saveWinAuthConfig()`
4. Implement `savePortIpRules()` / `loadPortIpRules()` — reuse existing IP rule serialization
5. Replace `savePortConfig()` — iterate `getAllPortConfigs()`, write indexed keys
6. Replace `loadPortConfig()` — read indexed keys, build `PortConfig` vector
7. Ensure `save()` calls both `saveWinAuthConfig()` (global, backward compat) AND `savePortConfig()` (per-port)
8. Ensure `load()` calls `loadPortConfig()` after `loadWinAuthConfig()` — per-port overrides global
9. Build and verify

## Todo List

- [ ] Add helper declarations to Configurator.h
- [ ] Implement savePasswordHex/loadPasswordHex
- [ ] Implement savePortGroupRules/loadPortGroupRules
- [ ] Implement savePortIpRules/loadPortIpRules
- [ ] Replace savePortConfig with per-port auth saving
- [ ] Replace loadPortConfig with per-port auth loading
- [ ] Verify save() ordering (global then per-port)
- [ ] Verify load() ordering (global then per-port overrides)
- [ ] Build verification (0 errors)

## Success Criteria

- Per-port config persists across server restarts
- Loading an old INI (no PortCount) still works (global config used)
- Loading a new INI correctly populates per-port auth settings
- Passwords stored securely (hex-encoded, not plaintext)
- Build: 0 errors

## Risk Assessment

- **MEDIUM:** Password hex encoding must match existing encryption pattern — verify `VncPassCrypt` usage
- **MEDIUM:** INI key naming — must not collide with existing keys
- **LOW:** Group rule serialization — reuses existing tested pattern
- Backward compat: old `ExtraPorts` key still loaded by `loadPortMappingContainer()` — migration in Phase 7 reconciles

## Security Considerations

- Passwords stored as hex-encoded encrypted bytes (same as current `PrimaryPassword` key)
- `SecureZeroMemory` used for password buffers during load
- INI file permissions should be restricted (existing behavior)
