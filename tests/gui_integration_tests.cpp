#include "common/i18n.h"
#include "common/utils.h"
#include "gui/gui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"
#include "runtime/logic_thread.h"

#include <GL/glew.h>
#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
extern std::atomic<bool> g_configLoaded;

namespace {

constexpr int kWindowWidth = 1600;
constexpr int kWindowHeight = 900;
constexpr wchar_t kWindowClassName[] = L"ToolscreenGuiIntegrationTestWindow";

void Expect(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::string Narrow(const std::wstring& value) {
    return WideToUtf8(value);
}

LRESULT CALLBACK TestWindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, message, wParam, lParam)) {
        return TRUE;
    }
    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void RegisterWindowClass() {
    static const ATOM windowClass = []() {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.style = CS_OWNDC;
        wc.lpfnWndProc = TestWindowProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kWindowClassName;
        return RegisterClassExW(&wc);
    }();

    Expect(windowClass != 0, "Failed to register GUI integration test window class.");
}

class ScopedTabSelection {
  public:
    ScopedTabSelection(const char* topLevelTabLabel, const char* inputsSubTabLabel) {
        SetGuiTabSelectionOverride(topLevelTabLabel, inputsSubTabLabel);
    }

    ~ScopedTabSelection() { ClearGuiTabSelectionOverride(); }
};

