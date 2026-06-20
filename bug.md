# Known Bugs / Issues

## 1) Uninitialized frame timing on first loop iteration
- **Location:** `src/application/AApplication.cpp`
- **Problem:** `lastFrameTime = thisFrameTime;` is executed before `thisFrameTime` has a defined value.
- **Impact:** Undefined behavior and potentially incorrect first-frame `deltaTime`.
- **Suggested fix:** Initialize both timestamps before entering the loop.

## 2) Mouse wheel sign is lost
- **Location:** `src/application/AInput.cpp`
- **Problem:** Scroll delta is passed/stored through unsigned types (`uint32_t`) even though GLFW provides signed deltas.
- **Impact:** Negative wheel movement can wrap/convert incorrectly.
- **Suggested fix:** Use signed integer/float consistently for scroll delta storage and setters.

## 3) Heavy framebuffer recreation during interaction
- **Location:** `src/ui/UViewport.cpp`, `src/ui/UViewportPicker.cpp`, `src/application/AGatorContext.cpp`
- **Problem:** Viewport and picking framebuffers are recreated frequently in update/render flow rather than only on size change.
- **Impact:** Unnecessary GPU overhead and potential stutter.
- **Suggested fix:** Cache last dimensions and recreate only when width/height actually changed.

## 4) File menu “Close” has no implemented behavior
- **Location:** `src/application/AGatorContext.cpp`
- **Problem:** Menu item exists but does not trigger app/window shutdown.
- **Impact:** User expectation mismatch and incomplete UI behavior.
- **Suggested fix:** Wire menu action to a shutdown signal (e.g., request window close).

## 5) `.ydr` import path appears incomplete
- **Location:** `src/application/ADrawableContext.cpp`
- **Problem:** Imported drawables are stored, but there is no visible render path in current UI/render loop.
- **Impact:** Users can load files without visual confirmation/results.
- **Suggested fix:** Add drawable render integration into viewport rendering pipeline.
