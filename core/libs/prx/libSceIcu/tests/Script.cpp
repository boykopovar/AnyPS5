#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>

namespace {

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "ICU script check failed at line %u\n", location.line());
    std::abort();
}

void CheckScript() {
    struct Case {
        std::int32_t character;
        std::int32_t script;
    };
    constexpr Case cases[] = {
        {0, 0}, {'0', 0}, {'A', 25}, {0x0300, 1}, {0x03a9, 14}, {0x042f, 8},
        {0x05d0, 19}, {0x0628, 2}, {0x0d02, 26}, {0x304b, 20}, {0x30ab, 22},
        {0x30fc, 0}, {0x5b57, 17}, {0xd800, 103}, {0xdfff, 103}, {0xe000, 103},
        {0x0378, 103}, {0x10414, 9}, {0x10d5d, 201}, {0x20000, 17}, {0x10ffff, 103}
    };
    for (const auto& item : cases) {
        std::int32_t status = 0;
        Require(uscript_getScript_nid_postfix(item.character, &status) == item.script && status == 0);
        status = -127;
        Require(uscript_getScript_nid_postfix(item.character, &status) == item.script && status == -127);
    }
    for (const auto character : {-1, 0x110000, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()}) {
        std::int32_t status = 0;
        Require(uscript_getScript_nid_postfix(character, &status) == -1 && status == 1);
        Require(uscript_hasScript_nid_postfix(character, 103) == 1);
        Require(uscript_hasScript_nid_postfix(character, 0) == 0);
    }
    std::int32_t status = 7;
    Require(uscript_getScript_nid_postfix('A', &status) == -1 && status == 7);
    Require(uscript_getScript_nid_postfix(-1, &status) == -1 && status == 7);
    Require(uscript_getScript_nid_postfix('A', nullptr) == -1);
    status = std::numeric_limits<std::int32_t>::min();
    Require(uscript_getScript_nid_postfix('A', &status) == 25 && status == std::numeric_limits<std::int32_t>::min());
    Require(uscript_hasScript_nid_postfix('A', 25) == 1);
    Require(uscript_hasScript_nid_postfix('A', 0) == 0);
    Require(uscript_hasScript_nid_postfix(0x0300, 1) == 0);
    Require(uscript_hasScript_nid_postfix(0x0300, 25) == 1);
    Require(uscript_hasScript_nid_postfix(0x034f, 1) == 1);
    Require(uscript_hasScript_nid_postfix(0x30fc, 20) == 1);
    Require(uscript_hasScript_nid_postfix(0x30fc, 22) == 1);
    Require(uscript_hasScript_nid_postfix(0x30fc, 0) == 0);
    for (const auto script : {-1, 213, 0x7fff, 0x8000, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()}) {
        Require(uscript_hasScript_nid_postfix('A', script) == 0);
        Require(uscript_hasScript_nid_postfix(0x30fc, script) == 0);
    }
}

