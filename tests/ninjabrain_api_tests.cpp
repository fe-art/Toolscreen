#include "features/ninjabrain_api.h"

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <windows.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#ifndef TOOLSCREEN_TEST_JAVA_EXECUTABLE
#error TOOLSCREEN_TEST_JAVA_EXECUTABLE must be defined for ninjabrain_api_tests.
#endif

#ifndef TOOLSCREEN_TEST_NINJABRAIN_BOT_JAR
#error TOOLSCREEN_TEST_NINJABRAIN_BOT_JAR must be defined for ninjabrain_api_tests.
#endif

namespace {

using json = nlohmann::json;
using namespace std::chrono_literals;

constexpr char kServerBaseUrl[] = "http://127.0.0.1:52533";
constexpr auto kStartupTimeout = 20s;
constexpr auto kReconnectTimeout = 20s;

const std::filesystem::path kJavaExecutable = std::filesystem::path(TOOLSCREEN_TEST_JAVA_EXECUTABLE);
const std::filesystem::path kNinjabrainBotJar = std::filesystem::path(TOOLSCREEN_TEST_NINJABRAIN_BOT_JAR);

[[noreturn]] void Fail(const std::string& message) {
    throw std::runtime_error(message);
}

std::string DescribeWindowsError(DWORD errorCode) {
    LPSTR buffer = nullptr;
    const DWORD size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        0,
        reinterpret_cast<LPSTR>(&buffer),
        0,
        nullptr);

    std::string message = size > 0 && buffer ? std::string(buffer, size) : "Unknown error";
    if (buffer) { LocalFree(buffer); }

    while (!message.empty() && (message.back() == '\r' || message.back() == '\n')) {
        message.pop_back();
    }

    return message;
}

#define REQUIRE(condition)                                                                                             \
    do {                                                                                                               \
        if (!(condition)) {                                                                                            \
            std::ostringstream requirementStream;                                                                      \
            requirementStream << "Requirement failed: " << #condition << " at " << __FILE__ << ':' << __LINE__;     \
            Fail(requirementStream.str());                                                                             \
        }                                                                                                              \
    } while (false)

template <typename T, typename U>
void RequireEqual(const T& actual, const U& expected, const char* label) {
    if (!(actual == expected)) {
        std::ostringstream message;
        message << label << " mismatch. Expected '" << expected << "' but got '" << actual << "'.";
        Fail(message.str());
    }
}

void RequireNear(double actual, double expected, double epsilon, const char* label) {
    if (std::abs(actual - expected) > epsilon) {
        std::ostringstream message;
        message << std::fixed << std::setprecision(6) << label << " mismatch. Expected " << expected << " but got "
                << actual << ".";
        Fail(message.str());
    }
}

struct TestCase {
    const char* name;
    void (*function)();
};

std::vector<TestCase>& TestRegistry() {
    static std::vector<TestCase> registry;
    return registry;
}

struct TestRegistration {
    TestRegistration(const char* name, void (*function)()) {
        TestRegistry().push_back({ name, function });
    }
};

#define TOOLSCREEN_TEST(name)                                                                                          \
    static void name();                                                                                                \
    namespace {                                                                                                        \
    TestRegistration registration_##name(#name, &name);                                                               \
    }                                                                                                                  \
    static void name()

template <typename Predicate>
void WaitFor(
    Predicate&& predicate,
    std::chrono::steady_clock::duration timeout,
    std::chrono::milliseconds pollInterval,
    const std::string& failureMessage) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) { return; }
        std::this_thread::sleep_for(pollInterval);
    }

    Fail(failureMessage);
}

std::string ReadTextFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

std::wstring QuoteCommandLineArgument(const std::wstring& argument) {
    if (argument.empty() || argument.find_first_of(L" \t\"") != std::wstring::npos) {
        std::wstring quoted = L"\"";
        size_t backslashCount = 0;
        for (wchar_t ch : argument) {
            if (ch == L'\\') {
                ++backslashCount;
                continue;
            }

            if (ch == L'\"') {
                quoted.append(backslashCount * 2 + 1, L'\\');
                quoted.push_back(ch);
                backslashCount = 0;
                continue;
            }

            quoted.append(backslashCount, L'\\');
            backslashCount = 0;
            quoted.push_back(ch);
        }

        quoted.append(backslashCount * 2, L'\\');
        quoted.push_back(L'\"');
        return quoted;
    }

    return argument;
}

std::wstring BuildCommandLine(const std::vector<std::wstring>& arguments) {
    std::wstring commandLine;
    for (size_t index = 0; index < arguments.size(); ++index) {
        if (index > 0) { commandLine.push_back(L' '); }
        commandLine += QuoteCommandLineArgument(arguments[index]);
    }
    return commandLine;
}

