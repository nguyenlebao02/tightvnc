# Per-Port VNC Configuration — Implementation Plan

**Date:** 2026-03-29
**Status:** Draft
**Scope:** Transform TightVNC v2.8.81 from single-global-config to per-port independent VNC servers

## Goal

Every VNC port runs as an independent server with its own auth mode, passwords, group rules, IP access control, and display configuration. No distinction between "main" and "extra" ports.

## Architecture Decision

**Hybrid Config Lookup (Option C):**
- New `PortConfig` class holds per-port auth/permission/display settings
- `ServerConfig` stores `vector<PortConfig>` replacing main+extra port split
- Port context passed through: RfbServer -> RfbClientManager -> RfbClient -> RfbInitializer
- RfbInitializer looks up per-port config instead of global ServerConfig
- Backward-compatible: old INI/registry auto-migrated on first load

## Phases

| # | Phase | Status | Files Changed |
|---|-------|--------|---------------|
| 1 | [Create PortConfig data model](phase-01-create-port-config-model.md) | **Done** | 2 new, 1 modified |
| 2 | [Unify port management in ServerConfig](phase-02-unify-port-management.md) | **Done** | 4 modified |
| 3 | [Configurator load/save per-port config](phase-03-configurator-per-port-storage.md) | **Done** | 2 modified |
| 4 | [Pass port context through connection pipeline](phase-04-port-context-pipeline.md) | **Done** | 7 modified |
| 5 | [RfbInitializer uses per-port config](phase-05-per-port-auth-in-initializer.md) | **Done** | 1 modified |
| 6 | [UI redesign — port list with per-port panels](phase-06-ui-redesign-port-list.md) | Pending | ~10 modified/new |
| 7 | [Migration — old config to per-port format](phase-07-config-migration.md) | **Done** | 2 modified |

## Key Dependencies

- Phase 2 depends on Phase 1 (PortConfig class must exist)
- Phase 3 depends on Phase 2 (ServerConfig API must be finalized)
- Phase 4 depends on Phase 1 (needs PortConfig pointer to pass)
- Phase 5 depends on Phase 4 (needs port context in RfbInitializer)
- Phase 6 depends on Phases 1-3 (needs config model stable)
- Phase 7 depends on Phase 3 (needs new save/load format defined)

## Build Verification

After each phase: `MSBuild tightvnc2019.sln -p:Configuration=Release -p:Platform=x86 -m -verbosity:minimal` must produce 0 errors.

## Risk Summary

- **HIGH:** Phase 4 thread-safety — PortConfig pointer lifetime must outlive RfbClient
- **MEDIUM:** Phase 7 migration — must handle both old and new INI formats
- **MEDIUM:** Phase 6 UI — Win32 dialog template changes in UTF-16LE .rc file
- **LOW:** Phase 5 auth logic — straightforward replacement of global config reads

## Constraints

- YAGNI/KISS/DRY
- Each new code file < 200 lines
- Backward compatible with existing INI/registry
- Follow existing patterns: AutoLock, StringStorage, TCHAR
- Build with 0 errors after each phase
