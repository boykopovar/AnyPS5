#include "prx/libharfbuzz/libharfbuzz.hpp"
#include "tests/CommandLineArgument.hpp"

#include <hb-ot.h>
#include <hb.h>

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
#include <limits>
#include <map>
#include <source_location>
#include <span>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "HarfBuzz contextual GSUB check failed at line %u\n", location.line());
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

struct Api {
    decltype(&hb_shape_nid_postfix) shape;
    decltype(&hb_blob_create_nid_postfix) blob;
    decltype(&hb_face_create_for_tables_nid_postfix) face;
    decltype(&hb_face_destroy_nid_postfix) destroyFace;
    decltype(&hb_font_create_nid_postfix) font;
    decltype(&hb_font_destroy_nid_postfix) destroyFont;
    decltype(&hb_buffer_create_nid_postfix) buffer;
    decltype(&hb_buffer_destroy_nid_postfix) destroyBuffer;
    decltype(&hb_buffer_add_utf16_nid_postfix) addUtf16;
    decltype(&hb_buffer_get_length_nid_postfix) getLength;
    decltype(&hb_buffer_set_direction_nid_postfix) setDirection;
    decltype(&hb_buffer_set_script_nid_postfix) setScript;
    decltype(&hb_buffer_get_glyph_infos_nid_postfix) getGlyphInfos;
    decltype(&hb_buffer_get_glyph_positions_nid_postfix) getGlyphPositions;
    decltype(&hb_buffer_reset_nid_postfix) resetBuffer;
    decltype(&hb_buffer_guess_segment_properties_nid_postfix) guessSegmentProperties;
    decltype(&hb_ot_layout_table_get_script_tags_nid_postfix) getScriptTags;
    decltype(&hb_ot_layout_script_get_language_tags_nid_postfix) getLanguageTags;
    decltype(&hb_ot_layout_language_find_feature_nid_postfix) findFeature;
};

enum GlyphId : std::uint16_t {
    GidNotdef = 0,
    GidA = 1,
    GidF = 2,
    GidI = 3,
    GidX = 4,
    GidY = 5,
    GidZ = 6,
    GidAAlt = 7,
    GidFi = 8,
    GidFfi = 9,
    GlyphCount = 10,
};

constexpr std::int32_t AdvanceForGlyph(std::uint32_t glyph) {
    return 300 + static_cast<std::int32_t>(glyph) * 70;
}

struct BinaryWriter {
    std::vector<std::uint8_t> bytes;

    void Push16(std::uint16_t value) {
        bytes.push_back(static_cast<std::uint8_t>(value >> 8));
        bytes.push_back(static_cast<std::uint8_t>(value));
    }

    void Push32(std::uint32_t value) {
        Push16(static_cast<std::uint16_t>(value >> 16));
        Push16(static_cast<std::uint16_t>(value));
    }

    void PushTag(hb_tag_t tag) {
        Push32(tag);
    }

    std::size_t Reserve16() {
        const auto pos = bytes.size();
        Push16(0);
        return pos;
    }

    void PatchOffset16(std::size_t fieldPos, std::size_t basePos, std::size_t targetPos) {
        Require(targetPos >= basePos && targetPos - basePos <= 0xFFFFu);
        const auto offset = static_cast<std::uint16_t>(targetPos - basePos);
        bytes[fieldPos] = static_cast<std::uint8_t>(offset >> 8);
        bytes[fieldPos + 1] = static_cast<std::uint8_t>(offset);
    }
};

static void WriteCoverage1(BinaryWriter& out, std::initializer_list<std::uint16_t> glyphs) {
    out.Push16(1);
    out.Push16(static_cast<std::uint16_t>(glyphs.size()));
    for (const auto glyph : glyphs) out.Push16(glyph);
}

