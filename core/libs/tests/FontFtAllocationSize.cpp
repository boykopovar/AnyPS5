#include "prx/libSceFontFt/include/FontFtDriver.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Font Ft allocation size check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

int main() {
    Require(FontFt::FitsGuestSize(1));
    Require(FontFt::FitsGuestSize(16));
    Require(!FontFt::FitsGuestSize(-1L));
    Require(FontFt::FitsGuestSize(std::numeric_limits<long>::max()) == (sizeof(long) < 8));
    if constexpr (sizeof(long) == 8) {
        const long boundary = 1L << (sizeof(long) * 4);
        Require(FontFt::FitsGuestSize(boundary - 1));
        Require(!FontFt::FitsGuestSize(boundary));
        Require(!FontFt::FitsGuestSize(boundary + 16));
    }
    return 0;
}
