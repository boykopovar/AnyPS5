#include "prx/libharfbuzz/libharfbuzz.hpp"

#include <hb.h>
#include <hb-ot.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <initializer_list>
#include <map>
#include <source_location>
#include <span>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "HarfBuzz GPOS check failed at line %u\n", location.line());
        std::abort();
    }
}

template <typename TFunction, typename TModule>
static TFunction Import(TModule module, const char* name) {
#ifdef _WIN32
    const auto address = GetProcAddress(module, name);
#else
    const auto address = dlsym(module, name);
#endif
    if (!address) std::fprintf(stderr, "Missing symbol: %s\n", name);
    Require(address != nullptr);
    return std::bit_cast<TFunction>(address);
}

struct GuestApi {
    decltype(&hb_shape_nid_postfix) shape;
    decltype(&hb_blob_create_nid_postfix) blob;
    decltype(&hb_face_create_for_tables_nid_postfix) face;
    decltype(&hb_face_destroy_nid_postfix) destroyFace;
    decltype(&hb_font_create_nid_postfix) font;
    decltype(&hb_font_destroy_nid_postfix) destroyFont;
    decltype(&hb_font_set_scale_nid_postfix) setScale;
    decltype(&hb_buffer_create_nid_postfix) buffer;
    decltype(&hb_buffer_destroy_nid_postfix) destroyBuffer;
    decltype(&hb_buffer_add_utf16_nid_postfix) addUtf16;
    decltype(&hb_buffer_set_direction_nid_postfix) direction;
    decltype(&hb_buffer_set_script_nid_postfix) script;
    decltype(&hb_buffer_get_length_nid_postfix) length;
    decltype(&hb_buffer_get_glyph_infos_nid_postfix) infos;
    decltype(&hb_buffer_get_glyph_positions_nid_postfix) positions;
    decltype(&hb_ot_layout_language_find_feature_nid_postfix) findFeature;
};

struct ByteWriter {
    std::vector<char> bytes;

    void Put16(std::uint16_t value) {
        bytes.push_back(static_cast<char>(value >> 8));
        bytes.push_back(static_cast<char>(value & 0xFF));
    }

    void PutS16(std::int16_t value) {
        Put16(static_cast<std::uint16_t>(value));
    }

    void Put32(std::uint32_t value) {
        Put16(static_cast<std::uint16_t>(value >> 16));
        Put16(static_cast<std::uint16_t>(value & 0xFFFF));
    }

    void PutWords(std::initializer_list<std::uint16_t> values) {
        for (const auto value : values) Put16(value);
    }

    std::size_t Reserve16() {
        const auto pos = bytes.size();
        Put16(0);
        return pos;
    }

    void PatchOffset16(std::size_t pos, std::size_t base, std::size_t target) {
        Require(target >= base && target - base <= 0xFFFFu);
        const auto value = static_cast<std::uint16_t>(target - base);
        bytes[pos] = static_cast<char>(value >> 8);
        bytes[pos + 1] = static_cast<char>(value & 0xFF);
    }
};

static std::vector<char> BuildHeadTable() {
    ByteWriter w;
    w.PutWords({
        1, 0,
        1, 0,
        0, 0,
        0x5F0F, 0x3CF5,
        0,
        1000,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0, 0, 0, 0,
        0,
        8,
        2,
        0,
        0
    });
    return w.bytes;
}

static std::vector<char> BuildHheaTable() {
    ByteWriter w;
    w.PutWords({
        1, 0,
        800,
        0xFF38,
        0,
        650,
        0, 0, 0,
        1, 0,
        0, 0, 0, 0, 0,
        0,
        4
    });
    return w.bytes;
}

static std::vector<char> BuildMaxpTable() {
    ByteWriter w;
    w.PutWords({0, 0x5000, 4});
    return w.bytes;
}

static std::vector<char> BuildHmtxTable() {
    ByteWriter w;
    w.Put16(500);
    w.PutS16(0);
    w.Put16(600);
    w.PutS16(0);
    w.Put16(650);
    w.PutS16(0);
    w.Put16(250);
    w.PutS16(0);
    return w.bytes;
}