static std::vector<std::uint8_t> BuildGsubTable() {
    BinaryWriter out;
    out.Push16(1);
    out.Push16(0);
    const auto scriptListField = out.Reserve16();
    const auto featureListField = out.Reserve16();
    const auto lookupListField = out.Reserve16();

    const auto scriptListStart = out.bytes.size();
    out.PatchOffset16(scriptListField, 0, scriptListStart);
    constexpr std::array<hb_tag_t, 3> scripts{
        HB_TAG('D', 'F', 'L', 'T'),
        HB_TAG('a', 'r', 'a', 'b'),
        HB_TAG('l', 'a', 't', 'n'),
    };
    out.Push16(static_cast<std::uint16_t>(scripts.size()));
    std::array<std::size_t, 3> scriptFields{};
    for (std::size_t i = 0; i < scripts.size(); ++i) {
        out.PushTag(scripts[i]);
        scriptFields[i] = out.Reserve16();
    }
    for (std::size_t i = 0; i < scripts.size(); ++i) {
        const auto scriptStart = out.bytes.size();
        out.PatchOffset16(scriptFields[i], scriptListStart, scriptStart);
        const auto defaultLangSysField = out.Reserve16();
        out.Push16(0);
        out.PatchOffset16(defaultLangSysField, scriptStart, out.bytes.size());
        out.Push16(0);
        out.Push16(0xFFFFu);
        out.Push16(2);
        out.Push16(0);
        out.Push16(1);
    }

    const auto featureListStart = out.bytes.size();
    out.PatchOffset16(featureListField, 0, featureListStart);
    out.Push16(2);
    out.PushTag(HB_TAG('c', 'a', 'l', 't'));
    const auto caltFeatureField = out.Reserve16();
    out.PushTag(HB_TAG('l', 'i', 'g', 'a'));
    const auto ligaFeatureField = out.Reserve16();

    out.PatchOffset16(caltFeatureField, featureListStart, out.bytes.size());
    out.Push16(0);
    out.Push16(1);
    out.Push16(2);

    out.PatchOffset16(ligaFeatureField, featureListStart, out.bytes.size());
    out.Push16(0);
    out.Push16(1);
    out.Push16(0);

    const auto lookupListStart = out.bytes.size();
    out.PatchOffset16(lookupListField, 0, lookupListStart);
    out.Push16(3);
    const auto lookup0Field = out.Reserve16();
    const auto lookup1Field = out.Reserve16();
    const auto lookup2Field = out.Reserve16();

    const auto lookup0Start = out.bytes.size();
    out.PatchOffset16(lookup0Field, lookupListStart, lookup0Start);
    out.Push16(4);
    out.Push16(0);
    out.Push16(1);
    const auto subtable0Field = out.Reserve16();

    const auto subtable0Start = out.bytes.size();
    out.PatchOffset16(subtable0Field, lookup0Start, subtable0Start);
    out.Push16(1);
    const auto ligCoverageField = out.Reserve16();
    out.Push16(1);
    const auto ligSetField = out.Reserve16();

    out.PatchOffset16(ligCoverageField, subtable0Start, out.bytes.size());
    WriteCoverage1(out, {GidF});

    const auto ligSetStart = out.bytes.size();
    out.PatchOffset16(ligSetField, subtable0Start, ligSetStart);
    out.Push16(2);
    const auto lig0Field = out.Reserve16();
    const auto lig1Field = out.Reserve16();

    out.PatchOffset16(lig0Field, ligSetStart, out.bytes.size());
    out.Push16(GidFfi);
    out.Push16(3);
    out.Push16(GidF);
    out.Push16(GidI);

    out.PatchOffset16(lig1Field, ligSetStart, out.bytes.size());
    out.Push16(GidFi);
    out.Push16(2);
    out.Push16(GidI);

    const auto lookup1Start = out.bytes.size();
    out.PatchOffset16(lookup1Field, lookupListStart, lookup1Start);
    out.Push16(1);
    out.Push16(0);
    out.Push16(1);
    const auto subtable1Field = out.Reserve16();

    const auto subtable1Start = out.bytes.size();
    out.PatchOffset16(subtable1Field, lookup1Start, subtable1Start);
    out.Push16(1);
    const auto singleCoverageField = out.Reserve16();
    out.Push16(GidAAlt - GidA);

    out.PatchOffset16(singleCoverageField, subtable1Start, out.bytes.size());
    WriteCoverage1(out, {GidA});

    const auto lookup2Start = out.bytes.size();
    out.PatchOffset16(lookup2Field, lookupListStart, lookup2Start);
    out.Push16(6);
    out.Push16(0);
    out.Push16(1);
    const auto subtable2Field = out.Reserve16();

    const auto subtable2Start = out.bytes.size();
    out.PatchOffset16(subtable2Field, lookup2Start, subtable2Start);
    out.Push16(3);
    out.Push16(1);
    const auto backtrackCovField = out.Reserve16();
    out.Push16(1);
    const auto inputCovField = out.Reserve16();
    out.Push16(1);
    const auto lookaheadCovField = out.Reserve16();
    out.Push16(1);
    out.Push16(0);
    out.Push16(1);

    out.PatchOffset16(backtrackCovField, subtable2Start, out.bytes.size());
    WriteCoverage1(out, {GidX, GidFi});

    out.PatchOffset16(inputCovField, subtable2Start, out.bytes.size());
    WriteCoverage1(out, {GidA});

    out.PatchOffset16(lookaheadCovField, subtable2Start, out.bytes.size());
    WriteCoverage1(out, {GidY});

    return out.bytes;
}

static std::vector<std::uint8_t> BuildHeadTable() {
    BinaryWriter out;
    constexpr std::array<std::uint16_t, 27> words{
        1, 0, 1, 0, 0, 0, 0x5F0F, 0x3CF5,
        0, 1000, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 1000, 1000, 0, 8,
        2, 0, 0};
    for (const auto word : words) out.Push16(word);
    return out.bytes;
}

static std::vector<std::uint8_t> BuildHheaTable() {
    BinaryWriter out;
    constexpr std::array<std::uint16_t, 18> words{
        1, 0, 800, 0xFF38, 0, 1200, 0, 0,
        1200, 1, 0, 0, 0, 0, 0, 0,
        0, GlyphCount};
    for (const auto word : words) out.Push16(word);
    return out.bytes;
}

static std::vector<std::uint8_t> BuildMaxpTable() {
    BinaryWriter out;
    out.Push16(0);
    out.Push16(0x5000);
    out.Push16(GlyphCount);
    return out.bytes;
}

static std::vector<std::uint8_t> BuildHmtxTable() {
    BinaryWriter out;
    for (std::uint16_t gid = 0; gid < GlyphCount; ++gid) {
        out.Push16(static_cast<std::uint16_t>(AdvanceForGlyph(gid)));
        out.Push16(0);
    }
    return out.bytes;
}

static std::vector<std::uint8_t> BuildCmapTable() {
    BinaryWriter out;
    out.Push16(0);
    out.Push16(1);
    out.Push16(3);
    out.Push16(1);
    out.Push32(12);

    out.Push16(4);
    out.Push16(72);
    out.Push16(0);
    out.Push16(14);
    out.Push16(8);
    out.Push16(2);
    out.Push16(6);

    constexpr std::array<std::uint16_t, 7> codepoints{'a', 'f', 'i', 'x', 'y', 'z', 0xFFFF};
    constexpr std::array<std::uint16_t, 7> glyphs{GidA, GidF, GidI, GidX, GidY, GidZ, GidNotdef};
    for (const auto cp : codepoints) out.Push16(cp);
    out.Push16(0);
    for (const auto cp : codepoints) out.Push16(cp);
    for (std::size_t i = 0; i < codepoints.size(); ++i) {
        const auto delta = static_cast<std::uint16_t>(glyphs[i] - codepoints[i]);
        out.Push16(delta);
    }
    for (std::size_t i = 0; i < codepoints.size(); ++i) out.Push16(0);
    return out.bytes;
}

static std::uint32_t ComputeTableChecksum(std::span<const std::uint8_t> data) {
    std::uint32_t sum = 0;
    for (std::size_t i = 0; i < data.size(); i += 4) {
        std::uint32_t word = 0;
        for (std::size_t b = 0; b < 4; ++b) {
            word <<= 8;
            if (i + b < data.size()) word |= data[i + b];
        }
        sum += word;
    }
    return sum;
}

