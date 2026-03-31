struct CapturedWindowMessage {
    UINT message = 0;
    WPARAM wParam = 0;
    LPARAM lParam = 0;
};

constexpr wchar_t kRebindCapturePropName[] = L"ToolscreenRebindCapture";
constexpr LRESULT kCapturedWndProcResult = 0x5A17;

class ScopedKeyboardStateOverride {
  public:
    ScopedKeyboardStateOverride() {
        m_valid = (GetKeyboardState(m_originalState) == TRUE);
        if (!m_valid) {
            memset(m_originalState, 0, sizeof(m_originalState));
        }
        memcpy(m_state, m_originalState, sizeof(m_state));
    }

    ~ScopedKeyboardStateOverride() {
        if (m_valid) {
            (void)SetKeyboardState(m_originalState);
        }
    }

    void SetKeyDown(int vk, bool down) {
        SetHighBit(vk, down);
        switch (vk) {
        case VK_SHIFT:
            SetHighBit(VK_LSHIFT, down);
            SetHighBit(VK_RSHIFT, down);
            break;
        case VK_CONTROL:
            SetHighBit(VK_LCONTROL, down);
            SetHighBit(VK_RCONTROL, down);
            break;
        case VK_MENU:
            SetHighBit(VK_LMENU, down);
            SetHighBit(VK_RMENU, down);
            break;
        default:
            break;
        }
    }

    void SetToggle(int vk, bool enabled) {
        if (vk < 0 || vk >= 256) return;
        if (enabled) {
            m_state[vk] |= 0x01;
        } else {
            m_state[vk] &= static_cast<BYTE>(~0x01);
        }
    }

    void Apply() {
        Expect(SetKeyboardState(m_state) == TRUE, "Failed to apply keyboard state override for rebind integration test.");
    }

  private:
    void SetHighBit(int vk, bool down) {
        if (vk < 0 || vk >= 256) return;
        if (down) {
            m_state[vk] |= 0x80;
        } else {
            m_state[vk] &= static_cast<BYTE>(~0x80);
        }
    }

    BYTE m_originalState[256] = {};
    BYTE m_state[256] = {};
    bool m_valid = false;
};

class ScopedRebindMessageCapture {
  public:
    explicit ScopedRebindMessageCapture(HWND hwnd) : m_hwnd(hwnd) {
        Expect(m_hwnd != nullptr, "ScopedRebindMessageCapture requires a valid HWND.");
        m_previousWindowProc = reinterpret_cast<WNDPROC>(GetWindowLongPtrW(m_hwnd, GWLP_WNDPROC));
        Expect(m_previousWindowProc != nullptr, "Failed to read the previous WNDPROC for rebind integration test capture.");
        const BOOL setPropOk = SetPropW(m_hwnd, kRebindCapturePropName, this);
        Expect(setPropOk == TRUE, "Failed to associate rebind message capture with the integration-test window.");
        const LONG_PTR setProcResult = SetWindowLongPtrW(m_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&CaptureRouterWndProc));
        Expect(setProcResult != 0, "Failed to install the rebind message capture window procedure.");
        m_previousOriginalWndProc = g_originalWndProc;
        g_originalWndProc = &CaptureOriginalWndProc;
    }

    ~ScopedRebindMessageCapture() {
        g_originalWndProc = m_previousOriginalWndProc;
        if (m_hwnd != nullptr && m_previousWindowProc != nullptr) {
            (void)SetWindowLongPtrW(m_hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(m_previousWindowProc));
            (void)RemovePropW(m_hwnd, kRebindCapturePropName);
        }
    }

    void Clear() { messages.clear(); }

    std::vector<CapturedWindowMessage> messages;

  private:
    static ScopedRebindMessageCapture* FromWindow(HWND hwnd) {
        return reinterpret_cast<ScopedRebindMessageCapture*>(GetPropW(hwnd, kRebindCapturePropName));
    }

    static LRESULT CALLBACK CaptureOriginalWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        if (auto* capture = FromWindow(hwnd)) {
            capture->messages.push_back({ message, wParam, lParam });
        }
        return kCapturedWndProcResult;
    }

    static LRESULT CALLBACK CaptureRouterWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
        if (message == WM_TOOLSCREEN_KEYDOWN_NO_REBIND || message == WM_TOOLSCREEN_KEYUP_NO_REBIND) {
            InputHandlerResult result = HandleCustomKeyNoRebind(hwnd, message, wParam, lParam);
            if (result.consumed) { return result.result; }
        }

        if (message == WM_TOOLSCREEN_CHAR_NO_REBIND) {
            InputHandlerResult result = HandleCustomCharNoRebind(hwnd, message, wParam, lParam);
            if (result.consumed) { return result.result; }
        }

        if (auto* capture = FromWindow(hwnd)) {
            return CallWindowProc(capture->m_previousWindowProc, hwnd, message, wParam, lParam);
        }
        return DefWindowProcW(hwnd, message, wParam, lParam);
    }

    HWND m_hwnd = nullptr;
    WNDPROC m_previousWindowProc = nullptr;
    WNDPROC m_previousOriginalWndProc = nullptr;
};

static LPARAM BuildTestKeyboardMessageLParam(DWORD vk, bool isKeyDown, bool isSystemKey = false, UINT repeatCount = 1) {
    const UINT scanCodeWithFlags = static_cast<UINT>(MapVirtualKeyW(vk, MAPVK_VK_TO_VSC_EX));
    LPARAM out = static_cast<LPARAM>(repeatCount & 0xFFFFu);
    out |= static_cast<LPARAM>(scanCodeWithFlags & 0xFFu) << 16;
    if ((scanCodeWithFlags & 0xFF00u) != 0) {
        out |= static_cast<LPARAM>(1) << 24;
    }
    if (isSystemKey) {
        out |= static_cast<LPARAM>(1) << 29;
    }
    if (!isKeyDown) {
        out |= (static_cast<LPARAM>(1) << 30) | (static_cast<LPARAM>(1) << 31);
    }
    return out;
}

static KeyRebind MakeEnabledRebind(DWORD fromKey, DWORD toKey) {
    KeyRebind rebind;
    rebind.fromKey = fromKey;
    rebind.toKey = toKey;
    rebind.enabled = true;
    return rebind;
}

