#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// CPU Math - Double precision for accurate geometric calculations (Zero latency / high precision)
using Vec2  = glm::dvec2;
using Vec3  = glm::dvec3;
using Vec4  = glm::dvec4;
using Mat3  = glm::dmat3;
using Mat4  = glm::dmat4;
using Quat  = glm::dquat;

// GPU Math - Float precision for rendering
using Vec2f = glm::vec2;
using Vec3f = glm::vec3;
using Mat3f = glm::mat3;
using Mat4f = glm::mat4;

// Axis Aligned Bounding Box
struct AABB {
    Vec3 min = { 1e9,  1e9,  1e9};
    Vec3 max = {-1e9, -1e9, -1e9};

    Vec3 Size()   const { return max - min; }
    Vec3 Center() const { return (min + max) * 0.5; }
    bool IsEmpty() const { return min.x > max.x; }
    
    void Expand(const Vec3& p) {
        min = glm::min(min, p);
        max = glm::max(max, p);
    }
    void Expand(const AABB& b) {
        min = glm::min(min, b.min);
        max = glm::max(max, b.max);
    }
};
