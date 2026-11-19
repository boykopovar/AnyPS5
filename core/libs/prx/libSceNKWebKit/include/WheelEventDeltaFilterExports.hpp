#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"

#include <cstddef>

namespace NKWebKit::WebCoreAbi {

struct WheelEventDeltaFilter {
    const void* vtable;
    WebCore::FloatSize currentFilteredDelta;
    WebCore::FloatSize currentFilteredVelocity;
    bool isFilteringDeltas;
};

static_assert(offsetof(WheelEventDeltaFilter, currentFilteredDelta) == 8);
static_assert(offsetof(WheelEventDeltaFilter, currentFilteredVelocity) == 16);
static_assert(offsetof(WheelEventDeltaFilter, isFilteringDeltas) == 24);
static_assert(sizeof(WheelEventDeltaFilter) == 32);

}  // namespace NKWebKit::WebCoreAbi

extern "C" {

bool APS5_VABI _ZNK7WebCore21WheelEventDeltaFilter17isFilteringDeltasEv(const NKWebKit::WebCoreAbi::WheelEventDeltaFilter* self);
WebCore::FloatSize APS5_VABI _ZNK7WebCore21WheelEventDeltaFilter13filteredDeltaEv(const NKWebKit::WebCoreAbi::WheelEventDeltaFilter* self);
WebCore::FloatSize APS5_VABI _ZNK7WebCore21WheelEventDeltaFilter16filteredVelocityEv(const NKWebKit::WebCoreAbi::WheelEventDeltaFilter* self);

}
