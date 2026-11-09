#include "prx/libSceFreeType/include/Api.hpp"
#include "FontFixture.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_TRUETYPE_TABLES_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

using namespace FreeType;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType charmap info check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static std::vector<unsigned char> Font(std::uint16_t language4, std::uint32_t language12) {
    auto cmap = FontFixture::Words({0, 2, 3, 1, 0, 20, 3, 10, 0, 52});
    const auto format4 = FontFixture::Words({4, 32, language4, 4, 4, 1, 0, 'A', 0xFFFF, 0, 'A', 0xFFFF, 1 - 'A', 1, 0, 0});
    const auto format12 = FontFixture::Words({12, 0, 0, 28, static_cast<int>(language12 >> 16), static_cast<int>(language12 & 0xFFFF),
                                             0, 1, 1, 0xF600, 1, 0xF600, 0, 2});
    cmap.insert(cmap.end(), format4.begin(), format4.end());
    cmap.insert(cmap.end(), format12.begin(), format12.end());
    return FontFixture::BuildFont(3, {{"cmap", cmap}});
}

struct FacePair {
    std::vector<unsigned char> bytes;
    GuestFace guest;
    FT_Face oracle = nullptr;

    FacePair(GuestLibrary& library, FT_Library nativeLibrary, std::vector<unsigned char> font) : bytes(std::move(font)) {
        guest.library = &library;
        Require(FT_New_Memory_Face(library.host, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &guest.host) == 0);
        Require(FT_New_Memory_Face(nativeLibrary, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &oracle) == 0);
        SyncFace(guest);
    }

    ~FacePair() {
        Require(FT_Done_Face(guest.host) == 0);
        Require(FT_Done_Face(oracle) == 0);
    }
};

static void Compare(FacePair& face, std::uint16_t language4, std::uint32_t language12) {
    Require(face.guest.rec.num_charmaps == 2 && face.oracle->num_charmaps == 2);
    const auto selected = face.guest.rec.charmap;
    const auto nativeSelected = face.oracle->charmap;
    for (int index = 0; index < face.guest.rec.num_charmaps; ++index) {
        GuestCharMapRec* guest = face.guest.rec.charmaps[index];
        FT_CharMap native = face.oracle->charmaps[index];
        Require(FT_Get_Charmap_Index_nid_postfix(guest) == index);
        Require(FT_Get_Charmap_Index_nid_postfix(guest) == FT_Get_Charmap_Index(native));
        const auto format = FT_Get_CMap_Format_nid_postfix(guest);
        Require(format == FT_Get_CMap_Format(native) && (format == 4 || format == 12));
        const auto language = FT_Get_CMap_Language_ID_nid_postfix(guest);
        Require(language == FT_Get_CMap_Language_ID(native));
        Require(language == (format == 4 ? language4 : language12));
    }
    Require(face.guest.rec.charmap == selected && face.oracle->charmap == nativeSelected);
}

template <typename TCall> static bool Unsupported(TCall call) {
    try {
        call();
    } catch (const std::runtime_error& error) {
        return std::string_view(error.what()).starts_with("FreeType: a charmap outside");
    }
    return false;
}

int main() {
    GuestLibrary library{};
    FT_Library nativeLibrary = nullptr;
    Require(FT_Init_FreeType(&library.host) == 0);
    Require(FT_Init_FreeType(&nativeLibrary) == 0);
    {
        FacePair first(library, nativeLibrary, Font(0x0411, 0x80000001u));
        FacePair second(library, nativeLibrary, Font(0x0412, 0xFFFFFFFFu));
        Compare(first, 0x0411, 0x80000001u);
        Compare(second, 0x0412, 0xFFFFFFFFu);
        for (int index = 0; index < first.guest.rec.num_charmaps; ++index) {
            Require(FT_Set_Charmap_nid_postfix(&first.guest.rec, first.guest.rec.charmaps[index]) == 0);
            Require(FT_Set_Charmap(first.oracle, first.oracle->charmaps[index]) == 0);
            Compare(first, 0x0411, 0x80000001u);
            Compare(second, 0x0412, 0xFFFFFFFFu);
        }
        GuestCharMapRec copied = *first.guest.rec.charmaps[0];
        Require(Unsupported([&] { FT_Get_Charmap_Index_nid_postfix(&copied); }));
        Require(Unsupported([&] { FT_Get_CMap_Language_ID_nid_postfix(&copied); }));
        Require(Unsupported([&] { FT_Get_CMap_Format_nid_postfix(&copied); }));
        FacePair bitmap(library, nativeLibrary, FontFixture::BitmapFont());
        Require(!FT_IS_SFNT(bitmap.oracle) && bitmap.guest.rec.num_charmaps == 1);
        GuestCharMapRec* bitmapMap = bitmap.guest.rec.charmaps[0];
        Require(FT_Get_Charmap_Index_nid_postfix(bitmapMap) == FT_Get_Charmap_Index(bitmap.oracle->charmaps[0]));
        Require(FT_Get_CMap_Language_ID_nid_postfix(bitmapMap) == FT_Get_CMap_Language_ID(bitmap.oracle->charmaps[0]));
        Require(FT_Get_CMap_Language_ID_nid_postfix(bitmapMap) == 0);
        Require(FT_Get_CMap_Format_nid_postfix(bitmapMap) == FT_Get_CMap_Format(bitmap.oracle->charmaps[0]));
        Require(FT_Get_CMap_Format_nid_postfix(bitmapMap) == -1);
        GuestCharMapRec empty{};
        FT_CharMapRec nativeEmpty{};
        for (GuestCharMapRec* guest : {static_cast<GuestCharMapRec*>(nullptr), &empty}) {
            Require(FT_Get_Charmap_Index_nid_postfix(guest) == FT_Get_Charmap_Index(guest ? &nativeEmpty : nullptr));
            Require(FT_Get_CMap_Language_ID_nid_postfix(guest) == FT_Get_CMap_Language_ID(guest ? &nativeEmpty : nullptr));
            Require(FT_Get_CMap_Format_nid_postfix(guest) == FT_Get_CMap_Format(guest ? &nativeEmpty : nullptr));
        }
    }
    Require(FT_Done_FreeType(library.host) == 0);
    Require(FT_Done_FreeType(nativeLibrary) == 0);
}