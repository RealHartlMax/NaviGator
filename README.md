# NaviGator

> This project is no longer under active development. Feel free to fork and continue it.

---

## English

### What this project is
NaviGator is a C++17 desktop OpenGL tool for inspecting and editing railway/nav-related game data.  
It focuses on:

- loading and rendering `.ynv` navmesh files,
- loading/editing/saving railway tracks from `traintracks.xml` + `.dat` files,
- simple 3D viewport interaction, picking, and gizmo-based node editing,
- optional conversion support for `swrailroad.wsi` (RDR1 utility flow).

### Core architecture

- **Application layer** (`include/application`, `src/application`)
  - app lifecycle (`AApplication`, `AGatorApplication`)
  - central orchestration (`AGatorContext`)
  - camera/input/options management
- **Track domain layer** (`include/tracks`, `src/tracks`)
  - track config serialization (XML)
  - node point parsing/saving (`.dat`)
  - junction/station/tunnel metadata
- **UI + rendering layer** (`include/ui`, `src/ui`, `asset/shader`)
  - main viewport framebuffer
  - object ID picking buffer
  - path rendering + shader programs
- **Utilities** (`include/util`, `src/util`)
  - shader loading
  - helper UI elements
  - RDR1 railroad extraction helper

### Tech stack

- C++17, CMake (>= 3.12)
- OpenGL (GLAD), GLFW3
- ImGui + ImGuizmo + ImGuiFileDialog
- GLM
- pugixml
- Recast
- librdr3
- Tracy (profiling hooks in code)

### Build

1. Clone with submodules:
   ```bash
   git submodule update --init --recursive
   ```
2. Configure:
   ```bash
   cmake -S . -B build
   ```
3. Build:
   ```bash
   cmake --build build
   ```

> Note: `GLFW3` must be discoverable by CMake (`find_package(GLFW3 REQUIRED)`).

### Runtime files

- Shaders: `asset/shader/*`
- Font: `asset/font/MaterialSymbolsRounded.ttf`
- Options file (generated): `navigator.xml`

### Current status

- No automated test suite is configured in this repository.
- Build in this environment currently fails at configure time if GLFW3 is not installed/discoverable.

---

## Deutsch

### Worum es in diesem Projekt geht
NaviGator ist ein C++17-Desktoptool auf OpenGL-Basis zum Anzeigen und Bearbeiten von eisenbahn-/navigationsbezogenen Spieldaten.  
Hauptfunktionen:

- Laden und Rendern von `.ynv`-Navmesh-Dateien,
- Laden/Bearbeiten/Speichern von Gleisdaten aus `traintracks.xml` + `.dat`,
- 3D-Viewport-Interaktion mit Picking und Gizmo-Manipulation,
- optionale Konvertierung für `swrailroad.wsi` (RDR1-Helferfunktion).

### Kernarchitektur

- **Application-Schicht** (`include/application`, `src/application`)
  - App-Lebenszyklus (`AApplication`, `AGatorApplication`)
  - zentrale Steuerung (`AGatorContext`)
  - Kamera/Input/Optionen
- **Track-Domain** (`include/tracks`, `src/tracks`)
  - XML-Serialisierung der Track-Konfiguration
  - Parsen/Speichern der Node-Daten (`.dat`)
  - Junction-/Stations-/Tunnel-Metadaten
- **UI + Rendering** (`include/ui`, `src/ui`, `asset/shader`)
  - Viewport-Framebuffer
  - ID-Picking-Buffer
  - Pfad-Rendering + Shader
- **Utilities** (`include/util`, `src/util`)
  - Shader-Loading
  - UI-Helfer
  - RDR1-Railroad-Extraktion

### Technologie-Stack

- C++17, CMake (>= 3.12)
- OpenGL (GLAD), GLFW3
- ImGui + ImGuizmo + ImGuiFileDialog
- GLM
- pugixml
- Recast
- librdr3
- Tracy (Profiling-Hooks)

### Build

1. Submodule initialisieren:
   ```bash
   git submodule update --init --recursive
   ```
2. Konfigurieren:
   ```bash
   cmake -S . -B build
   ```
3. Bauen:
   ```bash
   cmake --build build
   ```

> Hinweis: `GLFW3` muss für CMake auffindbar sein (`find_package(GLFW3 REQUIRED)`).

### Laufzeitdateien

- Shader: `asset/shader/*`
- Font: `asset/font/MaterialSymbolsRounded.ttf`
- Optionen-Datei (wird erzeugt): `navigator.xml`

### Aktueller Zustand

- Es gibt derzeit keine automatisierten Tests in diesem Repository.
- In dieser Umgebung schlägt die Konfiguration fehl, wenn GLFW3 nicht installiert/auffindbar ist.
