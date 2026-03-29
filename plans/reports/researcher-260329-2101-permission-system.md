# Research: TightVNC v2.8.81 Permission & Authentication System

**Date:** 2026-03-29
**Focus:** Understand current permission architecture and identify what must change for per-port permission profiles

---

## Executive Summary

TightVNC v2.8.81 (chenall fork) uses a **globally-configured, connection-time permission model**:
- Passwords (VNC auth) and group rules (Windows auth) are **stored server-wide** in ServerConfig
- Permissions are **resolved per-client** during RfbInitializer auth phase
- **Currently: all ports share the same auth rules, group rules, and default permissions**

To enable **per-port permission profiles**, we need to:
1. Extend PortMapping to include port-specific auth config
2. Pass port context to RfbInitializer during auth
3. Make Configurator load/save per-port auth settings
4. Update RfbClientManager to pass port info to clients

---

## 1. ServerConfig Permission Fields

### 1.1 Authentication Mode (Global)
```cpp
// ServerConfig.h:67-71
enum AuthMode {
  AUTH_VNC_ONLY     = 0,  // Traditional VNC password only
  AUTH_WINDOWS_ONLY = 1,  // Windows user/password only
  AUTH_BOTH         = 2   // Both VNC password and Windows auth
};

AuthMode m_authMode;  // Line 442
```
- **Scope:** Global, applies to all ports
- **Storage:** Registry key `AuthMode` or INI key `[server]AuthMode`
- **Type:** UINT32
- **Default:** AUTH_VNC_ONLY

### 1.2 Passwords (Global)
```cpp
// ServerConfig.h:361-363
unsigned char m_primaryPassword[VNC_PASSWORD_SIZE];     // 8 bytes, DES encrypted
unsigned char m_readonlyPassword[VNC_PASSWORD_SIZE];   // 8 bytes, for view-only clients
unsigned char m_controlPassword[VNC_PASSWORD_SIZE];    // 8 bytes, for control interface
```
- **Scope:** Global, shared across all ports
- **Storage:** Registry (encrypted) or INI (encrypted)
- **Usage:** VNC auth (types 0x02) uses these for challenge-response
- **Constraint:** Single password = same auth for all ports

### 1.3 Windows Auth Group Rules (Global)
```cpp
// ServerConfig.h:444
std::vector<GroupPermissionRule> m_groupRules;

// GroupPermissionRule.h:34-65
// Maps Windows group → permission flags + priority
struct GroupPermissionRule {
  StringStorage m_groupName;        // "DOMAIN\GroupName"
  UINT32 m_permissionFlags;         // Bitmask from ClientPermissions
  int m_priority;                   // Higher = evaluated first
};
```
- **Scope:** Global, all ports use same rules
- **Storage Format:** Comma-separated rules
  - INI: `[server]WinAuthGroupRules=DOMAIN\Admins:31:100,DOMAIN\Users:3:50`
  - Format per rule: `GroupName:flags:priority`
- **Resolution Logic:**
  1. Sort by priority (descending)
  2. Get user's groups from Windows token
  3. First matching rule wins
  4. Fallback to m_defaultWinAuthPermissions

### 1.4 Default Win Auth Permissions (Global)
```cpp
// ServerConfig.h:443
UINT32 m_defaultWinAuthPermissions;

// ServerConfig.cpp:54 (constructor)
m_defaultWinAuthPermissions(ClientPermissions::PERM_VIEW_ONLY)
```
- **Default:** VIEW_ONLY (0x01)
- **Storage:** Registry/INI key `DefaultWinAuthPermissions`

---

## 2. ClientPermissions Model

