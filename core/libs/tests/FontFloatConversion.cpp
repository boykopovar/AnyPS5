#include "prx/libSceFont/include/FontFreeType.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

static void Check(float value, std::int32_t expected) {
    volatile float input = value;
    const auto actual = Font::TruncateFloatToInt(input);
    if (actual == expected) return;
    std::fprintf(stderr, "Font float conversion: %a converted to %d, expected %d\n", static_cast<double>(value), actual, expected);
    std::abort();
}

int main() {
    constexpr auto invalid = std::numeric_limits<std::int32_t>::min();
    constexpr float upper = 2147483648.0f;
    constexpr float lower = -2147483648.0f;
    Check(0.0f, 0);
    Check(-0.0f, 0);
    Check(1.75f, 1);
    Check(-1.75f, -1);
    Check(std::numeric_limits<float>::denorm_min(), 0);
    Check(-std::numeric_limits<float>::denorm_min(), 0);
    Check(std::nextafter(upper, 0.0f), 2147483520);
    Check(upper, invalid);
    Check(std::nextafter(upper, std::numeric_limits<float>::infinity()), invalid);
    Check(std::nextafter(lower, 0.0f), -2147483520);
    Check(lower, invalid);
    Check(std::nextafter(lower, -std::numeric_limits<float>::infinity()), invalid);
    Check(std::numeric_limits<float>::max(), invalid);
    Check(std::numeric_limits<float>::lowest(), invalid);
    Check(std::numeric_limits<float>::infinity(), invalid);
    Check(-std::numeric_limits<float>::infinity(), invalid);
    Check(std::numeric_limits<float>::quiet_NaN(), invalid);
}
