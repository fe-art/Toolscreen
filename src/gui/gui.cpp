#include "gui.h"
#include "gui_internal.h"
#include "config/config_toml.h"
#include "common/mode_dimensions.h"
#include "features/fake_cursor.h"
#include "imgui_cache.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"
#include "imgui_input_queue.h"
#include "imgui_stdlib.h"
#include "runtime/logic_thread.h"
#include "render/mirror_thread.h"
#include "common/profiler.h"
#include "render/obs_thread.h"
#include "render/render.h"
#include "platform/resource.h"
#include <nlohmann/json.hpp>
#include "common/i18n.h"
#include "third_party/stb_image.h"
#include "common/utils.h"
#include "features/browser_overlay.h"
#include "features/ninjabrain_client.h"
#include "features/virtual_camera.h"
#include "features/window_overlay.h"
#include "hooks/input_hook.h"

#include <GL/glew.h>
#include <ShlObj.h>
#include <shellapi.h>
#include <Shlwapi.h>
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cmath>
#include <commdlg.h>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <set>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_set>
#include <unordered_map>
#include <winhttp.h>
#include <windowsx.h>

#pragma comment(lib, "Shlwapi.lib")
#pragma comment(lib, "Winhttp.lib")

static constexpr const wchar_t* DISCORD_URL = L"https://discord.gg/A2v6bCJg6K";

void ApplyKeyRepeatSettings();

extern std::atomic<bool> g_gameWindowActive;

static constexpr float spinnerHoldDelay = 0.2f;
static constexpr float spinnerHoldInterval = 0.01f;

ImFont* g_keyboardLayoutPrimaryFont = nullptr;
ImFont* g_keyboardLayoutSecondaryFont = nullptr;

#define SliderFloat SliderFloatDoubleClickInput
#define SliderInt SliderIntDoubleClickInput

// This function MUST be defined before the JSON serialization functions that call it
EyeZoomConfig GetDefaultEyeZoomConfig() { return GetDefaultEyeZoomConfigFromEmbedded(); }