class TemporaryDirectory {
  public:
    explicit TemporaryDirectory(const std::string& prefix) {
        const auto uniqueName = prefix + "-" + std::to_string(GetCurrentProcessId()) + "-" +
                                std::to_string(static_cast<long long>(GetTickCount64()));
        path_ = std::filesystem::temp_directory_path() / uniqueName;
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    const std::filesystem::path& path() const { return path_; }

  private:
    std::filesystem::path path_;
};

class NinjabrainBotProcess {
  public:
    ~NinjabrainBotProcess() {
        Stop();
    }

    void Start() {
        Stop();

        if (!std::filesystem::exists(kJavaExecutable)) {
            Fail("Java executable not found at '" + kJavaExecutable.string() + "'.");
        }
        if (!std::filesystem::exists(kNinjabrainBotJar)) {
            Fail("Ninjabrain Bot jar not found at '" + kNinjabrainBotJar.string() + "'.");
        }

        home_.emplace("toolscreen-ninjabrain");
        std::filesystem::create_directories(home_->path() / "prefs");
        logPath_ = home_->path() / "ninjabrainbot.log";

        SECURITY_ATTRIBUTES securityAttributes{};
        securityAttributes.nLength = sizeof(securityAttributes);
        securityAttributes.bInheritHandle = TRUE;

        HANDLE logHandle = CreateFileW(
            logPath_.c_str(),
            GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE,
            &securityAttributes,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (logHandle == INVALID_HANDLE_VALUE) {
            const DWORD error = GetLastError();
            home_.reset();
            Fail("Failed to create Ninjabrain Bot log file: " + DescribeWindowsError(error));
        }

        STARTUPINFOW startupInfo{};
        startupInfo.cb = sizeof(startupInfo);
        startupInfo.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
        startupInfo.wShowWindow = SW_HIDE;
        startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        startupInfo.hStdOutput = logHandle;
        startupInfo.hStdError = logHandle;

        const std::vector<std::wstring> arguments = {
            kJavaExecutable.wstring(),
            L"-Duser.home=" + home_->path().wstring(),
            L"-Djava.util.prefs.userRoot=" + (home_->path() / "prefs").wstring(),
            L"-jar",
            kNinjabrainBotJar.wstring(),
        };
        std::wstring commandLine = BuildCommandLine(arguments);
        std::vector<wchar_t> commandLineBuffer(commandLine.begin(), commandLine.end());
        commandLineBuffer.push_back(L'\0');

        STARTUPINFOEXW startupInfoEx{};
        startupInfoEx.StartupInfo = startupInfo;

        BOOL created = CreateProcessW(
            nullptr,
            commandLineBuffer.data(),
            nullptr,
            nullptr,
            TRUE,
            CREATE_NO_WINDOW,
            nullptr,
            home_->path().c_str(),
            &startupInfoEx.StartupInfo,
            &processInfo_);

        CloseHandle(logHandle);

        if (!created) {
            const DWORD error = GetLastError();
            home_.reset();
            Fail("Failed to launch Ninjabrain Bot: " + DescribeWindowsError(error));
        }

        running_ = true;

        WaitFor(
            [&]() {
                if (!IsRunning()) {
                    Fail("Ninjabrain Bot exited before the API became ready. Log output:\n" + ReadLog());
                }

                httplib::Client client(kServerBaseUrl);
                client.set_connection_timeout(1, 0);
                client.set_read_timeout(1, 0);

                const auto response = client.Get("/api/v1/ping");
                return response && response->status == 200 && response->body.find("active") != std::string::npos;
            },
            kStartupTimeout,
            200ms,
            "Timed out waiting for the Ninjabrain Bot API to start. Log output:\n" + ReadLog());
    }

    void Stop() {
        if (!running_) {
            home_.reset();
            logPath_.clear();
            return;
        }

        DWORD exitCode = 0;
        if (GetExitCodeProcess(processInfo_.hProcess, &exitCode) && exitCode == STILL_ACTIVE) {
            TerminateProcess(processInfo_.hProcess, 0);
            WaitForSingleObject(processInfo_.hProcess, 10000);
        }

        if (processInfo_.hThread) {
            CloseHandle(processInfo_.hThread);
            processInfo_.hThread = nullptr;
        }
        if (processInfo_.hProcess) {
            CloseHandle(processInfo_.hProcess);
            processInfo_.hProcess = nullptr;
        }

        running_ = false;
        home_.reset();
        logPath_.clear();
    }

    std::string ReadLog() const {
        if (logPath_.empty() || !std::filesystem::exists(logPath_)) { return {}; }
        return ReadTextFile(logPath_);
    }

  private:
    bool IsRunning() const {
        if (!processInfo_.hProcess) { return false; }

        DWORD exitCode = 0;
        if (!GetExitCodeProcess(processInfo_.hProcess, &exitCode)) { return false; }
        return exitCode == STILL_ACTIVE;
    }

    std::optional<TemporaryDirectory> home_;
    std::filesystem::path logPath_;
    PROCESS_INFORMATION processInfo_{};
    bool running_ = false;
};

std::string GetEndpointBody(const char* path) {
    httplib::Client client(kServerBaseUrl);
    client.set_connection_timeout(5, 0);
    client.set_read_timeout(5, 0);

    const auto response = client.Get(path);
    if (!response) {
        Fail(std::string("Request failed for '") + path + "': " + httplib::to_string(response.error()));
    }
    if (response->status != 200) {
        std::ostringstream message;
        message << "Unexpected HTTP status " << response->status << " for '" << path << "'.";
        Fail(message.str());
    }

    return response->body;
}

std::string ReadFirstSseMessage(const char* path) {
    httplib::Client client(kServerBaseUrl);
    client.set_keep_alive(true);
    client.set_connection_timeout(5, 0);
    client.set_read_timeout(5, 0);

    const httplib::Headers headers = {
        { "Accept", "text/event-stream" },
        { "Cache-Control", "no-cache" },
    };

    httplib::sse::SSEClient sse(client, path, headers);
    std::promise<std::string> firstMessagePromise;
    auto firstMessageFuture = firstMessagePromise.get_future();
    std::atomic<bool> completed = false;

    auto completeWithMessage = [&](const std::string& message) {
        if (!completed.exchange(true)) {
            firstMessagePromise.set_value(message);
        }
    };
    auto completeWithError = [&](const std::string& message) {
        if (!completed.exchange(true)) {
            firstMessagePromise.set_exception(std::make_exception_ptr(std::runtime_error(message)));
        }
    };

    sse.on_message([&](const httplib::sse::SSEMessage& message) {
        completeWithMessage(message.data);
        sse.stop();
        client.stop();
    });
    sse.on_error([&](httplib::Error error) {
        if (error == httplib::Error::Canceled && completed.load()) { return; }
        completeWithError(std::string("SSE request failed for '") + path + "': " + httplib::to_string(error));
        sse.stop();
        client.stop();
    });

    std::thread worker([&]() { sse.start(); });

    if (firstMessageFuture.wait_for(10s) != std::future_status::ready) {
        sse.stop();
        client.stop();
        if (worker.joinable()) { worker.join(); }
        Fail(std::string("Timed out waiting for the first SSE message from '") + path + "'.");
    }

    std::string message;
    try {
        message = firstMessageFuture.get();
    } catch (...) {
        sse.stop();
        client.stop();
        if (worker.joinable()) { worker.join(); }
        throw;
    }

    sse.stop();
    client.stop();
    if (worker.joinable()) { worker.join(); }
    return message;
}

struct SessionState {
    mutable std::mutex mutex;
    NinjabrainData data;
    std::vector<std::string> logs;
};

std::string DescribeSessionState(const SessionState& state) {
    std::lock_guard<std::mutex> lock(state.mutex);
    std::ostringstream description;
    description << "resultType=" << state.data.resultType << ", boatState=" << state.data.boatState
                << ", hasBoatAngle=" << (state.data.hasBoatAngle ? "true" : "false")
                << ", boatAngle=" << state.data.boatAngle << ", predictionCount=" << state.data.predictionCount
                << ", eyeCount=" << state.data.eyeCount
                << ", informationMessageCount=" << state.data.informationMessageCount
                << ", blindEnabled=" << (state.data.blind.enabled ? "true" : "false")
                << ", blindHasResult=" << (state.data.blind.hasResult ? "true" : "false");

    if (!state.logs.empty()) {
        description << ", lastLog='" << state.logs.back() << '\'';
    }

    return description.str();
}

bool SessionLogContains(const SessionState& state, const std::string& fragment) {
    std::lock_guard<std::mutex> lock(state.mutex);
    for (const auto& logMessage : state.logs) {
        if (logMessage.find(fragment) != std::string::npos) { return true; }
    }
    return false;
}

NinjabrainApiSessionCallbacks CreateSessionCallbacks(SessionState& state) {
    NinjabrainApiSessionCallbacks callbacks;
    callbacks.onLog = [&](const std::string& message) {
        std::lock_guard<std::mutex> lock(state.mutex);
        state.logs.push_back(message);
    };
    callbacks.onStrongholdConnect = []() {};
    callbacks.onStrongholdMessage = [&](const std::string& payload) {
        std::lock_guard<std::mutex> lock(state.mutex);
        ApplyNinjabrainStrongholdEvent(payload, state.data, [&](const std::string& message) {
            state.logs.push_back(message);
        });
    };
    callbacks.onStrongholdDisconnect = [&](const std::string&) {
        std::lock_guard<std::mutex> lock(state.mutex);
        ClearNinjabrainStrongholdData(state.data);
    };
    callbacks.onInformationMessagesConnect = []() {};
    callbacks.onInformationMessagesMessage = [&](const std::string& payload) {
        std::lock_guard<std::mutex> lock(state.mutex);
        ApplyNinjabrainInformationMessagesEvent(payload, state.data, [&](const std::string& message) {
            state.logs.push_back(message);
        });
    };
    callbacks.onInformationMessagesDisconnect = [&](const std::string&) {
        std::lock_guard<std::mutex> lock(state.mutex);
        ClearNinjabrainInformationMessagesData(state.data);
    };
    callbacks.onBoatConnect = []() {};
    callbacks.onBoatMessage = [&](const std::string& payload) {
        std::lock_guard<std::mutex> lock(state.mutex);
        ApplyNinjabrainBoatEvent(payload, state.data, [&](const std::string& message) {
            state.logs.push_back(message);
        });
    };
    callbacks.onBoatDisconnect = [&](const std::string&) {
        std::lock_guard<std::mutex> lock(state.mutex);
        ClearNinjabrainBoatData(state.data);
    };
    callbacks.onBlindConnect = []() {};
    callbacks.onBlindMessage = [&](const std::string& payload) {
        std::lock_guard<std::mutex> lock(state.mutex);
        ApplyNinjabrainBlindEvent(payload, state.data, [&](const std::string& message) {
            state.logs.push_back(message);
        });
    };
    callbacks.onBlindDisconnect = [&](const std::string&) {
        std::lock_guard<std::mutex> lock(state.mutex);
        ClearNinjabrainBlindData(state.data);
    };
    return callbacks;
}

TOOLSCREEN_TEST(connection_tracker_reports_offline_state) {
    NinjabrainApiConnectionTracker tracker;

    tracker.Start(std::string(" ") + kServerBaseUrl + "/");

    {
        const NinjabrainApiStatus status = tracker.Snapshot();
        REQUIRE(status.connectionState == NinjabrainApiConnectionState::Connecting);
        RequireEqual(status.apiBaseUrl, std::string(kServerBaseUrl), "apiBaseUrl");
        REQUIRE(status.error.empty());
    }

    tracker.MarkStrongholdDisconnected("Connection refused");

    {
        const NinjabrainApiStatus status = tracker.Snapshot();
        REQUIRE(status.connectionState == NinjabrainApiConnectionState::Offline);
        REQUIRE(status.error.find("Connection refused") != std::string::npos);
    }

    tracker.MarkBoatConnected();

    {
        const NinjabrainApiStatus status = tracker.Snapshot();
        REQUIRE(status.connectionState == NinjabrainApiConnectionState::Connected);
    }

    tracker.Stop();

    {
        const NinjabrainApiStatus status = tracker.Snapshot();
        REQUIRE(status.connectionState == NinjabrainApiConnectionState::Stopped);
        REQUIRE(status.error.empty());
    }
}

TOOLSCREEN_TEST(connection_tracker_treats_information_messages_as_connected) {
    NinjabrainApiConnectionTracker tracker;

    tracker.Start(std::string(" ") + kServerBaseUrl + "/");
    tracker.MarkStrongholdDisconnected("Connection refused");

    {
        const NinjabrainApiStatus status = tracker.Snapshot();
        REQUIRE(status.connectionState == NinjabrainApiConnectionState::Offline);
    }

    tracker.MarkInformationMessagesConnected();

    {
        const NinjabrainApiStatus status = tracker.Snapshot();
        REQUIRE(status.connectionState == NinjabrainApiConnectionState::Connected);
    }
}

TOOLSCREEN_TEST(connection_tracker_treats_blind_as_connected) {
    NinjabrainApiConnectionTracker tracker;

    tracker.Start(std::string(" ") + kServerBaseUrl + "/");
    tracker.MarkStrongholdDisconnected("Connection refused");

    {
        const NinjabrainApiStatus status = tracker.Snapshot();
        REQUIRE(status.connectionState == NinjabrainApiConnectionState::Offline);
    }

    tracker.MarkBlindConnected();

    {
        const NinjabrainApiStatus status = tracker.Snapshot();
        REQUIRE(status.connectionState == NinjabrainApiConnectionState::Connected);
    }
}

TOOLSCREEN_TEST(boat_event_parses_snapshot) {
    NinjabrainData data;

    ApplyNinjabrainBoatEvent(R"({"boatAngle":0,"boatState":"VALID"})", data);

    RequireEqual(data.boatState, std::string("VALID"), "boatState");
    RequireNear(data.boatAngle, 0.0, 1e-9, "boatAngle");
    REQUIRE(data.hasBoatAngle);
}

TOOLSCREEN_TEST(stronghold_failure_preserves_boat) {
    NinjabrainData data;
    data.boatState = "VALID";
    data.boatAngle = 12.5;
    data.hasBoatAngle = true;
    data.blind.enabled = true;
    data.blind.hasResult = true;
    data.blind.evaluation = "NOT_IN_RING";
    data.resultType = "TRIANGULATED";
    data.eyeCount = 2;
    data.predictionCount = 1;
    data.validPrediction = true;

    ApplyNinjabrainStrongholdEvent(R"({"resultType":"FAILED"})", data);

    RequireEqual(data.resultType, std::string("FAILED"), "resultType");
    RequireEqual(data.boatState, std::string("VALID"), "boatState");
    RequireNear(data.boatAngle, 12.5, 1e-9, "boatAngle");
    REQUIRE(data.hasBoatAngle);
    REQUIRE(data.blind.enabled);
    REQUIRE(data.blind.hasResult);
    RequireEqual(data.blind.evaluation, std::string("NOT_IN_RING"), "blind.evaluation");
    RequireEqual(data.eyeCount, 0, "eyeCount");
    RequireEqual(data.predictionCount, 0, "predictionCount");
    REQUIRE(!data.validPrediction);
}

TOOLSCREEN_TEST(blind_event_parses_snapshot) {
    NinjabrainData data;

    ApplyNinjabrainBlindEvent(
        R"({
            "isBlindModeEnabled":true,
            "hasDivine":false,
            "blindResult":{
                "evaluation":"NOT_IN_RING",
                "xInNether":27.15,
                "improveDistance":198.84401723376595,
                "zInNether":-0.57,
                "averageDistance":1718.3948071451914,
                "improveDirection":-1.591787718184331,
                "highrollProbability":0,
                "highrollThreshold":400
            }
        })",
        data);

