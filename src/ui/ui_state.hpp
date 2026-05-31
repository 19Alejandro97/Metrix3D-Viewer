#pragma once
#include <vector>
#include <string>
#include "../core/math_types.hpp"
#include "../core/datum_entity.hpp"

// ---------------------------------------------------------------------------
// Alignment State (Phase 15: Magics Style Constraints Manager)
// ---------------------------------------------------------------------------
enum class AlignEntitySelect { SinglePoint = 0, ThreePoints, Line, Circle, Plane, Cylinder, Sphere };
enum class AlignRelationType { Coincident = 0, Concentric, Parallel, Perpendicular, Distance, Angle };

struct ConstraintLine {
    DatumEntity sourceEntity; // Moving part
    DatumEntity targetEntity; // Static part
    std::string sourceObjectId;
    std::string targetObjectId;
    AlignRelationType relation = AlignRelationType::Coincident;
    bool isInverse = false; // Phase 21: Orientation control
    bool isValid = true;
};

enum class AlignStep { Idle, PickingSource, PickingTarget };

struct AlignmentState {
    AlignStep step = AlignStep::Idle;
    
    // Add Constraint Dialog State
    bool showAddDialog = false;
    AlignEntitySelect draftSourceType = AlignEntitySelect::Plane;
    AlignEntitySelect draftTargetType = AlignEntitySelect::Plane;
    AlignRelationType draftRelation = AlignRelationType::Coincident;
    bool draftIsInverse = false; // Phase 21: UI select state
    
    // Picked Draft Entities
    DatumEntity draftSourceEntity;
    DatumEntity draftTargetEntity;
    std::string draftSourceObjId;
    std::string draftTargetObjId;
    std::vector<Vec3> currentPoints;
    
    // Constraints Manager
    std::vector<ConstraintLine> activeConstraints;
    std::string lastResultMsg;

    void Reset() {
        step = AlignStep::Idle;
        showAddDialog = false;
        draftSourceEntity = {}; draftTargetEntity = {};
        draftSourceObjId.clear(); draftTargetObjId.clear();
        draftIsInverse = false;
        currentPoints.clear();
        activeConstraints.clear();
        lastResultMsg.clear();
    }

    // Clears any state dependent on a now-deleted object.
    void CleanForObject(const std::string& id) {
        for (auto it = activeConstraints.begin(); it != activeConstraints.end(); ) {
            if (it->sourceObjectId == id || it->targetObjectId == id) {
                it = activeConstraints.erase(it);
            } else {
                ++it;
            }
        }
        if (draftSourceObjId == id || draftTargetObjId == id) {
            draftSourceEntity = {}; draftTargetEntity = {};
            draftSourceObjId.clear(); draftTargetObjId.clear();
            step = AlignStep::Idle;
            showAddDialog = false;
        }
    }
};

// ---------------------------------------------------------------------------
// Measurement State
// ---------------------------------------------------------------------------
enum class MeasureTab { Distance, Circle, Angle };
enum class CircleMode { Auto, ThreePoints };

struct MeasurementAnnotation {
    MeasureTab  type;
    int         subMode = 0; // distanceMode or angleMode
    DatumEntity entity;     // Primary entity (Circle / Plane A)
    DatumEntity entity2;    // Secondary entity (Plane B / Axis B)
    std::vector<Vec3> points; // For Distance
    double      value = 0.0;  // result
    bool        isXRay = false; // Persistent visual hint
    bool        visible = true; // Visibility toggle for viewport

    // Phase 12: Local UCS Persistence
    Vec3        deltaLocal = {0,0,0};
    std::string referenceObjName;

    // Phase 14: Movable Cotas (Drafting Standards)
    Vec3        customLabelPos = {0,0,0};
    bool        hasCustomPos = false;
};

struct MeasurementState {
    bool           active       = false;
    MeasureTab     tab          = MeasureTab::Distance;
    CircleMode     circleMode   = CircleMode::Auto;
    
    // Professional UI state
    bool           showDiameter = true;   // Industrial R vs Ø toggle
    int            distanceMode = 0;      // 0=Point-to-Point, 1=Point-to-Plane, 2=Plane-to-Plane, 3=Center-to-Center
    int            angleMode    = 0;      // 0=Plane-to-Plane, 1=Axis-to-Axis, 2=3-Point Angle
    bool           xRayEnabled  = false;  // Direct control for occlusion picking
    
    std::vector<Vec3> currentPoints;
    std::vector<DatumEntity> currentEntities; // Intermediate picking
    std::string       refObjectId;        // Phase 12: Reference Frame context
    std::vector<MeasurementAnnotation> history;

    int         draggingMeasurementIndex = -1; // Phase 14
    int         hoveredMeasurementIndex  = -1; // Predict label hover
    bool        isInteractingWithUI      = false; // Phase 18: Interaction Priority Interlock

    void Reset() {
        active = false; tab = MeasureTab::Distance; circleMode = CircleMode::Auto;
        showDiameter = true; distanceMode = 0; angleMode = 0; xRayEnabled = false;
        currentPoints.clear(); currentEntities.clear(); refObjectId.clear(); history.clear();
        draggingMeasurementIndex = -1; hoveredMeasurementIndex = -1;
    }
};

// ---------------------------------------------------------------------------
// Global Snapping State (SSoT)
// ---------------------------------------------------------------------------
struct HoverSnapState {
    bool                  isSnapped   = false;
    int                   snapType    = 0; // 0=Face, 1=Edge, 2=Vertex
    Vec3                  snapPoint   = {0, 0, 0};
    std::string           snapObjectId;
    std::vector<uint32_t> snapEdgeLoop;
    DatumEntity           snappedEntity;       // Local mathematical definition if primitive
    bool                  isXRay      = false; // If true, we are snapping through the mesh
    float                 screenDistance = 1e9f;
    
    void Reset() {
        isSnapped = false;
        snapType = 0;
        snapPoint = {0,0,0};
        snapObjectId.clear();
        snapEdgeLoop.clear();
        snappedEntity = {};
        isXRay = false;
        screenDistance = 1e9f;
    }
};

