#include "../ui_manager.hpp"
#include "../../core/scene_manager.hpp"
#include "../../core/scene_object.hpp"
#include "../../core/camera.hpp"
#include "../../core/app_settings.hpp"
#include "../ui_state.hpp"
#include "../../core/snap_engine.hpp"
#include <imgui.h>
#include <ImGuizmo/ImGuizmo.h>
#include "ui_viewport_helpers.hpp"

void UIManager::UpdateHoverSnapState(ImVec2 vpPos, ImVec2 vpSize, Camera& camera, SceneManager& scene, bool isHovered) {
    auto objects = scene.GetObjects();
    
    // Removing '!toolActive' condition to allow SSOT picking universally
    if (!isHovered || ImGuizmo::IsUsing()) {
        hoverSnapState.Reset(); return;
    }
    ImVec2 mousePos = ImGui::GetMousePos();
    Ray ray = MathUtils::ScreenToWorldRay(glm::vec2(mousePos.x - vpPos.x, mousePos.y - vpPos.y), glm::vec2(vpSize.x, vpSize.y), camera.viewMatrix, camera.projMatrix);
    static std::string lastId = ""; static uint32_t lastTri = (uint32_t)-1; static bool lastToolActive = false;
    HoverSnapState newSnap = SnapEngine::Query(ray, Vec2(mousePos.x, mousePos.y), objects, camera, Vec2(vpSize.x, vpSize.y), Vec2(vpPos.x, vpPos.y), measureState.active && measureState.xRayEnabled, 20.0f);
    
    bool toolActive = measureState.active || alignState.step != AlignStep::Idle || alignState.showAddDialog;
    
    bool changed = (newSnap.snapObjectId != lastId);
    if (!changed && newSnap.isSnapped && !newSnap.snappedEntity.constituentIndices.empty()) {
        if (newSnap.snappedEntity.constituentIndices[0] != lastTri) changed = true;
    }
    if (toolActive != lastToolActive) changed = true;
    
    bool shouldHighlight = newSnap.isSnapped && toolActive;

    if (changed || !shouldHighlight) {
        for (auto& obj : objects) {
            if (obj->mesh && (!obj->mesh->highlightIndices.empty() || !obj->mesh->highlightEdgeIndices.empty())) {
                obj->mesh->highlightIndices.clear(); obj->mesh->highlightEdgeIndices.clear();
                obj->mesh->UpdateHighlightGPU(); obj->mesh->UpdateHighlightEdgeGPU();
            }
        }
    }
    if (shouldHighlight && changed) {
        for (auto& obj : objects) {
            if (obj->id == newSnap.snapObjectId && obj->mesh) {
                // Determine if we should visualize highlight
                if (newSnap.snapType == 1) { obj->mesh->highlightEdgeIndices = newSnap.snappedEntity.constituentIndices; obj->mesh->UpdateHighlightEdgeGPU(); }
                else { obj->mesh->highlightIndices = newSnap.snappedEntity.constituentIndices; obj->mesh->UpdateHighlightGPU(); }
                break;
            }
        }
    }
    lastId = newSnap.snapObjectId;
    lastTri = (newSnap.isSnapped && !newSnap.snappedEntity.constituentIndices.empty()) ? newSnap.snappedEntity.constituentIndices[0] : (uint32_t)-1;
    lastToolActive = toolActive;
    hoverSnapState = newSnap;
}

