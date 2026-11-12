#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TRIGONOMETRY_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <utility>

using namespace FreeType;

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

static void TestRotate() {
    const std::int64_t angles[] = {0, 1, -1, 0x10000, -0x10000, 45 << 16, 90 << 16, 180 << 16, 270 << 16, 360 << 16, 725 << 16, -90 << 16, 0x7fffffff};
    for (const std::int64_t x : {0, 1, -1, 64, -64, 0x10000, -0x10000, 12345, -54321, 0x7fffffff, -0x7fffffff}) {
        for (const std::int64_t y : {0, 64, -640, 0x10000, 777777}) {
            for (const std::int64_t angle : angles) {
                GuestVector guest{x, y};
                FT_Vector native{static_cast<FT_Pos>(x), static_cast<FT_Pos>(y)};
                FT_Vector_Rotate_nid_postfix(&guest, angle);
                FT_Vector_Rotate(&native, static_cast<FT_Angle>(angle));
                Require(guest.x == native.x && guest.y == native.y);
            }
        }
    }
    FT_Vector_Rotate_nid_postfix(nullptr, 90 << 16);
}

static void TestPolarize() {
    for (const std::int64_t x : {0, 1, -1, 64, -64, 0x10000, -0x10000, 12345, -54321, 0x7fffffff, -0x7fffffff}) {
        for (const std::int64_t y : {0, 1, -1, 64, -64, 0x10000, -0x10000, 12345, -54321, 0x7fffffff, -0x7fffffff}) {
            const GuestVector guest{x, y};
            FT_Vector native{static_cast<FT_Pos>(x), static_cast<FT_Pos>(y)};
            std::int64_t length = 3;
            std::int64_t angle = 5;
            FT_Fixed nativeLength = 3;
            FT_Angle nativeAngle = 5;
            FT_Vector_Polarize_nid_postfix(&guest, &length, &angle);
            FT_Vector_Polarize(&native, &nativeLength, &nativeAngle);
            Require(length == nativeLength && angle == nativeAngle);
        }
    }
    std::int64_t length = 3;
    std::int64_t angle = 5;
    const GuestVector vector{64, 64};
    FT_Vector_Polarize_nid_postfix(nullptr, &length, &angle);
    FT_Vector_Polarize_nid_postfix(&vector, nullptr, &angle);
    FT_Vector_Polarize_nid_postfix(&vector, &length, nullptr);
    Require(length == 3 && angle == 5);
}

static void TestFromPolar() {
    const std::int64_t angles[] = {0, 1, -1, 0x10000, 45 << 16, 90 << 16, 180 << 16, 270 << 16, 360 << 16, 725 << 16, -90 << 16};
    for (const std::int64_t length : {0, 1, -1, 64, -64, 0x10000, -0x10000, 12345, -54321, 0x7fffffff, -0x7fffffff}) {
        for (const std::int64_t angle : angles) {
            GuestVector guest{11, 13};
            FT_Vector native{11, 13};
            FT_Vector_From_Polar_nid_postfix(&guest, length, angle);
            FT_Vector_From_Polar(&native, static_cast<FT_Fixed>(length), static_cast<FT_Angle>(angle));
            Require(guest.x == native.x && guest.y == native.y);
        }
    }
    FT_Vector_From_Polar_nid_postfix(nullptr, 64, 0);
}

static void TestAccumulatedAngles() {
    constexpr std::int64_t turn = 360ll << 16;
    const std::pair<std::int64_t, FT_Angle> angles[] = {
        {turn * 1000 + (45ll << 16), 45 << 16},
        {-turn * 1000 - (45ll << 16), -(45 << 16)},
        {turn * 1000, 360 << 16}, {-turn * 1000, -(360 << 16)},
        {turn * 100000000000ll + (90ll << 16), 90 << 16},
        {-turn * 100000000000ll - (90ll << 16), -(90 << 16)},
        {std::numeric_limits<std::int64_t>::max(), 16252927},
        {std::numeric_limits<std::int64_t>::min(), -16252928}};
    for (const auto& [angle, reference] : angles) {
        for (const GuestVector input : {GuestVector{12345, -54321}, GuestVector{123456789, 987654321}}) {
            auto actual = input;
            FT_Vector expected{static_cast<FT_Pos>(input.x), static_cast<FT_Pos>(input.y)};
            FT_Vector_Rotate(&expected, reference);
            FT_Vector_Rotate_nid_postfix(&actual, angle);
            Require(actual.x == expected.x && actual.y == expected.y);
        }
        for (const std::int64_t length : {0, 1, -1, 12345, -54321}) {
            GuestVector actual{11, 13};
            FT_Vector expected{};
            FT_Vector_From_Polar(&expected, static_cast<FT_Fixed>(length), reference);
            FT_Vector_From_Polar_nid_postfix(&actual, length, angle);
            Require(actual.x == expected.x && actual.y == expected.y);
        }
    }
    FT_Vector_Rotate_nid_postfix(nullptr, std::numeric_limits<std::int64_t>::max());
    FT_Vector_From_Polar_nid_postfix(nullptr, 64, std::numeric_limits<std::int64_t>::min());
}

int main() {
    TestAtan2();
    TestLength();
    TestRotate();
    TestPolarize();
    TestFromPolar();
    TestAccumulatedAngles();
}
