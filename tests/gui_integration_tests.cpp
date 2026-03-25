#include "common/i18n.h"
#include "common/utils.h"
#include "config/config_toml.h"
#include "features/browser_overlay.h"
#include "features/window_overlay.h"
#include "gui/gui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"
#include "render/render.h"
#include "runtime/logic_thread.h"

#include <GL/glew.h>
#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdint>
#include <cmath>
#include <stdexcept>
#include <array>
#include <string>
#include <string_view>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
extern std::atomic<bool> g_configLoaded;

namespace {

constexpr int kWindowWidth = 1600;
constexpr int kWindowHeight = 900;
constexpr wchar_t kWindowClassName[] = L"ToolscreenGuiIntegrationTestWindow";
constexpr char kDefaultVisualTestCase[] = "settings-gui-advanced";

enum class TestRunMode {
    Automated,
    Visual,
};

using SetProcessDpiAwarenessContextFn = BOOL(WINAPI*)(DPI_AWARENESS_CONTEXT value);
using SetProcessDPIAwareFn = BOOL(WINAPI*)();

void EnsureProcessDpiAwareness() {
    static const bool configured = []() {
        if (HMODULE user32 = GetModuleHandleW(L"user32.dll")) {
            if (auto setDpiAwarenessContext = reinterpret_cast<SetProcessDpiAwarenessContextFn>(
                    GetProcAddress(user32, "SetProcessDpiAwarenessContext"))) {
                if (setDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)) {
                    return true;
                }

                if (setDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE)) {
                    return true;
                }
            }

            if (auto setProcessDpiAware = reinterpret_cast<SetProcessDPIAwareFn>(GetProcAddress(user32, "SetProcessDPIAware"))) {
                if (setProcessDpiAware()) {
                    return true;
                }
            }
        }

        return false;
    }();

    (void)configured;
}

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

    if (message == WM_CLOSE) {
        DestroyWindow(hwnd);
        return 0;
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
        DummyWindow(int width, int height, bool visible = false) : m_width(width), m_height(height) {
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

        m_previousHdc = wglGetCurrentDC();
        m_previousGlContext = wglGetCurrentContext();

        m_glContext = wglCreateContext(m_hdc);
        Expect(m_glContext != nullptr, "Failed to create an OpenGL context for GUI integration tests.");
        Expect(wglMakeCurrent(m_hdc, m_glContext) == TRUE, "Failed to activate the OpenGL context for GUI integration tests.");

        glewExperimental = GL_TRUE;
        const GLenum glewStatus = glewInit();
        glGetError();
        Expect(glewStatus == GLEW_OK,
               "Failed to initialize GLEW for GUI integration tests: " + std::string(reinterpret_cast<const char*>(glewGetErrorString(glewStatus))));

        RefreshClientSize();
        ShowWindow(m_hwnd, visible ? SW_SHOW : SW_HIDE);
        if (visible) {
            UpdateWindow(m_hwnd);
            SetForegroundWindow(m_hwnd);
            SetFocus(m_hwnd);
        }
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
        if (m_previousGlContext != nullptr && m_previousHdc != nullptr) {
            wglMakeCurrent(m_previousHdc, m_previousGlContext);
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

    bool isOpen() const { return m_isOpen; }

    void SetTitle(const std::string& title) {
        if (m_hwnd != nullptr) {
            SetWindowTextW(m_hwnd, Utf8ToWide(title).c_str());
        }
    }

    void Show(bool visible) {
        if (m_hwnd == nullptr) {
            return;
        }

        ShowWindow(m_hwnd, visible ? SW_SHOW : SW_HIDE);
        if (visible) {
            UpdateWindow(m_hwnd);
            SetForegroundWindow(m_hwnd);
            SetFocus(m_hwnd);
        }
    }

    bool PumpMessages() {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
                m_isOpen = false;
                return false;
            }

            TranslateMessage(&message);
            DispatchMessageW(&message);
        }

        m_isOpen = m_hwnd != nullptr && IsWindow(m_hwnd) != FALSE;
        return m_isOpen;
    }

    bool PrepareRenderSurface() {
        if (!PumpMessages()) {
            return false;
        }

        Expect(MakeCurrent(), "Failed to reactivate the OpenGL context for GUI integration test window.");

        InitializeImGuiContext(m_hwnd);
        RefreshClientSize();

        glViewport(0, 0, m_width, m_height);
        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        return true;
    }

    bool BeginFrame() {
        if (!PrepareRenderSurface()) {
            return false;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        SyncImGuiDisplayMetrics(m_hwnd);
        ApplyDynamicGuiFontRefresh();
        ImGui::NewFrame();
        return true;
    }

    void EndFrame() {
        ImGui::Render();
        ValidateDrawData();
        RenderImGuiWithStateProtection(true);
        PresentSurface();
    }

    void PresentSurface() {
        Expect(SwapBuffers(m_hdc) == TRUE, "SwapBuffers failed for GUI integration test window.");
        PumpMessages();
    }

    bool MakeCurrent() const {
        if (m_hdc == nullptr || m_glContext == nullptr) {
            return false;
        }

        if (wglGetCurrentContext() == m_glContext) {
            return true;
        }

        return wglMakeCurrent(m_hdc, m_glContext) == TRUE;
    }

  private:
    void RefreshClientSize() {
        RECT clientRect{};
        if (m_hwnd == nullptr || GetClientRect(m_hwnd, &clientRect) == FALSE) {
            return;
        }

        const int clientWidth = clientRect.right - clientRect.left;
        const int clientHeight = clientRect.bottom - clientRect.top;
        if (clientWidth <= 0 || clientHeight <= 0) {
            return;
        }

        m_width = clientWidth;
        m_height = clientHeight;
        UpdateCachedWindowMetricsFromSize(m_width, m_height);
    }

    static void ValidateDrawData() {
        const ImDrawData* drawData = ImGui::GetDrawData();
        Expect(drawData != nullptr, "Expected ImGui draw data to exist after rendering.");
        Expect(drawData->CmdListsCount > 0, "Expected rendered GUI to contain at least one command list.");
        Expect(drawData->TotalVtxCount > 0, "Expected rendered GUI to contain at least one vertex.");
    }

    HWND m_hwnd = nullptr;
    HDC m_hdc = nullptr;
    HGLRC m_glContext = nullptr;
    HDC m_previousHdc = nullptr;
    HGLRC m_previousGlContext = nullptr;
    int m_width = 0;
    int m_height = 0;
    bool m_isOpen = true;
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
    Expect(window.BeginFrame(), "GUI integration test window closed unexpectedly.");
    RenderSettingsGUI();
    window.EndFrame();
}

void RenderInteractiveSettingsFrame(DummyWindow& window) {
    if (!window.BeginFrame()) {
        return;
    }

    RenderSettingsGUI();
    window.EndFrame();
}

void RenderConfigErrorFrame(DummyWindow& window, bool presentFrame = false) {
    Expect(window.PrepareRenderSurface(), "GUI integration test window closed unexpectedly.");
    HandleConfigLoadFailed(nullptr, nullptr);

    const ImDrawData* drawData = ImGui::GetDrawData();
    Expect(drawData != nullptr, "Expected config-error rendering path to produce ImGui draw data.");
    Expect(drawData->DisplaySize.x > 0.0f && drawData->DisplaySize.y > 0.0f,
           "Expected config-error rendering path to populate a valid ImGui display size.");

    if (presentFrame) {
        window.PresentSurface();
    }
}

void RenderInteractiveConfigErrorFrame(DummyWindow& window) {
    RenderConfigErrorFrame(window, true);
}

template <typename RenderFrameFn>
void RunVisualLoop(DummyWindow& window, std::string_view testCaseName, RenderFrameFn&& renderFrame);

constexpr char kPrimaryModeId[] = "Primary Mode";
constexpr char kPrecisionModeId[] = "Precision Mode";
constexpr char kRelativeModeId[] = "Relative Mode";
constexpr char kExpressionModeId[] = "Expression Mode";
constexpr char kVerifierMirrorName[] = "Verifier Mirror";
constexpr char kAuxMirrorName[] = "Aux Mirror";
constexpr char kVerifierGroupName[] = "Verifier Group";
constexpr char kVerifierImageName[] = "Checklist Image";
constexpr char kVerifierWindowOverlayName[] = "Verifier Window";
constexpr char kVerifierBrowserOverlayName[] = "Verifier Browser";

bool NearlyEqual(float actual, float expected, float epsilon = 0.0001f) {
    return std::fabs(actual - expected) <= epsilon;
}

void ExpectFloatNear(float actual, float expected, const std::string& message, float epsilon = 0.0001f) {
    Expect(NearlyEqual(actual, expected, epsilon), message + " (expected " + std::to_string(expected) + ", got " +
                                             std::to_string(actual) + ")");
}

void ExpectColorNear(const Color& actual, const Color& expected, const std::string& message, float epsilon = 0.005f) {
    ExpectFloatNear(actual.r, expected.r, message + " [r]", epsilon);
    ExpectFloatNear(actual.g, expected.g, message + " [g]", epsilon);
    ExpectFloatNear(actual.b, expected.b, message + " [b]", epsilon);
    ExpectFloatNear(actual.a, expected.a, message + " [a]", epsilon);
}

bool IsColorNear(const Color& actual, const Color& expected, float epsilon = 0.02f) {
    return NearlyEqual(actual.r, expected.r, epsilon) && NearlyEqual(actual.g, expected.g, epsilon) &&
           NearlyEqual(actual.b, expected.b, epsilon) && NearlyEqual(actual.a, expected.a, epsilon);
}

std::vector<unsigned char> MakeSolidRgbaPixels(int width, int height, std::uint8_t r, std::uint8_t g, std::uint8_t b,
                                               std::uint8_t a = 255) {
    const size_t byteCount = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
    std::vector<unsigned char> pixels(byteCount);
    for (size_t i = 0; i < byteCount; i += 4) {
        pixels[i + 0] = r;
        pixels[i + 1] = g;
        pixels[i + 2] = b;
        pixels[i + 3] = a;
    }
    return pixels;
}

Color ReadFramebufferPixelColor(int screenX, int screenY, int surfaceHeight) {
    std::array<unsigned char, 4> pixel{ 0, 0, 0, 0 };
    glReadPixels(screenX, surfaceHeight - screenY - 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
    return {
        static_cast<float>(pixel[0]) / 255.0f,
        static_cast<float>(pixel[1]) / 255.0f,
        static_cast<float>(pixel[2]) / 255.0f,
        static_cast<float>(pixel[3]) / 255.0f,
    };
}

void ExpectFramebufferPixelColorNear(int screenX, int screenY, int surfaceHeight, const Color& expected,
                                     const std::string& message, float epsilon = 0.02f) {
    glFinish();
    ExpectColorNear(ReadFramebufferPixelColor(screenX, screenY, surfaceHeight), expected, message, epsilon);
}

void ExpectFramebufferNeighborhoodContainsColor(int screenX, int screenY, int radius, int surfaceWidth, int surfaceHeight,
                                                const Color& expected, const std::string& message,
                                                float epsilon = 0.02f) {
    glFinish();

    const int minX = (std::max)(0, screenX - radius);
    const int maxX = (std::min)(surfaceWidth - 1, screenX + radius);
    const int minY = (std::max)(0, screenY - radius);
    const int maxY = (std::min)(surfaceHeight - 1, screenY + radius);
    for (int y = minY; y <= maxY; ++y) {
        for (int x = minX; x <= maxX; ++x) {
            if (IsColorNear(ReadFramebufferPixelColor(x, y, surfaceHeight), expected, epsilon)) {
                return;
            }
        }
    }

    throw std::runtime_error(message);
}

struct SurfaceSize {
    int width = 0;
    int height = 0;
};

struct SimulatedOverlayGeometry {
    std::string label;
    int fullW = 1;
    int fullH = 1;
    int gameX = 0;
    int gameY = 0;
    int gameW = 1;
    int gameH = 1;
};

struct MirrorAnchorCaseDefinition {
    std::string name;
    std::string relativeTo;
    int captureWidth = 1;
    int captureHeight = 1;
    int outputX = 0;
    int outputY = 0;
    float scale = 1.0f;
};

struct MirrorAnchorRenderScenario {
    SimulatedOverlayGeometry geometry;
    std::vector<MirrorAnchorCaseDefinition> mirrors;
    bool expectVisibleRender = true;
};

class ScopedFramebufferSurface {
  public:
    ScopedFramebufferSurface(int width, int height) : m_width((std::max)(1, width)), m_height((std::max)(1, height)) {
        GLint previousTexture = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);

        glGenTextures(1, &m_colorTexture);
        Expect(m_colorTexture != 0, "Failed to create offscreen texture for GUI integration tests.");
        BindTextureDirect(GL_TEXTURE_2D, m_colorTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        BindTextureDirect(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));

        GLint previousReadFramebuffer = 0;
        GLint previousDrawFramebuffer = 0;
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousReadFramebuffer);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDrawFramebuffer);

        glGenFramebuffers(1, &m_framebuffer);
        Expect(m_framebuffer != 0, "Failed to create offscreen framebuffer for GUI integration tests.");
        glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTexture, 0);
        const GLenum framebufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previousReadFramebuffer));
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(previousDrawFramebuffer));

        Expect(framebufferStatus == GL_FRAMEBUFFER_COMPLETE,
               "Offscreen framebuffer for GUI integration tests was incomplete.");
    }

    ~ScopedFramebufferSurface() {
        if (m_restoreStateCaptured) {
            glBindFramebuffer(GL_READ_FRAMEBUFFER, m_previousReadFramebuffer);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_previousDrawFramebuffer);
            glViewport(m_previousViewport[0], m_previousViewport[1], m_previousViewport[2], m_previousViewport[3]);
        }

        if (m_framebuffer != 0) {
            glDeleteFramebuffers(1, &m_framebuffer);
        }
        if (m_colorTexture != 0) {
            glDeleteTextures(1, &m_colorTexture);
        }
    }

    void BindAndClear() {
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &m_previousReadFramebuffer);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &m_previousDrawFramebuffer);
        glGetIntegerv(GL_VIEWPORT, m_previousViewport);
        m_restoreStateCaptured = true;

        glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
        glViewport(0, 0, m_width, m_height);
        glClearColor(0.08f, 0.08f, 0.10f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    SurfaceSize size() const { return { m_width, m_height }; }

  private:
    GLuint m_framebuffer = 0;
    GLuint m_colorTexture = 0;
    int m_width = 0;
    int m_height = 0;
    GLint m_previousReadFramebuffer = 0;
    GLint m_previousDrawFramebuffer = 0;
    GLint m_previousViewport[4]{ 0, 0, 0, 0 };
    bool m_restoreStateCaptured = false;
};

struct ExpectedMirrorRect {
    std::string name;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

constexpr Color kExpectedMirrorRenderGreen{ 0.0f, 1.0f, 0.0f, 1.0f };
constexpr Color kExpectedRenderSurfaceClear{ 0.08f, 0.08f, 0.10f, 1.0f };

bool EndsWith(std::string_view value, std::string_view suffix) {
    return value.size() >= suffix.size() && value.substr(value.size() - suffix.size()) == suffix;
}

SurfaceSize GetWindowClientSize(HWND hwnd) {
    RECT clientRect{};
    Expect(hwnd != nullptr && GetClientRect(hwnd, &clientRect) == TRUE, "Failed to read GUI integration test window client rect.");
    return {
        clientRect.right - clientRect.left,
        clientRect.bottom - clientRect.top,
    };
}

class ScopedTexture2D {
  public:
    ScopedTexture2D(int width, int height, const std::vector<unsigned char>& rgbaPixels) {
        Expect(width > 0 && height > 0, "Texture dimensions must be positive.");
        Expect(rgbaPixels.size() == static_cast<size_t>(width) * static_cast<size_t>(height) * 4u,
               "Texture pixel payload size did not match the requested dimensions.");

        glGenTextures(1, &m_textureId);
        Expect(m_textureId != 0, "Failed to create OpenGL texture for mirror integration test.");

        GLint previousTexture = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);

        BindTextureDirect(GL_TEXTURE_2D, m_textureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgbaPixels.data());
        BindTextureDirect(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
    }

    ~ScopedTexture2D() {
        if (m_textureId != 0) {
            glDeleteTextures(1, &m_textureId);
        }
    }

    GLuint id() const { return m_textureId; }

  private:
    GLuint m_textureId = 0;
};

MirrorConfig MakeMirrorRenderTestConfig(std::string_view name, int captureWidth, int captureHeight, std::string_view relativeTo,
                                        int outputX, int outputY, float scale) {
    MirrorConfig mirror;
    mirror.name = std::string(name);
    mirror.captureWidth = captureWidth;
    mirror.captureHeight = captureHeight;
    mirror.input = { { 0, 0, "topLeftScreen" } };
    mirror.output.x = outputX;
    mirror.output.y = outputY;
    mirror.output.scale = scale;
    mirror.output.relativeTo = std::string(relativeTo);
    mirror.border.type = MirrorBorderType::Dynamic;
    mirror.border.dynamicThickness = 0;
    mirror.fps = 0;
    mirror.opacity = 1.0f;
    mirror.rawOutput = true;
    mirror.colorPassthrough = false;
    mirror.gradientOutput = false;
    return mirror;
}

MirrorConfig BuildExpectedGroupedMirrorConfig(const MirrorConfig& mirror, const MirrorGroupConfig& group, const MirrorGroupItem& item) {
    MirrorConfig groupedMirror = mirror;
    groupedMirror.output.x = group.output.x + item.offsetX;
    groupedMirror.output.y = group.output.y + item.offsetY;
    groupedMirror.output.relativeTo = group.output.relativeTo;
    groupedMirror.output.useRelativePosition = group.output.useRelativePosition;
    groupedMirror.output.relativeX = group.output.relativeX;
    groupedMirror.output.relativeY = group.output.relativeY;

    groupedMirror.output.separateScale = true;
    const float baseScaleX = mirror.output.separateScale ? mirror.output.scaleX : mirror.output.scale;
    const float baseScaleY = mirror.output.separateScale ? mirror.output.scaleY : mirror.output.scale;
    groupedMirror.output.scaleX = baseScaleX * item.widthPercent;
    groupedMirror.output.scaleY = baseScaleY * item.heightPercent;
    return groupedMirror;
}

ExpectedMirrorRect ComputeExpectedMirrorRect(const MirrorConfig& mirror, int fullW, int fullH, int gameX, int gameY, int gameW, int gameH) {
    const float scaleX = mirror.output.separateScale ? mirror.output.scaleX : mirror.output.scale;
    const float scaleY = mirror.output.separateScale ? mirror.output.scaleY : mirror.output.scale;

    ExpectedMirrorRect rect;
    rect.name = mirror.name;
    rect.width = static_cast<int>(static_cast<float>(mirror.captureWidth) * scaleX);
    rect.height = static_cast<int>(static_cast<float>(mirror.captureHeight) * scaleY);

    std::string anchor = mirror.output.relativeTo;
    bool isScreenRelative = false;
    if (EndsWith(anchor, "Screen")) {
        anchor.resize(anchor.size() - std::string_view("Screen").size());
        isScreenRelative = true;
    } else if (EndsWith(anchor, "Viewport")) {
        anchor.resize(anchor.size() - std::string_view("Viewport").size());
    }

    const int containerX = isScreenRelative ? 0 : gameX;
    const int containerY = isScreenRelative ? 0 : gameY;
    const int containerW = isScreenRelative ? fullW : gameW;
    const int containerH = isScreenRelative ? fullH : gameH;

    if (anchor == "topLeft") {
        rect.x = containerX + mirror.output.x;
        rect.y = containerY + mirror.output.y;
    } else if (anchor == "topRight") {
        rect.x = containerX + containerW - rect.width - mirror.output.x;
        rect.y = containerY + mirror.output.y;
    } else if (anchor == "bottomLeft") {
        rect.x = containerX + mirror.output.x;
        rect.y = containerY + containerH - rect.height - mirror.output.y;
    } else if (anchor == "bottomRight") {
        rect.x = containerX + containerW - rect.width - mirror.output.x;
        rect.y = containerY + containerH - rect.height - mirror.output.y;
    } else if (anchor == "center") {
        rect.x = containerX + (containerW - rect.width) / 2 + mirror.output.x;
        rect.y = containerY + (containerH - rect.height) / 2 + mirror.output.y;
    } else {
        throw std::runtime_error("Unsupported mirror test anchor: " + mirror.output.relativeTo);
    }

    return rect;
}

ExpectedMirrorRect GetCachedMirrorRect(std::string_view mirrorName) {
    std::shared_lock<std::shared_mutex> lock(g_mirrorInstancesMutex);
    auto it = g_mirrorInstances.find(std::string(mirrorName));
    Expect(it != g_mirrorInstances.end(), "Missing cached mirror instance for integration test: " + std::string(mirrorName));
    Expect(it->second.cachedRenderState.isValid, "Missing cached mirror render state for integration test: " + std::string(mirrorName));
    return {
        it->first,
        it->second.cachedRenderState.mirrorScreenX,
        it->second.cachedRenderState.mirrorScreenY,
        it->second.cachedRenderState.mirrorScreenW,
        it->second.cachedRenderState.mirrorScreenH,
    };
}

