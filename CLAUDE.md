# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

BaoVNC — a C++ Win32 VNC server/viewer forked from TightVNC v2.8.81. Extensions beyond upstream: Windows Authentication with AD group-based permissions, per-port configuration (auth rules, permissions, max connections per user), 7-tab configuration UI, INI file config support. Licensed under GPLv2.

## Build

**Solution:** `tightvnc2019.sln` (Visual Studio 2019+ / VS2026 v18, toolset v145, x86)

```bash
cd "E:/Phat trien VNC server/tightvnc-source"
"C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/amd64/MSBuild.exe" tightvnc2019.sln -p:Configuration=Release -p:Platform=x86 -m -verbosity:minimal
```

Output: `Release/x86/tvnserver.exe`

If build fails with LNK1104 (can't open tvnserver.exe), kill running instance first:
```bash
taskkill.exe //F //IM tvnserver.exe
```

**Testing config UI:** `Release\x86\tvnserver.exe -configapp`

There are no unit tests. Verification is done by building (0 errors) and manual testing.

## Architecture

### Core Components

- **`tvnserver-app/`** — Server application entry point and lifecycle
  - `TvnServer` — Singleton coordinating all subsystems: RfbServer(s), HttpServer, ControlServer, Configurator, ZombieKiller
  - `RfbServer` — TCP listener accepting connections. Holds a `PortConfig` snapshot for per-port auth. Passes connections to `RfbClientManager`
  - `ExtraRfbServers` — Manages additional RFB listeners on extra ports, each with its own `PortConfig`
  - `RfbClientManager` — Manages client sessions, enforces per-user connection limits, creates `RfbClient` threads
  - `NamingDefs` — Central branding constants (product name "BaoVNC", registry paths `Software\BaoVNC\*`, pipe names, service name)

- **`rfb-sconn/`** — RFB server-side connection protocol
  - `RfbInitializer` — Protocol handshake (version, auth, init). Auth: EXTERNAL (type 130, Windows Authentication) or NONE (loopback). Stores `m_authenticatedUsername` after Windows auth
  - `RfbClient` — Per-client thread: input handling, framebuffer updates, permission enforcement. Propagates username + PortConfig from initializer
  - `CapContainer` — Capability negotiation (TIGHT protocol extension)

- **`server-config-lib/`** — Configuration data model
  - `ServerConfig` — Central config: ports, passwords, auth mode, permissions, display, polling, etc. Thread-safe (inherits `Lockable`). Holds `vector<PortConfig>` for per-port settings
  - `Configurator` — Singleton. Loads/saves from INI file (`tvnserver.ini` in exe dir) or Windows Registry (fallback). Saves/loads per-port config keys (`Port%u.MaxConnectionsPerUser`, `Port%u.WinAuthGroupRules`, etc.)
  - `PortConfig` — Per-port definition: port number, geometry rect, device path, default permissions, group rules, max connections per user
  - `ClientPermissions` — Bitmask: VIEW(0x01), KEYBOARD(0x02), MOUSE(0x04), CLIPBOARD(0x08), FILE_TRANSFER(0x10), DENY(0x80000000)
  - `GroupPermissionRule` — Maps Windows group name → permission flags + priority

- **`win-auth-lib/`** — Windows Authentication implementation
  - `WinAuthenticator` — LogonUser API → GetTokenInformation(TokenGroups) → LookupAccountSid → match against port's group rules → resolve permissions. Rejects Guest accounts

- **`wsconfig-lib/`** — Configuration GUI (Win32 dialogs)
  - `ConfigDialog` — Main tabbed dialog hosting 5 tabs via `TabControl`. Manages per-port config list and port selector combo
  - Tab dialogs: `ConnectionConfigDialog` (ports, IP filters), `PortSettingsConfigDialog` (per-port: group rules, permissions, max conn/user, control auth), `DisplayInputConfigDialog`, `SessionConfigDialog` (file transfer, tray icon, RDP — sharing is hard-coded to Always Shared), `LoggingConfigDialog`
  - `EditPortMappingDialog` — Modal for adding/editing port mappings
  - `AdministrationConfigDialog` — Legacy single-page admin dialog (for tvncontrol). Also hard-codes Always Shared

- **`tvnserver/`** — Resources (`.rc` file, `resource.h`), dialog templates, string tables, icons

### Support Libraries

| Library | Purpose |
|---------|---------|
| `gui/` | Win32 control wrappers (BaseDialog, TextBox, CheckBox, ComboBox, ListView, SpinControl, TabControl) |
| `config-lib/` | Abstract SettingsManager + IniFileSettingsManager + RegistrySettingsManager |
| `rfb/` | RFB protocol constants, pixel format, encoding defs, AuthDefs |
| `network/` | TCP socket wrappers, TcpServer |
| `io-lib/` | Channel, DataInputStream/DataOutputStream |
| `thread/` | Thread, LocalMutex, AutoLock, ZombieKiller |
| `desktop/` | Screen capture (D3D Desktop Duplication, mirror driver, polling), input injection |
| `fb-update-sender/` | Framebuffer diff + encoding + sending to clients |
| `ft-server-lib/` / `ft-common/` | File transfer protocol (TightVNC extension) |
| `log-writer/` / `log-server/` | Logging infrastructure |
| `util/` | StringStorage, StringParser, Singleton, CommonHeader |
| `region/` | Rect, Region, Dimension geometry primitives |

### Config Storage

Config loads from INI file first (if `tvnserver.ini` exists in exe dir), otherwise falls back to Windows Registry under `HKLM\Software\BaoVNC\Server` (service) or `HKCU\...` (application). Per-port config stored as `Port0.*`, `Port1.*`, etc.

### Authentication Flow

1. RFB handshake in `RfbInitializer::authPhase()` — session sharing is hard-coded to Always Shared
2. EXTERNAL (Windows) auth (type 130): client sends `domain\username` + password → `WinAuthenticator::authenticate()` → LogonUser → enumerate group SIDs → match port's `GroupPermissionRule`s by priority → resolve `ClientPermissions`
3. Username stored in `RfbInitializer::m_authenticatedUsername` → copied to `RfbClient::m_authenticatedUsername`
4. `RfbClientManager::onClientAuth()` enforces per-user connection limit via `PortConfig::getMaxConnectionsPerUser()`
5. Loopback/no-auth connections receive `PERM_FULL_CONTROL`

### GUI Dialog Resource Pattern

All dialog templates defined in `tvnserver/tvnserver.rc` (UTF-16LE encoded). Resource IDs in `tvnserver/resource.h` (also UTF-16LE). Each dialog class in `wsconfig-lib/` maps to a `IDD_CONFIG_*_PAGE` template. `BaseDialog` provides standard Win32 dialog lifecycle (`onInitDialog`, `onCommand`, `onNotify`, `onDestroy`).

## Key Conventions

- All strings use `StringStorage` (TCHAR-based wrapper). Use `getString()` for raw pointer, `setString()` / `format()` to set.
- Thread safety via `AutoLock` + `LocalMutex` (RAII pattern throughout ServerConfig).
- GUI controls wrap HWND handles. Pattern: `m_control.setWindow(GetDlgItem(hwnd, IDC_...))` in `initControls()`.
- **UTF-16LE resource files** — `tvnserver.rc` and `resource.h` are UTF-16LE encoded. Standard text tools may show garbled output. Edit via Visual Studio resource editor or use Python scripts with `encoding='utf-16-le'`.
- Session sharing is hard-coded to "Always Shared" + "Do Nothing" on disconnect. The old sharing radio buttons and disconnect action controls have been removed from UI. Config keys still exist in Configurator for backward compat but are overridden by dialogs.
- Port mapping serialization: `port:WxH+X+Y` (legacy) or `port:WxH+X+Y|devicePath` (with display binding).

## Design Decisions

- **Hybrid VNC+TIGHT auth** — Server offers both VNC(2) standard password auth (for RealVNC, UltraVNC, etc.) and TIGHT(16) Windows auth (for TIGHT-capable viewers like TigerVNC, TurboVNC, tvnviewer). VNC auth grants full control; TIGHT/EXTERNAL auth resolves per-user AD group permissions. Protocol 3.3 limited to NONE auth (loopback only).
- **Per-port config model** — Each port has its own `PortConfig` with group rules, default permissions, and max connections per user. Stored in `ServerConfig::m_portConfigs` vector, serialized per-port in Configurator.
- **PortConfig passed to RfbServer** — `RfbServer` constructor takes optional `const PortConfig *portConfig`. The config is copied into `m_portConfig` member and propagated to each `RfbClient` via `RfbClientManager`.
