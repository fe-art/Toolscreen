if (ImGui::BeginTabItem("Other")) {
    g_currentlyEditingMirror = "";

    SliderCtrlClickTip();

    ImGui::SeparatorText("GUI Hotkey");
    ImGui::PushID("basic_gui_hotkey");
    std::string guiKeyStr = GetKeyComboString(g_config.guiHotkey);

    ImGui::Text("Open/Close GUI:");
    ImGui::SameLine();

    bool isBindingGui = (s_mainHotkeyToBind == -999);
    const char* guiButtonLabel = isBindingGui ? "[Press Keys...]" : (guiKeyStr.empty() ? "[Click to Bind]" : guiKeyStr.c_str());
    if (ImGui::Button(guiButtonLabel, ImVec2(150, 0))) {
        s_mainHotkeyToBind = -999;
        s_altHotkeyToBind = { -1, -1 };
        s_exclusionToBind = { -1, -1 };
            MarkHotkeyBindingActive();
    }
    ImGui::PopID();

    ImGui::SeparatorText("Overlay Visibility Hotkeys");

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

    ImGui::SeparatorText("Window Hotkeys");
    ImGui::PushID("basic_borderless_hotkey");
    std::string borderlessKeyStr = GetKeyComboString(g_config.borderlessHotkey);

    ImGui::Text("Toggle Borderless:");
    ImGui::SameLine();

    bool isBindingBorderless = (s_mainHotkeyToBind == -998);
    const char* borderlessButtonLabel =
        isBindingBorderless ? "[Press Keys...]" : (borderlessKeyStr.empty() ? "[Click to Bind]" : borderlessKeyStr.c_str());
    if (ImGui::Button(borderlessButtonLabel, ImVec2(150, 0))) {
        s_mainHotkeyToBind = -998;
        s_altHotkeyToBind = { -1, -1 };
        s_exclusionToBind = { -1, -1 };
            MarkHotkeyBindingActive();
    }
    ImGui::SameLine();
    HelpMarker("Toggles the game window between its previous windowed size and a borderless, monitor-sized window.");
    ImGui::PopID();

    {
        ImGui::PushID("basic_auto_borderless");
        ImGui::Text("Auto-Borderless:");
        ImGui::SameLine();
        const char* label = g_config.autoBorderless ? "Enabled" : "Disabled";
        if (ImGui::Button(label, ImVec2(150, 0))) {
            g_config.autoBorderless = !g_config.autoBorderless;
            g_configIsDirty = true;
        }
        ImGui::SameLine();
        HelpMarker("Automatically puts Minecraft in borderless mode when the window is detected on startup.");
        ImGui::PopID();
    }

    ImGui::SeparatorText("Display Settings");

    ImGui::Text("FPS Limit:");
    ImGui::SetNextItemWidth(300);
    int fpsLimitValue = (g_config.fpsLimit == 0) ? 1001 : g_config.fpsLimit;
    if (ImGui::SliderInt("##FpsLimit", &fpsLimitValue, 30, 1001, fpsLimitValue == 1001 ? "Unlimited" : "%d fps")) {
        g_config.fpsLimit = (fpsLimitValue == 1001) ? 0 : fpsLimitValue;
        g_configIsDirty = true;
    }
    ImGui::SameLine();
    HelpMarker("Limits the game's maximum frame rate.\n"
               "Lower FPS can reduce GPU load and power consumption.");

    if (ImGui::Checkbox("Hide animations in game", &g_config.hideAnimationsInGame)) { g_configIsDirty = true; }
    ImGui::SameLine();
    HelpMarker("When enabled, mode transitions appear instant on your screen,\n"
               "but OBS Game Capture will show the animations.");

/*    if (ImGui::Checkbox("Disable Fullscreen Prompt", &g_config.disableFullscreenPrompt)) { g_configIsDirty = true; }
    ImGui::SameLine();
    HelpMarker("Disables the fullscreen toast prompt (toast2).\n"
               "When disabled, toast2 appears in fullscreen and starts fading out after 10 seconds.");

    if (ImGui::Checkbox("Disable Configure Prompt", &g_config.disableConfigurePrompt)) { g_configIsDirty = true; }
    ImGui::SameLine();
    HelpMarker("Disables the configure toast prompt (toast1) shown in windowed mode.");*/

    ImGui::SeparatorText("Font");

    ImGui::Text("Font Path:");
    ImGui::SetNextItemWidth(300);
    if (ImGui::InputText("##FontPath", &g_config.fontPath)) { g_configIsDirty = true; }
    ImGui::SameLine();
    HelpMarker("Path to a .ttf font file for the GUI. Restart required for changes to take effect.");

    ImGui::EndTabItem();
}