static std::vector<std::uint8_t> BuildSfnt(bool includeGsub) {
    std::map<std::string, std::vector<std::uint8_t>> tables;
    tables.emplace("head", BuildHeadTable());
    tables.emplace("hhea", BuildHheaTable());
    tables.emplace("maxp", BuildMaxpTable());
    tables.emplace("hmtx", BuildHmtxTable());
    tables.emplace("cmap", BuildCmapTable());
    if (includeGsub) tables.emplace("GSUB", BuildGsubTable());

    const auto tableCount = static_cast<std::uint16_t>(tables.size());
    std::uint16_t entrySelector = 0;
    while ((2u << entrySelector) <= tableCount) ++entrySelector;
    const auto searchRange = static_cast<std::uint16_t>(16u << entrySelector);
    const auto rangeShift = static_cast<std::uint16_t>(tableCount * 16u - searchRange);

    BinaryWriter out;
    out.Push32(0x00010000u);
    out.Push16(tableCount);
    out.Push16(searchRange);
    out.Push16(entrySelector);
    out.Push16(rangeShift);

    std::uint32_t offset = 12u + static_cast<std::uint32_t>(tableCount) * 16u;
    for (const auto& [name, payload] : tables) {
        Require(name.size() == 4);
        for (const char ch : name) out.bytes.push_back(static_cast<std::uint8_t>(ch));
        out.Push32(ComputeTableChecksum(payload));
        out.Push32(offset);
        out.Push32(static_cast<std::uint32_t>(payload.size()));
        offset += static_cast<std::uint32_t>((payload.size() + 3u) & ~std::size_t{3});
    }
    for (const auto& [name, payload] : tables) {
        (void)name;
        out.bytes.insert(out.bytes.end(), payload.begin(), payload.end());
        while (out.bytes.size() & 3u) out.bytes.push_back(0);
    }
    return out.bytes;
}

static std::uint16_t ReadBe16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return static_cast<std::uint16_t>((static_cast<std::uint16_t>(bytes[offset]) << 8) | bytes[offset + 1]);
}

static std::uint32_t ReadBe32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return (static_cast<std::uint32_t>(ReadBe16(bytes, offset)) << 16) | ReadBe16(bytes, offset + 2);
}

struct SfntSource {
    const Api* api;
    std::span<const std::uint8_t> sfnt;
};

static hb_blob_t* APS5_VABI ReferenceSfntTable(hb_face_t*, std::uint32_t tag, void* opaque) {
    const auto& source = *static_cast<const SfntSource*>(opaque);
    if (source.sfnt.size() < 12) return source.api->blob(nullptr, 0, HB_MEMORY_MODE_DUPLICATE, nullptr, nullptr);
    const auto tableCount = ReadBe16(source.sfnt, 4);
    if (source.sfnt.size() < 12u + static_cast<std::size_t>(tableCount) * 16u) {
        return source.api->blob(nullptr, 0, HB_MEMORY_MODE_DUPLICATE, nullptr, nullptr);
    }
    for (std::uint16_t i = 0; i < tableCount; ++i) {
        const std::size_t entry = 12u + static_cast<std::size_t>(i) * 16u;
        if (ReadBe32(source.sfnt, entry) == tag) {
            const auto offset = ReadBe32(source.sfnt, entry + 8);
            const auto length = ReadBe32(source.sfnt, entry + 12);
            Require(static_cast<std::size_t>(offset) + static_cast<std::size_t>(length) <= source.sfnt.size());
            return source.api->blob(reinterpret_cast<const char*>(source.sfnt.data() + offset), length, HB_MEMORY_MODE_READONLY, nullptr, nullptr);
        }
    }
    return source.api->blob(nullptr, 0, HB_MEMORY_MODE_DUPLICATE, nullptr, nullptr);
}

struct ExpectedGlyph {
    std::uint32_t glyph;
    std::uint32_t cluster;
};

struct OracleFont {
    hb_blob_t* blob = nullptr;
    hb_face_t* face = nullptr;
    hb_font_t* font = nullptr;

    explicit OracleFont(std::span<const std::uint8_t> sfnt) {
        blob = hb_blob_create(reinterpret_cast<const char*>(sfnt.data()), static_cast<unsigned>(sfnt.size()), HB_MEMORY_MODE_READONLY, nullptr, nullptr);
        Require(blob != nullptr && hb_blob_get_length(blob) == sfnt.size());
        face = hb_face_create(blob, 0);
        Require(face != nullptr && hb_face_get_glyph_count(face) == GlyphCount);
        font = hb_font_create(face);
        Require(font != nullptr);
    }

    ~OracleFont() {
        hb_font_destroy(font);
        hb_face_destroy(face);
        hb_blob_destroy(blob);
    }
};

struct GuestFont {
    const Api* api;
    SfntSource source;
    hb_face_t* face = nullptr;
    hb_font_t* font = nullptr;

    GuestFont(const Api& apiRef, std::span<const std::uint8_t> sfnt) : api(&apiRef), source{&apiRef, sfnt} {
        face = api->face(ReferenceSfntTable, &source, nullptr);
        Require(face != nullptr);
        font = api->font(face);
        Require(font != nullptr);
    }

    ~GuestFont() {
        api->destroyFont(font);
        api->destroyFace(face);
    }
};

static std::vector<ExpectedGlyph> ShapeWithOracle(const OracleFont& oracle,
                                                  std::span<const std::uint16_t> text,
                                                  hb_direction_t direction,
                                                  hb_script_t script,
                                                  std::span<const HarfBuzz::Feature> features,
                                                  std::vector<hb_glyph_position_t>& outPositions) {
    auto* buffer = hb_buffer_create();
    Require(buffer != nullptr);
    hb_buffer_add_utf16(buffer, text.data(), static_cast<int>(text.size()), 0, static_cast<int>(text.size()));
    hb_buffer_set_direction(buffer, direction);
    hb_buffer_set_script(buffer, script);
    std::vector<hb_feature_t> nativeFeatures;
    nativeFeatures.reserve(features.size());
    for (const auto& f : features) nativeFeatures.push_back({f.tag, f.value, f.start, f.end});
    hb_shape(oracle.font, buffer, nativeFeatures.data(), static_cast<unsigned>(nativeFeatures.size()));
    unsigned infoCount = 0;
    const auto* infos = hb_buffer_get_glyph_infos(buffer, &infoCount);
    unsigned posCount = 0;
    const auto* positions = hb_buffer_get_glyph_positions(buffer, &posCount);
    Require(infoCount == posCount);
    std::vector<ExpectedGlyph> result;
    result.reserve(infoCount);
    outPositions.assign(positions, positions + posCount);
    for (unsigned i = 0; i < infoCount; ++i) {
        result.push_back({infos[i].codepoint, infos[i].cluster});
    }
    hb_buffer_destroy(buffer);
    return result;
}

