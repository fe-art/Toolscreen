if (BeginSelectableSettingsTopTabItem(trc("tabs.settings"))) {
    g_currentlyEditingMirror = "";
    g_imageDragMode.store(false);
    g_windowOverlayDragMode.store(false);

    auto drawMirrorColorspaceSetting = []() {
        const char* gammaModes[] = { trc("settings.mirrors_auto"), trc("settings.mirrors_assume_srgb"), trc("settings.mirrors_assume_linear") };
        int gm = static_cast<int>(g_config.mirrorGammaMode);
        ImGui::SetNextItemWidth(250);
        if (ImGui::Combo(trc("settings.mirrors_match_colorspace"), &gm, gammaModes, IM_ARRAYSIZE(gammaModes))) {
            g_config.mirrorGammaMode = static_cast<MirrorGammaMode>(gm);
            g_configIsDirty = true;

            SetGlobalMirrorGammaMode(g_config.mirrorGammaMode);

            std::unique_lock<std::shared_mutex> lock(g_mirrorInstancesMutex);
            for (auto& kv : g_mirrorInstances) {
                kv.second.forceUpdateFrames = 3;
                kv.second.hasValidContent = false;
            }
        }
        ImGui::SameLine();
        HelpMarker(trc("settings.tooltip.colorspace"));
    };

    SliderCtrlClickTip();

    ImGui::Spacing();
    ImGui::SeparatorText(trc("settings.capture_streaming"));
    if (ImGui::Checkbox(trc("settings.hide_animations_in_game"), &g_config.hideAnimationsInGame)) { g_configIsDirty = true; }
    ImGui::SameLine();
    HelpMarker(trc("tooltip.hide_animations_in_game"));

    ImGui::Spacing();
    ImGui::SeparatorText(trc("hotkeys.window_hotkeys"));

    ImGui::PushID("settings_borderless_toggle");
    {
        HWND hwnd = g_minecraftHwnd.load(std::memory_order_relaxed);
        const bool canToggleBorderless = (hwnd != NULL && IsWindow(hwnd));

        ImGui::Text(trc("label.toggle_borderless"));
        ImGui::SameLine();

        if (!canToggleBorderless) { ImGui::BeginDisabled(); }
        if (ImGui::Button(trc("general.go_borderless"), ImVec2(150, 0))) {
            ToggleBorderlessWindowedFullscreen(hwnd);
        }
        if (!canToggleBorderless) { ImGui::EndDisabled(); }
        ImGui::SameLine();
        HelpMarker(trc("tooltip.toggle_borderless"));
    }
    ImGui::PopID();

    ImGui::PushID("settings_auto_borderless");
    if (ImGui::Checkbox(trc("settings.auto_borderless"), &g_config.autoBorderless)) { g_configIsDirty = true; }
    ImGui::SameLine();
    HelpMarker(trc("tooltip.auto_borderless"));
    ImGui::PopID();

    ImGui::Spacing();
    ImGui::SeparatorText(trc("config_mode.advanced"));
    if (ImGui::Checkbox(trc("settings.restore_windowed_mode_on_fullscreen_exit"), &g_config.restoreWindowedModeOnFullscreenExit)) {
        g_configIsDirty = true;
    }
    ImGui::SameLine();
    HelpMarker(trc("settings.tooltip.restore_windowed_mode_on_fullscreen_exit"));

    ImGui::Spacing();
    ImGui::SeparatorText(trc("settings.performance"));
    ImGui::Text(trc("label.fps_limit"));
    ImGui::SetNextItemWidth(300);
    int fpsLimitValue = (g_config.fpsLimit == 0) ? 1001 : g_config.fpsLimit;
    if (ImGui::SliderInt("##fpsLimit", &fpsLimitValue, 30, 1001, fpsLimitValue == 1001 ? trc("label.unlimited") : "%d fps")) {
        g_config.fpsLimit = (fpsLimitValue == 1001) ? 0 : fpsLimitValue;
        g_configIsDirty = true;
    }
    ImGui::SameLine();
    HelpMarker(trc("tooltip.fps_limit.advanced"));

/*    if (ImGui::Checkbox("Disable Fullscreen Prompt", &g_config.disableFullscreenPrompt)) { g_configIsDirty = true; }
    ImGui::SameLine();
    HelpMarker("Disables the fullscreen toast prompt (toast2).\n"
               "When disabled, toast2 appears in fullscreen and starts fading out after 10 seconds.");

    if (ImGui::Checkbox("Disable Configure Prompt", &g_config.disableConfigurePrompt)) { g_configIsDirty = true; }
    ImGui::SameLine();
    HelpMarker("Disables the configure toast prompt (toast1) shown in windowed mode.");*/

    bool driverInstalled = IsVirtualCameraDriverInstalled();
    bool inUseByOBS = driverInstalled && IsVirtualCameraInUseByOBS();
    ImGui::BeginDisabled(!driverInstalled || inUseByOBS);
    bool vcEnabled = g_config.debug.virtualCameraEnabled;
    if (ImGui::Checkbox(trc("settings.enable_virtual_camera"), &vcEnabled)) {
        g_config.debug.virtualCameraEnabled = vcEnabled;
        g_configIsDirty = true;
        if (vcEnabled) {
            uint32_t vcWidth = 0;
            uint32_t vcHeight = 0;
            if (GetPreferredVirtualCameraResolution(vcWidth, vcHeight)) {
                StartVirtualCamera(vcWidth, vcHeight);
            }
        } else {
            StopVirtualCamera();
        }
    }

    ImGui::EndDisabled();
    ImGui::SameLine();
    if (!driverInstalled) {
        ImGui::TextDisabled(trc("settings.virtual.camera_not_installed"));
    } else if (inUseByOBS) {
        ImGui::TextDisabled(trc("settings.virtual.camera_in_use"));
    } else {
        HelpMarker(trc("settings.tooltip.virtual_camera"));
    }

    ImGui::Spacing();
    ImGui::SetNextItemWidth(300);
    int videoCacheBudgetMiB = g_config.debug.videoCacheBudgetMiB;
    if (ImGui::SliderInt(trc("settings.video_cache_budget_mib"), &videoCacheBudgetMiB, 0, 2048,
                         videoCacheBudgetMiB == 0 ? trc("label.disabled") : "%d MiB")) {
        g_config.debug.videoCacheBudgetMiB = videoCacheBudgetMiB;
        g_configIsDirty = true;
    }
    ImGui::SameLine();
    HelpMarker(trc("settings.tooltip.video_cache_budget_mib"));

    ImGui::Spacing();

    static bool s_debugUnlocked = false;
    static char s_passcodeInput[16] = "";

    if (!s_debugUnlocked) {
        if (ImGui::Button(trc("button.debug"))) {
            ImGui::OpenPopup(trc("settings.debug_passcode"));
            memset(s_passcodeInput, 0, sizeof(s_passcodeInput));
        }

        ImGuiIO& io = ImGui::GetIO();
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Appearing,
                                ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal(trc("settings.debug_passcode"), NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text(trc("settings.debug_passcode_prompt"));
            ImGui::Spacing();

            ImGui::SetNextItemWidth(150);
            bool enterPressed = ImGui::InputText("##passcode", s_passcodeInput, sizeof(s_passcodeInput),
                                                 ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue);

            ImGui::Spacing();

            if (ImGui::Button(trc("button.ok"), ImVec2(80, 0)) || enterPressed) {
                if (strcmp(s_passcodeInput, "5739") == 0) {
                    s_debugUnlocked = true;
                    ImGui::CloseCurrentPopup();
                } else {
                    memset(s_passcodeInput, 0, sizeof(s_passcodeInput));
                }
            }
            ImGui::SameLine();
            if (ImGui::Button(trc("button.cancel"), ImVec2(80, 0))) { ImGui::CloseCurrentPopup(); }

            ImGui::EndPopup();
        }
    } else {
        struct MpegVideoTextureDebugStats {
            size_t uploadedVramBytes = 0;
            size_t uploadedClipCount = 0;
            size_t uploadedTextureCount = 0;
        };

        auto collectMpegVideoTextureDebugStats = []() {
            MpegVideoTextureDebugStats stats;

            auto tryMultiply = [](size_t left, size_t right, size_t& out) {
                if (left == 0 || right == 0) {
                    out = 0;
                    return true;
                }
                if (left > (std::numeric_limits<size_t>::max)() / right) {
                    return false;
                }
                out = left * right;
                return true;
            };

            auto tryComputeRgbaBytes = [&tryMultiply](int width, int height, size_t& outBytes) {
                if (width <= 0 || height <= 0) {
                    return false;
                }

                size_t pixelCount = 0;
                if (!tryMultiply(static_cast<size_t>(width), static_cast<size_t>(height), pixelCount)) {
                    return false;
                }
                return tryMultiply(pixelCount, 4, outBytes);
            };

            auto computeTextureBytes = [&tryComputeRgbaBytes](int width, int height) {
                if (width <= 0 || height <= 0) {
                    return static_cast<size_t>(0);
                }

                size_t textureBytes = 0;
                if (!tryComputeRgbaBytes(width, height, textureBytes)) {
                    return static_cast<size_t>(0);
                }
                return textureBytes;
            };

            auto computeInstanceTextureBytes = [&computeTextureBytes](const auto& inst) {
                size_t totalBytes = 0;
                if (inst.isAnimated && !inst.frameTextures.empty()) {
                    if (!inst.frameTextureHeights.empty()) {
                        for (int textureHeight : inst.frameTextureHeights) {
                            totalBytes += computeTextureBytes(inst.width, textureHeight);
                        }
                        return totalBytes;
                    }

                    const int textureHeight = inst.textureStorageHeight > 0 ? inst.textureStorageHeight : inst.height;
                    return computeTextureBytes(inst.width, textureHeight) * inst.frameTextures.size();
                }

                if (inst.textureId == 0) {
                    return static_cast<size_t>(0);
                }

                const int textureHeight = inst.textureStorageHeight > 0 ? inst.textureStorageHeight : inst.height;
                return computeTextureBytes(inst.width, textureHeight);
            };

            {
                std::lock_guard<std::mutex> lock(g_backgroundTexturesMutex);
                for (const auto& [id, inst] : g_backgroundTextures) {
                    (void)id;
                    if (!inst.isVideo) {
                        continue;
                    }

                    const size_t textureCount = inst.isAnimated ? inst.frameTextures.size() : (inst.textureId != 0 ? 1u : 0u);
                    if (textureCount == 0) {
                        continue;
                    }

                    stats.uploadedClipCount += 1;
                    stats.uploadedTextureCount += textureCount;
                    stats.uploadedVramBytes += computeInstanceTextureBytes(inst);
                }
            }

            {
                std::lock_guard<std::mutex> lock(g_userImagesMutex);
                for (const auto& [id, inst] : g_userImages) {
                    (void)id;
                    if (!inst.isVideo) {
                        continue;
                    }

                    const size_t textureCount = inst.isAnimated ? inst.frameTextures.size() : (inst.textureId != 0 ? 1u : 0u);
                    if (textureCount == 0) {
                        continue;
                    }

                    stats.uploadedClipCount += 1;
                    stats.uploadedTextureCount += textureCount;
                    stats.uploadedVramBytes += computeInstanceTextureBytes(inst);
                }
            }

            return stats;
        };

        ImGui::SeparatorText(trc("settings.debug_options"));
        drawMirrorColorspaceSetting();
        ImGui::Spacing();
        if (ImGui::Checkbox(trc("settings.limit_capture_framerate"), &g_config.limitCaptureFramerate)) { g_configIsDirty = true; }
        ImGui::SameLine();
        HelpMarker(trc("settings.tooltip.limit_capture_framerate"));
        ImGui::Spacing();
        if (ImGui::Checkbox(trc("settings.delay_rendering_until_finished"), &g_config.debug.delayRenderingUntilFinished)) { g_configIsDirty = true; }
        ImGui::SameLine();
        HelpMarker(trc("settings.tooltip.delay_rendering_until_finished"));
        ImGui::Spacing();
        if (ImGui::Checkbox(trc("settings.show_performance_overlay"), &g_config.debug.showPerformanceOverlay)) { g_configIsDirty = true; }
        if (ImGui::Checkbox(trc("settings.show_profiler"), &g_config.debug.showProfiler)) { g_configIsDirty = true; }
        ImGui::SetNextItemWidth(300);
        if (ImGui::SliderFloat(trc("settings.profiler_scale"), &g_config.debug.profilerScale, 0.25f, 2.0f, "%.2f")) { g_configIsDirty = true; }
        ImGui::SameLine();
        HelpMarker(trc("settings.tooltip.profiler_scale"));
        if (ImGui::Checkbox(trc("settings.show_hotkey_debug"), &g_config.debug.showHotkeyDebug)) { g_configIsDirty = true; }
        if (ImGui::Checkbox(trc("settings.fake_cursor_overlay"), &g_config.debug.fakeCursor)) { g_configIsDirty = true; }
        ImGui::SameLine();
        HelpMarker(trc("settings.tooltip.fake_cursor"));
        if (ImGui::Checkbox(trc("settings.show_texture_grid"), &g_config.debug.showTextureGrid)) { g_configIsDirty = true; }
        ImGui::SameLine();
        HelpMarker(trc("settings.tooltip.show_texture_grid"));

        ImGui::Spacing();
        ImGui::SeparatorText(trc("settings.debug_mpeg_video_memory"));
        const MpegVideoTextureDebugStats mpegVideoStats = collectMpegVideoTextureDebugStats();
        if (mpegVideoStats.uploadedClipCount == 0) {
            ImGui::TextDisabled(trc("settings.debug_mpeg_video_memory_empty"));
        } else {
            ImGui::Text("%s %zu", trc("settings.debug_mpeg_video_memory_uploaded_clips"), mpegVideoStats.uploadedClipCount);
            ImGui::Text("%s %zu", trc("settings.debug_mpeg_video_memory_uploaded_textures"), mpegVideoStats.uploadedTextureCount);
            ImGui::Text("%s %.2f MiB (%llu bytes)", trc("settings.debug_mpeg_video_memory_vram"),
                        static_cast<double>(mpegVideoStats.uploadedVramBytes) / (1024.0 * 1024.0),
                        static_cast<unsigned long long>(mpegVideoStats.uploadedVramBytes));
            ImGui::TextDisabled(trc("settings.debug_mpeg_video_memory_note"));
        }

        ImGui::Spacing();
        if (ImGui::CollapsingHeader(trc("settings.advanced_logging"))) {
            ImGui::Indent();
            ImGui::TextDisabled(trc("settings.enable_verbose_logging"));
            ImGui::Spacing();
            if (ImGui::Checkbox(trc("settings.log_mode_switch"), &g_config.debug.logModeSwitch)) { g_configIsDirty = true; }
            if (ImGui::Checkbox(trc("settings.log_animation"), &g_config.debug.logAnimation)) { g_configIsDirty = true; }
            if (ImGui::Checkbox(trc("settings.log_hotkey"), &g_config.debug.logHotkey)) { g_configIsDirty = true; }
            if (ImGui::Checkbox(trc("settings.log_obs"), &g_config.debug.logObs)) { g_configIsDirty = true; }
            if (ImGui::Checkbox(trc("settings.log_window_overlay"), &g_config.debug.logWindowOverlay)) { g_configIsDirty = true; }
            if (ImGui::Checkbox(trc("settings.log_file_monitor"), &g_config.debug.logFileMonitor)) { g_configIsDirty = true; }
            if (ImGui::Checkbox(trc("settings.log_image_monitor"), &g_config.debug.logImageMonitor)) { g_configIsDirty = true; }
            if (ImGui::Checkbox(trc("settings.log_performance"), &g_config.debug.logPerformance)) { g_configIsDirty = true; }
            if (ImGui::Checkbox(trc("settings.log_texture_ops"), &g_config.debug.logTextureOps)) { g_configIsDirty = true; }
            if (ImGui::Checkbox(trc("settings.log_gui"), &g_config.debug.logGui)) { g_configIsDirty = true; }
            if (ImGui::Checkbox(trc("settings.log_init"), &g_config.debug.logInit)) { g_configIsDirty = true; }
            if (ImGui::Checkbox(trc("settings.log_cursor_textures"), &g_config.debug.logCursorTextures)) { g_configIsDirty = true; }
            ImGui::Unindent();
        }
    }
    ImGui::EndTabItem();
}


