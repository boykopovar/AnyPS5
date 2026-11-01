#include "FontFixture.hpp"
#include "Type1Fixture.hpp"
#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include FT_TYPE1_TABLES_H

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
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

int main() {
    {
        FacePair pair(Type1Fixture::Font());
        if constexpr (sizeof(FT_Long) == sizeof(std::int64_t)) {
            bool sized = false;
            for (int key = 0; key < static_cast<int>(PS_DICT_MAX) + 3; ++key) {
                for (const std::uint32_t index : {0u, 1u}) {
                    for (const std::int64_t length : {0LL, 1LL, 3LL, 64LL}) {
                        std::array<unsigned char, 64> actual;
                        std::array<unsigned char, 64> expected;
                        actual.fill(0xA5);
                        expected.fill(0xA5);
                        const std::int64_t actualLength = FT_Get_PS_Font_Value_nid_postfix(&pair.guest.rec, key, index, actual.data(), length);
                        const FT_Long expectedLength = FT_Get_PS_Font_Value(pair.oracle, static_cast<PS_Dict_Keys>(key), index, expected.data(), static_cast<FT_Long>(length));
                        Require(actualLength == expectedLength);
                        Require(actual == expected);
                        sized |= expectedLength > 0;
                    }
                    Require(FT_Get_PS_Font_Value_nid_postfix(&pair.guest.rec, key, index, nullptr, 0) == FT_Get_PS_Font_Value(pair.oracle, static_cast<PS_Dict_Keys>(key), index, nullptr, 0));
                }
            }
            Require(sized);
            char name[32] = {};
            Require(FT_Get_PS_Font_Value_nid_postfix(&pair.guest.rec, PS_DICT_FONT_NAME, 0, name, sizeof(name)) == 13);
            Require(std::strcmp(name, "FixtureType1") == 0);
            if constexpr (sizeof(FT_Long) == 8) {
                std::array<unsigned char, 64> actual;
                std::array<unsigned char, 64> expected;
                actual.fill(0xA5);
                expected.fill(0xA5);
                const auto hugeLength = std::numeric_limits<std::int64_t>::max();
                Require(FT_Get_PS_Font_Value_nid_postfix(&pair.guest.rec, PS_DICT_FONT_NAME, 0, actual.data(), hugeLength) ==
                        FT_Get_PS_Font_Value(pair.oracle, PS_DICT_FONT_NAME, 0, expected.data(), static_cast<FT_Long>(hugeLength)));
                Require(actual == expected);
            }
        }
    }
    {
        FacePair pair(FontFixture::BuildFont(3, {}));
        if constexpr (sizeof(FT_Long) == sizeof(std::int64_t)) {
            std::array<unsigned char, 16> actual{};
            std::array<unsigned char, 16> expected{};
            Require(FT_Get_PS_Font_Value_nid_postfix(&pair.guest.rec, PS_DICT_FONT_NAME, 0, actual.data(), 16) ==
                    FT_Get_PS_Font_Value(pair.oracle, PS_DICT_FONT_NAME, 0, expected.data(), 16));
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
