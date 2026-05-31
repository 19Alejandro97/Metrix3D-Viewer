#include "primitive_analyzer.hpp"
#include "../mesh.hpp"
#include "../math_utils.hpp"
#include "../../ui/app_console.hpp"
#include <unordered_set>
#include <queue>
#include <chrono>

void PrimitiveAnalyzer::Analyze(Mesh& mesh, float timeLimitMs) {
    AppConsole::Log("[Intelligence] Starting Volumetric Analysis (Modular) for " + mesh.name);
    auto startTime = std::chrono::steady_clock::now();
    
    mesh.primitiveCache.clear();
    mesh.triangleClusterIds.assign(mesh.GetTriangleCount(), -1);
    mesh.clusterToPrimitiveIdx.clear();

    {
        std::lock_guard<std::mutex> lock(mesh.m_primitiveMutex);
        mesh.m_isAnalysisReady = false;
    }

    std::unordered_set<uint32_t> visited;
    int triCount = mesh.GetTriangleCount();

    // 3. Inteligencia Adaptativa: Scaled thresholds based on mesh density
    float dotThreshold = (mesh.triangleDensity > 5e-5f) ? 0.90f : 0.98f;
    AppConsole::Log("[Intelligence] Adaptive Threshold: " + std::to_string(std::acos(dotThreshold) * 180.0/3.14159) + " deg");

    auto GetNormal = [&](uint32_t t) {
        Vec3 p0 = Vec3(mesh.vertices[mesh.indices[t*3]].position);
        Vec3 p1 = Vec3(mesh.vertices[mesh.indices[t*3+1]].position);
        Vec3 p2 = Vec3(mesh.vertices[mesh.indices[t*3+2]].position);
        return glm::normalize(glm::cross(p1-p0, p2-p0));
    };
    auto GetCentroid = [&](uint32_t t) {
        return (Vec3(mesh.vertices[mesh.indices[t*3]].position) + 
                Vec3(mesh.vertices[mesh.indices[t*3+1]].position) + 
                Vec3(mesh.vertices[mesh.indices[t*3+2]].position)) / 3.0;
    };

    for (int seed = 0; seed < triCount; ++seed) {
        if (visited.find(seed) != visited.end()) continue;
        if (timeLimitMs > 0) {
            auto now = std::chrono::steady_clock::now();
            if (std::chrono::duration<float, std::milli>(now - startTime).count() > timeLimitMs) break;
        }
        std::vector<uint32_t> cluster;
        std::queue<uint32_t> q;
        q.push(seed); visited.insert(seed);
        Vec3 seedNormal = GetNormal(seed);
        while(!q.empty()) {
            if (Mesh::m_shouldTerminate) return;
            uint32_t curr = q.front(); q.pop();
            cluster.push_back(curr);
            for (uint32_t n : mesh.triangleNeighbors[curr]) {
                if (visited.find(n) == visited.end()) {
                    if (glm::dot(GetNormal(n), seedNormal) > dotThreshold) {
                        visited.insert(n); q.push(n);
                    }
                }
            }
        }
        if (cluster.size() < 12) continue;
        Vec3 avgNormal(0);
        for(uint32_t t : cluster) avgNormal += GetNormal(t);
        avgNormal = MathUtils::SafeNormalize(avgNormal);
        float normalVariance = 0.0f;
        for(uint32_t t : cluster) normalVariance += (float)(1.0 - glm::dot(GetNormal(t), avgNormal));
        normalVariance /= (float)cluster.size();
        DatumEntity primitive;
        primitive.objectId = mesh.name; primitive.isValid = true;
        if (normalVariance < 0.01f) {
            primitive.type = DatumEntity::Type::Plane;
            Vec3 center(0);
            for(uint32_t t : cluster) center += GetCentroid(t);
            primitive.point = center / (double)cluster.size();
            primitive.normal = avgNormal;
        } else {
            std::vector<Vec3> axes;
            for (size_t i = 0; i < cluster.size(); i += cluster.size() / 5) {
                for (size_t j = i + 1; j < cluster.size(); j += cluster.size() / 5) {
                    Vec3 c1 = GetCentroid(cluster[i]), n1 = GetNormal(cluster[i]);
                    Vec3 c2 = GetCentroid(cluster[j]), n2 = GetNormal(cluster[j]);
                    Vec3 bpt; double bdist;
                    if (MathUtils::IntersectTwoRays(c1, -n1, c2, -n2, bpt, bdist)) axes.push_back(bpt);
                }
            }
            if (!axes.empty()) {
                Vec3 axisCenter(0);
                for(auto& a : axes) axisCenter += a;
                primitive.point = axisCenter / (double)axes.size();
                primitive.type = DatumEntity::Type::Cylinder;
                primitive.normal = avgNormal;
                double avgRad = 0;
                for(uint32_t t : cluster) avgRad += glm::distance(GetCentroid(t), primitive.point);
                primitive.radius = avgRad / (double)cluster.size();
            } else continue;
        }
        int clusterId = (int)mesh.primitiveCache.size();
        for(uint32_t t : cluster) {
            mesh.triangleClusterIds[t] = clusterId;
            primitive.constituentIndices.push_back(mesh.indices[t*3]);
            primitive.constituentIndices.push_back(mesh.indices[t*3+1]);
            primitive.constituentIndices.push_back(mesh.indices[t*3+2]);
        }
        mesh.primitiveCache.push_back(primitive);
        mesh.clusterToPrimitiveIdx[clusterId] = clusterId;
    }

    // Phase 2: RANSAC Spheres
    std::vector<uint32_t> unassigned;
    for(int i=0; i<triCount; ++i) if(mesh.triangleClusterIds[i] == -1) unassigned.push_back(i);
    if (unassigned.size() > 50) {
        int iterations = std::min(500, (int)unassigned.size() / 10);
        for (int i = 0; i < iterations; ++i) {
            if (Mesh::m_shouldTerminate) break;
            uint32_t ts[4]; for(int j=0; j<4; ++j) ts[j] = unassigned[rand() % unassigned.size()];
            Vec3 p[4]; for(int j=0; j<4; ++j) p[j] = GetCentroid(ts[j]);
            Vec3 center; double radius;
            if (MathUtils::CalculateSphereFrom4Points(p[0], p[1], p[2], p[3], center, radius)) {
                if (radius < 0.1 || radius > 500.0) continue;
                std::vector<uint32_t> inliers;
                for (uint32_t ut : unassigned) if (std::abs(glm::distance(GetCentroid(ut), center) - radius) < radius * 0.05) inliers.push_back(ut);
                if (inliers.size() > 30) {
                    DatumEntity sphere; sphere.type = DatumEntity::Type::Sphere; sphere.isValid = true;
                    sphere.point = center; sphere.radius = radius; sphere.objectId = mesh.name;
                    int clusterId = (int)mesh.primitiveCache.size();
                    for(uint32_t it : inliers) {
                        mesh.triangleClusterIds[it] = clusterId;
                        sphere.constituentIndices.push_back(mesh.indices[it*3]);
                        sphere.constituentIndices.push_back(mesh.indices[it*3+1]);
                        sphere.constituentIndices.push_back(mesh.indices[it*3+2]);
                    }
                    mesh.primitiveCache.push_back(sphere);
                }
            }
        }
    }
    mesh.m_isAnalysisReady = true;
    auto endTime = std::chrono::steady_clock::now();
    AppConsole::Log("[Intelligence] Volumetric Analysis complete in " + 
                    std::to_string((int)std::chrono::duration<float, std::milli>(endTime - startTime).count()) + "ms.");
}
