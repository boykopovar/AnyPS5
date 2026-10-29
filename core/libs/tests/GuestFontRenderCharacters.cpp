#include "FontFixture.hpp"
#include "prx/libSceFont/include/Api.hpp"
#include "prx/libSceFont/include/FontTypes.hpp"
#include "prx/libSceFontFt/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Font render characters check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static int allocations = 0;
static void* APS5_VABI Allocate(void*, std::uint32_t size) {
    ++allocations;
    return std::malloc(size == 0 ? 1 : size);
}
static void APS5_VABI Release(void*, void* pointer) {
    if (pointer) --allocations;
    std::free(pointer);
}

struct Codes {
    std::vector<std::uint32_t> values;
    FontHandle font = nullptr;
    std::size_t at = 0;
};

static std::int32_t APS5_VABI ParseCodes(FontTextSource* source, void** order, FontTextParseResult* result) {
    auto& codes = *static_cast<Codes*>(source->textObject);
    *order = reinterpret_cast<void*>(codes.at);
    if (codes.at == codes.values.size()) {
        result->Terminate.terminateCode = 0;
        return 0;
    }
    result->FontCode.font = codes.font;
    result->FontCode.code = codes.values[codes.at++];
    return 1;
}

static FontString Create(const FontMemory* memory, Codes& codes) {
    codes.at = 0;
    FontTextSource source{};
    Require(sceFontTextSourceInit(&source, &codes, static_cast<std::uint32_t>(codes.values.empty() ? 1 : codes.values.size()), ParseCodes, &codes) == SCE_FONT_OK);
    FontString string = nullptr;
    Require(sceFontCreateString(memory, &source, nullptr, &string) == SCE_FONT_OK && string != nullptr);
    return string;
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

    Codes empty;
    empty.font = font;
    FontString emptyString = Create(&memory, empty);
    std::uint32_t count = 77;
    Require(sceFontStringRefersRenderCharacters(emptyString, nullptr, nullptr, &count) == nullptr && count == 0);

    Codes text{{'a', 'b', 'c', 'd'}, font};
    FontString string = Create(&memory, text);
    std::uint32_t total = 0;
    FontTextCharacter* characters = sceFontStringRefersTextCharacters(string, &total);
    Require(characters != nullptr && total == 4);

    Require(sceFontStringRefersRenderCharacters(string, nullptr, nullptr, nullptr) == nullptr);
    count = 77;
    Require(sceFontStringRefersRenderCharacters(nullptr, nullptr, nullptr, &count) == nullptr && count == 0);

    count = 77;
    Require(sceFontStringRefersRenderCharacters(string, nullptr, nullptr, &count) == &characters[0] && count == 4);
    count = 77;
    Require(sceFontStringRefersRenderCharacters(string, &characters[1], &characters[1], &count) == &characters[1] && count == 1);
    count = 77;
    Require(sceFontStringRefersRenderCharacters(string, &characters[1], &characters[2], &count) == &characters[1] && count == 2);
    count = 77;
    Require(sceFontStringRefersRenderCharacters(string, &characters[0], &characters[3], &count) == &characters[0] && count == 4);
    count = 77;
    Require(sceFontStringRefersRenderCharacters(string, &characters[2], nullptr, &count) == &characters[2] && count == 2);

    count = 77;
    Require(sceFontStringRefersRenderCharacters(string, &characters[2], &characters[0], &count) == nullptr && count == 0);

    const auto at = [&](std::ptrdiff_t index) {
        return reinterpret_cast<FontTextCharacter*>(reinterpret_cast<std::uintptr_t>(characters) + static_cast<std::uintptr_t>(index * static_cast<std::ptrdiff_t>(sizeof(FontTextCharacter))));
    };
    FontTextCharacter* const before = at(-1);
    FontTextCharacter* const after = at(4);
    for (FontTextCharacter* outside : {before, after}) {
        count = 77;
        Require(sceFontStringRefersRenderCharacters(string, outside, nullptr, &count) == nullptr && count == 0);
        count = 77;
        Require(sceFontStringRefersRenderCharacters(string, &characters[0], outside, &count) == nullptr && count == 0);
    }
    Require(sceFontDestroyString(&string) == SCE_FONT_OK);
    Require(sceFontDestroyString(&emptyString) == SCE_FONT_OK);
    Require(sceFontCloseFont(font) == SCE_FONT_OK);
    Require(sceFontDestroyLibrary(&library) == SCE_FONT_OK);
    Require(allocations == 0);
    Require(sceFontMemoryTerm(&memory) == SCE_FONT_OK);
}