class DummyWindow {
  public:
    DummyWindow(int width, int height) : m_width(width), m_height(height) {
        RegisterWindowClass();

        m_hwnd = CreateWindowExW(0, kWindowClassName, L"Toolscreen GUI Integration Tests", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
                                 CW_USEDEFAULT, width, height, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        Expect(m_hwnd != nullptr, "Failed to create GUI integration test window.");

        m_hdc = GetDC(m_hwnd);
        Expect(m_hdc != nullptr, "Failed to acquire device context for GUI integration test window.");

        PIXELFORMATDESCRIPTOR pfd{};
        pfd.nSize = sizeof(pfd);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 32;
        pfd.cDepthBits = 24;
        pfd.cStencilBits = 8;
        pfd.iLayerType = PFD_MAIN_PLANE;

        const int pixelFormat = ChoosePixelFormat(m_hdc, &pfd);
        Expect(pixelFormat != 0, "Failed to choose a pixel format for GUI integration test window.");
        Expect(SetPixelFormat(m_hdc, pixelFormat, &pfd) == TRUE,
               "Failed to set the pixel format for GUI integration test window.");

        m_glContext = wglCreateContext(m_hdc);
        Expect(m_glContext != nullptr, "Failed to create an OpenGL context for GUI integration tests.");
        Expect(wglMakeCurrent(m_hdc, m_glContext) == TRUE, "Failed to activate the OpenGL context for GUI integration tests.");

        glewExperimental = GL_TRUE;
        const GLenum glewStatus = glewInit();
        glGetError();
        Expect(glewStatus == GLEW_OK,
               "Failed to initialize GLEW for GUI integration tests: " + std::string(reinterpret_cast<const char*>(glewGetErrorString(glewStatus))));

        ShowWindow(m_hwnd, SW_HIDE);
        UpdateCachedWindowMetricsFromSize(m_width, m_height);
        g_minecraftHwnd.store(m_hwnd, std::memory_order_release);
    }

    ~DummyWindow() {
        ClearGuiTabSelectionOverride();
        HandleImGuiContextReset();

        if (wglGetCurrentContext() == m_glContext) {
            wglMakeCurrent(nullptr, nullptr);
        }
        if (m_glContext != nullptr) {
            wglDeleteContext(m_glContext);
        }
        if (m_hwnd != nullptr && m_hdc != nullptr) {
            ReleaseDC(m_hwnd, m_hdc);
        }
        if (m_hwnd != nullptr) {
            DestroyWindow(m_hwnd);
        }

        g_minecraftHwnd.store(nullptr, std::memory_order_release);
    }

    HWND hwnd() const { return m_hwnd; }

    void PumpMessages() {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    void BeginFrame() {
        PumpMessages();
        InitializeImGuiContext(m_hwnd);

        glViewport(0, 0, m_width, m_height);
        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        SyncImGuiDisplayMetrics(m_hwnd);
        ApplyDynamicGuiFontRefresh();
        ImGui::NewFrame();
    }

    void EndFrame() {
        ImGui::Render();

        const ImDrawData* drawData = ImGui::GetDrawData();
        Expect(drawData != nullptr, "Expected ImGui draw data to exist after rendering.");
        Expect(drawData->CmdListsCount > 0, "Expected rendered GUI to contain at least one command list.");
        Expect(drawData->TotalVtxCount > 0, "Expected rendered GUI to contain at least one vertex.");

        RenderImGuiWithStateProtection(true);
        Expect(SwapBuffers(m_hdc) == TRUE, "SwapBuffers failed for GUI integration test window.");
        PumpMessages();
    }

  private:
    HWND m_hwnd = nullptr;
    HDC m_hdc = nullptr;
    HGLRC m_glContext = nullptr;
    int m_width = 0;
    int m_height = 0;
};

std::filesystem::path PrepareCaseDirectory(std::string_view caseName) {
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "toolscreen_gui_integration" / std::filesystem::path(caseName);

    std::error_code error;
    std::filesystem::remove_all(root, error);
    error.clear();
    std::filesystem::create_directories(root, error);
    Expect(!error, "Failed to create test directory: " + Narrow(root.wstring()));

    return root;
}

void ResetGlobalTestState(const std::filesystem::path& root) {
    g_toolscreenPath = root.wstring();
    g_modeFilePath = (root / "mode.txt").wstring();
    g_stateFilePath = (root / "state.txt").wstring();

    g_config = Config();
    g_configIsDirty.store(false, std::memory_order_release);
    g_configLoadFailed.store(false, std::memory_order_release);
    g_configLoaded.store(false, std::memory_order_release);
    g_showGui.store(true, std::memory_order_release);
    g_guiNeedsRecenter.store(true, std::memory_order_release);
    g_welcomeToastVisible.store(false, std::memory_order_release);
    g_configurePromptDismissedThisSession.store(false, std::memory_order_release);
    g_screenshotRequested.store(false, std::memory_order_release);

    {
        std::lock_guard<std::mutex> lock(g_modeIdMutex);
        g_currentModeId.clear();
        g_modeIdBuffers[0].clear();
        g_modeIdBuffers[1].clear();
    }
    g_currentModeIdIndex.store(0, std::memory_order_release);

    {
        std::lock_guard<std::mutex> lock(g_configErrorMutex);
        g_configLoadError.clear();
    }

    ClearGuiTabSelectionOverride();
    LoadLangs();
    Expect(!GetLangs().empty(), "Expected embedded language metadata to load.");
    Expect(LoadTranslation("en"), "Expected embedded English translations to load.");
    UpdateCachedWindowMetricsFromSize(kWindowWidth, kWindowHeight);
}

void RenderSettingsFrame(DummyWindow& window, const char* topLevelTabLabel, const char* inputsSubTabLabel = nullptr) {
    ScopedTabSelection scopedSelection(topLevelTabLabel, inputsSubTabLabel);
    window.BeginFrame();
    RenderSettingsGUI();
    window.EndFrame();
}

void RenderConfigErrorFrame(DummyWindow& window) {
    window.PumpMessages();
    InitializeImGuiContext(window.hwnd());
    HandleConfigLoadFailed(nullptr, nullptr);

    const ImDrawData* drawData = ImGui::GetDrawData();
    Expect(drawData != nullptr, "Expected config-error rendering path to produce ImGui draw data.");
    Expect(drawData->DisplaySize.x > 0.0f && drawData->DisplaySize.y > 0.0f,
           "Expected config-error rendering path to populate a valid ImGui display size.");
}

void RunConfigDefaultLoadTest() {
    DummyWindow window(kWindowWidth, kWindowHeight);
    const std::filesystem::path root = PrepareCaseDirectory("config_default_load");
    ResetGlobalTestState(root);

    LoadConfig();

    const std::filesystem::path configPath = root / "config.toml";
    Expect(std::filesystem::exists(configPath), "Expected LoadConfig to create config.toml when it is missing.");
    Expect(!g_configLoadFailed.load(std::memory_order_acquire), "Expected default config load to succeed.");
    Expect(g_configLoaded.load(std::memory_order_acquire), "Expected default config load to mark configuration as ready.");
    Expect(!g_config.modes.empty(), "Expected loaded config to contain at least one mode.");
    Expect(!g_config.hotkeys.empty(), "Expected loaded config to contain at least one hotkey.");
    Expect(!g_config.defaultMode.empty(), "Expected loaded config to have a default mode.");
}

void RunConfigRoundtripTest() {
    DummyWindow window(kWindowWidth, kWindowHeight);
    const std::filesystem::path root = PrepareCaseDirectory("config_roundtrip");
    ResetGlobalTestState(root);

    LoadConfig();
    Expect(!g_configLoadFailed.load(std::memory_order_acquire), "Expected initial config load to succeed before roundtrip.");

    g_config.lang = "zh_CN";
    g_config.mouseSensitivity = 1.75f;
    g_config.basicModeEnabled = true;
    g_config.disableConfigurePrompt = true;
    g_configIsDirty.store(true, std::memory_order_release);
    SaveConfigImmediate();

    g_config = Config();
    g_configLoadFailed.store(false, std::memory_order_release);
    g_configLoaded.store(false, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lock(g_configErrorMutex);
        g_configLoadError.clear();
    }
    {
        std::lock_guard<std::mutex> lock(g_modeIdMutex);
        g_currentModeId.clear();
        g_modeIdBuffers[0].clear();
        g_modeIdBuffers[1].clear();
    }
    g_currentModeIdIndex.store(0, std::memory_order_release);

    LoadConfig();

    Expect(!g_configLoadFailed.load(std::memory_order_acquire), "Expected config reload to succeed after roundtrip save.");
    Expect(g_config.lang == "zh_CN", "Expected saved language to roundtrip through config.toml.");
    Expect(std::abs(g_config.mouseSensitivity - 1.75f) < 0.0001f,
           "Expected saved mouse sensitivity to roundtrip through config.toml.");
    Expect(g_config.basicModeEnabled, "Expected saved basic-mode flag to roundtrip through config.toml.");
    Expect(g_config.disableConfigurePrompt, "Expected saved configure-prompt flag to roundtrip through config.toml.");
}

void RunConfigErrorGuiTest() {
    DummyWindow window(kWindowWidth, kWindowHeight);
    const std::filesystem::path root = PrepareCaseDirectory("config_error_gui");
    ResetGlobalTestState(root);

    const std::filesystem::path configPath = root / "config.toml";
    std::ofstream out(configPath, std::ios::binary | std::ios::trunc);
    Expect(out.is_open(), "Failed to open config.toml for invalid-config test setup.");
    out << "configVersion = 4\n[[modes]]\nid = \"Fullscreen\"\nwidth =\n";
    out.close();

    LoadConfig();

    Expect(g_configLoadFailed.load(std::memory_order_acquire), "Expected invalid TOML to mark config loading as failed.");
    {
        std::lock_guard<std::mutex> lock(g_configErrorMutex);
        Expect(!g_configLoadError.empty(), "Expected invalid TOML to populate a config error message.");
    }

    RenderConfigErrorFrame(window);
}

void RunSettingsGuiBasicTest() {
    DummyWindow window(kWindowWidth, kWindowHeight);
    const std::filesystem::path root = PrepareCaseDirectory("settings_gui_basic");
    ResetGlobalTestState(root);

    LoadConfig();
    Expect(!g_configLoadFailed.load(std::memory_order_acquire), "Expected config load to succeed before basic GUI rendering.");

    g_config.basicModeEnabled = true;
    g_configIsDirty.store(false, std::memory_order_release);

    const std::vector<std::string> tabs = {
        tr("tabs.general"),
        tr("tabs.other"),
        tr("tabs.supporters"),
    };

    for (const std::string& tab : tabs) {
        RenderSettingsFrame(window, tab.c_str());
    }
}

void RunSettingsGuiAdvancedTest() {
    DummyWindow window(kWindowWidth, kWindowHeight);
    const std::filesystem::path root = PrepareCaseDirectory("settings_gui_advanced");
    ResetGlobalTestState(root);

    LoadConfig();
    Expect(!g_configLoadFailed.load(std::memory_order_acquire), "Expected config load to succeed before advanced GUI rendering.");

    g_config.basicModeEnabled = false;
    g_configIsDirty.store(false, std::memory_order_release);

    const std::string inputsTab = tr("tabs.inputs");
    const std::string mouseSubTab = tr("inputs.mouse");
    const std::string keyboardSubTab = tr("inputs.keyboard");

    const std::vector<std::string> tabs = {
        tr("tabs.modes"),
        tr("tabs.mirrors"),
        tr("tabs.images"),
        tr("tabs.window_overlays"),
        tr("tabs.browser_overlays"),
        tr("tabs.hotkeys"),
        inputsTab,
        tr("tabs.settings"),
        tr("tabs.appearance"),
        tr("tabs.misc"),
        tr("tabs.supporters"),
    };

    for (const std::string& tab : tabs) {
        if (tab == inputsTab) {
            RenderSettingsFrame(window, tab.c_str(), mouseSubTab.c_str());
            RenderSettingsFrame(window, tab.c_str(), keyboardSubTab.c_str());
            continue;
        }

        RenderSettingsFrame(window, tab.c_str());
    }
}

} // namespace

int main(int argc, char** argv) {
    try {
        Expect(argc == 2, "Expected exactly one test case argument.");

        const std::string testCase = argv[1];
        if (testCase == "config-default-load") {
            RunConfigDefaultLoadTest();
        } else if (testCase == "config-roundtrip") {
            RunConfigRoundtripTest();
        } else if (testCase == "config-error-gui") {
            RunConfigErrorGuiTest();
        } else if (testCase == "settings-gui-basic") {
            RunSettingsGuiBasicTest();
        } else if (testCase == "settings-gui-advanced") {
            RunSettingsGuiAdvancedTest();
        } else {
            throw std::runtime_error("Unknown test case: " + testCase);
        }

        std::cout << "PASS " << testCase << std::endl;
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "FAIL: " << ex.what() << std::endl;
        return 1;
    }
}