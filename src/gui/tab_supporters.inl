if (ImGui::BeginTabItem("Supporters")) {
    g_currentlyEditingMirror = "";
    g_imageDragMode.store(false);
    g_windowOverlayDragMode.store(false);

    if (g_supporterTierTexturesDirty.exchange(false, std::memory_order_acq_rel)) { ClearSupporterTierTextureCache(); }

    ImGui::TextWrapped("Thanks to these people for supporting the development of Toolscreen!");
    ImGui::TextWrapped("If you'd like to support, please consider donating at:");
    ImGui::TextLinkOpenURL("https://patreon.com/jojoe77777");
    ImGui::Spacing();

    const bool loaded = g_supportersLoaded.load(std::memory_order_acquire);
    const bool failedBefore = g_supportersFetchEverFailed.load(std::memory_order_acquire);

    if (!loaded) {
        if (failedBefore) {
            ImGui::TextWrapped("Unable to load supporters.");
        } else {
            ImGui::TextWrapped("Loading supporters...");
        }
    } else {
        std::shared_lock<std::shared_mutex> readLock(g_supportersMutex);
        if (g_supporterRoles.empty()) {
            ImGui::TextDisabled("No supporters listed.");
        }

        for (size_t tierIndex = 0; tierIndex < g_supporterRoles.size(); ++tierIndex) {
            const auto& role = g_supporterRoles[tierIndex];
            GLuint tierTexture = 0;
            int tierTextureWidth = 0;
            int tierTextureHeight = 0;
            const bool hasTierTexture = EnsureSupporterTierTexture(role, tierTexture, tierTextureWidth, tierTextureHeight);

            const ImVec4 roleColor(role.color.r, role.color.g, role.color.b, role.color.a);
            ImVec2 iconSize(0.0f, 0.0f);
            if (hasTierTexture) {
                constexpr float kMaxIconSize = 22.0f;
                float iconScale = 1.0f;
                const int maxSide = (std::max)(tierTextureWidth, tierTextureHeight);
                if (maxSide > 0) { iconScale = kMaxIconSize / static_cast<float>(maxSide); }

                iconSize = ImVec2((std::max)(1.0f, static_cast<float>(tierTextureWidth) * iconScale),
                                  (std::max)(1.0f, static_cast<float>(tierTextureHeight) * iconScale));
            }

            const float tierHeaderStartY = ImGui::GetCursorPosY();
            const float textLineHeight = ImGui::GetTextLineHeight();
            const float tierHeaderHeight = hasTierTexture ? (std::max)(textLineHeight, iconSize.y) : textLineHeight;

            if (hasTierTexture) {
                ImGui::SetCursorPosY(tierHeaderStartY + (tierHeaderHeight - iconSize.y) * 0.5f);
                ImGui::Image((ImTextureID)(intptr_t)tierTexture, iconSize);
                ImGui::SameLine(0.0f, 8.0f);
            }

            ImGui::SetCursorPosY(tierHeaderStartY + (tierHeaderHeight - textLineHeight) * 0.5f);
            ImGui::PushStyleColor(ImGuiCol_Text, roleColor);
            ImGui::TextUnformatted(role.name.c_str());
            ImGui::PopStyleColor();

            ImGui::SetCursorPosY(tierHeaderStartY + tierHeaderHeight);
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::Indent(1.0f);

            if (role.members.empty()) {
                ImGui::TextDisabled("No members listed.");
            } else {
                for (const auto& member : role.members) {
                    ImGui::BulletText("%s", member.c_str());
                }
            }
            ImGui::Unindent(1.0f);

            if (tierIndex + 1 < g_supporterRoles.size()) {
                ImGui::Dummy(ImVec2(0.0f, 8.0f));
            }
        }
    }

    ImGui::EndTabItem();
}


