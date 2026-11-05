#pragma once

#include <string>

namespace toolscreen::auto_updater {

void Start(bool includeBetas);
bool IsActive();
bool IsUpdateReady(std::string& outVersion, bool& outIsBeta);
bool ApplyAndRelaunch();

} // namespace toolscreen::auto_updater