    void ExpectMirrorRectNear(const ExpectedMirrorRect& actual, const ExpectedMirrorRect& expected, const std::string& label,
                     int tolerance = 2) {
        Expect(std::abs(actual.x - expected.x) <= tolerance,
            label + " x was outside tolerance. expected " + std::to_string(expected.x) + ", got " + std::to_string(actual.x));
        Expect(std::abs(actual.y - expected.y) <= tolerance,
            label + " y was outside tolerance. expected " + std::to_string(expected.y) + ", got " + std::to_string(actual.y));
        Expect(std::abs(actual.width - expected.width) <= tolerance,
            label + " width was outside tolerance. expected " + std::to_string(expected.width) + ", got " +
             std::to_string(actual.width));
        Expect(std::abs(actual.height - expected.height) <= tolerance,
            label + " height was outside tolerance. expected " + std::to_string(expected.height) + ", got " +
             std::to_string(actual.height));
    }

void ExpectSolidColorRect(const ExpectedMirrorRect& rect, int surfaceHeight, const Color& expected, const std::string& label) {
    Expect(rect.width > 0 && rect.height > 0, label + " must have positive dimensions.");

    const int insetX = rect.width > 4 ? rect.width / 5 : 0;
    const int insetY = rect.height > 4 ? rect.height / 5 : 0;
    const int left = rect.x + insetX;
    const int right = rect.x + rect.width - 1 - insetX;
    const int top = rect.y + insetY;
    const int bottom = rect.y + rect.height - 1 - insetY;
    const int centerX = rect.x + rect.width / 2;
    const int centerY = rect.y + rect.height / 2;

    ExpectFramebufferPixelColorNear(centerX, centerY, surfaceHeight, expected, label + " should match at center.");
    ExpectFramebufferPixelColorNear(left, top, surfaceHeight, expected, label + " should match at top-left interior sample.");
    ExpectFramebufferPixelColorNear(right, top, surfaceHeight, expected, label + " should match at top-right interior sample.");
    ExpectFramebufferPixelColorNear(left, bottom, surfaceHeight, expected, label + " should match at bottom-left interior sample.");
    ExpectFramebufferPixelColorNear(right, bottom, surfaceHeight, expected, label + " should match at bottom-right interior sample.");
}

void ExpectBackgroundPixel(int screenX, int screenY, int surfaceHeight, const std::string& label) {
    ExpectFramebufferPixelColorNear(screenX, screenY, surfaceHeight, kExpectedRenderSurfaceClear, label);
}

void ResetOverlayRenderTestResources();

void ExpectMirrorRenderMatchesExpectedPlacement(const MirrorConfig& mirror, const SimulatedOverlayGeometry& geometry,
                                                const SurfaceSize& surface, const std::string& label,
                                                bool expectVisibleRender) {
    const ExpectedMirrorRect expectedRect = ComputeExpectedMirrorRect(mirror, surface.width, surface.height, geometry.gameX,
                                                                      geometry.gameY, geometry.gameW, geometry.gameH);

    Expect(expectedRect.width > 0 && expectedRect.height > 0,
           label + " should resolve to a positive mirror size on the simulated surface.");
    Expect(expectedRect.x >= 0 && expectedRect.y >= 0 && expectedRect.x + expectedRect.width <= surface.width &&
               expectedRect.y + expectedRect.height <= surface.height,
           label + " should remain fully inside the simulated surface.");

    (void)expectVisibleRender;
}

void InitializeMirrorRenderTestResources() {
    ResetOverlayRenderTestResources();
    for (const auto& mirror : g_config.mirrors) {
        CreateMirrorGPUResources(mirror);
    }
}

void ResetOverlayRenderTestResources() {
    InvalidateConfigLookupCaches();
    g_windowOverlaysVisible.store(true, std::memory_order_release);
    g_browserOverlaysVisible.store(true, std::memory_order_release);
    CleanupBrowserOverlayCache();
    CleanupWindowOverlayCache();
    CleanupGPUResources();
    CleanupShaders();
    InitializeGPUResources();
}

void RenderModeOverlayFrame(DummyWindow& window, const Config& config, const ModeConfig& mode, GLuint gameTextureId = 0) {
    Expect(window.PrepareRenderSurface(), "GUI integration test window closed unexpectedly.");

    GLState state{};
    SaveGLState(&state);

    const int surfaceWidth = (std::max)(1, GetCachedWindowWidth());
    const int surfaceHeight = (std::max)(1, GetCachedWindowHeight());
    const bool rendered = RenderModeOverlaysForIntegrationTest(config, mode, state, surfaceWidth, surfaceHeight, 0, 0,
                                                               surfaceWidth, surfaceHeight, false, gameTextureId);
    Expect(rendered, "Expected mode overlay render path to produce overlay output.");
}

template <typename AssertFn>
void RenderModeOverlayFrameToSimulatedSurface(DummyWindow& window, const Config& config, const ModeConfig& mode,
                                              const SimulatedOverlayGeometry& geometry, GLuint gameTextureId,
                                              AssertFn&& assertFn) {
    Expect(window.PrepareRenderSurface(), "GUI integration test window closed unexpectedly.");

    ScopedFramebufferSurface surface(geometry.fullW, geometry.fullH);
    surface.BindAndClear();

    GLState state{};
    SaveGLState(&state);

    const bool rendered = RenderModeOverlaysForIntegrationTest(config, mode, state, (std::max)(1, geometry.fullW),
                                                               (std::max)(1, geometry.fullH), geometry.gameX, geometry.gameY,
                                                               (std::max)(1, geometry.gameW), (std::max)(1, geometry.gameH),
                                                               false, gameTextureId);
    Expect(rendered, "Expected simulated mode overlay render path to produce overlay output.");

    glFinish();
    assertFn(surface.size());
}

template <typename AssertFn>
void RenderModeOverlayFrameWithGeometry(DummyWindow& window, const Config& config, const ModeConfig& mode,
                                        const SimulatedOverlayGeometry& geometry, GLuint gameTextureId,
                                        AssertFn&& assertFn) {
    Expect(window.PrepareRenderSurface(), "GUI integration test window closed unexpectedly.");

    const SurfaceSize surface = GetWindowClientSize(window.hwnd());

    GLState state{};
    SaveGLState(&state);

    const bool rendered = RenderModeOverlaysForIntegrationTest(config, mode, state, surface.width, surface.height,
                                                               geometry.gameX, geometry.gameY, geometry.gameW,
                                                               geometry.gameH, false, gameTextureId);
    Expect(rendered, "Expected window-backed mode overlay render path to produce overlay output.");

    glFinish();
    assertFn(surface);
}

template <typename T>
void ExpectVectorEquals(const std::vector<T>& actual, const std::vector<T>& expected, const std::string& message) {
    Expect(actual == expected, message);
}

std::filesystem::path GetCurrentConfigPath() {
    return std::filesystem::path(g_toolscreenPath) / "config.toml";
}

void ResetRuntimeStateForReload() {
    g_config = Config();
    g_configIsDirty.store(false, std::memory_order_release);
    g_configLoadFailed.store(false, std::memory_order_release);
    g_configLoaded.store(false, std::memory_order_release);
    g_showGui.store(true, std::memory_order_release);
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

    ResetTransientBindingUiState();
}

void ReloadConfigFromDisk() {
    ResetRuntimeStateForReload();
    LoadConfig();
}

void SaveAndReloadCurrentConfig() {
    g_configIsDirty.store(true, std::memory_order_release);
    SaveConfigImmediate();
    ReloadConfigFromDisk();
}

void WriteConfigFixtureToDisk(const Config& config) {
    g_config = config;
    g_configIsDirty.store(true, std::memory_order_release);
    SaveConfigImmediate();
    Expect(std::filesystem::exists(GetCurrentConfigPath()), "Failed to write config fixture to disk.");
}

void WriteRawConfigTomlToDisk(std::string_view tomlText) {
    const std::filesystem::path configPath = GetCurrentConfigPath();
    std::ofstream out(configPath, std::ios::binary | std::ios::trunc);
    Expect(out.is_open(), "Failed to open config fixture for raw TOML write.");
    out << tomlText;
    out.close();
    Expect(std::filesystem::exists(configPath), "Failed to write raw config fixture to disk.");
}

void ExpectConfigLoadSucceeded(const std::string& context) {
    std::string loadError;
    {
        std::lock_guard<std::mutex> lock(g_configErrorMutex);
        loadError = g_configLoadError;
    }

    Expect(!g_configLoadFailed.load(std::memory_order_acquire),
           context + " should load without config errors." + (loadError.empty() ? std::string() : (" Error: " + loadError)));
    Expect(g_configLoaded.load(std::memory_order_acquire), context + " should mark the config as loaded.");
}

const ModeConfig& FindModeOrThrow(std::string_view modeId) {
    for (const auto& mode : g_config.modes) {
        if (EqualsIgnoreCase(mode.id, std::string(modeId))) {
            return mode;
        }
    }

    throw std::runtime_error("Missing mode: " + std::string(modeId));
}

const MirrorConfig& FindMirrorOrThrow(std::string_view mirrorName) {
    for (const auto& mirror : g_config.mirrors) {
        if (EqualsIgnoreCase(mirror.name, std::string(mirrorName))) {
            return mirror;
        }
    }

    throw std::runtime_error("Missing mirror: " + std::string(mirrorName));
}

const MirrorGroupConfig& FindMirrorGroupOrThrow(std::string_view groupName) {
    for (const auto& group : g_config.mirrorGroups) {
        if (EqualsIgnoreCase(group.name, std::string(groupName))) {
            return group;
        }
    }

    throw std::runtime_error("Missing mirror group: " + std::string(groupName));
}

const ImageConfig& FindImageOrThrow(std::string_view imageName) {
    for (const auto& image : g_config.images) {
        if (EqualsIgnoreCase(image.name, std::string(imageName))) {
            return image;
        }
    }

    throw std::runtime_error("Missing image: " + std::string(imageName));
}

const WindowOverlayConfig& FindWindowOverlayOrThrow(std::string_view overlayName) {
    for (const auto& overlay : g_config.windowOverlays) {
        if (EqualsIgnoreCase(overlay.name, std::string(overlayName))) {
            return overlay;
        }
    }

    throw std::runtime_error("Missing window overlay: " + std::string(overlayName));
}

const BrowserOverlayConfig& FindBrowserOverlayOrThrow(std::string_view overlayName) {
    for (const auto& overlay : g_config.browserOverlays) {
        if (EqualsIgnoreCase(overlay.name, std::string(overlayName))) {
            return overlay;
        }
    }

    throw std::runtime_error("Missing browser overlay: " + std::string(overlayName));
}

const HotkeyConfig& FindHotkeyBySecondaryModeOrThrow(std::string_view secondaryModeId) {
    for (const auto& hotkey : g_config.hotkeys) {
        if (EqualsIgnoreCase(hotkey.secondaryMode, std::string(secondaryModeId))) {
            return hotkey;
        }
    }

    throw std::runtime_error("Missing hotkey for mode: " + std::string(secondaryModeId));
}

const HotkeyConfig& FindHotkeyByKeysOrThrow(const std::vector<DWORD>& keys) {
    for (const auto& hotkey : g_config.hotkeys) {
        if (hotkey.keys == keys) {
            return hotkey;
        }
    }

    throw std::runtime_error("Missing hotkey fixture.");
}

const SensitivityHotkeyConfig& FindSensitivityHotkeyOrThrow(const std::vector<DWORD>& keys) {
    for (const auto& hotkey : g_config.sensitivityHotkeys) {
        if (hotkey.keys == keys) {
            return hotkey;
        }
    }

    throw std::runtime_error("Missing sensitivity hotkey fixture.");
}

void PopulateRichConfigFixture() {
    g_config.lang = "zh_CN";
    g_config.fontPath = "C:\\Windows\\Fonts\\consola.ttf";
    g_config.fpsLimit = 144;
    g_config.fpsLimitSleepThreshold = 7;
    g_config.mirrorGammaMode = MirrorGammaMode::AssumeLinear;
    g_config.disableHookChaining = true;
    g_config.allowCursorEscape = true;
    g_config.mouseSensitivity = 1.75f;
    g_config.windowsMouseSpeed = 13;
    g_config.hideAnimationsInGame = true;
    g_config.limitCaptureFramerate = false;
    g_config.obsFramerate = 73;
    g_config.keyRepeatStartDelay = 275;
    g_config.keyRepeatDelay = 42;
    g_config.basicModeEnabled = false;
    g_config.restoreWindowedModeOnFullscreenExit = false;
    g_config.disableFullscreenPrompt = true;
    g_config.disableConfigurePrompt = true;
    g_config.guiHotkey = { VK_CONTROL, VK_SHIFT, 'G' };
    g_config.borderlessHotkey = { VK_MENU, VK_RETURN };
    g_config.autoBorderless = true;
    g_config.imageOverlaysHotkey = { VK_F8 };
    g_config.windowOverlaysHotkey = { VK_F7 };

    g_config.debug.showPerformanceOverlay = true;
    g_config.debug.showProfiler = true;
    g_config.debug.profilerScale = 1.25f;
    g_config.debug.showHotkeyDebug = true;
    g_config.debug.fakeCursor = true;
    g_config.debug.showTextureGrid = true;
    g_config.debug.delayRenderingUntilFinished = true;
    g_config.debug.virtualCameraEnabled = true;
    g_config.debug.logModeSwitch = true;
    g_config.debug.logAnimation = true;
    g_config.debug.logHotkey = true;
    g_config.debug.logObs = true;
    g_config.debug.logWindowOverlay = true;
    g_config.debug.logFileMonitor = true;
    g_config.debug.logImageMonitor = true;
    g_config.debug.logPerformance = true;
    g_config.debug.logTextureOps = true;
    g_config.debug.logGui = true;
    g_config.debug.logInit = true;
    g_config.debug.logCursorTextures = true;

    g_config.cursors.enabled = true;
    g_config.cursors.title.cursorName = "title.cur";
    g_config.cursors.title.cursorSize = 64;
    g_config.cursors.wall.cursorName = "wall.cur";
    g_config.cursors.wall.cursorSize = 72;
    g_config.cursors.ingame.cursorName = "ingame.cur";
    g_config.cursors.ingame.cursorSize = 88;

    g_config.eyezoom.cloneWidth = 28;
    g_config.eyezoom.overlayWidth = 9;
    g_config.eyezoom.cloneHeight = 1500;
    g_config.eyezoom.stretchWidth = 600;
    g_config.eyezoom.windowWidth = 420;
    g_config.eyezoom.windowHeight = 9000;
    g_config.eyezoom.zoomAreaWidth = 222;
    g_config.eyezoom.zoomAreaHeight = 777;
    g_config.eyezoom.useCustomSizePosition = true;
    g_config.eyezoom.positionX = 33;
    g_config.eyezoom.positionY = 44;
    g_config.eyezoom.fontSizeMode = EyeZoomFontSizeMode::Manual;
    g_config.eyezoom.textFontSize = 31;
    g_config.eyezoom.textFontPath = "C:\\Windows\\Fonts\\verdana.ttf";
    g_config.eyezoom.rectHeight = 35;
    g_config.eyezoom.linkRectToFont = false;
    g_config.eyezoom.gridColor1 = { 0.2f, 0.4f, 0.6f, 1.0f };
    g_config.eyezoom.gridColor1Opacity = 0.8f;
    g_config.eyezoom.gridColor2 = { 0.6f, 0.3f, 0.2f, 1.0f };
    g_config.eyezoom.gridColor2Opacity = 0.7f;
    g_config.eyezoom.centerLineColor = { 0.9f, 0.8f, 0.1f, 1.0f };
    g_config.eyezoom.centerLineColorOpacity = 0.65f;
    g_config.eyezoom.textColor = { 0.1f, 0.2f, 0.3f, 1.0f };
    g_config.eyezoom.textColorOpacity = 0.95f;
    g_config.eyezoom.slideZoomIn = true;
    g_config.eyezoom.slideMirrorsIn = true;
    g_config.eyezoom.overlays = {
        { "Overlay One", "C:\\temp\\overlay-one.png", EyeZoomOverlayDisplayMode::Fit, 100, 100, false, 0.5f },
        { "Overlay Two", "C:\\temp\\overlay-two.png", EyeZoomOverlayDisplayMode::Manual, 240, 140, false, 0.85f },
    };
    g_config.eyezoom.activeOverlayIndex = 1;

    g_config.keyRebinds.enabled = true;
    g_config.keyRebinds.resolveRebindTargetsForHotkeys = false;
    g_config.keyRebinds.toggleHotkey = { VK_F4 };
    KeyRebind rebind;
    rebind.fromKey = 'J';
    rebind.toKey = 'K';
    rebind.enabled = true;
    rebind.useCustomOutput = true;
    rebind.customOutputVK = 'L';
    rebind.customOutputUnicode = 0x00F8;
    rebind.customOutputScanCode = 0x26;
    rebind.baseOutputShifted = true;
    rebind.shiftLayerEnabled = true;
    rebind.shiftLayerOutputVK = 'P';
    rebind.shiftLayerOutputUnicode = 0x00D8;
    rebind.shiftLayerOutputShifted = true;
    g_config.keyRebinds.rebinds = { rebind };

    g_config.appearance.theme = "Sunrise";
    g_config.appearance.customColors["WindowBg"] = { 0.1f, 0.11f, 0.12f, 1.0f };
    g_config.appearance.customColors["Header"] = { 0.8f, 0.4f, 0.2f, 1.0f };

    MirrorConfig verifierMirror;
    verifierMirror.name = kVerifierMirrorName;
    verifierMirror.captureWidth = 73;
    verifierMirror.captureHeight = 41;
    verifierMirror.input = {
        { 10, 20, "centerViewport" },
        { -5, 8, "topLeftScreen" },
    };
    verifierMirror.output.x = 30;
    verifierMirror.output.y = -40;
    verifierMirror.output.useRelativePosition = true;
    verifierMirror.output.relativeX = 0.25f;
    verifierMirror.output.relativeY = 0.75f;
    verifierMirror.output.scale = 1.25f;
    verifierMirror.output.separateScale = true;
    verifierMirror.output.scaleX = 0.9f;
    verifierMirror.output.scaleY = 1.1f;
    verifierMirror.output.relativeTo = "centerViewport";
    verifierMirror.colors.targetColors = {
        { 1.0f, 0.0f, 0.0f, 1.0f },
        { 0.0f, 1.0f, 0.0f, 1.0f },
    };
    verifierMirror.colors.output = { 0.1f, 0.2f, 0.3f, 0.4f };
    verifierMirror.colors.border = { 0.7f, 0.8f, 0.2f, 1.0f };
    verifierMirror.colorSensitivity = 0.123f;
    verifierMirror.border.type = MirrorBorderType::Static;
    verifierMirror.border.staticShape = MirrorBorderShape::Circle;
    verifierMirror.border.staticColor = { 0.4f, 0.5f, 0.6f, 1.0f };
    verifierMirror.border.staticThickness = 5;
    verifierMirror.border.staticRadius = 18;
    verifierMirror.border.staticOffsetX = 7;
    verifierMirror.border.staticOffsetY = -4;
    verifierMirror.border.staticWidth = 91;
    verifierMirror.border.staticHeight = 87;
    verifierMirror.fps = 48;
    verifierMirror.opacity = 0.654f;
    verifierMirror.rawOutput = true;
    verifierMirror.colorPassthrough = true;
    verifierMirror.gradientOutput = true;
    verifierMirror.gradient.gradientStops = {
        { { 0.2f, 0.1f, 0.8f, 1.0f }, 0.0f },
        { { 0.8f, 0.9f, 0.2f, 1.0f }, 1.0f },
    };
    verifierMirror.gradient.gradientAngle = 42.0f;
    verifierMirror.gradient.gradientAnimation = GradientAnimationType::Rotate;
    verifierMirror.gradient.gradientAnimationSpeed = 1.7f;
    verifierMirror.gradient.gradientColorFade = true;
    verifierMirror.onlyOnMyScreen = true;

    MirrorConfig auxMirror;
    auxMirror.name = kAuxMirrorName;
    auxMirror.captureWidth = 88;
    auxMirror.captureHeight = 66;
    auxMirror.input = { { 3, 4, "centerViewport" } };
    auxMirror.output.x = 12;
    auxMirror.output.y = 18;
    auxMirror.output.scale = 0.95f;
    auxMirror.output.relativeTo = "topLeftScreen";
    auxMirror.fps = 30;
    auxMirror.opacity = 0.95f;

    g_config.mirrors.push_back(verifierMirror);
    g_config.mirrors.push_back(auxMirror);

    MirrorGroupConfig group;
    group.name = kVerifierGroupName;
    group.output.x = 15;
    group.output.y = 25;
    group.output.useRelativePosition = true;
    group.output.relativeX = 0.5f;
    group.output.relativeY = 0.4f;
    group.output.separateScale = true;
    group.output.scaleX = 1.3f;
    group.output.scaleY = 0.7f;
    group.output.relativeTo = "topLeftScreen";
    group.mirrors = {
        { kVerifierMirrorName, true, 0.6f, 0.4f, 10, -10 },
        { kAuxMirrorName, false, 0.4f, 0.6f, -20, 30 },
    };
    g_config.mirrorGroups.push_back(group);

    ImageConfig image;
    image.name = kVerifierImageName;
    image.path = "C:\\temp\\checklist.png";
    image.x = 11;
    image.y = 22;
    image.scale = 1.6f;
    image.relativeSizing = false;
    image.width = 320;
    image.height = 180;
    image.relativeTo = "centerViewport";
    image.crop_top = 3;
    image.crop_bottom = 4;
    image.crop_left = 5;
    image.crop_right = 6;
    image.enableColorKey = true;
    image.colorKeys = {
        { { 1.0f, 0.0f, 1.0f, 1.0f }, 0.02f },
        { { 0.0f, 1.0f, 1.0f, 1.0f }, 0.03f },
    };
    image.opacity = 0.81f;
    image.background.enabled = true;
    image.background.color = { 0.2f, 0.3f, 0.4f, 1.0f };
    image.background.opacity = 0.33f;
    image.pixelatedScaling = true;
    image.onlyOnMyScreen = true;
    image.border.enabled = true;
    image.border.color = { 0.9f, 0.5f, 0.1f, 1.0f };
    image.border.width = 7;
    image.border.radius = 12;
    g_config.images.push_back(image);

    WindowOverlayConfig windowOverlay;
    windowOverlay.name = kVerifierWindowOverlayName;
    windowOverlay.windowTitle = "Speedrun Timer";
    windowOverlay.windowClass = "TimerClass";
    windowOverlay.executableName = "Timer.exe";
    windowOverlay.windowMatchPriority = "title_executable";
    windowOverlay.x = -30;
    windowOverlay.y = 45;
    windowOverlay.scale = 1.2f;
    windowOverlay.relativeTo = "centerViewport";
    windowOverlay.crop_top = 4;
    windowOverlay.crop_bottom = 5;
    windowOverlay.crop_left = 6;
    windowOverlay.crop_right = 7;
    windowOverlay.enableColorKey = true;
    windowOverlay.colorKeys = {
        { { 0.1f, 0.9f, 0.1f, 1.0f }, 0.05f },
    };
    windowOverlay.opacity = 0.71f;
    windowOverlay.background.enabled = true;
    windowOverlay.background.color = { 0.05f, 0.06f, 0.07f, 1.0f };
    windowOverlay.background.opacity = 0.4f;
    windowOverlay.pixelatedScaling = true;
    windowOverlay.onlyOnMyScreen = true;
    windowOverlay.fps = 27;
    windowOverlay.searchInterval = 2500;
    windowOverlay.captureMethod = "BitBlt";
    windowOverlay.forceUpdate = true;
    windowOverlay.enableInteraction = true;
    windowOverlay.border.enabled = true;
    windowOverlay.border.color = { 0.4f, 0.7f, 0.9f, 1.0f };
    windowOverlay.border.width = 3;
    windowOverlay.border.radius = 8;
    g_config.windowOverlays.push_back(windowOverlay);

    BrowserOverlayConfig browserOverlay;
    browserOverlay.name = kVerifierBrowserOverlayName;
    browserOverlay.url = "https://example.com/dashboard";
    browserOverlay.customCss = "body { background: transparent; }";
    browserOverlay.browserWidth = 1024;
    browserOverlay.browserHeight = 576;
    browserOverlay.x = 9;
    browserOverlay.y = 19;
    browserOverlay.scale = 1.15f;
    browserOverlay.relativeTo = "centerViewport";
    browserOverlay.crop_top = 8;
    browserOverlay.crop_bottom = 7;
    browserOverlay.crop_left = 6;
    browserOverlay.crop_right = 5;
    browserOverlay.enableColorKey = true;
    browserOverlay.colorKeys = {
        { { 0.9f, 0.2f, 0.2f, 1.0f }, 0.04f },
    };
    browserOverlay.opacity = 0.67f;
    browserOverlay.background.enabled = true;
    browserOverlay.background.color = { 0.1f, 0.11f, 0.12f, 1.0f };
    browserOverlay.background.opacity = 0.44f;
    browserOverlay.pixelatedScaling = true;
    browserOverlay.onlyOnMyScreen = true;
    browserOverlay.fps = 25;
    browserOverlay.transparentBackground = true;
    browserOverlay.muteAudio = false;
    browserOverlay.allowSystemMediaKeys = false;
    browserOverlay.reloadOnUpdate = true;
    browserOverlay.reloadInterval = 2500;
    browserOverlay.border.enabled = true;
    browserOverlay.border.color = { 0.8f, 0.3f, 0.2f, 1.0f };
    browserOverlay.border.width = 4;
    browserOverlay.border.radius = 10;
    g_config.browserOverlays.push_back(browserOverlay);

    ModeConfig primaryMode;
    primaryMode.id = kPrimaryModeId;
    primaryMode.width = 1280;
    primaryMode.height = 720;
    primaryMode.manualWidth = 1280;
    primaryMode.manualHeight = 720;
    primaryMode.background.selectedMode = "gradient";
    primaryMode.background.gradientStops = {
        { { 0.15f, 0.2f, 0.35f, 1.0f }, 0.0f },
        { { 0.7f, 0.4f, 0.2f, 1.0f }, 1.0f },
    };
    primaryMode.background.gradientAngle = 55.0f;
    primaryMode.background.gradientAnimation = GradientAnimationType::Slide;
    primaryMode.background.gradientAnimationSpeed = 1.3f;
    primaryMode.background.gradientColorFade = true;
    primaryMode.mirrorIds = { kVerifierMirrorName };
    primaryMode.mirrorGroupIds = { kVerifierGroupName };
    primaryMode.imageIds = { kVerifierImageName };
    primaryMode.windowOverlayIds = { kVerifierWindowOverlayName };
    primaryMode.browserOverlayIds = { kVerifierBrowserOverlayName };
    primaryMode.stretch.enabled = true;
    primaryMode.stretch.width = 1400;
    primaryMode.stretch.height = 800;
    primaryMode.stretch.x = 15;
    primaryMode.stretch.y = 25;
    primaryMode.gameTransition = GameTransitionType::Bounce;
    primaryMode.overlayTransition = OverlayTransitionType::Cut;
    primaryMode.backgroundTransition = BackgroundTransitionType::Cut;
    primaryMode.transitionDurationMs = 777;
    primaryMode.easeInPower = 2.5f;
    primaryMode.easeOutPower = 4.25f;
    primaryMode.bounceCount = 3;
    primaryMode.bounceIntensity = 0.42f;
    primaryMode.bounceDurationMs = 333;
    primaryMode.relativeStretching = true;
    primaryMode.skipAnimateX = true;
    primaryMode.skipAnimateY = false;
    primaryMode.border.enabled = true;
    primaryMode.border.color = { 0.3f, 0.4f, 0.5f, 1.0f };
    primaryMode.border.width = 9;
    primaryMode.border.radius = 14;
    primaryMode.sensitivityOverrideEnabled = true;
    primaryMode.modeSensitivity = 0.88f;
    primaryMode.separateXYSensitivity = true;
    primaryMode.modeSensitivityX = 0.91f;
    primaryMode.modeSensitivityY = 0.72f;
    primaryMode.slideMirrorsIn = true;
    g_config.modes.push_back(primaryMode);

    ModeConfig precisionMode;
    precisionMode.id = kPrecisionModeId;
    precisionMode.width = 960;
    precisionMode.height = 540;
    precisionMode.manualWidth = 960;
    precisionMode.manualHeight = 540;
    precisionMode.background.selectedMode = "color";
    precisionMode.background.color = { 0.02f, 0.03f, 0.04f, 1.0f };
    precisionMode.mirrorIds = { kAuxMirrorName };
    g_config.modes.push_back(precisionMode);

    g_config.defaultMode = kPrimaryModeId;

    HotkeyConfig hotkey;
    hotkey.keys = { VK_F6, 'Q' };
    hotkey.mainMode = "Fullscreen";
    hotkey.secondaryMode = kPrimaryModeId;
    hotkey.altSecondaryModes = { { { 'E', 'R' }, kPrecisionModeId } };
    hotkey.conditions.gameState = { "ingame", "wall" };
    hotkey.conditions.exclusions = { VK_LSHIFT, VK_RBUTTON };
    hotkey.debounce = 220;
    hotkey.triggerOnRelease = true;
    hotkey.triggerOnHold = true;
    hotkey.blockKeyFromGame = true;
    hotkey.allowExitToFullscreenRegardlessOfGameState = true;
    g_config.hotkeys.push_back(hotkey);

    SensitivityHotkeyConfig sensitivityHotkey;
    sensitivityHotkey.keys = { VK_F9 };
    sensitivityHotkey.sensitivity = 0.55f;
    sensitivityHotkey.separateXY = true;
    sensitivityHotkey.sensitivityX = 0.75f;
    sensitivityHotkey.sensitivityY = 0.95f;
    sensitivityHotkey.toggle = true;
    sensitivityHotkey.conditions.gameState = { "menu" };
    sensitivityHotkey.conditions.exclusions = { VK_MENU };
    sensitivityHotkey.debounce = 350;
    g_config.sensitivityHotkeys.push_back(sensitivityHotkey);

    ResizeHotkeySecondaryModes(g_config.hotkeys.size());
    ResetAllHotkeySecondaryModes(g_config);
    {
        std::lock_guard<std::mutex> hotkeyLock(g_hotkeyMainKeysMutex);
        RebuildHotkeyMainKeys_Internal();
    }

    g_configIsDirty.store(true, std::memory_order_release);
}

void VerifyRichGlobalSettings() {
    Expect(g_config.lang == "zh_CN", "Expected language to roundtrip.");
    Expect(g_config.fontPath == "C:\\Windows\\Fonts\\consola.ttf", "Expected font path to roundtrip.");
    Expect(g_config.defaultMode == kPrimaryModeId, "Expected default mode to roundtrip.");
    Expect(g_config.fpsLimit == 144, "Expected fps limit to roundtrip.");
    Expect(g_config.fpsLimitSleepThreshold == 7, "Expected fps sleep threshold to roundtrip.");
    Expect(g_config.mirrorGammaMode == MirrorGammaMode::AssumeLinear, "Expected mirror gamma mode to roundtrip.");
    Expect(g_config.disableHookChaining, "Expected disableHookChaining to roundtrip.");
    Expect(g_config.allowCursorEscape, "Expected allowCursorEscape to roundtrip.");
    ExpectFloatNear(g_config.mouseSensitivity, 1.75f, "Expected mouse sensitivity to roundtrip.");
    Expect(g_config.windowsMouseSpeed == 13, "Expected Windows mouse speed to roundtrip.");
    Expect(g_config.hideAnimationsInGame, "Expected hideAnimationsInGame to roundtrip.");
    Expect(!g_config.limitCaptureFramerate, "Expected limitCaptureFramerate to roundtrip.");
    Expect(g_config.obsFramerate == 73, "Expected obsFramerate to roundtrip.");
    Expect(g_config.keyRepeatStartDelay == 275, "Expected keyRepeatStartDelay to roundtrip.");
    Expect(g_config.keyRepeatDelay == 42, "Expected keyRepeatDelay to roundtrip.");
    Expect(!g_config.basicModeEnabled, "Expected basicModeEnabled to roundtrip.");
    Expect(!g_config.restoreWindowedModeOnFullscreenExit, "Expected restoreWindowedModeOnFullscreenExit to roundtrip.");
    Expect(g_config.disableFullscreenPrompt, "Expected disableFullscreenPrompt to roundtrip.");
    Expect(g_config.disableConfigurePrompt, "Expected disableConfigurePrompt to roundtrip.");
    ExpectVectorEquals(g_config.guiHotkey, std::vector<DWORD>{ VK_CONTROL, VK_SHIFT, 'G' }, "Expected GUI hotkey to roundtrip.");
    ExpectVectorEquals(g_config.borderlessHotkey, std::vector<DWORD>{ VK_MENU, VK_RETURN }, "Expected borderless hotkey to roundtrip.");
    Expect(g_config.autoBorderless, "Expected autoBorderless to roundtrip.");
    ExpectVectorEquals(g_config.imageOverlaysHotkey, std::vector<DWORD>{ VK_F8 }, "Expected image overlay hotkey to roundtrip.");
    ExpectVectorEquals(g_config.windowOverlaysHotkey, std::vector<DWORD>{ VK_F7 }, "Expected window overlay hotkey to roundtrip.");

    std::string currentModeId;
    {
        std::lock_guard<std::mutex> lock(g_modeIdMutex);
        currentModeId = g_currentModeId;
    }
    Expect(EqualsIgnoreCase(currentModeId, kPrimaryModeId), "Expected LoadConfig to apply the saved default mode as current mode.");
}

void VerifyRichDebugSettings() {
    Expect(g_config.debug.showPerformanceOverlay, "Expected debug.showPerformanceOverlay to roundtrip.");
    Expect(g_config.debug.showProfiler, "Expected debug.showProfiler to roundtrip.");
    ExpectFloatNear(g_config.debug.profilerScale, 1.25f, "Expected debug.profilerScale to roundtrip.");
    Expect(g_config.debug.showHotkeyDebug, "Expected debug.showHotkeyDebug to roundtrip.");
    Expect(g_config.debug.fakeCursor, "Expected debug.fakeCursor to roundtrip.");
    Expect(g_config.debug.showTextureGrid, "Expected debug.showTextureGrid to roundtrip.");
    Expect(g_config.debug.delayRenderingUntilFinished, "Expected debug.delayRenderingUntilFinished to roundtrip.");
    Expect(g_config.debug.virtualCameraEnabled, "Expected debug.virtualCameraEnabled to roundtrip.");
    Expect(g_config.debug.logModeSwitch, "Expected debug.logModeSwitch to roundtrip.");
    Expect(g_config.debug.logAnimation, "Expected debug.logAnimation to roundtrip.");
    Expect(g_config.debug.logHotkey, "Expected debug.logHotkey to roundtrip.");
    Expect(g_config.debug.logObs, "Expected debug.logObs to roundtrip.");
    Expect(g_config.debug.logWindowOverlay, "Expected debug.logWindowOverlay to roundtrip.");
    Expect(g_config.debug.logFileMonitor, "Expected debug.logFileMonitor to roundtrip.");
    Expect(g_config.debug.logImageMonitor, "Expected debug.logImageMonitor to roundtrip.");
    Expect(g_config.debug.logPerformance, "Expected debug.logPerformance to roundtrip.");
    Expect(g_config.debug.logTextureOps, "Expected debug.logTextureOps to roundtrip.");
    Expect(g_config.debug.logGui, "Expected debug.logGui to roundtrip.");
    Expect(g_config.debug.logInit, "Expected debug.logInit to roundtrip.");
    Expect(g_config.debug.logCursorTextures, "Expected debug.logCursorTextures to roundtrip.");
}

void VerifyRichModes() {
        const ModeConfig& primaryMode = FindModeOrThrow(kPrimaryModeId);
        Expect(primaryMode.width == 1280, "Expected primary mode width to roundtrip.");
        Expect(primaryMode.height == 720, "Expected primary mode height to roundtrip.");
        Expect(primaryMode.manualWidth == 1280, "Expected primary mode manualWidth to roundtrip.");
        Expect(primaryMode.manualHeight == 720, "Expected primary mode manualHeight to roundtrip.");
        Expect(primaryMode.background.selectedMode == "gradient", "Expected primary mode background mode to roundtrip.");
        Expect(primaryMode.background.gradientStops.size() == 2, "Expected primary mode gradient stops to roundtrip.");
        ExpectColorNear(primaryMode.background.gradientStops[0].color, { 0.15f, 0.2f, 0.35f, 1.0f },
                  "Expected primary mode first gradient color to roundtrip.");
        ExpectFloatNear(primaryMode.background.gradientAngle, 55.0f, "Expected primary mode gradient angle to roundtrip.");
        Expect(primaryMode.background.gradientAnimation == GradientAnimationType::Slide,
            "Expected primary mode gradient animation to roundtrip.");
        ExpectFloatNear(primaryMode.background.gradientAnimationSpeed, 1.3f,
                  "Expected primary mode gradient animation speed to roundtrip.");
        Expect(primaryMode.background.gradientColorFade, "Expected primary mode gradient color fade to roundtrip.");
        ExpectVectorEquals(primaryMode.mirrorIds, std::vector<std::string>{ kVerifierMirrorName },
                  "Expected primary mode mirrorIds to roundtrip.");
        ExpectVectorEquals(primaryMode.mirrorGroupIds, std::vector<std::string>{ kVerifierGroupName },
                  "Expected primary mode mirrorGroupIds to roundtrip.");
        ExpectVectorEquals(primaryMode.imageIds, std::vector<std::string>{ kVerifierImageName },
                  "Expected primary mode imageIds to roundtrip.");
        ExpectVectorEquals(primaryMode.windowOverlayIds, std::vector<std::string>{ kVerifierWindowOverlayName },
                  "Expected primary mode windowOverlayIds to roundtrip.");
        ExpectVectorEquals(primaryMode.browserOverlayIds, std::vector<std::string>{ kVerifierBrowserOverlayName },
                  "Expected primary mode browserOverlayIds to roundtrip.");
        Expect(primaryMode.stretch.enabled, "Expected primary mode stretch.enabled to roundtrip.");
        Expect(primaryMode.stretch.width == 1400 && primaryMode.stretch.height == 800,
            "Expected primary mode stretch size to roundtrip.");
        Expect(primaryMode.stretch.x == 15 && primaryMode.stretch.y == 25,
            "Expected primary mode stretch position to roundtrip.");
        Expect(primaryMode.gameTransition == GameTransitionType::Bounce, "Expected primary mode game transition to roundtrip.");
        Expect(primaryMode.transitionDurationMs == 777, "Expected primary mode transitionDurationMs to roundtrip.");
        ExpectFloatNear(primaryMode.easeInPower, 2.5f, "Expected primary mode easeInPower to roundtrip.");
        ExpectFloatNear(primaryMode.easeOutPower, 4.25f, "Expected primary mode easeOutPower to roundtrip.");
        Expect(primaryMode.bounceCount == 3, "Expected primary mode bounceCount to roundtrip.");
        ExpectFloatNear(primaryMode.bounceIntensity, 0.42f, "Expected primary mode bounceIntensity to roundtrip.");
        Expect(primaryMode.bounceDurationMs == 333, "Expected primary mode bounceDurationMs to roundtrip.");
        Expect(primaryMode.relativeStretching, "Expected primary mode relativeStretching to roundtrip.");
        Expect(primaryMode.skipAnimateX, "Expected primary mode skipAnimateX to roundtrip.");
        Expect(!primaryMode.skipAnimateY, "Expected primary mode skipAnimateY to roundtrip.");
        Expect(primaryMode.border.enabled, "Expected primary mode border.enabled to roundtrip.");
        ExpectColorNear(primaryMode.border.color, { 0.3f, 0.4f, 0.5f, 1.0f }, "Expected primary mode border color to roundtrip.");
        Expect(primaryMode.border.width == 9 && primaryMode.border.radius == 14,
            "Expected primary mode border settings to roundtrip.");
        Expect(primaryMode.sensitivityOverrideEnabled, "Expected primary mode sensitivityOverrideEnabled to roundtrip.");
        ExpectFloatNear(primaryMode.modeSensitivity, 0.88f, "Expected primary mode modeSensitivity to roundtrip.");
        Expect(primaryMode.separateXYSensitivity, "Expected primary mode separateXYSensitivity to roundtrip.");
        ExpectFloatNear(primaryMode.modeSensitivityX, 0.91f, "Expected primary mode modeSensitivityX to roundtrip.");
        ExpectFloatNear(primaryMode.modeSensitivityY, 0.72f, "Expected primary mode modeSensitivityY to roundtrip.");
        Expect(primaryMode.slideMirrorsIn, "Expected primary mode slideMirrorsIn to roundtrip.");

    const ModeConfig& precisionMode = FindModeOrThrow(kPrecisionModeId);
    Expect(precisionMode.width == 960 && precisionMode.height == 540, "Expected precision mode dimensions to roundtrip.");
    ExpectVectorEquals(precisionMode.mirrorIds, std::vector<std::string>{ kAuxMirrorName },
                       "Expected precision mode to keep its mirror source.");
}

void VerifyRichMirrors() {
    const MirrorConfig& verifierMirror = FindMirrorOrThrow(kVerifierMirrorName);
    Expect(verifierMirror.captureWidth == 73 && verifierMirror.captureHeight == 41,
           "Expected verifier mirror capture dimensions to roundtrip.");
    Expect(verifierMirror.input.size() == 2, "Expected verifier mirror input zones to roundtrip.");
    Expect(verifierMirror.input[0].x == 10 && verifierMirror.input[0].y == 20 && verifierMirror.input[0].relativeTo == "centerViewport",
           "Expected verifier mirror first input zone to roundtrip.");
    Expect(verifierMirror.output.useRelativePosition, "Expected verifier mirror relative output positioning to roundtrip.");
    ExpectFloatNear(verifierMirror.output.relativeX, 0.25f, "Expected verifier mirror output.relativeX to roundtrip.");
    ExpectFloatNear(verifierMirror.output.relativeY, 0.75f, "Expected verifier mirror output.relativeY to roundtrip.");
    ExpectFloatNear(verifierMirror.output.scale, 1.25f, "Expected verifier mirror output.scale to roundtrip.");
    Expect(verifierMirror.output.separateScale, "Expected verifier mirror output.separateScale to roundtrip.");
    ExpectFloatNear(verifierMirror.output.scaleX, 0.9f, "Expected verifier mirror output.scaleX to roundtrip.");
    ExpectFloatNear(verifierMirror.output.scaleY, 1.1f, "Expected verifier mirror output.scaleY to roundtrip.");
    Expect(verifierMirror.colors.targetColors.size() == 2, "Expected verifier mirror target colors to roundtrip.");
    ExpectColorNear(verifierMirror.colors.output, { 0.1f, 0.2f, 0.3f, 0.4f }, "Expected verifier mirror output color to roundtrip.");
    ExpectColorNear(verifierMirror.colors.border, { 0.7f, 0.8f, 0.2f, 1.0f }, "Expected verifier mirror border color to roundtrip.");
    ExpectFloatNear(verifierMirror.colorSensitivity, 0.123f, "Expected verifier mirror color sensitivity to roundtrip.");
    Expect(verifierMirror.border.type == MirrorBorderType::Static, "Expected verifier mirror border type to roundtrip.");
    Expect(verifierMirror.border.staticShape == MirrorBorderShape::Circle, "Expected verifier mirror border shape to roundtrip.");
    ExpectColorNear(verifierMirror.border.staticColor, { 0.4f, 0.5f, 0.6f, 1.0f },
                    "Expected verifier mirror static border color to roundtrip.");
    Expect(verifierMirror.border.staticThickness == 5 && verifierMirror.border.staticRadius == 18,
           "Expected verifier mirror static border size to roundtrip.");
    Expect(verifierMirror.border.staticOffsetX == 7 && verifierMirror.border.staticOffsetY == -4,
           "Expected verifier mirror static border offsets to roundtrip.");
    Expect(verifierMirror.border.staticWidth == 91 && verifierMirror.border.staticHeight == 87,
           "Expected verifier mirror static border dimensions to roundtrip.");
    Expect(verifierMirror.fps == 48, "Expected verifier mirror fps to roundtrip.");
    ExpectFloatNear(verifierMirror.opacity, 0.654f, "Expected verifier mirror opacity to roundtrip.");
    Expect(verifierMirror.rawOutput, "Expected verifier mirror rawOutput to roundtrip.");
    Expect(verifierMirror.colorPassthrough, "Expected verifier mirror colorPassthrough to roundtrip.");
    Expect(verifierMirror.gradientOutput, "Expected verifier mirror gradientOutput to roundtrip.");
    Expect(verifierMirror.gradient.gradientStops.size() == 2, "Expected verifier mirror gradient stops to roundtrip.");
    ExpectFloatNear(verifierMirror.gradient.gradientAngle, 42.0f, "Expected verifier mirror gradient angle to roundtrip.");
    Expect(verifierMirror.gradient.gradientAnimation == GradientAnimationType::Rotate,
           "Expected verifier mirror gradient animation to roundtrip.");
    ExpectFloatNear(verifierMirror.gradient.gradientAnimationSpeed, 1.7f,
                    "Expected verifier mirror gradient animation speed to roundtrip.");
    Expect(verifierMirror.gradient.gradientColorFade, "Expected verifier mirror gradient color fade to roundtrip.");
    Expect(verifierMirror.onlyOnMyScreen, "Expected verifier mirror onlyOnMyScreen to roundtrip.");

    const MirrorConfig& auxMirror = FindMirrorOrThrow(kAuxMirrorName);
    Expect(auxMirror.captureWidth == 88 && auxMirror.captureHeight == 66, "Expected aux mirror capture dimensions to roundtrip.");
}

void VerifyRichMirrorGroups() {
    const MirrorGroupConfig& group = FindMirrorGroupOrThrow(kVerifierGroupName);
    Expect(group.output.useRelativePosition, "Expected mirror group relative positioning to roundtrip.");
    ExpectFloatNear(group.output.relativeX, 0.5f, "Expected mirror group output.relativeX to roundtrip.");
    ExpectFloatNear(group.output.relativeY, 0.4f, "Expected mirror group output.relativeY to roundtrip.");
    Expect(group.output.separateScale, "Expected mirror group separateScale to roundtrip.");
    ExpectFloatNear(group.output.scaleX, 1.3f, "Expected mirror group scaleX to roundtrip.");
    ExpectFloatNear(group.output.scaleY, 0.7f, "Expected mirror group scaleY to roundtrip.");
    Expect(group.mirrors.size() == 2, "Expected mirror group items to roundtrip.");
    Expect(group.mirrors[0].mirrorId == kVerifierMirrorName && group.mirrors[0].enabled,
           "Expected mirror group first mirror item to roundtrip.");
    ExpectFloatNear(group.mirrors[0].widthPercent, 0.6f, "Expected mirror group widthPercent to roundtrip.");
    ExpectFloatNear(group.mirrors[1].heightPercent, 0.6f, "Expected mirror group second heightPercent to roundtrip.");
    Expect(group.mirrors[1].offsetX == -20 && group.mirrors[1].offsetY == 30,
           "Expected mirror group second offsets to roundtrip.");
}

void VerifyRichImages() {
    const ImageConfig& image = FindImageOrThrow(kVerifierImageName);
    Expect(image.path == "C:\\temp\\checklist.png", "Expected image path to roundtrip.");
    Expect(image.x == 11 && image.y == 22, "Expected image position to roundtrip.");
    ExpectFloatNear(image.scale, 1.6f, "Expected image scale to roundtrip.");
    Expect(!image.relativeSizing, "Expected image relativeSizing to roundtrip.");
    Expect(image.width == 320 && image.height == 180, "Expected image dimensions to roundtrip.");
    Expect(image.relativeTo == "centerViewport", "Expected image relativeTo to roundtrip.");
    Expect(image.crop_top == 3 && image.crop_bottom == 4 && image.crop_left == 5 && image.crop_right == 6,
           "Expected image crop values to roundtrip.");
    Expect(image.enableColorKey, "Expected image enableColorKey to roundtrip.");
    Expect(image.colorKeys.size() == 2, "Expected image color keys to roundtrip.");
    ExpectFloatNear(image.colorKeys[0].sensitivity, 0.02f, "Expected image color key sensitivity to roundtrip.");
    ExpectFloatNear(image.opacity, 0.81f, "Expected image opacity to roundtrip.");
    Expect(image.background.enabled, "Expected image background.enabled to roundtrip.");
    ExpectColorNear(image.background.color, { 0.2f, 0.3f, 0.4f, 1.0f }, "Expected image background color to roundtrip.");
    ExpectFloatNear(image.background.opacity, 0.33f, "Expected image background opacity to roundtrip.");
    Expect(image.pixelatedScaling, "Expected image pixelatedScaling to roundtrip.");
    Expect(image.onlyOnMyScreen, "Expected image onlyOnMyScreen to roundtrip.");
    Expect(image.border.enabled, "Expected image border.enabled to roundtrip.");
    Expect(image.border.width == 7 && image.border.radius == 12, "Expected image border geometry to roundtrip.");
}

void VerifyRichWindowOverlays() {
    const WindowOverlayConfig& overlay = FindWindowOverlayOrThrow(kVerifierWindowOverlayName);
    Expect(overlay.windowTitle == "Speedrun Timer", "Expected window overlay title to roundtrip.");
    Expect(overlay.windowClass == "TimerClass", "Expected window overlay class to roundtrip.");
    Expect(overlay.executableName == "Timer.exe", "Expected window overlay executable to roundtrip.");
    Expect(overlay.windowMatchPriority == "title_executable", "Expected window overlay match priority to roundtrip.");
    Expect(overlay.x == -30 && overlay.y == 45, "Expected window overlay position to roundtrip.");
    ExpectFloatNear(overlay.scale, 1.2f, "Expected window overlay scale to roundtrip.");
    Expect(overlay.relativeTo == "centerViewport", "Expected window overlay relativeTo to roundtrip.");
    Expect(overlay.crop_top == 4 && overlay.crop_bottom == 5 && overlay.crop_left == 6 && overlay.crop_right == 7,
           "Expected window overlay crop values to roundtrip.");
    Expect(overlay.enableColorKey, "Expected window overlay enableColorKey to roundtrip.");
    Expect(overlay.colorKeys.size() == 1, "Expected window overlay color keys to roundtrip.");
    ExpectFloatNear(overlay.opacity, 0.71f, "Expected window overlay opacity to roundtrip.");
    Expect(overlay.background.enabled, "Expected window overlay background.enabled to roundtrip.");
    Expect(overlay.pixelatedScaling, "Expected window overlay pixelatedScaling to roundtrip.");
    Expect(overlay.onlyOnMyScreen, "Expected window overlay onlyOnMyScreen to roundtrip.");
    Expect(overlay.fps == 27, "Expected window overlay fps to roundtrip.");
    Expect(overlay.searchInterval == 2500, "Expected window overlay searchInterval to roundtrip.");
    Expect(overlay.captureMethod == "BitBlt", "Expected window overlay captureMethod to roundtrip.");
    Expect(overlay.forceUpdate, "Expected window overlay forceUpdate to roundtrip.");
    Expect(overlay.enableInteraction, "Expected window overlay enableInteraction to roundtrip.");
    Expect(overlay.border.enabled, "Expected window overlay border.enabled to roundtrip.");
}

void VerifyRichBrowserOverlays() {
    const BrowserOverlayConfig& overlay = FindBrowserOverlayOrThrow(kVerifierBrowserOverlayName);
    Expect(overlay.url == "https://example.com/dashboard", "Expected browser overlay URL to roundtrip.");
    Expect(overlay.customCss == "body { background: transparent; }", "Expected browser overlay CSS to roundtrip.");
    Expect(overlay.browserWidth == 1024 && overlay.browserHeight == 576, "Expected browser overlay dimensions to roundtrip.");
    Expect(overlay.x == 9 && overlay.y == 19, "Expected browser overlay position to roundtrip.");
    ExpectFloatNear(overlay.scale, 1.15f, "Expected browser overlay scale to roundtrip.");
    Expect(overlay.relativeTo == "centerViewport", "Expected browser overlay relativeTo to roundtrip.");
    Expect(overlay.enableColorKey, "Expected browser overlay enableColorKey to roundtrip.");
    Expect(overlay.colorKeys.size() == 1, "Expected browser overlay color keys to roundtrip.");
    ExpectFloatNear(overlay.opacity, 0.67f, "Expected browser overlay opacity to roundtrip.");
    Expect(overlay.background.enabled, "Expected browser overlay background.enabled to roundtrip.");
    Expect(overlay.pixelatedScaling, "Expected browser overlay pixelatedScaling to roundtrip.");
    Expect(overlay.onlyOnMyScreen, "Expected browser overlay onlyOnMyScreen to roundtrip.");
    Expect(overlay.fps == 25, "Expected browser overlay fps to roundtrip.");
    Expect(overlay.transparentBackground, "Expected browser overlay transparentBackground to roundtrip.");
    Expect(!overlay.muteAudio, "Expected browser overlay muteAudio to roundtrip.");
    Expect(!overlay.allowSystemMediaKeys, "Expected browser overlay allowSystemMediaKeys to roundtrip.");
    Expect(overlay.reloadOnUpdate, "Expected browser overlay reloadOnUpdate to roundtrip.");
    Expect(overlay.reloadInterval == 2500, "Expected browser overlay reloadInterval to roundtrip.");
    Expect(overlay.border.enabled, "Expected browser overlay border.enabled to roundtrip.");
}

void VerifyRichHotkeys() {
    const HotkeyConfig& hotkey = FindHotkeyBySecondaryModeOrThrow(kPrimaryModeId);
    ExpectVectorEquals(hotkey.keys, std::vector<DWORD>{ VK_F6, 'Q' }, "Expected hotkey keys to roundtrip.");
    Expect(hotkey.mainMode == "Fullscreen", "Expected hotkey main mode to roundtrip.");
    Expect(hotkey.secondaryMode == kPrimaryModeId, "Expected hotkey secondary mode to roundtrip.");
    Expect(hotkey.altSecondaryModes.size() == 1, "Expected alt secondary modes to roundtrip.");
    ExpectVectorEquals(hotkey.altSecondaryModes[0].keys, std::vector<DWORD>{ 'E', 'R' },
                       "Expected alt secondary mode keys to roundtrip.");
    Expect(hotkey.altSecondaryModes[0].mode == kPrecisionModeId, "Expected alt secondary mode target to roundtrip.");
    ExpectVectorEquals(hotkey.conditions.gameState, std::vector<std::string>{ "ingame", "wall" },
                       "Expected hotkey gameState conditions to roundtrip.");
    ExpectVectorEquals(hotkey.conditions.exclusions, std::vector<DWORD>{ VK_LSHIFT, VK_RBUTTON },
                       "Expected hotkey exclusions to roundtrip.");
    Expect(hotkey.debounce == 220, "Expected hotkey debounce to roundtrip.");
    Expect(hotkey.triggerOnRelease, "Expected hotkey triggerOnRelease to roundtrip.");
    Expect(hotkey.triggerOnHold, "Expected hotkey triggerOnHold to roundtrip.");
    Expect(hotkey.blockKeyFromGame, "Expected hotkey blockKeyFromGame to roundtrip.");
    Expect(hotkey.allowExitToFullscreenRegardlessOfGameState,
           "Expected hotkey allowExitToFullscreenRegardlessOfGameState to roundtrip.");
}

void VerifyRichSensitivityHotkeys() {
    const SensitivityHotkeyConfig& hotkey = FindSensitivityHotkeyOrThrow({ VK_F9 });
    ExpectFloatNear(hotkey.sensitivity, 0.55f, "Expected sensitivity hotkey sensitivity to roundtrip.");
    Expect(hotkey.separateXY, "Expected sensitivity hotkey separateXY to roundtrip.");
    ExpectFloatNear(hotkey.sensitivityX, 0.75f, "Expected sensitivity hotkey sensitivityX to roundtrip.");
    ExpectFloatNear(hotkey.sensitivityY, 0.95f, "Expected sensitivity hotkey sensitivityY to roundtrip.");
    Expect(hotkey.toggle, "Expected sensitivity hotkey toggle to roundtrip.");
    ExpectVectorEquals(hotkey.conditions.gameState, std::vector<std::string>{ "menu" },
                       "Expected sensitivity hotkey gameState conditions to roundtrip.");
    ExpectVectorEquals(hotkey.conditions.exclusions, std::vector<DWORD>{ VK_MENU },
                       "Expected sensitivity hotkey exclusions to roundtrip.");
    Expect(hotkey.debounce == 350, "Expected sensitivity hotkey debounce to roundtrip.");
}

void VerifyRichCursorsAndEyeZoom() {
    Expect(g_config.cursors.enabled, "Expected cursors.enabled to roundtrip.");
    Expect(g_config.cursors.title.cursorName == "title.cur" && g_config.cursors.title.cursorSize == 64,
           "Expected title cursor config to roundtrip.");
    Expect(g_config.cursors.wall.cursorName == "wall.cur" && g_config.cursors.wall.cursorSize == 72,
           "Expected wall cursor config to roundtrip.");
    Expect(g_config.cursors.ingame.cursorName == "ingame.cur" && g_config.cursors.ingame.cursorSize == 88,
           "Expected ingame cursor config to roundtrip.");

    Expect(g_config.eyezoom.cloneWidth == 28, "Expected eyezoom cloneWidth to roundtrip.");
    Expect(g_config.eyezoom.overlayWidth == 9, "Expected eyezoom overlayWidth to roundtrip.");
    Expect(g_config.eyezoom.cloneHeight == 1500, "Expected eyezoom cloneHeight to roundtrip.");
    Expect(g_config.eyezoom.stretchWidth == 600, "Expected eyezoom stretchWidth to roundtrip.");
    Expect(g_config.eyezoom.windowWidth == 420, "Expected eyezoom windowWidth to roundtrip.");
    Expect(g_config.eyezoom.windowHeight == 9000, "Expected eyezoom windowHeight to roundtrip.");
    Expect(g_config.eyezoom.zoomAreaWidth == 222 && g_config.eyezoom.zoomAreaHeight == 777,
           "Expected eyezoom zoom area to roundtrip.");
    Expect(g_config.eyezoom.useCustomSizePosition, "Expected eyezoom useCustomSizePosition to roundtrip.");
    Expect(g_config.eyezoom.positionX == 33 && g_config.eyezoom.positionY == 44,
           "Expected eyezoom position to roundtrip.");
        Expect(g_config.eyezoom.fontSizeMode == EyeZoomFontSizeMode::Manual,
            "Expected eyezoom fontSizeMode to roundtrip.");
    Expect(g_config.eyezoom.textFontSize == 31, "Expected eyezoom textFontSize to roundtrip.");
    Expect(g_config.eyezoom.textFontPath == "C:\\Windows\\Fonts\\verdana.ttf", "Expected eyezoom font path to roundtrip.");
    Expect(g_config.eyezoom.rectHeight == 35, "Expected eyezoom rectHeight to roundtrip.");
    Expect(!g_config.eyezoom.linkRectToFont, "Expected eyezoom linkRectToFont to roundtrip.");
    ExpectColorNear(g_config.eyezoom.gridColor1, { 0.2f, 0.4f, 0.6f, 1.0f }, "Expected eyezoom gridColor1 to roundtrip.");
    ExpectFloatNear(g_config.eyezoom.gridColor1Opacity, 0.8f, "Expected eyezoom gridColor1Opacity to roundtrip.");
    ExpectColorNear(g_config.eyezoom.gridColor2, { 0.6f, 0.3f, 0.2f, 1.0f }, "Expected eyezoom gridColor2 to roundtrip.");
    ExpectFloatNear(g_config.eyezoom.gridColor2Opacity, 0.7f, "Expected eyezoom gridColor2Opacity to roundtrip.");
    ExpectColorNear(g_config.eyezoom.centerLineColor, { 0.9f, 0.8f, 0.1f, 1.0f }, "Expected eyezoom centerLineColor to roundtrip.");
    ExpectFloatNear(g_config.eyezoom.centerLineColorOpacity, 0.65f, "Expected eyezoom centerLineColorOpacity to roundtrip.");
    ExpectColorNear(g_config.eyezoom.textColor, { 0.1f, 0.2f, 0.3f, 1.0f }, "Expected eyezoom textColor to roundtrip.");
    ExpectFloatNear(g_config.eyezoom.textColorOpacity, 0.95f, "Expected eyezoom textColorOpacity to roundtrip.");
    Expect(g_config.eyezoom.slideZoomIn, "Expected eyezoom slideZoomIn to roundtrip.");
    Expect(g_config.eyezoom.slideMirrorsIn, "Expected eyezoom slideMirrorsIn to roundtrip.");
    Expect(g_config.eyezoom.overlays.size() == 2, "Expected eyezoom overlays to roundtrip.");
    Expect(g_config.eyezoom.overlays[1].name == "Overlay Two", "Expected eyezoom second overlay name to roundtrip.");
    Expect(g_config.eyezoom.overlays[1].displayMode == EyeZoomOverlayDisplayMode::Manual,
           "Expected eyezoom second overlay displayMode to roundtrip.");
    Expect(g_config.eyezoom.activeOverlayIndex == 1, "Expected eyezoom activeOverlayIndex to roundtrip.");
}

void VerifyRichKeyRebindsAndAppearance() {
    Expect(g_config.keyRebinds.enabled, "Expected keyRebinds.enabled to roundtrip.");
    Expect(!g_config.keyRebinds.resolveRebindTargetsForHotkeys,
           "Expected keyRebinds.resolveRebindTargetsForHotkeys to roundtrip.");
    ExpectVectorEquals(g_config.keyRebinds.toggleHotkey, std::vector<DWORD>{ VK_F4 },
                       "Expected keyRebinds.toggleHotkey to roundtrip.");
    Expect(g_config.keyRebinds.rebinds.size() == 1, "Expected key rebind list to roundtrip.");
    const KeyRebind& rebind = g_config.keyRebinds.rebinds.front();
    Expect(rebind.fromKey == 'J' && rebind.toKey == 'K', "Expected key rebind source/target to roundtrip.");
    Expect(rebind.enabled, "Expected key rebind enabled flag to roundtrip.");
    Expect(rebind.useCustomOutput, "Expected key rebind useCustomOutput to roundtrip.");
    Expect(rebind.customOutputVK == 'L', "Expected key rebind customOutputVK to roundtrip.");
    Expect(rebind.customOutputUnicode == 0x00F8, "Expected key rebind customOutputUnicode to roundtrip.");
    Expect(rebind.customOutputScanCode == 0x26, "Expected key rebind customOutputScanCode to roundtrip.");
    Expect(rebind.baseOutputShifted, "Expected key rebind baseOutputShifted to roundtrip.");
    Expect(rebind.shiftLayerEnabled, "Expected key rebind shiftLayerEnabled to roundtrip.");
    Expect(rebind.shiftLayerOutputVK == 'P', "Expected key rebind shiftLayerOutputVK to roundtrip.");
    Expect(rebind.shiftLayerOutputUnicode == 0x00D8, "Expected key rebind shiftLayerOutputUnicode to roundtrip.");
    Expect(rebind.shiftLayerOutputShifted, "Expected key rebind shiftLayerOutputShifted to roundtrip.");

    Expect(g_config.appearance.theme == "Sunrise", "Expected appearance theme to roundtrip.");
    Expect(g_config.appearance.customColors.size() == 2, "Expected appearance custom colors to roundtrip.");
    ExpectColorNear(g_config.appearance.customColors.at("WindowBg"), { 0.1f, 0.11f, 0.12f, 1.0f },
                    "Expected appearance WindowBg custom color to roundtrip.");
    ExpectColorNear(g_config.appearance.customColors.at("Header"), { 0.8f, 0.4f, 0.2f, 1.0f },
                    "Expected appearance Header custom color to roundtrip.");
}

template <typename VerifyFn>
void RunRichConfigRoundtripCase(std::string_view caseName, VerifyFn&& verify, TestRunMode runMode = TestRunMode::Automated) {
    DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);
    const std::filesystem::path root = PrepareCaseDirectory(caseName);
    ResetGlobalTestState(root);

