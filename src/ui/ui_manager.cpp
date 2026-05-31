#include "ui_manager.hpp"
#include "../core/scene_manager.hpp"
#include "../core/scene_object.hpp"
#include "../core/camera.hpp"
#include "../core/mesh.hpp"
#include "ImGuizmo/ImGuizmo.h"
#include "app_console.hpp"
#include "../core/app_settings.hpp"
#include <imgui_internal.h>
#include "IconsFontAwesome5.h"
#include <glm/gtc/type_ptr.hpp>
#include "../render/renderer.hpp"
#include <algorithm>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#endif
#include <map>
#include <iostream>

static ImVec4 ToImVec4(const Vec3& v, float a = 1.0f) {
    return ImVec4((float)v.r, (float)v.g, (float)v.b, a);
}

// ===========================================================================
// UIManager Orchestration
// ===========================================================================
UIManager::UIManager() { 
    ApplyStylePremium_Impl(); 
}
UIManager::~UIManager() {
    std::cout << "[Shutdown] UIManager destructor started." << std::endl;
    Mesh::m_shouldTerminate = true;
    if (!meshLoadTasks.empty() || !analysisTasks.empty()) {
        std::cout << "[UIManager] Waiting for background tasks to finish before exit..." << std::endl;
        for (auto& task : meshLoadTasks) { if (task.valid()) task.wait_for(std::chrono::milliseconds(200)); }
        for (auto& task : analysisTasks) { if (task.valid()) task.wait_for(std::chrono::milliseconds(200)); }
        meshLoadTasks.clear();
        analysisTasks.clear();
    }
}

void UIManager::ApplyStyleDark()      { ApplyStyleDark_Impl(); }
void UIManager::ApplyStyleDark_Impl() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImGui::StyleColorsDark(&style);
}

void UIManager::ApplyStylePremium_Impl() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 6.0f;
    style.ChildRounding     = 6.0f;
    style.FrameRounding     = 4.0f;
    style.PopupRounding     = 4.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabRounding      = 4.0f;
    style.TabRounding       = 4.0f;
    style.WindowBorderSize  = 0.0f;
    style.FrameBorderSize   = 1.0f;
    style.FramePadding      = {8, 6};
    style.ItemSpacing       = {10, 5};

    ImVec4* c = style.Colors;
    c[ImGuiCol_WindowBg]         = {0.11f, 0.11f, 0.13f, 1.00f};
    c[ImGuiCol_ChildBg]          = {0.09f, 0.09f, 0.11f, 1.00f};
    c[ImGuiCol_PopupBg]          = {0.14f, 0.14f, 0.16f, 1.00f};
    c[ImGuiCol_Border]           = {0.18f, 0.18f, 0.22f, 1.00f};
    c[ImGuiCol_FrameBg]          = {0.16f, 0.16f, 0.19f, 1.00f};
    c[ImGuiCol_FrameBgHovered]   = {0.23f, 0.23f, 0.27f, 1.00f};
    c[ImGuiCol_Button]           = {0.22f, 0.22f, 0.26f, 1.00f};
    c[ImGuiCol_ButtonHovered]    = {0.20f, 0.45f, 0.45f, 1.00f}; 
    c[ImGuiCol_ButtonActive]     = {0.15f, 0.35f, 0.35f, 1.00f};
    c[ImGuiCol_Header]           = {0.18f, 0.35f, 0.35f, 0.40f};
    c[ImGuiCol_Text]             = {0.90f, 0.90f, 0.92f, 1.00f};
}

void UIManager::ResetGizmoState() {
    isGizmoOnObject  = false;
    gizmoAnchorId.clear();
    isDraggingToDock = false;
}

void UIManager::DockToPiece(SceneManager& scene, const std::string& id) {
    std::shared_ptr<SceneObject> obj;
    for (auto& o : scene.GetObjects()) {
        if (o->id == id) {
            obj = o;
            break;
        }
    }
    if (!obj) return;
    
    isGizmoOnObject = true;
    gizmoAnchorId = id;
    
    // Ensure piece is selected and active
    for (auto& o : scene.GetObjects()) o->isSelected = false;
    obj->isSelected = true;
    obj->isActive = true;
    scene.SetSelectedObject(obj);
}

