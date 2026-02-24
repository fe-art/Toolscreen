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

                // Inline binding state for the keyboard layout editor (so binding happens without closing/reopening the layout)
                enum class LayoutBindTarget {
                    None,
                    TypesVk,
                    TriggersVk,
                };
                static LayoutBindTarget s_layoutBindTarget = LayoutBindTarget::None;
                static int s_layoutBindIndex = -1;
                static uint64_t s_layoutBindLastSeq = 0;

                // Unicode prompt state for keyboard layout editor
                static int s_layoutUnicodeEditIndex = -1;
                static std::string s_layoutUnicodeEditText;

                // Per-rebind edit buffer for Unicode text output (so the user can type e.g. "ø" or "U+00F8").
                static std::vector<std::string> s_rebindUnicodeTextEdit;

                auto isValidUnicodeScalar = [](uint32_t cp) -> bool {
                    if (cp == 0) return false;
                    if (cp > 0x10FFFFu) return false;
                    if (cp >= 0xD800u && cp <= 0xDFFFu) return false;
                    return true;
                };

                auto codepointToUtf8 = [&](uint32_t cp) -> std::string {
                    if (!isValidUnicodeScalar(cp)) return std::string();
                    std::wstring w;
                    if (cp <= 0xFFFFu) {
                        w.push_back((wchar_t)cp);
                    } else {
                        uint32_t v = cp - 0x10000u;
                        wchar_t high = (wchar_t)(0xD800u + (v >> 10));
                        wchar_t low = (wchar_t)(0xDC00u + (v & 0x3FFu));
                        w.push_back(high);
                        w.push_back(low);
                    }
                    return WideToUtf8(w);
                };

                auto formatCodepointUPlus = [&](uint32_t cp) -> std::string {
                    char buf[32] = {};
                    if (cp <= 0xFFFFu) {
                        sprintf_s(buf, "U+%04X", (unsigned)cp);
                    } else {
                        sprintf_s(buf, "U+%06X", (unsigned)cp);
                    }
                    return std::string(buf);
                };

                auto codepointToDisplay = [&](uint32_t cp) -> std::string {
                    if (!isValidUnicodeScalar(cp)) return std::string("[None]");
                    // Avoid rendering control characters as-is.
                    if (cp < 0x20u || cp == 0x7Fu) return formatCodepointUPlus(cp);
                    std::string s = codepointToUtf8(cp);
                    if (s.empty()) return formatCodepointUPlus(cp);
                    return s;
                };

                auto tryParseUnicodeInput = [&](const std::string& in, uint32_t& outCp) -> bool {
                    auto isSpace = [](unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
                    std::string s = in;
                    while (!s.empty() && isSpace((unsigned char)s.front())) s.erase(s.begin());
                    while (!s.empty() && isSpace((unsigned char)s.back())) s.pop_back();
                    if (s.empty()) return false;

                    auto startsWithI = [&](const char* pfx) {
                        size_t n = std::char_traits<char>::length(pfx);
                        if (s.size() < n) return false;
                        for (size_t i = 0; i < n; ++i) {
                            char a = s[i];
                            char b = pfx[i];
                            if (a >= 'a' && a <= 'z') a = (char)(a - 'a' + 'A');
                            if (b >= 'a' && b <= 'z') b = (char)(b - 'a' + 'A');
                            if (a != b) return false;
                        }
                        return true;
                    };

                    // Explicit formats first
                    std::string hex;
                    if (startsWithI("U+")) hex = s.substr(2);
                    else if (startsWithI("\\\\u") || startsWithI("\\\\U")) hex = s.substr(2);
                    else if (startsWithI("0X")) hex = s.substr(2);

                    if (!hex.empty()) {
                        if (!hex.empty() && hex.front() == '{' && hex.back() == '}') hex = hex.substr(1, hex.size() - 2);
                        try {
                            size_t idx = 0;
                            unsigned long v = std::stoul(hex, &idx, 16);
                            if (idx == 0) return false;
                            if (!isValidUnicodeScalar((uint32_t)v)) return false;
                            outCp = (uint32_t)v;
                            return true;
                        } catch (...) {
                            return false;
                        }
                    }

                    // Single-character (or first codepoint) input (UTF-8)
                    std::wstring w = Utf8ToWide(s);
                    if (!w.empty()) {
                        uint32_t cp = 0;
                        if (w.size() >= 2 && w[0] >= 0xD800 && w[0] <= 0xDBFF && w[1] >= 0xDC00 && w[1] <= 0xDFFF) {
                            cp = 0x10000u + (((uint32_t)w[0] - 0xD800u) << 10) + ((uint32_t)w[1] - 0xDC00u);
                        } else {
                            cp = (uint32_t)w[0];
                        }
                        if (isValidUnicodeScalar(cp)) {
                            outCp = cp;
                            return true;
                        }
                    }

                    // Plain hex without prefix
                    try {
                        size_t idx = 0;
                        unsigned long v = std::stoul(s, &idx, 16);
                        if (idx == 0) return false;
                        if (!isValidUnicodeScalar((uint32_t)v)) return false;
                        outCp = (uint32_t)v;
                        return true;
                    } catch (...) {
                        return false;
                    }
                };

                auto syncUnicodeEditBuffers = [&]() {
                    if (s_rebindUnicodeTextEdit.size() != g_config.keyRebinds.rebinds.size()) {
                        s_rebindUnicodeTextEdit.resize(g_config.keyRebinds.rebinds.size());
                    }
                };

                // ---------------------------------------------------------------------
                // Keyboard layout visualization
                // ---------------------------------------------------------------------
                static bool s_keyboardLayoutOpen = false;
                // Larger default so the layout is readable without fiddling.
                static float s_keyboardLayoutScale = 1.45f;

                if (ImGui::Button("Open Keyboard Layout")) { s_keyboardLayoutOpen = true; }
                ImGui::SameLine();
                HelpMarker("Shows a visual keyboard. Keys with a configured rebind are outlined.\n"
                           "Outlined keys show the physical label, then a second line like \"B & C\" meaning:\n"
                           "  - B = what it types (text/VK output)\n"
                           "  - C = what game keybind it triggers (scan/VK)\n"
                           "Right-click a key to configure rebinds.\n"
                           "This is a visualization only.");

                if (s_keyboardLayoutOpen) {
                    // Default to a moderately large modal so the keyboard + mouse are visible without taking over the screen.
                    // Use the work area size (excludes taskbar/docking) and clamp to a sensible minimum.
                    const ImGuiViewport* vp = ImGui::GetMainViewport();
                    ImVec2 target = vp ? vp->WorkSize : ImVec2(1400.0f, 800.0f);
                    target.x *= 0.70f;
                    target.y *= 0.62f;
                    if (target.x < 920.0f) target.x = 920.0f;
                    if (target.y < 560.0f) target.y = 560.0f;
                    ImGui::SetNextWindowSize(target, ImGuiCond_Appearing);
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

                // Center the modal and force an opaque background.
                {
                    const ImGuiViewport* vp = ImGui::GetMainViewport();
                    const ImVec2 center = vp ? ImVec2(vp->WorkPos.x + vp->WorkSize.x * 0.5f, vp->WorkPos.y + vp->WorkSize.y * 0.5f)
                                              : ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f);
                    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
                    ImGui::SetNextWindowBgAlpha(1.0f);
                }

                ImGui::PushStyleColor(ImGuiCol_PopupBg, IM_COL32(18, 19, 22, 255));
                if (ImGui::BeginPopupModal("Keyboard Layout", &s_keyboardLayoutOpen, ImGuiWindowFlags_NoScrollbar)) {
                    // While this modal is open, treat it like a binding interaction so the global ESC-to-close-GUI
                    // hotkey doesn't close the whole settings UI.
                    MarkRebindBindingActive();

                    const bool anyRebindBindUiActive = (s_rebindFromKeyToBind != -1) || (s_rebindOutputVKToBind != -1) ||
                                                      (s_rebindTextOverrideVKToBind != -1) || (s_rebindOutputScanToBind != -1) ||
                                                      (s_layoutBindTarget != LayoutBindTarget::None) || (s_layoutUnicodeEditIndex != -1);

                    if (ImGui::IsKeyPressed(ImGuiKey_Escape) && !anyRebindBindUiActive) {
                        s_keyboardLayoutOpen = false;
                        ImGui::CloseCurrentPopup();
                    }

                    // View controls (inside the popup, not on the main config page)
                    {
                        float scalePct = s_keyboardLayoutScale * 100.0f;
                        ImGui::TextUnformatted("Scale:");
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(220.0f);
                        if (ImGui::SliderFloat("##keyboardLayoutScalePct", &scalePct, 60.0f, 160.0f, "%.0f%%", ImGuiSliderFlags_AlwaysClamp)) {
                            s_keyboardLayoutScale = scalePct / 100.0f;
                        }
                        ImGui::SameLine();
                        HelpMarker("Visual scale for the keyboard layout preview only.");

                        ImGui::Spacing();
                        ImGui::Separator();
                        ImGui::Spacing();
                    }

                    // Make the keyboard region scrollable (both axes) to fit on smaller windows.
                    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(14, 15, 18, 255));
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
                                                // NOTE: The first spacer is intentionally 1.0u so that F1 centers above the '2' key
                                                // and the right edge of F12 aligns with Backspace/Backslash/Enter/RShift/RCtrl.
                                                { Key(VK_ESCAPE), Spacer(1.0f), Key(VK_F1), Key(VK_F2), Key(VK_F3), Key(VK_F4), Spacer(0.5f), Key(VK_F5),
                          Key(VK_F6), Key(VK_F7), Key(VK_F8), Spacer(0.5f), Key(VK_F9), Key(VK_F10), Key(VK_F11), Key(VK_F12),
                                                    Spacer(0.5f), Key(VK_SNAPSHOT, 1.25f, "PRTSC"), Key(VK_SCROLL, 1.25f, "SCRLK"), Key(VK_PAUSE, 1.25f, "PAUSE") },

                        // Number row + nav + numpad
                        { Key(VK_OEM_3, 1.0f, "`") , Key('1'), Key('2'), Key('3'), Key('4'), Key('5'), Key('6'), Key('7'), Key('8'), Key('9'),
                          Key('0'), Key(VK_OEM_MINUS, 1.0f, "-"), Key(VK_OEM_PLUS, 1.0f, "="), Key(VK_BACK, 2.0f, "BACK"), Spacer(0.5f),
                                                    Key(VK_INSERT, 1.25f, "INS"), Key(VK_HOME, 1.25f, "HOME"), Key(VK_PRIOR, 1.25f, "PGUP"), Spacer(0.5f),
                                                    Key(VK_NUMLOCK, 1.25f, "NUMLK"), Key(VK_DIVIDE, 1.25f, "/"), Key(VK_MULTIPLY, 1.25f, "*"), Key(VK_SUBTRACT, 1.25f, "-") },

                        // Q row
                        { Key(VK_TAB, 1.5f, "TAB"), Key('Q'), Key('W'), Key('E'), Key('R'), Key('T'), Key('Y'), Key('U'), Key('I'), Key('O'), Key('P'),
                          Key(VK_OEM_4, 1.0f, "["), Key(VK_OEM_6, 1.0f, "]"), Key(VK_OEM_5, 1.5f, "\\"), Spacer(0.5f),
                                                    Key(VK_DELETE, 1.25f, "DEL"), Key(VK_END, 1.25f, "END"), Key(VK_NEXT, 1.25f, "PGDN"), Spacer(0.5f),
                                                    Key(VK_NUMPAD7, 1.25f, "NUM7"), Key(VK_NUMPAD8, 1.25f, "NUM8"), Key(VK_NUMPAD9, 1.25f, "NUM9"), Key(VK_ADD, 1.25f, "+") },

                        // A row
                        { Key(VK_CAPITAL, 1.75f, "CAPS"), Key('A'), Key('S'), Key('D'), Key('F'), Key('G'), Key('H'), Key('J'), Key('K'), Key('L'),
                          Key(VK_OEM_1, 1.0f, ";"), Key(VK_OEM_7, 1.0f, "'"), Key(VK_RETURN, 2.25f, "ENTER"), Spacer(0.5f),
                                                    // Keep the same implicit gaps as other rows by using 3 separate spacers (INS/HOME/PGUP column widths).
                                                                                                        Spacer(1.25f), Spacer(1.25f), Spacer(1.25f), Spacer(0.5f),
                                                                                                        Key(VK_NUMPAD4, 1.25f, "NUM4"), Key(VK_NUMPAD5, 1.25f, "NUM5"), Key(VK_NUMPAD6, 1.25f, "NUM6"),
                                                    Spacer(1.25f) },

                        // Z row + arrows + numpad
                        { Key(VK_LSHIFT, 2.25f, "LSHIFT"), Key('Z'), Key('X'), Key('C'), Key('V'), Key('B'), Key('N'), Key('M'),
                          Key(VK_OEM_COMMA, 1.0f, ","), Key(VK_OEM_PERIOD, 1.0f, "."), Key(VK_OEM_2, 1.0f, "/"), Key(VK_RSHIFT, 2.75f, "RSHIFT"),
                                                    Spacer(0.5f), Spacer(1.25f), Key(VK_UP, 1.25f, "UP"), Spacer(1.25f), Spacer(0.5f),
                                                    Key(VK_NUMPAD1, 1.25f, "NUM1"), Key(VK_NUMPAD2, 1.25f, "NUM2"), Key(VK_NUMPAD3, 1.25f, "NUM3"), Key(VK_RETURN, 1.25f, "ENTER") },

                        // Bottom row
                                                { Key(VK_LCONTROL, 1.25f, "LCTRL"), Key(VK_LWIN, 1.25f, "LWIN"), Key(VK_LMENU, 1.25f, "LALT"),
                                                    Key(VK_SPACE, 6.25f, "SPACE"), Key(VK_RMENU, 1.25f, "RALT"), Key(VK_RWIN, 1.25f, "RWIN"), Key(VK_APPS, 1.25f, "APPS"),
                                                    Key(VK_RCONTROL, 1.25f, "RCTRL"), Spacer(0.5f), Key(VK_LEFT, 1.25f, "LEFT"), Key(VK_DOWN, 1.25f, "DOWN"), Key(VK_RIGHT, 1.25f, "RIGHT"),
                                                    Spacer(0.5f), Key(VK_NUMPAD0, 2.5f, "NUM0"), Key(VK_DECIMAL, 1.25f, "NUM."), Spacer(1.25f) },
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

                    // Slightly squarer keycaps (closer to real keyboards).
                    // IMPORTANT: round to whole pixels to keep row edges perfectly aligned.
                    auto roundPx = [](float v) -> float { return (float)(int)(v + 0.5f); };

                    // -----------------------------------------------------------------
                    // Keyboard layout tuning (edit these constants)
                    // -----------------------------------------------------------------
                    // Overall scale multiplier (in addition to the UI slider).
                    constexpr float kKeyboardScaleMult = 1.0f;
                    // Key sizing.
                    constexpr float kKeyHeightMul = 1.55f;     // key height = frameHeight * mul * scale
                    constexpr float kKeyUnitMul = 0.92f;       // base 1.0u width = keyH * mul (before quantization)
                    // Spacing.
                    constexpr float kKeyGapMul = 1.00f;        // gap derived from ImGui style spacing
                    constexpr float kKeyCapInsetXMul = 0.55f;  // visual margin inside pitch rect (x)
                    constexpr float kKeyCapInsetYMul = 0.45f;  // visual margin inside pitch rect (y)
                    // Rounding.
                    constexpr float kKeyRoundingPx = 5.0f;     // corner radius (before scale)

                    const float keyboardScale = s_keyboardLayoutScale * kKeyboardScaleMult;

                    const float keyH = roundPx(ImGui::GetFrameHeight() * kKeyHeightMul * keyboardScale);
                    // Quantize unit to a multiple of 4 pixels so all 0.25-unit keys become exact integers.
                    float unit = roundPx(keyH * kKeyUnitMul);
                    unit = (float)(((int)(unit + 2.0f) / 4) * 4);
                    if (unit < 20.0f) unit = 20.0f;
                    float gap = roundPx(ImGui::GetStyle().ItemInnerSpacing.x * keyboardScale * kKeyGapMul);
                    if (gap < 1.0f) gap = 1.0f;
                    const float rounding = roundPx(kKeyRoundingPx * keyboardScale);
                    ImDrawList* dl = ImGui::GetWindowDrawList();

                    // Use a unit-based pitch horizontally (no accumulated "gap" per key) so that the right edges of
                    // F12/Backspace/Backslash/Enter/RShift/RCtrl land on the exact same pixel column.
                    // Visual spacing is created by drawing keycaps inset inside their pitch rectangles.
                    const float pitchX = unit;
                    const float keyPadX = roundPx(gap * kKeyCapInsetXMul);
                    const float keyPadY = roundPx(gap * kKeyCapInsetYMul);

                    // Pre-compute keyboard width so we can place the mouse panel to the right.
                    float keyboardMaxRowW = 0.0f;
                    for (const auto& row : rows) {
                        float w = 0.0f;
                        for (size_t c = 0; c < row.size(); ++c) {
                            w += row[c].w * pitchX;
                        }
                        if (w > keyboardMaxRowW) keyboardMaxRowW = w;
                    }

                    const ImVec2 layoutStart = ImGui::GetCursorPos();
                    const ImVec2 layoutStartScreen = ImGui::GetCursorScreenPos();
                    const float keyboardTotalH = (float)rows.size() * (keyH + gap);

                    // Keyboard base plate (behind keycaps)
                    {
                        const float platePad = 10.0f * keyboardScale;
                        const float plateRound = 10.0f * keyboardScale;
                        const ImVec2 plateMin = ImVec2(layoutStartScreen.x - platePad, layoutStartScreen.y - platePad);
                        const ImVec2 plateMax =
                            ImVec2(layoutStartScreen.x + keyboardMaxRowW + platePad, layoutStartScreen.y + keyboardTotalH - gap + platePad);

                        // Shadow
                        dl->AddRectFilled(ImVec2(plateMin.x + 5, plateMin.y + 6), ImVec2(plateMax.x + 5, plateMax.y + 6),
                                          IM_COL32(0, 0, 0, 130), plateRound);

                        // Plate fill (rounded). We avoid AddRectFilledMultiColor here because it doesn't support rounded corners,
                        // which caused square corners to show through the rounded border.
                        const ImU32 plateTop = IM_COL32(35, 38, 46, 255);
                        const ImU32 plateBot = IM_COL32(18, 20, 26, 255);
                        dl->AddRectFilled(plateMin, plateMax, plateBot, plateRound);
                        const float plateSplit = 0.55f;
                        const ImVec2 plateMid = ImVec2(plateMax.x, plateMin.y + (plateMax.y - plateMin.y) * plateSplit);
                        dl->AddRectFilled(plateMin, plateMid, plateTop, plateRound, ImDrawFlags_RoundCornersTop);
                        dl->AddRect(plateMin, plateMax, IM_COL32(10, 10, 12, 255), plateRound);
                        // Small highlight line
                        dl->AddLine(ImVec2(plateMin.x + 6, plateMin.y + 6), ImVec2(plateMax.x - 6, plateMin.y + 6), IM_COL32(255, 255, 255, 25),
                                    1.0f);
                    }

                    // Context menu state for right-click editing
                    static DWORD s_layoutContextVk = 0;
                    static int s_layoutContextPreferredIndex = -1;

                    // IMPORTANT: Popup identifiers are relative to the current ID stack.
                    // We compute the popup ID here (outside per-key PushID scopes) and open it by ID from nested stacks.
                    const ImGuiID rebindPopupId = ImGui::GetID("Rebind Config##layout");
                    auto openRebindContextFor = [&](DWORD vk) {
                        s_layoutContextVk = vk;
                        s_layoutContextPreferredIndex = -1;
                        ImGui::OpenPopup(rebindPopupId);
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
                        // Make the bottom tone slightly lighter overall.
                        theme.bottom = adjust(theme.bottom, 10);
                        if (hovered) {
                            theme.top = adjust(theme.top, 12);
                            theme.bottom = adjust(theme.bottom, 10);
                        }
                        if (active) {
                            theme.top = adjust(theme.top, -8);
                            theme.bottom = adjust(theme.bottom, -16);
                        }

                        const ImVec2 size = ImVec2(pMax.x - pMin.x, pMax.y - pMin.y);
                        const float shadow = 2.0f * keyboardScale;
                        const ImVec2 pressOff = active ? ImVec2(0.0f, 1.2f * keyboardScale) : ImVec2(0, 0);

                        // Shadow
                        dl->AddRectFilled(ImVec2(pMin.x + shadow, pMin.y + shadow), ImVec2(pMax.x + shadow, pMax.y + shadow),
                                          IM_COL32(0, 0, 0, 90), rounding);

                        const ImVec2 kMin = ImVec2(pMin.x + pressOff.x, pMin.y + pressOff.y);
                        const ImVec2 kMax = ImVec2(pMax.x + pressOff.x, pMax.y + pressOff.y);

                        // Keycap fill (rounded). Avoid AddRectFilledMultiColor to prevent square corners showing through.
                        dl->AddRectFilled(kMin, kMax, theme.bottom, rounding);
                        // Push the darker portion lower on the keycap.
                        const float split = 0.70f;
                        const ImVec2 kMid = ImVec2(kMax.x, kMin.y + (kMax.y - kMin.y) * split);
                        dl->AddRectFilled(kMin, kMid, theme.top, rounding, ImDrawFlags_RoundCornersTop);

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

                        const float padX = 6.0f * keyboardScale;
                        const float padY = 4.0f * keyboardScale;

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

                        // Rebind summary under the physical label: "B & C"
                        if (rb && rb->fromKey != 0 && rb->toKey != 0) {
                            const bool isNoOp = [&]() -> bool {
                                if (rb->toKey != vk) return false;
                                if (rb->customOutputVK != 0 && rb->customOutputVK != rb->toKey) return false;
                                if (rb->customOutputUnicode != 0) return false;
                                if (rb->customOutputScanCode != 0) return false;
                                return true;
                            }();

                            if (!isNoOp) {
                                DWORD triggerVK = rb->toKey;
                                DWORD typesVK = (rb->useCustomOutput && rb->customOutputVK != 0) ? rb->customOutputVK : triggerVK;
                                DWORD outScan = (rb->useCustomOutput && rb->customOutputScanCode != 0) ? rb->customOutputScanCode
                                                                                                        : getScanCodeWithExtendedFlag(triggerVK);
                                if (outScan != 0 && (outScan & 0xFF00) == 0) {
                                    DWORD derived = getScanCodeWithExtendedFlag(triggerVK);
                                    if ((derived & 0xFF00) != 0 && ((derived & 0xFF) == (outScan & 0xFF))) { outScan = derived; }
                                }

                                const std::string typesOut = (rb->useCustomOutput && rb->customOutputUnicode != 0)
                                                                 ? codepointToDisplay((uint32_t)rb->customOutputUnicode)
                                                                 : VkToString(typesVK);
                                const std::string triggersOut = scanCodeToDisplayName(outScan, triggerVK);
                                const std::string summary = typesOut + " & " + triggersOut;

                                // Only render the 2nd line when we have enough height to avoid overlap.
                                const float minHForSecondLine = ImGui::GetFontSize() * 2.05f;
                                if (size.y >= minHForSecondLine) {
                                    ImFont* f = ImGui::GetFont();
                                    float fs = ImGui::GetFontSize() * 0.72f;
                                    ImVec2 ssz = f->CalcTextSizeA(fs, FLT_MAX, 0.0f, summary.c_str());
                                    const float maxW = size.x - padX * 2.0f;
                                    if (maxW > 8.0f && ssz.x > maxW) {
                                        float scale = maxW / (ssz.x + 0.001f);
                                        if (scale < 0.55f) scale = 0.55f;
                                        if (scale > 1.0f) scale = 1.0f;
                                        fs *= scale;
                                        ssz = f->CalcTextSizeA(fs, FLT_MAX, 0.0f, summary.c_str());
                                    }

                                    float y = labelPos.y + labelSz.y + 1.0f * keyboardScale;
                                    const float maxY = kMax.y - padY - ssz.y;
                                    if (y > maxY) y = maxY;
                                    if (y < kMin.y + padY) y = kMin.y + padY;
                                    float x = kMin.x + (size.x - ssz.x) * 0.5f;
                                    if (x < kMin.x + padX) x = kMin.x + padX;

                                    const ImU32 infoCol = rb->enabled ? IM_COL32(245, 245, 245, 235) : IM_COL32(255, 220, 170, 235);
                                    dl->AddText(f, fs, ImVec2(x, y), infoCol, summary.c_str());
                                }
                            }
                        }

                        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                            openRebindContextFor(vk);
                        }
                    };

                    // Capture anchors for double-height numpad keys.
                    bool haveNumpadPlusAnchor = false;
                    ImVec2 numpadPlusAnchorMin = ImVec2(0, 0);
                    ImVec2 numpadPlusAnchorMax = ImVec2(0, 0);
                    bool haveNumpadEnterAnchor = false;
                    ImVec2 numpadEnterAnchorMin = ImVec2(0, 0);
                    ImVec2 numpadEnterAnchorMax = ImVec2(0, 0);

                    auto snapPx = [](float v) -> float { return (float)(int)(v + 0.5f); };

                    for (size_t rowIdx = 0; rowIdx < rows.size(); ++rowIdx) {
                        float xCursor = layoutStart.x;
                        float yCursor = layoutStart.y + (float)rowIdx * (keyH + gap);

                        for (size_t colIdx = 0; colIdx < rows[rowIdx].size(); ++colIdx) {
                            const KeyCell& kc = rows[rowIdx][colIdx];
                            const float keyW = kc.w * pitchX;

                            // Snap to pixels so row edges line up cleanly (avoids 1px drift from float accumulation).
                            ImGui::SetCursorPos(ImVec2(snapPx(xCursor), snapPx(yCursor)));
                            if (kc.vk == 0) {
                                // Spacer
                                ImGui::Dummy(ImVec2(keyW, keyH));
                                xCursor += keyW;
                                xCursor = snapPx(xCursor);
                                continue;
                            }

                            // Numpad '+' should be double-height (spans Q-row and A-row). We draw it once later.
                            if (kc.vk == VK_ADD && rowIdx == 2) {
                                const ImVec2 aMin = ImGui::GetCursorScreenPos();
                                ImGui::Dummy(ImVec2(keyW, keyH));
                                const ImVec2 aMax = ImVec2(aMin.x + keyW, aMin.y + keyH);
                                haveNumpadPlusAnchor = true;
                                numpadPlusAnchorMin = aMin;
                                numpadPlusAnchorMax = aMax;
                                xCursor += keyW;
                                xCursor = snapPx(xCursor);
                                continue;
                            }

                            // Numpad 'Enter' should be double-height (spans Z-row and bottom row). We draw it once later.
                            if (kc.vk == VK_RETURN && rowIdx == 4 && kc.w < 1.6f) {
                                const ImVec2 aMin = ImGui::GetCursorScreenPos();
                                ImGui::Dummy(ImVec2(keyW, keyH));
                                const ImVec2 aMax = ImVec2(aMin.x + keyW, aMin.y + keyH);
                                haveNumpadEnterAnchor = true;
                                numpadEnterAnchorMin = aMin;
                                numpadEnterAnchorMax = aMax;
                                xCursor += keyW;
                                xCursor = snapPx(xCursor);
                                continue;
                            }

                            ImGui::PushID((int)(rowIdx * 1000 + colIdx));
                            ImGui::InvisibleButton("##key", ImVec2(keyW, keyH),
                                                  ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);

                            const ImVec2 pMin = ImGui::GetItemRectMin();
                            const ImVec2 pMax = ImGui::GetItemRectMax();

                            const KeyRebind* rb = findRebindForKey(kc.vk);

                            std::string keyName = kc.labelOverride ? std::string(kc.labelOverride) : VkToString(kc.vk);
                            ImVec2 capMin = ImVec2(pMin.x + keyPadX, pMin.y + keyPadY);
                            ImVec2 capMax = ImVec2(pMax.x - keyPadX, pMax.y - keyPadY);
                            if (capMax.x <= capMin.x + 2.0f) { capMin.x = pMin.x; capMax.x = pMax.x; }
                            if (capMax.y <= capMin.y + 2.0f) { capMin.y = pMin.y; capMax.y = pMax.y; }
                            drawKeyCell(kc.vk, keyName.c_str(), capMin, capMax, rb);

                            if (ImGui::IsItemHovered()) {
                                ImGui::BeginTooltip();
                                ImGui::Text("%s (%u)", VkToString(kc.vk).c_str(), (unsigned)kc.vk);
                                if (!(rb && rb->fromKey != 0 && rb->toKey != 0)) {
                                    ImGui::TextUnformatted("Right-click to configure rebinds.");
                                }
                                ImGui::EndTooltip();
                            }

                            ImGui::PopID();
                            xCursor += keyW;
                            xCursor = snapPx(xCursor);
                        }

                        // no cursor advance: absolute positioning
                    }

                    // Draw double-height numpad keys (after normal pass, so they appear on top).
                    auto drawTallKey = [&](DWORD vk, const char* label, const ImVec2& anchorMin, const ImVec2& anchorMax) {
                        const float w = anchorMax.x - anchorMin.x;
                        const float h = keyH * 2.0f + gap;

                        ImGui::PushID((int)vk);
                        ImGui::SetCursorScreenPos(anchorMin);
                        ImGui::InvisibleButton("##tall", ImVec2(w, h),
                                              ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
                        const ImVec2 pMin = ImGui::GetItemRectMin();
                        const ImVec2 pMax = ImVec2(pMin.x + w, pMin.y + h);

                        const KeyRebind* rb = findRebindForKey(vk);
                        ImVec2 capMin = ImVec2(pMin.x + keyPadX, pMin.y + keyPadY);
                        ImVec2 capMax = ImVec2(pMax.x - keyPadX, pMax.y - keyPadY);
                        if (capMax.x <= capMin.x + 2.0f) { capMin.x = pMin.x; capMax.x = pMax.x; }
                        if (capMax.y <= capMin.y + 2.0f) { capMin.y = pMin.y; capMax.y = pMax.y; }
                        drawKeyCell(vk, label, capMin, capMax, rb);

                        if (ImGui::IsItemHovered()) {
                            ImGui::BeginTooltip();
                            ImGui::Text("%s (%u)", VkToString(vk).c_str(), (unsigned)vk);
                            if (!(rb && rb->fromKey != 0 && rb->toKey != 0)) {
                                ImGui::TextUnformatted("Right-click to configure rebinds.");
                            }
                            ImGui::EndTooltip();
                        }

                        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                            openRebindContextFor(vk);
                        }

                        ImGui::PopID();
                    };

                    if (haveNumpadPlusAnchor) {
                        drawTallKey(VK_ADD, "+", numpadPlusAnchorMin, numpadPlusAnchorMax);
                    }
                    if (haveNumpadEnterAnchor) {
                        drawTallKey(VK_RETURN, "ENTER", numpadEnterAnchorMin, numpadEnterAnchorMax);
                    }

                    // -----------------------------------------------------------------
                    // Mouse (to the right of the keyboard)
                    // -----------------------------------------------------------------
                    const float mousePanelX = layoutStart.x + keyboardMaxRowW + unit * 0.9f;
                    const float mousePanelY = layoutStart.y;
                    ImGui::SetCursorPos(ImVec2(mousePanelX, mousePanelY));

                    // No header text here (keeps the mouse diagram compact).
                    const float mouseHeaderH = 0.0f;

                    float mouseDiagramTotalH = mouseHeaderH;

                    // Mouse diagram (mouse-shaped buttons)
                    {
                        const float mouseW = unit * 3.6f;
                        const float mouseH = (keyboardTotalH - mouseHeaderH - gap);
                        mouseDiagramTotalH = mouseHeaderH + mouseH;
                        const float pad = 6.0f * keyboardScale;

                        ImGui::SetCursorPos(ImVec2(mousePanelX, mousePanelY + mouseHeaderH));
                        ImGui::Dummy(ImVec2(mouseW, mouseH));
                        const ImVec2 bodyMin = ImGui::GetItemRectMin();
                        const ImVec2 bodyMax = ImGui::GetItemRectMax();

                        const float bodyR = (mouseW < mouseH ? mouseW : mouseH) * 0.45f;
                        dl->AddRectFilled(bodyMin, bodyMax, IM_COL32(24, 26, 33, 255), bodyR);
                        dl->AddRect(bodyMin, bodyMax, IM_COL32(10, 10, 12, 255), bodyR, 0, 1.5f);

                        const ImVec2 innerMin = ImVec2(bodyMin.x + pad, bodyMin.y + pad);
                        const ImVec2 innerMax = ImVec2(bodyMax.x - pad, bodyMax.y - pad);
                        const float midX = (innerMin.x + innerMax.x) * 0.5f;
                        const float topH = (innerMax.y - innerMin.y) * 0.52f;
                        const float splitY = innerMin.y + topH;

                        auto drawMouseSegment = [&](DWORD vk, const char* label, const ImVec2& segMin, const ImVec2& segMax, float segR, ImDrawFlags segFlags) {
                            const bool hovered = ImGui::IsItemHovered();
                            const bool active = ImGui::IsItemActive();

                            // Reuse key theming for mouse buttons.
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
                            KeyTheme theme{ IM_COL32(88, 90, 108, 255), IM_COL32(44, 46, 60, 255), IM_COL32(18, 18, 20, 255),
                                            IM_COL32(245, 245, 245, 255) };
                            // Lighter bottom tone to match keycaps.
                            theme.bottom = adjust(theme.bottom, 10);
                            if (hovered) {
                                theme.top = adjust(theme.top, 12);
                                theme.bottom = adjust(theme.bottom, 10);
                            }
                            if (active) {
                                theme.top = adjust(theme.top, -8);
                                theme.bottom = adjust(theme.bottom, -16);
                            }

                            // Fill (rounded) + top tone (rounded top corners only where applicable)
                            dl->AddRectFilled(segMin, segMax, theme.bottom, segR, segFlags);
                            // Push the darker portion lower.
                            const ImVec2 segMid = ImVec2(segMax.x, segMin.y + (segMax.y - segMin.y) * 0.72f);
                            ImDrawFlags topFlags = segFlags;
                            // Only round top corners for the overlay.
                            if (segFlags == ImDrawFlags_RoundCornersAll) {
                                topFlags = ImDrawFlags_RoundCornersTop;
                            } else {
                                // Keep only top-related flags.
                                topFlags &= (ImDrawFlags_RoundCornersTopLeft | ImDrawFlags_RoundCornersTopRight);
                            }
                            dl->AddRectFilled(segMin, segMid, theme.top, segR, topFlags);
                            // No top highlight line here: it can show as a stray 1px line against the rounded mouse body.
                            dl->AddRect(segMin, segMax, theme.border, segR, segFlags, 1.0f);

                            const KeyRebind* rb = findRebindForKey(vk);
                            if (rb && rb->fromKey != 0 && rb->toKey != 0) {
                                const ImU32 outline = rb->enabled ? IM_COL32(0, 220, 110, 255) : IM_COL32(255, 170, 0, 255);
                                dl->AddRect(segMin, segMax, outline, segR, segFlags, 3.0f);
                            }

                            // Label
                            ImFont* f = ImGui::GetFont();
                            const float fs = ImGui::GetFontSize() * 0.85f;
                            const ImVec2 tsz = f->CalcTextSizeA(fs, FLT_MAX, 0.0f, label);
                            const ImVec2 tpos = ImVec2(segMin.x + (segMax.x - segMin.x - tsz.x) * 0.5f, segMin.y + (segMax.y - segMin.y - tsz.y) * 0.5f);
                            dl->AddText(f, fs, tpos, theme.text, label);

                            // Tooltip + context
                            if (ImGui::IsItemHovered()) {
                                ImGui::BeginTooltip();
                                ImGui::Text("Input: %s (%u)", VkToString(vk).c_str(), (unsigned)vk);
                                if (!(rb && rb->fromKey != 0 && rb->toKey != 0)) {
                                    ImGui::TextUnformatted("Right-click to configure rebinds.");
                                }
                                ImGui::EndTooltip();
                            }
                            if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
                                openRebindContextFor(vk);
                            }
                        };

                        // Segment rectangles
                        const ImVec2 leftMin = innerMin;
                        const ImVec2 leftMax = ImVec2(midX, splitY);
                        const ImVec2 rightMin = ImVec2(midX, innerMin.y);
                        const ImVec2 rightMax = ImVec2(innerMax.x, splitY);

                        const float wheelW = (innerMax.x - innerMin.x) * 0.16f;
                        const float wheelH = topH * 0.55f;
                        const ImVec2 wheelMin = ImVec2(midX - wheelW * 0.5f, innerMin.y + topH * 0.18f);
                        const ImVec2 wheelMax = ImVec2(midX + wheelW * 0.5f, wheelMin.y + wheelH);

                        const float sideW = (innerMax.x - innerMin.x) * 0.32f;
                        const float sideH = (innerMax.y - innerMin.y) * 0.12f;
                        const float sideX0 = innerMin.x + (innerMax.x - innerMin.x) * 0.07f;
                        const float sideY0 = innerMin.y + topH + (innerMax.y - innerMin.y - topH) * 0.26f;
                        const float sideGap = sideH * 0.35f;
                        const ImVec2 side1Min = ImVec2(sideX0, sideY0);
                        const ImVec2 side1Max = ImVec2(sideX0 + sideW, sideY0 + sideH);
                        const ImVec2 side2Min = ImVec2(sideX0, sideY0 + sideH + sideGap);
                        const ImVec2 side2Max = ImVec2(sideX0 + sideW, sideY0 + sideH + sideGap + sideH);

                        // Divider lines
                        dl->AddLine(ImVec2(midX, innerMin.y + 2), ImVec2(midX, splitY - 2), IM_COL32(10, 10, 12, 255), 1.0f);
                        dl->AddLine(ImVec2(innerMin.x + 2, splitY), ImVec2(innerMax.x - 2, splitY), IM_COL32(10, 10, 12, 255), 1.0f);

                        // Left button
                        ImGui::SetCursorScreenPos(leftMin);
                        ImGui::InvisibleButton("##mouse_left", ImVec2(leftMax.x - leftMin.x, leftMax.y - leftMin.y),
                                              ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
                        drawMouseSegment(VK_LBUTTON, "M1", leftMin, leftMax, bodyR * 0.75f, ImDrawFlags_RoundCornersTopLeft);

                        // Right button
                        ImGui::SetCursorScreenPos(rightMin);
                        ImGui::InvisibleButton("##mouse_right", ImVec2(rightMax.x - rightMin.x, rightMax.y - rightMin.y),
                                              ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
                        drawMouseSegment(VK_RBUTTON, "M2", rightMin, rightMax, bodyR * 0.75f, ImDrawFlags_RoundCornersTopRight);

                        // Wheel / middle
                        ImGui::SetCursorScreenPos(wheelMin);
                        ImGui::InvisibleButton("##mouse_mid", ImVec2(wheelMax.x - wheelMin.x, wheelMax.y - wheelMin.y),
                                              ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
                        drawMouseSegment(VK_MBUTTON, "M3", wheelMin, wheelMax, 6.0f * keyboardScale, ImDrawFlags_RoundCornersAll);

                        // Side buttons (typically on left side)
                        ImGui::SetCursorScreenPos(side1Min);
                        ImGui::InvisibleButton("##mouse_x1", ImVec2(side1Max.x - side1Min.x, side1Max.y - side1Min.y),
                                              ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
                        drawMouseSegment(VK_XBUTTON1, "M4", side1Min, side1Max, 6.0f * keyboardScale, ImDrawFlags_RoundCornersAll);

                        ImGui::SetCursorScreenPos(side2Min);
                        ImGui::InvisibleButton("##mouse_x2", ImVec2(side2Max.x - side2Min.x, side2Max.y - side2Min.y),
                                              ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
                        drawMouseSegment(VK_XBUTTON2, "M5", side2Min, side2Max, 6.0f * keyboardScale, ImDrawFlags_RoundCornersAll);
                    }

                    // Right-click context menu
                    ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_Appearing);
                    if (ImGui::BeginPopup("Rebind Config##layout")) {
                        // Also block global ESC-to-close-GUI while editing inside this popup.
                        MarkRebindBindingActive();

                        // -----------------------------------------------------------------
                        // Simplified inline editor (single rebind per key in this popup)
                        // -----------------------------------------------------------------
                        syncUnicodeEditBuffers();

                        auto isNoOpRebindForKey = [&](const KeyRebind& r, DWORD originalVk) -> bool {
                            if (r.fromKey != originalVk) return false;
                            if (r.toKey != originalVk) return false;
                            // Redundant customOutputVK that equals base output doesn't change behavior.
                            if (r.customOutputVK != 0 && r.customOutputVK != r.toKey) return false;
                            if (r.customOutputUnicode != 0) return false;
                            if (r.customOutputScanCode != 0) return false;
                            return true;
                        };

                        auto eraseRebindIndex = [&](int eraseIdx) {
                            if (eraseIdx < 0 || eraseIdx >= (int)g_config.keyRebinds.rebinds.size()) return;
                            g_config.keyRebinds.rebinds.erase(g_config.keyRebinds.rebinds.begin() + eraseIdx);
                            g_configIsDirty = true;
                            std::lock_guard<std::mutex> hotkeyLock(g_hotkeyMainKeysMutex);
                            RebuildHotkeyMainKeys_Internal();

                            auto decIfAfter = [&](int& v) {
                                if (v == -1) return;
                                if (v == eraseIdx) {
                                    v = -1;
                                } else if (v > eraseIdx) {
                                    v -= 1;
                                }
                            };
                            decIfAfter(s_rebindFromKeyToBind);
                            decIfAfter(s_rebindOutputVKToBind);
                            decIfAfter(s_rebindTextOverrideVKToBind);
                            decIfAfter(s_rebindOutputScanToBind);
                            decIfAfter(s_layoutBindIndex);
                            decIfAfter(s_layoutContextPreferredIndex);
                            decIfAfter(s_layoutUnicodeEditIndex);

                            syncUnicodeEditBuffers();
                        };

                        auto findBestRebindIndexForKey = [&](DWORD fromVk) -> int {
                            int first = -1;
                            int enabledAny = -1;
                            int enabledConfigured = -1;
                            int configuredAny = -1;

                            for (int ri = 0; ri < (int)g_config.keyRebinds.rebinds.size(); ++ri) {
                                const auto& r = g_config.keyRebinds.rebinds[ri];
                                if (r.fromKey != fromVk) continue;
                                if (first == -1) first = ri;

                                const bool configured = (r.fromKey != 0 && r.toKey != 0);
                                if (configured && configuredAny == -1) configuredAny = ri;
                                if (r.enabled && enabledAny == -1) enabledAny = ri;
                                if (r.enabled && configured) {
                                    enabledConfigured = ri;
                                    break;
                                }
                            }

                            if (enabledConfigured != -1) return enabledConfigured;
                            if (configuredAny != -1) return configuredAny;
                            if (enabledAny != -1) return enabledAny;
                            return first;
                        };

                        // IMPORTANT: do not create a rebind on right-click.
                        // Create the entry only when the user actually changes something.
                        int idx = s_layoutContextPreferredIndex;
                        if (idx < 0 || idx >= (int)g_config.keyRebinds.rebinds.size() || g_config.keyRebinds.rebinds[idx].fromKey != s_layoutContextVk) {
                            idx = findBestRebindIndexForKey(s_layoutContextVk);
                            s_layoutContextPreferredIndex = idx;
                        }

                        auto createRebindForKeyIfMissing = [&](DWORD fromVk) -> int {
                            int e = findBestRebindIndexForKey(fromVk);
                            if (e >= 0) return e;
                            KeyRebind r;
                            r.fromKey = fromVk;
                            r.toKey = fromVk;
                            r.enabled = true;
                            g_config.keyRebinds.rebinds.push_back(r);
                            g_configIsDirty = true;
                            std::lock_guard<std::mutex> hotkeyLock(g_hotkeyMainKeysMutex);
                            RebuildHotkeyMainKeys_Internal();
                            syncUnicodeEditBuffers();
                            return (int)g_config.keyRebinds.rebinds.size() - 1;
                        };

                        auto typesValueFor = [&](const KeyRebind* rb, DWORD originalVk) -> std::string {
                            if (!rb) return VkToString(originalVk);
                            if (rb->useCustomOutput && rb->customOutputUnicode != 0) return codepointToDisplay((uint32_t)rb->customOutputUnicode);
                            DWORD textVk = (rb->useCustomOutput && rb->customOutputVK != 0) ? rb->customOutputVK : rb->toKey;
                            if (textVk == 0) textVk = originalVk;
                            return VkToString(textVk);
                        };

                        auto triggersValueFor = [&](const KeyRebind* rb, DWORD originalVk) -> std::string {
                            DWORD triggerVk = rb ? rb->toKey : originalVk;
                            if (triggerVk == 0) triggerVk = originalVk;
                            DWORD displayScan = (rb && rb->useCustomOutput && rb->customOutputScanCode != 0) ? rb->customOutputScanCode
                                                                                                            : getScanCodeWithExtendedFlag(triggerVk);
                            return scanCodeToDisplayName(displayScan, triggerVk);
                        };

                        // Build a stable list of "known" scan codes by enumerating VKs and converting to scan codes.
                        // This covers typical keyboard keys (including extended keys) and is suitable for a dropdown.
                        static std::vector<std::pair<DWORD, std::string>> s_knownScanCodes;
                        static bool s_knownScanCodesBuilt = false;
                        if (!s_knownScanCodesBuilt) {
                            std::vector<std::pair<DWORD, std::string>> tmp;
                            tmp.reserve(300);
                            for (DWORD vk = 1; vk <= 255; ++vk) {
                                DWORD scan = getScanCodeWithExtendedFlag(vk);
                                if (scan == 0) continue;

                                // Prefer readable VK names; this is just for display.
                                tmp.emplace_back(scan, VkToString(vk));
                            }

                            // De-duplicate by scan code (keep the first name in scan order).
                            std::sort(tmp.begin(), tmp.end(), [](const auto& a, const auto& b) {
                                if (a.first == b.first) return a.second < b.second;
                                return a.first < b.first;
                            });

                            s_knownScanCodes.clear();
                            s_knownScanCodes.reserve(tmp.size());
                            DWORD lastScan = 0xFFFFFFFFu;
                            for (const auto& it : tmp) {
                                if (it.first == lastScan) continue;
                                s_knownScanCodes.push_back(it);
                                lastScan = it.first;
                            }

                            // Sort by display name for the dropdown.
                            std::sort(s_knownScanCodes.begin(), s_knownScanCodes.end(), [](const auto& a, const auto& b) {
                                if (a.second == b.second) return a.first < b.first;
                                return a.second < b.second;
                            });

                            s_knownScanCodesBuilt = true;
                        }

                        auto formatScanHex = [](DWORD scan) -> std::string {
                            auto hex2 = [](unsigned v) -> std::string {
                                static const char* kHex = "0123456789ABCDEF";
                                std::string s;
                                s.push_back(kHex[(v >> 4) & 0xF]);
                                s.push_back(kHex[v & 0xF]);
                                return s;
                            };

                            const unsigned low = (unsigned)(scan & 0xFF);
                            if ((scan & 0xFF00) != 0) {
                                // Extended flag uses 0xE000 in our representation.
                                return std::string("E0 ") + hex2(low);
                            }
                            return hex2(low);
                        };

                        // Inline binding capture
                        if (s_layoutBindTarget != LayoutBindTarget::None && s_layoutBindIndex >= 0 &&
                            s_layoutBindIndex < (int)g_config.keyRebinds.rebinds.size()) {
                            MarkRebindBindingActive();

                            DWORD capturedVk = 0;
                            LPARAM capturedLParam = 0;
                            bool capturedIsMouse = false;
                            if (ConsumeBindingInputEventSince(s_layoutBindLastSeq, capturedVk, capturedLParam, capturedIsMouse)) {
                                if (capturedVk == VK_ESCAPE) {
                                    // Cancel inline binding. If the underlying rebind is a no-op, prune it.
                                    auto& r = g_config.keyRebinds.rebinds[s_layoutBindIndex];
                                    int maybeErase = isNoOpRebindForKey(r, r.fromKey) ? s_layoutBindIndex : -1;
                                    s_layoutBindTarget = LayoutBindTarget::None;
                                    s_layoutBindIndex = -1;
                                    if (maybeErase != -1) {
                                        eraseRebindIndex(maybeErase);
                                        if (s_layoutContextPreferredIndex == maybeErase) s_layoutContextPreferredIndex = -1;
                                    }
                                } else if (capturedVk != VK_LWIN && capturedVk != VK_RWIN) {
                                    auto& r = g_config.keyRebinds.rebinds[s_layoutBindIndex];
                                    if (s_layoutBindTarget == LayoutBindTarget::TypesVk) {
                                        r.useCustomOutput = true;
                                        r.customOutputVK = capturedVk;
                                        r.customOutputUnicode = 0;
                                        if (s_layoutBindIndex >= 0 && s_layoutBindIndex < (int)s_rebindUnicodeTextEdit.size()) {
                                            s_rebindUnicodeTextEdit[s_layoutBindIndex].clear();
                                        }

                                        // Don't keep redundant overrides.
                                        if (r.customOutputVK == r.toKey) {
                                            r.customOutputVK = 0;
                                            if (r.customOutputScanCode == 0) r.useCustomOutput = false;
                                        }
                                        g_configIsDirty = true;
                                    } else if (s_layoutBindTarget == LayoutBindTarget::TriggersVk) {
                                        r.toKey = capturedVk;
                                        g_configIsDirty = true;
                                    }

                                    // If the edit resulted in a no-op, prune the entry.
                                    if (isNoOpRebindForKey(r, r.fromKey)) {
                                        int eraseIdx = s_layoutBindIndex;
                                        s_layoutBindTarget = LayoutBindTarget::None;
                                        s_layoutBindIndex = -1;
                                        eraseRebindIndex(eraseIdx);
                                    } else {
                                        s_layoutBindTarget = LayoutBindTarget::None;
                                        s_layoutBindIndex = -1;
                                        (void)capturedLParam;
                                        (void)capturedIsMouse;
                                    }
                                }
                            }
                        }

                        // Unicode prompt
                        if (s_layoutUnicodeEditIndex != -1) {
                            MarkRebindBindingActive();
                            ImGui::OpenPopup("Custom Unicode##layout");
                        }

                        if (ImGui::BeginPopupModal("Custom Unicode##layout", NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) {
                            MarkRebindBindingActive();
                            ImGui::TextUnformatted("Enter a Unicode character or codepoint:");
                            ImGui::TextDisabled("Examples: ø   U+00F8   0x00F8");
                            ImGui::Separator();
                            ImGui::SetNextItemWidth(260.0f);
                            ImGui::InputTextWithHint("##unicode", "ø or U+00F8", &s_layoutUnicodeEditText);
                            ImGui::Spacing();

                            const bool canApply = true;
                            if (ImGui::Button("Apply", ImVec2(120, 0)) && canApply) {
                                if (s_layoutUnicodeEditIndex >= 0 && s_layoutUnicodeEditIndex < (int)g_config.keyRebinds.rebinds.size()) {
                                    auto& r = g_config.keyRebinds.rebinds[s_layoutUnicodeEditIndex];
                                    if (s_layoutUnicodeEditText.empty()) {
                                        r.customOutputUnicode = 0;
                                        if (r.customOutputVK == 0 && r.customOutputScanCode == 0) r.useCustomOutput = false;
                                        g_configIsDirty = true;
                                    } else {
                                        uint32_t cp = 0;
                                        if (tryParseUnicodeInput(s_layoutUnicodeEditText, cp)) {
                                            r.useCustomOutput = true;
                                            r.customOutputUnicode = (DWORD)cp;
                                            g_configIsDirty = true;
                                        }
                                    }

                                    if (isNoOpRebindForKey(r, r.fromKey)) {
                                        int eraseIdx = s_layoutUnicodeEditIndex;
                                        s_layoutUnicodeEditIndex = -1;
                                        s_layoutUnicodeEditText.clear();
                                        ImGui::CloseCurrentPopup();
                                        eraseRebindIndex(eraseIdx);
                                    }
                                }

                                s_layoutUnicodeEditIndex = -1;
                                s_layoutUnicodeEditText.clear();
                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::SameLine();
                            if (ImGui::Button("Cancel", ImVec2(120, 0))) {
                                // Cancel prompt. If the underlying rebind is a no-op, prune it.
                                if (s_layoutUnicodeEditIndex >= 0 && s_layoutUnicodeEditIndex < (int)g_config.keyRebinds.rebinds.size()) {
                                    auto& r = g_config.keyRebinds.rebinds[s_layoutUnicodeEditIndex];
                                    int maybeErase = isNoOpRebindForKey(r, r.fromKey) ? s_layoutUnicodeEditIndex : -1;
                                    if (maybeErase != -1) {
                                        eraseRebindIndex(maybeErase);
                                        if (s_layoutContextPreferredIndex == maybeErase) s_layoutContextPreferredIndex = -1;
                                    }
                                }
                                s_layoutUnicodeEditIndex = -1;
                                s_layoutUnicodeEditText.clear();
                                ImGui::CloseCurrentPopup();
                            }
                            ImGui::EndPopup();
                        }

                        // Render (even if there is no rebind yet)
                        KeyRebind* rbPtr = (idx >= 0 && idx < (int)g_config.keyRebinds.rebinds.size()) ? &g_config.keyRebinds.rebinds[idx] : nullptr;
                        const std::string typesValue = typesValueFor(rbPtr, s_layoutContextVk);
                        const std::string triggersValue = triggersValueFor(rbPtr, s_layoutContextVk);

                        // Wider so key names like "RSHIFT" / "NUMLOCK" fit without truncation.
                        const float vBtnW = 138.0f;

                        ImGui::TextUnformatted("Types:");
                        ImGui::SameLine();
                        {
                            std::string label = (s_layoutBindTarget == LayoutBindTarget::TypesVk) ? std::string("[Press key...]") : typesValue;
                            if (ImGui::Button((label + "##types").c_str(), ImVec2(vBtnW, 0))) {
                                idx = createRebindForKeyIfMissing(s_layoutContextVk);
                                s_layoutContextPreferredIndex = idx;
                                if (idx >= 0) {
                                    s_layoutBindTarget = LayoutBindTarget::TypesVk;
                                    s_layoutBindIndex = idx;
                                    s_layoutBindLastSeq = GetLatestBindingInputSequence();
                                    MarkRebindBindingActive();
                                }
                            }
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Custom##types_custom", ImVec2(112, 0))) {
                            idx = createRebindForKeyIfMissing(s_layoutContextVk);
                            s_layoutContextPreferredIndex = idx;
                            if (idx >= 0) {
                                s_layoutUnicodeEditIndex = idx;
                                // Seed edit text from current value
                                const auto& r = g_config.keyRebinds.rebinds[idx];
                                s_layoutUnicodeEditText = (r.customOutputUnicode != 0) ? formatCodepointUPlus((uint32_t)r.customOutputUnicode) : std::string();
                                MarkRebindBindingActive();
                            }
                        }

                        ImGui::Spacing();

                        ImGui::TextUnformatted("Triggers:");
                        ImGui::SameLine();
                        {
                            std::string label = (s_layoutBindTarget == LayoutBindTarget::TriggersVk) ? std::string("[Press key...]") : triggersValue;
                            if (ImGui::Button((label + "##triggers").c_str(), ImVec2(vBtnW, 0))) {
                                idx = createRebindForKeyIfMissing(s_layoutContextVk);
                                s_layoutContextPreferredIndex = idx;
                                if (idx >= 0) {
                                    s_layoutBindTarget = LayoutBindTarget::TriggersVk;
                                    s_layoutBindIndex = idx;
                                    s_layoutBindLastSeq = GetLatestBindingInputSequence();
                                    MarkRebindBindingActive();
                                }
                            }
                        }

                        // Dropdown for scan-code selection (alternative to "press key").
                        ImGui::SameLine();
                        {
                            idx = (idx >= 0) ? idx : findBestRebindIndexForKey(s_layoutContextVk);
                            KeyRebind* r = (idx >= 0 && idx < (int)g_config.keyRebinds.rebinds.size()) ? &g_config.keyRebinds.rebinds[idx] : nullptr;

                            // Preview: current scan override (or derived from output VK if none).
                            DWORD curTriggerVk = r ? r->toKey : s_layoutContextVk;
                            if (curTriggerVk == 0) curTriggerVk = s_layoutContextVk;
                            DWORD curScan = (r && r->useCustomOutput && r->customOutputScanCode != 0) ? r->customOutputScanCode
                                                                                                      : getScanCodeWithExtendedFlag(curTriggerVk);
                            std::string preview = scanCodeToDisplayName(curScan, curTriggerVk);

                            ImGui::SetNextItemWidth(240.0f);
                            if (ImGui::BeginCombo("##triggers_scan_combo", preview.c_str())) {
                                // Default option: clear scan override.
                                bool isDefault = !(r && r->useCustomOutput && r->customOutputScanCode != 0);
                                if (ImGui::Selectable("Default (Same as Types)", isDefault)) {
                                    idx = createRebindForKeyIfMissing(s_layoutContextVk);
                                    s_layoutContextPreferredIndex = idx;
                                    if (idx >= 0) {
                                        auto& rr = g_config.keyRebinds.rebinds[idx];
                                        rr.customOutputScanCode = 0;
                                        if (rr.customOutputVK == 0 && rr.customOutputUnicode == 0) rr.useCustomOutput = false;
                                        g_configIsDirty = true;
                                    }
                                }
                                ImGui::Separator();

                                for (const auto& it : s_knownScanCodes) {
                                    const DWORD scan = it.first;
                                    // Map back to a VK for wParam consistency where possible.
                                    DWORD vkFromScan = MapVirtualKey(scan, MAPVK_VSC_TO_VK_EX);
                                    if (vkFromScan == 0) vkFromScan = curTriggerVk;
                                    const std::string name = scanCodeToDisplayName(scan, vkFromScan);
                                    const std::string itemLabel = name + "  (" + formatScanHex(scan) + ")##scan_" + std::to_string((unsigned)scan);

                                    const bool selected = (r && r->useCustomOutput && r->customOutputScanCode == scan);
                                    if (ImGui::Selectable(itemLabel.c_str(), selected)) {
                                        idx = createRebindForKeyIfMissing(s_layoutContextVk);
                                        s_layoutContextPreferredIndex = idx;
                                        if (idx >= 0) {
                                            auto& rr = g_config.keyRebinds.rebinds[idx];
                                            rr.useCustomOutput = true;
                                            rr.customOutputScanCode = scan;

                                            // Keep VK aligned with scan if we can resolve it.
                                            DWORD mapped = MapVirtualKey(scan, MAPVK_VSC_TO_VK_EX);
                                            if (mapped != 0) rr.toKey = mapped;

                                            g_configIsDirty = true;
                                        }
                                    }
                                }

                                ImGui::EndCombo();
                            }
                            if (ImGui::IsItemHovered()) {
                                ImGui::SetTooltip("Select a scan code for the game keybind output (lParam).\nThis is an alternative to clicking 'Triggers' then pressing a key.");
                            }
                        }

                        ImGui::Spacing();

                        if (ImGui::Button("Reset##layout_reset", ImVec2(170, 0))) {
                            if (idx >= 0 && idx < (int)g_config.keyRebinds.rebinds.size()) {
                                auto& r = g_config.keyRebinds.rebinds[idx];
                                r.toKey = r.fromKey;
                                r.customOutputVK = 0;
                                r.customOutputUnicode = 0;
                                r.customOutputScanCode = 0;
                                r.useCustomOutput = false;
                                r.enabled = true;
                                g_configIsDirty = true;

                                if (idx >= 0 && idx < (int)s_rebindUnicodeTextEdit.size()) s_rebindUnicodeTextEdit[idx].clear();

                                if (isNoOpRebindForKey(r, r.fromKey)) {
                                    eraseRebindIndex(idx);
                                    s_layoutContextPreferredIndex = -1;
                                } else {
                                    std::lock_guard<std::mutex> hotkeyLock(g_hotkeyMainKeysMutex);
                                    RebuildHotkeyMainKeys_Internal();
                                }
                            }
                        }

                        ImGui::EndPopup();
                    }

                    // Ensure the child cursor ends below the larger of keyboard/mouse regions.
                    const float totalH = (keyboardTotalH > mouseDiagramTotalH) ? keyboardTotalH : mouseDiagramTotalH;
                    ImGui::SetCursorPos(ImVec2(layoutStart.x, layoutStart.y + totalH + gap));

                    // -----------------------------------------------------------------
                    // Rebind list (read-only) below the keyboard
                    // -----------------------------------------------------------------
                    {
                        ImGui::Spacing();
                        ImGui::SeparatorText("Rebinds");
                        bool anyShown = false;
                        auto isNoOp = [&](const KeyRebind& r) -> bool {
                            if (r.fromKey == 0 || r.toKey == 0) return true;
                            if (r.toKey != r.fromKey) return false;
                            if (r.customOutputVK != 0 && r.customOutputVK != r.toKey) return false;
                            if (r.customOutputUnicode != 0) return false;
                            if (r.customOutputScanCode != 0) return false;
                            return true;
                        };
                        for (const auto& r : g_config.keyRebinds.rebinds) {
                            if (r.fromKey == 0 || r.toKey == 0) continue;
                            if (isNoOp(r)) continue;

                            std::string fromStr = VkToString(r.fromKey);
                            std::string typesStr;
                            if (r.useCustomOutput && r.customOutputUnicode != 0) {
                                typesStr = codepointToDisplay((uint32_t)r.customOutputUnicode);
                            } else {
                                DWORD textVk = (r.useCustomOutput && r.customOutputVK != 0) ? r.customOutputVK : r.toKey;
                                if (textVk == 0) textVk = r.fromKey;
                                typesStr = VkToString(textVk);
                            }

                            DWORD triggerVk = (r.toKey != 0) ? r.toKey : r.fromKey;
                            DWORD displayScan = (r.useCustomOutput && r.customOutputScanCode != 0) ? r.customOutputScanCode
                                                                                                   : getScanCodeWithExtendedFlag(triggerVk);
                            std::string triggersStr = scanCodeToDisplayName(displayScan, triggerVk);
                            ImGui::Text("%s -> %s & %s", fromStr.c_str(), typesStr.c_str(), triggersStr.c_str());
                            anyShown = true;
                        }
                        if (!anyShown) {
                            ImGui::TextDisabled("(No active rebinds)");
                        }
                    }

                    ImGui::EndChild();
                    ImGui::PopStyleColor(); // ImGuiCol_ChildBg

                    ImGui::EndPopup();
                }

                ImGui::PopStyleColor(); // ImGuiCol_PopupBg

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

                // Rebinds are configured via the keyboard layout visualizer.
                ImGui::Spacing();
                ImGui::TextDisabled("Configure key rebinds in the Keyboard Layout window (right-click keys). ");
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::EndTabItem();
}
