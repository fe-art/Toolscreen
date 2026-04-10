void RunConfigDefaultLoadTest(TestRunMode runMode = TestRunMode::Automated) {
    DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);
    const std::filesystem::path root = PrepareCaseDirectory("config_default_load");
    ResetGlobalTestState(root);

    LoadConfig();

    const std::filesystem::path configPath = root / "config.toml";
    Expect(std::filesystem::exists(configPath), "Expected LoadConfig to create config.toml when it is missing.");
    Expect(!g_configLoadFailed.load(std::memory_order_acquire), "Expected default config load to succeed.");
    Expect(g_configLoaded.load(std::memory_order_acquire), "Expected default config load to mark configuration as ready.");
    Expect(!g_config.modes.empty(), "Expected loaded config to contain at least one mode.");
    Expect(!g_config.hotkeys.empty(), "Expected loaded config to contain at least one hotkey.");
    Expect(!g_config.defaultMode.empty(), "Expected loaded config to have a default mode.");
    Expect(g_config.keyRebinds.indicatorMode == 0,
           "Expected default config load to leave the key rebind indicator disabled.");

    if (runMode == TestRunMode::Visual) {
        RunVisualLoop(window, "config-default-load", &RenderInteractiveSettingsFrame);
    }
}

void RunConfigRoundtripTest(TestRunMode runMode = TestRunMode::Automated) {
    RunRichConfigRoundtripCase("config_roundtrip", []() {
        VerifyRichGlobalSettings();
        VerifyRichModes();
        VerifyRichMirrors();
        VerifyRichMirrorGroups();
        VerifyRichImages();
        VerifyRichWindowOverlays();
        VerifyRichBrowserOverlays();
        VerifyRichHotkeys();
        VerifyRichSensitivityHotkeys();
        VerifyRichCursorsAndEyeZoom();
        VerifyRichKeyRebindsAndAppearance();
        VerifyRichDebugSettings();
    }, runMode);
}

void RunConfigRoundtripGlobalSettingsTest(TestRunMode runMode = TestRunMode::Automated) {
    RunRichConfigRoundtripCase("config_roundtrip_global_settings", &VerifyRichGlobalSettings, runMode);
}

void RunConfigRoundtripModesTest(TestRunMode runMode = TestRunMode::Automated) {
    RunRichConfigRoundtripCase("config_roundtrip_modes", &VerifyRichModes, runMode);
}

void RunConfigRoundtripMirrorsTest(TestRunMode runMode = TestRunMode::Automated) {
    RunRichConfigRoundtripCase("config_roundtrip_mirrors", &VerifyRichMirrors, runMode);
}

void RunConfigRoundtripMirrorGroupsTest(TestRunMode runMode = TestRunMode::Automated) {
    RunRichConfigRoundtripCase("config_roundtrip_mirror_groups", &VerifyRichMirrorGroups, runMode);
}

void RunConfigRoundtripImagesTest(TestRunMode runMode = TestRunMode::Automated) {
    RunRichConfigRoundtripCase("config_roundtrip_images", &VerifyRichImages, runMode);
}

void RunConfigRoundtripWindowOverlaysTest(TestRunMode runMode = TestRunMode::Automated) {
    RunRichConfigRoundtripCase("config_roundtrip_window_overlays", &VerifyRichWindowOverlays, runMode);
}

void RunConfigRoundtripBrowserOverlaysTest(TestRunMode runMode = TestRunMode::Automated) {
    RunRichConfigRoundtripCase("config_roundtrip_browser_overlays", &VerifyRichBrowserOverlays, runMode);
}

void RunConfigRoundtripHotkeysTest(TestRunMode runMode = TestRunMode::Automated) {
    RunRichConfigRoundtripCase("config_roundtrip_hotkeys", &VerifyRichHotkeys, runMode);
}

void RunConfigRoundtripSensitivityHotkeysTest(TestRunMode runMode = TestRunMode::Automated) {
    RunRichConfigRoundtripCase("config_roundtrip_sensitivity_hotkeys", &VerifyRichSensitivityHotkeys, runMode);
}

void RunConfigRoundtripCursorsAndEyeZoomTest(TestRunMode runMode = TestRunMode::Automated) {
    RunRichConfigRoundtripCase("config_roundtrip_cursors_eyezoom", &VerifyRichCursorsAndEyeZoom, runMode);
}

void RunConfigRoundtripKeyRebindsAndAppearanceTest(TestRunMode runMode = TestRunMode::Automated) {
    RunRichConfigRoundtripCase("config_roundtrip_key_rebinds_appearance", &VerifyRichKeyRebindsAndAppearance, runMode);
}

void RunConfigRoundtripDebugSettingsTest(TestRunMode runMode = TestRunMode::Automated) {
    RunRichConfigRoundtripCase("config_roundtrip_debug_settings", &VerifyRichDebugSettings, runMode);
}

