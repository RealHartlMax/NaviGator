# TODO

## High Priority

- [x] Add robust `.ynv` import validation in `ANavContext::LoadNavmesh` (null/empty checks before GL upload).
- [ ] Add user-facing error feedback for failed `.ynv`/`.xml`/`.dat` loads (popup/toast/log panel).
- [ ] Add unsaved-changes tracking for railroad edits and exit/save warning.
- [x] Add navmesh management UI (show loaded `.ynv`, remove selected, clear all).
- [ ] Add navmesh save/export workflow after flag edits.

## Track Editor

- [x] Drag box-selection for nodes in viewport.
- [x] Selection modifier behavior (Shift add, Ctrl toggle, none replace).
- [x] Multi-node interpolation support via curve neighborhood recalculation.
- [x] Selection mode options (fully enclosed vs touch radius).
- [x] Viewport shortcut hints overlay.
- [ ] Add per-track/node visibility filters in editor UI.
- [ ] Add optional snap/grid constraints for node translation.

## Stability And Validation

- [x] Initialize first-frame delta-time safely.
- [x] Avoid redundant viewport/picker framebuffer recreation.
- [x] Wire `File -> Close` to application shutdown.
- [ ] Harden parser error paths (`UTrackPoint::LoadPoint`, XML loading, `.ynv` import path).
- [ ] Ensure safe behavior for malformed or partial input files.

## Tooling

- [ ] Add CI for CMake configure + build (Windows/Linux).
- [ ] Add automated smoke tests for:
  - [ ] `.dat` load/save roundtrip
  - [ ] `traintracks.xml` parse/serialize
  - [ ] `.ynv` import validation path
- [ ] Add developer guide for debugging shader and picker failures.

## Notes

- Navmesh status and management UI is available in Properties (list, select, remove selected, clear all, last info/error).
- Navmesh flags (`ENavMeshFlags`) and polygon flags (`EPolygonFlags`) are editable in-memory from Properties.
