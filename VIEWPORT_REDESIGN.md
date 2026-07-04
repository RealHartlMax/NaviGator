# NaviGator Viewport Redesign Guide

**Ziel:** Verbesserte Viewport-Visualisierung für bessere Navmesh- und Track-Bearbeitung, inspiriert von CodeX-Architektur.

---

## 📐 Architektur-Übersicht

### Aktuelle NaviGator Struktur
```
UViewport (OpenGL + ImGui)
├── Framebuffer (Color + Depth)
├── Camera (Perspective/Orthographic)
├── Renderer (Basic)
└── Gizmo (Transform)
```

### Verbesserte Struktur (CodeX-inspiriert)
```
CMultiViewport (Enhanced Editor Viewport)
├── MainView (3D Render)
│   ├── DeferredRenderer
│   │   ├── G-Buffer Pass
│   │   ├── Lighting Pass
│   │   └── Post-Processing
│   ├── SceneObjects
│   │   ├── Navmeshes
│   │   ├── Tracks
│   │   └── Helpers
│   └── SelectionOutlines (Color-Coded)
│
├── OutlineView (Selection Highlight)
│   └── Entity Outline with Flag Colors
│
├── OverlayView (Gizmos & UI)
│   ├── Gizmo Layer (Position/Rotation/Scale)
│   ├── Grid & Axis
│   └── Measurements
│
└── PropertiesPanel
    ├── NodeProperties
    │   ├── Position (X/Y/Z with Sliders)
    │   ├── Rotation
    │   ├── Flags (Checkboxes)
    │   └── Metadata
    ├── NavmeshProperties
    ├── TrackProperties
    └── HierarchyView
```

---

## 🎨 Neue Features

### 1. **Enhanced Navmesh Rendering**

#### G-Buffer for Better Visibility
```cpp
class CNavmeshRenderer {
public:
    void RenderGBuffer() {
        // Render navmesh polygons to G-Buffer
        // Store: Position, Normal, Flag-ID
    }
    
    void RenderFlagColors() {
        // Apply deferred lighting with flag-based colors
        // SMALL = Blue, PAVED = Red, etc.
    }
    
    void RenderSelection() {
        // Render selected faces with bright outline
    }
};
```

#### Flag-Based Color Coding
| Flag | Color | Meaning |
|------|-------|---------|
| SMALL | `#4169E1` Blue | Tight space |
| LARGE | `#32CD32` Green | Open area |
| PAVED | `#FF6347` Red | Road surface |
| SHELTERED | `#FFD700` Yellow | Covered area |
| STEEP | `#FF4500` Orange | Slope >30° |
| WATER | `#1E90FF` Cyan | Water surface |

```glsl
// Fragment Shader Example
void main() {
    vec3 flagColor = GetFlagColor(polygonFlags);
    vec3 finalColor = mix(baseColor, flagColor, 0.6);
    
    if (isSelected) {
        finalColor += vec3(1.0, 1.0, 1.0) * 0.3;  // Bright highlight
    }
    gl_FragColor = vec4(finalColor, 1.0);
}
```

### 2. **Properties Panel - Node Editing**

#### Real-time Position Adjustment
```cpp
struct CNodeProperties {
    glm::vec3 Position;     // X, Y, Z sliders
    glm::vec3 Rotation;     // Euler angles
    float     Scale;
    
    // Navmesh-specific
    struct {
        bool SMALL;
        bool LARGE;
        bool PAVED;
        bool SHELTERED;
        bool STEEP;
        bool WATER;
    } Flags;
    
    // Track-specific
    struct {
        bool IsCurve;
        glm::vec3 HandleA;
        glm::vec3 HandleB;
        float Scalar;
    } TrackData;
};
```

#### UI Layout
```
┌─ Properties Panel ─────────────────┐
│ [Node Properties]                  │
│ ┌─ Position ────────────────────┐  │
│ │ X: [__2493.36__] [-] [+]      │  │
│ │ Y: [_-1482.21__] [-] [+]      │  │
│ │ Z: [___45.15___] [-] [+]      │  │
│ │ [Link to Game Coords]          │  │
│ └────────────────────────────────┘  │
│ ┌─ Flags ────────────────────────┐  │
│ │ ☑ SMALL    ☐ LARGE            │  │
│ │ ☑ PAVED    ☐ SHELTERED        │  │
│ │ ☐ STEEP    ☑ WATER            │  │
│ └────────────────────────────────┘  │
│ ┌─ Track Data ───────────────────┐  │
│ │ ☑ Is Curve                     │  │
│ │ Handle A: [x: __, y: __, z: __]│  │
│ │ Handle B: [x: __, y: __, z: __]│  │
│ │ Scalar: [__7.45__]             │  │
│ └────────────────────────────────┘  │
└────────────────────────────────────┘
```