void CheckExtensions() {
    constexpr std::array<std::int32_t, 7> commaScripts{5, 17, 18, 20, 22, 27, 41};
    constexpr std::int32_t sentinel = 0x5a5a5a5a;
    for (std::int32_t capacity = 0; capacity <= 8; ++capacity) {
        std::array<std::int32_t, 9> scripts;
        scripts.fill(sentinel);
        std::int32_t status = -127;
        Require(uscript_getScriptExtensions_nid_postfix(0x3001, scripts.data(), capacity, &status) == 7);
        Require(status == (capacity < 7 ? 15 : -127));
        for (std::int32_t index = 0; index < 9; ++index) {
            const auto expected = index < std::min(capacity, 7) ? commaScripts[index] : sentinel;
            Require(scripts[index] == expected);
        }
    }
    for (std::int32_t script = 0; script < 213; ++script) {
        const bool expected = std::find(commaScripts.begin(), commaScripts.end(), script) != commaScripts.end();
        Require(uscript_hasScript_nid_postfix(0x3001, script) == expected);
    }
    std::array<std::int32_t, 3> scripts{sentinel, sentinel, sentinel};
    std::int32_t status = 0;
    Require(uscript_getScriptExtensions_nid_postfix(0x3001, nullptr, 0, &status) == 7 && status == 15);
    status = 0;
    Require(uscript_getScriptExtensions_nid_postfix(0x30fc, scripts.data(), std::numeric_limits<std::int32_t>::max(), &status) == 2);
    Require(status == 0 && scripts[0] == 20 && scripts[1] == 22 && scripts[2] == sentinel);
    for (const auto warning : {-124, std::numeric_limits<std::int32_t>::min()}) {
        status = warning;
        Require(uscript_getScriptExtensions_nid_postfix(0x30fc, scripts.data(), 3, &status) == 2 && status == warning);
    }
    for (const auto character : std::array<std::int32_t, 4>{'A', 0x034f, -1, 0x110000}) {
        scripts.fill(sentinel);
        status = 0;
        const auto expected = character == 'A' ? 25 : character == 0x034f ? 1 : 103;
        Require(uscript_getScriptExtensions_nid_postfix(character, scripts.data(), 1, &status) == 1);
        Require(status == 0 && scripts[0] == expected && scripts[1] == sentinel);
        status = 0;
        Require(uscript_getScriptExtensions_nid_postfix(character, nullptr, 0, &status) == 1 && status == 15);
    }
    constexpr std::array<std::int32_t, 8> graveScripts{6, 7, 8, 14, 25, 52, 89, 205};
    std::array<std::int32_t, 8> graveOutput{};
    status = 0;
    Require(uscript_getScriptExtensions_nid_postfix(0x0300, graveOutput.data(), 8, &status) == 8);
    Require(status == 0 && graveOutput == graveScripts);
    scripts.fill(sentinel);
    status = 0;
    Require(uscript_getScriptExtensions_nid_postfix('A', scripts.data(), -1, &status) == 0 && status == 1);
    Require(scripts[0] == sentinel);
    status = 0;
    Require(uscript_getScriptExtensions_nid_postfix('A', nullptr, 1, &status) == 0 && status == 1);
    status = 7;
    Require(uscript_getScriptExtensions_nid_postfix(0x3001, scripts.data(), 3, &status) == 0 && status == 7);
    Require(uscript_getScriptExtensions_nid_postfix('A', nullptr, -1, &status) == 0 && status == 7);
    Require(uscript_getScriptExtensions_nid_postfix(0x3001, scripts.data(), 3, nullptr) == 0);
    Require(std::all_of(scripts.begin(), scripts.end(), [](auto value) { return value == sentinel; }));
}

void CheckMetadata() {
    struct Case {
        std::int32_t script;
        const char* shortName;
        const char* name;
        std::int32_t usage;
        bool rightToLeft;
        bool breaksBetweenLetters;
        bool cased;
    };
    constexpr Case cases[] = {
        {0, "Zyyy", "Common", 5, false, false, false},
        {1, "Zinh", "Inherited", 5, false, false, false},
        {2, "Arab", "Arabic", 5, true, false, false},
        {9, "Dsrt", "Deseret", 2, false, false, true},
        {17, "Hani", "Han", 5, false, true, false},
        {19, "Hebr", "Hebrew", 5, true, false, false},
        {25, "Latn", "Latin", 5, false, false, true},
        {38, "Thai", "Thai", 5, false, true, false},
        {103, "Zzzz", "Unknown", 1, false, false, false},
        {201, "Gara", "Garay", 2, true, false, true},
        {212, "Hntl", "Hntl", 0, false, false, false}
    };
    for (const auto& item : cases) {
        const auto* shortName = uscript_getShortName_nid_postfix(item.script);
        const auto* name = uscript_getName_nid_postfix(item.script);
        Require(shortName && std::strcmp(shortName, item.shortName) == 0);
        Require(name && std::strcmp(name, item.name) == 0);
        Require(uscript_getUsage_nid_postfix(item.script) == item.usage);
        Require(uscript_isRightToLeft_nid_postfix(item.script) == item.rightToLeft);
        Require(uscript_breaksBetweenLetters_nid_postfix(item.script) == item.breaksBetweenLetters);
        Require(uscript_isCased_nid_postfix(item.script) == item.cased);
    }
    for (const auto script : {-1, 213, std::numeric_limits<std::int32_t>::min(), std::numeric_limits<std::int32_t>::max()}) {
        Require(uscript_getShortName_nid_postfix(script) == nullptr);
        Require(uscript_getName_nid_postfix(script) == nullptr);
        Require(uscript_getUsage_nid_postfix(script) == 0);
        Require(uscript_isRightToLeft_nid_postfix(script) == 0);
        Require(uscript_breaksBetweenLetters_nid_postfix(script) == 0);
        Require(uscript_isCased_nid_postfix(script) == 0);
    }
}