    REQUIRE(data.blind.enabled);
    REQUIRE(!data.blind.hasDivine);
    REQUIRE(data.blind.hasResult);
    RequireEqual(data.blind.evaluation, std::string("NOT_IN_RING"), "blind.evaluation");
    RequireNear(data.blind.xInNether, 27.15, 1e-9, "blind.xInNether");
    RequireNear(data.blind.zInNether, -0.57, 1e-9, "blind.zInNether");
    RequireNear(data.blind.improveDistance, 198.84401723376595, 1e-9, "blind.improveDistance");
    RequireNear(data.blind.averageDistance, 1718.3948071451914, 1e-9, "blind.averageDistance");
    RequireNear(data.blind.improveDirection, -1.591787718184331, 1e-9, "blind.improveDirection");
    RequireNear(data.blind.highrollProbability, 0.0, 1e-9, "blind.highrollProbability");
    RequireNear(data.blind.highrollThreshold, 400.0, 1e-9, "blind.highrollThreshold");
}

TOOLSCREEN_TEST(blind_event_disabled_clears_snapshot) {
    NinjabrainData data;
    data.blind.enabled = true;
    data.blind.hasDivine = true;
    data.blind.hasResult = true;
    data.blind.evaluation = "NOT_IN_RING";

    ApplyNinjabrainBlindEvent(
        R"({
            "isBlindModeEnabled":false,
            "hasDivine":false,
            "blindResult":{}
        })",
        data);

    REQUIRE(!data.blind.enabled);
    REQUIRE(!data.blind.hasDivine);
    REQUIRE(!data.blind.hasResult);
    RequireEqual(data.blind.evaluation, std::string(), "blind.evaluation");
}

