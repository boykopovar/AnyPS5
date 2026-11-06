#include "prx/libSceFreeTypeOl/libSceFreeTypeOl.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "FreeTypeFontFixture.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>

using FreeType::GuestMemoryRec;
using FreeType::GuestLibrary;
using FreeType::GuestFaceRec;

static void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "FreeTypeOl module check failed at line %d\n", line);
        std::abort();
    }
}

#define REQUIRE(condition) Require((condition), __LINE__)

static void* APS5_VABI Allocate(GuestMemoryRec*, std::int64_t size) {
    return std::malloc(static_cast<std::size_t>(size));
}

static void APS5_VABI Release(GuestMemoryRec*, void* block) {
    std::free(block);
}

int main() {
    GuestLibrary* library = nullptr;
    REQUIRE(FT_Init_FreeType_nid_postfix(&library) == 0);
    REQUIRE(library != nullptr && library->host != nullptr);
    FT_Library native = library->host;

    struct ModuleCase {
        const char* name;
        void (APS5_VABI* add)(void*);
        int (APS5_VABI* remove)(void*);
    };
    const ModuleCase cases[] = {
        {"autofitter", sceFreeTypeOlFtaAddModuleAutofitter_nid_postfix, sceFreeTypeOlFtaRemoveModuleAutofitter_nid_postfix},
        {"bdf", sceFreeTypeOlFtaAddModuleBdf_nid_postfix, sceFreeTypeOlFtaRemoveModuleBdf_nid_postfix},
        {"cff", sceFreeTypeOlFtaAddModuleCff_nid_postfix, sceFreeTypeOlFtaRemoveModuleCff_nid_postfix},
        {"pcf", sceFreeTypeOlFtaAddModulePcf_nid_postfix, sceFreeTypeOlFtaRemoveModulePcf_nid_postfix},
        {"pfr", sceFreeTypeOlFtaAddModulePfr_nid_postfix, sceFreeTypeOlFtaRemoveModulePfr_nid_postfix},
        {"psaux", sceFreeTypeOlFtaAddModulePsaux_nid_postfix, sceFreeTypeOlFtaRemoveModulePsaux_nid_postfix},
        {"pshinter", sceFreeTypeOlFtaAddModulePshinter_nid_postfix, sceFreeTypeOlFtaRemoveModulePshinter_nid_postfix},
        {"psnames", sceFreeTypeOlFtaAddModulePsnames_nid_postfix, sceFreeTypeOlFtaRemoveModulePsnames_nid_postfix},
        {"raster1", sceFreeTypeOlFtaAddModuleRaster1_nid_postfix, sceFreeTypeOlFtaRemoveModuleRaster1_nid_postfix},
        {"sfnt", sceFreeTypeOlFtaAddModuleSfnt_nid_postfix, sceFreeTypeOlFtaRemoveModuleSfnt_nid_postfix},
        {"smooth", sceFreeTypeOlFtaAddModuleSmooth_nid_postfix, sceFreeTypeOlFtaRemoveModuleSmooth_nid_postfix},
        {"t1cid", sceFreeTypeOlFtaAddModuleT1cid_nid_postfix, sceFreeTypeOlFtaRemoveModuleT1cid_nid_postfix},
        {"truetype", sceFreeTypeOlFtaAddModuleTruetype_nid_postfix, sceFreeTypeOlFtaRemoveModuleTruetype_nid_postfix},
        {"type1", sceFreeTypeOlFtaAddModuleType1_nid_postfix, sceFreeTypeOlFtaRemoveModuleType1_nid_postfix},
        {"type42", sceFreeTypeOlFtaAddModuleType42_nid_postfix, sceFreeTypeOlFtaRemoveModuleType42_nid_postfix},
        {"winfonts", sceFreeTypeOlFtaAddModuleWinfonts_nid_postfix, sceFreeTypeOlFtaRemoveModuleWinfonts_nid_postfix},
        {"gxvalid", sceFreeTypeOlFtaAddModuleGxvalid_nid_postfix, sceFreeTypeOlFtaRemoveModuleGxvalid_nid_postfix},
        {"otvalid", sceFreeTypeOlFtaAddModuleOtvalid_nid_postfix, sceFreeTypeOlFtaRemoveModuleOtvalid_nid_postfix},
    };

    GuestLibrary invalidLibrary{};
    REQUIRE(sceFreeTypeOlFtaRemoveModuleAutofitter_nid_postfix(nullptr) == 0x21);
    REQUIRE(sceFreeTypeOlFtaRemoveModuleAutofitter_nid_postfix(&invalidLibrary) == 0x21);
    REQUIRE(sceFreeTypeOlFtaSupportFormatBdf_nid_postfix(nullptr) == 0x21);
    REQUIRE(sceFreeTypeOlFtaSupportFormatBdf_nid_postfix(&invalidLibrary) == 0x21);

    for (const auto& entry : cases) {
        REQUIRE(entry.remove(library) == 0);
        REQUIRE(FT_Get_Module(native, entry.name) == nullptr);
        entry.add(library);
        REQUIRE(FT_Get_Module(native, entry.name) != nullptr);
        REQUIRE(entry.remove(library) == 0);
        REQUIRE(FT_Get_Module(native, entry.name) == nullptr);
        entry.add(library);
        REQUIRE(FT_Get_Module(native, entry.name) != nullptr);
    }

    REQUIRE(sceFreeTypeOlFtaSupportFormatBdf_nid_postfix(library) == 0);
    REQUIRE(sceFreeTypeOlFtaSupportFormatPcf_nid_postfix(library) == 0);
    REQUIRE(sceFreeTypeOlFtaSupportFormatPfr_nid_postfix(library) == 0);
    REQUIRE(sceFreeTypeOlFtaSupportFormatWinFonts_nid_postfix(library) == 0);
    REQUIRE(sceFreeTypeOlFtaSupportHinterAutoFitter_nid_postfix(library) == 0);
    REQUIRE(sceFreeTypeOlFtaSupportRendererRaster1_nid_postfix(library) == 0);
    REQUIRE(sceFreeTypeOlFtaSupportRendererSmooth_nid_postfix(library) == 0);

    GuestMemoryRec memory{};
    memory.alloc = Allocate;
    memory.free = Release;
    void* block = nullptr;
    REQUIRE(sceFreeTypeOlAlloc_nid_postfix(&memory, 24, &block) == 0);
    REQUIRE(block != nullptr);
    sceFreeTypeOlFree_nid_postfix(&memory, &block);
    REQUIRE(block == nullptr);
    REQUIRE(sceFreeTypeOlAlloc_nid_postfix(&memory, -1, &block) == 6);
    REQUIRE(block == nullptr);

    const auto font = SquareGlyphFont('A', 700, true);
    GuestFaceRec* activeFace = nullptr;
    REQUIRE(FT_New_Memory_Face_nid_postfix(library, font.data(), font.size(), 0, &activeFace) == 0);
    REQUIRE(activeFace != nullptr);
    REQUIRE(FT_Get_Module(native, "truetype") != nullptr);
    REQUIRE(sceFreeTypeOlFtaRemoveModuleTruetype_nid_postfix(library) == 6);
    REQUIRE(FT_Get_Module(native, "truetype") != nullptr);
    REQUIRE(FT_Select_Charmap_nid_postfix(activeFace, FT_ENCODING_UNICODE) == 0);
    REQUIRE(FT_Get_Char_Index_nid_postfix(activeFace, 'A') != 0);
    REQUIRE(FT_Done_Face_nid_postfix(activeFace) == 0);
    REQUIRE(sceFreeTypeOlFtaRemoveModuleTruetype_nid_postfix(library) == 0);
    REQUIRE(FT_Get_Module(native, "truetype") == nullptr);
    sceFreeTypeOlFtaAddModuleTruetype_nid_postfix(library);
    REQUIRE(FT_Get_Module(native, "truetype") != nullptr);

    GuestFaceRec* face = nullptr;
    REQUIRE(FT_New_Memory_Face_nid_postfix(library, font.data(), font.size(), 0, &face) == 0);
    REQUIRE(face != nullptr && face->glyph != nullptr);
    REQUIRE(FT_Select_Charmap_nid_postfix(face, FT_ENCODING_UNICODE) == 0);
    REQUIRE(FT_Get_Char_Index_nid_postfix(face, 'A') != 0);
    sceFreeTypeOlDoneGlyphSlot_nid_postfix(nullptr);
    sceFreeTypeOlDoneGlyphSlot_nid_postfix(face->glyph);
    REQUIRE(face->glyph == nullptr);
    REQUIRE(FT_Done_Face_nid_postfix(face) == 0);

    REQUIRE(FT_Done_FreeType_nid_postfix(library) == 0);
}
