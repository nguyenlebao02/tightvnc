# Phase 2: Unify Port Management in ServerConfig

**Priority:** HIGH — Eliminates main/extra port distinction
**Status:** Pending
**Depends on:** Phase 1 (PortConfig class)
**Estimated effort:** Medium

## Context Links

- [Plan overview](plan.md)
- [Phase 1: PortConfig model](phase-01-create-port-config-model.md)
- `server-config-lib/ServerConfig.h` lines 428-429 (current main+extra split)
- `tvnserver-app/TvnServer.cpp` lines 124-125 (main vs extra server startup)
- `tvnserver-app/ExtraRfbServers.cpp` lines 109-147 (extra port startup)

## Overview

Replace the current dual storage (`m_mainPortMapping` + `m_portMappings`) in `ServerConfig` with a unified `vector<PortConfig> m_portConfigs`. Eliminate the concept of "main port" vs "extra ports". Update `TvnServer` and `ExtraRfbServers` to treat all ports equally.

## Key Insights

- `ServerConfig` currently has: `PortMapping m_mainPortMapping` (line 428) + `PortMappingContainer m_portMappings` (line 429)
- `TvnServer` creates main RfbServer separately (line 387) and extra via `ExtraRfbServers` (line 125)
- After this phase, `TvnServer` will iterate `m_portConfigs` and create one RfbServer per entry
- `ExtraRfbServers` class will be repurposed to manage ALL port servers (renamed or refactored)
- Global auth fields in ServerConfig (m_authMode, m_groupRules, m_primaryPassword, etc.) remain for backward compat but are no longer the primary source — per-port PortConfig is authoritative

## Requirements

### Functional
- `ServerConfig` stores `vector<PortConfig>` as the authoritative port list
- `getAllPortConfigs()` / `setAllPortConfigs()` methods
- `getPortConfigByPort(int port)` returns a copy of the matching PortConfig
- Backward-compatible methods: `getRfbPort()` returns first port's number, `getPortMappingContainer()` still works
- `TvnServer` starts ALL ports through a single unified server manager

### Non-functional
- Thread-safe: AutoLock on ServerConfig before accessing port configs
- Existing callers of `getPortMappingContainer()` continue to compile

## Architecture

### Before
```
ServerConfig
├── m_rfbPort (int, main port number)
├── m_mainPortMapping (PortMapping)
├── m_portMappings (PortMappingContainer, extra ports)
├── m_authMode (global)
├── m_primaryPassword (global)
└── m_groupRules (global)
```

### After
```
ServerConfig
├── m_portConfigs (vector<PortConfig>)  ← NEW: all ports with per-port auth
├── m_authMode (global, kept for backward compat / defaults)
├── m_primaryPassword (global, kept for migration)
├── m_groupRules (global, kept for migration)
├── getRfbPort() → m_portConfigs[0].getPort()  (backward compat)
└── getPortMappingContainer() → builds from m_portConfigs  (backward compat)
```

## Files to Modify

### 1. `server-config-lib/ServerConfig.h`

**Add:**
```cpp
#include "PortConfig.h"
// ...
  // Unified port configuration (replaces main+extra split)
  std::vector<PortConfig> getAllPortConfigs();
  void setAllPortConfigs(const std::vector<PortConfig> &configs);
  PortConfig getPortConfigByPort(int port);
  bool hasPortConfig(int port);
  size_t getPortConfigCount();

protected:
  std::vector<PortConfig> m_portConfigs;  // All ports, index 0 = "primary"
```

**Keep existing methods** for backward compat:
- `getRfbPort()` → returns `m_portConfigs.empty() ? 5900 : m_portConfigs[0].getPort()`
- `setRfbPort()` → updates `m_portConfigs[0].setPort()`
- `getPortMappingContainer()` → builds container from `m_portConfigs` (skip index 0)
- `getAllPortMappings()` / `setAllPortMappings()` → convert between PortConfig and PortMapping

### 2. `server-config-lib/ServerConfig.cpp`

**Modify:**
- `getRfbPort()`: read from `m_portConfigs[0]` if non-empty
- `setRfbPort()`: update `m_portConfigs[0]` port number
- `getPortMappingContainer()`: build from `m_portConfigs` (indices 1..N)
- `getAllPortMappings()`: convert all `m_portConfigs` to `PortMapping` vector
- `setAllPortMappings()`: convert `PortMapping` vector to `PortConfig` vector (preserving existing auth settings if port matches)

**Add new methods:**
- `getAllPortConfigs()`: return copy of `m_portConfigs` under lock
- `setAllPortConfigs()`: replace `m_portConfigs` under lock
- `getPortConfigByPort()`: linear search by port number, return copy
- `hasPortConfig()`: check if port exists
- `getPortConfigCount()`: return size

