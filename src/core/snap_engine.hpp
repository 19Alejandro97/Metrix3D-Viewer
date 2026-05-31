#pragma once
#include "math_types.hpp"
#include "datum_entity.hpp"
#include "scene_object.hpp"
#include "../ui/ui_state.hpp"
#include <memory>
#include <vector>

// --- Phase 29: SnapEngine - Pure Functional Geometry Query Service ---
// Decouples Raycasting/Detection from Visual/GPU highlights.
class SnapEngine {
public:
    // Performs a hierarchical geometric search (P1 Circles -> P2 Edges -> P3 Tesselation)
    // No side effects: No glBufferData, No Mesh modifications.
    static HoverSnapState Query(
        const Ray& ray, 
        const Vec2& mousePos,
        const std::vector<std::shared_ptr<SceneObject>>& objects, 
        const class Camera& camera,
        const Vec2& vpSize,
        const Vec2& vpPos,
        bool isXRay,
        float snapThreshold = 20.0f
    );
};
