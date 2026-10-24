#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_OUTLINE_H

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <vector>

using namespace FreeType;

extern "C" {
int APS5_VABI FT_Outline_Get_Bitmap_nid_postfix(GuestLibrary*, GuestOutline*, const GuestBitmap*);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType outline bitmap check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

struct Shape {
    std::vector<FT_Vector> points;
    std::vector<unsigned char> tags;
    std::vector<unsigned short> contours;
};

static Shape Square() {
    return {{{64, 64}, {576, 64}, {576, 576}, {64, 576}}, {1, 1, 1, 1}, {3}};
}

static Shape Triangle() {
    return {{{32, 32}, {672, 160}, {160, 672}}, {1, 1, 1}, {2}};
}

static Shape Curve() {
    return {{{64, 64}, {448, 64}, {704, 320}, {448, 576}, {64, 576}}, {1, 0, 1, 0, 1}, {4}};
}

static void Compare(GuestLibrary& library, FT_Library native, const Shape& shape, unsigned char mode) {
    std::vector<GuestVector> guestPoints;
    for (const FT_Vector& point : shape.points) guestPoints.push_back({point.x, point.y});
    std::vector<FT_Vector> nativePoints = shape.points;
    std::vector<unsigned char> tags = shape.tags;
    std::vector<unsigned short> contours = shape.contours;
    const auto points = static_cast<std::uint16_t>(shape.points.size());
    GuestOutline guest{1, points, guestPoints.data(), tags.data(), contours.data(), 0};
    FT_Outline expected{1, points, nativePoints.data(), tags.data(), contours.data(), 0};
    const std::uint32_t width = 12;
    const std::uint32_t rows = 12;
    const std::int32_t pitch = mode == FT_PIXEL_MODE_MONO ? 2 : 12;
    std::vector<unsigned char> guestPixels(static_cast<std::size_t>(pitch) * rows, 0);
    std::vector<unsigned char> nativePixels = guestPixels;
    const GuestBitmap guestBitmap{rows, width, pitch, guestPixels.data(), static_cast<std::uint16_t>(mode == FT_PIXEL_MODE_MONO ? 2 : 256), mode, 0, nullptr};
    const FT_Bitmap nativeBitmap{rows, width, pitch, nativePixels.data(), guestBitmap.num_grays, mode, 0, nullptr};
    Require(FT_Outline_Get_Bitmap_nid_postfix(&library, &guest, &guestBitmap) == FT_Outline_Get_Bitmap(native, &expected, &nativeBitmap));
    Require(guestPixels == nativePixels);
    bool drawn = false;
    for (const unsigned char value : guestPixels) drawn |= value != 0;
    Require(drawn);
}

int main() {
    GuestLibrary library{};
    FT_Library native = nullptr;
    Require(FT_Init_FreeType(&library.host) == 0 && FT_Init_FreeType(&native) == 0);
    for (const auto& make : {+[] { return Square(); }, +[] { return Triangle(); }, +[] { return Curve(); }}) {
        for (const unsigned char mode : {FT_PIXEL_MODE_GRAY, FT_PIXEL_MODE_MONO}) Compare(library, native, make(), mode);
    }
    Shape square = Square();
    std::vector<GuestVector> points;
    for (const FT_Vector& point : square.points) points.push_back({point.x, point.y});
    GuestOutline outline{1, 4, points.data(), square.tags.data(), square.contours.data(), 0};
    std::array<unsigned char, 144> pixels{};
    const GuestBitmap bitmap{12, 12, 12, pixels.data(), 256, FT_PIXEL_MODE_GRAY, 0, nullptr};
    const GuestBitmap unsupported{12, 12, 12, pixels.data(), 256, FT_PIXEL_MODE_BGRA, 0, nullptr};
    const FT_Bitmap nativeUnsupported{12, 12, 12, pixels.data(), 256, FT_PIXEL_MODE_BGRA, 0, nullptr};
    const FT_Bitmap nativeBitmap{12, 12, 12, pixels.data(), 256, FT_PIXEL_MODE_GRAY, 0, nullptr};
    FT_Outline nativeOutline{1, 4, square.points.data(), square.tags.data(), square.contours.data(), 0};
    Require(FT_Outline_Get_Bitmap_nid_postfix(&library, &outline, &unsupported) == FT_Outline_Get_Bitmap(native, &nativeOutline, &nativeUnsupported));
    Require(FT_Outline_Get_Bitmap_nid_postfix(&library, &outline, nullptr) == FT_Outline_Get_Bitmap(native, &nativeOutline, nullptr));
    Require(FT_Outline_Get_Bitmap_nid_postfix(&library, nullptr, &bitmap) == FT_Outline_Get_Bitmap(native, nullptr, &nativeBitmap));
    Require(FT_Outline_Get_Bitmap_nid_postfix(nullptr, &outline, &bitmap) == FT_Outline_Get_Bitmap(nullptr, &nativeOutline, &nativeBitmap));
    Require(FT_Done_FreeType(library.host) == 0 && FT_Done_FreeType(native) == 0);
}