static std::vector<char> BuildCmapTable() {
    ByteWriter w;
    w.Put16(0);
    w.Put16(1);
    w.Put16(3);
    w.Put16(1);
    w.Put32(12);
    w.Put16(4);
    w.Put16(48);
    w.Put16(0);
    w.Put16(8);
    w.Put16(8);
    w.Put16(2);
    w.Put16(0);
    w.PutWords({0x0041, 0x0056, 0x0301, 0xFFFF});
    w.Put16(0);
    w.PutWords({0x0041, 0x0056, 0x0301, 0xFFFF});
    w.PutS16(static_cast<std::int16_t>(1 - 0x0041));
    w.PutS16(static_cast<std::int16_t>(2 - 0x0056));
    w.PutS16(static_cast<std::int16_t>(3 - 0x0301));
    w.PutS16(1);
    w.PutWords({0, 0, 0, 0});
    return w.bytes;
}

static std::vector<char> BuildGdefTable() {
    ByteWriter w;
    w.Put16(1);
    w.Put16(0);
    w.Put16(12);
    w.Put16(0);
    w.Put16(0);
    w.Put16(0);
    w.Put16(1);
    w.Put16(1);
    w.Put16(3);
    w.PutWords({1, 1, 3});
    return w.bytes;
}

static void WriteDefaultLangSys(ByteWriter& w) {
    w.Put16(0);
    w.Put16(0xFFFF);
    w.Put16(2);
    w.PutWords({0, 1});
}

static std::vector<char> BuildGposTable() {
    ByteWriter w;
    w.Put16(1);
    w.Put16(0);
    const auto scriptListOffPos = w.Reserve16();
    const auto featureListOffPos = w.Reserve16();
    const auto lookupListOffPos = w.Reserve16();

    const auto scriptListBase = w.bytes.size();
    w.PatchOffset16(scriptListOffPos, 0, scriptListBase);
    w.Put16(2);
    w.Put32(HB_TAG('D', 'F', 'L', 'T'));
    const auto dfltScriptOffPos = w.Reserve16();
    w.Put32(HB_TAG('l', 'a', 't', 'n'));
    const auto latnScriptOffPos = w.Reserve16();

    const auto dfltScriptBase = w.bytes.size();
    w.PatchOffset16(dfltScriptOffPos, scriptListBase, dfltScriptBase);
    w.Put16(4);
    w.Put16(0);
    WriteDefaultLangSys(w);

    const auto latnScriptBase = w.bytes.size();
    w.PatchOffset16(latnScriptOffPos, scriptListBase, latnScriptBase);
    w.Put16(4);
    w.Put16(0);
    WriteDefaultLangSys(w);

    const auto featureListBase = w.bytes.size();
    w.PatchOffset16(featureListOffPos, 0, featureListBase);
    w.Put16(2);
    w.Put32(HB_TAG('k', 'e', 'r', 'n'));
    const auto kernFeatureOffPos = w.Reserve16();
    w.Put32(HB_TAG('m', 'a', 'r', 'k'));
    const auto markFeatureOffPos = w.Reserve16();

    w.PatchOffset16(kernFeatureOffPos, featureListBase, w.bytes.size());
    w.Put16(0);
    w.Put16(1);
    w.Put16(0);

    w.PatchOffset16(markFeatureOffPos, featureListBase, w.bytes.size());
    w.Put16(0);
    w.Put16(1);
    w.Put16(1);

    const auto lookupListBase = w.bytes.size();
    w.PatchOffset16(lookupListOffPos, 0, lookupListBase);
    w.Put16(2);
    const auto lookup0OffPos = w.Reserve16();
    const auto lookup1OffPos = w.Reserve16();

    const auto lookup0Base = w.bytes.size();
    w.PatchOffset16(lookup0OffPos, lookupListBase, lookup0Base);
    w.Put16(2);
    w.Put16(0x0008);
    w.Put16(1);
    const auto pairSubtableOffPos = w.Reserve16();

    const auto pairBase = w.bytes.size();
    w.PatchOffset16(pairSubtableOffPos, lookup0Base, pairBase);
    w.Put16(1);
    const auto pairCovOffPos = w.Reserve16();
    w.Put16(0x0005);
    w.Put16(0x0002);
    w.Put16(1);
    const auto pairSet0OffPos = w.Reserve16();

    w.PatchOffset16(pairCovOffPos, pairBase, w.bytes.size());
    w.Put16(1);
    w.Put16(1);
    w.Put16(1);

    w.PatchOffset16(pairSet0OffPos, pairBase, w.bytes.size());
    w.Put16(1);
    w.Put16(2);
    w.PutS16(15);
    w.PutS16(-80);
    w.PutS16(25);

    const auto lookup1Base = w.bytes.size();
    w.PatchOffset16(lookup1OffPos, lookupListBase, lookup1Base);
    w.Put16(4);
    w.Put16(0);
    w.Put16(1);
    const auto markBaseSubtableOffPos = w.Reserve16();

    const auto markBase = w.bytes.size();
    w.PatchOffset16(markBaseSubtableOffPos, lookup1Base, markBase);
    w.Put16(1);
    const auto markCovOffPos = w.Reserve16();
    const auto baseCovOffPos = w.Reserve16();
    w.Put16(1);
    const auto markArrayOffPos = w.Reserve16();
    const auto baseArrayOffPos = w.Reserve16();

    w.PatchOffset16(markCovOffPos, markBase, w.bytes.size());
    w.Put16(1);
    w.Put16(1);
    w.Put16(3);

    w.PatchOffset16(baseCovOffPos, markBase, w.bytes.size());
    w.Put16(1);
    w.Put16(2);
    w.PutWords({1, 2});

    const auto markArrayBase = w.bytes.size();
    w.PatchOffset16(markArrayOffPos, markBase, markArrayBase);
    w.Put16(1);
    w.Put16(0);
    const auto markAnchorOffPos = w.Reserve16();
    w.PatchOffset16(markAnchorOffPos, markArrayBase, w.bytes.size());
    w.Put16(1);
    w.PutS16(40);
    w.PutS16(480);

    const auto baseArrayBase = w.bytes.size();
    w.PatchOffset16(baseArrayOffPos, markBase, baseArrayBase);
    w.Put16(2);
    const auto base0AnchorOffPos = w.Reserve16();
    const auto base1AnchorOffPos = w.Reserve16();
    w.PatchOffset16(base0AnchorOffPos, baseArrayBase, w.bytes.size());
    w.Put16(1);
    w.PutS16(320);
    w.PutS16(750);
    w.PatchOffset16(base1AnchorOffPos, baseArrayBase, w.bytes.size());
    w.Put16(1);
    w.PutS16(340);
    w.PutS16(780);

    return w.bytes;
}

