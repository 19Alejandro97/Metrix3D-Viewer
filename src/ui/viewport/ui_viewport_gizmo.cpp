#include "../ui_manager.hpp"
#include "../../core/scene_manager.hpp"
#include "../../core/scene_object.hpp"
#include "../../core/camera.hpp"
#include "../../core/app_settings.hpp"
#include "../ui_state.hpp"
#include <imgui.h>
#include <ImGuizmo/ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>

void UIManager::DrawUCSWidget(ImVec2 vpPos, ImVec2 vpSize, Camera& camera,
                               SceneManager& scene, SceneObject* activePiece) {
    const float ucsSize = 100.0f;
    const float pad     = 20.0f;
    const float lineLen = 38.0f;

    ImVec2 center(vpPos.x + vpSize.x - ucsSize * 0.5f - pad,
                  vpPos.y + ucsSize * 0.5f + pad);

    glm::dmat3 rotView = glm::mat3_cast(glm::inverse(camera.smoothOrientation));

    glm::vec3 axW[3] = {
        glm::vec3(rotView * glm::dvec3(1.0, 0.0, 0.0)),
        glm::vec3(rotView * glm::dvec3(0.0, 1.0, 0.0)),
        glm::vec3(rotView * glm::dvec3(0.0, 0.0, 1.0))
    };
    const ImU32 axCol[3] = { IM_COL32(220,  60,  60, 255),
                             IM_COL32(  80, 200,  80, 255),
                             IM_COL32(  80, 100, 220, 255) };
    const char* axLabel[3] = { "X", "Y", "Z" };

    struct AxisDesc { int idx; float z; };
    AxisDesc order[3] = { {0, axW[0].z}, {1, axW[1].z}, {2, axW[2].z} };
    std::sort(order, order + 3, [](const AxisDesc& a, const AxisDesc& b) {
        return a.z < b.z; 
    });

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddCircleFilled(center, lineLen + 8.0f, IM_COL32(20, 20, 25, 160));

    for (int i = 0; i < 3; ++i) {
        int      idx = order[i].idx;
        glm::vec3 v  = axW[idx];
        ImVec2   tip = ImVec2(center.x + v.x * lineLen,
                               center.y - v.y * lineLen); 
        ImU32 col    = axCol[idx];
        if (v.z < -0.05f) col = (col & 0x00FFFFFF) | 0x55000000;

        dl->AddLine(center, tip, col, 2.5f);
        dl->AddCircleFilled(tip, 5.0f, col);
        dl->AddText(ImVec2(tip.x + 5.0f, tip.y - 7.0f), col, axLabel[idx]);
    }

    dl->AddCircleFilled(center, 5.0f, IM_COL32(255, 255, 255, 220));
    dl->AddCircle(center, 5.0f, IM_COL32(0, 0, 0, 180));

    if (isGizmoOnObject) dl->AddCircle(center, 9.0f, IM_COL32(80, 220, 80, 200), 0, 2.0f);

    if (activePiece) {
        ImGui::SetCursorScreenPos(ImVec2(center.x - 12.0f, center.y - 12.0f));
        if (ImGui::InvisibleButton("##UCS_Dock", ImVec2(24.0f, 24.0f))) {
            if (isGizmoOnObject) ResetGizmoState();
        }
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            isDraggingToDock = true;
            dl->AddLine(center, ImGui::GetIO().MousePos, IM_COL32(255, 255, 180, 200), 1.5f);
        }
        if (ImGui::IsItemDeactivated() && isDraggingToDock) {
            isDraggingToDock = false;
            if (hoverSnapState.isSnapped && !hoverSnapState.snapObjectId.empty()) {
                DockToPiece(scene, hoverSnapState.snapObjectId);
            } else if (activePiece) {
                // If released in middle of nowhere but we had an active piece, 
                // just re-dock to it or keep current state.
                DockToPiece(scene, activePiece->id);
            }
        }
    }
    if (isGizmoOnObject && ImGui::IsKeyPressed(ImGuiKey_Escape)) ResetGizmoState();
}

