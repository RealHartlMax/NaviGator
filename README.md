# NaviGator

NaviGator is a C++17 desktop OpenGL editor/viewer for Rockstar-style railroad and navmesh assets.

The current codebase supports:
- Loading and rendering `.ynv` navmeshes
- Loading, editing, and saving railroad tracks (`traintracks.xml` + `.dat`)
- Node picking, multi-selection, box selection, and gizmo-based transforms
- Curve node editing with automatic bezier handle recalculation

## Project Status

This project is feature-usable but still work-in-progress. There is no automated CI/test suite yet.

## Features

### Track editing
- Single-node and multi-node selection
- Box selection by drag in viewport
- Selection modifiers:
  - No modifier: replace selection
  - Shift: additive selection
  - Ctrl: toggle selection
- Node actions: add before/after, delete
- Junction assignment/clear
- Curve mode toggle and handle recalculation
- Multi-node curve/tunnel batch edit
- Undo/redo for editor operations

### Navmesh support (`.ynv`)
- `.ynv` files can be opened via file dialog or drag-and-drop
- Imported navmesh geometry is converted to GL buffers and rendered with simple lighting
- Loaded navmeshes are listed in Properties and can be removed individually or cleared all-at-once
- Editable flags in Properties:
  - Navmesh flags (`ENavMeshFlags`)
  - Per-polygon flags (`EPolygonFlags`), including bulk apply-to-all

## Controls

### Camera (viewport focused)
- RMB drag: look around
- MMB drag: pan
- Mouse wheel: movement speed / zoom behavior (camera mode dependent)
- W/A/S/D/Q/E: free-move in perspective mode

### Track selection/editing
- LMB click: select node/handle
- LMB drag: box selection
- Shift + selection: add
- Ctrl + selection: toggle
- Insert: add node after selected node
- Shift + Insert: add node before selected node
- Delete: remove selected node
- Gizmo translate while one/multiple nodes selected

## Build

### Prerequisites
- CMake >= 3.12
- C++17 compiler (MSVC, clang, or GCC)
- Git submodules initialized

### 1) Clone and fetch submodules

```bash
git submodule update --init --recursive
```

### 2) Configure

Recommended out-of-source build:

```bash
cmake -S . -B build-local
```

### 3) Build

```bash
cmake --build build-local --config Debug
```

Notes:
- If `VCPKG_ROOT` is set, the root `CMakeLists.txt` prefers the vcpkg toolchain.
- GLFW is resolved via `find_package(...)` and can fall back to CMake `FetchContent`.

## Runtime Files

Required assets:
- `asset/shader/*`
- `asset/font/MaterialSymbolsRounded.ttf`

Generated files:
- `navigator.xml` (options)
- `imgui.ini` (layout)

## Known Limitations

### Navmesh (`.ynv`) limitations
- `.ynv` import now has defensive validation (null/empty checks) and reports last load status/error in the Properties panel.
- Navmesh flag editing is in-memory only right now (no export/save workflow in UI yet).
- No deep navmesh inspection tools yet (for example adjacency/link graph editors).
- Diagnostics are basic (last load info/error text only), without a detailed import log view.

### General
- No unsaved-changes prompt for modified track data.
- No automated tests for parser/serializer or editor workflows.
- `.ydr` loading path exists but visible viewport integration is incomplete.

## Suggested Next Steps

1. Add navmesh save/export workflow from edited in-memory data.
2. Add error/toast reporting for failed `.ynv`/track file loads.
3. Add smoke tests for `.dat` and `.ynv` loading.
4. Add deeper navmesh inspection (links, adjacency, polygon metadata browser).

## License

See `LICENSE`.
