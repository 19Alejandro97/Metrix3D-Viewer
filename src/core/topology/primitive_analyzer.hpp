#pragma once
#include <vector>
#include <cstdint>

class Mesh;

class PrimitiveAnalyzer {
public:
    static void Analyze(Mesh& mesh, float timeLimitMs);
};
