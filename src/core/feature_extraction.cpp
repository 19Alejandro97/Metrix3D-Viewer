#include "feature_extraction.hpp"
#include "math_utils.hpp"
#include "mesh.hpp"
#include "../ui/app_console.hpp"
#include <queue>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cmath>
#include <iostream>

// ---------------------------------------------------------------------------
// Plane Extraction via coplanar flood‑fill
// ---------------------------------------------------------------------------
DatumEntity FeatureExtraction::ExtractPlaneFromTriangle(const Mesh& mesh, int seedTriangleIndex, const std::string& objId, const Mat4& worldMatrix) {
    if (seedTriangleIndex < 0 || seedTriangleIndex >= (int)mesh.indices.size() / 3) return DatumEntity();

    // --- Optimization: Region Caching ---
    int clusterId = mesh.triangleClusterIds[seedTriangleIndex];
    if (clusterId != -1 && mesh.clusterToPrimitiveIdx.count(clusterId)) {
        return mesh.primitiveCache[mesh.clusterToPrimitiveIdx.at(clusterId)];
    }

    std::queue<uint32_t> q;
    std::set<uint32_t> visited;
    std::vector<uint32_t> planeTris;

    q.push(seedTriangleIndex);
    visited.insert(seedTriangleIndex);

    auto GetNormal = [&](uint32_t triIdx) {
        uint32_t v0 = mesh.indices[triIdx * 3 + 0];
        uint32_t v1 = mesh.indices[triIdx * 3 + 1];
        uint32_t v2 = mesh.indices[triIdx * 3 + 2];
        Vec3 p0(mesh.vertices[v0].position), p1(mesh.vertices[v1].position), p2(mesh.vertices[v2].position);
        return glm::normalize(glm::cross(p1 - p0, p2 - p0));
    };

    Vec3 seedNormal = GetNormal(seedTriangleIndex);
    Vec3 avgCenter(0.0);
    Vec3 avgNormal(0.0);

    int iterGuard = 0;
    while (!q.empty()) {
        if (++iterGuard > 10000) {
            std::cerr << "ExtractPlaneFromTriangle: Aborted due to >10,000 iterations (Non-Manifold geometry)." << std::endl;
            break;
        }
        uint32_t current = q.front();
        q.pop();

        Vec3 n = GetNormal(current);
        if (glm::dot(n, seedNormal) > 0.9995) {
            planeTris.push_back(current);
            Vec3 c(0.0);
            for (int i = 0; i < 3; ++i) c += Vec3(mesh.vertices[mesh.indices[current * 3 + i]].position);
            avgCenter += c / 3.0;
            avgNormal += n;

            for (uint32_t neighbor : mesh.triangleNeighbors[current]) {
                if (visited.find(neighbor) == visited.end()) {
                    if (glm::dot(GetNormal(neighbor), seedNormal) > 0.9995) {
                        visited.insert(neighbor);
                        q.push(neighbor);
                    }
                }
            }
            if (Mesh::m_shouldTerminate) break;
        }
        if (planeTris.size() > 5000) break;
    }

    if (planeTris.empty()) return DatumEntity();

    avgCenter /= (double)planeTris.size();
    avgNormal  = MathUtils::SafeNormalize(avgNormal);

    DatumEntity de;
    de.type    = DatumEntity::Type::Plane;
    de.isValid = true;
    de.point   = Vec3(worldMatrix * Vec4(avgCenter, 1.0));
    glm::mat3 normMat = glm::transpose(glm::inverse(glm::mat3(worldMatrix)));
    de.normal  = MathUtils::SafeNormalize(Vec3(normMat * avgNormal));
    float sX = glm::length(Vec3(worldMatrix[0]));
    float sY = glm::length(Vec3(worldMatrix[1]));
    float sZ = glm::length(Vec3(worldMatrix[2]));
    de.radius  = 20.0 * (double)((sX + sY + sZ) / 3.0f);
    de.objectId = objId;

    de.constituentIndices.reserve(planeTris.size() * 3);
    for (uint32_t triIdx : planeTris) {
        de.constituentIndices.push_back(mesh.indices[triIdx * 3 + 0]);
        de.constituentIndices.push_back(mesh.indices[triIdx * 3 + 1]);
        de.constituentIndices.push_back(mesh.indices[triIdx * 3 + 2]);
    }
    return de;
}

