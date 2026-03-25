#include "config_diff.h"

// ── Color ────────────────────────────────────────────────────────────────────

static bool operator==(const Color& a, const Color& b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}
static bool operator!=(const Color& a, const Color& b) { return !(a == b); }

// ── GradientColorStop ────────────────────────────────────────────────────────

static bool operator==(const GradientColorStop& a, const GradientColorStop& b) {
    return a.color == b.color && a.position == b.position;
}

// ── BackgroundConfig ─────────────────────────────────────────────────────────

static bool operator==(const BackgroundConfig& a, const BackgroundConfig& b) {
    return a.selectedMode == b.selectedMode
        && a.image == b.image
        && a.color == b.color
        && a.gradientStops == b.gradientStops
        && a.gradientAngle == b.gradientAngle
        && a.gradientAnimation == b.gradientAnimation
        && a.gradientAnimationSpeed == b.gradientAnimationSpeed
        && a.gradientColorFade == b.gradientColorFade;
}

// ── MirrorCaptureConfig ──────────────────────────────────────────────────────

static bool operator==(const MirrorCaptureConfig& a, const MirrorCaptureConfig& b) {
    return a.x == b.x && a.y == b.y && a.relativeTo == b.relativeTo;
}

// ── MirrorRenderConfig ───────────────────────────────────────────────────────

static bool operator==(const MirrorRenderConfig& a, const MirrorRenderConfig& b) {
    return a.x == b.x && a.y == b.y
        && a.useRelativePosition == b.useRelativePosition
        && a.relativeX == b.relativeX && a.relativeY == b.relativeY
        && a.scale == b.scale
        && a.separateScale == b.separateScale
        && a.scaleX == b.scaleX && a.scaleY == b.scaleY
        && a.relativeTo == b.relativeTo;
}

// ── MirrorColors ─────────────────────────────────────────────────────────────

static bool operator==(const MirrorColors& a, const MirrorColors& b) {
    return a.targetColors == b.targetColors
        && a.output == b.output
        && a.border == b.border;
}

// ── MirrorBorderConfig ───────────────────────────────────────────────────────

static bool operator==(const MirrorBorderConfig& a, const MirrorBorderConfig& b) {
    return a.type == b.type
        && a.dynamicThickness == b.dynamicThickness
        && a.staticShape == b.staticShape
        && a.staticColor == b.staticColor
        && a.staticThickness == b.staticThickness
        && a.staticRadius == b.staticRadius
        && a.staticOffsetX == b.staticOffsetX
        && a.staticOffsetY == b.staticOffsetY
        && a.staticWidth == b.staticWidth
        && a.staticHeight == b.staticHeight;
}

// ── MirrorConfig ─────────────────────────────────────────────────────────────

static bool operator==(const MirrorConfig& a, const MirrorConfig& b) {
    return a.name == b.name
        && a.captureWidth == b.captureWidth
        && a.captureHeight == b.captureHeight
        && a.input == b.input
        && a.output == b.output
        && a.colors == b.colors
        && a.colorSensitivity == b.colorSensitivity
        && a.border == b.border
        && a.fps == b.fps
        && a.opacity == b.opacity
        && a.rawOutput == b.rawOutput
        && a.colorPassthrough == b.colorPassthrough
        && a.onlyOnMyScreen == b.onlyOnMyScreen;
}

// ── MirrorGroupItem ──────────────────────────────────────────────────────────

static bool operator==(const MirrorGroupItem& a, const MirrorGroupItem& b) {
    return a.mirrorId == b.mirrorId
        && a.enabled == b.enabled
        && a.widthPercent == b.widthPercent
        && a.heightPercent == b.heightPercent
        && a.offsetX == b.offsetX
        && a.offsetY == b.offsetY;
}

// ── MirrorGroupConfig ────────────────────────────────────────────────────────

static bool operator==(const MirrorGroupConfig& a, const MirrorGroupConfig& b) {
    return a.name == b.name
        && a.output == b.output
        && a.mirrors == b.mirrors;
}

// ── ImageBackgroundConfig ────────────────────────────────────────────────────

static bool operator==(const ImageBackgroundConfig& a, const ImageBackgroundConfig& b) {
    return a.enabled == b.enabled
        && a.color == b.color
        && a.opacity == b.opacity;
}

// ── StretchConfig ────────────────────────────────────────────────────────────

