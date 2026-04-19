#pragma once

#include <windows.h>

struct CursorTrailConfig;

void RenderCursorTrail(HWND hwnd, int windowWidth, int windowHeight, const CursorTrailConfig& cfg);
