# Research: TightVNC v2.8.81 Port Architecture & RfbServer Design

**Date:** 2026-03-29
**Researcher:** Analysis of TightVNC chenall fork architecture
**Focus:** Current multi-port implementation + architecture for per-port independent config

---

## Executive Summary

TightVNC v2.8.81 (chenall fork) currently uses a **single-centralized-config model** where:

- **Main port** (5900 default) uses global `ServerConfig` for auth mode, permissions, display rect
- **Extra ports** (configured via `PortMappingContainer`) also use same global config + per-port display rect
- All RfbServer instances share **one RfbClientManager** → all clients use same auth/permission rules
- Per-port independent auth, permissions, or security settings **NOT currently supported**

Architectural barriers to per-port config:
1. `RfbClientManager` is singleton/shared across all ports
2. `ServerConfig` is singleton with global auth mode + permission rules
3. Auth flow in `RfbInitializer` reads from global `ServerConfig`, not port-specific config
4. `RfbClient` stores permissions but doesn't track which port it came from

---

## Architecture Overview

### High-Level Component Relationships

```
TvnServer (Singleton)
├─ m_rfbServer (RfbServer for main port 5900)
│  └─ listens on port 5900
│     └─ calls RfbServer::onAcceptConnection()
│        └─ calls m_clientManager->addNewConnection()
│
├─ m_extraRfbServers (ExtraRfbServers)
│  └─ m_servers: std::list<RfbServer*> (extra ports)
│     ├─ RfbServer* for port N1
│     │  └─ listens on port N1
│     │     └─ calls m_clientManager->addNewConnection()
│     └─ RfbServer* for port N2
│        └─ listens on port N2
│           └─ calls m_clientManager->addNewConnection()
│
├─ m_rfbClientManager (RfbClientManager, Singleton)
│  ├─ m_clientList: std::list<RfbClient*> (authenticated clients)
│  ├─ m_nonAuthClientList: std::list<RfbClient*> (in-progress auth)
│  └─ m_desktop (shared WinDesktop)
│
└─ ServerConfig (Global singleton)
   ├─ m_authMode (VNC_ONLY | WINDOWS_ONLY | BOTH)
   ├─ m_groupPermissionRules (vector of GroupPermissionRule)
   ├─ m_primaryPassword / m_readOnlyPassword
   ├─ m_portMappingContainer (extra ports + rects)
   └─ m_objectCS (mutex for thread-safety)
```

### Class Hierarchy & Key Files

| Class | Location | Purpose |
|-------|----------|---------|
| **TvnServer** | `tvnserver-app/TvnServer.{h,cpp}` | Orchestrator; creates/destroys servers; handles config reload |
| **RfbServer** | `tvnserver-app/RfbServer.{h,cpp}` | TCP listener on port; accepts sockets; delegates to RfbClientManager |
| **ExtraRfbServers** | `tvnserver-app/ExtraRfbServers.{h,cpp}` | Manages list of extra RfbServers; starts/stops based on config |
| **RfbClientManager** | `tvnserver-app/RfbClientManager.{h,cpp}` | Receives sockets from all RfbServers; creates RfbClient threads |
| **RfbClient** | `rfb-sconn/RfbClient.{h,cpp}` | Per-client thread; handles protocol; stores permissions per-client |
| **RfbInitializer** | `rfb-sconn/RfbInitializer.{h,cpp}` | Performs RFB handshake & auth (VNC or Windows) |
| **ServerConfig** | `server-config-lib/ServerConfig.{h,cpp}` | Central config singleton; thread-safe (locked via `m_objectCS`) |
| **PortMapping** | `server-config-lib/PortMapping.{h,cpp}` | Holds: port + rect + optional devicePath |
| **PortMappingContainer** | `server-config-lib/PortMappingContainer.{h,cpp}` | Vector of PortMapping; supports serialization |
| **WinAuthenticator** | `win-auth-lib/WinAuthenticator.{h,cpp}` | Windows auth handler; looks up group SIDs → permissions |
| **ClientPermissions** | `server-config-lib/ClientPermissions.h` | Bitmask: VIEW, KEYBOARD, MOUSE, CLIPBOARD, FILE_TRANSFER, DENY |
| **GroupPermissionRule** | `server-config-lib/GroupPermissionRule.{h,cpp}` | Maps AD/local group → permission flags + priority |
| **Configurator** | `server-config-lib/Configurator.{h,cpp}` | Loads/saves config from INI or Registry; broadcasts config reload events |

---

## Current Data Flow

### 1. Server Startup (`TvnServer::TvnServer()`)

**Lines: tvnserver-app/TvnServer.cpp:58-129**

