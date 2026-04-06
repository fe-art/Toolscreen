if (BeginSelectableSettingsTopTabItem(trc("tabs.other"))) {
    g_currentlyEditingMirror = "";
    g_imageDragMode.store(false);
    g_windowOverlayDragMode.store(false);

    SliderCtrlClickTip();

    ImGui::SeparatorText(trc("hotkeys.gui_hotkey"));
    ImGui::PushID("basic_gui_hotkey");
    std::string guiKeyStr = GetKeyComboString(g_config.guiHotkey);

    ImGui::Text(trc("hotkeys.gui_hotkey_open_close"));
    ImGui::SameLine();

    bool isBindingGui = (s_mainHotkeyToBind == -999);
    const char* guiButtonLabel = isBindingGui ? trc("hotkeys.press_keys") : (guiKeyStr.empty() ? trc("hotkeys.click_to_bind") : guiKeyStr.c_str());
    if (ImGui::Button(guiButtonLabel, ImVec2(150, 0))) {
        s_mainHotkeyToBind = -999;
        s_altHotkeyToBind = { -1, -1 };
        s_exclusionToBind = { -1, -1 };
            MarkHotkeyBindingActive();
    }
    ImGui::PopID();

    ImGui::SeparatorText(trc("label.overlay_visibility_hotkeys"));

    ImGui::PushID("basic_image_overlay_toggle_hotkey");
    {
        const bool imgOverlaysVisible = g_imageOverlaysVisible.load(std::memory_order_acquire);
        const ImVec4 visibleGreen = ImVec4(0.20f, 1.00f, 0.20f, 1.00f);
        const ImVec4 hiddenRed = ImVec4(1.00f, 0.20f, 0.20f, 1.00f);

        std::string imgKeyStr = GetKeyComboString(g_config.imageOverlaysHotkey);
        ImGui::Text(trc("label.toggle_image_overlays"));
        ImGui::SameLine();
        const bool isBindingImg = (s_mainHotkeyToBind == -997);
        const char* imgBtnLabel = isBindingImg ? trc("hotkeys.press_keys") : (imgKeyStr.empty() ? trc("hotkeys.click_to_bind") : imgKeyStr.c_str());
        if (ImGui::Button(imgBtnLabel, ImVec2(150, 0))) {
            s_mainHotkeyToBind = -997;
            s_altHotkeyToBind = { -1, -1 };
            s_exclusionToBind = { -1, -1 };
                MarkHotkeyBindingActive();
        }
        ImGui::SameLine();
        ImGui::TextDisabled(trc("label.question_mark"));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(trc("tooltip.toggle_image_overlays.basic"));
        }

        ImGui::SameLine();
        ImGui::TextDisabled(trc("label.status"));
        ImGui::SameLine();
        ImGui::TextColored(imgOverlaysVisible ? visibleGreen : hiddenRed, "%s", imgOverlaysVisible ? trc("label.shown") : trc("label.hidden"));
    }
    ImGui::PopID();

    ImGui::PushID("basic_window_overlay_toggle_hotkey");
    {
        const bool winOverlaysVisible = g_windowOverlaysVisible.load(std::memory_order_acquire);
        const ImVec4 visibleGreen = ImVec4(0.20f, 1.00f, 0.20f, 1.00f);
        const ImVec4 hiddenRed = ImVec4(1.00f, 0.20f, 0.20f, 1.00f);

        std::string winKeyStr = GetKeyComboString(g_config.windowOverlaysHotkey);
        ImGui::Text(trc("label.toggle_window_overlays"));
        ImGui::SameLine();
        const bool isBindingWin = (s_mainHotkeyToBind == -996);
        const char* winBtnLabel = isBindingWin ? trc("hotkeys.press_keys") : (winKeyStr.empty() ? trc("hotkeys.click_to_bind") : winKeyStr.c_str());
        if (ImGui::Button(winBtnLabel, ImVec2(150, 0))) {
            s_mainHotkeyToBind = -996;
            s_altHotkeyToBind = { -1, -1 };
            s_exclusionToBind = { -1, -1 };
                MarkHotkeyBindingActive();
        }
        ImGui::SameLine();
        ImGui::TextDisabled(trc("label.question_mark"));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(trc("tooltip.toggle_window_overlays.basic"));
        }

        ImGui::SameLine();
        ImGui::TextDisabled(trc("label.status"));
        ImGui::SameLine();
        ImGui::TextColored(winOverlaysVisible ? visibleGreen : hiddenRed, "%s", winOverlaysVisible ? trc("label.shown") : trc("label.hidden"));
    }
    ImGui::PopID();

    ImGui::PushID("basic_ninjabrain_overlay_toggle_hotkey");
    {
        const bool ninjabrainOverlayVisible = g_ninjabrainOverlayVisible.load(std::memory_order_acquire);
        const ImVec4 visibleGreen = ImVec4(0.20f, 1.00f, 0.20f, 1.00f);
        const ImVec4 hiddenRed = ImVec4(1.00f, 0.20f, 0.20f, 1.00f);

        std::string ninjabrainKeyStr = GetKeyComboString(g_config.ninjabrainOverlayHotkey);
        ImGui::Text(trc("label.toggle_ninjabrain_overlay"));
        ImGui::SameLine();
        const bool isBindingNinjabrain = (s_mainHotkeyToBind == -994);
        const char* ninjabrainBtnLabel =
            isBindingNinjabrain ? trc("hotkeys.press_keys") : (ninjabrainKeyStr.empty() ? trc("hotkeys.click_to_bind") : ninjabrainKeyStr.c_str());
        if (ImGui::Button(ninjabrainBtnLabel, ImVec2(150, 0))) {
            s_mainHotkeyToBind = -994;
            s_altHotkeyToBind = { -1, -1 };
            s_exclusionToBind = { -1, -1 };
                MarkHotkeyBindingActive();
        }
        ImGui::SameLine();
        ImGui::TextDisabled(trc("label.question_mark"));
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip(trc("tooltip.toggle_ninjabrain_overlay.basic"));
        }

        ImGui::SameLine();
        ImGui::TextDisabled(trc("label.status"));
        ImGui::SameLine();
        ImGui::TextColored(ninjabrainOverlayVisible ? visibleGreen : hiddenRed, "%s",
                           ninjabrainOverlayVisible ? trc("label.shown") : trc("label.hidden"));
    }
    ImGui::PopID();

    ImGui::SeparatorText(trc("hotkeys.window_hotkeys"));
    ImGui::PushID("basic_borderless_toggle");
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
    ImGui::PopID();

    {
        ImGui::PushID("basic_auto_borderless");
        if (ImGui::Checkbox(trc("settings.auto_borderless"), &g_config.autoBorderless)) { g_configIsDirty = true; }
        ImGui::SameLine();
        HelpMarker(trc("tooltip.auto_borderless"));
        ImGui::PopID();
    }

    ImGui::SeparatorText(trc("label.display_settings"));

    if (ImGui::Checkbox(trc("label.hide_animations_in_game"), &g_config.hideAnimationsInGame)) { g_configIsDirty = true; }
    ImGui::SameLine();
    HelpMarker(trc("tooltip.hide_animations_in_game"));

