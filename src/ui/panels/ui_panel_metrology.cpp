#include "../ui_manager.hpp"
#include "../../core/scene_manager.hpp"
#include "../../core/scene_object.hpp"
#include "../../core/app_settings.hpp"
#include "../app_console.hpp"
#include "../../core/kinematic_solver.hpp"
#include <imgui.h>
#include <cstdio>
#include <glm/gtc/type_ptr.hpp>
#include "../IconsFontAwesome5.h"

void UIManager::DrawAnalysisPanel(SceneManager& scene) {
    auto& clipPlanes = scene.GetClipPlanes();
    ImGui::Spacing();
    static ImGuiTableFlags flags = ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg |
                                   ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY;
    if (ImGui::BeginTable("##section_table", 5, flags, ImVec2(0, 160))) {
        ImGui::TableSetupColumn("Active",  0, 40.0f);
        ImGui::TableSetupColumn("Type",    0, 60.0f);
        ImGui::TableSetupColumn("Flip",    0, 40.0f);
        ImGui::TableSetupColumn("Color",   0, 40.0f);
        ImGui::TableSetupColumn("Position", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (int i = 0; i < (int)clipPlanes.size(); ++i) {
            auto& p = clipPlanes[i];
            ImGui::TableNextRow(); ImGui::PushID(i);
            ImGui::TableSetColumnIndex(0); ImGui::Checkbox("##active", &p.active);
            ImGui::TableSetColumnIndex(1);
            const char* tps[] = { "X", "Y", "Z", "Cust" };
            ImGui::SetNextItemWidth(-1); ImGui::Combo("##t", &p.type, tps, 4);
            ImGui::TableSetColumnIndex(2); ImGui::Checkbox("##f", &p.flip);
            ImGui::TableSetColumnIndex(3); ImGui::ColorEdit3("##c", &p.color.x,
                ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
            ImGui::TableSetColumnIndex(4); ImGui::SetNextItemWidth(-1);
            if (ImGui::DragFloat("##p", &p.position, p.step, -2000.0f, 2000.0f, "%.3f")) {
                // Signal renderer to show ghost plane during drag
            }
            if (ImGui::IsItemActive()) {
                AppSettings::Instance().activeGizmoPlane = i;
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
        
        // Reset ghost plane signal if no slider is being manipulated
        bool anyActive = false;
        for (int i = 0; i < 6; ++i) {
            ImGui::PushID(i);
            if (ImGui::IsItemActive()) anyActive = true; // Wait! IsItemActive refers to last item.
            ImGui::PopID();
        }
        // Actually, cleaner to reset if no interaction is detected globally.
        // But ImGui handles it well if we just set it per frame.
        if (ImGui::IsAnyItemActive() == false) {
             AppSettings::Instance().activeGizmoPlane = -1;
        }
    }
    if (ImGui::Button("Reset All Sections", ImVec2(-1, 0)))
        for (auto& p : clipPlanes) p.active = false;
    ImGui::Spacing();
}

void UIManager::DrawAlignmentPanel(SceneManager& scene) {
    auto objects = scene.GetObjects();
    ImGui::Spacing();
    if (objects.empty()) {
        ImGui::TextDisabled("Import objects to align.");
        return;
    }

    ImGui::Spacing();
    
    // 1. Management Buttons
    float btnW = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 3) / 4.0f;
    if (ImGui::Button("Add Constraint...", ImVec2(btnW, 0))) {
        alignState.showAddDialog = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove Last", ImVec2(btnW, 0))) {
        if (!alignState.activeConstraints.empty()) {
            alignState.activeConstraints.pop_back();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove All", ImVec2(btnW, 0))) {
        alignState.activeConstraints.clear();
    }
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.4f, 0.4f, 0.4f, 1.0f));
    if (ImGui::Button("Exit Tool", ImVec2(btnW, 0))) {
        alignState.Reset();
    }
    ImGui::PopStyleColor();
    
    ImGui::Spacing();
    ImGui::Separator();
    
    if (!alignState.lastResultMsg.empty()) {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", alignState.lastResultMsg.c_str());
    }

    // 2. Add Constraint Dialog
    if (alignState.showAddDialog) {
        ImGui::Spacing(); ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
        if (ImGui::BeginChild("AddConstraintPanel", ImVec2(0, 0), true, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::SeparatorText("Add Constraint");
            
            const char* entityTypes[] = { "Point", "3 Points", "Line", "Circle", "Plane", "Cylinder", "Sphere" };
            const char* relationTypes[] = { "Coincident", "Concentric", "Parallel", "Perpendicular", "Distance", "Angle" };

            // Source
            ImGui::TextDisabled("First entity (moving part)");
            int tSrc = (int)alignState.draftSourceType;
            ImGui::SetNextItemWidth(120.0f);
            ImGui::Combo("##srcType", &tSrc, entityTypes, IM_ARRAYSIZE(entityTypes));
            alignState.draftSourceType = (AlignEntitySelect)tSrc;
            ImGui::SameLine();
            if (ImGui::Button(alignState.draftSourceEntity.isValid ? "Re-Pick##src" : "Pick##src")) {
                alignState.step = AlignStep::PickingSource;
            }

            // Target
            ImGui::TextDisabled("Second entity");
            int tTgt = (int)alignState.draftTargetType;
            ImGui::SetNextItemWidth(120.0f);
            ImGui::Combo("##tgtType", &tTgt, entityTypes, IM_ARRAYSIZE(entityTypes));
            alignState.draftTargetType = (AlignEntitySelect)tTgt;
            ImGui::SameLine();
            if (ImGui::Button(alignState.draftTargetEntity.isValid ? "Re-Pick##tgt" : "Pick##tgt")) {
                alignState.step = AlignStep::PickingTarget;
            }

            // Relation
            int rType = (int)alignState.draftRelation;
            ImGui::SetNextItemWidth(180.0f);
            ImGui::Combo("##rType", &rType, relationTypes, IM_ARRAYSIZE(relationTypes));
            alignState.draftRelation = (AlignRelationType)rType;
            ImGui::SameLine();
            ImGui::Checkbox("Inverse", &alignState.draftIsInverse);

            ImGui::Spacing();
            if (ImGui::Button("Accept", ImVec2(80, 0))) {
                if (alignState.draftSourceEntity.isValid && alignState.draftTargetEntity.isValid) {
                    SceneObject* objSrc = scene.GetObjectById(alignState.draftSourceObjId);
                    if (objSrc) {
                        // --- Capture Pre-State ---
                        Transform oldTransform = objSrc->transform;
                        std::vector<ConstraintLine> oldConstraints = alignState.activeConstraints;

                        ConstraintLine c;
                        c.sourceEntity = alignState.draftSourceEntity;
                        c.targetEntity = alignState.draftTargetEntity;
                        c.sourceObjectId = alignState.draftSourceObjId;
                        c.targetObjectId = alignState.draftTargetObjId;
                        c.relation = alignState.draftRelation;
                        c.isInverse = alignState.draftIsInverse;
                        c.isValid = true;
                        alignState.activeConstraints.push_back(c);

                        Mat4 initMat = objSrc->isAssembled ? objSrc->baseAlignmentMatrix : objSrc->transform.worldMatrix;
                        KinematicSolver::ApplySequentialConstraints(objSrc->transform, alignState.activeConstraints, initMat);
                        objSrc->mesh->RecalculateBounds();
                        scene.isDirty = true;
                        // Guard base matrix so sequential constraints start from pristine state
                        if (!objSrc->isAssembled) {
                            objSrc->baseAlignmentMatrix = objSrc->transform.worldMatrix;
                            objSrc->isAssembled = true;
                        }

                        // --- Capture Post-State and Push Command ---
                        undoManager.PushCommand(std::make_unique<AlignCommand>(
                            objSrc->id, oldTransform, objSrc->transform, 
                            oldConstraints, alignState.activeConstraints));
                    }
                    
                    alignState.draftSourceEntity = {};
                    alignState.draftTargetEntity = {};
                    alignState.step = AlignStep::Idle;
                    alignState.currentPoints.clear();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel##add", ImVec2(80, 0))) {
                alignState.draftSourceEntity = {};
                alignState.draftTargetEntity = {};
                alignState.step = AlignStep::Idle;
                alignState.currentPoints.clear();
            }
            ImGui::EndChild();
        }
        ImGui::PopStyleColor();
    }
}

void UIManager::DrawMeasurementPanel(SceneManager& scene) {
    ImGui::Spacing();
    
    // 1. Tab Bar for Metrology Categories
    if (ImGui::BeginTabBar("##meas_tabs", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem(ICON_FA_RULER "  Distance")) {
            measureState.tab = MeasureTab::Distance;
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(ICON_FA_CIRCLE "  Circle")) {
            measureState.tab = MeasureTab::Circle;
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem(ICON_FA_ANGLE_DOUBLE_RIGHT "  Angle")) {
            measureState.tab = MeasureTab::Angle;
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::Spacing();
    
    // 2. Mode-Specific Industrial Controls (Contextual UX)
    if (measureState.tab == MeasureTab::Distance) {
        ImGui::TextDisabled("Measurement Mode:");
        const char* distModes[] = { "Point-to-Point", "Point-to-Plane", "Plane-to-Plane", "Center-to-Center" };
        ImGui::SetNextItemWidth(-1);
        ImGui::Combo("##DistMode", &measureState.distanceMode, distModes, IM_ARRAYSIZE(distModes));
        
        ImGui::Spacing();
        ImGui::Checkbox(ICON_FA_EYE "  Enable Internal X-Ray Snapping", &measureState.xRayEnabled);
        ImGui::Spacing();
    } else if (measureState.tab == MeasureTab::Circle) {
        ImGui::TextDisabled("Detection Method:");
        int m = (int)measureState.circleMode;
        ImGui::RadioButton("Smart Auto-Detection", &m, 0); ImGui::SameLine();
        ImGui::RadioButton("3-Point Manual", &m, 1);
        measureState.circleMode = (CircleMode)m;

        ImGui::Spacing();
        ImGui::Checkbox("Show Diameter (Ø) instead of Radius (R)", &measureState.showDiameter);
        ImGui::Spacing();
    } else if (measureState.tab == MeasureTab::Angle) {
        ImGui::TextDisabled("Measurement Mode:");
        const char* angModes[] = { "Plane-to-Plane", "Axis-to-Axis", "3-Point Angle" };
        ImGui::SetNextItemWidth(-1);
        ImGui::Combo("##AngleMode", &measureState.angleMode, angModes, IM_ARRAYSIZE(angModes));
        ImGui::Spacing();
    }

    ImGui::Separator();
    ImGui::Spacing();

    // 3. Interactive Measurement History
    ImGui::TextDisabled("MEASUREMENT HISTORY");
    ImGui::BeginChild("##m_history", ImVec2(0, 180), true, ImGuiWindowFlags_HorizontalScrollbar);
    
    if (measureState.history.empty()) {
        ImGui::TextDisabled("  No anchored measurements.");
    } else {
        if (ImGui::BeginTable("##history_table", 3, ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Vis", ImGuiTableColumnFlags_WidthFixed, 30.0f);
            ImGui::TableSetupColumn("Measure", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Act", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            
            for (int i = 0; i < (int)measureState.history.size(); ++i) {
                auto& m = measureState.history[i];
                ImGui::TableNextRow();
                ImGui::PushID(i);
                
                // Column 0: Visibility
                ImGui::TableSetColumnIndex(0);
                const char* eyeIcon = m.visible ? ICON_FA_EYE : ICON_FA_EYE_SLASH;
                if (ImGui::Button(eyeIcon)) m.visible = !m.visible;
                
                // Column 1: Result Label
                ImGui::TableSetColumnIndex(1);
                char label[128];
                if (m.type == MeasureTab::Distance) {
                    sprintf(label, "Distance M%d: %.4f mm", i + 1, m.value);
                } else if (m.type == MeasureTab::Circle) {
                    double val = measureState.showDiameter ? m.value * 2.0 : m.value;
                    const char* sym = measureState.showDiameter ? "Ø" : "R";
                    sprintf(label, "Circle M%d: %s %.4f mm", i + 1, sym, val);
                } else {
                    sprintf(label, "Angle M%d: %.2f°", i + 1, m.value);
                }
                ImGui::TextUnformatted(label);
                
                // --- Phase 12: Triaxial Breakdown (Local UCS) ---
                if (m.type == MeasureTab::Distance) {
                    ImGui::TextDisabled("  UCS: %s", m.referenceObjName.c_str());
                    ImGui::TextDisabled("  dX: %+.3f | dY: %+.3f | dZ: %+.3f", 
                                       (float)m.deltaLocal.x, (float)m.deltaLocal.y, (float)m.deltaLocal.z);
                }

                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    if (m.type == MeasureTab::Circle) {
                        ImGui::Text("Center: %.3f, %.3f, %.3f", (float)m.entity.point.x, (float)m.entity.point.y, (float)m.entity.point.z);
                        ImGui::Text("Normal: %.3f, %.3f, %.3f", (float)m.entity.normal.x, (float)m.entity.normal.y, (float)m.entity.normal.z);
                    }
                    if (m.isXRay) ImGui::TextColored(ImVec4(1,0.5f,0,1), "Internal Geometry (X-Ray)");
                    ImGui::EndTooltip();
                }
                // Column 2: Actions
                ImGui::TableSetColumnIndex(2);
                if (m.hasCustomPos) {
                    if (ImGui::Button("RST")) {
                        m.hasCustomPos = false;
                    }
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset Cota Position");
                    ImGui::SameLine();
                }
                if (ImGui::Button(ICON_FA_TRASH)) {
                    measureState.history.erase(measureState.history.begin() + i);
                    i--; // Adjust index
                    RebuildMeasurementHighlights(scene);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Delete Measurement");
                
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
    
    if (measureState.active) {
        ImVec4 pickingColor = ImVec4(1.0f, 0.8f, 0.0f, 1.0f);
        ImGui::TextColored(pickingColor, "  " ICON_FA_CIRCLE "  ACTIVE CAPTURE...");
    }
    ImGui::EndChild();

    // 4. Action Buttons & Handoff Bridge
    ImGui::Spacing();
    
    // Mode toggle: Continuous vs Single
    ImGui::Checkbox(ICON_FA_RULER "  Continuous Capture", &AppSettings::Instance().continuousMeasurement);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("If enabled, the capture mode remains active after each measurement.");

    ImGui::Spacing();
    float btnWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) / 2.0f;
    
    if (ImGui::Button(measureState.active ? "STOP CAPTURE" : "NEW MEASUREMENT", ImVec2(btnWidth, 0))) {
        measureState.active = !measureState.active;
        measureState.currentPoints.clear();
        if (!measureState.active) {
            for (auto& obj : scene.GetObjects()) if (obj->mesh) {
                obj->mesh->highlightIndices.clear();
                obj->mesh->highlightEdgeIndices.clear();
                obj->mesh->UpdateHighlightGPU(); obj->mesh->UpdateHighlightEdgeGPU();
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("CLEAR ALL", ImVec2(btnWidth, 0))) {
        measureState.history.clear();
        RebuildMeasurementHighlights(scene);
    }

    // Smart Assembly Bridge (Handoff)
    if (measureState.tab == MeasureTab::Circle && !measureState.history.empty()) {
        ImGui::Spacing();
        if (alignState.showAddDialog && alignState.draftTargetEntity.isValid) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
            if (ImGui::Button("CANCEL ALIGNMENT TARGET", ImVec2(-1, 0))) {
                alignState.Reset();
                AppConsole::Log("[Metrology] Alignment handoff cancelled.");
            }
            ImGui::PopStyleColor();
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
            if (ImGui::Button(ICON_FA_OBJECT_GROUP "  USE AS ALIGNMENT TARGET", ImVec2(-1, 0))) {
                int lastIdx = -1;
                for (int i = (int)measureState.history.size() - 1; i >= 0; --i) {
                    if (measureState.history[i].type == MeasureTab::Circle) {
                        lastIdx = i; break;
                    }
                }
                if (lastIdx >= 0) {
                    auto& lastCircle = measureState.history[lastIdx];
                    alignState.Reset();
                    alignState.showAddDialog = true;
                    alignState.draftTargetEntity = lastCircle.entity;
                    alignState.draftTargetObjId = lastCircle.entity.objectId;
                    alignState.draftTargetType = AlignEntitySelect::Circle;
                    alignState.step = AlignStep::PickingSource;
                    AppConsole::Log("[Metrology] Handoff successful. Circle injected as Target for Smart Assembly.");
                }
            }
            ImGui::PopStyleColor();
        }
    }
}
