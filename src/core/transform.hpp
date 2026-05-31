#pragma once
#include "math_types.hpp"
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <cstring>

class Transform {
public:
    // Single Source of Truth for position/rotation/scale
    Vec3 position    = {0.0, 0.0, 0.0};
    Vec3 eulerAngles = {0.0, 0.0, 0.0}; // Degrees (Pitch, Yaw, Roll)
    Vec3 scale       = {1.0, 1.0, 1.0};

    // Cached Matrix
    Mat4 worldMatrix = Mat4(1.0);
    bool isDirty     = true;

    // Recalculates the Matrix from position/euler/scale
    void Rebuild() {
        Mat4 t  = glm::translate(Mat4(1.0), position);
        Mat4 rx = glm::rotate(Mat4(1.0), glm::radians(eulerAngles.x), {1.0, 0.0, 0.0});
        Mat4 ry = glm::rotate(Mat4(1.0), glm::radians(eulerAngles.y), {0.0, 1.0, 0.0});
        Mat4 rz = glm::rotate(Mat4(1.0), glm::radians(eulerAngles.z), {0.0, 0.0, 1.0});
        Mat4 s  = glm::scale(Mat4(1.0), scale);
        
        // TRS: Translate * RotateZ * RotateY * RotateX * Scale
        worldMatrix = t * rz * ry * rx * s;
        isDirty = false;
    }

    // Extracts position/euler/scale back from a pure Matrix
    // Used when moving the object via Gizmo
    void ExtractFromMatrix(const Mat4& mat) {
        worldMatrix = mat;
        Vec3 skew;
        Vec4 perspective;
        Quat orientation;
        
        glm::decompose(mat, scale, orientation, position, skew, perspective);
        // Note: glm::eulerAngles returns radians
        eulerAngles = glm::degrees(glm::eulerAngles(orientation));
        isDirty = false;
    }

    // Helper to get float[16] for ImGuizmo and OpenGL Shaders
    void GetMatrixFloat(float out_matrix[16]) const {
        Mat4f mf = Mat4f(worldMatrix); // Cast down to float matrix
        std::memcpy(out_matrix, &mf[0][0], 16 * sizeof(float));
    }
};
