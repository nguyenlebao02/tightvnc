# TightVNC v2.8.81 Configuration UI Architecture Research

**Date:** 2026-03-29
**Focus:** Dialog system, tab structure, resource organization for per-port config redesign

---

## 1. Current Dialog Hierarchy

```
ConfigDialog (main)
  ├─ ConnectionConfigDialog (Tab 0)
  ├─ AuthenticationConfigDialog (Tab 1)
  ├─ IpAccessControlDialog (Tab 2)
  ├─ DisplayInputConfigDialog (Tab 3)
  ├─ PermissionsConfigDialog (Tab 4)
  ├─ SessionConfigDialog (Tab 5)
  └─ LoggingConfigDialog (Tab 6)

Modal dialogs (launched from parent tabs):
  ├─ EditPortMappingDialog (launched from ConnectionConfigDialog)
  ├─ EditIpAccessRuleDialog (launched from IpAccessControlDialog)
  └─ ChangePasswordDialog (launched from AuthenticationConfigDialog)
```

### Key Observation:
- **ConfigDialog** is the singleton container hosting a **TabControl** widget
- Each tab is a full **BaseDialog** (modeless, positioned inside TabControl)
- **TabControl** manages visibility/positioning of child dialogs via `showTab(index)` / `hideTab(index)`
- Tab dialogs created in `ConfigDialog::onInitDialog()` and added to TabControl with `addTab(dialog, caption)`
- **Parent-child relationship**: Each tab dialog stores `BaseDialog *m_parentDialog` reference back to ConfigDialog
- No direct parent-child Win32 hierarchy; tabs are positioned via `MapWindowPoints()` inside TabControl's client area

---

## 2. Control Flow: UI → ServerConfig → Storage

### Apply Flow (ConfigDialog::onApplyButtonClick):
```
User clicks "Apply" or "OK"
  ↓
ConfigDialog::validateInput()  [all tabs validate]
  ↓
For each tab: tab->apply()  [writes to ServerConfig in-place]
  ↓
If m_reloadConfigCommand != NULL:
  m_reloadConfigCommand->execute()  [online mode - reload server]
Else:
  m_config->save()  [offline mode - write to INI/Registry]
  ↓
m_ctrlApplyButton.setEnabled(false)
```

### Data Model Path:
```
ServerConfig (singleton, Lockable, thread-safe)
  ├─ RFB port (int)
  ├─ HTTP port (int)
  ├─ PortMappingContainer (vector<PortMapping>)
  │   └─ PortMapping[i]
  │       ├─ port (int)
  │       ├─ rect (PortMappingRect = WxH+X+Y)
  │       └─ devicePath (StringStorage)
  ├─ IpAccessControl (vector<IpAccessRule>)
  ├─ GroupPermissionRules (vector<GroupPermissionRule>)
  │   └─ GroupPermissionRule[i]
  │       ├─ groupName (StringStorage)
  │       ├─ permissions (UINT32 flags)
  │       └─ priority (int)
  └─ [Auth, Display, Session, Logging settings...]

Storage:
  Configurator::load/save()  [INI file first, then Registry fallback]
    ├─ ConfigFile: tvnserver.ini (auto-detected in exe dir)
    ├─ Registry (online/service mode): HKLM\Software\TightVNC\Server
    └─ Registry (user/app mode): HKCU\Software\TightVNC\Server
```

---

## 3. Resource IDs (tvnserver/resource.h)