namespace {

const char* s_forcedSettingsTopTabLabel = nullptr;
const char* s_forcedSettingsInputsSubTabLabel = nullptr;

#ifdef TOOLSCREEN_GUI_INTEGRATION_TESTS
std::unordered_map<std::string, GuiTestInteractionRect> s_guiTestInteractionRects;
bool s_guiTestOpenKeyboardLayoutRequested = false;
DWORD s_guiTestOpenKeyboardLayoutContextVk = 0;
int s_guiTestKeyboardLayoutSplitModeRequest = -1;
GuiTestKeyboardLayoutBindTarget s_guiTestKeyboardLayoutBindTargetRequest = GuiTestKeyboardLayoutBindTarget::None;
int s_guiTestKeyboardLayoutShiftUppercaseRequest = -1;
int s_guiTestKeyboardLayoutShiftCapsLockRequest = -1;
bool s_guiTestKeyboardLayoutOpenScanPickerRequested = false;
int s_guiTestKeyboardLayoutScanFilterRequest = 99;
DWORD s_guiTestKeyboardLayoutSelectScanRequest = 0;
bool s_guiTestKeyboardLayoutResetScanToDefaultRequested = false;

void RecordGuiTestInteractionRect(const char* id, const ImVec2& min, const ImVec2& max) {
    if (id == nullptr) {
        return;
    }

    s_guiTestInteractionRects[std::string(id)] = GuiTestInteractionRect{ min.x, min.y, max.x, max.y };
}

void RecordGuiTestInteractionRect(const std::string& id, const ImVec2& min, const ImVec2& max) {
    RecordGuiTestInteractionRect(id.c_str(), min, max);
}

void RecordGuiTestKeyboardLayoutKeyRect(DWORD vk, const ImVec2& min, const ImVec2& max) {
    char id[96] = {};
    sprintf_s(id, "inputs.keyboard_layout.key.%u", static_cast<unsigned>(vk));
    RecordGuiTestInteractionRect(id, min, max);
}

bool ConsumeGuiTestOpenKeyboardLayoutRequest() {
    const bool requested = s_guiTestOpenKeyboardLayoutRequested;
    s_guiTestOpenKeyboardLayoutRequested = false;
    return requested;
}

DWORD ConsumeGuiTestOpenKeyboardLayoutContextRequest() {
    const DWORD vk = s_guiTestOpenKeyboardLayoutContextVk;
    s_guiTestOpenKeyboardLayoutContextVk = 0;
    return vk;
}

int ConsumeGuiTestKeyboardLayoutSplitModeRequest() {
    const int request = s_guiTestKeyboardLayoutSplitModeRequest;
    s_guiTestKeyboardLayoutSplitModeRequest = -1;
    return request;
}

GuiTestKeyboardLayoutBindTarget ConsumeGuiTestKeyboardLayoutBindTargetRequest() {
    const GuiTestKeyboardLayoutBindTarget request = s_guiTestKeyboardLayoutBindTargetRequest;
    s_guiTestKeyboardLayoutBindTargetRequest = GuiTestKeyboardLayoutBindTarget::None;
    return request;
}

int ConsumeGuiTestKeyboardLayoutShiftUppercaseRequest() {
    const int request = s_guiTestKeyboardLayoutShiftUppercaseRequest;
    s_guiTestKeyboardLayoutShiftUppercaseRequest = -1;
    return request;
}

int ConsumeGuiTestKeyboardLayoutShiftCapsLockRequest() {
    const int request = s_guiTestKeyboardLayoutShiftCapsLockRequest;
    s_guiTestKeyboardLayoutShiftCapsLockRequest = -1;
    return request;
}

bool ConsumeGuiTestKeyboardLayoutOpenScanPickerRequest() {
    const bool requested = s_guiTestKeyboardLayoutOpenScanPickerRequested;
    s_guiTestKeyboardLayoutOpenScanPickerRequested = false;
    return requested;
}

int ConsumeGuiTestKeyboardLayoutScanFilterRequest() {
    const int request = s_guiTestKeyboardLayoutScanFilterRequest;
    s_guiTestKeyboardLayoutScanFilterRequest = 99;
    return request;
}

DWORD ConsumeGuiTestKeyboardLayoutSelectScanRequest() {
    const DWORD request = s_guiTestKeyboardLayoutSelectScanRequest;
    s_guiTestKeyboardLayoutSelectScanRequest = 0;
    return request;
}

bool ConsumeGuiTestKeyboardLayoutResetScanToDefaultRequest() {
    const bool requested = s_guiTestKeyboardLayoutResetScanToDefaultRequested;
    s_guiTestKeyboardLayoutResetScanToDefaultRequested = false;
    return requested;
}
#endif

bool BeginSelectableSettingsTopTabItem(const char* label) {
    ImGuiTabItemFlags flags = ImGuiTabItemFlags_None;
    if (s_forcedSettingsTopTabLabel != nullptr && std::strcmp(s_forcedSettingsTopTabLabel, label) == 0) {
        flags |= ImGuiTabItemFlags_SetSelected;
    }
    return ImGui::BeginTabItem(label, nullptr, flags);
}

bool BeginSelectableSettingsInputsSubTabItem(const char* label) {
    ImGuiTabItemFlags flags = ImGuiTabItemFlags_None;
    if (s_forcedSettingsInputsSubTabLabel != nullptr && std::strcmp(s_forcedSettingsInputsSubTabLabel, label) == 0) {
        flags |= ImGuiTabItemFlags_SetSelected;
    }
    return ImGui::BeginTabItem(label, nullptr, flags);
}

const std::vector<std::pair<const char*, const char*>>& GetSettingsRelativeToOptions() {
    static std::vector<std::pair<const char*, const char*>> options;
    static uint64_t cachedGeneration = static_cast<uint64_t>(-1);

    const uint64_t generation = GetTranslationGeneration();
    if (cachedGeneration != generation) {
        options = {
            {"topLeftViewport", trc("position.top_left_viewport")},
            {"topRightViewport", trc("position.top_right_viewport")},
            {"bottomLeftViewport", trc("position.bottom_left_viewport")},
            {"bottomRightViewport", trc("position.bottom_right_viewport")},
            {"centerViewport", trc("position.center_viewport")},
            {"pieLeft", trc("position.pie_left")},
            {"pieRight", trc("position.pie_right")},
            {"topLeftScreen", trc("position.top_left_screen")},
            {"topRightScreen", trc("position.top_right_screen")},
            {"bottomLeftScreen", trc("position.bottom_left_screen")},
            {"bottomRightScreen", trc("position.bottom_right_screen")},
            {"centerScreen", trc("position.center_screen")}
        };
        cachedGeneration = generation;
    }

    return options;
}

const std::vector<std::pair<const char*, const char*>>& GetSettingsImageRelativeToOptions() {
    static std::vector<std::pair<const char*, const char*>> options;
    static uint64_t cachedGeneration = static_cast<uint64_t>(-1);

    const uint64_t generation = GetTranslationGeneration();
    if (cachedGeneration != generation) {
        options = {
            {"topLeftViewport", trc("position.top_left_viewport")},
            {"topRightViewport", trc("position.top_right_viewport")},
            {"bottomLeftViewport", trc("position.bottom_left_viewport")},
            {"bottomRightViewport", trc("position.bottom_right_viewport")},
            {"centerViewport", trc("position.center_viewport")},
            {"topLeftScreen", trc("position.top_left_screen")},
            {"topRightScreen", trc("position.top_right_screen")},
            {"bottomLeftScreen", trc("position.bottom_left_screen")},
            {"bottomRightScreen", trc("position.bottom_right_screen")},
            {"centerScreen", trc("position.center_screen")}
        };
        cachedGeneration = generation;
    }

    return options;
}

const char* GetSettingsFriendlyName(const std::string& key, const std::vector<std::pair<const char*, const char*>>& options) {
    for (const auto& option : options) {
        if (key == option.first) {
            return option.second;
        }
    }
    return "Unknown";
}

const std::vector<std::pair<const char*, const char*>>& GetSettingsGameStateDisplayNames() {
    static std::vector<std::pair<const char*, const char*>> names;
    static uint64_t cachedGeneration = static_cast<uint64_t>(-1);

    const uint64_t generation = GetTranslationGeneration();
    if (cachedGeneration != generation) {
        names = {
            {"wall", trc("game_state.wall")},
            {"inworld,cursor_free", trc("game_state.inworld_free")},
            {"inworld,cursor_grabbed", trc("game_state.inworld_grabbed")},
            {"title", trc("game_state.title")},
            {"waiting", trc("game_state.waiting")},
            {"generating", trc("game_state.generating")}
        };
        cachedGeneration = generation;
    }

    return names;
}

const char* GetSettingsGameStateFriendlyName(const std::string& gameState) {
    for (const auto& pair : GetSettingsGameStateDisplayNames()) {
        if (gameState == pair.first) {
            return pair.second;
        }
    }
    return gameState.c_str();
}

void ResetGuiTransientInteractionState() {
    g_currentlyEditingMirror = "";
    g_imageDragMode.store(false);
    g_windowOverlayDragMode.store(false);
    g_browserOverlayDragMode.store(false);

    s_hoveredImageName = "";
    s_draggedImageName = "";
    s_isDragging = false;

    s_hoveredWindowOverlayName = "";
    s_draggedWindowOverlayName = "";
    s_isWindowOverlayDragging = false;

    s_hoveredBrowserOverlayName = "";
    s_draggedBrowserOverlayName = "";
    s_isBrowserOverlayDragging = false;
}

void CloseSettingsGuiWindow() {
    g_showGui = false;
    InvalidateImGuiCache();

    HWND hwnd = g_minecraftHwnd.load(std::memory_order_relaxed);
    ImGuiInputQueue_Clear();
    ImGuiInputQueue_ResetMouseCapture(hwnd);

    if (ApplyConfineCursorToGameWindow()) {
        SetCursor(NULL);
    } else if (!g_wasCursorVisible.load(std::memory_order_acquire)) {
        RECT clipRect{};
        if (GetWindowClientRectInScreen(hwnd, clipRect)) {
            ClipCursor(&clipRect);
        } else {
            ClipCursor(NULL);
        }
        SetCursor(NULL);
    }

    ResetGuiTransientInteractionState();
}

}

