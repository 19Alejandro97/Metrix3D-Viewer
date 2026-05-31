#include "math_utils.hpp"
#include "transform.hpp"
#include "mesh.hpp"
// NOTE: feature_extraction and kinematic_solver are now in their own translation units.
// math_utils.cpp contains ONLY pure linear algebra kernels.
#include <limits>
#include <cmath>
#include <algorithm>
#include <iostream>

Vec3 MathUtils::SafeNormalize(const Vec3& v, const Vec3& fallback, double epsilon) {
    double lenSq = glm::dot(v, v);
    if (lenSq > epsilon * epsilon) {
        return v / std::sqrt(lenSq);
    }
    return fallback;
}

bool MathUtils::IntersectRayAABB(const Ray& ray, const AABB& box, double& tMin, double& tMax) {
    tMin = 0.0001;
    tMax = 1e30;

    for (int i = 0; i < 3; i++) {
        double invD = 1.0 / ray.direction[i];
        double t0 = (box.min[i] - ray.origin[i]) * invD;
        double t1 = (box.max[i] - ray.origin[i]) * invD;
        if (invD < 0.0) std::swap(t0, t1);
        tMin = t0 > tMin ? t0 : tMin;
        tMax = t1 < tMax ? t1 : tMax;
        if (tMax <= tMin) return false;
    }
    return true;
}

bool MathUtils::RaycastBVH(const Mesh& mesh, const Ray& ray, RaycastHit& outHit, std::vector<RaycastHit>* allHits) {
    if (mesh.bvhTree.empty()) return false;

    struct StackNode { int nodeIdx; double t; };
    StackNode stack[64];
    int stackPtr = 0;

    double tMin, tMax;
    if (!IntersectRayAABB(ray, mesh.bvhTree[0].bounds, tMin, tMax)) return false;
    stack[stackPtr++] = {0, tMin};

    bool   hitSomething = false;
    double closestT     = 1e30;

    while (stackPtr > 0) {
        StackNode current = stack[--stackPtr];

        if (!allHits && current.t >= closestT) continue;

        const BVHNode& node = mesh.bvhTree[current.nodeIdx];

        if (node.isLeaf()) {
            for (int i = 0; i < node.triCount; ++i) {
                uint32_t triIdx = mesh.bvhIndices[node.triStart + i];
                uint32_t i0 = mesh.indices[triIdx * 3 + 0];
                uint32_t i1 = mesh.indices[triIdx * 3 + 1];
                uint32_t i2 = mesh.indices[triIdx * 3 + 2];
                Vec3 v0(mesh.vertices[i0].position);
                Vec3 v1(mesh.vertices[i1].position);
                Vec3 v2(mesh.vertices[i2].position);

                double t;
                Vec3 bary;
                if (IntersectRayTriangle(ray, v0, v1, v2, t, bary)) {
                    RaycastHit hit;
                    hit.hit           = true;
                    hit.distance      = t;
                    hit.point         = ray.origin + ray.direction * t;
                    hit.triangleIndex = (int)triIdx;
                    hit.normal        = MathUtils::SafeNormalize(glm::cross(v1 - v0, v2 - v0));

                    if (allHits) {
                        allHits->push_back(hit);
                        hitSomething = true;
                    } else if (t < closestT) {
                        closestT     = t;
                        outHit       = hit;
                        hitSomething = true;
                    }
                }
            }
        } else {
            double tLMin, tLMax, tRMin, tRMax;
            bool hitL = IntersectRayAABB(ray, mesh.bvhTree[node.left].bounds,  tLMin, tLMax);
            bool hitR = IntersectRayAABB(ray, mesh.bvhTree[node.right].bounds, tRMin, tRMax);

            if (hitL && hitR) {
                if (tLMin < tRMin) {
                    stack[stackPtr++] = {node.right, tRMin};
                    stack[stackPtr++] = {node.left,  tLMin};
                } else {
                    stack[stackPtr++] = {node.left,  tLMin};
                    stack[stackPtr++] = {node.right, tRMin};
                }
            } else if (hitL) {
                stack[stackPtr++] = {node.left, tLMin};
            } else if (hitR) {
                stack[stackPtr++] = {node.right, tRMin};
            }
        }
    }

    if (allHits && hitSomething) {
        std::sort(allHits->begin(), allHits->end(),
                  [](const RaycastHit& a, const RaycastHit& b) { return a.distance < b.distance; });
        outHit = (*allHits)[0];
    }
    return hitSomething;
}

