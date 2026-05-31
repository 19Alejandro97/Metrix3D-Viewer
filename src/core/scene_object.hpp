#pragma once
#include "transform.hpp"
#include "mesh.hpp"
#include "material.hpp"
#include "math_utils.hpp"
#include <memory>
#include <string>

class SceneManager;

class SceneObject {
public:
    SceneManager*         scene = nullptr; // Back-pointer to scene
    std::string           id;          // Unique identifier
    std::string           name;        // Display name in Outliner
    bool                  isVisible  = true;
    bool                  isSelected = false;  // Highlighted in outliner
    bool                  isActive   = false;  // Active for transform (can be >1)

    Transform             transform;
    std::shared_ptr<Mesh> mesh;
    Material              material;

    // Returns the piece's local coordinate system origin in world space
    // (used for gizmo placement — NOT the AABB centroid)
    Vec3 GetLocalOrigin() const { return transform.position; }

    // Returns the 3x3 rotation of this piece's local axes (for UCS widget)
    Mat3 GetLocalAxes() const { return Mat3(transform.worldMatrix); }

    SceneObject(const std::string& name, std::shared_ptr<Mesh> mesh);
    
    // --- Phase 3: Kinematic Assembly ---
    bool      isAssembled = false;
    float     axialAngle  = 0.0f;     // Local Z-rotation in Radians
    Mat4      baseAlignmentMatrix = Mat4(1.0); // Reference world matrix post-alignment

    // Recalculates worldMatrix as Base * RotZ(axialAngle)
    void UpdateAssembledTransform();
    void UnlockAssembly() { isAssembled = false; }

    // Returns the AABB transformed into world space
    AABB GetWorldAABB() const;

    // Dimensions of the object taking into account scale
    Vec3 GetDimensions() const;

    // Casts a ray (in World Space) against this object's mesh
    // If allHits is not null, it will be populated with all intersections sorted by world distance
    bool RaycastHitTriangle(const Ray& worldRay, RaycastHit& outHit, std::vector<RaycastHit>* allHits = nullptr) const;
};
