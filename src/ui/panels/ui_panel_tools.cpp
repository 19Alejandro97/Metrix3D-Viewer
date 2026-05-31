#include "../ui_manager.hpp"
#include "../../core/scene_manager.hpp"
#include "../../core/scene_object.hpp"
#include "../../core/mesh.hpp"
#include "../../core/feature_extraction.hpp"
#include "../../core/kinematic_solver.hpp"
#include "../app_console.hpp"
#include "../../core/app_settings.hpp"
#include <imgui_internal.h>
#include <cstdio>
#include <sstream>
#include <algorithm>
#include "../IconsFontAwesome5.h"

void UIManager::DrawToolsPanel(SceneManager& scene) {
    auto selectedObject = scene.GetSelectedObject();
    
    ImGui::Begin("Tools", nullptr, ImGuiWindowFlags_NoCollapse);
    
    bool analysisReady = true;
    if (selectedObject && selectedObject->mesh) {
        analysisReady = selectedObject->mesh->m_isAnalysisReady;
    }

    if (ImGui::CollapsingHeader(ICON_FA_RULER "  Measurement")) {
        ImGui::BeginDisabled(!analysisReady);
        DrawMeasurementPanel(scene);
        ImGui::EndDisabled();
        if (!analysisReady) ImGui::TextDisabled("  " ICON_FA_COGS " Analyzing features...");
        ImGui::Spacing();
    }
    
    if (ImGui::CollapsingHeader(ICON_FA_SCISSORS "  Section Analysis")) {
        ImGui::BeginDisabled(!analysisReady);
        DrawAnalysisPanel(scene);
        ImGui::EndDisabled();
        if (!analysisReady) ImGui::TextDisabled("  " ICON_FA_COGS " Analyzing features...");
        ImGui::Spacing();
    }
    
    if (ImGui::CollapsingHeader(ICON_FA_OBJECT_GROUP "  Alignment Tool")) {
        ImGui::BeginDisabled(!analysisReady);
        DrawAlignmentPanel(scene);
        ImGui::EndDisabled();
        if (!analysisReady) ImGui::TextDisabled("  " ICON_FA_COGS " Analyzing features...");
        ImGui::Spacing();
    }
    
    if (ImGui::CollapsingHeader(ICON_FA_CUBE "  Geometry Processing")) {
        ImGui::BeginDisabled(!analysisReady);
        DrawGeometryPanel(selectedObject.get());
        ImGui::EndDisabled();
        if (!analysisReady) ImGui::TextDisabled("  " ICON_FA_COGS " Analyzing features...");
        ImGui::Spacing();
    }

    if (ImGui::CollapsingHeader(ICON_FA_ARROWS_ALT "  Scaling & Units")) {
        ImGui::BeginDisabled(!analysisReady);
        DrawScalingPanel(scene);
        ImGui::EndDisabled();
        if (!analysisReady) ImGui::TextDisabled("  " ICON_FA_COGS " Analyzing features...");
        ImGui::Spacing();
    }

    if (ImGui::CollapsingHeader(ICON_FA_OBJECT_GROUP "  Scene Actions")) {
        ImGui::Spacing();
        ImGui::TextDisabled("Active pieces will be merged into one.");
        auto objects = scene.GetObjects();
        int activeCount = 0;
        for (const auto& obj : objects) if (obj->isActive) ++activeCount;

        ImGui::BeginDisabled(activeCount < 2);
        if (ImGui::Button(ICON_FA_OBJECT_GROUP "  Merge Active Meshes", ImVec2(-1, 0))) {
            // Collect active objects
            std::vector<SceneObject*> actives;
            for (const auto& obj : objects) if (obj->isActive) actives.push_back(obj.get());

            if (actives.size() >= 2) {
                // Create merged mesh using the first active as base
                auto merged = std::make_shared<Mesh>();
                merged->name = "Merged";
                merged->importOffset = Vec3(0,0,0);

                for (auto* obj : actives) {
                    merged->MergeFrom(*obj->mesh, Mat4f(obj->transform.worldMatrix));
                }

                merged->RecalculateBounds();
                merged->RecalculateNormals();
                merged->BuildAdjacency();
                merged->UploadToGPU();
                merged->GenerateEdges(35.0f);

                // Remove original actives from scene
                std::vector<std::string> toRemove;
                for (auto* obj : actives) toRemove.push_back(obj->id);
                for (const auto& id : toRemove) scene.RemoveObject(id);

                // Add merged object at origin
                auto mergedObj = std::make_shared<SceneObject>("Merged", merged);
                mergedObj->transform.position    = Vec3(0,0,0);
                mergedObj->transform.eulerAngles = Vec3(0,0,0);
                mergedObj->transform.scale       = Vec3(1,1,1);
                mergedObj->transform.Rebuild();
                mergedObj->isActive   = true;
                mergedObj->isSelected = true;
                scene.AddObject(mergedObj);
                scene.SetSelectedObject(mergedObj);
                
                // Note: gizmoAnchorId and isGizmoOnObject are private to UIManager.
                // We assume there's a ResetGizmoState() call or direct access if needed.
                // Since this is part of UIManager class, we have access.
                gizmoAnchorId   = mergedObj->id;
                isGizmoOnObject = true;
                AppConsole::Log("[Scene] Merged " + std::to_string(actives.size()) + " meshes into 'Merged'.");
            }
        }
        ImGui::EndDisabled();
        if (activeCount < 2)
            ImGui::TextDisabled("Activate 2+ pieces to enable merge.");
        ImGui::Spacing();
    }

    ImGui::End();
}

