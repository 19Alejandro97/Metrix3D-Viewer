#pragma once
#include "../ui_viewport_interaction.hpp"

class SelectionTool : public IViewportTool {
public:
    bool HandleInput(const ViewportEvent& e, Camera& camera, SceneManager& scene, ViewportContext& ctx) override;
    void Update(const ViewportEvent& e, Camera& camera, SceneManager& scene, ViewportContext& ctx) override;
    void RenderOverlays(ImDrawList* dl, const ViewportEvent& e, Camera& camera, ViewportContext& ctx) override;
    const char* GetToolName() const override { return "Selection"; }
};

class MeasurementTool : public IViewportTool {
public:
    bool HandleInput(const ViewportEvent& e, Camera& camera, SceneManager& scene, ViewportContext& ctx) override;
    void Update(const ViewportEvent& e, Camera& camera, SceneManager& scene, ViewportContext& ctx) override;
    void RenderOverlays(ImDrawList* dl, const ViewportEvent& e, Camera& camera, ViewportContext& ctx) override;
    void OnDeactivate() override;
    const char* GetToolName() const override { return "Measurement"; }
};

class AlignmentTool : public IViewportTool {
public:
    bool HandleInput(const ViewportEvent& e, Camera& camera, SceneManager& scene, ViewportContext& ctx) override;
    void Update(const ViewportEvent& e, Camera& camera, SceneManager& scene, ViewportContext& ctx) override;
    void RenderOverlays(ImDrawList* dl, const ViewportEvent& e, Camera& camera, ViewportContext& ctx) override;
    const char* GetToolName() const override { return "Alignment"; }
};
