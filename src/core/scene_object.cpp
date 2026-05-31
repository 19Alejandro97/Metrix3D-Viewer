#include "scene_object.hpp"
#include <random>

// Simple UUID generator
static std::string GenerateUUID() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15); 
    static std::uniform_int_distribution<> dis2(8, 11);
    
    const char* v = "0123456789abcdef";
    std::string res;
    for (int i = 0; i < 8; i++) res += v[dis(gen)];    
    res += "-";
    for (int i = 0; i < 4; i++) res += v[dis(gen)];    
    res += "-4";
    for (int i = 0; i < 3; i++) res += v[dis(gen)];    
    res += "-";
    res += v[dis2(gen)];
    for (int i = 0; i < 3; i++) res += v[dis(gen)];    
    res += "-";
    for (int i = 0; i < 12; i++) res += v[dis(gen)];   
    return res;
}

SceneObject::SceneObject(const std::string& name, std::shared_ptr<Mesh> m)
    : id(GenerateUUID()), name(name), mesh(std::move(m)) {
    transform.Rebuild();
}

void SceneObject::UpdateAssembledTransform() {
    if (!isAssembled) return;

    // Apply local Z-rotation to the base alignment result
    // M_final = M_base * R_z(theta) -> Post-multiplication for local axis
    Mat4 rotZ = glm::rotate(Mat4(1.0), (double)axialAngle, Vec3(0, 0, 1));
    Mat4 finalWorld = baseAlignmentMatrix * rotZ;

    transform.ExtractFromMatrix(finalWorld);
    transform.isDirty = false;
}

AABB SceneObject::GetWorldAABB() const {
    if (!mesh) return {};

    AABB worldBounds;
    // For an accurate world AABB of a scaled/rotated generic mesh, all 8 corners
    // of the local AABB must be transformed and the new min/max computed.
    Vec3 min = mesh->boundingBox.min;
    Vec3 max = mesh->boundingBox.max;

    Vec3 corners[8] = {
        {min.x, min.y, min.z},
        {max.x, min.y, min.z},
        {min.x, max.y, min.z},
        {max.x, max.y, min.z},
        {min.x, min.y, max.z},
        {max.x, min.y, max.z},
        {min.x, max.y, max.z},
        {max.x, max.y, max.z}
    };

    for (int i = 0; i < 8; ++i) {
        Vec4 worldCorner = transform.worldMatrix * Vec4(corners[i], 1.0);
        Vec3 wc = Vec3(worldCorner);
        worldBounds.min = glm::min(worldBounds.min, wc);
        worldBounds.max = glm::max(worldBounds.max, wc);
    }

    return worldBounds;
}

Vec3 SceneObject::GetDimensions() const {
    if (!mesh) return {0,0,0};
    AABB wAABB = GetWorldAABB();
    return wAABB.Size();
}

bool SceneObject::RaycastHitTriangle(const Ray& worldRay, RaycastHit& outHit, std::vector<RaycastHit>* allHits) const {
    if (!mesh || !isVisible) return false;

    // Convert Ray to Local Space to test against raw mesh vertices
    Mat4 invWorld = glm::inverse(transform.worldMatrix);
    Vec3 localOrigin = Vec3(invWorld * Vec4(worldRay.origin, 1.0));
    Vec3 localDir = Vec3(invWorld * Vec4(worldRay.direction, 0.0));

    Ray localRay(localOrigin, localDir);

    bool hitSomething = MathUtils::RaycastBVH(*mesh, localRay, outHit, allHits);

    if (hitSomething) {
        Mat4 normalMatrix = glm::transpose(glm::inverse(transform.worldMatrix));
        
        auto TransformToWorld = [&](RaycastHit& hit) {
            Vec3 worldHitPos = Vec3(transform.worldMatrix * Vec4(hit.point, 1.0));
            hit.distance = glm::length(worldHitPos - worldRay.origin);
            hit.point = worldHitPos;
            hit.normal = glm::normalize(Vec3(normalMatrix * Vec4(hit.normal, 0.0)));
        };

        if (allHits) {
            for (auto& h : *allHits) {
                TransformToWorld(h);
            }
            // Re-sort in world space (in case local sorting differs due to non-uniform scale)
            std::sort(allHits->begin(), allHits->end(), [](const RaycastHit& a, const RaycastHit& b) {
                return a.distance < b.distance;
            });
            outHit = (*allHits)[0];
        } else {
            TransformToWorld(outHit);
        }
        return true;
    }

    return false;
}
