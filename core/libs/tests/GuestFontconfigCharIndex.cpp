#include "prx/libfontconfig/libfontconfig.hpp"
#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "FreeTypeFontFixture.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cstdio>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

}

int main() {
    Require(FcFreeTypeCharIndex(nullptr, 'A') == 0);
    const auto font = SquareGlyphFont('A', 700, true);
    FreeType::GuestLibrary* library = nullptr;
    Require(FT_Init_FreeType_nid_postfix(&library) == 0);
    FreeType::GuestFaceRec* face = nullptr;
    Require(FT_New_Memory_Face_nid_postfix(library, font.data(), static_cast<std::int64_t>(font.size()), 0, &face) == 0);
    Require(face->num_charmaps == 2);
    Require(FcFreeTypeCharIndex(face, 'A') == 1);
    Require(face->charmap->encoding == FT_ENCODING_UNICODE);
    Require(FT_Select_Charmap_nid_postfix(face, FT_ENCODING_MS_SYMBOL) == 0);
    Require(FcFreeTypeCharIndex(face, 'A') == 1);
    Require(face->charmap->encoding == FT_ENCODING_MS_SYMBOL);
    Require(FT_Select_Charmap_nid_postfix(face, FT_ENCODING_UNICODE) == 0);
    Require(FcFreeTypeCharIndex(face, 0xf041) == 1);
    Require(face->charmap->encoding == FT_ENCODING_MS_SYMBOL);
    Require(FcFreeTypeCharIndex(face, 0x110000) == 0);
    Require(FT_Done_Face_nid_postfix(face) == 0);
    Require(FT_Done_FreeType_nid_postfix(library) == 0);
    std::puts("fontconfig FreeType char index passed");
}
