#include "FontFixture.hpp"
#include "Type1Fixture.hpp"
#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_TYPE1_TABLES_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <string_view>
#include <utility>
#include <vector>

using namespace FreeType;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType PS font value check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

struct FacePair {
    std::vector<unsigned char> bytes;
    GuestLibrary library{};
    FT_Library native = nullptr;
    GuestFace guest;
    FT_Face oracle = nullptr;

    explicit FacePair(std::vector<unsigned char> font) : bytes(std::move(font)) {
        Require(FT_Init_FreeType(&library.host) == 0 && FT_Init_FreeType(&native) == 0);
        guest.library = &library;
        Require(FT_New_Memory_Face(library.host, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &guest.host) == 0);
        Require(FT_New_Memory_Face(native, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &oracle) == 0);
        SyncFace(guest);
    }

    ~FacePair() {
        Require(FT_Done_Face(guest.host) == 0 && FT_Done_Face(oracle) == 0);
        Require(FT_Done_FreeType(library.host) == 0 && FT_Done_FreeType(native) == 0);
    }
};

static void Replace(std::vector<unsigned char>& bytes, std::string_view from, std::string_view to) {
    const auto at = std::search(bytes.begin(), bytes.end(), from.begin(), from.end());
    Require(at != bytes.end());
    const auto offset = at - bytes.begin();
    bytes.erase(at, at + static_cast<std::ptrdiff_t>(from.size()));
    bytes.insert(bytes.begin() + offset, to.begin(), to.end());
}

static bool LongValue(int key) {
    return key == PS_DICT_FONT_MATRIX || key == PS_DICT_FONT_BBOX || key == PS_DICT_BLUE_SCALE || key == PS_DICT_PASSWORD ||
           key == PS_DICT_LANGUAGE_GROUP || key == PS_DICT_ITALIC_ANGLE;
}

static std::int64_t Expected(FT_Face oracle, int key, std::uint32_t index, unsigned char* value, std::int64_t length) {
    const auto dictKey = static_cast<PS_Dict_Keys>(key);
    if constexpr (sizeof(FT_Long) == sizeof(std::int64_t)) {
        return FT_Get_PS_Font_Value(oracle, dictKey, index, value, static_cast<FT_Long>(length));
    } else {
        if (!LongValue(key)) {
            const std::int64_t limit = std::numeric_limits<FT_Long>::max();
            return FT_Get_PS_Font_Value(oracle, dictKey, index, value, static_cast<FT_Long>(length > limit ? limit : length));
        }
        const FT_Long required = FT_Get_PS_Font_Value(oracle, dictKey, index, nullptr, 0);
        if (required <= 0) return required;
        Require(required == static_cast<FT_Long>(sizeof(FT_Long)));
        if (!value || length < 8) return 8;
        FT_Long native = 0;
        Require(FT_Get_PS_Font_Value(oracle, dictKey, index, &native, required) == required);
        const std::int64_t widened = native;
        std::memcpy(value, &widened, sizeof(widened));
        return 8;
    }
}

static std::int64_t Widened(GuestFace& guest, int key, std::uint32_t index) {
    std::array<unsigned char, 16> bytes;
    bytes.fill(0xA5);
    Require(FT_Get_PS_Font_Value_nid_postfix(&guest.rec, key, index, bytes.data(), 7) == 8);
    Require(bytes[0] == 0xA5);
    Require(FT_Get_PS_Font_Value_nid_postfix(&guest.rec, key, index, nullptr, 64) == 8);
    Require(FT_Get_PS_Font_Value_nid_postfix(&guest.rec, key, index, bytes.data(), 16) == 8);
    Require(bytes[8] == 0xA5);
    std::int64_t value = 0;
    std::memcpy(&value, bytes.data(), sizeof(value));
    return value;
}

