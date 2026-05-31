# AGENTS.md

## Contexto Del Proyecto

Metrix3D es una aplicacion nativa Windows para inspeccion CAD y trabajo con
mallas 3D. El producto combina importacion de modelos, gestion de escena,
render OpenGL, metrologia, snapping, secciones, alineacion geometrica y
procesado basico de mallas.

Nombre canonico actual:

- Producto visible: `Metrix3D`
- Proyecto CMake: `Metrix3D`
- Target CMake: `Metrix3D`
- Ejecutable: `Metrix3D.exe`
- Carpeta de settings: `%APPDATA%/Metrix3D/settings.json`

No renombrar `DatumEntity` ni `datum_entity.*`. En ese contexto, "datum" es un
termino CAD/metrologico y no branding antiguo del proyecto.

## Stack

- Lenguaje: C++20
- Build: CMake 3.26+, Visual Studio 2022, vcpkg manifest mode
- Graficos: OpenGL 4.5 Core Profile, GLAD, GLFW
- UI: Dear ImGui docking, ImGuizmo, FontAwesome como asset local
- Matematicas: GLM
- Importacion: Assimp
- Settings JSON: RapidJSON
- Vendor local: `src/vendor/ImGuizmo`

## Build Y Ejecucion

Build aislado recomendado para verificaciones:

```powershell
cmake -S . -B out/metrix3d-build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE="C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build out/metrix3d-build --config Release --target Metrix3D
```

Salida esperada:

```text
out/metrix3d-build/Release/Metrix3D.exe
```

Salida convencional del proyecto:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE="C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release --target Metrix3D
```

Salida esperada:

```text
build/Release/Metrix3D.exe
```

Si `build/` existe desde antes del rename, regenerarlo antes de usarlo como
evidencia. `build/` y `out/` son artefactos locales ignorados por Git.

## Reglas Para Cambios Futuros

- No editar ni commitear artefactos generados:
  - `build/`
  - `out/`
  - `CMakeFiles/`
  - `vcpkg_installed/`
  - `*.exe`, `*.dll`, `*.obj`, `*.pdb`, `*.lib`
- Mantener cambios manuales acotados. La app no tiene tests automatizados y hay
  acoplamiento fuerte entre UI, render, escena y mallas.
- No llamar funciones OpenGL desde workers. Importacion y analisis pueden correr
  en background, pero la subida a GPU y reconstruccion de buffers debe ocurrir
  en el hilo principal/render.
- Respetar los limites de `SceneManager` y `UIManager`. `SceneManager` es el
  propietario logico de objetos, seleccion y planos de corte; `UIManager`
  orquesta estado de herramientas, tareas async y paneles.
- Tratar `UIManager::meshLoadTasks` y `analysisTasks` con cuidado. Controlan
  importacion async y deteccion de primitivas; no mutar la misma malla desde UI
  mientras una tarea pueda estar leyendola o escribiendola.
- Si se cambian datos CPU de una malla, revisar si tambien hay que actualizar
  buffers GPU, BVH, adjacency, feature edges y cache de primitivas.
- La pila de undo actual es parcial. Principalmente cubre alineacion, no todas
  las acciones de transformacion, material, delete, merge o procesado.
- La diferencia entre seleccion y activacion es intencional:
  - `isSelected`: objeto principal para propiedades/UI.
  - `isActive`: conjunto operativo para transform, merge, clipping y alineacion.

## Mapa De Subsistemas

- `src/main.cpp`: entrada de la app, setup GLFW/OpenGL/ImGui, main loop,
  drag-and-drop, carga/guardado de settings.
- `src/core/`: modelo de dominio, geometria y persistencia:
  - `Mesh`: geometria CPU, handles GPU, import/export y caches de topologia.
  - `SceneObject`: transform, material, bounds, raycast y estado de ensamblaje.
  - `SceneManager`: lista de objetos, seleccion y planos de corte.
  - `Camera`: orbit/pan/zoom/focus y proyeccion.
  - `MathUtils`: raycasts, BVH, circulos, esferas, lineas y utilidades.
  - `FeatureExtraction`: extraccion de planos/circulos desde topologia.
  - `KinematicSolver`: solver secuencial limitado para alineacion.
  - `SnapEngine`: snapping contra superficies, features y primitivas.
  - `SettingsIO`: persistencia JSON parcial.
- `src/core/topology/`: adjacency, BVH, primitive analysis, feature edges,
  smoothing y hole filling.
- `src/render/`: FBO, shaders, pasadas de malla, edges, caps de seccion y ghost
  planes.
- `src/ui/`: orquestacion ImGui, paneles, consola y estado compartido de UI.
- `src/ui/viewport/`: imagen del viewport, camara, gizmo, snapping, overlays,
  mediciones e interacciones.
- `src/vendor/`: dependencias locales integradas en el build.

## Revision Funcional

La revision de funcionalidades, riesgos y recomendaciones vive en:

```text
docs/revision-funcional.md
```

Usar ese documento y el codigo como fuente de verdad antes de ampliar la app.