/*    if (ImGui::Checkbox("Disable Fullscreen Prompt", &g_config.disableFullscreenPrompt)) { g_configIsDirty = true; }
    ImGui::SameLine();
    HelpMarker("Disables the fullscreen toast prompt (toast2).\n"
               "When disabled, toast2 appears in fullscreen and starts fading out after 10 seconds.");

    if (ImGui::Checkbox("Disable Configure Prompt", &g_config.disableConfigurePrompt)) { g_configIsDirty = true; }
    ImGui::SameLine();
    HelpMarker("Disables the configure toast prompt (toast1) shown in windowed mode.");*/

    ImGui::SeparatorText(trc("label.font"));

    const std::vector<FontPickerOption> mainGuiFontOptions = BuildFontPickerOptions();
    auto applyMainGuiFontChange = []() {
        g_configIsDirty = true;
        RequestDynamicGuiFontRefresh(true);
    };

    ImGui::Text(trc("label.font"));
    const bool usingCustomMainGuiFont = RenderFontPickerCombo("##MainGuiFontChoice", 300.0f, mainGuiFontOptions, g_config.fontPath,
                                                              s_mainGuiFontPickerState, applyMainGuiFontChange);
    ImGui::SameLine();
    HelpMarker(trc("tooltip.font"));

    if (usingCustomMainGuiFont) {
        ImGui::Text(trc("label.font_path"));
        RenderCustomFontPathEditor("##FontPath", "##MainGuiFont", 300.0f, mainGuiFontOptions, g_config.fontPath,
                                   s_mainGuiFontPickerState, "Select Main GUI Font", applyMainGuiFontChange);
    }

    ImGui::Text(trc("label.scale"));
    ImGui::SetNextItemWidth(160);
    if (ImGui::SliderFloat("##GuiFontScale", &g_config.appearance.guiFontScale, 0.75f, 2.0f, "%.2fx")) {
        g_config.appearance.guiFontScale = std::clamp(g_config.appearance.guiFontScale, 0.75f, 2.0f);
        g_configIsDirty = true;
        RequestDynamicGuiFontRefresh(true);
    }
    ImGui::SameLine();
    HelpMarker(trc("tooltip.gui_font_scale"));

    ImGui::EndTabItem();
}


