#include "prx/libharfbuzz/libharfbuzz.hpp"

#include <hb.h>

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string_view>
#include <vector>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "HarfBuzz buffer check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

struct BufferPair {
    hb_buffer_t* guest = hb_buffer_create_nid_postfix();
    hb_buffer_t* native = hb_buffer_create();

    BufferPair() {
        Require(guest && native && guest != native);
    }

    ~BufferPair() {
        hb_buffer_destroy_nid_postfix(guest);
        hb_buffer_destroy(native);
    }

    void Reset() {
        hb_buffer_reset_nid_postfix(guest);
        hb_buffer_reset(native);
    }

    void Add(const std::uint16_t* text, int textLength, unsigned itemOffset, int itemLength) {
        hb_buffer_add_utf16_nid_postfix(guest, text, textLength, itemOffset, itemLength);
        hb_buffer_add_utf16(native, text, textLength, itemOffset, itemLength);
    }

    void Guess() {
        hb_buffer_guess_segment_properties_nid_postfix(guest);
        hb_buffer_guess_segment_properties(native);
    }

    void Direction(hb_direction_t direction) {
        hb_buffer_set_direction_nid_postfix(guest, direction);
        hb_buffer_set_direction(native, direction);
    }

    void Script(hb_script_t script) {
        hb_buffer_set_script_nid_postfix(guest, script);
        hb_buffer_set_script(native, script);
    }

    void Compare(bool comparePositions = false) {
        Require(hb_buffer_get_length_nid_postfix(guest) == hb_buffer_get_length(native));
        Require(hb_buffer_get_direction_nid_postfix(guest) == hb_buffer_get_direction(native));
        Require(hb_buffer_get_script(guest) == hb_buffer_get_script(native));
        const auto guestLanguage = hb_buffer_get_language(guest);
        const auto nativeLanguage = hb_buffer_get_language(native);
        Require(bool(guestLanguage) == bool(nativeLanguage));
        if (guestLanguage) Require(std::strcmp(hb_language_to_string(guestLanguage), hb_language_to_string(nativeLanguage)) == 0);

        std::array<unsigned, 3> lengths{0x11223344, 0x55667788, 0x99AABBCC};
        unsigned nativeLength = 0xFFFFFFFF;
        auto* infos = hb_buffer_get_glyph_infos_nid_postfix(guest, &lengths[1]);
        const auto* nativeInfos = hb_buffer_get_glyph_infos(native, &nativeLength);
        Require(lengths[0] == 0x11223344 && lengths[2] == 0x99AABBCC);
        Require(lengths[1] == nativeLength && nativeLength == hb_buffer_get_length(native));
        Require(bool(infos) == bool(nativeInfos));
        Require(hb_buffer_get_glyph_infos_nid_postfix(guest, nullptr) == infos);
        for (unsigned index = 0; index < nativeLength; ++index) {
            Require(infos[index].codepoint == nativeInfos[index].codepoint);
            Require(infos[index].mask == nativeInfos[index].mask);
            Require(infos[index].cluster == nativeInfos[index].cluster);
            Require(infos[index].var1.u32 == nativeInfos[index].var1.u32);
            Require(infos[index].var2.u32 == nativeInfos[index].var2.u32);
        }

        if (!comparePositions) return;
        lengths[1] = 0x55667788;
        auto* positions = hb_buffer_get_glyph_positions_nid_postfix(guest, &lengths[1]);
        const auto* nativePositions = hb_buffer_get_glyph_positions(native, &nativeLength);
        Require(lengths[0] == 0x11223344 && lengths[2] == 0x99AABBCC);
        Require(lengths[1] == nativeLength && nativeLength == hb_buffer_get_length(native));
        Require(bool(positions) == bool(nativePositions));
        Require(hb_buffer_get_glyph_positions_nid_postfix(guest, nullptr) == positions);
        for (unsigned index = 0; index < nativeLength; ++index) {
            Require(positions[index].x_advance == nativePositions[index].x_advance);
            Require(positions[index].y_advance == nativePositions[index].y_advance);
            Require(positions[index].x_offset == nativePositions[index].x_offset);
            Require(positions[index].y_offset == nativePositions[index].y_offset);
            Require(positions[index].var.u32 == nativePositions[index].var.u32);
        }
    }
};