void UIManager::DrawGeometryPanel(SceneObject* selectedObject) {
    ImGui::Spacing();
    if (!selectedObject || !selectedObject->mesh) {
        ImGui::TextDisabled("Select an object to process.");
        return;
    }
    
    ImGui::Text("Mesh Processing");
    ImGui::Separator();
    ImGui::Spacing();

    // Show original model-space offset (critical for future STL export)
    const Vec3& off = selectedObject->mesh->importOffset;
    ImGui::TextDisabled("Model Origin Offset (for export)");
    ImGui::Text("X: %.4f  Y: %.4f  Z: %.4f", (float)off.x, (float)off.y, (float)off.z);
    ImGui::Separator();
    ImGui::Spacing();
    static int fillResult = -1;
    if (ImGui::Button("Auto-Fill Holes (Centroid Umbrella)", ImVec2(-1, 0))) {
        fillResult = selectedObject->mesh->FillHoles();
        std::stringstream ss;
        ss << "[Geometry] Filled " << fillResult << " holes on " << selectedObject->mesh->name;
        AppConsole::Log(ss.str());
    }
    if (fillResult >= 0) {
        if (fillResult == 0) ImGui::TextColored(ImVec4(0.6f,0.6f,0.6f,1.0f), "No boundary loops/holes found.");
        else                 ImGui::TextColored(ImVec4(0.2f,0.9f,0.2f,1.0f), "Successfully filled %d hole(s)!", fillResult);
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    static int smoothIters = 1;
    static float smoothLambda = 0.5f;
    static bool justSmoothed = false;
    ImGui::SliderInt("Iterations", &smoothIters, 1, 10);
    ImGui::SliderFloat("Strength", &smoothLambda, 0.1f, 1.0f, "%.2f");
    
    if (ImGui::Button("Smooth Mesh (Laplacian)", ImVec2(-1, 0))) {
        selectedObject->mesh->SmoothMesh(smoothIters, smoothLambda);
        justSmoothed = true;
        std::stringstream ss;
        ss << "[Geometry] Smoothed " << selectedObject->mesh->name << " (Iters: " << smoothIters << ", Lambda: " << smoothLambda << ")";
        AppConsole::Log(ss.str());
    }
    if (justSmoothed) {
        ImGui::TextColored(ImVec4(0.2f,0.9f,0.2f,1.0f), "Mesh smoothed.");
    }
    ImGui::Spacing();
}

void UIManager::DrawScalingPanel(SceneManager& scene) {
    auto objects = scene.GetObjects();
    auto selected = scene.GetSelectedObject();
    
    ImGui::Spacing();
    
    int activeCount = 0;
    for (const auto& obj : objects) if (obj->isActive) ++activeCount;
    
    if (activeCount == 0 && !selected) {
        ImGui::TextDisabled("Select or activate a piece to scale.");
        return;
    }
    
    SceneObject* target = selected ? selected.get() : nullptr;
    if (!target) {
        for (const auto& obj : objects) if (obj->isActive) { target = obj.get(); break; }
    }
    
    if (!target) return;
    
    ImGui::Text("Scaling: %s", (activeCount > 1) ? "All Active Pieces" : target->name.c_str());
    ImGui::Separator();
    
    static float scaleFactor = 1.0f;
    static bool uniform = true;
    static Vec3 nonUniformScale = {1.0, 1.0, 1.0};
    
    ImGui::Checkbox("Uniform Scaling", &uniform);
    
    bool applied = false;
    if (uniform) {
        ImGui::DragFloat("Factor", &scaleFactor, 0.01f, 0.001f, 100.0f, "%.3f");
        if (ImGui::Button("Apply Scale##uni", ImVec2(-1, 0))) applied = true;
    } else {
        ImGui::DragScalarN("Scale X/Y/Z", ImGuiDataType_Double, &nonUniformScale[0], 3, 0.01, nullptr, nullptr, "%.3f");
        if (ImGui::Button("Apply Scale##non", ImVec2(-1, 0))) applied = true;
    }
    
    if (applied) {
        Vec3 factor = uniform ? Vec3(scaleFactor) : nonUniformScale;
        
        for (auto& obj : objects) {
            if (obj->isActive || obj.get() == target) {
                obj->transform.scale *= factor;
                obj->transform.Rebuild();
            }
        }
        
        // Reset inputs to avoid "double scaling" accidentally
        scaleFactor = 1.0f;
        nonUniformScale = {1.0, 1.0, 1.0};
        AppConsole::Log("[Tools] Applied scaling factor to active pieces.");
    }
    
    ImGui::Spacing();
    ImGui::TextDisabled("Quick Unit Conversion");
    if (ImGui::Button("mm -> inches (1/25.4)", ImVec2(-1, 0))) {
        for (auto& obj : objects) {
            if (obj->isActive || obj.get() == target) {
                obj->transform.scale *= (1.0 / 25.4);
                obj->transform.Rebuild();
            }
        }
    }
    if (ImGui::Button("inches -> mm (x25.4)", ImVec2(-1, 0))) {
        for (auto& obj : objects) {
            if (obj->isActive || obj.get() == target) {
                obj->transform.scale *= 25.4;
                obj->transform.Rebuild();
            }
        }
    }
}
