#include "ninjabrainClient.h"
#include "ninjabrain_data.h"

#include <windows.h>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <string>
#include <atomic>
#include <chrono>
#include <thread>

#pragma comment(lib, "libcurl.lib")

static const char* NB_STRONGHOLD_URL = "http://127.0.0.1:52533/api/v1/stronghold/events";
static const char* NB_BOAT_URL       = "http://127.0.0.1:52533/api/v1/boat/events";

static std::atomic<bool> g_running{ true };

static void parseStrongholdEvent(const std::string& json)
{
    if (json.empty()) return;
    try {
        auto j = nlohmann::json::parse(json);
        std::lock_guard<std::mutex> lock(g_ninjabrainMutex);

        std::string resultType = j.value("resultType", "NONE");
        g_ninjabrainData.resultType = resultType;

        if (resultType == "NONE" || resultType == "FAILED") {
            std::string bs  = g_ninjabrainData.boatState;
            double      ba  = g_ninjabrainData.boatAngle;
            bool        hba = g_ninjabrainData.hasBoatAngle;
            g_ninjabrainData              = NinjabrainData{};
            g_ninjabrainData.boatState    = bs;
            g_ninjabrainData.boatAngle    = ba;
            g_ninjabrainData.hasBoatAngle = hba;
            return;
        }

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

        auto& throwArr = j["eyeThrows"];
        int eyeCount   = (int)throwArr.size();
        if (eyeCount > 8) eyeCount = 8;

        int    prevEyeCount       = g_ninjabrainData.eyeCount;
        double prevLastCorrection = (prevEyeCount > 0)
            ? g_ninjabrainData.throws[prevEyeCount - 1].correction : 0.0;

        g_ninjabrainData.eyeCount       = eyeCount;
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

        if (eyeCount > 0 && !g_ninjabrainData.throws[eyeCount - 1].hasCorrectionIncrements) {
            double newCorrection = g_ninjabrainData.throws[eyeCount - 1].correction;
            if (eyeCount != prevEyeCount) {
                g_ninjabrainData.correctionIncrements151 = 0;
            } else {
                double delta = newCorrection - prevLastCorrection;
                if (delta > 1e-9)       g_ninjabrainData.correctionIncrements151++;
                else if (delta < -1e-9) g_ninjabrainData.correctionIncrements151--;
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

        auto& preds = j["predictions"];
        int count   = 0;
        g_ninjabrainData.validPrediction = false;
        for (auto& p : preds) {
            if (count >= 5) break;
            g_ninjabrainData.predictions[count].chunkX            = p.value("chunkX", 0);
            g_ninjabrainData.predictions[count].chunkZ            = p.value("chunkZ", 0);
            g_ninjabrainData.predictions[count].certainty         = p.value("certainty", 0.0);
            g_ninjabrainData.predictions[count].overworldDistance = p.value("overworldDistance", 0.0);

            if (g_ninjabrainData.hasPlayerPos) {
                static const double kPi = 3.14159265358979323846;
                double bx = g_ninjabrainData.predictions[count].chunkX * 16.0 + 4.0;
                double bz = g_ninjabrainData.predictions[count].chunkZ * 16.0 + 4.0;
                double xDiff = bx - g_ninjabrainData.playerX;
                double zDiff = bz - g_ninjabrainData.playerZ;
                double angleToStructure = -std::atan2(xDiff, zDiff) * 180.0 / kPi;
                double angleDiff = std::fmod(angleToStructure - playerHorizAngle, 360.0);
                while (angleDiff >  180.0) angleDiff -= 360.0;
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


struct SSEContext
{
    std::function<void(const std::string&)> onEvent;
    std::function<void()>                   onDisconnect;
    std::string                             lineBuf;
    std::string                             dataLine;
};

static size_t curlWriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    SSEContext* ctx   = reinterpret_cast<SSEContext*>(userdata);
    size_t      total = size * nmemb;

    for (size_t i = 0; i < total; i++) {
        char ch = ptr[i];
        if (ch == '\n') {
            // Strip trailing CR
            if (!ctx->lineBuf.empty() && ctx->lineBuf.back() == '\r')
                ctx->lineBuf.pop_back();

            if (ctx->lineBuf.rfind("data: ", 0) == 0) {
                ctx->dataLine = ctx->lineBuf.substr(6);
            } else if (ctx->lineBuf.empty() && !ctx->dataLine.empty()) {
                // Blank line = end of SSE event
                ctx->onEvent(ctx->dataLine);
                ctx->dataLine.clear();
            }
            ctx->lineBuf.clear();
        } else {
            ctx->lineBuf += ch;
        }
    }

    
    return g_running.load(std::memory_order_relaxed) ? total : 0;
}

static void runSSEStream(const char* url,
                         std::function<void(const std::string&)> onEvent,
                         std::function<void()>                   onDisconnect)
{
    SSEContext ctx;
    ctx.onEvent      = onEvent;
    ctx.onDisconnect = onDisconnect;

    while (g_running.load(std::memory_order_relaxed))
    {
        CURL* curl = curl_easy_init();
        if (!curl) {
            std::this_thread::sleep_for(std::chrono::seconds(3));
            continue;
        }

        curl_easy_setopt(curl, CURLOPT_URL,            url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  curlWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &ctx);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT,        0L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE,  1L);

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Accept: text/event-stream");
        headers = curl_slist_append(headers, "Cache-Control: no-cache");
        headers = curl_slist_append(headers, "Connection: keep-alive");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

        ctx.lineBuf.clear();
        ctx.dataLine.clear();

        curl_easy_perform(curl);  

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        onDisconnect();

        if (g_running.load(std::memory_order_relaxed))
            std::this_thread::sleep_for(std::chrono::seconds(3));
    }
}

void ninjabrainClient()
{
    curl_global_init(CURL_GLOBAL_DEFAULT);

    auto strongholdDisconnect = []() {
        std::lock_guard<std::mutex> lock(g_ninjabrainMutex);
        std::string bs  = g_ninjabrainData.boatState;
        double      ba  = g_ninjabrainData.boatAngle;
        bool        hba = g_ninjabrainData.hasBoatAngle;
        g_ninjabrainData              = NinjabrainData{};
        g_ninjabrainData.boatState    = bs;
        g_ninjabrainData.boatAngle    = ba;
        g_ninjabrainData.hasBoatAngle = hba;
    };

    auto boatDisconnect = []() {
        std::lock_guard<std::mutex> lock(g_ninjabrainMutex);
        g_ninjabrainData.boatState    = "NONE";
        g_ninjabrainData.hasBoatAngle = false;
    };

    CURLM* multi = curl_multi_init();

    SSEContext ctxStronghold;
    ctxStronghold.onEvent      = parseStrongholdEvent;
    ctxStronghold.onDisconnect = strongholdDisconnect;

    SSEContext ctxBoat;
    ctxBoat.onEvent      = parseBoatEvent;
    ctxBoat.onDisconnect = boatDisconnect;

    auto makeHandle = [&](const char* url, SSEContext& ctx) -> CURL* {
        CURL* curl = curl_easy_init();
        if (!curl) return nullptr;

        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Accept: text/event-stream");
        headers = curl_slist_append(headers, "Cache-Control: no-cache");
        headers = curl_slist_append(headers, "Connection: keep-alive");

        curl_easy_setopt(curl, CURLOPT_URL,            url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,  curlWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA,      &ctx);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT,        0L);
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE,  1L);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER,     headers);
        curl_easy_setopt(curl, CURLOPT_PRIVATE,        headers);

        curl_multi_add_handle(multi, curl);
        return curl;
    };

    struct HandleState {
        CURL*       curl       = nullptr;
        SSEContext* ctx        = nullptr;
        const char* url        = nullptr;
        std::function<void()> onDisconnect;
        DWORD       retryAt    = 0;
        bool        inMulti    = false;
    };

    HandleState states[2];
    states[0] = { nullptr, &ctxStronghold, NB_STRONGHOLD_URL, strongholdDisconnect, 0, false };
    states[1] = { nullptr, &ctxBoat,       NB_BOAT_URL,       boatDisconnect,       0, false };

    while (g_running.load(std::memory_order_relaxed))
    {
        DWORD now = GetTickCount();

        for (auto& s : states) {
            if (!s.inMulti && (DWORD)(now - s.retryAt) < 0x80000000u) {
                s.ctx->lineBuf.clear();
                s.ctx->dataLine.clear();
                s.curl = makeHandle(s.url, *s.ctx);
                if (s.curl) s.inMulti = true;
            }
        }

        int running = 0;
        curl_multi_perform(multi, &running);

        int msgsLeft = 0;
        CURLMsg* msg;
        while ((msg = curl_multi_info_read(multi, &msgsLeft)) != nullptr) {
            if (msg->msg != CURLMSG_DONE) continue;
            CURL* done = msg->easy_handle;

            for (auto& s : states) {
                if (s.curl != done) continue;

                char* priv = nullptr;
                curl_easy_getinfo(done, CURLINFO_PRIVATE, &priv);
                if (priv) curl_slist_free_all(reinterpret_cast<struct curl_slist*>(priv));

                curl_multi_remove_handle(multi, done);
                curl_easy_cleanup(done);
                s.curl    = nullptr;
                s.inMulti = false;

                s.onDisconnect();
                s.retryAt = GetTickCount() + 3000;
                break;
            }
        }

        long curlTimeout = 5;
        curl_multi_timeout(multi, &curlTimeout);
        if (curlTimeout < 0 || curlTimeout > 5) curlTimeout = 5;

        struct timeval tv{ 0, curlTimeout * 1000 };
        int maxfd = -1;
        fd_set r, w, e;
        FD_ZERO(&r); FD_ZERO(&w); FD_ZERO(&e);
        curl_multi_fdset(multi, &r, &w, &e, &maxfd);
        if (maxfd >= 0)
            select(maxfd + 1, &r, &w, &e, &tv);
        else
            Sleep(static_cast<DWORD>(curlTimeout));
    }

    // Cleanup
    for (auto& s : states) {
        if (s.curl) {
            char* priv = nullptr;
            curl_easy_getinfo(s.curl, CURLINFO_PRIVATE, &priv);
            if (priv) curl_slist_free_all(reinterpret_cast<struct curl_slist*>(priv));
            curl_multi_remove_handle(multi, s.curl);
            curl_easy_cleanup(s.curl);
        }
    }
    curl_multi_cleanup(multi);
    curl_global_cleanup();
}

void stopNinjabrainClient()
{
    g_running.store(false, std::memory_order_relaxed);
}
