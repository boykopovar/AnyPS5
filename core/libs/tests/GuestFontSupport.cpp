#include "FontFixture.hpp"
#include "prx/libSceFont/include/Api.hpp"
#include "prx/libSceFont/include/FontTypes.hpp"
#include "prx/libSceFontFt/include/Api.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Font support check failed at line %d\n", line);
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
    const FontFtGlyphSelection* selection = sceFontSelectGlyphsFt(0);
    Require(selection != nullptr);
    const auto* selectionTags = reinterpret_cast<const std::uint16_t*>(selection);
    Require(selectionTags[0] == 0x0F1F);
    Require(selectionTags[1] == 0xF200);
    Require(selectionTags[2] == 0x0F03);
    Require(selectionTags[3] == 0xF8F0);
    Require(sceFontSelectGlyphsFt(1) == nullptr);
    Require(sceFontSelectGlyphsFt(-1) == nullptr);

    alignas(16) std::uint16_t badTag[4] = {0x0F1E, 0xF200, 0x0F03, 0xF8F0};
    alignas(16) std::uint16_t badKind[4] = {0x0F1F, 0xF200, 0x0F02, 0xF8F0};
    alignas(16) std::uint16_t otherTable[4] = {0x0F1F, 0xF200, 0x0F03, 0xF8F0};
    alignas(16) std::uint16_t thirdTable[4] = {0x0F1F, 0xF200, 0x0F03, 0xF8F0};
    Require(sceFontSupportGlyphs(nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontSupportGlyphs(reinterpret_cast<const FontGlyphSupport*>(badTag)) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontSupportGlyphs(reinterpret_cast<const FontGlyphSupport*>(badKind)) == SCE_FONT_ERROR_FATAL);

    const FontMemoryInterface iface{Allocate, Release, nullptr, nullptr, nullptr, nullptr};
    FontMemory memory{};
    Require(sceFontMemoryInit(&memory, nullptr, 0, &iface, nullptr, nullptr, nullptr) == SCE_FONT_OK);
    FontLibrary library = nullptr;
    Require(sceFontCreateLibrary(&memory, sceFontSelectLibraryFt(0), &library) == SCE_FONT_OK);

    Require(sceFontSupportSystemUpdation(nullptr, 0x20230801) == SCE_FONT_ERROR_INVALID_LIBRARY);
    Require(sceFontSupportSystemUpdation(library, 0) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontSupportSystemUpdation(library, 0x20230800) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontSupportSystemUpdation(library, 0x20230801) == SCE_FONT_OK);
    Require(sceFontSupportSystemUpdation(library, 0x20230801) == SCE_FONT_OK);
    Require(sceFontSupportSystemUpdation(library, 0) == SCE_FONT_ERROR_INVALID_PARAMETER);

    Require(sceFontFtSupportSystemFonts(nullptr) == SCE_FONT_ERROR_INVALID_LIBRARY);
    Require(sceFontFtSupportSystemFonts(library) == sceFontFtSupportOpenType(library));

    alignas(16) std::uint8_t record[0x30];
    std::memset(record, 0xA5, sizeof(record));
    Require(sceFontOutlineVertexesInterfaceInit(nullptr, record, record, 1) == SCE_FONT_ERROR_INVALID_PARAMETER);
    int p1 = 0, p2 = 0;
    Require(sceFontOutlineVertexesInterfaceInit(record, nullptr, &p2, 2) == SCE_FONT_ERROR_INVALID_PARAMETER);
    for (int i = 0; i < 0x30; ++i) Require(record[i] == 0);
    std::memset(record, 0xA5, sizeof(record));
    Require(sceFontOutlineVertexesInterfaceInit(record, &p1, &p2, 0) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontOutlineVertexesInterfaceInit(record, &p1, &p2, 4) == SCE_FONT_ERROR_INVALID_PARAMETER);
    for (int i = 0; i < 0x30; ++i) Require(record[i] == 0);
    std::memset(record, 0xA5, sizeof(record));
    Require(sceFontOutlineVertexesInterfaceInit(record, &p1, &p2, 2) == SCE_FONT_OK);
    Require(record[0] == 0x08 && record[1] == 0x0F);
    Require(*reinterpret_cast<void**>(record + 0x08) == &p1);
    Require(*reinterpret_cast<void**>(record + 0x10) == &p2);
    Require(*reinterpret_cast<std::uint64_t*>(record + 0x18) == 2);
    for (int i = 2; i < 8; ++i) Require(record[i] == 0);
    for (int i = 0x20; i < 0x30; ++i) Require(record[i] == 0);
    std::memset(record, 0xA5, sizeof(record));
    Require(sceFontOutlineVertexesInterfaceInit(record, &p1, nullptr, 1) == SCE_FONT_OK);
    Require(*reinterpret_cast<void**>(record + 0x10) == nullptr);
    std::memset(record, 0xA5, sizeof(record));
    Require(sceFontOutlineVertexesInterfaceInit(record, &p1, &p2, 3) == SCE_FONT_OK);
    Require(*reinterpret_cast<std::uint64_t*>(record + 0x18) == 3);

    Require(sceFontSupportGlyphs(reinterpret_cast<const FontGlyphSupport*>(selection)) == SCE_FONT_OK);
    Require(sceFontSupportGlyphs(reinterpret_cast<const FontGlyphSupport*>(otherTable)) == SCE_FONT_OK);
    Require(sceFontSupportGlyphs(reinterpret_cast<const FontGlyphSupport*>(thirdTable)) == SCE_FONT_OK);
    Require(sceFontSupportGlyphs(reinterpret_cast<const FontGlyphSupport*>(selection)) == SCE_FONT_OK);

    Require(sceFontDestroyLibrary(&library) == SCE_FONT_OK);
    Require(allocations == 0);
    Require(sceFontMemoryTerm(&memory) == SCE_FONT_OK);
    std::puts("Font support registration tests passed");
}