bool MathUtils::IntersectRayTriangle(const Ray& ray, const Vec3& v0, const Vec3& v1, const Vec3& v2, double& outT, Vec3& outBarycentric) {
    constexpr double EPSILON = 0.0000001;
    Vec3   edge1 = v1 - v0;
    Vec3   edge2 = v2 - v0;
    Vec3   h     = glm::cross(ray.direction, edge2);
    double a     = glm::dot(edge1, h);

    if (a > -EPSILON && a < EPSILON) return false;

    double f = 1.0 / a;
    Vec3   s = ray.origin - v0;
    double u = f * glm::dot(s, h);
    if (u < 0.0 || u > 1.0) return false;

    Vec3   q = glm::cross(s, edge1);
    double v = f * glm::dot(ray.direction, q);
    if (v < 0.0 || u + v > 1.0) return false;

    double t = f * glm::dot(edge2, q);
    if (t > EPSILON) {
        outT           = t;
        outBarycentric = Vec3(1.0 - u - v, u, v);
        return true;
    }
    return false;
}

Ray MathUtils::ScreenToWorldRay(const glm::vec2& screenPos, const glm::vec2& viewportSize, const Mat4& viewMatrix, const Mat4& projMatrix) {
    double x = (2.0 * screenPos.x) / viewportSize.x - 1.0;
    double y = 1.0 - (2.0 * screenPos.y) / viewportSize.y;

    Vec4 ray_clip = Vec4(x, y, -1.0, 1.0);
    Vec4 ray_eye  = glm::inverse(projMatrix) * ray_clip;
    ray_eye       = Vec4(ray_eye.x, ray_eye.y, -1.0, 0.0);
    Vec3 ray_wor  = glm::inverse(viewMatrix) * ray_eye;

    return Ray(glm::inverse(viewMatrix)[3], ray_wor);
}

bool MathUtils::Calculate3PointCircle(const Vec3& p1, const Vec3& p2, const Vec3& p3, Vec3& outCenter, double& outRadius, Vec3& outNormal) {
    Vec3 v1 = p2 - p1;
    Vec3 v2 = p3 - p1;
    outNormal = glm::cross(v1, v2);

    double normalLength = glm::length(outNormal);
    if (normalLength < 1e-6) return false;

    outNormal = MathUtils::SafeNormalize(outNormal);

    Mat4 m;
    m[0] = Vec4(v1.x, v1.y, v1.z, 0.0);
    m[1] = Vec4(v2.x, v2.y, v2.z, 0.0);
    m[2] = Vec4(outNormal.x, outNormal.y, outNormal.z, 0.0);
    m[3] = Vec4(0.0, 0.0, 0.0, 1.0);

    Vec3 b;
    b.x = 0.5 * glm::dot(v1, v1);
    b.y = 0.5 * glm::dot(v2, v2);
    b.z = 0.0;

    Mat3 M = Mat3(m);
    Mat3 MT = glm::transpose(M);
    // Solve M^T * x = b using Normal Equations for maximum robustness: (M * M^T) * x = M * b
    Vec3 centerOffset = glm::inverse(M * MT) * (M * b);
    outCenter = p1 + centerOffset;
    outRadius = glm::length(centerOffset);
    return true;
}

double MathUtils::PointToRayDistance(const Vec3& p, const Vec3& rayOrigin, const Vec3& rayDir) {
    Vec3 v = p - rayOrigin;
    double projection = glm::dot(v, rayDir);
    if (projection < 0.0) return glm::length(v);
    return glm::length(v - rayDir * projection);
}