### Dialog Resource IDs:
| ID | Dialog Template | Class |
|---|---|---|
| **102** | IDD_CONFIG_ADMINISTRATION_PAGE | (legacy, unused) |
| **103** | IDD_CONFIG_SERVER_PAGE | (legacy, unused) |
| **104** | IDD_CONFIG_VIDEO_PAGE | (legacy, unused) |
| **105** | IDD_CONFIG_ACCESS_CONTROL_PAGE | (legacy, unused) |
| **106** | IDD_CONFIG_PORT_MAPPING_PAGE | (legacy, unused) |
| **107** | IDD_CONFIG | ConfigDialog (main container) |
| **136** | IDD_CONFIG_WIN_AUTH_PAGE | (legacy, unused) |
| **137** | IDD_CONFIG_CONNECTION_PAGE | ConnectionConfigDialog |
| **138** | IDD_CONFIG_AUTHENTICATION_PAGE | AuthenticationConfigDialog |
| **139** | IDD_CONFIG_DISPLAY_INPUT_PAGE | DisplayInputConfigDialog |
| **140** | IDD_CONFIG_PERMISSIONS_PAGE | PermissionsConfigDialog |
| **141** | IDD_CONFIG_SESSION_PAGE | SessionConfigDialog |
| **142** | IDD_CONFIG_LOGGING_PAGE | LoggingConfigDialog |

### Main Container Controls (IDD_CONFIG = 107):
| ID | Control | Name |
|---|---|---|
| **1003** | IDC_CONFIG_TAB | TabControl (hosts 7 tabs) |
| **1019** | IDC_APPLY | Apply button |
| IDOK | OK button |
| IDCANCEL | Cancel button |

### Connection Tab Controls (IDD_CONFIG_CONNECTION_PAGE = 137):
| ID | Control | Name | Purpose |
|---|---|---|---|
| **1045** | IDC_ACCEPT_RFB_CONNECTIONS | CheckBox | Enable/disable RFB |
| **1048** | IDC_RFB_PORT | TextBox | RFB port number |
| IDC_RFB_PORT_SPIN | SpinControl | RFB port spinner |
| **1046** | IDC_ACCEPT_HTTP_CONNECTIONS | CheckBox | Enable/disable HTTP |
| **1047** | IDC_HTTP_PORT | TextBox | HTTP port number |
| IDC_HTTP_PORT_SPIN | SpinControl | HTTP port spinner |
| **1039** | IDC_MAPPINGS | ListBox | Extra port mappings list |
| **1035** | IDC_ADD_PORT | Button | Add port mapping |
| **1036** | IDC_EDIT_PORT | Button | Edit port mapping |
| **1037** | IDC_REMOVE_PORT | Button | Remove port mapping |

### Authentication Tab Controls (IDD_CONFIG_AUTHENTICATION_PAGE = 138):
| ID | Control | Purpose |
|---|---|---|
| **1056** | IDC_USE_AUTHENTICATION | Enable VNC auth |
| **1062** | IDC_PRIMARY_PASSWORD | Set primary VNC password |
| **1022** | IDC_UNSET_PRIMARY_PASSWORD_BUTTON | Unset primary password |
| **1064** | IDC_VIEW_ONLY_PASSWORD | Set view-only VNC password |
| **1017** | IDC_UNSET_READONLY_PASSWORD_BUTTON | Unset view-only password |
| **1100** | IDC_WIN_AUTH_ENABLE | Enable Windows auth |
| **1101** | IDC_AUTH_MODE_COMBO | Auth mode selection |
| **1111** | IDC_DEFAULT_PERM_COMBO | Default permission for unmapped users |
| **1027** | IDC_USE_CONTROL_AUTH_CHECKBOX | Enable control interface auth |
| **1028** | IDC_CONTROL_PASSWORD_BUTTON | Set control password |
| **1031** | IDC_UNSET_CONTROL_PASWORD_BUTTON | Unset control password |

### Permissions Tab Controls (IDD_CONFIG_PERMISSIONS_PAGE = 140):
| ID | Control | Purpose |
|---|---|---|
| **1102** | IDC_GROUP_LIST | ListView of group permission rules |
| **1103** | IDC_GROUP_NAME_EDIT | Group name text input |
| **1104** | IDC_PERMISSION_COMBO | Permission selection (VIEW, KEYBOARD, MOUSE, CLIPBOARD, FILE_TRANSFER) |
| **1105** | IDC_PRIORITY_EDIT | Priority text input |
| **1115** | IDC_PRIORITY_SPIN | Priority spinner |
| **1106** | IDC_ADD_RULE_BUTTON | Add rule button |
| **1107** | IDC_EDIT_RULE_BUTTON | Edit rule button |
| **1108** | IDC_REMOVE_RULE_BUTTON | Remove rule button |
| **1109** | IDC_MOVE_UP_RULE_BUTTON | Move rule up |
| **1110** | IDC_MOVE_DOWN_RULE_BUTTON | Move rule down |
| **1116** | IDC_BROWSE_GROUPS_BUTTON | Browse groups via object picker |