void UIManager::CleanStateForDeletedObject(const std::string& deletedId) {
    alignState.CleanForObject(deletedId);
    measureState.Reset();
    if (gizmoAnchorId == deletedId) ResetGizmoState();
}

void UIManager::DrawUI(SceneManager& scene, Camera& camera, unsigned int viewportTextureID) {
    auto& s = AppSettings::Instance();
    camera.isOrthographic = s.isOrthographic;
    camera.fovY           = (double)s.fovY;

    ImGuiIO& io = ImGui::GetIO();
    io_WantsCaptureMouse = io.WantCaptureMouse;

    // --- Global Shortcuts ---
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
        undoManager.Undo(scene, alignState);
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
        undoManager.Redo(scene, alignState);
    }

    // --- Command Registration ---
    static bool commandsInitialized = false;
    if (!commandsInitialized) {
        commandsInitialized = true;
        AppConsole::Instance().SetCommandCallback([this](const std::string& cmd_line) {
            if (cmd_line == "/verify_measurement") {
                if (this->measureState.history.empty()) {
                    AppConsole::Log("[ERROR] No persistent measurements found to verify.");
                    return;
                }
                const auto& last = this->measureState.history.back();
                if (last.type == MeasureTab::Distance && last.points.size() == 2) {
                    AppConsole::Log("[VERIFY] Last Distance (World): " + std::to_string(last.value) + " mm");
                } else if (last.type == MeasureTab::Circle) {
                    AppConsole::Log("[VERIFY] Last Circle Diameter (World): " + std::to_string(last.value) + " mm");
                    if (std::abs(last.value - 2.400) < 0.001) AppConsole::Log("[VERIFY] CALIBRATION PASS: 2.400mm verified.");
                }
            } else if (cmd_line == "/help") {
                AppConsole::Log("[INFO] Available Commands: /verify_measurement, /help");
            }
        });
    }

    ImGuiViewport* mainViewport = ImGui::GetMainViewport();
    ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, mainViewport, ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_NoTabBar);

    if (!layoutInitialized) {
        layoutInitialized = true;
        ImGui::DockBuilderRemoveNode(dockspace_id);
        ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_NoTabBar);
        ImGui::DockBuilderSetNodeSize(dockspace_id, mainViewport->Size);

        ImGuiID dock_center = dockspace_id;
        ImGuiID dock_left   = ImGui::DockBuilderSplitNode(dock_center, ImGuiDir_Left,  0.22f, nullptr, &dock_center);
        ImGuiID dock_right  = ImGui::DockBuilderSplitNode(dock_center, ImGuiDir_Right, 0.28f, nullptr, &dock_center);
        ImGuiID dock_left_bottom = ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Down, 0.3f, nullptr, &dock_left);
        ImGui::DockBuilderDockWindow("Tools",      dock_left);
        ImGui::DockBuilderDockWindow("Console / Terminal", dock_left_bottom);
        ImGui::DockBuilderDockWindow("Properties", dock_right);
        ImGui::DockBuilderDockWindow("Viewport",   dock_center);
        ImGui::DockBuilderFinish(dockspace_id);
    }

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu(" " ICON_FA_FILE "  File")) {
            if (ImGui::MenuItem(" Import STL...")) ImportSTL(scene, camera);
            if (ImGui::MenuItem(" Export STL...")) ExportSTL(scene);
            ImGui::Separator();
            if (ImGui::MenuItem(" Exit")) { exit(0); }
            ImGui::EndMenu();
        }
        DrawSettingsMenuItems();

        float rightEdge = ImGui::GetWindowWidth();
        ImGui::SetCursorPosX(rightEdge - 75.0f);

        ImGui::BeginDisabled(!undoManager.CanUndo());
        if (ImGui::MenuItem(ICON_FA_UNDO "##global")) undoManager.Undo(scene, alignState);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Undo (Ctrl+Z)");
        ImGui::EndDisabled();

        ImGui::BeginDisabled(!undoManager.CanRedo());
        if (ImGui::MenuItem(ICON_FA_REDO "##global")) undoManager.Redo(scene, alignState);
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Redo (Ctrl+Y)");
        ImGui::EndDisabled();

        ImGui::EndMainMenuBar();
    }

    DrawToolsPanel(scene);
    DrawPropertiesPanel(scene);
    if (AppSettings::Instance().showConsole) {
        if (ImGui::Begin("Console / Terminal", nullptr)) AppConsole::Instance().Draw();
        ImGui::End();
    }
    
    ProcessBackgroundTasks(scene, camera);
    if (!meshLoadTasks.empty()) {
        ImGui::SetNextWindowPos(ImVec2(viewportPos.x + 10, viewportPos.y + viewportSize.y - 40));
        ImGui::Begin("LoadingStatus", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoBackground);
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "[Loading] Processing %zu meshes...", meshLoadTasks.size());
        ImGui::End();
    }
    
    static bool interactionInitialized = false;
    if (!interactionInitialized) {
        interactionInitialized = true;
        interactionManager.Initialize(scene);
    }

    DrawViewport(viewportTextureID, camera, scene);
    DrawStatusBar(scene);
}

