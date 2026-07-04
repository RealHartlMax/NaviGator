# 🚀 NaviGator Viewport Redesign - Konkrete Umsetzung

## Phase 1: Foundation (Woche 1-2)

### ✅ Task 1.1: Properties Panel Infrastructure
**Aufwand:** 3-4 Tage  
**Priorität:** 🔴 SEHR HOCH - Basis für alles andere

**Was wird gemacht:**
- Neue Klasse `CPropertiesPanel` mit ImGui
- Real-time Vector3 Input (X, Y, Z Sliders)
- Position Update → Viewport Callback

**Neuen File erstellen:**
```cpp
// src/ui/CPropertiesPanel.hpp
#pragma once
#include <imgui.h>
#include <glm/glm.hpp>
#include <functional>

class CPropertiesPanel {
public:
    CPropertiesPanel();
    ~CPropertiesPanel();
    
    // Render the panel
    void Draw(ImGuiID dockspaceId);
    
    // Update selected object
    void SetSelectedNode(class UTrackPoint* node);
    void ClearSelection();
    
    // Callbacks
    using PositionChangedCallback = std::function<void(const glm::vec3&)>;
    void SetOnPositionChanged(PositionChangedCallback cb) { 
        onPositionChanged = cb; 
    }
    
private:
    UTrackPoint* selectedNode = nullptr;
    glm::vec3 editPosition{};
    PositionChangedCallback onPositionChanged;
    
    void DrawPositionProperties();
    void DrawFlagProperties();
};
```

**Implementation:**
```cpp
// src/ui/CPropertiesPanel.cpp
#include "CPropertiesPanel.hpp"
#include "tracks/UTrackPoint.hpp"
#include <imgui.h>

CPropertiesPanel::CPropertiesPanel() {
}

void CPropertiesPanel::Draw(ImGuiID dockspaceId) {
    ImGui::SetNextWindowDockID(dockspaceId, ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Properties", nullptr, ImGuiWindowFlags_NoMove)) {
        if (selectedNode) {
            DrawPositionProperties();
            ImGui::Separator();
            DrawFlagProperties();
        } else {
            ImGui::TextDisabled("No selection");
        }
        ImGui::End();
    }
}

void CPropertiesPanel::SetSelectedNode(UTrackPoint* node) {
    selectedNode = node;
    if (node) {
        editPosition = node->GetPosition();
    }
}

void CPropertiesPanel::DrawPositionProperties() {
    ImGui::Text("Position");
    ImGui::Indent();
    
    bool changed = false;
    float pos[3] = {editPosition.x, editPosition.y, editPosition.z};
    
    if (ImGui::DragFloat("##x", &pos[0], 0.1f)) {
        editPosition.x = pos[0];
        changed = true;
    }
    ImGui::SameLine();
    ImGui::Text("X");
    
    if (ImGui::DragFloat("##y", &pos[1], 0.1f)) {
        editPosition.y = pos[1];
        changed = true;
    }
    ImGui::SameLine();
    ImGui::Text("Y");
    
    if (ImGui::DragFloat("##z", &pos[2], 0.1f)) {
        editPosition.z = pos[2];
        changed = true;
    }
    ImGui::SameLine();
    ImGui::Text("Z");
    
    if (changed && onPositionChanged) {
        onPositionChanged(editPosition);
    }
    
    ImGui::Unindent();
}

void CPropertiesPanel::DrawFlagProperties() {
    ImGui::Text("Flags");
    ImGui::Indent();
    
    bool isCurve = selectedNode && selectedNode->GetCurveFlag();
    ImGui::Checkbox("Is Curve Point", &isCurve);
    if (selectedNode) {
        selectedNode->SetCurveFlag(isCurve);
    }
    
    ImGui::Unindent();
}
```

**Integration in UViewport:**
```cpp
// Modify src/ui/UViewport.cpp

class UViewport {
private:
    CPropertiesPanel* propertiesPanel = nullptr;
    
public:
    void Initialize() {
        // ... existing code ...
        propertiesPanel = new CPropertiesPanel();
        propertiesPanel->SetOnPositionChanged(
            [this](const glm::vec3& newPos) {
                if (selectedTrackPoint) {
                    selectedTrackPoint->SetPosition(newPos);
                }
            }
        );
    }
    
    void Render() {
        // ... existing viewport code ...
        
        // Docking setup
        ImGuiID dockspaceId = ImGui::GetID("MainDockspace");
        ImGui::DockSpace(dockspaceId);
        
        // Draw main viewport in center
        DrawViewport(dockspaceId);
        
        // Draw properties panel on right
        if (propertiesPanel) {
            propertiesPanel->Draw(dockspaceId);
        }
    }
};
```

**Tests:**
- [ ] Panel renders ohne Fehler
- [ ] Sliders verändern Position
- [ ] Position Updates im Viewport sichtbar
- [ ] Kein Memory Leak

