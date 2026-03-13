#include "ninjabrainClient.h"
#include "ninjabrain_data.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <nlohmann/json.hpp>
#include <string>
#include <thread>
#include <chrono>

#pragma comment(lib, "ws2_32.lib")

static const char* NB_HOST = "127.0.0.1";
static const int   NB_PORT = 52533;

// Read one SSE stream until disconnected, calling callback for each event data
static void readSSEStream(SOCKET sock, const char* path,
                          std::function<void(const std::string&)> onEvent)
{
    std::string request =
        std::string("GET ") + path + " HTTP/1.1\r\n"
        "Host: 127.0.0.1\r\n"
        "Accept: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "\r\n";
    send(sock, request.c_str(), (int)request.size(), 0);

    std::string lineBuf;
    char ch;
    bool headersDone = false;
    std::string dataLine;

    while (true) {
        int r = recv(sock, &ch, 1, 0);
        if (r <= 0) break;
        if (ch == '\n') {
            if (!lineBuf.empty() && lineBuf.back() == '\r') lineBuf.pop_back();
            if (!headersDone) {
                if (lineBuf.empty()) headersDone = true;
                lineBuf.clear();
                continue;
            }
            if (lineBuf.rfind("data: ", 0) == 0)
                dataLine = lineBuf.substr(6);
            else if (lineBuf.empty() && !dataLine.empty()) {
                onEvent(dataLine);
                dataLine.clear();
            }
            lineBuf.clear();
        } else {
            lineBuf += ch;
        }
    }
}

static SOCKET connectToNB()
{
    SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) return INVALID_SOCKET;
    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port   = htons(NB_PORT);
    inet_pton(AF_INET, NB_HOST, &server.sin_addr);
    if (connect(sock, (sockaddr*)&server, sizeof(server)) < 0) {
        closesocket(sock);
        return INVALID_SOCKET;
    }
    return sock;
}