void UIManager::DrawSettingsMenuItems() {
    auto& s = AppSettings::Instance();
    if (ImGui::BeginMenu(" " ICON_FA_COGS "  Settings")) {
        // --- Visuals ---
        if (ImGui::BeginMenu(ICON_FA_EYE "  Visuals")) {
            ImGui::Checkbox(" " ICON_FA_ANGLE_DOUBLE_RIGHT "  Show Console", &s.showConsole);
            ImGui::Checkbox(" " ICON_FA_ANGLE_DOUBLE_RIGHT "  Instant View Snap", &s.instantViewJump);
            ImGui::Separator();
            ImGui::TextDisabled("Selection Highlighting");
            ImGui::ColorEdit4("Entity A (Moving)", &s.colorEntityA.r);
            ImGui::ColorEdit4("Entity B (Target)", &s.colorEntityB.r);
            ImGui::ColorEdit4("Coordinate Normal", &s.colorNormal.r);
            ImGui::Separator();
            ImGui::TextDisabled("Viewport Background");
            ImGui::Checkbox("Use Gradient Background", &s.useGradient);
            if (s.useGradient) {
                ImGui::ColorEdit3("Top Color", &s.viewportBackgroundTop.r);
                ImGui::ColorEdit3("Bottom Color", &s.viewportBackgroundBottom.r);
            } else {
                ImGui::ColorEdit3("Flat Background", &s.viewportBackgroundBottom.r);
            }
            ImGui::Separator();
            ImGui::TextDisabled("Projection & Lens");
            const char* projectionModes[] = { "Perspective (Organic)", "Orthographic (Nominal)" };
            int projIdx = s.isOrthographic ? 1 : 0;
            if (ImGui::Combo("Projection", &projIdx, projectionModes, 2)) {
                s.isOrthographic = (projIdx == 1);
            }
            ImGui::SliderFloat("Field of View", &s.fovY, 10.0f, 90.0f, "%.1f°");
            
            ImGui::EndMenu();
        }

        // --- Rendering ---
        if (ImGui::BeginMenu(ICON_FA_CUBE "  Rendering")) {
            const char* presets[] = { "Performance", "Balanced", "Quality", "Ultra" };
            int q = (int)s.qualityPreset;
            if (ImGui::Combo("Quality Preset", &q, presets, 4)) s.qualityPreset = (AppSettings::RenderQuality)q;
            
            ImGui::Separator();
            const char* msaa[] = { "None", "2x MSAA", "4x MSAA", "8x MSAA" };
            int mIdx = 0;
            if (s.msaaSamples == 2) mIdx = 1; else if (s.msaaSamples == 4) mIdx = 2; else if (s.msaaSamples == 8) mIdx = 3;
            if (ImGui::Combo("Anti-Aliasing", &mIdx, msaa, 4)) {
                if (mIdx == 0) s.msaaSamples = 1; else if (mIdx == 1) s.msaaSamples = 2; else if (mIdx == 2) s.msaaSamples = 4; else if (mIdx == 3) s.msaaSamples = 8;
            }
            ImGui::Checkbox("Smooth Shading", &s.useSmoothShading);
            ImGui::SliderFloat("Exposure", &s.exposure, 0.1f, 5.0f, "%.1f");
            ImGui::SliderFloat("Gamma",    &s.gamma,    1.0f, 3.0f, "%.1f");
            ImGui::EndMenu();
        }

        // --- Metrology ---
        if (ImGui::BeginMenu(ICON_FA_RULER "  Metrology")) {
            const char* units[] = { "Millimeters (mm)", "Centimeters (cm)", "Inches (in)" };
            int u = (int)s.currentUnit;
            if (ImGui::Combo("Unit System", &u, units, 3)) s.currentUnit = (AppSettings::UnitSystem)u;
            
            ImGui::Separator();
            ImGui::ColorEdit4("Result Color", &s.measurementColor.r);
            ImGui::SliderFloat("Thickness", &s.measurementThickness, 1.0f, 5.0f, "%.1f px");
            ImGui::Checkbox("Continuous Mode", &s.continuousMeasurement);
            ImGui::EndMenu();
        }

        // --- Advanced ---
        if (ImGui::BeginMenu(ICON_FA_COGS "  Advanced")) {
            ImGui::Checkbox("Preserve CAD Coordinates", &s.preserveCoordinatesOnImport);
            ImGui::Separator();
            if (ImGui::Button("Reset All to Defaults", ImVec2(-1, 0))) {
                s.ResetToDefaults();
            }
            ImGui::EndMenu();
        }

        ImGui::EndMenu();
    }
}