static void PrepareRebindRuntimeCase(std::string_view caseName, const std::vector<KeyRebind>& rebinds) {
    const std::filesystem::path root = PrepareCaseDirectory(caseName);
    ResetGlobalTestState(root);
    g_showGui.store(false, std::memory_order_release);
    g_config.keyRebinds = {};
    g_config.keyRebinds.enabled = true;
    g_config.keyRebinds.resolveRebindTargetsForHotkeys = false;
    g_config.keyRebinds.toggleHotkey.clear();
    g_config.keyRebinds.rebinds = rebinds;
    g_config.hotkeys.clear();
    g_config.sensitivityHotkeys.clear();
    PublishConfigSnapshot();
}

static void ExpectCapturedMessage(const ScopedRebindMessageCapture& capture, size_t index, UINT expectedMessage, WPARAM expectedWParam,
                                  const std::string& label) {
    Expect(index < capture.messages.size(), label + " missing captured message at index " + std::to_string(index) + ".");
    const CapturedWindowMessage& message = capture.messages[index];
    Expect(message.message == expectedMessage,
           label + " expected message " + std::to_string(expectedMessage) + ", got " + std::to_string(message.message) + ".");
    Expect(message.wParam == expectedWParam,
           label + " expected wParam " + std::to_string(static_cast<unsigned long long>(expectedWParam)) + ", got " +
               std::to_string(static_cast<unsigned long long>(message.wParam)) + ".");
}

static UINT ExtractScanCodeWithFlagsFromLParam(LPARAM lParam) {
    const auto raw = static_cast<unsigned long long>(lParam);
    UINT scanCode = static_cast<UINT>((raw >> 16) & 0xFFu);
    if ((raw & (1ull << 24)) != 0) {
        scanCode |= 0xE000u;
    }
    return scanCode;
}

static void ExpectCapturedScanCode(const ScopedRebindMessageCapture& capture, size_t index, UINT expectedScanCode, const std::string& label) {
    Expect(index < capture.messages.size(), label + " missing captured message at index " + std::to_string(index) + ".");
    const UINT actualScanCode = ExtractScanCodeWithFlagsFromLParam(capture.messages[index].lParam);
    Expect(actualScanCode == expectedScanCode,
           label + " expected scan code " + std::to_string(expectedScanCode) + ", got " + std::to_string(actualScanCode) + ".");
}

static void PrepareRebindGuiCase(std::string_view caseName, const std::vector<KeyRebind>& rebinds = {}) {
    const std::filesystem::path root = PrepareCaseDirectory(caseName);
    ResetGlobalTestState(root);
    g_config.basicModeEnabled = false;
    g_config.keyRebinds = {};
    g_config.keyRebinds.enabled = true;
    g_config.keyRebinds.resolveRebindTargetsForHotkeys = false;
    g_config.keyRebinds.toggleHotkey.clear();
    g_config.keyRebinds.rebinds = rebinds;
    g_config.hotkeys.clear();
    g_config.sensitivityHotkeys.clear();
    PublishConfigSnapshot();
}

static void RenderKeyboardInputsFrame(DummyWindow& window) {
    RenderSettingsFrame(window, trc("tabs.inputs"), trc("inputs.keyboard"));
}

static bool SkipIfNoModernGuiTestGL(const DummyWindow& window) {
    if (window.hasModernGL()) {
        return false;
    }

    std::cout << "SKIP (no GL 3.3+)" << std::endl;
    return true;
}

static GuiTestInteractionRect ExpectGuiInteractionRect(const char* id, const std::string& label) {
    GuiTestInteractionRect rect;
    Expect(GetGuiTestInteractionRect(id, rect), label + " missing GUI interaction rect '" + id + "'.");
    Expect(std::isfinite(rect.minX) && std::isfinite(rect.minY) && std::isfinite(rect.maxX) && std::isfinite(rect.maxY),
           label + " produced a non-finite GUI interaction rect.");
    if (rect.maxX <= rect.minX) {
        rect.maxX = rect.minX + 8.0f;
    }
    if (rect.maxY <= rect.minY) {
        rect.maxY = rect.minY + 8.0f;
    }
    return rect;
}

static POINT GetClientCenterPoint(HWND hwnd, const GuiTestInteractionRect& rect, const std::string& label) {
    POINT point{};
    point.x = static_cast<LONG>(std::lround((rect.minX + rect.maxX) * 0.5f));
    point.y = static_cast<LONG>(std::lround((rect.minY + rect.maxY) * 0.5f));
    Expect(ScreenToClient(hwnd, &point) == TRUE, label + " failed to convert a screen-space GUI rect to client coordinates.");
    return point;
}

static LPARAM MakeMouseClientLParam(const POINT& point) {
    return MAKELPARAM(static_cast<WORD>(point.x & 0xFFFF), static_cast<WORD>(point.y & 0xFFFF));
}

static void SendGuiMouseMessage(HWND hwnd, UINT message, WPARAM wParam, const POINT& point) {
    (void)SendMessageW(hwnd, message, wParam, MakeMouseClientLParam(point));
}

static void ClickGuiInteractionRect(DummyWindow& window, const char* id, bool rightButton, const std::string& label) {
    const GuiTestInteractionRect rect = ExpectGuiInteractionRect(id, label);
    const POINT point = GetClientCenterPoint(window.hwnd(), rect, label);
    const UINT downMessage = rightButton ? WM_RBUTTONDOWN : WM_LBUTTONDOWN;
    const UINT upMessage = rightButton ? WM_RBUTTONUP : WM_LBUTTONUP;
    const WPARAM downWParam = rightButton ? MK_RBUTTON : MK_LBUTTON;

    SendGuiMouseMessage(window.hwnd(), WM_MOUSEMOVE, 0, point);
    RenderKeyboardInputsFrame(window);

    SendGuiMouseMessage(window.hwnd(), downMessage, downWParam, point);
    RenderKeyboardInputsFrame(window);

    SendGuiMouseMessage(window.hwnd(), upMessage, 0, point);
    RenderKeyboardInputsFrame(window);
}

static void SubmitKeyboardBindingEvent(DWORD vk) {
    RegisterBindingInputEvent(WM_KEYDOWN, static_cast<WPARAM>(vk), BuildTestKeyboardMessageLParam(vk, true));
}

