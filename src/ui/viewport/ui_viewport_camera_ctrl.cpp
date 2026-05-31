#include "ui_viewport_camera_ctrl.hpp"
#include <imgui.h>
#include <algorithm>
#include <cmath>

void ViewportCameraController::HandleInput(const ViewportEvent& e, Camera& camera, SceneManager& scene) {
    if (!e.isHovered) return;

    float dx = e.mouseDelta.x;
    float dy = e.mouseDelta.y;

    // --- Orbit ---
    if (e.isLeftClicked) {
        AABB combined;
        for (auto& obj : scene.GetObjects()) combined.Expand(obj->GetWorldAABB());
        if (!combined.IsEmpty()) camera.SetOrbitPivot(combined.Center());
        else camera.ClearOrbitPivot();
    } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        SceneObject* pivotPiece = nullptr;
        for (auto& obj : scene.GetObjects()) if (obj->isActive) { pivotPiece = obj.get(); break; }
        if (pivotPiece) camera.SetOrbitPivot(pivotPiece->GetWorldAABB().Center());
        else {
            AABB combined;
            for (auto& obj : scene.GetObjects()) combined.Expand(obj->GetWorldAABB());
            if (!combined.IsEmpty()) camera.SetOrbitPivot(combined.Center());
            else camera.ClearOrbitPivot();
        }
    }

    if (e.isLeftDragging) camera.Orbit(dx, dy, false);
    else if (e.isRightDragging) camera.Orbit(dx, dy, true);

    // --- Pan ---
    if (e.isMiddleDown) camera.Pan(dx, dy);

    // --- Zoom ---
    if (std::abs(e.mouseWheel) > 0.01f) {
        // We need a snap point for ZoomTowardPoint, but since camera ctrl is global, 
        // we check if we have a snapped point in UI state (passed via event or global)
        // For now, let's just use the event's snap state if we decide to add it.
        // Or access it via Scene.
        camera.Zoom((double)e.mouseWheel);
    }

    // --- Focus (F) ---
    if (ImGui::IsKeyPressed(ImGuiKey_F)) {
        auto sel = scene.GetSelectedObject();
        if (sel) {
            AABB wb = sel->GetWorldAABB();
            camera.FocusOn(wb.Center(), glm::length(wb.Size()) * 0.5);
        } else {
            AABB combined;
            for (auto& obj : scene.GetObjects()) combined.Expand(obj->GetWorldAABB());
            if (!combined.IsEmpty()) camera.FocusOn(combined.Center(), glm::length(combined.Size()) * 0.5);
        }
    }
}

void ViewportCameraController::DrawPivotOverlay(ImDrawList* dl, const ViewportEvent& e, Camera& camera, SceneManager& scene) {
    bool isAnyOrbit = e.isLeftDragging || e.isRightDragging;
    if (isAnyOrbit && camera.useCustomPivot) {
        Vec3 pivot3D = camera.orbitPivot;
        Vec4 clip = camera.projMatrix * camera.viewMatrix * Vec4(pivot3D, 1.0);
        if (clip.w > 0.001f) {
            clip /= clip.w;
            float sx = (clip.x * 0.5f + 0.5f) * e.viewportSize.x + e.viewportPos.x;
            float sy = (1.0f - (clip.y * 0.5f + 0.5f)) * e.viewportSize.y + e.viewportPos.y;
            
            if (sx > e.viewportPos.x && sx < e.viewportPos.x + e.viewportSize.x && 
                sy > e.viewportPos.y && sy < e.viewportPos.y + e.viewportSize.y) {
                
                if (e.isLeftDragging) {
                    AABB combined;
                    for (auto& obj : scene.GetObjects()) combined.Expand(obj->GetWorldAABB());
                    if (!combined.IsEmpty()) {
                        float radius3D = (float)glm::length(combined.max - combined.Center());
                        float focalLength = e.viewportSize.y / (2.0f * std::tan(glm::radians((float)camera.fovY) * 0.5f));
                        float radius2D = glm::clamp((radius3D * focalLength) / (float)camera.smoothDistance, 60.0f, e.viewportSize.x * 0.45f);
                        dl->AddCircle(ImVec2(sx, sy), radius2D, IM_COL32(255, 255, 255, 40), 64, 1.5f);
                    }
                } else {
                    const float arm = 10.0f; const ImU32 colCross = IM_COL32(220, 220, 220, 200);
                    dl->AddLine(ImVec2(sx - arm, sy - arm), ImVec2(sx + arm, sy + arm), colCross, 1.5f);
                    dl->AddLine(ImVec2(sx + arm, sy - arm), ImVec2(sx - arm, sy + arm), colCross, 1.5f);
                }
            }
        }
    }
}
