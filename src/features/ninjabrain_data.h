#pragma once

#include <array>
#include <functional>
#include <memory>
#include <string>

inline constexpr size_t kNinjabrainPredictionLimit = 5;
inline constexpr size_t kNinjabrainThrowLimit = 8;
inline constexpr size_t kNinjabrainInformationMessageLimit = 8;

struct NinjabrainPrediction {
    int chunkX = 0;
    int chunkZ = 0;
    double certainty = 0.0;
    double overworldDistance = 0.0;
};

struct NinjabrainThrow {
    double xInOverworld = 0.0;
    double zInOverworld = 0.0;
    bool hasPosition = false;
    double angle = 0.0;
    double angleWithoutCorrection = 0.0;
    double correction = 0.0;
    double error = 0.0;
    int correctionIncrements = 0;
    bool hasCorrectionIncrements = false;
    std::string type;
};

struct NinjabrainPredictionAngle {
    double actualAngle = 0.0;
    double neededCorrection = 0.0;
    bool valid = false;
};

struct NinjabrainInformationMessage {
    std::string severity;
    std::string type;
    std::string message;
};

struct NinjabrainBlindData {
    bool enabled = false;
    bool hasDivine = false;
    bool hasResult = false;
    std::string evaluation;
    double xInNether = 0.0;
    double zInNether = 0.0;
    double improveDistance = 0.0;
    double averageDistance = 0.0;
    double improveDirection = 0.0;
    double highrollProbability = 0.0;
    double highrollThreshold = 0.0;
};

struct NinjabrainData {
    int strongholdX = 0;
    int strongholdZ = 0;
    double distance = 0.0;
    double certainty = 0.0;

    std::array<NinjabrainPrediction, kNinjabrainPredictionLimit> predictions{};
    std::array<NinjabrainPredictionAngle, kNinjabrainPredictionLimit> predictionAngles{};
    int predictionCount = 0;

    std::array<NinjabrainThrow, kNinjabrainThrowLimit> throws{};
    int eyeCount = 0;

    double lastAngle = 0.0;
    double prevAngle = 0.0;
    bool hasAngleChange = false;

    double lastCorrection             = 0.0;
    double lastThrowError             = 0.0;
    double lastAngleWithoutCorrection = 0.0;
    bool hasCorrection              = false;
    bool hasThrowError              = false;

    bool hasNetherAngle  = false;
    double netherAngle     = 0.0;
    double netherAngleDiff = 0.0;

    double playerX = 0.0;
    double playerZ = 0.0;
    double playerHorizontalAngle = 0.0;
    bool playerInNether = false;
    bool hasPlayerPos = false;

    std::array<NinjabrainInformationMessage, kNinjabrainInformationMessageLimit> informationMessages{};
    int informationMessageCount = 0;

    NinjabrainBlindData blind;

    // For NB 1.5.1 increment recovery: integer counter incremented/decremented by 1
    // per SSE event (one event = one hotkey press = one click). Never uses division.
    // Reset to 0 when eyeCount changes. Unused when hasCorrectionIncrements is true.
    int correctionIncrements151 = 0;

    std::string resultType = "NONE";

    bool validPrediction = false;

    std::string boatState = "NONE";
    double boatAngle = 0.0;
    bool hasBoatAngle = false;
};

inline double GetNinjabrainPredictionDisplayDistance(
    const NinjabrainData& data,
    const NinjabrainPrediction& prediction) {
    return data.playerInNether ? (prediction.overworldDistance / 8.0) : prediction.overworldDistance;
}

std::shared_ptr<const NinjabrainData> GetNinjabrainDataSnapshot();
void PublishNinjabrainData(NinjabrainData data);
void ModifyNinjabrainData(const std::function<void(NinjabrainData&)>& modifier);
