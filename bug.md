# Known Bugs / Issues

## 1) `.ynv` import path lacks defensive validation
- Status: resolved
- Location: `src/application/ANavContext.cpp` (`LoadNavmesh`, `ANavmeshRenderData::CreateNavResources`)
- Problem: import result from `librdr3::ImportYnv(...)` was previously used without null/empty validation.
- Impact: malformed/unsupported `.ynv` could crash or fail unpredictably.
- Resolution:
  - Null/empty validation is now performed before GL buffer upload.
  - Failed imports are rejected safely and exposed through status/error text in the Properties panel.

## 2) No full navmesh session management (partial fix)
- Status: resolved
- Location: `src/application/ANavContext.cpp`, UI layer
- Problem: navmesh management previously only supported clear-all.
- Resolution: Properties panel now supports list/select and per-navmesh remove.

## 6) Navmesh flag edits are not persisted via UI yet
- Status: open
- Location: `src/application/AGatorContext.cpp`, export flow
- Problem: `ENavMeshFlags` and `EPolygonFlags` can be edited in-memory, but there is no direct "save edited navmesh" UI action yet.
- Impact: users can edit flags but cannot persist changes from the current UI workflow.
- Suggested fix: add save/export command for selected navmesh using `librdr3::ExportYnv`.

## 7) Polygon/link deep editing is still limited
- Status: open
- Location: navmesh editor UI
- Problem: only high-level bit flags are currently editable; no link/adjacency/point-type editors are present.
- Impact: advanced navmesh workflows still require external tooling.
- Suggested fix: add dedicated navmesh inspector panels for polygon metadata and links.

## 3) `.ydr` load not fully integrated in visible render workflow
- Status: open
- Location: `src/application/ADrawableContext.cpp` + render pipeline integration
- Problem: load path exists, but visible editing/render integration remains incomplete.
- Impact: users may load `.ydr` and not see expected scene output.
- Suggested fix: connect drawable context into main viewport render flow and add status feedback.

## 4) No unsaved-changes protection for track edits
- Status: open
- Location: editor state management (`ATrackContext`/`AGatorContext`)
- Problem: edited track data can be lost on close without warning.
- Impact: accidental data loss.
- Suggested fix: dirty-flag + save/discard/cancel prompt on close/new load.

## 5) Parser robustness still limited for malformed track/nav inputs
- Status: open
- Location: `src/tracks/UTrackPoint.cpp`, track/xml load paths, nav load path
- Problem: several parse paths rely on optimistic token/format assumptions.
- Impact: unexpected file variants may fail silently or partially deserialize.
- Suggested fix: centralize parse validation and error reporting; add smoke tests.

## Recently Resolved

### A) Uninitialized frame timing on first loop iteration
- Status: resolved
- Location: `src/application/AApplication.cpp`

### B) Mouse wheel sign handling
- Status: resolved
- Location: `src/application/AInput.cpp`

### C) Heavy framebuffer recreation during interaction
- Status: resolved
- Location: `src/ui/UViewport.cpp`, `src/ui/UViewportPicker.cpp`, `src/application/AGatorContext.cpp`

### D) `File -> Close` had no shutdown behavior
- Status: resolved
- Location: `src/application/AGatorContext.cpp`

### E) Missing viewport drag box-selection / improved selection behavior
- Status: resolved
- Location: `src/application/ATrackContext.cpp`

### F) `.ynv` defensive load checks and status reporting
- Status: resolved
- Location: `src/application/ANavContext.cpp`, `src/application/AGatorContext.cpp`
