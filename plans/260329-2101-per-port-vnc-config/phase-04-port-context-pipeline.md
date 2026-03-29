# Phase 4: Pass Port Context Through Connection Pipeline

**Priority:** HIGH — Enables per-port auth decisions
**Status:** Pending
**Depends on:** Phase 1 (PortConfig class exists)
**Estimated effort:** Medium

## Context Links

- [Plan overview](plan.md)
- [Phase 1: PortConfig model](phase-01-create-port-config-model.md)
- `tvnserver-app/RfbServer.cpp` line 86 — `addNewConnection(socket, &m_viewPort, false, false)`
- `tvnserver-app/RfbClientManager.cpp` lines 374-406 — `addNewConnection()` creates RfbClient
- `rfb-sconn/RfbClient.cpp` lines 37-66 — constructor, no port context
- `rfb-sconn/RfbClient.cpp` line 190 — creates `RfbInitializer` without port info
- `rfb-sconn/RfbInitializer.cpp` lines 42-57 — constructor, no port config

## Overview

Thread the `PortConfig` through the entire connection pipeline so that `RfbInitializer` can read per-port auth settings instead of global `ServerConfig`. The chain is:

```
RfbServer (has PortConfig)
  → RfbClientManager::addNewConnection(socket, portConfig, ...)
    → RfbClient(socket, portConfig, ...)
      → RfbInitializer(stream, portConfig, ...)
```

## Key Insights

- `RfbServer` already holds `m_viewPort` (per-port display rect) — extend to hold `PortConfig`
- `RfbClientManager::addNewConnection()` receives `ViewPortState*` — add `PortConfig` parameter
- `RfbClient` stores `m_constViewPort` — add `m_portConfig` member (by value = thread-safe snapshot)
- `RfbInitializer` constructor gets `PortConfig` — stores as member for `authPhase()` use
- All parameters passed by value (copy) — no shared state, no lifetime issues

## Requirements

### Functional
- `RfbServer` stores a `PortConfig` copy (set at construction time)
- `RfbClientManager::addNewConnection()` accepts a `const PortConfig*` parameter
- `RfbClient` stores `PortConfig m_portConfig` member — copied from parameter
- `RfbInitializer` stores `PortConfig m_portConfig` — reads auth settings from it
- `RfbClient::execute()` passes `m_portConfig` to `RfbInitializer` constructor

### Non-functional
- Thread-safe: all PortConfig instances are copies (value semantics)
- Minimal API surface change — add parameter to existing methods
- No behavior change yet — Phase 5 actually uses the port config

## Files to Modify

### 1. `tvnserver-app/RfbServer.h` (lines 51-55, 74-79)

**Add `m_portConfig` member and constructor parameter:**

```cpp
// Before (line 51-56):
RfbServer(const TCHAR *bindHost, unsigned short bindPort,
          RfbClientManager *clientManager,
          bool lockAddr,
          LogWriter *log,
          const Rect *viewPort = 0) throw(Exception);

// After:
RfbServer(const TCHAR *bindHost, unsigned short bindPort,
          RfbClientManager *clientManager,
          bool lockAddr,
          LogWriter *log,
          const Rect *viewPort = 0,
          const PortConfig *portConfig = 0) throw(Exception);

// Add member (after line 79):
PortConfig m_portConfig;
```

### 2. `tvnserver-app/RfbServer.cpp` (lines 28-50, 57-91)

**Constructor — store PortConfig:**
```cpp
// Add to constructor (after line 39):
if (portConfig != 0) {
  m_portConfig = *portConfig;
}
```

**`onAcceptConnection()` — pass PortConfig to addNewConnection:**
```cpp
// Before (line 86):
m_clientManager->addNewConnection(socket, &m_viewPort, false, false);

// After:
m_clientManager->addNewConnection(socket, &m_viewPort, false, false, &m_portConfig);
```

### 3. `tvnserver-app/RfbClientManager.h` (line 94)

**Add PortConfig parameter to `addNewConnection()`:**
```cpp
// Before:
void addNewConnection(SocketIPv4 *socket, ViewPortState *constViewPort,
                      bool viewOnly, bool isOutgoing);

// After:
void addNewConnection(SocketIPv4 *socket, ViewPortState *constViewPort,
                      bool viewOnly, bool isOutgoing,
                      const PortConfig *portConfig = 0);
```

**Add include:**
```cpp
#include "server-config-lib/PortConfig.h"
```

### 4. `tvnserver-app/RfbClientManager.cpp` (lines 374-406)

**Pass PortConfig to RfbClient constructor:**
```cpp
// Before (lines 397-404):
m_nonAuthClientList.push_back(new RfbClient(m_newConnectionEvents,
                                            socket, this, this, viewOnly,
                                            isOutgoing,
                                            m_nextClientId,
                                            constViewPort,
                                            &m_dynViewPort,
                                            timeout,
                                            m_log));

// After:
m_nonAuthClientList.push_back(new RfbClient(m_newConnectionEvents,
                                            socket, this, this, viewOnly,
                                            isOutgoing,
                                            m_nextClientId,
                                            constViewPort,
                                            &m_dynViewPort,
                                            timeout,
                                            m_log,
                                            portConfig));
```

### 5. `rfb-sconn/RfbClient.h` (lines 62-69, ~150)

