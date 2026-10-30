#include "FontFixture.hpp"
#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_BITMAP_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace FreeType;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType bitmap init check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

int main() {
    GuestBitmap guest{7, 8, -9, reinterpret_cast<unsigned char*>(&guest), 10, 11, 12, &guest};
    FT_Bitmap native{7, 8, -9, reinterpret_cast<unsigned char*>(&native), 10, 11, 12, &native};
    FT_Bitmap_Init_nid_postfix(&guest);
    FT_Bitmap_Init(&native);
    Require(guest.rows == native.rows && guest.width == native.width && guest.pitch == native.pitch);
    Require(guest.buffer == nullptr && native.buffer == nullptr && guest.palette == nullptr && native.palette == nullptr);
    Require(guest.num_grays == native.num_grays && guest.pixel_mode == native.pixel_mode && guest.palette_mode == native.palette_mode);
    Require(guest.rows == 0 && guest.width == 0 && guest.pitch == 0 && guest.num_grays == 0 && guest.pixel_mode == 0 && guest.palette_mode == 0);
    FT_Bitmap_Init_nid_postfix(nullptr);

    GuestLibrary library{};
    Require(FT_Init_FreeType(&library.host) == 0);
    const std::vector<unsigned char> bytes = FontFixture::BuildFont(1, {});
    GuestFace face;
    face.library = &library;
    Require(FT_New_Memory_Face(library.host, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &face.host) == 0);
    SyncFace(face);
    face.host->glyph->bitmap.rows = 9;
    face.host->glyph->bitmap.width = 17;
    face.slot.bitmap.rows = 9;
    FT_Bitmap_Init_nid_postfix(&face.slot.bitmap);
    Require(face.slot.bitmap.rows == 0 && face.host->glyph->bitmap.rows == 0 && face.host->glyph->bitmap.width == 0);
    Require(FT_Done_Face(face.host) == 0);
    Require(FT_Done_FreeType(library.host) == 0);
}
