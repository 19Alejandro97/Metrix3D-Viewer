#pragma once
#include "math_types.hpp"
#include <glm/gtc/quaternion.hpp>

class Camera {
public:
    // -----------------------------------------------------------------------
    // Core Transform State (Compact Quaternion Model)
    // -----------------------------------------------------------------------
    Vec3       target      = {0.0, 0.0, 0.0};      // The pivot/focus point in world space
    double     distance    = 100.0;                 // Distance from eye to target
    glm::dquat orientation = {1.0, 0.0, 0.0, 0.0}; // Camera orientation (quaternion, double precision)

    // -----------------------------------------------------------------------
    // Smooth Rendered State (SLERP/LERP Inertia)
    // -----------------------------------------------------------------------
    Vec3       smoothTarget      = {0.0, 0.0, 0.0};
    double     smoothDistance    = 100.0;
    glm::dquat smoothOrientation = {1.0, 0.0, 0.0, 0.0};

    double smoothFactor = 0.6; // 1.0 = instant, 0.6 = snappy CAD feel

    // Projection params
    bool   isOrthographic = false;
    double fovY        = 30.0;
    double nearPlane   = 0.1;   // Increased from 0.01 to avoid Z-fighting in ortho
    double farPlane    = 10000.0;
    double aspectRatio = 1.0;

    // Cached matrices (float for GPU)
    Mat4f viewMatrix;
    Mat4f projMatrix;

    // Orbit Pivot State (Smart Pivot)
    bool useCustomPivot = false;
    Vec3 orbitPivot     = {0.0, 0.0, 0.0};

    Camera();

    // Rebuild view/proj from smooth state. Call every frame.
    void Update(int viewportWidth, int viewportHeight);

    // -----------------------------------------------------------------------
    // Controls
    // -----------------------------------------------------------------------

    // Orbit — Grip-Mode Turntable (rigid Z-Up) or Free-Space Trackball (organic).
    void Orbit(double deltaX, double deltaY, bool useTurntable = true);

    // Translate the camera and target parallel to the screen plane.
    void Pan(double deltaX, double deltaY);

    // Move the camera eye along the view ray.
    void Zoom(double delta);

    // Move toward a specific world point while zooming.
    void ZoomTowardPoint(const Vec3& worldPoint, double delta);

    // Frame a bounding sphere (uses 80% viewport coverage).
    void FocusOn(const Vec3& center, double radius);

    // Set the pivot point for Smart Pivot orbits.
    void SetOrbitPivot(const Vec3& pivot);
    void ClearOrbitPivot();

    // orientation = target orientation (Update() will SLERP towards it)
    void SetOrientation(const glm::dquat& q);

    // SnapToOrientation = Atomic jump (Update() will see identity diff)
    void SnapToOrientation(const glm::dquat& q);

    // Get the camera's eye position (derived from smooth state).
    Vec3 GetPosition() const {
        return Vec3(smoothTarget) + glm::dquat(smoothOrientation) * Vec3(0, 0, smoothDistance);
    }
};