static std::vector<ExpectedGlyph> ShapeWithGuest(const Api& api,
                                                 hb_font_t* font,
                                                 hb_buffer_t* buffer,
                                                 std::span<const std::uint16_t> text,
                                                 hb_direction_t direction,
                                                 hb_script_t script,
                                                 std::span<const HarfBuzz::Feature> features,
                                                 std::vector<hb_glyph_position_t>& outPositions) {
    api.resetBuffer(buffer);
    api.addUtf16(buffer, text.data(), static_cast<int>(text.size()), 0, static_cast<int>(text.size()));
    api.setDirection(buffer, direction);
    api.setScript(buffer, script);
    api.shape(font, buffer, features.data(), static_cast<unsigned>(features.size()));
    const unsigned len = api.getLength(buffer);
    unsigned infoCount = 0;
    const auto* infos = api.getGlyphInfos(buffer, &infoCount);
    unsigned posCount = 0;
    const auto* positions = api.getGlyphPositions(buffer, &posCount);
    Require(len == infoCount && infoCount == posCount);
    std::vector<ExpectedGlyph> result;
    result.reserve(infoCount);
    outPositions.assign(positions, positions + posCount);
    for (unsigned i = 0; i < infoCount; ++i) {
        result.push_back({infos[i].codepoint, infos[i].cluster});
    }
    return result;
}

static void AssertShapesMatch(const Api& api,
                              const GuestFont& guest,
                              const OracleFont& oracle,
                              hb_buffer_t* reusableBuffer,
                              std::span<const std::uint16_t> text,
                              hb_direction_t direction,
                              hb_script_t script,
                              std::span<const HarfBuzz::Feature> features,
                              std::span<const ExpectedGlyph> expected) {
    std::vector<hb_glyph_position_t> oraclePositions;
    const auto oracleOutput = ShapeWithOracle(oracle, text, direction, script, features, oraclePositions);
    Require(oracleOutput.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        Require(oracleOutput[i].glyph == expected[i].glyph);
        Require(oracleOutput[i].cluster == expected[i].cluster);
        Require(oraclePositions[i].x_advance == AdvanceForGlyph(expected[i].glyph));
        Require(oraclePositions[i].y_advance == 0 && oraclePositions[i].x_offset == 0 && oraclePositions[i].y_offset == 0);
    }

    auto* freshBuffer = api.buffer();
    Require(freshBuffer != nullptr);
    for (auto* buffer : std::array<hb_buffer_t*, 2>{freshBuffer, reusableBuffer}) {
        std::vector<hb_glyph_position_t> guestPositions;
        const auto guestOutput = ShapeWithGuest(api, guest.font, buffer, text, direction, script, features, guestPositions);
        Require(guestOutput.size() == oracleOutput.size());
        Require(guestPositions.size() == oraclePositions.size());
        for (std::size_t i = 0; i < guestOutput.size(); ++i) {
            Require(guestOutput[i].glyph == oracleOutput[i].glyph);
            Require(guestOutput[i].cluster == oracleOutput[i].cluster);
            Require(guestPositions[i].x_advance == oraclePositions[i].x_advance);
            Require(guestPositions[i].y_advance == oraclePositions[i].y_advance);
            Require(guestPositions[i].x_offset == oraclePositions[i].x_offset);
            Require(guestPositions[i].y_offset == oraclePositions[i].y_offset);
        }
    }
    api.destroyBuffer(freshBuffer);
}

static void CheckFeatureDiscovery(const Api& api,
                                  const GuestFont& withGsubGuest,
                                  const OracleFont& withGsubOracle,
                                  const GuestFont& withoutGsubGuest,
                                  const OracleFont& withoutGsubOracle) {
    std::array<hb_tag_t, 4> guestScripts{};
    std::array<hb_tag_t, 4> oracleScripts{};
    unsigned guestCount = 4;
    unsigned oracleCount = 4;
    Require(api.getScriptTags(withGsubGuest.face, HB_OT_TAG_GSUB, 0, &guestCount, guestScripts.data()) == 3);
    Require(hb_ot_layout_table_get_script_tags(withGsubOracle.face, HB_OT_TAG_GSUB, 0, &oracleCount, oracleScripts.data()) == 3);
    Require(guestCount == 3 && oracleCount == 3 && guestScripts == oracleScripts);
    Require(guestScripts[0] == HB_TAG('D', 'F', 'L', 'T'));
    Require(guestScripts[1] == HB_TAG('a', 'r', 'a', 'b'));
    Require(guestScripts[2] == HB_TAG('l', 'a', 't', 'n'));

    for (unsigned scriptIndex = 0; scriptIndex < 3; ++scriptIndex) {
        std::array<hb_tag_t, 2> guestLangs{};
        std::array<hb_tag_t, 2> oracleLangs{};
        unsigned gLangCount = 2;
        unsigned oLangCount = 2;
        Require(api.getLanguageTags(withGsubGuest.face, HB_OT_TAG_GSUB, scriptIndex, 0, &gLangCount, guestLangs.data()) == 0);
        Require(hb_ot_layout_script_get_language_tags(withGsubOracle.face, HB_OT_TAG_GSUB, scriptIndex, 0, &oLangCount, oracleLangs.data()) == 0);
        Require(gLangCount == 0 && oLangCount == 0);

        unsigned guestCalt = 0xFFFFu;
        unsigned oracleCalt = 0xFFFFu;
        Require(api.findFeature(withGsubGuest.face, HB_OT_TAG_GSUB, scriptIndex, HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX, HB_TAG('c', 'a', 'l', 't'), &guestCalt) == 1);
        Require(hb_ot_layout_language_find_feature(withGsubOracle.face, HB_OT_TAG_GSUB, scriptIndex, HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX, HB_TAG('c', 'a', 'l', 't'), &oracleCalt) == 1);
        Require(guestCalt == 0 && oracleCalt == 0);

        unsigned guestLiga = 0xFFFFu;
        unsigned oracleLiga = 0xFFFFu;
        Require(api.findFeature(withGsubGuest.face, HB_OT_TAG_GSUB, scriptIndex, HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX, HB_TAG('l', 'i', 'g', 'a'), &guestLiga) == 1);
        Require(hb_ot_layout_language_find_feature(withGsubOracle.face, HB_OT_TAG_GSUB, scriptIndex, HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX, HB_TAG('l', 'i', 'g', 'a'), &oracleLiga) == 1);
        Require(guestLiga == 1 && oracleLiga == 1);

        unsigned guestMissing = 0;
        unsigned oracleMissing = 0;
        Require(api.findFeature(withGsubGuest.face, HB_OT_TAG_GSUB, scriptIndex, HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX, HB_TAG('s', 'm', 'c', 'p'), &guestMissing) == 0);
        Require(hb_ot_layout_language_find_feature(withGsubOracle.face, HB_OT_TAG_GSUB, scriptIndex, HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX, HB_TAG('s', 'm', 'c', 'p'), &oracleMissing) == 0);
        Require(guestMissing == HB_OT_LAYOUT_NO_FEATURE_INDEX && oracleMissing == HB_OT_LAYOUT_NO_FEATURE_INDEX);
    }

    unsigned withoutScriptsGuest = 4;
    unsigned withoutScriptsOracle = 4;
    Require(api.getScriptTags(withoutGsubGuest.face, HB_OT_TAG_GSUB, 0, &withoutScriptsGuest, guestScripts.data()) == 0);
    Require(hb_ot_layout_table_get_script_tags(withoutGsubOracle.face, HB_OT_TAG_GSUB, 0, &withoutScriptsOracle, oracleScripts.data()) == 0);
    Require(withoutScriptsGuest == 0 && withoutScriptsOracle == 0);

    unsigned noIdxGuest = 0;
    unsigned noIdxOracle = 0;
    Require(api.findFeature(withoutGsubGuest.face, HB_OT_TAG_GSUB, 0, HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX, HB_TAG('l', 'i', 'g', 'a'), &noIdxGuest) == 0);
    Require(hb_ot_layout_language_find_feature(withoutGsubOracle.face, HB_OT_TAG_GSUB, 0, HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX, HB_TAG('l', 'i', 'g', 'a'), &noIdxOracle) == 0);
    Require(noIdxGuest == HB_OT_LAYOUT_NO_FEATURE_INDEX && noIdxOracle == HB_OT_LAYOUT_NO_FEATURE_INDEX);
    Require(api.findFeature(withoutGsubGuest.face, HB_OT_TAG_GSUB, 0, HB_OT_LAYOUT_DEFAULT_LANGUAGE_INDEX, HB_TAG('c', 'a', 'l', 't'), &noIdxGuest) == 0);
    Require(noIdxGuest == HB_OT_LAYOUT_NO_FEATURE_INDEX);
}

