#pragma once


#include <string>

namespace HookChain {

bool TryCreateAndEnableHook(void* target, void* detour, void** outOriginal, const char* what);

bool IsAllowedThirdPartyHookAddress(const void* addr);

void RefreshAllThirdPartyHookChains();

std::string DescribeAddressWithOwner(const void* addr);

}