void RunConfigLoadEmbeddedNinjabrainPresetsTest(TestRunMode runMode = TestRunMode::Automated) {
    DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);
    const std::filesystem::path root = PrepareCaseDirectory("config_load_embedded_ninjabrain_presets");
    ResetGlobalTestState(root);

    const std::vector<NinjabrainPresetDefinition> presets = GetEmbeddedNinjabrainPresets();
    Expect(presets.size() == 2, "Expected two embedded Ninjabrain presets.");

    const auto findPreset = [&](const std::string& presetId) -> const NinjabrainPresetDefinition* {
     for (const NinjabrainPresetDefinition& preset : presets) {
         if (preset.id == presetId) {
          return &preset;
         }
     }
     return nullptr;
    };

    const NinjabrainPresetDefinition* compactPreset = findPreset("compact");
    const NinjabrainPresetDefinition* ninjabrainBotPreset = findPreset("ninjabrainbot");
    Expect(compactPreset != nullptr, "Expected the embedded compact preset to be available.");
    Expect(ninjabrainBotPreset != nullptr, "Expected the embedded Ninjabrain Bot preset to be available.");

    Expect(compactPreset->translationKey == "ninjabrain.preset_compact",
        "Expected the compact preset to carry its translation key.");
    Expect(compactPreset->preserveCurrentPlacement,
        "Expected the compact preset to preserve placement-specific runtime fields.");
    Expect(compactPreset->overlay.layoutStyle == "compact",
        "Expected the compact preset overlay to stay on the compact layout.");
    Expect(compactPreset->overlay.shownPredictions == 1,
        "Expected the compact preset to show a single prediction row.");
    Expect(compactPreset->overlay.columns.size() == 5,
        "Expected the compact preset to define the current five-column layout.");
    Expect(!compactPreset->overlay.showBoatStateInTopBar,
        "Expected the compact preset to keep the top-bar boat state disabled by default.");
    Expect(compactPreset->overlay.columns.front().header == "Location",
        "Expected the compact preset to rename the coords column header to Location.");

    Expect(ninjabrainBotPreset->translationKey == "ninjabrain.preset_ninjabrainbot",
        "Expected the Ninjabrain Bot preset to carry its translation key.");
    Expect(!ninjabrainBotPreset->preserveCurrentPlacement,
        "Expected the Ninjabrain Bot preset to replace placement-specific runtime fields.");
        Expect(ninjabrainBotPreset->overlay.titleText == "Ninjabrain Bot",
            "Expected embedded presets to load directly from config-style ninjabrainOverlay TOML.");
    Expect(ninjabrainBotPreset->overlay.relativeTo == "topLeftScreen",
        "Expected the Ninjabrain Bot preset to anchor to the top-left screen corner.");
    Expect(ninjabrainBotPreset->overlay.x == 0 && ninjabrainBotPreset->overlay.y == 0,
        "Expected the Ninjabrain Bot preset to reset its position.");
    Expect(!ninjabrainBotPreset->overlay.fontAntialiasing,
        "Expected the Ninjabrain Bot preset to disable font antialiasing.");
    ExpectFloatNear(ninjabrainBotPreset->overlay.informationMessagesMinWidth, 285.0f,
              "Expected the Ninjabrain Bot preset information-message width to come from TOML.");
    ExpectFloatNear(ninjabrainBotPreset->overlay.failureMarginLeft, 24.0f,
              "Expected the Ninjabrain Bot preset failed-result left margin to come from TOML.");
    Expect(!ninjabrainBotPreset->overlay.onlyOnMyScreen,
        "Expected the Ninjabrain Bot preset to render outside the local-only path.");

    if (runMode == TestRunMode::Visual) {
     RunVisualLoop(window, "config-load-embedded-ninjabrain-presets", &RenderInteractiveSettingsFrame);
    }
}

void RunConfigLoadBundledFontPathsNormalizedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_bundled_font_paths_normalized",
                      []() {
                          Config config;
                          const std::filesystem::path root(g_toolscreenPath);

                          config.fontPath = Narrow((root / "fonts" / "OpenSans-Regular.ttf").wstring());
                          config.eyezoom.textFontPath = Narrow((root / "fonts" / "Monocraft.ttf").wstring());
                          config.ninjabrainOverlay.customFontPath = Narrow((root / "Minecraft.ttf").wstring());

                          WriteConfigFixtureToDisk(config);
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-bundled-font-paths-normalized");
                          Expect(g_config.fontPath == "fonts/OpenSans-Regular.ttf",
                                 "Expected absolute bundled GUI font path to normalize to the preset-relative path.");
                          Expect(g_config.eyezoom.textFontPath == "fonts/Monocraft.ttf",
                                 "Expected absolute bundled EyeZoom font path to normalize to the preset-relative path.");
                          Expect(g_config.ninjabrainOverlay.customFontPath == "fonts/Minecraft.ttf",
                                 "Expected legacy bundled Ninjabrain font path to normalize to the preset-relative path.");
                      },
                      runMode);
}

void RunConfigLoadMissingRequiredModesTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_missing_required_modes",
                      []() {
                          Config config;
                          config.defaultMode = kPrimaryModeId;
                          config.modes.clear();

                          ModeConfig primaryMode;
                          primaryMode.id = kPrimaryModeId;
                          primaryMode.width = 1111;
                          primaryMode.height = 666;
                          primaryMode.manualWidth = 1111;
                          primaryMode.manualHeight = 666;
                          config.modes.push_back(primaryMode);

                          WriteConfigFixtureToDisk(config);
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-missing-required-modes");
                          const ModeConfig& fullscreen = FindModeOrThrow("Fullscreen");
                          const ModeConfig& eyezoom = FindModeOrThrow("EyeZoom");
                          const ModeConfig& preemptive = FindModeOrThrow("Preemptive");
                          const ModeConfig& thin = FindModeOrThrow("Thin");
                          const ModeConfig& wide = FindModeOrThrow("Wide");
                          (void)thin;
                          (void)wide;
                          Expect(fullscreen.stretch.enabled, "Expected missing Fullscreen mode to be recreated with stretch enabled.");
                          Expect(fullscreen.width > 0 && fullscreen.height > 0,
                                 "Expected recreated Fullscreen mode to receive valid dimensions.");
                          Expect(eyezoom.width > 0 && eyezoom.height > 0, "Expected missing EyeZoom mode to be recreated.");
                          Expect(preemptive.width == eyezoom.width && preemptive.height == eyezoom.height,
                                 "Expected missing Preemptive mode to inherit EyeZoom dimensions.");
                          Expect(!preemptive.useRelativeSize, "Expected recreated Preemptive mode to use absolute sizing.");
                      },
                      runMode);
}

void RunConfigLoadInvalidHotkeyModeReferencesTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_invalid_hotkey_mode_references",
                      []() {
                          Config config;
                          config.defaultMode = kPrecisionModeId;
                          config.modes.clear();

                          ModeConfig precisionMode;
                          precisionMode.id = kPrecisionModeId;
                          precisionMode.width = 900;
                          precisionMode.height = 500;
                          precisionMode.manualWidth = 900;
                          precisionMode.manualHeight = 500;
                          config.modes.push_back(precisionMode);

                          HotkeyConfig invalidHotkey;
                          invalidHotkey.keys = { VK_F2 };
                          invalidHotkey.mainMode = "Missing Main";
                          invalidHotkey.secondaryMode = "Missing Secondary";
                          invalidHotkey.altSecondaryModes = {
                              { { 'A' }, "Missing Alt" },
                              { { 'B' }, kPrecisionModeId },
                          };
                          config.hotkeys = { invalidHotkey };

                          WriteConfigFixtureToDisk(config);
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-invalid-hotkey-mode-references");
                          Expect(!g_config.hotkeys.empty(), "Expected hotkey fixture to load.");
                          const HotkeyConfig& hotkey = g_config.hotkeys.front();
                          Expect(hotkey.mainMode == kPrecisionModeId,
                                 "Expected invalid hotkey main mode to reset to the existing default mode.");
                          Expect(hotkey.secondaryMode.empty(), "Expected invalid hotkey secondary mode to be cleared.");
                          Expect(hotkey.altSecondaryModes.size() == 1, "Expected invalid alt secondary modes to be removed.");
                          Expect(hotkey.altSecondaryModes.front().mode == kPrecisionModeId,
                                 "Expected valid alt secondary mode to remain after sanitization.");
                      },
                      runMode);
}