void UIManager::ImportSTL(SceneManager& scene, Camera& camera) {
#ifdef _WIN32
    char filename[MAX_PATH] = "";
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = "3D Models\0*.stl;*.obj;*.fbx;*.gltf;*.glb;*.ply\0";
    ofn.lpstrFile   = filename;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_EXPLORER | OFN_FILEMUSTEXIST;
    if (GetOpenFileNameA(&ofn)) ImportFromPath(scene, camera, filename);
#endif
}

void UIManager::ImportFromPath(SceneManager& scene, Camera& camera, const std::string& path) {
    bool center = !AppSettings::Instance().preserveCoordinatesOnImport;
    meshLoadTasks.push_back(std::async(std::launch::async, [path, center]() { return Mesh::LoadFromFile(path, center); }));
}

void UIManager::ProcessBackgroundTasks(SceneManager& scene, Camera& camera) {
    for (auto it = meshLoadTasks.begin(); it != meshLoadTasks.end(); ) {
        if (it->wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            std::shared_ptr<Mesh> mesh = it->get();
            it = meshLoadTasks.erase(it);
            if (!mesh) continue;
            mesh->UploadToGPU();
            for (auto& obj : scene.GetObjects()) { obj->isActive = false; obj->isSelected = false; }
            auto newObj = std::make_shared<SceneObject>(mesh->name, mesh);
            newObj->transform.position = mesh->importOffset;
            newObj->transform.Rebuild();
            newObj->isSelected = true; newObj->isActive = true;
            newObj->scene = &scene; // Back-pointer for clipping calculations
            scene.AddObject(newObj); scene.SetSelectedObject(newObj);
            analysisTasks.push_back(std::async(std::launch::async, [mesh]() { mesh->AnalyzePrimitives(); }));
            AABB combined;
            combined.min = Vec3( 1e9,  1e9,  1e9);
            combined.max = Vec3(-1e9, -1e9, -1e9);
            for (const auto& obj : scene.GetObjects()) {
                AABB wb = obj->GetWorldAABB();
                combined.min = glm::min(combined.min, wb.min);
                combined.max = glm::max(combined.max, wb.max);
            }
            double radius = glm::length(combined.max - combined.min) * 0.5;
            camera.FocusOn(combined.Center(), (radius < 1.0) ? 10.0 : radius);
        } else ++it;
    }
    for (auto it = analysisTasks.begin(); it != analysisTasks.end(); ) {
        if (it->wait_for(std::chrono::seconds(0)) == std::future_status::ready) it = analysisTasks.erase(it);
        else ++it;
    }
}