static void CheckGuestSegmentGuessAndReset(const Api& api, const GuestFont& guest, const OracleFont& oracle) {
    auto* gBuf = api.buffer();
    auto* nBuf = hb_buffer_create();
    const std::array<std::uint16_t, 3> text{'x', 'a', 'y'};
    api.addUtf16(gBuf, text.data(), 3, 0, 3);
    hb_buffer_add_utf16(nBuf, text.data(), 3, 0, 3);
    api.guessSegmentProperties(gBuf);
    hb_buffer_guess_segment_properties(nBuf);
    api.shape(guest.font, gBuf, nullptr, 0);
    hb_shape(oracle.font, nBuf, nullptr, 0);
    unsigned gLen = 0;
    unsigned nLen = 0;
    const auto* gInfo = api.getGlyphInfos(gBuf, &gLen);
    const auto* nInfo = hb_buffer_get_glyph_infos(nBuf, &nLen);
    Require(gLen == 3 && nLen == 3);
    for (unsigned i = 0; i < 3; ++i) {
        Require(gInfo[i].codepoint == nInfo[i].codepoint && gInfo[i].cluster == nInfo[i].cluster);
    }
    Require(gInfo[0].codepoint == GidX && gInfo[1].codepoint == GidAAlt && gInfo[2].codepoint == GidY);
    api.destroyBuffer(gBuf);
    hb_buffer_destroy(nBuf);
}