void RunConfigLoadRelativeModeDimensionsTest(TestRunMode runMode = TestRunMode::Automated) {
    int expectedWidth = 1000;
    int expectedHeight = 600;

    RunConfigLoadCase("config_load_relative_mode_dimensions",
                      [&]() {
                          Config config;
                          config.defaultMode = kRelativeModeId;
                          config.modes.clear();

                          ModeConfig relativeMode;
                          relativeMode.id = kRelativeModeId;
                          relativeMode.useRelativeSize = true;
                          relativeMode.relativeWidth = 0.625f;
                          relativeMode.relativeHeight = 0.4f;
                          relativeMode.width = 1000;
                          relativeMode.height = 600;
                          relativeMode.manualWidth = 1000;
                          relativeMode.manualHeight = 600;
                          config.modes.push_back(relativeMode);

                          const int loadScreenWidth = (std::max)(1, GetCachedWindowWidth());
                          const int loadScreenHeight = (std::max)(1, GetCachedWindowHeight());
                          expectedWidth = (std::max)(1, static_cast<int>(std::lround(0.625f * static_cast<float>(loadScreenWidth))));
                          expectedHeight = (std::max)(1, static_cast<int>(std::lround(0.4f * static_cast<float>(loadScreenHeight))));

                          WriteConfigFixtureToDisk(config);
                      },
                      [&]() {
                          ExpectConfigLoadSucceeded("config-load-relative-mode-dimensions");
                          const ModeConfig& relativeMode = FindModeOrThrow(kRelativeModeId);
                          Expect(relativeMode.useRelativeSize, "Expected relative mode to stay marked as relative.");
                          ExpectFloatNear(relativeMode.relativeWidth, 0.625f, "Expected relative mode relativeWidth to roundtrip.");
                          ExpectFloatNear(relativeMode.relativeHeight, 0.4f, "Expected relative mode relativeHeight to roundtrip.");
                          Expect(relativeMode.width == expectedWidth, "Expected relative mode width to be recomputed from cached client width.");
                          Expect(relativeMode.height == expectedHeight,
                                 "Expected relative mode height to be recomputed from cached client height.");
                          Expect(relativeMode.manualWidth == 1000 && relativeMode.manualHeight == 600,
                                 "Expected relative mode manual dimensions to preserve their persisted values.");
                      },
                      runMode);
}

void RunConfigLoadExpressionModeDimensionsTest(TestRunMode runMode = TestRunMode::Automated) {
    int expectedWidth = 123;
    int expectedHeight = 456;

    RunConfigLoadCase("config_load_expression_mode_dimensions",
                      [&]() {
                          Config config;
                          config.defaultMode = kExpressionModeId;
                          config.modes.clear();

                          ModeConfig expressionMode;
                          expressionMode.id = kExpressionModeId;
                          expressionMode.widthExpr = "screenWidth / 3";
                          expressionMode.heightExpr = "screenHeight - 111";
                          expressionMode.width = 123;
                          expressionMode.height = 456;
                          expressionMode.manualWidth = 123;
                          expressionMode.manualHeight = 456;
                          config.modes.push_back(expressionMode);

                          const int loadScreenWidth = (std::max)(1, GetCachedWindowWidth());
                          const int loadScreenHeight = (std::max)(1, GetCachedWindowHeight());
                          expectedWidth = (std::max)(1, loadScreenWidth / 3);
                          expectedHeight = (std::max)(1, loadScreenHeight - 111);

                          WriteConfigFixtureToDisk(config);
                      },
                      [&]() {
                          ExpectConfigLoadSucceeded("config-load-expression-mode-dimensions");
                          const ModeConfig& expressionMode = FindModeOrThrow(kExpressionModeId);
                          Expect(expressionMode.widthExpr == "screenWidth / 3", "Expected width expression to roundtrip.");
                          Expect(expressionMode.heightExpr == "screenHeight - 111", "Expected height expression to roundtrip.");
                          Expect(expressionMode.width == expectedWidth, "Expected width expression to be evaluated during load.");
                          Expect(expressionMode.height == expectedHeight, "Expected height expression to be evaluated during load.");
                      },
                      runMode);
}

void RunConfigLoadLegacyVersionUpgradeTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_legacy_version_upgrade",
                      []() {
                          Config config;
                          config.configVersion = 1;
                          config.disableHookChaining = true;
                          config.defaultMode = kPrecisionModeId;
                          config.keyRepeatStartDelay = 10;
                          config.keyRepeatDelay = 0;
                          config.modes.clear();

                          ModeConfig precisionMode;
                          precisionMode.id = kPrecisionModeId;
                          precisionMode.width = 800;
                          precisionMode.height = 450;
                          precisionMode.manualWidth = 800;
                          precisionMode.manualHeight = 450;
                          config.modes.push_back(precisionMode);

                          WriteConfigFixtureToDisk(config);
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-legacy-version-upgrade");
                          Expect(g_config.configVersion == GetConfigVersion(), "Expected legacy config version to upgrade to the current version.");
                          Expect(g_config.disableHookChaining,
                                 "Expected legacy disableHookChaining to remain preserved on the 1.2.1 branch.");
                             Expect(!g_config.useSystemKeyRepeat,
                                 "Expected legacy configs without the useSystemKeyRepeat flag to default to local repeat handling.");
                         Expect(!g_config.modifiersInterruptKeyRepeat,
                             "Expected legacy configs without the modifiersInterruptKeyRepeat flag to leave modifier interruption disabled.");
                          Expect(g_config.keyRepeatStartDelay == 10,
                                 "Expected legacy non-zero keyRepeatStartDelay to remain preserved after upgrade.");
                          Expect(g_config.keyRepeatDelay == ConfigDefaults::CONFIG_KEY_REPEAT_DELAY,
                                 "Expected legacy zero keyRepeatDelay to normalize to the default value.");
                      },
                      runMode);
}

void RunConfigLoadClampGlobalValuesTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_clamp_global_values",
                      []() {
                          Config config;
                          config.defaultMode = "Fullscreen";
                          config.obsFramerate = 999;
                          config.cursors.enabled = true;
                          config.cursors.title.cursorSize = 9999;
                          config.cursors.wall.cursorSize = 2;
                          config.cursors.ingame.cursorSize = 321;
                          WriteConfigFixtureToDisk(config);
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-clamp-global-values");
                          Expect(g_config.obsFramerate == 120, "Expected obsFramerate to clamp to the configured maximum.");
                          Expect(g_config.cursors.title.cursorSize == ConfigDefaults::CURSOR_MAX_SIZE,
                                 "Expected title cursor size to clamp to CURSOR_MAX_SIZE.");
                          Expect(g_config.cursors.wall.cursorSize == ConfigDefaults::CURSOR_MIN_SIZE,
                                 "Expected wall cursor size to clamp to CURSOR_MIN_SIZE.");
                          Expect(g_config.cursors.ingame.cursorSize == ConfigDefaults::CURSOR_MAX_SIZE,
                                 "Expected ingame cursor size to clamp to CURSOR_MAX_SIZE.");
                      },
                      runMode);
}

void RunConfigLoadModeDefaultDimensionsRestoredTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_mode_default_dimensions_restored",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Wide"

[[mode]]
id = "Wide"
width = 1
height = 0
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-mode-default-dimensions-restored");
                          const ModeConfig& wideMode = FindModeOrThrow("Wide");
                          Expect(wideMode.useRelativeSize, "Expected repaired Wide mode to remain relative-sized.");
                          Expect(!wideMode.widthExpr.empty(), "Expected repaired Wide mode width expression to come from embedded defaults.");
                          ExpectFloatNear(wideMode.relativeHeight, 0.25f,
                                          "Expected repaired Wide mode relativeHeight to come from embedded defaults.");
                          Expect(wideMode.width > 1, "Expected repaired Wide mode width to be recomputed from the restored default expression.");
                          Expect(wideMode.height > 1, "Expected repaired Wide mode height to be recomputed from the restored default percentage.");
                      },
                      runMode);
}

void RunConfigLoadModeSourceListsLoadedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_mode_source_lists_loaded",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Source Lists"

[[mode]]
id = "Source Lists"
width = 800
height = 600
mirrorIds = ["Mirror One"]
mirrorGroupIds = ["Group One"]
imageIds = ["Image One"]
windowOverlayIds = ["Window One"]
browserOverlayIds = ["Browser One"]
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-mode-source-lists-loaded");
                          const ModeConfig& mode = FindModeOrThrow("Source Lists");
                          ExpectVectorEquals(mode.mirrorIds, std::vector<std::string>{ "Mirror One" },
                                             "Expected mirrorIds to load on the 1.2.1 branch.");
                          ExpectVectorEquals(mode.mirrorGroupIds, std::vector<std::string>{ "Group One" },
                                             "Expected mirrorGroupIds to load on the 1.2.1 branch.");
                          ExpectVectorEquals(mode.imageIds, std::vector<std::string>{ "Image One" },
                                             "Expected imageIds to load on the 1.2.1 branch.");
                          ExpectVectorEquals(mode.windowOverlayIds, std::vector<std::string>{ "Window One" },
                                             "Expected windowOverlayIds to load on the 1.2.1 branch.");
                          ExpectVectorEquals(mode.browserOverlayIds, std::vector<std::string>{ "Browser One" },
                                             "Expected browserOverlayIds to load on the 1.2.1 branch.");
                      },
                      runMode);
}

void RunConfigLoadModePercentageDimensionsDetectedTest(TestRunMode runMode = TestRunMode::Automated) {
    int expectedWidth = 1;
    int expectedHeight = 1;

    RunConfigLoadCase("config_load_mode_percentage_dimensions_detected",
                      [&]() {
                          const int screenWidth = (std::max)(1, GetCachedWindowWidth());
                          const int screenHeight = (std::max)(1, GetCachedWindowHeight());
                          expectedWidth = (std::max)(1, static_cast<int>(std::lround(0.5f * static_cast<float>(screenWidth))));
                          expectedHeight = (std::max)(1, static_cast<int>(std::lround(0.25f * static_cast<float>(screenHeight))));

                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Percentage Mode"

[[mode]]
id = "Percentage Mode"
width = 0.5
height = 0.25
)");
                      },
                      [&]() {
                          ExpectConfigLoadSucceeded("config-load-mode-percentage-dimensions-detected");
                          const ModeConfig& mode = FindModeOrThrow("Percentage Mode");
                          Expect(mode.useRelativeSize, "Expected percentage width/height values to mark the mode as relative-sized.");
                          ExpectFloatNear(mode.relativeWidth, 0.5f, "Expected percentage width to map to relativeWidth.");
                          ExpectFloatNear(mode.relativeHeight, 0.25f, "Expected percentage height to map to relativeHeight.");
                          Expect(mode.width == expectedWidth, "Expected percentage width to resolve against the cached window width.");
                          Expect(mode.height == expectedHeight, "Expected percentage height to resolve against the cached window height.");
                      },
                      runMode);
}

void RunConfigLoadModeTypedSourcesIgnoredTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_mode_typed_sources_ignored",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Typed Sources"

[[mode]]
id = "Typed Sources"
width = 800
height = 600
sources = [
    { type = "Mirror", id = "" },
    { type = "Mirror", id = "Valid Mirror" },
    { type = "Image", id = "" }
]
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-mode-typed-sources-ignored");
                          const ModeConfig& mode = FindModeOrThrow("Typed Sources");
                          Expect(mode.mirrorIds.empty(), "Expected typed mode sources to be ignored on the 1.2.1 branch.");
                          Expect(mode.mirrorGroupIds.empty(), "Expected typed mode sources to leave mirrorGroupIds empty.");
                          Expect(mode.imageIds.empty(), "Expected typed mode sources to leave imageIds empty.");
                          Expect(mode.windowOverlayIds.empty(), "Expected typed mode sources to leave windowOverlayIds empty.");
                          Expect(mode.browserOverlayIds.empty(), "Expected typed mode sources to leave browserOverlayIds empty.");
                      },
                      runMode);
}