### 3. `tvnserver-app/TvnServer.h` and `TvnServer.cpp`

**Key change:** Replace separate main+extra server management with unified approach.

**TvnServer.h changes:**
- Remove `m_rfbServer` (single main server pointer)
- Keep `m_extraRfbServers` but it now manages ALL servers
- OR: Replace `m_extraRfbServers` + `m_rfbServer` with `PortRfbServers m_portServers`

**TvnServer.cpp changes:**

`restartMainRfbServer()` → `restartPortServers()`:
```cpp
void TvnServer::restartPortServers()
{
  // Stop all existing
  m_extraRfbServers.shutDown();  // now handles all ports

  if (!m_srvConfig->isAcceptingRfbConnections()) return;

  // Start all ports through unified manager
  m_extraRfbServers.reload(m_runAsService, m_rfbClientManager);
}
```

`onConfigReload()`:
- Remove separate main vs extra logic
- Single call to `restartPortServers()` when any port config changes

### 4. `tvnserver-app/ExtraRfbServers.h` and `.cpp`

**Refactor:** Now manages ALL port servers (not just "extra").

**ExtraRfbServers::Conf** changes:
- Replace `PortMappingContainer extraPorts` with `vector<PortConfig> portConfigs`
- `equals()` compares all port configs

**ExtraRfbServers::getConfiguration()** changes:
```cpp
void ExtraRfbServers::getConfiguration(Conf *out)
{
  ServerConfig *config = Configurator::getInstance()->getServerConfig();
  AutoLock l(config);
  out->acceptConnections = config->isAcceptingRfbConnections();
  out->loopbackOnly = config->isOnlyLoopbackConnectionsAllowed();
  out->portConfigs = config->getAllPortConfigs();  // ALL ports
}
```

**ExtraRfbServers::startUp()** changes:
```cpp
bool ExtraRfbServers::startUp(bool asService, RfbClientManager *mgr)
{
  // ...
  for (size_t i = 0; i < newConf.portConfigs.size(); i++) {
    PortConfig pc = newConf.portConfigs[i];
    Rect rect(pc.getRect().left, pc.getRect().top,
              pc.getRect().right, pc.getRect().bottom);
    int port = pc.getPort();

    RfbServer *s = new RfbServer(bindHost, port, mgr, asService, m_log, &rect);
    m_servers.push_back(s);
  }
  // ...
}
```

## Implementation Steps

1. Add `#include "PortConfig.h"` and `m_portConfigs` to `ServerConfig.h`
2. Implement `getAllPortConfigs()`, `setAllPortConfigs()`, `getPortConfigByPort()`, etc. in `ServerConfig.cpp`
3. Update backward-compat methods (`getRfbPort`, `getPortMappingContainer`, etc.) to read from `m_portConfigs`
4. Build and verify — existing code still compiles
5. Update `ExtraRfbServers::Conf` to use `vector<PortConfig>`
6. Update `ExtraRfbServers::getConfiguration()` and `startUp()`
7. Update `TvnServer` to use unified restart logic
8. Remove `restartMainRfbServer()` / `stopMainRfbServer()` — replace with unified approach
9. Build and verify

## Todo List

- [ ] Add PortConfig include + m_portConfigs to ServerConfig.h
- [ ] Implement new ServerConfig methods in .cpp
- [ ] Update backward-compat methods to use m_portConfigs
- [ ] Update ExtraRfbServers::Conf to use vector<PortConfig>
- [ ] Update ExtraRfbServers::getConfiguration() and startUp()
- [ ] Update TvnServer to use unified port management
- [ ] Remove separate main server management from TvnServer
- [ ] Build verification (0 errors)

## Success Criteria

- All ports managed through single `vector<PortConfig>` in ServerConfig
- `TvnServer` starts all ports via unified path
- Existing callers (`getRfbPort()`, `getPortMappingContainer()`) still compile and work
- Config reload properly detects per-port changes and restarts affected servers
- Build: 0 errors

## Risk Assessment

- **MEDIUM:** Many files touched — backward-compat wrappers must be correct
- **MEDIUM:** `TvnServer::onConfigReload()` logic change — must detect port changes correctly
- **LOW:** `ExtraRfbServers` refactor is straightforward container change
- Thread safety: `getAllPortConfigs()` returns copies under lock — safe

## Security Considerations

- Password arrays in `PortConfig` are copied by value — no shared mutable buffer
- `SecureZeroMemory` should be used in PortConfig destructor for password fields
