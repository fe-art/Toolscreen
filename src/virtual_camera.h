#pragma once

#include <cstdint>
#include <atomic>

// This works independently of OBS Studio - the driver just needs to be installed

bool StartVirtualCamera(uint32_t width, uint32_t height, int fps = 30);

void StopVirtualCamera();

bool WriteVirtualCameraFrame(const uint8_t* rgba_data, uint32_t width, uint32_t height, uint64_t timestamp);

// nv12_data must be width*height*3/2 bytes (NV12 format)
bool WriteVirtualCameraFrameNV12(const uint8_t* nv12_data, uint32_t width, uint32_t height, uint64_t timestamp);

bool IsVirtualCameraActive();

// Check if OBS Virtual Camera driver is installed
bool IsVirtualCameraDriverInstalled();

bool IsVirtualCameraInUseByOBS();

const char* GetVirtualCameraError();

extern std::atomic<bool> g_virtualCameraActive;


