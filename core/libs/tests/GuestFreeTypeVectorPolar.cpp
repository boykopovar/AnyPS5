#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRIGONOMETRY_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

using namespace FreeType;

extern "C" {
std::int64_t APS5_VABI FT_Atan2_nid_postfix(std::int64_t, std::int64_t);
std::int64_t APS5_VABI FT_Vector_Length_nid_postfix(const GuestVector*);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType vector check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static void TestAtan2() {
    for (const std::int64_t x : {0, 1, -1, 64, -64, 0x10000, -0x10000, 12345, -54321, 0x7fffffff, -0x7fffffff}) {
        for (const std::int64_t y : {0, 1, -1, 64, -64, 0x10000, -0x10000, 12345, -54321, 0x7fffffff, -0x7fffffff}) Require(FT_Atan2_nid_postfix(x, y) == FT_Atan2(static_cast<FT_Fixed>(x), static_cast<FT_Fixed>(y)));
    }
}

static void TestLength() {
    for (const std::int64_t x : {0, 1, -1, 64, -64, 0x10000, -0x10000, 12345, -54321, 0x7fffffff, -0x7fffffff}) {
        for (const std::int64_t y : {0, 1, -1, 64, -64, 0x10000, -0x10000, 12345, -54321, 0x7fffffff, -0x7fffffff}) {
            const GuestVector guest{x, y};
            FT_Vector native{static_cast<FT_Pos>(x), static_cast<FT_Pos>(y)};
            Require(FT_Vector_Length_nid_postfix(&guest) == FT_Vector_Length(&native));
        }
    }
    Require(FT_Vector_Length_nid_postfix(nullptr) == FT_Vector_Length(nullptr));
}

int main() {
    TestAtan2();
    TestLength();
}