static void OpenKeyboardLayoutContext(DummyWindow& window, DWORD sourceVk) {
    RenderKeyboardInputsFrame(window);
    RenderKeyboardInputsFrame(window);
    RequestGuiTestOpenKeyboardLayout();
    RenderKeyboardInputsFrame(window);
    RequestGuiTestOpenKeyboardLayoutContext(sourceVk);
    RenderKeyboardInputsFrame(window);
}

static void BindKeyboardLayoutTarget(DummyWindow& window, bool splitMode, DWORD targetVk) {
    if (splitMode) {
        RequestGuiTestKeyboardLayoutSetSplitMode(true);
        RenderKeyboardInputsFrame(window);
        RequestGuiTestKeyboardLayoutBeginBind(GuiTestKeyboardLayoutBindTarget::TriggersVk);
    } else {
        RequestGuiTestKeyboardLayoutBeginBind(GuiTestKeyboardLayoutBindTarget::FullOutputVk);
    }
    RenderKeyboardInputsFrame(window);
    SubmitKeyboardBindingEvent(targetVk);
    RenderKeyboardInputsFrame(window);
}

static void OpenKeyboardLayoutScanPicker(DummyWindow& window) {
    RequestGuiTestKeyboardLayoutOpenScanPicker();
    RenderKeyboardInputsFrame(window);
}

static void SetKeyboardLayoutScanFilter(DummyWindow& window, GuiTestKeyboardLayoutScanFilterGroup group) {
    RequestGuiTestKeyboardLayoutSetScanFilter(group);
    RenderKeyboardInputsFrame(window);
}

static void SelectKeyboardLayoutScan(DummyWindow& window, DWORD scan) {
    RequestGuiTestKeyboardLayoutSelectScan(scan);
    RenderKeyboardInputsFrame(window);
}

static void ResetKeyboardLayoutScanToDefault(DummyWindow& window) {
    RequestGuiTestKeyboardLayoutResetScanToDefault();
    RenderKeyboardInputsFrame(window);
}

void RunConfigLoadKeyRebindShiftLayerCapsLockParsedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_key_rebind_shift_layer_caps_lock_parsed",
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
shiftLayerEnabled = true
shiftLayerUsesCapsLock = true
shiftLayerOutputVK = 80
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-key-rebind-shift-layer-caps-lock-parsed");
                          Expect(g_config.keyRebinds.rebinds.size() == 1, "Expected exactly one key rebind fixture to load.");
                          const KeyRebind& rebind = g_config.keyRebinds.rebinds.front();
                          Expect(rebind.shiftLayerEnabled, "Expected shiftLayerEnabled fixture state to load.");
                          Expect(rebind.shiftLayerUsesCapsLock,
                                 "Expected shiftLayerUsesCapsLock to parse from the key rebind config fixture.");
                          Expect(rebind.shiftLayerOutputVK == 80, "Expected shiftLayerOutputVK fixture state to load.");
                      },
                      runMode);
}

void RunConfigLoadKeyRebindShiftLayerCapsLockDefaultedTest(TestRunMode runMode = TestRunMode::Automated) {
    RunConfigLoadCase("config_load_key_rebind_shift_layer_caps_lock_defaulted",
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
shiftLayerEnabled = true
shiftLayerOutputVK = 80
)");
                      },
                      []() {
                          ExpectConfigLoadSucceeded("config-load-key-rebind-shift-layer-caps-lock-defaulted");
                          Expect(g_config.keyRebinds.rebinds.size() == 1, "Expected exactly one key rebind fixture to load.");
                          const KeyRebind& rebind = g_config.keyRebinds.rebinds.front();
                          Expect(!rebind.shiftLayerUsesCapsLock,
                                 "Expected shiftLayerUsesCapsLock to default to false when omitted from config.");
                      },
                      runMode);
}

void RunKeyRebindRuntimeFullForwardingTest(TestRunMode runMode = TestRunMode::Automated) {
    DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);
    KeyRebind rebind = MakeEnabledRebind('A', 'B');
    rebind.useCustomOutput = true;
    rebind.customOutputVK = 'B';
    PrepareRebindRuntimeCase("key_rebind_runtime_full_forwarding", { rebind });
    ScopedRebindMessageCapture capture(window.hwnd());

    ScopedKeyboardStateOverride keyboardState;
    keyboardState.SetKeyDown(VK_SHIFT, false);
    keyboardState.SetToggle(VK_CAPITAL, false);
    keyboardState.Apply();

    const LPARAM keyDownLParam = BuildTestKeyboardMessageLParam('A', true);
    const InputHandlerResult keyDownResult = HandleKeyRebinding(window.hwnd(), WM_KEYDOWN, 'A', keyDownLParam);
    Expect(keyDownResult.consumed, "Expected full rebind WM_KEYDOWN to be consumed.");
    Expect(keyDownResult.result == kCapturedWndProcResult, "Expected full rebind WM_KEYDOWN to forward through the original WNDPROC.");
    Expect(capture.messages.size() == 1, "Expected full rebind WM_KEYDOWN to forward exactly one key message.");
    ExpectCapturedMessage(capture, 0, WM_KEYDOWN, 'B', "Full rebind WM_KEYDOWN");

    capture.Clear();
    const InputHandlerResult charResult = HandleCharRebinding(window.hwnd(), WM_CHAR, static_cast<WPARAM>('a'), keyDownLParam);
    Expect(charResult.consumed, "Expected full rebind WM_CHAR to be consumed.");
    Expect(charResult.result == kCapturedWndProcResult, "Expected full rebind WM_CHAR to forward through the original WNDPROC.");
    Expect(capture.messages.size() == 1, "Expected full rebind WM_CHAR to forward exactly one character message.");
    ExpectCapturedMessage(capture, 0, WM_CHAR, 'b', "Full rebind WM_CHAR");

    capture.Clear();
    const InputHandlerResult keyUpResult = HandleKeyRebinding(window.hwnd(), WM_KEYUP, 'A', BuildTestKeyboardMessageLParam('A', false));
    Expect(keyUpResult.consumed, "Expected full rebind WM_KEYUP to be consumed.");
    Expect(keyUpResult.result == kCapturedWndProcResult, "Expected full rebind WM_KEYUP to forward through the original WNDPROC.");
    Expect(capture.messages.size() == 1, "Expected full rebind WM_KEYUP to forward exactly one key message.");
    ExpectCapturedMessage(capture, 0, WM_KEYUP, 'B', "Full rebind WM_KEYUP");
}

