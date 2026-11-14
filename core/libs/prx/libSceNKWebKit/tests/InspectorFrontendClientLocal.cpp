#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <type_traits>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "InspectorFrontendClientLocal check failed at line %u\n", location.line());
        std::abort();
    }
}

using ConstrainedAttachedWindowSize = unsigned int (APS5_VABI *)(unsigned int, unsigned int);
static_assert(std::is_same_v<decltype(&_ZN7WebCore28InspectorFrontendClientLocal31constrainedAttachedWindowHeightEjj), ConstrainedAttachedWindowSize>);
static_assert(std::is_same_v<decltype(&_ZN7WebCore28InspectorFrontendClientLocal30constrainedAttachedWindowWidthEjj), ConstrainedAttachedWindowSize>);

unsigned int Height(unsigned int preferred, unsigned int total) {
    return _ZN7WebCore28InspectorFrontendClientLocal31constrainedAttachedWindowHeightEjj(preferred, total);
}

unsigned int Width(unsigned int preferred, unsigned int total) {
    return _ZN7WebCore28InspectorFrontendClientLocal30constrainedAttachedWindowWidthEjj(preferred, total);
}

void TestHeight() {
    Require(Height(100, 1000) == 250);
    Require(Height(400, 1000) == 400);
    Require(Height(900, 1000) == 750);
    Require(Height(0, 0) == 250);
    Require(Height(333, 1001) == 333);
    Require(Height(1000, 1001) == 751);
    Require(Height(1000, 1002) == 752);
    Require(Height(0xFFFFFFFFu, 0xFFFFFFFFu) == 3221225472u);
    std::printf("InspectorFrontendClientLocal::constrainedAttachedWindowHeight: minimum, ratio cap and rounding passed\n");
}

void TestWidth() {
    Require(Width(100, 2000) == 500);
    Require(Width(900, 2000) == 900);
    Require(Width(1900, 2000) == 1680);
    Require(Width(600, 700) == 500);
    Require(Width(1000, 0) == 500);
    Require(Width(0xFFFFFFFFu, 0xFFFFFFFFu) == 4294967040u);
    std::printf("InspectorFrontendClientLocal::constrainedAttachedWindowWidth: minimum, inspected-width cap and small windows passed\n");
}

}

int main() {
    TestHeight();
    TestWidth();
    return 0;
}
