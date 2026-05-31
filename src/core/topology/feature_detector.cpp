#include "feature_detector.hpp"
#include "../mesh.hpp"
#include <glad/glad.h>
#include <algorithm>
#include <map>
#include <unordered_set>

void FeatureDetector::GenerateEdges(Mesh& mesh, float angleThresholdDegrees) {
    mesh.edgeIndices.clear();
    
    std::vector<glm::vec3> faceNormals;
    int numTriangles = mesh.GetTriangleCount();
    faceNormals.reserve(numTriangles);
    
    for (int i = 0; i < numTriangles; ++i) {
        uint32_t i0 = mesh.indices[i * 3 + 0];
        uint32_t i1 = mesh.indices[i * 3 + 1];
        uint32_t i2 = mesh.indices[i * 3 + 2];
        
        glm::vec3 v0 = mesh.vertices[i0].position;
        glm::vec3 v1 = mesh.vertices[i1].position;
        glm::vec3 v2 = mesh.vertices[i2].position;
        
        glm::vec3 n = glm::normalize(glm::cross(v1 - v0, v2 - v0));
        faceNormals.push_back(n);
    }
    
    struct QuantizedVec { int64_t x, y, z; 
        bool operator<(const QuantizedVec& o) const { 
            if(x != o.x) return x < o.x; 
            if(y != o.y) return y < o.y; 
            return z < o.z; 
        }
    };
    double eps = 0.001; // 1 micron
    auto Quantize = [&](const glm::vec3& v) { 
        return QuantizedVec{(int64_t)std::round(v.x/eps), (int64_t)std::round(v.y/eps), (int64_t)std::round(v.z/eps)}; 
    };

    std::map<QuantizedVec, uint32_t> posToMasterIndex;
    std::vector<uint32_t> masterIndices(mesh.vertices.size());
    for (uint32_t i = 0; i < (uint32_t)mesh.vertices.size(); ++i) {
        QuantizedVec p = Quantize(mesh.vertices[i].position);
        auto it = posToMasterIndex.find(p);
        if (it == posToMasterIndex.end()) {
            posToMasterIndex[p] = i;
            masterIndices[i]   = i;
        } else {
            masterIndices[i] = it->second;
        }
    }

    std::map<std::pair<uint32_t, uint32_t>, std::vector<int>> edgeToTris;
    for (int i = 0; i < numTriangles; ++i) {
        for (int j = 0; j < 3; ++j) {
            uint32_t vA = masterIndices[mesh.indices[i * 3 + j]];
            uint32_t vB = masterIndices[mesh.indices[i * 3 + (j + 1) % 3]];
            if (vA == vB) continue;
            auto edge = std::make_pair(std::min(vA, vB), std::max(vA, vB));
            edgeToTris[edge].push_back(i);
        }
    }
    
    float cosThreshold = std::cos(glm::radians(angleThresholdDegrees));
    for (const auto& pair : edgeToTris) {
        const auto& tris = pair.second;
        bool isHardEdge = false;
        
        if (tris.size() == 1) {
            isHardEdge = true;
        } else if (tris.size() >= 2) {
            glm::vec3 n1 = faceNormals[tris[0]];
            glm::vec3 n2 = faceNormals[tris[1]];
            float dotP = glm::clamp(glm::dot(n1, n2), -1.0f, 1.0f);
            if (dotP < cosThreshold) isHardEdge = true;
        }
        
        if (isHardEdge) {
            mesh.edgeIndices.push_back(pair.first.first);
            mesh.edgeIndices.push_back(pair.first.second);
        }
    }
    
    if (mesh.m_gpu.edgeVao != 0) {
        glDeleteVertexArrays(1, &mesh.m_gpu.edgeVao);
        glDeleteBuffers(1, &mesh.m_gpu.edgeEbo);
        mesh.m_gpu.edgeVao = 0;
        mesh.m_gpu.edgeEbo = 0;
    }
    
    if (!mesh.edgeIndices.empty() && mesh.m_gpu.vbo != 0) {
        glGenVertexArrays(1, &mesh.m_gpu.edgeVao);
        glGenBuffers(1, &mesh.m_gpu.edgeEbo);
        glBindVertexArray(mesh.m_gpu.edgeVao);
        glBindBuffer(GL_ARRAY_BUFFER, mesh.m_gpu.vbo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.m_gpu.edgeEbo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.edgeIndices.size() * sizeof(uint32_t), mesh.edgeIndices.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
        glBindVertexArray(0);
    }
}

std::vector<uint32_t> FeatureDetector::ExtractFeatureLoop(const Mesh& mesh, uint32_t v0, uint32_t v1) {
    std::vector<uint32_t> loop;
    if (v0 >= mesh.vertices.size() || v1 >= mesh.vertices.size()) return loop;

    std::unordered_set<uint32_t> visited;
    std::vector<uint32_t> forward;
    std::vector<uint32_t> backward;

    auto Traverse = [&](uint32_t current, uint32_t prev, std::vector<uint32_t>& path) {
        for (int iter = 0; iter < 2500; ++iter) {
            path.push_back(current);
            visited.insert(current);
            const auto& neighbors = mesh.m_vertexToEdges[current];
            uint32_t next = -1;
            if (neighbors.size() == 2) {
                next = (neighbors[0] == prev) ? neighbors[1] : neighbors[0];
            } else if (neighbors.size() > 2) {
                glm::vec3 curPos = mesh.vertices[current].position;
                glm::vec3 prevPos = mesh.vertices[prev].position;
                glm::vec3 entryDir = glm::normalize(curPos - prevPos);
                float bestDot = -2.0f;
                for (uint32_t n : neighbors) {
                    if (n == prev) continue;
                    glm::vec3 nPos = mesh.vertices[n].position;
                    glm::vec3 exitDir = glm::normalize(nPos - curPos);
                    float d = glm::dot(entryDir, exitDir);
                    if (d > bestDot) { bestDot = d; next = n; }
                }
                if (bestDot < 0.707f) next = -1;
            }
            if (next == -1 || visited.count(next)) break;
            prev = current;
            current = next;
        }
    };

    forward.push_back(v0);
    visited.insert(v0);
    Traverse(v1, v0, forward);

    uint32_t lastVForward = forward.back();
    bool closed = false;
    const auto& lastNeighbors = mesh.m_vertexToEdges[lastVForward];
    for(uint32_t n : lastNeighbors) if(n == v0 && forward.size() > 2) closed = true;

    if (!closed) {
        uint32_t startNode = v0;
        const auto& neighbors = mesh.m_vertexToEdges[startNode];
        uint32_t otherNeighbor = -1;
        for (uint32_t n : neighbors) if (n != v1) { otherNeighbor = n; break; }
        if (otherNeighbor != -1) Traverse(otherNeighbor, v0, backward);
    }

    std::reverse(backward.begin(), backward.end());
    loop.insert(loop.end(), backward.begin(), backward.end());
    loop.insert(loop.end(), forward.begin(), forward.end());
    return loop;
}
