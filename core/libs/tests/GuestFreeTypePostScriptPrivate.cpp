#include "FreeTypeFontFixture.hpp"
#include "prx/libSceFreeType/include/PostScriptPrivate.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_TYPE1_TABLES_H

#include <algorithm>
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

namespace FreeType {
struct GuestLibrary;
}

using namespace FreeType;

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "PostScript private dictionary check failed at line %u\n", location.line());
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
    int (APS5_VABI *initialize)(GuestLibrary**);
    int (APS5_VABI *doneLibrary)(GuestLibrary*);
    int (APS5_VABI *newFace)(GuestLibrary*, const unsigned char*, std::int64_t, std::int64_t, GuestFaceRec**);
    int (APS5_VABI *doneFace)(GuestFaceRec*);
    int (APS5_VABI *getPrivate)(GuestFaceRec*, GuestPSPrivate*);
};

static std::string Numbers(unsigned count, int start) {
    std::string result = "[";
    for (unsigned index = 0; index < count; ++index) result += std::to_string(start + static_cast<int>(index) * 10) + " ";
    return result + "]";
}

static std::vector<unsigned char> Type1(unsigned profile) {
    std::string font = "%!PS-AdobeFont-1.0: AnyPS5Private 1.0\n11 dict begin\n"
        "/FontName /AnyPS5Private def\n/FontType 1 def\n/PaintType 0 def\n"
        "/FontMatrix [0.001 0 0 0.001 0 0] def\n/FontBBox [0 0 500 700] def\n"
        "/Encoding StandardEncoding def\n/FontInfo 0 dict def\ncurrentdict end\ncurrentfile eexec\n";
    std::string program = "ABCDdup /Private 24 dict dup begin\n"
        "/RD {string currentfile exch readstring pop} bind def\n/ND {noaccess def} bind def\n"
        "/NP {noaccess put} bind def\n/lenIV -1 def\n/Subrs 0 array def\n";
    if (profile != 0) {
        const bool largest = profile == 2;
        program += "/UniqueID 1234567 def\n/BlueValues " + Numbers(largest ? 14 : 4, -50) + " def\n";
        program += "/OtherBlues " + Numbers(largest ? 10 : 2, -300) + " def\n";
        program += "/FamilyBlues " + Numbers(largest ? 14 : 4, -49) + " def\n";
        program += "/FamilyOtherBlues " + Numbers(largest ? 10 : 2, -299) + " def\n";
        program += profile == 3 ? "/BlueScale -0.01 def\n/ExpansionFactor -0.25 def\n" :
            "/BlueScale 0.039625 def\n/ExpansionFactor 0.06 def\n";
        program += "/BlueShift 7 def\n/BlueFuzz 2 def\n/StdHW [75] def\n/StdVW [80] def\n";
        program += "/StemSnapH " + Numbers(largest ? 12 : 2, 75) + " def\n";
        program += "/StemSnapV " + Numbers(largest ? 12 : 2, 80) + " def\n";
        program += profile == 1 ? "/ForceBold true def\n" : "/ForceBold false def\n";
        program += "/LanguageGroup 1 def\n/password 5839 def\n/MinFeature [16 17] def\n";
    }
    program += "2 index /CharStrings 1 dict dup begin\n/.notdef 5 RD ";
    constexpr std::array<unsigned char, 5> charstring{139, 248, 136, 13, 14};
    for (const unsigned char byte : charstring) program += static_cast<char>(byte);
    program += " ND\nend end readonly put noaccess put\ndup /FontName get exch definefont pop\nmark currentfile closefile\n";
    constexpr char digits[] = "0123456789ABCDEF";
    std::uint16_t seed = 55665;
    for (const unsigned char byte : program) {
        const auto cipher = static_cast<unsigned char>(byte ^ (seed >> 8));
        seed = static_cast<std::uint16_t>((static_cast<unsigned>(cipher) + seed) * 52845u + 22719u);
        font += digits[cipher >> 4];
        font += digits[cipher & 15];
    }
    font += '\n';
    font.append(512, '0');
    font += "\ncleartomark\n";
    return {font.begin(), font.end()};
}

