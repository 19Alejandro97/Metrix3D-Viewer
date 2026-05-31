# AGENTS.md

## Project Context

Metrix3D is a native Windows CAD/mesh viewer focused on 3D inspection workflows:
model import, scene management, OpenGL rendering, metrology, snapping, section
analysis, geometric alignment, and mesh repair tools.

Important naming note: the product/project name is now **Metrix3D**, but several
technical identifiers are still legacy:

- CMake project: `D3tumViewer`
- CMake target: `D3tumViewer`
- Executable: `D3tumViewer.exe`
- Settings folder in code: `%APPDATA%/D3tumViewer`
- README still contains legacy `D3tum Viewer` text

Do not rename these identifiers casually. Treat a full rename as a separate,
tested refactor.

## Stack

- Language: C++20
- Build: CMake 3.26+, Visual Studio 2022 generator, vcpkg manifest mode
- Graphics: OpenGL 4.5 Core Profile, GLAD, GLFW
- UI: Dear ImGui docking branch, ImGuizmo, FontAwesome font asset
- Math: GLM
- Import: Assimp
- JSON settings: RapidJSON
- Local vendor code: `src/vendor/ImGuizmo`

## Build And Run

Preferred verification build, isolated from historical generated files:

```powershell
cmake -S . -B out/review-build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE="C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build out/review-build --config Release --target D3tumViewer
```

This produces:

```text
out/review-build/Release/D3tumViewer.exe
```

The historical/local project build convention uses:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE="C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release --target D3tumViewer
```

That produces:

```text
build/Release/D3tumViewer.exe
```

Current caution: the checked local `build/` directory was generated for an older
path, `C:/Proyectos/D3tum Viewer`, and should not be trusted. It is ignored by
Git. Use a clean build directory unless intentionally regenerating `build/`.

## Repository Rules

- Do not edit or commit generated build artifacts:
  - `build/`
  - `out/`
  - `CMakeFiles/`
  - `vcpkg_installed/`
  - `*.exe`, `*.dll`, `*.obj`, `*.pdb`, `*.lib`
- Keep manual source edits scoped. This codebase has many coupled UI/render/core
  paths and no automated tests yet.
- Do not call OpenGL functions from background worker threads. Mesh import and
  primitive analysis run asynchronously, but GPU upload and GPU buffer rebuilds
  must stay on the main/render thread.
- Treat `SceneObject::scene` as an owning-context back pointer, not an ownership
  model. Check it when changing clipping or rendering code.
- Selection and activation are intentionally different concepts:
  - `isSelected`: primary UI selection/properties target
  - `isActive`: operation set for transform, merge, clipping, alignment
- Be careful with `UIManager::meshLoadTasks` and `analysisTasks`. They control
  async import and primitive detection; do not mutate mesh data from the UI while
  an analysis task may still be reading/writing the same mesh.
- The renderer assumes mesh GPU buffers are valid when drawing. If mesh CPU data
  changes, rebuild relevant GPU buffers.
- The current undo stack is narrow. It mainly covers alignment commands, not all
  transform/material/mesh processing actions.
- The README is aspirational in places. Use `docs/revision-funcional.md` and the
  source code as the current ground truth.

## Subsystem Map

- `src/main.cpp`: application entry point, GLFW/OpenGL/ImGui setup, main loop,
  drag-and-drop import callback, global callback pointers.
- `src/core/`: domain model and geometry kernels:
  - `Mesh`: CPU geometry, GPU buffer handles, import/export, repair delegation.
  - `SceneObject`: transform, material, raycast, world bounds, assembly state.
  - `SceneManager`: object list, selected object, clipping planes.
  - `Camera`: orbit/pan/zoom/focus and projection state.
  - `MathUtils`: ray casts, BVH traversal, circle/sphere/line math.
  - `FeatureExtraction`: plane/circle extraction from mesh topology.
  - `KinematicSolver`: limited sequential alignment solver.
  - `SnapEngine`: hover snap query against surfaces, features, primitives.
  - `SettingsIO`: JSON persistence for selected app settings.
- `src/core/topology/`: adjacency, BVH, primitive analysis, feature edges, mesh
  smoothing and hole filling.
- `src/render/`: FBO setup, shader management, main mesh passes, edge overlays,
  section caps, ghost plane rendering.
- `src/ui/`: ImGui orchestration, panels, shared UI state, console.
- `src/ui/viewport/`: viewport image, camera controls, ImGuizmo, snapping
  overlays, measurement drawings, interaction tool chain.
- `assets/`: runtime UI assets copied after build.

## Current Verification Snapshot

On 2026-05-31, a clean isolated configure and Release build succeeded with the
Visual Studio 2022 Community vcpkg toolchain:

```text
out/review-build/Release/D3tumViewer.exe
```

See `docs/revision-funcional.md` for the complete functionality review, risks,
and recommended next work.