static std::vector<char> BuildSfnt(bool includeGpos) {
    std::map<std::uint32_t, std::vector<char>> tables;
    tables.emplace(HB_TAG('G', 'D', 'E', 'F'), BuildGdefTable());
    if (includeGpos) tables.emplace(HB_TAG('G', 'P', 'O', 'S'), BuildGposTable());
    tables.emplace(HB_TAG('c', 'm', 'a', 'p'), BuildCmapTable());
    tables.emplace(HB_TAG('h', 'e', 'a', 'd'), BuildHeadTable());
    tables.emplace(HB_TAG('h', 'h', 'e', 'a'), BuildHheaTable());
    tables.emplace(HB_TAG('h', 'm', 't', 'x'), BuildHmtxTable());
    tables.emplace(HB_TAG('m', 'a', 'x', 'p'), BuildMaxpTable());

    const auto tableCount = static_cast<std::uint16_t>(tables.size());
    std::uint16_t entrySelector = 0;
    while ((2u << entrySelector) <= tableCount) ++entrySelector;
    const auto searchRange = static_cast<std::uint16_t>(16u << entrySelector);
    const auto rangeShift = static_cast<std::uint16_t>(16u * tableCount - searchRange);

    ByteWriter sfnt;
    sfnt.Put32(0x00010000u);
    sfnt.Put16(tableCount);
    sfnt.Put16(searchRange);
    sfnt.Put16(entrySelector);
    sfnt.Put16(rangeShift);

    std::uint32_t offset = 12u + 16u * tableCount;
    for (const auto& [tag, data] : tables) {
        sfnt.Put32(tag);
        sfnt.Put32(0);
        sfnt.Put32(offset);
        sfnt.Put32(static_cast<std::uint32_t>(data.size()));
        offset += static_cast<std::uint32_t>((data.size() + 3u) & ~std::size_t{3});
    }
    for (const auto& [tag, data] : tables) {
        (void)tag;
        sfnt.bytes.insert(sfnt.bytes.end(), data.begin(), data.end());
        sfnt.bytes.resize((sfnt.bytes.size() + 3u) & ~std::size_t{3});
    }
    return sfnt.bytes;
}

static std::uint32_t ReadBe32(const char* ptr) {
    const auto* u = reinterpret_cast<const unsigned char*>(ptr);
    return (static_cast<std::uint32_t>(u[0]) << 24) |
           (static_cast<std::uint32_t>(u[1]) << 16) |
           (static_cast<std::uint32_t>(u[2]) << 8) |
           static_cast<std::uint32_t>(u[3]);
}

static std::uint16_t ReadBe16(const char* ptr) {
    const auto* u = reinterpret_cast<const unsigned char*>(ptr);
    return static_cast<std::uint16_t>((static_cast<unsigned>(u[0]) << 8) | u[1]);
}

