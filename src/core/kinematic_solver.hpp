#pragma once
// ---------------------------------------------------------------------------
// KinematicSolver
// Responsible for all alignment and assembly kinematic operations:
//   - Single entity-to-entity alignment (rotation + translation)
//   - Sequential multi-constraint solver (Magics-style)
//   - Transform-to-circle orientation utility
// ---------------------------------------------------------------------------
#include "math_types.hpp"
#include "datum_entity.hpp"
#include <vector>

struct ConstraintLine; // Forward declared from ui_state.hpp

class KinematicSolver {
public:
    // Aligns a transform so a circle center sits at origin and normal faces +Z.
    static void AlignTransformToCircle(
        class Transform& transform, const Vec3& center, const Vec3& normal);

    // Aligns the source entity to the target entity.
    // invertNormal: flips the source facing direction (face-to-back mode).
    static void AlignEntityToEntity(
        const DatumEntity& target, const DatumEntity& source,
        bool invertNormal, class Transform& sourceObjTransform);

    // Resolves a queue of alignment constraints cumulatively without
    // breaking previously applied kinematic registrations.
    static void ApplySequentialConstraints(
        class Transform& sourceObjTransform,
        const std::vector<ConstraintLine>& constraints,
        const Mat4& initialWorldMatrix);
};
