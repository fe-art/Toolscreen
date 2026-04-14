#pragma once

#include <GL/glew.h>
#include <atomic>
#include <windows.h>

// OBS capture redirect state used by the main glBlitFramebuffer hook.

void StartObsHookThread();
void StopObsHookThread();

extern std::atomic<bool> g_obsOverrideEnabled;
extern std::atomic<GLuint> g_obsOverrideTexture;
extern std::atomic<int> g_obsOverrideWidth;
extern std::atomic<int> g_obsOverrideHeight;

extern std::atomic<bool> g_obsPre113Windowed;
extern std::atomic<int> g_obsPre113OffsetX;
extern std::atomic<int> g_obsPre113OffsetY;
extern std::atomic<int> g_obsPre113ContentW;
extern std::atomic<int> g_obsPre113ContentH;

void BlitFramebufferDirect(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1,
						   GLint dstY1, GLbitfield mask, GLenum filter);
bool TryObsBlitFramebufferRedirect(GLint readFBO,
						   GLint srcX0,
						   GLint srcY0,
						   GLint srcX1,
						   GLint srcY1,
						   GLint dstX0,
						   GLint dstY0,
						   GLint dstX1,
						   GLint dstY1,
						   GLbitfield mask,
						   GLenum filter);

void CaptureBackbufferForObs(int width, int height);

// Set the override texture published by the synchronous OBS compose path.
void SetObsOverrideTexture(GLuint texture, int width, int height);

void ClearObsOverride();

bool ShouldUpdateObsTextureNow();

int GetObsTargetFramerate();

void ResetObsTextureUpdateSchedule();

void EnableObsOverride();

bool IsObsHookDetected();

GLuint GetObsCaptureTexture();
int GetObsCaptureWidth();
int GetObsCaptureHeight();