void RunConfigLoadEmptyMainHotkeyFallbackTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_empty_main_hotkey_fallback",
                      []() {
                          Config config;
                          config.defaultMode = kPrecisionModeId;

                          ModeConfig precisionMode;
                          precisionMode.id = kPrecisionModeId;
                          precisionMode.width = 900;
                          precisionMode.height = 500;
                          precisionMode.manualWidth = 900;
                          precisionMode.manualHeight = 500;
                          config.modes.push_back(precisionMode);

                          HotkeyConfig hotkey;
                          hotkey.keys = { VK_F2 };
                          hotkey.mainMode.clear();
                          hotkey.secondaryMode = kPrecisionModeId;
                          config.hotkeys.push_back(hotkey);

                          WriteConfigFixtureToDisk(config);
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-empty-main-hotkey-fallback");
                          const HotkeyConfig& hotkey = FindHotkeyByKeysOrThrow({ VK_F2 });
                          Expect(hotkey.mainMode == kPrecisionModeId,
                                 "Expected a hotkey with an empty main mode to fall back to the default mode.");
                      },
                      runMode);
}

void RunConfigLoadMissingGuiHotkeyDefaultedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_missing_gui_hotkey_defaulted",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-missing-gui-hotkey-defaulted");
                          ExpectVectorEquals(g_config.guiHotkey, ConfigDefaults::GetDefaultGuiHotkey(),
                                             "Expected a missing guiHotkey to default to the configured hotkey.");
                      },
                      runMode);
}

void RunConfigLoadEmptyGuiHotkeyDefaultedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_empty_gui_hotkey_defaulted",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"
guiHotkey = []
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-empty-gui-hotkey-defaulted");
                          ExpectVectorEquals(g_config.guiHotkey, ConfigDefaults::GetDefaultGuiHotkey(),
                                             "Expected an empty guiHotkey array to fall back to the configured default hotkey.");
                      },
                      runMode);
}

void RunConfigLoadLegacyMirrorGammaMigratedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_legacy_mirror_gamma_migrated",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[[mirror]]
name = "Legacy Gamma"
gammaMode = "Linear"
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-legacy-mirror-gamma-migrated");
                          Expect(g_config.mirrorGammaMode == MirrorGammaMode::AssumeLinear,
                                 "Expected legacy per-mirror gammaMode to migrate into the global mirror gamma mode.");
                      },
                      runMode);
}

void RunConfigLoadMirrorCaptureDimensionsClampedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_mirror_capture_dimensions_clamped",
                      []() {
                          Config config;
                          config.defaultMode = "Fullscreen";

                          MirrorConfig mirror;
                          mirror.name = "Clamp Mirror";
                          mirror.captureWidth = ConfigDefaults::MIRROR_CAPTURE_MAX_DIMENSION + 200;
                          mirror.captureHeight = 0;
                          config.mirrors.push_back(mirror);

                          WriteConfigFixtureToDisk(config);
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-mirror-capture-dimensions-clamped");
                          const MirrorConfig& mirror = FindMirrorOrThrow("Clamp Mirror");
                          Expect(mirror.captureWidth == ConfigDefaults::MIRROR_CAPTURE_MAX_DIMENSION,
                                 "Expected mirror captureWidth to clamp to MIRROR_CAPTURE_MAX_DIMENSION.");
                          Expect(mirror.captureHeight == ConfigDefaults::MIRROR_CAPTURE_MIN_DIMENSION,
                                 "Expected mirror captureHeight to clamp to MIRROR_CAPTURE_MIN_DIMENSION.");
                      },
                      runMode);
}

void RunConfigLoadEyeZoomCloneWidthNormalizedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_eyezoom_clone_width_normalized",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[eyezoom]
cloneWidth = 15
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-eyezoom-clone-width-normalized");
                          Expect(g_config.eyezoom.cloneWidth == 14,
                                 "Expected odd eyezoom cloneWidth values to normalize to the nearest even width.");
                      },
                      runMode);
}

void RunConfigLoadEyeZoomOverlayWidthDefaultedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_eyezoom_overlay_width_defaulted",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[eyezoom]
cloneWidth = 14
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-eyezoom-overlay-width-defaulted");
                          Expect(g_config.eyezoom.overlayWidth == 7,
                                 "Expected a missing eyezoom overlayWidth to default to half of cloneWidth.");
                      },
                      runMode);
}

void RunConfigLoadEyeZoomOverlayWidthClampedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_eyezoom_overlay_width_clamped",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[eyezoom]
cloneWidth = 10
overlayWidth = 99
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-eyezoom-overlay-width-clamped");
                          Expect(g_config.eyezoom.overlayWidth == 5,
                                 "Expected eyezoom overlayWidth to clamp to half of cloneWidth.");
                      },
                      runMode);
}

void RunConfigLoadEyeZoomLegacyMarginsMigratedTest(TestRunMode runMode = TestRunMode::Automated) {
    int expectedWidth = 1;
    int expectedHeight = 1;

    RunConfigLoadCase("config_load_eyezoom_legacy_margins_migrated",
                      [&]() {
                          const int screenWidth = (std::max)(1, GetCachedWindowWidth());
                          const int screenHeight = (std::max)(1, GetCachedWindowHeight());
                          const int viewportX = (screenWidth - 400) / 2;
                          expectedWidth = (viewportX > 0) ? (viewportX - (2 * 20)) : screenWidth;
                          expectedHeight = screenHeight - (2 * 30);

                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[eyezoom]
windowWidth = 400
horizontalMargin = 20
verticalMargin = 30
)");
                      },
                      [&]() {
                          ExpectConfigLoadSucceeded("config-load-eyezoom-legacy-margins-migrated");
                          Expect(g_config.eyezoom.zoomAreaWidth == expectedWidth,
                                 "Expected legacy eyezoom horizontalMargin to migrate into zoomAreaWidth.");
                          Expect(g_config.eyezoom.zoomAreaHeight == expectedHeight,
                                 "Expected legacy eyezoom verticalMargin to migrate into zoomAreaHeight.");
                      },
                      runMode);
}

void RunConfigLoadEyeZoomLegacyCustomPositionMigratedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_eyezoom_legacy_custom_position_migrated",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[eyezoom]
useCustomPosition = true
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-eyezoom-legacy-custom-position-migrated");
                          Expect(g_config.eyezoom.useCustomSizePosition,
                                 "Expected legacy eyezoom useCustomPosition to map to useCustomSizePosition.");
                      },
                      runMode);
}