struct GuestTableSource {
    const GuestApi* api;
    const std::vector<char>* sfnt;
};

static hb_blob_t* APS5_VABI ReferenceSfntTable(hb_face_t*, std::uint32_t tag, void* opaque) {
    const auto& source = *static_cast<const GuestTableSource*>(opaque);
    const auto& bytes = *source.sfnt;
    if (bytes.size() < 12) return source.api->blob(nullptr, 0, HB_MEMORY_MODE_READONLY, nullptr, nullptr);
    const std::size_t count = ReadBe16(bytes.data() + 4);
    if (bytes.size() < 12 + count * 16) return source.api->blob(nullptr, 0, HB_MEMORY_MODE_READONLY, nullptr, nullptr);
    for (std::size_t i = 0; i < count; ++i) {
        const char* record = bytes.data() + 12 + i * 16;
        if (ReadBe32(record) != tag) continue;
        const std::uint32_t offset = ReadBe32(record + 8);
        const std::uint32_t length = ReadBe32(record + 12);
        Require(static_cast<std::size_t>(offset) + length <= bytes.size());
        return source.api->blob(bytes.data() + offset, length, HB_MEMORY_MODE_READONLY, nullptr, nullptr);
    }
    return source.api->blob(nullptr, 0, HB_MEMORY_MODE_READONLY, nullptr, nullptr);
}

struct GlyphExpectation {
    std::uint32_t codepoint;
    std::uint32_t cluster;
    std::int32_t xAdvance;
    std::int32_t yAdvance;
    std::int32_t xOffset;
    std::int32_t yOffset;
};

struct ShapedGlyph {
    std::uint32_t codepoint;
    std::uint32_t cluster;
    std::int32_t xAdvance;
    std::int32_t yAdvance;
    std::int32_t xOffset;
    std::int32_t yOffset;
};

static std::vector<ShapedGlyph> ShapeAndVerifyAgainstOracle(
    const GuestApi& api,
    hb_font_t* guestFont,
    hb_font_t* nativeFont,
    std::span<const std::uint16_t> text,
    hb_direction_t direction,
    hb_script_t script,
    std::span<const HarfBuzz::Feature> features,
    std::span<const GlyphExpectation> expected = {}) {

    auto* guestBuffer = api.buffer();
    api.addUtf16(guestBuffer, text.data(), static_cast<int>(text.size()), 0, static_cast<int>(text.size()));
    api.direction(guestBuffer, direction);
    api.script(guestBuffer, script);
    api.shape(guestFont, guestBuffer, features.data(), static_cast<unsigned>(features.size()));

    auto* nativeBuffer = hb_buffer_create();
    hb_buffer_add_utf16(nativeBuffer, text.data(), static_cast<int>(text.size()), 0, static_cast<int>(text.size()));
    hb_buffer_set_direction(nativeBuffer, direction);
    hb_buffer_set_script(nativeBuffer, script);
    std::vector<hb_feature_t> nativeFeatures;
    nativeFeatures.reserve(features.size());
    for (const auto& f : features) nativeFeatures.push_back({f.tag, f.value, f.start, f.end});
    hb_shape(nativeFont, nativeBuffer, nativeFeatures.data(), static_cast<unsigned>(nativeFeatures.size()));

    unsigned guestInfoLen = 0;
    unsigned guestPosLen = 0;
    const auto* guestInfos = api.infos(guestBuffer, &guestInfoLen);
    const auto* guestPos = api.positions(guestBuffer, &guestPosLen);
    Require(api.length(guestBuffer) == guestInfoLen && guestInfoLen == guestPosLen);

    unsigned nativeInfoLen = 0;
    unsigned nativePosLen = 0;
    const auto* nativeInfos = hb_buffer_get_glyph_infos(nativeBuffer, &nativeInfoLen);
    const auto* nativePos = hb_buffer_get_glyph_positions(nativeBuffer, &nativePosLen);
    Require(guestInfoLen == nativeInfoLen && nativeInfoLen == nativePosLen);

    if (!expected.empty()) Require(guestInfoLen == expected.size());
    std::vector<ShapedGlyph> result;
    result.reserve(guestInfoLen);
    for (unsigned i = 0; i < guestInfoLen; ++i) {
        Require(guestInfos[i].codepoint == nativeInfos[i].codepoint);
        Require(guestInfos[i].cluster == nativeInfos[i].cluster);
        Require(guestPos[i].x_advance == nativePos[i].x_advance);
        Require(guestPos[i].y_advance == nativePos[i].y_advance);
        Require(guestPos[i].x_offset == nativePos[i].x_offset);
        Require(guestPos[i].y_offset == nativePos[i].y_offset);
        if (!expected.empty()) {
            Require(guestInfos[i].codepoint == expected[i].codepoint);
            Require(guestInfos[i].cluster == expected[i].cluster);
            Require(guestPos[i].x_advance == expected[i].xAdvance);
            Require(guestPos[i].y_advance == expected[i].yAdvance);
            Require(guestPos[i].x_offset == expected[i].xOffset);
            Require(guestPos[i].y_offset == expected[i].yOffset);
        }
        result.push_back({
            guestInfos[i].codepoint,
            guestInfos[i].cluster,
            guestPos[i].x_advance,
            guestPos[i].y_advance,
            guestPos[i].x_offset,
            guestPos[i].y_offset
        });
    }

    hb_buffer_destroy(nativeBuffer);
    api.destroyBuffer(guestBuffer);
    return result;
}

