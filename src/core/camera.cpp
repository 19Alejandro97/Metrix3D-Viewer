#include "camera.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <algorithm>
#include <cmath>

Camera::Camera() {
    smoothTarget      = target;
    smoothDistance    = distance;
    smoothOrientation = orientation;
    Update(100, 100);
}

// ---------------------------------------------------------------------------
// Update — SLERP Integration + View Matrix Reconstruction
// ---------------------------------------------------------------------------
void Camera::Update(int viewportWidth, int viewportHeight) {
    if (viewportWidth > 0 && viewportHeight > 0)
        aspectRatio = (double)viewportWidth / (double)viewportHeight;

    // 1. Smooth Integration
    double a = smoothFactor;
    smoothTarget      = glm::mix(smoothTarget, target, a);
    smoothDistance    = glm::mix(smoothDistance, distance, a);
    smoothOrientation = glm::slerp(smoothOrientation, orientation, a);
    smoothOrientation = glm::normalize(smoothOrientation);

    // 2. View Matrix Reconstruction (CAD Quaternion Model)
    // View = Translate(0,0,-dist) * Mat4(inverse(orientation)) * Translate(-target)
    Mat4 translationToTarget = glm::translate(Mat4(1.0), -smoothTarget);
    Mat4 rotation            = glm::mat4_cast(glm::inverse(smoothOrientation));
    Mat4 translationFromEye  = glm::translate(Mat4(1.0), Vec3(0.0, 0.0, -smoothDistance));

    viewMatrix = Mat4f(translationFromEye * rotation * translationToTarget);

    // 3. Projection
    if (isOrthographic) {
        // Map the current distance and fovY to a visible height (Match perspective size at target)
        // height = 2 * distance * tan(fovY / 2)
        double h = 2.0 * smoothDistance * std::tan(glm::radians(fovY * 0.5));
        double w = h * aspectRatio;
        projMatrix = Mat4f(glm::ortho(-w * 0.5, w * 0.5, -h * 0.5, h * 0.5, nearPlane, farPlane));
    } else {
        projMatrix = Mat4f(glm::perspective(glm::radians(fovY), aspectRatio, nearPlane, farPlane));
    }
}

// ---------------------------------------------------------------------------
// Orbit — Z-Up Trackball Virtual (Grip Mode)
// ---------------------------------------------------------------------------
void Camera::Orbit(double deltaX, double deltaY, bool useTurntable) {
    const double speed = 0.25;

    // Grip Mode: Negative deltas = "grab and drag" feel
    double dYaw   = -deltaX * speed;
    double dPitch = -deltaY * speed;

    glm::dquat deltaQ;
    if (useTurntable) {
        // --- TURNTABLE (Z-Up Rigid) ---
        // Yaw is strictly world-Z to keep the horizon flat (Poste Rígido)
        glm::dquat qYaw = glm::angleAxis(glm::radians(dYaw), Vec3(0.0, 0.0, 1.0));
        Vec3 camRight = orientation * Vec3(1.0, 0.0, 0.0);
        glm::dquat qPitch = glm::angleAxis(glm::radians(dPitch), camRight);
        deltaQ = qYaw * qPitch;
    } else {
        // --- VIRTUAL TRACKBALL (Organic Free-Space) ---
        // Both axes follow the camera orientation for free-axis rotation (Petting the Sphere)
        Vec3 camRight = orientation * Vec3(1.0, 0.0, 0.0);
        Vec3 camUp    = orientation * Vec3(0.0, 1.0, 0.0);
        glm::dquat qYaw   = glm::angleAxis(glm::radians(dYaw),   camUp);
        glm::dquat qPitch = glm::angleAxis(glm::radians(dPitch), camRight);
        deltaQ = qYaw * qPitch; // Combined rotation allows banking/rolling
    }

    // 2. Apply Rotation (Always eccentric if pivot is defined)
    if (useCustomPivot) {
        Vec3 eye = target + orientation * Vec3(0.0, 0.0, distance);
        
        Vec3 relTarget = target - orbitPivot;
        Vec3 relEye    = eye    - orbitPivot;

        Vec3 newRelTarget = deltaQ * relTarget;
        Vec3 newRelEye    = deltaQ * relEye;

        target      = newRelTarget + orbitPivot;
        Vec3 newEye = newRelEye    + orbitPivot;

        orientation = glm::normalize(deltaQ * orientation);
        distance    = glm::length(newEye - target);
    } else {
        orientation = glm::normalize(deltaQ * orientation);
    }
}

void Camera::Pan(double deltaX, double deltaY) {
    // Proportional speed: movement feels constant relative to zoom level
    double panSpeed = distance * 0.0015;

    // Direct axes derivation from current orientation
    Vec3 camRight = orientation * Vec3(1.0, 0.0, 0.0);
    Vec3 camUp    = orientation * Vec3(0.0, 1.0, 0.0);

    // Grip Mode: Target moves opposite to mouse drag to keep objects under the cursor
    Vec3 translation = -(camRight * (deltaX * panSpeed)) + (camUp * (deltaY * panSpeed));

    target += translation;
    
    // Sync HUD/Smooth state: prevent "focal drift" or trailing 'X' marker after pan
    smoothTarget = target;

    // Custom pivots (Global Anchor or Piece Centroid) are static in world space.
    // They MUST NOT move when the camera pans/displaces the viewport.
}

void Camera::Zoom(double delta) {
    double moveDist = distance * (delta * 0.1);
    distance -= moveDist;
    if (distance < 0.1) distance = 0.1;
}

void Camera::ZoomTowardPoint(const Vec3& hitPt, double delta) {
    target = glm::mix(target, hitPt, 0.1);
    Zoom(delta);
}

void Camera::SetOrbitPivot(const Vec3& pivot) {
    useCustomPivot = true;
    orbitPivot     = pivot;

    // CRITICAL: Silent Pivot Shift.
    // We DO NOT change target or orientation here to avoid visual jumps.
    // However, we snap the smooth state so the next Update() doesn't LERP from a stale position.
    smoothTarget      = target;
    smoothDistance    = distance;
    smoothOrientation = orientation;
}

void Camera::ClearOrbitPivot() {
    useCustomPivot = false;
}

// ---------------------------------------------------------------------------
// FocusOn — Frame a bounded region
// ---------------------------------------------------------------------------
void Camera::FocusOn(const Vec3& center, double radius) {
    target   = center;
    double alpha = 0.4 * glm::radians(fovY);
    distance = (radius < 1.0) ? 10.0 : radius / std::tan(alpha);

    // Snap smooth state immediately to avoid lerp drift
    smoothTarget   = target;
    smoothDistance = distance;
}

// ---------------------------------------------------------------------------
// ExtractStateFromMatrix — Sync from ViewCube external manipulation
// ---------------------------------------------------------------------------
void Camera::SetOrientation(const glm::dquat& q) {
    orientation = glm::normalize(q);
}

void Camera::SnapToOrientation(const glm::dquat& q) {
    orientation = glm::normalize(q);
    smoothOrientation = orientation;
    smoothTarget      = target;
    smoothDistance    = distance;
}
