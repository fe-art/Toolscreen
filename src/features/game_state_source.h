#pragma once

#include <cstdint>
#include <optional>
#include <string>

enum class GameStateSourceKind : int {
    None = 0,
    Hermes = 1,
    StateOutput = 2,
};

namespace game_state_source {

constexpr long long kHermesAliveStaleThresholdMs = 3000;

std::optional<std::string> DeriveStateFromHermesJson(const std::string& text);

std::optional<std::string> DeriveStateFromStateOutputText(const std::string& text);

GameStateSourceKind Select(bool hermesAlive, bool stateOutputAvailable);

bool EvaluateHermesAlive(const unsigned char bytes[16], uint64_t currentPid, long long nowEpochMs);

}  // namespace game_state_source
