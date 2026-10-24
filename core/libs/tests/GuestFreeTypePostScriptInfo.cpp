#include "FreeTypeFontFixture.hpp"
#include "prx/libSceFreeType/include/PostScriptInfo.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TYPE1_TABLES_H

#include <array>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <source_location>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

using namespace FreeType;

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "PostScript font information check failed at line %u\n", location.line());
        std::abort();
    }
}

template <typename TFunction, typename TModule>
static TFunction Import(TModule module, const char* name, const char* nid, bool patched) {
    const std::string symbol = patched ? nid : std::string(name) + "_nid_postfix";
#ifdef _WIN32
    auto address = GetProcAddress(module, symbol.c_str());
#else
    auto address = dlsym(module, symbol.c_str());
#endif
    Require(address != nullptr);
    return std::bit_cast<TFunction>(address);
}

struct Api {
    int (APS5_VABI *initialize)(void**);
    int (APS5_VABI *doneLibrary)(void*);
    int (APS5_VABI *newFace)(void*, const unsigned char*, std::int64_t, std::int64_t, GuestFaceRec**);
    int (APS5_VABI *doneFace)(GuestFaceRec*);
    int (APS5_VABI *setSize)(GuestFaceRec*, std::uint32_t, std::uint32_t);
    int (APS5_VABI *loadGlyph)(GuestFaceRec*, std::uint32_t, std::int32_t);
    int (APS5_VABI *info)(GuestFaceRec*, GuestPSFontInfo*);
    int (APS5_VABI *hasNames)(GuestFaceRec*);
};

static std::vector<unsigned char> Type42(bool complete, bool fixed, int angle, int position, unsigned thickness) {
    const auto sfnt = SquareGlyphFont('A', 700);
    std::string font = "%!PS-TrueTypeFont-1.0: AnyPS5Test 1.0\n11 dict begin\n"
        "/FontName /AnyPS5Test def\n/FontType 42 def\n/PaintType 0 def\n"
        "/FontMatrix [1 0 0 1 0 0] def\n/FontBBox [0 0 700 800] def\n"
        "/Encoding StandardEncoding def\n/FontInfo 9 dict dup begin\n";
    if (complete) {
        font += "/version (7.25) def\n/Notice (Generated test data) def\n"
            "/FullName (AnyPS5 Test Face) def\n/FamilyName (AnyPS5 Test Family) def\n/Weight (Book) def\n";
    }
    font += "/ItalicAngle " + std::to_string(angle) + " def\n/isFixedPitch " + (fixed ? "true" : "false") + " def\n";
    font += "/UnderlinePosition " + std::to_string(position) + " def\n/UnderlineThickness " + std::to_string(thickness) + " def\n";
    font += "end readonly def\n/CharStrings 2 dict dup begin\n/.notdef 0 def\n/A 1 def\nend readonly def\n/sfnts [<";
    constexpr char digits[] = "0123456789ABCDEF";
    for (std::size_t index = 0; index < sfnt.size(); ++index) {
        if (index != 0 && index % 64 == 0) font += ">\n<";
        const unsigned char byte = sfnt[index];
        font += digits[byte >> 4];
        font += digits[byte & 15];
    }
    font += ">] def\nFontName currentdict end definefont pop\n";
    return {font.begin(), font.end()};
}

