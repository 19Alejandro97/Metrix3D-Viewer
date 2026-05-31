#pragma once
#include <vector>
#include <cstdint>

class Mesh;

class AdjacencyBuilder {
public:
    static void Build(Mesh& mesh);
};
