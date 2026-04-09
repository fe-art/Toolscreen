struct TestCaseDefinition {
    const char* name;
    void (*run)(TestRunMode runMode);
};

const auto& GetTestCaseDefinitions() {
    static const std::vector<TestCaseDefinition> testCases = {
        {"config-default-load", &RunConfigDefaultLoadTest},
        {"log-multi-instance-latest-suffix", &RunLogMultiInstanceLatestSuffixTest},
        {"log-header-includes-peer-info", &RunLogHeaderIncludesPeerInfoTest},
        {"log-archive-collision-handling", &RunLogArchiveCollisionHandlingTest},
        {"log-released-slot-reuse", &RunLogReleasedSlotReuseTest},
        {"config-roundtrip", &RunConfigRoundtripTest},
        {"config-roundtrip-global-settings", &RunConfigRoundtripGlobalSettingsTest},
        {"config-roundtrip-modes", &RunConfigRoundtripModesTest},
        {"config-roundtrip-mirrors", &RunConfigRoundtripMirrorsTest},
        {"config-roundtrip-mirror-groups", &RunConfigRoundtripMirrorGroupsTest},
        {"config-roundtrip-images", &RunConfigRoundtripImagesTest},
        {"config-roundtrip-window-overlays", &RunConfigRoundtripWindowOverlaysTest},
        {"config-roundtrip-browser-overlays", &RunConfigRoundtripBrowserOverlaysTest},
        {"config-roundtrip-hotkeys", &RunConfigRoundtripHotkeysTest},
        {"config-roundtrip-sensitivity-hotkeys", &RunConfigRoundtripSensitivityHotkeysTest},
        {"config-roundtrip-cursors-eyezoom", &RunConfigRoundtripCursorsAndEyeZoomTest},
        {"config-roundtrip-key-rebinds-appearance", &RunConfigRoundtripKeyRebindsAndAppearanceTest},
        {"config-roundtrip-debug-settings", &RunConfigRoundtripDebugSettingsTest},
        {"config-load-embedded-ninjabrain-presets", &RunConfigLoadEmbeddedNinjabrainPresetsTest},
        {"config-load-bundled-font-paths-normalized", &RunConfigLoadBundledFontPathsNormalizedTest},
        {"config-load-missing-required-modes", &RunConfigLoadMissingRequiredModesTest},
        {"config-load-invalid-hotkey-mode-references", &RunConfigLoadInvalidHotkeyModeReferencesTest},
        {"config-load-relative-mode-dimensions", &RunConfigLoadRelativeModeDimensionsTest},
        {"config-load-expression-mode-dimensions", &RunConfigLoadExpressionModeDimensionsTest},
        {"config-load-legacy-version-upgrade", &RunConfigLoadLegacyVersionUpgradeTest},
        {"config-load-clamp-global-values", &RunConfigLoadClampGlobalValuesTest},
        {"config-load-mode-default-dimensions-restored", &RunConfigLoadModeDefaultDimensionsRestoredTest},
        {"config-load-mode-source-lists-loaded", &RunConfigLoadModeSourceListsLoadedTest},
        {"config-load-mode-percentage-dimensions-detected", &RunConfigLoadModePercentageDimensionsDetectedTest},
        {"config-load-mode-typed-sources-ignored", &RunConfigLoadModeTypedSourcesIgnoredTest},
        {"config-load-empty-main-hotkey-fallback", &RunConfigLoadEmptyMainHotkeyFallbackTest},
        {"config-load-missing-gui-hotkey-defaulted", &RunConfigLoadMissingGuiHotkeyDefaultedTest},
        {"config-load-empty-gui-hotkey-defaulted", &RunConfigLoadEmptyGuiHotkeyDefaultedTest},
        {"config-load-legacy-mirror-gamma-migrated", &RunConfigLoadLegacyMirrorGammaMigratedTest},
        {"config-load-mirror-capture-dimensions-clamped", &RunConfigLoadMirrorCaptureDimensionsClampedTest},
        {"config-load-eyezoom-clone-width-normalized", &RunConfigLoadEyeZoomCloneWidthNormalizedTest},
        {"config-load-eyezoom-overlay-width-defaulted", &RunConfigLoadEyeZoomOverlayWidthDefaultedTest},
        {"config-load-eyezoom-overlay-width-clamped", &RunConfigLoadEyeZoomOverlayWidthClampedTest},
        {"config-load-eyezoom-legacy-margins-migrated", &RunConfigLoadEyeZoomLegacyMarginsMigratedTest},
        {"config-load-eyezoom-legacy-custom-position-migrated", &RunConfigLoadEyeZoomLegacyCustomPositionMigratedTest},
        {"config-load-eyezoom-invalid-active-overlay-reset", &RunConfigLoadEyeZoomInvalidActiveOverlayResetTest},
        {"config-load-window-overlay-crop-migrated", &RunConfigLoadWindowOverlayCropMigratedTest},
        {"config-load-window-overlay-capture-method-migrated", &RunConfigLoadWindowOverlayCaptureMethodMigratedTest},
        {"config-load-key-rebind-unicode-string-parsed", &RunConfigLoadKeyRebindUnicodeStringParsedTest},
        {"config-load-key-rebind-escaped-unicode-string-parsed", &RunConfigLoadKeyRebindEscapedUnicodeStringParsedTest},
        {"config-load-key-rebind-hex-unicode-string-parsed", &RunConfigLoadKeyRebindHexUnicodeStringParsedTest},
        {"config-load-key-rebind-invalid-unicode-defaulted", &RunConfigLoadKeyRebindInvalidUnicodeDefaultedTest},
        {"config-load-key-rebind-legacy-show-indicator-migrated", &RunConfigLoadKeyRebindLegacyShowIndicatorMigratedTest},
        {"config-load-key-rebind-shift-layer-caps-lock-parsed", &RunConfigLoadKeyRebindShiftLayerCapsLockParsedTest},
        {"config-load-key-rebind-shift-layer-caps-lock-defaulted", &RunConfigLoadKeyRebindShiftLayerCapsLockDefaultedTest},
        {"config-load-key-rebind-cursor-state-defaulted", &RunConfigLoadKeyRebindCursorStateDefaultedTest},
        {"key-rebind-runtime-full-forwarding", &RunKeyRebindRuntimeFullForwardingTest},
        {"key-rebind-runtime-split-vk-output", &RunKeyRebindRuntimeSplitVkOutputTest},
        {"key-rebind-runtime-split-unicode-output", &RunKeyRebindRuntimeSplitUnicodeOutputTest},
        {"key-rebind-runtime-shift-layer-shift-activated", &RunKeyRebindRuntimeShiftLayerShiftActivatedTest},
        {"key-rebind-runtime-shift-layer-caps-lock-activated", &RunKeyRebindRuntimeShiftLayerCapsLockActivatedTest},
        {"key-rebind-runtime-full-caps-lock-honored", &RunKeyRebindRuntimeFullCapsLockHonoredTest},
        {"key-rebind-runtime-split-caps-lock-ignored", &RunKeyRebindRuntimeSplitCapsLockIgnoredWithoutOptInTest},
        {"key-rebind-runtime-non-typable-trigger-consumes-char", &RunKeyRebindRuntimeNonTypableTriggerConsumesCharTest},
        {"key-rebind-runtime-trigger-disabled-still-types", &RunKeyRebindRuntimeTriggerDisabledStillTypesTest},
        {"key-rebind-runtime-types-disabled-still-triggers", &RunKeyRebindRuntimeTypesDisabledStillTriggersTest},
        {"key-rebind-runtime-shift-types-disabled-still-triggers", &RunKeyRebindRuntimeShiftTypesDisabledStillTriggersTest},
        {"key-rebind-runtime-mouse-source-emits-key-and-char", &RunKeyRebindRuntimeMouseSourceEmitsKeyAndCharTest},
        {"key-rebind-runtime-modifier-output-released-on-deactivate", &RunKeyRebindRuntimeModifierOutputReleasedOnDeactivateTest},
        {"key-rebind-runtime-disabled-rebind-ignored", &RunKeyRebindRuntimeDisabledRebindIgnoredTest},
        {"key-rebind-runtime-cursor-state-priority-and-fallback", &RunKeyRebindRuntimeCursorStatePriorityAndFallbackTest},
        {"key-rebind-gui-keyboard-layout-full-bind-and-trigger", &RunKeyRebindGuiKeyboardLayoutFullBindAndTriggerTest},
        {"key-rebind-gui-keyboard-layout-split-bind-and-trigger", &RunKeyRebindGuiKeyboardLayoutSplitBindAndTriggerTest},
        {"key-rebind-gui-keyboard-layout-disabled-output", &RunKeyRebindGuiKeyboardLayoutDisabledOutputTest},
        {"key-rebind-gui-keyboard-layout-split-disabled-targets", &RunKeyRebindGuiKeyboardLayoutSplitDisabledTargetsTest},
        {"key-rebind-gui-keyboard-layout-mouse-source-bind-and-trigger", &RunKeyRebindGuiKeyboardLayoutMouseSourceBindAndTriggerTest},
        {"key-rebind-gui-keyboard-layout-mouse-trigger-label-mapping", &RunKeyRebindGuiKeyboardLayoutMouseTriggerLabelMappingTest},
        {"key-rebind-gui-keyboard-layout-xbutton-trigger-label-mapping", &RunKeyRebindGuiKeyboardLayoutXButtonTriggerLabelMappingTest},
        {"key-rebind-gui-keyboard-layout-scroll-trigger-label-mapping", &RunKeyRebindGuiKeyboardLayoutScrollTriggerLabelMappingTest},
        {"key-rebind-gui-keyboard-layout-scroll-source-popup-options", &RunKeyRebindGuiKeyboardLayoutScrollSourcePopupOptionsTest},
        {"key-rebind-gui-keyboard-layout-cursor-state-override", &RunKeyRebindGuiKeyboardLayoutCursorStateOverrideTest},
        {"key-rebind-gui-keyboard-layout-add-custom-bind-button", &RunKeyRebindGuiKeyboardLayoutAddCustomBindButtonTest},
        {"key-rebind-gui-keyboard-layout-remove-custom-bind-button", &RunKeyRebindGuiKeyboardLayoutRemoveCustomBindButtonTest},
        {"key-rebind-gui-keyboard-layout-add-built-in-custom-bind-button", &RunKeyRebindGuiKeyboardLayoutAddBuiltInCustomBindButtonTest},
        {"key-rebind-gui-keyboard-layout-change-custom-input-picker", &RunKeyRebindGuiKeyboardLayoutChangeCustomInputPickerTest},
        {"key-rebind-gui-keyboard-layout-change-custom-input-capture", &RunKeyRebindGuiKeyboardLayoutChangeCustomInputCaptureTest},
        {"key-rebind-gui-keyboard-layout-full-bind-scan-picker-runtime", &RunKeyRebindGuiKeyboardLayoutFullBindScanPickerRuntimeTest},
        {"key-rebind-gui-keyboard-layout-scan-picker-filter", &RunKeyRebindGuiKeyboardLayoutScanPickerFilterTest},
        {"key-rebind-gui-keyboard-layout-scan-picker-reset-to-default", &RunKeyRebindGuiKeyboardLayoutScanPickerResetToDefaultTest},
        {"config-load-fullscreen-stretch-repaired", &RunConfigLoadFullscreenStretchRepairedTest},
        {"config-load-preemptive-sync-existing-mode", &RunConfigLoadPreemptiveSyncExistingModeTest},
        {"config-load-thin-min-width-enforced", &RunConfigLoadThinMinWidthEnforcedTest},
        {"config-load-browser-overlay-defaults", &RunConfigLoadBrowserOverlayDefaultsTest},
        {"config-load-window-overlay-defaults", &RunConfigLoadWindowOverlayDefaultsTest},
        {"config-load-image-defaults", &RunConfigLoadImageDefaultsTest},
        {"config-load-hotkey-default-flags", &RunConfigLoadHotkeyDefaultFlagsTest},
        {"config-load-sensitivity-hotkey-default-flags", &RunConfigLoadSensitivityHotkeyDefaultFlagsTest},
        {"config-load-image-color-key-default-sensitivity", &RunConfigLoadImageColorKeyDefaultSensitivityTest},
        {"config-load-window-overlay-color-key-default-sensitivity", &RunConfigLoadWindowOverlayColorKeyDefaultSensitivityTest},
        {"config-load-browser-overlay-color-key-default-sensitivity", &RunConfigLoadBrowserOverlayColorKeyDefaultSensitivityTest},
        {"mode-mirror-render-screen-anchors", &RunModeMirrorRenderScreenAnchorsTest},
        {"mode-mirror-render-viewport-anchors", &RunModeMirrorRenderViewportAnchorsTest},
        {"mode-mirror-render-screen-anchor-size-matrix", &RunModeMirrorRenderScreenAnchorSizeMatrixTest},
        {"mode-mirror-render-viewport-anchor-size-matrix", &RunModeMirrorRenderViewportAnchorSizeMatrixTest},
        {"mode-mirror-group-render", &RunModeMirrorGroupRenderTest},
        {"mode-mirror-group-relative-position-resolution", &RunModeMirrorGroupRelativePositionResolutionTest},
        {"mode-mirror-group-slide-unit-transition", &RunModeMirrorGroupSlideUnitTransitionTest},
        {"mode-window-overlay-render", &RunModeWindowOverlayRenderTest},
        {"mode-window-overlay-render-resets-blend-equation", &RunModeWindowOverlayRenderResetsBlendEquationTest},
        {"mode-window-overlay-render-unbinds-sampler", &RunModeWindowOverlayRenderUnbindsSamplerTest},
        {"mode-browser-overlay-render", &RunModeBrowserOverlayRenderTest},
        {"mode-image-overlay-render-png", &RunModeImageOverlayRenderPngTest},
        {"mode-image-overlay-render-mpeg", &RunModeImageOverlayRenderMpegTest},
        {"mode-ninjabrain-overlay-render", &RunModeNinjabrainOverlayRenderTest},
        {"rebind-indicator-renders-below-settings-gui", &RunRebindIndicatorRendersBelowSettingsGuiTest},
        {"config-error-gui", &RunConfigErrorGuiTest},
        {"profiler-unspecified-breakdown", &RunProfilerUnspecifiedBreakdownTest},
        {"settings-gui-basic", &RunSettingsGuiBasicTest},
        {"settings-gui-advanced", &RunSettingsGuiAdvancedTest},
        {"settings-search-subcategory-filtering", &RunSettingsSearchSubcategoryFilteringTest},
        {"settings-search-specific-options", &RunSettingsSearchSpecificOptionsTest},
        {"settings-search-reset-on-close", &RunSettingsSearchResetOnCloseTest},
        {"settings-tab-general-default", &RunSettingsTabGeneralDefaultTest},
        {"settings-tab-other-default", &RunSettingsTabOtherDefaultTest},
        {"settings-tab-supporters-default", &RunSettingsTabSupportersDefaultTest},
        {"settings-tab-modes-default", &RunSettingsTabModesDefaultTest},
        {"settings-tab-mirrors-default", &RunSettingsTabMirrorsDefaultTest},
        {"settings-tab-images-default", &RunSettingsTabImagesDefaultTest},
        {"settings-tab-window-overlays-default", &RunSettingsTabWindowOverlaysDefaultTest},
        {"settings-tab-browser-overlays-default", &RunSettingsTabBrowserOverlaysDefaultTest},
        {"settings-tab-hotkeys-default", &RunSettingsTabHotkeysDefaultTest},
        {"settings-tab-inputs-mouse-default", &RunSettingsTabInputsMouseDefaultTest},
        {"settings-tab-inputs-keyboard-default", &RunSettingsTabInputsKeyboardDefaultTest},
        {"settings-tab-settings-default", &RunSettingsTabSettingsDefaultTest},
        {"settings-tab-appearance-default", &RunSettingsTabAppearanceDefaultTest},
        {"settings-tab-misc-default", &RunSettingsTabMiscDefaultTest},
        {"settings-tab-general-populated", &RunSettingsTabGeneralPopulatedTest},
        {"settings-tab-other-populated", &RunSettingsTabOtherPopulatedTest},
        {"settings-tab-modes-populated", &RunSettingsTabModesPopulatedTest},
        {"settings-tab-mirrors-populated", &RunSettingsTabMirrorsPopulatedTest},
        {"settings-tab-images-populated", &RunSettingsTabImagesPopulatedTest},
        {"settings-tab-window-overlays-populated", &RunSettingsTabWindowOverlaysPopulatedTest},
        {"settings-tab-browser-overlays-populated", &RunSettingsTabBrowserOverlaysPopulatedTest},
        {"settings-tab-hotkeys-populated", &RunSettingsTabHotkeysPopulatedTest},
        {"settings-tab-inputs-mouse-populated", &RunSettingsTabInputsMousePopulatedTest},
        {"settings-tab-inputs-keyboard-populated", &RunSettingsTabInputsKeyboardPopulatedTest},
        {"settings-tab-settings-populated", &RunSettingsTabSettingsPopulatedTest},
        {"settings-tab-appearance-populated", &RunSettingsTabAppearancePopulatedTest},
        {"settings-tab-misc-populated", &RunSettingsTabMiscPopulatedTest},
        {"settings-tab-supporters-populated", &RunSettingsTabSupportersPopulatedTest},
        {"profile-apply-fields-roundtrip", &RunProfileApplyFieldsRoundtripTest},
        {"profile-profiles-config-roundtrip", &RunProfilesConfigRoundtripTest},
        {"profile-name-validation", &RunProfileNameValidationTest},
        {"profile-create-duplicate-delete", &RunProfileCreateDuplicateDeleteTest},
        {"profile-migrate", &RunProfileMigrateTest},
        {"profile-delete-guards", &RunProfileDeleteGuardsTest},
        {"profile-rename", &RunProfileRenameTest},
        {"profile-selective-switch-shared-fallback", &RunProfileSelectiveSwitchSharedFallbackTest},
        {"profile-case-insensitive-collisions", &RunProfileCaseInsensitiveCollisionTest},
        {"profile-recover-missing-metadata", &RunProfileRecoverMissingMetadataTest},
        {"profile-async-save-skip-deleted-profile", &RunProfileAsyncSaveSkipDeletedProfileTest},
        {"profile-switch-concurrent-readers", &RunProfileSwitchConcurrentReadersTest},
        {"profile-switch-concurrent-lifecycle", &RunProfileSwitchConcurrentLifecycleTest},
        {"profile-switch-concurrent-metadata-rebuild", &RunProfileSwitchConcurrentMetadataRebuildTest},
        {"profile-switch-concurrent-snapshot-writes", &RunProfileSwitchConcurrentSnapshotWritesTest},
    };

    return testCases;
}

