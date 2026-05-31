#include "ui_viewport_tools.hpp"
#include <imgui.h>
#include <ImGuizmo/ImGuizmo.h>
#include "../../../core/math_utils.hpp"
#include "../../../core/feature_extraction.hpp"
#include "../../../core/app_settings.hpp"
#include "../../ui_state.hpp"
#include "../ui_viewport_helpers.hpp"

// --- Selection Tool ---
bool SelectionTool::HandleInput(const ViewportEvent& e, Camera& camera, SceneManager& scene, ViewportContext& ctx) {
    if (!e.isHovered || ImGuizmo::IsUsing() || ctx.measureState.isInteractingWithUI) return false;
    
    // Gizmo Priority: Only yield if gizmo is actually visible and hovered
    if (ctx.isGizmoOnObject && ImGuizmo::IsOver()) return false;

    // Defer to specialized tools
    if (ctx.measureState.active || ctx.alignState.step != AlignStep::Idle) return false;

    // --- Mode Switching ---
    if (e.isLeftClicked) {
        if (ImGui::IsKeyPressed(ImGuiKey_W)) ctx.gizmoMode = 0; // Translate
        if (ImGui::IsKeyPressed(ImGuiKey_E)) ctx.gizmoMode = 1; // Rotate
    }

    if (e.isLeftClicked) {
        if (ctx.snapState.isSnapped && !ctx.snapState.snapObjectId.empty()) {
            // Picking SSOT: Leverage the already computed snap state
            for (auto& obj : scene.GetObjects()) {
                if (obj->id == ctx.snapState.snapObjectId) {
                    if (!e.ctrlHeld) { for (auto& o : scene.GetObjects()) o->isSelected = false; }
                    obj->isSelected = true;
                    scene.SetSelectedObject(obj);
                    break;
                }
            }
        } else {
            // Clicked on empty space
            if (!e.ctrlHeld) { 
                for (auto& obj : scene.GetObjects()) obj->isSelected = false; 
                ctx.isGizmoOnObject = false; 
                ctx.gizmoAnchorId.clear();
            }
        }
        return true;
    }
    return false;
}
void SelectionTool::Update(const ViewportEvent& e, Camera& camera, SceneManager& scene, ViewportContext& ctx) {}
void SelectionTool::RenderOverlays(ImDrawList* dl, const ViewportEvent& e, Camera& camera, ViewportContext& ctx) {}

