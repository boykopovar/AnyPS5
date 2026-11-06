#include "FontFixture.hpp"
#include "prx/libSceFreeType/libSceFreeType.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

using namespace FreeType;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType subglyph check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static std::vector<unsigned char> CompositeFont() {
    auto glyf = FontFixture::Words({1, 0, 0, 400, 600, 3, 0});
    glyf.insert(glyf.end(), {1, 1, 1, 1});
    const auto points = FontFixture::Words({0, 400, 0, -400, 0, 0, 600, 0});
    glyf.insert(glyf.end(), points.begin(), points.end());
    const auto simpleEnd = static_cast<int>(glyf.size() / 2);
    const auto transformed = FontFixture::Words({-1, -32768, -32768, 32767, 32767,
        0x0023, 1, -300, 500,
        0x002A, 1, 0x807F, 0x2000,
        0x0063, 1, 32767, -32768, 0x6000, 0xE000,
        0x0083, 1, -500, -600, 0x4000, 0xE000, 0x2000, 0xC000});
    glyf.insert(glyf.end(), transformed.begin(), transformed.end());
    const auto transformedEnd = static_cast<int>(glyf.size() / 2);
    const auto matched = FontFixture::Words({-1, 0, 0, 400, 600, 0x0022, 1, 0, 0x0201, 1, 0, 1});
    glyf.insert(glyf.end(), matched.begin(), matched.end());
    return FontFixture::BuildFont(4, {{"glyf", glyf},
        {"loca", FontFixture::Words({0, 0, simpleEnd, transformedEnd, static_cast<int>(glyf.size() / 2)})},
        {"maxp", FontFixture::Words({1, 0, 4, 4, 1, 16, 4, 2, 0, 0, 0, 0, 0, 0, 4, 1})}});
}

struct Output {
    std::uint64_t before = 0x0123456789ABCDEFu;
    std::int32_t index = -77;
    std::uint32_t flags = 0xABCDEF01u;
    std::int32_t arg1 = -88;
    std::int32_t arg2 = -99;
    GuestMatrix transform{0x1234567812345678ll, -0x1234567812345678ll, 0x2345678923456789ll, -0x2345678923456789ll};
    std::uint64_t after = 0xFEDCBA9876543210u;
};

static bool MatrixMatches(const GuestMatrix& guest, const FT_Matrix& host) {
    return guest.xx == host.xx && guest.xy == host.xy && guest.yx == host.yx && guest.yy == host.yy;
}

static FT_Error Compare(GuestGlyphSlotRec* slot, FT_GlyphSlot native, std::uint32_t subIndex, int& successes, int& errors) {
    Output actual;
    std::array<unsigned char, sizeof(actual)> saved{};
    std::memcpy(saved.data(), &actual, sizeof(actual));
    FT_Int index = -77;
    FT_UInt flags = 0xABCDEF01u;
    FT_Int arg1 = -88;
    FT_Int arg2 = -99;
    FT_Matrix transform{};
    const auto expected = FT_Get_SubGlyph_Info(native, subIndex, &index, &flags, &arg1, &arg2, &transform);
    Require(FT_Get_SubGlyph_Info_nid_postfix(slot, subIndex, &actual.index, &actual.flags, &actual.arg1, &actual.arg2, &actual.transform) == expected);
    if (expected != 0) {
        ++errors;
        Require(std::memcmp(saved.data(), &actual, sizeof(actual)) == 0);
        Require(FT_Get_SubGlyph_Info(native, subIndex, nullptr, nullptr, nullptr, nullptr, nullptr) == expected);
        Require(FT_Get_SubGlyph_Info_nid_postfix(slot, subIndex, nullptr, nullptr, nullptr, nullptr, nullptr) == expected);
    } else {
        ++successes;
        Require(actual.index == index && actual.flags == flags && actual.arg1 == arg1 && actual.arg2 == arg2);
        Require(MatrixMatches(actual.transform, transform));
        Require(actual.before == 0x0123456789ABCDEFu && actual.after == 0xFEDCBA9876543210u);
        FT_Int aliased = 0;
        FT_Int nativeArg2 = 0;
        FT_UInt nativeFlags = 0;
        FT_Matrix nativeMatrix{};
        Require(FT_Get_SubGlyph_Info(native, subIndex, &aliased, &nativeFlags, &aliased, &nativeArg2, &nativeMatrix) == 0);
        Require(FT_Get_SubGlyph_Info_nid_postfix(slot, subIndex, &actual.index, &actual.flags, &actual.index, &actual.arg2, &actual.transform) == 0);
        Require(actual.index == aliased && actual.arg2 == nativeArg2 && actual.flags == nativeFlags && MatrixMatches(actual.transform, nativeMatrix));
    }
    return expected;
}

