# Phase 6: UI Redesign — Port List with Per-Port Config Panels

**Priority:** MEDIUM — User-facing configuration interface
**Status:** Pending
**Depends on:** Phases 1-3 (PortConfig model + storage must be stable)
**Estimated effort:** Large

## Context Links

- [Plan overview](plan.md)
- `wsconfig-lib/ConfigDialog.h` — current 7-tab dialog
- `wsconfig-lib/ConfigDialog.cpp` — tab creation + apply logic
- `wsconfig-lib/ConnectionConfigDialog.h` — current port/connection tab
- `wsconfig-lib/AuthenticationConfigDialog.h` — current auth tab
- `wsconfig-lib/PermissionsConfigDialog.h` — current group rules tab
- `wsconfig-lib/IpAccessControlDialog.h` — current IP access tab
- `wsconfig-lib/EditPortMappingDialog.h` — current port mapping edit dialog
- `tvnserver/resource.h` — dialog template IDs

## Overview

Redesign the configuration UI from a flat 7-tab layout to a **port-centric** design:
- Left panel: ListView of all configured ports
- Right panel: Per-port tabbed settings (Display, Authentication, Permissions, IP Access)
- Separate "Global" tab group for server-wide settings (Session, Logging, HTTP)
- Add/Remove/Edit port buttons on the left panel

### Current UI Layout (7 tabs, all global)
```
[Connection] [Authentication] [IP Access] [Display & Input] [Permissions] [Session] [Logging]
```

### New UI Layout
```
┌───────────────────────────────────────────────────────────┐
│ TightVNC Configuration                                     │
├──────────────┬────────────────────────────────────────────┤
│ Ports        │  [Display] [Auth] [Permissions] [IP Access]│
│ ┌──────────┐ │  ┌─────────────────────────────────────┐   │
│ │ :5900    │ │  │ (Per-port settings for selected     │   │
│ │ :5901    │ │  │  port appear here)                  │   │
│ │          │ │  │                                      │   │
│ └──────────┘ │  └─────────────────────────────────────┘   │
│ [Add][Remove]│                                            │
├──────────────┼────────────────────────────────────────────┤
│ Global       │  [Session] [Logging] [HTTP]                │
│ Settings     │  (Server-wide settings)                    │
├──────────────┴────────────────────────────────────────────┤
│                          [OK] [Cancel] [Apply]            │
└───────────────────────────────────────────────────────────┘
```

## Key Insights

- Current tabs are already modular: each tab has `validateInput()`, `updateUI()`, `apply()`
- `AuthenticationConfigDialog` reads from global `ServerConfig` → must be adapted to read from a `PortConfig`
- `PermissionsConfigDialog` reads from global `ServerConfig` → same adaptation
- `IpAccessControlDialog` reads from global `ServerConfig` → same adaptation
- `ConnectionConfigDialog` currently manages main port + extra ports → replaced by port list
- The `.rc` dialog template (UTF-16LE) needs a new main dialog layout
- Existing per-port dialog classes can be reused with a `PortConfig*` target instead of `ServerConfig*`

## Requirements

### Functional
- Port list shows all configured ports with status (port number, display device)
- Add port: opens dialog to create new PortConfig with defaults
- Remove port: confirms and removes selected port
- Selecting a port in list shows its per-port tabs (Display, Auth, Permissions, IP)
- Per-port tabs read/write from selected PortConfig, not global ServerConfig
- Global tabs (Session, Logging) remain unchanged
- Apply saves all port configs + global settings
- HTTP port settings move to global section

### Non-functional
- Dialog resizable via anchoring (optional, nice-to-have)
- Port list uses Win32 ListView control (consistent with existing IP access list)
- Tab switching responsive — no flicker

## Architecture

### New Dialog Classes

```
ConfigDialog (redesigned main dialog)
├── m_portListView (ListView — left panel)
├── m_portTabs (TabControl — right panel, per-port tabs)
│   ├── PortDisplayConfigDialog (per-port display settings)
│   ├── PortAuthConfigDialog (per-port auth: VNC passwords + Windows auth mode)
│   ├── PortPermissionsConfigDialog (per-port group rules)
│   └── PortIpAccessConfigDialog (per-port IP access rules)
├── m_globalTabs (TabControl — bottom or separate section)
│   ├── SessionConfigDialog (unchanged)
│   └── LoggingConfigDialog (unchanged)
└── m_portConfigs (vector<PortConfig> — working copy for editing)
```

### Data Flow

