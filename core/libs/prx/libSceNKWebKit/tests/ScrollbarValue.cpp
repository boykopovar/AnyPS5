#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <limits>

namespace {

struct Case {
    float position;
    float total;
    float visible;
    float value;
    float overhang;
    float aliased;
};

bool Same(float actual, float expected) {
    return std::isnan(expected) ? std::isnan(actual) : std::bit_cast<std::uint32_t>(actual) == std::bit_cast<std::uint32_t>(expected);
}

void Run() {
    constexpr float infinity = std::numeric_limits<float>::infinity();
    constexpr float nan = std::numeric_limits<float>::quiet_NaN();
    const Case cases[] = {
        {-12, 100, 20, 0, 12, 12},
        {-0.0f, 100, 20, -0.0f, 0, -0.0f},
        {0, 100, 20, 0, 0, 0},
        {20, 100, 20, 0.25f, 0, 0.25f},
        {80, 100, 20, 1, 0, 1},
        {90, 100, 20, 1, 10, 10},
        {0, 20, 20, 0, 0, 0},
        {1, 20, 20, 1, 1, 1},
        {0, 0, 0, 0, 0, 0},
        {0, 10, 20, 1, 10, 10},
        {-2, 10, 20, 0, 2, 2},
        {1, 0x1p24f, 0x1p24f, 0, 0, 0},
        {0x1p24f, 0x1p24f, 1, 0x1.000002p0f, 0, 0x1.000002p0f},
        {-infinity, 100, 20, 0, infinity, infinity},
        {infinity, 100, 20, 1, infinity, infinity},
        {20, infinity, 20, 0, 0, 0},
        {20, -infinity, 20, 1, infinity, infinity},
        {20, 100, infinity, 1, infinity, infinity},
        {20, 100, -infinity, 0, 0, 0},
        {infinity, infinity, 20, nan, 0, nan},
        {0, infinity, infinity, 0, 0, 0},
        {infinity, 100, -infinity, nan, 0, nan},
        {nan, 100, 20, nan, 0, nan},
        {20, nan, 20, 0, 0, 0},
        {20, 100, nan, 0, 0, 0},
        {-2, nan, nan, 0, 2, 2},
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        const auto& input = cases[i];
        float value = -123;
        float overhang = -456;
        _ZN7WebCore14ScrollableArea32computeScrollbarValueAndOverhangEfffRfS1_(input.position, input.total, input.visible, value, overhang);
        float aliased = -789;
        _ZN7WebCore14ScrollableArea32computeScrollbarValueAndOverhangEfffRfS1_(input.position, input.total, input.visible, aliased, aliased);
        if (!Same(value, input.value) || !Same(overhang, input.overhang) || !Same(aliased, input.aliased)) {
            std::fprintf(stderr, "Scrollbar case %u failed: value=%a overhang=%a aliased=%a\n", i, value, overhang, aliased);
            std::abort();
        }
    }
}

}

int main() {
    try {
        Run();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "Scrollbar value exception: %s\n", error.what());
        return 1;
    }
}