double MathUtils::SegmentToRayDistance(const Vec3& p0, const Vec3& p1, const Vec3& rayOrigin, const Vec3& rayDir, double& outT) {
    Vec3 u = p1 - p0; // segment direction
    Vec3 w = p0 - rayOrigin;
    double a = glm::dot(u, u);
    double b = glm::dot(u, rayDir);
    double c = glm::dot(rayDir, rayDir);
    double d = glm::dot(u, w);
    double e = glm::dot(rayDir, w);
    double denom = a * c - b * b;

    double sc, tc;

    if (denom < 1e-9) { // Parallel
        sc = 0.0;
        tc = (c > 1e-9) ? e / c : 0.0;
    } else {
        sc = (b * e - c * d) / denom;
        tc = (a * e - b * d) / denom;
    }

    sc = std::max(0.0, std::min(1.0, sc));
    outT = sc;
    
    Vec3 closestOnSegment = p0 + sc * u;
    // We want distance to INFINITE ray, so if tc < 0 we still use the ray formula
    Vec3 closestOnRay = rayOrigin + std::max(0.0, tc) * rayDir;
    
    return glm::distance(closestOnSegment, closestOnRay);
}
bool MathUtils::CalculateSphereFrom4Points(const Vec3& p1, const Vec3& p2, const Vec3& p3, const Vec3& p4,
                                       Vec3& outCenter, double& outRadius) {
    // Solve the system of linear equations for the sphere center
    // |x-x1|^2 + |y-y1|^2 + |z-z1|^2 = r^2
    // Subtracting equation 1 from 2, 3, and 4 gives 3 linear equations for (x, y, z)
    double x1 = p1.x, y1 = p1.y, z1 = p1.z;
    double x2 = p2.x, y2 = p2.y, z2 = p2.z;
    double x3 = p3.x, y3 = p3.y, z3 = p3.z;
    double x4 = p4.x, y4 = p4.y, z4 = p4.z;

    Mat3 A;
    A[0] = Vec3(2 * (x2 - x1), 2 * (y2 - y1), 2 * (z2 - z1));
    A[1] = Vec3(2 * (x3 - x1), 2 * (y3 - y1), 2 * (z3 - z1));
    A[2] = Vec3(2 * (x4 - x1), 2 * (y4 - y1), 2 * (z4 - z1));

    Vec3 b;
    b.x = x2 * x2 + y2 * y2 + z2 * z2 - (x1 * x1 + y1 * y1 + z1 * z1);
    b.y = x3 * x3 + y3 * y3 + z3 * z3 - (x1 * x1 + y1 * y1 + z1 * z1);
    b.z = x4 * x4 + y4 * y4 + z4 * z4 - (x1 * x1 + y1 * y1 + z1 * z1);

    if (std::abs(glm::determinant(A)) < 1e-9) return false;

    outCenter = glm::inverse(A) * b;
    outRadius = glm::distance(outCenter, p1);
    return true;
}

bool MathUtils::IntersectTwoRays(const Vec3& p1, const Vec3& d1, const Vec3& p2, const Vec3& d2,
                                Vec3& outCenter, double& outDistance) {
    // Shortest distance between two lines P1 + t1*D1 and P2 + t2*D2
    Vec3 w0 = p1 - p2;
    double a = glm::dot(d1, d1);
    double b = glm::dot(d1, d2);
    double c = glm::dot(d2, d2);
    double d = glm::dot(d1, w0);
    double e = glm::dot(d2, w0);
    double denom = a * c - b * b;

    if (std::abs(denom) < 1e-9) return false;

    double t1 = (b * e - c * d) / denom;
    double t2 = (a * e - b * d) / denom;

    Vec3 cp1 = p1 + t1 * d1;
    Vec3 cp2 = p2 + t2 * d2;
    outCenter = (cp1 + cp2) * 0.5;
    outDistance = glm::distance(cp1, cp2);
    return true;
}
