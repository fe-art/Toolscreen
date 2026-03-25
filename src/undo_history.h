#pragma once

#include "config_diff.h"
#include <vector>

struct UndoEntry {
    Config snapshot;           // Full config state from BEFORE the change
    UndoLocationInfo location; // Where the change was, for navigation
};

class UndoHistory {
public:
    static constexpr size_t MAX_ENTRIES = 100;

    // Push a pre-change config snapshot onto the undo stack.
    // Clears the redo stack (any new edit invalidates redo history).
    void Push(Config beforeState, UndoLocationInfo location);

    // Undo: restores the top of the undo stack into currentConfig.
    // The current config is moved to the redo stack.
    // outLocation receives the navigation target.
    // Returns false if the undo stack is empty.
    bool Undo(Config& currentConfig, UndoLocationInfo& outLocation);

    // Redo: restores the top of the redo stack into currentConfig.
    // The current config is moved to the undo stack.
    // Returns false if the redo stack is empty.
    bool Redo(Config& currentConfig, UndoLocationInfo& outLocation);

    bool CanUndo() const { return !m_undoStack.empty(); }
    bool CanRedo() const { return !m_redoStack.empty(); }
    size_t UndoCount() const { return m_undoStack.size(); }
    size_t RedoCount() const { return m_redoStack.size(); }

private:
    std::vector<UndoEntry> m_undoStack; // back = most recent
    std::vector<UndoEntry> m_redoStack; // back = most recent
};
