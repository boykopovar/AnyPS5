#include "FontFixture.hpp"
#include "prx/libSceFreeType/include/GuestFreeType.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <utility>
#include <vector>

using namespace FreeType;

extern "C" {
int APS5_VABI FT_Init_FreeType_nid_postfix(void**);
int APS5_VABI FT_Done_FreeType_nid_postfix(void*);
int APS5_VABI FT_New_Memory_Face_nid_postfix(void*, const unsigned char*, std::int64_t, std::int64_t, GuestFaceRec**);
int APS5_VABI FT_Done_Face_nid_postfix(GuestFaceRec*);
int APS5_VABI FT_Set_Charmap_nid_postfix(GuestFaceRec*, GuestCharMapRec*);
std::uint32_t APS5_VABI FT_Face_GetCharVariantIndex_nid_postfix(GuestFaceRec*, std::uint64_t, std::uint64_t);
int APS5_VABI FT_Face_GetCharVariantIsDefault_nid_postfix(GuestFaceRec*, std::uint64_t, std::uint64_t);
std::uint32_t* APS5_VABI FT_Face_GetVariantSelectors_nid_postfix(GuestFaceRec*);
std::uint32_t* APS5_VABI FT_Face_GetVariantsOfChar_nid_postfix(GuestFaceRec*, std::uint64_t);
std::uint32_t* APS5_VABI FT_Face_GetCharsOfVariant_nid_postfix(GuestFaceRec*, std::uint64_t);
}

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "FreeType variant check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static void Put32(std::vector<unsigned char>& bytes, std::uint32_t value) {
    FontFixture::Put16(bytes, static_cast<int>(value >> 16));
    FontFixture::Put16(bytes, static_cast<int>(value & 0xFFFF));
}

static void Put24(std::vector<unsigned char>& bytes, std::uint32_t value) {
    bytes.push_back(static_cast<unsigned char>(value >> 16));
    FontFixture::Put16(bytes, static_cast<int>(value & 0xFFFF));
}

static std::vector<unsigned char> VariationFont(bool baseMaps, bool variations) {
    const auto format4 = FontFixture::Words({4, 32, 0, 4, 4, 1, 0, 'A', 0xFFFF, 0, 'A', 0xFFFF, 3 - 'A', 1, 0, 0});
    auto format12 = FontFixture::Words({12, 0, 0, 16 + 12 * 5, 0, 0, 0, 5});
    for (const auto& mapping : {std::pair{0x41u, 1u}, {0x42u, 2u}, {0x43u, 3u}, {0x4E00u, 3u}, {0x1F600u, 1u}}) {
        Put32(format12, mapping.first);
        Put32(format12, mapping.first);
        Put32(format12, mapping.second);
    }
    std::vector<unsigned char> defaultFirst;
    Put32(defaultFirst, 2);
    Put24(defaultFirst, 'A');
    defaultFirst.push_back(1);
    Put24(defaultFirst, 0x1F600);
    defaultFirst.push_back(0);
    std::vector<unsigned char> otherFirst;
    Put32(otherFirst, 2);
    for (const auto& mapping : {std::pair{0x43u, 2u}, {0x4E00u, 3u}}) {
        Put24(otherFirst, mapping.first);
        FontFixture::Put16(otherFirst, static_cast<int>(mapping.second));
    }
    std::vector<unsigned char> defaultSecond;
    Put32(defaultSecond, 1);
    Put24(defaultSecond, 'B');
    defaultSecond.push_back(1);
    std::vector<unsigned char> otherSecond;
    Put32(otherSecond, 2);
    for (const auto& mapping : {std::pair{0x41u, 3u}, {0x1F600u, 2u}}) {
        Put24(otherSecond, mapping.first);
        FontFixture::Put16(otherSecond, static_cast<int>(mapping.second));
    }
    auto format14 = FontFixture::Words({14});
    const std::uint32_t dataStart = 10 + 2 * 11;
    Put32(format14, dataStart + static_cast<std::uint32_t>(defaultFirst.size() + otherFirst.size() + defaultSecond.size() + otherSecond.size()));
    Put32(format14, 2);
    Put24(format14, 0xFE0F);
    Put32(format14, dataStart);
    Put32(format14, dataStart + static_cast<std::uint32_t>(defaultFirst.size()));
    Put24(format14, 0xE0100);
    Put32(format14, dataStart + static_cast<std::uint32_t>(defaultFirst.size() + otherFirst.size()));
    Put32(format14, dataStart + static_cast<std::uint32_t>(defaultFirst.size() + otherFirst.size() + defaultSecond.size()));
    for (const auto* table : {&defaultFirst, &otherFirst, &defaultSecond, &otherSecond}) format14.insert(format14.end(), table->begin(), table->end());
    struct Map {
        int platform;
        int encoding;
        std::vector<unsigned char> bytes;
    };
    std::vector<Map> maps;
    if (baseMaps) maps = {{3, 10, format12}, {3, 1, format4}, {3, 0, format4}};
    if (variations) maps.push_back({0, 5, format14});
    if (maps.empty()) return FontFixture::BuildFont(4, {});
    auto cmap = FontFixture::Words({0, static_cast<int>(maps.size())});
    auto offset = static_cast<std::uint32_t>(4 + 8 * maps.size());
    for (const auto& map : maps) {
        FontFixture::Put16(cmap, map.platform);
        FontFixture::Put16(cmap, map.encoding);
        Put32(cmap, offset);
        offset += static_cast<std::uint32_t>(map.bytes.size());
    }
    for (const auto& map : maps) cmap.insert(cmap.end(), map.bytes.begin(), map.bytes.end());
    return FontFixture::BuildFont(4, {{"cmap", cmap}});
}

