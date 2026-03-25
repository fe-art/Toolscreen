#include "undo_history.h"

void UndoHistory::Push(Config beforeState, UndoLocationInfo location) {
    m_redoStack.clear();

    m_undoStack.push_back({std::move(beforeState), std::move(location)});

    if (m_undoStack.size() > MAX_ENTRIES) {
        m_undoStack.erase(m_undoStack.begin());
    }
}

bool UndoHistory::Undo(Config& currentConfig, UndoLocationInfo& outLocation) {
    if (m_undoStack.empty()) return false;

    UndoEntry entry = std::move(m_undoStack.back());
    m_undoStack.pop_back();

    outLocation = entry.location;

    // Save current state to redo stack so the user can redo
    m_redoStack.push_back({std::move(currentConfig), outLocation});

    // Restore the old state
    currentConfig = std::move(entry.snapshot);

    return true;
}

bool UndoHistory::Redo(Config& currentConfig, UndoLocationInfo& outLocation) {
    if (m_redoStack.empty()) return false;

    UndoEntry entry = std::move(m_redoStack.back());
    m_redoStack.pop_back();

    outLocation = entry.location;

    // Save current state to undo stack
    m_undoStack.push_back({std::move(currentConfig), outLocation});

    // Apply the redo state
    currentConfig = std::move(entry.snapshot);

    return true;
}