void DrawSnapCursor(ImDrawList* dl, ImVec2 vpPos, ImVec2 vpSize, Camera& camera, const HoverSnapState& snapState, const AlignmentState& alignState, bool toolActive) {
    if (!snapState.isSnapped || !toolActive) return;
    auto& s = AppSettings::Instance();
    Mat4 pv = camera.projMatrix * camera.viewMatrix;
    
    auto Project = [&](const Vec3& p) -> ImVec2 {
        Vec4 clip = pv * Vec4(p, 1.0);
        if (clip.w < 0.001f) return ImVec2(-1000, -1000);
        clip /= clip.w;
        return ImVec2((clip.x * 0.5f + 0.5f) * vpSize.x + vpPos.x, (1.0f - (clip.y * 0.5f + 0.5f)) * vpSize.y + vpPos.y);
    };

    ImU32 snapCol = ::ToCol(s.measurementColor);
    if (alignState.step == AlignStep::PickingSource) snapCol = ::ToCol(s.colorEntityA);
    else if (alignState.step == AlignStep::PickingTarget) snapCol = ::ToCol(s.colorEntityB);
    
    ImU32 ghostCol = (snapCol & IM_COL32_A_MASK) ? (snapCol & 0x00FFFFFF) | (60 << 24) : IM_COL32(255, 255, 255, 60);

    // --- Phase 40: Snap Ghost (Ideal Geometry Feedback) ---
    const DatumEntity& de = snapState.snappedEntity;
    if (de.isValid) {
        if (de.type == DatumEntity::Type::Circle) {
            Vec3 u = MathUtils::SafeNormalize(glm::cross(de.normal, Vec3(1, 0, 0)));
            if (glm::length(u) < 0.1) u = MathUtils::SafeNormalize(glm::cross(de.normal, Vec3(0, 1, 0)));
            Vec3 v = glm::cross(de.normal, u);
            const int segs = 64;
            for (int i = 0; i < segs; ++i) {
                double a1 = (i / (double)segs) * 2.0 * 3.14159;
                double a2 = ((i + 1) / (double)segs) * 2.0 * 3.14159;
                ImVec2 p1 = Project(de.point + u * std::cos(a1) * de.radius + v * std::sin(a1) * de.radius);
                ImVec2 p2 = Project(de.point + u * std::cos(a2) * de.radius + v * std::sin(a2) * de.radius);
                if (p1.x > -500 && p2.x > -500) dl->AddLine(p1, p2, ghostCol, 2.0f);
            }
        } else if (de.type == DatumEntity::Type::Plane) {
             Vec3 u = MathUtils::SafeNormalize(glm::cross(de.normal, Vec3(1, 0, 0)));
             if (glm::length(u) < 0.1) u = MathUtils::SafeNormalize(glm::cross(de.normal, Vec3(0, 1, 0)));
             Vec3 v = glm::cross(de.normal, u);
             double size = de.radius > 0 ? de.radius : 50.0;
             for (int i = -2; i <= 2; ++i) {
                 ImVec2 p1 = Project(de.point + u * (i * size * 0.5) - v * size);
                 ImVec2 p2 = Project(de.point + u * (i * size * 0.5) + v * size);
                 if (p1.x > -500 && p2.x > -500) dl->AddLine(p1, p2, ghostCol, 1.0f);
                 p1 = Project(de.point - u * size + v * (i * size * 0.5));
                 p2 = Project(de.point + u * size + v * (i * size * 0.5));
                 if (p1.x > -500 && p2.x > -500) dl->AddLine(p1, p2, ghostCol, 1.0f);
             }
        } else if (de.type == DatumEntity::Type::Cylinder) {
             Vec3 u = MathUtils::SafeNormalize(glm::cross(de.normal, Vec3(1, 0, 0)));
             if (glm::length(u) < 0.1) u = MathUtils::SafeNormalize(glm::cross(de.normal, Vec3(0, 1, 0)));
             Vec3 v = glm::cross(de.normal, u);
             double h = 40.0; // Visual height
             for (int j = -1; j <= 1; j += 2) {
                 Vec3 base = de.point + de.normal * (h * 0.5 * j);
                 for (int i = 0; i < 32; ++i) {
                     double a1 = (i / 32.0) * 6.28318;
                     double a2 = ((i + 1) / 32.0) * 6.28318;
                     ImVec2 p1 = Project(base + u * std::cos(a1) * de.radius + v * std::sin(a1) * de.radius);
                     ImVec2 p2 = Project(base + u * std::cos(a2) * de.radius + v * std::sin(a2) * de.radius);
                     if (p1.x > -500 && p2.x > -500) dl->AddLine(p1, p2, ghostCol, 1.5f);
                 }
             }
        } else if (de.type == DatumEntity::Type::Sphere) {
             const int segs = 32;
             Vec3 axes[3] = { {1,0,0}, {0,1,0}, {0,0,1} };
             for (int k = 0; k < 3; ++k) {
                 Vec3 u = axes[k], v = axes[(k+1)%3], n = axes[(k+2)%3];
                 for (int i = 0; i < segs; ++i) {
                     double a1 = (i / (double)segs) * 6.28318;
                     double a2 = ((i + 1) / (double)segs) * 6.28318;
                     ImVec2 p1 = Project(de.point + u * std::cos(a1) * de.radius + v * std::sin(a1) * de.radius);
                     ImVec2 p2 = Project(de.point + u * std::cos(a2) * de.radius + v * std::sin(a2) * de.radius);
                     if (p1.x > -500 && p2.x > -500) dl->AddLine(p1, p2, ghostCol, 1.5f);
                 }
             }
        }
    }

    ImVec2 sp = Project(snapState.snapPoint);
    if (sp.x < -500) return;

    float thick = s.measurementThickness;
    if (snapState.snapType == 2) dl->AddCircleFilled(sp, thick * 4.0f, snapCol);
    else if (snapState.snapType == 1) dl->AddCircleFilled(sp, thick * 2.5f, snapCol);
    else dl->AddCircleFilled(sp, thick * 1.5f, IM_COL32(255, 255, 255, 180));
}