    LoadConfig();
    ExpectConfigLoadSucceeded(std::string(caseName) + " initial default load");

    PopulateRichConfigFixture();
    SaveAndReloadCurrentConfig();
    ExpectConfigLoadSucceeded(std::string(caseName) + " roundtrip reload");

    verify();

    if (runMode == TestRunMode::Visual) {
        RunVisualLoop(window, caseName, &RenderInteractiveSettingsFrame);
    }
}

template <typename WriteFixtureFn, typename VerifyFn>
void RunConfigLoadCase(std::string_view caseName, WriteFixtureFn&& writeFixture, VerifyFn&& verify,
                       TestRunMode runMode = TestRunMode::Automated) {
    DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);
    const std::filesystem::path root = PrepareCaseDirectory(caseName);
    ResetGlobalTestState(root);

    writeFixture();
    LoadConfig();
    verify();

    if (runMode == TestRunMode::Visual) {
        RunVisualLoop(window, caseName, &RenderInteractiveSettingsFrame);
    }
}

void PrepareRichConfigForGui(std::string_view caseName) {
    const std::filesystem::path root = PrepareCaseDirectory(caseName);
    ResetGlobalTestState(root);
    LoadConfig();
    ExpectConfigLoadSucceeded(std::string(caseName) + " initial GUI fixture load");
    PopulateRichConfigFixture();
    SaveAndReloadCurrentConfig();
    ExpectConfigLoadSucceeded(std::string(caseName) + " GUI fixture reload");
}

