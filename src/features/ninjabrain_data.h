#pragma once
#include <string>
#include <mutex>

struct NinjabrainPrediction {
    int chunkX = 0;
    int chunkZ = 0;
    double certainty = 0.0;
    double overworldDistance = 0.0;
};


struct NinjabrainThrow {
    double angle                  = 0.0; 
    double angleWithoutCorrection = 0.0;  
    double correction             = 0.0; 
    int    correctionIncrements   = 0;    // integer step count (NB 1.5.2+)
    bool   hasCorrectionIncrements = false;
    std::string type              = "";   
};
 
struct NinjabrainPredictionAngle {
    double actualAngle          = 0.0; 
    double neededCorrection     = 0.0; 
    bool   valid                = false;
};

struct NinjabrainData {
    int strongholdX = 0;
    int strongholdZ = 0;
    double distance = 0.0;
    double certainty = 0.0;

    NinjabrainPrediction predictions[5];
    NinjabrainPredictionAngle predAngles[5]; 
    int predictionCount = 0;

    NinjabrainThrow throws[8];
    int eyeCount = 0;

    double lastAngle   = 0.0; 
    double prevAngle   = 0.0;
    bool   hasAngleChange = false;

    double lastCorrection             = 0.0;
    double lastAngleWithoutCorrection = 0.0;
    bool   hasCorrection              = false; 
    
    bool   hasNetherAngle  = false;
    double netherAngle     = 0.0; 
    double netherAngleDiff = 0.0; 

    double playerX = 0.0;
    double playerZ = 0.0;
    double playerHorizontalAngle = 0.0;
    bool playerInNether = false;
    bool hasPlayerPos = false;

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

extern NinjabrainData g_ninjabrainData;
extern std::mutex g_ninjabrainMutex;