### IP Access Control Tab Controls (IDD_CONFIG_ACCESS_CONTROL_PAGE = 105):
| ID | Control | Purpose |
|---|---|---|
| **1005** | IDC_IP_ACCESS_CONTROL_LIST | ListView of IP rules |
| **1026** | IDC_ADD_BUTTON | Add rule |
| **1029** | IDC_EDIT_BUTTON | Edit rule |
| **1030** | IDC_REMOVE_BUTTON | Remove rule |
| **1033** | IDC_MOVE_UP_BUTTON | Move up |
| **1034** | IDC_MOVE_DOWN_BUTTON | Move down |
| **1057** | IDC_ALLOW | Default to ALLOW radio |
| **1058** | IDC_DENY | Default to DENY radio |
| **1012** | IDC_ALLOW_LOOPBACK_CONNECTIONS | Allow loopback |
| **1054** | IDC_ALLOW_ONLY_LOOPBACK_CONNECTIONS | Loopback only |
| **1072** | IDC_QUERY_TIMEOUT_SPIN | Query timeout spinner |
| **1075** | IDC_IP_FOR_CHECK_EDIT | IP test field |
| **1076** | IDC_IP_CHECK_RESULT_LABEL | IP test result label |

### Display & Input Tab Controls (IDD_CONFIG_DISPLAY_INPUT_PAGE = 139):
| ID | Control | Purpose |
|---|---|---|
| **1025** | IDC_USE_D3D | Use Direct3D capture |
| **1082** | IDC_USE_MIRROR_DRIVER | Use mirror driver |
| **1052** | IDC_REMOVE_WALLPAPER | Remove wallpaper |
| **1069** | IDC_POLLING_INTERVAL | Polling interval (ms) |
| **1071** | IDC_POLLING_INTERVAL_SPIN | Polling interval spinner |
| **1023** | IDC_BLOCK_REMOTE_INPUT | Block remote input |
| **1038** | IDC_BLOCK_LOCAL_INPUT | Block local input |
| **1041** | IDC_LOCAL_INPUT_PRIORITY | Enable local input priority |
| **1040** | IDC_LOCAL_INPUT_PRIORITY_TIMEOUT | Local input priority timeout |
| **1009** | IDC_VIDEO_CLASS_NAMES | Video class names filter |
| **999** | IDC_VIDEO_RECTS | Video rectangles |
| **1018** | IDC_VIDEO_RECOGNITION_INTERVAL | Video recognition interval |
| **1074** | IDC_VIDEO_RECOGNITION_INTERVAL_SPIN | Recognition interval spinner |

### Session Tab Controls (IDD_CONFIG_SESSION_PAGE = 141):
| ID | Control | Purpose |
|---|---|---|
| **1063-1068** | IDC_SHARED_RADIO1-5 | Session sharing mode (5 radio buttons) |
| **1078, 1049, 1050** | IDC_DO_NOTHING, IDC_LOCK_WORKSTATION, IDC_LOGOFF_WORKSTATION | Disconnect action (3 radio buttons) |
| **1051** | IDC_ENABLE_FILE_TRANSFERS | Enable file transfers |
| **1001** | IDC_SHOW_TVNCONTROL_ICON_CHECKBOX | Show tray icon |
| **1097** | IDC_CONNECT_RDP_SESSION | Connect to RDP session |

### Logging Tab Controls (IDD_CONFIG_LOGGING_PAGE = 142):
| ID | Control | Purpose |
|---|---|---|
| **1053** | IDC_LOG_LEVEL | Log level |
| **1006** | IDC_LOG_LEVEL_SPIN | Log level spinner |
| **1014** | IDC_LOG_FILEPATH_EDIT | Log file path |
| **1077** | IDC_OPEN_LOG_FOLDER_BUTTON | Open log folder |
| **1020** | IDC_LOG_FOR_ALL_USERS | Log for all users |

