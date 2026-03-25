#pragma once

#include "gui.h"

struct UndoLocationInfo {
    std::string tabName;     // "Mirrors", "Modes", etc. Empty = no navigation.
    int vectorIndex = -1;    // e.g. mirrors[2], -1 if scalar field
    std::string sectionId;   // field group identifier within the tab

    bool empty() const { return tabName.empty(); }
};

// Returns true if the two configs are identical.
bool ConfigEqual(const Config& a, const Config& b);

// Returns location info for the first difference found.
// Returns an empty UndoLocationInfo (tabName == "") if configs are equal.
UndoLocationInfo ConfigDiffLocation(const Config& a, const Config& b);
