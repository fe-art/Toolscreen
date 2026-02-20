#pragma once

// Hook chaining / hook compatibility subsystem.
//
// Toolscreen can detect third-party detours installed after our hooks and optionally chain behind them.
// This file exposes the entrypoints used by dllmain.cpp to refresh the chain and to create hooks.

#include <string>

namespace HookChain {

// Create + enable a MinHook detour (idempotent if already created/enabled).
// Returns true if the hook is created/enabled (or already exists/enabled).
bool TryCreateAndEnableHook(void* target, void* detour, void** outOriginal, const char* what);

// Refresh all supported third-party hook chains (prolog detours + IAT hooks).
// Safe to call periodically.
void RefreshAllThirdPartyHookChains();

// Pretty-print an address with owning module + version metadata (best effort).
std::string DescribeAddressWithOwner(const void* addr);

} // namespace HookChain
