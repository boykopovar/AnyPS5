#include "prx/libSceFont/include/FontDriver.hpp"
#include "prx/libSceFont/libSceFont.hpp"
#include "prx/libSceFontFt/libSceFontFt.hpp"
#include "tests/FontFixture.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <string_view>
#include <vector>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Font style information check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

using FontFixture::BuildFont;
using FontFixture::Words;

static void* APS5_VABI Allocate(void*, std::uint32_t size) {
    return std::malloc(size);
}

static void APS5_VABI Release(void*, void* pointer) {
    std::free(pointer);
}

static std::vector<unsigned char> NameTable(std::string_view family, std::string_view style) {
    const int familyBytes = static_cast<int>(family.size() * 2);
    const int styleBytes = static_cast<int>(style.size() * 2);
    std::vector<unsigned char> table = Words({0, 2, 30, 3, 1, 0x409, 1, familyBytes, 0, 3, 1, 0x409, 2, styleBytes, familyBytes});
    for (const char c : family) FontFixture::Put16(table, c);
    for (const char c : style) FontFixture::Put16(table, c);
    return table;
}

static std::vector<unsigned char> Os2Table(int weight, int width) {
    return Words({0, 500, weight, width, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0x20, 0x7E, 700, -300, 100, 900, 250});
}

static void PatchHead(std::vector<unsigned char>& font, int macStyle, int direction) {
    const int tableCount = (font[4] << 8) | font[5];
    for (int i = 0; i < tableCount; ++i) {
        const std::size_t entry = 12 + 16 * static_cast<std::size_t>(i);
        if (std::memcmp(font.data() + entry, "head", 4) != 0) continue;
        const std::size_t offset = (static_cast<std::size_t>(font[entry + 8]) << 24) | (static_cast<std::size_t>(font[entry + 9]) << 16) |
            (static_cast<std::size_t>(font[entry + 10]) << 8) | font[entry + 11];
        font[offset + 44] = static_cast<unsigned char>((macStyle >> 8) & 0xFF);
        font[offset + 45] = static_cast<unsigned char>(macStyle & 0xFF);
        font[offset + 48] = static_cast<unsigned char>((direction >> 8) & 0xFF);
        font[offset + 49] = static_cast<unsigned char>(direction & 0xFF);
        return;
    }
    Require(false);
}

static FontHandle Open(FontLibrary library, const std::vector<unsigned char>& data) {
    FontHandle font = nullptr;
    Require(sceFontOpenFontMemory(library, data.data(), static_cast<std::uint32_t>(data.size()), nullptr, &font) == SCE_FONT_OK && font != nullptr);
    return font;
}

static FontStyleInformation Information(FontHandle font) {
    FontStyleInformation information{};
    std::memset(&information, 0x7F, sizeof(information));
    Require(sceFontGetFontStyleInformation(font, &information) == SCE_FONT_OK);
    return information;
}

static bool Cleared(const FontStyleInformation& information) {
    return information.familyName && information.familyName[0] == '\0' && information.styleName && information.styleName[0] == '\0' &&
        information.styleFlags == 0 && information.weight == 0 && information.width == 0 && information.layoutFlags == 0;
}