int main() {
    {
        std::vector<unsigned char> font = Type1Fixture::Font();
        Replace(font, "/ItalicAngle 0 def", "/ItalicAngle -12 def");
        Replace(font, "/FontBBox {0 0 600 700}", "/FontBBox {-50 -200 600 700}");
        FacePair pair(std::move(font));
        bool sized = false;
        for (int key = 0; key < static_cast<int>(PS_DICT_MAX) + 3; ++key) {
            for (const std::uint32_t index : {0u, 1u, 3u, 4u}) {
                for (const std::int64_t length : {-1LL, 0LL, 1LL, 3LL, 4LL, 7LL, 8LL, 64LL}) {
                    std::array<unsigned char, 64> actual;
                    std::array<unsigned char, 64> expected;
                    actual.fill(0xA5);
                    expected.fill(0xA5);
                    const std::int64_t actualLength = FT_Get_PS_Font_Value_nid_postfix(&pair.guest.rec, key, index, actual.data(), length);
                    Require(actualLength == Expected(pair.oracle, key, index, expected.data(), length));
                    Require(actual == expected);
                    sized |= actualLength > 0;
                }
                Require(FT_Get_PS_Font_Value_nid_postfix(&pair.guest.rec, key, index, nullptr, 0) == Expected(pair.oracle, key, index, nullptr, 0));
            }
        }
        Require(sized);
        Require(Widened(pair.guest, PS_DICT_ITALIC_ANGLE, 0) < 0);
        Require(Widened(pair.guest, PS_DICT_FONT_BBOX, 0) == -50 * 65536 && Widened(pair.guest, PS_DICT_FONT_BBOX, 1) == -200 * 65536);
        Require(Widened(pair.guest, PS_DICT_FONT_BBOX, 3) == 700 * 65536);
        Require(Widened(pair.guest, PS_DICT_FONT_MATRIX, 0) > 0 && Widened(pair.guest, PS_DICT_FONT_MATRIX, 1) == 0);
        Require(Widened(pair.guest, PS_DICT_PASSWORD, 0) == 5839);
        Require(FT_Get_PS_Font_Value_nid_postfix(&pair.guest.rec, PS_DICT_FONT_BBOX, 4, nullptr, 0) == -1);
        char name[32] = {};
        Require(FT_Get_PS_Font_Value_nid_postfix(&pair.guest.rec, PS_DICT_FONT_NAME, 0, name, sizeof(name)) == 13);
        Require(std::strcmp(name, "FixtureType1") == 0);
        std::array<unsigned char, 64> actual;
        std::array<unsigned char, 64> expected;
        actual.fill(0xA5);
        expected.fill(0xA5);
        const auto hugeLength = std::numeric_limits<std::int64_t>::max();
        Require(FT_Get_PS_Font_Value_nid_postfix(&pair.guest.rec, PS_DICT_FONT_NAME, 0, actual.data(), hugeLength) ==
                Expected(pair.oracle, PS_DICT_FONT_NAME, 0, expected.data(), hugeLength));
        Require(actual == expected);
        actual.fill(0xA5);
        expected.fill(0xA5);
        Require(FT_Get_PS_Font_Value_nid_postfix(&pair.guest.rec, PS_DICT_ITALIC_ANGLE, 0, actual.data(), hugeLength) ==
                Expected(pair.oracle, PS_DICT_ITALIC_ANGLE, 0, expected.data(), hugeLength));
        Require(actual == expected);
    }
    {
        FacePair pair(FontFixture::BuildFont(3, {}));
        for (const int key : {PS_DICT_FONT_NAME, PS_DICT_ITALIC_ANGLE, PS_DICT_FONT_MATRIX}) {
            std::array<unsigned char, 16> actual{};
            std::array<unsigned char, 16> expected{};
            Require(FT_Get_PS_Font_Value_nid_postfix(&pair.guest.rec, key, 0, actual.data(), 16) == Expected(pair.oracle, key, 0, expected.data(), 16));
            Require(actual == expected);
        }
    }
    Require(FT_Get_PS_Font_Value_nid_postfix(nullptr, PS_DICT_FONT_NAME, 0, nullptr, 0) == FT_Get_PS_Font_Value(nullptr, PS_DICT_FONT_NAME, 0, nullptr, 0));
    std::array<unsigned char, 1> untouched{0xA5};
    const auto hugeLength = std::numeric_limits<std::int64_t>::max();
    Require(FT_Get_PS_Font_Value_nid_postfix(nullptr, PS_DICT_FONT_NAME, 0, untouched.data(), hugeLength) ==
            FT_Get_PS_Font_Value(nullptr, PS_DICT_FONT_NAME, 0, untouched.data(), static_cast<FT_Long>(hugeLength)));
    Require(untouched[0] == 0xA5);
    GuestFaceRec empty{};
    Require(FT_Get_PS_Font_Value_nid_postfix(&empty, PS_DICT_FONT_NAME, 0, nullptr, 0) == FT_Get_PS_Font_Value(nullptr, PS_DICT_FONT_NAME, 0, nullptr, 0));
    Require(FT_Get_PS_Font_Value_nid_postfix(&empty, PS_DICT_FONT_NAME, 0, untouched.data(), hugeLength) ==
            FT_Get_PS_Font_Value(nullptr, PS_DICT_FONT_NAME, 0, untouched.data(), static_cast<FT_Long>(hugeLength)));
    Require(untouched[0] == 0xA5);
}
