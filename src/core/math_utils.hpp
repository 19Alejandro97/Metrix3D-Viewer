#pragma once
// ---------------------------------------------------------------------------
// MathUtils – Pure Linear Algebra & Ray Casting
// All geometric feature mining lives in feature_extraction.hpp
// All kinematic assembly logic lives in kinematic_solver.hpp
// ---------------------------------------------------------------------------
#include "math_types.hpp"
#include "datum_entity.hpp"
#include <vector>

struct Ray {
    Vec3 origin;
    Vec3 direction;

    Ray(const Vec3& o, const Vec3& d) : origin(o), direction(glm::normalize(d)) {}
};

struct RaycastHit {
    bool hit          = false;
    double distance   = -1.0;
    Vec3 point        = {0, 0, 0};
    Vec3 normal       = {0, 0, 0};
    int triangleIndex = -1;
};

class MathUtils {
public:
    // Safe Math: Prevents NaN/Inf when normalizing zero-length vectors
    static Vec3 SafeNormalize(const Vec3& v, const Vec3& fallback = Vec3(0.0, 1.0, 0.0), double epsilon = 1e-9);

    // Möller–Trumbore ray-triangle intersection
    static bool IntersectRayTriangle(const Ray& ray, const Vec3& v0, const Vec3& v1, const Vec3& v2, double& outT, Vec3& outBarycentric);

    // Fast Ray-AABB intersection
    static bool IntersectRayAABB(const Ray& ray, const AABB& box, double& tMin, double& tMax);

    // Traverses a Mesh's BVH tree to find the closest triangle intersection.
    // If allHits is not null, finds ALL intersections sorted by distance.
    static bool RaycastBVH(const class Mesh& mesh, const Ray& ray, struct RaycastHit& outHit, std::vector<struct RaycastHit>* allHits = nullptr);

    // Unprojects a 2D screen point into a 3D ray in World Space.
    // screenPos: x,y in Viewport coordinates (0,0 top-left)
    static Ray ScreenToWorldRay(const glm::vec2& screenPos, const glm::vec2& viewportSize, const Mat4& viewMatrix, const Mat4& projMatrix);

    // 3-Point Circle calculation.
    // Returns true if a valid circle can be formed; false if points are collinear.
    static bool Calculate3PointCircle(const Vec3& p1, const Vec3& p2, const Vec3& p3,
                                      Vec3& outCenter, double& outRadius, Vec3& outNormal);

    // Distance from a point to an infinite ray
    static double PointToRayDistance(const Vec3& p, const Vec3& rayOrigin, const Vec3& rayDir);

    // Shortest distance between a segment [p0, p1] and an infinite ray (origin, dir).
    // Returns the distance, and outT is the parameter along the segment [0, 1].
    static double SegmentToRayDistance(const Vec3& p0, const Vec3& p1, const Vec3& rayOrigin, const Vec3& rayDir, double& outT);

    // Calculates a sphere from 4 non-coplanar points.
    static bool CalculateSphereFrom4Points(const Vec3& p1, const Vec3& p2, const Vec3& p3, const Vec3& p4,
                                           Vec3& outCenter, double& outRadius);

    // Finds the shortest line segment connecting two infinite rays. 
    // Returns true if rays are not parallel. outCenter is the midpoint of the shortest segment.
    static bool IntersectTwoRays(const Vec3& p1, const Vec3& d1, const Vec3& p2, const Vec3& d2,
                                Vec3& outCenter, double& outDistance);
};
