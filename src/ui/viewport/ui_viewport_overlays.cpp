#include "../ui_manager.hpp"
#include "../../core/scene_manager.hpp"
#include "../../core/scene_object.hpp"
#include "../../core/camera.hpp"
#include "../../core/app_settings.hpp"
#include "../ui_state.hpp"
#include <imgui.h>
#include <cstdio>
#include <algorithm>

// --- Static Helpers ---
#include "ui_viewport_helpers.hpp"

static void DrawTextOutlined(ImDrawList* dl, ImVec2 p, ImU32 col, const char* text) {
    dl->AddText(ImVec2(p.x - 1, p.y - 1), 0xFF000000, text);
    dl->AddText(ImVec2(p.x + 1, p.y - 1), 0xFF000000, text);
    dl->AddText(ImVec2(p.x - 1, p.y + 1), 0xFF000000, text);
    dl->AddText(ImVec2(p.x + 1, p.y + 1), 0xFF000000, text);
    dl->AddText(p, col, text);
}

void UIManager::DrawPieceUCS(ImDrawList* drawList, ImVec2 vpPos, ImVec2 vpSize, Camera& camera, SceneManager& scene) {
    auto objects = scene.GetObjects();
    glm::dmat3 rotView = glm::mat3_cast(glm::inverse(camera.smoothOrientation));

    for (auto& obj : objects) {
        if (!obj->isActive || !obj->isVisible) continue;
        Vec3 originW = obj->GetLocalOrigin();
        
        Vec4 clip = camera.projMatrix * camera.viewMatrix * Vec4(originW, 1.0);
        if (clip.w < 0.001f) continue; 
        
        clip /= clip.w;
        float sx = (clip.x * 0.5f + 0.5f) * vpSize.x + vpPos.x;
        float sy = (1.0f - (clip.y * 0.5f + 0.5f)) * vpSize.y + vpPos.y;

        if (sx < vpPos.x || sx > vpPos.x + vpSize.x || sy < vpPos.y || sy > vpPos.y + vpSize.y) continue;

        Mat3 localAxesW = obj->GetLocalAxes();
        const float L = 22.0f; 
        const ImU32 axCol[3] = { IM_COL32(255, 80, 80, 220), IM_COL32( 80, 255, 80, 220), IM_COL32( 80, 80, 255, 220) };
        const char* axLabel[3] = { "x", "y", "z" };

        for (int i = 0; i < 3; ++i) {
            Vec3 axisW = localAxesW[i]; Vec3 axisV = rotView * axisW;
            ImVec2 tip = ImVec2(sx + (float)axisV.x * L, sy - (float)axisV.y * L);
            ImU32 col = axCol[i];
            if (axisV.z < -0.1f) col = (col & 0x00FFFFFF) | 0x44000000;
            drawList->AddLine(ImVec2(sx, sy), tip, col, 1.5f);
            drawList->AddText(ImVec2(tip.x + 3, tip.y - 7), col, axLabel[i]);
        }
        drawList->AddCircleFilled(ImVec2(sx, sy), 2.5f, IM_COL32(255, 255, 255, 180));
    }
}

void UIManager::DrawAlignmentAxes(ImDrawList* drawList, ImVec2 vpPos, ImVec2 vpSize, Camera& camera) {
    if (alignState.step == AlignStep::Idle && !alignState.draftSourceEntity.isValid) return;

    auto DrawOneAxis = [&](const DatumEntity& ent, const glm::vec4& colorVal) {
        if (!ent.isValid) return;
        ImU32 col = ToCol(colorVal);
        const double length = 12.0; // Fixed short length for clarity (double precision)
        
        ImVec2 p0 = WorldToScreenHelper(ent.point, camera, vpPos, vpSize); // Origin
        ImVec2 pm = WorldToScreenHelper(ent.point + (ent.normal * (length * 0.5)), camera, vpPos, vpSize); // Midpoint
        ImVec2 p1 = WorldToScreenHelper(ent.point + (ent.normal * length), camera, vpPos, vpSize); // Tip
        
        if (p0.x > 0 && p1.x > 0) {
            // Directional segments: Red -> Green
            drawList->AddLine(p0, pm, IM_COL32(255, 50, 50, 255), 2.5f);
            drawList->AddLine(pm, p1, IM_COL32(50, 255, 50, 255), 2.5f);
            
            // Base Anchor (with entity color identity)
            drawList->AddCircleFilled(p0, 3.5f, col);
            drawList->AddCircle(p0, 4.5f, IM_COL32(255, 255, 255, 180), 0, 1.0f);
            
            // Directional Tip (Bright Green)
            drawList->AddCircleFilled(p1, 3.0f, IM_COL32(0, 255, 100, 255));
        }
    };
    if (alignState.draftSourceEntity.isValid) DrawOneAxis(alignState.draftSourceEntity, AppSettings::Instance().colorEntityA);
    if (alignState.draftTargetEntity.isValid) DrawOneAxis(alignState.draftTargetEntity, AppSettings::Instance().colorEntityB);
    
    if (alignState.step != AlignStep::Idle) {
        glm::vec4 col = (alignState.step == AlignStep::PickingTarget) ? AppSettings::Instance().colorEntityA : AppSettings::Instance().colorEntityB;
        for (auto& pt : alignState.currentPoints) {
            ImVec2 sp = WorldToScreenHelper(pt, camera, vpPos, vpSize);
            if (sp.x > 0) drawList->AddCircleFilled(sp, 4.0f, ToCol(col));
        }
    }
}