### 2.1 Permission Flags (Bitmask)
```cpp
// ClientPermissions.h:36-48
static const UINT32 PERM_NONE          = 0x00;
static const UINT32 PERM_VIEW_SCREEN   = 0x01;  // Can see remote desktop
static const UINT32 PERM_KEYBOARD      = 0x02;  // Can send keyboard events
static const UINT32 PERM_MOUSE         = 0x04;  // Can send mouse events
static const UINT32 PERM_CLIPBOARD     = 0x08;  // Can exchange clipboard
static const UINT32 PERM_FILE_TRANSFER = 0x10;  // Can use file transfer
static const UINT32 PERM_DENY          = 0x80000000;  // Connection denied

// Composite flags
PERM_VIEW_ONLY    = 0x01
PERM_FULL_CONTROL = 0x01 | 0x02 | 0x04 | 0x08 | 0x10 = 0x1F
```

### 2.2 Permission Check Methods
- `canView()` — checks PERM_VIEW_SCREEN
- `canKeyboard()` — checks PERM_KEYBOARD
- `canMouse()` — checks PERM_MOUSE
- `canClipboard()` — checks PERM_CLIPBOARD
- `canFileTransfer()` — checks PERM_FILE_TRANSFER
- `isDenied()` — checks PERM_DENY (immediate rejection)
- `isViewOnly()` — VIEW && !KEYBOARD && !MOUSE
- `hasAnyPermission()` — !DENIED && flags != NONE

### 2.3 Client Usage
```cpp
// RfbClient.h:89-90
ClientPermissions getPermissions() const { return m_permissions; }
void setPermissions(const ClientPermissions &perms) { m_permissions = perms; }

// RfbClient.cpp (approximate)
m_permissions = rfbInitializer.getClientPermissions();
m_clientInputHandler->setPermissions(m_permissions);
m_clipboardExchange->setPermissions(m_permissions);
```

---

## 3. GroupPermissionRule System

### 3.1 Structure
```cpp
// GroupPermissionRule.h:34-65
class GroupPermissionRule {
  StringStorage m_groupName;      // Windows group, format: "DOMAIN\Name"
  UINT32 m_permissionFlags;       // Bitmask from ClientPermissions
  int m_priority;                 // Higher value = evaluated first
};

// Parsing: toString() / parse()
// Format: "DOMAIN\GroupName:permFlags:priority"
// Example: "BUILTIN\Administrators:31:100"
```

### 3.2 Resolution Flow
```cpp
// WinAuthenticator::resolvePermissions() — static method
Input:
  - groups: vector<StringStorage>  // User's Windows groups
  - rules: vector<GroupPermissionRule>  // All configured rules
  - defaultPerms: UINT32  // Fallback if no match

Algorithm:
  1. Sort rules by priority (descending) — compareByPriority()
  2. For each rule (highest priority first):
     - For each user group:
       - If group name matches rule (case-insensitive):
         → Return rule's permission flags
  3. No match found → Return defaultPerms

Output: ClientPermissions with resolved flags
```

### 3.3 Storage/Load (Configurator)
```cpp
// Configurator.cpp:892-923 (saveWinAuthConfig)
Save:
  1. Get rules from ServerConfig::getGroupRules()
  2. For each rule: rule.toString() → "DOMAIN\Group:flags:priority"
  3. Join with commas
  4. Save to Registry/INI key "WinAuthGroupRules"

// Configurator.cpp:925-975 (loadWinAuthConfig)
Load:
  1. Read "WinAuthGroupRules" string
  2. Split by comma
  3. Parse each chunk via GroupPermissionRule::parse()
  4. Load into ServerConfig::setGroupRules()
```

---

## 4. WinAuthenticator Authentication Flow

### 4.1 Three-Phase Process
```
Phase 1: authenticate()
  Input: username, password, domain
  → LogonUser(domain\username, password, LOGON32_LOGON_NETWORK)
  → Check if Guest token (reject if true)
  Output: m_token (Windows HANDLE)

Phase 2: getGroupMemberships()
  Input: m_token (from Phase 1)
  → GetTokenInformation(TokenGroups)
  → LookupAccountSid() per group SID
  → Convert to "DOMAIN\GroupName" format (case-insensitive)
  Output: std::vector<StringStorage> groups

Phase 3: resolvePermissions() [STATIC]
  Input: groups (from Phase 2), rules, defaultPerms
  → Match groups against rules by priority
  Output: ClientPermissions with resolved flags
```

