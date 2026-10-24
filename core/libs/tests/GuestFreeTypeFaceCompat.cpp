#include "FontFixture.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_FONT_FORMATS_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

using namespace FreeType;

extern "C" {
const char* APS5_VABI FT_Get_Font_Format_nid_postfix(GuestFaceRec*);
std::uint8_t APS5_VABI FT_Face_CheckTrueTypePatents_nid_postfix(GuestFaceRec*);
std::uint8_t APS5_VABI FT_Face_SetUnpatentedHinting_nid_postfix(GuestFaceRec*, std::uint8_t);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType face compat check failed at line %d\n", line);
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
};

static void Query(FacePair& face, const char* format) {
    const char* actual = FT_Get_Font_Format_nid_postfix(&face.guest.rec);
    const char* expected = FT_Get_Font_Format(face.oracle);
    Require(actual && expected && std::strcmp(actual, expected) == 0 && std::strcmp(actual, format) == 0);
    Require(actual == FT_Get_Font_Format(face.guest.host));
    Require(FT_Get_Font_Format_nid_postfix(&face.guest.rec) == FT_Get_X11_Font_Format(face.guest.host));
    Require(FT_Face_CheckTrueTypePatents_nid_postfix(&face.guest.rec) == FT_Face_CheckTrueTypePatents(face.oracle));
    Require(FT_Face_CheckTrueTypePatents_nid_postfix(&face.guest.rec) == 0);
    for (const std::uint8_t value : {0, 1, 0xFF}) {
        Require(FT_Face_SetUnpatentedHinting_nid_postfix(&face.guest.rec, value) == FT_Face_SetUnpatentedHinting(face.oracle, value));
        Require(FT_Face_SetUnpatentedHinting_nid_postfix(&face.guest.rec, value) == 0);
    }
}

int main() {
    GuestLibrary library{};
    FT_Library nativeLibrary = nullptr;
    Require(FT_Init_FreeType(&library.host) == 0 && FT_Init_FreeType(&nativeLibrary) == 0);
    {
        FacePair bitmap(library, nativeLibrary, FontFixture::BitmapFont());
        Query(bitmap, "BDF");
        FacePair outline(library, nativeLibrary, FontFixture::BuildFont(3, {}));
        Query(outline, "TrueType");
        GuestFaceRec empty{};
        for (GuestFaceRec* face : {static_cast<GuestFaceRec*>(nullptr), &empty}) {
            Require(FT_Get_Font_Format_nid_postfix(face) == FT_Get_Font_Format(nullptr));
            Require(FT_Face_CheckTrueTypePatents_nid_postfix(face) == 0);
            Require(FT_Face_SetUnpatentedHinting_nid_postfix(face, 1) == 0);
        }
    }
    Require(FT_Done_FreeType(library.host) == 0 && FT_Done_FreeType(nativeLibrary) == 0);
}