void CheckSamples() {
    constexpr char16_t sentinel = 0x5555;
    std::array<char16_t, 4> sample;
    for (std::int32_t capacity = 0; capacity <= 3; ++capacity) {
        sample.fill(sentinel);
        std::int32_t status = 0;
        Require(uscript_getSampleString_nid_postfix(9, sample.data(), capacity, &status) == 2);
        Require(status == (capacity < 2 ? 15 : capacity == 2 ? -124 : 0));
        Require(sample[0] == (capacity >= 2 ? 0xd801 : sentinel));
        Require(sample[1] == (capacity >= 2 ? 0xdc14 : sentinel));
        Require(sample[2] == (capacity > 2 ? 0 : sentinel));
        Require(sample[3] == sentinel);
    }
    sample.fill(sentinel);
    std::int32_t status = -127;
    Require(uscript_getSampleString_nid_postfix(25, sample.data(), 2, &status) == 1);
    Require(status == -127 && sample[0] == u'L' && sample[1] == 0 && sample[2] == sentinel);
    status = std::numeric_limits<std::int32_t>::min();
    Require(uscript_getSampleString_nid_postfix(25, sample.data(), 2, &status) == 1);
    Require(status == std::numeric_limits<std::int32_t>::min());
    sample.fill(sentinel);
    status = -124;
    Require(uscript_getSampleString_nid_postfix(25, sample.data(), std::numeric_limits<std::int32_t>::max(), &status) == 1);
    Require(status == 0 && sample[0] == u'L' && sample[1] == 0 && sample[2] == sentinel);
    sample.fill(sentinel);
    status = 0;
    Require(uscript_getSampleString_nid_postfix(25, sample.data(), 1, &status) == 1);
    Require(status == -124 && sample[0] == u'L' && sample[1] == sentinel);
    status = 0;
    Require(uscript_getSampleString_nid_postfix(25, nullptr, 0, &status) == 1 && status == 15);
    for (const auto script : {-1, 54, 213}) {
        sample.fill(sentinel);
        status = -124;
        Require(uscript_getSampleString_nid_postfix(script, sample.data(), 1, &status) == 0);
        Require(status == 0 && sample[0] == 0 && sample[1] == sentinel);
        status = 0;
        Require(uscript_getSampleString_nid_postfix(script, nullptr, 0, &status) == 0 && status == -124);
    }
    sample.fill(sentinel);
    status = 0;
    Require(uscript_getSampleString_nid_postfix(25, sample.data(), -1, &status) == 0 && status == 1);
    status = 0;
    Require(uscript_getSampleString_nid_postfix(25, nullptr, 1, &status) == 0 && status == 1);
    status = 7;
    Require(uscript_getSampleString_nid_postfix(25, sample.data(), 4, &status) == 0 && status == 7);
    Require(uscript_getSampleString_nid_postfix(25, nullptr, -1, &status) == 0 && status == 7);
    Require(std::all_of(sample.begin(), sample.end(), [](auto value) { return value == sentinel; }));
}

}

int main() {
    CheckScript();
    CheckExtensions();
    CheckMetadata();
    CheckSamples();
}
