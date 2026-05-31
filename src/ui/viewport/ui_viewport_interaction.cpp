#include "ui_viewport_interaction.hpp"
#include "tools/ui_viewport_tools.hpp"
#include <string>
#include <algorithm>

ViewportInteractionManager::ViewportInteractionManager() : m_activeTool(nullptr) {}

ViewportInteractionManager::~ViewportInteractionManager() {
    if (m_activeTool) m_activeTool->OnDeactivate();
}

void ViewportInteractionManager::Initialize(SceneManager& scene) {
    m_tools.push_back(std::make_unique<MeasurementTool>());
    m_tools.push_back(std::make_unique<AlignmentTool>());
    m_tools.push_back(std::make_unique<SelectionTool>());
    
    // Default tool is Selection
    m_activeTool = m_tools[2].get();
    m_activeTool->OnActivate();
}

bool ViewportInteractionManager::Update(const ViewportEvent& e, Camera& camera, SceneManager& scene, ViewportContext& ctx) {
    bool consumed = false;
    // 1. Input Handling (Chain of Responsibility)
    for (auto& tool : m_tools) {
        if (tool->HandleInput(e, camera, scene, ctx)) {
            consumed = true;
            break; 
        }
    }

    // 2. Logic Update (All tools)
    for (auto& tool : m_tools) {
        tool->Update(e, camera, scene, ctx);
    }
    return consumed;
}

void ViewportInteractionManager::RenderOverlays(ImDrawList* dl, const ViewportEvent& e, Camera& camera, ViewportContext& ctx) {
    for (auto& tool : m_tools) {
        tool->RenderOverlays(dl, e, camera, ctx);
    }
}

void ViewportInteractionManager::SetActiveTool(const std::string& name) {
    auto it = std::find_if(m_tools.begin(), m_tools.end(), [&](const std::unique_ptr<IViewportTool>& t) {
        return std::string(t->GetToolName()) == name;
    });

    if (it != m_tools.end()) {
        if (m_activeTool) m_activeTool->OnDeactivate();
        m_activeTool = it->get();
        m_activeTool->OnActivate();
    }
}