TOOLSCREEN_TEST(information_messages_event_parses_snapshot) {
    NinjabrainData data;

    ApplyNinjabrainInformationMessagesEvent(
        R"({
            "informationMessages":[
                {
                    "severity":"WARNING",
                    "type":"MISMEASURE",
                    "message":"Detected unusually large errors, you probably mismeasured or your standard deviation is too low."
                },
                {
                    "severity":"INFO",
                    "type":"NEXT_THROW_DIRECTION",
                    "message":"Go left 1 blocks, or right 1 blocks, for ~95% certainty after next measurement."
                }
            ]
        })",
        data);

    RequireEqual(data.informationMessageCount, 2, "informationMessageCount");
    RequireEqual(data.informationMessages[0].severity, std::string("WARNING"), "informationMessages[0].severity");
    RequireEqual(data.informationMessages[0].type, std::string("MISMEASURE"), "informationMessages[0].type");
    RequireEqual(
        data.informationMessages[0].message,
        std::string("Detected unusually large errors, you probably mismeasured or your standard deviation is too low."),
        "informationMessages[0].message");
    RequireEqual(data.informationMessages[1].severity, std::string("INFO"), "informationMessages[1].severity");
    RequireEqual(data.informationMessages[1].type, std::string("NEXT_THROW_DIRECTION"), "informationMessages[1].type");
    RequireEqual(
        data.informationMessages[1].message,
        std::string("Go left 1 blocks, or right 1 blocks, for ~95% certainty after next measurement."),
        "informationMessages[1].message");
}

