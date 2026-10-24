#include "prx/libSceFont/include/FontDriver.hpp"
#include "tests/FontFixture.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

extern "C" {
int APS5_VABI sceFontMemoryInit(FontMemory*, void*, std::uint32_t, const FontMemoryInterface*, void*, FontMemoryDestroyFunction, void*);
int APS5_VABI sceFontMemoryTerm(FontMemory*);
int APS5_VABI sceFontCreateLibrary(const FontMemory*, const void*, FontLibrary*);
int APS5_VABI sceFontDestroyLibrary(FontLibrary*);
int APS5_VABI sceFontSupportExternalFonts(FontLibrary, std::uint32_t, std::uint32_t);
int APS5_VABI sceFontOpenFontMemory(FontLibrary, const void*, std::uint32_t, const FontOpenDetail*, FontHandle*);
int APS5_VABI sceFontCloseFont(FontHandle);
int APS5_VABI sceFontSetScalePixel(FontHandle, float, float);
int APS5_VABI sceFontSetEffectWeight(FontHandle, float, float, std::uint32_t);
int APS5_VABI sceFontSetEffectSlant(FontHandle, float);
int APS5_VABI sceFontGetFontMetrics(FontHandle, std::uint32_t, FontMetrics*);
const void* APS5_VABI sceFontSelectLibraryFt(int);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Font metrics check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

struct Expected {
    std::uint32_t form;
    float ascender;
    float descender;
    float lineGap;
    float maxAdvance;
    float verticalDescender;
    float verticalAscender;
};

static FontMetrics Metrics(FontHandle font, std::uint32_t form) {
    FontMetrics metrics{};
    std::memset(&metrics, 0x7F, sizeof(metrics));
    Require(sceFontGetFontMetrics(font, form, &metrics) == SCE_FONT_OK);
    return metrics;
}

static void RequireMetrics(FontHandle font, std::uint32_t form, const Expected& expected) {
    const FontMetrics metrics = Metrics(font, form);
    Require(metrics.form == expected.form);
    Require(metrics.ascender == expected.ascender && metrics.descender == expected.descender && metrics.lineGap == expected.lineGap);
    Require(metrics.maxAdvance == expected.maxAdvance);
    Require(metrics.verticalDescender == expected.verticalDescender && metrics.verticalAscender == expected.verticalAscender);
    Require(metrics.xMin == 0.0f && metrics.yMin == 0.0f && metrics.xMax == 0.0f && metrics.yMax == 0.0f);
    Require(metrics.emWidth == 99.984375f && metrics.emHeight == 99.984375f);
}

static void* APS5_VABI Allocate(void*, std::uint32_t size) {
    return std::malloc(size);
}

static void APS5_VABI Release(void*, void* pointer) {
    std::free(pointer);
}

using FontFixture::BuildFont;
using FontFixture::Words;

static std::vector<unsigned char> Os2Table() {
    return Words({0, 500, 400, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0x20, 0x7E, 700, -300, 100, 900, 250});
}

static FontHandle Open(FontLibrary library, const std::vector<unsigned char>& data) {
    FontHandle font = nullptr;
    Require(sceFontOpenFontMemory(library, data.data(), static_cast<std::uint32_t>(data.size()), nullptr, &font) == SCE_FONT_OK && font != nullptr);
    Require(sceFontSetScalePixel(font, 100.0f, 100.0f) == SCE_FONT_OK);
    return font;
}

static std::vector<unsigned char> VerticalHeader(int major) {
    return Words({major, 0, 880, -120, 60, 1000, 40, 0, 950, 0, 0, 0, 0, 0, 0, 0, 0, 1});
}

int main() {
    const FontMemoryInterface iface{Allocate, Release, nullptr, nullptr, nullptr, nullptr};
    FontMemory memory{};
    Require(sceFontMemoryInit(&memory, nullptr, 0, &iface, nullptr, nullptr, nullptr) == SCE_FONT_OK);
    FontLibrary library = nullptr;
    Require(sceFontCreateLibrary(&memory, sceFontSelectLibraryFt(0), &library) == SCE_FONT_OK);
    Require(sceFontSupportExternalFonts(library, 4, 0x52) == SCE_FONT_OK);

    const auto plainData = BuildFont(1, {});
    FontHandle plain = Open(library, plainData);
    RequireMetrics(plain, 1, {1, 80.0f, -20.0f, 0.0f, 50.0f, 0.0f, 0.0f});
    RequireMetrics(plain, 2, {1, 80.0f, -20.0f, 0.0f, 50.0f, 0.0f, 0.0f});
    RequireMetrics(plain, 3, {1, 80.0f, -20.0f, 0.0f, 50.0f, 0.0f, 0.0f});
    RequireMetrics(plain, 4, {4, 0.0f, 100.0f, 0.0f, 100.0f, -50.0f, 80.0f});
    FontMetrics untouched{};
    untouched.ascender = 5.0f;
    untouched.descender = 6.0f;
    untouched.lineGap = 7.0f;
    Require(sceFontGetFontMetrics(plain, 7, &untouched) == SCE_FONT_OK);
    Require(untouched.form == 7 && untouched.ascender == 5.0f && untouched.descender == 6.0f && untouched.lineGap == 7.0f);
    Require(untouched.maxAdvance == 50.0f && untouched.verticalDescender == 0.0f && untouched.verticalAscender == 0.0f);

    const auto typoData = BuildFont(1, {{"OS/2", Os2Table()}, {"vhea", VerticalHeader(0x11)}, {"vmtx", Words({1000, 40})}});
    FontHandle typo = Open(library, typoData);
    RequireMetrics(typo, 1, {1, 80.0f, -20.0f, 0.0f, 50.0f, 0.0f, 0.0f});
    RequireMetrics(typo, 2, {2, 70.0f, -30.0f, 10.0f, 50.0f, 0.0f, 0.0f});
    RequireMetrics(typo, 3, {3, 90.0f, -25.0f, 0.0f, 50.0f, 0.0f, 0.0f});
    RequireMetrics(typo, 4, {4, 4.0f, 95.0f, -93.984375f, 100.0f, -12.0f, 70.0f});
    Require(sceFontSetEffectWeight(typo, 1.03125f, 0.984375f, 0) == SCE_FONT_OK);
    Require(sceFontSetEffectSlant(typo, 0.25f) == SCE_FONT_OK);
    const FontMetrics styled = Metrics(typo, 2);
    Require(styled.effectWeightX == 0.03125f && styled.effectWeightY == -0.015625f && styled.slantRatio == 0.25f);
    Require(sceFontSetEffectWeight(typo, 1.5f, 0.5f, 0) == SCE_FONT_OK);
    const FontMetrics clamped = Metrics(typo, 2);
    Require(clamped.effectWeightX == 0.04f && clamped.effectWeightY == -0.04f);

    const auto versionOneData = BuildFont(1, {{"vhea", VerticalHeader(1)}, {"vmtx", Words({1000, 40})}});
    FontHandle versionOne = Open(library, versionOneData);
    RequireMetrics(versionOne, 4, {4, 4.0f, 95.0f, 0.0f, 100.0f, -12.0f, 4.0f});

    const auto os2OnlyData = BuildFont(1, {{"OS/2", Os2Table()}});
    FontHandle os2Only = Open(library, os2OnlyData);
    RequireMetrics(os2Only, 4, {4, 70.0f, 70.0f, 0.0f, 100.0f, -50.0f, 70.0f});

    FontMetrics cleared{};
    std::memset(&cleared, 0x7F, sizeof(cleared));
    Require(sceFontGetFontMetrics(nullptr, 1, &cleared) == SCE_FONT_ERROR_INVALID_FONT_HANDLE);
    Require(cleared.form == 0 && cleared.ascender == 0.0f && cleared.maxAdvance == 0.0f && cleared.emHeight == 0.0f);
    Require(cleared.effectWeightX == 1.0f && cleared.effectWeightY == 1.0f && cleared.slantRatio == 0.0f);
    std::uint64_t notAFont[32]{};
    std::memset(&cleared, 0x7F, sizeof(cleared));
    Require(sceFontGetFontMetrics(reinterpret_cast<FontHandle>(notAFont), 1, &cleared) == SCE_FONT_ERROR_INVALID_FONT_HANDLE);
    Require(cleared.form == 0 && cleared.effectWeightX == 1.0f);
    Require(sceFontGetFontMetrics(plain, 1, nullptr) == SCE_FONT_ERROR_INVALID_FONT_HANDLE);
    Require(sceFontGetFontMetrics(nullptr, 1, nullptr) == SCE_FONT_ERROR_INVALID_FONT_HANDLE);

    for (FontHandle font : {plain, typo, versionOne, os2Only}) Require(sceFontCloseFont(font) == SCE_FONT_OK);
    Require(sceFontDestroyLibrary(&library) == SCE_FONT_OK);
    Require(sceFontMemoryTerm(&memory) == SCE_FONT_OK);
    std::puts("Font metrics tests passed");
    return 0;
}