static void Compare(const GuestPSPrivate& output, const PS_PrivateRec& native) {
    Require(output.uniqueId == native.unique_id && output.lenIV == native.lenIV);
    Require(output.numBlueValues == native.num_blue_values && output.numOtherBlues == native.num_other_blues);
    Require(output.numFamilyBlues == native.num_family_blues && output.numFamilyOtherBlues == native.num_family_other_blues);
    Require(std::equal(std::begin(output.blueValues), std::end(output.blueValues), std::begin(native.blue_values)));
    Require(std::equal(std::begin(output.otherBlues), std::end(output.otherBlues), std::begin(native.other_blues)));
    Require(std::equal(std::begin(output.familyBlues), std::end(output.familyBlues), std::begin(native.family_blues)));
    Require(std::equal(std::begin(output.familyOtherBlues), std::end(output.familyOtherBlues), std::begin(native.family_other_blues)));
    Require(output.blueScale == native.blue_scale && output.blueShift == native.blue_shift && output.blueFuzz == native.blue_fuzz);
    Require(output.standardWidth[0] == native.standard_width[0] && output.standardHeight[0] == native.standard_height[0]);
    Require(output.numSnapWidths == native.num_snap_widths && output.numSnapHeights == native.num_snap_heights);
    Require(output.forceBold == native.force_bold && output.roundStemUp == native.round_stem_up);
    Require(std::equal(std::begin(output.snapWidths), std::end(output.snapWidths), std::begin(native.snap_widths)));
    Require(std::equal(std::begin(output.snapHeights), std::end(output.snapHeights), std::begin(native.snap_heights)));
    Require(output.expansionFactor == native.expansion_factor && output.languageGroup == native.language_group && output.password == native.password);
    Require(std::equal(std::begin(output.minFeature), std::end(output.minFeature), std::begin(native.min_feature)));
}

static void CheckFont(const Api& api, GuestLibrary* library, FT_Library nativeLibrary, const std::vector<unsigned char>& font, int profile) {
    const auto fontBefore = font;
    FT_Face nativeFace = nullptr;
    Require(FT_New_Memory_Face(nativeLibrary, font.data(), static_cast<FT_Long>(font.size()), 0, &nativeFace) == 0);
    GuestFaceRec* face = nullptr;
    Require(api.newFace(library, font.data(), static_cast<std::int64_t>(font.size()), 0, &face) == 0);
    std::array<unsigned char, sizeof(GuestFaceRec)> faceBefore{};
    std::memcpy(faceBefore.data(), face, faceBefore.size());
    struct Output {
        std::uint64_t before;
        GuestPSPrivate value;
        std::uint64_t after;
    } output;
    std::memset(&output, 0xA5, sizeof(output));
    std::array<unsigned char, sizeof(output)> untouched{};
    std::memcpy(untouched.data(), &output, sizeof(output));
    PS_PrivateRec native{};
    const int expected = FT_Get_PS_Font_Private(nativeFace, &native);
    Require((expected == 0) == (profile >= 0));
    Require(api.getPrivate(face, &output.value) == expected);
    Require(output.before == 0xA5A5A5A5A5A5A5A5ull && output.after == output.before);
    Require(api.getPrivate(face, nullptr) == FT_Get_PS_Font_Private(nativeFace, nullptr));
    if (expected == 0) {
        Compare(output.value, native);
        Require(output.value.lenIV == -1);
        if (profile != 0) {
            Require(output.value.uniqueId == 1234567 && output.value.blueValues[0] == -50);
            Require(output.value.standardWidth[0] == 75 && output.value.minFeature[1] == 17);
            Require(output.value.numBlueValues == (profile == 2 ? 14 : 4));
            Require(output.value.numSnapWidths == (profile == 2 ? 12 : 2));
        }
        if (profile == 3) Require(output.value.blueScale < 0 && output.value.expansionFactor < 0);
        GuestPSPrivate again{};
        Require(api.getPrivate(face, &again) == 0);
        Compare(again, native);
    } else {
        Require(std::memcmp(&output, untouched.data(), sizeof(output)) == 0);
    }
    Require(std::memcmp(face, faceBefore.data(), faceBefore.size()) == 0);
    Require(api.doneFace(face) == 0 && FT_Done_Face(nativeFace) == 0);
    Require(font == fontBefore);
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
    api.getPrivate = Import<decltype(api.getPrivate)>(module, "FT_Get_PS_Font_Private", "+yNEa4OFH1M", patched);
    GuestPSPrivate output;
    std::memset(&output, 0xA5, sizeof(output));
    std::array<unsigned char, sizeof(output)> untouched{};
    std::memcpy(untouched.data(), &output, sizeof(output));
    Require(api.getPrivate(nullptr, &output) == FT_Get_PS_Font_Private(nullptr, nullptr));
    Require(api.getPrivate(nullptr, nullptr) == FT_Get_PS_Font_Private(nullptr, nullptr));
    Require(std::memcmp(&output, untouched.data(), sizeof(output)) == 0);
    GuestLibrary* library = nullptr;
    FT_Library nativeLibrary = nullptr;
    Require(api.initialize(&library) == 0 && FT_Init_FreeType(&nativeLibrary) == 0);
    for (unsigned profile = 0; profile < 4; ++profile) CheckFont(api, library, nativeLibrary, Type1(profile), static_cast<int>(profile));
    CheckFont(api, library, nativeLibrary, SquareGlyphFont('A', 700), -1);
    Require(api.doneLibrary(library) == 0 && FT_Done_FreeType(nativeLibrary) == 0);
#ifdef _WIN32
    Require(FreeLibrary(module) != 0);
#else
    Require(dlclose(module) == 0);
#endif
}
