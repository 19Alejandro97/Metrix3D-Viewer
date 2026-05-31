#pragma once
#include "math_types.hpp"
#include <vector>
#include <string>

// Represents a geometric datum entity picked from a mesh
struct DatumEntity {
    enum class Type { None, Circle, Plane, Line, Sphere, Cylinder, SinglePoint };

    Type    type    = Type::None;
    Vec3    point   = {0,0,0};  // Center for circle/plane, start for line
    Vec3    normal  = {0,1,0};  // Normal for circle/plane, direction for line
    Vec3    extra   = {0,0,0};  // p2 for line, or aux
    double  radius  = 0.0;      // For circle
    float   importance = 1.0f;  // Score for snapping prioritization (higher = more important)
    std::string objectId;       // ID of the SceneObject this was picked from

    std::vector<Vec3> pickedPoints; // Raw input points (2 or 3)
    std::vector<uint32_t> constituentIndices; // Primary geometry (Triangles for planes, Edges for loops)
    std::vector<uint32_t> secondaryIndices;   // Secondary geometry (e.g., triangle faces for a circle)
    bool isValid = false;

    static DatumEntity FromCircle(const Vec3& p1, const Vec3& p2, const Vec3& p3, const std::string& objId);
    static DatumEntity FromPlane (const Vec3& p1, const Vec3& p2, const Vec3& p3, const std::string& objId);
    static DatumEntity FromLine  (const Vec3& p1, const Vec3& p2, const std::string& objId);

    const char* TypeName() const {
        switch(type) {
            case Type::Circle:   return "Circle";
            case Type::Plane:    return "Plane";
            case Type::Line:     return "Line";
            case Type::Sphere:   return "Sphere";
            case Type::Cylinder: return "Cylinder";
            default:             return "None";
        }
    }

    int RequiredPoints() const {
        switch(type) {
            case Type::Line:     return 2;
            case Type::Sphere:   return 4;
            case Type::Cylinder: return 3; 
            default:             return 3;
        }
    }
};