---

## 4. GUI Framework Capabilities

### BaseDialog Class:
- Base class for all dialog windows (modeless & modal)
- Constructor: `BaseDialog(DWORD resourceId)` or `BaseDialog(const TCHAR *resourceName)`
- Key methods:
  - `create()` — creates non-modal window (not shown)
  - `show()` / `showModal()` — creates & shows window
  - `setParent(Control *ctrlParent)` — sets parent control
  - `setControlById(Control &control, DWORD id)` — binds control to HWND
  - `kill(int code)` — closes dialog
- Virtual methods for override: `onInitDialog()`, `onCommand()`, `onNotify()`, `onDestroy()`

### TabControl Class:
- Wrapper around Win32 Tab control (LVS_REPORT style)
- Key methods:
  - `addTab(BaseDialog *dialog, const TCHAR *caption)` — adds tab & stores dialog
  - `showTab(int index)` / `showTab(const BaseDialog *dialog)` — shows tab (hides others)
  - `getTab(int index)` / `getSelectedTabIndex()` — access tabs
  - `removeTab(int index)` / `deleteAllTabs()` — remove tabs
  - `adjustRect(RECT *rect)` — gets usable tab client area (for positioning children)
- Sends **TCN_SELCHANGE** / **TCN_SELCHANGING** notifications

### ListView Class:
- Wrapper around Win32 ListView (LVS_REPORT style)
- Key methods:
  - `addColumn(int index, const TCHAR *caption, int width, int fmt)` — add column header
  - `addItem(int index, const TCHAR *caption, LPARAM tag)` — add row
  - `setSubItemText(int index, int subIndex, const TCHAR *caption)` — update cell
  - `getSelectedIndex()` / `selectItem(int index)` — selection
  - `removeItem(int i)` / `clear()` — remove rows
  - `allowMultiSelection(bool allow)` — enable multi-select
  - `setFullRowSelectStyle(bool fullRowSelect)` — full-row highlight
- Sends **LVN_ITEMCHANGED** (selection) notifications

### TextBox, CheckBox, SpinControl, ComboBox:
- Simple wrappers around Win32 controls
- TextBox: `getText()`, `setText()`, `setSignedInt()`, `getSignedInt()`
- CheckBox: `check()`, `uncheck()`, `isChecked()`
- SpinControl: `setBuddy(TextBox *)`, `setRange32()`, `setAccel()`
- ComboBox: `addString()`, `getCurSel()`, `setData()`, `getData()`

---

## 5. Tab Dialog Pattern (Typical Implementation)

### Header (e.g., ConnectionConfigDialog.h):
```cpp
class ConnectionConfigDialog : public BaseDialog {
public:
  ConnectionConfigDialog();
  void setParentDialog(BaseDialog *dialog);  // Store parent ref

  virtual BOOL onInitDialog();
  virtual BOOL onCommand(UINT controlID, UINT notificationID);
  virtual BOOL onNotify(UINT controlID, LPARAM data);

  bool validateInput();   // Validate before apply
  void updateUI();        // Load ServerConfig → UI
  void apply();           // Save UI → ServerConfig

private:
  void initControls();    // Bind HWNDs to control wrappers
  void onAddButtonClick(); // Handler methods

protected:
  BaseDialog *m_parent;   // Ref to ConfigDialog
  TextBox m_port;         // Control wrappers
  CheckBox m_accept;
  // ...
};
```

