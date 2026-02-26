if (ImGui::BeginTabItem("Supporters")) {
    g_currentlyEditingMirror = "";
    g_imageDragMode.store(false);
    g_windowOverlayDragMode.store(false);

    ImGui::TextWrapped("Thanks to these people for supporting the development of Toolscreen!");
    ImGui::TextWrapped("If you'd like to support, please consider donating at https://patreon.com/jojoe77777");
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

        for (const auto& role : g_supporterRoles) {
            const ImVec4 roleColor(role.color.r, role.color.g, role.color.b, role.color.a);
            ImGui::PushStyleColor(ImGuiCol_Text, roleColor);
            ImGui::SeparatorText(role.name.c_str());
            ImGui::PopStyleColor();

            if (role.members.empty()) {
                ImGui::TextDisabled("No members listed.");
            } else {
                for (const auto& member : role.members) {
                    ImGui::BulletText("%s", member.c_str());
                }
            }

            ImGui::Spacing();
        }
    }

    ImGui::EndTabItem();
}


