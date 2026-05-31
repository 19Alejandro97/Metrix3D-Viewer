#pragma once
#include "interaction/viewport_event.hpp"
#include "../../core/camera.hpp"
#include "../../core/scene_manager.hpp"

class ViewportCameraController {
public:
    static void HandleInput(const ViewportEvent& e, Camera& camera, SceneManager& scene);
    static void DrawPivotOverlay(ImDrawList* dl, const ViewportEvent& e, Camera& camera, SceneManager& scene);
};
