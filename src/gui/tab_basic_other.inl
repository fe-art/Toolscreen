if (ImGui::BeginTabItem(TR("Other"))) {
    g_currentlyEditingMirror = "";
    g_imageDragMode.store(false);
    g_windowOverlayDragMode.store(false);

    SliderCtrlClickTip();

    // --- GUI HOTKEY SECTION ---
    ImGui::SeparatorText(TR("GUI Hotkey"));
    ImGui::PushID("basic_gui_hotkey");
    std::string guiKeyStr = GetKeyComboString(g_config.guiHotkey);

    ImGui::Text(TR("Open/Close GUI:"));
    ImGui::SameLine();

    bool isBindingGui = (s_mainHotkeyToBind == -999);
    const char* guiButtonLabel = isBindingGui ? "[Press Keys...]" : (guiKeyStr.empty() ? "[Click to Bind]" : guiKeyStr.c_str());
    if (ImGui::Button(guiButtonLabel, ImVec2(150, 0))) {
        s_mainHotkeyToBind = -999; // Special ID for GUI hotkey
        s_altHotkeyToBind = { -1, -1 };
        s_exclusionToBind = { -1, -1 };
            MarkHotkeyBindingActive();
    }
    ImGui::PopID();

    // --- OVERLAY VISIBILITY HOTKEYS ---
    ImGui::SeparatorText("Overlay Visibility Hotkeys");

    // Image overlays
    ImGui::PushID("basic_image_overlay_toggle_hotkey");
    {
        const bool imgOverlaysVisible = g_imageOverlaysVisible.load(std::memory_order_acquire);
        const ImVec4 visibleGreen = ImVec4(0.20f, 1.00f, 0.20f, 1.00f);
        const ImVec4 hiddenRed = ImVec4(1.00f, 0.20f, 0.20f, 1.00f);

        std::string imgKeyStr = GetKeyComboString(g_config.imageOverlaysHotkey);
        ImGui::Text("Toggle Image Overlays:");
        ImGui::SameLine();
        const bool isBindingImg = (s_mainHotkeyToBind == -997);
        const char* imgBtnLabel = isBindingImg ? "[Press Keys...]" : (imgKeyStr.empty() ? "[Click to Bind]" : imgKeyStr.c_str());
        if (ImGui::Button(imgBtnLabel, ImVec2(150, 0))) {
            s_mainHotkeyToBind = -997;
            s_altHotkeyToBind = { -1, -1 };
            s_exclusionToBind = { -1, -1 };
                MarkHotkeyBindingActive();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Toggles visibility of all Image Overlays.");
        }

        ImGui::SameLine();
        ImGui::TextDisabled("Status:");
        ImGui::SameLine();
        ImGui::TextColored(imgOverlaysVisible ? visibleGreen : hiddenRed, "%s", imgOverlaysVisible ? "Shown" : "Hidden");
    }
    ImGui::PopID();

    // Window overlays
    ImGui::PushID("basic_window_overlay_toggle_hotkey");
    {
        const bool winOverlaysVisible = g_windowOverlaysVisible.load(std::memory_order_acquire);
        const ImVec4 visibleGreen = ImVec4(0.20f, 1.00f, 0.20f, 1.00f);
        const ImVec4 hiddenRed = ImVec4(1.00f, 0.20f, 0.20f, 1.00f);

        std::string winKeyStr = GetKeyComboString(g_config.windowOverlaysHotkey);
        ImGui::Text("Toggle Window Overlays:");
        ImGui::SameLine();
        const bool isBindingWin = (s_mainHotkeyToBind == -996);
        const char* winBtnLabel = isBindingWin ? "[Press Keys...]" : (winKeyStr.empty() ? "[Click to Bind]" : winKeyStr.c_str());
        if (ImGui::Button(winBtnLabel, ImVec2(150, 0))) {
            s_mainHotkeyToBind = -996;
            s_altHotkeyToBind = { -1, -1 };
            s_exclusionToBind = { -1, -1 };
                MarkHotkeyBindingActive();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Toggles visibility of all Window Overlays. When hidden, interaction forwarding is disabled.");
        }

        ImGui::SameLine();
        ImGui::TextDisabled("Status:");
        ImGui::SameLine();
        ImGui::TextColored(winOverlaysVisible ? visibleGreen : hiddenRed, "%s", winOverlaysVisible ? "Shown" : "Hidden");
    }
    ImGui::PopID();

    // --- BORDERLESS TOGGLE HOTKEY SECTION ---
    ImGui::SeparatorText("Window Hotkeys");
    ImGui::PushID("basic_borderless_hotkey");
    std::string borderlessKeyStr = GetKeyComboString(g_config.borderlessHotkey);

    ImGui::Text("Toggle Borderless:");
    ImGui::SameLine();

    bool isBindingBorderless = (s_mainHotkeyToBind == -998);
    const char* borderlessButtonLabel =
        isBindingBorderless ? "[Press Keys...]" : (borderlessKeyStr.empty() ? "[Click to Bind]" : borderlessKeyStr.c_str());
    if (ImGui::Button(borderlessButtonLabel, ImVec2(150, 0))) {
        s_mainHotkeyToBind = -998; // Special ID for borderless toggle hotkey
        s_altHotkeyToBind = { -1, -1 };
        s_exclusionToBind = { -1, -1 };
            MarkHotkeyBindingActive();
    }
    ImGui::SameLine();
    HelpMarker("Toggles the game window between its previous windowed size and a borderless, monitor-sized window.");
    ImGui::PopID();

    // --- DISPLAY SETTINGS ---
    ImGui::SeparatorText(TR("Display Settings"));

    ImGui::Text(TR("FPS Limit:"));
    ImGui::SetNextItemWidth(300);
    int fpsLimitValue = (g_config.fpsLimit == 0) ? 1001 : g_config.fpsLimit;
    if (ImGui::SliderInt("##FpsLimit", &fpsLimitValue, 30, 1001, fpsLimitValue == 1001 ? TR("Unlimited") : "%d fps")) {
        g_config.fpsLimit = (fpsLimitValue == 1001) ? 0 : fpsLimitValue;
        g_configIsDirty = true;
    }
    ImGui::SameLine();
    HelpMarker(TR("Limits the game's maximum frame rate.\n"
               "Lower FPS can reduce GPU load and power consumption."));

    if (ImGui::Checkbox(TR("Hide animations in game"), &g_config.hideAnimationsInGame)) { g_configIsDirty = true; }
    ImGui::SameLine();
    HelpMarker(TR("When enabled, mode transitions appear instant on your screen,\n"
               "but OBS Game Capture will show the animations."));

/*    if (ImGui::Checkbox(TR("Disable Fullscreen Prompt"), &g_config.disableFullscreenPrompt)) { g_configIsDirty = true; }
    ImGui::SameLine();
    HelpMarker(TR("Disables the fullscreen toast prompt (toast2).\n"
               "When disabled, toast2 appears in fullscreen and starts fading out after 10 seconds."));

    if (ImGui::Checkbox(TR("Disable Configure Prompt"), &g_config.disableConfigurePrompt)) { g_configIsDirty = true; }
    ImGui::SameLine();
    HelpMarker(TR("Disables the configure toast prompt (toast1) shown in windowed mode."));*/

    // --- FONT SETTINGS ---
    ImGui::SeparatorText(TR("Font"));

    ImGui::Text(TR("Font Path:"));
    ImGui::SetNextItemWidth(300);
    if (ImGui::InputText("##FontPath", &g_config.fontPath)) { g_configIsDirty = true; }
    ImGui::SameLine();
    HelpMarker(TR("Path to a .ttf font file for the GUI. Restart required for changes to take effect."));

    ImGui::EndTabItem();
}
