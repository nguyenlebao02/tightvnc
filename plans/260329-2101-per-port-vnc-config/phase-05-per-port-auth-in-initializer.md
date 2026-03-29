# Phase 5: RfbInitializer Uses Per-Port Config for Auth

**Priority:** HIGH — Core feature activation
**Status:** Pending
**Depends on:** Phase 4 (port context available in RfbInitializer)
**Estimated effort:** Medium

## Context Links

- [Plan overview](plan.md)
- [Phase 4: Port context pipeline](phase-04-port-context-pipeline.md)
- `rfb-sconn/RfbInitializer.cpp` lines 133-198 — `doTightAuth()` reads global config
- `rfb-sconn/RfbInitializer.cpp` lines 219-277 — `doVncAuth()` reads global passwords
- `rfb-sconn/RfbInitializer.cpp` lines 283-382 — `doWinAuth()` reads global group rules
- `rfb-sconn/RfbInitializer.cpp` lines 384-443 — `initAuthenticate()` reads global auth mode
- `rfb-sconn/RfbClient.cpp` lines 179, 209-217 — reads global config for permissions

## Overview

Replace all global `ServerConfig` reads in `RfbInitializer` with reads from `m_portConfig`. This is the core change that makes each port authenticate independently. Also update `RfbClient::execute()` and `RfbServer::onAcceptConnection()` to use per-port IP access control.

## Key Insights

- `RfbInitializer` currently calls `Configurator::getInstance()->getServerConfig()` in 5 places
- After Phase 4, `m_portConfig` is available as a member — replace global reads with it
- `doVncAuth()` reads primary/readonly passwords from global config → read from `m_portConfig`
- `doWinAuth()` reads group rules + default perms from global config → read from `m_portConfig`
- `doTightAuth()` reads auth mode from global config → read from `m_portConfig`
- `initAuthenticate()` reads auth mode + `isUsingAuthentication()` → read from `m_portConfig`
- `RfbServer::onAcceptConnection()` checks IP access from global → check from port config
- `checkForLoopback()` reads loopback settings — stays global (server-wide policy)

## Requirements

### Functional
- Auth mode per-port: port 5900 can use VNC-only while port 5901 uses Windows-only
- Passwords per-port: each port has its own primary/read-only VNC password
- Group rules per-port: each port has its own Windows auth group→permission mapping
- IP access per-port: each port has its own IP allow/deny rules
- Loopback policy remains global (server-wide setting in ServerConfig)

### Non-functional
- No wire protocol changes — clients see standard RFB auth negotiation
- Backward compat: if PortConfig has no per-port overrides, behavior identical to current

## Files to Modify

### 1. `rfb-sconn/RfbInitializer.cpp`

**`doTightAuth()` (lines 133-198) — use m_portConfig instead of global:**

```cpp
void RfbInitializer::doTightAuth()
{
  m_output->writeUInt32(0);  // No tunneling

  // USE PER-PORT CONFIG instead of global
  ServerConfig::AuthMode authMode = m_portConfig.getAuthMode();
  bool useAuth = m_portConfig.isUsingAuthentication();

  if (useAuth && m_authAllowed) {
    CapContainer authInfo;

    if ((authMode == ServerConfig::AUTH_VNC_ONLY ||
         authMode == ServerConfig::AUTH_BOTH) &&
        (m_portConfig.hasPrimaryPassword() || m_portConfig.hasReadOnlyPassword())) {
      authInfo.addCap(AuthDefs::VNC, VendorDefs::STANDARD, AuthDefs::SIG_VNC);
    }
    if (authMode == ServerConfig::AUTH_WINDOWS_ONLY ||
        authMode == ServerConfig::AUTH_BOTH) {
      authInfo.addCap(AuthDefs::EXTERNAL, VendorDefs::TIGHTVNC,
                      AuthDefs::SIG_EXTERNAL);
    }
    // ... rest same but using m_portConfig ...
  }
  // ... rest of method ...
}
```

**`doVncAuth()` (lines 219-277) — use m_portConfig passwords:**

```cpp
void RfbInitializer::doVncAuth()
{
  // ... challenge generation unchanged ...

  // USE PER-PORT passwords
  bool hasPrim = m_portConfig.hasPrimaryPassword();
  bool hasRdly = m_portConfig.hasReadOnlyPassword();

  if (!hasPrim && !hasRdly) {
    throw AuthException(_T("Server is not configured properly"));
  }

  if (hasPrim) {
    UINT8 crypPrimPass[8];
    m_portConfig.getPrimaryPassword(crypPrimPass);
    VncPassCrypt passCrypt;
    passCrypt.updatePlain(crypPrimPass);
    if (passCrypt.challengeAndResponseIsValid(challenge, response)) {
      return;
    }
  }
  if (hasRdly) {
    UINT8 crypReadOnlyPass[8];
    m_portConfig.getReadOnlyPassword(crypReadOnlyPass);
    // ... rest same ...
  }
  // ... rest same ...
}
```

**`doWinAuth()` (lines 283-382) — use m_portConfig group rules:**

```cpp
// Before (lines 344-346):
ServerConfig *srvConf = Configurator::getInstance()->getServerConfig();
std::vector<GroupPermissionRule> rules = srvConf->getGroupRules();
UINT32 defaultPerms = srvConf->getDefaultWinAuthPermissions();

// After:
std::vector<GroupPermissionRule> rules = m_portConfig.getGroupRules();
UINT32 defaultPerms = m_portConfig.getDefaultWinAuthPermissions();
```

**`initAuthenticate()` (lines 384-443) — use m_portConfig auth mode:**

