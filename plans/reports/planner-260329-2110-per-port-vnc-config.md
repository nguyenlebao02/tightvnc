# Planner Report: Per-Port VNC Configuration

**Date:** 2026-03-29
**Plan:** `plans/260329-2101-per-port-vnc-config/`
**Status:** Complete — 7 phases planned

## Summary

Created comprehensive implementation plan to transform TightVNC v2.8.81 from single-global-config to per-port independent VNC servers. Each port gets its own auth mode, VNC passwords, Windows auth group rules, IP access control, and display configuration.

## Architecture Decision

**Hybrid Config Lookup** — new `PortConfig` value class holds per-port settings; stored as `vector<PortConfig>` in `ServerConfig`; passed by value through RfbServer → RfbClientManager → RfbClient → RfbInitializer pipeline. Thread-safe via copy semantics.

## Phase Summary

| # | Phase | Effort | Risk | Files |
|---|-------|--------|------|-------|
| 1 | PortConfig data model | Small | LOW | 2 new, 1 mod |
| 2 | Unify port management | Medium | MED | 4 modified |
| 3 | Configurator per-port storage | Medium | MED | 2 modified |
| 4 | Port context pipeline | Medium | HIGH (thread safety) | 9 modified |
| 5 | Per-port auth in RfbInitializer | Medium | MED | 2 modified |
| 6 | UI redesign — port list | Large | HIGH (.rc editing) | ~10 new/mod |
| 7 | Config migration | Small | LOW | 2 modified |

## Key Source Files Analyzed

- `server-config-lib/PortMapping.h/.cpp` — current port+rect+device structure (63+167 lines)
- `server-config-lib/ServerConfig.h` — global auth fields at lines 361-363, 442-444 (490 lines)
- `tvnserver-app/TvnServer.cpp` — main+extra server startup at lines 124-125, 371-391 (449 lines)
- `tvnserver-app/ExtraRfbServers.cpp` — extra port startup at lines 109-147 (167 lines)
- `tvnserver-app/RfbServer.cpp` — onAcceptConnection at lines 57-91, passes ViewPort only (92 lines)
- `tvnserver-app/RfbClientManager.cpp` — addNewConnection at lines 374-406, no port context (420+ lines)
- `rfb-sconn/RfbClient.cpp` — execute() at lines 169-254, creates RfbInitializer without port info
- `rfb-sconn/RfbInitializer.cpp` — doTightAuth/doVncAuth/doWinAuth all read global ServerConfig (527 lines)
- `wsconfig-lib/ConfigDialog.cpp` — 7-tab flat layout, all tabs operate on global config (343 lines)
- `server-config-lib/Configurator.cpp` — existing savePortConfig/loadPortConfig at lines 981-1044

## Critical Path

Phase 1 → 2 → 3 → 7 (data model + storage + migration)
Phase 1 → 4 → 5 (pipeline + auth activation)
Phase 1+2+3 → 6 (UI needs stable model)

Phases 4+5 and 3+7 can proceed in parallel once Phase 1+2 are done.

## Highest Risk Items

1. **Phase 4 thread safety** — PortConfig must be copied (not referenced) at each pipeline boundary. All parameters use value semantics with `const PortConfig*` → copy on store.
2. **Phase 6 .rc file editing** — UTF-16LE dialog templates. Use Visual Studio resource editor, not text editing.
3. **Phase 7 IpAccessControl deep copy** — IpAccessRule objects are heap-allocated; need proper clone in migration.

## Deliverables

- `plans/260329-2101-per-port-vnc-config/plan.md` — Overview (67 lines)
- `phase-01-create-port-config-model.md` — PortConfig class design
- `phase-02-unify-port-management.md` — Eliminate main/extra distinction
- `phase-03-configurator-per-port-storage.md` — INI/Registry per-port format
- `phase-04-port-context-pipeline.md` — Thread PortConfig through connection chain
- `phase-05-per-port-auth-in-initializer.md` — Replace global config reads with per-port
- `phase-06-ui-redesign-port-list.md` — Port-centric UI with per-port tabs
- `phase-07-config-migration.md` — Auto-convert old global config

## Unresolved Questions

1. **File transfer per-port?** Current plan keeps `isFileTransfersEnabled()` as global. Could be per-port but increases complexity. Recommend global for now (YAGNI).
2. **Control interface password per-port?** Control interface auth is for the config UI pipe, not RFB. Recommend keeping global.
3. **Shared desktop per-port?** Currently all clients share one `WinDesktop`. Per-port desktop isolation would require major refactor. Out of scope — all ports share the desktop, just capture different rects.
4. **HTTP server per-port?** Current plan keeps HTTP port as global. A per-port HTTP server would be a separate feature.