void RunKeyRebindRuntimeSplitVkOutputTest(TestRunMode runMode = TestRunMode::Automated) {
    DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);
    KeyRebind rebind = MakeEnabledRebind('A', 'B');
    rebind.useCustomOutput = true;
    rebind.customOutputVK = 'C';
    PrepareRebindRuntimeCase("key_rebind_runtime_split_vk_output", { rebind });
    ScopedRebindMessageCapture capture(window.hwnd());

    ScopedKeyboardStateOverride keyboardState;
    keyboardState.SetKeyDown(VK_SHIFT, false);
    keyboardState.SetToggle(VK_CAPITAL, false);
    keyboardState.Apply();

    const LPARAM sourceLParam = BuildTestKeyboardMessageLParam('A', true);
    const InputHandlerResult keyDownResult = HandleKeyRebinding(window.hwnd(), WM_KEYDOWN, 'A', sourceLParam);
    Expect(keyDownResult.consumed, "Expected split rebind WM_KEYDOWN to be consumed.");
    Expect(capture.messages.size() == 1, "Expected split rebind WM_KEYDOWN to forward exactly one key message.");
    ExpectCapturedMessage(capture, 0, WM_KEYDOWN, 'B', "Split rebind trigger WM_KEYDOWN");

    capture.Clear();
    const InputHandlerResult charResult = HandleCharRebinding(window.hwnd(), WM_CHAR, static_cast<WPARAM>('a'), sourceLParam);
    Expect(charResult.consumed, "Expected split rebind WM_CHAR to be consumed.");
    Expect(capture.messages.size() == 1, "Expected split rebind WM_CHAR to forward exactly one character message.");
    ExpectCapturedMessage(capture, 0, WM_CHAR, 'c', "Split rebind typed WM_CHAR");
}

void RunKeyRebindRuntimeSplitUnicodeOutputTest(TestRunMode runMode = TestRunMode::Automated) {
    DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);
    KeyRebind rebind = MakeEnabledRebind('A', 'B');
    rebind.useCustomOutput = true;
    rebind.customOutputUnicode = 0x00F8;
    PrepareRebindRuntimeCase("key_rebind_runtime_split_unicode_output", { rebind });
    ScopedRebindMessageCapture capture(window.hwnd());

    ScopedKeyboardStateOverride keyboardState;
    keyboardState.SetKeyDown(VK_SHIFT, false);
    keyboardState.SetToggle(VK_CAPITAL, false);
    keyboardState.Apply();

    const InputHandlerResult charResult =
        HandleCharRebinding(window.hwnd(), WM_CHAR, static_cast<WPARAM>('a'), BuildTestKeyboardMessageLParam('A', true));
    Expect(charResult.consumed, "Expected Unicode split rebind WM_CHAR to be consumed.");
    Expect(capture.messages.size() == 1, "Expected Unicode split rebind to forward exactly one character message.");
    ExpectCapturedMessage(capture, 0, WM_CHAR, 0x00F8, "Split rebind Unicode WM_CHAR");
}

void RunKeyRebindRuntimeShiftLayerShiftActivatedTest(TestRunMode runMode = TestRunMode::Automated) {
    DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);
    KeyRebind rebind = MakeEnabledRebind('A', 'B');
    rebind.useCustomOutput = true;
    rebind.customOutputVK = 'C';
    rebind.shiftLayerEnabled = true;
    rebind.shiftLayerOutputVK = 'D';
    rebind.shiftLayerOutputShifted = true;
    PrepareRebindRuntimeCase("key_rebind_runtime_shift_layer_shift_activated", { rebind });
    ScopedRebindMessageCapture capture(window.hwnd());

    ScopedKeyboardStateOverride keyboardState;
    keyboardState.SetKeyDown(VK_SHIFT, true);
    keyboardState.SetToggle(VK_CAPITAL, false);
    keyboardState.Apply();

    const InputHandlerResult charResult =
        HandleCharRebinding(window.hwnd(), WM_CHAR, static_cast<WPARAM>('A'), BuildTestKeyboardMessageLParam('A', true));
    Expect(charResult.consumed, "Expected Shift-layer WM_CHAR to be consumed while Shift is down.");
    Expect(capture.messages.size() == 1, "Expected Shift-layer WM_CHAR to forward exactly one character message.");
    ExpectCapturedMessage(capture, 0, WM_CHAR, 'D', "Shift-layer WM_CHAR");
}

void RunKeyRebindRuntimeShiftLayerCapsLockActivatedTest(TestRunMode runMode = TestRunMode::Automated) {
    DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);
    KeyRebind rebind = MakeEnabledRebind('A', 'B');
    rebind.useCustomOutput = true;
    rebind.customOutputVK = 'C';
    rebind.shiftLayerEnabled = true;
    rebind.shiftLayerUsesCapsLock = true;
    rebind.shiftLayerOutputUnicode = 0x00D8;
    PrepareRebindRuntimeCase("key_rebind_runtime_shift_layer_caps_lock_activated", { rebind });
    ScopedRebindMessageCapture capture(window.hwnd());

    ScopedKeyboardStateOverride keyboardState;
    keyboardState.SetKeyDown(VK_SHIFT, false);
    keyboardState.SetToggle(VK_CAPITAL, true);
    keyboardState.Apply();

    const InputHandlerResult charResult =
        HandleCharRebinding(window.hwnd(), WM_CHAR, static_cast<WPARAM>('A'), BuildTestKeyboardMessageLParam('A', true));
    Expect(charResult.consumed, "Expected Caps-Lock-driven Shift-layer WM_CHAR to be consumed.");
    Expect(capture.messages.size() == 1,
           "Expected Caps-Lock-driven Shift-layer WM_CHAR to forward exactly one character message.");
    ExpectCapturedMessage(capture, 0, WM_CHAR, 0x00D8, "Caps-Lock Shift-layer WM_CHAR");
}