static bool operator==(const StretchConfig& a, const StretchConfig& b) {
    return a.enabled == b.enabled
        && a.width == b.width && a.height == b.height
        && a.x == b.x && a.y == b.y
        && a.widthExpr == b.widthExpr && a.heightExpr == b.heightExpr
        && a.xExpr == b.xExpr && a.yExpr == b.yExpr;
}

// ── BorderConfig ─────────────────────────────────────────────────────────────

static bool operator==(const BorderConfig& a, const BorderConfig& b) {
    return a.enabled == b.enabled
        && a.color == b.color
        && a.width == b.width
        && a.radius == b.radius;
}

// ── ColorKeyConfig ───────────────────────────────────────────────────────────

static bool operator==(const ColorKeyConfig& a, const ColorKeyConfig& b) {
    return a.color == b.color && a.sensitivity == b.sensitivity;
}

// ── ImageConfig ──────────────────────────────────────────────────────────────

static bool operator==(const ImageConfig& a, const ImageConfig& b) {
    return a.name == b.name
        && a.path == b.path
        && a.x == b.x && a.y == b.y
        && a.scale == b.scale
        && a.relativeTo == b.relativeTo
        && a.crop_top == b.crop_top && a.crop_bottom == b.crop_bottom
        && a.crop_left == b.crop_left && a.crop_right == b.crop_right
        && a.enableColorKey == b.enableColorKey
        && a.colorKeys == b.colorKeys
        && a.colorKey == b.colorKey
        && a.colorKeySensitivity == b.colorKeySensitivity
        && a.opacity == b.opacity
        && a.background == b.background
        && a.pixelatedScaling == b.pixelatedScaling
        && a.onlyOnMyScreen == b.onlyOnMyScreen
        && a.border == b.border;
}

// ── WindowOverlayConfig ──────────────────────────────────────────────────────

static bool operator==(const WindowOverlayConfig& a, const WindowOverlayConfig& b) {
    return a.name == b.name
        && a.windowTitle == b.windowTitle
        && a.windowClass == b.windowClass
        && a.executableName == b.executableName
        && a.windowMatchPriority == b.windowMatchPriority
        && a.x == b.x && a.y == b.y
        && a.scale == b.scale
        && a.relativeTo == b.relativeTo
        && a.crop_top == b.crop_top && a.crop_bottom == b.crop_bottom
        && a.crop_left == b.crop_left && a.crop_right == b.crop_right
        && a.enableColorKey == b.enableColorKey
        && a.colorKeys == b.colorKeys
        && a.colorKey == b.colorKey
        && a.colorKeySensitivity == b.colorKeySensitivity
        && a.opacity == b.opacity
        && a.background == b.background
        && a.pixelatedScaling == b.pixelatedScaling
        && a.onlyOnMyScreen == b.onlyOnMyScreen
        && a.fps == b.fps
        && a.searchInterval == b.searchInterval
        && a.captureMethod == b.captureMethod
        && a.enableInteraction == b.enableInteraction
        && a.border == b.border;
}

// ── ModeConfig ───────────────────────────────────────────────────────────────

static bool operator==(const ModeConfig& a, const ModeConfig& b) {
    return a.id == b.id
        && a.width == b.width && a.height == b.height
        && a.useRelativeSize == b.useRelativeSize
        && a.relativeWidth == b.relativeWidth && a.relativeHeight == b.relativeHeight
        && a.widthExpr == b.widthExpr && a.heightExpr == b.heightExpr
        && a.background == b.background
        && a.mirrorIds == b.mirrorIds
        && a.mirrorGroupIds == b.mirrorGroupIds
        && a.imageIds == b.imageIds
        && a.windowOverlayIds == b.windowOverlayIds
        && a.stretch == b.stretch
        && a.gameTransition == b.gameTransition
        && a.overlayTransition == b.overlayTransition
        && a.backgroundTransition == b.backgroundTransition
        && a.transitionDurationMs == b.transitionDurationMs
        && a.easeInPower == b.easeInPower
        && a.easeOutPower == b.easeOutPower
        && a.bounceCount == b.bounceCount
        && a.bounceIntensity == b.bounceIntensity
        && a.bounceDurationMs == b.bounceDurationMs
        && a.relativeStretching == b.relativeStretching
        && a.skipAnimateX == b.skipAnimateX
        && a.skipAnimateY == b.skipAnimateY
        && a.border == b.border
        && a.sensitivityOverrideEnabled == b.sensitivityOverrideEnabled
        && a.modeSensitivity == b.modeSensitivity
        && a.separateXYSensitivity == b.separateXYSensitivity
        && a.modeSensitivityX == b.modeSensitivityX
        && a.modeSensitivityY == b.modeSensitivityY
        && a.slideMirrorsIn == b.slideMirrorsIn;
}

