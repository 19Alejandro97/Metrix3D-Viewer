#include "adjacency_builder.hpp"
#include "../mesh.hpp"
#include <unordered_map>
#include <map>
#include <algorithm>
#include <cmath>

void AdjacencyBuilder::Build(Mesh& mesh) {
    mesh.triangleNeighbors.clear();
    mesh.triangleNeighbors.resize(mesh.indices.size() / 3);
    mesh.vertexAdjacency.clear();
    mesh.vertexAdjacency.resize(mesh.vertices.size());

    // Infrastructure: Epsilon Merging (0.001mm)
    struct QuantizedVec {
        int64_t x, y, z;
        bool operator==(const QuantizedVec& o) const { return x == o.x && y == o.y && z == o.z; }
    };
    auto hashQuant = [](const QuantizedVec& v) -> size_t {
        return std::hash<int64_t>{}(v.x) ^ (std::hash<int64_t>{}(v.y) << 1) ^ (std::hash<int64_t>{}(v.z) << 2);
    };
    
    // Memory Optimization: posToUnifiedIdx is local to this scope
    {
        std::unordered_map<QuantizedVec, uint32_t, decltype(hashQuant)> posToUnifiedIdx(mesh.vertices.size(), hashQuant);
        std::vector<uint32_t> unifiedIndices(mesh.vertices.size());

        double eps = 0.001; // 1 micron tolerance
        for (uint32_t i = 0; i < (uint32_t)mesh.vertices.size(); ++i) {
            Vec3 pos = Vec3(mesh.vertices[i].position);
            QuantizedVec qv = {
                (int64_t)std::round(pos.x / eps),
                (int64_t)std::round(pos.y / eps),
                (int64_t)std::round(pos.z / eps)
            };
            auto it = posToUnifiedIdx.find(qv);
            if (it == posToUnifiedIdx.end()) {
                posToUnifiedIdx[qv] = i;
                unifiedIndices[i] = i;
            } else {
                unifiedIndices[i] = it->second;
            }
        }

        // Map: Unified Edge {v1, v2} -> list of triangle indices sharing it
        struct Edge {
            uint32_t a, b;
            bool operator<(const Edge& o) const { return a != o.a ? a < o.a : b < o.b; }
        };
        std::map<Edge, std::vector<uint32_t>> edgeToTriangles;

        for (uint32_t i = 0; i < (uint32_t)mesh.indices.size() / 3; ++i) {
            uint32_t v0_raw = mesh.indices[i * 3 + 0];
            uint32_t v1_raw = mesh.indices[i * 3 + 1];
            uint32_t v2_raw = mesh.indices[i * 3 + 2];
            
            uint32_t v0 = unifiedIndices[v0_raw];
            uint32_t v1 = unifiedIndices[v1_raw];
            uint32_t v2 = unifiedIndices[v2_raw];

            // V2F Adjacency uses RAW indices because mesh.vertices is not unified
            mesh.vertexAdjacency[v0_raw].push_back(i);
            mesh.vertexAdjacency[v1_raw].push_back(i);
            mesh.vertexAdjacency[v2_raw].push_back(i);

            auto AddEdge = [&](uint32_t a, uint32_t b) {
                Edge e = { std::min(a, b), std::max(a, b) };
                edgeToTriangles[e].push_back(i);
            };

            AddEdge(v0, v1);
            AddEdge(v1, v2);
            AddEdge(v2, v0);
        }

        mesh.boundaryEdgeIndices.clear();
        mesh.featureEdgeIndices.clear();
        mesh.m_vertexToEdges.clear();
        mesh.m_vertexToEdges.resize(mesh.vertices.size());

        auto GetFaceNormal = [&](uint32_t t) {
            Vec3 p0 = Vec3(mesh.vertices[mesh.indices[t * 3 + 0]].position);
            Vec3 p1 = Vec3(mesh.vertices[mesh.indices[t * 3 + 1]].position);
            Vec3 p2 = Vec3(mesh.vertices[mesh.indices[t * 3 + 2]].position);
            Vec3 cross = glm::cross(p1 - p0, p2 - p0);
            double len = glm::length(cross);
            if (len < 1e-12) return Vec3(0, 1, 0);
            return cross / len;
        };

        float creaseThreshold = (float)glm::cos(glm::radians(30.0f));

        for (auto it = edgeToTriangles.begin(); it != edgeToTriangles.end(); ++it) {
            const auto& edge = it->first;
            const auto& tris = it->second;
            if (tris.size() > 1) {
                bool isFeature = false;
                for (size_t i = 0; i < tris.size(); ++i) {
                    for (size_t j = i + 1; j < tris.size(); ++j) {
                        mesh.triangleNeighbors[tris[i]].push_back(tris[j]);
                        mesh.triangleNeighbors[tris[j]].push_back(tris[i]);

                        Vec3 n1 = GetFaceNormal(tris[i]);
                        Vec3 n2 = GetFaceNormal(tris[j]);
                        float dot = (float)glm::clamp(glm::dot(n1, n2), -1.0, 1.0);
                        if (dot < creaseThreshold) isFeature = true;
                    }
                }
                if (isFeature) {
                    mesh.featureEdgeIndices.push_back(edge.a);
                    mesh.featureEdgeIndices.push_back(edge.b);
                    mesh.m_vertexToEdges[edge.a].push_back(edge.b);
                    mesh.m_vertexToEdges[edge.b].push_back(edge.a);
                }
            } else if (tris.size() == 1) {
                mesh.boundaryEdgeIndices.push_back(edge.a);
                mesh.boundaryEdgeIndices.push_back(edge.b);
                mesh.m_vertexToEdges[edge.a].push_back(edge.b);
                mesh.m_vertexToEdges[edge.b].push_back(edge.a);
            }
        }
    }
}