---

### ✅ Task 1.2: Hierarchy View
**Aufwand:** 3-4 Tage  
**Priorität:** 🔴 SEHR HOCH - Bessere Navigation

**Was wird gemacht:**
- Tree View mit ImGui
- Track Points anzeigen
- Single/Multiple Selection

**Neuen File:**
```cpp
// src/ui/CHierarchyView.hpp
#pragma once
#include <imgui.h>
#include <vector>
#include <functional>

class UTrackPoint;
class UTrack;

class CHierarchyView {
public:
    CHierarchyView();
    ~CHierarchyView();
    
    void Draw(ImGuiID dockspaceId);
    void SetTrack(UTrack* track);
    
    using SelectionCallback = std::function<void(UTrackPoint*)>;
    void SetOnSelectionChanged(SelectionCallback cb) {
        onSelectionChanged = cb;
    }
    
    UTrackPoint* GetSelectedNode() const { return selectedNode; }
    
private:
    UTrack* track = nullptr;
    UTrackPoint* selectedNode = nullptr;
    SelectionCallback onSelectionChanged;
    
    void DrawTrackTree();
};
```

**Implementation:**
```cpp
// src/ui/CHierarchyView.cpp
#include "CHierarchyView.hpp"
#include "tracks/UTrack.hpp"
#include "tracks/UTrackPoint.hpp"
#include <imgui.h>

void CHierarchyView::Draw(ImGuiID dockspaceId) {
    ImGui::SetNextWindowDockID(dockspaceId, ImGuiCond_FirstUseEver);
    
    if (ImGui::Begin("Hierarchy", nullptr, ImGuiWindowFlags_NoMove)) {
        if (track) {
            DrawTrackTree();
        } else {
            ImGui::TextDisabled("No track loaded");
        }
        ImGui::End();
    }
}

void CHierarchyView::SetTrack(UTrack* t) {
    track = t;
}

void CHierarchyView::DrawTrackTree() {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Framed |
                              ImGuiTreeNodeFlags_NoAutoOpenOnLog;
    
    if (ImGui::TreeNodeEx("Track", flags)) {
        for (size_t i = 0; i < track->GetPointCount(); i++) {
            UTrackPoint* point = track->GetPoint(i);
            
            ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_Leaf;
            if (selectedNode == point) {
                nodeFlags |= ImGuiTreeNodeFlags_Selected;
            }
            
            bool open = ImGui::TreeNodeEx(
                (void*)(intptr_t)i,
                nodeFlags,
                "Point_%03zu %s",
                i,
                point->GetCurveFlag() ? "[C]" : "[L]"
            );
            
            if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
                selectedNode = point;
                if (onSelectionChanged) {
                    onSelectionChanged(point);
                }
            }
            
            if (open) {
                ImGui::TreePop();
            }
        }
        ImGui::TreePop();
    }
}
```

**Tests:**
- [ ] Tree renders alle Points
- [ ] Selection funktioniert
- [ ] Callback wird aufgerufen
- [ ] Performance ok bei vielen Points

---

### ✅ Task 1.3: Flag-Based Rendering (Vereinfacht)
**Aufwand:** 2-3 Tage  
**Priorität:** 🟡 HOCH - Sichtbare Verbesserung

**Was wird gemacht:**
- Shader für Flag-basierte Farben
- Kein deferred rendering (noch nicht), einfach Vertex Colors
- Navmesh umpaint mit Farben basierend auf Flag

**Neuen Shader:**
```glsl
// asset/shader/navmesh_flags.vert
#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in uint flags;

out vec4 vertexColor;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 model;

void main() {
    gl_Position = projection * view * model * vec4(position, 1.0);
    
    // Flag-based coloring
    if ((flags & 1u) != 0u) {  // PAVED
        vertexColor = vec4(1.0, 0.2, 0.2, 0.7);  // Red
    } else if ((flags & 2u) != 0u) {  // WATER
        vertexColor = vec4(0.2, 1.0, 1.0, 0.7);  // Cyan
    } else if ((flags & 4u) != 0u) {  // STEEP
        vertexColor = vec4(1.0, 0.6, 0.2, 0.7);  // Orange
    } else {
        vertexColor = vec4(0.5, 0.5, 0.5, 0.7);  // Gray
    }
}
```

```glsl
// asset/shader/navmesh_flags.frag
#version 330 core

in vec4 vertexColor;
out vec4 FragColor;

void main() {
    FragColor = vertexColor;
}
```

**Rendering Update:**
```cpp
// Modify src/ui/UViewport.cpp

void UViewport::RenderNavmesh() {
    // Use new flag shader
    flagShader->Use();
    
    for (const auto& face : currentNavmesh->GetFaces()) {
        // Pass flags as vertex attribute
        uint32_t flags = face.GetFlags();
        
        glVertexAttribI1ui(2, flags);  // location = 2
        
        // Render face...
    }
}
```