void RunKeyRebindRuntimeFullCapsLockHonoredTest(TestRunMode runMode = TestRunMode::Automated) {
    DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);
    KeyRebind rebind = MakeEnabledRebind('A', 'B');
    rebind.useCustomOutput = true;
    rebind.customOutputVK = 'B';
    PrepareRebindRuntimeCase("key_rebind_runtime_full_caps_lock_honored", { rebind });
    ScopedRebindMessageCapture capture(window.hwnd());

    ScopedKeyboardStateOverride keyboardState;
    keyboardState.SetKeyDown(VK_SHIFT, false);
    keyboardState.SetToggle(VK_CAPITAL, true);
    keyboardState.Apply();

    const InputHandlerResult charResult =
        HandleCharRebinding(window.hwnd(), WM_CHAR, static_cast<WPARAM>('A'), BuildTestKeyboardMessageLParam('A', true));
    Expect(charResult.consumed, "Expected full rebind WM_CHAR to be consumed while Caps Lock is on.");
    Expect(capture.messages.size() == 1, "Expected full rebind WM_CHAR to forward exactly one character message.");
    ExpectCapturedMessage(capture, 0, WM_CHAR, 'B', "Full rebind Caps Lock WM_CHAR");
}

void RunKeyRebindRuntimeSplitCapsLockIgnoredWithoutOptInTest(TestRunMode runMode = TestRunMode::Automated) {
    DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);
    KeyRebind rebind = MakeEnabledRebind('A', 'B');
    rebind.useCustomOutput = true;
    rebind.customOutputVK = 'C';
    PrepareRebindRuntimeCase("key_rebind_runtime_split_caps_lock_ignored", { rebind });
    ScopedRebindMessageCapture capture(window.hwnd());

    ScopedKeyboardStateOverride keyboardState;
    keyboardState.SetKeyDown(VK_SHIFT, false);
    keyboardState.SetToggle(VK_CAPITAL, true);
    keyboardState.Apply();

    const InputHandlerResult charResult =
        HandleCharRebinding(window.hwnd(), WM_CHAR, static_cast<WPARAM>('A'), BuildTestKeyboardMessageLParam('A', true));
    Expect(charResult.consumed, "Expected split rebind WM_CHAR to be consumed while Caps Lock is on.");
    Expect(capture.messages.size() == 1, "Expected split rebind WM_CHAR to forward exactly one character message.");
    ExpectCapturedMessage(capture, 0, WM_CHAR, 'c', "Split rebind Caps Lock WM_CHAR");
}

void RunKeyRebindRuntimeNonTypableTriggerConsumesCharTest(TestRunMode runMode = TestRunMode::Automated) {
    DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);
    PrepareRebindRuntimeCase("key_rebind_runtime_non_typable_trigger", { MakeEnabledRebind('A', VK_HOME) });
    ScopedRebindMessageCapture capture(window.hwnd());

    ScopedKeyboardStateOverride keyboardState;
    keyboardState.SetKeyDown(VK_SHIFT, false);
    keyboardState.SetToggle(VK_CAPITAL, false);
    keyboardState.Apply();

    const InputHandlerResult charResult =
        HandleCharRebinding(window.hwnd(), WM_CHAR, static_cast<WPARAM>('a'), BuildTestKeyboardMessageLParam('A', true));
    Expect(charResult.consumed, "Expected a rebind that cannot type to consume WM_CHAR.");
    Expect(charResult.result == 0, "Expected a rebind that cannot type to return result 0.");
    Expect(capture.messages.empty(), "Expected a rebind that cannot type to avoid forwarding WM_CHAR.");
}

void RunKeyRebindRuntimeMouseSourceEmitsKeyAndCharTest(TestRunMode runMode = TestRunMode::Automated) {
    DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);
    KeyRebind rebind = MakeEnabledRebind(VK_LBUTTON, 'B');
    rebind.useCustomOutput = true;
    rebind.customOutputVK = 'B';
    PrepareRebindRuntimeCase("key_rebind_runtime_mouse_source_emits_key_and_char", { rebind });
    ScopedRebindMessageCapture capture(window.hwnd());

    ScopedKeyboardStateOverride keyboardState;
    keyboardState.SetKeyDown(VK_SHIFT, false);
    keyboardState.SetToggle(VK_CAPITAL, false);
    keyboardState.Apply();

    const InputHandlerResult mouseDownResult = HandleKeyRebinding(window.hwnd(), WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(12, 34));
    Expect(mouseDownResult.consumed, "Expected a mouse-source rebind to consume WM_LBUTTONDOWN.");
    Expect(mouseDownResult.result == kCapturedWndProcResult,
           "Expected a mouse-source rebind to forward the rebinding result through the original WNDPROC.");
    Expect(capture.messages.size() == 2,
           "Expected a mouse-source rebind to forward both the target WM_KEYDOWN and generated WM_CHAR.");
    ExpectCapturedMessage(capture, 0, WM_KEYDOWN, 'B', "Mouse-source rebind WM_KEYDOWN");
    ExpectCapturedMessage(capture, 1, WM_CHAR, 'b', "Mouse-source rebind WM_CHAR");

    capture.Clear();
    const InputHandlerResult mouseUpResult = HandleKeyRebinding(window.hwnd(), WM_LBUTTONUP, 0, MAKELPARAM(12, 34));
    Expect(mouseUpResult.consumed, "Expected a mouse-source rebind to consume WM_LBUTTONUP.");
    Expect(capture.messages.size() == 1, "Expected a mouse-source rebind WM_LBUTTONUP to forward exactly one key-up message.");
    ExpectCapturedMessage(capture, 0, WM_KEYUP, 'B', "Mouse-source rebind WM_KEYUP");
}

void RunKeyRebindRuntimeDisabledRebindIgnoredTest(TestRunMode runMode = TestRunMode::Automated) {
    DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);
    KeyRebind rebind = MakeEnabledRebind('A', 'B');
    rebind.enabled = false;
    PrepareRebindRuntimeCase("key_rebind_runtime_disabled_rebind_ignored", { rebind });
    ScopedRebindMessageCapture capture(window.hwnd());

    const InputHandlerResult keyResult = HandleKeyRebinding(window.hwnd(), WM_KEYDOWN, 'A', BuildTestKeyboardMessageLParam('A', true));
    Expect(!keyResult.consumed, "Expected disabled rebinds to be ignored for WM_KEYDOWN.");
    Expect(capture.messages.empty(), "Expected disabled rebinds to avoid forwarding WM_KEYDOWN.");

    const InputHandlerResult charResult =
        HandleCharRebinding(window.hwnd(), WM_CHAR, static_cast<WPARAM>('a'), BuildTestKeyboardMessageLParam('A', true));
    Expect(!charResult.consumed, "Expected disabled rebinds to be ignored for WM_CHAR.");
    Expect(capture.messages.empty(), "Expected disabled rebinds to avoid forwarding WM_CHAR.");
}

