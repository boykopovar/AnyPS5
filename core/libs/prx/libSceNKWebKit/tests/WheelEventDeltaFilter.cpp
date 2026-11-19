#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WheelEventDeltaFilterExports.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

using NKWebKit::WebCoreAbi::WheelEventDeltaFilter;

void Check(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WheelEventDeltaFilter check failed at line %u\n", location.line());
        std::abort();
    }
}

bool Equal(WebCore::FloatSize a, float width, float height) {
    return a.m_width == width && a.m_height == height;
}

void TestDefaultFilterIsIdle() {
    int vtable = 0;
    WheelEventDeltaFilter filter{&vtable, {}, {}, false};
    Check(!_ZNK7WebCore21WheelEventDeltaFilter17isFilteringDeltasEv(&filter));
    Check(Equal(_ZNK7WebCore21WheelEventDeltaFilter13filteredDeltaEv(&filter), 0.0f, 0.0f));
    Check(Equal(_ZNK7WebCore21WheelEventDeltaFilter16filteredVelocityEv(&filter), 0.0f, 0.0f));
}

void TestFilteringFlagIsRead() {
    int vtable = 0;
    WheelEventDeltaFilter filter{&vtable, {1.0f, 2.0f}, {3.0f, 4.0f}, true};
    Check(_ZNK7WebCore21WheelEventDeltaFilter17isFilteringDeltasEv(&filter));
    filter.isFilteringDeltas = false;
    Check(!_ZNK7WebCore21WheelEventDeltaFilter17isFilteringDeltasEv(&filter));
}

void TestDeltaAndVelocityAreDistinct() {
    int vtable = 0;
    WheelEventDeltaFilter filter{&vtable, {-12.5f, 0.0f}, {0.25f, -640.0f}, true};
    Check(Equal(_ZNK7WebCore21WheelEventDeltaFilter13filteredDeltaEv(&filter), -12.5f, 0.0f));
    Check(Equal(_ZNK7WebCore21WheelEventDeltaFilter16filteredVelocityEv(&filter), 0.25f, -640.0f));
}

}  // namespace

int main() {
    TestDefaultFilterIsIdle();
    TestFilteringFlagIsRead();
    TestDeltaAndVelocityAreDistinct();
    std::puts("WheelEventDeltaFilter tests passed");
    return 0;
}