TOOLSCREEN_TEST(stronghold_clear_preserves_information_messages) {
    NinjabrainData data;
    data.informationMessageCount = 1;
    data.informationMessages[0].severity = "WARNING";
    data.informationMessages[0].type = "MISMEASURE";
    data.informationMessages[0].message = "Keep this warning";

    ApplyNinjabrainStrongholdEvent(R"({"resultType":"NONE"})", data);

    RequireEqual(data.resultType, std::string("NONE"), "resultType");
    RequireEqual(data.informationMessageCount, 1, "informationMessageCount");
    RequireEqual(data.informationMessages[0].severity, std::string("WARNING"), "informationMessages[0].severity");
    RequireEqual(data.informationMessages[0].type, std::string("MISMEASURE"), "informationMessages[0].type");
    RequireEqual(data.informationMessages[0].message, std::string("Keep this warning"), "informationMessages[0].message");
}

TOOLSCREEN_TEST(stronghold_clear_preserves_blind) {
    NinjabrainData data;
    data.blind.enabled = true;
    data.blind.hasResult = true;
    data.blind.evaluation = "NOT_IN_RING";
    data.blind.xInNether = 27.15;

    ApplyNinjabrainStrongholdEvent(R"({"resultType":"NONE"})", data);

    REQUIRE(data.blind.enabled);
    REQUIRE(data.blind.hasResult);
    RequireEqual(data.blind.evaluation, std::string("NOT_IN_RING"), "blind.evaluation");
    RequireNear(data.blind.xInNether, 27.15, 1e-9, "blind.xInNether");
}