static void CheckGpos(const GuestApi& api) {
    const auto sfntWithGpos = BuildSfnt(true);
    const auto sfntWithoutGpos = BuildSfnt(false);

    GuestTableSource guestSource{&api, &sfntWithGpos};
    auto* guestFace = api.face(ReferenceSfntTable, &guestSource, nullptr);
    auto* guestFont = api.font(guestFace);
    api.setScale(guestFont, 1000, 1000);

    auto* nativeBlob = hb_blob_create(sfntWithGpos.data(), static_cast<unsigned>(sfntWithGpos.size()), HB_MEMORY_MODE_READONLY, nullptr, nullptr);
    auto* nativeFace = hb_face_create(nativeBlob, 0);
    hb_blob_destroy(nativeBlob);
    auto* nativeFont = hb_font_create(nativeFace);
    hb_font_set_scale(nativeFont, 1000, 1000);

    GuestTableSource noGposSource{&api, &sfntWithoutGpos};
    auto* guestNoGposFace = api.face(ReferenceSfntTable, &noGposSource, nullptr);
    auto* guestNoGposFont = api.font(guestNoGposFace);
    api.setScale(guestNoGposFont, 1000, 1000);

    auto* nativeNoGposBlob = hb_blob_create(sfntWithoutGpos.data(), static_cast<unsigned>(sfntWithoutGpos.size()), HB_MEMORY_MODE_READONLY, nullptr, nullptr);
    auto* nativeNoGposFace = hb_face_create(nativeNoGposBlob, 0);
    hb_blob_destroy(nativeNoGposBlob);
    auto* nativeNoGposFont = hb_font_create(nativeNoGposFace);
    hb_font_set_scale(nativeNoGposFont, 1000, 1000);

    std::uint32_t guestFeatureIndex = 0xFFFFFFFFu;
    std::uint32_t nativeFeatureIndex = 0xFFFFFFFFu;
    Require(api.findFeature(guestFace, HB_OT_TAG_GPOS, 1, HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX, HB_TAG('k', 'e', 'r', 'n'), &guestFeatureIndex) == 1);
    Require(hb_ot_layout_language_find_feature(nativeFace, HB_OT_TAG_GPOS, 1, HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX, HB_TAG('k', 'e', 'r', 'n'), &nativeFeatureIndex) == 1);
    Require(guestFeatureIndex == nativeFeatureIndex && guestFeatureIndex == 0);
    Require(api.findFeature(guestFace, HB_OT_TAG_GPOS, 1, HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX, HB_TAG('m', 'a', 'r', 'k'), &guestFeatureIndex) == 1);
    Require(hb_ot_layout_language_find_feature(nativeFace, HB_OT_TAG_GPOS, 1, HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX, HB_TAG('m', 'a', 'r', 'k'), &nativeFeatureIndex) == 1);
    Require(guestFeatureIndex == nativeFeatureIndex && guestFeatureIndex == 1);

    const std::array<std::uint16_t, 2> avText{'A', 'V'};
    const std::array<GlyphExpectation, 2> avKerned{{
        {1, 0, 520, 0, 15, 0},
        {2, 1, 650, 0, 0, 25}
    }};
    const std::array<GlyphExpectation, 2> avUnkerned{{
        {1, 0, 600, 0, 0, 0},
        {2, 1, 650, 0, 0, 0}
    }};
    const std::array<HarfBuzz::Feature, 1> kernOn{{{HB_TAG('k', 'e', 'r', 'n'), 1, 0, 0xFFFFFFFFu}}};
    const std::array<HarfBuzz::Feature, 1> kernOff{{{HB_TAG('k', 'e', 'r', 'n'), 0, 0, 0xFFFFFFFFu}}};

    const auto avDefault = ShapeAndVerifyAgainstOracle(api, guestFont, nativeFont, avText, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, {}, avKerned);
    ShapeAndVerifyAgainstOracle(api, guestFont, nativeFont, avText, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, kernOn, avKerned);
    const auto avDisabled = ShapeAndVerifyAgainstOracle(api, guestFont, nativeFont, avText, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, kernOff, avUnkerned);
    const auto avNoGpos = ShapeAndVerifyAgainstOracle(api, guestNoGposFont, nativeNoGposFont, avText, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, {}, avUnkerned);
    Require(avDisabled[0].xAdvance != avDefault[0].xAdvance);
    Require(avDisabled[0].xOffset != avDefault[0].xOffset);
    Require(avDisabled[1].yOffset != avDefault[1].yOffset);
    Require(avNoGpos[0].xAdvance != avDefault[0].xAdvance);
    Require(avNoGpos[0].xOffset != avDefault[0].xOffset);
    Require(avNoGpos[1].yOffset != avDefault[1].yOffset);

    const std::array<std::uint16_t, 4> avavText{'A', 'V', 'A', 'V'};
    const std::array<HarfBuzz::Feature, 2> kernSecondPairOnly{{{HB_TAG('k', 'e', 'r', 'n'), 0, 0, 0xFFFFFFFFu}, {HB_TAG('k', 'e', 'r', 'n'), 1, 2, 4}}};
    const std::array<HarfBuzz::Feature, 3> kernFirstPairOnly{{kernOff[0], {HB_TAG('k', 'e', 'r', 'n'), 1, 0, 4}, {HB_TAG('k', 'e', 'r', 'n'), 0, 2, 4}}};
    const std::array<HarfBuzz::Feature, 3> kernReverseOrder{{kernOff[0], {HB_TAG('k', 'e', 'r', 'n'), 0, 2, 4}, {HB_TAG('k', 'e', 'r', 'n'), 1, 0, 4}}};
    const std::array<GlyphExpectation, 4> avavSecondKerned{{
        {1, 0, 600, 0, 0, 0},
        {2, 1, 650, 0, 0, 0},
        {1, 2, 520, 0, 15, 0},
        {2, 3, 650, 0, 0, 25}
    }};
    const std::array<GlyphExpectation, 4> avavFirstKerned{{
        {1, 0, 520, 0, 15, 0},
        {2, 1, 650, 0, 0, 25},
        {1, 2, 600, 0, 0, 0},
        {2, 3, 650, 0, 0, 0}
    }};
    const std::array<GlyphExpectation, 4> avavBothKerned{{
        {1, 0, 520, 0, 15, 0},
        {2, 1, 650, 0, 0, 25},
        {1, 2, 520, 0, 15, 0},
        {2, 3, 650, 0, 0, 25}
    }};
    ShapeAndVerifyAgainstOracle(api, guestFont, nativeFont, avavText, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, kernSecondPairOnly, avavSecondKerned);
    ShapeAndVerifyAgainstOracle(api, guestFont, nativeFont, avavText, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, kernFirstPairOnly, avavFirstKerned);
    ShapeAndVerifyAgainstOracle(api, guestFont, nativeFont, avavText, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, kernReverseOrder, avavBothKerned);

    const std::array<std::uint16_t, 2> aAcuteText{'A', 0x0301};
    const std::array<GlyphExpectation, 2> aAcuteMarked{{
        {1, 0, 600, 0, 0, 0},
        {3, 0, 0, 0, -320, 270}
    }};
    const std::array<GlyphExpectation, 2> aAcuteUnmarked{{
        {1, 0, 600, 0, 0, 0},
        {3, 0, 0, 0, 0, 0}
    }};
    const std::array<HarfBuzz::Feature, 1> markOn{{{HB_TAG('m', 'a', 'r', 'k'), 1, 0, 0xFFFFFFFFu}}};
    const std::array<HarfBuzz::Feature, 1> markOff{{{HB_TAG('m', 'a', 'r', 'k'), 0, 0, 0xFFFFFFFFu}}};

    const auto aAcuteDefault = ShapeAndVerifyAgainstOracle(api, guestFont, nativeFont, aAcuteText, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, {}, aAcuteMarked);
    ShapeAndVerifyAgainstOracle(api, guestFont, nativeFont, aAcuteText, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, markOn, aAcuteMarked);
    const auto aAcuteDisabled = ShapeAndVerifyAgainstOracle(api, guestFont, nativeFont, aAcuteText, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, markOff, aAcuteUnmarked);
    const auto aAcuteNoGpos = ShapeAndVerifyAgainstOracle(api, guestNoGposFont, nativeNoGposFont, aAcuteText, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, {});
    Require(aAcuteDisabled[1].xOffset != aAcuteDefault[1].xOffset);
    Require(aAcuteDisabled[1].yOffset != aAcuteDefault[1].yOffset);
    Require(aAcuteNoGpos.size() == 2);
    Require(aAcuteNoGpos[1].xOffset != aAcuteDefault[1].xOffset);
    Require(aAcuteNoGpos[1].yOffset != aAcuteDefault[1].yOffset);

    const std::array<std::uint16_t, 3> aAcuteVText{'A', 0x0301, 'V'};
    const std::array<GlyphExpectation, 3> aAcuteVKernAndMark{{
        {1, 0, 520, 0, 15, 0},
        {3, 0, 0, 0, -225, 270},
        {2, 2, 650, 0, 0, 25}
    }};
    const std::array<GlyphExpectation, 3> aAcuteVMarkOnly{{
        {1, 0, 600, 0, 0, 0},
        {3, 0, 0, 0, -320, 270},
        {2, 2, 650, 0, 0, 0}
    }};
    const std::array<GlyphExpectation, 3> aAcuteVKernOnly{{
        {1, 0, 520, 0, 15, 0},
        {3, 0, 0, 0, 0, 0},
        {2, 2, 650, 0, 0, 25}
    }};
    ShapeAndVerifyAgainstOracle(api, guestFont, nativeFont, aAcuteVText, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, {}, aAcuteVKernAndMark);
    ShapeAndVerifyAgainstOracle(api, guestFont, nativeFont, aAcuteVText, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, kernOff, aAcuteVMarkOnly);
    ShapeAndVerifyAgainstOracle(api, guestFont, nativeFont, aAcuteVText, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, markOff, aAcuteVKernOnly);

    const std::array<std::uint16_t, 4> aAcuteVAcuteText{'A', 0x0301, 'V', 0x0301};
    const std::array<HarfBuzz::Feature, 3> markSecondOnlyPair{{kernOff[0], markOff[0], {HB_TAG('m', 'a', 'r', 'k'), 1, 2, 4}}};
    const std::array<GlyphExpectation, 4> aAcuteVAcuteSecondMarked{{
        {1, 0, 600, 0, 0, 0},
        {3, 0, 0, 0, 0, 0},
        {2, 2, 650, 0, 0, 0},
        {3, 2, 0, 0, -350, 300}
    }};
    ShapeAndVerifyAgainstOracle(api, guestFont, nativeFont, aAcuteVAcuteText, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, markSecondOnlyPair, aAcuteVAcuteSecondMarked);

    const std::array<std::uint16_t, 2> vaText{'V', 'A'};
    const std::array<GlyphExpectation, 2> vaRtlKerned{{
        {1, 1, 520, 0, 15, 0},
        {2, 0, 650, 0, 0, 25}
    }};
    const std::array<GlyphExpectation, 2> vaRtlUnkerned{{
        {1, 1, 600, 0, 0, 0},
        {2, 0, 650, 0, 0, 0}
    }};
    const auto vaRtlDefault = ShapeAndVerifyAgainstOracle(api, guestFont, nativeFont, vaText, HB_DIRECTION_RTL, HB_SCRIPT_LATIN, {}, vaRtlKerned);
    const auto vaRtlDisabled = ShapeAndVerifyAgainstOracle(api, guestFont, nativeFont, vaText, HB_DIRECTION_RTL, HB_SCRIPT_LATIN, kernOff, vaRtlUnkerned);
    Require(vaRtlDisabled[0].xAdvance != vaRtlDefault[0].xAdvance);
    Require(vaRtlDisabled[0].xOffset != vaRtlDefault[0].xOffset);
    Require(vaRtlDisabled[1].yOffset != vaRtlDefault[1].yOffset);

    const std::array<GlyphExpectation, 2> aAcuteRtlMarked{{
        {1, 0, 600, 0, 0, 0},
        {3, 0, 0, 0, -320, 270}
    }};
    const std::array<GlyphExpectation, 2> aAcuteRtlUnmarked{{
        {1, 0, 600, 0, 0, 0},
        {3, 0, 0, 0, 0, 0}
    }};
    const auto aAcuteRtlDefault = ShapeAndVerifyAgainstOracle(api, guestFont, nativeFont, aAcuteText, HB_DIRECTION_RTL, HB_SCRIPT_LATIN, {}, aAcuteRtlMarked);
    const auto aAcuteRtlDisabled = ShapeAndVerifyAgainstOracle(api, guestFont, nativeFont, aAcuteText, HB_DIRECTION_RTL, HB_SCRIPT_LATIN, markOff, aAcuteRtlUnmarked);
    Require(aAcuteRtlDisabled[1].xOffset != aAcuteRtlDefault[1].xOffset);
    Require(aAcuteRtlDisabled[1].yOffset != aAcuteRtlDefault[1].yOffset);

    api.setScale(guestFont, 2000, 3000);
    hb_font_set_scale(nativeFont, 2000, 3000);
    const std::array<GlyphExpectation, 2> avScaledKerned{{
        {1, 0, 1040, 0, 30, 0},
        {2, 1, 1300, 0, 0, 75}
    }};
    const std::array<GlyphExpectation, 2> aAcuteScaledMarked{{
        {1, 0, 1200, 0, 0, 0},
        {3, 0, 0, 0, -640, 810}
    }};
    ShapeAndVerifyAgainstOracle(api, guestFont, nativeFont, avText, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, {}, avScaledKerned);
    ShapeAndVerifyAgainstOracle(api, guestFont, nativeFont, aAcuteText, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, {}, aAcuteScaledMarked);

    api.setScale(guestFont, 500, 500);
    hb_font_set_scale(nativeFont, 500, 500);
    ShapeAndVerifyAgainstOracle(api, guestFont, nativeFont, aAcuteVText, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, {});

    api.destroyFont(guestNoGposFont);
    api.destroyFace(guestNoGposFace);
    hb_font_destroy(nativeNoGposFont);
    hb_face_destroy(nativeNoGposFace);

    api.destroyFont(guestFont);
    api.destroyFace(guestFace);
    hb_font_destroy(nativeFont);
    hb_face_destroy(nativeFace);
}

