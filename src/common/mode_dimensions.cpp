#include "mode_dimensions.h"

#include "expression_parser.h"

#include "gui/gui.h"
#include "runtime/logic_thread.h"

#include <cmath>

bool SyncPreemptiveModeFromEyeZoom(Config& config) {
    ModeConfig* eyezoomMode = nullptr;
    ModeConfig* preemptiveMode = nullptr;
    for (auto& mode : config.modes) {
        if (!eyezoomMode && mode.id == "EyeZoom") { eyezoomMode = &mode; }
        if (!preemptiveMode && mode.id == "Preemptive") { preemptiveMode = &mode; }
    }

    if (!eyezoomMode || !preemptiveMode) { return false; }

    bool changed = false;
    if (preemptiveMode->width != eyezoomMode->width) {
        preemptiveMode->width = eyezoomMode->width;
        changed = true;
    }
    if (preemptiveMode->height != eyezoomMode->height) {
        preemptiveMode->height = eyezoomMode->height;
        changed = true;
    }

    const int syncedManualWidth = (eyezoomMode->manualWidth > 0) ? eyezoomMode->manualWidth : eyezoomMode->width;
    const int syncedManualHeight = (eyezoomMode->manualHeight > 0) ? eyezoomMode->manualHeight : eyezoomMode->height;
    if (preemptiveMode->manualWidth != syncedManualWidth) {
        preemptiveMode->manualWidth = syncedManualWidth;
        changed = true;
    }
    if (preemptiveMode->manualHeight != syncedManualHeight) {
        preemptiveMode->manualHeight = syncedManualHeight;
        changed = true;
    }
    if (preemptiveMode->useRelativeSize) {
        preemptiveMode->useRelativeSize = false;
        changed = true;
    }
    if (preemptiveMode->relativeWidth != -1.0f) {
        preemptiveMode->relativeWidth = -1.0f;
        changed = true;
    }
    if (preemptiveMode->relativeHeight != -1.0f) {
        preemptiveMode->relativeHeight = -1.0f;
        changed = true;
    }
    if (!preemptiveMode->widthExpr.empty()) {
        preemptiveMode->widthExpr.clear();
        changed = true;
    }
    if (!preemptiveMode->heightExpr.empty()) {
        preemptiveMode->heightExpr.clear();
        changed = true;
    }

    return changed;
}

void RecalculateModeDimensions(Config& config, int screenW, int screenH) {
    if (screenW < 1) screenW = 1;
    if (screenH < 1) screenH = 1;

    for (auto& mode : config.modes) {
        if (mode.id == "Fullscreen") {
            // Fullscreen is always defined by the live game-window client size.
            // Keep width/height in sync so all consumers see the latest dimensions,
            // not just the stretch rect.
            mode.width = screenW;
            mode.height = screenH;

            mode.useRelativeSize = true;
            mode.relativeWidth = 1.0f;
            mode.relativeHeight = 1.0f;

            mode.stretch.enabled = true;
            mode.stretch.x = 0;
            mode.stretch.y = 0;
            mode.stretch.width = screenW;
            mode.stretch.height = screenH;
        }

        if (mode.id == "Preemptive") {
            mode.useRelativeSize = false;
            mode.relativeWidth = -1.0f;
            mode.relativeHeight = -1.0f;
            mode.widthExpr.clear();
            mode.heightExpr.clear();
        }

        const bool expressionAllowed = mode.id != "Fullscreen" && mode.id != "Preemptive";
        const bool widthIsRelative = expressionAllowed && mode.useRelativeSize && mode.relativeWidth >= 0.0f && mode.relativeWidth <= 1.0f;
        const bool heightIsRelative = expressionAllowed && mode.useRelativeSize && mode.relativeHeight >= 0.0f && mode.relativeHeight <= 1.0f;
        const bool widthUsesExpression = expressionAllowed && !widthIsRelative && !mode.widthExpr.empty();
        const bool heightUsesExpression = expressionAllowed && !heightIsRelative && !mode.heightExpr.empty();

        if (widthIsRelative) {
            int newWidth = static_cast<int>(std::lround(mode.relativeWidth * static_cast<float>(screenW)));
            if (newWidth < 1) newWidth = 1;
            mode.width = newWidth;
            if (mode.manualWidth < 1) {
                mode.manualWidth = newWidth;
            }
        }
        if (heightIsRelative) {
            int newHeight = static_cast<int>(std::lround(mode.relativeHeight * static_cast<float>(screenH)));
            if (newHeight < 1) newHeight = 1;
            mode.height = newHeight;
            if (mode.manualHeight < 1) {
                mode.manualHeight = newHeight;
            }
        }

        if (widthUsesExpression) {
            int newWidth = EvaluateExpression(mode.widthExpr, screenW, screenH, mode.width);
            if (newWidth > 0) {
                mode.width = newWidth;
            }
        }
        if (heightUsesExpression) {
            int newHeight = EvaluateExpression(mode.heightExpr, screenW, screenH, mode.height);
            if (newHeight > 0) {
                mode.height = newHeight;
            }
        }

        if (mode.id == "Thin" && mode.width < 330) { mode.width = 330; }
    }

    SyncPreemptiveModeFromEyeZoom(config);
}

void RecalculateModeDimensions() {
    int screenW = GetCachedWindowWidth();
    int screenH = GetCachedWindowHeight();
    RecalculateModeDimensions(g_config, screenW, screenH);
}