static void Check(const Api& api) {
    const auto sfntWithGsub = BuildSfnt(true);
    const auto sfntWithoutGsub = BuildSfnt(false);
    Require(sfntWithGsub.size() > sfntWithoutGsub.size());

    const GuestFont withGsubGuest(api, sfntWithGsub);
    const OracleFont withGsubOracle(sfntWithGsub);
    const GuestFont withoutGsubGuest(api, sfntWithoutGsub);
    const OracleFont withoutGsubOracle(sfntWithoutGsub);

    CheckFeatureDiscovery(api, withGsubGuest, withGsubOracle, withoutGsubGuest, withoutGsubOracle);
    CheckGuestSegmentGuessAndReset(api, withGsubGuest, withGsubOracle);

    auto* reusableBuffer = api.buffer();
    Require(reusableBuffer != nullptr);

    const std::array<std::uint16_t, 3> xay{'x', 'a', 'y'};
    const std::array<std::uint16_t, 3> xaz{'x', 'a', 'z'};
    const std::array<std::uint16_t, 3> zay{'z', 'a', 'y'};
    const std::array<std::uint16_t, 1> singleA{'a'};
    const std::array<std::uint16_t, 2> xa{'x', 'a'};
    const std::array<std::uint16_t, 2> ay{'a', 'y'};
    const std::array<std::uint16_t, 2> fi{'f', 'i'};
    const std::array<std::uint16_t, 3> ffi{'f', 'f', 'i'};
    const std::array<std::uint16_t, 2> ff{'f', 'f'};
    const std::array<std::uint16_t, 4> ffif{'f', 'f', 'i', 'f'};
    const std::array<std::uint16_t, 5> fiffi{'f', 'i', 'f', 'f', 'i'};
    const std::array<std::uint16_t, 6> xayffi{'x', 'a', 'y', 'f', 'f', 'i'};
    const std::array<std::uint16_t, 4> fiay{'f', 'i', 'a', 'y'};
    const std::array<std::uint16_t, 5> ffiay{'f', 'f', 'i', 'a', 'y'};
    const std::array<std::uint16_t, 3> yax{'y', 'a', 'x'};
    const std::array<std::uint16_t, 2> infi{'i', 'f'};
    const std::array<std::uint16_t, 3> inffi{'i', 'f', 'f'};

    const std::array<ExpectedGlyph, 3> xayLtr{{{GidX, 0}, {GidAAlt, 1}, {GidY, 2}}};
    const std::array<ExpectedGlyph, 3> xayRtl{{{GidY, 2}, {GidAAlt, 1}, {GidX, 0}}};
    const std::array<ExpectedGlyph, 3> xazLtr{{{GidX, 0}, {GidA, 1}, {GidZ, 2}}};
    const std::array<ExpectedGlyph, 3> xazRtl{{{GidZ, 2}, {GidA, 1}, {GidX, 0}}};
    const std::array<ExpectedGlyph, 3> zayLtr{{{GidZ, 0}, {GidA, 1}, {GidY, 2}}};
    const std::array<ExpectedGlyph, 3> zayRtl{{{GidY, 2}, {GidA, 1}, {GidZ, 0}}};
    const std::array<ExpectedGlyph, 1> singleALtr{{{GidA, 0}}};
    const std::array<ExpectedGlyph, 2> xaLtr{{{GidX, 0}, {GidA, 1}}};
    const std::array<ExpectedGlyph, 2> ayLtr{{{GidA, 0}, {GidY, 1}}};
    const std::array<ExpectedGlyph, 1> fiMerged{{{GidFi, 0}}};
    const std::array<ExpectedGlyph, 1> ffiMerged{{{GidFfi, 0}}};
    const std::array<ExpectedGlyph, 2> ffLtr{{{GidF, 0}, {GidF, 1}}};
    const std::array<ExpectedGlyph, 2> ffifLtr{{{GidFfi, 0}, {GidF, 3}}};
    const std::array<ExpectedGlyph, 2> ffifRtl{{{GidF, 3}, {GidFfi, 0}}};
    const std::array<ExpectedGlyph, 2> fiffiLtr{{{GidFi, 0}, {GidFfi, 2}}};
    const std::array<ExpectedGlyph, 2> fiffiRtl{{{GidFfi, 2}, {GidFi, 0}}};
    const std::array<ExpectedGlyph, 4> xayffiLtr{{{GidX, 0}, {GidAAlt, 1}, {GidY, 2}, {GidFfi, 3}}};
    const std::array<ExpectedGlyph, 4> xayffiRtl{{{GidFfi, 3}, {GidY, 2}, {GidAAlt, 1}, {GidX, 0}}};
    const std::array<ExpectedGlyph, 3> fiayLtr{{{GidFi, 0}, {GidAAlt, 2}, {GidY, 3}}};
    const std::array<ExpectedGlyph, 3> fiayNoCaltRtl{{{GidY, 3}, {GidA, 2}, {GidFi, 0}}};
    const std::array<ExpectedGlyph, 3> ffiayLtr{{{GidFfi, 0}, {GidA, 3}, {GidY, 4}}};
    const std::array<ExpectedGlyph, 3> ffiayRtl{{{GidY, 4}, {GidA, 3}, {GidFfi, 0}}};

    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, xay, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, {}, xayLtr);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, xaz, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, {}, xazLtr);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, zay, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, {}, zayLtr);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, singleA, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, {}, singleALtr);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, xa, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, {}, xaLtr);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, ay, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, {}, ayLtr);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, fi, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, {}, fiMerged);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, ffi, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, {}, ffiMerged);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, ff, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, {}, ffLtr);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, ffif, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, {}, ffifLtr);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, fiffi, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, {}, fiffiLtr);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, xayffi, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, {}, xayffiLtr);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, fiay, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, {}, fiayLtr);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, ffiay, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, {}, ffiayLtr);

    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, xay, HB_DIRECTION_RTL, HB_SCRIPT_ARABIC, {}, xayRtl);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, xaz, HB_DIRECTION_RTL, HB_SCRIPT_ARABIC, {}, xazRtl);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, zay, HB_DIRECTION_RTL, HB_SCRIPT_ARABIC, {}, zayRtl);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, fi, HB_DIRECTION_RTL, HB_SCRIPT_ARABIC, {}, fiMerged);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, ffi, HB_DIRECTION_RTL, HB_SCRIPT_ARABIC, {}, ffiMerged);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, ffif, HB_DIRECTION_RTL, HB_SCRIPT_ARABIC, {}, ffifRtl);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, fiffi, HB_DIRECTION_RTL, HB_SCRIPT_ARABIC, {}, fiffiRtl);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, xayffi, HB_DIRECTION_RTL, HB_SCRIPT_ARABIC, {}, xayffiRtl);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, fiay, HB_DIRECTION_RTL, HB_SCRIPT_ARABIC, {}, fiayNoCaltRtl);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, ffiay, HB_DIRECTION_RTL, HB_SCRIPT_ARABIC, {}, ffiayRtl);

    const std::array<std::uint16_t, 4> yaif{'y', 'a', 'i', 'f'};
    const std::array<ExpectedGlyph, 3> yaifLatinRtlMatched{{{GidFi, 2}, {GidAAlt, 1}, {GidY, 0}}};
    const std::array<ExpectedGlyph, 3> xayLatinRtlUnmatched{{{GidY, 2}, {GidA, 1}, {GidX, 0}}};
    const std::array<ExpectedGlyph, 3> yaxLatinRtlMatched{{{GidX, 2}, {GidAAlt, 1}, {GidY, 0}}};
    const std::array<ExpectedGlyph, 2> fiLatinRtlUnligated{{{GidI, 1}, {GidF, 0}}};
    const std::array<ExpectedGlyph, 1> infiLatinRtlLigated{{{GidFi, 0}}};
    const std::array<ExpectedGlyph, 1> inffiLatinRtlLigated{{{GidFfi, 0}}};
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, yaif, HB_DIRECTION_RTL, HB_SCRIPT_LATIN, {}, yaifLatinRtlMatched);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, xay, HB_DIRECTION_RTL, HB_SCRIPT_LATIN, {}, xayLatinRtlUnmatched);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, yax, HB_DIRECTION_RTL, HB_SCRIPT_LATIN, {}, yaxLatinRtlMatched);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, fi, HB_DIRECTION_RTL, HB_SCRIPT_LATIN, {}, fiLatinRtlUnligated);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, infi, HB_DIRECTION_RTL, HB_SCRIPT_LATIN, {}, infiLatinRtlLigated);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, inffi, HB_DIRECTION_RTL, HB_SCRIPT_LATIN, {}, inffiLatinRtlLigated);

    constexpr auto GlobalEnd = std::numeric_limits<std::uint32_t>::max();
    const std::array<HarfBuzz::Feature, 1> ligaOff{{{HB_TAG('l', 'i', 'g', 'a'), 0, 0, GlobalEnd}}};
    const std::array<HarfBuzz::Feature, 1> caltOff{{{HB_TAG('c', 'a', 'l', 't'), 0, 0, GlobalEnd}}};
    const std::array<HarfBuzz::Feature, 2> bothOff{{{HB_TAG('l', 'i', 'g', 'a'), 0, 0, GlobalEnd}, {HB_TAG('c', 'a', 'l', 't'), 0, 0, GlobalEnd}}};

    const std::array<ExpectedGlyph, 2> fiUnligatedLtr{{{GidF, 0}, {GidI, 1}}};
    const std::array<ExpectedGlyph, 3> ffiUnligatedLtr{{{GidF, 0}, {GidF, 1}, {GidI, 2}}};
    const std::array<ExpectedGlyph, 3> ffiUnligatedRtl{{{GidI, 2}, {GidF, 1}, {GidF, 0}}};
    const std::array<ExpectedGlyph, 3> xayUnmatchedLtr{{{GidX, 0}, {GidA, 1}, {GidY, 2}}};
    const std::array<ExpectedGlyph, 3> xayUnmatchedRtl{{{GidY, 2}, {GidA, 1}, {GidX, 0}}};
    const std::array<ExpectedGlyph, 4> fiayNoLigaLtr{{{GidF, 0}, {GidI, 1}, {GidA, 2}, {GidY, 3}}};
    const std::array<ExpectedGlyph, 4> fiayNoLigaRtl{{{GidY, 3}, {GidA, 2}, {GidI, 1}, {GidF, 0}}};
    const std::array<ExpectedGlyph, 3> fiayNoCaltLtr{{{GidFi, 0}, {GidA, 2}, {GidY, 3}}};
    const std::array<ExpectedGlyph, 6> xayffiUnshapedLtr{{{GidX, 0}, {GidA, 1}, {GidY, 2}, {GidF, 3}, {GidF, 4}, {GidI, 5}}};
    const std::array<ExpectedGlyph, 6> xayffiUnshapedRtl{{{GidI, 5}, {GidF, 4}, {GidF, 3}, {GidY, 2}, {GidA, 1}, {GidX, 0}}};

    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, fi, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, ligaOff, fiUnligatedLtr);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, ffi, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, ligaOff, ffiUnligatedLtr);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, ffi, HB_DIRECTION_RTL, HB_SCRIPT_ARABIC, ligaOff, ffiUnligatedRtl);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, fiay, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, ligaOff, fiayNoLigaLtr);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, fiay, HB_DIRECTION_RTL, HB_SCRIPT_ARABIC, ligaOff, fiayNoLigaRtl);

    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, xay, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, caltOff, xayUnmatchedLtr);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, xay, HB_DIRECTION_RTL, HB_SCRIPT_ARABIC, caltOff, xayUnmatchedRtl);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, fiay, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, caltOff, fiayNoCaltLtr);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, fiay, HB_DIRECTION_RTL, HB_SCRIPT_ARABIC, caltOff, fiayNoCaltRtl);

    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, xayffi, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, bothOff, xayffiUnshapedLtr);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, xayffi, HB_DIRECTION_RTL, HB_SCRIPT_ARABIC, bothOff, xayffiUnshapedRtl);

    const std::array<HarfBuzz::Feature, 1> ligaFirstTwo{{{HB_TAG('l', 'i', 'g', 'a'), 0, 2, 3}}};
    const std::array<HarfBuzz::Feature, 1> ligaTailOnly{{{HB_TAG('l', 'i', 'g', 'a'), 0, 0, 1}}};
    const std::array<ExpectedGlyph, 2> ffiTailLigatedLtr{{{GidF, 0}, {GidFi, 1}}};
    const std::array<ExpectedGlyph, 2> ffiTailLigatedRtl{{{GidFi, 1}, {GidF, 0}}};
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, ffi, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, ligaFirstTwo, ffiUnligatedLtr);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, ffi, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, ligaTailOnly, ffiTailLigatedLtr);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, ffi, HB_DIRECTION_RTL, HB_SCRIPT_ARABIC, ligaTailOnly, ffiTailLigatedRtl);

    const std::array<HarfBuzz::Feature, 2> ligaOnlyRange{{{HB_TAG('l', 'i', 'g', 'a'), 0, 0, GlobalEnd}, {HB_TAG('l', 'i', 'g', 'a'), 1, 1, 3}}};
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, ffi, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, ligaOnlyRange, ffiTailLigatedLtr);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, ffi, HB_DIRECTION_RTL, HB_SCRIPT_ARABIC, ligaOnlyRange, ffiTailLigatedRtl);

    const std::array<HarfBuzz::Feature, 2> caltInputOnly{{{HB_TAG('c', 'a', 'l', 't'), 0, 0, GlobalEnd}, {HB_TAG('c', 'a', 'l', 't'), 1, 1, 2}}};
    const std::array<HarfBuzz::Feature, 2> caltBacktrackOnly{{{HB_TAG('c', 'a', 'l', 't'), 0, 0, GlobalEnd}, {HB_TAG('c', 'a', 'l', 't'), 1, 0, 1}}};
    const std::array<HarfBuzz::Feature, 2> caltLookaheadOnly{{{HB_TAG('c', 'a', 'l', 't'), 0, 0, GlobalEnd}, {HB_TAG('c', 'a', 'l', 't'), 1, 2, 3}}};
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, xay, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, caltInputOnly, xayLtr);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, xay, HB_DIRECTION_RTL, HB_SCRIPT_ARABIC, caltInputOnly, xayRtl);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, xay, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, caltBacktrackOnly, xayUnmatchedLtr);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, xay, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, caltLookaheadOnly, xayUnmatchedLtr);

    const std::array<HarfBuzz::Feature, 4> fiayPartialEnabled{
        {{HB_TAG('l', 'i', 'g', 'a'), 0, 0, GlobalEnd},
         {HB_TAG('l', 'i', 'g', 'a'), 1, 0, 2},
         {HB_TAG('c', 'a', 'l', 't'), 0, 0, GlobalEnd},
         {HB_TAG('c', 'a', 'l', 't'), 1, 2, 3}}};
    const std::array<HarfBuzz::Feature, 2> fiayPartialBlockedLiga{{{HB_TAG('l', 'i', 'g', 'a'), 0, 1, 2}}};
    const std::array<HarfBuzz::Feature, 2> fiayPartialBlockedCalt{{{HB_TAG('c', 'a', 'l', 't'), 0, 2, 3}}};
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, fiay, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, fiayPartialEnabled, fiayLtr);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, fiay, HB_DIRECTION_RTL, HB_SCRIPT_ARABIC, fiayPartialEnabled, fiayNoCaltRtl);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, fiay, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, fiayPartialBlockedLiga, fiayNoLigaLtr);
    AssertShapesMatch(api, withGsubGuest, withGsubOracle, reusableBuffer, fiay, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, fiayPartialBlockedCalt, fiayNoCaltLtr);

    AssertShapesMatch(api, withoutGsubGuest, withoutGsubOracle, reusableBuffer, xay, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, {}, xayUnmatchedLtr);
    AssertShapesMatch(api, withoutGsubGuest, withoutGsubOracle, reusableBuffer, xay, HB_DIRECTION_RTL, HB_SCRIPT_ARABIC, {}, xayUnmatchedRtl);
    AssertShapesMatch(api, withoutGsubGuest, withoutGsubOracle, reusableBuffer, fi, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, {}, fiUnligatedLtr);
    AssertShapesMatch(api, withoutGsubGuest, withoutGsubOracle, reusableBuffer, ffi, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, {}, ffiUnligatedLtr);
    AssertShapesMatch(api, withoutGsubGuest, withoutGsubOracle, reusableBuffer, ffi, HB_DIRECTION_RTL, HB_SCRIPT_ARABIC, {}, ffiUnligatedRtl);
    AssertShapesMatch(api, withoutGsubGuest, withoutGsubOracle, reusableBuffer, fiay, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, {}, fiayNoLigaLtr);
    AssertShapesMatch(api, withoutGsubGuest, withoutGsubOracle, reusableBuffer, fiay, HB_DIRECTION_RTL, HB_SCRIPT_ARABIC, {}, fiayNoLigaRtl);
    AssertShapesMatch(api, withoutGsubGuest, withoutGsubOracle, reusableBuffer, xayffi, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, {}, xayffiUnshapedLtr);
    AssertShapesMatch(api, withoutGsubGuest, withoutGsubOracle, reusableBuffer, xayffi, HB_DIRECTION_RTL, HB_SCRIPT_ARABIC, {}, xayffiUnshapedRtl);

    std::vector<hb_glyph_position_t> withPos;
    std::vector<hb_glyph_position_t> withoutPos;
    const auto withOut = ShapeWithGuest(api, withGsubGuest.font, reusableBuffer, xayffi, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, {}, withPos);
    const auto withoutOut = ShapeWithGuest(api, withoutGsubGuest.font, reusableBuffer, xayffi, HB_DIRECTION_LTR, HB_SCRIPT_LATIN, {}, withoutPos);
    Require(withOut.size() == 4 && withoutOut.size() == 6);
    Require(withOut[1].glyph != withoutOut[1].glyph);
    Require(withPos[1].x_advance != withoutPos[1].x_advance);
    Require(withOut[3].glyph != withoutOut[3].glyph);

    api.destroyBuffer(reusableBuffer);
}