void UIManager::RebuildMeasurementHighlights(SceneManager& scene) {
    for (auto& obj : scene.GetObjects()) { if (obj->mesh) obj->mesh->measuredIndices.clear(); }
}

void UIManager::DrawStatusBar(SceneManager& scene) {
    auto objects = scene.GetObjects();
    auto selectedObject = scene.GetSelectedObject();
    SceneObject* primarySelected = selectedObject.get();
    ImGuiViewport* vp = ImGui::GetMainViewport();
    if (ImGui::BeginViewportSideBar("##StatusBar", vp, ImGuiDir_Down, 24.0f,
                                    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        int activeCount = 0;
        for (const auto& obj : objects) if (obj->isActive) ++activeCount;

        if (primarySelected && primarySelected->mesh) {
            Vec3 d = primarySelected->GetDimensions();
            bool analysisReady = primarySelected->mesh->m_isAnalysisReady;

            if (!analysisReady) {
                // Spinning indicator using system time
                float t = (float)ImGui::GetTime();
                char spinner = "|/-\\"[(int)(t * 10.0f) % 4];
                ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), " %c  " ICON_FA_COGS "  Analizando geometría inteligente...", spinner);
                ImGui::SameLine();
            }

            if (activeCount > 1) {
                ImGui::Text(" " ICON_FA_CUBE "  Active: %s (+%d more) | Dim: %.1f x %.1f x %.1f %s | Tris: %d",
                    primarySelected->name.c_str(), activeCount - 1,
                    (float)d.x, (float)d.y, (float)d.z,
                    AppSettings::Instance().GetUnitString(),
                    (int)primarySelected->mesh->GetTriangleCount());
            } else {
                ImGui::Text(" " ICON_FA_CUBE "  Active: %s | Dim: %.1f x %.1f x %.1f %s | Tris: %d",
                    primarySelected->name.c_str(),
                    (float)d.x, (float)d.y, (float)d.z,
                    AppSettings::Instance().GetUnitString(),
                    (int)primarySelected->mesh->GetTriangleCount());
            }
        } else {
            ImGui::Text("Ready. | %zu object(s) in scene.", objects.size());
        }

        // --- Right-aligned Unit System indicator ---
        float windowWidth = ImGui::GetWindowWidth();
        const char* unitStr = AppSettings::Instance().GetUnitString();
        
        // High visibility, clean right alignment
        ImGui::SameLine(windowWidth - 75.0f);
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        
        // Use an intense but professional color for the actual unit value
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), ICON_FA_RULER "  %s", unitStr); 
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Sistema de unidades actual (Clic en Settings para cambiar)");

        ImGui::End();
    }
}

void UIManager::ExportSTL(SceneManager& scene) {
#ifdef _WIN32
    SceneObject* target = nullptr;
    for (const auto& obj : scene.GetObjects()) { if (obj->isActive) { target = obj.get(); break; } }
    if (!target || !target->mesh) return;
    char filename[MAX_PATH] = "";
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = "Binary STL\0*.stl\0";
    ofn.lpstrFile   = filename;
    ofn.nMaxFile    = MAX_PATH;
    if (GetSaveFileNameA(&ofn)) target->mesh->ExportSTL(filename, Mat4f(target->transform.worldMatrix));
#endif
}