// ── HotkeyConditions ─────────────────────────────────────────────────────────

static bool operator==(const HotkeyConditions& a, const HotkeyConditions& b) {
    return a.gameState == b.gameState && a.exclusions == b.exclusions;
}

// ── AltSecondaryMode ─────────────────────────────────────────────────────────

static bool operator==(const AltSecondaryMode& a, const AltSecondaryMode& b) {
    return a.keys == b.keys && a.mode == b.mode;
}

// ── HotkeyConfig ─────────────────────────────────────────────────────────────

static bool operator==(const HotkeyConfig& a, const HotkeyConfig& b) {
    return a.keys == b.keys
        && a.mainMode == b.mainMode
        && a.secondaryMode == b.secondaryMode
        && a.altSecondaryModes == b.altSecondaryModes
        && a.conditions == b.conditions
        && a.debounce == b.debounce
        && a.triggerOnRelease == b.triggerOnRelease
        && a.blockKeyFromGame == b.blockKeyFromGame
        && a.allowExitToFullscreenRegardlessOfGameState == b.allowExitToFullscreenRegardlessOfGameState;
}

// ── SensitivityHotkeyConfig ──────────────────────────────────────────────────

static bool operator==(const SensitivityHotkeyConfig& a, const SensitivityHotkeyConfig& b) {
    return a.keys == b.keys
        && a.sensitivity == b.sensitivity
        && a.separateXY == b.separateXY
        && a.sensitivityX == b.sensitivityX
        && a.sensitivityY == b.sensitivityY
        && a.toggle == b.toggle
        && a.conditions == b.conditions
        && a.debounce == b.debounce;
}

// ── DebugGlobalConfig ────────────────────────────────────────────────────────

static bool operator==(const DebugGlobalConfig& a, const DebugGlobalConfig& b) {
    return a.showPerformanceOverlay == b.showPerformanceOverlay
        && a.showProfiler == b.showProfiler
        && a.profilerScale == b.profilerScale
        && a.showHotkeyDebug == b.showHotkeyDebug
        && a.fakeCursor == b.fakeCursor
        && a.showTextureGrid == b.showTextureGrid
        && a.delayRenderingUntilFinished == b.delayRenderingUntilFinished
        && a.delayRenderingUntilBlitted == b.delayRenderingUntilBlitted
        && a.virtualCameraEnabled == b.virtualCameraEnabled
        && a.virtualCameraFps == b.virtualCameraFps
        && a.logModeSwitch == b.logModeSwitch
        && a.logAnimation == b.logAnimation
        && a.logHotkey == b.logHotkey
        && a.logObs == b.logObs
        && a.logWindowOverlay == b.logWindowOverlay
        && a.logFileMonitor == b.logFileMonitor
        && a.logImageMonitor == b.logImageMonitor
        && a.logPerformance == b.logPerformance
        && a.logTextureOps == b.logTextureOps
        && a.logGui == b.logGui
        && a.logInit == b.logInit
        && a.logCursorTextures == b.logCursorTextures;
}

// ── CursorConfig ─────────────────────────────────────────────────────────────

static bool operator==(const CursorConfig& a, const CursorConfig& b) {
    return a.cursorName == b.cursorName && a.cursorSize == b.cursorSize;
}

// ── CursorsConfig ────────────────────────────────────────────────────────────

static bool operator==(const CursorsConfig& a, const CursorsConfig& b) {
    return a.enabled == b.enabled
        && a.title == b.title
        && a.wall == b.wall
        && a.ingame == b.ingame;
}

// ── EyeZoomConfig ────────────────────────────────────────────────────────────

