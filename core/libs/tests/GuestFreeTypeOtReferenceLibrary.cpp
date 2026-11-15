#include "FreeTypeFontFixture.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libSceFreeTypeOt/libSceFreeTypeOt.hpp"
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include <freetype/internal/ftobjs.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

using FreeType::GuestFaceRec;
using FreeType::GuestLibrary;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libSceFreeTypeOt reference library: %s\n", message);
    std::exit(1);
}

}

int main() {
    const std::vector<unsigned char> bytes = SquareGlyphFont('A', 700);
    GuestLibrary* library = nullptr;
    Require(FT_Init_FreeType_nid_postfix(&library) == 0 && library && library->host, "initialize");
    Require(library->host->refcount == 1, "a new library holds one reference");
    Require(FT_Reference_Library_nid_postfix(library) == 0 && library->host->refcount == 2, "first reference");
    Require(FT_Reference_Library_nid_postfix(library) == 0 && library->host->refcount == 3, "second reference");
    Require(FT_Reference_Library_nid_postfix(nullptr) == FT_Err_Invalid_Library_Handle, "null library");
    Require(library->host->refcount == 3, "null library leaves the count");

    GuestFaceRec* face = nullptr;
    Require(FT_New_Memory_Face_nid_postfix(library, bytes.data(), static_cast<std::int64_t>(bytes.size()), 0, &face) == 0 && face, "open face");
    Require(FT_Done_Library(library->host) == 0 && library->host->refcount == 2, "second reference released");
    Require(FT_Done_Library(library->host) == 0 && library->host->refcount == 1, "first reference released");
    Require(FT_Set_Pixel_Sizes_nid_postfix(face, 24, 24) == 0 && FT_Load_Glyph_nid_postfix(face, 1, FT_LOAD_DEFAULT) == 0,
            "the library and its face outlive the released references");
    Require(FT_Done_Face_nid_postfix(face) == 0, "close face");
    Require(FT_Done_FreeType_nid_postfix(library) == 0, "release library");
    std::puts("libSceFreeTypeOt: FT_Reference_Library counts references as FreeType does");
    return 0;
}
