#include "topology_engine.hpp"
#include "mesh.hpp"
#include "topology/adjacency_builder.hpp"
#include "topology/spatial_indexer.hpp"
#include "topology/primitive_analyzer.hpp"
#include "topology/feature_detector.hpp"
#include "topology/mesh_processor.hpp"
#include <algorithm>
#include <vector>
#include <glm/glm.hpp>

void TopologyEngine::GenerateEdges(Mesh& mesh, float angleThresholdDegrees) {
    FeatureDetector::GenerateEdges(mesh, angleThresholdDegrees);
}

void TopologyEngine::BuildAdjacency(Mesh& mesh) {
    AdjacencyBuilder::Build(mesh);
}

void TopologyEngine::BuildBVH(Mesh& mesh) {
    SpatialIndexer::BuildBVH(mesh);
}

void TopologyEngine::AnalyzePrimitives(Mesh& mesh, float timeLimitMs) {
    PrimitiveAnalyzer::Analyze(mesh, timeLimitMs);
}

std::vector<uint32_t> TopologyEngine::ExtractFeatureLoop(const Mesh& mesh, uint32_t v0, uint32_t v1) {
    return FeatureDetector::ExtractFeatureLoop(mesh, v0, v1);
}

// Mesh Processing logic (Keep here for now or move to MeshProcessor in future phase)
void TopologyEngine::SmoothMesh(Mesh& mesh, int iterations, float lambda) {
    MeshProcessor::SmoothMesh(mesh, iterations, lambda);
}

int TopologyEngine::FillHoles(Mesh& mesh) {
    return MeshProcessor::FillHoles(mesh);
}