### 4.2 Full Auth Method
```cpp
// WinAuthenticator.h:67-72
WinAuthResult performAuth(
  const TCHAR *username,
  TCHAR *password,     // Password buffer (zeros itself)
  const TCHAR *domain,
  const std::vector<GroupPermissionRule> &rules,
  UINT32 defaultPerms);
```
- Combines all three phases
- **CRITICAL:** Password is zeroed after use
- Returns `WinAuthResult` with perms + success flag

---

## 5. Authentication Flow During Client Connection

### 5.1 RfbInitializer::authPhase()
```cpp
// RfbInitializer.cpp:139-215 (simplified)

ServerConfig::AuthMode authMode = srvConf->getAuthMode();

// Step 1: Select auth type based on mode
if (authMode == AUTH_WINDOWS_ONLY) {
  doWinAuth();
} else if (authMode == AUTH_VNC_ONLY) {
  doVncAuth();
} else if (authMode == AUTH_BOTH) {
  // Client chooses from offered auth types
  offerAuthTypes([VNC, EXTERNAL/Windows]);
  UINT32 selectedType = readClientChoice();
  doAuth(selectedType);
}

// Step 2: After auth succeeds
if (winAuthWasUsed) {
  m_clientPermissions = WinAuthenticator::resolvePermissions(
    userGroups,
    serverConfig->getGroupRules(),         // ← GLOBAL
    serverConfig->getDefaultWinAuthPermissions()  // ← GLOBAL
  );
} else if (vncAuth) {
  // Legacy: derive perms from readOnly flag
  m_clientPermissions = ClientPermissions::fromViewOnlyFlag(m_viewOnlyAuth);
}
```

### 5.2 RfbClient Permission Application
```cpp
// RfbClient.cpp (excerpt from auth completion)
m_permissions = rfbInitializer.getClientPermissions();
m_clientInputHandler->setPermissions(m_permissions);
m_clipboardExchange->setPermissions(m_permissions);
```

### 5.3 Client State Access
```cpp
// RfbClient.h:89
ClientPermissions getPermissions() const { return m_permissions; }

// During normal operation:
// ClientInputHandler checks: if (!m_permissions.canKeyboard()) return;
// ClipboardExchange checks: if (!m_permissions.canClipboard()) return;
// UpdateSender checks: if (!m_permissions.canView()) return;
```

---

## 6. IP Access Control System

### 6.1 IpAccessRule Structure
```cpp
// IpAccessRule.h:35-123
class IpAccessRule {
  enum ActionType {
    ACTION_TYPE_ALLOW = 0,
    ACTION_TYPE_DENY  = 1,
    ACTION_TYPE_QUERY = 2
  };

  ActionType m_action;
  StringStorage m_firstIp;   // Start of IP range
  StringStorage m_lastIp;    // End of IP range
};

// Supports:
// - Single IP: "192.168.1.1"
// - Range: "192.168.1.1-192.168.1.10"
// - Subnet: "192.168.1.0/255.255.255.0"
```

### 6.2 Access Control Container (Global)
```cpp
// ServerConfig.h:435
IpAccessControl m_accessControlContainer;

// IpAccessControl.h:39
class IpAccessControl : public vector<IpAccessRule *> {}

// ServerConfig.h:253
IpAccessRule::ActionType getActionByAddress(unsigned long ip);
```
- **Scope:** Global across all ports
- **Storage:** Serialized in Registry/INI
- **Check:** Performed before RfbInitializer auth phase
- **Result:** ALLOW, DENY, or QUERY (ask user)

### 6.3 Storage Format
```
INI/Registry: "IpAccessRules"
Format: Comma-separated rules
Example: "ALLOW 192.168.1.0/255.255.255.0,DENY 10.0.0.0/255.0.0.0,QUERY 0.0.0.0/0.0.0.0"
```

---

## 7. PortMapping Structure (Current)