#ifdef TOOLSCREEN_GUI_INTEGRATION_TESTS
void ResetGuiTestInteractionRects() {
    s_guiTestInteractionRects.clear();
}

bool GetGuiTestInteractionRect(const char* id, GuiTestInteractionRect& outRect) {
    if (id == nullptr) {
        return false;
    }

    const auto found = s_guiTestInteractionRects.find(id);
    if (found == s_guiTestInteractionRects.end()) {
        return false;
    }

    outRect = found->second;
    return true;
}

void RequestGuiTestOpenKeyboardLayout() {
    s_guiTestOpenKeyboardLayoutRequested = true;
}

void RequestGuiTestOpenKeyboardLayoutContext(DWORD vk) {
    s_guiTestOpenKeyboardLayoutContextVk = vk;
}

void RequestGuiTestKeyboardLayoutSetSplitMode(bool splitMode) {
    s_guiTestKeyboardLayoutSplitModeRequest = splitMode ? 1 : 0;
}

void RequestGuiTestKeyboardLayoutBeginBind(GuiTestKeyboardLayoutBindTarget target) {
    s_guiTestKeyboardLayoutBindTargetRequest = target;
}

void RequestGuiTestKeyboardLayoutSetShiftLayerUppercase(bool enabled) {
    s_guiTestKeyboardLayoutShiftUppercaseRequest = enabled ? 1 : 0;
}

void RequestGuiTestKeyboardLayoutSetShiftLayerUsesCapsLock(bool enabled) {
    s_guiTestKeyboardLayoutShiftCapsLockRequest = enabled ? 1 : 0;
}

void RequestGuiTestKeyboardLayoutOpenScanPicker() {
    s_guiTestKeyboardLayoutOpenScanPickerRequested = true;
}

void RequestGuiTestKeyboardLayoutSetScanFilter(GuiTestKeyboardLayoutScanFilterGroup group) {
    s_guiTestKeyboardLayoutScanFilterRequest = static_cast<int>(group);
}

void RequestGuiTestKeyboardLayoutSelectScan(DWORD scan) {
    s_guiTestKeyboardLayoutSelectScanRequest = scan;
}

void RequestGuiTestKeyboardLayoutResetScanToDefault() {
    s_guiTestKeyboardLayoutResetScanToDefaultRequested = true;
}
#endif

void SetGuiTabSelectionOverride(const char* topLevelTabLabel, const char* inputsSubTabLabel) {
    s_forcedSettingsTopTabLabel = topLevelTabLabel;
    s_forcedSettingsInputsSubTabLabel = inputsSubTabLabel;
}

void ClearGuiTabSelectionOverride() {
    s_forcedSettingsTopTabLabel = nullptr;
    s_forcedSettingsInputsSubTabLabel = nullptr;
}

void RenderConfigErrorGUI() {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 center = ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(600, 0));
    if (ImGui::Begin(trc("error.configuration_error"), NULL,
                     ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove)) {
        static std::chrono::steady_clock::time_point s_lastCopyTime{};
        std::string errorMsg;
        {
            std::lock_guard<std::mutex> lock(g_configErrorMutex);
            errorMsg = g_configLoadError;
        }
        ImGui::TextWrapped("A critical error occurred while loading the configuration file (config.toml).");
        ImGui::Separator();
        ImGui::TextWrapped("%s", errorMsg.c_str());
        ImGui::Separator();
        ImGui::TextWrapped("The application cannot continue. To get help, copy the debug info and send it to a "
                           "developer. Otherwise, please quit the game.");
        ImGui::Separator();

        bool show_feedback =
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - s_lastCopyTime).count() < 3;

        float button_width_copy = ImGui::CalcTextSize(trc("button.copy_debug_info")).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        float button_width_quit = ImGui::CalcTextSize(trc("button.quit")).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        float total_button_width = button_width_copy + button_width_quit + ImGui::GetStyle().ItemSpacing.x;
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - total_button_width) * 0.5f);

        if (ImGui::Button(trc("button.copy_debug_info"))) {
            std::string configContent = "ERROR: Could not read config.toml.";
            std::wstring configPath = g_toolscreenPath + L"\\config.toml";
            std::ifstream f(std::filesystem::path(configPath), std::ios::binary);
            if (f.is_open()) {
                std::ostringstream ss;
                ss << f.rdbuf();
                configContent = ss.str();
                f.close();
            }

            std::string fullDebugInfo = "Error Message:\r\n";
            fullDebugInfo += "----------------------------------------\r\n";
            fullDebugInfo += errorMsg;
            fullDebugInfo += "\r\n\r\n\r\nRaw config.toml Content:\r\n";
            fullDebugInfo += "----------------------------------------\r\n";
            fullDebugInfo += configContent;

            CopyToClipboard(g_minecraftHwnd.load(), fullDebugInfo);
            s_lastCopyTime = std::chrono::steady_clock::now();
        }

        ImGui::SameLine();
        if (ImGui::Button(trc("button.quit"))) { exit(0); }

        if (show_feedback) {
            const char* feedback_text = "Debug info copied to clipboard!";
            float feedback_width = ImGui::CalcTextSize(feedback_text).x;
            ImGui::SetCursorPosX((ImGui::GetWindowWidth() - feedback_width) * 0.5f);
            ImGui::TextUnformatted(feedback_text);
        }

        ImGui::End();
    }
}

