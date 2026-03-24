#pragma once


#include <string>

namespace HookChain {

bool TryCreateAndEnableHook(void* target, void* detour, void** outOriginal, const char* what);

bool IsAllowedSwapBuffersThirdPartyHookAddress(const void* addr);

void RefreshAllThirdPartyHookChains();

std::string DescribeAddressWithOwner(const void* addr);

}


