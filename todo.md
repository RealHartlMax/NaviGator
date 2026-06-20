# TODO

## High priority

- [ ] Fix first-frame delta time initialization in `AApplication::Run` (`src/application/AApplication.cpp`).
- [ ] Make dependency setup reproducible (document/package GLFW3 + other required system libs for Linux/Windows).
- [ ] Add robust file validation/error feedback for XML/DAT/navmesh loading flows.
- [ ] Add bounds checks for user input paths and parsing errors to prevent crashes on malformed files.

## Medium priority

- [ ] Optimize viewport/picking buffer resize logic to avoid recreating framebuffers every frame.
- [ ] Add an explicit “Close/Quit” action in UI (`File -> Close` currently has no shutdown handling).
- [ ] Implement drawable rendering flow after `.ydr` import (currently data loads but is not displayed).
- [ ] Improve UX for track editing (batch edit, clearer junction editing state, undo/redo support).
- [ ] Add save-state dirtiness tracking and prompt before losing unsaved changes.

## Low priority

- [ ] Add CI workflow for CMake configure/build on at least one Linux toolchain.
- [ ] Add smoke tests / parser tests for track data formats.
- [ ] Split rendering, data, and UI responsibilities further for easier maintenance.
- [ ] Add contribution guide and development setup docs.
