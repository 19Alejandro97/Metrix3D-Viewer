# Revision funcional y tecnica de Metrix3D

Fecha de revision: 2026-05-31  
Repositorio: `git@github.com:19Alejandro97/Metrix3D-Viewer.git`  
Commit base revisado: `d959199 chore: baseline import`

## 1. Resumen ejecutivo

Metrix3D es una aplicacion nativa C++20 para Windows orientada a inspeccion y
manipulacion de mallas CAD. Usa OpenGL 4.5 para renderizar a un framebuffer,
Dear ImGui para la interfaz, ImGuizmo para manipuladores 3D, Assimp para
importacion y GLM para geometria.

El codigo actual implementa una base funcional amplia:

- Importacion de modelos 3D con Assimp.
- Gestion de escena multiobjeto.
- Viewport 3D con camara orbital, ViewCube, UCS y gizmo de transformacion.
- Materiales, opacidad, modos solido/wireframe/puntos y aristas duras.
- Metrologia de distancia, circulo y angulo con historial persistente en sesion.
- Snapping a superficies, aristas, loops y primitivas detectadas.
- Secciones por hasta 6 planos con caps por stencil.
- Alineacion geometrica limitada mediante restricciones secuenciales.
- Procesado basico de mallas: rellenar agujeros, suavizado, merge y bake.
- Settings JSON parciales y consola integrada.

La aplicacion compila en un build limpio aislado. No hay tests automatizados.
Varias funcionalidades existen, pero algunas son parciales o tienen deuda
arquitectonica relevante antes de seguir ampliando la app.

## 2. Estado de Git y build

El directorio fue inicializado como repositorio Git y conectado a:

```text
git@github.com:19Alejandro97/Metrix3D-Viewer.git
```

El commit baseline existe como:

```text
d959199 chore: baseline import
```

El usuario confirmo que el baseline ya fue empujado a GitHub.

### Build verificado

Comando de configuracion usado:

```powershell
cmake -S . -B out/metrix3d-build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE="C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake"
```

Resultado: configuracion correcta. vcpkg restauro/instalo dependencias y CMake
genero los archivos en:

```text
C:/Proyectos/Metrix3D Viewer/out/metrix3d-build
```

Comando de build usado:

```powershell
cmake --build out/metrix3d-build --config Release --target Metrix3D
```

Resultado: build correcto. Salida generada:

```text
C:/Proyectos/Metrix3D Viewer/out/metrix3d-build/Release/Metrix3D.exe
```

### Build historico del proyecto

