#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCore::WheelEventDeltaFilter check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

template<typename TFunction>
static bool Throws(TFunction function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

int main() {
    static void* const vtable[1]{};
    NKWebKit::WheelAbi::WheelEventDeltaFilter filter{vtable, {1.5f, -2.25f}, {-300.0f, 1e-3f}, true};
    Require(_ZNK7WebCore21WheelEventDeltaFilter17isFilteringDeltasEv(&filter));
    WebCore::FloatSize delta = _ZNK7WebCore21WheelEventDeltaFilter13filteredDeltaEv(&filter);
    Require(delta.m_width == 1.5f && delta.m_height == -2.25f);
    WebCore::FloatSize velocity = _ZNK7WebCore21WheelEventDeltaFilter16filteredVelocityEv(&filter);
    Require(velocity.m_width == -300.0f && velocity.m_height == 1e-3f);

    filter.isFilteringDeltas = false;
    filter.currentFilteredDelta = {std::numeric_limits<float>::infinity(), -0.0f};
    filter.currentFilteredVelocity = {std::numeric_limits<float>::quiet_NaN(), 0.0f};
    Require(!_ZNK7WebCore21WheelEventDeltaFilter17isFilteringDeltasEv(&filter));
    delta = _ZNK7WebCore21WheelEventDeltaFilter13filteredDeltaEv(&filter);
    Require(std::isinf(delta.m_width) && delta.m_width > 0 && delta.m_height == 0.0f && std::signbit(delta.m_height));
    velocity = _ZNK7WebCore21WheelEventDeltaFilter16filteredVelocityEv(&filter);
    Require(std::isnan(velocity.m_width) && velocity.m_height == 0.0f && !std::signbit(velocity.m_height));

    Require(Throws([] { _ZNK7WebCore21WheelEventDeltaFilter17isFilteringDeltasEv(nullptr); }));
    Require(Throws([] { _ZNK7WebCore21WheelEventDeltaFilter13filteredDeltaEv(nullptr); }));
    Require(Throws([] { _ZNK7WebCore21WheelEventDeltaFilter16filteredVelocityEv(nullptr); }));

    std::puts("libSceNKWebKit WebCore::WheelEventDeltaFilter passed");
    return 0;
}