void PrepareDefaultConfigForGui(std::string_view caseName, bool basicModeEnabled) {
    const std::filesystem::path root = PrepareCaseDirectory(caseName);
    ResetGlobalTestState(root);
    LoadConfig();
    ExpectConfigLoadSucceeded(std::string(caseName) + " default GUI fixture load");
    g_config.basicModeEnabled = basicModeEnabled;
    g_configIsDirty.store(false, std::memory_order_release);
}

void RunDefaultSettingsTabCase(std::string_view caseName, const std::string& topLevelTabLabel,
                               const std::string& inputsSubTabLabel, bool basicModeEnabled,
                               TestRunMode runMode = TestRunMode::Automated) {
    DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);
    PrepareDefaultConfigForGui(caseName, basicModeEnabled);

    if (runMode == TestRunMode::Visual) {
        RunVisualLoop(window, caseName, [&](DummyWindow& visualWindow) {
            RenderSettingsFrame(visualWindow, topLevelTabLabel.c_str(),
                                inputsSubTabLabel.empty() ? nullptr : inputsSubTabLabel.c_str());
        });
        return;
    }

    RenderSettingsFrame(window, topLevelTabLabel.c_str(), inputsSubTabLabel.empty() ? nullptr : inputsSubTabLabel.c_str());
}

void RunPopulatedSettingsTabCase(std::string_view caseName, const std::string& topLevelTabLabel,
                                 const std::string& inputsSubTabLabel = std::string(),
                                 TestRunMode runMode = TestRunMode::Automated) {
    DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);
    PrepareRichConfigForGui(caseName);

    if (runMode == TestRunMode::Visual) {
        RunVisualLoop(window, caseName, [&](DummyWindow& visualWindow) {
            RenderSettingsFrame(visualWindow, topLevelTabLabel.c_str(),
                                inputsSubTabLabel.empty() ? nullptr : inputsSubTabLabel.c_str());
        });
        return;
    }

    RenderSettingsFrame(window, topLevelTabLabel.c_str(), inputsSubTabLabel.empty() ? nullptr : inputsSubTabLabel.c_str());
}

template <typename RenderFrameFn>
void RunVisualLoop(DummyWindow& window, std::string_view testCaseName, RenderFrameFn&& renderFrame) {
    window.SetTitle("Toolscreen GUI Integration Tests [visual] - " + std::string(testCaseName));

    std::cout << "VISUAL " << testCaseName << std::endl;
    std::cout << "Close the dummy test window to exit visual mode." << std::endl;

    while (window.PumpMessages()) {
        renderFrame(window);
        Sleep(16);
    }

    std::cout << "EXIT " << testCaseName << std::endl;
}

void RunConfigDefaultLoadTest(TestRunMode runMode = TestRunMode::Automated) {
    DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);
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

    if (runMode == TestRunMode::Visual) {
        RunVisualLoop(window, "config-default-load", &RenderInteractiveSettingsFrame);
    }
}

void RunConfigRoundtripTest(TestRunMode runMode = TestRunMode::Automated) {
    RunRichConfigRoundtripCase("config_roundtrip", []() {
        VerifyRichGlobalSettings();
        VerifyRichModes();
        VerifyRichMirrors();
        VerifyRichMirrorGroups();
        VerifyRichImages();
        VerifyRichWindowOverlays();
        VerifyRichBrowserOverlays();
        VerifyRichHotkeys();
        VerifyRichSensitivityHotkeys();
        VerifyRichCursorsAndEyeZoom();
        VerifyRichKeyRebindsAndAppearance();
        VerifyRichDebugSettings();
    }, runMode);
}

void RunConfigRoundtripGlobalSettingsTest(TestRunMode runMode = TestRunMode::Automated) {
    RunRichConfigRoundtripCase("config_roundtrip_global_settings", &VerifyRichGlobalSettings, runMode);
}

void RunConfigRoundtripModesTest(TestRunMode runMode = TestRunMode::Automated) {
    RunRichConfigRoundtripCase("config_roundtrip_modes", &VerifyRichModes, runMode);
}

void RunConfigRoundtripMirrorsTest(TestRunMode runMode = TestRunMode::Automated) {
    RunRichConfigRoundtripCase("config_roundtrip_mirrors", &VerifyRichMirrors, runMode);
}

void RunConfigRoundtripMirrorGroupsTest(TestRunMode runMode = TestRunMode::Automated) {
    RunRichConfigRoundtripCase("config_roundtrip_mirror_groups", &VerifyRichMirrorGroups, runMode);
}

void RunConfigRoundtripImagesTest(TestRunMode runMode = TestRunMode::Automated) {
    RunRichConfigRoundtripCase("config_roundtrip_images", &VerifyRichImages, runMode);
}

void RunConfigRoundtripWindowOverlaysTest(TestRunMode runMode = TestRunMode::Automated) {
    RunRichConfigRoundtripCase("config_roundtrip_window_overlays", &VerifyRichWindowOverlays, runMode);
}

void RunConfigRoundtripBrowserOverlaysTest(TestRunMode runMode = TestRunMode::Automated) {
    RunRichConfigRoundtripCase("config_roundtrip_browser_overlays", &VerifyRichBrowserOverlays, runMode);
}

void RunConfigRoundtripHotkeysTest(TestRunMode runMode = TestRunMode::Automated) {
    RunRichConfigRoundtripCase("config_roundtrip_hotkeys", &VerifyRichHotkeys, runMode);
}

void RunConfigRoundtripSensitivityHotkeysTest(TestRunMode runMode = TestRunMode::Automated) {
    RunRichConfigRoundtripCase("config_roundtrip_sensitivity_hotkeys", &VerifyRichSensitivityHotkeys, runMode);
}

void RunConfigRoundtripCursorsAndEyeZoomTest(TestRunMode runMode = TestRunMode::Automated) {
    RunRichConfigRoundtripCase("config_roundtrip_cursors_eyezoom", &VerifyRichCursorsAndEyeZoom, runMode);
}

void RunConfigRoundtripKeyRebindsAndAppearanceTest(TestRunMode runMode = TestRunMode::Automated) {
    RunRichConfigRoundtripCase("config_roundtrip_key_rebinds_appearance", &VerifyRichKeyRebindsAndAppearance, runMode);
}

void RunConfigRoundtripDebugSettingsTest(TestRunMode runMode = TestRunMode::Automated) {
    RunRichConfigRoundtripCase("config_roundtrip_debug_settings", &VerifyRichDebugSettings, runMode);
}

void RunConfigLoadMissingRequiredModesTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_missing_required_modes",
                      []() {
                          Config config;
                          config.defaultMode = kPrimaryModeId;
                          config.modes.clear();

                          ModeConfig primaryMode;
                          primaryMode.id = kPrimaryModeId;
                          primaryMode.width = 1111;
                          primaryMode.height = 666;
                          primaryMode.manualWidth = 1111;
                          primaryMode.manualHeight = 666;
                          config.modes.push_back(primaryMode);

                          WriteConfigFixtureToDisk(config);
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-missing-required-modes");
                          const ModeConfig& fullscreen = FindModeOrThrow("Fullscreen");
                          const ModeConfig& eyezoom = FindModeOrThrow("EyeZoom");
                          const ModeConfig& preemptive = FindModeOrThrow("Preemptive");
                          const ModeConfig& thin = FindModeOrThrow("Thin");
                          const ModeConfig& wide = FindModeOrThrow("Wide");
                          (void)thin;
                          (void)wide;
                          Expect(fullscreen.stretch.enabled, "Expected missing Fullscreen mode to be recreated with stretch enabled.");
                          Expect(fullscreen.width > 0 && fullscreen.height > 0,
                                 "Expected recreated Fullscreen mode to receive valid dimensions.");
                          Expect(eyezoom.width > 0 && eyezoom.height > 0, "Expected missing EyeZoom mode to be recreated.");
                          Expect(preemptive.width == eyezoom.width && preemptive.height == eyezoom.height,
                                 "Expected missing Preemptive mode to inherit EyeZoom dimensions.");
                          Expect(!preemptive.useRelativeSize, "Expected recreated Preemptive mode to use absolute sizing.");
                      },
                      runMode);
}

void RunConfigLoadInvalidHotkeyModeReferencesTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_invalid_hotkey_mode_references",
                      []() {
                          Config config;
                          config.defaultMode = kPrecisionModeId;
                          config.modes.clear();

                          ModeConfig precisionMode;
                          precisionMode.id = kPrecisionModeId;
                          precisionMode.width = 900;
                          precisionMode.height = 500;
                          precisionMode.manualWidth = 900;
                          precisionMode.manualHeight = 500;
                          config.modes.push_back(precisionMode);

                          HotkeyConfig invalidHotkey;
                          invalidHotkey.keys = { VK_F2 };
                          invalidHotkey.mainMode = "Missing Main";
                          invalidHotkey.secondaryMode = "Missing Secondary";
                          invalidHotkey.altSecondaryModes = {
                              { { 'A' }, "Missing Alt" },
                              { { 'B' }, kPrecisionModeId },
                          };
                          config.hotkeys = { invalidHotkey };

                          WriteConfigFixtureToDisk(config);
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-invalid-hotkey-mode-references");
                          Expect(!g_config.hotkeys.empty(), "Expected hotkey fixture to load.");
                          const HotkeyConfig& hotkey = g_config.hotkeys.front();
                          Expect(hotkey.mainMode == kPrecisionModeId,
                                 "Expected invalid hotkey main mode to reset to the existing default mode.");
                          Expect(hotkey.secondaryMode.empty(), "Expected invalid hotkey secondary mode to be cleared.");
                          Expect(hotkey.altSecondaryModes.size() == 1, "Expected invalid alt secondary modes to be removed.");
                          Expect(hotkey.altSecondaryModes.front().mode == kPrecisionModeId,
                                 "Expected valid alt secondary mode to remain after sanitization.");
                      },
                      runMode);
}