struct FacePair {
    std::vector<unsigned char> bytes;
    GuestFaceRec* guest = nullptr;
    FT_Face native = nullptr;

    FacePair(void* library, FT_Library nativeLibrary, std::vector<unsigned char> font) : bytes(std::move(font)) {
        Require(FT_New_Memory_Face_nid_postfix(library, bytes.data(), static_cast<std::int64_t>(bytes.size()), 0, &guest) == 0);
        Require(FT_New_Memory_Face(nativeLibrary, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &native) == 0);
    }

    ~FacePair() {
        Require(FT_Done_Face_nid_postfix(guest) == 0 && FT_Done_Face(native) == 0);
    }
};

static std::vector<std::uint32_t> CopyList(const std::uint32_t* values) {
    std::vector<std::uint32_t> copy;
    if (!values) return copy;
    for (unsigned index = 0; index < 64; ++index) {
        if (values[index] == 0) return copy;
        copy.push_back(values[index]);
    }
    Require(false);
    return copy;
}

static void ListMatches(const std::uint32_t* guest, const FT_UInt32* native, int& count) {
    Require((guest == nullptr) == (native == nullptr));
    Require(CopyList(guest) == CopyList(native));
    ++count;
}

static void CompareQueries(GuestFaceRec* guest, FT_Face native, int& scalars, int& lists) {
    constexpr std::array<std::uint64_t, 12> characters{'A', 'B', 'C', 'D', 0, 0x4E00, 0x1F600, 0x1F601, 0xFFFFFFFF,
                                                     0x100000041ull, 0x10001F600ull, std::numeric_limits<std::uint64_t>::max()};
    constexpr std::array<std::uint64_t, 8> selectors{0xFE0E, 0xFE0F, 0xE0100, 0xE0101, 0, 0xFFFFFFFF,
                                                   0x10000FE0Full, std::numeric_limits<std::uint64_t>::max()};
    const auto selected = guest ? guest->charmap : nullptr;
    const auto nativeSelected = native ? native->charmap : nullptr;
    std::array<unsigned char, sizeof(GuestFaceRec)> savedFace{};
    std::array<unsigned char, sizeof(GuestGlyphSlotRec)> savedSlot{};
    if (guest) {
        std::memcpy(savedFace.data(), guest, savedFace.size());
        std::memcpy(savedSlot.data(), guest->glyph, savedSlot.size());
    }
    ListMatches(FT_Face_GetVariantSelectors_nid_postfix(guest), FT_Face_GetVariantSelectors(native), lists);
    for (const auto character : characters) {
        ListMatches(FT_Face_GetVariantsOfChar_nid_postfix(guest, character), FT_Face_GetVariantsOfChar(native, static_cast<FT_ULong>(character)), lists);
        for (const auto selector : selectors) {
            Require(FT_Face_GetCharVariantIndex_nid_postfix(guest, character, selector) ==
                    FT_Face_GetCharVariantIndex(native, static_cast<FT_ULong>(character), static_cast<FT_ULong>(selector)));
            Require(FT_Face_GetCharVariantIsDefault_nid_postfix(guest, character, selector) ==
                    FT_Face_GetCharVariantIsDefault(native, static_cast<FT_ULong>(character), static_cast<FT_ULong>(selector)));
            scalars += 2;
        }
    }
    for (const auto selector : selectors) {
        ListMatches(FT_Face_GetCharsOfVariant_nid_postfix(guest, selector), FT_Face_GetCharsOfVariant(native, static_cast<FT_ULong>(selector)), lists);
    }
    if (guest) {
        Require(guest->charmap == selected && native->charmap == nativeSelected);
        Require(std::memcmp(savedFace.data(), guest, savedFace.size()) == 0 && std::memcmp(savedSlot.data(), guest->glyph, savedSlot.size()) == 0);
    }
}