// ---------------------------------------------------------------------------
// Welzl Minimum Enclosing Circle (2D)
// ---------------------------------------------------------------------------
namespace {
    struct Circle2D { Vec2 center; double radius; };

    Circle2D CircleFrom3(Vec2 A, Vec2 B, Vec2 C) {
        double D = 2.0 * (A.x*(B.y-C.y) + B.x*(C.y-A.y) + C.x*(A.y-B.y));
        if (std::abs(D) < 1e-7) return {(A+B)*0.5, glm::length(A-B)*0.5};
        double cx = ((A.x*A.x + A.y*A.y)*(B.y-C.y) + (B.x*B.x + B.y*B.y)*(C.y-A.y) + (C.x*C.x + C.y*C.y)*(A.y-B.y)) / D;
        double cy = ((A.x*A.x + A.y*A.y)*(C.x-B.x) + (B.x*B.x + B.y*B.y)*(A.x-C.x) + (C.x*C.x + C.y*C.y)*(B.x-A.x)) / D;
        Vec2 c(cx, cy); return {c, glm::length(c-A)};
    }
    Circle2D CircleFrom2(Vec2 A, Vec2 B) { return {(A+B)*0.5, glm::length(A-B)*0.5}; }
    Circle2D MinCircleTrivial(std::vector<Vec2>& R) {
        if (R.empty()) return {Vec2(0.0), 0.0};
        if (R.size() == 1) return {R[0], 0};
        if (R.size() == 2) return CircleFrom2(R[0], R[1]);
        return CircleFrom3(R[0], R[1], R[2]);
    }
    Circle2D WelzlRecursive(std::vector<Vec2>& P, std::vector<Vec2> R, int n) {
        if (n == 0 || R.size() == 3) return MinCircleTrivial(R);
        Vec2 p = P[n - 1];
        Circle2D c = WelzlRecursive(P, R, n - 1);
        if (glm::length(p - c.center) <= c.radius + 1e-5) return c;
        R.push_back(p);
        return WelzlRecursive(P, R, n - 1);
    }
} // anonymous namespace

// ---------------------------------------------------------------------------
// Edge Loop Extraction via crease‑edge topology walk + Welzl MEC
// ---------------------------------------------------------------------------
DatumEntity FeatureExtraction::ExtractEdgeLoop(const Mesh& mesh, const Vec3& localSnapPoint, const std::string& objId, const Mat4& worldMatrix) {
    if (mesh.boundaryEdgeIndices.empty()) return DatumEntity();

    double minDist  = 1e9;
    uint32_t bestV0 = (uint32_t)-1, bestV1 = (uint32_t)-1;

    auto ClosestDistToSeg = [](const Vec3& pt, const Vec3& a, const Vec3& b) {
        Vec3 ab = b - a;
        double lensq = glm::dot(ab, ab);
        if (lensq < 1e-9) return glm::distance(pt, a);
        double t = glm::dot(pt - a, ab) / lensq;
        t = std::max(0.0, std::min(1.0, t));
        return glm::distance(pt, a + ab * t);
    };

    for (size_t i = 0; i < std::min(mesh.boundaryEdgeIndices.size(), (size_t)20000); i += 2) {
        uint32_t i0 = mesh.boundaryEdgeIndices[i], i1 = mesh.boundaryEdgeIndices[i + 1];
        Vec3 p0(mesh.vertices[i0].position), p1(mesh.vertices[i1].position);
        double d = ClosestDistToSeg(localSnapPoint, p0, p1);
        if (d < minDist) { minDist = d; bestV0 = i0; bestV1 = i1; }
    }

    if (minDist > 10.0) return DatumEntity();

    std::vector<uint32_t> loop = mesh.ExtractFeatureLoop(bestV0, bestV1);
    return ExtractCircleFromIndices(mesh, loop, objId, worldMatrix);
}