void RunKeyRebindGuiKeyboardLayoutFullBindAndTriggerTest(TestRunMode runMode = TestRunMode::Automated) {
    DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);
    if (SkipIfNoModernGuiTestGL(window)) { return; }
    PrepareRebindGuiCase("key_rebind_gui_keyboard_layout_full_bind_and_trigger");

    RenderKeyboardInputsFrame(window);
    RenderKeyboardInputsFrame(window);
    RequestGuiTestOpenKeyboardLayout();
    RenderKeyboardInputsFrame(window);
    RequestGuiTestOpenKeyboardLayoutContext('A');
    RenderKeyboardInputsFrame(window);
    RequestGuiTestKeyboardLayoutBeginBind(GuiTestKeyboardLayoutBindTarget::FullOutputVk);
    RenderKeyboardInputsFrame(window);

    SubmitKeyboardBindingEvent('B');
    RenderKeyboardInputsFrame(window);

    Expect(g_config.keyRebinds.rebinds.size() == 1, "Expected the GUI full rebind flow to create exactly one key rebind.");
    const KeyRebind& rebind = g_config.keyRebinds.rebinds.front();
    Expect(rebind.fromKey == 'A', "Expected the GUI full rebind flow to bind from the clicked A key.");
    Expect(rebind.toKey == 'B', "Expected the GUI full rebind flow to bind the trigger output to B.");
    Expect(rebind.useCustomOutput, "Expected the GUI full rebind flow to enable the mirrored output binding state.");
    Expect(rebind.customOutputVK == 'B', "Expected the GUI full rebind flow to store B as the custom output VK.");
    Expect(!rebind.shiftLayerEnabled, "Expected the GUI full rebind flow to leave the shift layer disabled.");

    g_showGui.store(false, std::memory_order_release);
    PublishConfigSnapshot();

    ScopedRebindMessageCapture capture(window.hwnd());
    ScopedKeyboardStateOverride keyboardState;
    keyboardState.SetKeyDown(VK_SHIFT, false);
    keyboardState.SetToggle(VK_CAPITAL, false);
    keyboardState.Apply();

    const LPARAM keyDownLParam = BuildTestKeyboardMessageLParam('A', true);
    const InputHandlerResult keyDownResult = HandleKeyRebinding(window.hwnd(), WM_KEYDOWN, 'A', keyDownLParam);
    Expect(keyDownResult.consumed, "Expected the GUI-created full rebind to consume WM_KEYDOWN.");
    Expect(capture.messages.size() == 1, "Expected the GUI-created full rebind to forward exactly one WM_KEYDOWN message.");
    ExpectCapturedMessage(capture, 0, WM_KEYDOWN, 'B', "GUI full rebind WM_KEYDOWN");

    capture.Clear();
    const InputHandlerResult charResult = HandleCharRebinding(window.hwnd(), WM_CHAR, static_cast<WPARAM>('a'), keyDownLParam);
    Expect(charResult.consumed, "Expected the GUI-created full rebind to consume WM_CHAR.");
    Expect(capture.messages.size() == 1, "Expected the GUI-created full rebind to forward exactly one WM_CHAR message.");
    ExpectCapturedMessage(capture, 0, WM_CHAR, 'b', "GUI full rebind WM_CHAR");
}

void RunKeyRebindGuiKeyboardLayoutSplitBindAndTriggerTest(TestRunMode runMode = TestRunMode::Automated) {
    DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);
    if (SkipIfNoModernGuiTestGL(window)) { return; }
    PrepareRebindGuiCase("key_rebind_gui_keyboard_layout_split_bind_and_trigger");

    RenderKeyboardInputsFrame(window);
    RenderKeyboardInputsFrame(window);
    RequestGuiTestOpenKeyboardLayout();
    RenderKeyboardInputsFrame(window);
    RequestGuiTestOpenKeyboardLayoutContext('A');
    RenderKeyboardInputsFrame(window);
    RequestGuiTestKeyboardLayoutSetSplitMode(true);
    RenderKeyboardInputsFrame(window);

    RequestGuiTestKeyboardLayoutBeginBind(GuiTestKeyboardLayoutBindTarget::TypesVk);
    RenderKeyboardInputsFrame(window);
    SubmitKeyboardBindingEvent('C');
    RenderKeyboardInputsFrame(window);

    RequestGuiTestKeyboardLayoutBeginBind(GuiTestKeyboardLayoutBindTarget::TypesVkShift);
    RenderKeyboardInputsFrame(window);
    SubmitKeyboardBindingEvent('D');
    RenderKeyboardInputsFrame(window);

    RequestGuiTestKeyboardLayoutSetShiftLayerUppercase(true);
    RenderKeyboardInputsFrame(window);
    RequestGuiTestKeyboardLayoutSetShiftLayerUsesCapsLock(true);
    RenderKeyboardInputsFrame(window);
    RequestGuiTestKeyboardLayoutBeginBind(GuiTestKeyboardLayoutBindTarget::TriggersVk);
    RenderKeyboardInputsFrame(window);
    SubmitKeyboardBindingEvent('B');
    RenderKeyboardInputsFrame(window);

    Expect(g_config.keyRebinds.rebinds.size() == 1, "Expected the GUI split rebind flow to create exactly one key rebind.");
    const KeyRebind& rebind = g_config.keyRebinds.rebinds.front();
    Expect(rebind.fromKey == 'A', "Expected the GUI split rebind flow to bind from the clicked A key.");
    Expect(rebind.toKey == 'B', "Expected the GUI split rebind flow to bind B as the trigger key.");
    Expect(rebind.useCustomOutput, "Expected the GUI split rebind flow to enable a custom base output.");
    Expect(rebind.customOutputVK == 'C', "Expected the GUI split rebind flow to store C as the base output VK.");
    Expect(rebind.shiftLayerEnabled, "Expected the GUI split rebind flow to enable the shift layer.");
    Expect(rebind.shiftLayerOutputVK == 'D', "Expected the GUI split rebind flow to store D as the shift-layer output VK.");
    Expect(rebind.shiftLayerOutputShifted, "Expected the GUI split rebind flow to store the shift-layer uppercase toggle.");
    Expect(rebind.shiftLayerUsesCapsLock, "Expected the GUI split rebind flow to store the Caps Lock shift-layer toggle.");

    g_showGui.store(false, std::memory_order_release);
    PublishConfigSnapshot();

    ScopedRebindMessageCapture capture(window.hwnd());
    ScopedKeyboardStateOverride keyboardState;
    keyboardState.SetKeyDown(VK_SHIFT, false);
    keyboardState.SetToggle(VK_CAPITAL, true);
    keyboardState.Apply();

    const LPARAM keyDownLParam = BuildTestKeyboardMessageLParam('A', true);
    const InputHandlerResult keyDownResult = HandleKeyRebinding(window.hwnd(), WM_KEYDOWN, 'A', keyDownLParam);
    Expect(keyDownResult.consumed, "Expected the GUI-created split rebind to consume WM_KEYDOWN.");
    Expect(capture.messages.size() == 1, "Expected the GUI-created split rebind to forward exactly one WM_KEYDOWN message.");
    ExpectCapturedMessage(capture, 0, WM_KEYDOWN, 'B', "GUI split rebind WM_KEYDOWN");

    capture.Clear();
    const InputHandlerResult charResult = HandleCharRebinding(window.hwnd(), WM_CHAR, static_cast<WPARAM>('A'), keyDownLParam);
    Expect(charResult.consumed, "Expected the GUI-created split rebind to consume WM_CHAR.");
    Expect(capture.messages.size() == 1, "Expected the GUI-created split rebind to forward exactly one WM_CHAR message.");
    ExpectCapturedMessage(capture, 0, WM_CHAR, 'D', "GUI split rebind WM_CHAR");
}