### 7.1 Definition
```cpp
// PortMapping.h:31-61
class PortMapping {
  int m_port;                    // Listening port (e.g., 5900)
  PortMappingRect m_rect;        // Display geometry (WxH+X+Y)
  StringStorage m_devicePath;    // Display device (e.g., "\\.\DISPLAY1")
};

// Parse format: "5901:1920x1080+0+0|\\.\DISPLAY2"
// Legacy: "5901:1920x1080+0+0"
```

### 7.2 Container (Global)
```cpp
// ServerConfig.h:428-429
PortMapping m_mainPortMapping;          // Primary port (5900)
PortMappingContainer m_portMappings;    // Extra ports

// Returns:
std::vector<PortMapping> getAllPortMappings();  // All port mappings
```

### 7.3 Storage (Configurator)
```cpp
// Configurator.cpp:223-261 (savePortMappingContainer)
// Configurator.cpp:263-295 (loadPortMappingContainer)

Storage key: "ExtraPorts"
Format: Comma-separated port mapping strings
Example: "5901:1920x1080+0+0|\\.\DISPLAY1,5902:1024x768+1920+0|\\.\DISPLAY2"
```

### 7.4 Current Limitation
- PortMapping contains **only** port + display geometry + device path
- **NO auth config** — all ports use global ServerConfig auth
- **NO group rules** — all ports use global group rules
- **NO IP access control** — all ports share global IP rules

---

## 8. Configuration Storage Format

### 8.1 INI File Layout
**File:** `tvnserver.ini` (same directory as tvnserver.exe)

```ini
[server]
; Authentication
AuthMode=2                  ; 0=VNC_ONLY, 1=WINDOWS_ONLY, 2=BOTH
DefaultWinAuthPermissions=31  ; Bitmask: 0x01|0x02|0x04|0x08|0x10 = full control

; Passwords (DES-encrypted)
PrimaryPassword=...base64...
ReadOnlyPassword=...base64...
ControlPassword=...base64...

; Windows Auth Rules (comma-separated: "GroupName:flags:priority,...")
WinAuthGroupRules=BUILTIN\Administrators:31:100,BUILTIN\Users:3:50

; Ports
RfbPort=5900
HttpPort=5800
ExtraPorts=5901:1920x1080+0+0|\\.\DISPLAY1,5902:1024x768+1920+0

; IP Access Control
IpAccessControl=ALLOW 192.168.1.0/255.255.255.0,DENY 10.0.0.0/255.0.0.0

; Other
FileTransfersEnabled=1
PollingInterval=1000
...
```

### 8.2 Registry Layout (Fallback)
**Root:** `HKEY_LOCAL_MACHINE\Software\TightVNC\Server` (service)
or `HKEY_CURRENT_USER\Software\TightVNC\Server` (app)

```
AuthMode                    (DWORD)
DefaultWinAuthPermissions   (DWORD)
PrimaryPassword             (BINARY, 8 bytes encrypted)
ReadOnlyPassword            (BINARY)
ControlPassword             (BINARY)
WinAuthGroupRules           (SZ, comma-separated)
RfbPort                     (DWORD)
HttpPort                    (DWORD)
ExtraPorts                  (SZ, comma-separated)
IpAccessControl             (SZ, comma-separated)
... (all other settings)
```

### 8.3 Load Priority
```cpp
// Configurator.cpp:99-124 (load method)
1. If INI file exists (.ini in exe dir):
   → Load from INI [server] section
2. If INI not found:
   → Load from Windows Registry (HKEY_LOCAL_MACHINE or HKEY_CURRENT_USER)
3. After loading:
   → notifyReload() → notify all ConfigReloadListener instances
```

---

## 9. Auth Integration Points

### 9.1 RfbServer → RfbClientManager
```cpp
// RfbServer listens on port, accepts connection
// Creates RfbClient with:
//   - socket (client connection)
//   - ServerConfig (global config)
//   - RfbClientManager callbacks
```

### 9.2 RfbClient → RfbInitializer
```cpp
// RfbClient::run() → creates RfbInitializer
RfbInitializer init(&channel, authListener, this, authAllowed);
init.authPhase();

// After auth:
m_permissions = init.getClientPermissions();
```

