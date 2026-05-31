#include "../ui_manager.hpp"
#include "../../core/scene_manager.hpp"
#include "../../core/scene_object.hpp"
#include "../../core/camera.hpp"
#include "../../core/app_settings.hpp"
#include "../app_console.hpp"
#include "../ui_state.hpp"
#include <imgui.h>
#include <ImGuizmo/ImGuizmo.h>
#include "../IconsFontAwesome5.h"
#include "../../core/math_utils.hpp"
#include <cstdio>
#include <algorithm>
#include "../../core/feature_extraction.hpp"
#include "../../core/mesh.hpp"

#include "ui_viewport_camera_ctrl.hpp"
#include "interaction/viewport_event.hpp"
#include "ui_viewport_helpers.hpp"

void UIManager::DrawViewport(unsigned int textureID, Camera& camera, SceneManager& scene) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImVec2 pos  = ImGui::GetCursorScreenPos();
    ImVec2 size = ImGui::GetContentRegionAvail();
    viewportPos  = pos;
    viewportSize = size;

    ImGui::Image((ImTextureID)(intptr_t)textureID, size, ImVec2(0, 1), ImVec2(1, 0));
    bool isViewportHovered = ImGui::IsItemHovered();

    // Prepare Event Data
    ViewportEvent e;
    e.mousePos       = ImGui::GetMousePos();
    e.viewportPos    = pos;
    e.viewportSize   = size;
    e.localMouse     = ImVec2(e.mousePos.x - pos.x, e.mousePos.y - pos.y);
    e.isHovered      = isViewportHovered;
    e.ray            = MathUtils::ScreenToWorldRay(glm::vec2(e.localMouse.x, e.localMouse.y), glm::vec2(size.x, size.y), camera.viewMatrix, camera.projMatrix);
    e.mouseDelta     = ImGui::GetIO().MouseDelta;
    e.mouseWheel     = ImGui::GetIO().MouseWheel;
    e.isLeftClicked  = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
    e.isLeftDragging = ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.5f);
    e.isRightDragging = ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.5f);
    e.isMiddleDown   = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
    e.ctrlHeld       = ImGui::GetIO().KeyCtrl;
    e.shiftHeld      = ImGui::GetIO().KeyShift;

    // Prepare Tool Context
    ViewportContext ctx = { measureState, alignState, hoverSnapState, isGizmoOnObject, gizmoAnchorId, gizmoMode, gizmoSpace };

    // --- Interaction Dispatching ---
    if (isViewportHovered) {
        UpdateHoverSnapState(pos, size, camera, scene, true);
    } else {
        hoverSnapState.Reset();
    }

    // Refined Input Consumption for Gizmo-Block (Explicit and Restrictive)
    bool isInputConsumed = (measureState.draggingMeasurementIndex != -1) || 
                           (measureState.hoveredMeasurementIndex != -1) ||
                           (alignState.step != AlignStep::Idle);

    // Phase 1: Gizmo Orchestration (State Calculation)
    // Run Gizmo manipulation FIRST so IsUsing() and IsOver() reflect CURRENT frame's reality.
    ImGuizmo::SetOrthographic(camera.isOrthographic);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(pos.x, pos.y, size.x, size.y);

    SceneObject* primaryActive = scene.GetSelectedObject().get();
    
    // UCS Widget (Origin axes and docking)
    DrawUCSWidget(pos, size, camera, scene, primaryActive);

    if (isGizmoOnObject && primaryActive) {
        ImGuizmo::SetID(1);  // Explicit ID 1 for transform gizmo
        // DrawTransformGizmo calls ImGuizmo::Manipulate which computes intersections immediately
        DrawTransformGizmo(pos, size, camera, scene, isInputConsumed);
    }

    // Phase 2: Camera Control (State-Guarded)
    if (ImGui::IsWindowHovered() && !ImGuizmo::IsUsing() && !isInputConsumed) {
        ViewportCameraController::HandleInput(e, camera, scene);
    }

    // Phase 3: Interaction Manager (Tools)
    // Execution benefits from accurate ImGuizmo::IsOver() to yield safely
    interactionManager.Update(e, camera, scene, ctx);

    // Phase 4: Overlay Painting & ViewCube
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    interactionManager.RenderOverlays(drawList, e, camera, ctx);
    
    // DrawAlignmentOverlays removed - covered by DrawAlignmentAxes
    bool toolActive = measureState.active || alignState.step != AlignStep::Idle || alignState.showAddDialog;
    DrawSnapCursor(drawList, pos, size, camera, hoverSnapState, alignState, toolActive);
    DrawMeasurementAnnotations(drawList, pos, size, camera, measureState);
    
    // Restore Coordinate Overlays
    DrawPieceUCS(drawList, pos, size, camera, scene);
    DrawAlignmentAxes(drawList, pos, size, camera);

    ViewportCameraController::DrawPivotOverlay(drawList, e, camera, scene);
    DrawViewCube(pos, size, camera);

    ImGui::End();
    ImGui::PopStyleVar();
}
