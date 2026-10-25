#include "prx/libSceFreeType/include/Api.hpp"
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

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType face info check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static std::vector<unsigned char> Font(std::uint16_t flags) {
    std::vector<unsigned char> os2(78, 0);
    os2[8] = static_cast<unsigned char>(flags >> 8);
    os2[9] = static_cast<unsigned char>(flags);
    return FontFixture::BuildFont(3, {{"OS/2", os2}});
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

static const char* Query(FacePair& face, const char* format, std::uint16_t flags) {
    const char* actual = FT_Get_X11_Font_Format_nid_postfix(&face.guest.rec);
    const char* expected = FT_Get_X11_Font_Format(face.oracle);
    Require(actual && expected && std::strcmp(actual, expected) == 0 && std::strcmp(actual, format) == 0);
    Require(actual == FT_Get_X11_Font_Format(face.guest.host));
    Require(FT_Get_FSType_Flags_nid_postfix(&face.guest.rec) == FT_Get_FSType_Flags(face.oracle));
    Require(FT_Get_FSType_Flags_nid_postfix(&face.guest.rec) == flags);
    return actual;
}

int main() {
    GuestLibrary library{};
    FT_Library nativeLibrary = nullptr;
    Require(FT_Init_FreeType(&library.host) == 0 && FT_Init_FreeType(&nativeLibrary) == 0);
    std::vector<const char*> retained;
    const char* bitmapFormat = nullptr;
    {
        FacePair bitmap(library, nativeLibrary, FontFixture::BitmapFont());
        bitmapFormat = Query(bitmap, "BDF", 0);
        for (const std::uint16_t flags : {0, 2, 4, 8, 0x0100, 0x0200, 0x0308, 0x8000, 0xFFFF}) {
            FacePair face(library, nativeLibrary, Font(flags));
            retained.push_back(Query(face, "TrueType", flags));
            Require(Query(bitmap, "BDF", 0) == bitmapFormat);
        }
        FacePair missing(library, nativeLibrary, FontFixture::BuildFont(3, {}));
        Query(missing, "TrueType", 0);
        GuestFaceRec empty{};
        for (GuestFaceRec* face : {static_cast<GuestFaceRec*>(nullptr), &empty}) {
            Require(FT_Get_X11_Font_Format_nid_postfix(face) == FT_Get_X11_Font_Format(nullptr));
            Require(FT_Get_FSType_Flags_nid_postfix(face) == FT_Get_FSType_Flags(nullptr));
        }
    }
    Require(FT_Done_FreeType(library.host) == 0 && FT_Done_FreeType(nativeLibrary) == 0);
    for (const char* format : retained) Require(std::strcmp(format, "TrueType") == 0);
    Require(std::strcmp(bitmapFormat, "BDF") == 0);
}