TOOLSCREEN_TEST(stronghold_event_preserves_information_messages) {
    NinjabrainData data;
    data.informationMessageCount = 1;
    data.informationMessages[0].severity = "WARNING";
    data.informationMessages[0].type = "MISMEASURE";
    data.informationMessages[0].message = "Keep this warning";

    ApplyNinjabrainStrongholdEvent(
        R"({
            "eyeThrows":[],
            "resultType":"TRIANGULATION",
            "playerPosition":{},
            "predictions":[{"chunkX":1,"chunkZ":2,"certainty":0.5,"overworldDistance":100.0}]
        })",
        data);

    RequireEqual(data.resultType, std::string("TRIANGULATION"), "resultType");
    RequireEqual(data.informationMessageCount, 1, "informationMessageCount");
    RequireEqual(data.informationMessages[0].severity, std::string("WARNING"), "informationMessages[0].severity");
    RequireEqual(data.informationMessages[0].type, std::string("MISMEASURE"), "informationMessages[0].type");
    RequireEqual(data.informationMessages[0].message, std::string("Keep this warning"), "informationMessages[0].message");
}

TOOLSCREEN_TEST(stronghold_event_parses_prediction_details) {
    NinjabrainData data;
    data.boatState = "VALID";
    data.boatAngle = 18.0;
    data.hasBoatAngle = true;

    ApplyNinjabrainStrongholdEvent(
        R"({
            "resultType":"TRIANGULATED",
            "playerPosition":{
                "xInOverworld":4.0,
                "zInOverworld":-12.0,
                "isInNether":false,
                "horizontalAngle":10.0
            },
            "eyeThrows":[
                {
                    "angle":30.0,
                    "angleWithoutCorrection":29.75,
                    "correction":0.25,
                    "error":0.0025,
                    "type":"NORMAL",
                    "correctionIncrements":1
                },
                {
                    "angle":35.5,
                    "angleWithoutCorrection":35.25,
                    "correction":0.25,
                    "error":-0.0004,
                    "type":"PROJECTED",
                    "correctionIncrements":2
                }
            ],
            "predictions":[
                {
                    "chunkX":0,
                    "chunkZ":0,
                    "certainty":0.91,
                    "overworldDistance":128.5
                },
                {
                    "chunkX":1,
                    "chunkZ":-1,
                    "certainty":0.42,
                    "overworldDistance":144.0
                }
            ]
        })",
        data);

    RequireEqual(data.resultType, std::string("TRIANGULATED"), "resultType");
    REQUIRE(data.hasPlayerPos);
    RequireNear(data.playerX, 4.0, 1e-9, "playerX");
    RequireNear(data.playerZ, -12.0, 1e-9, "playerZ");
    RequireEqual(data.eyeCount, 2, "eyeCount");
    RequireNear(data.lastAngle, 35.5, 1e-9, "lastAngle");
    RequireNear(data.lastAngleWithoutCorrection, 35.25, 1e-9, "lastAngleWithoutCorrection");
    RequireNear(data.lastCorrection, 0.25, 1e-9, "lastCorrection");
    RequireNear(data.lastThrowError, -0.0004, 1e-9, "lastThrowError");
    RequireNear(data.prevAngle, 30.0, 1e-9, "prevAngle");
    REQUIRE(data.hasAngleChange);
    REQUIRE(data.hasNetherAngle);
    RequireNear(data.netherAngle, 35.5, 1e-9, "netherAngle");
    RequireNear(data.netherAngleDiff, 5.5, 1e-9, "netherAngleDiff");
    RequireEqual(data.predictionCount, 2, "predictionCount");
    RequireEqual(data.strongholdX, 4, "strongholdX");
    RequireEqual(data.strongholdZ, 4, "strongholdZ");
    RequireNear(data.distance, 128.5, 1e-9, "distance");
    RequireNear(data.certainty, 0.91, 1e-9, "certainty");
    REQUIRE(data.validPrediction);
    REQUIRE(data.predictionAngles[0].valid);
    RequireNear(data.predictionAngles[0].actualAngle, 0.0, 1e-9, "predictionAngles[0].actualAngle");
    RequireNear(data.predictionAngles[0].neededCorrection, -10.0, 1e-9, "predictionAngles[0].neededCorrection");
    RequireEqual(data.boatState, std::string("VALID"), "boatState");
    RequireNear(data.boatAngle, 18.0, 1e-9, "boatAngle");
    REQUIRE(data.hasBoatAngle);
}

