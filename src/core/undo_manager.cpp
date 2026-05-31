#include "undo_manager.hpp"
#include "scene_manager.hpp"
#include "scene_object.hpp"
#include "mesh.hpp"
#include "../ui/app_console.hpp"

// ---------------------------------------------------------------------------
// AlignCommand Implementation
// ---------------------------------------------------------------------------
AlignCommand::AlignCommand(const std::string& objId, 
                           const Transform& oldT, const Transform& newT,
                           const std::vector<ConstraintLine>& oldC, const std::vector<ConstraintLine>& newC)
    : m_objectId(objId), m_oldTransform(oldT), m_newTransform(newT),
      m_oldConstraints(oldC), m_newConstraints(newC)
{}

void AlignCommand::Undo(SceneManager& scene, AlignmentState& alignState) {
    SceneObject* obj = scene.GetObjectById(m_objectId);
    if (obj) {
        obj->transform = m_oldTransform;
        obj->transform.Rebuild();
        if (obj->mesh) obj->mesh->RecalculateBounds();
        
        // Restore UI constraints
        alignState.activeConstraints = m_oldConstraints;
        
        scene.isDirty = true;
        AppConsole::Log("[Undo] Restored alignment state for: " + obj->name);
    } else {
        AppConsole::Log("[Undo] Warning: Object " + m_objectId + " not found. Skipping.", {1, 0.5f, 0, 1});
    }
}

void AlignCommand::Redo(SceneManager& scene, AlignmentState& alignState) {
    SceneObject* obj = scene.GetObjectById(m_objectId);
    if (obj) {
        obj->transform = m_newTransform;
        obj->transform.Rebuild();
        if (obj->mesh) obj->mesh->RecalculateBounds();
        
        // Restore UI constraints
        alignState.activeConstraints = m_newConstraints;
        
        scene.isDirty = true;
        AppConsole::Log("[Redo] Applied alignment state for: " + obj->name);
    } else {
        AppConsole::Log("[Redo] Warning: Object " + m_objectId + " not found. Skipping.", {1, 0.5f, 0, 1});
    }
}

// ---------------------------------------------------------------------------
// UndoManager Implementation
// ---------------------------------------------------------------------------
void UndoManager::PushCommand(std::unique_ptr<ICommand> cmd) {
    m_undoStack.push_back(std::move(cmd));
    m_redoStack.clear(); // Standard behavior: clear redo on new action
    
    if (m_undoStack.size() > MAX_STACK_SIZE) {
        m_undoStack.erase(m_undoStack.begin());
    }
}

void UndoManager::Undo(SceneManager& scene, AlignmentState& alignState) {
    if (m_undoStack.empty()) return;
    
    auto cmd = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    
    cmd->Undo(scene, alignState);
    m_redoStack.push_back(std::move(cmd));
}

void UndoManager::Redo(SceneManager& scene, AlignmentState& alignState) {
    if (m_redoStack.empty()) return;
    
    auto cmd = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    
    cmd->Redo(scene, alignState);
    m_undoStack.push_back(std::move(cmd));
}

void UndoManager::Clear() {
    m_undoStack.clear();
    m_redoStack.clear();
}
