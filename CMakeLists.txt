cmake_minimum_required(VERSION 3.26)
project(Metrix3D VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Find packages via vcpkg
find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(RapidJSON CONFIG REQUIRED)

# 2. Handle Vendor Dependencies (ImGuizmo)
if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/ImGuizmo/ImGuizmo.h")
    message(STATUS "ImGuizmo not found. Downloading...")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/ImGuizmo")
    include(download_imguizmo.cmake)
endif()

# --- Core Executable ---
set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/mesh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/scene_object.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/camera.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/math_utils.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/feature_extraction.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/kinematic_solver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/topology_engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/topology/adjacency_builder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/topology/spatial_indexer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/topology/primitive_analyzer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/topology/feature_detector.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/topology/mesh_processor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/datum_entity.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/settings_io.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/scene_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/undo_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/core/snap_engine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/shader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/renderer_core.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/render/renderer_passes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/ui_manager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/viewport/ui_viewport_main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/viewport/ui_viewport_interaction.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/viewport/ui_viewport_camera_ctrl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/viewport/tools/ui_viewport_tools.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/viewport/ui_viewport_gizmo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/viewport/ui_viewport_overlays.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/viewport/ui_viewport_snapping.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/panels/ui_panel_tools.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/panels/ui_panel_explorer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/panels/ui_panel_properties.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/panels/ui_panel_metrology.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/ImGuizmo/ImGuizmo.cpp
)

add_executable(Metrix3D ${SOURCES})

# --- Includes & Links ---
target_include_directories(Metrix3D PRIVATE
    src/
    src/vendor/
)

target_link_libraries(Metrix3D PRIVATE
    glfw
    glad::glad
    glm::glm
    imgui::imgui
    assimp::assimp
    rapidjson
)

# On Windows, link the standard OpenGL library and suppress the console window
if (WIN32)
    target_link_libraries(Metrix3D PRIVATE opengl32)
    # WIN32_EXECUTABLE hides the console; /ENTRY:mainCRTStartup keeps int main() as entry point
    set_target_properties(Metrix3D PROPERTIES
        WIN32_EXECUTABLE TRUE
        LINK_FLAGS "/ENTRY:mainCRTStartup"
    )
endif()

# Define ImGui Backend constants
target_compile_definitions(Metrix3D PRIVATE
    IMGUI_IMPL_OPENGL_LOADER_GLAD
)

# --- Asset & DLL Deployment ---
# Copy assets directory and required vcpkg DLLs to the output folder after build
add_custom_command(TARGET Metrix3D POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:Metrix3D>/assets"
    
    # Copy all DLLs from vcpkg_installed to the binary directory for runtime linkage
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin/assimp-vc143-mt.dll"
    "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin/draco.dll"
    "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin/glfw3.dll"
    "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin/kubazip.dll"
    "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin/minizip.dll"
    "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin/poly2tri.dll"
    "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin/pugixml.dll"
    "${CMAKE_BINARY_DIR}/vcpkg_installed/x64-windows/bin/zlib1.dll"
    "$<TARGET_FILE_DIR:Metrix3D>"
    
    COMMENT "Copying assets and vcpkg DLLs to build directory..."
)

# --- Installation ---
install(TARGETS Metrix3D RUNTIME DESTINATION bin)
install(DIRECTORY assets DESTINATION bin)
