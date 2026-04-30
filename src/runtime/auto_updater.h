#pragma once

#include <string>

namespace toolscreen::auto_updater {

void Start();
bool IsUpdateReady(std::string& outVersion);
bool ApplyAndRelaunch();

} // namespace toolscreen::auto_updater