static bool operator==(const EyeZoomConfig& a, const EyeZoomConfig& b) {
    return a.cloneWidth == b.cloneWidth
        && a.overlayWidth == b.overlayWidth
        && a.cloneHeight == b.cloneHeight
        && a.stretchWidth == b.stretchWidth
        && a.windowWidth == b.windowWidth
        && a.windowHeight == b.windowHeight
        && a.horizontalMargin == b.horizontalMargin
        && a.verticalMargin == b.verticalMargin
        && a.useCustomPosition == b.useCustomPosition
        && a.positionX == b.positionX
        && a.positionY == b.positionY
        && a.autoFontSize == b.autoFontSize
        && a.textFontSize == b.textFontSize
        && a.textFontPath == b.textFontPath
        && a.rectHeight == b.rectHeight
        && a.linkRectToFont == b.linkRectToFont
        && a.gridColor1 == b.gridColor1
        && a.gridColor1Opacity == b.gridColor1Opacity
        && a.gridColor2 == b.gridColor2
        && a.gridColor2Opacity == b.gridColor2Opacity
        && a.centerLineColor == b.centerLineColor
        && a.centerLineColorOpacity == b.centerLineColorOpacity
        && a.textColor == b.textColor
        && a.textColorOpacity == b.textColorOpacity
        && a.slideZoomIn == b.slideZoomIn
        && a.slideMirrorsIn == b.slideMirrorsIn;
}

// ── KeyRebind ────────────────────────────────────────────────────────────────

static bool operator==(const KeyRebind& a, const KeyRebind& b) {
    return a.fromKey == b.fromKey
        && a.toKey == b.toKey
        && a.enabled == b.enabled
        && a.useCustomOutput == b.useCustomOutput
        && a.customOutputVK == b.customOutputVK
        && a.customOutputUnicode == b.customOutputUnicode
        && a.customOutputScanCode == b.customOutputScanCode;
}

// ── KeyRebindsConfig ─────────────────────────────────────────────────────────

static bool operator==(const KeyRebindsConfig& a, const KeyRebindsConfig& b) {
    return a.enabled == b.enabled && a.rebinds == b.rebinds;
}

// ── AppearanceConfig ─────────────────────────────────────────────────────────

static bool operator==(const AppearanceConfig& a, const AppearanceConfig& b) {
    return a.theme == b.theme && a.customColors == b.customColors;
}

// ═════════════════════════════════════════════════════════════════════════════
// ConfigEqual
// ═════════════════════════════════════════════════════════════════════════════

bool ConfigEqual(const Config& a, const Config& b) {
    return ConfigDiffLocation(a, b).empty();
}

// ═════════════════════════════════════════════════════════════════════════════
// ConfigDiffLocation — returns the location of the first difference found.
// ═════════════════════════════════════════════════════════════════════════════

// Helper: compare two vectors element-by-element, return index of first diff
// or -2 if sizes differ.
template<typename T>
static int VectorDiffIndex(const std::vector<T>& a, const std::vector<T>& b) {
    if (a.size() != b.size()) return -2; // size changed (add/delete)
    for (size_t i = 0; i < a.size(); ++i) {
        if (!(a[i] == b[i])) return static_cast<int>(i);
    }
    return -1; // equal
}

