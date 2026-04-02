if (BeginSelectableSettingsTopTabItem(trc("label.overlays"))) {
    if (ImGui::BeginTabBar("OverlaySettingsTabs")) {
#include "tab_mirrors.inl"
#include "tab_images.inl"

#define TOOLSCREEN_WINDOW_OVERLAYS_ONLY
#include "tab_window_overlays.inl"
#undef TOOLSCREEN_WINDOW_OVERLAYS_ONLY

#include "tab_browser_overlays.inl"

#define TOOLSCREEN_NINJABRAIN_OVERLAY_ONLY
#include "tab_window_overlays.inl"
#undef TOOLSCREEN_NINJABRAIN_OVERLAY_ONLY

        ImGui::EndTabBar();
    }

    ImGui::EndTabItem();
}