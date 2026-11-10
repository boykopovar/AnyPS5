#include "prx/libSceFreeTypeOl/libSceFreeTypeOl.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "FreeTypeFontFixture.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include <freetype/internal/ftobjs.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

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
    REQUIRE(sceFreeTypeOlFtaSupportFormatCid_nid_postfix(nullptr) == 0x21);
    REQUIRE(sceFreeTypeOlFtaSupportFormatCid_nid_postfix(&invalidLibrary) == 0x21);
    REQUIRE(sceFreeTypeOlFtaSupportFormatOpenType_nid_postfix(nullptr) == 0x21);
    REQUIRE(sceFreeTypeOlFtaSupportFormatOpenType_nid_postfix(&invalidLibrary) == 0x21);
    REQUIRE(sceFreeTypeOlFtaSupportFormatOpenTypeOtf_nid_postfix(nullptr) == 0x21);
    REQUIRE(sceFreeTypeOlFtaSupportFormatOpenTypeOtf_nid_postfix(&invalidLibrary) == 0x21);
    REQUIRE(sceFreeTypeOlFtaSupportFormatOpenTypeTtf_nid_postfix(nullptr) == 0x21);
    REQUIRE(sceFreeTypeOlFtaSupportFormatOpenTypeTtf_nid_postfix(&invalidLibrary) == 0x21);
    REQUIRE(sceFreeTypeOlFtaSupportFormatTrueType_nid_postfix(nullptr) == 0x21);
    REQUIRE(sceFreeTypeOlFtaSupportFormatTrueType_nid_postfix(&invalidLibrary) == 0x21);
    REQUIRE(sceFreeTypeOlFtaSupportFormatTrueTypeGx_nid_postfix(nullptr) == 0x21);
    REQUIRE(sceFreeTypeOlFtaSupportFormatTrueTypeGx_nid_postfix(&invalidLibrary) == 0x21);
    REQUIRE(sceFreeTypeOlFtaSupportFormatType1_nid_postfix(nullptr) == 0x21);
    REQUIRE(sceFreeTypeOlFtaSupportFormatType1_nid_postfix(&invalidLibrary) == 0x21);
    REQUIRE(sceFreeTypeOlFtaSupportFormatType42_nid_postfix(nullptr) == 0x21);
    REQUIRE(sceFreeTypeOlFtaSupportFormatType42_nid_postfix(&invalidLibrary) == 0x21);

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
    REQUIRE(sceFreeTypeOlFtaSupportFormatCid_nid_postfix(library) == 0);
    REQUIRE(sceFreeTypeOlFtaSupportFormatOpenType_nid_postfix(library) == 0);
    REQUIRE(sceFreeTypeOlFtaSupportFormatOpenTypeOtf_nid_postfix(library) == 0);
    REQUIRE(sceFreeTypeOlFtaSupportFormatOpenTypeTtf_nid_postfix(library) == 0);
    REQUIRE(sceFreeTypeOlFtaSupportFormatTrueType_nid_postfix(library) == 0);
    REQUIRE(sceFreeTypeOlFtaSupportFormatTrueTypeGx_nid_postfix(library) == 0);
    REQUIRE(sceFreeTypeOlFtaSupportFormatType1_nid_postfix(library) == 0);
    REQUIRE(sceFreeTypeOlFtaSupportFormatType42_nid_postfix(library) == 0);

    REQUIRE(sceFreeTypeOlFtaRemoveModuleT1cid_nid_postfix(library) == 0);
    REQUIRE(FT_Get_Module(native, "t1cid") == nullptr);
    REQUIRE(sceFreeTypeOlFtaSupportFormatCid_nid_postfix(library) == 0);
    REQUIRE(FT_Get_Module(native, "t1cid") != nullptr);

    REQUIRE(sceFreeTypeOlFtaRemoveModuleCff_nid_postfix(library) == 0);
    REQUIRE(sceFreeTypeOlFtaRemoveModuleTruetype_nid_postfix(library) == 0);
    REQUIRE(FT_Get_Module(native, "cff") == nullptr);
    REQUIRE(FT_Get_Module(native, "truetype") == nullptr);
    REQUIRE(sceFreeTypeOlFtaSupportFormatOpenType_nid_postfix(library) == 0);
    REQUIRE(FT_Get_Module(native, "cff") != nullptr);
    REQUIRE(FT_Get_Module(native, "truetype") != nullptr);

    REQUIRE(sceFreeTypeOlFtaRemoveModuleCff_nid_postfix(library) == 0);
    REQUIRE(FT_Get_Module(native, "cff") == nullptr);
    REQUIRE(sceFreeTypeOlFtaSupportFormatOpenTypeOtf_nid_postfix(library) == 0);
    REQUIRE(FT_Get_Module(native, "cff") != nullptr);

    REQUIRE(sceFreeTypeOlFtaRemoveModuleTruetype_nid_postfix(library) == 0);
    REQUIRE(FT_Get_Module(native, "truetype") == nullptr);
    REQUIRE(sceFreeTypeOlFtaSupportFormatOpenTypeTtf_nid_postfix(library) == 0);
    REQUIRE(FT_Get_Module(native, "truetype") != nullptr);

    REQUIRE(sceFreeTypeOlFtaRemoveModuleTruetype_nid_postfix(library) == 0);
    REQUIRE(FT_Get_Module(native, "truetype") == nullptr);
    REQUIRE(sceFreeTypeOlFtaSupportFormatTrueType_nid_postfix(library) == 0);
    REQUIRE(FT_Get_Module(native, "truetype") != nullptr);

    REQUIRE(sceFreeTypeOlFtaRemoveModuleTruetype_nid_postfix(library) == 0);
    REQUIRE(sceFreeTypeOlFtaRemoveModuleGxvalid_nid_postfix(library) == 0);
    REQUIRE(FT_Get_Module(native, "truetype") == nullptr);
    REQUIRE(FT_Get_Module(native, "gxvalid") == nullptr);
    REQUIRE(sceFreeTypeOlFtaSupportFormatTrueTypeGx_nid_postfix(library) == 0);
    REQUIRE(FT_Get_Module(native, "truetype") != nullptr);
    REQUIRE(FT_Get_Module(native, "gxvalid") != nullptr);

    REQUIRE(sceFreeTypeOlFtaRemoveModuleType1_nid_postfix(library) == 0);
    REQUIRE(FT_Get_Module(native, "type1") == nullptr);
    REQUIRE(sceFreeTypeOlFtaSupportFormatType1_nid_postfix(library) == 0);
    REQUIRE(FT_Get_Module(native, "type1") != nullptr);

    REQUIRE(sceFreeTypeOlFtaRemoveModuleType42_nid_postfix(library) == 0);
    REQUIRE(FT_Get_Module(native, "type42") == nullptr);
    REQUIRE(sceFreeTypeOlFtaSupportFormatType42_nid_postfix(library) == 0);
    REQUIRE(FT_Get_Module(native, "type42") != nullptr);


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

    REQUIRE(sceFreeTypeOlFtaRemoveModuleTruetype_nid_postfix(library) == 0);
    GuestFaceRec* openTypeFace = nullptr;
    REQUIRE(FT_New_Memory_Face_nid_postfix(library, font.data(), font.size(), 0, &openTypeFace) != 0);
    REQUIRE(sceFreeTypeOlFtaSupportFormatOpenType_nid_postfix(library) == 0);
    REQUIRE(FT_New_Memory_Face_nid_postfix(library, font.data(), font.size(), 0, &openTypeFace) == 0);
    REQUIRE(openTypeFace != nullptr && openTypeFace->glyph != nullptr);
    REQUIRE(FT_Select_Charmap_nid_postfix(openTypeFace, FT_ENCODING_UNICODE) == 0);
    REQUIRE(FT_Get_Char_Index_nid_postfix(openTypeFace, 'A') != 0);
    REQUIRE(FT_Done_Face_nid_postfix(openTypeFace) == 0);

    GuestLibrary* fullLib = nullptr;
    REQUIRE(FT_Init_FreeType_nid_postfix(&fullLib) == 0);
    REQUIRE(fullLib != nullptr && fullLib->host != nullptr);
    FT_Library fullNative = fullLib->host;

    REQUIRE(sceFreeTypeOlFtaRemoveModuleCff_nid_postfix(fullLib) == 0);

    std::vector<FT_Module_Class> dummies;
    std::vector<std::string> dummyNames;
    while (fullNative->num_modules < 32) {
        dummyNames.push_back("dummy_" + std::to_string(fullNative->num_modules));
        FT_Module_Class dummy{};
        dummy.module_size = sizeof(FT_ModuleRec);
        dummy.module_name = dummyNames.back().c_str();
        dummy.module_version = 0x10000;
        dummies.push_back(dummy);
        REQUIRE(FT_Add_Module(fullNative, &dummies.back()) == 0);
    }
    REQUIRE(fullNative->num_modules == 32);
    REQUIRE(sceFreeTypeOlFtaSupportFormatOpenType_nid_postfix(fullLib) == 0x30);

    REQUIRE(sceFreeTypeOlFtaRemoveModuleTruetype_nid_postfix(fullLib) == 0);
    sceFreeTypeOlFtaAddModuleGxvalid_nid_postfix(fullLib);
    REQUIRE(fullNative->num_modules == 32);
    REQUIRE(sceFreeTypeOlFtaSupportFormatTrueTypeGx_nid_postfix(fullLib) == 0x30);

    REQUIRE(FT_Done_FreeType_nid_postfix(fullLib) == 0);

    REQUIRE(FT_Done_FreeType_nid_postfix(library) == 0);

#ifdef _WIN32
    HMODULE prx = GetModuleHandleA("libSceFreeTypeOl.prx");
    if (!prx) prx = GetModuleHandleA("libSceFreeTypeOl.dll");
    REQUIRE(prx != nullptr);
    const auto lookup = [&](const char* nid) {
        const std::string name = std::string(nid) + "_nid_no_patch_cut";
        return GetProcAddress(prx, name.c_str()) != nullptr;
    };
#else
    void* prx = dlopen(nullptr, RTLD_NOW);
    REQUIRE(prx != nullptr);
    const auto lookup = [&](const char* nid) {
        const std::string name = std::string(nid) + "_nid_no_patch_cut";
        return dlsym(prx, name.c_str()) != nullptr;
    };
#endif
    for (const char* nid : {"JuzrPZClLew", "sylevnx-fqk", "0RSuGozMl9o", "J3HnvW8FJV8", "YpxR8CqGMIM", "83qtpn8+XnA", "g2p7fkJpuC0", "xqjAbn8VNGo"}) {
        REQUIRE(lookup(nid));
    }
}
