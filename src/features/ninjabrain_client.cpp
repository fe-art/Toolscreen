#include "ninjabrain_client.h"

#include "features/ninjabrain_api.h"

#include "common/utils.h"
#include "config/config_defaults.h"
#include "gui/gui.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <thread>

namespace {

std::mutex g_ninjabrainClientMutex;
NinjabrainApiConnectionTracker g_ninjabrainClientStatus;
std::thread g_ninjabrainClientStopThread;
bool g_ninjabrainClientStopInProgress = false;
bool g_ninjabrainClientRestartPending = false;

bool IsNinjabrainOverlayEnabled() {
    if (const auto configSnapshot = GetConfigSnapshot()) {
        return configSnapshot->ninjabrainOverlay.enabled;
    }

    return g_config.ninjabrainOverlay.enabled;
}

void LogNinjabrainMessage(const std::string& message) {
    LogCategory("ninjabrain", message);
}

std::string ResolveApiBaseUrl() {
    std::string apiBaseUrl = ConfigDefaults::CONFIG_NINJABRAIN_API_BASE_URL;

    if (const auto configSnapshot = GetConfigSnapshot()) {
        apiBaseUrl = configSnapshot->ninjabrainOverlay.apiBaseUrl;
    } else {
        apiBaseUrl = g_config.ninjabrainOverlay.apiBaseUrl;
    }

    return NormalizeNinjabrainApiBaseUrl(std::move(apiBaseUrl));
}

std::unique_ptr<NinjabrainApiSession> g_ninjabrainClientSession;

void ReapCompletedNinjabrainClientStopThread(std::thread& threadToJoin) {
    std::lock_guard<std::mutex> lock(g_ninjabrainClientMutex);
    if (!g_ninjabrainClientStopInProgress && g_ninjabrainClientStopThread.joinable() &&
        g_ninjabrainClientStopThread.get_id() != std::this_thread::get_id()) {
        threadToJoin = std::move(g_ninjabrainClientStopThread);
    }
}

} // namespace

void StartNinjabrainClient() {
    std::thread threadToJoin;
    ReapCompletedNinjabrainClientStopThread(threadToJoin);
    if (threadToJoin.joinable()) { threadToJoin.join(); }

    std::lock_guard<std::mutex> lock(g_ninjabrainClientMutex);
    if (!IsNinjabrainOverlayEnabled()) {
        g_ninjabrainClientRestartPending = false;
        return;
    }
    if (g_ninjabrainClientSession) {
        g_ninjabrainClientRestartPending = false;
        return;
    }
    if (g_ninjabrainClientStopInProgress) {
        g_ninjabrainClientRestartPending = true;
        return;
    }

    const std::string apiBaseUrl = ResolveApiBaseUrl();

    PublishNinjabrainData(NinjabrainData{});

    g_ninjabrainClientStatus.Start(apiBaseUrl);

    NinjabrainApiSessionCallbacks callbacks;
    callbacks.onStrongholdMessage = [](const std::string& payload) {
        ModifyNinjabrainData([&](NinjabrainData& data) {
            ApplyNinjabrainStrongholdEvent(payload, data, LogNinjabrainMessage);
        });
    };
    callbacks.onStrongholdConnect = []() {
        std::lock_guard<std::mutex> lock(g_ninjabrainClientMutex);
        g_ninjabrainClientStatus.MarkStrongholdConnected();
    };
    callbacks.onStrongholdDisconnect = [](const std::string& error) {
        {
            std::lock_guard<std::mutex> lock(g_ninjabrainClientMutex);
            g_ninjabrainClientStatus.MarkStrongholdDisconnected(error);
        }
        ModifyNinjabrainData([](NinjabrainData& data) {
            ClearNinjabrainStrongholdData(data);
        });
    };
    callbacks.onBoatMessage = [](const std::string& payload) {
        ModifyNinjabrainData([&](NinjabrainData& data) {
            ApplyNinjabrainBoatEvent(payload, data, LogNinjabrainMessage);
        });
    };
    callbacks.onBoatConnect = []() {
        std::lock_guard<std::mutex> lock(g_ninjabrainClientMutex);
        g_ninjabrainClientStatus.MarkBoatConnected();
    };
    callbacks.onBoatDisconnect = [](const std::string& error) {
        {
            std::lock_guard<std::mutex> lock(g_ninjabrainClientMutex);
            g_ninjabrainClientStatus.MarkBoatDisconnected(error);
        }
        ModifyNinjabrainData([](NinjabrainData& data) {
            ClearNinjabrainBoatData(data);
        });
    };
    callbacks.onLog = LogNinjabrainMessage;

    try {
        g_ninjabrainClientSession = std::make_unique<NinjabrainApiSession>(apiBaseUrl, std::move(callbacks));
    } catch (...) {
        g_ninjabrainClientStatus.Stop();
        throw;
    }
}

