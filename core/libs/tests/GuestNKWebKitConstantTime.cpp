#include "prx/libSceNKWebKit/include/WtfCryptoUtilities.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <type_traits>

using Compare = int (APS5_VABI*)(const void*, const void*, std::size_t);
static_assert(std::is_same_v<decltype(&_ZN3WTF18constantTimeMemcmpEPKvS1_m), Compare>);

namespace {

void Check(bool passed, int line) {
    if (passed) return;
    std::fprintf(stderr, "WTF constantTimeMemcmp check failed at line %d\n", line);
    std::abort();
}

#define Require(expression) Check((expression), __LINE__)

void TestComparison() {
    const auto compare = _ZN3WTF18constantTimeMemcmpEPKvS1_m;
    Require(compare(nullptr, nullptr, 0) == 0);
    Require(compare(nullptr, "test", 0) == 0);
    Require(compare("test", nullptr, 0) == 0);

    const std::array<std::uint8_t, 6> a{0, 1, 2, 3, 4, 255};
    std::array<std::uint8_t, 6> b = a;
    Require(compare(a.data(), b.data(), a.size()) == 0);
    Require(compare(a.data(), b.data(), 1) == 0);
    b[5] = 127;
    Require(compare(a.data(), b.data(), 5) == 0);
    Require(compare(a.data(), b.data(), 6) == 0x80);
    b[0] = 8;
    Require(compare(a.data(), b.data(), 6) == 0x88);
    Require(compare(a.data(), b.data(), 1) == 8);
    b[0] = 0;
    b[5] = 255;
    Require(compare(a.data(), b.data(), 6) == 0);

    const std::array<std::uint8_t, 4> nonaligned{11, 20, 33, 44};
    Require(compare(nonaligned.data() + 1, nonaligned.data() + 1, 3) == 0);
    Require(compare(nonaligned.data(), nonaligned.data() + 1, 3) != 0);
    Require(compare(nonaligned.data(), nonaligned.data(), 4) == 0);
}

}

int main() {
    TestComparison();
    return 0;
}