// --- Measurement Tool ---
bool MeasurementTool::HandleInput(const ViewportEvent& e, Camera& camera, SceneManager& scene, ViewportContext& ctx) {
    // --- Label Hover / Hand Cursor logic ---
    auto WorldToScreen = [&](const Vec3& wp) -> ImVec2 {
        return WorldToScreenHelper(wp, camera, e.viewportPos, e.viewportSize);
    };
    
    ctx.measureState.hoveredMeasurementIndex = -1;

    if (e.isHovered && !ctx.measureState.isInteractingWithUI) {
        auto GetLabelCenter = [](const MeasurementAnnotation& ann) -> Vec3 {
            if (ann.hasCustomPos) return ann.customLabelPos;
            switch (ann.type) {
                case MeasureTab::Circle:
                    return ann.entity.point;
                case MeasureTab::Angle:
                    if (ann.subMode == 2 && ann.points.size() >= 3)
                        return ann.points[1];
                    return (ann.entity.point + ann.entity2.point) * 0.5;
                case MeasureTab::Distance:
                default:
                    if (ann.subMode == 0 && ann.points.size() >= 2)
                        return (ann.points[0] + ann.points[1]) * 0.5;
                    if (ann.subMode == 1 && !ann.points.empty())
                        return ann.points[0];
                    if (ann.subMode == 2)
                        return (ann.entity.point + ann.entity2.point) * 0.5;
                    if (ann.subMode == 3)
                        return (ann.entity.point + ann.entity2.point) * 0.5;
                    return ann.entity.point;
            }
        };

        for (int i = 0; i < (int)ctx.measureState.history.size(); ++i) {
            auto& ann = ctx.measureState.history[i];
            if (!ann.visible) continue;
            
            char buf[64];
            if (ann.type == MeasureTab::Circle) {
                double val = ctx.measureState.showDiameter ? ann.value * 2.0 : ann.value;
                const char* sym = ctx.measureState.showDiameter ? "Ø" : "R";
                snprintf(buf, sizeof(buf), "%s %.2f mm", sym, (float)val);
            } else {
                snprintf(buf, sizeof(buf), "%.2f mm", (float)ann.value);
            }

            Vec3 labelPos3D = GetLabelCenter(ann);
            ImVec2 spLabel = WorldToScreen(labelPos3D);
            if (ann.type == MeasureTab::Distance && !ann.hasCustomPos) spLabel.y -= 15.0f;

            ImVec2 textSize = ImGui::CalcTextSize(buf);
            bool isHovered = (e.mousePos.x >= spLabel.x - 4 && e.mousePos.x <= spLabel.x + textSize.x + 4 &&
                              e.mousePos.y >= spLabel.y - 4 && e.mousePos.y <= spLabel.y + textSize.y + 4);
            if (isHovered) {
                ctx.measureState.hoveredMeasurementIndex = i;
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                if (e.isLeftClicked) {
                    ctx.measureState.draggingMeasurementIndex = i;
                    return true; // Click consumed by label
                }
            }
        }
    }

    // --- Label Dragging Logic ---
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        ctx.measureState.draggingMeasurementIndex = -1;
    }

    if (e.isLeftDragging && ctx.measureState.draggingMeasurementIndex != -1) {
        auto& ann = ctx.measureState.history[ctx.measureState.draggingMeasurementIndex];
        ann.hasCustomPos = true;
        
        auto GetLabelCenter = [](const MeasurementAnnotation& ann) -> Vec3 {
            if (ann.hasCustomPos) return ann.customLabelPos;
            switch (ann.type) {
                case MeasureTab::Circle:
                    return ann.entity.point;
                case MeasureTab::Angle:
                    if (ann.subMode == 2 && ann.points.size() >= 3)
                        return ann.points[1];
                    return (ann.entity.point + ann.entity2.point) * 0.5;
                case MeasureTab::Distance:
                default:
                    if (ann.subMode == 0 && ann.points.size() >= 2)
                        return (ann.points[0] + ann.points[1]) * 0.5;
                    if (ann.subMode == 1 && !ann.points.empty())
                        return ann.points[0];
                    if (ann.subMode == 2)
                        return (ann.entity.point + ann.entity2.point) * 0.5;
                    if (ann.subMode == 3)
                        return (ann.entity.point + ann.entity2.point) * 0.5;
                    return ann.entity.point;
            }
        };

        Vec3 planeCenter = GetLabelCenter(ann);
        Vec3 planeNormal = -(glm::dquat(camera.smoothOrientation) * Vec3(0.0, 0.0, 1.0));

        double denom = glm::dot(e.ray.direction, planeNormal);
        if (std::abs(denom) > 1e-6) {
            double t = glm::dot(planeCenter - e.ray.origin, planeNormal) / denom;
            if (t > 0) ann.customLabelPos = e.ray.origin + e.ray.direction * t;
        }
        return true;
    }

    // --- Target Geometry Picking (Only active if measurement tool is enabled)
    if (!ctx.measureState.active) return false;
    if (ctx.measureState.isInteractingWithUI) return false;

    if (e.isLeftClicked && ctx.snapState.isSnapped) {
        SceneObject* target = scene.GetObjectById(ctx.snapState.snapObjectId);
        if (!target) return false;

        auto extractPlane = [&]() -> DatumEntity {
            DatumEntity ent;
            if (ctx.snapState.snappedEntity.isValid && ctx.snapState.snappedEntity.type == DatumEntity::Type::Plane) {
                 return ctx.snapState.snappedEntity;
            }
            RaycastHit hit;
            if (target->RaycastHitTriangle(e.ray, hit)) {
                ent = FeatureExtraction::ExtractPlaneFromTriangle(*target->mesh, hit.triangleIndex, target->id, target->transform.worldMatrix);
            }
            if (!ent.isValid) {
                ent.type = DatumEntity::Type::Plane;
                ent.point = ctx.snapState.snapPoint;
                ent.normal = -(glm::dquat(camera.smoothOrientation) * Vec3(0.0, 0.0, 1.0));
                ent.isValid = true;
            }
            return ent;
        };

        if (ctx.measureState.tab == MeasureTab::Distance) {
            int dMode = ctx.measureState.distanceMode;
            
            if (dMode == 0) { // Point-to-Point
                ctx.measureState.currentPoints.push_back(ctx.snapState.snapPoint);
                if (ctx.measureState.currentPoints.size() == 1) {
                    ctx.measureState.refObjectId = ctx.snapState.snapObjectId;
                }
                if (ctx.measureState.currentPoints.size() == 2) {
                    double dist = glm::length(ctx.measureState.currentPoints[1] - ctx.measureState.currentPoints[0]);
                    MeasurementAnnotation m;
                    m.type = MeasureTab::Distance;
                    m.subMode = 0;
                    m.points = ctx.measureState.currentPoints;
                    m.value = dist;
                    m.isXRay = ctx.snapState.isXRay;
                    
                    // Reference Object Context
                    SceneObject* refObj = scene.GetObjectById(ctx.measureState.refObjectId);
                    Mat4 invMat(1.0);
                    m.referenceObjName = "WCS (World)";
                    if (refObj) {
                        invMat = glm::inverse(Mat4(refObj->transform.worldMatrix));
                        m.referenceObjName = refObj->name;
                    }
                    Vec4 p1_loc = invMat * Vec4(ctx.measureState.currentPoints[0], 1.0);
                    Vec4 p2_loc = invMat * Vec4(ctx.measureState.currentPoints[1], 1.0);
                    m.deltaLocal = Vec3(p2_loc.x - p1_loc.x, p2_loc.y - p1_loc.y, p2_loc.z - p1_loc.z);

                    ctx.measureState.history.push_back(m);
                    ctx.measureState.currentPoints.clear();
                    ctx.measureState.refObjectId.clear();
                    if (!AppSettings::Instance().continuousMeasurement) ctx.measureState.active = false;
                }
            } else if (dMode == 1) { // Point-to-Plane
                if (ctx.measureState.currentPoints.empty() && ctx.measureState.currentEntities.empty()) {
                    ctx.measureState.currentPoints.push_back(ctx.snapState.snapPoint);
                    ctx.measureState.refObjectId = ctx.snapState.snapObjectId;
                } else if (ctx.measureState.currentPoints.size() == 1) {
                    DatumEntity plane = extractPlane();
                    Vec3 point = ctx.measureState.currentPoints[0];
                    double dist = std::abs(glm::dot(point - plane.point, plane.normal));
                    
                    MeasurementAnnotation m;
                    m.type = MeasureTab::Distance;
                    m.subMode = 1;
                    m.points = { point };
                    m.entity = plane;
                    m.value = dist;
                    m.isXRay = ctx.snapState.isXRay;
                    ctx.measureState.history.push_back(m);
                    ctx.measureState.currentPoints.clear();
                    ctx.measureState.currentEntities.clear();
                    if (!AppSettings::Instance().continuousMeasurement) ctx.measureState.active = false;
                }
            } else if (dMode == 2) { // Plane-to-Plane
                DatumEntity plane = extractPlane();
                ctx.measureState.currentEntities.push_back(plane);
                if (ctx.measureState.currentEntities.size() == 2) {
                    DatumEntity p1 = ctx.measureState.currentEntities[0];
                    DatumEntity p2 = ctx.measureState.currentEntities[1];
                    double dist = std::abs(glm::dot(p2.point - p1.point, p1.normal));
                    
                    MeasurementAnnotation m;
                    m.type = MeasureTab::Distance;
                    m.subMode = 2;
                    m.entity = p1;
                    m.entity2 = p2;
                    m.value = dist;
                    m.isXRay = ctx.snapState.isXRay;
                    ctx.measureState.history.push_back(m);
                    ctx.measureState.currentEntities.clear();
                    if (!AppSettings::Instance().continuousMeasurement) ctx.measureState.active = false;
                }
            } else if (dMode == 3) { // Center-to-Center
                DatumEntity circle;
                if (ctx.snapState.snappedEntity.isValid && ctx.snapState.snappedEntity.type == DatumEntity::Type::Circle) {
                    circle = ctx.snapState.snappedEntity;
                } else if (!ctx.snapState.snapEdgeLoop.empty()) {
                    circle = FeatureExtraction::ExtractCircleFromIndices(*target->mesh, ctx.snapState.snapEdgeLoop, target->id, target->transform.worldMatrix);
                }
                if (circle.isValid) {
                    ctx.measureState.currentEntities.push_back(circle);
                    if (ctx.measureState.currentEntities.size() == 2) {
                        DatumEntity c1 = ctx.measureState.currentEntities[0];
                        DatumEntity c2 = ctx.measureState.currentEntities[1];
                        double dist = glm::length(c2.point - c1.point);
                        
                        MeasurementAnnotation m;
                        m.type = MeasureTab::Distance;
                        m.subMode = 3;
                        m.entity = c1;
                        m.entity2 = c2;
                        m.value = dist;
                        m.isXRay = ctx.snapState.isXRay;
                        ctx.measureState.history.push_back(m);
                        ctx.measureState.currentEntities.clear();
                        if (!AppSettings::Instance().continuousMeasurement) ctx.measureState.active = false;
                    }
                }
            }
            return true;
        } else if (ctx.measureState.tab == MeasureTab::Circle) {
            if (ctx.measureState.circleMode == CircleMode::Auto) {
                DatumEntity circle;
                if (ctx.snapState.snappedEntity.isValid && ctx.snapState.snappedEntity.type == DatumEntity::Type::Circle) {
                    circle = ctx.snapState.snappedEntity;
                } else if (!ctx.snapState.snapEdgeLoop.empty()) {
                    circle = FeatureExtraction::ExtractCircleFromIndices(*target->mesh, ctx.snapState.snapEdgeLoop, target->id, target->transform.worldMatrix);
                }
                
                if (circle.isValid) {
                    MeasurementAnnotation m;
                    m.type = MeasureTab::Circle;
                    m.entity = circle;
                    m.value = circle.radius; 
                    m.isXRay = ctx.snapState.isXRay;
                    ctx.measureState.history.push_back(m);
                    if (!AppSettings::Instance().continuousMeasurement) ctx.measureState.active = false;
                }
            } else { // 3 points mode
                ctx.measureState.currentPoints.push_back(ctx.snapState.snapPoint);
                if (ctx.measureState.currentPoints.size() == 3) {
                    Vec3 center, normal; double radius;
                    if (MathUtils::Calculate3PointCircle(ctx.measureState.currentPoints[0], ctx.measureState.currentPoints[1], ctx.measureState.currentPoints[2], center, radius, normal)) {
                        MeasurementAnnotation m;
                        m.type = MeasureTab::Circle;
                        m.entity.type = DatumEntity::Type::Circle;
                        m.entity.point = center; m.entity.radius = radius; m.entity.normal = normal;
                        m.value = radius; m.isXRay = ctx.snapState.isXRay;
                        ctx.measureState.history.push_back(m);
                    }
                    ctx.measureState.currentPoints.clear();
                    if (!AppSettings::Instance().continuousMeasurement) ctx.measureState.active = false;
                }
            }
            return true;
        } else if (ctx.measureState.tab == MeasureTab::Angle) {
            int aMode = ctx.measureState.angleMode;
            if (aMode == 0 || aMode == 1) { // Plane-to-Plane or Axis-to-Axis
                DatumEntity ent = extractPlane();
                ctx.measureState.currentEntities.push_back(ent);
                if (ctx.measureState.currentEntities.size() == 2) {
                    DatumEntity p1 = ctx.measureState.currentEntities[0];
                    DatumEntity p2 = ctx.measureState.currentEntities[1];
                    double dot = glm::clamp(glm::dot(p1.normal, p2.normal), -1.0, 1.0);
                    double rads = std::acos(dot);
                    
                    MeasurementAnnotation m;
                    m.type = MeasureTab::Angle;
                    m.subMode = aMode;
                    m.entity = p1;
                    m.entity2 = p2;
                    m.value = rads * 180.0 / 3.14159265358979323846;
                    m.isXRay = ctx.snapState.isXRay;
                    ctx.measureState.history.push_back(m);
                    ctx.measureState.currentEntities.clear();
                    if (!AppSettings::Instance().continuousMeasurement) ctx.measureState.active = false;
                }
            } else if (aMode == 2) { // 3-Point Angle
                ctx.measureState.currentPoints.push_back(ctx.snapState.snapPoint);
                if (ctx.measureState.currentPoints.size() == 3) {
                    Vec3 p1 = ctx.measureState.currentPoints[0];
                    Vec3 p2 = ctx.measureState.currentPoints[1]; // Vertex
                    Vec3 p3 = ctx.measureState.currentPoints[2];
                    Vec3 dir1 = MathUtils::SafeNormalize(p1 - p2, Vec3(1,0,0));
                    Vec3 dir2 = MathUtils::SafeNormalize(p3 - p2, Vec3(0,1,0));
                    double dot = glm::clamp(glm::dot(dir1, dir2), -1.0, 1.0);
                    
                    MeasurementAnnotation m;
                    m.type = MeasureTab::Angle;
                    m.subMode = aMode;
                    m.points = ctx.measureState.currentPoints;
                    m.value = std::acos(dot) * 180.0 / 3.14159265358979323846;
                    m.isXRay = ctx.snapState.isXRay;
                    ctx.measureState.history.push_back(m);
                    ctx.measureState.currentPoints.clear();
                    if (!AppSettings::Instance().continuousMeasurement) ctx.measureState.active = false;
                }
            }
            return true;
        }
    }
    return false;
}
void MeasurementTool::Update(const ViewportEvent& e, Camera& camera, SceneManager& scene, ViewportContext& ctx) {}
void MeasurementTool::RenderOverlays(ImDrawList* dl, const ViewportEvent& e, Camera& camera, ViewportContext& ctx) {}
void MeasurementTool::OnDeactivate() { /* Clean points */ }

