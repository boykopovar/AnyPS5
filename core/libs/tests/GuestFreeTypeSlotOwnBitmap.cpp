#include "FontFixture.hpp"
#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_BITMAP_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

using namespace FreeType;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType slot own bitmap check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static void Matches(const GuestBitmap& guest, const FT_Bitmap& native) {
    Require(guest.rows == native.rows && guest.width == native.width && guest.pitch == native.pitch);
    Require(guest.num_grays == native.num_grays && guest.pixel_mode == native.pixel_mode);
    Require(guest.buffer && native.buffer);
    Require(std::memcmp(guest.buffer, native.buffer, static_cast<std::size_t>(native.pitch) * native.rows) == 0);
}

int main() {
    GuestLibrary library{};
    FT_Library nativeLibrary = nullptr;
    Require(FT_Init_FreeType(&library.host) == 0 && FT_Init_FreeType(&nativeLibrary) == 0);
    {
        const std::vector<unsigned char> bytes = FontFixture::BitmapFont();
        GuestFace guest;
        guest.library = &library;
        FT_Face oracle = nullptr;
        Require(FT_New_Memory_Face(library.host, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &guest.host) == 0);
        Require(FT_New_Memory_Face(nativeLibrary, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &oracle) == 0);
        SyncFace(guest);
        const std::uint32_t glyph = FT_Get_Char_Index(oracle, 65);
        Require(glyph != 0);
        Require(FT_Load_Glyph_nid_postfix(&guest.rec, glyph, FT_LOAD_DEFAULT) == FT_Load_Glyph(oracle, glyph, FT_LOAD_DEFAULT));
        Require(guest.host->glyph->format == FT_GLYPH_FORMAT_BITMAP);
        const unsigned char* before = guest.slot.bitmap.buffer;
        Require(before != nullptr);
        Require(FT_GlyphSlot_Own_Bitmap_nid_postfix(&guest.slot) == FT_GlyphSlot_Own_Bitmap(oracle->glyph));
        Matches(guest.slot.bitmap, oracle->glyph->bitmap);
        Require(guest.slot.bitmap.buffer != before);
        Require(guest.slot.bitmap.buffer == guest.host->glyph->bitmap.buffer);
        const unsigned char* owned = guest.slot.bitmap.buffer;
        Require(FT_GlyphSlot_Own_Bitmap_nid_postfix(&guest.slot) == FT_GlyphSlot_Own_Bitmap(oracle->glyph));
        Require(guest.slot.bitmap.buffer == owned);
        Require(FT_Done_Face(guest.host) == 0);
        Require(FT_Done_Face(oracle) == 0);
    }
    Require(FT_GlyphSlot_Own_Bitmap_nid_postfix(nullptr) == FT_GlyphSlot_Own_Bitmap(nullptr));
    GuestGlyphSlotRec unregistered{};
    Require(FT_GlyphSlot_Own_Bitmap_nid_postfix(&unregistered) == FT_GlyphSlot_Own_Bitmap(nullptr));
    Require(FT_Done_FreeType(library.host) == 0 && FT_Done_FreeType(nativeLibrary) == 0);
}
