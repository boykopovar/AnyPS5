#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_BITMAP_H

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

using namespace FreeType;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType bitmap embolden check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static void Matches(const GuestBitmap& guest, const FT_Bitmap& native) {
    Require(guest.rows == native.rows && guest.width == native.width && guest.pitch == native.pitch);
    Require(guest.num_grays == native.num_grays && guest.pixel_mode == native.pixel_mode);
    Require((guest.buffer == nullptr) == (native.buffer == nullptr));
    if (!guest.buffer) return;
    const auto pitch = static_cast<std::size_t>(native.pitch < 0 ? -native.pitch : native.pitch);
    Require(std::memcmp(guest.buffer, native.buffer, pitch * native.rows) == 0);
}

static void Compare(GuestLibrary& library, FT_Library native, unsigned char mode, std::uint32_t width, std::uint32_t rows,
                    FT_Pos xStrength, FT_Pos yStrength) {
    const std::uint32_t pitch = mode == FT_PIXEL_MODE_MONO ? (width + 7) / 8 : mode == FT_PIXEL_MODE_LCD ? width * 3 : width;
    std::array<unsigned char, 64> pixels{};
    for (std::size_t index = 0; index < pixels.size(); ++index) pixels[index] = static_cast<unsigned char>(index * 37 + 11);
    GuestBitmap source{rows, mode == FT_PIXEL_MODE_LCD ? width * 3 : width, static_cast<std::int32_t>(pitch), pixels.data(),
                       static_cast<std::uint16_t>(mode == FT_PIXEL_MODE_MONO ? 2 : 256), mode, 0, nullptr};
    FT_Bitmap nativeSource{source.rows, source.width, source.pitch, source.buffer, source.num_grays, source.pixel_mode, 0, nullptr};
    GuestBitmap guest{};
    FT_Bitmap expected{};
    FT_Bitmap_Init(&expected);
    Require(FT_Bitmap_Copy_nid_postfix(&library, &source, &guest) == FT_Bitmap_Copy(native, &nativeSource, &expected));
    Require(FT_Bitmap_Embolden_nid_postfix(&library, &guest, xStrength, yStrength) == FT_Bitmap_Embolden(native, &expected, xStrength, yStrength));
    Matches(guest, expected);
    Require(FT_Bitmap_Done_nid_postfix(&library, &guest) == FT_Bitmap_Done(native, &expected));
}

int main() {
    GuestLibrary library{};
    FT_Library native = nullptr;
    Require(FT_Init_FreeType(&library.host) == 0 && FT_Init_FreeType(&native) == 0);
    for (const unsigned char mode : {FT_PIXEL_MODE_MONO, FT_PIXEL_MODE_GRAY, FT_PIXEL_MODE_LCD, FT_PIXEL_MODE_BGRA}) {
        for (const FT_Pos strength : {0, 32, 64, 128, 256, -64}) {
            Compare(library, native, mode, 5, 4, strength, strength);
            Compare(library, native, mode, 3, 3, strength, 2 * strength);
        }
    }
    GuestBitmap bitmap{};
    Require(FT_Bitmap_Embolden_nid_postfix(nullptr, &bitmap, 64, 64) == FT_Err_Invalid_Library_Handle);
    Require(FT_Bitmap_Embolden_nid_postfix(&library, nullptr, 64, 64) == FT_Err_Invalid_Argument);
    unsigned char pixel = 0x80;
    GuestBitmap bounded{1, 1, 1, &pixel, 256, FT_PIXEL_MODE_GRAY, 0, nullptr};
    Require(FT_Bitmap_Embolden_nid_postfix(&library, &bounded, std::numeric_limits<FT_Pos>::max(), 0) == FT_Err_Invalid_Argument);
    if constexpr (sizeof(FT_Pos) > sizeof(FT_Int)) {
        constexpr std::int64_t minimumRounded = static_cast<std::int64_t>(std::numeric_limits<FT_Int>::min()) * 64 - 32;
        constexpr std::int64_t maximumRounded = static_cast<std::int64_t>(std::numeric_limits<FT_Int>::max()) * 64 + 31;
        Require(FT_Bitmap_Embolden_nid_postfix(&library, &bounded, maximumRounded + 1, 0) == FT_Err_Invalid_Argument);
        Require(FT_Bitmap_Embolden_nid_postfix(&library, &bounded, minimumRounded - 1, 0) == FT_Err_Invalid_Argument);
    }
    FT_Bitmap emptyNative{};
    Require(FT_Bitmap_Embolden_nid_postfix(&library, &bitmap, 64, 64) == FT_Bitmap_Embolden(native, &emptyNative, 64, 64));
    Require(FT_Done_FreeType(library.host) == 0 && FT_Done_FreeType(native) == 0);
}