static std::vector<unsigned char> NamedTrueType() {
    auto font = SquareGlyphFont('A', 700);
    const unsigned count = (static_cast<unsigned>(font[4]) << 8) | font[5];
    auto write16 = [&](std::size_t offset, unsigned value) {
        font[offset] = static_cast<unsigned char>(value >> 8);
        font[offset + 1] = static_cast<unsigned char>(value);
    };
    auto write32 = [&](std::size_t offset, std::uint32_t value) {
        write16(offset, value >> 16);
        write16(offset + 2, value & 0xFFFF);
    };
    const std::size_t directoryEnd = 12 + count * 16;
    font.insert(font.begin() + static_cast<std::ptrdiff_t>(directoryEnd), 16, 0);
    write16(4, count + 1);
    write16(6, 128);
    write16(8, 3);
    write16(10, 0);
    for (unsigned index = 0; index < count; ++index) {
        const std::size_t field = 12 + index * 16 + 8;
        const auto old = (static_cast<std::uint32_t>(font[field]) << 24) | (static_cast<std::uint32_t>(font[field + 1]) << 16) |
            (static_cast<std::uint32_t>(font[field + 2]) << 8) | font[field + 3];
        write32(field, old + 16);
    }
    std::vector<unsigned char> post;
    Put32(post, 0x00020000);
    for (int index = 0; index < 7; ++index) Put32(post, 0);
    PutAll16(post, {2, 0, 36});
    std::memcpy(font.data() + directoryEnd, "post", 4);
    write32(directoryEnd + 8, static_cast<std::uint32_t>(font.size()));
    write32(directoryEnd + 12, static_cast<std::uint32_t>(post.size()));
    font.insert(font.end(), post.begin(), post.end());
    return font;
}

static void CheckText(const char* actual, const char* expected) {
    Require((actual == nullptr) == (expected == nullptr));
    if (actual) Require(std::strcmp(actual, expected) == 0);
}

static void CheckFont(const Api& api, void* library, FT_Library nativeLibrary, const std::vector<unsigned char>& font, bool supported, bool complete) {
    FT_Face nativeFace = nullptr;
    const int opened = FT_New_Memory_Face(nativeLibrary, font.data(), static_cast<FT_Long>(font.size()), 0, &nativeFace);
    if (opened != 0) std::fprintf(stderr, "Native font load returned %d\n", opened);
    Require(opened == 0);
    GuestFaceRec* face = nullptr;
    Require(api.newFace(library, font.data(), static_cast<std::int64_t>(font.size()), 0, &face) == 0);
    std::array<unsigned char, sizeof(GuestFaceRec)> faceBefore{};
    std::memcpy(faceBefore.data(), face, faceBefore.size());
    struct Output {
        std::uint64_t before;
        GuestPSFontInfo info;
        std::uint64_t after;
    } output;
    std::memset(&output, 0xA5, sizeof(output));
    std::array<unsigned char, sizeof(output)> untouched{};
    std::memcpy(untouched.data(), &output, sizeof(output));
    PS_FontInfoRec expected{};
    const int error = FT_Get_PS_Font_Info(nativeFace, &expected);
    Require((error == 0) == supported);
    Require(api.info(face, &output.info) == error);
    Require(output.before == 0xA5A5A5A5A5A5A5A5ull && output.after == output.before);
    const int nativeNames = FT_Has_PS_Glyph_Names(nativeFace);
    Require(nativeNames == (supported ? 1 : 0));
    Require(api.hasNames(face) == nativeNames);
    Require(api.info(face, nullptr) == FT_Get_PS_Font_Info(nativeFace, nullptr));
    if (supported) {
        CheckText(output.info.version, expected.version);
        CheckText(output.info.notice, expected.notice);
        CheckText(output.info.fullName, expected.full_name);
        CheckText(output.info.familyName, expected.family_name);
        CheckText(output.info.weight, expected.weight);
        Require(output.info.italicAngle == expected.italic_angle);
        Require(output.info.isFixedPitch == expected.is_fixed_pitch);
        Require(output.info.underlinePosition == expected.underline_position);
        Require(output.info.underlineThickness == expected.underline_thickness);
        CheckText(output.info.version, complete ? "7.25" : nullptr);
        CheckText(output.info.familyName, complete ? "AnyPS5 Test Family" : nullptr);
        GuestPSFontInfo again{};
        Require(api.info(face, &again) == 0);
        Require(again.version == output.info.version && again.notice == output.info.notice);
        Require(again.fullName == output.info.fullName && again.familyName == output.info.familyName && again.weight == output.info.weight);
    } else {
        Require(std::memcmp(&output, untouched.data(), sizeof(output)) == 0);
        Require(FT_HAS_GLYPH_NAMES(nativeFace));
    }
    Require(std::memcmp(face, faceBefore.data(), faceBefore.size()) == 0);
    Require(api.setSize(face, 0, 24) == 0);
    Require(api.loadGlyph(face, 1, FT_LOAD_DEFAULT) == 0);
    if (supported) CheckText(output.info.familyName, complete ? "AnyPS5 Test Family" : nullptr);
    Require(api.doneFace(face) == 0);
    Require(FT_Done_Face(nativeFace) == 0);
}