```
TvnServer constructor:
  1. Creates Configurator singleton
  2. Configurator::load() → reads from INI or Registry
  3. Gets ServerConfig* from Configurator
  4. Creates RfbClientManager (shared)
  5. Calls restartMainRfbServer()
     └─ Creates RfbServer(bindHost, port, m_rfbClientManager)
        └─ Main port (5900) listens via m_rfbClientManager
  6. Calls m_extraRfbServers.reload(m_runAsService, m_rfbClientManager)
     └─ Reads PortMappingContainer from ServerConfig
     └─ Creates RfbServer for each extra port
        └─ All pass same m_rfbClientManager
  7. Calls restartHttpServer()
  8. Calls restartControlServer()
```

### 2. Port Listen & Connection Accept

**Main RfbServer (port 5900):**
- `RfbServer::onAcceptConnection()` (RfbServer.cpp:57-91)
  1. Gets peer address
  2. Reads global ServerConfig
  3. Checks IP access control rules
  4. Calls `m_clientManager->addNewConnection(socket, &m_viewPort, ...)`

**Extra RfbServers (port N1, N2, ...):**
- Same `RfbServer::onAcceptConnection()` logic
- Each RfbServer has its own `m_viewPort` (from PortMapping rect)
- But **all pass same `m_clientManager` pointer**

### 3. New Connection Processing

**RfbClientManager::addNewConnection()** (RfbClientManager.cpp:374-406)

```cpp
void RfbClientManager::addNewConnection(
    SocketIPv4 *socket,
    ViewPortState *constViewPort,  // ← Port-specific rect from RfbServer
    bool viewOnly,
    bool isOutgoing)
{
  // Create RfbClient thread
  m_nonAuthClientList.push_back(new RfbClient(
    m_newConnectionEvents,
    socket,
    this,           // ← RfbClientManager listener
    this,           // ← ClientAuthListener
    viewOnly,
    isOutgoing,
    m_nextClientId,
    constViewPort,  // ← Rect from RfbServer (port-specific)
    &m_dynViewPort, // ← Global dynamic viewport
    timeout,
    m_log));
  m_nextClientId++;
}
```

**Key:** constViewPort is port-specific, but auth/perms logic is not.

### 4. RFB Handshake & Authentication

**RfbClient::run()** → **RfbInitializer::authPhase()** (RfbClient.cpp:197-220)

```cpp
// RfbInitializer::authPhase() reads from global ServerConfig:
ServerConfig *config = Configurator::getInstance()->getServerConfig();
AuthMode authMode = config->getAuthMode();  // VNC_ONLY | WINDOWS_ONLY | BOTH

if (authMode supports EXTERNAL auth) {
  // Offer Windows auth (type 130)
  // Client sends domain\username + password
  // WinAuthenticator::authenticate() resolves groups
  // Reads global GroupPermissionRules from ServerConfig
  // Returns ClientPermissions
  m_clientPermissions = WinAuthenticator::authenticate(...)
}
else {
  // VNC auth: read global password from ServerConfig
  // m_viewOnlyAuth = comparePassword(readOnlyPassword)
}

// Store permissions in RfbInitializer
// Later transferred to RfbClient
```

**Key point:** Auth decision is **100% from global ServerConfig**, NOT port-specific.

### 5. Client Authentication Complete

**RfbClient** → **RfbClientManager::onClientAuth()** (RfbClientManager.cpp:57-121)

```cpp
Desktop *RfbClientManager::onClientAuth(RfbClient *client)
{
  // Remove IP from ban list
  // Check shared flag policy (global config)
  // Remove non-auth clients if needed (global config)
  // Move client from m_nonAuthClientList → m_clientList
  // Create shared m_desktop if first client
  return m_desktop;
}
```

**Then in RfbClient:**
```cpp
// (RfbClient.cpp:210-220)
m_permissions = rfbInitializer.getClientPermissions();
m_viewOnly = m_permissions.isViewOnly();

// Apply permissions to handlers
m_clientInputHandler->setPermissions(m_permissions);
m_clipboardExchange->setPermissions(m_permissions);

// Check file transfer permission (global + per-client)
bool ftAllowed = config->isFileTransfersEnabled() && m_permissions.canFileTransfer();
```

**Issue:** Permissions applied uniformly. No per-port override.

### 6. Config Reload

**Configurator notifies listeners** via `ConfigReloadListener::onConfigReload()` (TvnServer.cpp:160-205)

```cpp
void TvnServer::onConfigReload(ServerConfig *serverConfig)
{
  // Check if main RFB server needs restart
  bool toggleMainRfbServer =
    m_srvConfig->isAcceptingRfbConnections() != (m_rfbServer != 0);
  bool changeMainRfbPort = m_rfbServer != 0 &&
    (m_srvConfig->getRfbPort() != (int)m_rfbServer->getBindPort());

  if (toggleMainRfbServer || changeMainRfbPort || changeBindHost) {
    restartMainRfbServer();
  }

  // Reload extra servers
  (void)m_extraRfbServers.reload(m_runAsService, m_rfbClientManager);
}
```

