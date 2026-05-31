#include "snap_engine.hpp"
#include "math_utils.hpp"
#include "topology_engine.hpp"
#include "feature_extraction.hpp"
#include "camera.hpp"
#include <glm/gtc/matrix_inverse.hpp>

// --- Phase 32: Occlusion-Aware Snapping (Depth Fix) ---

HoverSnapState SnapEngine::Query(
    const Ray& ray, 
    const Vec2& mousePos,
    const std::vector<std::shared_ptr<SceneObject>>& objects, 
    const Camera& camera,
    const Vec2& vpSize,
    const Vec2& vpPos,
    bool isXRay,
    float snapThreshold
) {
    HoverSnapState result;
    result.isXRay = isXRay;

    Mat4 pv = camera.projMatrix * camera.viewMatrix;

    auto WorldToScreen = [&](const Vec3& wp) -> Vec2 {
        Vec4 clip = pv * Vec4(wp, 1.0);
        if (std::abs(clip.w) < 0.0001) return Vec2(-1e6, -1e6);
        clip /= clip.w;
        double sx = (clip.x * 0.5 + 0.5) * vpSize.x + vpPos.x;
        double sy = (1.0 - (clip.y * 0.5 + 0.5)) * vpSize.y + vpPos.y;
        return Vec2(sx, sy);
    };

    // --- STEP 1: Surface Scan (Visible Depth Buffer) ---
    // We must know what the closest surface is to detect if a circle is hidden.
    SceneObject* closestSurfaceObj = nullptr;
    RaycastHit closestSurfaceHit;
    double minSurfaceDist = 1e9;

    for (const auto& obj : objects) {
        if (!obj->mesh) continue;
        RaycastHit hit;
        if (obj->RaycastHitTriangle(ray, hit)) {
            if (hit.distance < minSurfaceDist) {
                minSurfaceDist = hit.distance;
                closestSurfaceObj = obj.get();
                closestSurfaceHit = hit;
            }
        }
    }

    // --- STEP 2: Feature Scan (Tiers 1 & 2) ---
    double thresholdD = (double)snapThreshold;
    SceneObject* bestP1Obj = nullptr; DatumEntity bestP1Ent; double bestP1DistScale = thresholdD; Vec3 p1;
    SceneObject* bestP2Obj = nullptr; uint32_t v0 = -1, v1 = -1; double bestP2DistScale = thresholdD; Vec3 p2;

    for (const auto& obj : objects) {
        if (!obj->mesh) continue;

        // Tier 1: Datum Entities
        if (obj->mesh->m_isAnalysisReady) {
            auto& cache = obj->mesh->primitiveCache;
            for (const auto& ent : cache) {
                if (ent.type == DatumEntity::Type::Circle) {
                    Vec3 worldPt = obj->transform.worldMatrix * Vec4(ent.point, 1.0);
                    
                    // Occlusion Check: Is this circle hidden behind a surface?
                    double dist3D = glm::distance(ray.origin, worldPt);
                    if (!isXRay && dist3D > minSurfaceDist + 1e-3) continue;

                    Vec2 screenPt = WorldToScreen(worldPt);
                    double dist2D = glm::distance(mousePos, screenPt);

                    if (dist2D < bestP1DistScale) {
                        bestP1DistScale = dist2D;
                        bestP1Obj = obj.get();
                        bestP1Ent = ent;
                        p1 = worldPt;
                    }
                }
            }
        }

        // Tier 2: Feature Edges
        auto CheckEdges = [&](const std::vector<uint32_t>& edges) {
            for (size_t i = 0; i < edges.size(); i += 2) {
                Vec3 lp0 = obj->mesh->vertices[edges[i]].position;
                Vec3 lp1 = obj->mesh->vertices[edges[i+1]].position;
                Vec3 wp0 = obj->transform.worldMatrix * Vec4(lp0, 1.0);
                Vec3 wp1 = obj->transform.worldMatrix * Vec4(lp1, 1.0);
                
                // Occlusion Check (using segment midpoint for speed)
                Vec3 midpoint = (wp0 + wp1) * 0.5;
                double dist3D = glm::distance(ray.origin, midpoint);
                if (!isXRay && dist3D > minSurfaceDist + 1e-3) continue;

                Vec2 s0 = WorldToScreen(wp0);
                Vec2 s1 = WorldToScreen(wp1);
                Vec2 v = s1 - s0;
                Vec2 w = mousePos - s0;
                double l2 = glm::dot(v, v);
                double t = (l2 < 1e-9) ? 0.0 : glm::clamp(glm::dot(w, v) / l2, 0.0, 1.0);
                Vec2 projection = s0 + t * v;
                double dist2D = glm::distance(mousePos, projection);
                
                if (dist2D < bestP2DistScale) {
                    bestP2DistScale = dist2D;
                    bestP2Obj = obj.get();
                    v0 = edges[i]; v1 = edges[i+1];
                    double t3d = 0;
                    MathUtils::SegmentToRayDistance(wp0, wp1, ray.origin, ray.direction, t3d);
                    p2 = glm::mix(wp0, wp1, t3d);
                }
            }
        };
        CheckEdges(obj->mesh->featureEdgeIndices);
        CheckEdges(obj->mesh->boundaryEdgeIndices);
    }

    // --- STEP 3: Result Resolution ---
    if (bestP1Obj) {
        result.isSnapped = true;
        result.snapType = 1; 
        result.snapPoint = p1;
        result.screenDistance = (float)bestP1DistScale;
        result.snappedEntity = bestP1Ent;
        result.snappedEntity.point = p1;
        
        double sX = glm::length(Vec3(bestP1Obj->transform.worldMatrix[0]));
        double sY = glm::length(Vec3(bestP1Obj->transform.worldMatrix[1]));
        double sZ = glm::length(Vec3(bestP1Obj->transform.worldMatrix[2]));
        double avgScale = (sX + sY + sZ) / 3.0;
        result.snappedEntity.radius = bestP1Ent.radius * avgScale;
        
        Mat3 normMat = glm::transpose(glm::inverse(Mat3(bestP1Obj->transform.worldMatrix)));
        result.snappedEntity.normal = MathUtils::SafeNormalize(Vec3(normMat * bestP1Ent.normal));
        result.snapObjectId = bestP1Obj->id;
        result.snappedEntity.objectId = bestP1Obj->id;
    } 
    else if (bestP2Obj) {
        result.isSnapped = true;
        result.snapType = 1; 
        result.snapPoint = p2;
        result.screenDistance = (float)bestP2DistScale;
        
        std::vector<uint32_t> rawLoop = bestP2Obj->mesh->ExtractFeatureLoop(v0, v1);
        result.snapEdgeLoop = rawLoop;
        result.snappedEntity.constituentIndices.clear();
        for (size_t i = 0; i < rawLoop.size(); ++i) {
            result.snappedEntity.constituentIndices.push_back(rawLoop[i]);
            result.snappedEntity.constituentIndices.push_back(rawLoop[(i + 1) % rawLoop.size()]);
        }
        result.snappedEntity.isValid = true;
        result.snapObjectId = bestP2Obj->id;
        result.snappedEntity.objectId = bestP2Obj->id;

        if (rawLoop.size() >= 3) {
             std::vector<Vec3> pts;
             for (uint32_t vIdx : rawLoop) {
                 pts.push_back(bestP2Obj->transform.worldMatrix * Vec4(bestP2Obj->mesh->vertices[vIdx].position, 1.0));
             }
             Vec3 c, n; double r;
             if (MathUtils::Calculate3PointCircle(pts[0], pts[pts.size()/3], pts[2*pts.size()/3], c, r, n)) {
                 result.snappedEntity.type = DatumEntity::Type::Circle;
                 result.snappedEntity.radius = r;
                 result.snappedEntity.point = c;
                 result.snappedEntity.normal = n;
                 result.snapPoint = c;
             }
        }
    }
    else if (closestSurfaceObj) {
        result.isSnapped = true;
        result.snapType = 0;
        result.snapPoint = closestSurfaceHit.point;
        result.snapObjectId = closestSurfaceObj->id;
        
        DatumEntity de = FeatureExtraction::ExtractCircleFromTriangle(*closestSurfaceObj->mesh, closestSurfaceHit.triangleIndex, closestSurfaceObj->id, closestSurfaceObj->transform.worldMatrix);
        if (de.isValid && de.type == DatumEntity::Type::Circle) {
            result.snapType = 1;
            result.snappedEntity = de;
            result.snapPoint = de.point;
        } else {
            result.snappedEntity.type = DatumEntity::Type::Plane;
            result.snappedEntity.point = closestSurfaceHit.point;
            result.snappedEntity.normal = closestSurfaceHit.normal;
            result.snappedEntity.isValid = true;
        }
    }

    return result;
}


