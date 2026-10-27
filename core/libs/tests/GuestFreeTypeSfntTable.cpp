#include "FontFixture.hpp"
#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_TRUETYPE_TABLES_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

using namespace FreeType;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType sfnt table check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

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

    void Compare(std::int32_t tag, std::size_t size, bool present) const {
        void* actual = FT_Get_Sfnt_Table_nid_postfix(const_cast<GuestFaceRec*>(&guest.rec), tag);
        void* expected = FT_Get_Sfnt_Table(oracle, static_cast<FT_Sfnt_Tag>(tag));
        Require((actual != nullptr) == (expected != nullptr));
        Require((actual != nullptr) == present);
        Require(actual == FT_Get_Sfnt_Table(guest.host, static_cast<FT_Sfnt_Tag>(tag)));
        if (actual) Require(std::memcmp(actual, expected, size) == 0);
    }
};

int main() {
    GuestLibrary library{};
    FT_Library nativeLibrary = nullptr;
    Require(FT_Init_FreeType(&library.host) == 0 && FT_Init_FreeType(&nativeLibrary) == 0);
    {
        std::vector<unsigned char> os2(78, 0);
        os2[8] = 0x01;
        os2[9] = 0x04;
        os2[68] = 0x03;
        std::vector<unsigned char> post = FontFixture::Words({3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});
        FacePair face(library, nativeLibrary, FontFixture::BuildFont(3, {{"OS/2", os2}, {"post", post}}));
        if constexpr (sizeof(FT_Long) == sizeof(std::int64_t)) {
            face.Compare(FT_SFNT_HEAD, sizeof(TT_Header), true);
            face.Compare(FT_SFNT_HHEA, sizeof(TT_HoriHeader), true);
            face.Compare(FT_SFNT_MAXP, sizeof(TT_MaxProfile), true);
            face.Compare(FT_SFNT_OS2, sizeof(TT_OS2), true);
            face.Compare(FT_SFNT_POST, sizeof(TT_Postscript), true);
            face.Compare(FT_SFNT_VHEA, sizeof(TT_VertHeader), false);
            face.Compare(FT_SFNT_PCLT, sizeof(TT_PCLT), false);
            face.Compare(FT_SFNT_MAX, 0, false);
            face.Compare(-1, 0, false);
            FacePair bitmap(library, nativeLibrary, FontFixture::BitmapFont());
            bitmap.Compare(FT_SFNT_HEAD, sizeof(TT_Header), false);
            bitmap.Compare(FT_SFNT_OS2, sizeof(TT_OS2), false);
        }
    }
    Require(FT_Get_Sfnt_Table_nid_postfix(nullptr, FT_SFNT_HEAD) == FT_Get_Sfnt_Table(nullptr, FT_SFNT_HEAD));
    GuestFaceRec empty{};
    Require(FT_Get_Sfnt_Table_nid_postfix(&empty, FT_SFNT_HEAD) == nullptr);
    Require(FT_Done_FreeType(library.host) == 0 && FT_Done_FreeType(nativeLibrary) == 0);
}