```cpp
// Before (lines 389-404):
ServerConfig *srvConf = Configurator::getInstance()->getServerConfig();
ServerConfig::AuthMode authMode = srvConf->getAuthMode();
if (!m_authAllowed) {
  primSecType = SecurityDefs::NONE;
} else if (!srvConf->isUsingAuthentication()) {
  // ...
}

// After:
ServerConfig::AuthMode authMode = m_portConfig.getAuthMode();
if (!m_authAllowed) {
  primSecType = SecurityDefs::NONE;
} else if (!m_portConfig.isUsingAuthentication()) {
  if (authMode == ServerConfig::AUTH_WINDOWS_ONLY ||
      authMode == ServerConfig::AUTH_BOTH) {
    primSecType = SecurityDefs::VNC;  // Force TIGHT for Win auth
  } else {
    primSecType = SecurityDefs::NONE;
  }
}
```

**`checkForLoopback()` (lines 116-131) — KEEP global (server-wide policy):**
- This method checks `srvConf->isLoopbackConnectionsAllowed()` and `isOnlyLoopbackConnectionsAllowed()`
- These are server-wide policies, not per-port → keep reading from global ServerConfig
- No change needed

### 2. `tvnserver-app/RfbServer.cpp` (lines 57-91)

**`onAcceptConnection()` — use per-port IP access control:**

```cpp
void RfbServer::onAcceptConnection(SocketIPv4 *socket)
{
  try {
    SocketAddressIPv4 peerAddr;
    socket->getPeerAddr(&peerAddr);
    // ...

    struct sockaddr_in addr_in = peerAddr.getSockAddr();

    // USE PER-PORT IP access control
    const IpAccessControl *ipRules = m_portConfig.getIpAccessControl();
    IpAccessRule::ActionType action = IpAccessRule::ACTION_TYPE_ALLOW;

    // If port has IP rules, use them; otherwise fall back to global
    if (ipRules != 0 && ipRules->size() > 0) {
      action = ipRules->getActionByAddress(
        (unsigned long)addr_in.sin_addr.S_un.S_addr);
    } else {
      // Fallback to global config
      ServerConfig *config = Configurator::getInstance()->getServerConfig();
      action = config->getActionByAddress(
        (unsigned long)addr_in.sin_addr.S_un.S_addr);
    }

    if (action == IpAccessRule::ACTION_TYPE_DENY) {
      m_log->message(_T("Connection rejected due to access control rules"));
      delete socket;
      return;
    }

    socket->enableNaggleAlgorithm(false);
    m_clientManager->addNewConnection(socket, &m_viewPort, false, false,
                                      &m_portConfig);
  } catch (Exception &ex) {
    // ...
  }
}
```

### 3. `rfb-sconn/RfbClient.cpp` (lines 179, 209-217)

**`execute()` — use per-port config for file transfer check:**

```cpp
// Before (line 179):
ServerConfig *config = Configurator::getInstance()->getServerConfig();

// After — still need global config for non-port settings:
ServerConfig *config = Configurator::getInstance()->getServerConfig();
// But use m_portConfig for auth-related decisions (already done via RfbInitializer)
```

Note: Most of the auth logic is already handled by RfbInitializer. The `RfbClient::execute()` method just reads back the permissions that RfbInitializer resolved. No changes needed to the permission-application code (lines 209-217) — it already uses `rfbInitializer.getClientPermissions()`.

## Implementation Steps

1. In `RfbInitializer.cpp`, replace `Configurator::getInstance()->getServerConfig()` calls in `doTightAuth()`, `doVncAuth()`, `doWinAuth()`, `initAuthenticate()` with `m_portConfig` reads
2. Keep `checkForLoopback()` reading from global config (server-wide policy)
3. In `RfbServer.cpp`, update `onAcceptConnection()` to use per-port IP access rules
4. Verify `RfbClient::execute()` — no changes needed (permissions flow via RfbInitializer)
5. Build and verify
6. Manual test: configure two ports with different auth modes, verify each authenticates independently

## Todo List

- [ ] Replace global config reads in doTightAuth() with m_portConfig
- [ ] Replace global config reads in doVncAuth() with m_portConfig
- [ ] Replace global config reads in doWinAuth() with m_portConfig
- [ ] Replace global config reads in initAuthenticate() with m_portConfig
- [ ] Keep checkForLoopback() using global config
- [ ] Update RfbServer::onAcceptConnection() for per-port IP access
- [ ] Verify RfbClient::execute() needs no auth changes
- [ ] Build verification (0 errors)
- [ ] Manual test: two ports, different auth modes

## Success Criteria

- Port 5900 with VNC-only auth and port 5901 with Windows-only auth work simultaneously
- Each port uses its own VNC password
- Each port evaluates its own group permission rules
- Per-port IP access rules enforced correctly
- Loopback policy still works globally
- Build: 0 errors

## Risk Assessment

- **MEDIUM:** Regression risk — replacing global config reads could break edge cases. Must verify ALL code paths in `doTightAuth()` (4 branches) and `initAuthenticate()` (3 branches)
- **LOW:** `doVncAuth()` is straightforward password substitution
- **LOW:** `doWinAuth()` is straightforward rules/perms substitution
- **LOW:** IP access control fallback to global ensures backward compat

## Security Considerations

- Password buffers from `m_portConfig` are stack-local in `doVncAuth()` — properly scoped
- Windows auth credentials still cleared with `SecureZeroMemory` — unchanged
- Per-port IP filtering provides defense-in-depth (port-level network access control)
