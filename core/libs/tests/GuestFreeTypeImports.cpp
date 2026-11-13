#include "FreeTypeFontFixture.hpp"
#include "prx/libSceFreeType/include/GuestFreeType.hpp"
#include <nid/NidCompute.hpp>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <string>

using namespace FreeType;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType import check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

template <typename TFunction>
static TFunction Import(void* module, const char* name, bool patched) {
    const auto symbol = patched ? Nid::ComputeNid(name, "libSceFreeType") : std::string(name) + "_nid_postfix";
    auto address = dlsym(module, symbol.c_str());
    Require(address != nullptr);
    return reinterpret_cast<TFunction>(address);
}

int main(int argc, char** argv) {
    Require(argc == 3);
    Require(std::strcmp(argv[2], "unpatched") == 0 || std::strcmp(argv[2], "patched") == 0);
    const bool patched = std::strcmp(argv[2], "patched") == 0;
    void* module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL | RTLD_FIRST);
    if (!module) std::fprintf(stderr, "%s\n", dlerror());
    Require(module != nullptr);
    for (const char* name : {"FT_Init_FreeType", "FT_New_Memory_Face", "FT_Load_Glyph", "FT_Render_Glyph", "FT_Outline_Translate"}) {
        Require(dlsym(module, name) == nullptr);
    }
    const auto initialize = Import<int (APS5_VABI *)(void**)>(module, "FT_Init_FreeType", patched);
    const auto doneLibrary = Import<int (APS5_VABI *)(void*)>(module, "FT_Done_FreeType", patched);
    const auto newFace = Import<int (APS5_VABI *)(void*, const unsigned char*, std::int64_t, std::int64_t, GuestFaceRec**)>(module, "FT_New_Memory_Face", patched);
    const auto doneFace = Import<int (APS5_VABI *)(GuestFaceRec*)>(module, "FT_Done_Face", patched);
    const auto setSize = Import<int (APS5_VABI *)(GuestFaceRec*, std::uint32_t, std::uint32_t)>(module, "FT_Set_Pixel_Sizes", patched);
    const auto charIndex = Import<std::uint32_t (APS5_VABI *)(GuestFaceRec*, std::uint64_t)>(module, "FT_Get_Char_Index", patched);
    const auto loadGlyph = Import<int (APS5_VABI *)(GuestFaceRec*, std::uint32_t, std::int32_t)>(module, "FT_Load_Glyph", patched);
    const auto renderGlyph = Import<int (APS5_VABI *)(GuestGlyphSlotRec*, std::int32_t)>(module, "FT_Render_Glyph", patched);

    const auto font = SquareGlyphFont('A', 700);
    FT_Library nativeLibrary = nullptr;
    FT_Face nativeFace = nullptr;
    Require(FT_Init_FreeType(&nativeLibrary) == 0);
    Require(FT_New_Memory_Face(nativeLibrary, font.data(), static_cast<FT_Long>(font.size()), 0, &nativeFace) == 0);
    void* library = nullptr;
    GuestFaceRec* face = nullptr;
    Require(initialize(&library) == 0);
    Require(newFace(library, font.data(), static_cast<std::int64_t>(font.size()), 0, &face) == 0);
    Require(face->driver == nullptr && face->memory == nullptr && face->stream == nullptr);
    Require(face->glyph->library == library && face->glyph->face == face && face->size->face == face);
    Require(charIndex(face, 'A') == 1 && charIndex(face, 'B') == 0);

    for (const std::uint32_t height : {16u, 37u, 100u}) {
        Require(setSize(face, 0, height) == 0 && FT_Set_Pixel_Sizes(nativeFace, 0, height) == 0);
        Require(face->size->metrics.y_ppem == nativeFace->size->metrics.y_ppem);
        Require(loadGlyph(face, 1, FT_LOAD_DEFAULT) == 0 && FT_Load_Glyph(nativeFace, 1, FT_LOAD_DEFAULT) == 0);
        Require(face->glyph->outline.n_points == 4 && face->glyph->outline.n_contours == 1);
        Require(face->glyph->advance.x == nativeFace->glyph->advance.x);
        for (unsigned index = 0; index < face->glyph->outline.n_points; ++index) face->glyph->outline.points[index].x += 64;
        FT_Outline_Translate(&nativeFace->glyph->outline, 64, 0);
        Require(renderGlyph(face->glyph, FT_RENDER_MODE_NORMAL) == 0 && FT_Render_Glyph(nativeFace->glyph, FT_RENDER_MODE_NORMAL) == 0);
        const auto& bitmap = face->glyph->bitmap;
        const auto& expected = nativeFace->glyph->bitmap;
        Require(face->glyph->format == FT_GLYPH_FORMAT_BITMAP && bitmap.pixel_mode == FT_PIXEL_MODE_GRAY);
        Require(bitmap.rows == expected.rows && bitmap.width == expected.width && bitmap.pitch == expected.pitch);
        Require(face->glyph->bitmap_left == nativeFace->glyph->bitmap_left && face->glyph->bitmap_top == nativeFace->glyph->bitmap_top);
        Require(bitmap.rows != 0 && bitmap.width != 0 && bitmap.pitch > 0);
        for (unsigned row = 0; row < bitmap.rows; ++row) {
            Require(std::memcmp(bitmap.buffer + row * bitmap.pitch, expected.buffer + row * expected.pitch, bitmap.width) == 0);
        }
    }

    Require(doneFace(face) == 0 && doneLibrary(library) == 0);
    Require(FT_Done_Face(nativeFace) == 0 && FT_Done_FreeType(nativeLibrary) == 0);
    Require(dlclose(module) == 0);
}