1. `onInitDialog()`: load `vector<PortConfig>` from `ServerConfig::getAllPortConfigs()`
2. Populate port ListView from `m_portConfigs`
3. On port selection: copy selected `PortConfig` to per-port tab dialogs
4. Per-port tab dialogs operate on a `PortConfig*` pointer (the selected item in `m_portConfigs`)
5. `apply()`: write all `m_portConfigs` back via `ServerConfig::setAllPortConfigs()`
6. Global tabs apply directly to `ServerConfig` as before

## Files to Create

### 1. `wsconfig-lib/PortDisplayConfigDialog.h` + `.cpp` (NEW, ~100 lines each)

Per-port display settings dialog. Reuses controls from current `EditPortMappingDialog`:
- Port number (TextBox + SpinControl)
- Display geometry (TextBox for WxH+X+Y)
- Display device combo (populated from `WindowsDisplays`)

Operates on a `PortConfig*` target:
```cpp
class PortDisplayConfigDialog : public BaseDialog
{
public:
  void setPortConfig(PortConfig *pc);  // Set target port config
  void updateUI();                      // Load from m_portConfig
  void apply();                         // Save to m_portConfig
  bool validateInput();
private:
  PortConfig *m_portConfig;  // Not owned, points into ConfigDialog::m_portConfigs
  TextBox m_portTextBox;
  TextBox m_geometryTextBox;
  ComboBox m_displayCombo;
};
```

### 2. `wsconfig-lib/PortAuthConfigDialog.h` + `.cpp` (NEW, ~130 lines each)

