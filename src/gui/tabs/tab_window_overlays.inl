#if defined(TOOLSCREEN_NINJABRAIN_OVERLAY_ONLY)
if (BeginSelectableSettingsTopTabItem(trc("ninjabrain.title"))) {
#else
if (BeginSelectableSettingsTopTabItem(trc("tabs.window_overlays"))) {
#endif
    g_currentlyEditingMirror = "";
    g_imageDragMode.store(false);

#if defined(TOOLSCREEN_NINJABRAIN_OVERLAY_ONLY)
    g_windowOverlayDragMode.store(false);
#else
    g_windowOverlayDragMode.store(true);
#endif

    SliderCtrlClickTip();

#if !defined(TOOLSCREEN_NINJABRAIN_OVERLAY_ONLY)
    int windowOverlay_to_remove = -1;
    for (size_t i = 0; i < g_config.windowOverlays.size(); ++i) {
        auto& overlay = g_config.windowOverlays[i];
        ImGui::PushID((int)i);

        std::string delete_overlay_label = "X##delete_overlay_" + std::to_string(i);
        if (ImGui::Button(delete_overlay_label.c_str(), ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()))) {
            std::string overlay_popup_id = (tr("window.overlays_delete") + "##" + std::to_string(i));
            ImGui::OpenPopup(overlay_popup_id.c_str());
        }

        std::string overlay_popup_id = (tr("window.overlays_delete") + "##" + std::to_string(i));
        if (ImGui::BeginPopupModal(overlay_popup_id.c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text(trc("window.tooltip_delete", overlay.name));
            ImGui::Separator();
            if (ImGui::Button(trc("button.ok"), ImVec2(120, 0))) {
                windowOverlay_to_remove = (int)i;
                g_configIsDirty = true;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();
            if (ImGui::Button(trc("button.cancel"), ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }

        ImGui::SameLine();

        std::string oldOverlayName = overlay.name;

        bool node_open = ImGui::TreeNodeEx("##overlay_node", ImGuiTreeNodeFlags_SpanAvailWidth, "%s", overlay.name.c_str());

        if (node_open) {

            bool hasDuplicate = HasDuplicateWindowOverlayName(overlay.name, i);
            if (hasDuplicate) {
                ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.6f, 0.2f, 0.2f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.7f, 0.3f, 0.3f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
            }

            if (ImGui::InputText(trc("window.overlays_name"), &overlay.name)) {
                if (!HasDuplicateWindowOverlayName(overlay.name, i)) {
                    g_configIsDirty = true;
                    if (oldOverlayName != overlay.name) {
                        for (auto& mode : g_config.modes) {
                            for (auto& overlayId : mode.windowOverlayIds) {
                                if (overlayId == oldOverlayName) { overlayId = overlay.name; }
                            }
                        }
                    }
                } else {
                    overlay.name = oldOverlayName;
                }
            }

            if (hasDuplicate) { ImGui::PopStyleColor(3); }

            if (hasDuplicate) {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), trc("images.name_duplicate"));
            }

            // Dropdown for selecting from currently open windows
            ImGui::Separator();
            ImGui::Text(trc("window.overlays_select_window"));

            // Get cached window list from background thread (non-blocking)
            auto s_cachedWindows = GetCachedWindowList();

            ImGui::PushID(("window_dropdown_" + std::to_string(i)).c_str());

            std::string dropdownPreview = trc("window.overlays_choose_window");
            if (!overlay.windowTitle.empty()) {
                WindowInfo currentInfo;
                currentInfo.title = overlay.windowTitle;
                currentInfo.className = overlay.windowClass;
                currentInfo.executableName = overlay.executableName;
                dropdownPreview = currentInfo.GetDisplayName();
                if (dropdownPreview.length() > 60) { dropdownPreview = dropdownPreview.substr(0, 57) + "..."; }
            }

            if (ImGui::BeginCombo("##WindowSelector", dropdownPreview.c_str())) {
                if (s_cachedWindows.empty()) {
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), trc("window.overlays_not_found"));
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), trc("window.overlays_thread_exit"));
                } else {
                    for (const auto& windowInfo : s_cachedWindows) {
                        bool isSelected = (overlay.windowTitle == windowInfo.title && overlay.windowClass == windowInfo.className &&
                                           overlay.executableName == windowInfo.executableName);

                        std::string displayText = windowInfo.GetDisplayName();
                        if (!IsWindowInfoValid(windowInfo)) {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
                        } else {
                            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
                        }

                        if (ImGui::Selectable(displayText.c_str(), isSelected)) {
                            if (IsWindowInfoValid(windowInfo)) {
                                overlay.windowTitle = windowInfo.title;
                                overlay.windowClass = windowInfo.className;
                                overlay.executableName = windowInfo.executableName;
                                g_configIsDirty = true;
                                // Queue deferred reload to avoid blocking GUI thread
                                QueueOverlayReload(overlay.name, overlay);
                            }
                        }
                        ImGui::PopStyleColor();
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::PopID();

            ImGui::Text(trc("window.overlays_match_priority"));
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "(?)");
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(trc("window.tooltip_match_priority"));
            }

            const char* priorityOptions[] = { trc("window.overlays_match_priority_title"), trc("window.overlays_match_priority_title_executable") };
            const char* priorityValues[] = { "title", "title_executable" };

            int currentPriorityIdx = 0;
            for (int idx = 0; idx < 2; idx++) {
                if (overlay.windowMatchPriority == priorityValues[idx]) {
                    currentPriorityIdx = idx;
                    break;
                }
            }

            ImGui::PushItemWidth(300.0f);
            if (ImGui::Combo("##MatchPriority", &currentPriorityIdx, priorityOptions, 2)) {
                overlay.windowMatchPriority = priorityValues[currentPriorityIdx];
                g_configIsDirty = true;
                // Queue deferred reload to avoid blocking GUI thread
                QueueOverlayReload(overlay.name, overlay);
            }
            ImGui::PopItemWidth();
            ImGui::SeparatorText(trc("window.overlays_rendering"));
            if (ImGui::SliderFloat(trc("label.opacity"), &overlay.opacity, 0.0f, 1.0f)) g_configIsDirty = true;
            if (ImGui::Checkbox(trc("window.overlays_pixelated_scaling"), &overlay.pixelatedScaling)) g_configIsDirty = true;
            if (ImGui::Checkbox(trc("window.overlays_only_on_my_screen"), &overlay.onlyOnMyScreen)) g_configIsDirty = true;
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(trc("window.tooltip.only_on_my_screen"));
            }

            ImGui::Columns(2, "overlay_render", false);
            ImGui::SetColumnWidth(0, 120);
            ImGui::Text(trc("label.x"));
            ImGui::NextColumn();
            if (Spinner("##overlay_x", &overlay.x)) g_configIsDirty = true;
            ImGui::NextColumn();
            ImGui::Text(trc("label.y"));
            ImGui::NextColumn();
            if (Spinner("##overlay_y", &overlay.y)) g_configIsDirty = true;
            ImGui::NextColumn();
            ImGui::Text(trc("label.scale"));
            ImGui::NextColumn();
            float scalePercent = overlay.scale * 100.0f;
            ImGui::SetNextItemWidth(250);
            if (ImGui::SliderFloat("##overlay_scale", &scalePercent, 10.0f, 200.0f, "%.0f%%")) {
                overlay.scale = scalePercent / 100.0f;
                g_configIsDirty = true;
            }
            ImGui::NextColumn();
            ImGui::Text(trc("label.relative_to"));
            ImGui::NextColumn();
            const char* current_rel_to = getFriendlyName(overlay.relativeTo, imageRelativeToOptions);
            ImGui::SetNextItemWidth(150);
            if (ImGui::BeginCombo("##overlay_rel_to", current_rel_to)) {
                for (const auto& option : imageRelativeToOptions) {
                    if (ImGui::Selectable(option.second, overlay.relativeTo == option.first)) {
                        overlay.relativeTo = option.first;
                        g_configIsDirty = true;
                    }
                }
                ImGui::EndCombo();
            }
            ImGui::Columns(1);

            ImGui::SeparatorText(trc("window.overlays_cropping"));
            ImGui::Columns(2, "overlay_crop", false);
            ImGui::SetColumnWidth(0, 120);
            ImGui::Text(trc("window.overlays_crop_top"));
            ImGui::NextColumn();
            if (Spinner("##overlay_crop_t", &overlay.crop_top, 1, 0)) g_configIsDirty = true;
            ImGui::NextColumn();
            ImGui::Text(overlay.cropToHeight ? trc("label.crop_to_height") : trc("window.overlays_crop_bottom"));
            ImGui::NextColumn();
            if (Spinner("##overlay_crop_b", &overlay.crop_bottom, 1, 0)) g_configIsDirty = true;
            ImGui::SameLine();
            if (ImGui::Checkbox(trc("label.crop_to_target_h"), &overlay.cropToHeight)) g_configIsDirty = true;
            ImGui::SameLine(); HelpMarker(trc("label.tooltip.crop_to"));
            ImGui::NextColumn();
            ImGui::Text(trc("window.overlays_crop_left"));
            ImGui::NextColumn();
            if (Spinner("##overlay_crop_l", &overlay.crop_left, 1, 0)) g_configIsDirty = true;
            ImGui::NextColumn();
            ImGui::Text(overlay.cropToWidth ? trc("label.crop_to_width") : trc("window.overlays_crop_right"));
            ImGui::NextColumn();
            if (Spinner("##overlay_crop_r", &overlay.crop_right, 1, 0)) g_configIsDirty = true;
            ImGui::SameLine();
            if (ImGui::Checkbox(trc("label.crop_to_target_w"), &overlay.cropToWidth)) g_configIsDirty = true;
            ImGui::SameLine(); HelpMarker(trc("label.tooltip.crop_to"));
            ImGui::NextColumn();
            ImGui::Columns(1);

            ImGui::SeparatorText(trc("window.overlays_capture_settings"));
            ImGui::Columns(2, "overlay_capture", false);
            ImGui::SetColumnWidth(0, 150);
            ImGui::Text(trc("label.fps"));
            ImGui::NextColumn();
            ImGui::SetNextItemWidth(250);
            if (ImGui::SliderInt("##fps", &overlay.fps, 1, 60, "%d fps")) {
                g_configIsDirty = true;
                UpdateWindowOverlayFPS(overlay.name, overlay.fps);
            }
            ImGui::NextColumn();

            ImGui::Text(trc("window.overlays_search_interval"));
            ImGui::NextColumn();
            float searchIntervalSeconds = overlay.searchInterval / 1000.0f;
            ImGui::SetNextItemWidth(250);
            if (ImGui::SliderFloat("##searchInterval", &searchIntervalSeconds, 0.5f, 5.0f, "%.1f s")) {
                overlay.searchInterval = static_cast<int>(searchIntervalSeconds * 1000.0f);
                g_configIsDirty = true;
                UpdateWindowOverlaySearchInterval(overlay.name, overlay.searchInterval);
            }
            ImGui::NextColumn();
            ImGui::Columns(1);

            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(trc("tooltip.capture_search_interval"));
            }

            ImGui::Text(trc("window.overlays_capture_method"));
            const char* captureMethods[] = { "Windows 10+", "BitBlt" };
            int currentMethodIdx = 0;
            for (int i = 0; i < 2; i++) {
                if (overlay.captureMethod == captureMethods[i]) {
                    currentMethodIdx = i;
                    break;
                }
            }
            ImGui::PushItemWidth(150.0f);
            if (ImGui::Combo("##captureMethod", &currentMethodIdx, captureMethods, 2)) {
                overlay.captureMethod = captureMethods[currentMethodIdx];
                g_configIsDirty = true;
                // Queue deferred reload to avoid blocking GUI thread
                QueueOverlayReload(overlay.name, overlay);
            }
            ImGui::PopItemWidth();
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(trc("tooltip.capture_method"));
            }

            if (ImGui::Checkbox(trc("window.overlays_force_update"), &overlay.forceUpdate)) g_configIsDirty = true;
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(trc("window.overlays_tooltip_force_update"));
            }

            ImGui::SeparatorText(trc("window.overlays_interaction"));
            if (ImGui::Checkbox(trc("window.overlays_enable_interaction"), &overlay.enableInteraction)) g_configIsDirty = true;
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip(trc("tooltip.capture_interaction"));
            }

            ImGui::SeparatorText(trc("window.overlays_background"));
            if (ImGui::Checkbox(trc("window.overlays_enable_background"), &overlay.background.enabled)) g_configIsDirty = true;
            ImGui::BeginDisabled(!overlay.background.enabled);
            if (ImGui::ColorEdit3(trc("window.overlays_bg_color"), &overlay.background.color.r)) g_configIsDirty = true;
            if (ImGui::SliderFloat(trc("window.overlays_bg_opacity"), &overlay.background.opacity, 0.0f, 1.0f)) g_configIsDirty = true;
            ImGui::EndDisabled();

            ImGui::SeparatorText(trc("window.overlays_color_keying"));
            if (ImGui::Checkbox(trc("window.overlays_enable_color_keying"), &overlay.enableColorKey)) g_configIsDirty = true;
            ImGui::BeginDisabled(!overlay.enableColorKey);

            int colorKeyToRemove = -1;
            for (size_t k = 0; k < overlay.colorKeys.size(); k++) {
                ImGui::PushID(static_cast<int>(k));
                auto& ck = overlay.colorKeys[k];

                ImGui::Text("Key %zu:", k + 1);
                ImGui::SameLine();
                ImGui::PushItemWidth(150.0f);
                if (ImGui::ColorEdit3("##color", &ck.color.r, ImGuiColorEditFlags_NoLabel)) g_configIsDirty = true;
                ImGui::PopItemWidth();
                ImGui::SameLine();
                ImGui::PushItemWidth(80.0f);
                if (ImGui::SliderFloat("##sens", &ck.sensitivity, 0.001f, 1.0f, "%.3f")) g_configIsDirty = true;
                ImGui::PopItemWidth();
                ImGui::SameLine();
                if (ImGui::Button("X##remove")) { colorKeyToRemove = static_cast<int>(k); }
                ImGui::PopID();
            }

            if (colorKeyToRemove >= 0) {
                overlay.colorKeys.erase(overlay.colorKeys.begin() + colorKeyToRemove);
                g_configIsDirty = true;
            }

            ImGui::BeginDisabled(overlay.colorKeys.size() >= ConfigDefaults::MAX_COLOR_KEYS);
            if (ImGui::Button(trc("window.overlays_add_color_key"))) {
                ColorKeyConfig newKey;
                newKey.color = { 0.0f, 0.0f, 0.0f };
                newKey.sensitivity = 0.05f;
                overlay.colorKeys.push_back(newKey);
                g_configIsDirty = true;
            }
            ImGui::EndDisabled();

            ImGui::EndDisabled();

            ImGui::SeparatorText(trc("window.overlays_border"));
            if (ImGui::Checkbox((tr("window.overlays_enable_border") + "##WindowOverlay").c_str(), &overlay.border.enabled)) { g_configIsDirty = true; }
            ImGui::SameLine();
            HelpMarker(trc("window.overlays_tooltip_border"));

            if (overlay.border.enabled) {
                ImGui::Text(trc("images.border_color"));
                ImVec4 borderCol = ImVec4(overlay.border.color.r, overlay.border.color.g, overlay.border.color.b, 1.0f);
                if (ImGui::ColorEdit3("##BorderColorWindowOverlay", (float*)&borderCol, ImGuiColorEditFlags_NoInputs)) {
                    overlay.border.color = { borderCol.x, borderCol.y, borderCol.z };
                    g_configIsDirty = true;
                }

                ImGui::Text(trc("images.border_width"));
                ImGui::SetNextItemWidth(100);
                if (Spinner("##BorderWidthWindowOverlay", &overlay.border.width, 1, 1, 50)) { g_configIsDirty = true; }
                ImGui::SameLine();
                ImGui::TextDisabled(trc("label.px"));

                ImGui::Text(trc("images.border_radius"));
                ImGui::SetNextItemWidth(100);
                if (Spinner("##BorderRadiusWindowOverlay", &overlay.border.radius, 1, 0, 100)) { g_configIsDirty = true; }
                ImGui::SameLine();
                ImGui::TextDisabled(trc("label.px"));
            }

            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    if (windowOverlay_to_remove >= 0) {
        std::string deletedOverlayName = g_config.windowOverlays[windowOverlay_to_remove].name;
        RemoveWindowOverlayFromCache(deletedOverlayName);
        g_config.windowOverlays.erase(g_config.windowOverlays.begin() + windowOverlay_to_remove);
        for (auto& mode : g_config.modes) {
            auto it = std::find(mode.windowOverlayIds.begin(), mode.windowOverlayIds.end(), deletedOverlayName);
            while (it != mode.windowOverlayIds.end()) {
                mode.windowOverlayIds.erase(it);
                it = std::find(mode.windowOverlayIds.begin(), mode.windowOverlayIds.end(), deletedOverlayName);
            }
        }
        g_configIsDirty = true;
    }
    ImGui::Separator();
    if (ImGui::Button(trc("button.add_overlay"))) {
        WindowOverlayConfig newOverlay;
        newOverlay.name = tr("window.overlays_new_window_overlay") + " " + std::to_string(g_config.windowOverlays.size() + 1);
        newOverlay.relativeTo = "centerViewport";
        g_config.windowOverlays.push_back(newOverlay);
        g_configIsDirty = true;

        if (!g_currentModeId.empty()) {
            for (auto& mode : g_config.modes) {
                if (mode.id == g_currentModeId) {
                    if (std::find(mode.windowOverlayIds.begin(), mode.windowOverlayIds.end(), newOverlay.name) ==
                        mode.windowOverlayIds.end()) {
                        mode.windowOverlayIds.push_back(newOverlay.name);
                    }
                    break;
                }
            }
        }
    }

    ImGui::SameLine();
    if (ImGui::Button((tr("button.reset_defaults") + "##windowoverlays").c_str())) { ImGui::OpenPopup(trc("window.overlays_reset_to_defaults")); }

    if (ImGui::BeginPopupModal(trc("window.overlays_reset_to_defaults"), NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.0f, 1.0f), trc("label.warning"));
        ImGui::Text(trc("window.overlays_warning_reset_to_defaults"));
        ImGui::Text(trc("label.action_cannot_be_undone"));
        ImGui::Separator();
        if (ImGui::Button(trc("button.confirm_reset"), ImVec2(120, 0))) {
            for (const auto& overlay : g_config.windowOverlays) { RemoveWindowOverlayFromCache(overlay.name); }
            g_config.windowOverlays = GetDefaultWindowOverlays();
            g_configIsDirty = true;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SetItemDefaultFocus();
        ImGui::SameLine();
        if (ImGui::Button(trc("button.cancel"), ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }
#endif

#if !defined(TOOLSCREEN_WINDOW_OVERLAYS_ONLY)
    // NinjabrainBot Overlay
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Text("%s", trc("ninjabrain.title"));
    ImGui::Spacing();
    {
        auto& nb = g_config.ninjabrainOverlay;
        bool changed = false;

        if (nb.layoutStyle != "compact") {
            nb.layoutStyle = "compact";
            changed = true;
        }

        auto setNinjabrainColumns = [](NinjabrainOverlayConfig& overlay, std::initializer_list<NinjabrainColumn> columns) {
            overlay.columns.assign(columns.begin(), columns.end());
        };

        auto applyNinjabrainPreset = [&](const char* presetId) {
            const bool enabled = nb.enabled;
            const int x = nb.x;
            const int y = nb.y;
            const std::string relativeTo = nb.relativeTo;
            const std::string customFontPath = nb.customFontPath;
            const std::string apiBaseUrl = nb.apiBaseUrl;
            const std::vector<std::string> allowedModes = nb.allowedModes;
            const bool onlyOnMyScreen = nb.onlyOnMyScreen;
            const bool onlyOnObs = nb.onlyOnObs;

            NinjabrainOverlayConfig preset;
            preset.layoutStyle = "compact";
            const std::string presetName = presetId;
            bool preserveCurrentPlacement = true;
            if (presetName == "compact") {
                preset.showTitleBar = false;
                preset.fontSize = 64.0f;
                preset.bgEnabled = true;
                preset.bgOpacity = 0.6f;
                preset.bgColor = { 0.0f, 0.0f, 0.0f, 1.0f };
                preset.showThrowDetails = false;
                preset.showSeparators = false;
                preset.showRowStripes = false;
                preset.borderWidth = 0;
                preset.cornerRadius = 3.0f;
                preset.headerFillColor = preset.bgColor;
                preset.coordsDisplay = "block";
                preset.borderColor = { 0.31f, 0.34f, 0.38f, 1.0f };
                preset.dividerColor = { 0.24f, 0.27f, 0.31f, 1.0f };
                preset.headerDividerColor = preset.borderColor;
                preset.textColor = { 0.549f, 0.549f, 0.549f, 1.0f };
                preset.dataColor = { 1.0f, 1.0f, 1.0f, 1.0f };
                preset.titleTextColor = preset.dataColor;
                preset.throwsTextColor = preset.dataColor;
                preset.divineTextColor = preset.dataColor;
                preset.versionTextColor = preset.textColor;
                preset.throwsBackgroundColor = preset.bgColor;
                preset.negCoordColorEnabled = false;
                preset.certaintyMidColor = { 1.0f, 0.74f, 0.17f, 1.0f };
                preset.certaintyLowColor = { 0.97f, 0.20f, 0.20f, 1.0f };
                preset.subpixelPositiveColor = { 0.459f, 0.800f, 0.424f, 1.0f };
                preset.subpixelNegativeColor = { 0.800f, 0.431f, 0.447f, 1.0f };
                preset.outlineWidth = 1;
                preset.overlayScale = 0.30f;
                preset.shownPredictions = 1;
                preset.rowSpacing = 10.0f;
                preset.colSpacing = 30.0f;
                preset.sidePadding = 0.0f;
                setNinjabrainColumns(preset,
                                     { { "coords", "Location", true },
                                       { "certainty", "%", true },
                                       { "distance", "Dist.", true },
                                       { "nether", "Nether", true },
                                       { "angle", "Angle", true },
                                       { "boat", "Boat", false } });
            } else if (presetName == "classic_151") {
                preserveCurrentPlacement = false;
            }

            if (preserveCurrentPlacement) {
                preset.enabled = enabled;
                preset.x = x;
                preset.y = y;
                preset.relativeTo = relativeTo;
                preset.customFontPath = customFontPath;
                preset.apiBaseUrl = apiBaseUrl;
                preset.allowedModes = allowedModes;
                preset.onlyOnMyScreen = onlyOnMyScreen;
                preset.onlyOnObs = onlyOnObs;
            }
            nb = std::move(preset);
            g_eyeZoomFontNeedsReload.store(true);
        };

        static char nbFontBuf[512] = {};
        static bool nbFontBufInit = false;
        if (!nbFontBufInit || std::string(nbFontBuf) != nb.customFontPath) {
            strncpy_s(nbFontBuf, nb.customFontPath.c_str(), sizeof(nbFontBuf) - 1);
            nbFontBufInit = true;
        }

        {
            const bool wasEnabled = nb.enabled;
            changed |= ImGui::Checkbox((std::string(trc("ninjabrain.enable")) + "##nb").c_str(), &nb.enabled);
            if (nb.enabled != wasEnabled) {
                if (nb.enabled) {
                    StartNinjabrainClient();
                } else {
                    StopNinjabrainClientAsync();
                }
            }
        }

        if (nb.enabled) {
        ImGui::Spacing();

        const NinjabrainApiStatus apiStatus = GetNinjabrainClientStatus();
        ImVec4 statusColor = ImVec4(0.65f, 0.65f, 0.65f, 1.0f);
        const char* statusKey = "ninjabrain.status_stopped";
        switch (apiStatus.connectionState) {
        case NinjabrainApiConnectionState::Connected:
            statusColor = ImVec4(0.35f, 0.85f, 0.35f, 1.0f);
            statusKey = "ninjabrain.status_connected";
            break;
        case NinjabrainApiConnectionState::Connecting:
            statusColor = ImVec4(1.0f, 0.8f, 0.35f, 1.0f);
            statusKey = "ninjabrain.status_connecting";
            break;
        case NinjabrainApiConnectionState::Offline:
            statusColor = ImVec4(1.0f, 0.45f, 0.45f, 1.0f);
            statusKey = "ninjabrain.status_offline";
            break;
        case NinjabrainApiConnectionState::Stopped:
            break;
        }

        ImGui::SeparatorText(trc("ninjabrain.api"));
        ImGui::TextDisabled("%s", trc("label.status"));
        ImGui::SameLine();
        ImGui::TextColored(statusColor, "%s", trc(statusKey));
        if (!apiStatus.apiBaseUrl.empty()) {
            ImGui::TextDisabled("%s", apiStatus.apiBaseUrl.c_str());
        }
        if (apiStatus.connectionState == NinjabrainApiConnectionState::Connecting && !apiStatus.apiBaseUrl.empty()) {
            ImGui::TextWrapped("%s", trc("ninjabrain.status_connecting_detail", apiStatus.apiBaseUrl));
        } else if (apiStatus.connectionState == NinjabrainApiConnectionState::Offline &&
                   !apiStatus.apiBaseUrl.empty() && !apiStatus.error.empty()) {
            ImGui::TextWrapped("%s", trc("ninjabrain.status_offline_detail", apiStatus.apiBaseUrl, apiStatus.error));
        }
        if (apiStatus.connectionState != NinjabrainApiConnectionState::Connected) {
            if (ImGui::Button(trc("ninjabrain.retry_button"))) {
                RestartNinjabrainClient();
            }
            ImGui::SameLine();
            ImGui::TextWrapped("%s", trc("ninjabrain.enable_api_hint"));
        }
        ImGui::Spacing();

        {
            std::string preview = nb.allowedModes.empty() ? trc("ninjabrain.all_modes") : "";
            if (!nb.allowedModes.empty()) {
                for (size_t mi = 0; mi < nb.allowedModes.size(); ++mi) {
                    if (mi > 0) preview += ", ";
                    preview += nb.allowedModes[mi];
                }
                if (preview.size() > 40) preview = preview.substr(0, 37) + "...";
            }
            bool modesNodeOpen = ImGui::TreeNodeEx("##nbModesNode", ImGuiTreeNodeFlags_SpanAvailWidth, "Show in modes: %s", preview.c_str());
            if (modesNodeOpen) {
                bool allModes = nb.allowedModes.empty();
                if (ImGui::Checkbox((std::string(trc("ninjabrain.all_modes")) + "##nbAllModes").c_str(), &allModes)) {
                    nb.allowedModes.clear(); changed = true;
                }
                ImGui::Separator();
                for (auto& mode : g_config.modes) {
                    bool inList = false;
                    for (auto& m : nb.allowedModes) if (m == mode.id) { inList = true; break; }
                    std::string cbLabel = mode.id + "##nbMode_" + mode.id;
                    if (ImGui::Checkbox(cbLabel.c_str(), &inList)) {
                        if (inList) nb.allowedModes.push_back(mode.id);
                        else        nb.allowedModes.erase(std::remove(nb.allowedModes.begin(), nb.allowedModes.end(), mode.id), nb.allowedModes.end());
                        changed = true;
                    }
                }
                ImGui::TreePop();
            }
        }
        ImGui::Spacing();

        ImGui::SeparatorText(trc("ninjabrain.presets"));
        ImGui::TextDisabled("%s", trc("ninjabrain.presets_hint"));
        if (ImGui::Button(trc("ninjabrain.preset_compact"))) {
            applyNinjabrainPreset("compact");
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::Button(trc("ninjabrain.preset_classic_151"))) {
            applyNinjabrainPreset("classic_151");
            changed = true;
        }
        ImGui::Spacing();

        // Rendering
        ImGui::SeparatorText(trc("ninjabrain.rendering"));

        if (ImGui::SliderFloat((std::string(trc("ninjabrain.opacity")) + "##nb").c_str(), &nb.overlayOpacity, 0.0f, 1.0f)) changed = true;
        if (ImGui::Checkbox((std::string(trc("ninjabrain.only_on_my_screen")) + "##nb").c_str(), &nb.onlyOnMyScreen)) {
            if (nb.onlyOnMyScreen) nb.onlyOnObs = false;
            changed = true;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", trc("ninjabrain.tooltip_only_on_my_screen"));
        if (ImGui::Checkbox((std::string(trc("ninjabrain.only_on_obs")) + "##nb").c_str(), &nb.onlyOnObs)) {
            if (nb.onlyOnObs) nb.onlyOnMyScreen = false;
            changed = true;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", trc("ninjabrain.tooltip_only_on_obs"));

        ImGui::Columns(2, "nb_render_cols", false);
        ImGui::SetColumnWidth(0, 120);

        ImGui::Text("%s", trc("ninjabrain.pos_x"));
        ImGui::NextColumn();
        if (Spinner("##nb_x", &nb.x)) changed = true;
        ImGui::NextColumn();

        ImGui::Text("%s", trc("ninjabrain.pos_y"));
        ImGui::NextColumn();
        if (Spinner("##nb_y", &nb.y)) changed = true;
        ImGui::NextColumn();

        ImGui::Text("%s", trc("ninjabrain.font_size"));
        ImGui::NextColumn();
        ImGui::SetNextItemWidth(250);
        if (ImGui::SliderFloat("##nbFontSize", &nb.fontSize, 24.0f, 96.0f, "%.0f px")) {
            changed = true;
            g_eyeZoomFontNeedsReload.store(true);
        }
        ImGui::NextColumn();

        ImGui::Text("%s", trc("ninjabrain.scale"));
        ImGui::NextColumn();
        float scalePercent = nb.overlayScale * 100.0f;
        ImGui::SetNextItemWidth(250);
        if (ImGui::SliderFloat("##nbScale", &scalePercent, 5.0f, 100.0f, "%.0f%%")) {
            nb.overlayScale = scalePercent / 100.0f; changed = true;
        }
        ImGui::NextColumn();

        ImGui::Text("%s", trc("ninjabrain.side_padding"));
        ImGui::NextColumn();
        ImGui::SetNextItemWidth(250);
        if (ImGui::SliderFloat("##nbSidePadding", &nb.sidePadding, 0.0f, 80.0f, "%.0f px")) changed = true;
        ImGui::NextColumn();

        ImGui::Text("%s", trc("ninjabrain.relative_to"));
        ImGui::NextColumn();
        const char* current_rel_to = getFriendlyName(nb.relativeTo, imageRelativeToOptions);
        ImGui::SetNextItemWidth(150);
        if (ImGui::BeginCombo("##nb_rel_to", current_rel_to)) {
            for (const auto& option : imageRelativeToOptions) {
                if (ImGui::Selectable(option.second, nb.relativeTo == option.first)) {
                    nb.relativeTo = option.first; changed = true;
                }
            }
            ImGui::EndCombo();
        }
        ImGui::NextColumn();

        ImGui::Columns(1);

        // Appearance
        ImGui::SeparatorText(trc("ninjabrain.appearance"));

        if (ImGui::Checkbox((std::string(trc("ninjabrain.show_throw_details")) + "##nb").c_str(), &nb.showThrowDetails)) changed = true;
        if (ImGui::Checkbox((std::string(trc("ninjabrain.show_separators")) + "##nb").c_str(), &nb.showSeparators)) changed = true;
        if (ImGui::Checkbox((std::string(trc("ninjabrain.show_row_stripes")) + "##nb").c_str(), &nb.showRowStripes)) changed = true;

        ImGui::Columns(2, "nb_appear_cols", false);
        ImGui::SetColumnWidth(0, 120);

        ImGui::Text("%s", trc("ninjabrain.border_width"));
        ImGui::NextColumn();
        if (Spinner("##nbBorderWidth", &nb.borderWidth, 1, 0, 8)) changed = true;
        ImGui::NextColumn();

        ImGui::Text("%s", trc("ninjabrain.corner_radius"));
        ImGui::NextColumn();
        ImGui::SetNextItemWidth(250);
        if (ImGui::SliderFloat("##nbCornerRadius", &nb.cornerRadius, 0.0f, 16.0f, "%.0f px")) changed = true;
        ImGui::NextColumn();

        ImGui::Text("%s", trc("ninjabrain.outline_width"));
        ImGui::NextColumn();
        if (Spinner("##nbOutline", &nb.outlineWidth, 1, 0, 10)) changed = true;
        ImGui::NextColumn();

        ImGui::Columns(1);

        ImGui::Text("%s", trc("ninjabrain.custom_font"));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(300);
        if (ImGui::InputText("##nbCustomFont", nbFontBuf, sizeof(nbFontBuf))) {
            nb.customFontPath = nbFontBuf;
            changed = true; g_eyeZoomFontNeedsReload.store(true);
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s", trc("ninjabrain.custom_font_hint"));

        // Background
        ImGui::SeparatorText(trc("ninjabrain.background"));

        if (ImGui::Checkbox((std::string(trc("ninjabrain.enable_background")) + "##nb").c_str(), &nb.bgEnabled)) changed = true;
        ImGui::BeginDisabled(!nb.bgEnabled);
        ImGui::SetNextItemWidth(250);
        if (ImGui::SliderFloat((std::string(trc("ninjabrain.bg_opacity")) + "##nb").c_str(), &nb.bgOpacity, 0.f, 1.f, "%.2f")) changed = true;
        ImGui::EndDisabled();

        ImGui::SeparatorText(trc("ninjabrain.colors"));

        if (ImGui::ColorEdit3((std::string(trc("ninjabrain.header_fill")) + "##nb").c_str(), &nb.headerFillColor.r)) changed = true;
        if (ImGui::ColorEdit3((std::string(trc("ninjabrain.bg_color")) + "##nb").c_str(), &nb.bgColor.r)) changed = true;
        if (ImGui::ColorEdit3((std::string(trc("ninjabrain.throws_background")) + "##nb").c_str(), &nb.throwsBackgroundColor.r)) changed = true;
        if (ImGui::ColorEdit3((std::string(trc("ninjabrain.divider_color")) + "##nb").c_str(), &nb.dividerColor.r)) changed = true;
        if (ImGui::ColorEdit3((std::string(trc("ninjabrain.header_divider_color")) + "##nb").c_str(), &nb.headerDividerColor.r)) changed = true;
        if (ImGui::ColorEdit3((std::string(trc("ninjabrain.color_data")) + "##nb").c_str(), &nb.dataColor.r)) changed = true;
        if (ImGui::ColorEdit3((std::string(trc("ninjabrain.throws_text_color")) + "##nb").c_str(), &nb.throwsTextColor.r)) changed = true;
        if (ImGui::ColorEdit3((std::string(trc("ninjabrain.divine_text_color")) + "##nb").c_str(), &nb.divineTextColor.r)) changed = true;
        if (ImGui::ColorEdit3((std::string(trc("ninjabrain.color_headers")) + "##nb").c_str(), &nb.textColor.r)) changed = true;
        if (ImGui::ColorEdit3((std::string(trc("ninjabrain.subpixel_positive_color")) + "##nb").c_str(), &nb.subpixelPositiveColor.r)) changed = true;
        if (ImGui::ColorEdit3((std::string(trc("ninjabrain.subpixel_negative_color")) + "##nb").c_str(), &nb.subpixelNegativeColor.r)) changed = true;
        if (ImGui::ColorEdit3((std::string(trc("ninjabrain.certainty_color")) + "##nb").c_str(), &nb.certaintyColor.r)) changed = true;
        if (ImGui::ColorEdit3((std::string(trc("ninjabrain.certainty_mid_color")) + "##nb").c_str(), &nb.certaintyMidColor.r)) changed = true;
        if (ImGui::ColorEdit3((std::string(trc("ninjabrain.certainty_low_color")) + "##nb").c_str(), &nb.certaintyLowColor.r)) changed = true;
        if (ImGui::Checkbox((std::string(trc("ninjabrain.color_neg_coords")) + "##nb").c_str(), &nb.negCoordColorEnabled)) changed = true;
        ImGui::Spacing();

        // Eye Throws Overlay
        ImGui::SeparatorText(trc("ninjabrain.throws"));

        if (ImGui::Checkbox((std::string(trc("ninjabrain.always_show_boat")) + "##nb").c_str(), &nb.alwaysShowBoat)) {
            changed = true;
            if (nb.alwaysShowBoat) {
                for (auto& col : nb.columns) {
                    if (col.id == "boat") { col.show = true; break; }
                }
            }
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s", trc("ninjabrain.tooltip_always_show_boat"));
        ImGui::Spacing();

        ImGui::Columns(2, "nb_eye_cols", false);
        ImGui::SetColumnWidth(0, 120);
        ImGui::Text("%s", trc("ninjabrain.predictions"));
        ImGui::NextColumn();
        if (Spinner("##nbShownPreds", &nb.shownPredictions, 1, 1, 5)) changed = true;
        ImGui::NextColumn();
        ImGui::Text("%s", trc("ninjabrain.coords_display"));
        ImGui::NextColumn();
        const char* currentCoordsDisplay = (nb.coordsDisplay == "chunk")
            ? trc("ninjabrain.coords_display_chunk")
            : trc("ninjabrain.coords_display_block");
        ImGui::SetNextItemWidth(180.0f);
        if (ImGui::BeginCombo("##nbCoordsDisplay", currentCoordsDisplay)) {
            const bool coordsAreChunk = (nb.coordsDisplay == "chunk");
            if (ImGui::Selectable(trc("ninjabrain.coords_display_block"), !coordsAreChunk)) {
                nb.coordsDisplay = "block";
                for (auto& col : nb.columns) {
                    if (col.id == "coords" && (col.header.empty() || col.header == "Chunk")) {
                        col.header = "Location";
                        break;
                    }
                }
                changed = true;
            }
            if (ImGui::Selectable(trc("ninjabrain.coords_display_chunk"), coordsAreChunk)) {
                nb.coordsDisplay = "chunk";
                for (auto& col : nb.columns) {
                    if (col.id == "coords" && (col.header.empty() || col.header == "Location")) {
                        col.header = "Chunk";
                        break;
                    }
                }
                changed = true;
            }
            ImGui::EndCombo();
        }
        ImGui::NextColumn();
        ImGui::Text("%s", trc("ninjabrain.row_spacing"));
        ImGui::NextColumn();
        ImGui::SetNextItemWidth(120.f);
        if (ImGui::SliderFloat("##nbRowSpacing", &nb.rowSpacing, 0.0f, 30.0f, "%.0f px")) changed = true;
        ImGui::NextColumn();
        ImGui::Text("%s", trc("ninjabrain.col_spacing"));
        ImGui::NextColumn();
        ImGui::SetNextItemWidth(120.f);
        if (ImGui::SliderFloat("##nbColSpacing", &nb.colSpacing, 0.0f, 60.0f, "%.0f px")) changed = true;
        ImGui::NextColumn();
        ImGui::Columns(1);
        ImGui::Spacing();

        ImGui::Text("%s", trc("ninjabrain.columns"));
        ImGui::Spacing();
        if (ImGui::BeginTable("##nbCols", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
        {
            ImGui::TableSetupColumn("#",      ImGuiTableColumnFlags_WidthFixed,   22.f);
            ImGui::TableSetupColumn("Show",   ImGuiTableColumnFlags_WidthFixed,   44.f);
            ImGui::TableSetupColumn("Header", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Order",  ImGuiTableColumnFlags_WidthFixed,   80.f);
            ImGui::TableHeadersRow();

            int moveFrom = -1, moveTo = -1;
            for (int ci = 0; ci < (int)nb.columns.size(); ci++) {
                auto& col = nb.columns[ci];
                ImGui::TableNextRow();
                ImGui::PushID(ci);

                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("%d", ci + 1);

                ImGui::TableSetColumnIndex(1);
                bool show = col.show;
                if (ImGui::Checkbox("##show", &show)) { col.show = show; changed = true; }

                ImGui::TableSetColumnIndex(2);
                char buf[32]; strncpy_s(buf, col.header.c_str(), sizeof(buf) - 1);
                ImGui::SetNextItemWidth(-1);
                if (ImGui::InputText("##hdr", buf, sizeof(buf))) { col.header = buf; changed = true; }

                ImGui::TableSetColumnIndex(3);
                if (ci == 0) ImGui::BeginDisabled();
                if (ImGui::Button("Up##col"))   { moveFrom = ci; moveTo = ci - 1; changed = true; }
                if (ci == 0) ImGui::EndDisabled();
                ImGui::SameLine();
                if (ci == (int)nb.columns.size() - 1) ImGui::BeginDisabled();
                if (ImGui::Button("Down##col")) { moveFrom = ci; moveTo = ci + 1; changed = true; }
                if (ci == (int)nb.columns.size() - 1) ImGui::EndDisabled();

                ImGui::PopID();
            }
            if (moveFrom >= 0 && moveTo >= 0 && moveTo < (int)nb.columns.size())
                std::swap(nb.columns[moveFrom], nb.columns[moveTo]);

            ImGui::EndTable();
        }
        ImGui::Spacing();

        } // nb.enabled

        ImGui::Spacing();
        if (ImGui::Button(trc("ninjabrain.reset_button"))) {
            ImGui::OpenPopup(trc("ninjabrain.reset_title"));
        }
        if (ImGui::BeginPopupModal(trc("ninjabrain.reset_title"), NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("%s", trc("ninjabrain.reset_confirm"));
            ImGui::Separator();
            if (ImGui::Button(trc("button.confirm_reset"), ImVec2(120, 0))) {
                nb = NinjabrainOverlayConfig{};
                changed = true;
                nbFontBufInit = false;
                g_eyeZoomFontNeedsReload.store(true);
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button(trc("button.cancel"), ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
            ImGui::EndPopup();
        }

        if (changed) g_configIsDirty = true;
    }
#endif

    ImGui::EndTabItem();
}

#if !defined(TOOLSCREEN_NINJABRAIN_OVERLAY_ONLY)
else {
    g_windowOverlayDragMode.store(false);
}
#endif
