# Phase 1: Create PortConfig Data Model

**Priority:** HIGH — Foundation for all subsequent phases
**Status:** Pending
**Estimated effort:** Small

## Context Links

- [Plan overview](plan.md)
- [Port architecture research](../reports/researcher-260329-2101-port-architecture.md)
- [Permission system research](../reports/researcher-260329-2101-permission-system.md)

## Overview

Create a new `PortConfig` class that encapsulates all per-port settings: auth mode, VNC passwords, Windows auth group rules, default permissions, IP access control, and display config. This replaces the scattered fields currently stored globally in `ServerConfig`.

## Key Insights

- Currently `ServerConfig` holds auth mode (line 442), passwords (lines 361-363), group rules (line 444), IP access (line 435) — ALL global
- `PortMapping` only stores port + rect + devicePath — needs to reference `PortConfig`
- Must be thread-safe: `PortConfig` will be read from RfbInitializer thread while ServerConfig thread writes
- Keep `PortConfig` as a value type (copyable) so thread-safe copies can be passed to RfbClient

## Requirements

### Functional
- `PortConfig` stores: port number, display rect, device path, auth mode, primary password, read-only password, group rules, default win auth permissions, IP access rules
- Copyable (value semantics) so RfbClient can hold a snapshot
- `isEqualTo()` for config-change detection (like current `PortMapping::isEqualTo`)
- Serialization: `toString()` / `parse()` for INI storage

### Non-functional
- Thread-safe when used via copies (no shared mutable state)
- Under 200 lines per file
- Follow existing patterns (TCHAR, StringStorage, etc.)

## Architecture

```
PortConfig (new class, value type)
├── int m_port
├── PortMappingRect m_rect
├── StringStorage m_devicePath
├── AuthMode m_authMode              (VNC_ONLY | WINDOWS_ONLY | BOTH)
├── unsigned char m_primaryPassword[8]
├── unsigned char m_readonlyPassword[8]
├── bool m_hasPrimaryPassword
├── bool m_hasReadonlyPassword
├── vector<GroupPermissionRule> m_groupRules
├── UINT32 m_defaultWinAuthPermissions
├── IpAccessControl m_ipAccessRules  (per-port IP rules)
└── bool m_useAuthentication         (VNC password auth enabled flag)
```

## Files to Create

### 1. `server-config-lib/PortConfig.h` (NEW, ~90 lines)

```cpp
#ifndef _PORT_CONFIG_H_
#define _PORT_CONFIG_H_

#include "util/StringStorage.h"
#include "PortMappingRect.h"
#include "IpAccessControl.h"
#include "ClientPermissions.h"
#include "GroupPermissionRule.h"
#include "ServerConfig.h"  // For AuthMode enum

#include <vector>

// Per-port configuration: auth, passwords, permissions, display.
// Value type — safe to copy across threads.
class PortConfig
{
public:
  static const int VNC_PASSWORD_SIZE = 8;

  PortConfig();
  PortConfig(const PortConfig &other);
  PortConfig &operator=(const PortConfig &other);
  virtual ~PortConfig();

  bool isEqualTo(const PortConfig *other) const;

  // Port & display
  int getPort() const;
  void setPort(int port);
  PortMappingRect getRect() const;
  void setRect(PortMappingRect rect);
  const StringStorage &getDevicePath() const;
  void setDevicePath(const TCHAR *path);

  // Auth mode
  ServerConfig::AuthMode getAuthMode() const;
  void setAuthMode(ServerConfig::AuthMode mode);

  // VNC passwords
  void getPrimaryPassword(unsigned char *out) const;
  void setPrimaryPassword(const unsigned char *value);
  bool hasPrimaryPassword() const;
  void deletePrimaryPassword();

  void getReadOnlyPassword(unsigned char *out) const;
  void setReadOnlyPassword(const unsigned char *value);
  bool hasReadOnlyPassword() const;
  void deleteReadOnlyPassword();

  bool isUsingAuthentication() const;
  void setUseAuthentication(bool use);

  // Windows auth group rules
  std::vector<GroupPermissionRule> getGroupRules() const;
  void setGroupRules(const std::vector<GroupPermissionRule> &rules);
  UINT32 getDefaultWinAuthPermissions() const;
  void setDefaultWinAuthPermissions(UINT32 perms);

  // IP access control
  IpAccessControl *getIpAccessControl();
  const IpAccessControl *getIpAccessControl() const;

  // Convert to/from legacy PortMapping (display fields only)
  PortMapping toPortMapping() const;
  void fromPortMapping(const PortMapping &pm);

protected:
  int m_port;
  PortMappingRect m_rect;
  StringStorage m_devicePath;

  ServerConfig::AuthMode m_authMode;
  unsigned char m_primaryPassword[VNC_PASSWORD_SIZE];
  unsigned char m_readonlyPassword[VNC_PASSWORD_SIZE];
  bool m_hasPrimaryPassword;
  bool m_hasReadonlyPassword;
  bool m_useAuthentication;

  std::vector<GroupPermissionRule> m_groupRules;
  UINT32 m_defaultWinAuthPermissions;

  IpAccessControl m_ipAccessRules;
};

#endif // _PORT_CONFIG_H_
```

### 2. `server-config-lib/PortConfig.cpp` (NEW, ~160 lines)

Implements all methods. Key patterns:
- Constructor zeros passwords, sets defaults (AUTH_VNC_ONLY, PERM_VIEW_ONLY, port=0)
- Copy constructor/operator= deep-copies all fields including password arrays
- `isEqualTo()` compares all fields (port, rect, device, auth mode, passwords, rules, IP rules)
- `toPortMapping()` creates a `PortMapping` from port+rect+device fields
- `fromPortMapping()` populates port+rect+device from a `PortMapping`

## Files to Modify

### 3. `server-config-lib/server-config-lib.vcxproj` — Add PortConfig.h/.cpp to project

### 4. `server-config-lib/PortMapping.h` — No changes needed (PortConfig replaces it at higher level)

## Implementation Steps

1. Create `PortConfig.h` with class declaration
2. Create `PortConfig.cpp` with all method implementations
3. Add both files to `server-config-lib.vcxproj` (ClCompile + ClInclude)
4. Build and verify 0 errors

## Todo List

- [ ] Create `server-config-lib/PortConfig.h`
- [ ] Create `server-config-lib/PortConfig.cpp`
- [ ] Add files to vcxproj
- [ ] Build verification (0 errors)

## Success Criteria

- `PortConfig` compiles standalone
- All getters/setters work correctly
- `isEqualTo()` detects changes in any field
- Copy semantics work (modifying copy doesn't affect original)
- Build: 0 errors, 0 warnings related to PortConfig

## Risk Assessment

- **LOW:** Standalone new class, no existing code modified
- **LOW:** Simple value type with well-understood patterns
- Password handling must match existing `ServerConfig` pattern (raw 8-byte arrays)

## Build Verification

```bash
cd "E:/Phat trien VNC server/tightvnc-source"
"C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/amd64/MSBuild.exe" tightvnc2019.sln -p:Configuration=Release -p:Platform=x86 -m -verbosity:minimal
```

Expected: 0 errors. PortConfig.obj produced.
