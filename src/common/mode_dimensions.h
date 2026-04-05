#pragma once

struct Config;

void RecalculateModeDimensions();
void RecalculateModeDimensions(Config& config, int screenW, int screenH);
bool SyncPreemptiveModeFromEyeZoom(Config& config);