void RunConfigLoadEyeZoomInvalidActiveOverlayResetTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_eyezoom_invalid_active_overlay_reset",
                      []() {
                          Config config;
                          config.defaultMode = "Fullscreen";
                          config.eyezoom.activeOverlayIndex = 4;
                          config.eyezoom.overlays = {
                              { "Only Overlay", "C:\\temp\\overlay.png", EyeZoomOverlayDisplayMode::Fit, 120, 80, false, 0.75f },
                          };
                          WriteConfigFixtureToDisk(config);
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-eyezoom-invalid-active-overlay-reset");
                          Expect(g_config.eyezoom.activeOverlayIndex == -1,
                                 "Expected out-of-range eyezoom activeOverlayIndex values to reset to -1.");
                      },
                      runMode);
}

void RunConfigLoadWindowOverlayCaptureMethodMigratedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_window_overlay_capture_method_migrated",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[[windowOverlay]]
name = "Legacy Capture"
captureMethod = "Auto"
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-window-overlay-capture-method-migrated");
                          const WindowOverlayConfig& overlay = FindWindowOverlayOrThrow("Legacy Capture");
                          Expect(overlay.captureMethod == "Windows 10+",
                                 "Expected legacy window overlay captureMethod values to migrate to Windows 10+.");
                      },
                      runMode);
}

void RunConfigLoadWindowOverlayCropMigratedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_window_overlay_crop_migrated",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[[windowOverlay]]
name = "Legacy Crop"
crop_top = 4
crop_bottom = 5
crop_left = 6
crop_right = 7
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-window-overlay-crop-migrated");
                          const WindowOverlayConfig& overlay = FindWindowOverlayOrThrow("Legacy Crop");
                          Expect(overlay.crop_top == 8 && overlay.crop_bottom == 10 && overlay.crop_left == 12 &&
                                     overlay.crop_right == 14,
                                 "Expected legacy window overlay crop values to migrate to the v5 scale.");
                          Expect(!overlay.cropToWidth && !overlay.cropToHeight,
                                 "Expected crop-to toggles to stay disabled for migrated legacy window overlays.");
                          Expect(g_config.configVersion == GetConfigVersion(),
                                 "Expected legacy crop migration to upgrade the config to the current version.");
                      },
                      runMode);
}

void RunConfigLoadKeyRebindUnicodeStringParsedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_key_rebind_unicode_string_parsed",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[keyRebinds]
enabled = true
resolveRebindTargetsForHotkeys = false
toggleHotkey = []

[[keyRebinds.rebinds]]
fromKey = 74
toKey = 75
enabled = true
useCustomOutput = true
customOutputUnicode = "U+00F8"
shiftLayerEnabled = true
shiftLayerOutputUnicode = "{00D8}"
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-key-rebind-unicode-string-parsed");
                          Expect(g_config.keyRebinds.rebinds.size() == 1, "Expected exactly one key rebind fixture to load.");
                          const KeyRebind& rebind = g_config.keyRebinds.rebinds.front();
                          Expect(rebind.customOutputUnicode == 0x00F8,
                                 "Expected customOutputUnicode strings to parse into Unicode code points.");
                          Expect(rebind.shiftLayerOutputUnicode == 0x00D8,
                                 "Expected shiftLayerOutputUnicode strings to parse into Unicode code points.");
                      },
                      runMode);
}

void RunConfigLoadKeyRebindEscapedUnicodeStringParsedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_key_rebind_escaped_unicode_string_parsed",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[keyRebinds]
enabled = true
resolveRebindTargetsForHotkeys = false
toggleHotkey = []

[[keyRebinds.rebinds]]
fromKey = 74
toKey = 75
enabled = true
useCustomOutput = true
customOutputUnicode = "\\u00f8"
shiftLayerEnabled = true
shiftLayerOutputUnicode = "\\U00D8"
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-key-rebind-escaped-unicode-string-parsed");
                          Expect(g_config.keyRebinds.rebinds.size() == 1, "Expected exactly one key rebind fixture to load.");
                          const KeyRebind& rebind = g_config.keyRebinds.rebinds.front();
                          Expect(rebind.customOutputUnicode == 0x00F8,
                                 "Expected escaped customOutputUnicode strings to parse into Unicode code points.");
                          Expect(rebind.shiftLayerOutputUnicode == 0x00D8,
                                 "Expected escaped shiftLayerOutputUnicode strings to parse into Unicode code points.");
                      },
                      runMode);
}

void RunConfigLoadKeyRebindHexUnicodeStringParsedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_key_rebind_hex_unicode_string_parsed",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[keyRebinds]
enabled = true
resolveRebindTargetsForHotkeys = false
toggleHotkey = []

[[keyRebinds.rebinds]]
fromKey = 74
toKey = 75
enabled = true
useCustomOutput = true
customOutputUnicode = " 0x00f8 "
shiftLayerEnabled = true
shiftLayerOutputUnicode = " 0X00D8 "
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-key-rebind-hex-unicode-string-parsed");
                          Expect(g_config.keyRebinds.rebinds.size() == 1, "Expected exactly one key rebind fixture to load.");
                          const KeyRebind& rebind = g_config.keyRebinds.rebinds.front();
                          Expect(rebind.customOutputUnicode == 0x00F8,
                                 "Expected hexadecimal customOutputUnicode strings to parse into Unicode code points.");
                          Expect(rebind.shiftLayerOutputUnicode == 0x00D8,
                                 "Expected hexadecimal shiftLayerOutputUnicode strings to parse into Unicode code points.");
                      },
                      runMode);
}

void RunConfigLoadKeyRebindInvalidUnicodeDefaultedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_key_rebind_invalid_unicode_defaulted",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[keyRebinds]
enabled = true
resolveRebindTargetsForHotkeys = false
toggleHotkey = []

[[keyRebinds.rebinds]]
fromKey = 74
toKey = 75
enabled = true
useCustomOutput = true
customOutputUnicode = "bogus"
shiftLayerEnabled = true
shiftLayerOutputUnicode = "U+D800"
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-key-rebind-invalid-unicode-defaulted");
                          Expect(g_config.keyRebinds.rebinds.size() == 1, "Expected exactly one key rebind fixture to load.");
                          const KeyRebind& rebind = g_config.keyRebinds.rebinds.front();
                          Expect(rebind.customOutputUnicode == ConfigDefaults::KEY_REBIND_CUSTOM_OUTPUT_UNICODE,
                                 "Expected invalid customOutputUnicode strings to fall back to the configured default.");
                          Expect(rebind.shiftLayerOutputUnicode == ConfigDefaults::KEY_REBIND_SHIFT_LAYER_OUTPUT_UNICODE,
                                 "Expected invalid shiftLayerOutputUnicode strings to fall back to the configured default.");
                      },
                      runMode);
}