**Tests:**
- [ ] Shader kompiliert
- [ ] Farben sind korrekt sichtbar
- [ ] Performance ok
- [ ] Verschiedene Flags haben unterschiedliche Farben

---

## Phase 2: Enhancements (Woche 3-4)

### ✅ Task 2.1: Viewport Gizmo
**Aufwand:** 2-3 Tage

- ImGuizmo integrieren (ist schon in Dependencies!)
- Position, Rotation, Scale Gizmos
- Nur wenn Node selected

### ✅ Task 2.2: Undo/Redo System
**Aufwand:** 3-4 Tage

- Command Pattern implementieren
- Position Changes speichern
- Keyboard: Ctrl+Z / Ctrl+Y

### ✅ Task 2.3: Search & Filter in Hierarchy
**Aufwand:** 2 Tage

- Search Bar in Hierarchy
- Filter by Type (Curve/Linear)
- Highlight matches

---

## Phase 3: Polish (Woche 5)

### ✅ Task 3.1: Keyboard Shortcuts
- G = Move Mode
- E = Rotate (wenn implementiert)
- Delete = Remove Point
- Ctrl+D = Duplicate Point

### ✅ Task 3.2: Context Menu
- Right-Click auf Point
- Insert Point Before/After
- Delete Point
- Duplicate Point

### ✅ Task 3.3: Performance Optimization
- Rendering optimization
- Memory profiling
- Caching

---

## 🎯 Start Recommendation

### SOFORT STARTEN mit Task 1.1 (Properties Panel):

**Warum:**
1. ✅ Unabhängig (keine Dependencies)
2. ✅ Sichtbares Resultat in 3-4 Tagen
3. ✅ Basis für die anderen Tasks
4. ✅ Benutzer merkt sofort Verbesserung

**Schritte:**
1. Erstelle `CPropertiesPanel.hpp` + `.cpp`
2. Integriere in `UViewport.cpp`
3. Test mit aktuellem Track
4. Performance check

**Timeframe:** ~1 Woche zum Laufen bringen

---

### DANN Task 1.2 (Hierarchy View):

**Abhängigkeiten:** Keine (unabhängig parallel möglich)

**Timeframe:** +1 Woche

---

### PARALLEL: Task 1.3 (Flag Colors):

**Abhängigkeiten:** Nur Shader (kann gleichzeitig entwickelt werden)

**Timeframe:** +1 Woche

---

## 📊 Ressourcen bereits vorhanden

✅ **ImGui** - schon in Dependencies (docking, sliders, trees)  
✅ **ImGuizmo** - schon vorhanden  
✅ **GLM Math** - vorhanden  
✅ **OpenGL** - schon im Build  

**Nichts Neues installieren notwendig!**

---

## 🔍 Code Review Points

Bevor du startest:

1. Schaue `src/ui/UViewport.cpp` (wie wird aktuell gerendert?)
2. Schaue `include/tracks/UTrackPoint.hpp` (Datenstruktur)
3. Schaue `include/tracks/UTrack.hpp` (Track Container)

---

## ✅ Milestones

```
Week 1: Properties Panel basic functionality ✓
Week 2: Hierarchy View + Flag Colors ✓
Week 3: Gizmo Integration ✓
Week 4: Undo/Redo + Polish ✓
Week 5: Final Optimization & Testing ✓
```

---

## 💡 Implementation Tips

### ImGui Docking
```cpp
ImGuiID dockspaceId = ImGui::GetID("MainDockspace");
ImGuiDockNodeFlags flags = ImGuiDockNodeFlags_PassthruCentralNode;
ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), flags);
```

### Callbacks Pattern
```cpp
// In header
using Callback = std::function<void(const glm::vec3&)>;
void SetOnPositionChanged(Callback cb) { onChanged = cb; }

// In implementation
if (onChanged) {
    onChanged(newPosition);
}
```

### ImGui Selectable/Clickable
```cpp
if (ImGui::Selectable(label, isSelected)) {
    // Handle click
}
```

---

## 🐛 Mögliche Issues

**Issue:** Property Panel nicht synchronized mit Viewport  
→ Ensure bidirectional callback

**Issue:** Tree performance bei vielen Points  
→ Use ImGuiListClipper wenn > 10k Points

**Issue:** Memory Leak in Panel  
→ Destruktor aufräumen, WeakPtr für selected node

---

## 📋 Vorher Checklist

- [ ] Git branch erstellt (`feature/properties-panel`)
- [ ] CMakeLists.txt Updated (falls neue Dependencies)
- [ ] Build successful
- [ ] Tests laufen

---

## 🚀 Los geht's!

**Nächster Step:** Erstelle die 3 neuen Files und Integrations-Code

**Geschätzte Zeit bis lauffähig:** 1 Woche für Task 1.1

Sollen wir direkt mit dem Code starten?

