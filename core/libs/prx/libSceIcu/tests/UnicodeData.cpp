#include "prx/libSceIcu/libSceIcu.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <string>
#include <vector>

namespace {

constexpr std::int32_t kCodePoints = 0x110000;

struct Properties {
    std::int32_t category = 0;
    std::int32_t direction = -1;
    std::int32_t combining = 0;
    std::int32_t digit = -1;
};

[[noreturn]] void Fail(const std::string& what) {
    std::fprintf(stderr, "failed: %s\n", what.c_str());
    std::abort();
}

std::int32_t Index(const char* const* names, std::size_t count, const std::string& name, const std::string& line) {
    for (std::size_t index = 0; index < count; ++index)
        if (name == names[index]) return static_cast<std::int32_t>(index);
    Fail("unknown value " + name + " in " + line);
}

std::int32_t Category(const std::string& name, const std::string& line) {
    static const char* const names[] = {"Cn", "Lu", "Ll", "Lt", "Lm", "Lo", "Mn", "Me", "Mc", "Nd", "Nl", "No", "Zs", "Zl", "Zp",
                                        "Cc", "Cf", "Co", "Cs", "Pd", "Ps", "Pe", "Pc", "Po", "Sm", "Sc", "Sk", "So", "Pi", "Pf"};
    return Index(names, sizeof(names) / sizeof(names[0]), name, line);
}

std::int32_t Direction(const std::string& name, const std::string& line) {
    static const char* const names[] = {"L", "R", "EN", "ES", "ET", "AN", "CS", "B", "S", "WS", "ON", "LRE",
                                        "LRO", "AL", "RLE", "RLO", "PDF", "NSM", "BN", "FSI", "LRI", "RLI", "PDI"};
    return Index(names, sizeof(names) / sizeof(names[0]), name, line);
}

std::int32_t Number(const std::string& text, int base, const std::string& line) {
    if (text.empty()) Fail("empty number in " + line);
    std::size_t used = 0;
    long value = 0;
    try {
        value = std::stol(text, &used, base);
    } catch (...) {
        Fail("bad number " + text + " in " + line);
    }
    if (used != text.size()) Fail("bad number " + text + " in " + line);
    return static_cast<std::int32_t>(value);
}

bool EndsWith(const std::string& text, const std::string& suffix) {
    return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::vector<Properties> Load(std::int32_t& listed) {
    std::ifstream file(ANYPS5_ICU_UNICODE_DATA);
    if (!file) Fail(std::string("cannot open ") + ANYPS5_ICU_UNICODE_DATA);
    std::vector<Properties> table(kCodePoints);
    std::vector<bool> seen(kCodePoints);
    std::int32_t first = -1;
    std::int32_t previous = -1;
    std::string line;
    listed = 0;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::vector<std::string> fields;
        std::size_t start = 0;
        for (std::size_t end; (end = line.find(';', start)) != std::string::npos; start = end + 1) fields.push_back(line.substr(start, end - start));
        fields.push_back(line.substr(start));
        if (fields.size() != 15) Fail("expected 15 fields in " + line);
        const std::int32_t code = Number(fields[0], 16, line);
        if (code <= previous || code >= kCodePoints) Fail("code point out of order in " + line);
        previous = code;
        Properties properties;
        properties.category = Category(fields[2], line);
        properties.combining = Number(fields[3], 10, line);
        properties.direction = Direction(fields[4], line);
        properties.digit = fields[6].empty() ? -1 : Number(fields[6], 10, line);
        if (EndsWith(fields[1], ", First>")) {
            if (first >= 0) Fail("nested range in " + line);
            first = code;
            continue;
        }
        std::int32_t low = code;
        if (EndsWith(fields[1], ", Last>")) {
            if (first < 0) Fail("range end without start in " + line);
            low = first;
            first = -1;
        } else if (first >= 0) {
            Fail("range start without end before " + line);
        }
        for (std::int32_t character = low; character <= code; ++character) {
            table[character] = properties;
            seen[character] = true;
            ++listed;
        }
    }
    if (first >= 0) Fail("unterminated range at end of file");
    if (!seen[0x41] || !seen[0x10fffd] || !seen[0xd800] || !seen[0x4e00]) Fail("UnicodeData.txt is incomplete");
    return table;
}

struct Mismatches {
    std::int32_t count = 0;

    void Check(const char* what, std::int32_t character, std::int64_t actual, std::int64_t expected) {
        if (actual == expected) return;
        if (++count <= 20)
            std::fprintf(stderr, "%s(0x%x) returned %lld, expected %lld\n", what, static_cast<unsigned>(character),
                         static_cast<long long>(actual), static_cast<long long>(expected));
    }
};

struct Enumerated {
    const std::vector<Properties>* table;
    Mismatches* mismatches;
    std::int32_t next;
};

std::int8_t APS5_VABI CheckRange(const void* context, std::int32_t start, std::int32_t limit, std::int32_t type) {
    auto& enumerated = *static_cast<Enumerated*>(const_cast<void*>(context));
    enumerated.mismatches->Check("u_enumCharTypes start", start, start, enumerated.next);
    for (std::int32_t character = start; character < limit && character < kCodePoints; ++character)
        enumerated.mismatches->Check("u_enumCharTypes", character, type, (*enumerated.table)[character].category);
    enumerated.next = limit;
    return 1;
}

}

int main() {
    std::int32_t listed = 0;
    const std::vector<Properties> table = Load(listed);
    Mismatches mismatches;
    for (std::int32_t character = 0; character < kCodePoints; ++character) {
        const Properties& expected = table[character];
        mismatches.Check("u_charType", character, u_charType_nid_postfix(character), expected.category);
        mismatches.Check("u_getCombiningClass", character, u_getCombiningClass_nid_postfix(character), expected.combining);
        mismatches.Check("u_charDigitValue", character, u_charDigitValue_nid_postfix(character), expected.digit);
        if (expected.direction >= 0)
            mismatches.Check("u_charDirection", character, u_charDirection_nid_postfix(character), expected.direction);
    }
    Enumerated enumerated{&table, &mismatches, 0};
    u_enumCharTypes_nid_postfix(CheckRange, &enumerated);
    mismatches.Check("u_enumCharTypes end", 0, enumerated.next, kCodePoints);
    for (const std::int32_t character : {std::numeric_limits<std::int32_t>::min(), -1, kCodePoints, std::numeric_limits<std::int32_t>::max()}) {
        mismatches.Check("u_charType", character, u_charType_nid_postfix(character), 0);
        mismatches.Check("u_getCombiningClass", character, u_getCombiningClass_nid_postfix(character), 0);
        mismatches.Check("u_charDigitValue", character, u_charDigitValue_nid_postfix(character), -1);
        mismatches.Check("u_charDirection", character, u_charDirection_nid_postfix(character), 0);
    }
    if (mismatches.count) {
        std::fprintf(stderr, "%d mismatches against UnicodeData.txt\n", mismatches.count);
        return 1;
    }
    std::printf("ICU properties match UnicodeData.txt for all %d code points (%d listed)\n", kCodePoints, listed);
}
