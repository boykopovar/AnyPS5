#include "FreeTypeFontFixture.hpp"
#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <random>
#include <string>
#include <utility>
#include <vector>

using namespace FreeType;

extern "C" {
int APS5_VABI FT_Init_FreeType_nid_postfix(void**);
int APS5_VABI FT_New_Face_nid_postfix(void*, const char*, std::int64_t, GuestFaceRec**);
std::uint32_t APS5_VABI FT_Get_Char_Index_nid_postfix(GuestFaceRec*, std::uint64_t);
int APS5_VABI FT_Load_Glyph_nid_postfix(GuestFaceRec*, std::uint32_t, std::int32_t);
int APS5_VABI FT_Select_Charmap_nid_postfix(GuestFaceRec*, std::int32_t);
int APS5_VABI FT_Set_Charmap_nid_postfix(GuestFaceRec*, GuestCharMapRec*);
std::uint64_t APS5_VABI FT_Get_First_Char_nid_postfix(GuestFaceRec*, std::uint32_t*);
std::uint64_t APS5_VABI FT_Get_Next_Char_nid_postfix(GuestFaceRec*, std::uint64_t, std::uint32_t*);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType charmap check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

static std::vector<std::pair<std::uint64_t, std::uint32_t>> Walk(GuestFaceRec* guest, FT_Face host) {
    std::vector<std::pair<std::uint64_t, std::uint32_t>> result;
    std::uint32_t guestIndex = 999;
    FT_UInt hostIndex = 999;
    std::uint64_t guestCode = FT_Get_First_Char_nid_postfix(guest, &guestIndex);
    FT_ULong hostCode = FT_Get_First_Char(host, &hostIndex);
    for (int iteration = 0; iteration < 4; ++iteration) {
        Require(guestCode == hostCode && guestIndex == hostIndex);
        if (guestIndex == 0) return result;
        result.emplace_back(guestCode, guestIndex);
        guestCode = FT_Get_Next_Char_nid_postfix(guest, guestCode, &guestIndex);
        hostCode = FT_Get_Next_Char(host, hostCode, &hostIndex);
    }
    std::abort();
}

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("anyps5-freetype-charmap-" + std::to_string(std::random_device{}()));
    Require(std::filesystem::create_directory(root));
    std::filesystem::create_directory(root / "app0");
    const auto font = SquareGlyphFont('A', 700, true);
    const auto zeroFont = SquareGlyphFont(0, 700);
    for (const auto& [name, bytes] : {std::pair{"maps.ttf", &font}, std::pair{"zero.ttf", &zeroFont}}) {
        std::ofstream file(root / "app0" / name, std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes->data()), static_cast<std::streamsize>(bytes->size()));
        Require(static_cast<bool>(file));
    }
    const auto previous = std::filesystem::current_path();
    std::filesystem::current_path(root);
    FT_Library native = nullptr;
    FT_Face host = nullptr;
    FT_Face foreignHost = nullptr;
    FT_Face zeroHost = nullptr;
    Require(FT_Init_FreeType(&native) == 0);
    Require(FT_New_Memory_Face(native, font.data(), static_cast<FT_Long>(font.size()), 0, &host) == 0);
    Require(FT_New_Memory_Face(native, font.data(), static_cast<FT_Long>(font.size()), 0, &foreignHost) == 0);
    Require(FT_New_Memory_Face(native, zeroFont.data(), static_cast<FT_Long>(zeroFont.size()), 0, &zeroHost) == 0);
    void* library = nullptr;
    GuestFaceRec* face = nullptr;
    GuestFaceRec* foreign = nullptr;
    GuestFaceRec* zero = nullptr;
    Require(FT_Init_FreeType_nid_postfix(&library) == 0);
    Require(FT_New_Face_nid_postfix(library, "/app0/maps.ttf", 0, &face) == 0);
    Require(FT_New_Face_nid_postfix(library, "/app0/maps.ttf", 0, &foreign) == 0);
    Require(FT_New_Face_nid_postfix(library, "/app0/zero.ttf", 0, &zero) == 0);
    Require(face->num_charmaps == 2 && host->num_charmaps == 2);
    const auto unicode = Walk(face, host);
    Require(unicode.size() == 1 && unicode[0].first == 'A' && unicode[0].second == 1);
    const auto zeroChars = Walk(zero, zeroHost);
    Require(zeroChars.size() == 1 && zeroChars[0].first == 0 && zeroChars[0].second == 1);

    std::uint32_t index = 999;
    Require(FT_Get_First_Char_nid_postfix(nullptr, &index) == 0 && index == 0);
    index = 999;
    Require(FT_Get_Next_Char_nid_postfix(nullptr, 0, &index) == 0 && index == 0);
    Require(FT_Get_First_Char_nid_postfix(face, nullptr) == FT_Get_First_Char(host, nullptr));
    Require(FT_Get_Next_Char_nid_postfix(face, 0, nullptr) == FT_Get_Next_Char(host, 0, nullptr));
    Require(FT_Select_Charmap_nid_postfix(nullptr, FT_ENCODING_UNICODE) == FT_Err_Invalid_Face_Handle);
    Require(FT_Set_Charmap_nid_postfix(nullptr, face->charmaps[0]) == FT_Err_Invalid_Face_Handle);
    Require(FT_Load_Glyph_nid_postfix(face, 1, FT_LOAD_NO_SCALE) == 0);
    Require(face->glyph->outline.n_points == 4);
    auto* points = face->glyph->outline.points;
    const auto changedX = ++points[0].x;

    for (const FT_Encoding encoding : {FT_ENCODING_MS_SYMBOL, FT_ENCODING_UNICODE}) {
        Require(FT_Select_Charmap_nid_postfix(face, encoding) == FT_Select_Charmap(host, encoding));
        Require(face->charmap->encoding == static_cast<std::int32_t>(host->charmap->encoding));
        Require(face->glyph->outline.points == points && points[0].x == changedX);
        const auto characters = Walk(face, host);
        Require(characters.size() == 1 && characters[0].first == (encoding == FT_ENCODING_UNICODE ? 'A' : 0xF041));
        Require(FT_Get_Char_Index_nid_postfix(face, characters[0].first) == characters[0].second);
        for (const std::uint64_t code : std::array<std::uint64_t, 7>{0, 64, 65, 0xF041, 0xFFFFFFFF, 0x100000040, std::numeric_limits<std::uint64_t>::max()}) {
            std::uint32_t guestIndex = 999;
            FT_UInt hostIndex = 999;
            const auto actual = FT_Get_Next_Char_nid_postfix(face, code, &guestIndex);
            const auto expected = FT_Get_Next_Char(host, static_cast<FT_ULong>(code), &hostIndex);
            Require(actual == expected && guestIndex == hostIndex);
        }
        const auto selected = face->charmap;
        Require(FT_Select_Charmap_nid_postfix(face, FT_ENCODING_NONE) == FT_Select_Charmap(host, FT_ENCODING_NONE));
        Require(face->charmap == selected);
        Require(FT_Set_Charmap_nid_postfix(face, nullptr) == FT_Set_Charmap(host, nullptr));
        Require(face->charmap == selected);
        Require(FT_Set_Charmap_nid_postfix(face, foreign->charmaps[0]) == FT_Set_Charmap(host, foreignHost->charmaps[0]));
        Require(face->charmap == selected);
        FT_CharMapRec fakeHost{};
        GuestCharMapRec fakeGuest{};
        Require(FT_Set_Charmap_nid_postfix(face, &fakeGuest) == FT_Set_Charmap(host, &fakeHost));
        Require(face->charmap == selected);
    }
    for (int map = 1; map >= 0; --map) {
        Require(FT_Set_Charmap_nid_postfix(face, face->charmaps[map]) == 0 && FT_Set_Charmap(host, host->charmaps[map]) == 0);
        Require(face->charmap == face->charmaps[map]);
        Require(face->glyph->outline.points == points && points[0].x == changedX);
        Require(Walk(face, host).size() == 1);
    }

    Require(FT_Done_FreeType(native) == 0);
    std::filesystem::current_path(previous);
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
}
