#include "FreeTypeFontFixture.hpp"
#include "prx/libSceFreeTypeOt/libSceFreeTypeOt.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <vector>

namespace {

using FreeType::GuestFaceRec;
using FreeType::GuestGlyphSlotRec;
using FreeType::GuestLibrary;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libSceFreeTypeOt render internal: %s\n", message);
    std::exit(1);
}

template<typename TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

struct Face {
    GuestFaceRec* guest = nullptr;
    FT_Face oracle = nullptr;
};

Face Open(GuestLibrary* library, FT_Library native, const std::vector<unsigned char>& bytes) {
    Face face;
    Require(FT_New_Memory_Face_nid_postfix(library, bytes.data(), static_cast<std::int64_t>(bytes.size()), 0, &face.guest) == 0 && face.guest, "open guest face");
    Require(FT_New_Memory_Face(native, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &face.oracle) == 0, "open oracle face");
    Require(FT_Set_Pixel_Sizes_nid_postfix(face.guest, 24, 24) == 0 && FT_Set_Pixel_Sizes(face.oracle, 24, 24) == 0, "pixel sizes");
    Require(FT_Load_Glyph_nid_postfix(face.guest, 1, FT_LOAD_DEFAULT) == 0 && FT_Load_Glyph(face.oracle, 1, FT_LOAD_DEFAULT) == 0, "load glyph");
    Require(face.guest->glyph && face.oracle->glyph->format == FT_GLYPH_FORMAT_OUTLINE, "outline glyph");
    return face;
}

void Close(Face& face) {
    Require(FT_Done_Face_nid_postfix(face.guest) == 0 && FT_Done_Face(face.oracle) == 0, "release faces");
}

}

int main() {
    const std::vector<unsigned char> bytes = SquareGlyphFont('A', 700);
    GuestLibrary* library = nullptr;
    GuestLibrary* other = nullptr;
    FT_Library native = nullptr;
    Require(FT_Init_FreeType_nid_postfix(&library) == 0 && FT_Init_FreeType_nid_postfix(&other) == 0 && FT_Init_FreeType(&native) == 0, "initialize");
    for (const std::int32_t mode : {FT_RENDER_MODE_NORMAL, FT_RENDER_MODE_MONO}) {
        Face face = Open(library, native, bytes);
        GuestGlyphSlotRec* slot = face.guest->glyph;
        Require(FT_Render_Glyph_Internal_nid_postfix(library, slot, mode) == FT_Render_Glyph(face.oracle->glyph, static_cast<FT_Render_Mode>(mode)), "render result");
        const FT_Bitmap& expected = face.oracle->glyph->bitmap;
        Require(slot->format == static_cast<std::uint32_t>(face.oracle->glyph->format), "rendered format");
        Require(slot->bitmap.rows == expected.rows && slot->bitmap.width == expected.width && slot->bitmap.pitch == expected.pitch, "bitmap size");
        Require(slot->bitmap_left == face.oracle->glyph->bitmap_left && slot->bitmap_top == face.oracle->glyph->bitmap_top, "bitmap origin");
        Require(expected.rows > 0 && expected.buffer && slot->bitmap.buffer, "bitmap pixels");
        const auto pitch = static_cast<std::size_t>(expected.pitch < 0 ? -expected.pitch : expected.pitch);
        Require(std::memcmp(slot->bitmap.buffer, expected.buffer, pitch * expected.rows) == 0, "bitmap bytes");
        Require(FT_Render_Glyph_Internal_nid_postfix(library, slot, mode) == FT_Render_Glyph(face.oracle->glyph, static_cast<FT_Render_Mode>(mode)), "render bitmap again");
        Require(Throws([&] { FT_Render_Glyph_Internal_nid_postfix(other, slot, mode); }), "foreign library throws");
        Require(Throws([&] { FT_Render_Glyph_Internal_nid_postfix(nullptr, slot, mode); }), "null library throws");
        Require(Throws([&] { FT_Render_Glyph_Internal_nid_postfix(library, nullptr, mode); }), "null slot throws");
        GuestGlyphSlotRec copy = *slot;
        Require(Throws([&] { FT_Render_Glyph_Internal_nid_postfix(library, &copy, mode); }), "unregistered slot throws");
        Close(face);
    }
    Require(FT_Done_FreeType_nid_postfix(other) == 0 && FT_Done_FreeType_nid_postfix(library) == 0 && FT_Done_FreeType(native) == 0, "release libraries");
    std::puts("libSceFreeTypeOt: FT_Render_Glyph_Internal matches FreeType");
    return 0;
}
