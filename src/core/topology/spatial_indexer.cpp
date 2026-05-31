#include "spatial_indexer.hpp"
#include "../mesh.hpp"
#include <numeric>
#include <algorithm>
#include <vector>

void SpatialIndexer::BuildBVH(Mesh& mesh) {
    mesh.bvhTree.clear();
    mesh.bvhIndices.clear();
    mesh.bvhIndices.resize(mesh.indices.size() / 3);
    std::iota(mesh.bvhIndices.begin(), mesh.bvhIndices.end(), 0);

    struct TriRef {
        AABB bounds;
        glm::vec3 center;
        uint32_t index;
    };

    std::vector<TriRef> refs;
    refs.reserve(mesh.bvhIndices.size());
    for (size_t i = 0; i < mesh.bvhIndices.size(); ++i) {
        uint32_t idx = mesh.bvhIndices[i];
        Vec3 v0 = Vec3(mesh.vertices[mesh.indices[idx * 3 + 0]].position);
        Vec3 v1 = Vec3(mesh.vertices[mesh.indices[idx * 3 + 1]].position);
        Vec3 v2 = Vec3(mesh.vertices[mesh.indices[idx * 3 + 2]].position);
        if (glm::length(glm::cross(v1 - v0, v2 - v0)) < 1e-9) continue;
        TriRef ref;
        ref.index = idx;
        ref.bounds = {};
        ref.bounds.Expand(v0); ref.bounds.Expand(v1); ref.bounds.Expand(v2);
        ref.center = ref.bounds.Center();
        refs.push_back(ref);
    }
    
    mesh.bvhIndices.resize(refs.size());
    for (size_t i = 0; i < refs.size(); ++i) mesh.bvhIndices[i] = refs[i].index;

    auto recursiveBuild = [&](auto self, int start, int count) -> int {
        int nodeIdx = (int)mesh.bvhTree.size();
        mesh.bvhTree.emplace_back();
        AABB nodeBounds;
        for (int i = 0; i < count; ++i) nodeBounds.Expand(refs[start + i].bounds);
        mesh.bvhTree[nodeIdx].bounds = nodeBounds;
        if (count <= 4) {
            mesh.bvhTree[nodeIdx].triStart = start;
            mesh.bvhTree[nodeIdx].triCount = count;
            for (int i = 0; i < count; ++i) mesh.bvhIndices[start + i] = refs[start + i].index;
            return nodeIdx;
        }
        glm::vec3 size = glm::vec3(nodeBounds.Size());
        int axis = (size.y > size.x && size.y > size.z) ? 1 : (size.z > size.x && size.z > size.y ? 2 : 0);
        float splitPos = (float)nodeBounds.Center()[axis];
        int i = start, j = start + count - 1;
        while (i <= j) {
            if (refs[i].center[axis] < splitPos) i++;
            else std::swap(refs[i], refs[j--]);
        }
        int leftCount = i - start;
        if (leftCount == 0 || leftCount == count) leftCount = count / 2;
        int left = self(self, start, leftCount);
        int right = self(self, start + leftCount, count - leftCount);
        mesh.bvhTree[nodeIdx].left = left;
        mesh.bvhTree[nodeIdx].right = right;
        mesh.bvhTree[nodeIdx].triCount = 0;
        return nodeIdx;
    };

    if (!refs.empty()) recursiveBuild(recursiveBuild, 0, (int)refs.size());
}
