#include "features/interactive_mirror_create.h"

#include <algorithm>
#include <cmath>

namespace {

int RoundDiv(int numerator, float scale) {
    if (scale <= 0.0001f) return numerator;
    return static_cast<int>(std::lround(static_cast<float>(numerator) / scale));
}

int ClampInt(int value, int lo, int hi) {
    return std::clamp(value, lo, hi);
}

float ClampScale(float value) {
    using namespace InteractiveMirrorLimits;
    return std::clamp(value, kMinScale, kMaxScale);
}

}  // namespace

InteractiveMirrorParams BuildInteractiveMirrorParams(const InteractiveRect& sourceRectScreen,
                                                     const InteractiveRect& destRectScreen,
                                                     bool relativeToScreen,
                                                     int finalX,
                                                     int finalY,
                                                     int finalW,
                                                     int finalH,
                                                     int gameW,
                                                     int gameH,
                                                     int fullW,
                                                     int fullH) {
    using namespace InteractiveMirrorLimits;

    const float xScale = (gameW > 0 && finalW > 0) ? static_cast<float>(finalW) / static_cast<float>(gameW) : 1.0f;
    const float yScale = (gameH > 0 && finalH > 0) ? static_cast<float>(finalH) / static_cast<float>(gameH) : 1.0f;

    InteractiveMirrorParams params;

    params.captureWidth = ClampInt(RoundDiv(sourceRectScreen.w, xScale), kMinCaptureDimension, kMaxCaptureDimension);
    params.captureHeight = ClampInt(RoundDiv(sourceRectScreen.h, yScale), kMinCaptureDimension, kMaxCaptureDimension);
    params.inputX = RoundDiv(sourceRectScreen.x - finalX, xScale);
    params.inputY = RoundDiv(sourceRectScreen.y - finalY, yScale);
    params.captureRelativeTo = "topLeftViewport";

    const float fitW = static_cast<float>(destRectScreen.w) / static_cast<float>(params.captureWidth);
    const float fitH = static_cast<float>(destRectScreen.h) / static_cast<float>(params.captureHeight);
    const float fitScale = ClampScale(std::min(fitW, fitH));
    params.separateScale = false;
    params.scale = fitScale;
    params.scaleX = fitScale;
    params.scaleY = fitScale;

    const int contentW = static_cast<int>(std::lround(static_cast<float>(params.captureWidth) * fitScale));
    const int contentH = static_cast<int>(std::lround(static_cast<float>(params.captureHeight) * fitScale));
    const int contentX = destRectScreen.x + (destRectScreen.w - contentW) / 2;
    const int contentY = destRectScreen.y + (destRectScreen.h - contentH) / 2;

    if (relativeToScreen) {
        params.outputRelativeTo = "topLeftScreen";
        params.outputX = contentX;
        params.outputY = contentY;
        params.useRelativePosition = true;
    } else {
        params.outputRelativeTo = "topLeftViewport";
        params.outputX = contentX - finalX;
        params.outputY = contentY - finalY;
        params.useRelativePosition = false;
    }
    params.relativeX = (fullW > 0) ? static_cast<float>(contentX) / static_cast<float>(fullW) : 0.0f;
    params.relativeY = (fullH > 0) ? static_cast<float>(contentY) / static_cast<float>(fullH) : 0.0f;

    return params;
}