void RenderSettingsGUI() {
    ResetTransientBindingUiState();

#ifdef TOOLSCREEN_GUI_INTEGRATION_TESTS
    ResetGuiTestInteractionRects();
#endif

    const auto& relativeToOptions = GetSettingsRelativeToOptions();
    const auto& imageRelativeToOptions = GetSettingsImageRelativeToOptions();
    auto getFriendlyName = [](const std::string& key, const std::vector<std::pair<const char*, const char*>>& options) {
        return GetSettingsFriendlyName(key, options);
    };

    static std::vector<DWORD> s_bindingKeys;
    static std::unordered_set<DWORD> s_bindingKeySet;
    static bool s_hadKeysPressed = false;
    static std::set<DWORD> s_preHeldKeys;
    static bool s_bindingInitialized = false;

    static const std::vector<const char*> validGameStates = { "wall",    "inworld,cursor_free", "inworld,cursor_grabbed", "title",
                                                              "waiting", "generating" };

    static const std::vector<const char*> guiGameStates = { "wall", "inworld,cursor_free", "inworld,cursor_grabbed", "title",
                                                            "generating" };

    auto getGameStateFriendlyName = [](const std::string& gameState) {
        return GetSettingsGameStateFriendlyName(gameState);
    };

    bool is_binding = IsHotkeyBindingActive_UiState();
    if (is_binding) { MarkHotkeyBindingActive(); }

    if (is_binding) {
        if (!s_bindingInitialized) {
            s_bindingKeys.reserve(8);
            s_bindingKeySet.reserve(8);
            s_preHeldKeys.clear();
            for (int vk = 1; vk < 0xFF; ++vk) {
                if (GetAsyncKeyState(vk) & 0x8000) {
                    s_preHeldKeys.insert(static_cast<DWORD>(vk));
                }
            }
            s_bindingInitialized = true;
        }
        ImGui::OpenPopup(trc("hotkeys.bind_hotkey"));
    } else {
        s_bindingKeys.clear();
        s_bindingKeySet.clear();
        s_hadKeysPressed = false;
        s_preHeldKeys.clear();
        s_bindingInitialized = false;
    }

    if (ImGui::BeginPopupModal(trc("hotkeys.bind_hotkey"), NULL, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar)) {
        PROFILE_SCOPE_CAT("Settings Hotkey Binding Popup", "ImGui");

        ImGui::Text(trc("hotkeys.bind_hotkey.tooltip.prompt"));
        ImGui::Text(trc("hotkeys.bind_hotkey.tooltip.confirm"));
        ImGui::Text(trc("hotkeys.bind_hotkey.tooltip.clear"));
        ImGui::Text(trc("hotkeys.bind_hotkey.tooltip.cancel"));
        ImGui::Separator();

        static uint64_t s_lastBindingInputSeqHotkeyBind = 0;
        if (ImGui::IsWindowAppearing()) { s_lastBindingInputSeqHotkeyBind = GetLatestBindingInputSequence(); }

        static std::string s_hotkeyConflictMessage;

        auto finalize_bind = [&](const std::vector<DWORD>& keys) {
            if (!keys.empty() && s_exclusionToBind.hotkey_idx == -1) {
                std::string excludeLabel;
                if (s_mainHotkeyToBind == -999) excludeLabel = "GUI Toggle";
                else if (s_mainHotkeyToBind == -998) excludeLabel = "Borderless Toggle";
                else if (s_mainHotkeyToBind == -997) excludeLabel = "Image Overlays Toggle";
                else if (s_mainHotkeyToBind == -996) excludeLabel = "Window Overlays Toggle";
                else if (s_mainHotkeyToBind == -995) excludeLabel = "Key Rebinds Toggle";
                else if (s_mainHotkeyToBind >= 0) excludeLabel = "Mode Hotkey #" + std::to_string(s_mainHotkeyToBind + 1);
                else if (s_sensHotkeyToBind != -1) excludeLabel = "Sensitivity Hotkey #" + std::to_string(s_sensHotkeyToBind + 1);
                else if (s_altHotkeyToBind.hotkey_idx != -1) excludeLabel = "Mode Hotkey #" + std::to_string(s_altHotkeyToBind.hotkey_idx + 1) + " Alt #" + std::to_string(s_altHotkeyToBind.alt_idx + 1);

                std::string conflict = FindHotkeyConflict(keys, excludeLabel);
                if (!conflict.empty()) {
                    s_hotkeyConflictMessage = "Already assigned to " + conflict;
                    s_bindingKeys.clear();
                    s_bindingKeySet.clear();
                    s_hadKeysPressed = false;
                    return;
                }
            }
            s_hotkeyConflictMessage.clear();

            if (s_mainHotkeyToBind != -1) {
                if (s_mainHotkeyToBind == -999) {
                    g_config.guiHotkey = keys;
                } else if (s_mainHotkeyToBind == -998) {
                    g_config.borderlessHotkey = keys;
                } else if (s_mainHotkeyToBind == -997) {
                    g_config.imageOverlaysHotkey = keys;
                } else if (s_mainHotkeyToBind == -996) {
                    g_config.windowOverlaysHotkey = keys;
                } else if (s_mainHotkeyToBind == -995) {
                    g_config.keyRebinds.toggleHotkey = keys;
                } else {
                    g_config.hotkeys[s_mainHotkeyToBind].keys = keys;
                }
                s_mainHotkeyToBind = -1;
            } else if (s_sensHotkeyToBind != -1) {
                g_config.sensitivityHotkeys[s_sensHotkeyToBind].keys = keys;
                s_sensHotkeyToBind = -1;
            } else if (s_altHotkeyToBind.hotkey_idx != -1) {
                g_config.hotkeys[s_altHotkeyToBind.hotkey_idx].altSecondaryModes[s_altHotkeyToBind.alt_idx].keys = keys;
                s_altHotkeyToBind = { -1, -1 };
            } else if (s_exclusionToBind.hotkey_idx != -1) {
                if (!keys.empty()) {
                    g_config.hotkeys[s_exclusionToBind.hotkey_idx].conditions.exclusions[s_exclusionToBind.exclusion_idx] = keys.back();
                }
                s_exclusionToBind = { -1, -1 };
            }
            g_configIsDirty = true;

            {
                std::lock_guard<std::mutex> hotkeyLock(g_hotkeyMainKeysMutex);
                RebuildHotkeyMainKeys_Internal();
            }

            s_bindingKeys.clear();
            s_bindingKeySet.clear();
            s_hadKeysPressed = false;
            s_preHeldKeys.clear();
            s_bindingInitialized = false;
            ImGui::CloseCurrentPopup();
        };

        auto isModifierVk = [](DWORD key) {
            return key == VK_CONTROL || key == VK_SHIFT || key == VK_MENU || key == VK_LCONTROL || key == VK_RCONTROL ||
                   key == VK_LSHIFT || key == VK_RSHIFT || key == VK_LMENU || key == VK_RMENU;
        };

        DWORD capturedVk = 0;
        LPARAM capturedLParam = 0;
        bool capturedIsMouse = false;
        if (ConsumeBindingInputEventSince(s_lastBindingInputSeqHotkeyBind, capturedVk, capturedLParam, capturedIsMouse)) {
            if (capturedVk == VK_ESCAPE) {
                Log("Binding cancelled from Escape key.");
                s_mainHotkeyToBind = -1;
                s_sensHotkeyToBind = -1;
                s_exclusionToBind = { -1, -1 };
                s_altHotkeyToBind = { -1, -1 };
                s_bindingKeys.clear();
                s_bindingKeySet.clear();
                s_hadKeysPressed = false;
                s_preHeldKeys.clear();
                s_bindingInitialized = false;
                ImGui::CloseCurrentPopup();
                (void)capturedLParam;
                (void)capturedIsMouse;
                ImGui::EndPopup();
                return;
            }

            const bool canClear = (s_exclusionToBind.hotkey_idx == -1);
            if (canClear && (capturedVk == VK_BACK || capturedVk == VK_DELETE)) {
                Log("Binding cleared from Backspace/Delete.");
                finalize_bind({});
                ImGui::EndPopup();
                return;
            }

            const bool canAddCapturedKey = !s_preHeldKeys.count(capturedVk) && (capturedIsMouse || !isModifierVk(capturedVk));
            if (canAddCapturedKey && s_bindingKeySet.insert(capturedVk).second) {
                s_bindingKeys.push_back(capturedVk);
            }
            if (canAddCapturedKey) {
                if (!s_hadKeysPressed) s_hotkeyConflictMessage.clear();
                s_hadKeysPressed = true;
            }
        }

        {
            const bool canClear = (s_exclusionToBind.hotkey_idx == -1);
            if (!canClear) { ImGui::BeginDisabled(); }
            if (ImGui::Button(trc("button.clear"))) {
                finalize_bind({});
                ImGui::EndPopup();
                return;
            }
            if (!canClear) { ImGui::EndDisabled(); }

            ImGui::SameLine();
            if (ImGui::Button(trc("button.cancel"))) {
                Log("Binding cancelled from Cancel button.");
                s_mainHotkeyToBind = -1;
                s_sensHotkeyToBind = -1;
                s_exclusionToBind = { -1, -1 };
                s_altHotkeyToBind = { -1, -1 };
                s_bindingKeys.clear();
                s_bindingKeySet.clear();
                s_hadKeysPressed = false;
                s_preHeldKeys.clear();
                s_bindingInitialized = false;
                ImGui::CloseCurrentPopup();
                ImGui::EndPopup();
                return;
            }
            ImGui::Separator();
        }

        // Evict pre-held keys once they are physically released
        for (auto it = s_preHeldKeys.begin(); it != s_preHeldKeys.end(); ) {
            if (!(GetAsyncKeyState(*it) & 0x8000)) {
                it = s_preHeldKeys.erase(it);
            } else {
                ++it;
            }
        }

        std::vector<DWORD> currentlyDownKeys;
        currentlyDownKeys.reserve(8);

        std::vector<DWORD> modifierKeysToInsert;
        modifierKeysToInsert.reserve(8);

        const bool lctrlDown = (GetAsyncKeyState(VK_LCONTROL) & 0x8000) != 0;
        const bool rctrlDown = (GetAsyncKeyState(VK_RCONTROL) & 0x8000) != 0;
        const bool lshiftDown = (GetAsyncKeyState(VK_LSHIFT) & 0x8000) != 0;
        const bool rshiftDown = (GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0;
        const bool laltDown = (GetAsyncKeyState(VK_LMENU) & 0x8000) != 0;
        const bool raltDown = (GetAsyncKeyState(VK_RMENU) & 0x8000) != 0;

        const bool lctrlPreHeld = s_preHeldKeys.count(VK_LCONTROL) || s_preHeldKeys.count(VK_CONTROL);
        const bool rctrlPreHeld = s_preHeldKeys.count(VK_RCONTROL) || s_preHeldKeys.count(VK_CONTROL);
        const bool lshiftPreHeld = s_preHeldKeys.count(VK_LSHIFT) || s_preHeldKeys.count(VK_SHIFT);
        const bool rshiftPreHeld = s_preHeldKeys.count(VK_RSHIFT) || s_preHeldKeys.count(VK_SHIFT);
        const bool laltPreHeld = s_preHeldKeys.count(VK_LMENU) || s_preHeldKeys.count(VK_MENU);
        const bool raltPreHeld = s_preHeldKeys.count(VK_RMENU) || s_preHeldKeys.count(VK_MENU);

        if (lctrlDown && !lctrlPreHeld) {
            currentlyDownKeys.push_back(VK_LCONTROL);
            modifierKeysToInsert.push_back(VK_LCONTROL);
        }
        if (rctrlDown && !rctrlPreHeld) {
            currentlyDownKeys.push_back(VK_RCONTROL);
            modifierKeysToInsert.push_back(VK_RCONTROL);
        }
        if (lshiftDown && !lshiftPreHeld) {
            currentlyDownKeys.push_back(VK_LSHIFT);
            modifierKeysToInsert.push_back(VK_LSHIFT);
        }
        if (rshiftDown && !rshiftPreHeld) {
            currentlyDownKeys.push_back(VK_RSHIFT);
            modifierKeysToInsert.push_back(VK_RSHIFT);
        }
        if (laltDown && !laltPreHeld) {
            currentlyDownKeys.push_back(VK_LMENU);
            modifierKeysToInsert.push_back(VK_LMENU);
        }
        if (raltDown && !raltPreHeld) {
            currentlyDownKeys.push_back(VK_RMENU);
            modifierKeysToInsert.push_back(VK_RMENU);
        }

        for (int vk = 1; vk < 0xFF; ++vk) {
            // Skip escape (used for cancel), generic modifiers, and Windows keys
            if (vk == VK_ESCAPE || vk == VK_LBUTTON || vk == VK_CONTROL || vk == VK_SHIFT || vk == VK_MENU || vk == VK_LWIN || vk == VK_RWIN ||
                vk == VK_LCONTROL || vk == VK_RCONTROL || vk == VK_LSHIFT || vk == VK_RSHIFT || vk == VK_LMENU || vk == VK_RMENU) {
                continue;
            }
            if (s_preHeldKeys.count(static_cast<DWORD>(vk))) continue;
            if (GetAsyncKeyState(vk) & 0x8000) { currentlyDownKeys.push_back(vk); }
        }

        for (DWORD key : modifierKeysToInsert) {
            if (s_bindingKeySet.insert(key).second) {
                auto insertPos = s_bindingKeys.begin();
                for (auto it = s_bindingKeys.begin(); it != s_bindingKeys.end(); ++it) {
                    if (!isModifierVk(*it)) {
                        insertPos = it;
                        break;
                    }
                    insertPos = it + 1;
                }
                s_bindingKeys.insert(insertPos, key);
            }
        }

        if (!currentlyDownKeys.empty()) {
            if (!s_hadKeysPressed) s_hotkeyConflictMessage.clear();
            s_hadKeysPressed = true;
        }

        if (s_hadKeysPressed && currentlyDownKeys.empty()) {
            finalize_bind(s_bindingKeys);
            if (s_hotkeyConflictMessage.empty()) {
                ImGui::EndPopup();
                return;
            }
        }

        if (!s_bindingKeys.empty()) {
            std::string combo = GetKeyComboString(s_bindingKeys);
            ImGui::Text(tr("hotkeys.bind_hotkey.current", combo.c_str()).c_str());
        } else {
            ImGui::Text(trc("hotkeys.bind_hotkey.current_none"));
        }

        if (!s_hotkeyConflictMessage.empty())
            ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%s", s_hotkeyConflictMessage.c_str());

        ImGui::EndPopup();
    }

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowSizeConstraints(ImVec2(500, 400), ImVec2(FLT_MAX, FLT_MAX));

    const int screenWidth = GetCachedWindowWidth();
    const int screenHeight = GetCachedWindowHeight();

    const float scaleFactor = ComputeGuiScaleFactorFromCachedWindowSize();
    static float s_lastRuntimeScaleFactor = -1.0f;
    if (s_lastRuntimeScaleFactor < 0.0f || fabsf(scaleFactor - s_lastRuntimeScaleFactor) > 0.001f) {
        g_guiNeedsRecenter.store(true, std::memory_order_relaxed);
        s_lastRuntimeScaleFactor = scaleFactor;
    }

    static ImVec2 s_lastDisplaySize = ImVec2(-1.0f, -1.0f);
    if (io.DisplaySize.x > 0.0f && io.DisplaySize.y > 0.0f) {
        const bool displaySizeChanged =
            fabsf(io.DisplaySize.x - s_lastDisplaySize.x) > 0.5f || fabsf(io.DisplaySize.y - s_lastDisplaySize.y) > 0.5f;
        if (displaySizeChanged) { g_guiNeedsRecenter.store(true, std::memory_order_relaxed); }
        s_lastDisplaySize = io.DisplaySize;
    }

    if (g_guiNeedsRecenter.exchange(false)) {
        ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x * 0.5f, io.DisplaySize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(850 * scaleFactor, 650 * scaleFactor), ImGuiCond_Always);
    }

    std::string windowTitle = "Toolscreen v" + GetToolscreenVersionString() + " by jojoe77777";

    bool windowOpen = true;
    const bool windowVisible = ImGui::Begin(windowTitle.c_str(), &windowOpen, ImGuiWindowFlags_NoCollapse);
    if (!windowOpen) {
        CloseSettingsGuiWindow();
    }

    if (windowVisible && windowOpen) {

        float headerRightStartX = 0;
        {
            PROFILE_SCOPE_CAT("Settings Header Controls", "ImGui");

            static std::chrono::steady_clock::time_point s_lastScreenshotTime{};
            auto now = std::chrono::steady_clock::now();
            bool showCopied = std::chrono::duration_cast<std::chrono::seconds>(now - s_lastScreenshotTime).count() < 3;

            const char* buttonLabel = showCopied ? trc("button.screenshot.copied") : trc("button.screenshot");
            float buttonWidth = ImGui::CalcTextSize(buttonLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f;
            float iconSize = ImGui::GetFrameHeight();
            float margin = ImGui::GetStyle().ItemSpacing.x;
            headerRightStartX = ImGui::GetWindowContentRegionMax().x - buttonWidth - iconSize * 2 - margin * 2;

            ImVec2 savedCursor = ImGui::GetCursorPos();

            {
                static GLuint s_languageTexture = 0;
                static HGLRC s_languageLastCtx = NULL;
                HGLRC currentCtx = wglGetCurrentContext();
                if (currentCtx != s_languageLastCtx) {
                    s_languageTexture = 0;
                    s_languageLastCtx = currentCtx;
                }

                LoadEmbeddedResourceTexture(s_languageTexture, IDR_LANGUAGE_PNG);

                if (s_languageTexture != 0) {
                    float iconSize = ImGui::GetFrameHeight();
                    float margin = ImGui::GetStyle().ItemSpacing.x;
                    ImGui::SetCursorPos(ImVec2(ImGui::GetWindowContentRegionMax().x - buttonWidth - iconSize * 2 - margin * 2, 30.0f));
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.1f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.2f));
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                    if (ImGui::ImageButton("##language", (ImTextureID)(intptr_t)s_languageTexture, ImVec2(iconSize, iconSize))) {
                        ImGui::OpenPopup("##LanguagePopup");
                    }
                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor(3);

                    if (ImGui::BeginPopup("##LanguagePopup")) {
                        PROFILE_SCOPE_CAT("Settings Language Popup", "ImGui");

                        const nlohmann::json& langs = GetLangs();
                        for (const auto& [langCode, langName] : langs.items()) {
                            bool isSelected = (g_config.lang == langCode);
                            if (ImGui::Selectable(langName.get<std::string>().c_str(), isSelected)) {
                                if (g_config.lang != langCode) {
                                    g_config.lang = langCode;
                                    LoadTranslation(langCode);
                                    RequestDynamicGuiFontRefresh(true);
                                    g_configIsDirty = true;
                                }
                            }
                            if (isSelected) {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndPopup(); 
                    }
                }
            }

            {
                static GLuint s_discordTexture = 0;
                static HGLRC s_discordLastCtx = NULL;
                HGLRC currentCtx = wglGetCurrentContext();
                if (currentCtx != s_discordLastCtx) {
                    s_discordTexture = 0;
                    s_discordLastCtx = currentCtx;
                }

                LoadEmbeddedResourceTexture(s_discordTexture, IDR_DISCORD_PNG);

                if (s_discordTexture != 0) {
                    float iconSize = ImGui::GetFrameHeight();
                    float margin = ImGui::GetStyle().ItemSpacing.x;
                    ImGui::SetCursorPos(ImVec2(ImGui::GetWindowContentRegionMax().x - buttonWidth - iconSize - margin, 30.0f));
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.1f));
                    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.2f));
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                    if (ImGui::ImageButton("##discord", (ImTextureID)(intptr_t)s_discordTexture, ImVec2(iconSize, iconSize))) {
                        ShellExecuteW(NULL, L"open", DISCORD_URL, NULL, NULL, SW_SHOWNORMAL);
                    }
                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor(3);
                    if (ImGui::IsItemHovered()) {
                        ImGui::SetTooltip(trc("tooltip.join_discord"));
                    }
                }
            }

            ImGui::SetCursorPos(ImVec2(ImGui::GetWindowContentRegionMax().x - buttonWidth, 30.0f));

            if (ImGui::Button(buttonLabel)) {
                g_screenshotRequested = true;
                s_lastScreenshotTime = std::chrono::steady_clock::now();
            }

            ImGui::SetCursorPos(savedCursor);
        }

        {
            bool isAdvanced = !g_config.basicModeEnabled;
            if (ImGui::RadioButton(trc("config_mode.basic"), !isAdvanced)) {
                g_config.basicModeEnabled = true;
                g_configIsDirty = true;
            }
            ImGui::SameLine();
            if (ImGui::RadioButton(trc("config_mode.advanced"), isAdvanced)) {
                g_config.basicModeEnabled = false;
                g_configIsDirty = true;
            }
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        {
            static std::string s_renameBuffer;
            static std::string s_newProfileName;
            static float s_renameColor[3] = { kDefaultProfileColor[0], kDefaultProfileColor[1], kDefaultProfileColor[2] };

            static GLuint s_iconAdd = 0, s_iconDuplicate = 0, s_iconRename = 0, s_iconDelete = 0;
            static HGLRC s_iconLastCtx = NULL;
            {
                HGLRC ctx = wglGetCurrentContext();
                if (ctx != s_iconLastCtx) { s_iconAdd = s_iconDuplicate = s_iconRename = s_iconDelete = 0; s_iconLastCtx = ctx; }
            }
            LoadEmbeddedResourceTexture(s_iconAdd, IDR_ICON_ADD, GL_NEAREST);
            LoadEmbeddedResourceTexture(s_iconDuplicate, IDR_ICON_DUPLICATE, GL_NEAREST);
            LoadEmbeddedResourceTexture(s_iconRename, IDR_ICON_RENAME, GL_NEAREST);
            LoadEmbeddedResourceTexture(s_iconDelete, IDR_ICON_DELETE, GL_NEAREST);

            float iconSz = ImGui::GetFrameHeight() * 0.75f;
            auto iconBtnStyle = []() {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1, 1, 1, 0.1f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1, 1, 1, 0.2f));
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
            };
            auto iconBtnStylePop = []() {
                ImGui::PopStyleVar();
                ImGui::PopStyleColor(3);
            };

            float profileButtonsWidth = (iconSz + 6) * 4 + ImGui::GetStyle().ItemSpacing.x * 4;
            float labelWidth = ImGui::CalcTextSize(trc("profiles.label")).x + ImGui::GetStyle().ItemSpacing.x;
            float availableForCombo = headerRightStartX - ImGui::GetCursorPosX() - labelWidth - profileButtonsWidth;
            if (availableForCombo < 60) availableForCombo = 60;

            ImGui::TextUnformatted(trc("profiles.label"));
            ImGui::SameLine();
            ImGui::SetNextItemWidth((std::min)(150.0f, availableForCombo));
            float* activeColor = nullptr;
            for (auto& pm : g_profilesConfig.profiles) {
                if (pm.name == g_profilesConfig.activeProfile) { activeColor = pm.color; break; }
            }
            if (activeColor) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(activeColor[0], activeColor[1], activeColor[2], 1.0f));
            if (ImGui::BeginCombo("##profileSelector", g_profilesConfig.activeProfile.c_str())) {
                if (activeColor) ImGui::PopStyleColor();
                for (const auto& pm : g_profilesConfig.profiles) {
                    bool selected = (pm.name == g_profilesConfig.activeProfile);
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(pm.color[0], pm.color[1], pm.color[2], 1.0f));
                    if (ImGui::Selectable(pm.name.c_str(), selected)) {
                        if (pm.name != g_profilesConfig.activeProfile) {
                            SwitchProfile(pm.name);
                        }
                    }
                    ImGui::PopStyleColor();
                    if (selected) ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            } else {
                if (activeColor) ImGui::PopStyleColor();
            }

            if (s_iconAdd != 0) {
                ImGui::SameLine();
                iconBtnStyle();
                if (ImGui::ImageButton("##profileNew", (ImTextureID)(intptr_t)s_iconAdd, ImVec2(iconSz, iconSz))) {
                    s_newProfileName = "New Profile";
                    ImGui::OpenPopup(trc("profiles.new_popup"));
                }
                iconBtnStylePop();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", trc("profiles.tooltip.new"));
            }

            if (s_iconDuplicate != 0) {
                ImGui::SameLine();
                iconBtnStyle();
                if (ImGui::ImageButton("##profileDup", (ImTextureID)(intptr_t)s_iconDuplicate, ImVec2(iconSz, iconSz))) {
                    std::string base = g_profilesConfig.activeProfile + " " + tr("profiles.copy_suffix");
                    std::string newName = base;
                    for (int i = 2; !DuplicateProfile(g_profilesConfig.activeProfile, newName); i++) {
                        newName = base + " " + std::to_string(i);
                        if (i > 99) break;
                    }
                }
                iconBtnStylePop();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", trc("profiles.tooltip.duplicate"));
            }

            if (s_iconRename != 0) {
                ImGui::SameLine();
                iconBtnStyle();
                if (ImGui::ImageButton("##profileRen", (ImTextureID)(intptr_t)s_iconRename, ImVec2(iconSz, iconSz))) {
                    s_renameBuffer = g_profilesConfig.activeProfile;
                    for (const auto& pm : g_profilesConfig.profiles) {
                        if (pm.name == g_profilesConfig.activeProfile) {
                            s_renameColor[0] = pm.color[0];
                            s_renameColor[1] = pm.color[1];
                            s_renameColor[2] = pm.color[2];
                            break;
                        }
                    }
                    ImGui::OpenPopup(trc("profiles.rename_popup"));
                }
                iconBtnStylePop();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", trc("profiles.tooltip.rename"));
            }

            if (s_iconDelete != 0) {
                ImGui::SameLine();
                ImGui::BeginDisabled(g_profilesConfig.profiles.size() <= 1);
                iconBtnStyle();
                if (ImGui::ImageButton("##profileDel", (ImTextureID)(intptr_t)s_iconDelete, ImVec2(iconSz, iconSz))) {
                    ImGui::OpenPopup(trc("profiles.delete_popup"));
                }
                iconBtnStylePop();
                ImGui::EndDisabled();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", trc("profiles.tooltip.delete"));
            }

            if (ImGui::BeginPopup(trc("profiles.new_popup"))) {
                ImGui::InputText("##newProfileName", &s_newProfileName);
                bool nameValid = IsValidProfileName(s_newProfileName);
                if (!s_newProfileName.empty() && !nameValid)
                    ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", trc("profiles.invalid_name"));
                ImGui::BeginDisabled(!nameValid);
                if (ImGui::Button(trc("button.ok"), ImVec2(80, 0))) {
                    if (CreateNewProfile(s_newProfileName)) {
                        SwitchProfile(s_newProfileName);
                    }
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button(trc("button.cancel"), ImVec2(80, 0))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            if (ImGui::BeginPopup(trc("profiles.rename_popup"))) {
                ImGui::InputText("##renameProfileName", &s_renameBuffer);
                const ProfileMetadata* activeProfile = nullptr;
                for (const auto& pm : g_profilesConfig.profiles) {
                    if (pm.name == g_profilesConfig.activeProfile) {
                        activeProfile = &pm;
                        break;
                    }
                }

                const bool nameChanged = s_renameBuffer != g_profilesConfig.activeProfile;
                const bool nameValid = IsValidProfileName(s_renameBuffer);
                const bool colorChanged = activeProfile != nullptr &&
                    (activeProfile->color[0] != s_renameColor[0] ||
                     activeProfile->color[1] != s_renameColor[1] ||
                     activeProfile->color[2] != s_renameColor[2]);
                const bool renameValid = (nameChanged ? nameValid : true) && (nameChanged || colorChanged);

                if (nameChanged && !nameValid)
                    ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", trc("profiles.invalid_name"));
                ImGui::ColorEdit3(trc("profiles.color"), s_renameColor, ImGuiColorEditFlags_NoInputs);
                ImGui::BeginDisabled(!renameValid);
                if (ImGui::Button(trc("button.ok"), ImVec2(80, 0))) {
                    if (UpdateProfileMetadata(g_profilesConfig.activeProfile, s_renameBuffer, s_renameColor)) {
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button(trc("button.cancel"), ImVec2(80, 0))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            if (ImGui::BeginPopup(trc("profiles.delete_popup"))) {
                std::string toDelete = g_profilesConfig.activeProfile;
                std::string switchTo;
                for (const auto& pm : g_profilesConfig.profiles) {
                    if (pm.name != toDelete) { switchTo = pm.name; break; }
                }
                ImGui::Text("%s", tr("profiles.confirm_delete", toDelete).c_str());
                if (!switchTo.empty())
                    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "%s", tr("profiles.switch_to", switchTo).c_str());
                if (ImGui::Button(trc("button.ok"), ImVec2(80, 0))) {
                    if (!switchTo.empty()) {
                        SwitchProfile(switchTo);
                        DeleteProfile(toDelete);
                    } else {
                        Log("DeleteProfile: no fallback profile found");
                    }
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button(trc("button.cancel"), ImVec2(80, 0))) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
        }

        ImGui::Separator();

        // Drag modes must only be enabled by the currently active tab.
        g_imageDragMode.store(false, std::memory_order_relaxed);
        g_windowOverlayDragMode.store(false, std::memory_order_relaxed);
        g_browserOverlayDragMode.store(false, std::memory_order_relaxed);

        {
            PROFILE_SCOPE_CAT("Settings Tabs", "ImGui");

            if (g_config.basicModeEnabled) {
                if (ImGui::BeginTabBar("BasicSettingsTabs")) {
#include "tabs/tab_basic_general.inl"
#include "tabs/tab_basic_other.inl"

#include "tabs/tab_supporters.inl"

                    ImGui::EndTabBar();
                }
            } else {
                if (ImGui::BeginTabBar("SettingsTabs")) {
#include "tabs/tab_modes.inl"
#include "tabs/tab_overlays.inl"
#include "tabs/tab_hotkeys.inl"
#include "tabs/tab_inputs.inl"
#include "tabs/tab_settings.inl"

#include "tabs/tab_appearance.inl"

#include "tabs/tab_misc.inl"

#include "tabs/tab_supporters.inl"

                    ImGui::EndTabBar();
                }
            }
        }

    } else if (windowOpen) {
        ResetGuiTransientInteractionState();
    }
    ImGui::End();

    SaveConfig();

    // Ensure config snapshot is published for reader threads after GUI mutations.
    // update to prevent reader threads from seeing stale/freed vector data.
    if (g_configIsDirty.load()) { PublishConfigSnapshot(); }
}

