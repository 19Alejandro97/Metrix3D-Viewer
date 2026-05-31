#pragma once
#include <vector>
#include <cstdint>

class Mesh;

class FeatureDetector {
public:
    static void GenerateEdges(Mesh& mesh, float angleThresholdDegrees);
    static std::vector<uint32_t> ExtractFeatureLoop(const Mesh& mesh, uint32_t v0, uint32_t v1);
};