UndoLocationInfo ConfigDiffLocation(const Config& a, const Config& b) {
    // ── Mirrors ──────────────────────────────────────────────────────────
    {
        int idx = VectorDiffIndex(a.mirrors, b.mirrors);
        if (idx >= 0) // specific element differs
            return {"Mirrors", idx, "mirror"};
        if (idx == -2) // size changed
            return {"Mirrors", -1, "mirror"};
    }

    // ── Mirror Groups ────────────────────────────────────────────────────
    {
        int idx = VectorDiffIndex(a.mirrorGroups, b.mirrorGroups);
        if (idx >= 0) return {"Mirrors", idx, "mirrorGroup"};
        if (idx == -2) return {"Mirrors", -1, "mirrorGroup"};
    }

    // ── Images ───────────────────────────────────────────────────────────
    {
        int idx = VectorDiffIndex(a.images, b.images);
        if (idx >= 0) return {"Images", idx, "image"};
        if (idx == -2) return {"Images", -1, "image"};
    }

    // ── Window Overlays ──────────────────────────────────────────────────
    {
        int idx = VectorDiffIndex(a.windowOverlays, b.windowOverlays);
        if (idx >= 0) return {"Window Overlays", idx, "windowOverlay"};
        if (idx == -2) return {"Window Overlays", -1, "windowOverlay"};
    }

    // ── Modes ────────────────────────────────────────────────────────────
    {
        int idx = VectorDiffIndex(a.modes, b.modes);
        if (idx >= 0) return {"Modes", idx, "mode"};
        if (idx == -2) return {"Modes", -1, "mode"};
    }

    // ── Hotkeys ──────────────────────────────────────────────────────────
    {
        int idx = VectorDiffIndex(a.hotkeys, b.hotkeys);
        if (idx >= 0) return {"Hotkeys", idx, "hotkey"};
        if (idx == -2) return {"Hotkeys", -1, "hotkey"};
    }

    // ── Sensitivity Hotkeys ──────────────────────────────────────────────
    {
        int idx = VectorDiffIndex(a.sensitivityHotkeys, b.sensitivityHotkeys);
        if (idx >= 0) return {"Hotkeys", idx, "sensitivityHotkey"};
        if (idx == -2) return {"Hotkeys", -1, "sensitivityHotkey"};
    }

    // ── EyeZoom ──────────────────────────────────────────────────────────
    if (!(a.eyezoom == b.eyezoom))
        return {"Modes", -1, "eyezoom"};

    // ── Mouse / Sensitivity ──────────────────────────────────────────────
    if (a.mouseSensitivity != b.mouseSensitivity
        || a.windowsMouseSpeed != b.windowsMouseSpeed
        || a.allowCursorEscape != b.allowCursorEscape)
        return {"Inputs", -1, "mouse"};

    // ── Key Rebinds ──────────────────────────────────────────────────────
    if (!(a.keyRebinds == b.keyRebinds))
        return {"Inputs", -1, "keyboard"};

    // ── Key Repeat ───────────────────────────────────────────────────────
    if (a.keyRepeatStartDelay != b.keyRepeatStartDelay
        || a.keyRepeatDelay != b.keyRepeatDelay)
        return {"Inputs", -1, "keyboard"};

    // ── Appearance ───────────────────────────────────────────────────────
    if (!(a.appearance == b.appearance))
        return {"Appearance", -1, "appearance"};

    // ── Cursors ──────────────────────────────────────────────────────────
    if (!(a.cursors == b.cursors))
        return {"Settings", -1, "cursors"};

    // ── Debug ────────────────────────────────────────────────────────────
    if (!(a.debug == b.debug))
        return {"Settings", -1, "debug"};

    // ── Performance / FPS ────────────────────────────────────────────────
    if (a.fpsLimit != b.fpsLimit
        || a.fpsLimitSleepThreshold != b.fpsLimitSleepThreshold)
        return {"Settings", -1, "performance"};

    // ── Mirror Gamma ─────────────────────────────────────────────────────
    if (a.mirrorGammaMode != b.mirrorGammaMode)
        return {"Settings", -1, "performance"};

    // ── Hook Chaining ────────────────────────────────────────────────────
    if (a.disableHookChaining != b.disableHookChaining
        || a.hookChainingNextTarget != b.hookChainingNextTarget)
        return {"Settings", -1, "general"};

    // ── Global Hotkeys ───────────────────────────────────────────────────
    if (a.guiHotkey != b.guiHotkey
        || a.borderlessHotkey != b.borderlessHotkey
        || a.imageOverlaysHotkey != b.imageOverlaysHotkey
        || a.windowOverlaysHotkey != b.windowOverlaysHotkey)
        return {"Hotkeys", -1, "globalHotkey"};

    // ── Misc Scalars ─────────────────────────────────────────────────────
    if (a.autoBorderless != b.autoBorderless)
        return {"Settings", -1, "general"};

    if (a.fontPath != b.fontPath)
        return {"Settings", -1, "general"};

    if (a.hideAnimationsInGame != b.hideAnimationsInGame)
        return {"Settings", -1, "general"};

    if (a.defaultMode != b.defaultMode)
        return {"Modes", -1, "mode"};

    // ── Basic mode / prompts (no navigation for these) ───────────────────
    if (a.basicModeEnabled != b.basicModeEnabled
        || a.disableFullscreenPrompt != b.disableFullscreenPrompt
        || a.disableConfigurePrompt != b.disableConfigurePrompt
        || a.configVersion != b.configVersion)
        return {"", -1, ""};

    // Configs are equal
    return {};
}
