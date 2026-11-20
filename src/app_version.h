#pragma once
#include "../build/version.h"

namespace appversion {
constexpr bool Same(const char* left, const char* right) {
    while (*left && *right && *left == *right) { ++left; ++right; }
    return *left == *right;
}
constexpr const char* text = WB_VERSION_STRING;
constexpr const wchar_t* wide = WB_VERSION_WSTRING;
constexpr bool official = WB_OFFICIAL_RELEASE != 0;
constexpr bool seelen = Same(WB_BUILD_CHANNEL, "seelen");
constexpr bool updates = official || seelen;
}