void DrawMeasurementAnnotations(ImDrawList* dl, ImVec2 vpPos, ImVec2 vpSize, Camera& camera, MeasurementState& measureState) {
    auto& s = AppSettings::Instance();
    ImU32 measColor = ToCol(s.measurementColor);
    float thick = s.measurementThickness;
    auto WorldToScreen = [&](const Vec3& wp) -> ImVec2 {
        return WorldToScreenHelper(wp, camera, vpPos, vpSize);
    };

    if (measureState.active) {
        for (auto& pt : measureState.currentPoints) {
            ImVec2 sp = WorldToScreen(pt);
            if (sp.x != -1000) dl->AddCircleFilled(sp, thick * 3.0f, measColor);
        }
    }
    for (const auto& ann : measureState.history) {
        if (!ann.visible) continue;
        if (ann.type == MeasureTab::Distance) {
            if (ann.subMode == 0) { // Point-to-Point
                if (ann.points.size() < 2) continue;
                ImVec2 sa = WorldToScreen(ann.points[0]), sb = WorldToScreen(ann.points[1]);
                if (sa.x != -1000 && sb.x != -1000) {
                    dl->AddCircleFilled(sa, thick * 3.0f, measColor); dl->AddCircleFilled(sb, thick * 3.0f, measColor);
                    dl->AddLine(sa, sb, measColor, thick);
                    char buf[64]; snprintf(buf, sizeof(buf), "%.2f mm", (float)ann.value);
                    ImVec2 textPos = ann.hasCustomPos ? WorldToScreen(ann.customLabelPos) : ImVec2((sa.x+sb.x)*0.5f, (sa.y+sb.y)*0.5f - 12.0f - thick);
                    DrawTextOutlined(dl, textPos, measColor, buf);
                }
            } else if (ann.subMode == 1) { // Point-to-Plane
                if (ann.points.empty()) continue;
                Vec3 p = ann.points[0];
                Vec3 pProj = p - ann.entity.normal * glm::dot(p - ann.entity.point, ann.entity.normal);
                ImVec2 sa = WorldToScreen(p), sb = WorldToScreen(pProj);
                if (sa.x != -1000 && sb.x != -1000) {
                    dl->AddCircleFilled(sa, thick * 3.0f, measColor); dl->AddCircleFilled(sb, thick * 3.0f, measColor);
                    dl->AddLine(sa, sb, measColor, thick);
                    char buf[64]; snprintf(buf, sizeof(buf), "%.2f mm", (float)ann.value);
                    ImVec2 textPos = ann.hasCustomPos ? WorldToScreen(ann.customLabelPos) : ImVec2((sa.x+sb.x)*0.5f, (sa.y+sb.y)*0.5f - 12.0f - thick);
                    DrawTextOutlined(dl, textPos, measColor, buf);
                }
            } else if (ann.subMode == 2) { // Plane-to-Plane
                Vec3 p1 = ann.entity.point;
                Vec3 p2 = ann.entity2.point;
                Vec3 p2Proj = p2 - ann.entity.normal * glm::dot(p2 - p1, ann.entity.normal);
                ImVec2 sa = WorldToScreen(p1), sb = WorldToScreen(p2Proj);
                if (sa.x != -1000 && sb.x != -1000) {
                    dl->AddCircleFilled(sa, thick * 3.0f, measColor); dl->AddCircleFilled(sb, thick * 3.0f, measColor);
                    dl->AddLine(sa, sb, measColor, thick);
                    char buf[64]; snprintf(buf, sizeof(buf), "%.2f mm", (float)ann.value);
                    ImVec2 textPos = ann.hasCustomPos ? WorldToScreen(ann.customLabelPos) : ImVec2((sa.x+sb.x)*0.5f, (sa.y+sb.y)*0.5f - 12.0f - thick);
                    DrawTextOutlined(dl, textPos, measColor, buf);
                }
            } else if (ann.subMode == 3) { // Center-to-Center
                ImVec2 sa = WorldToScreen(ann.entity.point), sb = WorldToScreen(ann.entity2.point);
                if (sa.x != -1000 && sb.x != -1000) {
                    dl->AddCircleFilled(sa, thick * 3.0f, measColor); dl->AddCircleFilled(sb, thick * 3.0f, measColor);
                    dl->AddLine(sa, sb, measColor, thick);
                    char buf[64]; snprintf(buf, sizeof(buf), "%.2f mm", (float)ann.value);
                    ImVec2 textPos = ann.hasCustomPos ? WorldToScreen(ann.customLabelPos) : ImVec2((sa.x+sb.x)*0.5f, (sa.y+sb.y)*0.5f - 12.0f - thick);
                    DrawTextOutlined(dl, textPos, measColor, buf);
                }
            }
        } else if (ann.type == MeasureTab::Circle) {
            Vec3 center = ann.entity.point, normal = ann.entity.normal; double radius = ann.value;
            ImVec2 cp = WorldToScreen(center);
            if (cp.x == -1000) continue;
            dl->AddLine(ImVec2(cp.x - 5, cp.y), ImVec2(cp.x + 5, cp.y), measColor, thick);
            dl->AddLine(ImVec2(cp.x, cp.y - 5), ImVec2(cp.x, cp.y + 5), measColor, thick);
            Vec3 labelPos3D = ann.hasCustomPos ? ann.customLabelPos : center;
            Vec3 planeDir = (ann.hasCustomPos && glm::length(labelPos3D - center) > 1e-4) ? glm::normalize((labelPos3D-center) - normal * glm::dot(labelPos3D-center, normal)) : glm::normalize(glm::cross(normal, (glm::length(glm::cross(normal, Vec3(0,1,0))) < 1e-4) ? Vec3(1,0,0) : Vec3(0,1,0)));
            Vec3 p1 = center + planeDir * radius; ImVec2 sp1 = WorldToScreen(p1);
            Vec3 p2 = center - planeDir * radius; ImVec2 sp2 = WorldToScreen(p2);
            if (sp1.x != -1000 && sp2.x != -1000) {
                dl->AddLine(sp1, sp2, measColor, thick);
                dl->AddCircleFilled(sp1, thick * 2.0f, measColor);
                dl->AddCircleFilled(sp2, thick * 2.0f, measColor);
            }
            char buf[64]; double val = (measureState.showDiameter ? ann.value * 2.0 : ann.value);
            snprintf(buf, sizeof(buf), "%s %.2f mm", (measureState.showDiameter ? "Ø" : "R"), (float)val);
            if (ann.hasCustomPos) {
                dl->AddLine(sp1.x != -1000 ? sp1 : cp, WorldToScreen(labelPos3D), measColor, thick * 0.5f);
                DrawTextOutlined(dl, WorldToScreen(labelPos3D), measColor, buf);
            } else DrawTextOutlined(dl, ImVec2(cp.x + 10, cp.y - 10), measColor, buf);
        } else if (ann.type == MeasureTab::Angle) {
            Vec3 p1, p2;
            if (ann.subMode == 2 && ann.points.size() >= 3) { // 3-Point
                p1 = ann.points[0]; p2 = ann.points[2];
            } else { // Plane/Axis
                p1 = ann.entity.point; p2 = ann.entity2.point;
            }
            ImVec2 sa = WorldToScreen(p1), sb = WorldToScreen(p2);
            if (sa.x != -1000 && sb.x != -1000) {
                dl->AddLine(sa, sb, measColor, thick * 0.5f); // line between the two reference entities
                char buf[64]; snprintf(buf, sizeof(buf), "%.2f°", (float)ann.value);
                ImVec2 textPos = ann.hasCustomPos ? WorldToScreen(ann.customLabelPos) : ImVec2((sa.x+sb.x)*0.5f, (sa.y+sb.y)*0.5f - 12.0f - thick);
                DrawTextOutlined(dl, textPos, measColor, buf);
            }
        }
    }
}