int main() {
    const FontMemoryInterface iface{Allocate, Release, nullptr, nullptr, nullptr, nullptr};
    FontMemory memory{};
    Require(sceFontMemoryInit(&memory, nullptr, 0, &iface, nullptr, nullptr, nullptr) == SCE_FONT_OK);
    FontLibrary library = nullptr;
    Require(sceFontCreateLibrary(&memory, sceFontSelectLibraryFt(0), &library) == SCE_FONT_OK);
    Require(sceFontSupportExternalFonts(library, 4, 0x52) == SCE_FONT_OK);

    auto namedData = BuildFont(1, {{"name", NameTable("Tst", "Bd")}});
    PatchHead(namedData, 3, -2);
    FontHandle named = Open(library, namedData);
    const FontStyleInformation namedInfo = Information(named);
    Require(namedInfo.familyName && std::string(namedInfo.familyName) == "Tst");
    Require(namedInfo.styleName && std::string(namedInfo.styleName) == "Bd");
    Require(namedInfo.styleFlags == 3 && namedInfo.weight == 0 && namedInfo.width == 0 && namedInfo.layoutFlags == 2);

    const auto os2Data = BuildFont(1, {{"name", NameTable("Tst", "Rg")}, {"OS/2", Os2Table(400, 5)},
        {"vhea", Words({0x11, 0, 880, -120, 60, 1000, 40, 0, 950, 0, 0, 0, 0, 0, 0, 0, 0, 1})}, {"vmtx", Words({1000, 40})}});
    FontHandle os2 = Open(library, os2Data);
    const FontStyleInformation os2Info = Information(os2);
    Require(os2Info.styleFlags == 0 && os2Info.weight == 400 && os2Info.width == 5 && os2Info.layoutFlags == 0x81);

    const auto oddData = BuildFont(1, {{"name", NameTable("Tst", "Rg")}, {"OS/2", Os2Table(450, 12)}});
    FontHandle odd = Open(library, oddData);
    const FontStyleInformation oddInfo = Information(odd);
    Require(oddInfo.weight == 0 && oddInfo.width == 0 && oddInfo.layoutFlags == 1);

    for (const int weight : {100, 900}) {
        const auto data = BuildFont(1, {{"name", NameTable("Tst", "Rg")}, {"OS/2", Os2Table(weight, 9)}});
        FontHandle font = Open(library, data);
        const FontStyleInformation info = Information(font);
        Require(info.weight == weight && info.width == 9);
        Require(sceFontCloseFont(font) == SCE_FONT_OK);
    }
    for (const int weight : {0, 1000}) {
        const auto data = BuildFont(1, {{"name", NameTable("Tst", "Rg")}, {"OS/2", Os2Table(weight, 10)}});
        FontHandle font = Open(library, data);
        const FontStyleInformation info = Information(font);
        Require(info.weight == 0 && info.width == 0);
        Require(sceFontCloseFont(font) == SCE_FONT_OK);
    }

    static const char recordFamily[] = "Rf";
    static const char recordStyle[] = "Rs";
    std::array<std::byte, 116> record{};
    record[4] = std::byte{1};
    record[5] = std::byte{7};
    record[6] = std::byte{3};
    record[7] = std::byte{0x81};
    const char* familyPointer = recordFamily;
    const char* stylePointer = recordStyle;
    std::memcpy(record.data() + 8, &familyPointer, sizeof(familyPointer));
    std::memcpy(record.data() + 16, &stylePointer, sizeof(stylePointer));
    auto* nativeFont = reinterpret_cast<Font::FontHandleNative*>(os2);
    nativeFont->open_info.fontset_record = record.data();
    const FontStyleInformation recordInfo = Information(os2);
    nativeFont->open_info.fontset_record = nullptr;
    Require(recordInfo.familyName == recordFamily && recordInfo.styleName == recordStyle);
    Require(recordInfo.styleFlags == 1 && recordInfo.weight == 700 && recordInfo.width == 3 && recordInfo.layoutFlags == 0x81);

    FontStyleInformation cleared{};
    std::memset(&cleared, 0x7F, sizeof(cleared));
    Require(sceFontGetFontStyleInformation(nullptr, &cleared) == SCE_FONT_ERROR_INVALID_FONT_HANDLE && Cleared(cleared));
    std::uint64_t notAFont[32]{};
    std::memset(&cleared, 0x7F, sizeof(cleared));
    Require(sceFontGetFontStyleInformation(reinterpret_cast<FontHandle>(notAFont), &cleared) == SCE_FONT_ERROR_INVALID_FONT_HANDLE && Cleared(cleared));
    Require(sceFontGetFontStyleInformation(named, nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontGetFontStyleInformation(nullptr, nullptr) == SCE_FONT_ERROR_INVALID_FONT_HANDLE);

    float w = 0.0f;
    float h = 0.0f;
    float slant = 0.0f;
    std::uint32_t hdpi = 0;
    std::uint32_t vdpi = 0;
    Require(sceFontGetScalePixel(nullptr, &w, &h) == SCE_FONT_ERROR_INVALID_FONT_HANDLE);
    Require(sceFontGetScalePoint(nullptr, &w, &h) == SCE_FONT_ERROR_INVALID_FONT_HANDLE);
    Require(sceFontGetEffectSlant(nullptr, &slant) == SCE_FONT_ERROR_INVALID_FONT_HANDLE);
    Require(sceFontGetResolutionDpi(nullptr, &hdpi, &vdpi) == SCE_FONT_ERROR_INVALID_FONT_HANDLE);
    Require(sceFontGetResolutionDpi(named, nullptr, nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);

    for (FontHandle font : {named, os2, odd}) Require(sceFontCloseFont(font) == SCE_FONT_OK);
    Require(sceFontDestroyLibrary(&library) == SCE_FONT_OK);
    Require(sceFontMemoryTerm(&memory) == SCE_FONT_OK);
    std::puts("Font style information tests passed");
    return 0;
}
