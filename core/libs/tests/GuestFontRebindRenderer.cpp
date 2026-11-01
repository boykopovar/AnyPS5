#include "FontFixture.hpp"
#include "prx/libSceFont/include/Api.hpp"
#include "prx/libSceFont/include/FontTypes.hpp"
#include "prx/libSceFontFt/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Font rebind renderer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static int allocations = 0;
static void* APS5_VABI Allocate(void*, std::uint32_t size) {
    ++allocations;
    return std::malloc(size);
}
static void APS5_VABI Release(void*, void* pointer) {
    if (pointer) --allocations;
    std::free(pointer);
}

int main() {
    const FontMemoryInterface iface{Allocate, Release, nullptr, nullptr, nullptr, nullptr};
    FontMemory memory{};
    Require(sceFontMemoryInit(&memory, nullptr, 0, &iface, nullptr, nullptr, nullptr) == SCE_FONT_OK);
    FontLibrary library = nullptr;
    Require(sceFontCreateLibrary(&memory, sceFontSelectLibraryFt(0), &library) == SCE_FONT_OK);
    Require(sceFontSupportExternalFonts(library, 1, 0x52) == SCE_FONT_OK);
    const std::vector<unsigned char> fontData = FontFixture::BuildFont(1, {});
    FontHandle font = nullptr;
    Require(sceFontOpenFontMemory(library, fontData.data(), static_cast<std::uint32_t>(fontData.size()), nullptr, &font) == SCE_FONT_OK);

    Require(sceFontRebindRenderer(nullptr) == SCE_FONT_ERROR_INVALID_FONT_HANDLE);
    alignas(16) static unsigned char notAFont[16384] = {};
    Require(sceFontRebindRenderer(reinterpret_cast<FontHandle>(notAFont)) == SCE_FONT_ERROR_INVALID_FONT_HANDLE);

    Require(sceFontRebindRenderer(font) == SCE_FONT_ERROR_NOT_BOUND_RENDERER);

    FontRenderer renderer = nullptr;
    Require(sceFontCreateRenderer(&memory, sceFontSelectRendererFt(0), &renderer) == SCE_FONT_OK);
    Require(sceFontBindRenderer(font, renderer) == SCE_FONT_OK);
    Require(sceFontRebindRenderer(font) == SCE_FONT_OK);
    Require(sceFontRebindRenderer(font) == SCE_FONT_OK);

    Require(sceFontUnbindRenderer(font) == SCE_FONT_OK);
    Require(sceFontRebindRenderer(font) == SCE_FONT_ERROR_NOT_BOUND_RENDERER);

    Require(sceFontDestroyRenderer(&renderer) == SCE_FONT_OK);
    Require(sceFontCloseFont(font) == SCE_FONT_OK);
    Require(sceFontDestroyLibrary(&library) == SCE_FONT_OK);
    Require(allocations == 0);
    Require(sceFontMemoryTerm(&memory) == SCE_FONT_OK);
}
