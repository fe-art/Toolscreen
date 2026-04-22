#pragma once

#include <cstdint>
#include <windows.h>

struct CursorTrailConfig;

void RenderCursorTrail(HWND hwnd, int windowWidth, int windowHeight, const CursorTrailConfig& cfg, uint64_t frameTag = 0);
