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
#include <initializer_list>
#include <vector>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Font outline profile check failed at line %d\n", line);
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

static void PatchMaxp(std::vector<unsigned char>& font, std::initializer_list<int> words) {
    const int tableCount = (font[4] << 8) | font[5];
    for (int i = 0; i < tableCount; ++i) {
        const std::size_t entry = 12 + 16 * static_cast<std::size_t>(i);
        if (std::memcmp(font.data() + entry, "maxp", 4) != 0) continue;
        std::size_t offset = (static_cast<std::size_t>(font[entry + 8]) << 24) | (static_cast<std::size_t>(font[entry + 9]) << 16) |
            (static_cast<std::size_t>(font[entry + 10]) << 8) | font[entry + 11];
        for (const int word : words) {
            if (word >= 0) {
                font[offset] = static_cast<unsigned char>((word >> 8) & 0xFF);
                font[offset + 1] = static_cast<unsigned char>(word & 0xFF);
            }
            offset += 2;
        }
        return;
    }
    Require(false);
}

static FontHandle Open(FontLibrary library, const std::vector<unsigned char>& data) {
    FontHandle font = nullptr;
    Require(sceFontOpenFontMemory(library, data.data(), static_cast<std::uint32_t>(data.size()), nullptr, &font) == SCE_FONT_OK && font != nullptr);
    return font;
}

static void Fill(FontOutlineProfile& profile) {
    std::memset(&profile, 0x7F, sizeof(profile));
}

static bool Zero(const FontOutlineProfile& profile) {
    return profile.maxContours == 0 && profile.maxPoints == 0 && profile.maxCompositeContours == 0 && profile.maxCompositePoints == 0;
}

int main() {
    const FontMemoryInterface iface{Allocate, Release, nullptr, nullptr, nullptr, nullptr};
    FontMemory memory{};
    Require(sceFontMemoryInit(&memory, nullptr, 0, &iface, nullptr, nullptr, nullptr) == SCE_FONT_OK);
    FontLibrary library = nullptr;
    Require(sceFontCreateLibrary(&memory, sceFontSelectLibraryFt(0), &library) == SCE_FONT_OK);
    Require(sceFontSupportExternalFonts(library, 4, 0x52) == SCE_FONT_OK);

    auto profiledData = FontFixture::BuildFont(1, {});
    PatchMaxp(profiledData, {-1, -1, -1, 120, 7, 300, 11});
    FontHandle profiled = Open(library, profiledData);
    FontOutlineProfile profile{};
    Fill(profile);
    Require(sceFontGetFontGlyphsOutlineProfile(profiled, &profile) == SCE_FONT_OK);
    Require(profile.maxContours == 7 && profile.maxPoints == 120 && profile.maxCompositeContours == 11 && profile.maxCompositePoints == 300);

    const auto plainData = FontFixture::BuildFont(1, {});
    FontHandle plain = Open(library, plainData);
    Fill(profile);
    Require(sceFontGetFontGlyphsOutlineProfile(plain, &profile) == SCE_FONT_OK && Zero(profile));

    auto shortData = FontFixture::BuildFont(1, {});
    PatchMaxp(shortData, {0, 0x5000});
    FontHandle shortMaxp = Open(library, shortData);
    Fill(profile);
    Require(sceFontGetFontGlyphsOutlineProfile(shortMaxp, &profile) == SCE_FONT_ERROR_NO_SUPPORT_FUNCTION && Zero(profile));

    std::array<std::byte, 116> record{};
    auto* nativeFont = reinterpret_cast<Font::FontHandleNative*>(profiled);
    nativeFont->open_info.fontset_record = record.data();
    Require(sceFontGetFontGlyphsOutlineProfile(profiled, nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    nativeFont->open_info.fontset_record = nullptr;
    Fill(profile);
    Require(sceFontGetFontGlyphsOutlineProfile(profiled, &profile) == SCE_FONT_OK && profile.maxPoints == 120);

    Fill(profile);
    Require(sceFontGetFontGlyphsOutlineProfile(nullptr, &profile) == SCE_FONT_ERROR_INVALID_FONT_HANDLE && Zero(profile));
    std::uint64_t notAFont[32]{};
    Fill(profile);
    Require(sceFontGetFontGlyphsOutlineProfile(reinterpret_cast<FontHandle>(notAFont), &profile) == SCE_FONT_ERROR_INVALID_FONT_HANDLE && Zero(profile));
    Require(sceFontGetFontGlyphsOutlineProfile(plain, nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    Require(sceFontGetFontGlyphsOutlineProfile(nullptr, nullptr) == SCE_FONT_ERROR_INVALID_FONT_HANDLE);
    Require(sceFontGetFontGlyphsOutlineProfile(reinterpret_cast<FontHandle>(notAFont), nullptr) == SCE_FONT_ERROR_INVALID_FONT_HANDLE);
    auto* plainFont = reinterpret_cast<Font::FontHandleNative*>(plain);
    const FontLibrary owner = plainFont->library;
    plainFont->library = nullptr;
    Fill(profile);
    Require(sceFontGetFontGlyphsOutlineProfile(plain, &profile) == SCE_FONT_ERROR_INVALID_FONT_HANDLE && Zero(profile));
    Require(sceFontGetFontGlyphsOutlineProfile(plain, nullptr) == SCE_FONT_ERROR_INVALID_PARAMETER);
    plainFont->library = owner;
    Fill(profile);
    Require(sceFontGetFontGlyphsOutlineProfile(plain, &profile) == SCE_FONT_OK && Zero(profile));

    for (FontHandle font : {profiled, plain, shortMaxp}) Require(sceFontCloseFont(font) == SCE_FONT_OK);
    Require(sceFontDestroyLibrary(&library) == SCE_FONT_OK);
    Require(sceFontMemoryTerm(&memory) == SCE_FONT_OK);
    std::puts("Font outline profile tests passed");
    return 0;
}