### 3. **Hierarchy View (Left Panel)**

```
┌─ Hierarchy ─────────────────────┐
│ 📁 Scene                         │
│  ├─ 📍 Navmesh                  │
│  │  ├─ ► Face_001 (PAVED)       │
│  │  ├─ ► Face_002 (STEEP)       │
│  │  └─ ► Face_003 (WATER)       │
│  └─ 🚂 Track: trains1            │
│     ├─ ► Point_000 (Curve)      │
│     ├─ ► Point_001 (Linear)     │
│     └─ ► Point_002 (Curve)      │
│                                  │
│ [Search: _______________]        │
│ [Filter by Type: All ▼]          │
└──────────────────────────────────┘
```

### 4. **Viewport Gizmos**

#### Position Gizmo
- **Red Arrow** = X-Axis
- **Green Arrow** = Y-Axis
- **Blue Arrow** = Z-Axis
- **Center Cube** = Free movement

#### Interactive Dragging
```cpp
class CViewportGizmo {
public:
    void HandleMouseDrag() {
        if (DraggingAxis == AXIS_X) {
            selectedNode.Position.x += mouseDelta.x * speedMultiplier;
        }
        // Y and Z similar...
    }
    
    void Render() {
        if (selectedNode) {
            DrawArrows(selectedNode.Position);
            DrawCenterCube();
            if (hoveredAxis != NONE) {
                HighlightAxis(hoveredAxis);
            }
        }
    }
};
```

### 5. **Viewport Controls Info Overlay**

```
┌─────────────────────────────────────┐
│ Viewport Controls                   │
│ ──────────────────────────────────  │
│ Mouse:                              │
│   Middle + Drag   = Rotate Camera   │
│   Scroll          = Zoom            │
│   Right Click     = Context Menu    │
│                                     │
│ Gizmo:                              │
│   Click + Drag    = Move Node       │
│   SHIFT + Drag    = Snap to Grid    │
│   CTRL + Drag     = Slow Mode       │
│                                     │
│ Keyboard:                           │
│   F = Focus Selection               │
│   G = Toggle Grid                   │
│   V = Cycle View Mode               │
│   H = Hide/Show Selection           │
│                                     │
│ [☑ Show This Panel] [Close]         │
└─────────────────────────────────────┘
```

### 6. **Viewport Rendering Modes**

```cpp
enum class EViewportRenderMode {
    Standard,           // Normal rendering
    FlagOverlay,        // Flag-colored faces
    SelectionMask,      // Only selected items
    DepthVisualization, // Depth heatmap
    NormalVisualization // Surface normals
};
```

**Toolbar:**
```
[View: Perspective ▼] [RenderMode: Standard ▼] [Grid: ON] [Snap: OFF] [Gizmo: ON]
```

---

## 🔧 Implementation Plan

### Phase 1: Core Infrastructure
1. **MultiViewport System**
   - Refactor from single `UViewport` to `CMultiViewport`
   - Separate main view, outline view, overlay view
   - Implement view composition

2. **Properties Panel**
   - Create `CPropertiesPanel` widget
   - Implement numeric sliders for position/rotation
   - Add flag checkboxes with real-time updates

3. **Hierarchy View**
   - Create `CHierarchyView` widget
   - Display scene tree with icons
   - Add selection synchronization

### Phase 2: Enhanced Rendering
1. **Flag-Based Colors**
   - Update shaders to render flag colors
   - Implement color-coding system
   - Add flag visualization toggle

2. **Selection Outlines**
   - Implement outline rendering pass
   - Color-code by object type
   - Add glow effects for better visibility

3. **G-Buffer System**
   - Add deferred rendering pipeline
   - Store flag IDs in G-Buffer
   - Implement flag-based lighting

### Phase 3: Interaction
1. **Gizmo Enhancement**
   - Implement 3D transformation gizmo (like ImGuizmo advanced)
   - Add axis constraints
   - Implement snapping/grid

2. **Node Editing**
   - Real-time position updates from panel
   - Undo/Redo for property changes
   - Validation feedback

3. **Viewport Navigation**
   - Improve camera controls
   - Add focus-on-selection
   - Implement view presets

---

## 📁 File Structure Changes

