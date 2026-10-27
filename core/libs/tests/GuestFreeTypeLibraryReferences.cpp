#include "FontFixture.hpp"
#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_MODULE_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace FreeType;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType library reference check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static void OpenFace(GuestLibrary* library, FT_Library native, const std::vector<unsigned char>& font) {
    GuestFaceRec* guest = nullptr;
    FT_Face expected = nullptr;
    Require(FT_New_Memory_Face_nid_postfix(library, font.data(), static_cast<std::int64_t>(font.size()), 0, &guest) == 0);
    Require(FT_New_Memory_Face(native, font.data(), static_cast<FT_Long>(font.size()), 0, &expected) == 0);
    Require(guest->num_glyphs == expected->num_glyphs);
    Require(FT_Done_Face_nid_postfix(guest) == 0 && FT_Done_Face(expected) == 0);
}

static void TestDone() {
    const std::vector<unsigned char> font = FontFixture::BuildFont(3, {});
    GuestLibrary* guest = nullptr;
    FT_Library native = nullptr;
    Require(FT_Init_FreeType_nid_postfix(&guest) == 0 && FT_Init_FreeType(&native) == 0);
    OpenFace(guest, native, font);
    GuestFaceRec* kept = nullptr;
    FT_Face nativeKept = nullptr;
    Require(FT_New_Memory_Face_nid_postfix(guest, font.data(), static_cast<std::int64_t>(font.size()), 0, &kept) == 0);
    Require(FT_New_Memory_Face(native, font.data(), static_cast<FT_Long>(font.size()), 0, &nativeKept) == 0);
    Require(FT_Done_Library_nid_postfix(guest) == FT_Done_Library(native));
    Require(FT_Done_Library_nid_postfix(nullptr) == FT_Done_Library(nullptr));
}

static void TestReference() {
    const std::vector<unsigned char> font = FontFixture::BuildFont(3, {});
    GuestLibrary* guest = nullptr;
    FT_Library native = nullptr;
    Require(FT_Init_FreeType_nid_postfix(&guest) == 0 && FT_Init_FreeType(&native) == 0);
    for (int extra = 1; extra <= 3; ++extra) {
        Require(FT_Reference_Library_nid_postfix(guest) == FT_Reference_Library(native));
    }
    for (int round = 0; round < 3; ++round) {
        Require(FT_Done_Library_nid_postfix(guest) == FT_Done_Library(native));
        OpenFace(guest, native, font);
    }
    Require(FT_Done_Library_nid_postfix(guest) == FT_Done_Library(native));
    Require(FT_Reference_Library_nid_postfix(nullptr) == FT_Reference_Library(nullptr));
}

int main() {
    TestDone();
    TestReference();
}