// ---------------------------------------------------------------------------
// Circle Fit from vertex index loop (PCA normal + Welzl MEC)
// ---------------------------------------------------------------------------
DatumEntity FeatureExtraction::ExtractCircleFromIndices(const Mesh& mesh, const std::vector<uint32_t>& loop, const std::string& objId, const Mat4& worldMatrix) {
    if (loop.size() < 3) return DatumEntity();

    AppConsole::Log("[Telemetry] Industrial Feature Extracted: Loop size " + std::to_string(loop.size()));

    std::vector<glm::dvec3> pts;
    glm::dvec3 centroid(0.0);
    for (uint32_t idx : loop) {
        if (idx >= mesh.vertices.size()) continue;
        glm::dvec3 p(mesh.vertices[idx].position);
        pts.push_back(p);
        centroid += p;
    }
    if (pts.size() < 3) return DatumEntity();
    centroid /= (double)pts.size();

    glm::dvec3 normal(0.0);
    for (size_t i = 0; i < pts.size(); ++i) {
        glm::dvec3 a = pts[i] - centroid;
        glm::dvec3 b = pts[(i+1) % pts.size()] - centroid;
        normal += glm::cross(a, b);
    }
    double nLen = glm::length(normal);
    if (nLen < 1e-12) return DatumEntity();
    normal /= nLen;

    glm::dvec3 tangent = MathUtils::SafeNormalize(glm::cross(normal, glm::dvec3(1, 0, 0)));
    if (glm::length(tangent) < 0.1) tangent = MathUtils::SafeNormalize(glm::cross(normal, glm::dvec3(0, 1, 0)));
    glm::dvec3 bitangent = glm::cross(normal, tangent);

    std::vector<glm::dvec2> pts2d;
    for (const auto& p : pts) {
        glm::dvec3 rel = p - centroid;
        pts2d.push_back(glm::dvec2(glm::dot(rel, tangent), glm::dot(rel, bitangent)));
    }

    glm::dvec2 c2d(0.0);
    for (const auto& p : pts2d) c2d += p;
    c2d /= (double)pts2d.size();

    double radius = 0.0;
    for (const auto& p : pts2d) radius += glm::distance(p, c2d);
    radius /= (double)pts2d.size();

    Vec3 finalCenterLocal = Vec3(centroid + tangent * c2d.x + bitangent * c2d.y);

    DatumEntity de;
    de.isValid = true;
    de.type    = DatumEntity::Type::Circle;
    de.point   = Vec3(worldMatrix * Vec4(finalCenterLocal, 1.0));
    glm::mat3 normMat = glm::transpose(glm::inverse(glm::mat3(worldMatrix)));
    de.normal  = MathUtils::SafeNormalize(Vec3(normMat * Vec3(normal)));
    float scaleX = glm::length(Vec3(worldMatrix[0]));
    float scaleY = glm::length(Vec3(worldMatrix[1]));
    float scaleZ = glm::length(Vec3(worldMatrix[2]));
    de.radius = radius * (double)((scaleX + scaleY + scaleZ) / 3.0f);
    de.importance = (float)loop.size();
    de.objectId   = objId;

    for (size_t i = 0; i < loop.size(); ++i) {
        de.constituentIndices.push_back(loop[i]);
        de.constituentIndices.push_back(loop[(i + 1) % loop.size()]);
    }
    return de;
}

