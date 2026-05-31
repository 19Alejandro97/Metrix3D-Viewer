#pragma once
// ---------------------------------------------------------------------------
// FeatureExtraction Engine
// Responsible for all geometric feature mining from mesh data:
//   - Plane extraction via flood-fill
//   - Circle/Edge loop extraction via Welzl MEC
//   - Edge loop propagation along crease edges
// ---------------------------------------------------------------------------
#include "math_types.hpp"
#include "datum_entity.hpp"
#include <vector>
#include <string>

class FeatureExtraction {
public:
    // Extracts a best-fit plane from a seed triangle using coplanar flood-fill.
    static DatumEntity ExtractPlaneFromTriangle(
        const class Mesh& mesh, int seedTriangleIndex,
        const std::string& objId, const Mat4& worldMatrix);

    // Extracts a circle by fitting a Minimum Enclosing Circle to the boundary
    // loop of a contiguous cylindrical surface patch (flood-fill + boundary walk).
    static DatumEntity ExtractCircleFromTriangle(
        const class Mesh& mesh, int seedTriangleIndex,
        const std::string& objId, const Mat4& worldMatrix);

    // Walks the crease-edge topology from a snap point to extract a complete
    // circular edge loop and fits a circle via Welzl MEC.
    static DatumEntity ExtractEdgeLoop(
        const class Mesh& mesh, const Vec3& localSnapPoint,
        const std::string& objId, const Mat4& worldMatrix);

    // Fits a circle to an arbitrary set of vertex indices using PCA + Welzl MEC.
    static DatumEntity ExtractCircleFromIndices(
        const class Mesh& mesh, const std::vector<uint32_t>& loop,
        const std::string& objId, const Mat4& worldMatrix);
};