Per-port authentication dialog. Derived from current `AuthenticationConfigDialog` but operates on `PortConfig*`:
- VNC password auth (use auth checkbox, set/unset primary, set/unset view-only)
- Windows auth mode (VNC-only / Windows-only / Both combo)
- Default Windows auth permissions
- No control interface auth (that's global)

```cpp
class PortAuthConfigDialog : public BaseDialog
{
public:
  void setPortConfig(PortConfig *pc);
  void updateUI();
  void apply();
  bool validateInput();
private:
  PortConfig *m_portConfig;
  CheckBox m_useAuthentication;
  Control m_primaryPasswordBtn;
  Control m_unsetPrimaryPasswordBtn;
  Control m_viewOnlyPasswordBtn;
  Control m_unsetViewOnlyPasswordBtn;
  CheckBox m_winAuthEnable;
  Control m_authModeCombo;
  Control m_defaultPermCombo;
};
```

### 3. `wsconfig-lib/PortPermissionsConfigDialog.h` + `.cpp` (NEW, ~150 lines each)

Per-port group permission rules. Same UI as current `PermissionsConfigDialog` but reads/writes `PortConfig`:

```cpp
class PortPermissionsConfigDialog : public BaseDialog
{
public:
  void setPortConfig(PortConfig *pc);
  void updateUI();
  void apply();
  bool validateInput();
private:
  PortConfig *m_portConfig;
  ListView m_ruleList;
  std::vector<GroupPermissionRule> m_rules;
  // ... same controls as PermissionsConfigDialog ...
};
```

### 4. `wsconfig-lib/PortIpAccessConfigDialog.h` + `.cpp` (NEW, ~120 lines each)

Per-port IP access rules. Simplified version of `IpAccessControlDialog`:
- IP rule list (ListView)
- Add/Edit/Remove buttons
- No loopback settings (those stay global)

```cpp
class PortIpAccessConfigDialog : public BaseDialog
{
public:
  void setPortConfig(PortConfig *pc);
  void updateUI();
  void apply();
  bool validateInput();
private:
  PortConfig *m_portConfig;
  IpAccessControl m_ipRules;
  ListView m_list;
  // ... add/edit/remove buttons ...
};
```

## Files to Modify

### 5. `wsconfig-lib/ConfigDialog.h` + `.cpp` — Major redesign

**Remove:** old per-port dialogs as direct members
```cpp
// Remove:
ConnectionConfigDialog m_connectionDialog;
AuthenticationConfigDialog m_authenticationDialog;
IpAccessControlDialog m_ipAccessControlDialog;
DisplayInputConfigDialog m_displayInputDialog;
PermissionsConfigDialog m_permissionsDialog;
```

**Add:** port list + per-port dialogs + working copy
```cpp
// Add:
ListView m_portListView;
Control m_addPortButton;
Control m_removePortButton;

// Per-port tab dialogs
TabControl m_portTabControl;
PortDisplayConfigDialog m_portDisplayDialog;
PortAuthConfigDialog m_portAuthDialog;
PortPermissionsConfigDialog m_portPermissionsDialog;
PortIpAccessConfigDialog m_portIpAccessDialog;

// Global tab dialogs (kept)
TabControl m_globalTabControl;
SessionConfigDialog m_sessionDialog;
LoggingConfigDialog m_loggingDialog;

// Working data
std::vector<PortConfig> m_portConfigs;
int m_selectedPortIndex;
```

**Key methods:**
- `onPortSelChange()`: save current port dialog state, load new port's config
- `onAddPort()`: add default PortConfig to `m_portConfigs`, select it
- `onRemovePort()`: remove selected, adjust selection
- `apply()`: write all `m_portConfigs` via `setAllPortConfigs()`, apply global tabs

### 6. `tvnserver/resource.h` — Add new dialog IDs

```cpp
#define IDD_CONFIG_PORT_DISPLAY_PAGE     150
#define IDD_CONFIG_PORT_AUTH_PAGE        151
#define IDD_CONFIG_PORT_PERMISSIONS_PAGE 152
#define IDD_CONFIG_PORT_IP_ACCESS_PAGE   153

#define IDC_PORT_LIST                    1080
#define IDC_ADD_PORT_BUTTON              1081
#define IDC_REMOVE_PORT_BUTTON           1082
#define IDC_PORT_TAB                     1083
#define IDC_GLOBAL_TAB                   1084
```

### 7. `tvnserver/tvnserver.rc` — New dialog templates

Add dialog templates for:
- Redesigned `IDD_CONFIG` (main dialog with split layout)
- `IDD_CONFIG_PORT_DISPLAY_PAGE` (port display settings)
- `IDD_CONFIG_PORT_AUTH_PAGE` (port auth settings)
- `IDD_CONFIG_PORT_PERMISSIONS_PAGE` (port group rules)
- `IDD_CONFIG_PORT_IP_ACCESS_PAGE` (port IP access)

**Note:** `.rc` file is UTF-16LE — use Visual Studio resource editor or careful binary editing.

### 8. `wsconfig-lib/wsconfig-lib.vcxproj` — Add new files

Add all new Port*ConfigDialog .h/.cpp files.

## Implementation Steps

1. Design dialog templates in Visual Studio resource editor
2. Add new resource IDs to `resource.h`
3. Create `PortDisplayConfigDialog` class (simplest per-port dialog)
4. Create `PortAuthConfigDialog` — adapt from `AuthenticationConfigDialog`
5. Create `PortPermissionsConfigDialog` — adapt from `PermissionsConfigDialog`
6. Create `PortIpAccessConfigDialog` — adapt from `IpAccessControlDialog`
7. Redesign `ConfigDialog` — add port list, per-port tabs, global tabs
8. Implement port selection change handler
9. Implement add/remove port handlers
10. Update `apply()` to save all port configs
11. Update `validateInput()` to validate all ports
12. Add all files to vcxproj
13. Build and verify
14. Manual test: add/remove ports, configure different auth per port

## Todo List

- [ ] Design new dialog templates in resource editor
- [ ] Add resource IDs to resource.h
- [ ] Create PortDisplayConfigDialog
- [ ] Create PortAuthConfigDialog
- [ ] Create PortPermissionsConfigDialog
- [ ] Create PortIpAccessConfigDialog
- [ ] Redesign ConfigDialog for port-list layout
- [ ] Implement port selection/add/remove handlers
- [ ] Update apply() and validateInput()
- [ ] Add files to vcxproj
- [ ] Update dialog templates in .rc file
- [ ] Build verification (0 errors)
- [ ] Manual test: full UI workflow

## Success Criteria

- Port list shows all configured ports
- Selecting a port loads its settings into per-port tabs
- Adding a port creates a new entry with defaults
- Removing a port deletes it (with confirmation)
- Per-port auth settings save independently
- Global settings (session, logging) still work
- Build: 0 errors

## Risk Assessment

- **HIGH:** `.rc` file editing — UTF-16LE binary, easy to corrupt. Use Visual Studio resource editor.
- **HIGH:** Dialog layout complexity — Win32 dialog positioning is manual pixel work
- **MEDIUM:** Code duplication between Port*ConfigDialog and old global dialogs. Extract shared logic where practical, but don't over-engineer.
- **MEDIUM:** Port selection change must save/load correctly — off-by-one or stale pointer risk
- **LOW:** Old dialog classes (ConnectionConfigDialog, AuthenticationConfigDialog, etc.) can be kept for reference or removed. Recommend keeping but marking deprecated.

## Security Considerations

- Password fields must use `ES_PASSWORD` style (masked input)
- Password set/unset buttons must properly zero password buffers
- Port config working copy (`m_portConfigs`) lives on UI thread — no cross-thread access
