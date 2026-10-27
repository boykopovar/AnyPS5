#include "FontFixture.hpp"
#include "FreeTypeFontFixture.hpp"
#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_OUTLINE_H
#include FT_SYNTHESIS_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

using namespace FreeType;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType slot synthesis check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

struct SlotPair {
    std::vector<unsigned char> bytes;
    GuestFace guest;
    FT_Face oracle = nullptr;

    SlotPair(GuestLibrary& library, FT_Library nativeLibrary, std::vector<unsigned char> font) : bytes(std::move(font)) {
        guest.library = &library;
        Require(FT_New_Memory_Face(library.host, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &guest.host) == 0);
        Require(FT_New_Memory_Face(nativeLibrary, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &oracle) == 0);
        Require(FT_Set_Pixel_Sizes(guest.host, 18, 18) == 0 && FT_Set_Pixel_Sizes(oracle, 18, 18) == 0);
        Require(FT_Load_Glyph(guest.host, 1, FT_LOAD_DEFAULT) == 0 && FT_Load_Glyph(oracle, 1, FT_LOAD_DEFAULT) == 0);
        SyncFace(guest);
    }

    ~SlotPair() {
        Require(FT_Done_Face(guest.host) == 0);
        Require(FT_Done_Face(oracle) == 0);
    }

    void Matches() const {
        const GuestGlyphSlotRec& actual = guest.slot;
        const FT_GlyphSlot expected = oracle->glyph;
        Require(actual.format == static_cast<std::uint32_t>(expected->format));
        Require(actual.advance.x == expected->advance.x && actual.advance.y == expected->advance.y);
        Require(actual.metrics.width == expected->metrics.width && actual.metrics.height == expected->metrics.height);
        Require(actual.metrics.horiAdvance == expected->metrics.horiAdvance && actual.metrics.horiBearingX == expected->metrics.horiBearingX);
        Require(actual.metrics.horiBearingY == expected->metrics.horiBearingY && actual.metrics.vertAdvance == expected->metrics.vertAdvance);
        Require(actual.outline.n_points == expected->outline.n_points && actual.outline.n_contours == expected->outline.n_contours);
        for (std::uint16_t index = 0; index < expected->outline.n_points; ++index) {
            Require(actual.outline.points[index].x == expected->outline.points[index].x);
            Require(actual.outline.points[index].y == expected->outline.points[index].y);
        }
        Require(guest.host->glyph->outline.n_points == expected->outline.n_points);
    }
};

static void EmboldenBitmapGlyph(GuestLibrary& library, FT_Library nativeLibrary) {
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
    const FT_Bitmap before = oracle->glyph->bitmap;
    FT_GlyphSlot_Embolden_nid_postfix(&guest.slot);
    FT_GlyphSlot_Embolden(oracle->glyph);
    const GuestBitmap& actual = guest.slot.bitmap;
    const FT_Bitmap& expected = oracle->glyph->bitmap;
    Require(expected.width != before.width || expected.rows != before.rows);
    Require(actual.rows == expected.rows && actual.width == expected.width && actual.pitch == expected.pitch);
    Require(actual.pixel_mode == expected.pixel_mode && actual.buffer && expected.buffer);
    Require(std::memcmp(actual.buffer, expected.buffer, static_cast<std::size_t>(expected.pitch) * expected.rows) == 0);
    Require(guest.slot.bitmap_left == oracle->glyph->bitmap_left && guest.slot.bitmap_top == oracle->glyph->bitmap_top);
    Require(guest.slot.advance.x == oracle->glyph->advance.x && guest.slot.metrics.width == oracle->glyph->metrics.width);
    Require(FT_Done_Face(guest.host) == 0 && FT_Done_Face(oracle) == 0);
}

int main() {
    GuestLibrary library{};
    FT_Library nativeLibrary = nullptr;
    Require(FT_Init_FreeType(&library.host) == 0 && FT_Init_FreeType(&nativeLibrary) == 0);
    {
        SlotPair pair(library, nativeLibrary, SquareGlyphFont('A', 700));
        pair.Matches();
        const FT_Outline& outline = pair.oracle->glyph->outline;
        std::vector<FT_Vector> initial(outline.points, outline.points + outline.n_points);
        FT_GlyphSlot_Oblique_nid_postfix(&pair.guest.slot);
        FT_GlyphSlot_Oblique(pair.oracle->glyph);
        pair.Matches();
        bool changed = false;
        for (std::size_t index = 0; index < initial.size(); ++index) changed |= initial[index].x != outline.points[index].x;
        Require(changed);
        pair.guest.slot.outline.points[1].y += 64;
        pair.oracle->glyph->outline.points[1].y += 64;
        FT_GlyphSlot_Oblique_nid_postfix(&pair.guest.slot);
        FT_GlyphSlot_Oblique(pair.oracle->glyph);
        pair.Matches();
    }
    {
        SlotPair pair(library, nativeLibrary, SquareGlyphFont('A', 700));
        const FT_Outline& outline = pair.oracle->glyph->outline;
        std::vector<FT_Vector> initial(outline.points, outline.points + outline.n_points);
        FT_GlyphSlot_Embolden_nid_postfix(&pair.guest.slot);
        FT_GlyphSlot_Embolden(pair.oracle->glyph);
        pair.Matches();
        bool changed = false;
        for (std::size_t index = 0; index < initial.size(); ++index) changed |= initial[index].x != outline.points[index].x || initial[index].y != outline.points[index].y;
        Require(changed);
        pair.guest.slot.outline.points[0].x += 32;
        pair.oracle->glyph->outline.points[0].x += 32;
        FT_GlyphSlot_Embolden_nid_postfix(&pair.guest.slot);
        FT_GlyphSlot_Embolden(pair.oracle->glyph);
        pair.Matches();
    }
    EmboldenBitmapGlyph(library, nativeLibrary);
    FT_GlyphSlot_Embolden_nid_postfix(nullptr);
    FT_GlyphSlot_Oblique_nid_postfix(nullptr);
    GuestGlyphSlotRec unregistered{};
    FT_GlyphSlot_Oblique_nid_postfix(&unregistered);
    FT_GlyphSlot_Embolden_nid_postfix(&unregistered);
    Require(FT_Done_FreeType(library.host) == 0 && FT_Done_FreeType(nativeLibrary) == 0);
}