### Implementation (e.g., ConnectionConfigDialog.cpp):
```cpp
BOOL ConnectionConfigDialog::onInitDialog() {
  initControls();
  updateUI();
  return TRUE;
}

void ConnectionConfigDialog::initControls() {
  HWND dialogHwnd = m_ctrlThis.getWindow();
  m_port.setWindow(GetDlgItem(dialogHwnd, IDC_RFB_PORT));
  // ...
}

void ConnectionConfigDialog::updateUI() {
  ServerConfig *config = Configurator::getInstance()->getServerConfig();
  m_port.setSignedInt(config->getRfbPort());
  // ...
}

void ConnectionConfigDialog::apply() {
  ServerConfig *config = Configurator::getInstance()->getServerConfig();
  config->setRfbPort(m_port.getSignedInt());
  // ...
}

BOOL ConnectionConfigDialog::onCommand(UINT controlID, UINT notificationID) {
  switch (controlID) {
    case IDC_RFB_PORT:
      if (notificationID == EN_CHANGE) onRfbPortUpdate();
      break;
    case IDC_ADD_PORT:
      if (notificationID == BN_CLICKED) onAddButtonClick();
      break;
  }
  return TRUE;
}
```

---

## 6. Modal Dialog Pattern (EditPortMappingDialog)

### Header:
```cpp
class EditPortMappingDialog : public BaseDialog {
public:
  typedef enum { Add = 0x0, Edit = 0x1 } DialogType;

  EditPortMappingDialog(DialogType dlgType);
  void setMapping(PortMapping *mapping);  // Pass data in

  virtual BOOL onInitDialog();
  virtual BOOL onCommand(UINT controlID, UINT notificationID);

private:
  void onOkButtonClick();
  void onCancelButtonClick();

protected:
  TextBox m_port;
  TextBox m_geometry;
  ComboBox m_display;
  PortMapping *m_mapping;  // Data to edit (not owned)
};
```

### Usage from Parent:
```cpp
// In ConnectionConfigDialog::onEditButtonClick():
int sel = m_mappingsListBox.getSelectedIndex();
EditPortMappingDialog editDlg(EditPortMappingDialog::Edit);
editDlg.setMapping(m_extraPorts->at(sel));
if (editDlg.showModal() == IDOK) {
  // m_mapping was mutated in-place by editDlg
  updateUI();  // Refresh listbox
}
```

**Key Pattern**: Modal dialog receives **pointer to data**, mutates it in-place, caller handles refresh.

---

## 7. Key Patterns for New Dialog Implementation

### Tab Dialog Checklist:
1. Create `.h` file in `wsconfig-lib/`
2. Add `IDD_CONFIG_*_PAGE` resource ID to `tvnserver/resource.h`
3. Create dialog template in `tvnserver/tvnserver.rc` (UTF-16LE)
4. Inherit from `BaseDialog`, store `BaseDialog *m_parentDialog`
5. Implement: `onInitDialog()`, `onCommand()`, `onNotify()`, `initControls()`, `updateUI()`, `apply()`, `validateInput()`
6. In `ConfigDialog::onInitDialog()`: create, add to TabControl, hide initially
7. Instantiate in `ConfigDialog` as member: `MyConfigDialog m_myDialog;`

### Modal Dialog Checklist:
1. Create `.h` file in `wsconfig-lib/`
2. Add resource ID to `tvnserver/resource.h`
3. Create template in `tvnserver.rc`
4. Inherit from `BaseDialog`
5. Add setters for input data: `setData(const MyData *data)`
6. Implement: `onInitDialog()`, `onCommand()`, `onOkButtonClick()`, `onCancelButtonClick()`
7. Mutate input data in-place in OK handler
8. Call `kill(IDOK)` / `kill(IDCANCEL)` to close

---

## 8. EditPortMappingDialog Deep Dive

### Current Features:
- **Port field** (IDC_PORT_EDIT): port number
- **Geometry field** (IDC_GEOMETRY_EDIT): WxH+X+Y format (e.g., "1920x1080+0+0")
- **Display combo** (IDC_DISPLAY_COMBO): "All Displays" or specific device (\\.\DISPLAY1, etc.)
- PopulateDisplayCombo(): enumerates system displays using WindowsDisplays API
- Validates user input before OK

### Data Flow:
```
setMapping(PortMapping *pm)  [set pointer]
  ↓
showModal()  [user fills fields]
  ↓
onOkButtonClick()  [validate, then mutate *pm in-place]
  ↓
kill(IDOK)  [close modal, return IDOK]
  ↓
Caller sees mutated PortMapping
```

