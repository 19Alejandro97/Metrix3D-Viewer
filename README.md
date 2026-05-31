# Metrix3D

Metrix3D is a native Windows CAD and mesh viewer for 3D inspection workflows.
It focuses on importing mesh assets, scene management, OpenGL rendering,
metrology, snapping, section analysis, geometric alignment, and basic mesh
processing.

## Key Features

### User Experience

- Modular application structure with separate core, render, UI, viewport, and
  topology subsystems.
- Dear ImGui docking interface with scene explorer, visibility toggles,
  selection state, material controls, and tool panels.
- 3D viewport with orbit camera, ViewCube, UCS indicator, and ImGuizmo
  transform controls.

### Metrology

- Distance measurements: point-to-point, point-to-plane, plane-to-plane,
  center-to-center, and X-Ray snapping mode.
- Circle measurements: auto-detection and manual 3-point capture.
- Angle measurements: plane-to-plane, axis-to-axis, and 3-point angle.
- Session history with visibility toggles and movable annotations.

### Alignment And Kinematics

- Geometric alignment workflow using captured planes, circles, and related
  entities.
- Assembly lock state and indexing angle controls.
- Measurement-to-alignment handoff for selected geometric features.

### Geometry Processing

- Import through Assimp for common mesh formats: STL, OBJ, FBX, GLTF, GLB, and
  PLY.
- Binary STL export for the active object.
- Mesh repair operations including hole filling, Laplacian smoothing, hard edge
  extraction, active-object merge, and bake transformations.

### Rendering

- OpenGL 4.5 renderer with framebuffer rendering for the ImGui viewport.
- Solid, wireframe, points, transparency, hard-edge overlays, and section
  analysis.
- Up to 6 clipping planes with section caps.

## Technology Stack

| Component | Technology |
| --- | --- |
| Language | C++20 |
| Build | CMake 3.26+, Visual Studio 2022, vcpkg |
| Graphics | OpenGL 4.5 Core Profile, GLAD, GLFW |
| UI | Dear ImGui docking, ImGuizmo |
| Math | GLM |
| Import | Assimp |
| Settings | RapidJSON |

## Build Guide

### Prerequisites

- Git
- CMake 3.26 or newer
- Visual Studio 2022 with the "Desktop development with C++" workload
- vcpkg toolchain bundled with Visual Studio or another compatible vcpkg
  installation

### Clone

```powershell
git clone git@github.com:19Alejandro97/Metrix3D-Viewer.git
cd Metrix3D-Viewer
```

### Configure And Build

Run these commands from the project root:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE="C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release --target Metrix3D
```

Expected executable:

```text
build/Release/Metrix3D.exe
```

### Run

```powershell
.\build\Release\Metrix3D.exe
```

The build copies runtime assets and required vcpkg DLLs into the output
directory after compilation.

## Source Structure

- `src/main.cpp`: entry point, GLFW/OpenGL/ImGui setup, main loop, settings, and
  drag-and-drop import.
- `src/core/`: scene model, mesh data, import/export, settings, undo, snapping,
  math, feature extraction, and alignment solver.
- `src/core/topology/`: adjacency, BVH, primitive analysis, feature detection,
  smoothing, and hole filling.
- `src/render/`: shader management, framebuffer setup, mesh passes, overlays,
  and section rendering.
- `src/ui/`: ImGui orchestration, panels, viewport, tools, console, and
  interaction state.
- `src/vendor/ImGuizmo/`: local ImGuizmo source.
- `assets/`: runtime fonts and UI assets copied during build.

## Notes For Contributors

- The canonical product name, CMake project, CMake target, and executable are
  `Metrix3D`.
- Keep `DatumEntity` and `datum_entity.*` unchanged. In that context, "datum"
  is a CAD/metrology concept, not project branding.
- Do not edit generated build directories such as `build/` or `out/`.
- See `AGENTS.md` for agent workflow rules and `docs/revision-funcional.md` for
  the current functionality review.

## License

Metrix3D is private software. All rights reserved. See the `LICENSE` document
for complete legal permissions and restrictions.