void RunKeyRebindGuiKeyboardLayoutMouseSourceBindAndTriggerTest(TestRunMode runMode = TestRunMode::Automated) {
    DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);
    if (SkipIfNoModernGuiTestGL(window)) { return; }
    PrepareRebindGuiCase("key_rebind_gui_keyboard_layout_mouse_source_bind_and_trigger");

    RenderKeyboardInputsFrame(window);
    RenderKeyboardInputsFrame(window);
    RequestGuiTestOpenKeyboardLayout();
    RenderKeyboardInputsFrame(window);
    RequestGuiTestOpenKeyboardLayoutContext(VK_LBUTTON);
    RenderKeyboardInputsFrame(window);
    RequestGuiTestKeyboardLayoutBeginBind(GuiTestKeyboardLayoutBindTarget::FullOutputVk);
    RenderKeyboardInputsFrame(window);

    SubmitKeyboardBindingEvent('Q');
    RenderKeyboardInputsFrame(window);

    Expect(g_config.keyRebinds.rebinds.size() == 1, "Expected the GUI mouse-source rebind flow to create exactly one key rebind.");
    const KeyRebind& rebind = g_config.keyRebinds.rebinds.front();
    Expect(rebind.fromKey == VK_LBUTTON, "Expected the GUI mouse-source rebind flow to bind from MB1.");
    Expect(rebind.toKey == 'Q', "Expected the GUI mouse-source rebind flow to bind Q as the target key.");
    Expect(rebind.useCustomOutput, "Expected the GUI mouse-source rebind flow to enable mirrored output binding state.");
    Expect(rebind.customOutputVK == 'Q', "Expected the GUI mouse-source rebind flow to store Q as the custom output VK.");

    g_showGui.store(false, std::memory_order_release);
    PublishConfigSnapshot();

    ScopedRebindMessageCapture capture(window.hwnd());
    ScopedKeyboardStateOverride keyboardState;
    keyboardState.SetKeyDown(VK_SHIFT, false);
    keyboardState.SetToggle(VK_CAPITAL, false);
    keyboardState.Apply();

    const InputHandlerResult mouseDownResult = HandleKeyRebinding(window.hwnd(), WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(12, 34));
    Expect(mouseDownResult.consumed, "Expected the GUI-created mouse-source rebind to consume WM_LBUTTONDOWN.");
    Expect(capture.messages.size() == 2,
           "Expected the GUI-created mouse-source rebind to forward both WM_KEYDOWN and WM_CHAR for the target key.");
    ExpectCapturedMessage(capture, 0, WM_KEYDOWN, 'Q', "GUI mouse-source rebind WM_KEYDOWN");
    ExpectCapturedMessage(capture, 1, WM_CHAR, 'q', "GUI mouse-source rebind WM_CHAR");

    capture.Clear();
    const InputHandlerResult mouseUpResult = HandleKeyRebinding(window.hwnd(), WM_LBUTTONUP, 0, MAKELPARAM(12, 34));
    Expect(mouseUpResult.consumed, "Expected the GUI-created mouse-source rebind to consume WM_LBUTTONUP.");
    Expect(capture.messages.size() == 1, "Expected the GUI-created mouse-source rebind to forward exactly one WM_KEYUP message.");
    ExpectCapturedMessage(capture, 0, WM_KEYUP, 'Q', "GUI mouse-source rebind WM_KEYUP");
}

    void RunKeyRebindGuiKeyboardLayoutFullBindScanPickerRuntimeTest(TestRunMode runMode = TestRunMode::Automated) {
        constexpr DWORD kNumpadEnterScan = 0xE01C;

        DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);
        if (SkipIfNoModernGuiTestGL(window)) { return; }
        PrepareRebindGuiCase("key_rebind_gui_keyboard_layout_full_bind_scan_picker_runtime");

        OpenKeyboardLayoutContext(window, 'A');
        BindKeyboardLayoutTarget(window, false, VK_RETURN);
        OpenKeyboardLayoutScanPicker(window);
        SetKeyboardLayoutScanFilter(window, GuiTestKeyboardLayoutScanFilterGroup::Numpad);
        SelectKeyboardLayoutScan(window, kNumpadEnterScan);

        Expect(g_config.keyRebinds.rebinds.size() == 1, "Expected the full rebind scan picker flow to create exactly one key rebind.");
        const KeyRebind& rebind = g_config.keyRebinds.rebinds.front();
        Expect(rebind.fromKey == 'A', "Expected the full rebind scan picker flow to bind from the clicked A key.");
        Expect(rebind.toKey == VK_RETURN, "Expected the full rebind scan picker flow to bind Return as the trigger key.");
        Expect(rebind.useCustomOutput, "Expected the full rebind scan picker flow to enable custom output state.");
        Expect(rebind.customOutputVK == VK_RETURN, "Expected the full rebind scan picker flow to mirror Return as the output VK.");
        Expect(rebind.customOutputScanCode == kNumpadEnterScan,
            "Expected the full rebind scan picker flow to store the Numpad Enter scan code override.");

        g_showGui.store(false, std::memory_order_release);
        PublishConfigSnapshot();

        ScopedRebindMessageCapture capture(window.hwnd());
        ScopedKeyboardStateOverride keyboardState;
        keyboardState.SetKeyDown(VK_SHIFT, false);
        keyboardState.SetToggle(VK_CAPITAL, false);
        keyboardState.Apply();

        const InputHandlerResult keyDownResult = HandleKeyRebinding(window.hwnd(), WM_KEYDOWN, 'A', BuildTestKeyboardMessageLParam('A', true));
        Expect(keyDownResult.consumed, "Expected the GUI-created full rebind scan picker flow to consume WM_KEYDOWN.");
        Expect(capture.messages.size() == 1,
            "Expected the GUI-created full rebind scan picker flow to forward exactly one WM_KEYDOWN message.");
        ExpectCapturedMessage(capture, 0, WM_KEYDOWN, VK_RETURN, "GUI full rebind scan picker WM_KEYDOWN");
        ExpectCapturedScanCode(capture, 0, kNumpadEnterScan, "GUI full rebind scan picker WM_KEYDOWN");
    }

    void RunKeyRebindGuiKeyboardLayoutScanPickerFilterTest(TestRunMode runMode = TestRunMode::Automated) {
        constexpr DWORD kNumpadEnterScan = 0xE01C;

        DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);
        if (SkipIfNoModernGuiTestGL(window)) { return; }
        PrepareRebindGuiCase("key_rebind_gui_keyboard_layout_scan_picker_filter");

        OpenKeyboardLayoutContext(window, 'A');
        BindKeyboardLayoutTarget(window, true, VK_RETURN);
        OpenKeyboardLayoutScanPicker(window);

        SetKeyboardLayoutScanFilter(window, GuiTestKeyboardLayoutScanFilterGroup::Alpha);
        SelectKeyboardLayoutScan(window, kNumpadEnterScan);

        Expect(g_config.keyRebinds.rebinds.size() == 1, "Expected the split rebind scan picker filter flow to create exactly one key rebind.");
        const KeyRebind& blockedRebind = g_config.keyRebinds.rebinds.front();
        Expect(blockedRebind.toKey == VK_RETURN, "Expected the split rebind scan picker filter flow to keep Return as the trigger key.");
        Expect(blockedRebind.customOutputScanCode == 0,
            "Expected the Alpha filter to reject selecting the Numpad Enter scan code override.");
        Expect(!blockedRebind.useCustomOutput,
            "Expected the Alpha filter rejection to avoid enabling custom output state for the trigger scan override.");

        SetKeyboardLayoutScanFilter(window, GuiTestKeyboardLayoutScanFilterGroup::Numpad);
        SelectKeyboardLayoutScan(window, kNumpadEnterScan);

        const KeyRebind& selectedRebind = g_config.keyRebinds.rebinds.front();
        Expect(selectedRebind.customOutputScanCode == kNumpadEnterScan,
            "Expected the Numpad filter to allow selecting the Numpad Enter scan code override.");
        Expect(selectedRebind.useCustomOutput,
            "Expected selecting the Numpad Enter scan code override to enable custom output state.");
    }

    void RunKeyRebindGuiKeyboardLayoutScanPickerResetToDefaultTest(TestRunMode runMode = TestRunMode::Automated) {
        constexpr DWORD kNumpadEnterScan = 0xE01C;
        constexpr DWORD kDefaultEnterScan = 0x001C;

        DummyWindow window(kWindowWidth, kWindowHeight, runMode == TestRunMode::Visual);
        if (SkipIfNoModernGuiTestGL(window)) { return; }
        PrepareRebindGuiCase("key_rebind_gui_keyboard_layout_scan_picker_reset_to_default");

        OpenKeyboardLayoutContext(window, 'A');
        BindKeyboardLayoutTarget(window, true, VK_RETURN);
        OpenKeyboardLayoutScanPicker(window);
        SetKeyboardLayoutScanFilter(window, GuiTestKeyboardLayoutScanFilterGroup::Numpad);
        SelectKeyboardLayoutScan(window, kNumpadEnterScan);

        Expect(g_config.keyRebinds.rebinds.size() == 1, "Expected the split rebind scan picker reset flow to create exactly one key rebind.");
        Expect(g_config.keyRebinds.rebinds.front().customOutputScanCode == kNumpadEnterScan,
            "Expected the split rebind scan picker reset flow to start with the Numpad Enter scan code override.");

        ResetKeyboardLayoutScanToDefault(window);

        const KeyRebind& rebind = g_config.keyRebinds.rebinds.front();
        Expect(rebind.fromKey == 'A', "Expected resetting the scan picker to preserve the source key.");
        Expect(rebind.toKey == VK_RETURN, "Expected resetting the scan picker to preserve the trigger key.");
        Expect(rebind.customOutputScanCode == 0, "Expected resetting the scan picker to clear the trigger scan override.");
        Expect(!rebind.useCustomOutput,
            "Expected resetting the scan picker to disable custom output state when no other overrides remain.");

        g_showGui.store(false, std::memory_order_release);
        PublishConfigSnapshot();

        ScopedRebindMessageCapture capture(window.hwnd());
        ScopedKeyboardStateOverride keyboardState;
        keyboardState.SetKeyDown(VK_SHIFT, false);
        keyboardState.SetToggle(VK_CAPITAL, false);
        keyboardState.Apply();

        const InputHandlerResult keyDownResult = HandleKeyRebinding(window.hwnd(), WM_KEYDOWN, 'A', BuildTestKeyboardMessageLParam('A', true));
        Expect(keyDownResult.consumed, "Expected the reset split rebind scan picker flow to consume WM_KEYDOWN.");
        Expect(capture.messages.size() == 1,
            "Expected the reset split rebind scan picker flow to forward exactly one WM_KEYDOWN message.");
        ExpectCapturedMessage(capture, 0, WM_KEYDOWN, VK_RETURN, "GUI split rebind scan picker reset WM_KEYDOWN");
        ExpectCapturedScanCode(capture, 0, kDefaultEnterScan, "GUI split rebind scan picker reset WM_KEYDOWN");
    }