int main(int argc, char** argv) try {
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
    GuestApi api{};
    api.shape = Import<decltype(api.shape)>(module, patched ? "dM3hP4y99rM" : "hb_shape_nid_postfix");
    api.blob = Import<decltype(api.blob)>(module, patched ? "hj1KIIN5D54" : "hb_blob_create_nid_postfix");
    api.face = Import<decltype(api.face)>(module, patched ? "l7GEYs5PMVU" : "hb_face_create_for_tables_nid_postfix");
    api.destroyFace = Import<decltype(api.destroyFace)>(module, patched ? "ULNHQXciJ3s" : "hb_face_destroy_nid_postfix");
    api.font = Import<decltype(api.font)>(module, patched ? "FVhYusaWqA4" : "hb_font_create_nid_postfix");
    api.destroyFont = Import<decltype(api.destroyFont)>(module, patched ? "-8t2aEtFws4" : "hb_font_destroy_nid_postfix");
    api.setScale = Import<decltype(api.setScale)>(module, patched ? "eG5EI3x6twg" : "hb_font_set_scale_nid_postfix");
    api.buffer = Import<decltype(api.buffer)>(module, patched ? "7f13XmbjTZo" : "hb_buffer_create_nid_postfix");
    api.destroyBuffer = Import<decltype(api.destroyBuffer)>(module, patched ? "K-y1p7Z9THQ" : "hb_buffer_destroy_nid_postfix");
    api.addUtf16 = Import<decltype(api.addUtf16)>(module, patched ? "P+hgu937lFs" : "hb_buffer_add_utf16_nid_postfix");
    api.direction = Import<decltype(api.direction)>(module, patched ? "845S2XQdxhw" : "hb_buffer_set_direction_nid_postfix");
    api.script = Import<decltype(api.script)>(module, patched ? "eGbbvD8o7go" : "hb_buffer_set_script_nid_postfix");
    api.length = Import<decltype(api.length)>(module, patched ? "leHmVbjHyGw" : "hb_buffer_get_length_nid_postfix");
    api.infos = Import<decltype(api.infos)>(module, patched ? "ENsHmJCNG4Q" : "hb_buffer_get_glyph_infos_nid_postfix");
    api.positions = Import<decltype(api.positions)>(module, patched ? "43mnfTQPCbw" : "hb_buffer_get_glyph_positions_nid_postfix");
    api.findFeature = Import<decltype(api.findFeature)>(module, patched ? "qos3JIrKrDg" : "hb_ot_layout_language_find_feature_nid_postfix");
    CheckGpos(api);
#ifdef _WIN32
    Require(FreeLibrary(module) != 0);
#else
    Require(dlclose(module) == 0);
#endif
} catch (const std::exception& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
}
