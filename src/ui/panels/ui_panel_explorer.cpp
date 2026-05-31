#include "../ui_manager.hpp"
#include "../../core/scene_manager.hpp"
#include "../../core/scene_object.hpp"
#include "../../core/app_settings.hpp"
#include <imgui.h>
#include "../IconsFontAwesome5.h"

void UIManager::DrawOutliner(SceneManager& scene) {
    auto objects = scene.GetObjects();
    auto selectedObject = scene.GetSelectedObject();
    
    // Header with better spacing
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.9f, 0.9f, 1.0f));
    ImGui::SeparatorText("SCENE EXPLORER");
    ImGui::PopStyleColor();
    
    ImGui::BeginChild("##outliner_scroll", ImVec2(0, 160), true, ImGuiWindowFlags_None);

    for (const auto& obj : objects) {
        ImGui::PushID(obj->id.c_str());

        // --- 1. Active Checkbox (Active/Inactive) ---
        if (ImGui::Checkbox("##act", &obj->isActive)) {
            // Decoupling Update: Activation no longer forces selection.
            // But we must reset gizmo if NOTHING is active in the entire scene.
            bool anythingActive = false;
            for (auto& other : objects) if (other->isActive) anythingActive = true;
            if (!anythingActive) ResetGizmoState();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Active/Inactive");
        ImGui::SameLine();

        // --- 2. Visibility Toggle (Eye Icon) ---
        if (ImGui::Selectable(obj->isVisible ? " " ICON_FA_EYE " " : " " ICON_FA_EYE_SLASH " ", false, 0, ImVec2(24, 0))) {
            obj->isVisible = !obj->isVisible;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(obj->isVisible ? "Visible (Click to Hide)" : "Hidden (Click to Show)");
        ImGui::SameLine();

        // --- 3. Color Indicator / Picker ---
        glm::vec3 c = obj->material.diffuseColor;
        if (ImGui::ColorButton("##color_ind", ImVec4(c.r, c.g, c.b, 1.0f), ImGuiColorEditFlags_NoTooltip, ImVec2(18, 18))) {
            ImGui::OpenPopup("##color_picker_popup");
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to change color");
        
        if (ImGui::BeginPopup("##color_picker_popup")) {
            ImGui::Text("Pick Color");
            if (ImGui::ColorPicker3("##picker", &obj->material.diffuseColor[0], ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_NoInputs)) {
                // Color updated
            }
            ImGui::Separator();
            // Show Palette Quick Picks inside the color picker popup
            auto& palette = AppSettings::Instance().cadPalette;
            for (size_t p = 0; p < palette.size(); p++) {
                ImGui::PushID((int)p);
                if (ImGui::ColorButton("##p", ImVec4(palette[p].r, palette[p].g, palette[p].b, 1.0f))) {
                    obj->material.diffuseColor = palette[p];
                }
                if ((p + 1) % 4 != 0) ImGui::SameLine();
                ImGui::PopID();
            }
            ImGui::EndPopup();
        }
        ImGui::SameLine();

        // --- 4. Object Name ---
        bool isPrimary = (selectedObject && selectedObject.get() == obj.get());
        if (ImGui::Selectable(obj->name.c_str(), isPrimary, ImGuiSelectableFlags_None)) {
            // Decoupling Update: Selection is now exclusive but DOES NOT affect Activation flags.
            for (auto& other : objects) {
                other->isSelected = false;
            }
            obj->isSelected = true;
            scene.SetSelectedObject(obj);
        }

        ImGui::PopID();
    }
    ImGui::EndChild();
    
    // Action Bar (replaces the old [+][!][X] buttons)
    ImGui::Spacing();
    float btnWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 2) / 3.0f;
    
    if (ImGui::Button("All", ImVec2(btnWidth, 0))) {
        for (auto& obj : objects) { obj->isActive = true; }
        ResetGizmoState();
    }
    ImGui::SameLine();
    if (ImGui::Button("Invert", ImVec2(btnWidth, 0))) {
        for (auto& obj : objects) { obj->isActive = !obj->isActive; }
        ResetGizmoState();
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
    if (ImGui::Button(" " ICON_FA_TRASH "  Delete", ImVec2(btnWidth, 0))) {
        for (int i = (int)objects.size() - 1; i >= 0; --i) {
            auto& obj = objects[i];
            if (obj->isActive) { // ONLY delete explicitly activated (checked) objects
                std::string idToDelete = obj->id;
                
                // Prevent Gizmo crash from dangling pointer
                if (idToDelete == gizmoAnchorId) {
                    isGizmoOnObject = false;
                    gizmoAnchorId = ""; 
                }

                CleanStateForDeletedObject(idToDelete);
                scene.RemoveObject(idToDelete);
            }
        }
        ResetGizmoState();
    }
    ImGui::PopStyleColor();
}
