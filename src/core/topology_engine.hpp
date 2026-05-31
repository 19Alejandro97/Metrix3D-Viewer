#pragma once

#include <vector>
#include <cstdint>
#include <memory>

class Mesh;

// ---------------------------------------------------------------------------
// Topology Engine
// 
// Professional Pillar: Separation of Concerns
// Extracts heavy topology, BVH generation, and spatial analysis out of
// the pure Mesh data container. All operations mutate or query the Mesh.
// ---------------------------------------------------------------------------
class TopologyEngine {
public:
    // Builds the half-edge logical neighbor structure and detects hard feature edges
    static void BuildAdjacency(Mesh& mesh);

    // Builds the Spatial Acceleration Tree (LBVH) for raycasting
    static void BuildBVH(Mesh& mesh);

    // Analyzes mesh curvature and topology to find circles, planes, cylinders.
    // timeLimitMs: If > 0, performs fragment-based analysis for 60FPS responsive loading.
    static void AnalyzePrimitives(Mesh& mesh, float timeLimitMs = -1.0f);

    // Generates the line-segment indices for rendering hard edges
    static void GenerateEdges(Mesh& mesh, float angleThresholdDegrees);

    // Applies a Laplacian smoothing pass to the mesh geometry
    static void SmoothMesh(Mesh& mesh, int iterations = 1, float lambda = 0.5f);

    // Detects boundary loops and triangulates a centroid fan to fill holes. Returns hole count.
    static int FillHoles(Mesh& mesh);

    // Navigates the crease edge graph to find a continuous bounding feature loop
    static std::vector<uint32_t> ExtractFeatureLoop(const Mesh& mesh, uint32_t v0, uint32_t v1);
};
