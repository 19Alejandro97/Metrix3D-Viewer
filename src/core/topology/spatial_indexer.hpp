#pragma once
#include <vector>
#include <cstdint>

class Mesh;

class SpatialIndexer {
public:
    static void BuildBVH(Mesh& mesh);
};
