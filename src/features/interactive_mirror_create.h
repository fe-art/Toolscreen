#pragma once

#include <string>

struct InteractiveRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
};

struct InteractiveMirrorParams {
    int captureWidth = 1;
    int captureHeight = 1;
    int inputX = 0;
    int inputY = 0;
    std::string captureRelativeTo = "topLeftViewport";

    std::string outputRelativeTo = "topLeftViewport";
    int outputX = 0;
    int outputY = 0;
    bool useRelativePosition = false;
    float relativeX = 0.0f;
    float relativeY = 0.0f;

    bool separateScale = false;
    float scale = 1.0f;
    float scaleX = 1.0f;
    float scaleY = 1.0f;
};

namespace InteractiveMirrorLimits {
constexpr int kMinCaptureDimension = 4;
constexpr int kMaxCaptureDimension = 2000;
constexpr float kMinScale = 0.1f;
constexpr float kMaxScale = 20.0f;
}  // namespace InteractiveMirrorLimits

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
                                                     int fullH);
