#pragma once
#include <memory>
#include <vector>
#include "interaction/viewport_event.hpp"
#include "../../core/camera.hpp"
#include "../../core/scene_manager.hpp"

// Forward declarations
struct MeasurementState;
struct AlignmentState;
struct HoverSnapState;

struct ViewportContext {
    MeasurementState& measureState;
    AlignmentState&   alignState;
    HoverSnapState&   snapState;
    bool&             isGizmoOnObject;
    std::string&      gizmoAnchorId;
    int&              gizmoMode;
    int&              gizmoSpace;
};

class IViewportTool {
public:
    virtual ~IViewportTool() = default;
    
    // Core Handlers
    virtual bool HandleInput(const ViewportEvent& e, Camera& camera, SceneManager& scene, ViewportContext& ctx) = 0;
    virtual void Update(const ViewportEvent& e, Camera& camera, SceneManager& scene, ViewportContext& ctx) = 0;
    virtual void RenderOverlays(ImDrawList* dl, const ViewportEvent& e, Camera& camera, ViewportContext& ctx) = 0;
    
    // Lifecycle
    virtual void OnActivate() {}
    virtual void OnDeactivate() {}
    
    virtual const char* GetToolName() const = 0;
};

class ViewportInteractionManager {
public:
    ViewportInteractionManager();
    ~ViewportInteractionManager();

    void Initialize(SceneManager& scene);
    bool Update(const ViewportEvent& e, Camera& camera, SceneManager& scene, ViewportContext& ctx);
    void RenderOverlays(ImDrawList* dl, const ViewportEvent& e, Camera& camera, ViewportContext& ctx);

    void SetActiveTool(const std::string& name);
    IViewportTool* GetActiveTool() const { return m_activeTool; }

private:
    std::vector<std::unique_ptr<IViewportTool>> m_tools;
    IViewportTool* m_activeTool = nullptr;
};