**Add PortConfig parameter and member:**
```cpp
// Constructor — add portConfig param (after line 69):
RfbClient(NewConnectionEvents *newConnectionEvents, SocketIPv4 *socket,
          ClientTerminationListener *extTermListener,
          ClientAuthListener *extAuthListener, bool viewOnly,
          bool isOutgoing, unsigned int id,
          const ViewPortState *constViewPort,
          const ViewPortState *dynViewPort,
          int idleTimeout,
          LogWriter *log,
          const PortConfig *portConfig = 0);

// Add member (after line 150, near m_permissions):
PortConfig m_portConfig;

// Add accessor:
const PortConfig &getPortConfig() const { return m_portConfig; }
```

**Add include:**
```cpp
#include "server-config-lib/PortConfig.h"
```

### 6. `rfb-sconn/RfbClient.cpp` (lines 37-66, 190)

**Constructor — store PortConfig:**
```cpp
// Add parameter to constructor signature and initializer:
RfbClient::RfbClient(..., const PortConfig *portConfig)
: m_socket(socket),
  // ... existing initializers ...
  m_portConfig(portConfig ? *portConfig : PortConfig())
{
  resume();
}
```

**`execute()` — pass PortConfig to RfbInitializer:**
```cpp
// Before (line 190-191):
RfbInitializer rfbInitializer(&sockStream, m_extAuthListener, this,
                              !m_isOutgoing);

// After:
RfbInitializer rfbInitializer(&sockStream, m_extAuthListener, this,
                              !m_isOutgoing, &m_portConfig);
```

### 7. `rfb-sconn/RfbInitializer.h` (lines 41-43, ~103)

**Add PortConfig to constructor and member:**
```cpp
// Before:
RfbInitializer(Channel *stream,
               ClientAuthListener *extAuthListener,
               RfbClient *client, bool authAllowed);

// After:
RfbInitializer(Channel *stream,
               ClientAuthListener *extAuthListener,
               RfbClient *client, bool authAllowed,
               const PortConfig *portConfig = 0);

// Add member (after line 103):
PortConfig m_portConfig;
```

**Add include:**
```cpp
#include "server-config-lib/PortConfig.h"
```

### 8. `rfb-sconn/RfbInitializer.cpp` (lines 42-57)

**Constructor — store PortConfig:**
```cpp
RfbInitializer::RfbInitializer(Channel *stream,
                               ClientAuthListener *extAuthListener,
                               RfbClient *client, bool authAllowed,
                               const PortConfig *portConfig)
: // ... existing initializers ...
  m_portConfig(portConfig ? *portConfig : PortConfig())
{
  // ... existing body ...
}
```

### 9. `tvnserver-app/ExtraRfbServers.cpp` (line 134)

**Pass PortConfig when creating RfbServer:**
```cpp
// Before:
RfbServer *s = new RfbServer(bindHost, port, mgr, asService, m_log, &rect);

// After:
PortConfig pc = newConf.portConfigs[i];
RfbServer *s = new RfbServer(bindHost, port, mgr, asService, m_log, &rect, &pc);
```

## Implementation Steps

1. Add `#include "server-config-lib/PortConfig.h"` to: RfbServer.h, RfbClientManager.h, RfbClient.h, RfbInitializer.h
2. Add `PortConfig m_portConfig` member to: RfbServer, RfbClient, RfbInitializer
3. Add `const PortConfig *portConfig` parameter to: RfbServer constructor, addNewConnection, RfbClient constructor, RfbInitializer constructor
4. Store PortConfig in each constructor (copy from pointer, default if null)
5. Thread the parameter through call sites: RfbServer::onAcceptConnection → addNewConnection → RfbClient → RfbInitializer
6. Update ExtraRfbServers::startUp() to pass PortConfig to RfbServer
7. Build and verify — no behavior change, just plumbing

## Todo List

- [ ] Add PortConfig include to RfbServer.h
- [ ] Add m_portConfig + constructor param to RfbServer
- [ ] Pass m_portConfig in RfbServer::onAcceptConnection()
- [ ] Add PortConfig param to RfbClientManager::addNewConnection()
- [ ] Pass PortConfig to RfbClient constructor
- [ ] Add m_portConfig + constructor param to RfbClient
- [ ] Pass m_portConfig to RfbInitializer in RfbClient::execute()
- [ ] Add m_portConfig + constructor param to RfbInitializer
- [ ] Update ExtraRfbServers::startUp() to pass PortConfig
- [ ] Build verification (0 errors)

## Success Criteria

- PortConfig flows from RfbServer → RfbClient → RfbInitializer
- No behavior change yet (Phase 5 activates per-port auth)
- All existing tests/builds pass with 0 errors
- Default PortConfig (null pointer) preserves current behavior

## Risk Assessment

- **HIGH:** Thread safety — PortConfig is copied by value at each boundary, so no shared mutable state. Must verify no reference/pointer retention to caller's copy.
- **LOW:** API change — all new parameters have default `= 0`, so existing callers compile unchanged
- **LOW:** Constructor chain — straightforward parameter addition

## Security Considerations

- PortConfig copies include password arrays — copies are on stack/heap of each thread, cleaned up on destruction
- `SecureZeroMemory` in PortConfig destructor for password fields
