#include "prx/libSceFont/include/Api.hpp"
#include "prx/libSceFontFt/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Require(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FontFt bitmap check failed at line %d\n", line);
        std::abort();
    }
}

#define Check(value) Require((value), __LINE__)

static void* APS5_VABI Allocate(void*, std::uint32_t size) {
    return std::malloc(size);
}

static void APS5_VABI Release(void*, void* pointer) {
    std::free(pointer);
}

int main(int argc, char** argv) {
    static constexpr char font[] =
        "STARTFONT 2.1\n"
        "FONT -misc-square-medium-r-normal--8-80-75-75-C-80-iso10646-1\n"
        "SIZE 8 75 75\n"
        "FONTBOUNDINGBOX 8 8 0 0\n"
        "STARTPROPERTIES 4\n"
        "FONT_ASCENT 8\n"
        "FONT_DESCENT 0\n"
        "FAMILY_NAME \"Square\"\n"
        "WEIGHT_NAME \"Medium\"\n"
        "ENDPROPERTIES\n"
        "CHARS 1\n"
        "STARTCHAR A\n"
        "ENCODING 65\n"
        "SWIDTH 500 0\n"
        "DWIDTH 8 0\n"
        "BBX 8 8 0 0\n"
        "BITMAP\n"
        "18\n"
        "24\n"
        "42\n"
        "7E\n"
        "42\n"
        "42\n"
        "42\n"
        "00\n"
        "ENDCHAR\n"
        "ENDFONT\n";
    const FontMemoryInterface iface{Allocate, Release, nullptr, nullptr, nullptr, nullptr};
    FontMemory memory{};
    Check(sceFontMemoryInit(&memory, nullptr, 0, &iface, nullptr, nullptr, nullptr) == SCE_FONT_OK);
    FontLibrary library = nullptr;
    Check(sceFontCreateLibrary(&memory, sceFontSelectLibraryFt(0), &library) == SCE_FONT_OK);
    Check(sceFontSupportExternalFonts(library, 2, 0x52) == SCE_FONT_OK);
    FontHandle handle = nullptr;
    Check(sceFontOpenFontMemory(library, font, sizeof(font) - 1, nullptr, &handle) == SCE_FONT_OK && handle != nullptr);
    if (argc == 1 || std::strcmp(argv[1], "style") == 0) {
        FontStyleInformation style{};
        Check(sceFontGetFontStyleInformation(handle, &style) == SCE_FONT_OK);
        Check(style.layoutFlags == 1 && style.styleFlags == 0 && style.weight == 0 && style.width == 0);
    }
    if (argc == 1 || std::strcmp(argv[1], "outline") == 0) {
        FontOutlineProfile outline{};
        std::memset(&outline, 0x7f, sizeof(outline));
        Check(sceFontGetFontGlyphsOutlineProfile(handle, &outline) == SCE_FONT_ERROR_NO_SUPPORT_FUNCTION);
        Check(outline.maxContours == 0 && outline.maxPoints == 0 &&
              outline.maxCompositeContours == 0 && outline.maxCompositePoints == 0);
    }
    Check(sceFontCloseFont(handle) == SCE_FONT_OK);
    Check(sceFontDestroyLibrary(&library) == SCE_FONT_OK && library == nullptr);
    Check(sceFontMemoryTerm(&memory) == SCE_FONT_OK);
    std::puts("FontFt BDF query verified");
}