int main(int argc, char** argv) {
    Require(argc == 3);
    const bool patched = std::strcmp(argv[2], "patched") == 0;
    Require(patched || std::strcmp(argv[2], "unpatched") == 0);
#ifdef _WIN32
    auto module = LoadLibraryW(std::filesystem::path(argv[1]).c_str());
    if (!module) std::fprintf(stderr, "LoadLibraryW failed: %lu\n", GetLastError());
#else
    auto module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!module) std::fprintf(stderr, "%s\n", dlerror());
#endif
    Require(module != nullptr);
    Api api{};
    api.initialize = Import<decltype(api.initialize)>(module, "FT_Init_FreeType", "GNsiLzm4SH8", patched);
    api.doneLibrary = Import<decltype(api.doneLibrary)>(module, "FT_Done_FreeType", "dH02CpEEcYk", patched);
    api.newFace = Import<decltype(api.newFace)>(module, "FT_New_Memory_Face", "40Wa2neSLM0", patched);
    api.doneFace = Import<decltype(api.doneFace)>(module, "FT_Done_Face", "OgFczHko0w8", patched);
    api.setSize = Import<decltype(api.setSize)>(module, "FT_Set_Pixel_Sizes", "lJxMIwj81SQ", patched);
    api.loadGlyph = Import<decltype(api.loadGlyph)>(module, "FT_Load_Glyph", "M156RuYoUbU", patched);
    api.info = Import<decltype(api.info)>(module, "FT_Get_PS_Font_Info", "JQyLCevN3aM", patched);
    api.hasNames = Import<decltype(api.hasNames)>(module, "FT_Has_PS_Glyph_Names", "x8y0qzLiYOU", patched);
    GuestPSFontInfo output;
    std::memset(&output, 0xA5, sizeof(output));
    std::array<unsigned char, sizeof(output)> untouched{};
    std::memcpy(untouched.data(), &output, sizeof(output));
    Require(api.info(nullptr, &output) == FT_Get_PS_Font_Info(nullptr, nullptr));
    Require(api.info(nullptr, nullptr) == FT_Get_PS_Font_Info(nullptr, nullptr));
    Require(std::memcmp(&output, untouched.data(), sizeof(output)) == 0);
    Require(api.hasNames(nullptr) == 0);
    void* library = nullptr;
    FT_Library nativeLibrary = nullptr;
    Require(api.initialize(&library) == 0 && FT_Init_FreeType(&nativeLibrary) == 0);
    CheckFont(api, library, nativeLibrary, Type42(true, true, -17, -32768, 65535), true, true);
    CheckFont(api, library, nativeLibrary, Type42(true, false, 67, 32767, 0), true, true);
    CheckFont(api, library, nativeLibrary, Type42(false, false, 0, -100, 50), true, false);
    CheckFont(api, library, nativeLibrary, NamedTrueType(), false, false);
    Require(api.doneLibrary(library) == 0 && FT_Done_FreeType(nativeLibrary) == 0);
#ifdef _WIN32
    Require(FreeLibrary(module) != 0);
#else
    Require(dlclose(module) == 0);
#endif
}
