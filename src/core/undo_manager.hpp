#pragma once
#include <vector>
#include <memory>
#include <string>
#include "transform.hpp"
#include "../ui/ui_state.hpp"

class SceneManager;

// ---------------------------------------------------------------------------
// ICommand Interface
// ---------------------------------------------------------------------------
class ICommand {
public:
    virtual ~ICommand() = default;
    virtual void Undo(SceneManager& scene, AlignmentState& alignState) = 0;
    virtual void Redo(SceneManager& scene, AlignmentState& alignState) = 0;
};

// ---------------------------------------------------------------------------
// AlignCommand: Stores alignment transformations and constraints for Undo/Redo
// ---------------------------------------------------------------------------
class AlignCommand : public ICommand {
public:
    AlignCommand(const std::string& objId, 
                 const Transform& oldT, const Transform& newT,
                 const std::vector<ConstraintLine>& oldC, const std::vector<ConstraintLine>& newC);

    void Undo(SceneManager& scene, AlignmentState& alignState) override;
    void Redo(SceneManager& scene, AlignmentState& alignState) override;

private:
    std::string m_objectId;
    Transform m_oldTransform;
    Transform m_newTransform;
    std::vector<ConstraintLine> m_oldConstraints;
    std::vector<ConstraintLine> m_newConstraints;
};

// ---------------------------------------------------------------------------
// UndoManager: Centralized command stack management
// ---------------------------------------------------------------------------
class UndoManager {
public:
    UndoManager() = default;

    void PushCommand(std::unique_ptr<ICommand> cmd);
    void Undo(SceneManager& scene, AlignmentState& alignState);
    void Redo(SceneManager& scene, AlignmentState& alignState);

    bool CanUndo() const { return !m_undoStack.empty(); }
    bool CanRedo() const { return !m_redoStack.empty(); }

    void Clear();

private:
    std::vector<std::unique_ptr<ICommand>> m_undoStack;
    std::vector<std::unique_ptr<ICommand>> m_redoStack;
    const size_t MAX_STACK_SIZE = 50;
};
