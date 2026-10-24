#include "FreeTypeFontFixture.hpp"
#include "prx/libSceFreeType/include/GuestFreeType.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_ADVANCES_H

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace FreeType;

extern "C" {
int APS5_VABI FT_Init_FreeType_nid_postfix(void**);
int APS5_VABI FT_Done_FreeType_nid_postfix(void*);
int APS5_VABI FT_New_Memory_Face_nid_postfix(void*, const unsigned char*, std::int64_t, std::int64_t, GuestFaceRec**);
int APS5_VABI FT_Set_Pixel_Sizes_nid_postfix(GuestFaceRec*, std::uint32_t, std::uint32_t);
int APS5_VABI FT_Load_Glyph_nid_postfix(GuestFaceRec*, std::uint32_t, std::int32_t);
int APS5_VABI FT_Get_Advance_nid_postfix(GuestFaceRec*, std::uint32_t, std::int32_t, std::int64_t*);
int APS5_VABI FT_Get_Advances_nid_postfix(GuestFaceRec*, std::uint32_t, std::uint32_t, std::int32_t, std::int64_t*);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType advance check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

constexpr std::int64_t Sentinel = 0x1122334455667788;

struct Fixture {
    std::vector<unsigned char> bytes;
    FT_Library library = nullptr;
    FT_Face native = nullptr;
    void* guestLibrary = nullptr;
    GuestFaceRec* guest = nullptr;

    explicit Fixture(std::vector<unsigned char> font) : bytes(std::move(font)) {
        Require(FT_Init_FreeType(&library) == 0);
        Require(FT_New_Memory_Face(library, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &native) == 0);
        Require(FT_Init_FreeType_nid_postfix(&guestLibrary) == 0);
        Require(FT_New_Memory_Face_nid_postfix(guestLibrary, bytes.data(), static_cast<std::int64_t>(bytes.size()), 0, &guest) == 0);
        SetSize(37, 53);
    }

    ~Fixture() {
        Require(FT_Done_FreeType_nid_postfix(guestLibrary) == 0);
        Require(FT_Done_FreeType(library) == 0);
    }

    void SetSize(unsigned width, unsigned height) {
        Require(FT_Set_Pixel_Sizes(native, width, height) == 0);
        Require(FT_Set_Pixel_Sizes_nid_postfix(guest, width, height) == 0);
    }

    void Load(unsigned glyph) {
        Require(FT_Load_Glyph(native, glyph, FT_LOAD_DEFAULT) == 0);
        Require(FT_Load_Glyph_nid_postfix(guest, glyph, FT_LOAD_DEFAULT) == 0);
    }

    void CheckSlot() const {
        const auto& actual = *guest->glyph;
        const auto& expected = *native->glyph;
        Require(actual.glyph_index == expected.glyph_index && actual.format == static_cast<std::uint32_t>(expected.format));
        Require(actual.metrics.width == expected.metrics.width && actual.metrics.height == expected.metrics.height);
        Require(actual.metrics.horiBearingX == expected.metrics.horiBearingX && actual.metrics.horiBearingY == expected.metrics.horiBearingY);
        Require(actual.metrics.horiAdvance == expected.metrics.horiAdvance && actual.metrics.vertAdvance == expected.metrics.vertAdvance);
        Require(actual.advance.x == expected.advance.x && actual.advance.y == expected.advance.y);
        Require(actual.linearHoriAdvance == expected.linearHoriAdvance && actual.linearVertAdvance == expected.linearVertAdvance);
        Require(actual.outline.n_points == expected.outline.n_points && actual.outline.n_contours == expected.outline.n_contours);
        Require(actual.outline.flags == expected.outline.flags);
        for (unsigned index = 0; index < actual.outline.n_points; ++index) {
            Require(actual.outline.points[index].x == expected.outline.points[index].x);
            Require(actual.outline.points[index].y == expected.outline.points[index].y);
        }
    }
};

static void CheckSingle(Fixture& fixture, unsigned glyph, std::int32_t flags) {
    fixture.Load(1);
    FT_Fixed expected = static_cast<FT_Fixed>(Sentinel);
    const int error = FT_Get_Advance(fixture.native, glyph, flags, &expected);
    std::array<std::int64_t, 3> actual{Sentinel, Sentinel, Sentinel};
    Require(FT_Get_Advance_nid_postfix(fixture.guest, glyph, flags, &actual[1]) == error);
    Require(actual.front() == Sentinel && actual.back() == Sentinel);
    if (error == 0 || sizeof(FT_Fixed) == sizeof(std::int64_t)) Require(actual[1] == expected);
    else Require(actual[1] == Sentinel);
    fixture.CheckSlot();
}

static void CheckBatch(Fixture& fixture, unsigned start, unsigned count, std::int32_t flags) {
    fixture.Load(1);
    std::array<FT_Fixed, 4> expected;
    expected.fill(static_cast<FT_Fixed>(Sentinel));
    const int error = FT_Get_Advances(fixture.native, start, count, flags, expected.data());
    std::array<std::int64_t, 6> actual;
    actual.fill(Sentinel);
    Require(FT_Get_Advances_nid_postfix(fixture.guest, start, count, flags, &actual[1]) == error);
    Require(actual.front() == Sentinel && actual.back() == Sentinel);
    for (unsigned index = 0; index < expected.size(); ++index) {
        if ((error == 0 && index < count) || sizeof(FT_Fixed) == sizeof(std::int64_t)) Require(actual[index + 1] == expected[index]);
        else Require(actual[index + 1] == Sentinel);
    }
    fixture.CheckSlot();
}

static void CheckFastPreservesGuestEdits(Fixture& fixture) {
    fixture.Load(1);
    auto* slot = fixture.guest->glyph;
    Require(slot->outline.n_points > 0);
    slot->outline.points[0].x += 137;
    slot->outline.flags ^= 2;
    const auto point = slot->outline.points[0].x;
    const auto outlineFlags = slot->outline.flags;
    const auto slotBefore = *slot;
    std::array<std::int64_t, 2> actual{Sentinel, Sentinel};
    Require(FT_Get_Advances_nid_postfix(fixture.guest, 0, 2, FT_LOAD_NO_SCALE | FT_ADVANCE_FLAG_FAST_ONLY, actual.data()) == 0);
    Require(std::memcmp(&slotBefore, slot, sizeof(slotBefore)) == 0);
    Require(slot->outline.points[0].x == point && slot->outline.flags == outlineFlags);
    Require(FT_Get_Advance_nid_postfix(fixture.guest, 0, FT_LOAD_DEFAULT | FT_ADVANCE_FLAG_FAST_ONLY, actual.data()) == FT_Err_Unimplemented_Feature);
    Require(slot->outline.points[0].x == point && slot->outline.flags == outlineFlags);
    Require(FT_Get_Advance_nid_postfix(fixture.guest, 0, FT_LOAD_DEFAULT, actual.data()) == 0);
    FT_Fixed expected = 0;
    Require(FT_Get_Advance(fixture.native, 0, FT_LOAD_DEFAULT, &expected) == 0);
    fixture.CheckSlot();
}

static void CheckAliasedOutput(Fixture& fixture) {
    fixture.Load(1);
    Require(FT_Get_Advance(fixture.native, 0, FT_LOAD_DEFAULT, &fixture.native->glyph->advance.x) == 0);
    Require(FT_Get_Advance_nid_postfix(fixture.guest, 0, FT_LOAD_DEFAULT, &fixture.guest->glyph->advance.x) == 0);
    fixture.CheckSlot();
}

static std::uint32_t Read32(const std::vector<unsigned char>& bytes, std::size_t offset) {
    return (std::uint32_t{bytes[offset]} << 24) | (std::uint32_t{bytes[offset + 1]} << 16) |
           (std::uint32_t{bytes[offset + 2]} << 8) | bytes[offset + 3];
}

static std::vector<unsigned char> BrokenGlyphFont() {
    auto bytes = SquareGlyphFont('A', 913);
    for (std::size_t offset = 12; offset + 16 <= bytes.size(); offset += 16) {
        if (std::memcmp(bytes.data() + offset, "glyf", 4) != 0) continue;
        const auto glyph = Read32(bytes, offset + 8);
        bytes[glyph] = 0x7F;
        bytes[glyph + 1] = 0xFF;
        return bytes;
    }
    std::abort();
}

static void CffIndex(std::vector<unsigned char>& font, const std::vector<std::vector<unsigned char>>& entries) {
    Put16(font, static_cast<std::uint32_t>(entries.size()));
    if (entries.empty()) return;
    font.push_back(1);
    std::size_t offset = 1;
    font.push_back(static_cast<unsigned char>(offset));
    for (const auto& entry : entries) {
        offset += entry.size();
        Require(offset <= 255);
        font.push_back(static_cast<unsigned char>(offset));
    }
    for (const auto& entry : entries) font.insert(font.end(), entry.begin(), entry.end());
}

static std::vector<unsigned char> CffFont() {
    std::vector<unsigned char> font{1, 0, 4, 1};
    CffIndex(font, {{'A', 'd', 'v', 'a', 'n', 'c', 'e'}});
    const auto topDict = font.size();
    CffIndex(font, {{0, 17}});
    CffIndex(font, {});
    CffIndex(font, {});
    Require(font.size() < 107);
    font[topDict + 5] = static_cast<unsigned char>(font.size() + 139);
    CffIndex(font, {{249, 80, 14}, {254, 37, 239, 139, 21, 139, 249, 80, 248, 136, 139, 139, 253, 80, 5, 14}});
    return font;
}

static std::vector<unsigned char> OtfCffFont() {
    const auto original = SquareGlyphFont('A', 913);
    using Table = std::pair<std::array<unsigned char, 4>, std::vector<unsigned char>>;
    std::vector<Table> tables{{{'C', 'F', 'F', ' '}, CffFont()}};
    for (std::size_t record = 12; record < 12 + 7 * 16; record += 16) {
        if (std::memcmp(original.data() + record, "glyf", 4) == 0 || std::memcmp(original.data() + record, "loca", 4) == 0) continue;
        std::array<unsigned char, 4> tag;
        std::memcpy(tag.data(), original.data() + record, tag.size());
        const auto start = Read32(original, record + 8);
        const auto length = Read32(original, record + 12);
        tables.push_back({tag, {original.begin() + start, original.begin() + start + length}});
    }
    std::vector<unsigned char> font;
    Put32(font, 0x4F54544F);
    PutAll16(font, {6, 64, 2, 32});
    std::size_t offset = 12 + tables.size() * 16;
    for (const auto& [tag, data] : tables) {
        font.insert(font.end(), tag.begin(), tag.end());
        Put32(font, 0);
        Put32(font, static_cast<std::uint32_t>(offset));
        Put32(font, static_cast<std::uint32_t>(data.size()));
        offset += (data.size() + 3) & ~std::size_t{3};
    }
    for (const auto& [tag, data] : tables) {
        font.insert(font.end(), data.begin(), data.end());
        font.resize((font.size() + 3) & ~std::size_t{3});
    }
    return font;
}

static void CheckCff() {
    Fixture fixture(CffFont());
    for (const std::int32_t flags : std::array<std::int32_t, 4>{FT_LOAD_NO_SCALE, FT_LOAD_NO_HINTING,
             FT_ADVANCE_FLAG_FAST_ONLY | FT_LOAD_NO_SCALE, FT_LOAD_DEFAULT}) {
        CheckSingle(fixture, 1, flags);
        CheckBatch(fixture, 0, 2, flags);
    }
    fixture.Load(1);
    Require(fixture.guest->glyph->outline.n_points > 0);
    fixture.guest->glyph->outline.points[0].x += 137;
    FT_Fixed expected = 0;
    std::int64_t actual = Sentinel;
    Require(FT_Get_Advance(fixture.native, 1, FT_LOAD_NO_SCALE, &expected) == 0 && expected < 0);
    Require(FT_Get_Advance_nid_postfix(fixture.guest, 1, FT_LOAD_NO_SCALE, &actual) == 0 && actual == expected);
    fixture.CheckSlot();

    Fixture otf(OtfCffFont());
    for (const std::int32_t flags : std::array<std::int32_t, 3>{FT_LOAD_NO_SCALE, FT_LOAD_NO_SCALE | FT_LOAD_VERTICAL_LAYOUT, FT_LOAD_DEFAULT}) {
        CheckSingle(otf, 1, flags);
        CheckBatch(otf, 0, 2, flags);
    }
    otf.Load(1);
    Require(otf.guest->glyph->outline.n_points > 0);
    otf.guest->glyph->outline.points[0].x += 137;
    const auto point = otf.guest->glyph->outline.points[0].x;
    Require(FT_Get_Advance_nid_postfix(otf.guest, 1, FT_LOAD_NO_SCALE, &actual) == 0);
    Require(otf.guest->glyph->outline.points[0].x == point);
    const auto flags = FT_LOAD_NO_SCALE | FT_LOAD_VERTICAL_LAYOUT;
    Require(FT_Get_Advance(otf.native, 1, flags, &expected) == 0);
    Require(FT_Get_Advance_nid_postfix(otf.guest, 1, flags, &actual) == 0 && actual == expected);
    otf.CheckSlot();
}

static void CheckPartialFailure() {
    Fixture fixture(BrokenGlyphFont());
    std::array<FT_Fixed, 2> expected{static_cast<FT_Fixed>(Sentinel), static_cast<FT_Fixed>(Sentinel)};
    const int error = FT_Get_Advances(fixture.native, 0, 2, FT_LOAD_DEFAULT, expected.data());
    Require(error != 0 && expected[0] != static_cast<FT_Fixed>(Sentinel) && expected[1] == static_cast<FT_Fixed>(Sentinel));
    std::array<std::int64_t, 4> actual{Sentinel, Sentinel, Sentinel, Sentinel};
    if constexpr (sizeof(FT_Fixed) == sizeof(std::int64_t)) {
        Require(FT_Get_Advances_nid_postfix(fixture.guest, 0, 2, FT_LOAD_DEFAULT, &actual[1]) == error);
        Require(actual[1] == expected[0] && actual[2] == Sentinel);
    } else {
        bool threw = false;
        try {
            FT_Get_Advances_nid_postfix(fixture.guest, 0, 2, FT_LOAD_DEFAULT, &actual[1]);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw && actual[1] == Sentinel && actual[2] == Sentinel);
    }
    Require(actual.front() == Sentinel && actual.back() == Sentinel);
    fixture.CheckSlot();
}

int main() {
    Fixture fixture(SquareGlyphFont('A', 913));
    for (const unsigned size : {7u, 37u, 113u}) {
        fixture.SetSize(size, size + 11);
        for (const std::int32_t flags : std::array<std::int32_t, 9>{FT_LOAD_DEFAULT, FT_LOAD_NO_SCALE, FT_LOAD_NO_HINTING, FT_LOAD_VERTICAL_LAYOUT,
                 FT_LOAD_NO_SCALE | FT_LOAD_VERTICAL_LAYOUT, FT_LOAD_TARGET_LIGHT, FT_ADVANCE_FLAG_FAST_ONLY,
                 FT_ADVANCE_FLAG_FAST_ONLY | FT_LOAD_NO_SCALE, FT_ADVANCE_FLAG_FAST_ONLY | FT_LOAD_NO_HINTING}) {
            CheckSingle(fixture, 0, flags);
            CheckSingle(fixture, 1, flags);
            CheckBatch(fixture, 0, 2, flags);
            CheckBatch(fixture, 1, 1, flags);
        }
    }
    for (const unsigned glyph : {2u, std::numeric_limits<unsigned>::max()}) CheckSingle(fixture, glyph, FT_LOAD_DEFAULT);
    CheckBatch(fixture, 0, 0, FT_LOAD_DEFAULT);
    CheckBatch(fixture, 1, 0, FT_LOAD_DEFAULT);
    CheckBatch(fixture, 2, 0, FT_LOAD_DEFAULT);
    CheckBatch(fixture, 1, 2, FT_LOAD_DEFAULT);
    CheckBatch(fixture, 1, std::numeric_limits<unsigned>::max(), FT_LOAD_DEFAULT);
    CheckBatch(fixture, std::numeric_limits<unsigned>::max(), 2, FT_LOAD_DEFAULT);

    std::int64_t output = Sentinel;
    Require(FT_Get_Advance_nid_postfix(nullptr, 0, 0, &output) == FT_Err_Invalid_Face_Handle);
    Require(FT_Get_Advance_nid_postfix(nullptr, 0, 0, nullptr) == FT_Err_Invalid_Face_Handle);
    Require(FT_Get_Advance_nid_postfix(fixture.guest, 2, 0, nullptr) == FT_Err_Invalid_Argument);
    Require(FT_Get_Advances_nid_postfix(nullptr, 0, 0, 0, &output) == FT_Err_Invalid_Face_Handle);
    Require(FT_Get_Advances_nid_postfix(nullptr, 0, 0, 0, nullptr) == FT_Err_Invalid_Face_Handle);
    Require(FT_Get_Advances_nid_postfix(fixture.guest, 2, 0, 0, nullptr) == FT_Err_Invalid_Argument);
    Require(output == Sentinel);
    CheckFastPreservesGuestEdits(fixture);
    CheckAliasedOutput(fixture);
    CheckPartialFailure();
    CheckCff();
}
