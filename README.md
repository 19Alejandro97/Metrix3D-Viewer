# D3tum Viewer 🚀

## ✨ Key Features

### 🖥️ Premium User Experience
- **Modular Architecture**: Clean separation between the UI orchestrator, 3D viewport, and tool systems.
- **Docking UI**: Sophisticated visual design featuring dark mode, micro-interactions, responsive fluid layouts, and a full-featured **Scene Explorer** for multi-object management, visibility toggles, and color palettes.
- **Advanced Navigation**: Integrated 3D **ViewCube** with UCS (User Coordinate System) indicator, and an interactive transform gizmo (translation, rotation, scaling) with local/global space support.

### 📐 Professional Metrology
- **Distance**: Point-to-Point, Point-to-Plane, Plane-to-Plane, Center-to-Center, and **Internal X-Ray Snapping**.
- **Circle**: Smart Auto-Detection and 3-Point Manual with diameter/radius toggles.
- **Angle**: Plane-to-Plane, Axis-to-Axis, and 3-Point Angle.
- **Interactive History**: Tracks all measurements with triaxial UCS breakdowns.

### ⚙️ Smart Alignment & Kinematics
- **Geometric Assembly**: Align parts using geometric entities (planes, circles) with automatic Radii Match validation.
- **Kinematic Controls**: Lock assemblies, apply indexing angles (e.g., +/- 60 deg for hex bolts), or unlock for free movement.
- **Smart Assembly Bridge**: Directly handoff measured features as alignment targets.

### 🧩 Advanced Geometry Processing
- **Native Importer**: Seamlessly read, import, and drag-and-drop multiple industry-standard formats (**STL, OBJ, FBX, GLTF, GLB, PLY**).
- **Mesh Repair**: Auto-Fill Holes via Centroid Umbrella algorithms, Laplacian Relaxation (Smoothing) with adjustable iterations, and Hard Edges extraction based on angle thresholds.
- **Operations**: Merge active meshes into a single object and **Bake Transformations** directly into vertex telemetry.

### 🎨 High-Quality Rendering
- **Material Properties**: Granular control over Opacity, Specular, and Gloss.
- **Visualization Modes**: Solid, Wireframe, Points, and Section Analysis (up to 6 simultaneous clipping planes).
- **Anti-Aliasing**: 4x MSAA (Multi-Sample Anti-Aliasing) and dynamic transparency.

---

## 🛠️ Technology Stack

| Component | Technology | Description |
|-----------|------------|-------------|
| **Language** | C++20 | Core application logic and performance. |
| **Graphics API** | OpenGL 4.5 Core Profile | High-performance rendering pipeline. |
| **GUI Framework** | Dear ImGui (Docking Branch) | Hardware-accelerated user interface. |
| **Mathematics** | GLM | OpenGL Mathematics library for 3D calculations. |
| **Model Loading** | Assimp | Asset importer for STL, OBJ, FBX, GLTF, etc. |
| **Package Manager**| vcpkg | C++ dependency resolving. |
| **Build System** | CMake 3.26+ | Cross-platform build configuration. |

---

### 🚀 Installation & Build Guide (Windows)

Follow these steps to set up D3tum Viewer on a brand-new computer.

#### 1. Prerequisites (Verification)
Ensure the following are installed and accessible from your terminal (**PowerShell** or **Git Bash**):
- **Git**: Run `git --version` to verify.
- **CMake**: Run `cmake --version` to verify (version 3.26+ required).
- **C++ Build Tools**: Install [Visual Studio 2022](https://visualstudio.microsoft.com/downloads/) with the **"Desktop development with C++"** workload.

#### 2. Get the Source Code
```bash
git clone https://github.com/YOUR-USERNAME/D3tumViewer.git
cd D3tumViewer
```

#### 3. Locate the `vcpkg` Toolchain
This project manages dependencies using the `vcpkg` integrated with Visual Studio. You need the path to `vcpkg.cmake`:

> 📍 **Standard Path**: `C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/vcpkg/scripts/buildsystems/vcpkg.cmake`  
> *(If using VS Community, replace `BuildTools` with `Community`)*

#### 4. Configure & Build
Run these commands from the project root (`D3tumViewer/`):

```bash
# 1. Generate project (Replaces "..." with the path from Step 3)
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/vcpkg/scripts/buildsystems/vcpkg.cmake"

# 2. Compile the project
cmake --build build --config Release
```

> 💡 **Tip:** The first build will automatically download and compile all dependencies (`glfw`, `assimp`, etc.) listed in `vcpkg.json`.

#### 5. Run the Application
The build process handles the deployment of all required DLLs and assets automatically.

```bash
.\build\Release\D3tumViewer.exe
```

---

## 📂 Source Code Structure

- `src/core/`: 
    - `topology_engine.cpp`: Mesh analysis, BVH, and feature extraction.
    - `mesh.cpp`: GPU-encapsulated geometry container.
    - `math_utils.cpp`: Pure linear algebra and geometric primitives.
- `src/render/`: Shaders (embedded) and low-level OpenGL renderer.
- `src/ui/`: 
    - `ui_overlays.cpp`: Dedicated 2D/3D viewport drawings.
    - `ui_manager.cpp`: Layout and orchestration.
- `assets/`: UI assets (Fonts/Icons) deployed automatically during build.

---

## 📜 License

D3tum Viewer is private software. All rights reserved. See the `LICENSE` document for complete legal permissions and restrictions.
