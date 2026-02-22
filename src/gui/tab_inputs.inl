if (ImGui::BeginTabItem("Inputs")) {
    g_currentlyEditingMirror = "";
    g_imageDragMode.store(false);
    g_windowOverlayDragMode.store(false);

    // Sub-tabs for Mouse and Keyboard
    if (ImGui::BeginTabBar("InputsSubTabs")) {
        // =====================================================================
        // MOUSE SUB-TAB
        // =====================================================================
        if (ImGui::BeginTabItem("Mouse")) {
            SliderCtrlClickTip();

            ImGui::SeparatorText("Mouse Settings");

            ImGui::Text("Mouse Sensitivity:");
            ImGui::SetNextItemWidth(600);
            if (ImGui::SliderFloat("##mouseSensitivity", &g_config.mouseSensitivity, 0.001f, 10.0f, "%.3fx")) { g_configIsDirty = true; }
            ImGui::SameLine();
            HelpMarker("Multiplies mouse movement for raw input events (mouselook).\n"
                       "1.0 = normal sensitivity, higher = faster, lower = slower.\n"
                       "Useful for adjusting mouse speed when using stretched resolutions.");

            ImGui::Text("Windows Mouse Speed:");
            ImGui::SetNextItemWidth(600);
            int windowsSpeedValue = g_config.windowsMouseSpeed;
            if (ImGui::SliderInt("##windowsMouseSpeed", &windowsSpeedValue, 0, 20, windowsSpeedValue == 0 ? "Disabled" : "%d")) {
                g_config.windowsMouseSpeed = windowsSpeedValue;
                g_configIsDirty = true;
            }
            ImGui::SameLine();
            HelpMarker("Temporarily overrides Windows mouse speed setting while game is running.\n"
                       "0 = Disabled (use system setting)\n"
                       "1-20 = Override Windows mouse speed (10 = default Windows speed)\n"
                       "Affects cursor movement in game menus. Original setting is restored on exit.");

            if (g_gameVersion < GameVersion(1, 13, 0)) {
                if (ImGui::Checkbox("Let Cursor Escape Window", &g_config.allowCursorEscape)) { g_configIsDirty = true; }
                ImGui::SameLine();
                HelpMarker("For pre 1.13, prevents the cursor being locked to the game window when in fullscreen");
            }

            ImGui::Spacing();
            ImGui::SeparatorText("Cursor Configuration");

            if (ImGui::Checkbox("Enable Custom Cursors", &g_config.cursors.enabled)) {
                g_configIsDirty = true;
                // Schedule cursor reload (will happen outside GUI rendering to avoid deadlock)
                g_cursorsNeedReload = true;
            }
            ImGui::SameLine();
            HelpMarker("When enabled, the mouse cursor will change based on the current game state.");

            ImGui::Spacing();

            if (g_config.cursors.enabled) {
                ImGui::Text("Configure cursors for different game states:");
                ImGui::Spacing();

                // Available cursor options
                struct CursorOption {
                    std::string key;
                    std::string name;
                    std::string description;
                };

                // Build cursor list dynamically from detected cursors
                static std::vector<CursorOption> availableCursors;
                static bool cursorListInitialized = false;

                if (!cursorListInitialized) {
                    // Initialize cursor definitions first
                    CursorTextures::InitializeCursorDefinitions();

                    // Get all available cursor names
                    auto cursorNames = CursorTextures::GetAvailableCursorNames();

                    // Build display list from available cursors
                    for (const auto& cursorName : cursorNames) {
                        std::string displayName = cursorName;

                        // Create user-friendly name (capitalize first letter, replace underscores/hyphens with spaces)
                        if (!displayName.empty()) {
                            displayName[0] = std::toupper(displayName[0]);
                            for (auto& c : displayName) {
                                if (c == '_' || c == '-') c = ' ';
                            }
                        }

                        std::string description;
                        // Determine description based on cursor name
                        if (cursorName.find("Cross") != std::string::npos) {
                            description = "Crosshair cursor";
                        } else if (cursorName.find("Arrow") != std::string::npos) {
                            description = "Arrow pointer cursor";
                        } else {
                            description = "Custom cursor";
                        }

                        availableCursors.push_back({ cursorName, displayName, description });
                    }

                    cursorListInitialized = true;
                }

                // Fixed 3 cursor configurations
                struct CursorConfigUI {
                    const char* name;
                    CursorConfig* config;
                };

                CursorConfigUI cursors[] = { { "Title Screen", &g_config.cursors.title },
                                             { "Wall", &g_config.cursors.wall },
                                             { "In World", &g_config.cursors.ingame } };

                for (int i = 0; i < 3; ++i) {
                    auto& cursorUI = cursors[i];
                    auto& cursorConfig = *cursorUI.config;
                    ImGui::PushID(i);

                    ImGui::SeparatorText(cursorUI.name);

                    // Find current cursor display name
                    const char* currentCursorName = cursorConfig.cursorName.c_str();
                    std::string currentDescription = "";
                    for (const auto& option : availableCursors) {
                        if (cursorConfig.cursorName == option.key) {
                            currentCursorName = option.name.c_str();
                            currentDescription = option.description;
                            break;
                        }
                    }

                    // Cursor dropdown
                    ImGui::Text("Cursor:");
                    ImGui::SameLine();
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.35f);
                    if (ImGui::BeginCombo("##cursor", currentCursorName)) {
                        // Display each cursor option with name and description
                        for (const auto& option : availableCursors) {
                            ImGui::PushID(option.key.c_str());

                            bool is_selected = (cursorConfig.cursorName == option.key);

                            if (ImGui::Selectable(option.name.c_str(), is_selected)) {
                                cursorConfig.cursorName = option.key;
                                g_configIsDirty = true;
                                // Schedule cursor reload (will happen outside GUI rendering to avoid deadlock)
                                g_cursorsNeedReload = true;

                                // Apply cursor immediately via SetCursor (loads on-demand if needed)
                                std::wstring cursorPath;
                                UINT loadType = IMAGE_CURSOR;
                                CursorTextures::GetCursorPathByName(option.key, cursorPath, loadType);

                                const CursorTextures::CursorData* cursorData =
                                    CursorTextures::LoadOrFindCursor(cursorPath, loadType, cursorConfig.cursorSize);
                                if (cursorData && cursorData->hCursor) { SetCursor(cursorData->hCursor); }
                            }

                            // Show description on hover
                            if (ImGui::IsItemHovered()) {
                                ImGui::BeginTooltip();
                                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.6f, 1.0f), "%s", option.name.c_str());
                                ImGui::Separator();
                                ImGui::TextUnformatted(option.description.c_str());
                                ImGui::EndTooltip();
                            }

                            if (is_selected) { ImGui::SetItemDefaultFocus(); }

                            ImGui::PopID();
                        }
                        ImGui::EndCombo();
                    }

                    // Show current cursor description on hover
                    if (!currentDescription.empty() && ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted(currentDescription.c_str());
                        ImGui::EndTooltip();
                    }

                    // Cursor size slider on the same line
                    ImGui::SameLine();
                    ImGui::Spacing();
                    ImGui::SameLine();
                    ImGui::Text("Size:");
                    ImGui::SameLine();

                    // Slider takes remaining width
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.8f); // Leave space for help marker
                    int sliderValue = cursorConfig.cursorSize;
                    if (ImGui::SliderInt("##cursorSize", &sliderValue, 8, 144, "%d px", ImGuiSliderFlags_AlwaysClamp)) {
                        int newSize = sliderValue;
                        if (newSize != cursorConfig.cursorSize) {
                            cursorConfig.cursorSize = newSize;
                            g_configIsDirty = true;

                            // Apply cursor immediately via SetCursor (loads on-demand if needed)
                            std::wstring cursorPath;
                            UINT loadType = IMAGE_CURSOR;
                            CursorTextures::GetCursorPathByName(cursorConfig.cursorName, cursorPath, loadType);

                            const CursorTextures::CursorData* cursorData = CursorTextures::LoadOrFindCursor(cursorPath, loadType, newSize);
                            if (cursorData && cursorData->hCursor) { SetCursor(cursorData->hCursor); }
                        }
                    }

                    ImGui::PopID();
                }

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                if (ImGui::Button("Reset to Defaults##cursors")) { ImGui::OpenPopup("Reset Cursors to Defaults?"); }

                if (ImGui::BeginPopupModal("Reset Cursors to Defaults?", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
                    ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), "WARNING:");
                    ImGui::Text("This will reset all cursor settings to their default values.");
                    ImGui::Text("This action cannot be undone.");
                    ImGui::Separator();
                    if (ImGui::Button("Confirm Reset", ImVec2(120, 0))) {
                        g_config.cursors = GetDefaultCursors();
                        g_configIsDirty = true;
                        g_cursorsNeedReload = true;
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::SetItemDefaultFocus();
                    ImGui::SameLine();
                    if (ImGui::Button("Cancel", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
                    ImGui::EndPopup();
                }
            }

            ImGui::EndTabItem();
        }

        // =====================================================================
        // KEYBOARD SUB-TAB
        // =====================================================================
        if (ImGui::BeginTabItem("Keyboard")) {
            SliderCtrlClickTip();

            // --- Key Repeat Rate Settings ---
            ImGui::SeparatorText("Key Repeat Rate");

            ImGui::Text("Key Repeat Start Delay:");
            ImGui::SetNextItemWidth(600);
            int startDelayValue = g_config.keyRepeatStartDelay;
            if (ImGui::SliderInt("##keyRepeatStartDelay", &startDelayValue, 0, 500, startDelayValue == 0 ? "Default" : "%d ms")) {
                g_config.keyRepeatStartDelay = startDelayValue;
                g_configIsDirty = true;
                ApplyKeyRepeatSettings();
            }
            ImGui::SameLine();
            HelpMarker("Delay before a held key starts repeating.\n"
                       "0 = Use Windows default, 1-500ms = custom delay.\n"
                       "Only applied while the game window is focused.");

            ImGui::Text("Key Repeat Delay:");
            ImGui::SetNextItemWidth(600);
            int repeatDelayValue = g_config.keyRepeatDelay;
            if (ImGui::SliderInt("##keyRepeatDelay", &repeatDelayValue, 0, 500, repeatDelayValue == 0 ? "Default" : "%d ms")) {
                g_config.keyRepeatDelay = repeatDelayValue;
                g_configIsDirty = true;
                ApplyKeyRepeatSettings();
            }
            ImGui::SameLine();
            HelpMarker("Time between repeated key presses while held.\n"
                       "0 = Use Windows default, 1-500ms = custom delay.\n"
                       "Only applied while the game window is focused.");

            ImGui::Spacing();

            // --- Key Rebinding Section ---
            ImGui::SeparatorText("Key Rebinding");
            ImGui::TextWrapped("Intercept keyboard inputs and remap them before they reach the game.");
            ImGui::Spacing();

            // Master toggle
            if (ImGui::Checkbox("Enable Key Rebinding", &g_config.keyRebinds.enabled)) {
                g_configIsDirty = true;
                std::lock_guard<std::mutex> hotkeyLock(g_hotkeyMainKeysMutex);
                RebuildHotkeyMainKeys_Internal();
            }
            ImGui::SameLine();
            HelpMarker("When enabled, configured key rebinds will intercept keyboard input and send the remapped key to the game instead.");

            if (g_config.keyRebinds.enabled) {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                auto getScanCodeWithExtendedFlag = [](DWORD vk) -> DWORD {
                    DWORD scan = MapVirtualKey(vk, MAPVK_VK_TO_VSC_EX);
                    if (scan == 0) { scan = MapVirtualKey(vk, MAPVK_VK_TO_VSC); }

                    if ((scan & 0xFF00) == 0) {
                        switch (vk) {
                        case VK_LEFT:
                        case VK_RIGHT:
                        case VK_UP:
                        case VK_DOWN:
                        case VK_INSERT:
                        case VK_DELETE:
                        case VK_HOME:
                        case VK_END:
                        case VK_PRIOR:
                        case VK_NEXT:
                        case VK_RCONTROL:
                        case VK_RMENU:
                        case VK_DIVIDE:
                        case VK_NUMLOCK:
                        case VK_SNAPSHOT:
                            if ((scan & 0xFF) != 0) { scan |= 0xE000; }
                            break;
                        default:
                            break;
                        }
                    }

                    return scan;
                };

                // Shared binding state (used by both the list view and the layout popup)
                static int s_rebindFromKeyToBind = -1;          // Index of rebind being bound (from key)
                static int s_rebindOutputVKToBind = -1;         // Index of rebind being bound (base output VK)
                static int s_rebindTextOverrideVKToBind = -1;   // Index of rebind being bound (text override VK)
                static int s_rebindOutputScanToBind = -1;       // Index of rebind being bound (trigger scan code)

                // ---------------------------------------------------------------------
                // Keyboard layout visualization
                // ---------------------------------------------------------------------
                static bool s_keyboardLayoutOpen = false;
                static float s_keyboardLayoutScale = 1.0f;

                if (ImGui::Button("Open Layout")) { s_keyboardLayoutOpen = true; }
                ImGui::SameLine();
                ImGui::SetNextItemWidth(180.0f);
                ImGui::SliderFloat("Scale##keyboardLayout", &s_keyboardLayoutScale, 0.6f, 1.6f, "%.2fx", ImGuiSliderFlags_AlwaysClamp);
                ImGui::SameLine();
                HelpMarker("Shows a visual keyboard. Keys with a configured rebind are outlined.\n"
                           "Each outlined key shows: \"Types X\" (text/VK output) and \"Triggers Y\" (game keybind/scan code).\n"
                           "This is a visualization only.");

                if (s_keyboardLayoutOpen) {
                    ImGui::SetNextWindowSize(ImVec2(1100.0f, 620.0f), ImGuiCond_Appearing);
                    ImGui::OpenPopup("Keyboard Layout");
                }

                auto scanCodeToDisplayName = [&](DWORD scan, DWORD fallbackVk) -> std::string {
                    if (scan == 0) {
                        // For mouse outputs we store scan=0; display the VK name instead.
                        if (fallbackVk == VK_LBUTTON || fallbackVk == VK_RBUTTON || fallbackVk == VK_MBUTTON || fallbackVk == VK_XBUTTON1 ||
                            fallbackVk == VK_XBUTTON2) {
                            return VkToString(fallbackVk);
                        }
                        return std::string("[Unbound]");
                    }

                    DWORD scanDisplayVK = MapVirtualKey(scan, MAPVK_VSC_TO_VK_EX);
                    if (scanDisplayVK != 0) { return VkToString(scanDisplayVK); }

                    LONG keyNameLParam = static_cast<LONG>((scan & 0xFF) << 16);
                    if ((scan & 0xFF00) != 0) { keyNameLParam |= (1 << 24); } // extended key bit
                    char keyName[64] = {};
                    if (GetKeyNameTextA(keyNameLParam, keyName, sizeof(keyName)) > 0) { return std::string(keyName); }
                    return std::string("[Unknown]");
                };

                if (ImGui::BeginPopupModal("Keyboard Layout", &s_keyboardLayoutOpen, ImGuiWindowFlags_NoScrollbar)) {
                    // While this modal is open, treat it like a binding interaction so the global ESC-to-close-GUI
                    // hotkey doesn't close the whole settings UI.
                    MarkRebindBindingActive();

                    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                        s_keyboardLayoutOpen = false;
                        ImGui::CloseCurrentPopup();
                    }

                    ImGui::TextUnformatted("Keyboard Layout");
                    ImGui::Separator();

                    // Make the keyboard region scrollable (both axes) to fit on smaller windows.
                    ImGui::BeginChild("##keyboardLayoutChild", ImVec2(0, 0), true,
                                     ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_AlwaysVerticalScrollbar);

                    struct KeyCell {
                        DWORD vk;
                        const char* labelOverride; // optional
                        float w;                   // width in units
                    };

                    auto Spacer = [](float wUnits) -> KeyCell { return KeyCell{ 0, nullptr, wUnits }; };
                    auto Key = [](DWORD vk, float wUnits = 1.0f, const char* overrideLabel = nullptr) -> KeyCell {
                        return KeyCell{ vk, overrideLabel, wUnits };
                    };

                    // A reasonably complete 104-key ANSI-ish layout (approximate; single-height keys).
                    const std::vector<std::vector<KeyCell>> rows = {
                        // Function row
                        { Key(VK_ESCAPE), Spacer(0.5f), Key(VK_F1), Key(VK_F2), Key(VK_F3), Key(VK_F4), Spacer(0.5f), Key(VK_F5),
                          Key(VK_F6), Key(VK_F7), Key(VK_F8), Spacer(0.5f), Key(VK_F9), Key(VK_F10), Key(VK_F11), Key(VK_F12),
                          Spacer(0.5f), Key(VK_SNAPSHOT, 1.2f, "PRTSC"), Key(VK_SCROLL, 1.2f, "SCRLK"), Key(VK_PAUSE, 1.2f, "PAUSE") },

                        // Number row + nav + numpad
                        { Key(VK_OEM_3, 1.0f, "`") , Key('1'), Key('2'), Key('3'), Key('4'), Key('5'), Key('6'), Key('7'), Key('8'), Key('9'),
                          Key('0'), Key(VK_OEM_MINUS, 1.0f, "-"), Key(VK_OEM_PLUS, 1.0f, "="), Key(VK_BACK, 2.0f, "BACK"), Spacer(0.5f),
                          Key(VK_INSERT, 1.2f, "INS"), Key(VK_HOME, 1.2f, "HOME"), Key(VK_PRIOR, 1.2f, "PGUP"), Spacer(0.5f),
                          Key(VK_NUMLOCK, 1.2f, "NUMLK"), Key(VK_DIVIDE, 1.2f, "/"), Key(VK_MULTIPLY, 1.2f, "*"), Key(VK_SUBTRACT, 1.2f, "-") },

                        // Q row
                        { Key(VK_TAB, 1.5f, "TAB"), Key('Q'), Key('W'), Key('E'), Key('R'), Key('T'), Key('Y'), Key('U'), Key('I'), Key('O'), Key('P'),
                          Key(VK_OEM_4, 1.0f, "["), Key(VK_OEM_6, 1.0f, "]"), Key(VK_OEM_5, 1.5f, "\\"), Spacer(0.5f),
                          Key(VK_DELETE, 1.2f, "DEL"), Key(VK_END, 1.2f, "END"), Key(VK_NEXT, 1.2f, "PGDN"), Spacer(0.5f),
                          Key(VK_NUMPAD7, 1.2f, "NUM7"), Key(VK_NUMPAD8, 1.2f, "NUM8"), Key(VK_NUMPAD9, 1.2f, "NUM9"), Key(VK_ADD, 1.2f, "+") },

                        // A row
                        { Key(VK_CAPITAL, 1.75f, "CAPS"), Key('A'), Key('S'), Key('D'), Key('F'), Key('G'), Key('H'), Key('J'), Key('K'), Key('L'),
                          Key(VK_OEM_1, 1.0f, ";"), Key(VK_OEM_7, 1.0f, "'"), Key(VK_RETURN, 2.25f, "ENTER"), Spacer(0.5f),
                          Spacer(3.6f), Spacer(0.5f), Key(VK_NUMPAD4, 1.2f, "NUM4"), Key(VK_NUMPAD5, 1.2f, "NUM5"), Key(VK_NUMPAD6, 1.2f, "NUM6"),
                          Key(VK_ADD, 1.2f, "+") },

                        // Z row + arrows + numpad
                        { Key(VK_LSHIFT, 2.25f, "LSHIFT"), Key('Z'), Key('X'), Key('C'), Key('V'), Key('B'), Key('N'), Key('M'),
                          Key(VK_OEM_COMMA, 1.0f, ","), Key(VK_OEM_PERIOD, 1.0f, "."), Key(VK_OEM_2, 1.0f, "/"), Key(VK_RSHIFT, 2.75f, "RSHIFT"),
                          Spacer(0.5f), Spacer(1.2f), Key(VK_UP, 1.2f, "UP"), Spacer(1.2f), Spacer(0.5f),
                          Key(VK_NUMPAD1, 1.2f, "NUM1"), Key(VK_NUMPAD2, 1.2f, "NUM2"), Key(VK_NUMPAD3, 1.2f, "NUM3"), Key(VK_RETURN, 1.2f, "ENTER") },

                        // Bottom row
                        { Key(VK_LCONTROL, 1.6f, "LCTRL"), Key(VK_LWIN, 1.3f, "LWIN"), Key(VK_LMENU, 1.3f, "LALT"),
                          Key(VK_SPACE, 6.6f, "SPACE"), Key(VK_RMENU, 1.3f, "RALT"), Key(VK_RWIN, 1.3f, "RWIN"), Key(VK_APPS, 1.3f, "APPS"),
                          Key(VK_RCONTROL, 1.6f, "RCTRL"), Spacer(0.5f), Key(VK_LEFT, 1.2f, "LEFT"), Key(VK_DOWN, 1.2f, "DOWN"), Key(VK_RIGHT, 1.2f, "RIGHT"),
                          Spacer(0.5f), Key(VK_NUMPAD0, 2.4f, "NUM0"), Key(VK_DECIMAL, 1.2f, "NUM."), Key(VK_RETURN, 1.2f, "ENTER") },
                    };

                    // Resolve a rebind by fromKey. Prefer enabled + fully-configured entries.
                    auto findRebindForKey = [&](DWORD fromVk) -> const KeyRebind* {
                        const KeyRebind* first = nullptr;
                        const KeyRebind* enabledAny = nullptr;
                        const KeyRebind* enabledConfigured = nullptr;
                        const KeyRebind* configuredAny = nullptr;

                        for (const auto& r : g_config.keyRebinds.rebinds) {
                            if (r.fromKey != fromVk) continue;
                            if (!first) first = &r;

                            const bool configured = (r.fromKey != 0 && r.toKey != 0);
                            if (configured && !configuredAny) configuredAny = &r;
                            if (r.enabled && !enabledAny) enabledAny = &r;
                            if (r.enabled && configured) {
                                enabledConfigured = &r;
                                break;
                            }
                        }

                        if (enabledConfigured) return enabledConfigured;
                        if (configuredAny) return configuredAny;
                        if (enabledAny) return enabledAny;
                        return first;
                    };

                    const float unit = ImGui::GetFrameHeight() * 1.25f * s_keyboardLayoutScale;
                    const float keyH = ImGui::GetFrameHeight() * 1.6f * s_keyboardLayoutScale;
                    const float gap = ImGui::GetStyle().ItemInnerSpacing.x * s_keyboardLayoutScale;
                    const float rounding = 5.0f * s_keyboardLayoutScale;
                    ImDrawList* dl = ImGui::GetWindowDrawList();

                    // Pre-compute keyboard width so we can place the mouse panel to the right.
                    float keyboardMaxRowW = 0.0f;
                    for (const auto& row : rows) {
                        float w = 0.0f;
                        for (size_t c = 0; c < row.size(); ++c) {
                            w += row[c].w * unit;
                            if (c + 1 < row.size()) w += gap;
                        }
                        if (w > keyboardMaxRowW) keyboardMaxRowW = w;
                    }

                    const ImVec2 layoutStart = ImGui::GetCursorPos();
                    const ImVec2 layoutStartScreen = ImGui::GetCursorScreenPos();
                    const float keyboardTotalH = (float)rows.size() * (keyH + gap);

                    // Keyboard base plate (behind keycaps)
                    {
                        const float platePad = 10.0f * s_keyboardLayoutScale;
                        const float plateRound = 10.0f * s_keyboardLayoutScale;
                        const ImVec2 plateMin = ImVec2(layoutStartScreen.x - platePad, layoutStartScreen.y - platePad);
                        const ImVec2 plateMax =
                            ImVec2(layoutStartScreen.x + keyboardMaxRowW + platePad, layoutStartScreen.y + keyboardTotalH - gap + platePad);

                        // Shadow
                        dl->AddRectFilled(ImVec2(plateMin.x + 5, plateMin.y + 6), ImVec2(plateMax.x + 5, plateMax.y + 6),
                                          IM_COL32(0, 0, 0, 130), plateRound);

                        // Plate gradient
                        dl->AddRectFilledMultiColor(plateMin, plateMax, IM_COL32(35, 38, 46, 255), IM_COL32(35, 38, 46, 255),
                                                    IM_COL32(18, 20, 26, 255), IM_COL32(18, 20, 26, 255));
                        dl->AddRect(plateMin, plateMax, IM_COL32(10, 10, 12, 255), plateRound);
                        // Small highlight line
                        dl->AddLine(ImVec2(plateMin.x + 6, plateMin.y + 6), ImVec2(plateMax.x - 6, plateMin.y + 6), IM_COL32(255, 255, 255, 25),
                                    1.0f);
                    }

                    // Context menu state for right-click editing
                    static DWORD s_layoutContextVk = 0;
                    static int s_layoutContextPreferredIndex = -1;
                    auto openRebindContextFor = [&](DWORD vk) {
                        s_layoutContextVk = vk;
                        s_layoutContextPreferredIndex = -1;
                        ImGui::OpenPopup("Rebind Config##layout");
                    };

                    auto drawKeyCell = [&](DWORD vk, const char* label, const ImVec2& pMin, const ImVec2& pMax, const KeyRebind* rb) {
                        const bool hovered = ImGui::IsItemHovered();
                        const bool active = ImGui::IsItemActive();

                        struct KeyTheme {
                            ImU32 top;
                            ImU32 bottom;
                            ImU32 border;
                            ImU32 text;
                        };

                        auto clamp255 = [](int v) -> int { return v < 0 ? 0 : (v > 255 ? 255 : v); };
                        auto adjust = [&](ImU32 c, int add) -> ImU32 {
                            int r = (int)(c & 0xFF);
                            int g = (int)((c >> 8) & 0xFF);
                            int b = (int)((c >> 16) & 0xFF);
                            int a = (int)((c >> 24) & 0xFF);
                            r = clamp255(r + add);
                            g = clamp255(g + add);
                            b = clamp255(b + add);
                            return (ImU32)(r | (g << 8) | (b << 16) | (a << 24));
                        };

                        auto themeForVk = [&](DWORD v) -> KeyTheme {
                            const bool isMouse = (v == VK_LBUTTON || v == VK_RBUTTON || v == VK_MBUTTON || v == VK_XBUTTON1 || v == VK_XBUTTON2);
                            const bool isFn = (v == VK_ESCAPE || (v >= VK_F1 && v <= VK_F24) || v == VK_SNAPSHOT || v == VK_SCROLL || v == VK_PAUSE);
                            const bool isMod = (v == VK_SHIFT || v == VK_LSHIFT || v == VK_RSHIFT || v == VK_CONTROL || v == VK_LCONTROL ||
                                                v == VK_RCONTROL || v == VK_MENU || v == VK_LMENU || v == VK_RMENU || v == VK_LWIN || v == VK_RWIN ||
                                                v == VK_TAB || v == VK_CAPITAL || v == VK_BACK || v == VK_RETURN || v == VK_SPACE || v == VK_APPS);
                            const bool isNav = (v == VK_INSERT || v == VK_DELETE || v == VK_HOME || v == VK_END || v == VK_PRIOR || v == VK_NEXT ||
                                                v == VK_UP || v == VK_DOWN || v == VK_LEFT || v == VK_RIGHT);
                            const bool isNum = (v >= VK_NUMPAD0 && v <= VK_NUMPAD9) || v == VK_ADD || v == VK_SUBTRACT || v == VK_MULTIPLY ||
                                                v == VK_DIVIDE || v == VK_DECIMAL || v == VK_NUMLOCK;

                            KeyTheme t{ IM_COL32(80, 86, 98, 255), IM_COL32(45, 49, 60, 255), IM_COL32(18, 18, 20, 255),
                                        IM_COL32(245, 245, 245, 255) };
                            if (isMod) {
                                t.top = IM_COL32(72, 78, 92, 255);
                                t.bottom = IM_COL32(38, 42, 52, 255);
                            }
                            if (isFn) {
                                t.top = IM_COL32(92, 80, 74, 255);
                                t.bottom = IM_COL32(52, 42, 38, 255);
                            }
                            if (isNav) {
                                t.top = IM_COL32(78, 86, 104, 255);
                                t.bottom = IM_COL32(40, 45, 56, 255);
                            }
                            if (isNum) {
                                t.top = IM_COL32(72, 88, 90, 255);
                                t.bottom = IM_COL32(36, 46, 48, 255);
                            }
                            if (isMouse) {
                                t.top = IM_COL32(88, 90, 108, 255);
                                t.bottom = IM_COL32(44, 46, 60, 255);
                            }
                            return t;
                        };

                        KeyTheme theme = themeForVk(vk);
                        if (hovered) {
                            theme.top = adjust(theme.top, 12);
                            theme.bottom = adjust(theme.bottom, 10);
                        }
                        if (active) {
                            theme.top = adjust(theme.top, -8);
                            theme.bottom = adjust(theme.bottom, -16);
                        }

                        const ImVec2 size = ImVec2(pMax.x - pMin.x, pMax.y - pMin.y);
                        const float shadow = 2.0f * s_keyboardLayoutScale;
                        const ImVec2 pressOff = active ? ImVec2(0.0f, 1.2f * s_keyboardLayoutScale) : ImVec2(0, 0);

                        // Shadow
                        dl->AddRectFilled(ImVec2(pMin.x + shadow, pMin.y + shadow), ImVec2(pMax.x + shadow, pMax.y + shadow),
                                          IM_COL32(0, 0, 0, 90), rounding);

                        const ImVec2 kMin = ImVec2(pMin.x + pressOff.x, pMin.y + pressOff.y);
                        const ImVec2 kMax = ImVec2(pMax.x + pressOff.x, pMax.y + pressOff.y);

                        // Keycap gradient
                        dl->AddRectFilledMultiColor(kMin, kMax, theme.top, theme.top, theme.bottom, theme.bottom);

                        // Inner highlight
                        dl->AddLine(ImVec2(kMin.x + 2, kMin.y + 2), ImVec2(kMax.x - 2, kMin.y + 2), IM_COL32(255, 255, 255, 35), 1.0f);

                        // Border
                        dl->AddRect(kMin, kMax, theme.border, rounding, 0, 1.0f);

                        // Rebind outline + tint
                        if (rb && rb->fromKey != 0 && rb->toKey != 0) {
                            const ImU32 outline = rb->enabled ? IM_COL32(0, 220, 110, 255) : IM_COL32(255, 170, 0, 255);
                            dl->AddRect(kMin, kMax, outline, rounding, 0, 3.0f);
                            const ImU32 tint = rb->enabled ? IM_COL32(0, 190, 95, 30) : IM_COL32(255, 165, 0, 35);
                            dl->AddRectFilled(kMin, kMax, tint, rounding);
                        }

                        const float padX = 6.0f * s_keyboardLayoutScale;
                        const float padY = 4.0f * s_keyboardLayoutScale;

                        // Centered label (auto-scale to fit if needed)
                        ImFont* fLabel = ImGui::GetFont();
                        float labelFontSize = ImGui::GetFontSize();
                        ImVec2 labelSz = fLabel->CalcTextSizeA(labelFontSize, FLT_MAX, 0.0f, label);
                        const float maxLabelW = size.x - padX * 2.0f;
                        if (maxLabelW > 8.0f && labelSz.x > maxLabelW) {
                            float scale = maxLabelW / (labelSz.x + 0.001f);
                            if (scale < 0.60f) scale = 0.60f;
                            if (scale > 1.0f) scale = 1.0f;
                            labelFontSize = ImGui::GetFontSize() * scale;
                            labelSz = fLabel->CalcTextSizeA(labelFontSize, FLT_MAX, 0.0f, label);
                        }
                        ImVec2 labelPos = ImVec2(kMin.x + (size.x - labelSz.x) * 0.5f, kMin.y + padY);
                        if (labelPos.x < kMin.x + padX) labelPos.x = kMin.x + padX;
                        dl->AddText(fLabel, labelFontSize, labelPos, theme.text, label);

                        // Rebind info (Types/Triggers) - only when there is room
                        if (rb && rb->fromKey != 0 && rb->toKey != 0) {
                            DWORD triggerVK = rb->toKey;
                            DWORD typesVK = (rb->useCustomOutput && rb->customOutputVK != 0) ? rb->customOutputVK : triggerVK;
                            DWORD outScan = (rb->useCustomOutput && rb->customOutputScanCode != 0) ? rb->customOutputScanCode
                                                                                                    : getScanCodeWithExtendedFlag(triggerVK);

                            if (outScan != 0 && (outScan & 0xFF00) == 0) {
                                DWORD derived = getScanCodeWithExtendedFlag(triggerVK);
                                if ((derived & 0xFF00) != 0 && ((derived & 0xFF) == (outScan & 0xFF))) { outScan = derived; }
                            }

                            const float minWForInfo = unit * 1.55f;
                            const float minHForInfo = ImGui::GetFontSize() * 3.6f;
                            if (size.x >= minWForInfo && size.y >= minHForInfo) {
                                std::string types = std::string("Types ") + VkToString(typesVK);
                                std::string triggers = std::string("Triggers ") + scanCodeToDisplayName(outScan, triggerVK);
                                const ImU32 infoCol = rb->enabled ? IM_COL32(245, 245, 245, 235) : IM_COL32(255, 220, 170, 235);
                                ImFont* f = ImGui::GetFont();
                                const float infoSize = ImGui::GetFontSize() * 0.78f;
                                dl->AddText(f, infoSize, ImVec2(kMin.x + padX, kMin.y + size.y * 0.55f), infoCol, types.c_str());
                                dl->AddText(f, infoSize, ImVec2(kMin.x + padX, kMin.y + size.y * 0.72f), infoCol, triggers.c_str());
                            }
                        }

                        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                            openRebindContextFor(vk);
                        }
                    };

                    for (size_t rowIdx = 0; rowIdx < rows.size(); ++rowIdx) {
                        float xCursor = layoutStart.x;
                        float yCursor = layoutStart.y + (float)rowIdx * (keyH + gap);

                        for (size_t colIdx = 0; colIdx < rows[rowIdx].size(); ++colIdx) {
                            const KeyCell& kc = rows[rowIdx][colIdx];
                            const float keyW = kc.w * unit;

                            ImGui::SetCursorPos(ImVec2(xCursor, yCursor));
                            if (kc.vk == 0) {
                                // Spacer
                                ImGui::Dummy(ImVec2(keyW, keyH));
                                xCursor += keyW + gap;
                                continue;
                            }

                            ImGui::PushID((int)(rowIdx * 1000 + colIdx));
                            ImGui::InvisibleButton("##key", ImVec2(keyW, keyH));

                            const ImVec2 pMin = ImGui::GetItemRectMin();
                            const ImVec2 pMax = ImGui::GetItemRectMax();

                            const KeyRebind* rb = findRebindForKey(kc.vk);

                            std::string keyName = kc.labelOverride ? std::string(kc.labelOverride) : VkToString(kc.vk);
                            drawKeyCell(kc.vk, keyName.c_str(), pMin, pMax, rb);

                            if (ImGui::IsItemHovered()) {
                                ImGui::BeginTooltip();
                                ImGui::Text("%s (%u)", VkToString(kc.vk).c_str(), (unsigned)kc.vk);
                                if (!(rb && rb->fromKey != 0 && rb->toKey != 0)) {
                                    ImGui::TextUnformatted("Right-click to configure rebinds.");
                                }
                                ImGui::EndTooltip();
                            }

                            ImGui::PopID();
                            xCursor += keyW + gap;
                        }

                        // no cursor advance: absolute positioning
                    }

                    // -----------------------------------------------------------------
                    // Mouse (to the right of the keyboard)
                    // -----------------------------------------------------------------
                    const float mousePanelX = layoutStart.x + keyboardMaxRowW + unit * 0.9f;
                    const float mousePanelY = layoutStart.y;
                    ImGui::SetCursorPos(ImVec2(mousePanelX, mousePanelY));

                    ImGui::TextUnformatted("Mouse");
                    const float mouseHeaderH = ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;

                    struct MouseBtn {
                        DWORD vk;
                        const char* label;
                    };

                    const MouseBtn mouseBtns[] = {
                        { VK_LBUTTON, "MOUSE1" },
                        { VK_RBUTTON, "MOUSE2" },
                        { VK_MBUTTON, "MOUSE3" },
                        { VK_XBUTTON1, "MOUSE4" },
                        { VK_XBUTTON2, "MOUSE5" },
                    };

                    const float mouseW = unit * 3.2f;
                    const float mouseKeyH = keyH * 1.05f;

                    for (int mb = 0; mb < (int)(sizeof(mouseBtns) / sizeof(mouseBtns[0])); ++mb) {
                        const auto& btn = mouseBtns[mb];
                        ImGui::PushID(btn.label);
                        ImGui::SetCursorPos(ImVec2(mousePanelX, mousePanelY + mouseHeaderH + (mouseKeyH + gap) * (float)mb));
                        ImGui::InvisibleButton("##mousebtn", ImVec2(mouseW, mouseKeyH));

                        const ImVec2 pMin = ImGui::GetItemRectMin();
                        const ImVec2 pMax = ImGui::GetItemRectMax();

                        const KeyRebind* rb = findRebindForKey(btn.vk);

                        drawKeyCell(btn.vk, btn.label, pMin, pMax, rb);

                        if (ImGui::IsItemHovered()) {
                            ImGui::BeginTooltip();
                            ImGui::Text("Input: %s (%u)", VkToString(btn.vk).c_str(), (unsigned)btn.vk);
                            if (rb && rb->fromKey != 0 && rb->toKey != 0) {
                                DWORD triggerVK = rb->toKey;
                                DWORD typesVK = (rb->useCustomOutput && rb->customOutputVK != 0) ? rb->customOutputVK : triggerVK;
                                DWORD outScan = (rb->useCustomOutput && rb->customOutputScanCode != 0) ? rb->customOutputScanCode
                                                                                                        : getScanCodeWithExtendedFlag(triggerVK);
                                ImGui::Text("Types: %s (%u)", VkToString(typesVK).c_str(), (unsigned)typesVK);
                                ImGui::Text("Triggers: %s (scan=%u)", scanCodeToDisplayName(outScan, triggerVK).c_str(), (unsigned)outScan);
                            } else {
                                ImGui::TextUnformatted("No rebind configured for this button.");
                            }
                            ImGui::EndTooltip();
                        }

                        ImGui::PopID();
                    }

                    // Right-click context menu
                    ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_Appearing);
                    if (ImGui::BeginPopup("Rebind Config##layout")) {
                        // Also block global ESC-to-close-GUI while editing inside this popup.
                        MarkRebindBindingActive();
                        std::string keyTitle = VkToString(s_layoutContextVk);
                        ImGui::Text("Rebinds for: %s", keyTitle.c_str());
                        ImGui::Separator();

                        // Gather matching rebind indices
                        std::vector<int> matches;
                        matches.reserve(g_config.keyRebinds.rebinds.size());
                        for (int ri = 0; ri < (int)g_config.keyRebinds.rebinds.size(); ++ri) {
                            if (g_config.keyRebinds.rebinds[ri].fromKey == s_layoutContextVk) { matches.push_back(ri); }
                        }

                        int toRemove = -1;
                        if (ImGui::Button("Add Rebind##layout")) {
                            KeyRebind r;
                            r.fromKey = s_layoutContextVk;
                            g_config.keyRebinds.rebinds.push_back(r);
                            g_configIsDirty = true;
                            std::lock_guard<std::mutex> hotkeyLock(g_hotkeyMainKeysMutex);
                            RebuildHotkeyMainKeys_Internal();
                            matches.push_back((int)g_config.keyRebinds.rebinds.size() - 1);
                        }

                        ImGui::Spacing();

                        if (matches.empty()) {
                            ImGui::TextUnformatted("No rebinds configured.");
                        }

                        for (int mi = 0; mi < (int)matches.size(); ++mi) {
                            int idx = matches[mi];
                            if (idx < 0 || idx >= (int)g_config.keyRebinds.rebinds.size()) continue;
                            auto& rebind = g_config.keyRebinds.rebinds[idx];
                            ImGui::PushID(idx);

                            ImGui::SeparatorText((std::string("Rebind #") + std::to_string(idx + 1)).c_str());

                            if (ImGui::Checkbox("Enabled##layout", &rebind.enabled)) {
                                g_configIsDirty = true;
                                std::lock_guard<std::mutex> hotkeyLock(g_hotkeyMainKeysMutex);
                                RebuildHotkeyMainKeys_Internal();
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("Remove##layout")) { toRemove = idx; }

                            ImGui::Text("Output:");
                            ImGui::SameLine();
                            std::string outStr = VkToString(rebind.toKey);
                            if (ImGui::Button((outStr + "##out").c_str(), ImVec2(160, 0))) {
                                s_rebindOutputVKToBind = idx;
                                s_rebindFromKeyToBind = -1;
                                s_rebindTextOverrideVKToBind = -1;
                                s_rebindOutputScanToBind = -1;
                                MarkRebindBindingActive();
                            }

                            bool overrideText = (rebind.useCustomOutput && rebind.customOutputVK != 0);
                            if (ImGui::Checkbox("Override Text##layout", &overrideText)) {
                                if (overrideText) {
                                    rebind.useCustomOutput = true;
                                    rebind.customOutputVK = (rebind.toKey != 0) ? rebind.toKey : rebind.customOutputVK;
                                } else {
                                    rebind.customOutputVK = 0;
                                    if (rebind.customOutputScanCode == 0) { rebind.useCustomOutput = false; }
                                }
                                g_configIsDirty = true;
                            }
                            ImGui::SameLine();
                            if (overrideText) {
                                DWORD textVk = (rebind.customOutputVK != 0) ? rebind.customOutputVK : rebind.toKey;
                                std::string textStr = VkToString(textVk);
                                if (ImGui::Button((textStr + "##text").c_str(), ImVec2(140, 0))) {
                                    s_rebindTextOverrideVKToBind = idx;
                                    s_rebindFromKeyToBind = -1;
                                    s_rebindOutputVKToBind = -1;
                                    s_rebindOutputScanToBind = -1;
                                    MarkRebindBindingActive();
                                }
                            }

                            ImGui::Text("Game Keybind:");
                            ImGui::SameLine();
                            DWORD displayScan = (rebind.useCustomOutput && rebind.customOutputScanCode != 0) ? rebind.customOutputScanCode
                                                                                                            : getScanCodeWithExtendedFlag(rebind.toKey);
                            std::string scanStr = scanCodeToDisplayName(displayScan, rebind.toKey);
                            if (ImGui::Button((scanStr + "##scan").c_str(), ImVec2(160, 0))) {
                                s_rebindOutputScanToBind = idx;
                                s_rebindFromKeyToBind = -1;
                                s_rebindOutputVKToBind = -1;
                                s_rebindTextOverrideVKToBind = -1;
                                MarkRebindBindingActive();
                            }

                            ImGui::PopID();
                        }

                        if (toRemove >= 0 && toRemove < (int)g_config.keyRebinds.rebinds.size()) {
                            g_config.keyRebinds.rebinds.erase(g_config.keyRebinds.rebinds.begin() + toRemove);
                            g_configIsDirty = true;
                            std::lock_guard<std::mutex> hotkeyLock(g_hotkeyMainKeysMutex);
                            RebuildHotkeyMainKeys_Internal();
                        }

                        ImGui::EndPopup();
                    }

                    // Ensure the child cursor ends below the larger of keyboard/mouse regions.
                    const float mouseTotalH = mouseHeaderH + (float)(sizeof(mouseBtns) / sizeof(mouseBtns[0])) * (mouseKeyH + gap);
                    const float totalH = (keyboardTotalH > mouseTotalH) ? keyboardTotalH : mouseTotalH;
                    ImGui::SetCursorPos(ImVec2(layoutStart.x, layoutStart.y + totalH + gap));

                    ImGui::EndChild();

                    ImGui::EndPopup();
                }

                // Rebind binding popup (for from key)
                bool is_rebind_from_binding = (s_rebindFromKeyToBind != -1);
                if (is_rebind_from_binding) { MarkRebindBindingActive(); }
                if (is_rebind_from_binding) { ImGui::OpenPopup("Bind From Key"); }

                if (ImGui::BeginPopupModal("Bind From Key", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) {
                    ImGui::Text("Press a key to bind as INPUT.");
                    ImGui::Text("Press ESC to cancel.");
                    ImGui::Separator();

                    static uint64_t s_lastBindingInputSeqInputs1 = 0;
                    if (ImGui::IsWindowAppearing()) { s_lastBindingInputSeqInputs1 = GetLatestBindingInputSequence(); }

                    DWORD capturedVk = 0;
                    LPARAM capturedLParam = 0;
                    bool capturedIsMouse = false;
                    if (ConsumeBindingInputEventSince(s_lastBindingInputSeqInputs1, capturedVk, capturedLParam, capturedIsMouse)) {
                        if (capturedVk == VK_ESCAPE) {
                            s_rebindFromKeyToBind = -1;
                            ImGui::CloseCurrentPopup();
                        } else {
                            // Allow binding modifier keys (L/R Ctrl/Shift/Alt) for key rebinding.
                            // Only disallow Windows keys.
                            if (capturedVk != VK_LWIN && capturedVk != VK_RWIN) {
                                if (s_rebindFromKeyToBind != -1 && s_rebindFromKeyToBind < (int)g_config.keyRebinds.rebinds.size()) {
                                    g_config.keyRebinds.rebinds[s_rebindFromKeyToBind].fromKey = capturedVk;
                                    g_configIsDirty = true;
                                    std::lock_guard<std::mutex> hotkeyLock(g_hotkeyMainKeysMutex);
                                    RebuildHotkeyMainKeys_Internal();
                                    (void)capturedLParam;
                                    (void)capturedIsMouse;
                                }
                                s_rebindFromKeyToBind = -1;
                                ImGui::CloseCurrentPopup();
                            }
                        }
                    }

                    ImGui::EndPopup();
                }

                // Output VK binding popup
                bool is_vk_binding = (s_rebindOutputVKToBind != -1);
                if (is_vk_binding) { MarkRebindBindingActive(); }
                if (is_vk_binding) { ImGui::OpenPopup("Bind Output VK"); }

                if (ImGui::BeginPopupModal("Bind Output VK", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) {
                    ImGui::Text("Press a key to set OUTPUT Virtual Key Code.");
                    ImGui::Text("Press ESC to cancel.");
                    ImGui::Separator();

                    static uint64_t s_lastBindingInputSeqInputs2 = 0;
                    if (ImGui::IsWindowAppearing()) { s_lastBindingInputSeqInputs2 = GetLatestBindingInputSequence(); }

                    DWORD capturedVk = 0;
                    LPARAM capturedLParam = 0;
                    bool capturedIsMouse = false;
                    if (ConsumeBindingInputEventSince(s_lastBindingInputSeqInputs2, capturedVk, capturedLParam, capturedIsMouse)) {
                        if (capturedVk == VK_ESCAPE) {
                            s_rebindOutputVKToBind = -1;
                            ImGui::CloseCurrentPopup();
                        } else {
                            // Allow modifier keys here as well (useful when the desired output is a modifier).
                            if (capturedVk != VK_LWIN && capturedVk != VK_RWIN) {
                                if (s_rebindOutputVKToBind >= 0 && s_rebindOutputVKToBind < (int)g_config.keyRebinds.rebinds.size()) {
                                    auto& rebind = g_config.keyRebinds.rebinds[s_rebindOutputVKToBind];
                                    rebind.toKey = capturedVk;
                                    // Base output affects BOTH text + trigger by default.
                                    // Do not touch custom text override here.
                                    g_configIsDirty = true;
                                    (void)capturedLParam;
                                    (void)capturedIsMouse;
                                }
                                s_rebindOutputVKToBind = -1;
                                ImGui::CloseCurrentPopup();
                            }
                        }
                    }

                    ImGui::EndPopup();
                }

                // Text Override VK binding popup
                bool is_text_vk_binding = (s_rebindTextOverrideVKToBind != -1);
                if (is_text_vk_binding) { MarkRebindBindingActive(); }
                if (is_text_vk_binding) { ImGui::OpenPopup("Bind Text Override VK"); }

                if (ImGui::BeginPopupModal("Bind Text Override VK", NULL,
                                           ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) {
                    ImGui::Text("Press a key to set TEXT OVERRIDE Virtual Key Code.");
                    ImGui::Text("Press ESC to cancel.");
                    ImGui::Separator();

                    static uint64_t s_lastBindingInputSeqInputs2b = 0;
                    if (ImGui::IsWindowAppearing()) { s_lastBindingInputSeqInputs2b = GetLatestBindingInputSequence(); }

                    DWORD capturedVk = 0;
                    LPARAM capturedLParam = 0;
                    bool capturedIsMouse = false;
                    if (ConsumeBindingInputEventSince(s_lastBindingInputSeqInputs2b, capturedVk, capturedLParam, capturedIsMouse)) {
                        if (capturedVk == VK_ESCAPE) {
                            s_rebindTextOverrideVKToBind = -1;
                            ImGui::CloseCurrentPopup();
                        } else {
                            if (capturedVk != VK_LWIN && capturedVk != VK_RWIN) {
                                if (s_rebindTextOverrideVKToBind >= 0 &&
                                    s_rebindTextOverrideVKToBind < (int)g_config.keyRebinds.rebinds.size()) {
                                    auto& rebind = g_config.keyRebinds.rebinds[s_rebindTextOverrideVKToBind];
                                    rebind.useCustomOutput = true;
                                    rebind.customOutputVK = capturedVk;
                                    g_configIsDirty = true;
                                    (void)capturedLParam;
                                    (void)capturedIsMouse;
                                }
                                s_rebindTextOverrideVKToBind = -1;
                                ImGui::CloseCurrentPopup();
                            }
                        }
                    }

                    ImGui::EndPopup();
                }

                // Output Scan Code binding popup
                bool is_scan_binding = (s_rebindOutputScanToBind != -1);
                if (is_scan_binding) { MarkRebindBindingActive(); }
                if (is_scan_binding) { ImGui::OpenPopup("Bind Output Scan"); }

                if (ImGui::BeginPopupModal("Bind Output Scan", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) {
                    ImGui::Text("Press a key to set OUTPUT Scan Code.");
                    ImGui::Text("Press ESC to cancel.");
                    ImGui::Separator();

                    static uint64_t s_lastBindingInputSeqInputs3 = 0;
                    if (ImGui::IsWindowAppearing()) { s_lastBindingInputSeqInputs3 = GetLatestBindingInputSequence(); }

                    DWORD capturedVk = 0;
                    LPARAM capturedLParam = 0;
                    bool capturedIsMouse = false;
                    if (ConsumeBindingInputEventSince(s_lastBindingInputSeqInputs3, capturedVk, capturedLParam, capturedIsMouse)) {
                        if (capturedVk == VK_ESCAPE) {
                            s_rebindOutputScanToBind = -1;
                            ImGui::CloseCurrentPopup();
                        } else {
                            // Allow modifier keys when capturing scan codes too.
                            if (capturedVk != VK_LWIN && capturedVk != VK_RWIN) {
                                if (s_rebindOutputScanToBind >= 0 && s_rebindOutputScanToBind < (int)g_config.keyRebinds.rebinds.size()) {
                                    auto& rebind = g_config.keyRebinds.rebinds[s_rebindOutputScanToBind];

                                if (capturedVk == VK_LBUTTON || capturedVk == VK_RBUTTON || capturedVk == VK_MBUTTON ||
                                    capturedVk == VK_XBUTTON1 || capturedVk == VK_XBUTTON2) {
                                    // Mouse outputs have no meaningful scan code. Treat this as setting the base output
                                    // so by default it affects both text + trigger. (Text can still be overridden separately.)
                                    rebind.toKey = capturedVk;
                                    rebind.customOutputScanCode = 0;
                                } else {
                                    UINT scanCode = static_cast<UINT>((capturedLParam >> 16) & 0xFF);
                                    if ((capturedLParam & (1LL << 24)) != 0) { scanCode |= 0xE000; }
                                    if ((capturedLParam & (1LL << 24)) == 0 && scanCode == 0) {
                                        scanCode = getScanCodeWithExtendedFlag(capturedVk);
                                    }

                                    if ((scanCode & 0xFF00) == 0) { scanCode = getScanCodeWithExtendedFlag(capturedVk); }

                                    rebind.customOutputScanCode = scanCode;
                                    rebind.useCustomOutput = true;

                                    Log("[Rebind][GameKeybind] capturedVk=" + std::to_string(capturedVk) +
                                        " capturedLParam=" + std::to_string(static_cast<long long>(capturedLParam)) +
                                        " storedScan=" + std::to_string(scanCode) + " ext=" + std::string((scanCode & 0xFF00) ? "1" : "0"));
                                }

                                    g_configIsDirty = true;
                                    (void)capturedIsMouse;
                                }
                                s_rebindOutputScanToBind = -1;
                                ImGui::CloseCurrentPopup();
                            }
                        }
                    }

                    ImGui::EndPopup();
                }

                int rebind_to_remove = -1;
                for (size_t i = 0; i < g_config.keyRebinds.rebinds.size(); ++i) {
                    auto& rebind = g_config.keyRebinds.rebinds[i];
                    ImGui::PushID((int)i);

                    // Delete button
                    if (ImGui::Button("X", ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()))) { rebind_to_remove = (int)i; }
                    ImGui::SameLine();

                    // Enable checkbox
                    if (ImGui::Checkbox("##enabled", &rebind.enabled)) {
                        g_configIsDirty = true;
                        std::lock_guard<std::mutex> hotkeyLock(g_hotkeyMainKeysMutex);
                        RebuildHotkeyMainKeys_Internal();
                    }
                    ImGui::SameLine();

                    // --- INPUT KEY ---
                    ImGui::Text("Input:");
                    ImGui::SameLine();
                    std::string fromKeyStr = VkToString(rebind.fromKey);
                    std::string fromLabel = (s_rebindFromKeyToBind == (int)i) ? "[Press key...]##from" : (fromKeyStr + "##from");
                    if (ImGui::Button(fromLabel.c_str(), ImVec2(100, 0))) {
                        s_rebindFromKeyToBind = (int)i;
                        s_rebindOutputVKToBind = -1;
                        s_rebindOutputScanToBind = -1;
                        MarkRebindBindingActive();
                    }
                    if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Click to bind the key to intercept"); }

                    ImGui::SameLine();
                    ImGui::Text("->");
                    ImGui::SameLine();

                    // --- OUTPUT VK CODE ---
                    ImGui::Text("Output:");
                    ImGui::SameLine();
                    DWORD baseVK = rebind.toKey;
                    std::string vkKeyStr = VkToString(baseVK);
                    std::string vkLabel =
                        (s_rebindOutputVKToBind == (int)i) ? "[Press key...]##vk" : (vkKeyStr + " (" + std::to_string(baseVK) + ")##vk");
                    if (ImGui::Button(vkLabel.c_str(), ImVec2(120, 0))) {
                        s_rebindOutputVKToBind = (int)i;
                        s_rebindFromKeyToBind = -1;
                        s_rebindOutputScanToBind = -1;
                        s_rebindTextOverrideVKToBind = -1;
                        MarkRebindBindingActive();
                    }
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip("Click to bind the base output key (affects BOTH text + game keybind by default)");
                    }

                    // Optional text override
                    ImGui::SameLine();
                    bool overrideText = (rebind.useCustomOutput && rebind.customOutputVK != 0);
                    if (ImGui::Checkbox("Override Text##overrideText", &overrideText)) {
                        if (overrideText) {
                            rebind.useCustomOutput = true;
                            rebind.customOutputVK = (rebind.toKey != 0) ? rebind.toKey : rebind.customOutputVK;
                        } else {
                            rebind.customOutputVK = 0;
                            if (rebind.customOutputScanCode == 0) { rebind.useCustomOutput = false; }
                        }
                        g_configIsDirty = true;
                    }
                    ImGui::SameLine();

                    DWORD textVK = (rebind.useCustomOutput && rebind.customOutputVK != 0) ? rebind.customOutputVK : rebind.toKey;
                    std::string textKeyStr = VkToString(textVK);
                    std::string textLabel = (s_rebindTextOverrideVKToBind == (int)i) ? "[Press key...]##textvk" : (textKeyStr + "##textvk");
                    if (overrideText) {
                        if (ImGui::Button(textLabel.c_str(), ImVec2(100, 0))) {
                            s_rebindTextOverrideVKToBind = (int)i;
                            s_rebindFromKeyToBind = -1;
                            s_rebindOutputVKToBind = -1;
                            s_rebindOutputScanToBind = -1;
                            MarkRebindBindingActive();
                        }
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Click to bind what is TYPED (text output). Trigger keybind stays based on Output/Game Keybind.");
                        }
                    } else {
                        ImGui::TextDisabled("(Types: %s)", textKeyStr.c_str());
                        if (ImGui::IsItemHovered()) {
                            ImGui::SetTooltip("Currently types the same as Output. Enable Override Text to change only typed text.");
                        }
                    }

                    ImGui::SameLine();

                    // --- OUTPUT SCAN CODE ---
                    ImGui::Text("Game Keybind:");
                    ImGui::SameLine();
                    // Get the scan code to display - use custom scan if set, otherwise derive from base output.
                    DWORD displayScan = (rebind.useCustomOutput && rebind.customOutputScanCode != 0) ? rebind.customOutputScanCode
                                                                                                    : getScanCodeWithExtendedFlag(rebind.toKey);
                    if (displayScan != 0 && (displayScan & 0xFF00) == 0) {
                        DWORD derived = getScanCodeWithExtendedFlag(rebind.toKey);
                        if ((derived & 0xFF00) != 0 && ((derived & 0xFF) == (displayScan & 0xFF))) { displayScan = derived; }
                    }

                    std::string scanKeyStr;
                    if (displayScan != 0) {
                        DWORD scanDisplayVK = MapVirtualKey(displayScan, MAPVK_VSC_TO_VK_EX);
                        if (scanDisplayVK != 0) {
                            scanKeyStr = VkToString(scanDisplayVK);
                        } else {
                            LONG keyNameLParam = static_cast<LONG>((displayScan & 0xFF) << 16);
                            if ((displayScan & 0xFF00) != 0) { keyNameLParam |= (1 << 24); } // extended key bit

                            char keyName[64] = {};
                            if (GetKeyNameTextA(keyNameLParam, keyName, sizeof(keyName)) > 0) { scanKeyStr = keyName; }
                        }
                    }

                    if (scanKeyStr.empty()) { scanKeyStr = "[Unbound]"; }

                    std::string scanLabel = (s_rebindOutputScanToBind == (int)i) ? "[Press key...]##scan" : (scanKeyStr + "##scan");
                    if (ImGui::Button(scanLabel.c_str(), ImVec2(100, 0))) {
                        s_rebindOutputScanToBind = (int)i;
                        s_rebindFromKeyToBind = -1;
                        s_rebindOutputVKToBind = -1;
                        MarkRebindBindingActive();
                    }
                    if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Click to bind which game keybind is triggered"); }

                    ImGui::PopID();
                }

                // Remove rebind if marked
                if (rebind_to_remove >= 0 && rebind_to_remove < (int)g_config.keyRebinds.rebinds.size()) {
                    g_config.keyRebinds.rebinds.erase(g_config.keyRebinds.rebinds.begin() + rebind_to_remove);
                    g_configIsDirty = true;
                    std::lock_guard<std::mutex> hotkeyLock(g_hotkeyMainKeysMutex);
                    RebuildHotkeyMainKeys_Internal();
                }

                ImGui::Spacing();
                if (ImGui::Button("Add Rebind")) {
                    g_config.keyRebinds.rebinds.push_back(KeyRebind{});
                    g_configIsDirty = true;
                    std::lock_guard<std::mutex> hotkeyLock(g_hotkeyMainKeysMutex);
                    RebuildHotkeyMainKeys_Internal();
                }
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::EndTabItem();
}
