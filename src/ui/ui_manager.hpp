#pragma once
#include <imgui.h>
#include <vector>
#include <string>
#include <memory>
#include <glm/glm.hpp>
#include "../core/math_utils.hpp"
#include "../core/datum_entity.hpp"
#include <future>

#include "ui_state.hpp"
#include "../core/undo_manager.hpp"
#include "viewport/ui_viewport_interaction.hpp"

// Forward declarations
class SceneObject;
class Camera;
class SceneManager;

class UIManager {
public:
    UIManager();
    ~UIManager();

    static void ApplyStyleDark();

    void DrawUI(SceneManager& scene,
                Camera& camera,
                unsigned int viewportTextureID);

    ImVec2 GetViewportSize() const { return viewportSize; }
    ImVec2 GetViewportPos()  const { return viewportPos;  }
    bool   IsInteracting()   const { return io_WantsCaptureMouse; }

    // Called by drag-and-drop or any external code to import a file by path
    void ImportFromPath(SceneManager& scene, Camera& camera, const std::string& path);

private:
    std::vector<std::future<std::shared_ptr<Mesh>>> meshLoadTasks;
    std::vector<std::future<void>>                  analysisTasks;
    void ProcessBackgroundTasks(SceneManager& scene, Camera& camera);

    // --- Panels ---
    void DrawOutliner        (SceneManager& scene);
    void DrawTransformPanel  (SceneObject* obj, SceneManager& scene);
    void DrawMaterialPanel   (SceneObject* obj);
    void DrawAnalysisPanel   (SceneManager& scene);
    void DrawAlignmentPanel  (SceneManager& scene);
    void DrawMeasurementPanel(SceneManager& scene);
    void DrawScalingPanel    (SceneManager& scene);
    void DrawGeometryPanel   (SceneObject* selectedObject);
    void DrawToolsPanel      (SceneManager& scene);
    void DrawPropertiesPanel (SceneManager& scene);
    void DrawViewport        (unsigned int textureID, Camera& camera, SceneManager& scene);
    void DrawStatusBar       (SceneManager& scene);
    void DrawSettingsMenuItems();

    // Import from file dialog (extracted from menu block for clean separation of concerns)
    void ImportSTL (SceneManager& scene, Camera& camera);
    void ExportSTL (SceneManager& scene);

    static void ApplyStyleDark_Impl();
    static void ApplyStylePremium_Impl();

    // --- Snapping Engine (Unification) ---
    void UpdateHoverSnapState(ImVec2 vpPos, ImVec2 vpSize, Camera& camera, SceneManager& scene, bool isHovered);
    void DrawAlignmentAxes(ImDrawList* drawList, ImVec2 vpPos, ImVec2 vpSize, Camera& camera);
    void DrawPieceUCS(ImDrawList* drawList, ImVec2 vpPos, ImVec2 vpSize, Camera& camera, SceneManager& scene);

    // -----------------------------------------------------------------------
    // UCS / Gizmo sub-widgets  (extracted from DrawViewport for clarity)
    // -----------------------------------------------------------------------

    // Floating local-UCS indicator (top-right corner).
    // Shows piece LOCAL axes when activePiece != nullptr, world axes otherwise.
    // Clicking and dragging it docks the translate gizmo onto the active piece.
    void DrawUCSWidget(ImVec2 vpPos, ImVec2 vpSize, Camera& camera,
                       SceneManager& scene, SceneObject* activePiece);

    // ImGuizmo translate gizmo anchored to the LOCAL ORIGIN of the active piece(s).
    // Applies the computed delta to ALL pieces with isActive == true.
    // Returns true if any piece was moved.
    bool DrawTransformGizmo(ImVec2 vpPos, ImVec2 vpSize, Camera& camera,
                            SceneManager& scene, bool blockInteraction);

    // -----------------------------------------------------------------------
    // State helpers
    // -----------------------------------------------------------------------

    // Anchors the gizmo to a specific piece by ID.
    // Handles state updates (isGizmoOnObject, gizmoAnchorId) and piece activation.
    void DockToPiece(SceneManager& scene, const std::string& id);

    // Fully resets gizmo docking state.
    // Called on any add/remove from the scene, or on first import.
    void ResetGizmoState();

    // Invalidates alignment and measurement state that depend on a deleted object.
    void CleanStateForDeletedObject(const std::string& deletedId);

    // Rebuilds persistent GPU buffers for measured geometries
    void RebuildMeasurementHighlights(SceneManager& scene);

    // -----------------------------------------------------------------------
    // UI State
    // -----------------------------------------------------------------------
    AlignmentState   alignState;
    MeasurementState measureState;
    HoverSnapState   hoverSnapState;

    // -----------------------------------------------------------------------
    // Gizmo State
    // -----------------------------------------------------------------------
    // Anchored by PIECE ID (string), not raw pointer → dangling-safe.
    bool        isGizmoOnObject  = false;
    std::string gizmoAnchorId;       // ID of the piece the gizmo is docked to
    bool        isDraggingToDock = false;

    // --- Gizmo Mode & Space ---
    int         gizmoMode        = 0; // 0=Translate, 1=Rotate, 2=Scale
    int         gizmoSpace       = 1; // 0=Local, 1=World

    // --- Undo/Redo ---
    UndoManager     undoManager;

    // -----------------------------------------------------------------------
    // General
    // -----------------------------------------------------------------------
    ViewportInteractionManager interactionManager;
    ImVec2 viewportPos         = {0,   0};
    ImVec2 viewportSize        = {800, 600};
    bool   io_WantsCaptureMouse = false;
    bool   layoutInitialized   = false;
};