bool UIManager::DrawTransformGizmo(ImVec2 vpPos, ImVec2 vpSize, Camera& camera,
                                  SceneManager& scene, bool blockInteraction) {
    auto objects = scene.GetObjects();
    std::vector<SceneObject*> actives;
    SceneObject* anchor = nullptr;
    for (auto& obj : objects) {
        if (obj->isActive) {
            actives.push_back(obj.get());
            if (obj->id == gizmoAnchorId) anchor = obj.get();
        }
    }
    if (actives.empty()) { ResetGizmoState(); return false; }
    if (!anchor) anchor = actives[0];
    if (anchor->isAssembled) return false; 

    ImGuizmo::SetRect(vpPos.x, vpPos.y, vpSize.x, vpSize.y);
    ImGuizmo::SetID(1);
    glm::mat4 view = camera.viewMatrix;
    glm::mat4 proj = camera.projMatrix;

    ImGuizmo::OPERATION op = (ImGuizmo::OPERATION)(ImGuizmo::TRANSLATE_X | ImGuizmo::TRANSLATE_Y | ImGuizmo::TRANSLATE_Z);
    if (gizmoMode == 1)      op = ImGuizmo::ROTATE;
    else if (gizmoMode == 2) op = ImGuizmo::SCALE;
    
    ImGuizmo::MODE mode = ImGuizmo::LOCAL;
    glm::mat4 gizmoMatrix_f = glm::mat4(anchor->transform.worldMatrix);

    ImGuizmo::Enable(!blockInteraction);
    bool moved = ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj), op, mode, glm::value_ptr(gizmoMatrix_f));

    static bool wasUsingGizmo = false;
    static Vec3 cumulativeDragDelta(0.0);

    if (moved && ImGuizmo::IsUsing()) {
        Mat4 gizmoMatrix_new = Mat4(gizmoMatrix_f);
        Mat4 gizmoMatrix_old = anchor->transform.worldMatrix;
        Mat4 worldDelta = gizmoMatrix_new * glm::inverse(gizmoMatrix_old);

        for (auto* obj : actives) {
            obj->transform.worldMatrix = worldDelta * obj->transform.worldMatrix;
            obj->transform.ExtractFromMatrix(obj->transform.worldMatrix);
            obj->transform.Rebuild();
        }
        if (op == (ImGuizmo::OPERATION)(ImGuizmo::TRANSLATE_X | ImGuizmo::TRANSLATE_Y | ImGuizmo::TRANSLATE_Z)) {
            Vec3 frameDelta = Vec3(gizmoMatrix_new[3]) - Vec3(gizmoMatrix_old[3]);
            cumulativeDragDelta += frameDelta;
        }
        wasUsingGizmo = true;
    } else if (!ImGuizmo::IsUsing()) {
        wasUsingGizmo = false;
        cumulativeDragDelta = Vec3(0.0);
    }

    if (wasUsingGizmo) {
        ImGui::BeginTooltip();
        if (op == ImGuizmo::ROTATE) ImGui::Text("Rotating piece...");
        else if (op == ImGuizmo::SCALE) ImGui::Text("Scaling piece...");
        else {
            ImGui::Text("Translate Delta X: %+.2f mm", (float)cumulativeDragDelta.x);
            ImGui::Text("Translate Delta Y: %+.2f mm", (float)cumulativeDragDelta.y);
            ImGui::Text("Translate Delta Z: %+.2f mm", (float)cumulativeDragDelta.z);
            ImGui::Text("Total Distance: %.2f mm", (float)glm::length(cumulativeDragDelta));
        }
        ImGui::EndTooltip();
    }
    return moved;
}

void DrawViewCube(ImVec2 vpPos, ImVec2 vpSize, Camera& camera) {
    float vcSide = 128.0f;
    float vcPad = 12.0f;
    ImVec2 vcPos = ImVec2(vpPos.x + vpSize.x - vcSide - vcPad, vpPos.y + vpSize.y - vcSide - vcPad);
    Mat4f bridge = glm::rotate(Mat4f(1.0f), glm::radians(90.0f), Vec3f(1, 0, 0));
    Mat4f viewCube = camera.viewMatrix * bridge;
    Mat4f originalBoundView = viewCube;

    ImGuizmo::SetID(0);
    ImGuizmo::SetRect(vcPos.x, vcPos.y, vcSide, vcSide);
    float dist = (float)camera.smoothDistance;

    ImGuizmo::ViewManipulate(glm::value_ptr(viewCube), dist, vcPos, ImVec2(vcSide, vcSide), 0x18000000);

    bool hasChanged = false;
    const float* a = glm::value_ptr(originalBoundView);
    const float* b = glm::value_ptr(viewCube);
    for (int i = 0; i < 16; ++i) if (std::abs(a[i] - b[i]) > 0.0001f) { hasChanged = true; break; }

    if (hasChanged) {
        Mat4f finalView = viewCube * glm::inverse(bridge);
        Mat3f rotMat = Mat3f(finalView);
        
        // Detect if we snapped to a cardinal axis (Top, Front, etc.)
        bool isCardinal = false;
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 3; ++j) {
                if (std::abs(std::abs(rotMat[i][j]) - 1.0f) < 0.001f) {
                    isCardinal = true;
                    break;
                }
            }
            if (isCardinal) break;
        }
        if (isCardinal) AppSettings::Instance().isOrthographic = true;

        glm::quat newQ = glm::normalize(glm::quat_cast(glm::inverse(rotMat)));
        if (AppSettings::Instance().instantViewJump) camera.SnapToOrientation(glm::dquat(newQ));
        else camera.SetOrientation(glm::dquat(newQ));
    }
}
