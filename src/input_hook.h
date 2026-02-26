#pragma once

#include <Windows.h>
#include <string>

extern WNDPROC g_originalWndProc;

struct InputHandlerResult {
    bool consumed;
    LRESULT result;
};


InputHandlerResult HandleMouseMoveViewportOffset(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM& lParam);

InputHandlerResult HandleShutdownCheck(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

InputHandlerResult HandleWindowValidation(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

InputHandlerResult HandleNonFullscreenCheck(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

void HandleCharLogging(UINT uMsg, WPARAM wParam, LPARAM lParam);

InputHandlerResult HandleWindowPosChanged(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

InputHandlerResult HandleAltF4(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

InputHandlerResult HandleConfigLoadFailure(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

InputHandlerResult HandleSetCursor(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, const std::string& gameState);

InputHandlerResult HandleDestroy(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

InputHandlerResult HandleImGuiInput(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

InputHandlerResult HandleGuiToggle(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

InputHandlerResult HandleBorderlessToggle(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

InputHandlerResult HandleImageOverlaysToggle(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
InputHandlerResult HandleWindowOverlaysToggle(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

InputHandlerResult HandleWindowOverlayKeyboard(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

InputHandlerResult HandleWindowOverlayMouse(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Block all input when GUI is open
InputHandlerResult HandleGuiInputBlocking(UINT uMsg);

InputHandlerResult HandleActivate(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, const std::string& currentModeId);

InputHandlerResult HandleHotkeys(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, const std::string& currentModeId,
                                 const std::string& gameState);

InputHandlerResult HandleMouseCoordinateTranslationPhase(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM& lParam);

InputHandlerResult HandleKeyRebinding(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

InputHandlerResult HandleCharRebinding(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK SubclassedWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);


