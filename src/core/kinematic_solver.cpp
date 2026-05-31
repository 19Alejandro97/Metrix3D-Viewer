#include "kinematic_solver.hpp"
#include "math_utils.hpp"
#include "transform.hpp"
#include "../ui/app_console.hpp"
#include "../ui/ui_state.hpp"
#include <cmath>

// ---------------------------------------------------------------------------
// AlignTransformToCircle
// Moves the transform so that `center` sits at world origin and `normal` → +Z
// ---------------------------------------------------------------------------
void KinematicSolver::AlignTransformToCircle(Transform& transform, const Vec3& center, const Vec3& normal) {
    Mat4 translationMat = glm::translate(Mat4(1.0), -center);

    Vec3 zAxis(0.0, 0.0, 1.0);
    Vec3 cross = glm::cross(normal, zAxis);
    double dot  = glm::dot(normal, zAxis);

    Quat rotation;
    if (dot < -0.999999) {
        Vec3 ortho = std::abs(normal.z) < 0.99 ? Vec3(0,0,1) : Vec3(1,0,0);
        Vec3 axis  = glm::normalize(glm::cross(normal, ortho));
        rotation   = glm::angleAxis(glm::pi<double>(), axis);
    } else if (dot > 0.999999) {
        rotation = Quat(1.0, 0.0, 0.0, 0.0);
    } else {
        double s    = std::sqrt((1.0 + dot) * 2.0);
        double invs = 1.0 / s;
        rotation    = Quat(s * 0.5, cross.x * invs, cross.y * invs, cross.z * invs);
        rotation    = glm::normalize(rotation);
    }

    Mat4 rotationMat = glm::mat4_cast(rotation);
    Mat4 newWorld    = rotationMat * translationMat * transform.worldMatrix;
    transform.ExtractFromMatrix(newWorld);
}

// ---------------------------------------------------------------------------
// AlignEntityToEntity
// Rotates + translates the source transform so that source entity maps to target entity
// ---------------------------------------------------------------------------
void KinematicSolver::AlignEntityToEntity(const DatumEntity& target, const DatumEntity& source,
                                           bool invertNormal, Transform& sourceObjTransform) {
    Mat4 invInitial = glm::inverse(sourceObjTransform.worldMatrix);

    Vec3 srcCenterLocal = Vec3(invInitial * Vec4(source.point, 1.0));
    Vec3 srcNormalLocal = MathUtils::SafeNormalize(Vec3(glm::mat3(invInitial) * source.normal));

    Vec3 tgtCenterWorld = target.point;
    Vec3 tgtNormalWorld = target.normal;

    Vec3 targetNormal = invertNormal ? -target.normal : target.normal;
    Vec3 v1 = srcNormalLocal;
    Vec3 v2 = -targetNormal; // Face‑to‑face by default

    Quat q;
    Vec3 axis = glm::cross(v1, v2);
    double dot = glm::dot(v1, v2);

    if (dot > 0.999999) {
        q = Quat(1.0, 0.0, 0.0, 0.0);
    } else if (dot < -0.999999) {
        Vec3 ortho   = (std::abs(v1.z) < 0.9) ? Vec3(0,0,1) : Vec3(1,0,0);
        Vec3 rotAxis = MathUtils::SafeNormalize(glm::cross(v1, ortho));
        q = glm::angleAxis(glm::pi<double>(), rotAxis);
    } else {
        double s    = std::sqrt((1.0 + dot) * 2.0);
        double invs = 1.0 / s;
        q = Quat(s * 0.5, axis.x * invs, axis.y * invs, axis.z * invs);
        q = glm::normalize(q);
    }

    Mat4 R = glm::mat4_cast(q);

    Vec3 rotatedSrcCenter = Vec3(R * Vec4(srcCenterLocal, 1.0));
    Vec3 translation      = tgtCenterWorld - rotatedSrcCenter;
    Mat4 T                = glm::translate(Mat4(1.0), translation);

    Mat4 finalWorld = T * R;
    sourceObjTransform.ExtractFromMatrix(finalWorld);

    // Radius audit
    double tolerance = (source.radius + target.radius) * 0.0005;
    double deltaR    = std::abs(source.radius - target.radius);
    if (deltaR > tolerance) {
        AppConsole::Log("[Warning] Radius discrepancy: " + std::to_string(deltaR) + " mm. Coupling might be inconsistent.",
                        ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
    } else {
        AppConsole::Log("[SmartAssembly] Nominal coupling achieved (diff: " + std::to_string(deltaR) + " mm).",
                        ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
    }
}

// ---------------------------------------------------------------------------
// ApplySequentialConstraints
// Resolves a queue of constraints cumulatively without breaking previous registrations
// ---------------------------------------------------------------------------
void KinematicSolver::ApplySequentialConstraints(Transform& sourceObjTransform,
                                                   const std::vector<ConstraintLine>& constraints,
                                                   const Mat4& initialWorldMatrix) {
    if (constraints.empty()) return;

    // 1st Constraint: Full registration (Rotation + Translation)
    const auto& c1 = constraints[0];
    AlignEntityToEntity(c1.targetEntity, c1.sourceEntity, c1.isInverse, sourceObjTransform);

    // 2nd Constraint: Pure translation (preserves C1 rotation)
    if (constraints.size() > 1) {
        const auto& c2 = constraints[1];
        if (c2.relation == AlignRelationType::Concentric || c2.relation == AlignRelationType::Coincident) {
            Mat4 invInitial  = glm::inverse(initialWorldMatrix);
            Vec3 srcLocalPt  = Vec3(invInitial * Vec4(c2.sourceEntity.point, 1.0));
            Vec3 srcWorldNow = Vec3(sourceObjTransform.worldMatrix * Vec4(srcLocalPt, 1.0));
            Vec3 deltaPos    = c2.targetEntity.point - srcWorldNow;

            sourceObjTransform.position += deltaPos;
            sourceObjTransform.Rebuild();
            AppConsole::Log("[Align Solver] Secondary constraint solved. Pure translation: " + std::to_string(glm::length(deltaPos)) + " mm.",
                            ImVec4(0.3f, 0.9f, 0.9f, 1.0f));
        }
    }

    if (constraints.size() > 2) {
        AppConsole::Log("[Align Solver] Tertiary constraint ignored in current solver tier.",
                        ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
    }
}
