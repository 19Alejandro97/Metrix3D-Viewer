#pragma once
#include <imgui.h>
#include "../../core/camera.hpp"
#include "../../core/scene_manager.hpp"
#include "../ui_state.hpp"

#include <cmath>

inline ImU32 ToCol(const glm::vec4& c) {
    return IM_COL32((int)(c.r*255),(int)(c.g*255),(int)(c.b*255),(int)(c.a*255));
}

inline ImVec2 WorldToScreenHelper(const Vec3& wp, const Camera& camera, ImVec2 vpPos, ImVec2 vpSize) {
    Vec4 clip = camera.projMatrix * camera.viewMatrix * Vec4(wp, 1.0);
    if (std::abs(clip.w) < 0.001f) return ImVec2(-1000, -1000);
    clip /= clip.w;
    return ImVec2((clip.x * 0.5f + 0.5f) * vpSize.x + vpPos.x, (1.0f - (clip.y * 0.5f + 0.5f)) * vpSize.y + vpPos.y);
}

// Viewcube (src/ui/viewport/ui_viewport_gizmo.cpp)
void DrawViewCube(ImVec2 vpPos, ImVec2 vpSize, Camera& camera);

// Overlays (src/ui/viewport/ui_viewport_overlays.cpp)
void DrawMeasurementAnnotations(ImDrawList* dl, ImVec2 vpPos, ImVec2 vpSize, Camera& camera, MeasurementState& measureState);

// Snapping (src/ui/viewport/ui_viewport_snapping.cpp)
void DrawSnapCursor(ImDrawList* dl, ImVec2 vpPos, ImVec2 vpSize, Camera& camera, const HoverSnapState& snapState, const AlignmentState& alignState, bool toolActive);
