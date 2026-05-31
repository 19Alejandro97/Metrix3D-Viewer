#include "../ui_manager.hpp"
#include "../../core/scene_manager.hpp"
#include "../../core/scene_object.hpp"
#include "../../core/app_settings.hpp"
#include "../app_console.hpp"
#include "../../core/kinematic_solver.hpp"
#include <imgui.h>
#include <cstdio>
#include "../IconsFontAwesome5.h"

void UIManager::DrawPropertiesPanel(SceneManager& scene) {
    auto selectedObject = scene.GetSelectedObject();
    ImGui::Begin("Properties");
    DrawOutliner(scene);
    if (selectedObject && selectedObject->isSelected) {
        DrawTransformPanel(selectedObject.get(), scene);
        DrawMaterialPanel(selectedObject.get());
    } else {
        ImGui::Spacing();
        ImGui::Separator();
    }
    ImGui::TextWrapped("Click an object name in the list above to view properties.");
    ImGui::End();
}

void UIManager::DrawTransformPanel(SceneObject* obj, SceneManager& scene) {
    auto objects = scene.GetObjects(); 
    
    if (ImGui::CollapsingHeader(ICON_FA_ARROWS_ALT "  Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();

        int activeCount = 0;
        for (const auto& o : objects) if (o->isActive) ++activeCount;
        const bool multiActive = (activeCount > 1);

        if (multiActive) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.8f, 0.0f, 1.0f)); 
            ImGui::Text("Editing %d active objects", activeCount);
            ImGui::PopStyleColor();
        }

        Transform& t = obj->transform;
        Vec3  prevPos   = t.position;
        Vec3  prevRot   = t.eulerAngles;
        // Vec3  prevScale = t.scale; // Unused for now

        bool changed = false;
        ImGui::PushItemWidth(-1);
        ImGui::TextDisabled("Position (X, Y, Z)");
        changed |= ImGui::DragScalarN("##Pos", ImGuiDataType_Double, &t.position[0],    3, 0.05,  nullptr, nullptr, "%.3f");
        
        ImGui::Spacing();
        ImGui::TextDisabled("Rotation (Deg)");
        changed |= ImGui::DragScalarN("##Rot", ImGuiDataType_Double, &t.eulerAngles[0], 3, 0.1,   nullptr, nullptr, "%.2f");
        ImGui::PopItemWidth();

        if (changed) {
            t.Rebuild();
            if (multiActive) {
                Vec3 dPos   = t.position    - prevPos;
                Vec3 dRot   = t.eulerAngles - prevRot;
                for (auto& o : objects) {
                    if (!o->isActive || o.get() == obj) continue;
                    o->transform.position    += dPos;
                    o->transform.eulerAngles += dRot;
                    o->transform.Rebuild();
                }
            }
        }
        ImGui::Spacing();
    }
}

void UIManager::DrawMaterialPanel(SceneObject* obj) {
    if (!obj) return;
    if (ImGui::CollapsingHeader(ICON_FA_PALETTE "  Material", ImGuiTreeNodeFlags_DefaultOpen)) {
        Material& mat = obj->material;
        
        ImGui::TextDisabled("Appearance");
        ImGui::ColorEdit3("Color", &mat.diffuseColor[0], ImGuiColorEditFlags_NoInputs);
        ImGui::SameLine(); ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##Opacity", &mat.opacity, 0.0f, 1.0f, "Opacity %.2f");
        
        float btnWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 3) / 4.0f;
        if (ImGui::Button("25%", ImVec2(btnWidth, 0))) mat.opacity = 0.25f;
        ImGui::SameLine();
        if (ImGui::Button("50%", ImVec2(btnWidth, 0))) mat.opacity = 0.50f;
        ImGui::SameLine();
        if (ImGui::Button("75%", ImVec2(btnWidth, 0))) mat.opacity = 0.75f;
        ImGui::SameLine();
        if (ImGui::Button("100%", ImVec2(btnWidth, 0))) mat.opacity = 1.00f;
        
        ImGui::TextDisabled("Reflectance");
        ImGui::SliderFloat("Specular", &mat.specular, 0.0f, 1.0f, "%.2f");
        ImGui::SliderFloat("Gloss",    &mat.shininess, 0.0f, 100.0f, "%.1f");

        ImGui::Spacing();
        ImGui::TextDisabled("Render Style");
        const char* modes[] = { "Solid", "Wireframe", "Points" };
        int currentMode = (int)mat.renderMode;
        ImGui::SetNextItemWidth(-1);
        if (ImGui::Combo("##Mode", &currentMode, modes, 3)) mat.renderMode = (RenderMode)currentMode;
        
        ImGui::Checkbox("Draw Hard Edges", &mat.showEdges);
        if (mat.showEdges) {
            if (obj->mesh && obj->mesh->m_gpu.edgeVao == 0) obj->mesh->GenerateEdges(mat.edgeThreshold);
            ImGui::SetNextItemWidth(-1);
            bool changed = ImGui::SliderFloat("##EdgeT", &mat.edgeThreshold, 5.0f, 90.0f, "Threshold %.0f");
            if (ImGui::IsItemDeactivatedAfterEdit() || (changed && obj->mesh->vertices.size() < 10000)) {
                if (obj->mesh) obj->mesh->GenerateEdges(mat.edgeThreshold);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Angle threshold for hard edges (CAD style)");
        }
        ImGui::Spacing();
        
        ImGui::Separator();
        ImGui::TextDisabled(ICON_FA_COGS "  Industrial Baking");
        if (ImGui::Button(ICON_FA_CUBE "  Bake Transformation", ImVec2(-1, 0))) {
            if (obj->mesh) {
                obj->mesh->BakeTransform(Mat4f(obj->transform.worldMatrix));
                obj->transform.position = {0, 0, 0};
                obj->transform.eulerAngles = {0, 0, 0};
                obj->transform.scale = {1, 1, 1};
                obj->transform.Rebuild();
                AppConsole::Log("[Telemetry] Transform baked into vertices. Object reset to identity.");
            }
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Apply current position/rotation permanently to vertex data.");
        ImGui::Spacing();
    }
}
