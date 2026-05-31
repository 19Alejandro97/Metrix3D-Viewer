#include "mesh_processor.hpp"
#include "../mesh.hpp"
#include "adjacency_builder.hpp"
#include "feature_detector.hpp"
#include <vector>
#include <map>
#include <algorithm>
#include <glm/glm.hpp>

void MeshProcessor::SmoothMesh(Mesh& mesh, int iterations, float lambda) {
    if (mesh.vertices.empty() || mesh.indices.empty()) return;
    
    // 1. Build Coordinate-Level Adjacency (to handle unmerged STLs)
    struct QuantizedVec { int64_t x, y, z; 
        bool operator<(const QuantizedVec& o) const { if(x!=o.x) return x<o.x; if(y!=o.y) return y<o.y; return z<o.z; }
    };
    double eps = 0.001;
    auto Quantize = [&](const Vec3f& v) { return QuantizedVec{(int64_t)std::round(v.x/eps), (int64_t)std::round(v.y/eps), (int64_t)std::round(v.z/eps)}; };

    std::map<QuantizedVec, uint32_t> posToMaster;
    std::vector<uint32_t> masterIndices(mesh.vertices.size());
    std::vector<QuantizedVec> masterCoords;

    for (uint32_t i = 0; i < (uint32_t)mesh.vertices.size(); ++i) {
        auto q = Quantize(mesh.vertices[i].position);
        auto it = posToMaster.find(q);
        if (it == posToMaster.end()) {
            uint32_t mIdx = (uint32_t)masterCoords.size();
            posToMaster[q] = mIdx;
            masterIndices[i] = mIdx;
            masterCoords.push_back(q);
        } else {
            masterIndices[i] = it->second;
        }
    }

    // 2. Build adjacency on MASTER indices
    std::vector<std::vector<uint32_t>> masterAdj(masterCoords.size());
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        uint32_t m0 = masterIndices[mesh.indices[i+0]];
        uint32_t m1 = masterIndices[mesh.indices[i+1]];
        uint32_t m2 = masterIndices[mesh.indices[i+2]];
        auto AddEdge = [&](uint32_t a, uint32_t b) {
            if (std::find(masterAdj[a].begin(), masterAdj[a].end(), b) == masterAdj[a].end()) masterAdj[a].push_back(b);
        };
        AddEdge(m0, m1); AddEdge(m0, m2); AddEdge(m1, m0); AddEdge(m1, m2); AddEdge(m2, m0); AddEdge(m2, m1);
    }
    
    // 3. Smooth MASTER positions
    std::vector<glm::vec3> masterPositions(masterCoords.size());
    for(size_t i=0; i<mesh.vertices.size(); ++i) masterPositions[masterIndices[i]] = mesh.vertices[i].position;

    std::vector<glm::vec3> nextMasterPos(masterCoords.size());
    for (int iter = 0; iter < iterations; ++iter) {
        for (size_t i = 0; i < masterPositions.size(); ++i) {
            const auto& neighbors = masterAdj[i];
            if (neighbors.empty()) { nextMasterPos[i] = masterPositions[i]; continue; }
            glm::vec3 centroid(0.0f);
            for (uint32_t nIdx : neighbors) centroid += masterPositions[nIdx];
            centroid /= (float)neighbors.size();
            nextMasterPos[i] = glm::mix(masterPositions[i], centroid, lambda);
        }
        masterPositions = nextMasterPos;
    }
    
    // 4. Map back to RAW vertices
    for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        mesh.vertices[i].position = masterPositions[masterIndices[i]];
    }
    
    mesh.RecalculateBounds();
    mesh.RecalculateNormals();
    mesh.UploadToGPU();
    FeatureDetector::GenerateEdges(mesh, 35.0f);
}

int MeshProcessor::FillHoles(Mesh& mesh) {
    if (mesh.vertices.empty() || mesh.indices.empty()) return 0;
    
    struct DirectedEdge {
        uint32_t from, to;
        bool operator<(const DirectedEdge& other) const {
            if (from != other.from) return from < other.from;
            return to < other.to;
        }
    };
    
    std::map<DirectedEdge, int> edgeCount;
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        edgeCount[{mesh.indices[i+0], mesh.indices[i+1]}]++;
        edgeCount[{mesh.indices[i+1], mesh.indices[i+2]}]++;
        edgeCount[{mesh.indices[i+2], mesh.indices[i+0]}]++;
    }
    
    std::map<uint32_t, uint32_t> nextNodeMap; 
    for (auto it = edgeCount.begin(); it != edgeCount.end(); ++it) {
        const DirectedEdge& edge = it->first;
        DirectedEdge opposite = {edge.to, edge.from};
        if (edgeCount.find(opposite) == edgeCount.end()) {
            nextNodeMap[edge.from] = edge.to;
        }
    }
    
    int filledHoles = 0;
    std::vector<bool> visitedNodes(mesh.vertices.size(), false);
    
    for (auto it = nextNodeMap.begin(); it != nextNodeMap.end(); ++it) {
        uint32_t startNode = it->first;
        if (visitedNodes[startNode]) continue;
        
        std::vector<uint32_t> loop;
        uint32_t current = startNode;
        bool closedLoop = false;
        while (true) {
            if (visitedNodes[current]) break;
            visitedNodes[current] = true;
            loop.push_back(current);
            if (nextNodeMap.find(current) == nextNodeMap.end()) break;
            current = nextNodeMap[current];
            if (current == startNode) { closedLoop = true; break; }
        }
        
        if (closedLoop && loop.size() >= 3) {
            glm::vec3 centroid(0.0f);
            for (uint32_t idx : loop) centroid += mesh.vertices[idx].position;
            centroid /= (float)loop.size();
            
            Vertex centerVtx;
            centerVtx.position = centroid;
            centerVtx.normal = {0,0,0};
            uint32_t centerIdx = (uint32_t)mesh.vertices.size();
            mesh.vertices.push_back(centerVtx);
            
            for (size_t i = 0; i < loop.size(); ++i) {
                uint32_t vA = loop[i];
                uint32_t vB = loop[(i + 1) % loop.size()];
                mesh.indices.push_back(centerIdx);
                mesh.indices.push_back(vB); 
                mesh.indices.push_back(vA);
            }
            filledHoles++;
        }
    }
    
    if (filledHoles > 0) {
        mesh.RecalculateBounds();
        mesh.RecalculateNormals();
        AdjacencyBuilder::Build(mesh);
        mesh.UploadToGPU();
        FeatureDetector::GenerateEdges(mesh, 35.0f);
    }
    
    return filledHoles;
}