TOOLSCREEN_TEST(stronghold_display_distance_uses_nether_scale_when_player_is_in_nether) {
    NinjabrainData data;

    ApplyNinjabrainStrongholdEvent(
        R"({
            "resultType":"TRIANGULATED",
            "playerPosition":{
                "xInOverworld":4.0,
                "zInOverworld":-12.0,
                "isInNether":true,
                "horizontalAngle":10.0
            },
            "predictions":[
                {
                    "chunkX":0,
                    "chunkZ":0,
                    "certainty":0.91,
                    "overworldDistance":128.5
                }
            ]
        })",
        data);

    REQUIRE(data.playerInNether);
    RequireNear(
        GetNinjabrainPredictionDisplayDistance(data, data.predictions[0]),
        16.0625,
        1e-9,
        "displayDistance");
}

TOOLSCREEN_TEST(stronghold_increment_recovery_without_correction_increments) {
    NinjabrainData data;
    data.eyeCount = 1;
    data.throws[0].correction = 0.25;
    data.correctionIncrements151 = 3;

    ApplyNinjabrainStrongholdEvent(
        R"({
            "resultType":"TRIANGULATED",
            "eyeThrows":[
                {
                    "angle":30.0,
                    "angleWithoutCorrection":30.0,
                    "correction":0.5,
                    "type":"NORMAL"
                }
            ],
            "predictions":[]
        })",
        data);

    RequireEqual(data.eyeCount, 1, "eyeCount");
    RequireEqual(data.correctionIncrements151, 4, "correctionIncrements151");
    RequireNear(data.lastThrowError, 0.5, 1e-9, "lastThrowError fallback");
    REQUIRE(!data.throws[0].hasCorrectionIncrements);
}