### 9.3 RfbInitializer → WinAuthenticator
```cpp
// RfbInitializer::doWinAuth()
WinAuthenticator auth(m_log);
WinAuthResult result = auth.performAuth(
  username,
  password,
  domain,
  serverConfig->getGroupRules(),         // ← GLOBAL
  serverConfig->getDefaultWinAuthPermissions()  // ← GLOBAL
);
```

### 9.4 RfbClientManager → IP Access Control
```cpp
// RfbClientManager::onCheckAccessControl(RfbClient *client)
unsigned long ip = client->getSocketAddr();
IpAccessRule::ActionType action =
  serverConfig->getAccessControl()->getActionByAddress(ip);
// ALLOW → continue, DENY → disconnect, QUERY → ask user
```

---

## 10. What's Global vs What Could Be Per-Port

### 10.1 Currently GLOBAL (Must Change)
| Component | Current Scope | Impact | Priority |
|-----------|--------------|--------|----------|
| AuthMode | All ports | Limits per-port auth strategy | HIGH |
| Group Rules | All ports | Can't have port-specific groups | HIGH |
| Default Win Auth Perms | All ports | No port-specific defaults | MEDIUM |
| Primary Password | All ports | Same VNC password for all | MEDIUM |
| IP Access Control | All ports | No per-port IP filters | LOW |

### 10.2 Currently Per-Port (OK)
| Component | Current Scope |
|-----------|--------------|
| Port number | Each PortMapping |
| Display device | Each PortMapping |
| Display geometry | Each PortMapping |

---

## 11. Data Flow Diagram (Text Format)

```
┌─────────────────────────────────────────────────────────────────┐
│                          TightVNC Client                        │
│                      (TCP connection to port)                   │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│                      RfbServer (per port)                       │
│  (Listens on RfbPort + ExtraPorts, accepts connections)        │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│                   RfbClientManager                              │
│  (Per-server manager, creates RfbClient threads)               │
│  - Checks IP access control (GLOBAL: m_accessControlContainer) │
│  - May ask user (QUERY action)                                 │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│                      RfbClient (per client)                     │
│  (Thread-per-connection model)                                 │
│  - Calls RfbInitializer.authPhase()                            │
│  - Stores: m_permissions (resolved during auth)                │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│                    RfbInitializer                               │
│  (Handles auth protocol negotiation)                           │
│                                                                 │
│  ┌─ AuthMode = serverConfig.getAuthMode() [GLOBAL]            │
│  ├─ Passwords = serverConfig.getPrimaryPassword() [GLOBAL]    │
│  │                                                              │
│  ├─► VNC Auth:                                                 │
│  │     - Challenge-response with primary/readonly password     │
│  │     - Determines m_viewOnlyAuth flag                        │
│  │     - Derives m_clientPermissions from flag                 │
│  │                                                              │
│  └─► Windows Auth (EXTERNAL type 130):                        │
│      1. Read username/password from client                     │
│      2. Call WinAuthenticator.performAuth()                   │
│         ├─► authenticate() — LogonUser()                       │
│         ├─► getGroupMemberships() — GetTokenInformation()      │
│         └─► resolvePermissions(groups, rules, defaults)       │
│             ├─ rules = serverConfig.getGroupRules() [GLOBAL]  │
│             └─ defaults = serverConfig.getDefaultWinAuthPerms │
│      3. Set m_clientPermissions from resolved result          │
│      4. Set m_winAuthUsed = true                              │
└────────────────────────────┬────────────────────────────────────┘
                             │
                             ▼
         m_clientPermissions returned to RfbClient
                             │
                             ▼
┌─────────────────────────────────────────────────────────────────┐
│                    RfbClient (continued)                        │
│  m_permissions = rfbInitializer.getClientPermissions()         │
│  m_clientInputHandler->setPermissions(m_permissions)           │
│  m_clipboardExchange->setPermissions(m_permissions)            │
│                                                                 │
│  During normal operation:                                       │
│  - ClientInputHandler enforces KEYBOARD, MOUSE perms           │
│  - ClipboardExchange enforces CLIPBOARD perm                   │
│  - UpdateSender enforces VIEW_SCREEN perm                      │
│  - FileTransfer enforces FILE_TRANSFER perm                    │
└─────────────────────────────────────────────────────────────────┘

[GLOBAL] = affects all ports
[PER-PORT] = could be per PortMapping (future enhancement)
```

