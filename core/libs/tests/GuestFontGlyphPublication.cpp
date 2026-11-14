#include "tests/FontFixture.hpp"
#include "prx/libSceFont/include/Api.hpp"
#include "prx/libSceFont/include/FontInternal.hpp"
#include "prx/libSceFontFt/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <vector>

namespace {

bool active = false;
bool failPublication = false;
bool qualified = false;
Font::GeneratedGlyph* captured = nullptr;
FontHandle expectedOwner = nullptr;
unsigned objectFrees = 0;
int guestAllocations = 0;

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Generated glyph publication check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

void Reset(bool fail) {
    active = true;
    failPublication = fail;
    qualified = false;
    captured = nullptr;
    objectFrees = 0;
}

void* APS5_VABI Allocate(void*, std::uint32_t size) {
    void* pointer = std::malloc(size == 0 ? 1 : size);
    if (pointer) ++guestAllocations;
    return pointer;
}

void APS5_VABI Release(void*, void* pointer) {
    if (pointer) --guestAllocations;
    std::free(pointer);
}

}

void* operator new(std::size_t bytes) {
    if (active && failPublication && captured) {
        qualified = captured->glyph.magic == Font::GLYPH_MAGIC && captured->codepoint == 'A' && captured->owner == expectedOwner;
        active = false;
        std::fprintf(stderr, "stage=registry_insert qualified=%d request=%zu\n", qualified ? 1 : 0, bytes);
        throw std::bad_alloc();
    }
    void* pointer = std::malloc(bytes == 0 ? 1 : bytes);
    if (!pointer) throw std::bad_alloc();
    return pointer;
}

void* operator new(std::size_t bytes, const std::nothrow_t&) noexcept {
    try {
        void* pointer = ::operator new(bytes);
        if (active && !captured && bytes == sizeof(Font::GeneratedGlyph)) captured = static_cast<Font::GeneratedGlyph*>(pointer);
        return pointer;
    } catch (...) {
        return nullptr;
    }
}

void operator delete(void* pointer) noexcept {
    if (pointer && pointer == captured) ++objectFrees;
    std::free(pointer);
}

void operator delete(void* pointer, std::size_t) noexcept {
    ::operator delete(pointer);
}

void operator delete(void* pointer, const std::nothrow_t&) noexcept {
    ::operator delete(pointer);
}

int main() {
    const FontMemoryInterface iface{Allocate, Release, nullptr, nullptr, nullptr, nullptr};
    FontMemory memory{};
    Require(sceFontMemoryInit(&memory, nullptr, 0, &iface, nullptr, nullptr, nullptr) == SCE_FONT_OK);
    FontLibrary library = nullptr;
    Require(sceFontCreateLibrary(&memory, sceFontSelectLibraryFt(0), &library) == SCE_FONT_OK);
    Require(sceFontFtSupportTrueType(library) == SCE_FONT_OK);
    Require(sceFontSupportExternalFonts(library, 1, 0x52) == SCE_FONT_OK);
    const std::vector<unsigned char> bytes = FontFixture::BuildFont(3, {
        {"cmap", FontFixture::Words({0, 1, 3, 1, 0, 12, 4, 40, 0, 6, 4, 1, 2, 'A', 'V', 0xFFFF, 0, 'A', 'V', 0xFFFF, 1 - 'A', 2 - 'V', 1, 0, 0, 0})},
    });
    FontHandle font = nullptr;
    Require(sceFontOpenFontMemory(library, bytes.data(), static_cast<std::uint32_t>(bytes.size()), nullptr, &font) == SCE_FONT_OK && font);
    expectedOwner = font;
    Require(sceFontSetScalePixel(font, 32.0f, 32.0f) == SCE_FONT_OK);
    FontGlyphMetrics metrics{};
    Require(sceFontGetCharGlyphMetrics(font, 'A', &metrics) == SCE_FONT_OK);

    Reset(false);
    FontGlyph glyph = nullptr;
    Require(sceFontGenerateCharGlyph(font, 'A', nullptr, &glyph) == SCE_FONT_OK);
    active = false;
    Require(captured && glyph == &captured->glyph);
    Require(captured->codepoint == 'A' && captured->owner == font && captured->glyph.magic == Font::GLYPH_MAGIC);
    Require(sceFontDeleteGlyph(&memory, &glyph) == SCE_FONT_OK && !glyph);
    Require(objectFrees == 1);
    std::fprintf(stderr, "stage=allocation_interposition qualified=1 frees=%u\n", objectFrees);

    Reset(true);
    bool caught = false;
    try {
        static_cast<void>(sceFontGenerateCharGlyph(font, 'A', nullptr, &glyph));
    } catch (const std::bad_alloc&) {
        caught = true;
    }
    active = false;
    Require(caught && qualified && captured && !glyph);
    std::fprintf(stderr, "stage=unpublished_cleanup qualified=1 frees=%u\n", objectFrees);
    Require(objectFrees == 1);

    captured = nullptr;
    Require(sceFontGenerateCharGlyph(font, 'A', nullptr, &glyph) == SCE_FONT_OK && glyph);
    Require(sceFontDeleteGlyph(&memory, &glyph) == SCE_FONT_OK && !glyph);
    Require(sceFontCloseFont(font) == SCE_FONT_OK);
    Require(sceFontDestroyLibrary(&library) == SCE_FONT_OK);
    Require(guestAllocations == 0);
    Require(sceFontMemoryTerm(&memory) == SCE_FONT_OK);
}