void RunConfigLoadRelativeModeDimensionsTest(TestRunMode runMode = TestRunMode::Automated) {
    int expectedWidth = 1000;
    int expectedHeight = 600;

    RunConfigLoadCase("config_load_relative_mode_dimensions",
                      [&]() {
                          Config config;
                          config.defaultMode = kRelativeModeId;
                          config.modes.clear();

                          ModeConfig relativeMode;
                          relativeMode.id = kRelativeModeId;
                          relativeMode.useRelativeSize = true;
                          relativeMode.relativeWidth = 0.625f;
                          relativeMode.relativeHeight = 0.4f;
                          relativeMode.width = 1000;
                          relativeMode.height = 600;
                          relativeMode.manualWidth = 1000;
                          relativeMode.manualHeight = 600;
                          config.modes.push_back(relativeMode);

                          const int loadScreenWidth = (std::max)(1, GetCachedWindowWidth());
                          const int loadScreenHeight = (std::max)(1, GetCachedWindowHeight());
                          expectedWidth = (std::max)(1, static_cast<int>(std::lround(0.625f * static_cast<float>(loadScreenWidth))));
                          expectedHeight = (std::max)(1, static_cast<int>(std::lround(0.4f * static_cast<float>(loadScreenHeight))));

                          WriteConfigFixtureToDisk(config);
                      },
                      [&]() {
                          ExpectConfigLoadSucceeded("config-load-relative-mode-dimensions");
                          const ModeConfig& relativeMode = FindModeOrThrow(kRelativeModeId);
                          Expect(relativeMode.useRelativeSize, "Expected relative mode to stay marked as relative.");
                          ExpectFloatNear(relativeMode.relativeWidth, 0.625f, "Expected relative mode relativeWidth to roundtrip.");
                          ExpectFloatNear(relativeMode.relativeHeight, 0.4f, "Expected relative mode relativeHeight to roundtrip.");
                          Expect(relativeMode.width == expectedWidth, "Expected relative mode width to be recomputed from cached client width.");
                          Expect(relativeMode.height == expectedHeight,
                                 "Expected relative mode height to be recomputed from cached client height.");
                          Expect(relativeMode.manualWidth == 1000 && relativeMode.manualHeight == 600,
                                 "Expected relative mode manual dimensions to preserve their persisted values.");
                      },
                      runMode);
}

void RunConfigLoadExpressionModeDimensionsTest(TestRunMode runMode = TestRunMode::Automated) {
    int expectedWidth = 123;
    int expectedHeight = 456;

    RunConfigLoadCase("config_load_expression_mode_dimensions",
                      [&]() {
                          Config config;
                          config.defaultMode = kExpressionModeId;
                          config.modes.clear();

                          ModeConfig expressionMode;
                          expressionMode.id = kExpressionModeId;
                          expressionMode.widthExpr = "screenWidth / 3";
                          expressionMode.heightExpr = "screenHeight - 111";
                          expressionMode.width = 123;
                          expressionMode.height = 456;
                          expressionMode.manualWidth = 123;
                          expressionMode.manualHeight = 456;
                          config.modes.push_back(expressionMode);

                          const int loadScreenWidth = (std::max)(1, GetCachedWindowWidth());
                          const int loadScreenHeight = (std::max)(1, GetCachedWindowHeight());
                          expectedWidth = (std::max)(1, loadScreenWidth / 3);
                          expectedHeight = (std::max)(1, loadScreenHeight - 111);

                          WriteConfigFixtureToDisk(config);
                      },
                      [&]() {
                          ExpectConfigLoadSucceeded("config-load-expression-mode-dimensions");
                          const ModeConfig& expressionMode = FindModeOrThrow(kExpressionModeId);
                          Expect(expressionMode.widthExpr == "screenWidth / 3", "Expected width expression to roundtrip.");
                          Expect(expressionMode.heightExpr == "screenHeight - 111", "Expected height expression to roundtrip.");
                          Expect(expressionMode.width == expectedWidth, "Expected width expression to be evaluated during load.");
                          Expect(expressionMode.height == expectedHeight, "Expected height expression to be evaluated during load.");
                      },
                      runMode);
}

void RunConfigLoadLegacyVersionUpgradeTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_legacy_version_upgrade",
                      []() {
                          Config config;
                          config.configVersion = 1;
                          config.disableHookChaining = true;
                          config.defaultMode = kPrecisionModeId;
                          config.keyRepeatStartDelay = 10;
                          config.keyRepeatDelay = 0;
                          config.modes.clear();

                          ModeConfig precisionMode;
                          precisionMode.id = kPrecisionModeId;
                          precisionMode.width = 800;
                          precisionMode.height = 450;
                          precisionMode.manualWidth = 800;
                          precisionMode.manualHeight = 450;
                          config.modes.push_back(precisionMode);

                          WriteConfigFixtureToDisk(config);
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-legacy-version-upgrade");
                          Expect(g_config.configVersion == GetConfigVersion(), "Expected legacy config version to upgrade to the current version.");
                         Expect(g_config.disableHookChaining,
                             "Expected legacy disableHookChaining to remain preserved on the 1.2.1 branch.");
                         Expect(g_config.keyRepeatStartDelay == 10,
                             "Expected legacy non-zero keyRepeatStartDelay to remain preserved after upgrade.");
                          Expect(g_config.keyRepeatDelay == ConfigDefaults::CONFIG_KEY_REPEAT_DELAY,
                                 "Expected legacy zero keyRepeatDelay to normalize to the default value.");
                      },
                      runMode);
}

void RunConfigLoadClampGlobalValuesTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_clamp_global_values",
                      []() {
                          Config config;
                          config.defaultMode = "Fullscreen";
                          config.obsFramerate = 999;
                          config.cursors.enabled = true;
                          config.cursors.title.cursorSize = 9999;
                          config.cursors.wall.cursorSize = 2;
                          config.cursors.ingame.cursorSize = 321;
                          WriteConfigFixtureToDisk(config);
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-clamp-global-values");
                          Expect(g_config.obsFramerate == 120, "Expected obsFramerate to clamp to the configured maximum.");
                          Expect(g_config.cursors.title.cursorSize == ConfigDefaults::CURSOR_MAX_SIZE,
                                 "Expected title cursor size to clamp to CURSOR_MAX_SIZE.");
                          Expect(g_config.cursors.wall.cursorSize == ConfigDefaults::CURSOR_MIN_SIZE,
                                 "Expected wall cursor size to clamp to CURSOR_MIN_SIZE.");
                          Expect(g_config.cursors.ingame.cursorSize == ConfigDefaults::CURSOR_MAX_SIZE,
                                 "Expected ingame cursor size to clamp to CURSOR_MAX_SIZE.");
                      },
                      runMode);
}

void RunConfigLoadModeDefaultDimensionsRestoredTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_mode_default_dimensions_restored",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Wide"

[[mode]]
id = "Wide"
width = 1
height = 0
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-mode-default-dimensions-restored");
                          const ModeConfig& wideMode = FindModeOrThrow("Wide");
                          Expect(wideMode.useRelativeSize, "Expected repaired Wide mode to remain relative-sized.");
                          Expect(!wideMode.widthExpr.empty(), "Expected repaired Wide mode width expression to come from embedded defaults.");
                          ExpectFloatNear(wideMode.relativeHeight, 0.25f,
                                          "Expected repaired Wide mode relativeHeight to come from embedded defaults.");
                          Expect(wideMode.width > 1, "Expected repaired Wide mode width to be recomputed from the restored default expression.");
                          Expect(wideMode.height > 1, "Expected repaired Wide mode height to be recomputed from the restored default percentage.");
                      },
                      runMode);
}

void RunConfigLoadModeSourceListsLoadedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_mode_source_lists_loaded",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Source Lists"

[[mode]]
id = "Source Lists"
width = 800
height = 600
mirrorIds = ["Mirror One"]
mirrorGroupIds = ["Group One"]
imageIds = ["Image One"]
windowOverlayIds = ["Window One"]
browserOverlayIds = ["Browser One"]
)");
                      },
                      []() {
                 ExpectConfigLoadSucceeded("config-load-mode-source-lists-loaded");
                 const ModeConfig& mode = FindModeOrThrow("Source Lists");
                 ExpectVectorEquals(mode.mirrorIds, std::vector<std::string>{ "Mirror One" },
                           "Expected mirrorIds to load on the 1.2.1 branch.");
                 ExpectVectorEquals(mode.mirrorGroupIds, std::vector<std::string>{ "Group One" },
                           "Expected mirrorGroupIds to load on the 1.2.1 branch.");
                 ExpectVectorEquals(mode.imageIds, std::vector<std::string>{ "Image One" },
                           "Expected imageIds to load on the 1.2.1 branch.");
                 ExpectVectorEquals(mode.windowOverlayIds, std::vector<std::string>{ "Window One" },
                           "Expected windowOverlayIds to load on the 1.2.1 branch.");
                 ExpectVectorEquals(mode.browserOverlayIds, std::vector<std::string>{ "Browser One" },
                           "Expected browserOverlayIds to load on the 1.2.1 branch.");
                      },
                      runMode);
}

void RunConfigLoadModePercentageDimensionsDetectedTest(TestRunMode runMode = TestRunMode::Automated) {
    int expectedWidth = 1;
    int expectedHeight = 1;

    RunConfigLoadCase("config_load_mode_percentage_dimensions_detected",
                      [&]() {
                          const int screenWidth = (std::max)(1, GetCachedWindowWidth());
                          const int screenHeight = (std::max)(1, GetCachedWindowHeight());
                          expectedWidth = (std::max)(1, static_cast<int>(std::lround(0.5f * static_cast<float>(screenWidth))));
                          expectedHeight = (std::max)(1, static_cast<int>(std::lround(0.25f * static_cast<float>(screenHeight))));

                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Percentage Mode"

[[mode]]
id = "Percentage Mode"
width = 0.5
height = 0.25
)");
                      },
                      [&]() {
                          ExpectConfigLoadSucceeded("config-load-mode-percentage-dimensions-detected");
                          const ModeConfig& mode = FindModeOrThrow("Percentage Mode");
                          Expect(mode.useRelativeSize, "Expected percentage width/height values to mark the mode as relative-sized.");
                          ExpectFloatNear(mode.relativeWidth, 0.5f, "Expected percentage width to map to relativeWidth.");
                          ExpectFloatNear(mode.relativeHeight, 0.25f, "Expected percentage height to map to relativeHeight.");
                          Expect(mode.width == expectedWidth, "Expected percentage width to resolve against the cached window width.");
                          Expect(mode.height == expectedHeight, "Expected percentage height to resolve against the cached window height.");
                      },
                      runMode);
}

void RunConfigLoadModeTypedSourcesIgnoredTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_mode_typed_sources_ignored",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Typed Sources"

[[mode]]
id = "Typed Sources"
width = 800
height = 600
sources = [
    { type = "Mirror", id = "" },
    { type = "Mirror", id = "Valid Mirror" },
    { type = "Image", id = "" }
]
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-mode-typed-sources-ignored");
                          const ModeConfig& mode = FindModeOrThrow("Typed Sources");
                          Expect(mode.mirrorIds.empty(), "Expected typed mode sources to be ignored on the 1.2.1 branch.");
                          Expect(mode.mirrorGroupIds.empty(), "Expected typed mode sources to leave mirrorGroupIds empty.");
                          Expect(mode.imageIds.empty(), "Expected typed mode sources to leave imageIds empty.");
                          Expect(mode.windowOverlayIds.empty(), "Expected typed mode sources to leave windowOverlayIds empty.");
                          Expect(mode.browserOverlayIds.empty(), "Expected typed mode sources to leave browserOverlayIds empty.");
                      },
                      runMode);
}

void RunConfigLoadEmptyMainHotkeyFallbackTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_empty_main_hotkey_fallback",
                      []() {
                          Config config;
                          config.defaultMode = kPrecisionModeId;

                          ModeConfig precisionMode;
                          precisionMode.id = kPrecisionModeId;
                          precisionMode.width = 900;
                          precisionMode.height = 500;
                          precisionMode.manualWidth = 900;
                          precisionMode.manualHeight = 500;
                          config.modes.push_back(precisionMode);

                          HotkeyConfig hotkey;
                          hotkey.keys = { VK_F2 };
                          hotkey.mainMode.clear();
                          hotkey.secondaryMode = kPrecisionModeId;
                          config.hotkeys.push_back(hotkey);

                          WriteConfigFixtureToDisk(config);
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-empty-main-hotkey-fallback");
                          const HotkeyConfig& hotkey = FindHotkeyByKeysOrThrow({ VK_F2 });
                          Expect(hotkey.mainMode == kPrecisionModeId,
                                 "Expected a hotkey with an empty main mode to fall back to the default mode.");
                      },
                      runMode);
}

void RunConfigLoadMissingGuiHotkeyDefaultedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_missing_gui_hotkey_defaulted",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-missing-gui-hotkey-defaulted");
                          ExpectVectorEquals(g_config.guiHotkey, ConfigDefaults::GetDefaultGuiHotkey(),
                                             "Expected a missing guiHotkey to default to the configured hotkey.");
                      },
                      runMode);
}

void RunConfigLoadEmptyGuiHotkeyDefaultedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_empty_gui_hotkey_defaulted",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"
guiHotkey = []
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-empty-gui-hotkey-defaulted");
                          ExpectVectorEquals(g_config.guiHotkey, ConfigDefaults::GetDefaultGuiHotkey(),
                                             "Expected an empty guiHotkey array to fall back to the configured default hotkey.");
                      },
                      runMode);
}

void RunConfigLoadLegacyMirrorGammaMigratedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_legacy_mirror_gamma_migrated",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[[mirror]]
name = "Legacy Gamma"
gammaMode = "Linear"
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-legacy-mirror-gamma-migrated");
                          Expect(g_config.mirrorGammaMode == MirrorGammaMode::AssumeLinear,
                                 "Expected legacy per-mirror gammaMode to migrate into the global mirror gamma mode.");
                      },
                      runMode);
}

void RunConfigLoadMirrorCaptureDimensionsClampedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_mirror_capture_dimensions_clamped",
                      []() {
                          Config config;
                          config.defaultMode = "Fullscreen";

                          MirrorConfig mirror;
                          mirror.name = "Clamp Mirror";
                          mirror.captureWidth = ConfigDefaults::MIRROR_CAPTURE_MAX_DIMENSION + 200;
                          mirror.captureHeight = 0;
                          config.mirrors.push_back(mirror);

                          WriteConfigFixtureToDisk(config);
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-mirror-capture-dimensions-clamped");
                          const MirrorConfig& mirror = FindMirrorOrThrow("Clamp Mirror");
                          Expect(mirror.captureWidth == ConfigDefaults::MIRROR_CAPTURE_MAX_DIMENSION,
                                 "Expected mirror captureWidth to clamp to MIRROR_CAPTURE_MAX_DIMENSION.");
                          Expect(mirror.captureHeight == ConfigDefaults::MIRROR_CAPTURE_MIN_DIMENSION,
                                 "Expected mirror captureHeight to clamp to MIRROR_CAPTURE_MIN_DIMENSION.");
                      },
                      runMode);
}

void RunConfigLoadEyeZoomCloneWidthNormalizedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_eyezoom_clone_width_normalized",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[eyezoom]
cloneWidth = 15
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-eyezoom-clone-width-normalized");
                          Expect(g_config.eyezoom.cloneWidth == 14,
                                 "Expected odd eyezoom cloneWidth values to normalize to the nearest even width.");
                      },
                      runMode);
}

void RunConfigLoadEyeZoomOverlayWidthDefaultedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_eyezoom_overlay_width_defaulted",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[eyezoom]
cloneWidth = 14
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-eyezoom-overlay-width-defaulted");
                          Expect(g_config.eyezoom.overlayWidth == 7,
                                 "Expected a missing eyezoom overlayWidth to default to half of cloneWidth.");
                      },
                      runMode);
}

void RunConfigLoadEyeZoomOverlayWidthClampedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_eyezoom_overlay_width_clamped",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[eyezoom]
cloneWidth = 10
overlayWidth = 99
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-eyezoom-overlay-width-clamped");
                          Expect(g_config.eyezoom.overlayWidth == 5,
                                 "Expected eyezoom overlayWidth to clamp to half of cloneWidth.");
                      },
                      runMode);
}

void RunConfigLoadEyeZoomLegacyMarginsMigratedTest(TestRunMode runMode = TestRunMode::Automated) {
    int expectedWidth = 1;
    int expectedHeight = 1;

    RunConfigLoadCase("config_load_eyezoom_legacy_margins_migrated",
                      [&]() {
                          const int screenWidth = (std::max)(1, GetCachedWindowWidth());
                          const int screenHeight = (std::max)(1, GetCachedWindowHeight());
                          const int viewportX = (screenWidth - 400) / 2;
                          expectedWidth = (viewportX > 0) ? (viewportX - (2 * 20)) : screenWidth;
                          expectedHeight = screenHeight - (2 * 30);

                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[eyezoom]
windowWidth = 400
horizontalMargin = 20
verticalMargin = 30
)");
                      },
                      [&]() {
                          ExpectConfigLoadSucceeded("config-load-eyezoom-legacy-margins-migrated");
                          Expect(g_config.eyezoom.zoomAreaWidth == expectedWidth,
                                 "Expected legacy eyezoom horizontalMargin to migrate into zoomAreaWidth.");
                          Expect(g_config.eyezoom.zoomAreaHeight == expectedHeight,
                                 "Expected legacy eyezoom verticalMargin to migrate into zoomAreaHeight.");
                      },
                      runMode);
}

void RunConfigLoadEyeZoomLegacyCustomPositionMigratedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_eyezoom_legacy_custom_position_migrated",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[eyezoom]
useCustomPosition = true
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-eyezoom-legacy-custom-position-migrated");
                          Expect(g_config.eyezoom.useCustomSizePosition,
                                 "Expected legacy eyezoom useCustomPosition to map to useCustomSizePosition.");
                      },
                      runMode);
}

void RunConfigLoadEyeZoomInvalidActiveOverlayResetTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_eyezoom_invalid_active_overlay_reset",
                      []() {
                          Config config;
                          config.defaultMode = "Fullscreen";
                          config.eyezoom.activeOverlayIndex = 4;
                          config.eyezoom.overlays = {
                              { "Only Overlay", "C:\\temp\\overlay.png", EyeZoomOverlayDisplayMode::Fit, 120, 80, false, 0.75f },
                          };
                          WriteConfigFixtureToDisk(config);
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-eyezoom-invalid-active-overlay-reset");
                          Expect(g_config.eyezoom.activeOverlayIndex == -1,
                                 "Expected out-of-range eyezoom activeOverlayIndex values to reset to -1.");
                      },
                      runMode);
}

void RunConfigLoadWindowOverlayCaptureMethodMigratedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_window_overlay_capture_method_migrated",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[[windowOverlay]]
name = "Legacy Capture"
captureMethod = "Auto"
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-window-overlay-capture-method-migrated");
                          const WindowOverlayConfig& overlay = FindWindowOverlayOrThrow("Legacy Capture");
                          Expect(overlay.captureMethod == "Windows 10+",
                                 "Expected legacy window overlay captureMethod values to migrate to Windows 10+.");
                      },
                      runMode);
}

void RunConfigLoadKeyRebindUnicodeStringParsedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_key_rebind_unicode_string_parsed",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[keyRebinds]
enabled = true
resolveRebindTargetsForHotkeys = false
toggleHotkey = []

[[keyRebinds.rebinds]]
fromKey = 74
toKey = 75
enabled = true
useCustomOutput = true
customOutputUnicode = "U+00F8"
shiftLayerEnabled = true
shiftLayerOutputUnicode = "{00D8}"
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-key-rebind-unicode-string-parsed");
                          Expect(g_config.keyRebinds.rebinds.size() == 1, "Expected exactly one key rebind fixture to load.");
                          const KeyRebind& rebind = g_config.keyRebinds.rebinds.front();
                          Expect(rebind.customOutputUnicode == 0x00F8,
                                 "Expected customOutputUnicode strings to parse into Unicode code points.");
                          Expect(rebind.shiftLayerOutputUnicode == 0x00D8,
                                 "Expected shiftLayerOutputUnicode strings to parse into Unicode code points.");
                      },
                      runMode);
}

void RunConfigLoadKeyRebindEscapedUnicodeStringParsedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_key_rebind_escaped_unicode_string_parsed",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[keyRebinds]
enabled = true
resolveRebindTargetsForHotkeys = false
toggleHotkey = []

[[keyRebinds.rebinds]]
fromKey = 74
toKey = 75
enabled = true
useCustomOutput = true
customOutputUnicode = "\\u00f8"
shiftLayerEnabled = true
shiftLayerOutputUnicode = "\\U00D8"
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-key-rebind-escaped-unicode-string-parsed");
                          Expect(g_config.keyRebinds.rebinds.size() == 1, "Expected exactly one key rebind fixture to load.");
                          const KeyRebind& rebind = g_config.keyRebinds.rebinds.front();
                          Expect(rebind.customOutputUnicode == 0x00F8,
                                 "Expected escaped customOutputUnicode strings to parse into Unicode code points.");
                          Expect(rebind.shiftLayerOutputUnicode == 0x00D8,
                                 "Expected escaped shiftLayerOutputUnicode strings to parse into Unicode code points.");
                      },
                      runMode);
}

void RunConfigLoadKeyRebindHexUnicodeStringParsedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_key_rebind_hex_unicode_string_parsed",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[keyRebinds]
enabled = true
resolveRebindTargetsForHotkeys = false
toggleHotkey = []

[[keyRebinds.rebinds]]
fromKey = 74
toKey = 75
enabled = true
useCustomOutput = true
customOutputUnicode = " 0x00f8 "
shiftLayerEnabled = true
shiftLayerOutputUnicode = " 0X00D8 "
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-key-rebind-hex-unicode-string-parsed");
                          Expect(g_config.keyRebinds.rebinds.size() == 1, "Expected exactly one key rebind fixture to load.");
                          const KeyRebind& rebind = g_config.keyRebinds.rebinds.front();
                          Expect(rebind.customOutputUnicode == 0x00F8,
                                 "Expected hexadecimal customOutputUnicode strings to parse into Unicode code points.");
                          Expect(rebind.shiftLayerOutputUnicode == 0x00D8,
                                 "Expected hexadecimal shiftLayerOutputUnicode strings to parse into Unicode code points.");
                      },
                      runMode);
}

void RunConfigLoadKeyRebindInvalidUnicodeDefaultedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_key_rebind_invalid_unicode_defaulted",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[keyRebinds]
enabled = true
resolveRebindTargetsForHotkeys = false
toggleHotkey = []

[[keyRebinds.rebinds]]
fromKey = 74
toKey = 75
enabled = true
useCustomOutput = true
customOutputUnicode = "bogus"
shiftLayerEnabled = true
shiftLayerOutputUnicode = "U+D800"
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-key-rebind-invalid-unicode-defaulted");
                          Expect(g_config.keyRebinds.rebinds.size() == 1, "Expected exactly one key rebind fixture to load.");
                          const KeyRebind& rebind = g_config.keyRebinds.rebinds.front();
                          Expect(rebind.customOutputUnicode == ConfigDefaults::KEY_REBIND_CUSTOM_OUTPUT_UNICODE,
                                 "Expected invalid customOutputUnicode strings to fall back to the configured default.");
                          Expect(rebind.shiftLayerOutputUnicode == ConfigDefaults::KEY_REBIND_SHIFT_LAYER_OUTPUT_UNICODE,
                                 "Expected invalid shiftLayerOutputUnicode strings to fall back to the configured default.");
                      },
                      runMode);
}

void RunConfigLoadFullscreenStretchRepairedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_fullscreen_stretch_repaired",
                      []() {
                          Config config;
                          config.defaultMode = "Fullscreen";

                          ModeConfig fullscreenMode;
                          fullscreenMode.id = "Fullscreen";
                          fullscreenMode.width = 0;
                          fullscreenMode.height = 0;
                          fullscreenMode.manualWidth = 0;
                          fullscreenMode.manualHeight = 0;
                          fullscreenMode.stretch.enabled = false;
                          fullscreenMode.stretch.x = 33;
                          fullscreenMode.stretch.y = 44;
                          fullscreenMode.stretch.width = 55;
                          fullscreenMode.stretch.height = 66;
                          config.modes.push_back(fullscreenMode);

                          WriteConfigFixtureToDisk(config);
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-fullscreen-stretch-repaired");
                          const ModeConfig& fullscreenMode = FindModeOrThrow("Fullscreen");
                          Expect(fullscreenMode.width > 0 && fullscreenMode.height > 0,
                                 "Expected Fullscreen mode dimensions to repair to valid values.");
                          Expect(fullscreenMode.manualWidth == fullscreenMode.width && fullscreenMode.manualHeight == fullscreenMode.height,
                                 "Expected Fullscreen manual dimensions to repair alongside the live dimensions.");
                          Expect(fullscreenMode.stretch.enabled, "Expected Fullscreen stretch to be forced on during load.");
                          Expect(fullscreenMode.stretch.x == 0 && fullscreenMode.stretch.y == 0,
                                 "Expected Fullscreen stretch origin to reset to the top-left corner.");
                          Expect(fullscreenMode.stretch.width == fullscreenMode.width &&
                                     fullscreenMode.stretch.height == fullscreenMode.height,
                                 "Expected Fullscreen stretch bounds to match the repaired mode dimensions.");
                      },
                      runMode);
}

void RunConfigLoadPreemptiveSyncExistingModeTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_preemptive_sync_existing_mode",
                      []() {
                          Config config;
                          config.defaultMode = "EyeZoom";

                          ModeConfig eyezoomMode;
                          eyezoomMode.id = "EyeZoom";
                          eyezoomMode.width = 640;
                          eyezoomMode.height = 1200;
                          eyezoomMode.manualWidth = 640;
                          eyezoomMode.manualHeight = 1200;
                          config.modes.push_back(eyezoomMode);

                          ModeConfig preemptiveMode;
                          preemptiveMode.id = "Preemptive";
                          preemptiveMode.width = 123;
                          preemptiveMode.height = 456;
                          preemptiveMode.manualWidth = 123;
                          preemptiveMode.manualHeight = 456;
                          preemptiveMode.useRelativeSize = true;
                          preemptiveMode.relativeWidth = 0.5f;
                          preemptiveMode.relativeHeight = 0.25f;
                          config.modes.push_back(preemptiveMode);

                          WriteConfigFixtureToDisk(config);
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-preemptive-sync-existing-mode");
                          const ModeConfig& eyezoomMode = FindModeOrThrow("EyeZoom");
                          const ModeConfig& preemptiveMode = FindModeOrThrow("Preemptive");
                          Expect(preemptiveMode.width == eyezoomMode.width && preemptiveMode.height == eyezoomMode.height,
                                 "Expected Preemptive mode dimensions to sync to EyeZoom during load.");
                          Expect(preemptiveMode.manualWidth == eyezoomMode.manualWidth &&
                                     preemptiveMode.manualHeight == eyezoomMode.manualHeight,
                                 "Expected Preemptive manual dimensions to sync to EyeZoom during load.");
                          Expect(!preemptiveMode.useRelativeSize && preemptiveMode.relativeWidth < 0.0f &&
                                     preemptiveMode.relativeHeight < 0.0f,
                                 "Expected Preemptive relative sizing to clear when it is resynced from EyeZoom.");
                      },
                      runMode);
}

void RunConfigLoadThinMinWidthEnforcedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_thin_min_width_enforced",
                      []() {
                          Config config;
                          config.defaultMode = "Thin";

                          ModeConfig thinMode;
                          thinMode.id = "Thin";
                          thinMode.width = 100;
                          thinMode.height = 700;
                          thinMode.manualWidth = 100;
                          thinMode.manualHeight = 700;
                          config.modes.push_back(thinMode);

                          WriteConfigFixtureToDisk(config);
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-thin-min-width-enforced");
                          const ModeConfig& thinMode = FindModeOrThrow("Thin");
                          Expect(thinMode.width == 330, "Expected Thin mode width to clamp to the hard minimum during load.");
                      },
                      runMode);
}

void RunConfigLoadBrowserOverlayDefaultsTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_browser_overlay_defaults",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[[browserOverlay]]
name = "Default Browser"
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-browser-overlay-defaults");
                          const BrowserOverlayConfig& overlay = FindBrowserOverlayOrThrow("Default Browser");
                          Expect(overlay.url == "https://example.com", "Expected browser overlay URL to default when omitted.");
                          Expect(overlay.browserWidth == ConfigDefaults::BROWSER_OVERLAY_WIDTH,
                                 "Expected browser overlay width to default when omitted.");
                          Expect(overlay.browserHeight == ConfigDefaults::BROWSER_OVERLAY_HEIGHT,
                                 "Expected browser overlay height to default when omitted.");
                          Expect(overlay.transparentBackground == ConfigDefaults::BROWSER_OVERLAY_TRANSPARENT_BACKGROUND,
                                 "Expected browser overlay transparentBackground to default when omitted.");
                          Expect(overlay.reloadInterval == ConfigDefaults::BROWSER_OVERLAY_RELOAD_INTERVAL,
                                 "Expected browser overlay reloadInterval to default when omitted.");
                      },
                      runMode);
}

void RunConfigLoadWindowOverlayDefaultsTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_window_overlay_defaults",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[[windowOverlay]]
name = "Default Window"
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-window-overlay-defaults");
                          const WindowOverlayConfig& overlay = FindWindowOverlayOrThrow("Default Window");
                          Expect(overlay.windowMatchPriority == ConfigDefaults::WINDOW_OVERLAY_MATCH_PRIORITY,
                                 "Expected window overlay match priority to default when omitted.");
                          Expect(overlay.captureMethod == ConfigDefaults::WINDOW_OVERLAY_CAPTURE_METHOD,
                                 "Expected window overlay captureMethod to default when omitted.");
                          Expect(overlay.fps == ConfigDefaults::WINDOW_OVERLAY_FPS,
                                 "Expected window overlay FPS to default when omitted.");
                          Expect(overlay.searchInterval == ConfigDefaults::WINDOW_OVERLAY_SEARCH_INTERVAL,
                                 "Expected window overlay searchInterval to default when omitted.");
                          Expect(overlay.enableInteraction == ConfigDefaults::WINDOW_OVERLAY_ENABLE_INTERACTION,
                                 "Expected window overlay enableInteraction to default when omitted.");
                      },
                      runMode);
}

void RunConfigLoadImageDefaultsTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_image_defaults",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[[image]]
name = "Default Image"
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-image-defaults");
                          const ImageConfig& image = FindImageOrThrow("Default Image");
                          ExpectFloatNear(image.scale, ConfigDefaults::IMAGE_SCALE, "Expected image scale to default when omitted.");
                          Expect(image.relativeSizing == ConfigDefaults::IMAGE_RELATIVE_SIZING,
                                 "Expected image relativeSizing to default when omitted.");
                          Expect(image.relativeTo == ConfigDefaults::IMAGE_RELATIVE_TO,
                                 "Expected image relativeTo to default when omitted.");
                          ExpectFloatNear(image.opacity, ConfigDefaults::IMAGE_OPACITY,
                                          "Expected image opacity to default when omitted.");
                          Expect(image.onlyOnMyScreen == ConfigDefaults::IMAGE_ONLY_ON_MY_SCREEN,
                                 "Expected image onlyOnMyScreen to default when omitted.");
                      },
                      runMode);
}

void RunConfigLoadHotkeyDefaultFlagsTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_hotkey_default_flags",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[[mode]]
id = "Target Mode"
width = 800
height = 600

[[hotkey]]
keys = [65]
mainMode = "Fullscreen"
secondaryMode = "Target Mode"
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-hotkey-default-flags");
                          const HotkeyConfig& hotkey = FindHotkeyByKeysOrThrow({ 65 });
                          Expect(hotkey.debounce == ConfigDefaults::HOTKEY_DEBOUNCE,
                                 "Expected hotkey debounce to default when omitted.");
                          Expect(!hotkey.triggerOnRelease && !hotkey.triggerOnHold,
                                 "Expected hotkey trigger flags to default to false when omitted.");
                          Expect(!hotkey.blockKeyFromGame && !hotkey.allowExitToFullscreenRegardlessOfGameState,
                                 "Expected hotkey behavior flags to default to false when omitted.");
                          Expect(hotkey.conditions.gameState.empty() && hotkey.conditions.exclusions.empty(),
                                 "Expected omitted hotkey conditions to remain empty.");
                      },
                      runMode);
}

void RunConfigLoadSensitivityHotkeyDefaultFlagsTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_sensitivity_hotkey_default_flags",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[[sensitivityHotkey]]
keys = [70]
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-sensitivity-hotkey-default-flags");
                          const SensitivityHotkeyConfig& hotkey = FindSensitivityHotkeyOrThrow({ 70 });
                          ExpectFloatNear(hotkey.sensitivity, 1.0f, "Expected sensitivity hotkey sensitivity to default when omitted.");
                          Expect(!hotkey.separateXY, "Expected sensitivity hotkey separateXY to default to false when omitted.");
                          ExpectFloatNear(hotkey.sensitivityX, 1.0f,
                                          "Expected sensitivity hotkey sensitivityX to default when omitted.");
                          ExpectFloatNear(hotkey.sensitivityY, 1.0f,
                                          "Expected sensitivity hotkey sensitivityY to default when omitted.");
                          Expect(hotkey.debounce == ConfigDefaults::HOTKEY_DEBOUNCE,
                                 "Expected sensitivity hotkey debounce to default when omitted.");
                          Expect(!hotkey.toggle, "Expected sensitivity hotkey toggle to default to false when omitted.");
                      },
                      runMode);
}

void RunConfigLoadImageColorKeyDefaultSensitivityTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_image_color_key_default_sensitivity",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[[image]]
name = "Color Key Image"
colorKeys = [{ color = [255, 0, 255] }]
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-image-color-key-default-sensitivity");
                          const ImageConfig& image = FindImageOrThrow("Color Key Image");
                          Expect(image.colorKeys.size() == 1, "Expected image color key fixture to load.");
                          ExpectFloatNear(image.colorKeys.front().sensitivity, ConfigDefaults::COLOR_KEY_SENSITIVITY,
                                          "Expected image color key sensitivity to default when omitted.");
                      },
                      runMode);
}

void RunConfigLoadWindowOverlayColorKeyDefaultSensitivityTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_window_overlay_color_key_default_sensitivity",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[[windowOverlay]]
name = "Color Key Window"
colorKeys = [{ color = [0, 255, 0] }]
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-window-overlay-color-key-default-sensitivity");
                          const WindowOverlayConfig& overlay = FindWindowOverlayOrThrow("Color Key Window");
                          Expect(overlay.colorKeys.size() == 1, "Expected window overlay color key fixture to load.");
                          ExpectFloatNear(overlay.colorKeys.front().sensitivity, ConfigDefaults::COLOR_KEY_SENSITIVITY,
                                          "Expected window overlay color key sensitivity to default when omitted.");
                      },
                      runMode);
}

void RunConfigLoadBrowserOverlayColorKeyDefaultSensitivityTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_browser_overlay_color_key_default_sensitivity",
                      []() {
                          WriteRawConfigTomlToDisk(R"(configVersion = 4
defaultMode = "Fullscreen"

[[browserOverlay]]
name = "Color Key Browser"
colorKeys = [{ color = [0, 0, 0] }]
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-browser-overlay-color-key-default-sensitivity");
                          const BrowserOverlayConfig& overlay = FindBrowserOverlayOrThrow("Color Key Browser");
                          Expect(overlay.colorKeys.size() == 1, "Expected browser overlay color key fixture to load.");
                          ExpectFloatNear(overlay.colorKeys.front().sensitivity, ConfigDefaults::COLOR_KEY_SENSITIVITY,
                                          "Expected browser overlay color key sensitivity to default when omitted.");
                      },
                      runMode);
}

void RunConfigErrorGuiTest(TestRunMode runMode = TestRunMode::Automated) {
    DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);
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

    if (runMode == TestRunMode::Visual) {
        RunVisualLoop(window, "config-error-gui", &RenderInteractiveConfigErrorFrame);
        return;
    }

    RenderConfigErrorFrame(window);
}

void RunSettingsGuiBasicTest(TestRunMode runMode = TestRunMode::Automated) {
    DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);
    const std::filesystem::path root = PrepareCaseDirectory("settings_gui_basic");
    ResetGlobalTestState(root);

    LoadConfig();
    Expect(!g_configLoadFailed.load(std::memory_order_acquire), "Expected config load to succeed before basic GUI rendering.");

    g_config.basicModeEnabled = true;
    g_configIsDirty.store(false, std::memory_order_release);

    if (runMode == TestRunMode::Visual) {
        RunVisualLoop(window, "settings-gui-basic", &RenderInteractiveSettingsFrame);
        return;
    }

    const std::vector<std::string> tabs = {
        tr("tabs.general"),
        tr("tabs.other"),
        tr("tabs.supporters"),
    };
    for (const std::string& tab : tabs) {
        RenderSettingsFrame(window, tab.c_str());
    }
}

void RunModeMirrorRenderScreenAnchorsTest(TestRunMode runMode = TestRunMode::Automated) {
    DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);

    const std::filesystem::path root = PrepareCaseDirectory("mode_mirror_render_screen_anchors");
    ResetGlobalTestState(root);

    constexpr char kModeId[] = "Mirror Screen Anchors Mode";

    MirrorConfig topLeftMirror = MakeMirrorRenderTestConfig("Top Left Mirror", 18, 12, "topLeftScreen", 30, 40, 4.0f);
    MirrorConfig topRightMirror = MakeMirrorRenderTestConfig("Top Right Mirror", 15, 15, "topRightScreen", 55, 35, 4.0f);
    MirrorConfig bottomLeftMirror = MakeMirrorRenderTestConfig("Bottom Left Mirror", 20, 10, "bottomLeftScreen", 70, 45, 4.0f);
    MirrorConfig bottomRightMirror = MakeMirrorRenderTestConfig("Bottom Right Mirror", 21, 14, "bottomRightScreen", 50, 60, 4.0f);

    ModeConfig mode;
    mode.id = kModeId;
    mode.width = kWindowWidth;
    mode.height = kWindowHeight;
    mode.manualWidth = kWindowWidth;
    mode.manualHeight = kWindowHeight;
    mode.mirrorIds = { topLeftMirror.name, topRightMirror.name, bottomLeftMirror.name, bottomRightMirror.name };

    g_config.defaultMode = kModeId;
    g_config.mirrors = { topLeftMirror, topRightMirror, bottomLeftMirror, bottomRightMirror };
    g_config.modes = { mode };
    g_configLoaded.store(true, std::memory_order_release);

    InitializeMirrorRenderTestResources();

    const SurfaceSize surface = GetWindowClientSize(window.hwnd());
    ScopedTexture2D sourceTexture(surface.width, surface.height, MakeSolidRgbaPixels(surface.width, surface.height, 0, 255, 0));

    auto renderAndAssert = [&](DummyWindow& targetWindow) {
        RenderModeOverlayFrame(targetWindow, g_config, g_config.modes.front(), sourceTexture.id());
        if (runMode == TestRunMode::Automated) {
            std::vector<MirrorConfig> activeMirrors;
            std::vector<ImageConfig> unusedImages;
            std::vector<WindowOverlayConfig> unusedWindowOverlays;
            std::vector<BrowserOverlayConfig> unusedBrowserOverlays;
            CollectActiveElementsForMode(g_config, kModeId, false, activeMirrors, unusedImages, unusedWindowOverlays,
                                         unusedBrowserOverlays);

            Expect(activeMirrors.size() == 4, "Expected all direct mirror sources to resolve for the screen-anchor test.");
            Expect(activeMirrors[0].name == topLeftMirror.name, "Expected the first direct mirror source to remain ordered.");
            Expect(activeMirrors[1].name == topRightMirror.name, "Expected the second direct mirror source to remain ordered.");
            Expect(activeMirrors[2].name == bottomLeftMirror.name, "Expected the third direct mirror source to remain ordered.");
            Expect(activeMirrors[3].name == bottomRightMirror.name, "Expected the fourth direct mirror source to remain ordered.");

            const ExpectedMirrorRect expectedTopLeftRect = ComputeExpectedMirrorRect(activeMirrors[0], surface.width, surface.height,
                                                                                     0, 0, surface.width, surface.height);
            const ExpectedMirrorRect expectedTopRightRect = ComputeExpectedMirrorRect(activeMirrors[1], surface.width, surface.height,
                                                                                      0, 0, surface.width, surface.height);
            const ExpectedMirrorRect expectedBottomLeftRect = ComputeExpectedMirrorRect(activeMirrors[2], surface.width, surface.height,
                                                                                        0, 0, surface.width, surface.height);
            const ExpectedMirrorRect expectedBottomRightRect = ComputeExpectedMirrorRect(activeMirrors[3], surface.width,
                                                                                         surface.height, 0, 0, surface.width,
                                                                                         surface.height);
            const ExpectedMirrorRect topLeftRect = GetCachedMirrorRect(activeMirrors[0].name);
            const ExpectedMirrorRect topRightRect = GetCachedMirrorRect(activeMirrors[1].name);
            const ExpectedMirrorRect bottomLeftRect = GetCachedMirrorRect(activeMirrors[2].name);
            const ExpectedMirrorRect bottomRightRect = GetCachedMirrorRect(activeMirrors[3].name);

            ExpectMirrorRectNear(topLeftRect, expectedTopLeftRect, "Top-left mirror cached bounds");
            ExpectMirrorRectNear(topRightRect, expectedTopRightRect, "Top-right mirror cached bounds");
            ExpectMirrorRectNear(bottomLeftRect, expectedBottomLeftRect, "Bottom-left mirror cached bounds");
            ExpectMirrorRectNear(bottomRightRect, expectedBottomRightRect, "Bottom-right mirror cached bounds");

            ExpectSolidColorRect(topLeftRect, surface.height, kExpectedMirrorRenderGreen,
                                 "Expected the top-left screen mirror to draw the staged game texture.");
            ExpectSolidColorRect(topRightRect, surface.height, kExpectedMirrorRenderGreen,
                                 "Expected the top-right screen mirror to draw the staged game texture.");
            ExpectSolidColorRect(bottomLeftRect, surface.height, kExpectedMirrorRenderGreen,
                                 "Expected the bottom-left screen mirror to draw the staged game texture.");
            ExpectSolidColorRect(bottomRightRect, surface.height, kExpectedMirrorRenderGreen,
                                 "Expected the bottom-right screen mirror to draw the staged game texture.");

            ExpectBackgroundPixel(topLeftRect.x - 2, topLeftRect.y + topLeftRect.height / 2, surface.height,
                                  "Expected pixels just left of the top-left mirror to remain background.");
            ExpectBackgroundPixel(topRightRect.x + topRightRect.width + 2, topRightRect.y + topRightRect.height / 2, surface.height,
                                  "Expected pixels just right of the top-right mirror to remain background.");
            ExpectBackgroundPixel(bottomLeftRect.x - 2, bottomLeftRect.y + bottomLeftRect.height / 2, surface.height,
                                  "Expected pixels just left of the bottom-left mirror to remain background.");
            ExpectBackgroundPixel(bottomRightRect.x + bottomRightRect.width / 2, bottomRightRect.y - 2, surface.height,
                                  "Expected pixels just above the bottom-right mirror to remain background.");
        }
    };

    if (runMode == TestRunMode::Visual) {
        RunVisualLoop(window, "mode-mirror-render-screen-anchors", [&](DummyWindow& visualWindow) { renderAndAssert(visualWindow); });
    } else {
        renderAndAssert(window);
    }

    CleanupBrowserOverlayCache();
    CleanupWindowOverlayCache();
    CleanupGPUResources();
    CleanupShaders();
}

void RunModeMirrorRenderViewportAnchorsTest(TestRunMode runMode = TestRunMode::Automated) {
    DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);

    const std::filesystem::path root = PrepareCaseDirectory("mode_mirror_render_viewport_anchors");
    ResetGlobalTestState(root);

    constexpr char kModeId[] = "Mirror Viewport Anchors Mode";

    MirrorConfig centerMirror = MakeMirrorRenderTestConfig("Center Viewport Mirror", 15, 12, "centerViewport", 25, -30, 5.0f);
    MirrorConfig topRightMirror = MakeMirrorRenderTestConfig("Top Right Viewport Mirror", 12, 10, "topRightViewport", 35, 24, 6.0f);

    ModeConfig mode;
    mode.id = kModeId;
    mode.width = kWindowWidth;
    mode.height = kWindowHeight;
    mode.manualWidth = kWindowWidth;
    mode.manualHeight = kWindowHeight;
    mode.mirrorIds = { centerMirror.name, topRightMirror.name };

    g_config.defaultMode = kModeId;
    g_config.mirrors = { centerMirror, topRightMirror };
    g_config.modes = { mode };
    g_configLoaded.store(true, std::memory_order_release);

    InitializeMirrorRenderTestResources();

    const SurfaceSize surface = GetWindowClientSize(window.hwnd());
    ScopedTexture2D sourceTexture(surface.width, surface.height, MakeSolidRgbaPixels(surface.width, surface.height, 0, 255, 0));

    auto renderAndAssert = [&](DummyWindow& targetWindow) {
        RenderModeOverlayFrame(targetWindow, g_config, g_config.modes.front(), sourceTexture.id());
        if (runMode == TestRunMode::Automated) {
            std::vector<MirrorConfig> activeMirrors;
            std::vector<ImageConfig> unusedImages;
            std::vector<WindowOverlayConfig> unusedWindowOverlays;
            std::vector<BrowserOverlayConfig> unusedBrowserOverlays;
            CollectActiveElementsForMode(g_config, kModeId, false, activeMirrors, unusedImages, unusedWindowOverlays,
                                         unusedBrowserOverlays);

            Expect(activeMirrors.size() == 2, "Expected both viewport-relative mirrors to resolve for the viewport-anchor test.");

            const ExpectedMirrorRect expectedCenterRect = ComputeExpectedMirrorRect(activeMirrors[0], surface.width, surface.height,
                                                                                    0, 0, surface.width, surface.height);
            const ExpectedMirrorRect expectedTopRightRect = ComputeExpectedMirrorRect(activeMirrors[1], surface.width,
                                                                                      surface.height, 0, 0, surface.width,
                                                                                      surface.height);
            const ExpectedMirrorRect centerRect = GetCachedMirrorRect(activeMirrors[0].name);
            const ExpectedMirrorRect topRightRect = GetCachedMirrorRect(activeMirrors[1].name);

            ExpectMirrorRectNear(centerRect, expectedCenterRect, "Center-viewport mirror cached bounds", 3);
            ExpectMirrorRectNear(topRightRect, expectedTopRightRect, "Top-right viewport mirror cached bounds", 3);

            ExpectSolidColorRect(centerRect, surface.height, kExpectedMirrorRenderGreen,
                                 "Expected the center-viewport mirror to render at the viewport center offset.");
            ExpectSolidColorRect(topRightRect, surface.height, kExpectedMirrorRenderGreen,
                                 "Expected the top-right viewport mirror to render at the viewport anchor.");

            ExpectBackgroundPixel(centerRect.x + centerRect.width / 2, centerRect.y - 2, surface.height,
                                  "Expected pixels above the center-viewport mirror to remain background.");
            ExpectBackgroundPixel(topRightRect.x - 2, topRightRect.y + topRightRect.height / 2, surface.height,
                                  "Expected pixels left of the top-right viewport mirror to remain background.");
        }
    };

    if (runMode == TestRunMode::Visual) {
        RunVisualLoop(window, "mode-mirror-render-viewport-anchors", [&](DummyWindow& visualWindow) { renderAndAssert(visualWindow); });
    } else {
        renderAndAssert(window);
    }

    CleanupBrowserOverlayCache();
    CleanupWindowOverlayCache();
    CleanupGPUResources();
    CleanupShaders();
}