static void parseStrongholdEvent(const std::string& json)
{
    if (json.empty()) return;
    try {
        auto j = nlohmann::json::parse(json);
        std::lock_guard<std::mutex> lock(g_ninjabrainMutex);

        std::string resultType = j.value("resultType", "NONE");
        g_ninjabrainData.resultType = resultType;

        if (resultType == "NONE" || resultType == "FAILED") {
            // Reset prediction data but keep boat
            std::string bs = g_ninjabrainData.boatState;
            double ba = g_ninjabrainData.boatAngle;
            bool hba = g_ninjabrainData.hasBoatAngle;
            g_ninjabrainData = NinjabrainData{};
            g_ninjabrainData.boatState  = bs;
            g_ninjabrainData.boatAngle  = ba;
            g_ninjabrainData.hasBoatAngle = hba;
            return;
        }

        // Player position (for nether coords and per-prediction angle)
        double playerHorizAngle = 0.0;
        if (j.contains("playerPosition") && j["playerPosition"].is_object() && !j["playerPosition"].empty()) {
            auto& pp = j["playerPosition"];
            g_ninjabrainData.playerX               = pp.value("xInOverworld", 0.0);
            g_ninjabrainData.playerZ               = pp.value("zInOverworld", 0.0);
            g_ninjabrainData.playerInNether        = pp.value("isInNether", false);
            playerHorizAngle                       = pp.value("horizontalAngle", 0.0);
            g_ninjabrainData.playerHorizontalAngle = playerHorizAngle;
            g_ninjabrainData.hasPlayerPos          = true;
        } else {
            g_ninjabrainData.hasPlayerPos = false;
        }

        // Eye throws — parse full per-throw data including correctionIncrements (NB 1.5.2+)
        auto& throwArr = j["eyeThrows"];
        int eyeCount = (int)throwArr.size();
        if (eyeCount > 8) eyeCount = 8;

        int    prevEyeCount      = g_ninjabrainData.eyeCount;
        double prevLastCorrection = (prevEyeCount > 0)
            ? g_ninjabrainData.throws[prevEyeCount - 1].correction : 0.0;

        g_ninjabrainData.eyeCount = eyeCount;
        g_ninjabrainData.hasAngleChange = false;
        g_ninjabrainData.hasCorrection  = false;
        g_ninjabrainData.hasNetherAngle = false;

        for (int ti = 0; ti < eyeCount; ti++) {
            auto& t = throwArr[ti];
            g_ninjabrainData.throws[ti].angle                  = t.value("angle", 0.0);
            g_ninjabrainData.throws[ti].angleWithoutCorrection = t.value("angleWithoutCorrection", t.value("angle", 0.0));
            g_ninjabrainData.throws[ti].correction             = t.value("correction", 0.0);
            g_ninjabrainData.throws[ti].type                   = t.value("type", "NORMAL");
            if (t.contains("correctionIncrements") && !t["correctionIncrements"].is_null()) {
                g_ninjabrainData.throws[ti].correctionIncrements    = t.value("correctionIncrements", 0);
                g_ninjabrainData.throws[ti].hasCorrectionIncrements = true;
            } else {
                g_ninjabrainData.throws[ti].correctionIncrements    = 0;
                g_ninjabrainData.throws[ti].hasCorrectionIncrements = false;
            }
        }

        // For NB 1.5.1 (no correctionIncrements in API):
        // Each hotkey press fires exactly one SSE event and changes correction by exactly
        // one step. So we count ±1 per event instead of dividing — no floating point error,
        // works correctly no matter how fast the hotkey is spammed.
        if (eyeCount > 0 && !g_ninjabrainData.throws[eyeCount - 1].hasCorrectionIncrements) {
            double newCorrection = g_ninjabrainData.throws[eyeCount - 1].correction;
            if (eyeCount != prevEyeCount) {
                // New throw added — reset counter (correction starts at whatever NB set it to)
                g_ninjabrainData.correctionIncrements151 = 0;
            } else {
                // Same throw, correction changed: exactly one click happened.
                double delta = newCorrection - prevLastCorrection;
                if (delta > 1e-9)
                    g_ninjabrainData.correctionIncrements151++;
                else if (delta < -1e-9)
                    g_ninjabrainData.correctionIncrements151--;
                // delta == 0: player position update only, no click — do nothing
            }
        }

        if (eyeCount >= 1) {
            g_ninjabrainData.lastAngle                  = g_ninjabrainData.throws[eyeCount - 1].angle;
            g_ninjabrainData.lastAngleWithoutCorrection = g_ninjabrainData.throws[eyeCount - 1].angleWithoutCorrection;
            g_ninjabrainData.lastCorrection             = g_ninjabrainData.throws[eyeCount - 1].correction;
            g_ninjabrainData.hasCorrection              = std::abs(g_ninjabrainData.lastCorrection) > 1e-9;

            if (eyeCount >= 2) {
                g_ninjabrainData.prevAngle       = g_ninjabrainData.throws[eyeCount - 2].angle;
                g_ninjabrainData.hasAngleChange  = true;
                g_ninjabrainData.hasNetherAngle  = true;
                g_ninjabrainData.netherAngle     = g_ninjabrainData.lastAngle;
                g_ninjabrainData.netherAngleDiff = g_ninjabrainData.lastAngle - g_ninjabrainData.throws[0].angle;
            }
        }

        // Predictions + per-prediction angle adjustment
        auto& preds = j["predictions"];
        int count = 0;
        g_ninjabrainData.validPrediction = false;
        for (auto& p : preds) {
            if (count >= 5) break;
            g_ninjabrainData.predictions[count].chunkX            = p.value("chunkX", 0);
            g_ninjabrainData.predictions[count].chunkZ            = p.value("chunkZ", 0);
            g_ninjabrainData.predictions[count].certainty         = p.value("certainty", 0.0);
            g_ninjabrainData.predictions[count].overworldDistance = p.value("overworldDistance", 0.0);

            // Compute the angle the player needs to turn to face this prediction
            if (g_ninjabrainData.hasPlayerPos) {
                static const double kPi = 3.14159265358979323846;
                double bx = g_ninjabrainData.predictions[count].chunkX * 16.0 + 4.0;
                double bz = g_ninjabrainData.predictions[count].chunkZ * 16.0 + 4.0;
                double xDiff = bx - g_ninjabrainData.playerX;
                double zDiff = bz - g_ninjabrainData.playerZ;
                double angleToStructure = -std::atan2(xDiff, zDiff) * 180.0 / kPi;
                double angleDiff = std::fmod(angleToStructure - playerHorizAngle, 360.0);
                while (angleDiff > 180.0)  angleDiff -= 360.0;
                while (angleDiff < -180.0) angleDiff += 360.0;
                g_ninjabrainData.predAngles[count].actualAngle      = angleToStructure;
                g_ninjabrainData.predAngles[count].neededCorrection = angleDiff;
                g_ninjabrainData.predAngles[count].valid            = true;
            } else {
                g_ninjabrainData.predAngles[count].valid = false;
            }
            count++;
        }
        g_ninjabrainData.predictionCount = count;
        if (count > 0) {
            g_ninjabrainData.strongholdX     = g_ninjabrainData.predictions[0].chunkX * 16 + 4;
            g_ninjabrainData.strongholdZ     = g_ninjabrainData.predictions[0].chunkZ * 16 + 4;
            g_ninjabrainData.distance        = g_ninjabrainData.predictions[0].overworldDistance;
            g_ninjabrainData.certainty       = g_ninjabrainData.predictions[0].certainty;
            g_ninjabrainData.validPrediction = true;
        }

    } catch (...) {}
}