static void Utf16() {
    BufferPair pair;
    pair.Compare();
    const std::array<std::uint16_t, 9> text{'A', 0xD83D, 0xDE00, 0xD800, 'B', 0xDC00, 0, 'C', 0};
    pair.Add(text.data(), 8, 0, -1);
    const auto* infos = hb_buffer_get_glyph_infos_nid_postfix(pair.guest, nullptr);
    const std::array<std::uint32_t, 7> codepoints{'A', 0x1F600, 0xFFFD, 'B', 0xFFFD, 0, 'C'};
    const std::array<unsigned, 7> clusters{0, 1, 3, 4, 5, 6, 7};
    Require(hb_buffer_get_length_nid_postfix(pair.guest) == codepoints.size());
    for (unsigned index = 0; index < codepoints.size(); ++index) {
        Require(infos[index].codepoint == codepoints[index]);
        Require(infos[index].cluster == clusters[index]);
    }
    pair.Compare(true);
    for (int length : {0, 1, 2, 3, 6, 8, -1}) {
        const unsigned size = length == -1 ? 6 : static_cast<unsigned>(length);
        for (unsigned offset = 0; offset <= size; ++offset) {
            for (unsigned count = 0; count <= size - offset; ++count) {
                pair.Reset();
                pair.Add(text.data(), length, offset, static_cast<int>(count));
                pair.Compare();
            }
            pair.Reset();
            pair.Add(text.data(), length, offset, -1);
            pair.Compare();
        }
    }

    std::vector<std::uint16_t> units(65537);
    for (unsigned index = 0; index < 65536; ++index) units[index] = static_cast<std::uint16_t>(index);
    pair.Reset();
    pair.Add(units.data(), 65536, 0, 65536);
    pair.Compare(true);

    std::mt19937 random(0x48424246);
    for (unsigned iteration = 0; iteration < 1000; ++iteration) {
        const unsigned size = random() % 257;
        units.resize(size + 1);
        for (unsigned index = 0; index < size; ++index) units[index] = static_cast<std::uint16_t>(random());
        units[size] = 0;
        const unsigned offset = random() % (size + 1);
        const unsigned count = random() % (size - offset + 1);
        pair.Reset();
        pair.Add(units.data(), static_cast<int>(size), offset, static_cast<int>(count));
        pair.Compare();
        pair.Add(text.data(), 8, 1, 2);
        pair.Compare();
    }
}

