#include "prx/libSceFont/include/FontFreeType.hpp"
#include "tests/FontFixture.hpp"
#include <cstdio>
#include <cstdlib>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Font charmap fallback check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static void CheckSelection(FT_Face face, FT_CharMap original, FT_UInt first, FT_UInt second) {
    Require(face->charmap == original);
    Require(FT_Get_Char_Index(face, 65) == first);
    Require(FT_Get_Char_Index(face, 66) == second);
    if (original) {
        Require(FT_Load_Char(face, 65, FT_LOAD_NO_SCALE) == 0);
        Require(face->glyph->glyph_index == first);
        Require(FT_Load_Char(face, 66, FT_LOAD_NO_SCALE) == 0);
        Require(face->glyph->glyph_index == second);
    }
}

static void CheckLookups(FT_Face face) {
    const FT_CharMap original = face->charmap;
    const FT_UInt first = FT_Get_Char_Index(face, 65);
    const FT_UInt second = FT_Get_Char_Index(face, 66);
    Require(Font::ResolveGlyphIndexWithFallback(face, 65) == (face->num_charmaps > 0 ? 1u : 0u));
    CheckSelection(face, original, first, second);
    Require(Font::ResolveGlyphIndexWithFallback(face, 66) == (face->num_charmaps == 2 ? 2u : 0u));
    CheckSelection(face, original, first, second);
    Require(Font::ResolveGlyphIndexWithFallback(face, 67) == 0);
    CheckSelection(face, original, first, second);
}

static void CheckFont(FT_Library library, const std::vector<unsigned char>& data, int charmaps) {
    FT_Face face = nullptr;
    Require(FT_New_Memory_Face(library, data.data(), static_cast<FT_Long>(data.size()), 0, &face) == 0);
    Require(face->num_charmaps == charmaps);
    for (int i = 0; i < face->num_charmaps; ++i) {
        Require(FT_Set_Charmap(face, face->charmaps[i]) == 0);
        CheckLookups(face);
    }
    face->charmap = nullptr;
    CheckLookups(face);
    Require(FT_Done_Face(face) == 0);
}

int main() {
    const auto dual = FontFixture::Words({0, 2, 3, 1, 0, 20, 0, 0, 0, 52,
        4, 32, 0, 4, 4, 1, 0, 65, 0xFFFF, 0, 65, 0xFFFF, 1 - 65, 1, 0, 0,
        4, 32, 0, 4, 4, 1, 0, 66, 0xFFFF, 0, 66, 0xFFFF, 2 - 66, 1, 0, 0});
    const auto single = FontFixture::Words({0, 1, 3, 1, 0, 12,
        4, 32, 0, 4, 4, 1, 0, 65, 0xFFFF, 0, 65, 0xFFFF, 1 - 65, 1, 0, 0});
    FT_Library library = nullptr;
    Require(FT_Init_FreeType(&library) == 0);
    CheckFont(library, FontFixture::BuildFont(3, {{"cmap", dual}}), 2);
    CheckFont(library, FontFixture::BuildFont(2, {{"cmap", single}}), 1);
    CheckFont(library, FontFixture::BuildFont(2, {}), 0);
    Require(Font::ResolveGlyphIndexWithFallback(nullptr, 65) == 0);
    Require(FT_Done_FreeType(library) == 0);
}