TOOLSCREEN_TEST(live_ninjabrain_api_http_server_smoke) {
    NinjabrainBotProcess process;
    process.Start();

    const auto pingBody = GetEndpointBody("/api/v1/ping");
    REQUIRE(pingBody.find("active") != std::string::npos);

    const json versionBody = json::parse(GetEndpointBody("/api/v1/version"));
    RequireEqual(versionBody.at("version").get<std::string>(), std::string("1.5.2"), "version");

    const json strongholdBody = json::parse(GetEndpointBody("/api/v1/stronghold"));
    RequireEqual(strongholdBody.at("resultType").get<std::string>(), std::string("NONE"), "stronghold.resultType");
    REQUIRE(strongholdBody.at("eyeThrows").is_array());
    REQUIRE(strongholdBody.at("eyeThrows").empty());
    REQUIRE(strongholdBody.at("predictions").is_array());
    REQUIRE(strongholdBody.at("predictions").empty());

    const json boatBody = json::parse(GetEndpointBody("/api/v1/boat"));
    RequireEqual(boatBody.at("boatState").get<std::string>(), std::string("VALID"), "boat.boatState");
    RequireNear(boatBody.at("boatAngle").get<double>(), 0.0, 1e-9, "boat.boatAngle");

    const json blindBody = json::parse(GetEndpointBody("/api/v1/blind"));
    REQUIRE(!blindBody.at("isBlindModeEnabled").get<bool>());
    REQUIRE(!blindBody.at("hasDivine").get<bool>());
    REQUIRE(blindBody.at("blindResult").is_object());
    REQUIRE(blindBody.at("blindResult").empty());

    const json strongholdSseBody = json::parse(ReadFirstSseMessage("/api/v1/stronghold/events"));
    RequireEqual(strongholdSseBody.at("resultType").get<std::string>(), std::string("NONE"), "strongholdSse.resultType");
    REQUIRE(strongholdSseBody.at("eyeThrows").is_array());
    REQUIRE(strongholdSseBody.at("eyeThrows").empty());

    const json boatSseBody = json::parse(ReadFirstSseMessage("/api/v1/boat/events"));
    RequireEqual(boatSseBody.at("boatState").get<std::string>(), std::string("VALID"), "boatSse.boatState");
    RequireNear(boatSseBody.at("boatAngle").get<double>(), 0.0, 1e-9, "boatSse.boatAngle");

    const json blindSseBody = json::parse(ReadFirstSseMessage("/api/v1/blind/events"));
    REQUIRE(!blindSseBody.at("isBlindModeEnabled").get<bool>());
    REQUIRE(!blindSseBody.at("hasDivine").get<bool>());
    REQUIRE(blindSseBody.at("blindResult").is_object());
    REQUIRE(blindSseBody.at("blindResult").empty());
}

TOOLSCREEN_TEST(live_ninjabrain_api_session_reconnects) {
    NinjabrainBotProcess process;
    process.Start();

    SessionState sessionState;
    NinjabrainApiSession session(kServerBaseUrl, CreateSessionCallbacks(sessionState));

    WaitFor(
        [&]() {
            std::lock_guard<std::mutex> lock(sessionState.mutex);
            return sessionState.data.resultType == "NONE" && sessionState.data.boatState == "VALID" &&
                   sessionState.data.hasBoatAngle;
        },
        kStartupTimeout,
        100ms,
        "Timed out waiting for the initial Ninjabrain Bot snapshots. State: " + DescribeSessionState(sessionState));

    process.Stop();

    {
        std::lock_guard<std::mutex> lock(sessionState.mutex);
        sessionState.data = NinjabrainData{};
        sessionState.data.boatState = "WAITING";
        sessionState.logs.clear();
    }

    process.Start();

    WaitFor(
        [&]() {
            std::lock_guard<std::mutex> lock(sessionState.mutex);
            const bool reconnected = std::any_of(
                sessionState.logs.begin(),
                sessionState.logs.end(),
                [](const std::string& message) {
                    return message.find("Reconnected stronghold stream.") != std::string::npos ||
                           message.find("Reconnected boat stream.") != std::string::npos;
                });
            return reconnected && sessionState.data.resultType == "NONE" && sessionState.data.boatState == "VALID" &&
                   sessionState.data.hasBoatAngle;
        },
        kReconnectTimeout,
        100ms,
        "Timed out waiting for the client to reconnect to the Ninjabrain Bot API. State: " +
            DescribeSessionState(sessionState));

    session.Stop();
}

void PrintUsage(const char* executableName) {
    std::cout << "Usage: " << executableName << " [--list | --run <test-name> | --run-all]\n";
}

int RunNamedTest(const std::string& requestedName) {
    for (const auto& testCase : TestRegistry()) {
        if (requestedName != testCase.name) { continue; }

        testCase.function();
        std::cout << "PASS " << testCase.name << '\n';
        return 0;
    }

    std::cerr << "Unknown test: " << requestedName << '\n';
    return 2;
}

int RunAllTests() {
    int failureCount = 0;
    for (const auto& testCase : TestRegistry()) {
        try {
            testCase.function();
            std::cout << "PASS " << testCase.name << '\n';
        } catch (const std::exception& exception) {
            ++failureCount;
            std::cerr << "FAIL " << testCase.name << ": " << exception.what() << '\n';
        }
    }

    return failureCount == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 1 || (argc == 2 && std::string(argv[1]) == "--run-all")) {
            return RunAllTests();
        }

        if (argc == 2 && std::string(argv[1]) == "--list") {
            for (const auto& testCase : TestRegistry()) {
                std::cout << testCase.name << '\n';
            }
            return 0;
        }

        if (argc == 3 && std::string(argv[1]) == "--run") {
            return RunNamedTest(argv[2]);
        }

        PrintUsage(argv[0]);
        return 2;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}