#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfTimeTypes.hpp"
#include <cstdint>
#include <type_traits>

static_assert(sizeof(WTF::WallTime) == 8);
static_assert(alignof(WTF::WallTime) == 8);
static_assert(std::is_trivially_copyable_v<WTF::WallTime>);

extern "C" {

WTF::WallTime APS5_VABI _ZN7WebCore22ResourceLoadStatistics20reduceTimeResolutionEN3WTF8WallTimeE(WTF::WallTime time);
bool APS5_VABI _ZN7WebCore12SettingsBase33platformDefaultMediaSourceEnabledEv();
std::int32_t APS5_VABI _ZN7WebCore16scriptNameToCodeERKN3WTF6StringE(const NKWebKit::WtfAbi::String& scriptName);

}
