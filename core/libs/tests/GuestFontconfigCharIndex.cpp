#include "prx/libfontconfig/libfontconfig.hpp"
#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "FreeTypeFontFixture.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <utility>
#include <vector>

namespace {

void Require(bool value) {
    if (!value) std::abort();
}

std::vector<unsigned char> SymbolOnlyFont(std::uint32_t codepoint, int width) {
    std::vector<unsigned char> head;
    Put32(head, 0x00010000u);
    Put32(head, 0x00010000u);
    Put32(head, 0);
    Put32(head, 0x5F0F3CF5u);
    PutAll16(head, {0x000B, 1000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, width, 800, 0, 8, 2, 0, 0});
    std::vector<unsigned char> hhea;
    Put32(hhea, 0x00010000u);
    PutAll16(hhea, {800, -200, 0, width, 0, 0, width, 1, 0, 0, 0, 0, 0, 0, 0, 2});
    std::vector<unsigned char> maxp;
    Put32(maxp, 0x00010000u);
    PutAll16(maxp, {2, 4, 1, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0});
    std::vector<unsigned char> hmtx;
    PutAll16(hmtx, {width, 0, width, 100});
    std::vector<unsigned char> glyf;
    PutAll16(glyf, {1, 100, 0, width - 100, 700, 3, 0});
    glyf.insert(glyf.end(), {1, 1, 1, 1});
    PutAll16(glyf, {100, 0, width - 200, 0, 0, 700, 0, -700});
    std::vector<unsigned char> loca;
    PutAll16(loca, {0, 0, static_cast<int>(glyf.size() / 2)});
    std::vector<unsigned char> cmap;
    PutAll16(cmap, {0, 1, 3, 0});
    Put32(cmap, 12);
    const int symbol = static_cast<int>(codepoint + 0xF000);
    PutAll16(cmap, {4, 32, 0, 4, 4, 1, 0, symbol, 0xFFFF, 0, symbol, 0xFFFF, 1 - symbol, 1, 0, 0});
    std::vector<std::pair<const char*, const std::vector<unsigned char>*>> tables = {
        {"cmap", &cmap}, {"glyf", &glyf}, {"head", &head}, {"hhea", &hhea}, {"hmtx", &hmtx}, {"loca", &loca}, {"maxp", &maxp}};
    std::vector<unsigned char> font;
    Put32(font, 0x00010000u);
    PutAll16(font, {static_cast<int>(tables.size()), 64, 2, static_cast<int>(tables.size() * 16 - 64)});
    std::uint32_t offset = 12 + static_cast<std::uint32_t>(tables.size()) * 16;
    for (const auto& [tag, data] : tables) {
        font.insert(font.end(), tag, tag + 4);
        Put32(font, 0);
        Put32(font, offset);
        Put32(font, static_cast<std::uint32_t>(data->size()));
        offset += (static_cast<std::uint32_t>(data->size()) + 3u) & ~3u;
    }
    for (const auto& [tag, data] : tables) {
        font.insert(font.end(), data->begin(), data->end());
        font.resize((font.size() + 3u) & ~std::size_t{3});
    }
    return font;
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

    const auto symbolOnly = SymbolOnlyFont('A', 700);
    FreeType::GuestFaceRec* symbolFace = nullptr;
    Require(FT_New_Memory_Face_nid_postfix(library, symbolOnly.data(), static_cast<std::int64_t>(symbolOnly.size()), 0, &symbolFace) == 0);
    Require(symbolFace->num_charmaps == 1);
    Require(symbolFace->charmap == nullptr);
    Require(FcFreeTypeCharIndex(symbolFace, 'A') == 1);
    Require(symbolFace->charmap != nullptr && symbolFace->charmap->encoding == FT_ENCODING_MS_SYMBOL);
    Require(FcFreeTypeCharIndex(symbolFace, 0xf041) == 1);
    Require(FcFreeTypeCharIndex(symbolFace, 0x100) == 0);
    Require(FcFreeTypeCharIndex(symbolFace, 'B') == 0);
    Require(FT_Done_Face_nid_postfix(symbolFace) == 0);

    Require(FT_Done_FreeType_nid_postfix(library) == 0);
    std::puts("fontconfig FreeType char index passed");
}