// ---------------------------------------------------------------------------
// Circle Extraction from seed triangle (flood‑fill + boundary loop walk + Welzl)
// ---------------------------------------------------------------------------
DatumEntity FeatureExtraction::ExtractCircleFromTriangle(const Mesh& mesh, int seedTriangleIndex, const std::string& objId, const Mat4& worldMatrix) {
    if (seedTriangleIndex < 0 || seedTriangleIndex >= (int)mesh.indices.size() / 3) return DatumEntity();

    // --- Optimization: Region Caching ---
    int clusterId = mesh.triangleClusterIds[seedTriangleIndex];
    if (clusterId != -1 && mesh.clusterToPrimitiveIdx.count(clusterId)) {
        return mesh.primitiveCache[mesh.clusterToPrimitiveIdx.at(clusterId)];
    }

    std::queue<uint32_t> q;
    std::unordered_set<uint32_t> visited;
    std::vector<uint32_t> component;

    auto GetNormal = [&](uint32_t t) {
        Vec3 p0(mesh.vertices[mesh.indices[t*3]].position);
        Vec3 p1(mesh.vertices[mesh.indices[t*3+1]].position);
        Vec3 p2(mesh.vertices[mesh.indices[t*3+2]].position);
        return glm::normalize(glm::cross(p1-p0, p2-p0));
    };

    Vec3 seedNormal = GetNormal(seedTriangleIndex);
    q.push(seedTriangleIndex);
    visited.insert(seedTriangleIndex);

    int iterGuard = 0;
    while (!q.empty()) {
        if (++iterGuard > 10000) {
            std::cerr << "ExtractCircleFromTriangle: Aborted due to >10000 iterations." << std::endl;
            break;
        }
        uint32_t curr = q.front(); q.pop();
        component.push_back(curr);
        Vec3 currN = GetNormal(curr);
        for (uint32_t n : mesh.triangleNeighbors[curr]) {
            if (visited.find(n) == visited.end()) {
                if (glm::dot(GetNormal(n), currN) > 0.85) {
                    visited.insert(n); q.push(n);
                }
            }
            if (Mesh::m_shouldTerminate) break;
        }
        if (component.size() > 5000) break;
    }

    std::unordered_map<uint64_t, int> edgeCounts;
    for (uint32_t t : component) {
        uint32_t v[3] = {mesh.indices[t*3], mesh.indices[t*3+1], mesh.indices[t*3+2]};
        for (int i = 0; i < 3; ++i) {
            uint32_t a = v[i], b = v[(i+1)%3];
            uint64_t key = (a < b) ? (((uint64_t)a << 32) | b) : (((uint64_t)b << 32) | a);
            edgeCounts[key]++;
        }
    }

    std::vector<uint32_t> boundary;
    for (auto& pair : edgeCounts) {
        if (pair.second == 1) {
            boundary.push_back((uint32_t)(pair.first >> 32));
            boundary.push_back((uint32_t)(pair.first & 0xFFFFFFFF));
        }
    }
    if (boundary.empty()) return DatumEntity();

    std::unordered_map<uint32_t, std::vector<uint32_t>> locAdj;
    for (size_t i = 0; i < boundary.size(); i += 2) {
        locAdj[boundary[i]].push_back(boundary[i+1]);
        locAdj[boundary[i+1]].push_back(boundary[i]);
    }

    Vec3 seedCentroid(0);
    {
        uint32_t sv[3] = {mesh.indices[seedTriangleIndex*3], mesh.indices[seedTriangleIndex*3+1], mesh.indices[seedTriangleIndex*3+2]};
        seedCentroid = (Vec3(mesh.vertices[sv[0]].position) + Vec3(mesh.vertices[sv[1]].position) + Vec3(mesh.vertices[sv[2]].position)) / 3.0;
    }

    std::vector<uint32_t> bestLoop;
    double bestLoopDist = 1e9;
    std::unordered_set<uint32_t> globallyVisited;

    for (size_t i = 0; i < boundary.size(); i += 2) {
        uint32_t startNode = boundary[i];
        if (globallyVisited.count(startNode)) continue;

        std::vector<uint32_t> currentLoop;
        std::unordered_set<uint32_t> loopVisited;
        uint32_t curr = startNode;
        currentLoop.push_back(curr);
        loopVisited.insert(curr);
        globallyVisited.insert(curr);

        bool closed = false;
        for (int iter = 0; iter < 5000; ++iter) {
            uint32_t lNext = (uint32_t)-1;
            for (uint32_t n : locAdj[curr]) {
                if (n == startNode && currentLoop.size() > 2) { closed = true; break; }
                if (loopVisited.find(n) == loopVisited.end()) { lNext = n; break; }
            }
            if (closed || lNext == (uint32_t)-1) break;
            currentLoop.push_back(lNext);
            loopVisited.insert(lNext);
            globallyVisited.insert(lNext);
            curr = lNext;
        }

        if (closed && currentLoop.size() >= 3) {
            Vec3 loopCentroid(0);
            for (uint32_t v : currentLoop) loopCentroid += Vec3(mesh.vertices[v].position);
            loopCentroid /= (double)currentLoop.size();
            double d = glm::distance(seedCentroid, loopCentroid);
            if (d < bestLoopDist) { bestLoopDist = d; bestLoop = currentLoop; }
        }
    }

    if (bestLoop.empty()) return DatumEntity();

    std::vector<Vec3> pts;
    Vec3 centroid(0);
    for (uint32_t v : bestLoop) { centroid += Vec3(mesh.vertices[v].position); pts.push_back(Vec3(mesh.vertices[v].position)); }
    centroid /= (double)pts.size();

    Vec3 normal(0);
    for (size_t i = 0; i < pts.size(); ++i) normal += glm::cross(pts[i]-centroid, pts[(i+1)%pts.size()]-centroid);
    
    // --- Phase 29: Anti-Radio Fallback (Integrated with SnapEngine Refactor) ---
    Vec3 planeNormal = MathUtils::SafeNormalize(normal, Vec3(0, 0, 0));
    if (glm::length(planeNormal) < 0.1 && pts.size() >= 3) {
        Vec3 p1 = pts[0], p2 = pts[pts.size()/3], p3 = pts[2*pts.size()/3];
        Vec3 c, n; double r;
        if (MathUtils::Calculate3PointCircle(p1, p2, p3, c, r, n)) {
            planeNormal = n;
        }
    }
    normal = MathUtils::SafeNormalize(planeNormal, seedNormal);

    Vec3 uVec = MathUtils::SafeNormalize(pts[0] - centroid);
    Vec3 vVec = MathUtils::SafeNormalize(glm::cross(normal, uVec));
    std::vector<Vec2> pts2d;
    for (auto& p : pts) pts2d.push_back({glm::dot(p-centroid, uVec), glm::dot(p-centroid, vVec)});

    Circle2D mec  = WelzlRecursive(pts2d, {}, (int)pts2d.size());
    Vec3 mec3D    = centroid + uVec * static_cast<double>(mec.center.x) + vVec * static_cast<double>(mec.center.y);

    DatumEntity de;
    de.type    = DatumEntity::Type::Circle;
    de.isValid = true;
    de.point   = Vec3(worldMatrix * Vec4(mec3D, 1.0));
    glm::mat3 normMat = glm::transpose(glm::inverse(glm::mat3(worldMatrix)));
    de.normal  = MathUtils::SafeNormalize(Vec3(normMat * normal));
    float sX = glm::length(Vec3(worldMatrix[0]));
    float sY = glm::length(Vec3(worldMatrix[1]));
    float sZ = glm::length(Vec3(worldMatrix[2]));
    float fScale = (sX + sY + sZ) / 3.0f;
    de.radius  = mec.radius * (double)fScale;

    AppConsole::Log("[Metrology] MEC Local Radius: " + std::to_string(mec.radius) + " | World Scale: " + std::to_string(fScale));
    de.objectId = objId;

    for (size_t i = 0; i < bestLoop.size(); ++i) {
        de.constituentIndices.push_back(bestLoop[i]);
        de.constituentIndices.push_back(bestLoop[(i + 1) % bestLoop.size()]);
    }

    de.secondaryIndices.reserve(component.size() * 3 + bestLoop.size() * 3);
    for (uint32_t triIdx : component) {
        de.secondaryIndices.push_back(mesh.indices[triIdx * 3 + 0]);
        de.secondaryIndices.push_back(mesh.indices[triIdx * 3 + 1]);
        de.secondaryIndices.push_back(mesh.indices[triIdx * 3 + 2]);
    }

    if (bestLoop.size() >= 3) {
        uint32_t v0 = bestLoop[0];
        for (size_t i = 1; i < bestLoop.size() - 1; ++i) {
            de.secondaryIndices.push_back(v0);
            de.secondaryIndices.push_back(bestLoop[i]);
            de.secondaryIndices.push_back(bestLoop[i + 1]);
        }
    }

    return de;
}