**ExtraRfbServers::reload()** (ExtraRfbServers.cpp:72-93):
```cpp
bool ExtraRfbServers::reload(bool asService, RfbClientManager *mgr)
{
  Conf newConf;
  getConfiguration(&newConf);  // ← Read from global ServerConfig

  if (newConf.equals(&m_effectiveConf) && enoughServers) {
    return true;  // No changes
  }

  shutDown();
  return startUp(asService, mgr);
}
```

**ExtraRfbServers::startUp()** (ExtraRfbServers.cpp:109-147):
```cpp
// Read config once at startup
ServerConfig *config = Configurator::getInstance()->getServerConfig();
AutoLock l(config);

out->acceptConnections = config->isAcceptingRfbConnections();
out->loopbackOnly = config->isOnlyLoopbackConnectionsAllowed();
out->extraPorts = *config->getPortMappingContainer();  // ← All ports use same global flags

// Then for each port:
for (size_t i = 0; i < newConf.extraPorts.count(); i++) {
  PortMapping pm = *newConf.extraPorts.at(i);
  Rect rect = pm.getRect();  // ← Per-port rect
  int port = pm.getPort();

  RfbServer *s = new RfbServer(bindHost, port, mgr, asService, m_log, &rect);
  // But all use same mgr (RfbClientManager) + same global config
}
```

---

## Current Port Mapping System

### PortMapping Structure

**server-config-lib/PortMapping.h**

```cpp
class PortMapping {
  int m_port;                      // Port number
  PortMappingRect m_rect;          // Display rect (WxH+X+Y)
  StringStorage m_devicePath;      // Optional: \\.\DISPLAY1 for multi-monitor
};
```

### Serialization Format

**PortMapping::toString()** (PortMapping.cpp:99-111):
```
Format: port:WxH+X+Y
Extended: port:WxH+X+Y|devicePath

Examples:
5901:1920x1080+0+0
5902:1280x1024+1920+0|\\.\DISPLAY2
```

### Parsing

**PortMapping::parse()** (PortMapping.cpp:113-167):
```cpp
// Parses string → PortMapping
// Splits on ':' for port:rect
// Splits on '|' for optional devicePath
```

### Storage

**server-config-lib/PortMappingContainer.h** → `std::vector<PortMapping>`

- Methods: `pushBack()`, `find()`, `findByPort()`, `at()`, `count()`, `equals()`, `serialize()`, `deserialize()`
- Allows multiple ports with different rects
- All stored in global ServerConfig

---

## Current Limitations for Per-Port Config

### 1. Shared RfbClientManager

**Problem:** All RfbServer instances → same RfbClientManager

```cpp
// TvnServer constructor (TvnServer.cpp:111)
m_rfbClientManager = new RfbClientManager(...);

// Main RfbServer (line 387)
m_rfbServer = new RfbServer(bindHost, bindPort, m_rfbClientManager, ...);

// Extra servers (ExtraRfbServers.cpp:134)
RfbServer *s = new RfbServer(bindHost, port, mgr, ...);  // ← Same mgr
```

**Consequence:**
- All incoming connections funneled to same `RfbClientManager`
- Clients can't be isolated by port
- Shared desktop, shared auth policy

### 2. Singleton ServerConfig

**Problem:** Global configuration is singular

```cpp
// RfbServer::onAcceptConnection() reads global config
ServerConfig *config = Configurator::getInstance()->getServerConfig();
IpAccessRule::ActionType action = config->getActionByAddress(...);
```

**Consequence:**
- IP access control is global (all ports)
- Auth mode is global (VNC_ONLY vs WINDOWS_ONLY vs BOTH)
- Group permission rules are global
- Password is global

### 3. Auth Resolved from Global Config

**Problem:** RfbInitializer always reads from ServerConfig

```cpp
// RfbInitializer::authPhase() (conceptual from RfbClient.cpp:197)
ServerConfig *config = Configurator::getInstance()->getServerConfig();
AuthMode authMode = config->getAuthMode();  // ← No port param

if (authMode == AUTH_WINDOWS_ONLY || authMode == AUTH_BOTH) {
  // Windows auth
  // Reads global GroupPermissionRules
  m_clientPermissions = authenticateWithGroupRules(config);
}
```

**Consequence:**
- Can't have "VNC auth only" on port 5900 and "Windows auth only" on port 5901
- Permission rules are global

### 4. No Port Context in RfbClient