void RunConfigLoadKeyRebindLegacyShowIndicatorMigratedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_key_rebind_legacy_show_indicator_migrated",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[keyRebinds]
showIndicator = true
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-key-rebind-legacy-show-indicator-migrated");
                          Expect(g_config.keyRebinds.indicatorMode == 1,
                                 "Expected legacy showIndicator=true to migrate to the active-only indicator mode.");
                      },
                      runMode);
}

void RunConfigLoadFullscreenStretchRepairedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_fullscreen_stretch_repaired",
                      []() {
                          Config config;
                          config.defaultMode = "Fullscreen";

                          ModeConfig fullscreenMode;
                          fullscreenMode.id = "Fullscreen";
                          fullscreenMode.width = 0;
                          fullscreenMode.height = 0;
                          fullscreenMode.manualWidth = 0;
                          fullscreenMode.manualHeight = 0;
                          fullscreenMode.stretch.enabled = false;
                          fullscreenMode.stretch.x = 33;
                          fullscreenMode.stretch.y = 44;
                          fullscreenMode.stretch.width = 55;
                          fullscreenMode.stretch.height = 66;
                          config.modes.push_back(fullscreenMode);

                          WriteConfigFixtureToDisk(config);
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-fullscreen-stretch-repaired");
                          const ModeConfig& fullscreenMode = FindModeOrThrow("Fullscreen");
                          Expect(fullscreenMode.width > 0 && fullscreenMode.height > 0,
                                 "Expected Fullscreen mode dimensions to repair to valid values.");
                          Expect(fullscreenMode.manualWidth == fullscreenMode.width && fullscreenMode.manualHeight == fullscreenMode.height,
                                 "Expected Fullscreen manual dimensions to repair alongside the live dimensions.");
                          Expect(fullscreenMode.stretch.enabled, "Expected Fullscreen stretch to be forced on during load.");
                          Expect(fullscreenMode.stretch.x == 0 && fullscreenMode.stretch.y == 0,
                                 "Expected Fullscreen stretch origin to reset to the top-left corner.");
                          Expect(fullscreenMode.stretch.width == fullscreenMode.width &&
                                     fullscreenMode.stretch.height == fullscreenMode.height,
                                 "Expected Fullscreen stretch bounds to match the repaired mode dimensions.");
                      },
                      runMode);
}

void RunConfigLoadPreemptiveSyncExistingModeTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_preemptive_sync_existing_mode",
                      []() {
                          Config config;
                          config.defaultMode = "EyeZoom";

                          ModeConfig eyezoomMode;
                          eyezoomMode.id = "EyeZoom";
                          eyezoomMode.width = 640;
                          eyezoomMode.height = 1200;
                          eyezoomMode.manualWidth = 640;
                          eyezoomMode.manualHeight = 1200;
                          config.modes.push_back(eyezoomMode);

                          ModeConfig preemptiveMode;
                          preemptiveMode.id = "Preemptive";
                          preemptiveMode.width = 123;
                          preemptiveMode.height = 456;
                          preemptiveMode.manualWidth = 123;
                          preemptiveMode.manualHeight = 456;
                          preemptiveMode.useRelativeSize = true;
                          preemptiveMode.relativeWidth = 0.5f;
                          preemptiveMode.relativeHeight = 0.25f;
                          config.modes.push_back(preemptiveMode);

                          WriteConfigFixtureToDisk(config);
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-preemptive-sync-existing-mode");
                          const ModeConfig& eyezoomMode = FindModeOrThrow("EyeZoom");
                          const ModeConfig& preemptiveMode = FindModeOrThrow("Preemptive");
                          Expect(preemptiveMode.width == eyezoomMode.width && preemptiveMode.height == eyezoomMode.height,
                                 "Expected Preemptive mode dimensions to sync to EyeZoom during load.");
                          Expect(preemptiveMode.manualWidth == eyezoomMode.manualWidth &&
                                     preemptiveMode.manualHeight == eyezoomMode.manualHeight,
                                 "Expected Preemptive manual dimensions to sync to EyeZoom during load.");
                          Expect(!preemptiveMode.useRelativeSize && preemptiveMode.relativeWidth < 0.0f &&
                                     preemptiveMode.relativeHeight < 0.0f,
                                 "Expected Preemptive relative sizing to clear when it is resynced from EyeZoom.");
                      },
                      runMode);
}

void RunConfigLoadThinMinWidthEnforcedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_thin_min_width_enforced",
                      []() {
                          Config config;
                          config.defaultMode = "Thin";

                          ModeConfig thinMode;
                          thinMode.id = "Thin";
                          thinMode.width = 100;
                          thinMode.height = 700;
                          thinMode.manualWidth = 100;
                          thinMode.manualHeight = 700;
                          config.modes.push_back(thinMode);

                          WriteConfigFixtureToDisk(config);
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-thin-min-width-enforced");
                          const ModeConfig& thinMode = FindModeOrThrow("Thin");
                          Expect(thinMode.width == 330, "Expected Thin mode width to clamp to the hard minimum during load.");
                      },
                      runMode);
}

void RunConfigLoadBrowserOverlayDefaultsTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_browser_overlay_defaults",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[[browserOverlay]]
name = "Default Browser"
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-browser-overlay-defaults");
                          const BrowserOverlayConfig& overlay = FindBrowserOverlayOrThrow("Default Browser");
                          Expect(overlay.url == "https://example.com", "Expected browser overlay URL to default when omitted.");
                          Expect(overlay.browserWidth == ConfigDefaults::BROWSER_OVERLAY_WIDTH,
                                 "Expected browser overlay width to default when omitted.");
                          Expect(overlay.browserHeight == ConfigDefaults::BROWSER_OVERLAY_HEIGHT,
                                 "Expected browser overlay height to default when omitted.");
                          Expect(overlay.transparentBackground == ConfigDefaults::BROWSER_OVERLAY_TRANSPARENT_BACKGROUND,
                                 "Expected browser overlay transparentBackground to default when omitted.");
                          Expect(overlay.reloadInterval == ConfigDefaults::BROWSER_OVERLAY_RELOAD_INTERVAL,
                                 "Expected browser overlay reloadInterval to default when omitted.");
                      },
                      runMode);
}