int main(int argc, char** argv) try {
    Require(argc == 3);
    const bool patched = std::strcmp(argv[2], "patched") == 0;
    Require(patched || std::strcmp(argv[2], "unpatched") == 0);
#ifdef _WIN32
    auto module = LoadLibraryW(ArgumentPath(argv, 1).c_str());
    if (!module) std::fprintf(stderr, "LoadLibraryW failed: %lu\n", GetLastError());
#else
    auto module = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!module) std::fprintf(stderr, "%s\n", dlerror());
#endif
    Require(module != nullptr);
    Api api{};
    api.shape = Import<decltype(api.shape)>(module, patched ? "dM3hP4y99rM" : "hb_shape_nid_postfix");
    api.blob = Import<decltype(api.blob)>(module, patched ? "hj1KIIN5D54" : "hb_blob_create_nid_postfix");
    api.face = Import<decltype(api.face)>(module, patched ? "l7GEYs5PMVU" : "hb_face_create_for_tables_nid_postfix");
    api.destroyFace = Import<decltype(api.destroyFace)>(module, patched ? "ULNHQXciJ3s" : "hb_face_destroy_nid_postfix");
    api.font = Import<decltype(api.font)>(module, patched ? "FVhYusaWqA4" : "hb_font_create_nid_postfix");
    api.destroyFont = Import<decltype(api.destroyFont)>(module, patched ? "-8t2aEtFws4" : "hb_font_destroy_nid_postfix");
    api.buffer = Import<decltype(api.buffer)>(module, patched ? "7f13XmbjTZo" : "hb_buffer_create_nid_postfix");
    api.destroyBuffer = Import<decltype(api.destroyBuffer)>(module, patched ? "K-y1p7Z9THQ" : "hb_buffer_destroy_nid_postfix");
    api.addUtf16 = Import<decltype(api.addUtf16)>(module, patched ? "P+hgu937lFs" : "hb_buffer_add_utf16_nid_postfix");
    api.getLength = Import<decltype(api.getLength)>(module, patched ? "leHmVbjHyGw" : "hb_buffer_get_length_nid_postfix");
    api.setDirection = Import<decltype(api.setDirection)>(module, patched ? "845S2XQdxhw" : "hb_buffer_set_direction_nid_postfix");
    api.setScript = Import<decltype(api.setScript)>(module, patched ? "eGbbvD8o7go" : "hb_buffer_set_script_nid_postfix");
    api.getGlyphInfos = Import<decltype(api.getGlyphInfos)>(module, patched ? "ENsHmJCNG4Q" : "hb_buffer_get_glyph_infos_nid_postfix");
    api.getGlyphPositions = Import<decltype(api.getGlyphPositions)>(module, patched ? "43mnfTQPCbw" : "hb_buffer_get_glyph_positions_nid_postfix");
    api.resetBuffer = Import<decltype(api.resetBuffer)>(module, patched ? "PEggcTbDlfE" : "hb_buffer_reset_nid_postfix");
    api.guessSegmentProperties = Import<decltype(api.guessSegmentProperties)>(module, patched ? "DcrJr531A4w" : "hb_buffer_guess_segment_properties_nid_postfix");
    api.getScriptTags = Import<decltype(api.getScriptTags)>(module, patched ? "wf6CfVJj9PQ" : "hb_ot_layout_table_get_script_tags_nid_postfix");
    api.getLanguageTags = Import<decltype(api.getLanguageTags)>(module, patched ? "HH-sRSNiSaU" : "hb_ot_layout_script_get_language_tags_nid_postfix");
    api.findFeature = Import<decltype(api.findFeature)>(module, patched ? "qos3JIrKrDg" : "hb_ot_layout_language_find_feature_nid_postfix");
    Check(api);
#ifdef _WIN32
    Require(FreeLibrary(module) != 0);
#else
    Require(dlclose(module) == 0);
#endif
} catch (const std::exception& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
}