const TestCaseDefinition* FindTestCaseDefinition(std::string_view testCaseName) {
    for (const TestCaseDefinition& testCase : GetTestCaseDefinitions()) {
        if (testCase.name == testCaseName) {
            return &testCase;
        }
    }

    return nullptr;
}

void RunTestCaseByName(std::string_view testCaseName, TestRunMode runMode = TestRunMode::Automated);
void RunAllTestCases();

void PrintTestCaseList(std::ostream& stream) {
    stream << "Available test cases:" << std::endl;
    for (const TestCaseDefinition& testCase : GetTestCaseDefinitions()) {
        stream << "  " << testCase.name << std::endl;
    }
}

int FindDefaultVisualTestCaseIndex() {
    const auto& testCases = GetTestCaseDefinitions();
    for (size_t i = 0; i < testCases.size(); ++i) {
        if (std::string_view(testCases[i].name) == kDefaultVisualTestCase) {
            return static_cast<int>(i);
        }
    }

    return 0;
}

enum class LauncherAction {
    None,
    RunSelectedAutomated,
    RunSelectedVisual,
    RunAllAutomated,
    Exit,
};

struct LauncherState {
    int selectedTestCaseIndex = FindDefaultVisualTestCaseIndex();
    std::string lastStatus = "Choose a test case and a run mode.";
};