### New Files to Create
```
src/ui/
├── UMultiViewport.hpp           [NEW] Multi-view system
├── UMultiViewport.cpp           [NEW]
├── UPropertiesPanel.hpp         [NEW] Property editing
├── UPropertiesPanel.cpp         [NEW]
├── UHierarchyView.hpp           [NEW] Scene tree
├── UHierarchyView.cpp           [NEW]
├── UViewportGizmo.hpp           [NEW] Enhanced gizmo
├── UViewportGizmo.cpp           [NEW]
├── IViewportRenderer.hpp        [NEW] Abstract renderer
├── IViewportRenderer.cpp        [NEW]
├── Renderers/
│   ├── CNavmeshRenderer.hpp     [NEW]
│   ├── CNavmeshRenderer.cpp     [NEW]
│   ├── CTrackRenderer.hpp       [NEW]
│   └── CTrackRenderer.cpp       [NEW]
│
└── [Modify existing:]
    ├── UViewport.cpp             Use new MultiViewport system
    └── UViewportPicker.cpp       Integration
```

### Shader Changes
```
asset/shader/
├── navmesh.vert                 [NEW] Navmesh rendering
├── navmesh_flags.frag           [NEW] Flag-based colors
├── selection_outline.vert       [NEW] Selection highlight
├── selection_outline.frag       [NEW]
└── gizmo.vert/.frag            [Update]
```

---

## 🎮 Workflow Improvement

### Before (Current)
```
1. Load navmesh/track
2. Rotate view to find node
3. Right-click node to edit properties
4. Edit properties in dialog
5. Apply changes (may need to reload)
```

### After (Improved)
```
1. Load navmesh/track
2. Browse hierarchy view
3. Click node in hierarchy → auto-focus viewport
4. Drag gizmo in viewport OR edit in properties panel (real-time)
5. See changes immediately with visual feedback
6. Keyboard shortcuts for quick operations
```

---

## 🔌 Game Path Integration

### Configuration Dialog
```
┌─ NaviGator Settings ──────────────┐
│                                   │
│ Game Installation Paths:          │
│                                   │
│ RDR3 Path:                        │
│ [_________________________]  [▼]   │
│                                   │
│ Load Navmesh from Game:           │
│ [☑] Auto-load when starting       │
│ [☑] Enable live sync              │
│                                   │
│ Export Settings:                  │
│ [Save path: ____________]  [▼]    │
│ [Export Format: ▼ (DAT/XML)]      │
│                                   │
│ [OK] [Cancel]                     │
└───────────────────────────────────┘
```

### Navmesh Loading
```cpp
class CNavmeshManager {
public:
    bool LoadFromGame(const std::string& gamePath) {
        // 1. Detect game installation
        // 2. Find navmesh files (.ynv, .ynv.xml)
        // 3. Parse and load
        // 4. Display in viewport with all flags
        return true;
    }
    
    bool ExportToFormat(EExportFormat format) {
        // Export current navmesh to DAT/XML/etc
        return true;
    }
};
```

---

## 🚀 Performance Considerations

| Feature | Performance Impact | Optimization |
|---------|-------------------|--------------|
| Multi-View Rendering | Moderate | Use view culling |
| Flag Overlay | Low | GPU shader-based |
| Selection Outline | Low | Screen-space filtering |
| Real-time Updates | Medium | Throttle property updates |
| Hierarchy Tree | Low | Virtual tree (LOD) |

---

## 📊 Comparison: NaviGator vs CodeX

| Feature | NaviGator (Current) | CodeX | NaviGator (Proposed) |
|---------|-------------------|-------|---------------------|
| Viewport Type | Single 3D View | Multi-View | **Multi-View** ✓ |
| Rendering | Basic OpenGL | Deferred DirectX | **Deferred OpenGL** ✓ |
| Properties Panel | Dialog-based | Sidebar Panel | **Real-time Panel** ✓ |
| Hierarchy View | None | Sidebar Tree | **Sidebar Tree** ✓ |
| Gizmo | ImGuizmo Basic | Advanced 3D | **ImGuizmo+ Advanced** ✓ |
| Flag Visualization | Text-based | Color-coded | **Color-coded** ✓ |
| Node Position Edit | Via Dialog | Gizmo + Panel | **Gizmo + Panel** ✓ |
| Game Integration | Manual | Via File Manager | **Via File Manager** ✓ |

---

## 📚 References

- CodeX Editor Architecture: `H:\RP_Scripte\REDM_scripts\01_Tools\CodeX_030_src`
- NaviGator Current Viewport: [src/ui/UViewport.cpp](../src/ui/UViewport.cpp)
- ImGui Docking: `imgui/imgui_demo.cpp` (Docking example)
- ImGuizmo: `lib/imguizmo/ImGuizmo.h` (Current implementation)

---

## ✅ Success Criteria

- [ ] Multi-view rendering system operational
- [ ] Properties panel shows and updates node positions in real-time
- [ ] Hierarchy view properly displays all objects
- [ ] Flag colors correctly render navmesh polygons
- [ ] Gizmo allows position/rotation adjustment
- [ ] Game file paths can be configured
- [ ] All operations undo/redo correctly
- [ ] Performance stable at 60+ FPS with full scene

