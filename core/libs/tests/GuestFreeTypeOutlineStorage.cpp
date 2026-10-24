#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_OUTLINE_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>

using namespace FreeType;

extern "C" {
int APS5_VABI FT_Outline_Done_nid_postfix(GuestLibrary*, GuestOutline*);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType outline storage check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static GuestOutline Owned(std::uint16_t points, std::uint16_t contours) {
    GuestOutline outline{contours, points, nullptr, nullptr, nullptr, FT_OUTLINE_OWNER};
    outline.points = static_cast<GuestVector*>(std::calloc(points, sizeof(GuestVector)));
    outline.tags = static_cast<unsigned char*>(std::calloc(points, 1));
    outline.contours = static_cast<std::uint16_t*>(std::calloc(contours, sizeof(std::uint16_t)));
    return outline;
}

int main() {
    GuestLibrary library{};
    FT_Library native = nullptr;
    Require(FT_Init_FreeType(&library.host) == 0 && FT_Init_FreeType(&native) == 0);
    for (const std::uint16_t points : {0, 1, 7, 65535}) {
        for (const std::uint16_t contours : {0, 1, 3}) {
            if (contours > points) continue;
            GuestOutline guest = Owned(points, contours);
            FT_Outline expected{};
            Require(FT_Outline_New(native, points, contours, &expected) == 0);
            Require(FT_Outline_Done_nid_postfix(&library, &guest) == FT_Outline_Done(native, &expected));
            Require(guest.n_points == expected.n_points && guest.n_contours == expected.n_contours && guest.flags == expected.flags);
            Require(!guest.points && !guest.tags && !guest.contours);
        }
    }
    GuestOutline outline{};
    Require(FT_Outline_Done_nid_postfix(nullptr, &outline) == FT_Err_Invalid_Library_Handle);
    Require(FT_Outline_Done_nid_postfix(&library, nullptr) == FT_Err_Invalid_Outline);
    FT_Outline nativeOutline{};
    Require(FT_Outline_Done(nullptr, &nativeOutline) == FT_Err_Invalid_Library_Handle);
    Require(FT_Outline_Done(native, nullptr) == FT_Err_Invalid_Outline);

    GuestVector borrowedPoints[2]{};
    unsigned char borrowedTags[2]{};
    std::uint16_t borrowedContours[1]{1};
    GuestOutline borrowed{1, 2, borrowedPoints, borrowedTags, borrowedContours, 0};
    Require(FT_Outline_Done_nid_postfix(&library, &borrowed) == 0);
    Require(borrowed.n_points == 0 && !borrowed.points && borrowedContours[0] == 1);
    Require(FT_Done_FreeType(library.host) == 0 && FT_Done_FreeType(native) == 0);
}