LauncherAction RenderLauncherFrame(LauncherState& launcherState) {
    const auto& testCases = GetTestCaseDefinitions();
    LauncherAction action = LauncherAction::None;

    ImGui::SetNextWindowPos(ImVec2(40.0f, 40.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(620.0f, 420.0f), ImGuiCond_Always);

    if (ImGui::Begin("Toolscreen GUI Integration Test Runner", nullptr,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextWrapped("Select a test case, then choose whether to run it interactively or as an automated pass/fail check.");
        ImGui::Spacing();

        if (ImGui::BeginListBox("##gui-test-cases", ImVec2(-1.0f, 180.0f))) {
            for (int i = 0; i < static_cast<int>(testCases.size()); ++i) {
                const bool isSelected = launcherState.selectedTestCaseIndex == i;
                if (ImGui::Selectable(testCases[i].name, isSelected)) {
                    launcherState.selectedTestCaseIndex = i;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndListBox();
        }

        ImGui::Spacing();
        ImGui::Text("Selected: %s", testCases[launcherState.selectedTestCaseIndex].name);

        if (ImGui::Button("Run Selected Visual", ImVec2(190.0f, 0.0f))) {
            action = LauncherAction::RunSelectedVisual;
        }
        ImGui::SameLine();
        if (ImGui::Button("Run Selected Automated", ImVec2(190.0f, 0.0f))) {
            action = LauncherAction::RunSelectedAutomated;
        }
        ImGui::SameLine();
        if (ImGui::Button("Run All Automated", ImVec2(190.0f, 0.0f))) {
            action = LauncherAction::RunAllAutomated;
        }

        ImGui::Spacing();
        if (ImGui::Button("Close Launcher", ImVec2(190.0f, 0.0f))) {
            action = LauncherAction::Exit;
        }

        ImGui::Separator();
        ImGui::TextWrapped("%s", launcherState.lastStatus.c_str());
    }
    ImGui::End();

    return action;
}

std::string ExecuteLauncherAction(DummyWindow& launcherWindow, const LauncherAction action, const LauncherState& launcherState) {
    const auto& testCases = GetTestCaseDefinitions();
    const char* selectedTestCaseName = testCases[launcherState.selectedTestCaseIndex].name;

    auto runSingle = [&](const TestRunMode runMode) {
        const bool hideLauncher = runMode == TestRunMode::Visual;
        if (hideLauncher) {
            launcherWindow.Show(false);
        }

        HandleImGuiContextReset();

        try {
            RunTestCaseByName(selectedTestCaseName, runMode);
        } catch (...) {
            g_minecraftHwnd.store(launcherWindow.hwnd(), std::memory_order_release);
            launcherWindow.SetTitle("Toolscreen GUI Integration Tests - Launcher");
            if (hideLauncher) {
                launcherWindow.Show(true);
            }
            throw;
        }

        g_minecraftHwnd.store(launcherWindow.hwnd(), std::memory_order_release);
        launcherWindow.SetTitle("Toolscreen GUI Integration Tests - Launcher");
        if (hideLauncher) {
            launcherWindow.Show(true);
        }

        return std::string("PASS ") + selectedTestCaseName + (runMode == TestRunMode::Visual ? " [visual]" : " [automated]");
    };

    switch (action) {
        case LauncherAction::RunSelectedAutomated:
            return runSingle(TestRunMode::Automated);
        case LauncherAction::RunSelectedVisual:
            return runSingle(TestRunMode::Visual);
        case LauncherAction::RunAllAutomated:
            HandleImGuiContextReset();
            RunAllTestCases();
            g_minecraftHwnd.store(launcherWindow.hwnd(), std::memory_order_release);
            launcherWindow.SetTitle("Toolscreen GUI Integration Tests - Launcher");
            return "PASS all automated test cases";
        case LauncherAction::Exit:
            return "Launcher closed.";
        case LauncherAction::None:
            break;
    }

    return launcherState.lastStatus;
}

void RunLauncherGui() {
    DummyWindow launcherWindow(kWindowWidth, kWindowHeight, true);
    launcherWindow.SetTitle("Toolscreen GUI Integration Tests - Launcher");

    LauncherState launcherState;
    while (launcherWindow.PumpMessages()) {
        if (!launcherWindow.BeginFrame()) {
            break;
        }

        const LauncherAction action = RenderLauncherFrame(launcherState);
        launcherWindow.EndFrame();

        if (action == LauncherAction::Exit) {
            break;
        }

        if (action != LauncherAction::None) {
            try {
                launcherState.lastStatus = ExecuteLauncherAction(launcherWindow, action, launcherState);
            } catch (const std::exception& ex) {
                launcherState.lastStatus = std::string("FAIL: ") + ex.what();
            }
        }

        Sleep(16);
    }
}

void PrintUsage(std::ostream& stream) {
    stream << "Usage:" << std::endl;
    stream << "  toolscreen_gui_integration_tests" << std::endl;
    stream << "  toolscreen_gui_integration_tests --run-all" << std::endl;
    stream << "  toolscreen_gui_integration_tests <test-case>" << std::endl;
    stream << "  toolscreen_gui_integration_tests --visual [<test-case>]" << std::endl;
    stream << "  toolscreen_gui_integration_tests --list" << std::endl;
    stream << "  toolscreen_gui_integration_tests --help" << std::endl;
    stream << std::endl;
    stream << "No arguments opens a launcher GUI where you can choose which test mode to run." << std::endl;
    stream << "Use --run-all for pure CLI pass/fail execution of every test case." << std::endl;
    stream << "Visual mode keeps the dummy Win32/WGL window open so the GUI can be inspected interactively." << std::endl;
    stream << "If no visual test case is provided, it defaults to " << kDefaultVisualTestCase << "." << std::endl;
    stream << std::endl;
    PrintTestCaseList(stream);
}

struct CommandLineOptions {
    bool openLauncher = false;
    bool showUsage = false;
    bool listOnly = false;
    bool runAll = false;
    TestRunMode runMode = TestRunMode::Automated;
    std::string testCaseName;
};

CommandLineOptions ParseCommandLine(int argc, char** argv) {
    if (argc == 1) {
        CommandLineOptions options;
        options.openLauncher = true;
        return options;
    }

    if (argc > 3) {
        throw std::runtime_error("Expected at most two arguments.");
    }

    const std::string firstArg = argv[1];
    if (firstArg == "--help" || firstArg == "-h") {
        if (argc != 2) {
            throw std::runtime_error("--help does not accept additional arguments.");
        }

        CommandLineOptions options;
        options.showUsage = true;
        return options;
    }

    if (firstArg == "--list") {
        if (argc != 2) {
            throw std::runtime_error("--list does not accept additional arguments.");
        }

        CommandLineOptions options;
        options.listOnly = true;
        return options;
    }

    if (firstArg == "--run-all") {
        if (argc != 2) {
            throw std::runtime_error("--run-all does not accept additional arguments.");
        }

        CommandLineOptions options;
        options.runAll = true;
        return options;
    }

    if (firstArg == "--visual") {
        CommandLineOptions options;
        options.runMode = TestRunMode::Visual;
        options.testCaseName = argc == 3 ? argv[2] : kDefaultVisualTestCase;
        if (options.testCaseName == "all") {
            throw std::runtime_error("Visual mode requires a single test case.");
        }

        return options;
    }

    if (argc != 2) {
        throw std::runtime_error("Unexpected extra arguments.");
    }

    if (firstArg == "all") {
        CommandLineOptions options;
        options.runAll = true;
        return options;
    }

    CommandLineOptions options;
    options.testCaseName = firstArg;
    return options;
}

void RunTestCaseByName(std::string_view testCaseName, TestRunMode runMode) {
    const TestCaseDefinition* testCase = FindTestCaseDefinition(testCaseName);
    if (testCase == nullptr) {
        throw std::runtime_error("Unknown test case: " + std::string(testCaseName));
    }

    std::cout << "RUN " << testCase->name;
    if (runMode == TestRunMode::Visual) {
        std::cout << " [visual]";
    }
    std::cout << std::endl;

    testCase->run(runMode);
    std::cout << "PASS " << testCase->name;
    if (runMode == TestRunMode::Visual) {
        std::cout << " [visual]";
    }
    std::cout << std::endl;
}

void RunAllTestCases() {
    for (const TestCaseDefinition& testCase : GetTestCaseDefinitions()) {
        RunTestCaseByName(testCase.name);
    }
}

bool ShouldPauseForTransientConsole() {
    DWORD consoleProcessIds[2]{};
    return GetConsoleProcessList(consoleProcessIds, static_cast<DWORD>(std::size(consoleProcessIds))) == 1;
}

void PauseForTransientConsole() {
    if (!ShouldPauseForTransientConsole()) {
        return;
    }

    std::cerr << "Press Enter to close..." << std::flush;
    std::string ignored;
    std::getline(std::cin, ignored);
}

} // namespace

int main(int argc, char** argv) {
    try {
        EnsureProcessDpiAwareness();
        const CommandLineOptions options = ParseCommandLine(argc, argv);

        if (options.openLauncher) {
            RunLauncherGui();
            return 0;
        }

        if (options.showUsage) {
            PrintUsage(std::cout);
            return 0;
        }

        if (options.listOnly) {
            PrintTestCaseList(std::cout);
            return 0;
        }

        if (options.runAll) {
            std::cout << "Running all GUI integration tests." << std::endl;
            PrintTestCaseList(std::cout);
            RunAllTestCases();
            return 0;
        }

        RunTestCaseByName(options.testCaseName, options.runMode);
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "FAIL: " << ex.what() << std::endl;
        PrintUsage(std::cerr);
        PauseForTransientConsole();
        return 1;
    }
}