int main() {
    void* library = nullptr;
    FT_Library nativeLibrary = nullptr;
    Require(FT_Init_FreeType_nid_postfix(&library) == 0 && FT_Init_FreeType(&nativeLibrary) == 0);
    int scalars = 0;
    int lists = 0;
    CompareQueries(nullptr, nullptr, scalars, lists);
    {
        FacePair regular(library, nativeLibrary, VariationFont(true, true));
        FacePair noVariants(library, nativeLibrary, VariationFont(true, false));
        FacePair noBase(library, nativeLibrary, VariationFont(false, true));
        FacePair noMap(library, nativeLibrary, VariationFont(false, false));
        Require(regular.guest->num_charmaps == 4 && noBase.guest->charmap != nullptr && noMap.guest->num_charmaps == 0);
        Require(noBase.guest->charmap->encoding_id == 5);
        Require(regular.guest->charmap->encoding_id == 10);
        Require(CopyList(FT_Face_GetVariantSelectors_nid_postfix(regular.guest)) == std::vector<std::uint32_t>({0xFE0F, 0xE0100}));
        Require(CopyList(FT_Face_GetCharsOfVariant_nid_postfix(regular.guest, 0xFE0F)) == std::vector<std::uint32_t>({'A', 'B', 'C', 0x4E00, 0x1F600}));
        Require(CopyList(FT_Face_GetVariantsOfChar_nid_postfix(regular.guest, 'D')).empty());
        Require(FT_Face_GetCharVariantIndex_nid_postfix(regular.guest, 'A', 0xFE0F) == 1);
        Require(FT_Face_GetCharVariantIndex_nid_postfix(regular.guest, 'A', 0xE0100) == 3);
        Require(FT_Face_GetCharVariantIndex_nid_postfix(regular.guest, 0x100000041ull, 0x10000FE0Full) == 1);
        Require(FT_Face_GetCharVariantIsDefault_nid_postfix(regular.guest, 'A', 0xFE0F) == 1);
        Require(FT_Face_GetCharVariantIsDefault_nid_postfix(regular.guest, 'C', 0xFE0F) == 0);
        Require(FT_Face_GetCharVariantIsDefault_nid_postfix(regular.guest, 'D', 0xFE0F) == -1);
        regular.guest->bbox.xMin = -0x123456789ABCll;
        CompareQueries(regular.guest, regular.native, scalars, lists);
        for (int index = 0; index < regular.guest->num_charmaps; ++index) {
            const auto* map = regular.guest->charmaps[index];
            if (map->platform_id != 3 || map->encoding_id == 10) continue;
            Require(FT_Set_Charmap_nid_postfix(regular.guest, regular.guest->charmaps[index]) == 0);
            Require(FT_Set_Charmap(regular.native, regular.native->charmaps[index]) == 0);
            Require(FT_Face_GetCharVariantIndex_nid_postfix(regular.guest, 'A', 0xFE0F) == (map->encoding_id == 1 ? 3u : 0u));
            CompareQueries(regular.guest, regular.native, scalars, lists);
        }
        Require(FT_Face_GetCharVariantIndex_nid_postfix(noBase.guest, 'A', 0xE0100) == 3);
        Require(FT_Face_GetCharVariantIndex_nid_postfix(noBase.guest, 'A', 0xFE0F) == 0);
        Require(FT_Face_GetCharVariantIsDefault_nid_postfix(noBase.guest, 'A', 0xFE0F) == 1);
        CompareQueries(noVariants.guest, noVariants.native, scalars, lists);
        CompareQueries(noBase.guest, noBase.native, scalars, lists);
        CompareQueries(noMap.guest, noMap.native, scalars, lists);
    }
    Require(scalars == 1344 && lists == 147);
    std::printf("FreeType variation selectors: %d scalar comparisons, %d terminated-list comparisons\n", scalars, lists);
    Require(FT_Done_FreeType_nid_postfix(library) == 0 && FT_Done_FreeType(nativeLibrary) == 0);
}