void RunConfigLoadWindowOverlayDefaultsTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_window_overlay_defaults",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[[windowOverlay]]
name = "Default Window"
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-window-overlay-defaults");
                          const WindowOverlayConfig& overlay = FindWindowOverlayOrThrow("Default Window");
                          Expect(overlay.windowMatchPriority == ConfigDefaults::WINDOW_OVERLAY_MATCH_PRIORITY,
                                 "Expected window overlay match priority to default when omitted.");
                          Expect(overlay.captureMethod == ConfigDefaults::WINDOW_OVERLAY_CAPTURE_METHOD,
                                 "Expected window overlay captureMethod to default when omitted.");
                          Expect(overlay.fps == ConfigDefaults::WINDOW_OVERLAY_FPS,
                                 "Expected window overlay FPS to default when omitted.");
                          Expect(overlay.searchInterval == ConfigDefaults::WINDOW_OVERLAY_SEARCH_INTERVAL,
                                 "Expected window overlay searchInterval to default when omitted.");
                          Expect(overlay.enableInteraction == ConfigDefaults::WINDOW_OVERLAY_ENABLE_INTERACTION,
                                 "Expected window overlay enableInteraction to default when omitted.");
                      },
                      runMode);
}

void RunConfigLoadImageDefaultsTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_image_defaults",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[[image]]
name = "Default Image"
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-image-defaults");
                          const ImageConfig& image = FindImageOrThrow("Default Image");
                          ExpectFloatNear(image.scale, ConfigDefaults::IMAGE_SCALE, "Expected image scale to default when omitted.");
                          Expect(image.relativeSizing == ConfigDefaults::IMAGE_RELATIVE_SIZING,
                                 "Expected image relativeSizing to default when omitted.");
                          Expect(image.relativeTo == ConfigDefaults::IMAGE_RELATIVE_TO,
                                 "Expected image relativeTo to default when omitted.");
                          ExpectFloatNear(image.opacity, ConfigDefaults::IMAGE_OPACITY,
                                          "Expected image opacity to default when omitted.");
                          Expect(image.onlyOnMyScreen == ConfigDefaults::IMAGE_ONLY_ON_MY_SCREEN,
                                 "Expected image onlyOnMyScreen to default when omitted.");
                      },
                      runMode);
}

void RunConfigLoadHotkeyDefaultFlagsTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_hotkey_default_flags",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[[mode]]
id = "Target Mode"
width = 800
height = 600

[[hotkey]]
keys = [65]
mainMode = "Fullscreen"
secondaryMode = "Target Mode"
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-hotkey-default-flags");
                          const HotkeyConfig& hotkey = FindHotkeyByKeysOrThrow({ 65 });
                          Expect(hotkey.debounce == ConfigDefaults::HOTKEY_DEBOUNCE,
                                 "Expected hotkey debounce to default when omitted.");
                          Expect(!hotkey.triggerOnRelease && !hotkey.triggerOnHold,
                                 "Expected hotkey trigger flags to default to false when omitted.");
                          Expect(!hotkey.blockKeyFromGame && !hotkey.allowExitToFullscreenRegardlessOfGameState,
                                 "Expected hotkey behavior flags to default to false when omitted.");
                          Expect(hotkey.conditions.gameState.empty() && hotkey.conditions.exclusions.empty(),
                                 "Expected omitted hotkey conditions to remain empty.");
                      },
                      runMode);
}

void RunConfigLoadSensitivityHotkeyDefaultFlagsTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_sensitivity_hotkey_default_flags",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[[sensitivityHotkey]]
keys = [70]
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-sensitivity-hotkey-default-flags");
                          const SensitivityHotkeyConfig& hotkey = FindSensitivityHotkeyOrThrow({ 70 });
                          ExpectFloatNear(hotkey.sensitivity, 1.0f, "Expected sensitivity hotkey sensitivity to default when omitted.");
                          Expect(!hotkey.separateXY, "Expected sensitivity hotkey separateXY to default to false when omitted.");
                          ExpectFloatNear(hotkey.sensitivityX, 1.0f,
                                          "Expected sensitivity hotkey sensitivityX to default when omitted.");
                          ExpectFloatNear(hotkey.sensitivityY, 1.0f,
                                          "Expected sensitivity hotkey sensitivityY to default when omitted.");
                          Expect(hotkey.debounce == ConfigDefaults::HOTKEY_DEBOUNCE,
                                 "Expected sensitivity hotkey debounce to default when omitted.");
                          Expect(!hotkey.toggle, "Expected sensitivity hotkey toggle to default to false when omitted.");
                      },
                      runMode);
}

void RunConfigLoadImageColorKeyDefaultSensitivityTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_image_color_key_default_sensitivity",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[[image]]
name = "Color Key Image"
colorKeys = [{ color = [255, 0, 255] }]
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-image-color-key-default-sensitivity");
                          const ImageConfig& image = FindImageOrThrow("Color Key Image");
                          Expect(image.colorKeys.size() == 1, "Expected image color key fixture to load.");
                          ExpectFloatNear(image.colorKeys.front().sensitivity, ConfigDefaults::COLOR_KEY_SENSITIVITY,
                                          "Expected image color key sensitivity to default when omitted.");
                      },
                      runMode);
}

void RunConfigLoadWindowOverlayColorKeyDefaultSensitivityTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_window_overlay_color_key_default_sensitivity",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[[windowOverlay]]
name = "Color Key Window"
colorKeys = [{ color = [0, 255, 0] }]
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-window-overlay-color-key-default-sensitivity");
                          const WindowOverlayConfig& overlay = FindWindowOverlayOrThrow("Color Key Window");
                          Expect(overlay.colorKeys.size() == 1, "Expected window overlay color key fixture to load.");
                          ExpectFloatNear(overlay.colorKeys.front().sensitivity, ConfigDefaults::COLOR_KEY_SENSITIVITY,
                                          "Expected window overlay color key sensitivity to default when omitted.");
                      },
                      runMode);
}

void RunConfigLoadBrowserOverlayColorKeyDefaultSensitivityTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_browser_overlay_color_key_default_sensitivity",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[[browserOverlay]]
name = "Color Key Browser"
colorKeys = [{ color = [0, 0, 0] }]
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-browser-overlay-color-key-default-sensitivity");
                          const BrowserOverlayConfig& overlay = FindBrowserOverlayOrThrow("Color Key Browser");
                          Expect(overlay.colorKeys.size() == 1, "Expected browser overlay color key fixture to load.");
                          ExpectFloatNear(overlay.colorKeys.front().sensitivity, ConfigDefaults::COLOR_KEY_SENSITIVITY,
                                          "Expected browser overlay color key sensitivity to default when omitted.");
                      },
                      runMode);
}