int main() {
    const auto font = CompositeFont();
    GuestLibrary* library = nullptr;
    FT_Library nativeLibrary = nullptr;
    Require(FT_Init_FreeType_nid_postfix(&library) == 0 && FT_Init_FreeType(&nativeLibrary) == 0);
    GuestFaceRec* face = nullptr;
    FT_Face native = nullptr;
    Require(FT_New_Memory_Face_nid_postfix(library, font.data(), static_cast<std::int64_t>(font.size()), 0, &face) == 0);
    Require(FT_New_Memory_Face(nativeLibrary, font.data(), static_cast<FT_Long>(font.size()), 0, &native) == 0);
    Require(FT_Set_Pixel_Sizes_nid_postfix(face, 0, 16) == 0 && FT_Set_Pixel_Sizes(native, 0, 16) == 0);
    int successes = 0;
    int errors = 0;
    for (const auto index : {0u, std::numeric_limits<std::uint32_t>::max()}) Require(Compare(nullptr, nullptr, index, successes, errors) != 0);
    Require(Compare(face->glyph, native->glyph, 0, successes, errors) != 0);
    for (const auto loadFlags : std::array<std::int32_t, 4>{FT_LOAD_NO_RECURSE, FT_LOAD_NO_RECURSE | FT_LOAD_RENDER,
                                                         FT_LOAD_NO_RECURSE | FT_LOAD_VERTICAL_LAYOUT, FT_LOAD_DEFAULT}) {
        for (const auto glyph : {2u, 1u, 3u, 0u, 2u}) {
            const auto loadError = FT_Load_Glyph(native, glyph, loadFlags);
            Require(loadError == 0 && FT_Load_Glyph_nid_postfix(face, glyph, loadFlags) == loadError);
            const bool composite = glyph > 1 && (loadFlags & FT_LOAD_NO_RECURSE) != 0;
            Require((native->glyph->format == FT_GLYPH_FORMAT_COMPOSITE) == composite);
            Require(face->glyph->format == static_cast<std::uint32_t>(native->glyph->format) && face->glyph->num_subglyphs == native->glyph->num_subglyphs);
            Require(face->glyph->subglyphs == nullptr);
            std::array<unsigned char, sizeof(GuestGlyphSlotRec)> saved{};
            std::memcpy(saved.data(), face->glyph, saved.size());
            const unsigned count = composite ? (glyph == 2 ? 4 : 2) : 0;
            Require(native->glyph->num_subglyphs == count);
            for (unsigned index = 0; index < count; ++index) Require(Compare(face->glyph, native->glyph, index, successes, errors) == 0);
            for (const auto index : {count, count + 1, std::numeric_limits<std::uint32_t>::max()}) Require(Compare(face->glyph, native->glyph, index, successes, errors) != 0);
            Require(std::memcmp(saved.data(), face->glyph, saved.size()) == 0);
        }
    }
    Require(successes == 30 && errors == 63);
    std::printf("FT_Get_SubGlyph_Info: %d component comparisons, %d error comparisons; aliases and unchanged error outputs checked\n", successes, errors);
    Require(FT_Done_FreeType_nid_postfix(library) == 0 && FT_Done_FreeType(nativeLibrary) == 0);
}
