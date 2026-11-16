#include "prx/libSceFreeTypeOl/libSceFreeTypeOl.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "FreeTypeFontFixture.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
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
    REQUIRE(sceFreeTypeOlFtaSupportModules_nid_postfix(nullptr) == 0x21);
    REQUIRE(sceFreeTypeOlFtaSupportModules_nid_postfix(&invalidLibrary) == 0x21);

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

    REQUIRE(sceFreeTypeOlFtaRemoveModuleTruetype_nid_postfix(library) == 0);
    REQUIRE(sceFreeTypeOlFtaRemoveModuleCff_nid_postfix(library) == 0);
    REQUIRE(sceFreeTypeOlFtaRemoveModuleType1_nid_postfix(library) == 0);
    REQUIRE(FT_Get_Module(native, "truetype") == nullptr);
    REQUIRE(FT_Get_Module(native, "cff") == nullptr);
    REQUIRE(FT_Get_Module(native, "type1") == nullptr);

    GuestFaceRec* missingFace = nullptr;
    REQUIRE(FT_New_Memory_Face_nid_postfix(library, font.data(), font.size(), 0, &missingFace) != 0);

    REQUIRE(sceFreeTypeOlFtaSupportModules_nid_postfix(library) == 0);
    REQUIRE(FT_Get_Module(native, "truetype") != nullptr);
    REQUIRE(FT_Get_Module(native, "cff") != nullptr);
    REQUIRE(FT_Get_Module(native, "type1") != nullptr);

    GuestFaceRec* restoredFace = nullptr;
    REQUIRE(FT_New_Memory_Face_nid_postfix(library, font.data(), font.size(), 0, &restoredFace) == 0);
    REQUIRE(restoredFace != nullptr && restoredFace->glyph != nullptr);
    REQUIRE(FT_Select_Charmap_nid_postfix(restoredFace, FT_ENCODING_UNICODE) == 0);
    REQUIRE(FT_Get_Char_Index_nid_postfix(restoredFace, 'A') != 0);
    REQUIRE(FT_Done_Face_nid_postfix(restoredFace) == 0);

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
    REQUIRE(lookup("oM0zMNCfXKM"));

    struct ExportCase {
        const char* name;
        const void* (APS5_VABI* exportModule)();
        std::uint64_t kind;
    };
    const ExportCase exports[] = {
        {"autofitter", sceFreeTypeOlFtaExportModuleAutofitter_nid_postfix, 0},
        {"bdf", sceFreeTypeOlFtaExportModuleBdf_nid_postfix, FT_MODULE_FONT_DRIVER},
        {"cff", sceFreeTypeOlFtaExportModuleCff_nid_postfix, FT_MODULE_FONT_DRIVER},
        {"gxvalid", sceFreeTypeOlFtaExportModuleGxvalid_nid_postfix, 0},
        {"otvalid", sceFreeTypeOlFtaExportModuleOtvalid_nid_postfix, 0},
        {"pcf", sceFreeTypeOlFtaExportModulePcf_nid_postfix, FT_MODULE_FONT_DRIVER},
        {"pfr", sceFreeTypeOlFtaExportModulePfr_nid_postfix, FT_MODULE_FONT_DRIVER},
        {"psaux", sceFreeTypeOlFtaExportModulePsaux_nid_postfix, 0},
        {"pshinter", sceFreeTypeOlFtaExportModulePshinter_nid_postfix, 0},
        {"psnames", sceFreeTypeOlFtaExportModulePsnames_nid_postfix, 0},
        {"raster1", sceFreeTypeOlFtaExportModuleRaster1_nid_postfix, FT_MODULE_RENDERER},
        {"sfnt", sceFreeTypeOlFtaExportModuleSfnt_nid_postfix, 0},
        {"smooth", sceFreeTypeOlFtaExportModuleSmooth_nid_postfix, FT_MODULE_RENDERER},
        {"t1cid", sceFreeTypeOlFtaExportModuleT1cid_nid_postfix, FT_MODULE_FONT_DRIVER},
        {"truetype", sceFreeTypeOlFtaExportModuleTruetype_nid_postfix, FT_MODULE_FONT_DRIVER},
        {"type1", sceFreeTypeOlFtaExportModuleType1_nid_postfix, FT_MODULE_FONT_DRIVER},
        {"type42", sceFreeTypeOlFtaExportModuleType42_nid_postfix, FT_MODULE_FONT_DRIVER},
        {"winfonts", sceFreeTypeOlFtaExportModuleWinfonts_nid_postfix, FT_MODULE_FONT_DRIVER},
    };

    constexpr std::size_t flagsOffset = 0;
    constexpr std::size_t nameOffset = 16;
    constexpr std::size_t versionOffset = 24;
    constexpr std::size_t interfaceOffset = 40;
    constexpr std::size_t initOffset = 48;
    constexpr std::size_t getInterfaceOffset = 64;
    constexpr std::size_t driverObjectSizesOffset = 72;
    constexpr std::size_t driverLoadGlyphOffset = 96 + 6 * 8;
    constexpr std::size_t rendererFormatOffset = 72;
    constexpr std::size_t rendererRenderOffset = 80;
    constexpr std::size_t rendererRasterOffset = 112;
    constexpr std::size_t driverRecordSize = 192;

    auto field = [](const void* record, std::size_t offset) {
        std::uint64_t value = 0;
        std::memcpy(&value, static_cast<const unsigned char*>(record) + offset, sizeof(value));
        return value;
    };
    auto throws = [](auto&& call) {
        try {
            call();
        } catch (const std::exception&) {
            return true;
        }
        return false;
    };

    const void* previous = nullptr;
    for (const auto& entry : exports) {
        const void* record = entry.exportModule();
        REQUIRE(record != nullptr && record != previous);
        REQUIRE(entry.exportModule() == record);
        previous = record;

        REQUIRE((field(record, flagsOffset) & (FT_MODULE_FONT_DRIVER | FT_MODULE_RENDERER)) == entry.kind);
        const auto* name = reinterpret_cast<const char*>(field(record, nameOffset));
        REQUIRE(name != nullptr && std::strcmp(name, entry.name) == 0);
        REQUIRE(static_cast<std::int64_t>(field(record, versionOffset)) > 0);
        REQUIRE(field(record, interfaceOffset) == 0);
        if (entry.kind == FT_MODULE_FONT_DRIVER) {
            for (std::size_t size = 0; size < 3; ++size)
                REQUIRE(static_cast<std::int64_t>(field(record, driverObjectSizesOffset + size * 8)) > 0);
            REQUIRE(field(record, driverLoadGlyphOffset) != 0);
        }
        if (entry.kind == FT_MODULE_RENDERER) {
            REQUIRE(static_cast<std::uint32_t>(field(record, rendererFormatOffset)) == FT_GLYPH_FORMAT_OUTLINE);
            REQUIRE(field(record, rendererRenderOffset) != 0);
            REQUIRE(field(record, rendererRasterOffset) == 0);
        }

        if (FT_Module module = FT_Get_Module(native, entry.name)) REQUIRE(FT_Remove_Module(native, module) == 0);
        REQUIRE(sceFreeTypeOlAddModule_nid_postfix(library, record) == FT_Err_Ok);
        REQUIRE(FT_Get_Module(native, entry.name) != nullptr);
        REQUIRE(FT_Remove_Module(native, FT_Get_Module(native, entry.name)) == 0);
        REQUIRE(sceFreeTypeOlFtaAddModule_nid_postfix(library, record) == FT_Err_Ok);
        REQUIRE(FT_Get_Module(native, entry.name) != nullptr);
        REQUIRE(sceFreeTypeOlFtaAddModule_nid_postfix(library, record) == FT_Err_Lower_Module_Version);
        REQUIRE(sceFreeTypeOlAddModule_nid_postfix(nullptr, record) == FT_Err_Invalid_Library_Handle);
        REQUIRE(sceFreeTypeOlFtaAddModule_nid_postfix(&invalidLibrary, record) == FT_Err_Invalid_Library_Handle);
    }

    REQUIRE(sceFreeTypeOlAddModule_nid_postfix(library, nullptr) == FT_Err_Invalid_Argument);
    REQUIRE(sceFreeTypeOlFtaAddModule_nid_postfix(library, nullptr) == FT_Err_Invalid_Argument);

    unsigned char copied[driverRecordSize];
    std::memcpy(copied, sceFreeTypeOlFtaExportModuleTruetype_nid_postfix(), sizeof(copied));
    REQUIRE(throws([&] { sceFreeTypeOlAddModule_nid_postfix(library, copied); }));
    REQUIRE(throws([&] { sceFreeTypeOlFtaAddModule_nid_postfix(library, copied); }));

    const void* sfnt = sceFreeTypeOlFtaExportModuleSfnt_nid_postfix();
    REQUIRE(field(sfnt, initOffset) == 0);
    using Method = void (APS5_VABI*)();
    const auto getInterface = reinterpret_cast<Method>(field(sfnt, getInterfaceOffset));
    REQUIRE(getInterface != nullptr);
    REQUIRE(throws([&] { getInterface(); }));

    REQUIRE(FT_Done_FreeType_nid_postfix(library) == 0);
}