**Problem:** RfbClient doesn't track which port it connected from

```cpp
// RfbClient.h (line 89-90)
ClientPermissions getPermissions() const { return m_permissions; }
void setPermissions(const ClientPermissions &perms) { m_permissions = perms; }

// No per-port config reference
// No way to query "what auth should I use for this client's port?"
```

**Consequence:**
- Can't apply per-port overrides
- No way to query port-specific policies

### 5. No Per-Port Config Structure

**Problem:** PortMapping only stores port + rect + device

```cpp
// PortMapping.h
class PortMapping {
  int m_port;
  PortMappingRect m_rect;
  StringStorage m_devicePath;
  // Missing: auth mode, permissions, password, access rules, etc.
};
```

**Consequence:**
- Extra ports can't have custom auth or security settings
- Extra ports inherit global policy

---

## Key Integration Points

### Critical Files & Functions

| File | Function/Method | Line | Purpose | Port-Specific? |
|------|-----------------|------|---------|---|
| TvnServer.cpp | `onConfigReload()` | 160 | Config reload handler | ✗ Global only |
| TvnServer.cpp | `restartMainRfbServer()` | 371 | Main port startup | ✗ |
| ExtraRfbServers.cpp | `reload()` | 72 | Extra port reload | ✗ Uses global config |
| ExtraRfbServers.cpp | `startUp()` | 109 | Creates RfbServers | ✓ Per-port rect only |
| RfbServer.cpp | `onAcceptConnection()` | 57 | New socket handler | ✗ Reads global config |
| RfbClientManager.cpp | `addNewConnection()` | 374 | Socket → RfbClient | ✓ Receives rect (unused for auth) |
| RfbClient.cpp | `run()` | 197 | Auth phase | ✗ Calls RfbInitializer |
| RfbInitializer.cpp | `authPhase()` | (see CLAUDE.md) | Protocol handshake | ✗ Reads global config |
| RfbClient.cpp | Line 210-220 | Permissions applied | ✗ Global + per-client |
| PortMapping.cpp | `parse()` / `toString()` | 113 / 99 | Serialization | ✓ Only stores port:rect |

---

## Architecture for Per-Port Independent Config

### Proposed Solution Overview

To support per-port independent configuration, the architecture must evolve:

**Option A: Per-Port Config Objects (Recommended)**
```
PortConfig (new class)
├─ authMode
├─ groupPermissionRules (copy or reference)
├─ passwords (or reference to global)
├─ accessControlRules (or reference)
└─ other port-specific settings

PortMappingContainer stores:
├─ PortMapping (port + rect + device)
└─ PortConfig* (per-port auth/perm settings)
```

**Option B: Port-Aware RfbClientManager**
```
RfbClientManager
├─ Map<port, ClientSubManager>
├─ Each submanager has clients for that port
└─ Config lookup via port in map
```

**Option C: Hybrid (Config Lookup, Minimal Changes)**
```
RfbClient stores:
├─ m_listeningPort (added)
└─ Lookup config via port in method calls

ServerConfig adds:
└─ Map<port, PortAuthPolicy>

RfbInitializer receives:
└─ port parameter to RfbInitializer::authPhase(port)
```

---

## Summary of Key Findings

### What Works Now (Per-Port)
- ✓ Display rectangles (PortMapping.rect)
- ✓ Multiple display devices (PortMapping.devicePath)
- ✓ Port numbers themselves

### What's Global (Needs Change)
- ✗ Authentication mode (VNC vs Windows vs Both)
- ✗ Group permission rules
- ✗ VNC passwords
- ✗ IP access control rules
- ✗ Client permission enforcement

### Architecture Barriers
1. Single RfbClientManager (no port awareness)
2. Singleton ServerConfig (global policy)
3. RfbInitializer reads global config (no port parameter)
4. RfbClient doesn't track listening port
5. PortMapping stores geometry only, not policy

### Minimal Intervention Points
1. **Add port tracking to RfbClient** → pass listening port from socket accept
2. **Extend PortMapping** → include auth policy or reference to PortConfig
3. **Add port-aware config lookup** → ServerConfig or separate PortAuthRegistry
4. **Modify RfbInitializer** → accept port parameter, lookup per-port auth mode
5. **Update ExtraRfbServers** → load per-port config when starting servers

---

## Unresolved Questions

1. Should per-port config completely override global, or be additive?
2. Should main port (5900) also be independently configurable?
3. How to handle password storage — per-port passwords or shared?
4. Should IP access control be per-port or remain global?
5. Should file transfer, wallpaper removal, disconnect actions be per-port?
6. Backward compatibility — how to migrate existing INI/Registry format?
7. Should per-port config be stored in PortMapping or separate PortConfig object?

