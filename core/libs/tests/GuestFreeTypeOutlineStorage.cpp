#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_OUTLINE_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>

using namespace FreeType;

extern "C" {
int APS5_VABI FT_Outline_New_nid_postfix(GuestLibrary*, std::uint32_t, std::int32_t, GuestOutline*);
int APS5_VABI FT_Outline_Done_nid_postfix(GuestLibrary*, GuestOutline*);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType outline storage check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static void Compare(GuestLibrary& library, FT_Library native, std::uint32_t points, std::int32_t contours) {
    GuestOutline guest{1, 2, nullptr, nullptr, nullptr, 3};
    FT_Outline expected{1, 2, nullptr, nullptr, nullptr, 3};
    const int guestError = FT_Outline_New_nid_postfix(&library, points, contours, &guest);
    const int nativeError = FT_Outline_New(native, points, contours, &expected);
    Require(guestError == nativeError);
    Require(guest.n_points == expected.n_points && guest.n_contours == expected.n_contours && guest.flags == expected.flags);
    Require((guest.points == nullptr) == (expected.points == nullptr));
    Require((guest.tags == nullptr) == (expected.tags == nullptr));
    Require((guest.contours == nullptr) == (expected.contours == nullptr));
    for (std::uint32_t index = 0; nativeError == 0 && index < points; ++index) {
        Require(guest.points[index].x == 0 && guest.points[index].y == 0 && guest.tags[index] == 0);
    }
    for (std::int32_t index = 0; nativeError == 0 && index < contours; ++index) Require(guest.contours[index] == 0);
    Require(FT_Outline_Done_nid_postfix(&library, &guest) == FT_Outline_Done(native, &expected));
    Require(guest.n_points == 0 && guest.n_contours == 0 && guest.flags == 0);
    Require(!guest.points && !guest.tags && !guest.contours);
}

int main() {
    GuestLibrary library{};
    FT_Library native = nullptr;
    Require(FT_Init_FreeType(&library.host) == 0 && FT_Init_FreeType(&native) == 0);
    for (const std::uint32_t points : {0u, 1u, 7u, 65535u, 65536u, 0xFFFFFFFFu}) {
        for (const std::int32_t contours : {-1, 0, 1, 3, 7, 8, 65535}) Compare(library, native, points, contours);
    }
    GuestOutline outline{};
    Require(FT_Outline_New_nid_postfix(nullptr, 3, 1, &outline) == FT_Err_Invalid_Library_Handle);
    Require(FT_Outline_New_nid_postfix(&library, 3, 1, nullptr) == FT_Err_Invalid_Argument);
    Require(FT_Outline_Done_nid_postfix(nullptr, &outline) == FT_Err_Invalid_Library_Handle);
    Require(FT_Outline_Done_nid_postfix(&library, nullptr) == FT_Err_Invalid_Outline);

    GuestVector borrowedPoints[2]{};
    unsigned char borrowedTags[2]{};
    std::uint16_t borrowedContours[1]{1};
    GuestOutline borrowed{1, 2, borrowedPoints, borrowedTags, borrowedContours, 0};
    Require(FT_Outline_Done_nid_postfix(&library, &borrowed) == 0);
    Require(borrowed.n_points == 0 && !borrowed.points && borrowedContours[0] == 1);
    Require(FT_Done_FreeType(library.host) == 0 && FT_Done_FreeType(native) == 0);
}
