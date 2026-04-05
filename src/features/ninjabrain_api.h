#pragma once

#include "features/ninjabrain_data.h"

#include <functional>
#include <stop_token>
#include <string>
#include <thread>

using NinjabrainLogCallback = std::function<void(const std::string&)>;

enum class NinjabrainApiConnectionState {
        Stopped,
        Connecting,
        Connected,
        Offline,
};

struct NinjabrainApiStatus {
        NinjabrainApiConnectionState connectionState = NinjabrainApiConnectionState::Stopped;
        std::string apiBaseUrl;
        std::string error;
};

class NinjabrainApiConnectionTracker {
    public:
        void Start(std::string apiBaseUrl);
        void Stop();

        void MarkStrongholdConnected();
        void MarkStrongholdDisconnected(std::string error);
        void MarkInformationMessagesConnected();
        void MarkInformationMessagesDisconnected(std::string error);
        void MarkBoatConnected();
        void MarkBoatDisconnected(std::string error);
        void MarkBlindConnected();
        void MarkBlindDisconnected(std::string error);

        NinjabrainApiStatus Snapshot() const;

    private:
        enum class StreamState {
                Connecting,
                Connected,
                Disconnected,
        };

        void MarkStreamConnected(StreamState& streamState);
        void MarkStreamDisconnected(StreamState& streamState, std::string error);

        bool sessionRunning_ = false;
        std::string apiBaseUrl_;
        std::string lastError_;
        StreamState strongholdState_ = StreamState::Disconnected;
        StreamState informationMessagesState_ = StreamState::Disconnected;
        StreamState boatState_ = StreamState::Disconnected;
        StreamState blindState_ = StreamState::Disconnected;
};

void ClearNinjabrainStrongholdData(NinjabrainData& data);
void ClearNinjabrainInformationMessagesData(NinjabrainData& data);
void ClearNinjabrainBoatData(NinjabrainData& data);
    void ClearNinjabrainBlindData(NinjabrainData& data);

void ApplyNinjabrainBoatEvent(
    const std::string& payload,
    NinjabrainData& data,
    const NinjabrainLogCallback& logError = {});

void ApplyNinjabrainStrongholdEvent(
    const std::string& payload,
    NinjabrainData& data,
    const NinjabrainLogCallback& logError = {});

void ApplyNinjabrainInformationMessagesEvent(
    const std::string& payload,
    NinjabrainData& data,
    const NinjabrainLogCallback& logError = {});

void ApplyNinjabrainBlindEvent(
    const std::string& payload,
    NinjabrainData& data,
    const NinjabrainLogCallback& logError = {});

std::string NormalizeNinjabrainApiBaseUrl(std::string apiBaseUrl);

struct NinjabrainApiSessionCallbacks {
    std::function<void(const std::string&)> onStrongholdMessage;
    std::function<void()> onStrongholdConnect;
    std::function<void(const std::string&)> onStrongholdDisconnect;
    std::function<void(const std::string&)> onInformationMessagesMessage;
    std::function<void()> onInformationMessagesConnect;
    std::function<void(const std::string&)> onInformationMessagesDisconnect;
    std::function<void(const std::string&)> onBoatMessage;
    std::function<void()> onBoatConnect;
    std::function<void(const std::string&)> onBoatDisconnect;
    std::function<void(const std::string&)> onBlindMessage;
    std::function<void()> onBlindConnect;
    std::function<void(const std::string&)> onBlindDisconnect;
    NinjabrainLogCallback onLog;
};

class NinjabrainApiSession {
  public:
    NinjabrainApiSession(std::string apiBaseUrl, NinjabrainApiSessionCallbacks callbacks);
    ~NinjabrainApiSession();

    NinjabrainApiSession(const NinjabrainApiSession&) = delete;
    NinjabrainApiSession& operator=(const NinjabrainApiSession&) = delete;

    void Stop();

  private:
    void RunStream(
        std::stop_token stopToken,
        const char* streamName,
        const char* path,
        const std::function<void(const std::string&)>& onMessage,
        const std::function<void()>& onConnect,
        const std::function<void(const std::string&)>& onDisconnect) const;

    std::string apiBaseUrl_;
    NinjabrainApiSessionCallbacks callbacks_;
    std::jthread strongholdThread_;
    std::jthread informationMessagesThread_;
    std::jthread boatThread_;
    std::jthread blindThread_;
};