### Per-Port Config Implication:
Current mapping stores **only**: port + geometry + device. No auth, permissions, or display settings. To support per-port config, need to extend PortMapping or create wrapper.

---

## 9. Config Flow in Action (ConnectionConfigDialog Example)

### Tab initialization:
```cpp
ConfigDialog::onInitDialog() {
  m_connectionDialog.setParent(&m_ctrlThis);
  m_connectionDialog.setParentDialog(this);
  m_connectionDialog.create();  // Create HWND, but don't show
  moveDialogToTabControl(&m_connectionDialog);  // Position inside TabControl
  m_tabControl.addTab(&m_connectionDialog, _T("Connection"));
}
```

### Tab visibility switching:
```cpp
ConfigDialog::onTabChange() {
  int idx = m_tabControl.getSelectedTabIndex();
  Tab *tab = m_tabControl.getTab(idx);
  tab->setVisible(true);
}

ConfigDialog::onTabChanging() {
  int idx = m_tabControl.getSelectedTabIndex();
  Tab *tab = m_tabControl.getTab(idx);
  tab->setVisible(false);
}
```

### Apply (all tabs save):
```cpp
ConfigDialog::onApplyButtonClick() {
  if (!validateInput()) return;  // Each tab validates

  m_connectionDialog.apply();     // Write to ServerConfig
  m_authenticationDialog.apply();
  m_permissionsDialog.apply();
  // ... all tabs apply

  m_config->save();  // Write to INI/Registry
  m_ctrlApplyButton.setEnabled(false);
}
```

### List editing pattern (Port Mappings):
```cpp
ConnectionConfigDialog::onAddButtonClick() {
  PortMapping newMapping;
  EditPortMappingDialog editDlg(EditPortMappingDialog::Add);
  editDlg.setMapping(&newMapping);

  if (editDlg.showModal() == IDOK) {
    m_extraPorts->pushBack(newMapping);  // Mutated in-place
    updateUI();  // Refresh listbox
  }
}
```

---

## 10. Design Suggestions for Per-Port Configuration

### Option A: Tree-Based Master-Detail (Recommended)
**Concept**: Replace flat port list with tree structure + detail pane.
- **Left pane**: Tree view
  - "Main Server" (RFB + HTTP ports)
  - "Extra Ports" (folder)
    - ":5900 (Display 1)" (port mapping 1)
    - ":5901 (Display 2)" (port mapping 2)
- **Right pane**: Detail panel (tabs or sections)
  - Port/geometry (always present)
  - Per-port auth mode (VNC/Windows/Both)
  - Per-port permission rules
  - Per-port IP access rules (optional)

**Pros**: Clear hierarchy, per-port settings obvious
**Cons**: Requires TreeView control, more complex state management

### Option B: Per-Port Dialog Tabs
**Concept**: Each port mapping gets its own dialog within EditPortMappingDialog.
- EditPortMappingDialog becomes a tab container:
  - "Basic" tab: port, geometry, device
  - "Authentication" tab: VNC/Windows auth settings
  - "Permissions" tab: group rules (per-port copy)
  - "IP Access" tab: per-port IP rules (optional)

**Pros**: Modular, reuses existing tab pattern
**Cons**: Modal dialog gets complex; duplication of auth/permission logic

### Option C: Sidebar in Main Dialog
**Concept**: Add collapsible "Port Configuration" section to Connection tab.
- Keep existing layout
- Add expandable section below port list
- When port selected, section shows detail form for that port
- Bind controls to selected port dynamically

**Pros**: No new container types needed
**Cons**: Tight space in Connection tab, complex binding

### Recommendation for Architecture:
- **Option A (Tree-based)** is cleanest, most scalable
- Requires: new TreeView handling + Tab abstraction
- Path: Extend ConnectionConfigDialog to use TreeView + detail panel instead of ListBox
- Can reuse PermissionsConfigDialog logic for per-port permission rules
- Create `PerPortConfigPanel` (or similar) to encapsulate per-port auth/perm settings