static void parseBoatEvent(const std::string& json)
{
    if (json.empty()) return;
    try {
        auto j = nlohmann::json::parse(json);
        std::lock_guard<std::mutex> lock(g_ninjabrainMutex);
        g_ninjabrainData.boatState = j.value("boatState", "NONE");
        if (j.contains("boatAngle") && !j["boatAngle"].is_null()) {
            g_ninjabrainData.boatAngle    = j["boatAngle"].get<double>();
            g_ninjabrainData.hasBoatAngle = true;
        } else {
            g_ninjabrainData.hasBoatAngle = false;
        }
    } catch (...) {}
}

// Stronghold SSE thread
static void strongholdThread()
{
    while (true) {
        SOCKET sock = connectToNB();
        if (sock == INVALID_SOCKET) {
            std::this_thread::sleep_for(std::chrono::seconds(3));
            continue;
        }
        readSSEStream(sock, "/api/v1/stronghold/events", parseStrongholdEvent);
        closesocket(sock);
        {
            std::lock_guard<std::mutex> lock(g_ninjabrainMutex);
            std::string bs = g_ninjabrainData.boatState;
            double ba = g_ninjabrainData.boatAngle;
            bool hba = g_ninjabrainData.hasBoatAngle;
            g_ninjabrainData = NinjabrainData{};
            g_ninjabrainData.boatState = bs;
            g_ninjabrainData.boatAngle = ba;
            g_ninjabrainData.hasBoatAngle = hba;
        }
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}

// Boat SSE thread
static void boatThread()
{
    while (true) {
        SOCKET sock = connectToNB();
        if (sock == INVALID_SOCKET) {
            std::this_thread::sleep_for(std::chrono::seconds(3));
            continue;
        }
        readSSEStream(sock, "/api/v1/boat/events", parseBoatEvent);
        closesocket(sock);
        {
            std::lock_guard<std::mutex> lock(g_ninjabrainMutex);
            g_ninjabrainData.boatState    = "NONE";
            g_ninjabrainData.hasBoatAngle = false;
        }
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}

void ninjabrainClient()
{
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    // Launch boat thread as detached
    std::thread(boatThread).detach();

    // Run stronghold thread on this thread
    strongholdThread();

    WSACleanup();
}
