#include "FontFixture.hpp"
#include "prx/libSceFontFt/include/Api.hpp"
#include "prx/libSceFont/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Font output pointer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static void* APS5_VABI Allocate(void*, std::uint32_t size) {
    return std::malloc(size);
}

static void APS5_VABI Release(void*, void* pointer) {
    std::free(pointer);
}

static std::vector<unsigned char> BuildCharacterFont() {
    return FontFixture::BuildFont(3, {
        {"cmap", FontFixture::Words({0, 1, 3, 1, 0, 12, 4, 40, 0, 6, 4, 1, 2, 'A', 'V', 0xFFFF, 0, 'A', 'V', 0xFFFF, 1 - 'A', 2 - 'V', 1, 0, 0, 0})},
        {"kern", FontFixture::Words({0, 1, 0, 20, 1, 1, 6, 0, 0, 1, 2, -200})},
    });
}

int main() {
    const FontMemoryInterface iface{Allocate, Release, nullptr, nullptr, nullptr, nullptr};
    FontMemory memory{};
    Require(sceFontMemoryInit(&memory, nullptr, 0, &iface, nullptr, nullptr, nullptr) == SCE_FONT_OK);
    FontLibrary library = nullptr;
    Require(sceFontCreateLibrary(&memory, sceFontSelectLibraryFt(0), &library) == SCE_FONT_OK && library != nullptr);
    FontRenderer renderer = nullptr;
    Require(sceFontCreateRenderer(&memory, sceFontSelectRendererFt(0), &renderer) == SCE_FONT_OK && renderer != nullptr);

    auto* const unmappedCount = reinterpret_cast<std::uint32_t*>(0x10);
    auto* const unmappedLibrary = reinterpret_cast<FontLibrary*>(0x10);

    errno = 71;
    Require(sceFontRendererGetOutlineBufferSize(renderer, unmappedCount) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(errno == 71);
    errno = 71;
    Require(sceFontRendererGetOutlineBufferSize(nullptr, unmappedCount) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(errno == 71);

    errno = 71;
    Require(sceFontGetPixelResolution(library, unmappedCount) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(errno == 71);
    errno = 71;
    Require(sceFontGetPixelResolution(nullptr, unmappedCount) == SCE_FONT_ERROR_INVALID_LIBRARY);
    Require(errno == 71);

    errno = 71;
    Require(sceFontGetLibrary(nullptr, unmappedLibrary) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(errno == 71);

    errno = 71;
    Require(sceFontGetFontGlyphsCount(nullptr, unmappedCount) == SCE_FONT_ERROR_INVALID_FONT_HANDLE);
    Require(errno == 71);

    Require(sceFontRendererGetOutlineBufferSize(renderer, nullptr) == sceFontRendererGetOutlineBufferSize(renderer, unmappedCount));
    Require(sceFontRendererGetOutlineBufferSize(nullptr, nullptr) == sceFontRendererGetOutlineBufferSize(nullptr, unmappedCount));
    Require(sceFontGetPixelResolution(library, nullptr) == sceFontGetPixelResolution(library, unmappedCount));
    Require(sceFontGetPixelResolution(nullptr, nullptr) == sceFontGetPixelResolution(nullptr, unmappedCount));
    Require(sceFontGetLibrary(nullptr, nullptr) == sceFontGetLibrary(nullptr, unmappedLibrary));
    Require(sceFontGetFontGlyphsCount(nullptr, nullptr) == sceFontGetFontGlyphsCount(nullptr, unmappedCount));

    std::uint32_t size = 7;
    Require(sceFontRendererGetOutlineBufferSize(renderer, &size) == SCE_FONT_OK && size == 0x4000);
    std::uint32_t subPixelCount = 1;
    Require(sceFontGetPixelResolution(library, &subPixelCount) == SCE_FONT_OK && subPixelCount == 64);

    auto* const unmappedAttribute = reinterpret_cast<std::int32_t*>(0x10);
    Require(sceFontSupportExternalFonts(library, 1, 0x52) == SCE_FONT_OK);
    const std::vector<unsigned char> fontData = BuildCharacterFont();
    FontHandle font = nullptr;
    Require(sceFontOpenFontMemory(library, fontData.data(), static_cast<std::uint32_t>(fontData.size()), nullptr, &font) == SCE_FONT_OK);
    Require(sceFontSetScalePixel(font, 100.0f, 100.0f) == SCE_FONT_OK);
    FontGlyph glyph = nullptr;
    Require(sceFontGenerateCharGlyph(font, 'A', nullptr, &glyph) == SCE_FONT_OK && glyph != nullptr);

    errno = 71;
    Require(sceFontGlyphDefineAttribute(nullptr, 0x10, unmappedAttribute) == SCE_FONT_ERROR_INVALID_GLYPH);
    Require(errno == 71);
    errno = 71;
    Require(sceFontGlyphDefineAttribute(glyph, 0x20, unmappedAttribute) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(errno == 71);
    errno = 71;
    Require(sceFontGlyphDefineAttribute(glyph, 0x10, unmappedAttribute) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(errno == 71);
    Require(sceFontGlyphDefineAttribute(glyph, 0x11, nullptr) == SCE_FONT_OK);
    std::int32_t previous0 = 0, previous1 = 0, previous2 = 0;
    Require(sceFontGlyphDefineAttribute(glyph, 0x10, &previous0) == SCE_FONT_OK);
    Require(sceFontGlyphDefineAttribute(glyph, 0x11, &previous1) == SCE_FONT_OK && previous1 == 0x10);
    Require(sceFontGlyphDefineAttribute(glyph, 0x10, &previous2) == SCE_FONT_OK && previous2 == 0x11);

    Require(sceFontDeleteGlyph(&memory, &glyph) == SCE_FONT_OK && glyph == nullptr);
    Require(sceFontCloseFont(font) == SCE_FONT_OK);

    Require(sceFontDestroyRenderer(&renderer) == SCE_FONT_OK && renderer == nullptr);
    Require(sceFontDestroyLibrary(&library) == SCE_FONT_OK && library == nullptr);
    return 0;
}