### Data Model Extension:
```cpp
struct PortMapping {
  int port;
  PortMappingRect rect;
  StringStorage devicePath;

  // NEW: Per-port config
  AuthMode authMode;  // AUTH_VNC_ONLY, AUTH_WINDOWS_ONLY, AUTH_BOTH
  std::vector<GroupPermissionRule> portRules;  // Per-port permission rules
  std::vector<IpAccessRule> portAccessRules;   // Per-port IP rules (optional)
  bool useDefaultRules;  // If true, fallback to ServerConfig's global rules
};
```

### UI Precedent:
- Similar to Visual Studio project properties (Configuration tree + detail pane)
- Similar to IIS web site/application management (hierarchy + properties)

---

## 11. ListView Multi-Column Example (Permissions Tab)

### ListView Setup (from PermissionsConfigDialog):
```cpp
void PermissionsConfigDialog::initControls() {
  HWND hwnd = m_ctrlThis.getWindow();
  m_ruleList.setWindow(GetDlgItem(hwnd, IDC_GROUP_LIST));

  // Configure columns
  m_ruleList.addColumn(0, _T("Group Name"), 200);
  m_ruleList.addColumn(1, _T("Permissions"), 150);
  m_ruleList.addColumn(2, _T("Priority"), 80);
  m_ruleList.setFullRowSelectStyle(true);
}

void PermissionsConfigDialog::refreshGroupList() {
  m_ruleList.clear();

  for (int i = 0; i < (int)m_rules.size(); i++) {
    setListViewItemText(i, m_rules[i]);
  }
}

void PermissionsConfigDialog::setListViewItemText(int index,
                                                  const GroupPermissionRule &rule) {
  StringStorage groupName = rule.getGroupName();
  StringStorage permStr = permissionToString(rule.getPermissions());
  StringStorage priorityStr;
  priorityStr.format(_T("%d"), rule.getPriority());

  m_ruleList.addItem(index, groupName.getString());
  m_ruleList.setSubItemText(index, 1, permStr.getString());
  m_ruleList.setSubItemText(index, 2, priorityStr.getString());
}
```

**Pattern**: Add rows with first column, then fill subitems. Works for any N-column report view.

---

## 12. Unresolved Questions & Considerations

### Q1: Per-Port Permission Rules Scope?
Should per-port permission rules **override** or **combine with** global rules?
- Recommendation: Per-port rules take precedence (simpler mental model)

### Q2: EditPortMappingDialog Modal vs. In-Place Editing?
Current: Modal dialog. Alternatives?
- Could make EditPortMappingDialog modeless (dockable side panel)
- Could integrate into tree-based design as detail pane

### Q3: Localization?
All hardcoded strings in current code use `StringTable::getString(IDS_*)`. Need to ensure resource strings updated for new per-port options.

### Q4: RC File UTF-16LE Encoding?
tvnserver.rc is UTF-16LE. Visual Studio resource editor should handle transparently, but manual editing risky. Use resource editor for new templates.

### Q5: Persistence Format?
How to serialize per-port config in INI?
- Current: `EXTRA_PORTS=5900:1920x1080+0+0, 5901:1024x768+1920+0|\\.\\DISPLAY2`
- Extended: Add JSON or key-value section per port? Break compatibility?

### Q6: UI Testing?
No unit tests in codebase. Per-port config adds complexity. Recommend manual test checklist:
- Add/edit/remove ports
- Per-port auth mode switching
- Permissions inheritance (global vs. per-port)
- Apply/Cancel scenarios
- Config save/reload (INI + Registry)

---

## Summary

**Architecture**: ConfigDialog (container) → 7 TabControl tabs → each tab is BaseDialog
**Config Flow**: UI → ServerConfig (thread-safe) → Configurator::save() → INI/Registry
**Pattern**: Tab dialogs expose `updateUI()` / `apply()` / `validateInput()`; modals mutate data in-place
**GUI Toolkit**: Win32 wrappers (TextBox, CheckBox, ListView, TabControl, etc.)
**Extensibility**: New tabs follow standard pattern; per-port config best served by tree-based redesign (Option A)

