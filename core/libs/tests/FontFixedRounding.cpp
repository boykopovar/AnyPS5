#include "prx/libSceFont/include/FontFreeType.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>

int main() {
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    constexpr struct {
        std::int64_t value;
        std::int64_t scale;
        std::int32_t expected;
    } cases[] = {
        {0, 0, 0}, {1, 0, 0}, {-1, 0, 0},
        {1, 65536, 1}, {-1, 65536, -1},
        {1, 32767, 0}, {1, 32768, 1}, {1, 32769, 1},
        {1, 98304, 2}, {1, 98305, 2},
        {-1, 32767, 0}, {-1, 32768, -1}, {-1, 32769, -1},
        {12345, 98304, 18518}, {-12345, 98304, -18518},
        {-2147483648LL, 4294967296LL, -1}, {2147483647, 4294967296LL, -65536},
        {minimum, -1, -1}, {minimum, 1, -1}, {minimum, minimum, 0}, {minimum, maximum, -1},
        {maximum, -1, 1}, {maximum, 1, 1}, {maximum, 2, 0}, {maximum, maximum, 0},
        {1, minimum, 1}, {1, minimum + 65535, 2}, {1, minimum + 65536, 2}, {1, minimum + 98304, 2},
        {-1, minimum + 1, -1}, {-1, minimum + 32767, -1},
        {-1, minimum + 32768, -1}, {-1, minimum + 32769, -2},
        {minimum + 32768, 1, 0}, {minimum + 32767, 1, -1}, {minimum + 1, 65536, 0}
    };
    for (const auto& test : cases) {
        const auto actual = Font::RoundMulFixed(test.value, test.scale);
        if (actual != test.expected) {
            std::fprintf(stderr, "Font fixed rounding: %lld * %lld: expected %d, got %d\n",
                static_cast<long long>(test.value), static_cast<long long>(test.scale), test.expected, actual);
            std::abort();
        }
    }
}
