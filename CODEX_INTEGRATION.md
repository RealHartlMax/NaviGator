# NaviGator CodeX Integration Guide

**Ziel:** NaviGator native als Tool in CodeX integrieren und als Single-Window-Editor betreiben.

---

## 📋 Inhaltsverzeichnis

1. [Architecture Overview](#-architecture-overview)
2. [Integration Points](#-integration-points)
3. [Implementation Steps](#-implementation-steps)
4. [File Structure](#-file-structure)
5. [CodeX Module Creation](#-codex-module-creation)
6. [Testing & Deployment](#-testing--deployment)

---

## 🏗️ Architecture Overview

### CodeX Plugin System

CodeX nutzt ein **Plugin/Tool-basiertes System**, das auf folgende Komponenten aufbaut:

```
CodeX.Core
├── Engine (Rendering, Physics, etc.)
├── Editor (EditorScene, Selection, Gizmo)
└── UI (ImGui-based)

CodeX.Games.RDR2
├── Game Definition (RDR2Game.cs)
├── Project Tools (RDR2Project.cs) ← NAVigation Tool HERE
├── File Manager (RPF8FileManager)
└── Prefab Manager

NaviGator Integration
├── NavMesh Editor Tool
├── Track Editor Tool
└── Combined UI Container
```

### NaviGator as CodeX Tool

```csharp
public class NaviGatorTool : ProjectTool
{
    public override string ToolName => "NaviGator - Track & Navmesh Editor";
    public override string ToolDescription => "Edit track paths, curves, and navmesh flags";
    
    private TrackEditor trackEditor;
    private NavmeshEditor navmeshEditor;
    private TabbedViewport viewport;
    
    public override void Initialize(EditorScene editor) { }
    public override void Render() { }
    public override void HandleInput() { }
}
```

---

## 🔌 Integration Points

### 1. **Game File Access**

CodeX bietet zentrale Datei-Verwaltung:

```csharp
// Current NaviGator: Manual file loading
navmesh.Load("path/to/file.dat");

// With CodeX Integration:
var fileManager = editor.Game.FileManager;
var navmeshFile = fileManager.GetFile("common:/data/levels/rdr3/navmeshes/track_001.ynv");
navmesh.Load(navmeshFile.Stream);
```

**Vorteil:** Automatische RPF8-Dekompression, caching, etc.

### 2. **Rendering Pipeline**

```csharp
// Current NaviGator: Standalone OpenGL context
viewportRenderer.Render(navmeshData);

// With CodeX Integration:
editorScene.Renderer.AddRenderable(navmeshEntity);
editorScene.Renderer.Render(viewMatrix, projMatrix);
```

**Vorteil:** Deferred rendering, shadows, lighting

### 3. **Editor Selection System**

```csharp
// Current: Custom selection logic
if (clickedNode) {
    selectedNode = clickedNode;
}

// With CodeX Integration:
editorScene.SelectionItems.Add(
    new EditorSelectionItem { Entity = navmeshFaceEntity, ... }
);
```

**Vorteil:** Unified undo/redo, outlining, gizmos

### 4. **Property Panel**

```csharp
// Current: Custom dialog
EditNodePropertiesDialog(node);

// With CodeX Integration:
var properties = new PropertyPanel();
properties.AddVector3("Position", node.Position);
properties.AddFloatSlider("Height", node.Height, 0, 100);
editorScene.PropertyPanel.SetProperties(properties);
```

**Vorteil:** Konsistente UI mit CodeX

---

## 🔧 Implementation Steps

### Step 1: Create CodeX.NaviGator Module

**File Structure:**
```
CodeX.NaviGator/
├── CodeX.NaviGator.csproj
├── NaviGatorTool.cs                 # Main entry point
├── Editors/
│   ├── TrackEditor.cs
│   ├── NavmeshEditor.cs
│   └── CombinedEditor.cs
├── Formats/
│   ├── TrackFileFormat.cs           # .dat parsing
│   ├── NavmeshFileFormat.cs         # .ynv.xml parsing
│   └── FormatConverters.cs
├── Rendering/
│   ├── NavmeshRenderer.cs
│   ├── TrackRenderer.cs
│   └── SelectionOutlineRenderer.cs
├── Properties/
│   ├── NodeProperties.cs
│   ├── TrackProperties.cs
│   └── NavmeshProperties.cs
├── UI/
│   ├── NaviGatorViewport.cs
│   ├── PropertiesPanel.cs
│   ├── HierarchyView.cs
│   └── ControlsOverlay.cs
└── Utils/
    ├── NavmeshFlagHelper.cs
    ├── TrackCurveCalculator.cs
    └── CoordinateSystem.cs
```

### Step 2: Implement NaviGatorTool

```csharp
using CodeX.Core.Editor;
using CodeX.Core.Engine;
using CodeX.Core.Rendering;
using System.Numerics;

namespace CodeX.NaviGator
{
    public class NaviGatorTool : ProjectTool
    {
        public override string ToolName => "NaviGator";
        public override string ToolDescription => 
            "Edit navmesh polygons and train track paths";
        public override FileTypeIcon Icon => FileTypeIcon.Navigation;

        private EditorScene Editor;
        private NaviGatorUI MainUI;
        private TrackEditor TrackEditor;
        private NavmeshEditor NavmeshEditor;
        
        public override void Initialize(EditorScene editor)
        {
            Editor = editor;
            
            // Create track editor
            TrackEditor = new TrackEditor(editor);
            
            // Create navmesh editor
            NavmeshEditor = new NavmeshEditor(editor);
            
            // Create main UI container
            MainUI = new NaviGatorUI(editor, TrackEditor, NavmeshEditor);
            editor.AddUIControl(MainUI);
        }

        public override void OnProjectLoaded(Project project)
        {
            // Load all available navmesh and track files
            LoadGameAssets();
        }

        private void LoadGameAssets()
        {
            var fileManager = Editor.ProjectMan.CurrentProject.Game.FileManager;
            
            // Load track files
            var trackFiles = fileManager.FindFiles("common:/data/levels/rdr3/tracks/");
            foreach (var file in trackFiles) {
                if (file.Name.EndsWith(".dat")) {
                    TrackEditor.LoadTrack(file);
                }
            }
            
            // Load navmesh files
            var navmeshFiles = fileManager.FindFiles("common:/data/levels/rdr3/");
            foreach (var file in navmeshFiles) {
                if (file.Name.EndsWith(".ynv") || file.Name.EndsWith(".ynv.xml")) {
                    NavmeshEditor.LoadNavmesh(file);
                }
            }
        }
    }
}
```

### Step 3: Implement Rendering Integration

```csharp
using CodeX.Core.Engine;
using CodeX.Core.Numerics;
using System.Numerics;

namespace CodeX.NaviGator.Rendering
{
    public class NavmeshRenderer : IRenderable
    {
        private Mesh NavmeshGeometry;
        private Material FlagMaterial;
        private EditorScene Editor;
        
        public NavmeshRenderer(EditorScene editor, NavmeshData navmeshData)
        {
            Editor = editor;
            BuildGeometry(navmeshData);
        }

        private void BuildGeometry(NavmeshData data)
        {
            // Convert navmesh polygons to renderable mesh
            var vertices = new List<Vector3>();
            var colors = new List<Vector4>();
            var indices = new List<uint>();

            foreach (var polygon in data.Polygons)
            {
                uint baseIndex = (uint)vertices.Count;
                
                // Add polygon vertices
                foreach (var vertex in polygon.Vertices)
                {
                    vertices.Add(vertex);
                    colors.Add(GetFlagColor(polygon.Flags));
                }
                
                // Triangulate polygon
                for (uint i = 1; i < polygon.Vertices.Count - 1; i++)
                {
                    indices.Add(baseIndex);
                    indices.Add(baseIndex + i);
                    indices.Add(baseIndex + i + 1);
                }
            }

            NavmeshGeometry = new Mesh
            {
                Vertices = vertices.ToArray(),
                Colors = colors.ToArray(),
                Indices = indices.ToArray()
            };
        }

        private Vector4 GetFlagColor(NavmeshFlags flags)
        {
            // Map flags to colors (RGB)
            if ((flags & NavmeshFlags.PAVED) != 0) return new Vector4(1.0f, 0.2f, 0.2f, 0.7f); // Red
            if ((flags & NavmeshFlags.SMALL) != 0) return new Vector4(0.2f, 0.4f, 1.0f, 0.7f); // Blue
            if ((flags & NavmeshFlags.STEEP) != 0) return new Vector4(1.0f, 0.5f, 0.0f, 0.7f); // Orange
            if ((flags & NavmeshFlags.WATER) != 0) return new Vector4(0.0f, 0.8f, 1.0f, 0.7f); // Cyan
            
            return new Vector4(0.5f, 0.5f, 0.5f, 0.7f); // Gray (default)
        }

        public void Render(RenderContext context)
        {
            if (NavmeshGeometry == null) return;
            
            // Render with editor materials
            FlagMaterial.Bind(context);
            context.DrawMesh(NavmeshGeometry);
        }
    }
}
```

### Step 4: Properties Panel Integration

```csharp
using CodeX.Core.UI;

namespace CodeX.NaviGator.Properties
{
    public class NavmeshPolygonProperties : UIControl
    {
        public NavmeshPolygon Polygon { get; set; }
        
        public NavmeshPolygonProperties()
        {
            // Create property fields
            AddFlagCheckbox("SMALL");
            AddFlagCheckbox("LARGE");
            AddFlagCheckbox("PAVED");
            AddFlagCheckbox("SHELTERED");
            AddFlagCheckbox("STEEP");
            AddFlagCheckbox("WATER");
            
            // Metadata
            AddTextField("Ped Density", "Value");
            AddTextField("Audio Zone", "Value");
        }

        private void AddFlagCheckbox(string flagName)
        {
            var checkbox = new UICheckbox();
            checkbox.Text = flagName;
            checkbox.OnChange += (value) =>
            {
                Polygon?.SetFlag(flagName, value);
            };
            this.Add(checkbox);
        }
    }

    public class TrackPointProperties : UIControl
    {
        public TrackPoint Point { get; set; }

        public TrackPointProperties()
        {
            // Position controls
            AddVector3Control("Position", () => Point.Position, (v) => Point.Position = v);
            
            // Curve controls
            AddCheckbox("Is Curve", () => Point.IsCurve, (v) => Point.IsCurve = v);
            
            if (Point.IsCurve)
            {
                AddVector3Control("Handle A", () => Point.HandleA, (v) => Point.HandleA = v);
                AddVector3Control("Handle B", () => Point.HandleB, (v) => Point.HandleB = v);
            }
            
            // Station info
            AddEnumDropdown("Station Type", () => Point.StationType);
        }

        private void AddVector3Control(string label, 
            System.Func<Vector3> getter, 
            System.Action<Vector3> setter)
        {
            // Create X, Y, Z input fields
            var xField = new UINumEdit();
            var yField = new UINumEdit();
            var zField = new UINumEdit();
            
            // Wire up getters/setters
            xField.OnChange += (v) => 
            {
                var pos = getter();
                pos.X = (float)v;
                setter(pos);
            };
            // Similar for Y, Z...
        }
    }
}
```

### Step 5: File Format Handling

```csharp
using System.IO;
using CodeX.Core.Engine;

namespace CodeX.NaviGator.Formats
{
    public class TrackDataFormat
    {
        public static Track LoadTrack(FileEntry entry)
        {
            using (var stream = entry.Open())
            using (var reader = new StreamReader(stream))
            {
                var headerLine = reader.ReadLine();
                var parts = headerLine.Split(' ');
                
                int nodeCount = int.Parse(parts[0]);
                int curveNodeCount = int.Parse(parts[1]);
                bool isClosed = parts[2] == "close";
                
                var track = new Track
                {
                    NodeCount = nodeCount,
                    CurveNodeCount = curveNodeCount,
                    IsClosed = isClosed,
                    Points = new List<TrackPoint>()
                };
                
                for (int i = 0; i < nodeCount; i++)
                {
                    var point = ParseTrackPoint(reader);
                    track.Points.Add(point);
                }
                
                return track;
            }
        }

        private static TrackPoint ParseTrackPoint(StreamReader reader)
        {
            var line = reader.ReadLine();
            var parts = line.Split(' ');
            
            if (parts[0] == "c")
            {
                // Curve point
                return new TrackPoint
                {
                    IsCurve = true,
                    Position = new Vector3(
                        float.Parse(parts[1]),
                        float.Parse(parts[2]),
                        float.Parse(parts[3])
                    ),
                    HandleA = new Vector3(
                        float.Parse(parts[4]),
                        float.Parse(parts[5]),
                        float.Parse(parts[6])
                    ),
                    HandleB = new Vector3(
                        float.Parse(parts[7]),
                        float.Parse(parts[8]),
                        float.Parse(parts[9])
                    )
                };
            }
            else
            {
                // Linear point
                return new TrackPoint
                {
                    IsCurve = false,
                    Position = new Vector3(
                        float.Parse(parts[0]),
                        float.Parse(parts[1]),
                        float.Parse(parts[2])
                    )
                };
            }
        }
    }
}
```

---

## 📁 File Structure

### Project Organization

```
CodeX-main/
├── CodeX.NaviGator/                    [NEW]
│   ├── CodeX.NaviGator.csproj
│   ├── NaviGatorTool.cs
│   ├── Editors/
│   │   ├── TrackEditor.cs
│   │   ├── NavmeshEditor.cs
│   │   └── CombinedEditor.cs
│   ├── Formats/
│   │   ├── TrackFileFormat.cs
│   │   ├── NavmeshFileFormat.cs
│   │   └── FormatConverters.cs
│   ├── Rendering/
│   │   ├── NavmeshRenderer.cs
│   │   ├── TrackRenderer.cs
│   │   └── SelectionOutlineRenderer.cs
│   ├── Properties/
│   │   ├── NodeProperties.cs
│   │   ├── TrackProperties.cs
│   │   └── NavmeshProperties.cs
│   ├── UI/
│   │   ├── NaviGatorViewport.cs
│   │   ├── PropertiesPanel.cs
│   │   ├── HierarchyView.cs
│   │   └── ControlsOverlay.cs
│   └── Utils/
│       ├── NavmeshFlagHelper.cs
│       ├── TrackCurveCalculator.cs
│       └── CoordinateSystem.cs
│
├── CodeX.Games.RDR2/
│   └── RDR2Project.cs               [MODIFY]
│       └── Register NaviGatorTool
│
└── CodeX/
    └── Program.cs                   [MODIFY]
        └── Load NaviGator module
```

### Codex Module Registration

In `CodeX.Games.RDR2/RDR2Project.cs`:

```csharp
public static class RDR2Project
{
    public static ProjectTool[] Tools => new ProjectTool[]
    {
        // ... existing tools ...
        new NaviGatorTool(),  // ← Add this
    };
}
```

---

## 🏗️ CodeX Module Creation

### Step 1: Create Project File

`CodeX.NaviGator/CodeX.NaviGator.csproj`:

```xml
<Project Sdk="Microsoft.NET.Sdk.WindowsDesktop">

  <PropertyGroup>
    <TargetFramework>net8.0-windows</TargetFramework>
    <UseWindowsForms>true</UseWindowsForms>
    <LangVersion>latest</LangVersion>
  </PropertyGroup>

  <ItemGroup>
    <ProjectReference Include="..\CodeX.Core\CodeX.Core.csproj" />
    <ProjectReference Include="..\CodeX.Games.RDR2\CodeX.Games.RDR2.csproj" />
  </ItemGroup>

  <ItemGroup>
    <PackageReference Include="System.Numerics.Vectors" Version="4.5.0" />
    <PackageReference Include="ImGui.NET" Version="1.89.9.3" />
  </ItemGroup>

</Project>
```

### Step 2: Namespace & Assembly Structure

```csharp
// File: CodeX.NaviGator/NaviGatorTool.cs
namespace CodeX.NaviGator
{
    using CodeX.Core.Editor;
    using CodeX.Games.RDR2;
    
    public class NaviGatorTool : ProjectTool
    {
        // Implementation...
    }
}
```

### Step 3: Add to Main Solution

Edit `CodeX.sln`:

```xml
Project("{9A19103F-16F7-4668-BE54-9A1E7A4F7556}") = "CodeX.NaviGator", "CodeX.NaviGator\CodeX.NaviGator.csproj", "{...GUID...}"
EndProject
```

### Step 4: Configuration in RDR2 Module

`CodeX.Games.RDR2/RDR2Project.cs`:

```csharp
using CodeX.NaviGator;

public static class RDR2Project
{
    public static ProjectTool[] Tools = new ProjectTool[]
    {
        new NaviGatorTool(),
        // ... other tools
    };
}
```

---

## 🎯 UI Integration Points

### Main Viewport Layout

```
┌─────────────────────────────────────────────────┐
│  CodeX - RDR2 Project                      [_▭▯] │
├──────────────────────────────────────────────────┤
│ File Edit View Tools Window Help                │
├──────────────────────────────────────────────────┤
│                                                  │
│ ┌─Hierarchy──────┐  ┌──────────────────────┐   │
│ │ 📁 NaviGator   │  │   Viewport           │   │
│ │  ├─ 🚂 Tracks  │  │  [3D Render]         │   │
│ │  │ ├─ trains1  │  │  (with overlays)     │   │
│ │  │ └─ trains2  │  │                      │   │
│ │  ├─ 📍 Navmesh │  │                      │   │
│ │  │ ├─ Face_1   │  │                      │   │
│ │  │ └─ Face_2   │  └──────────────────────┘   │
│ │  └─ ⚙️ Settings │                             │
│ └────────────────┘  ┌──────────────────────┐   │
│                     │   Properties         │   │
│                     │ ┌────────────────┐   │   │
│                     │ │ Position:      │   │   │
│                     │ │ X: [__]        │   │   │
│                     │ │ Y: [__]        │   │   │
│                     │ │ Z: [__]        │   │   │
│                     │ │ Flags:         │   │   │
│                     │ │ ☑ PAVED        │   │   │
│                     │ │ ☐ WATER        │   │   │
│                     │ └────────────────┘   │   │
│                     └──────────────────────┘   │
└──────────────────────────────────────────────────┘
```

---

## 🧪 Testing & Deployment

### Unit Tests

```csharp
using Xunit;
using CodeX.NaviGator.Formats;

namespace CodeX.NaviGator.Tests
{
    public class TrackFileFormatTests
    {
        [Fact]
        public void LoadTrack_WithValidData_ShouldParseCorrectly()
        {
            // Arrange
            var trackData = "474 455 close\nc 2493.36 -1482.21 45.15 ...";
            
            // Act
            var track = TrackDataFormat.Parse(trackData);
            
            // Assert
            Assert.Equal(474, track.NodeCount);
            Assert.Equal(455, track.CurveNodeCount);
            Assert.True(track.IsClosed);
        }
    }
}
```

### Integration Checklist

- [ ] NaviGatorTool loads in CodeX without errors
- [ ] Track files render correctly in viewport
- [ ] Navmesh polygons display with flag colors
- [ ] Hierarchy view shows all objects
- [ ] Properties panel updates in real-time
- [ ] Undo/Redo works for all operations
- [ ] File I/O (load/save) works correctly
- [ ] Game path configuration works
- [ ] Performance acceptable (60+ FPS)
- [ ] All unit tests pass

### Build & Deploy Steps

```powershell
# 1. Build solution
dotnet build CodeX.sln -c Release

# 2. Run tests
dotnet test CodeX.NaviGator.Tests -c Release

# 3. Create package
dotnet publish CodeX.NaviGator -c Release -o ./publish

# 4. Copy to CodeX plugins directory
Copy-Item -Path ./publish/* -Destination "./CodeX/plugins/NaviGator/" -Recurse
```

---

## 🔗 Dependencies & Requirements

### .NET Framework
- **Minimum:** .NET 6.0
- **Recommended:** .NET 8.0

### Libraries
- **CodeX.Core** - Editor framework
- **CodeX.Games.RDR2** - Game definitions
- **ImGui.NET** - UI framework
- **SharpDX** - DirectX wrapper (for rendering)

### Game Files
- Access to RDR3/RDR2 game installation
- Read permission on game folders
- Write permission for exported files

---

## 🚀 Deployment Options

### Option 1: Standalone Plugin (Recommended)
```
Deploy as CodeX plugin module
- Place in: CodeX/plugins/NaviGator/
- Auto-loads when CodeX starts
- Updates independently
- No core CodeX modifications
```

### Option 2: Integrated into CodeX
```
Compile into CodeX.exe
- Better performance
- Tighter integration
- Requires CodeX rebuild
- Simplifies distribution
```

### Option 3: Hybrid Approach
```
Core as plugin, optional features as DLLs
- Flexibility
- Modularity
- Complexity management
```

---

## 📚 Documentation & Support

### User Documentation
- Installation guide
- Quickstart tutorial
- Keyboard shortcuts
- Troubleshooting

### Developer Documentation
- Architecture overview
- API reference
- Extension points
- Contributing guidelines

### Resources
- CodeX Source: `H:\RP_Scripte\REDM_scripts\01_Tools\CodeX_030_src\`
- NaviGator Source: `d:\GitHub\NaviGator\`
- ImGui Documentation: https://github.com/ocornut/imgui/wiki

---

## ✅ Success Criteria

### Phase 1: Core Integration
- [x] CodeX module structure created
- [ ] NaviGatorTool registers in CodeX
- [ ] File loading works
- [ ] Basic rendering functional

### Phase 2: Feature Parity
- [ ] All track editing features working
- [ ] All navmesh editing features working
- [ ] Properties panel fully functional
- [ ] Gizmo controls working

### Phase 3: Polish & Optimization
- [ ] UI/UX matches CodeX standards
- [ ] Performance meets targets
- [ ] All tests passing
- [ ] Documentation complete

### Phase 4: Deployment
- [ ] Build process automated
- [ ] Plugin packaging complete
- [ ] Installation tested
- [ ] End-user documentation provided

---

## 📞 Contact & Questions

For questions about CodeX integration:
- CodeX Repository: https://github.com/...
- Issue Tracker: [link]
- Discord: [link]

For NaviGator specific:
- GitHub: https://github.com/Sage-of-Mirrors/NaviGator
- Issues: [GitHub Issues]

---

**Document Version:** 1.0  
**Last Updated:** 2026-07-04  
**Status:** Ready for Implementation