// --- Alignment Tool ---
bool AlignmentTool::HandleInput(const ViewportEvent& e, Camera& camera, SceneManager& scene, ViewportContext& ctx) {
    if (ctx.alignState.step == AlignStep::Idle) return false;
    
    if (e.isLeftClicked) {
        SceneObject* pickTarget = nullptr;
        DatumEntity  detected;

        if (ctx.snapState.isSnapped) {
            pickTarget = scene.GetObjectById(ctx.snapState.snapObjectId);
            if (pickTarget) {
                AlignEntitySelect type = (ctx.alignState.step == AlignStep::PickingSource)
                               ? ctx.alignState.draftSourceType : ctx.alignState.draftTargetType;

                if (ctx.snapState.snappedEntity.isValid) {
                    detected = ctx.snapState.snappedEntity;
                } else if (type == AlignEntitySelect::SinglePoint) {
                    detected.type = DatumEntity::Type::SinglePoint;
                    detected.isValid = true;
                    detected.point = ctx.snapState.snapPoint;
                    detected.objectId = pickTarget->id;
                } else if (type == AlignEntitySelect::Line || type == AlignEntitySelect::ThreePoints) {
                    ctx.alignState.currentPoints.push_back(ctx.snapState.snapPoint);
                    if (type == AlignEntitySelect::Line && ctx.alignState.currentPoints.size() == 2) {
                        detected = DatumEntity::FromLine(ctx.alignState.currentPoints[0], ctx.alignState.currentPoints[1], pickTarget->id);
                    } else if (type == AlignEntitySelect::ThreePoints && ctx.alignState.currentPoints.size() == 3) {
                        Vec3 center, normal; double radius;
                        if (MathUtils::Calculate3PointCircle(ctx.alignState.currentPoints[0], ctx.alignState.currentPoints[1], ctx.alignState.currentPoints[2], center, radius, normal)) {
                            detected.type = DatumEntity::Type::Circle; detected.isValid = true;
                            detected.point = center; detected.normal = normal; detected.radius = radius;
                            detected.objectId = pickTarget->id;
                        } else {
                            ctx.alignState.currentPoints.clear();
                        }
                    }
                }
            }
        }
        
        if (!detected.isValid) {
            float minT = 1e9f;
            for (auto& obj : scene.GetObjects()) {
                RaycastHit hit;
                if (obj->RaycastHitTriangle(e.ray, hit) && (float)hit.distance < minT) {
                    minT = (float)hit.distance; pickTarget = obj.get();
                }
            }
            if (pickTarget) {
                AlignEntitySelect type = (ctx.alignState.step == AlignStep::PickingSource)
                               ? ctx.alignState.draftSourceType : ctx.alignState.draftTargetType;
                RaycastHit hit; pickTarget->RaycastHitTriangle(e.ray, hit);
                
                if (type == AlignEntitySelect::Plane) {
                    detected = FeatureExtraction::ExtractPlaneFromTriangle(*pickTarget->mesh, hit.triangleIndex, pickTarget->id, pickTarget->transform.worldMatrix);
                } else if (type == AlignEntitySelect::Circle || type == AlignEntitySelect::Cylinder) {
                    detected = FeatureExtraction::ExtractCircleFromTriangle(*pickTarget->mesh, hit.triangleIndex, pickTarget->id, pickTarget->transform.worldMatrix);
                } else if (type == AlignEntitySelect::SinglePoint) {
                    detected.type = DatumEntity::Type::SinglePoint; detected.isValid = true;
                    detected.point = hit.point; detected.objectId = pickTarget->id;
                }
            }
        }

        if (pickTarget && detected.isValid) {
            if (ctx.alignState.step == AlignStep::PickingSource) {
                ctx.alignState.draftSourceEntity = detected;
                ctx.alignState.draftSourceObjId = pickTarget->id;
                ctx.alignState.step = AlignStep::Idle;
                for (auto& obj : scene.GetObjects()) { obj->isSelected = false; }
                pickTarget->isSelected = true;
                for (auto& obj : scene.GetObjects()) if (obj.get() == pickTarget) { scene.SetSelectedObject(obj); break; }
            } else if (ctx.alignState.step == AlignStep::PickingTarget) {
                ctx.alignState.draftTargetEntity = detected;
                ctx.alignState.draftTargetObjId = pickTarget->id;
                ctx.alignState.step = AlignStep::Idle;
            }
            ctx.alignState.currentPoints.clear();
            return true;
        }
    }
    return false;
}
void AlignmentTool::Update(const ViewportEvent& e, Camera& camera, SceneManager& scene, ViewportContext& ctx) {}
void AlignmentTool::RenderOverlays(ImDrawList* dl, const ViewportEvent& e, Camera& camera, ViewportContext& ctx) {}