El flujo historico del proyecto usa `build/`:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE="C:/Program Files/Microsoft Visual Studio/2022/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release --target Metrix3D
```

Salida esperada:

```text
build/Release/Metrix3D.exe
```

Advertencia importante: el `build/` local existente puede contener cache
generado antes del rename global del proyecto. Por eso no debe usarse como
evidencia de estado actual hasta regenerarlo limpiamente.

## 3. Mapa de arquitectura

### Entrada y ciclo principal

`src/main.cpp` crea la ventana GLFW, inicializa GLAD, ImGui, ImGuizmo, renderer,
camara, escena y UI. El loop principal ejecuta:

1. `glfwPollEvents()`
2. actualizacion de camara y resize del FBO segun viewport ImGui
3. render 3D a FBO con `Renderer::Render`
4. frame ImGui
5. `UIManager::DrawUI`
6. render final de ImGui y swap buffers

Tambien gestiona drag-and-drop de modelos 3D y guarda settings al salir.

### Modelo de escena

`SceneManager` mantiene:

- vector de `shared_ptr<SceneObject>`
- objeto seleccionado
- 6 planos de corte
- flags de escena

`SceneObject` contiene:

- `id` UUID
- `name`
- `shared_ptr<Mesh>`
- `Transform`
- `Material`
- flags `isVisible`, `isSelected`, `isActive`
- estado de ensamblaje/alineacion
- back pointer opcional a `SceneManager`

La diferencia entre seleccionado y activo es clave. Seleccionado controla el
panel de propiedades; activo controla operaciones sobre piezas.

### Mesh y geometria

`Mesh` mezcla varias responsabilidades:

- vertices e indices CPU
- AABB
- offset de importacion
- handles OpenGL
- caches de topologia y primitivas
- importacion Assimp
- export STL
- GPU upload/update
- delegacion a `TopologyEngine`

Este diseno funciona, pero es una fuente de acoplamiento. Cualquier cambio sobre
mallas debe revisar si afecta CPU, GPU, BVH, adjacency, edges y primitive cache.

### Topologia

`TopologyEngine` delega en:

- `AdjacencyBuilder`: vecinos de triangulos, adjacency de vertices, boundary
  edges, feature edges.
- `SpatialIndexer`: BVH para raycast.
- `PrimitiveAnalyzer`: clustering de planos/cilindros y RANSAC basico de esferas.
- `FeatureDetector`: aristas duras y extraccion de loops.
- `MeshProcessor`: suavizado Laplaciano y relleno de agujeros.

El flujo de importacion crea adjacency, BVH, aristas y luego lanza analisis de
primitivas en background desde `UIManager`.

### Render

`Renderer` renderiza a un framebuffer multisample y resuelve a textura ImGui.
Pases principales:

1. fondo plano o gradiente
2. objetos opacos y transparentes
3. caps de seccion con stencil
4. overlays de highlights y medidas
5. ghost plane de seccion
6. blit al FBO resuelto

Los shaders estan embebidos como headers en `src/render/shaders/`.

### UI y herramientas

`UIManager` orquesta:

- docking layout
- menu principal
- settings
- panel Tools
- panel Properties con Scene Explorer
- viewport
- status bar
- importacion async
- estado de medicion, alineacion, snap y gizmo

El viewport usa `ViewportInteractionManager` con cadena:

1. `MeasurementTool`
2. `AlignmentTool`
3. `SelectionTool`

Esto permite que medicion y alineacion consuman input antes que seleccion.

## 4. Flujo de datos principal

### Importacion

1. Usuario importa por dialogo o drag-and-drop.
2. `UIManager::ImportFromPath` crea un `std::future` con `Mesh::LoadFromFile`.
3. `Mesh::LoadFromFile` usa Assimp y toma solo `scene->mMeshes[0]`.
4. Calcula bounds, opcionalmente centra vertices, construye adjacency/BVH/edges.
5. `UIManager::ProcessBackgroundTasks` recoge el mesh listo en el hilo principal.
6. Ejecuta `mesh->UploadToGPU()`.
7. Crea `SceneObject`, lo selecciona/activa y enfoca la camara.
8. Lanza `mesh->AnalyzePrimitives()` en background.

### Snapping y picking

1. `DrawViewport` construye `ViewportEvent` con ray de pantalla a mundo.
2. `UpdateHoverSnapState` llama a `SnapEngine::Query`.
3. `SnapEngine` busca superficie mas cercana, primitivas detectadas y feature
   edges/boundary edges.
4. El snap resultante alimenta seleccion, medicion y alineacion.
5. Los highlights se actualizan mediante buffers GPU de la malla.

### Metrologia

1. Usuario activa `NEW MEASUREMENT`.
2. El modo actual define que se captura: puntos, planos, circulos o ejes.
3. Los clicks usan `HoverSnapState`.
4. Al completar la captura se guarda un `MeasurementAnnotation`.
5. El historial permite mostrar/ocultar, borrar y mover etiquetas.

### Alineacion

1. Usuario abre `Add Constraint`.
2. Escoge entidades fuente y destino.
3. `AlignmentTool` captura entidades desde snap o raycast.
4. Al aceptar, se agrega `ConstraintLine`.
5. `KinematicSolver::ApplySequentialConstraints` aplica la transformacion.
6. Se guarda `AlignCommand` para undo/redo.

El solver actual aplica una primera restriccion completa y una segunda
traslacion para relaciones coincident/concentric. La tercera y posteriores se
registran pero el solver las ignora con warning.

## 5. Inventario funcional

### Confirmado

- Ventana nativa Windows con OpenGL 4.5.
- Dear ImGui con docking y multi viewport.
- Tema oscuro custom.
- Carga de FontAwesome desde `assets/fa-solid-900.ttf`.
- Importacion de extensiones `.stl`, `.obj`, `.fbx`, `.gltf`, `.glb`, `.ply`.
- Drag-and-drop de archivos soportados.
- Exportacion binaria STL del objeto activo.
- Scene Explorer con activar/desactivar, visibilidad, seleccion, color y borrar.
- Paleta CAD de colores.
- Transform manual de posicion y rotacion.
- Edicion multi-activa de transform parcial.
- Material: color, opacidad, specular, gloss, modo solid/wireframe/points.
- Hard edges con umbral.
- Bake transformation a vertices.
- Merge de objetos activos.
- Scaling uniforme/no uniforme y conversion rapida mm/in.
- Rellenar agujeros por centroid umbrella.
- Suavizado Laplaciano.
- Camara orbit/pan/zoom/focus.
- ViewCube con cambio a ortografica al snap cardinal.
- UCS widget y UCS por pieza activa.
- Transform gizmo con translate/rotate/scale via ImGuizmo.
- Secciones por 6 planos, color y flip.
- Caps de seccion con stencil.
- Mediciones:
  - point-to-point
  - point-to-plane
  - plane-to-plane
  - center-to-center
  - circle auto
  - circle 3 puntos
  - plane-to-plane angle
  - axis-to-axis angle basado en normales capturadas
  - 3-point angle
- Historial de mediciones con visibilidad, borrado y etiquetas movibles.
- X-Ray snapping como modo de medicion.
- Handoff de circulo medido a objetivo de alineacion.
- Undo/redo de alineacion.
- Settings JSON parciales en `%APPDATA%/Metrix3D/settings.json`.
- Consola con `/help` y `/verify_measurement`.

### Parcial o limitado

- Importacion Assimp toma solo el primer mesh del archivo.
- README habla de multi-format import correctamente, pero el menu todavia dice
  `Import STL...` aunque acepta mas formatos.
- `AppSettings::qualityPreset` existe en UI pero no parece afectar al renderer.
- `gizmoSpace` existe pero `DrawTransformGizmo` fuerza `ImGuizmo::LOCAL`.
- Undo/redo no cubre transformaciones manuales, material, delete, merge, bake,
  smooth, fill holes ni scaling.
- `MeasureTab::Angle` axis-to-axis usa la misma extraccion de plano en el flujo
  actual, por lo que no es un eje cilindrico independiente robusto.
- Alineacion permite seleccionar tipos Sphere/Cylinder/Distance/Angle en UI,
  pero el solver no implementa toda esa semantica.
- `SceneObject::UpdateAssembledTransform` existe, pero no se ve integrado en el
  loop principal.
- `Mesh::m_shouldTerminate` se revisa en tareas largas, pero el destructor de
  `UIManager` solo espera 200 ms por future y luego limpia los vectores.

### No detectado

- Tests automatizados.
- CI.
- Serializacion de escenas.
- Guardado/carga de proyectos.
- Exportacion multiobjeto salvo merge manual previo.
- Manejo avanzado de materiales importados por Assimp.
- Picking por depth buffer real; el snapping usa raycast CPU/BVH.

## 6. README vs codigo real

| Tema README | Estado en codigo | Nota |
| --- | --- | --- |
| Arquitectura modular | Parcialmente real | Hay subsistemas separados, pero `Mesh` y `UIManager` concentran mucha responsabilidad. |
| Docking UI | Real | Usa `DockSpaceOverViewport` y DockBuilder. |
| Scene Explorer | Real | Panel en Properties. |
| ViewCube | Real | Usa `ImGuizmo::ViewManipulate`. |
| UCS indicator | Real | Hay widget y ejes por pieza activa. |
| Transform gizmo local/global | Parcial | UI state existe, pero el gizmo usa local fijo. |
| Import STL/OBJ/FBX/GLTF/GLB/PLY | Parcial | Assimp acepta formatos, pero solo primer mesh. |
| Measurement profesional | Real/parcial | Muchos modos existen, algunas entidades son aproximadas. |
| Internal X-Ray Snapping | Parcial | Implementado como bypass de oclusion en snapping. |
| Smart Alignment & Kinematics | Parcial | Solver limitado a primer constraint completo y segundo translation-only. |
| Mesh Repair | Real basico | Fill holes y smoothing existen. |
| Hard edges extraction | Real | FeatureDetector genera edge buffers. |
| Up to 6 clipping planes | Real | `SceneManager` inicializa 6 planos. |
| 4x MSAA/dynamic transparency | Real | MSAA configurable; transparentes ordenados por distancia. |

## 7. Riesgos tecnicos

### Alta prioridad

1. No hay tests automatizados.
   - Cambios en geometria, snapping, importacion o solver pueden romper flujos
     sin deteccion temprana.

2. `Mesh` mezcla CPU geometry, GPU state, IO, topology y processing.
   - Dificulta pruebas y aumenta riesgo de llamar OpenGL desde contexto errado.

3. Async de analisis primitivo sobre datos compartidos.
   - `PrimitiveAnalyzer` escribe `primitiveCache`, `triangleClusterIds` y flags
     mientras UI y snapping pueden consultar datos de malla.

4. `build/` local estaba cacheado contra ruta antigua.
   - Cualquier diagnostico basado en ese build puede ser falso hasta limpiarlo.

5. Solver de alineacion promete mas de lo que resuelve.
   - La UI permite relaciones y entidades que no tienen implementacion completa.

### Media prioridad

6. Undo/redo parcial.
   - Muchas acciones destructivas o geometricas no se pueden revertir.

7. Importacion toma solo el primer mesh Assimp.
   - Archivos CAD/FBX/GLTF con varias piezas pierden estructura.

8. Estados de seleccion/activacion/gizmo son sutiles.
   - Cambios de UI pueden dejar gizmo anclado a objetos borrados si no se limpia
     el estado.

9. Codificacion rota en textos.
   - Hay mojibake en README y algunos labels (`geometria`, grados, diametro).

10. Configuracion persistida es parcial.
   - Muchas opciones de `AppSettings` no se guardan.

### Baja prioridad

11. `download_imguizmo.cmake` descarga desde `master` sin version pin.
   - Reproducibilidad futura depende del repositorio externo.

12. UI con algunos comentarios legacy y codigo comentado.
   - Hay callbacks antiguos de mouse/scroll en `main.cpp` parcialmente
     desactivados.

13. `qualityPreset` aun no tiene efecto visible.
   - Puede confundir al usuario.

## 8. Recomendaciones antes de seguir mejorando

1. Crear un proyecto de tests minimo para geometria pura.
   - Cubrir `MathUtils`, `DatumEntity`, `FeatureDetector`, `MeshProcessor` y
     `KinematicSolver` sin OpenGL.

2. Separar `Mesh` en responsabilidades.
   - Mantener `MeshGeometry` CPU, `MeshGpuBuffers`, `MeshTopologyCache` y
     loaders/processors como unidades testeables.

3. Definir contrato de threading.
   - Documentar que import/analysis pueden correr en worker, pero GPU y
     mutaciones UI deben ocurrir en hilo principal.

4. Endurecer el solver de alineacion o reducir UI a lo soportado.
   - Evitar prometer Distance/Angle/Sphere/Cylinder si no hay solucion completa.

5. Ampliar command stack.
   - Hacer undo/redo para transform, delete, merge, bake, scaling y processing.

6. Mantener consistente el naming Metrix3D.
   - El producto, proyecto CMake, target, binario, settings path y cadenas
     visibles deben usar `Metrix3D`.

7. Regenerar `build/` limpiamente si se quiere mantener esa carpeta como salida
   local principal.
   - No versionarla.
   - Confirmar `build/Release/Metrix3D.exe`.

8. Corregir textos con codificacion rota.
   - Usar UTF-8 de forma consistente o mantener ASCII en codigo/documentacion.

## 9. Guia para proximos cambios

- Antes de tocar funcionalidades, comprobar si dependen de `m_isAnalysisReady`.
- Para cambios de snapping, revisar `SnapEngine`, `FeatureExtraction`,
  `DrawSnapCursor` y `MeasurementTool`.
- Para cambios de medicion, revisar `MeasurementState`, `MeasurementAnnotation`,
  `DrawMeasurementPanel`, `MeasurementTool` y `DrawMeasurementAnnotations`.
- Para cambios de alineacion, revisar `AlignmentState`, `AlignmentTool`,
  `DrawAlignmentPanel`, `KinematicSolver` y `UndoManager`.
- Para cambios de render, revisar `Renderer::Render` y los pases en
  `renderer_passes.cpp`; validar con modelos opacos, transparentes y clip planes.
- Para cambios de import/export, revisar `Mesh::LoadFromFile`,
  `UIManager::ImportFromPath`, `ProcessBackgroundTasks` y `Mesh::ExportSTL`.
- No asumir que una operacion CPU actualiza GPU. Confirmar llamadas a
  `UploadToGPU`, `UpdateHighlightGPU`, `GenerateEdges` o equivalentes.
