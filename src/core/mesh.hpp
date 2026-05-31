#pragma once
#include "math_types.hpp"
#include "datum_entity.hpp"
#include "topology_engine.hpp"
#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>

struct Vertex {
    Vec3f position;
    Vec3f normal;
};

// (AABB is defined in math_types.hpp)

struct BVHNode {
    AABB bounds;
    int  left    = -1; // Index of left child or -1
    int  right   = -1; // Index of right child or -1
    int  triStart = -1; // Index into sorted indices
    int  triCount = 0;  // Number of triangles in leaf
    bool isLeaf() const { return triCount > 0; }
};

class Mesh {
public:
    static std::atomic<bool> m_shouldTerminate;
    std::atomic<bool>        m_isAnalysisReady {false};
    std::string           name;
    std::vector<Vertex>   vertices;  // CPU copy for geometry processing
    std::vector<uint32_t> indices;
    AABB                  boundingBox;

    // Stores the centroid subtracted from vertices at import time.
    // Add this back to restore original absolute coordinates for STL export.
    Vec3 importOffset = {0.0, 0.0, 0.0};

    // GPU State (Professional Pillar 3)
    struct GPUData {
        unsigned int vao = 0, vbo = 0, ebo = 0;
        unsigned int highlightVao = 0, highlightEbo = 0;
        unsigned int highlightEdgeVao = 0, highlightEdgeEbo = 0;
        unsigned int measuredVao = 0, measuredEbo = 0;
        unsigned int measuredEdgeVao = 0, measuredEdgeEbo = 0;
        unsigned int measuredXRayVao = 0, measuredXRayEbo = 0;
        unsigned int measuredXRayEdgeVao = 0, measuredXRayEdgeEbo = 0;
        unsigned int edgeVao = 0, edgeEbo = 0;
    } m_gpu;

    ~Mesh();

    int GetTriangleCount() const { return static_cast<int>(indices.size() / 3); }

    // Upload vertices and indices to GPU (Called once after loading)
    void UploadToGPU();
    void UpdateHighlightGPU();
    void UpdateHighlightEdgeGPU();
    void UpdateMeasuredGPU();
    void UpdateMeasuredEdgeGPU();
    void UpdateMeasuredXRayGPU();
    void UpdateMeasuredXRayEdgeGPU();
    void FreeGPU();

    // Recalculates the AABB based on current vertices
    void RecalculateBounds();
    
    // Recalculates all vertex normals based on connected triangle faces
    void RecalculateNormals();

    // Adjacency and processing data (Phase 11: Moved below GPU data)
    std::vector<std::vector<uint32_t>> triangleNeighbors;
    std::vector<std::vector<uint32_t>> vertexAdjacency; 
    std::vector<BVHNode> bvhTree;
    std::vector<uint32_t> bvhIndices;
    std::vector<uint32_t> highlightIndices; 
    std::vector<uint32_t> highlightEdgeIndices;
    std::vector<uint32_t> measuredIndices;
    std::vector<uint32_t> measuredEdgeIndices;
    std::vector<uint32_t> measuredXRayIndices;
    std::vector<uint32_t> measuredXRayEdgeIndices;
    std::vector<DatumEntity> primitiveCache; 
    bool                  isXRay = false; 
    float                 triangleDensity = 0.0f; // Average Area / Bounding Box Size
    std::vector<int>      triangleClusterIds;     // Maps each triangle to a primitive cluster (-1 if none)
    std::unordered_map<int, size_t> clusterToPrimitiveIdx; // clusterId -> index in primitiveCache
    std::vector<uint32_t> edgeIndices; 
    std::vector<uint32_t> boundaryEdgeIndices; 
    std::vector<uint32_t> featureEdgeIndices;  
    std::vector<std::vector<uint32_t>> m_vertexToEdges; 

    // Topology Delegation (Refactoring Phase 3)
    void BuildAdjacency();
    void BuildBVH();
    void AnalyzePrimitives(float timeLimitMs = -1.0f);
    void GenerateEdges(float angleThresholdDegrees);
    void SmoothMesh(int iterations = 1, float lambda = 0.5f);
    int  FillHoles();
    std::vector<uint32_t> ExtractFeatureLoop(uint32_t v0, uint32_t v1) const;

    // Factory method to load a mesh from a file using Assimp
    static std::shared_ptr<Mesh> LoadFromFile(const std::string& path, bool centerMesh = true);

    // Bakes the transformation into the vertices and normals
    void BakeTransform(const Mat4f& worldMatrix);

    // Write world-baked binary STL (vertices transformed by worldMatrix)
    bool ExportSTL(const std::string& path, const Mat4f& worldMatrix) const;

    // Append all world-baked vertices from another mesh into this one (for Merge)
    void MergeFrom(const Mesh& other, const Mat4f& otherWorld);

    mutable std::mutex m_primitiveMutex;
};