void RunModeMirrorRenderScreenAnchorSizeMatrixTest(TestRunMode runMode = TestRunMode::Automated) {
    DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);

    const std::filesystem::path root = PrepareCaseDirectory("mode_mirror_render_screen_anchor_size_matrix");
    ResetGlobalTestState(root);

    constexpr char kModeId[] = "Mirror Screen Anchor Size Matrix Mode";

    const std::vector<MirrorAnchorRenderScenario> scenarios = {
        {
            { "screen-medium", 257, 193, 0, 0, 257, 193 },
            {
                { "Screen Medium Top Left", "topLeftScreen", 7, 5, 9, 11, 3.0f },
                { "Screen Medium Top Right Pixel", "topRightScreen", 1, 1, 13, 17, 2.0f },
                { "Screen Medium Bottom Left", "bottomLeftScreen", 2, 5, 7, 9, 2.0f },
                { "Screen Medium Bottom Right", "bottomRightScreen", 5, 2, 15, 14, 3.0f },
                { "Screen Medium Center", "centerScreen", 4, 3, 5, -7, 4.0f },
            },
            true,
        },
        {
            { "screen-small", 149, 113, 0, 0, 149, 113 },
            {
                { "Screen Small Top Left", "topLeftScreen", 1, 1, 2, 2, 4.0f },
                { "Screen Small Top Right", "topRightScreen", 1, 1, 3, 2, 4.0f },
                { "Screen Small Bottom Left", "bottomLeftScreen", 1, 1, 2, 2, 4.0f },
                { "Screen Small Bottom Right", "bottomRightScreen", 1, 1, 2, 2, 4.0f },
                { "Screen Small Center", "centerScreen", 1, 1, 0, 0, 5.0f },
            },
            true,
        },
        {
            { "screen-single-pixel", 1, 1, 0, 0, 1, 1 },
            {
                { "Screen Pixel Top Left", "topLeftScreen", 1, 1, 0, 0, 1.0f },
                { "Screen Pixel Top Right", "topRightScreen", 1, 1, 0, 0, 1.0f },
                { "Screen Pixel Bottom Left", "bottomLeftScreen", 1, 1, 0, 0, 1.0f },
                { "Screen Pixel Bottom Right", "bottomRightScreen", 1, 1, 0, 0, 1.0f },
                { "Screen Pixel Center", "centerScreen", 1, 1, 0, 0, 1.0f },
            },
            false,
        },
    };

    for (const MirrorAnchorRenderScenario& scenario : scenarios) {
        g_config = Config();

        ModeConfig mode;
        mode.id = kModeId;
        mode.width = (std::max)(1, scenario.geometry.gameW);
        mode.height = (std::max)(1, scenario.geometry.gameH);
        mode.manualWidth = mode.width;
        mode.manualHeight = mode.height;

        g_config.defaultMode = kModeId;
        g_config.mirrors.clear();
        g_config.mirrors.reserve(scenario.mirrors.size());
        mode.mirrorIds.reserve(scenario.mirrors.size());
        for (const MirrorAnchorCaseDefinition& mirrorCase : scenario.mirrors) {
            g_config.mirrors.push_back(MakeMirrorRenderTestConfig(mirrorCase.name, mirrorCase.captureWidth,
                                                                  mirrorCase.captureHeight, mirrorCase.relativeTo,
                                                                  mirrorCase.outputX, mirrorCase.outputY, mirrorCase.scale));
            mode.mirrorIds.push_back(mirrorCase.name);
        }

        g_config.modes = { mode };
        g_configLoaded.store(true, std::memory_order_release);

        InitializeMirrorRenderTestResources();
        auto assertScenario = [&](const SimulatedOverlayGeometry& geometry, const SurfaceSize& surface) {
            std::vector<MirrorConfig> activeMirrors;
            std::vector<ImageConfig> unusedImages;
            std::vector<WindowOverlayConfig> unusedWindowOverlays;
            std::vector<BrowserOverlayConfig> unusedBrowserOverlays;
            CollectActiveElementsForMode(g_config, kModeId, false, activeMirrors, unusedImages, unusedWindowOverlays,
                                         unusedBrowserOverlays);

            Expect(activeMirrors.size() == scenario.mirrors.size(),
                   geometry.label + " should resolve every screen-anchor mirror in the scenario.");

            for (const MirrorConfig& activeMirror : activeMirrors) {
                ExpectMirrorRenderMatchesExpectedPlacement(activeMirror, geometry, surface,
                                                           geometry.label + " render output for " + activeMirror.name,
                                                           scenario.expectVisibleRender);
            }
        };

        if (scenario.expectVisibleRender) {
            DummyWindow scenarioWindow(scenario.geometry.fullW, scenario.geometry.fullH, false);
            const SurfaceSize surface = GetWindowClientSize(scenarioWindow.hwnd());
            SimulatedOverlayGeometry geometry = scenario.geometry;
            geometry.fullW = surface.width;
            geometry.fullH = surface.height;
            geometry.gameW = surface.width;
            geometry.gameH = surface.height;

            ScopedTexture2D sourceTexture(surface.width, surface.height,
                                          MakeSolidRgbaPixels(surface.width, surface.height, 0, 255, 0));

            RenderModeOverlayFrameWithGeometry(scenarioWindow, g_config, g_config.modes.front(), geometry,
                                               sourceTexture.id(), [&](const SurfaceSize& renderSurface) {
                assertScenario(geometry, renderSurface);
            });
        } else {
            ScopedTexture2D sourceTexture((std::max)(1, scenario.geometry.fullW), (std::max)(1, scenario.geometry.fullH),
                                          MakeSolidRgbaPixels((std::max)(1, scenario.geometry.fullW),
                                                              (std::max)(1, scenario.geometry.fullH), 0, 255, 0));

            RenderModeOverlayFrameToSimulatedSurface(window, g_config, g_config.modes.front(), scenario.geometry,
                                                     sourceTexture.id(), [&](const SurfaceSize& surface) {
                assertScenario(scenario.geometry, surface);
            });
        }
    }

    CleanupBrowserOverlayCache();
    CleanupWindowOverlayCache();
    CleanupGPUResources();
    CleanupShaders();
}

void RunModeMirrorRenderViewportAnchorSizeMatrixTest(TestRunMode runMode = TestRunMode::Automated) {
    DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);

    const std::filesystem::path root = PrepareCaseDirectory("mode_mirror_render_viewport_anchor_size_matrix");
    ResetGlobalTestState(root);

    constexpr char kModeId[] = "Mirror Viewport Anchor Size Matrix Mode";

    const std::vector<MirrorAnchorRenderScenario> scenarios = {
        {
            { "viewport-medium", 301, 211, 47, 39, 167, 109 },
            {
                { "Viewport Medium Top Left", "topLeftViewport", 7, 5, 6, 8, 3.0f },
                { "Viewport Medium Top Right Pixel", "topRightViewport", 1, 1, 11, 9, 2.0f },
                { "Viewport Medium Bottom Left", "bottomLeftViewport", 3, 4, 7, 6, 2.0f },
                { "Viewport Medium Bottom Right", "bottomRightViewport", 4, 2, 9, 5, 3.0f },
                { "Viewport Medium Center", "centerViewport", 5, 3, 4, -6, 2.0f },
            },
            true,
        },
        {
            { "viewport-small", 171, 139, 23, 19, 83, 61 },
            {
                { "Viewport Small Top Left", "topLeftViewport", 1, 1, 2, 2, 4.0f },
                { "Viewport Small Top Right", "topRightViewport", 1, 1, 2, 2, 4.0f },
                { "Viewport Small Bottom Left", "bottomLeftViewport", 1, 1, 2, 2, 4.0f },
                { "Viewport Small Bottom Right", "bottomRightViewport", 1, 1, 2, 2, 4.0f },
                { "Viewport Small Center", "centerViewport", 1, 1, 0, 0, 5.0f },
            },
            true,
        },
        {
            { "viewport-single-pixel", 17, 15, 8, 7, 1, 1 },
            {
                { "Viewport Pixel Top Left", "topLeftViewport", 1, 1, 0, 0, 1.0f },
                { "Viewport Pixel Top Right", "topRightViewport", 1, 1, 0, 0, 1.0f },
                { "Viewport Pixel Bottom Left", "bottomLeftViewport", 1, 1, 0, 0, 1.0f },
                { "Viewport Pixel Bottom Right", "bottomRightViewport", 1, 1, 0, 0, 1.0f },
                { "Viewport Pixel Center", "centerViewport", 1, 1, 0, 0, 1.0f },
            },
            false,
        },
    };

    for (const MirrorAnchorRenderScenario& scenario : scenarios) {
        g_config = Config();

        ModeConfig mode;
        mode.id = kModeId;
        mode.width = (std::max)(1, scenario.geometry.gameW);
        mode.height = (std::max)(1, scenario.geometry.gameH);
        mode.manualWidth = mode.width;
        mode.manualHeight = mode.height;

        g_config.defaultMode = kModeId;
        g_config.mirrors.clear();
        g_config.mirrors.reserve(scenario.mirrors.size());
        mode.mirrorIds.reserve(scenario.mirrors.size());
        for (const MirrorAnchorCaseDefinition& mirrorCase : scenario.mirrors) {
            g_config.mirrors.push_back(MakeMirrorRenderTestConfig(mirrorCase.name, mirrorCase.captureWidth,
                                                                  mirrorCase.captureHeight, mirrorCase.relativeTo,
                                                                  mirrorCase.outputX, mirrorCase.outputY, mirrorCase.scale));
            mode.mirrorIds.push_back(mirrorCase.name);
        }

        g_config.modes = { mode };
        g_configLoaded.store(true, std::memory_order_release);

        InitializeMirrorRenderTestResources();
        auto assertScenario = [&](const SimulatedOverlayGeometry& geometry, const SurfaceSize& surface) {
            std::vector<MirrorConfig> activeMirrors;
            std::vector<ImageConfig> unusedImages;
            std::vector<WindowOverlayConfig> unusedWindowOverlays;
            std::vector<BrowserOverlayConfig> unusedBrowserOverlays;
            CollectActiveElementsForMode(g_config, kModeId, false, activeMirrors, unusedImages, unusedWindowOverlays,
                                         unusedBrowserOverlays);

            Expect(activeMirrors.size() == scenario.mirrors.size(),
                   geometry.label + " should resolve every viewport-anchor mirror in the scenario.");

            for (const MirrorConfig& activeMirror : activeMirrors) {
                ExpectMirrorRenderMatchesExpectedPlacement(activeMirror, geometry, surface,
                                                           geometry.label + " render output for " + activeMirror.name,
                                                           scenario.expectVisibleRender);
            }
        };

        if (scenario.expectVisibleRender) {
            DummyWindow scenarioWindow(scenario.geometry.fullW, scenario.geometry.fullH, false);
            const SurfaceSize surface = GetWindowClientSize(scenarioWindow.hwnd());
            SimulatedOverlayGeometry geometry = scenario.geometry;
            geometry.fullW = surface.width;
            geometry.fullH = surface.height;

            ScopedTexture2D sourceTexture(surface.width, surface.height,
                                          MakeSolidRgbaPixels(surface.width, surface.height, 0, 255, 0));

            RenderModeOverlayFrameWithGeometry(scenarioWindow, g_config, g_config.modes.front(), geometry,
                                               sourceTexture.id(), [&](const SurfaceSize& renderSurface) {
                assertScenario(geometry, renderSurface);
            });
        } else {
            ScopedTexture2D sourceTexture((std::max)(1, scenario.geometry.fullW), (std::max)(1, scenario.geometry.fullH),
                                          MakeSolidRgbaPixels((std::max)(1, scenario.geometry.fullW),
                                                              (std::max)(1, scenario.geometry.fullH), 0, 255, 0));

            RenderModeOverlayFrameToSimulatedSurface(window, g_config, g_config.modes.front(), scenario.geometry,
                                                     sourceTexture.id(), [&](const SurfaceSize& surface) {
                assertScenario(scenario.geometry, surface);
            });
        }
    }

    CleanupBrowserOverlayCache();
    CleanupWindowOverlayCache();
    CleanupGPUResources();
    CleanupShaders();
}

void RunModeMirrorGroupRenderTest(TestRunMode runMode = TestRunMode::Automated) {
    DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);

    const std::filesystem::path root = PrepareCaseDirectory("mode_mirror_group_render");
    ResetGlobalTestState(root);

    constexpr char kModeId[] = "Mirror Group Render Mode";
    constexpr char kGroupName[] = "Mirror Group Under Test";

    MirrorConfig leftMirror = MakeMirrorRenderTestConfig("Group Left Mirror", 24, 16, "topLeftScreen", 0, 0, 4.0f);
    MirrorConfig disabledMirror = MakeMirrorRenderTestConfig("Disabled Group Mirror", 14, 14, "topLeftScreen", 0, 0, 6.0f);

    MirrorGroupConfig group;
    group.name = kGroupName;
    group.output.x = 300;
    group.output.y = 220;
    group.output.relativeTo = "topLeftScreen";
    group.output.separateScale = true;
    group.output.scaleX = 1.0f;
    group.output.scaleY = 1.0f;
    group.mirrors = {
        { leftMirror.name, true, 0.5f, 0.5f, 10, 20 },
        { disabledMirror.name, false, 1.0f, 1.0f, 210, 30 },
    };

    ModeConfig mode;
    mode.id = kModeId;
    mode.width = kWindowWidth;
    mode.height = kWindowHeight;
    mode.manualWidth = kWindowWidth;
    mode.manualHeight = kWindowHeight;
    mode.mirrorGroupIds = { kGroupName };

    g_config.defaultMode = kModeId;
    g_config.mirrors = { leftMirror, disabledMirror };
    g_config.mirrorGroups = { group };
    g_config.modes = { mode };
    g_configLoaded.store(true, std::memory_order_release);

    InitializeMirrorRenderTestResources();

    const SurfaceSize surface = GetWindowClientSize(window.hwnd());
    ScopedTexture2D sourceTexture(surface.width, surface.height, MakeSolidRgbaPixels(surface.width, surface.height, 0, 255, 0));

    auto renderAndAssert = [&](DummyWindow& targetWindow) {
        RenderModeOverlayFrame(targetWindow, g_config, g_config.modes.front(), sourceTexture.id());
        if (runMode == TestRunMode::Automated) {
            std::vector<MirrorConfig> activeMirrors;
            std::vector<ImageConfig> unusedImages;
            std::vector<WindowOverlayConfig> unusedWindowOverlays;
            std::vector<BrowserOverlayConfig> unusedBrowserOverlays;
            CollectActiveElementsForMode(g_config, kModeId, false, activeMirrors, unusedImages, unusedWindowOverlays,
                                         unusedBrowserOverlays);

            Expect(activeMirrors.size() == 1, "Expected only enabled group mirrors to resolve for the mirror-group render test.");
            Expect(activeMirrors[0].name == leftMirror.name, "Expected the enabled group mirror to preserve group order.");

            const ExpectedMirrorRect expectedLeftRect = ComputeExpectedMirrorRect(activeMirrors[0], surface.width, surface.height, 0, 0,
                                                                                  surface.width, surface.height);
            const ExpectedMirrorRect leftRect = GetCachedMirrorRect(leftMirror.name);
            const MirrorConfig disabledGroupMirror = BuildExpectedGroupedMirrorConfig(disabledMirror, group, group.mirrors[1]);
            const ExpectedMirrorRect disabledRect = ComputeExpectedMirrorRect(disabledGroupMirror, surface.width, surface.height,
                                                                              0, 0, surface.width, surface.height);

            Expect(leftRect.x == expectedLeftRect.x && leftRect.y == expectedLeftRect.y && leftRect.width == expectedLeftRect.width &&
                       leftRect.height == expectedLeftRect.height,
                   "Expected the first mirror-group member cached bounds to match the grouped placement math.");

            ExpectSolidColorRect(leftRect, surface.height, kExpectedMirrorRenderGreen,
                                 "Expected the enabled mirror-group member to render its staged texture.");

            ExpectBackgroundPixel(leftRect.x - 2, leftRect.y + leftRect.height / 2, surface.height,
                                  "Expected pixels left of the enabled group mirror to remain background.");
            ExpectBackgroundPixel(disabledRect.x + disabledRect.width / 2, disabledRect.y + disabledRect.height / 2, surface.height,
                                  "Expected the disabled mirror-group item to remain absent from the render output.");
        }
    };

    if (runMode == TestRunMode::Visual) {
        RunVisualLoop(window, "mode-mirror-group-render", [&](DummyWindow& visualWindow) { renderAndAssert(visualWindow); });
    } else {
        renderAndAssert(window);
    }

    CleanupBrowserOverlayCache();
    CleanupWindowOverlayCache();
    CleanupGPUResources();
    CleanupShaders();
}

void RunModeWindowOverlayRenderTest(TestRunMode runMode = TestRunMode::Automated) {
    DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);

    const std::filesystem::path root = PrepareCaseDirectory("mode_window_overlay_render");
    ResetGlobalTestState(root);

    constexpr char kModeId[] = "Window Overlay Render Mode";
    constexpr char kOverlayName[] = "Window Overlay Render";
    constexpr int kOverlayX = 48;
    constexpr int kOverlayY = 64;

    WindowOverlayConfig overlay;
    overlay.name = kOverlayName;
    overlay.x = kOverlayX;
    overlay.y = kOverlayY;
    overlay.scale = 16.0f;
    overlay.relativeTo = "topLeftScreen";
    overlay.opacity = 1.0f;
    overlay.onlyOnMyScreen = false;
    overlay.pixelatedScaling = true;
    overlay.background.enabled = false;
    overlay.border.enabled = false;

    ModeConfig mode;
    mode.id = kModeId;
    mode.width = kWindowWidth;
    mode.height = kWindowHeight;
    mode.manualWidth = kWindowWidth;
    mode.manualHeight = kWindowHeight;
    mode.windowOverlayIds = { kOverlayName };

    g_config.defaultMode = kModeId;
    g_config.windowOverlays = { overlay };
    g_config.modes = { mode };
    g_configLoaded.store(true, std::memory_order_release);

    ResetOverlayRenderTestResources();
    Expect(StageWindowOverlayTestFrame(overlay, MakeSolidRgbaPixels(2, 2, 255, 64, 32), 2, 2),
           "Failed to stage synthetic window overlay pixels for integration testing.");

    auto renderAndAssert = [&](DummyWindow& targetWindow) {
        RenderModeOverlayFrame(targetWindow, g_config, g_config.modes.front());
        if (runMode == TestRunMode::Automated) {
            ExpectFramebufferPixelColorNear(kOverlayX + 12, kOverlayY + 12, GetCachedWindowHeight(),
                                            { 1.0f, 64.0f / 255.0f, 32.0f / 255.0f, 1.0f },
                                            "Expected the mode-assigned window overlay to render its staged texture color.");
        }
    };

    if (runMode == TestRunMode::Visual) {
        RunVisualLoop(window, "mode-window-overlay-render", [&](DummyWindow& visualWindow) { renderAndAssert(visualWindow); });
    } else {
        renderAndAssert(window);
    }

    CleanupBrowserOverlayCache();
    CleanupWindowOverlayCache();
    CleanupGPUResources();
    CleanupShaders();
}

void RunModeBrowserOverlayRenderTest(TestRunMode runMode = TestRunMode::Automated) {
    DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);

    const std::filesystem::path root = PrepareCaseDirectory("mode_browser_overlay_render");
    ResetGlobalTestState(root);

    constexpr char kModeId[] = "Browser Overlay Render Mode";
    constexpr char kOverlayName[] = "Browser Overlay Render";
    constexpr int kOverlayX = 132;
    constexpr int kOverlayY = 96;

    BrowserOverlayConfig overlay;
    overlay.name = kOverlayName;
    overlay.url = "https://example.com/render-test";
    overlay.browserWidth = 2;
    overlay.browserHeight = 2;
    overlay.x = kOverlayX;
    overlay.y = kOverlayY;
    overlay.scale = 18.0f;
    overlay.relativeTo = "topLeftScreen";
    overlay.opacity = 1.0f;
    overlay.onlyOnMyScreen = false;
    overlay.pixelatedScaling = true;
    overlay.background.enabled = false;
    overlay.border.enabled = false;

    ModeConfig mode;
    mode.id = kModeId;
    mode.width = kWindowWidth;
    mode.height = kWindowHeight;
    mode.manualWidth = kWindowWidth;
    mode.manualHeight = kWindowHeight;
    mode.browserOverlayIds = { kOverlayName };

    g_config.defaultMode = kModeId;
    g_config.browserOverlays = { overlay };
    g_config.modes = { mode };
    g_configLoaded.store(true, std::memory_order_release);

    ResetOverlayRenderTestResources();
    Expect(StageBrowserOverlayTestFrame(overlay, MakeSolidRgbaPixels(2, 2, 32, 192, 96), 2, 2),
           "Failed to stage synthetic browser overlay pixels for integration testing.");

    auto renderAndAssert = [&](DummyWindow& targetWindow) {
        RenderModeOverlayFrame(targetWindow, g_config, g_config.modes.front());
        if (runMode == TestRunMode::Automated) {
            ExpectFramebufferPixelColorNear(kOverlayX + 12, kOverlayY + 12, GetCachedWindowHeight(),
                                            { 32.0f / 255.0f, 192.0f / 255.0f, 96.0f / 255.0f, 1.0f },
                                            "Expected the mode-assigned browser overlay to render its staged texture color.");
        }
    };

    if (runMode == TestRunMode::Visual) {
        RunVisualLoop(window, "mode-browser-overlay-render", [&](DummyWindow& visualWindow) { renderAndAssert(visualWindow); });
    } else {
        renderAndAssert(window);
    }

    CleanupBrowserOverlayCache();
    CleanupWindowOverlayCache();
    CleanupGPUResources();
    CleanupShaders();
}

void RunSettingsGuiAdvancedTest(TestRunMode runMode = TestRunMode::Automated) {
    DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);
    const std::filesystem::path root = PrepareCaseDirectory("settings_gui_advanced");
    ResetGlobalTestState(root);

    LoadConfig();
    Expect(!g_configLoadFailed.load(std::memory_order_acquire), "Expected config load to succeed before advanced GUI rendering.");

    g_config.basicModeEnabled = false;
    g_configIsDirty.store(false, std::memory_order_release);

    if (runMode == TestRunMode::Visual) {
        RunVisualLoop(window, "settings-gui-advanced", &RenderInteractiveSettingsFrame);
        return;
    }

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

void RunSettingsTabGeneralPopulatedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunPopulatedSettingsTabCase("settings_tab_general_populated", tr("tabs.general"), std::string(), runMode);
}

void RunSettingsTabOtherPopulatedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunPopulatedSettingsTabCase("settings_tab_other_populated", tr("tabs.other"), std::string(), runMode);
}

void RunSettingsTabModesPopulatedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunPopulatedSettingsTabCase("settings_tab_modes_populated", tr("tabs.modes"), std::string(), runMode);
}

void RunSettingsTabMirrorsPopulatedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunPopulatedSettingsTabCase("settings_tab_mirrors_populated", tr("tabs.mirrors"), std::string(), runMode);
}

void RunSettingsTabImagesPopulatedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunPopulatedSettingsTabCase("settings_tab_images_populated", tr("tabs.images"), std::string(), runMode);
}

void RunSettingsTabWindowOverlaysPopulatedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunPopulatedSettingsTabCase("settings_tab_window_overlays_populated", tr("tabs.window_overlays"), std::string(), runMode);
}

void RunSettingsTabBrowserOverlaysPopulatedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunPopulatedSettingsTabCase("settings_tab_browser_overlays_populated", tr("tabs.browser_overlays"), std::string(), runMode);
}

void RunSettingsTabHotkeysPopulatedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunPopulatedSettingsTabCase("settings_tab_hotkeys_populated", tr("tabs.hotkeys"), std::string(), runMode);
}

void RunSettingsTabInputsMousePopulatedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunPopulatedSettingsTabCase("settings_tab_inputs_mouse_populated", tr("tabs.inputs"), tr("inputs.mouse"), runMode);
}

void RunSettingsTabInputsKeyboardPopulatedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunPopulatedSettingsTabCase("settings_tab_inputs_keyboard_populated", tr("tabs.inputs"), tr("inputs.keyboard"), runMode);
}

void RunSettingsTabSettingsPopulatedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunPopulatedSettingsTabCase("settings_tab_settings_populated", tr("tabs.settings"), std::string(), runMode);
}

void RunSettingsTabAppearancePopulatedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunPopulatedSettingsTabCase("settings_tab_appearance_populated", tr("tabs.appearance"), std::string(), runMode);
}

void RunSettingsTabMiscPopulatedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunPopulatedSettingsTabCase("settings_tab_misc_populated", tr("tabs.misc"), std::string(), runMode);
}

void RunSettingsTabSupportersPopulatedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunPopulatedSettingsTabCase("settings_tab_supporters_populated", tr("tabs.supporters"), std::string(), runMode);
}

void RunSettingsTabGeneralDefaultTest(TestRunMode runMode = TestRunMode::Automated) {
    RunDefaultSettingsTabCase("settings_tab_general_default", tr("tabs.general"), std::string(), true, runMode);
}

void RunSettingsTabOtherDefaultTest(TestRunMode runMode = TestRunMode::Automated) {
    RunDefaultSettingsTabCase("settings_tab_other_default", tr("tabs.other"), std::string(), true, runMode);
}

void RunSettingsTabSupportersDefaultTest(TestRunMode runMode = TestRunMode::Automated) {
    RunDefaultSettingsTabCase("settings_tab_supporters_default", tr("tabs.supporters"), std::string(), true, runMode);
}

void RunSettingsTabModesDefaultTest(TestRunMode runMode = TestRunMode::Automated) {
    RunDefaultSettingsTabCase("settings_tab_modes_default", tr("tabs.modes"), std::string(), false, runMode);
}

void RunSettingsTabMirrorsDefaultTest(TestRunMode runMode = TestRunMode::Automated) {
    RunDefaultSettingsTabCase("settings_tab_mirrors_default", tr("tabs.mirrors"), std::string(), false, runMode);
}

void RunSettingsTabImagesDefaultTest(TestRunMode runMode = TestRunMode::Automated) {
    RunDefaultSettingsTabCase("settings_tab_images_default", tr("tabs.images"), std::string(), false, runMode);
}

void RunSettingsTabWindowOverlaysDefaultTest(TestRunMode runMode = TestRunMode::Automated) {
    RunDefaultSettingsTabCase("settings_tab_window_overlays_default", tr("tabs.window_overlays"), std::string(), false, runMode);
}

void RunSettingsTabBrowserOverlaysDefaultTest(TestRunMode runMode = TestRunMode::Automated) {
    RunDefaultSettingsTabCase("settings_tab_browser_overlays_default", tr("tabs.browser_overlays"), std::string(), false,
                              runMode);
}

void RunSettingsTabHotkeysDefaultTest(TestRunMode runMode = TestRunMode::Automated) {
    RunDefaultSettingsTabCase("settings_tab_hotkeys_default", tr("tabs.hotkeys"), std::string(), false, runMode);
}

void RunSettingsTabInputsMouseDefaultTest(TestRunMode runMode = TestRunMode::Automated) {
    RunDefaultSettingsTabCase("settings_tab_inputs_mouse_default", tr("tabs.inputs"), tr("inputs.mouse"), false, runMode);
}

void RunSettingsTabInputsKeyboardDefaultTest(TestRunMode runMode = TestRunMode::Automated) {
    RunDefaultSettingsTabCase("settings_tab_inputs_keyboard_default", tr("tabs.inputs"), tr("inputs.keyboard"), false, runMode);
}

void RunSettingsTabSettingsDefaultTest(TestRunMode runMode = TestRunMode::Automated) {
    RunDefaultSettingsTabCase("settings_tab_settings_default", tr("tabs.settings"), std::string(), false, runMode);
}

void RunSettingsTabAppearanceDefaultTest(TestRunMode runMode = TestRunMode::Automated) {
    RunDefaultSettingsTabCase("settings_tab_appearance_default", tr("tabs.appearance"), std::string(), false, runMode);
}

void RunSettingsTabMiscDefaultTest(TestRunMode runMode = TestRunMode::Automated) {
    RunDefaultSettingsTabCase("settings_tab_misc_default", tr("tabs.misc"), std::string(), false, runMode);
}

struct TestCaseDefinition {
    const char* name;
    void (*run)(TestRunMode runMode);
};

const auto& GetTestCaseDefinitions() {
    static const std::vector<TestCaseDefinition> testCases = {
        {"config-default-load", &RunConfigDefaultLoadTest},
        {"config-roundtrip", &RunConfigRoundtripTest},
        {"config-roundtrip-global-settings", &RunConfigRoundtripGlobalSettingsTest},
        {"config-roundtrip-modes", &RunConfigRoundtripModesTest},
        {"config-roundtrip-mirrors", &RunConfigRoundtripMirrorsTest},
        {"config-roundtrip-mirror-groups", &RunConfigRoundtripMirrorGroupsTest},
        {"config-roundtrip-images", &RunConfigRoundtripImagesTest},
        {"config-roundtrip-window-overlays", &RunConfigRoundtripWindowOverlaysTest},
        {"config-roundtrip-browser-overlays", &RunConfigRoundtripBrowserOverlaysTest},
        {"config-roundtrip-hotkeys", &RunConfigRoundtripHotkeysTest},
        {"config-roundtrip-sensitivity-hotkeys", &RunConfigRoundtripSensitivityHotkeysTest},
        {"config-roundtrip-cursors-eyezoom", &RunConfigRoundtripCursorsAndEyeZoomTest},
        {"config-roundtrip-key-rebinds-appearance", &RunConfigRoundtripKeyRebindsAndAppearanceTest},
        {"config-roundtrip-debug-settings", &RunConfigRoundtripDebugSettingsTest},
        {"config-load-missing-required-modes", &RunConfigLoadMissingRequiredModesTest},
        {"config-load-invalid-hotkey-mode-references", &RunConfigLoadInvalidHotkeyModeReferencesTest},
        {"config-load-relative-mode-dimensions", &RunConfigLoadRelativeModeDimensionsTest},
        {"config-load-expression-mode-dimensions", &RunConfigLoadExpressionModeDimensionsTest},
        {"config-load-legacy-version-upgrade", &RunConfigLoadLegacyVersionUpgradeTest},
        {"config-load-clamp-global-values", &RunConfigLoadClampGlobalValuesTest},
        {"config-load-mode-default-dimensions-restored", &RunConfigLoadModeDefaultDimensionsRestoredTest},
        {"config-load-mode-source-lists-loaded", &RunConfigLoadModeSourceListsLoadedTest},
        {"config-load-mode-percentage-dimensions-detected", &RunConfigLoadModePercentageDimensionsDetectedTest},
        {"config-load-mode-typed-sources-ignored", &RunConfigLoadModeTypedSourcesIgnoredTest},
        {"config-load-empty-main-hotkey-fallback", &RunConfigLoadEmptyMainHotkeyFallbackTest},
        {"config-load-missing-gui-hotkey-defaulted", &RunConfigLoadMissingGuiHotkeyDefaultedTest},
        {"config-load-empty-gui-hotkey-defaulted", &RunConfigLoadEmptyGuiHotkeyDefaultedTest},
        {"config-load-legacy-mirror-gamma-migrated", &RunConfigLoadLegacyMirrorGammaMigratedTest},
        {"config-load-mirror-capture-dimensions-clamped", &RunConfigLoadMirrorCaptureDimensionsClampedTest},
        {"config-load-eyezoom-clone-width-normalized", &RunConfigLoadEyeZoomCloneWidthNormalizedTest},
        {"config-load-eyezoom-overlay-width-defaulted", &RunConfigLoadEyeZoomOverlayWidthDefaultedTest},
        {"config-load-eyezoom-overlay-width-clamped", &RunConfigLoadEyeZoomOverlayWidthClampedTest},
        {"config-load-eyezoom-legacy-margins-migrated", &RunConfigLoadEyeZoomLegacyMarginsMigratedTest},
        {"config-load-eyezoom-legacy-custom-position-migrated", &RunConfigLoadEyeZoomLegacyCustomPositionMigratedTest},
        {"config-load-eyezoom-invalid-active-overlay-reset", &RunConfigLoadEyeZoomInvalidActiveOverlayResetTest},
        {"config-load-window-overlay-capture-method-migrated", &RunConfigLoadWindowOverlayCaptureMethodMigratedTest},
        {"config-load-key-rebind-unicode-string-parsed", &RunConfigLoadKeyRebindUnicodeStringParsedTest},
        {"config-load-key-rebind-escaped-unicode-string-parsed", &RunConfigLoadKeyRebindEscapedUnicodeStringParsedTest},
        {"config-load-key-rebind-hex-unicode-string-parsed", &RunConfigLoadKeyRebindHexUnicodeStringParsedTest},
        {"config-load-key-rebind-invalid-unicode-defaulted", &RunConfigLoadKeyRebindInvalidUnicodeDefaultedTest},
        {"config-load-fullscreen-stretch-repaired", &RunConfigLoadFullscreenStretchRepairedTest},
        {"config-load-preemptive-sync-existing-mode", &RunConfigLoadPreemptiveSyncExistingModeTest},
        {"config-load-thin-min-width-enforced", &RunConfigLoadThinMinWidthEnforcedTest},
        {"config-load-browser-overlay-defaults", &RunConfigLoadBrowserOverlayDefaultsTest},
        {"config-load-window-overlay-defaults", &RunConfigLoadWindowOverlayDefaultsTest},
        {"config-load-image-defaults", &RunConfigLoadImageDefaultsTest},
        {"config-load-hotkey-default-flags", &RunConfigLoadHotkeyDefaultFlagsTest},
        {"config-load-sensitivity-hotkey-default-flags", &RunConfigLoadSensitivityHotkeyDefaultFlagsTest},
        {"config-load-image-color-key-default-sensitivity", &RunConfigLoadImageColorKeyDefaultSensitivityTest},
        {"config-load-window-overlay-color-key-default-sensitivity", &RunConfigLoadWindowOverlayColorKeyDefaultSensitivityTest},
        {"config-load-browser-overlay-color-key-default-sensitivity", &RunConfigLoadBrowserOverlayColorKeyDefaultSensitivityTest},
        {"mode-mirror-render-screen-anchors", &RunModeMirrorRenderScreenAnchorsTest},
        {"mode-mirror-render-viewport-anchors", &RunModeMirrorRenderViewportAnchorsTest},
        {"mode-mirror-render-screen-anchor-size-matrix", &RunModeMirrorRenderScreenAnchorSizeMatrixTest},
        {"mode-mirror-render-viewport-anchor-size-matrix", &RunModeMirrorRenderViewportAnchorSizeMatrixTest},
        {"mode-mirror-group-render", &RunModeMirrorGroupRenderTest},
        {"mode-window-overlay-render", &RunModeWindowOverlayRenderTest},
        {"mode-browser-overlay-render", &RunModeBrowserOverlayRenderTest},
        {"config-error-gui", &RunConfigErrorGuiTest},
        {"settings-gui-basic", &RunSettingsGuiBasicTest},
        {"settings-gui-advanced", &RunSettingsGuiAdvancedTest},
        {"settings-tab-general-default", &RunSettingsTabGeneralDefaultTest},
        {"settings-tab-other-default", &RunSettingsTabOtherDefaultTest},
        {"settings-tab-supporters-default", &RunSettingsTabSupportersDefaultTest},
        {"settings-tab-modes-default", &RunSettingsTabModesDefaultTest},
        {"settings-tab-mirrors-default", &RunSettingsTabMirrorsDefaultTest},
        {"settings-tab-images-default", &RunSettingsTabImagesDefaultTest},
        {"settings-tab-window-overlays-default", &RunSettingsTabWindowOverlaysDefaultTest},
        {"settings-tab-browser-overlays-default", &RunSettingsTabBrowserOverlaysDefaultTest},
        {"settings-tab-hotkeys-default", &RunSettingsTabHotkeysDefaultTest},
        {"settings-tab-inputs-mouse-default", &RunSettingsTabInputsMouseDefaultTest},
        {"settings-tab-inputs-keyboard-default", &RunSettingsTabInputsKeyboardDefaultTest},
        {"settings-tab-settings-default", &RunSettingsTabSettingsDefaultTest},
        {"settings-tab-appearance-default", &RunSettingsTabAppearanceDefaultTest},
        {"settings-tab-misc-default", &RunSettingsTabMiscDefaultTest},
        {"settings-tab-general-populated", &RunSettingsTabGeneralPopulatedTest},
        {"settings-tab-other-populated", &RunSettingsTabOtherPopulatedTest},
        {"settings-tab-modes-populated", &RunSettingsTabModesPopulatedTest},
        {"settings-tab-mirrors-populated", &RunSettingsTabMirrorsPopulatedTest},
        {"settings-tab-images-populated", &RunSettingsTabImagesPopulatedTest},
        {"settings-tab-window-overlays-populated", &RunSettingsTabWindowOverlaysPopulatedTest},
        {"settings-tab-browser-overlays-populated", &RunSettingsTabBrowserOverlaysPopulatedTest},
        {"settings-tab-hotkeys-populated", &RunSettingsTabHotkeysPopulatedTest},
        {"settings-tab-inputs-mouse-populated", &RunSettingsTabInputsMousePopulatedTest},
        {"settings-tab-inputs-keyboard-populated", &RunSettingsTabInputsKeyboardPopulatedTest},
        {"settings-tab-settings-populated", &RunSettingsTabSettingsPopulatedTest},
        {"settings-tab-appearance-populated", &RunSettingsTabAppearancePopulatedTest},
        {"settings-tab-misc-populated", &RunSettingsTabMiscPopulatedTest},
        {"settings-tab-supporters-populated", &RunSettingsTabSupportersPopulatedTest},
    };

    return testCases;
}

const TestCaseDefinition* FindTestCaseDefinition(std::string_view testCaseName) {
    for (const TestCaseDefinition& testCase : GetTestCaseDefinitions()) {
        if (testCase.name == testCaseName) {
            return &testCase;
        }
    }

    return nullptr;
}

void RunTestCaseByName(std::string_view testCaseName, TestRunMode runMode = TestRunMode::Automated);
void RunAllTestCases();

void PrintTestCaseList(std::ostream& stream) {
    stream << "Available test cases:" << std::endl;
    for (const TestCaseDefinition& testCase : GetTestCaseDefinitions()) {
        stream << "  " << testCase.name << std::endl;
    }
}

int FindDefaultVisualTestCaseIndex() {
    const auto& testCases = GetTestCaseDefinitions();
    for (size_t i = 0; i < testCases.size(); ++i) {
        if (std::string_view(testCases[i].name) == kDefaultVisualTestCase) {
            return static_cast<int>(i);
        }
    }

    return 0;
}

enum class LauncherAction {
    None,
    RunSelectedAutomated,
    RunSelectedVisual,
    RunAllAutomated,
    Exit,
};

struct LauncherState {
    int selectedTestCaseIndex = FindDefaultVisualTestCaseIndex();
    std::string lastStatus = "Choose a test case and a run mode.";
};

LauncherAction RenderLauncherFrame(LauncherState& launcherState) {
    const auto& testCases = GetTestCaseDefinitions();
    LauncherAction action = LauncherAction::None;

    ImGui::SetNextWindowPos(ImVec2(40.0f, 40.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(620.0f, 420.0f), ImGuiCond_Always);

    if (ImGui::Begin("Toolscreen GUI Integration Test Runner", nullptr,
                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextWrapped("Select a test case, then choose whether to run it interactively or as an automated pass/fail check.");
        ImGui::Spacing();

        if (ImGui::BeginListBox("##gui-test-cases", ImVec2(-1.0f, 180.0f))) {
            for (int i = 0; i < static_cast<int>(testCases.size()); ++i) {
                const bool isSelected = launcherState.selectedTestCaseIndex == i;
                if (ImGui::Selectable(testCases[i].name, isSelected)) {
                    launcherState.selectedTestCaseIndex = i;
                }
                if (isSelected) {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndListBox();
        }

        ImGui::Spacing();
        ImGui::Text("Selected: %s", testCases[launcherState.selectedTestCaseIndex].name);

        if (ImGui::Button("Run Selected Visual", ImVec2(190.0f, 0.0f))) {
            action = LauncherAction::RunSelectedVisual;
        }
        ImGui::SameLine();
        if (ImGui::Button("Run Selected Automated", ImVec2(190.0f, 0.0f))) {
            action = LauncherAction::RunSelectedAutomated;
        }
        ImGui::SameLine();
        if (ImGui::Button("Run All Automated", ImVec2(190.0f, 0.0f))) {
            action = LauncherAction::RunAllAutomated;
        }

        ImGui::Spacing();
        if (ImGui::Button("Close Launcher", ImVec2(190.0f, 0.0f))) {
            action = LauncherAction::Exit;
        }

        ImGui::Separator();
        ImGui::TextWrapped("%s", launcherState.lastStatus.c_str());
    }
    ImGui::End();

    return action;
}

std::string ExecuteLauncherAction(DummyWindow& launcherWindow, const LauncherAction action, const LauncherState& launcherState) {
    const auto& testCases = GetTestCaseDefinitions();
    const char* selectedTestCaseName = testCases[launcherState.selectedTestCaseIndex].name;

    auto runSingle = [&](const TestRunMode runMode) {
        const bool hideLauncher = runMode == TestRunMode::Visual;
        if (hideLauncher) {
            launcherWindow.Show(false);
        }

        HandleImGuiContextReset();

        try {
            RunTestCaseByName(selectedTestCaseName, runMode);
        } catch (...) {
            g_minecraftHwnd.store(launcherWindow.hwnd(), std::memory_order_release);
            launcherWindow.SetTitle("Toolscreen GUI Integration Tests - Launcher");
            if (hideLauncher) {
                launcherWindow.Show(true);
            }
            throw;
        }

        g_minecraftHwnd.store(launcherWindow.hwnd(), std::memory_order_release);
        launcherWindow.SetTitle("Toolscreen GUI Integration Tests - Launcher");
        if (hideLauncher) {
            launcherWindow.Show(true);
        }

        return std::string("PASS ") + selectedTestCaseName + (runMode == TestRunMode::Visual ? " [visual]" : " [automated]");
    };

    switch (action) {
        case LauncherAction::RunSelectedAutomated:
            return runSingle(TestRunMode::Automated);
        case LauncherAction::RunSelectedVisual:
            return runSingle(TestRunMode::Visual);
        case LauncherAction::RunAllAutomated:
            HandleImGuiContextReset();
            RunAllTestCases();
            g_minecraftHwnd.store(launcherWindow.hwnd(), std::memory_order_release);
            launcherWindow.SetTitle("Toolscreen GUI Integration Tests - Launcher");
            return "PASS all automated test cases";
        case LauncherAction::Exit:
            return "Launcher closed.";
        case LauncherAction::None:
            break;
    }

    return launcherState.lastStatus;
}

void RunLauncherGui() {
    DummyWindow launcherWindow(kWindowWidth, kWindowHeight, true);
    launcherWindow.SetTitle("Toolscreen GUI Integration Tests - Launcher");

    LauncherState launcherState;
    while (launcherWindow.PumpMessages()) {
        if (!launcherWindow.BeginFrame()) {
            break;
        }

        const LauncherAction action = RenderLauncherFrame(launcherState);
        launcherWindow.EndFrame();

        if (action == LauncherAction::Exit) {
            break;
        }

        if (action != LauncherAction::None) {
            try {
                launcherState.lastStatus = ExecuteLauncherAction(launcherWindow, action, launcherState);
            } catch (const std::exception& ex) {
                launcherState.lastStatus = std::string("FAIL: ") + ex.what();
            }
        }

        Sleep(16);
    }
}

void PrintUsage(std::ostream& stream) {
    stream << "Usage:" << std::endl;
    stream << "  toolscreen_gui_integration_tests" << std::endl;
    stream << "  toolscreen_gui_integration_tests --run-all" << std::endl;
    stream << "  toolscreen_gui_integration_tests <test-case>" << std::endl;
    stream << "  toolscreen_gui_integration_tests --visual [<test-case>]" << std::endl;
    stream << "  toolscreen_gui_integration_tests --list" << std::endl;
    stream << "  toolscreen_gui_integration_tests --help" << std::endl;
    stream << std::endl;
    stream << "No arguments opens a launcher GUI where you can choose which test mode to run." << std::endl;
    stream << "Use --run-all for pure CLI pass/fail execution of every test case." << std::endl;
    stream << "Visual mode keeps the dummy Win32/WGL window open so the GUI can be inspected interactively." << std::endl;
    stream << "If no visual test case is provided, it defaults to " << kDefaultVisualTestCase << "." << std::endl;
    stream << std::endl;
    PrintTestCaseList(stream);
}

struct CommandLineOptions {
    bool openLauncher = false;
    bool showUsage = false;
    bool listOnly = false;
    bool runAll = false;
    TestRunMode runMode = TestRunMode::Automated;
    std::string testCaseName;
};

CommandLineOptions ParseCommandLine(int argc, char** argv) {
    if (argc == 1) {
        CommandLineOptions options;
        options.openLauncher = true;
        return options;
    }

    if (argc > 3) {
        throw std::runtime_error("Expected at most two arguments.");
    }

    const std::string firstArg = argv[1];
    if (firstArg == "--help" || firstArg == "-h") {
        if (argc != 2) {
            throw std::runtime_error("--help does not accept additional arguments.");
        }

        CommandLineOptions options;
        options.showUsage = true;
        return options;
    }

    if (firstArg == "--list") {
        if (argc != 2) {
            throw std::runtime_error("--list does not accept additional arguments.");
        }

        CommandLineOptions options;
        options.listOnly = true;
        return options;
    }

    if (firstArg == "--run-all") {
        if (argc != 2) {
            throw std::runtime_error("--run-all does not accept additional arguments.");
        }

        CommandLineOptions options;
        options.runAll = true;
        return options;
    }

    if (firstArg == "--visual") {
        CommandLineOptions options;
        options.runMode = TestRunMode::Visual;
        options.testCaseName = argc == 3 ? argv[2] : kDefaultVisualTestCase;
        if (options.testCaseName == "all") {
            throw std::runtime_error("Visual mode requires a single test case.");
        }

        return options;
    }

    if (argc != 2) {
        throw std::runtime_error("Unexpected extra arguments.");
    }

    if (firstArg == "all") {
        CommandLineOptions options;
        options.runAll = true;
        return options;
    }

    CommandLineOptions options;
    options.testCaseName = firstArg;
    return options;
}

void RunTestCaseByName(std::string_view testCaseName, TestRunMode runMode) {
    const TestCaseDefinition* testCase = FindTestCaseDefinition(testCaseName);
    if (testCase == nullptr) {
        throw std::runtime_error("Unknown test case: " + std::string(testCaseName));
    }

    std::cout << "RUN " << testCase->name;
    if (runMode == TestRunMode::Visual) {
        std::cout << " [visual]";
    }
    std::cout << std::endl;

    testCase->run(runMode);
    std::cout << "PASS " << testCase->name;
    if (runMode == TestRunMode::Visual) {
        std::cout << " [visual]";
    }
    std::cout << std::endl;
}

void RunAllTestCases() {
    for (const TestCaseDefinition& testCase : GetTestCaseDefinitions()) {
        RunTestCaseByName(testCase.name);
    }
}

bool ShouldPauseForTransientConsole() {
    DWORD consoleProcessIds[2]{};
    return GetConsoleProcessList(consoleProcessIds, static_cast<DWORD>(std::size(consoleProcessIds))) == 1;
}

void PauseForTransientConsole() {
    if (!ShouldPauseForTransientConsole()) {
        return;
    }

    std::cerr << "Press Enter to close..." << std::flush;
    std::string ignored;
    std::getline(std::cin, ignored);
}

} // namespace

int main(int argc, char** argv) {
    try {
        EnsureProcessDpiAwareness();
        const CommandLineOptions options = ParseCommandLine(argc, argv);

        if (options.openLauncher) {
            RunLauncherGui();
            return 0;
        }

        if (options.showUsage) {
            PrintUsage(std::cout);
            return 0;
        }

        if (options.listOnly) {
            PrintTestCaseList(std::cout);
            return 0;
        }

        if (options.runAll) {
            std::cout << "Running all GUI integration tests." << std::endl;
            PrintTestCaseList(std::cout);
            RunAllTestCases();
            return 0;
        }

        RunTestCaseByName(options.testCaseName, options.runMode);
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "FAIL: " << ex.what() << std::endl;
        PrintUsage(std::cerr);
        PauseForTransientConsole();
        return 1;
    }
}