static void Properties() {
    BufferPair pair;
    pair.Guess();
    Require(hb_buffer_get_direction_nid_postfix(pair.guest) == HB_DIRECTION_LTR);
    pair.Compare();
    struct ScriptCase {
        std::uint16_t character;
        hb_script_t script;
        hb_direction_t direction;
    };
    const std::array<ScriptCase, 8> cases{{
        {'A', HB_SCRIPT_LATIN, HB_DIRECTION_LTR},
        {0x0627, HB_SCRIPT_ARABIC, HB_DIRECTION_RTL},
        {0x05D0, HB_SCRIPT_HEBREW, HB_DIRECTION_RTL},
        {0x03B1, HB_SCRIPT_GREEK, HB_DIRECTION_LTR},
        {0x0410, HB_SCRIPT_CYRILLIC, HB_DIRECTION_LTR},
        {0x0915, HB_SCRIPT_DEVANAGARI, HB_DIRECTION_LTR},
        {0x4E00, HB_SCRIPT_HAN, HB_DIRECTION_LTR},
        {0x3042, HB_SCRIPT_HIRAGANA, HB_DIRECTION_LTR}
    }};
    for (const auto& test : cases) {
        pair.Reset();
        const std::array<std::uint16_t, 5> text{' ', 0x0301, test.character, 'A', 0};
        pair.Add(text.data(), -1, 0, -1);
        pair.Guess();
        Require(hb_buffer_get_direction_nid_postfix(pair.guest) == test.direction);
        Require(hb_buffer_get_script(pair.guest) == test.script);
        pair.Compare();
        pair.Guess();
        pair.Compare();
        for (auto direction : {HB_DIRECTION_LTR, HB_DIRECTION_RTL, HB_DIRECTION_TTB, HB_DIRECTION_BTT}) {
            pair.Direction(direction);
            pair.Guess();
            Require(hb_buffer_get_direction_nid_postfix(pair.guest) == direction);
            pair.Compare();
        }
        pair.Script(HB_SCRIPT_ARABIC);
        pair.Direction(HB_DIRECTION_INVALID);
        pair.Guess();
        Require(hb_buffer_get_direction_nid_postfix(pair.guest) == HB_DIRECTION_RTL);
        pair.Compare();
        pair.Script(HB_SCRIPT_LATIN);
        pair.Guess();
        Require(hb_buffer_get_direction_nid_postfix(pair.guest) == HB_DIRECTION_RTL);
        pair.Compare();
    }
    pair.Reset();
    Require(hb_buffer_get_direction_nid_postfix(pair.guest) == HB_DIRECTION_INVALID);
    Require(hb_buffer_get_script(pair.guest) == HB_SCRIPT_INVALID);
    Require(hb_buffer_get_language(pair.guest) == HB_LANGUAGE_INVALID);
    pair.Compare();
}

static void Lifetime() {
    hb_buffer_destroy_nid_postfix(nullptr);
    BufferPair first;
    BufferPair second;
    const std::array<std::uint16_t, 4> text{'A', 'B', 'C', 0};
    for (unsigned iteration = 0; iteration < 128; ++iteration) {
        first.Add(text.data(), -1, 0, -1);
        first.Direction(HB_DIRECTION_RTL);
        second.Reset();
        second.Add(text.data(), 3, 1, 1);
        second.Script(HB_SCRIPT_LATIN);
        second.Guess();
        Require(hb_buffer_get_length_nid_postfix(first.guest) == 3 * (iteration + 1));
        Require(hb_buffer_get_length_nid_postfix(second.guest) == 1);
        first.Compare();
        second.Compare();
    }
    first.Compare(true);
    auto* infos = hb_buffer_get_glyph_infos_nid_postfix(first.guest, nullptr);
    auto* nativeInfos = hb_buffer_get_glyph_infos(first.native, nullptr);
    auto* positions = hb_buffer_get_glyph_positions_nid_postfix(first.guest, nullptr);
    auto* nativePositions = hb_buffer_get_glyph_positions(first.native, nullptr);
    infos[0].mask = nativeInfos[0].mask = 0xF00D;
    infos[0].cluster = nativeInfos[0].cluster = 0xABCDEF;
    positions[0].x_advance = nativePositions[0].x_advance = -1234;
    positions[0].y_offset = nativePositions[0].y_offset = 5678;
    first.Guess();
    first.Compare(true);
    Require(hb_buffer_get_glyph_infos_nid_postfix(first.guest, nullptr) == infos);
    Require(hb_buffer_get_glyph_positions_nid_postfix(first.guest, nullptr) == positions);
    second.Reset();
    first.Compare(true);
    first.Reset();
    first.Compare();
    first.Add(text.data(), -1, 0, -1);
    first.Compare(true);
    const auto* resetPositions = hb_buffer_get_glyph_positions_nid_postfix(first.guest, nullptr);
    Require(resetPositions[0].x_advance == 0 && resetPositions[0].y_offset == 0);
}

int main(int argc, char** argv) {
    Require(argc == 2);
    const std::string_view mode = argv[1];
    if (mode == "utf16") Utf16();
    else if (mode == "properties") Properties();
    else if (mode == "lifetime") Lifetime();
    else Require(false);
    std::printf("HarfBuzz buffer %s checks passed\n", argv[1]);
}