void StopNinjabrainClient() {
    std::thread threadToJoin;
    std::unique_ptr<NinjabrainApiSession> session;
    {
        std::lock_guard<std::mutex> lock(g_ninjabrainClientMutex);
        g_ninjabrainClientRestartPending = false;
        session = std::move(g_ninjabrainClientSession);
        if (g_ninjabrainClientStopThread.joinable() && g_ninjabrainClientStopThread.get_id() != std::this_thread::get_id()) {
            threadToJoin = std::move(g_ninjabrainClientStopThread);
        }
        g_ninjabrainClientStatus.Stop();
    }

    if (session) { session->Stop(); }
    if (threadToJoin.joinable()) { threadToJoin.join(); }

    std::lock_guard<std::mutex> lock(g_ninjabrainClientMutex);
    g_ninjabrainClientStopInProgress = false;
    g_ninjabrainClientRestartPending = false;
}

void StopNinjabrainClientAsync() {
    std::thread threadToJoin;
    ReapCompletedNinjabrainClientStopThread(threadToJoin);
    if (threadToJoin.joinable()) { threadToJoin.join(); }

    std::unique_ptr<NinjabrainApiSession> session;
    {
        std::lock_guard<std::mutex> lock(g_ninjabrainClientMutex);
        g_ninjabrainClientRestartPending = false;
        if (!g_ninjabrainClientSession) {
            g_ninjabrainClientStatus.Stop();
            return;
        }

        session = std::move(g_ninjabrainClientSession);
        g_ninjabrainClientStatus.Stop();
        g_ninjabrainClientStopInProgress = true;
        g_ninjabrainClientStopThread = std::thread([session = std::move(session)]() mutable {
            const auto stopStart = std::chrono::steady_clock::now();
            session->Stop();
            session.reset();

            bool shouldRestart = false;
            {
                std::lock_guard<std::mutex> lock(g_ninjabrainClientMutex);
                g_ninjabrainClientStopInProgress = false;
                shouldRestart = g_ninjabrainClientRestartPending;
                g_ninjabrainClientRestartPending = false;
            }

            const auto stopDurationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - stopStart).count();
            LogNinjabrainMessage("Client shutdown finished in " + std::to_string(stopDurationMs) + " ms.");

            if (shouldRestart && IsNinjabrainOverlayEnabled()) {
                try {
                    StartNinjabrainClient();
                } catch (const std::exception& ex) {
                    LogNinjabrainMessage(std::string("Failed to restart client after shutdown: ") + ex.what());
                } catch (...) {
                    LogNinjabrainMessage("Failed to restart client after shutdown.");
                }
            }
        });
    }
}

void RestartNinjabrainClient() {
    StopNinjabrainClient();
    StartNinjabrainClient();
}

NinjabrainApiStatus GetNinjabrainClientStatus() {
    std::lock_guard<std::mutex> lock(g_ninjabrainClientMutex);
    return g_ninjabrainClientStatus.Snapshot();
}