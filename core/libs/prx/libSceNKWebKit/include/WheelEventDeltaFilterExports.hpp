#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"

#include <cstddef>
#include <type_traits>

namespace NKWebKit::WheelAbi {

struct WheelEventDeltaFilter {
    void* const* vtable;
    WebCore::FloatSize currentFilteredDelta;
    WebCore::FloatSize currentFilteredVelocity;
    bool isFilteringDeltas;
};

static_assert(sizeof(WheelEventDeltaFilter) == 32);
static_assert(offsetof(WheelEventDeltaFilter, currentFilteredDelta) == 8);
static_assert(offsetof(WheelEventDeltaFilter, currentFilteredVelocity) == 16);
static_assert(offsetof(WheelEventDeltaFilter, isFilteringDeltas) == 24);
static_assert(sizeof(WebCore::FloatSize) == 8);
static_assert(std::is_trivially_copyable_v<WebCore::FloatSize>);

}  // namespace NKWebKit::WheelAbi

extern "C" {

bool APS5_VABI _ZNK7WebCore21WheelEventDeltaFilter17isFilteringDeltasEv(const NKWebKit::WheelAbi::WheelEventDeltaFilter* self);
WebCore::FloatSize APS5_VABI _ZNK7WebCore21WheelEventDeltaFilter13filteredDeltaEv(const NKWebKit::WheelAbi::WheelEventDeltaFilter* self);
WebCore::FloatSize APS5_VABI _ZNK7WebCore21WheelEventDeltaFilter16filteredVelocityEv(const NKWebKit::WheelAbi::WheelEventDeltaFilter* self);

}