---

## 12. Key Integration Points for Per-Port Permissions

### 12.1 Extension Points (What Must Change)
1. **PortMapping** → Add auth config fields
2. **ServerConfig** → Add port-auth mapping methods
3. **Configurator** → Load/save per-port auth from INI/Registry
4. **RfbServer** → Pass port context to RfbClientManager
5. **RfbClientManager** → Pass port context to RfbClient
6. **RfbClient** → Pass port context to RfbInitializer
7. **RfbInitializer** → Accept per-port auth config, use instead of global

### 12.2 Backward Compatibility Strategy
- If per-port config not found → use ServerConfig global settings
- Migration: Pre-populate per-port config from globals on first load
- Legacy INI/Registry keys remain for global defaults

### 12.3 Example: Per-Port Config Structure
```cpp
// Proposed PortMapping extension
class PortMapping {
  int m_port;
  PortMappingRect m_rect;
  StringStorage m_devicePath;

  // NEW: Per-port auth config
  bool m_useGlobalAuth;  // If true, fall back to ServerConfig
  ServerConfig::AuthMode m_authMode;  // Port-specific auth mode
  std::vector<GroupPermissionRule> m_groupRules;  // Port-specific rules
  UINT32 m_defaultWinAuthPermissions;

  // Passwords could remain global or per-port (TBD)
};

// INI storage:
// [server]
// ExtraPorts=5901:1920x1080+0+0|\\.\DISPLAY1|AuthMode=2|DefaultPerms=31|Rules=...
```

---

## 13. Unresolved Questions

1. **Passwords per-port or global?**
   - Current: Primary password is single, shared across all auth modes
   - Option A: Keep global (simpler)
   - Option B: Per-port passwords (more flexible but complex)

2. **IP Access Control per-port?**
   - Current: Global rules
   - Should each port have its own IP filter rules? (Currently marked LOW priority)

3. **Configurator state during per-port config changes**
   - When a per-port rule is added, should other ports with global config auto-migrate?
   - Or keep explicit "use global" flag per port?

4. **Backward compatibility for existing INI/Registry entries**
   - How to detect old global-only config and auto-migrate?
   - Version stamp in config file?

5. **RfbServer port context**
   - RfbServer currently doesn't track which port it's listening on
   - Must add port awareness to RfbServer constructor?
   - Or pass PortMapping object instead?

6. **File transfer per-port permission**
   - Currently, global `isFileTransfersEnabled()` controls file transfer availability
   - Should port-level file transfer be independent?
   - Or always follow global + permission flags?

---

## Summary: What Needs To Change

### Minimal Approach (Per-Port Groups + Auth Mode)
1. Add auth config to PortMapping structure
2. Extend Configurator to load/save per-port auth config
3. Pass port PortMapping to RfbInitializer
4. RfbInitializer uses port-level config instead of ServerConfig globals
5. **Scope:** AuthMode, GroupRules, DefaultWinAuthPermissions
6. **Passwords:** Keep global (use default primary password for all)

### Full Approach (Complete Per-Port Isolation)
- Above + per-port IP access control + per-port passwords
- Requires more restructuring but provides total isolation

### File Organization
- New header: `server-config-lib/PortAuthConfig.h`
- Modify: `server-config-lib/PortMapping.h` (add auth config)
- Modify: `server-config-lib/Configurator.cpp` (persist auth)
- Modify: `rfb-sconn/RfbInitializer.cpp` (use port config)
- Modify: `rfb-sconn/RfbClientManager.cpp` (pass port context)

