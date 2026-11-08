#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

std::string Context;

void Expect(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "%s: %s\n", Context.c_str(), what);
    std::abort();
}

void Equal(std::int32_t actual, std::int32_t expected, const char* what) {
    if (actual == expected) return;
    std::fprintf(stderr, "%s: %s: got %d, expected %d\n", Context.c_str(), what, actual, expected);
    std::abort();
}

void Append(std::u16string& text, std::uint32_t point) {
    Expect(point <= 0x10ffff, "valid test code point");
    if (point > 0xffff) {
        text += static_cast<char16_t>(0xd800 + ((point - 0x10000) >> 10));
        text += static_cast<char16_t>(0xdc00 + ((point - 0x10000) & 0x3ff));
    } else {
        text += static_cast<char16_t>(point);
    }
}

void Check(void* iterator, const std::u16string& text, const std::vector<std::int32_t>& boundaries,
           const std::vector<std::int32_t>& tags = {}) {
    std::int32_t status = 0;
    ubrk_setText(iterator, text.data(), static_cast<std::int32_t>(text.size()), &status);
    Expect(status <= 0, "setText success");
    Equal(ubrk_first(iterator), 0, "first");
    for (std::size_t index = 0; index < boundaries.size(); ++index) {
        if (index) Equal(ubrk_next(iterator), boundaries[index], "next");
        Equal(ubrk_current(iterator), boundaries[index], "current forward");
        if (!tags.empty()) Equal(ubrk_getRuleStatus(iterator), tags[index], "rule status forward");
    }
    Equal(ubrk_next(iterator), -1, "next DONE");
    Equal(ubrk_current(iterator), static_cast<std::int32_t>(text.size()), "current after DONE");
    Equal(ubrk_last(iterator), static_cast<std::int32_t>(text.size()), "last");
    for (std::size_t index = boundaries.size(); index-- > 0;) {
        if (index + 1 != boundaries.size()) Equal(ubrk_previous(iterator), boundaries[index], "previous");
        Equal(ubrk_current(iterator), boundaries[index], "current reverse");
        if (!tags.empty()) Equal(ubrk_getRuleStatus(iterator), tags[index], "rule status reverse");
    }
    Equal(ubrk_previous(iterator), -1, "previous DONE");
    for (std::int32_t offset = -1; offset <= static_cast<std::int32_t>(text.size()) + 1; ++offset) {
        auto normalized = std::clamp(offset, 0, static_cast<std::int32_t>(text.size()));
        if (normalized > 0 && normalized < static_cast<std::int32_t>(text.size()) &&
            text[normalized] >= 0xdc00 && text[normalized] <= 0xdfff &&
            text[normalized - 1] >= 0xd800 && text[normalized - 1] <= 0xdbff) --normalized;
        const auto after = std::upper_bound(boundaries.begin(), boundaries.end(), normalized);
        Equal(ubrk_following(iterator, offset), offset < 0 ? 0 : after == boundaries.end() ? -1 : *after, "following");
        const auto before = std::lower_bound(boundaries.begin(), boundaries.end(), normalized);
        Equal(ubrk_preceding(iterator, offset), offset > static_cast<std::int32_t>(text.size()) ? boundaries.back() :
              before == boundaries.begin() ? -1 : *std::prev(before), "preceding");
        Equal(ubrk_isBoundary(iterator, offset), std::binary_search(boundaries.begin(), boundaries.end(), offset), "isBoundary");
        if (offset >= 0 && offset <= static_cast<std::int32_t>(text.size())) {
            const auto at = std::lower_bound(boundaries.begin(), boundaries.end(), offset);
            Equal(ubrk_current(iterator), *at, "isBoundary positions at next boundary");
        }
    }
}

void* Open(std::int32_t type, const char* locale = "en") {
    std::int32_t status = 0;
    auto* iterator = ubrk_open(type, locale, nullptr, 0, &status);
    Expect(iterator && status <= 0, "open");
    return iterator;
}

void Unicode(std::int32_t type) {
    const char* files[] = {"GraphemeBreakTest.txt", "WordBreakTest.txt", "LineBreakTest.txt", "SentenceBreakTest.txt"};
    std::ifstream input(std::string(ANYPS5_ICU_BREAK_TEST_DATA) + "/" + files[type]);
    Expect(input.good(), "open Unicode test data");
    void* iterator = Open(type);
    std::string line;
    int number = 0;
    int count = 0;
    while (std::getline(input, line)) {
        ++number;
        line = line.substr(0, line.find('#'));
        std::istringstream tokens(line);
        std::string token;
        std::u16string text;
        std::vector<std::int32_t> boundaries;
        while (tokens >> token) {
            if (token == "÷") boundaries.push_back(static_cast<std::int32_t>(text.size()));
            else if (token != "×") Append(text, static_cast<std::uint32_t>(std::stoul(token, nullptr, 16)));
        }
        if (text.empty()) continue;
        if (boundaries.empty() || boundaries.front() != 0) boundaries.insert(boundaries.begin(), 0);
        Context = std::string(files[type]) + ":" + std::to_string(number);
        Check(iterator, text, boundaries);
        ++count;
    }
    Expect(count > 400, "complete Unicode test file");
    ubrk_close(iterator);
    std::printf("%s: %d source cases passed\n", files[type], count);
}

std::u16string Decode(const std::string& input) {
    std::u16string result;
    for (std::size_t i = 0; i < input.size();) {
        auto point = static_cast<unsigned char>(input[i++]);
        std::uint32_t value = point;
        if (point == '\\') {
            Expect(i < input.size(), "test escape");
            const char escaped = input[i++];
            if (escaped == 'u' || escaped == 'U') {
                const auto length = escaped == 'u' ? 4 : 8;
                value = static_cast<std::uint32_t>(std::stoul(input.substr(i, length), nullptr, 16));
                i += length;
            } else if (escaped == 'n') value = '\n';
            else if (escaped == 'r') value = '\r';
            else if (escaped == 't') value = '\t';
            else value = static_cast<unsigned char>(escaped);
        } else if (point >= 0xc0) {
            int extra = point < 0xe0 ? 1 : point < 0xf0 ? 2 : 3;
            value &= (1u << (6 - extra)) - 1;
            while (extra--) value = (value << 6) | (static_cast<unsigned char>(input[i++]) & 0x3f);
        }
        Append(result, value);
    }
    return result;
}

void SourceExamples() {
    std::ifstream input(std::string(ANYPS5_ICU_BREAK_TEST_DATA) + "/rbbitst.txt");
    Expect(input.good(), "open ICU regression data");
    const std::array selected = {94, 95, 668, 669, 677, 788, 791, 795, 796, 1242, 1250, 1255, 1325};
    std::string line;
    std::int32_t type = -1;
    std::string locale = "en";
    int number = 0;
    int count = 0;
    while (std::getline(input, line)) {
        ++number;
        if (line.starts_with("<locale ")) locale = line.substr(8, line.find('>') - 8);
        if (line == "<char>") type = 0;
        if (line == "<word>") type = 1;
        if (line == "<line>") type = 2;
        if (line == "<sent>") type = 3;
        if (std::find(selected.begin(), selected.end(), number) == selected.end()) continue;
        Context = "rbbitst.txt:" + std::to_string(number);
        Expect(line.starts_with("<data>") && line.find("</data>") != std::string::npos, "selected source fixture");
        auto source = Decode(line.substr(6, line.find("</data>") - 6));
        std::u16string text;
        std::vector<std::int32_t> boundaries;
        std::vector<std::int32_t> tags;
        for (std::size_t i = 0; i < source.size(); ++i) {
            if (source[i] == u'•' || source[i] == u'<') {
                int tag = 0;
                if (source[i] == u'<') {
                    while (++i < source.size() && source[i] != u'>') tag = tag * 10 + source[i] - u'0';
                }
                boundaries.push_back(static_cast<std::int32_t>(text.size()));
                tags.push_back(tag);
            } else text += source[i];
        }
        void* iterator = Open(type, locale.c_str());
        Check(iterator, text, boundaries, tags);
        ubrk_close(iterator);
        ++count;
    }
    Equal(count, static_cast<int>(selected.size()), "all ICU fixtures read");
}

void Api() {
    Context = "API";
    for (int type = 0; type < 4; ++type) {
        void* iterator = Open(type);
        Check(iterator, u"", {0});
        ubrk_close(iterator);
    }
    void* iterator = Open(0);
    Check(iterator, std::u16string(u"a\0b", 3), {0, 1, 2, 3});
    std::int32_t status = 0;
    ubrk_setText(iterator, u"a\0b", -1, &status);
    Equal(ubrk_last(iterator), 1, "terminated text");
    ubrk_setText(iterator, nullptr, 0, &status);
    Equal(ubrk_last(iterator), 0, "null empty text");
    for (auto length : {-2, 1}) {
        status = 0;
        ubrk_setText(iterator, nullptr, length, &status);
        Equal(status, 1, "invalid text");
    }
    status = 15;
    ubrk_setText(iterator, u"new text", -1, &status);
    Equal(status, 15, "input failure preserved");
    Equal(ubrk_last(iterator), 0, "failed setText preserves text");
    status = -128;
    ubrk_setText(iterator, u"xy", -1, &status);
    Equal(status, -128, "input warning preserved");
    Equal(ubrk_last(iterator), 2, "warning allows setText");
    ubrk_close(iterator);
    ubrk_close(nullptr);
    status = 0;
    Expect(!ubrk_open(99, "en", u"a", 1, &status), "invalid iterator type");
    Equal(status, 1, "invalid type error");
    status = 15;
    Expect(!ubrk_open(0, "en", u"a", 1, &status), "input failure prevents open");
    Equal(status, 15, "open preserves failure");
    status = 0;
    iterator = ubrk_open(0, "zz_ZZ", u"a", 1, &status);
    Expect(iterator && status == -127, "unknown locale uses root with default warning");
    Equal(ubrk_last(iterator), 1, "fallback iterator works");
    ubrk_close(iterator);
    status = 0;
    iterator = ubrk_open(0, "en", nullptr, -99, &status);
    Expect(iterator && status <= 0, "null open text ignores length");
    ubrk_close(iterator);
    status = 0;
    iterator = ubrk_open(0, "en", u"a", -2, &status);
    Expect(iterator && status == 1, "invalid open text returns allocated iterator and error");
    ubrk_close(iterator);
    const auto count = ubrk_countAvailable();
    Expect(count > 100, "locale availability");
    for (const auto index : {-1, count}) {
        bool threw = false;
        try {
            ubrk_getAvailable(index);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        Expect(threw, "locale bounds throw where ICU reads outside its array");
    }
    Expect(!ubrk_getAvailable(count + 1), "locale index beyond count returns null");
    bool en = false;
    bool th = false;
    for (int i = 0; i < count; ++i) {
        const char* available = ubrk_getAvailable(i);
        Expect(available && *available, "available locale name");
        en |= std::string(available) == "en";
        th |= std::string(available) == "th";
    }
    Expect(en && th, "English and Thai locales available");
    auto worker = [] {
        auto* own = Open(0);
        for (int i = 0; i < 100; ++i) {
            std::int32_t error = 0;
            ubrk_setText(own, u"a\u0301\U0001f600", 4, &error);
            Expect(error <= 0, "concurrent setText");
            Equal(ubrk_first(own), 0, "concurrent first");
            Equal(ubrk_next(own), 2, "concurrent next");
            Equal(ubrk_next(own), 4, "concurrent supplementary");
        }
        ubrk_close(own);
    };
    std::thread first(worker);
    std::thread second(worker);
    first.join();
    second.join();
}

}

int main(int argc, char** argv) {
    if (argc == 2) Unicode(std::stoi(argv[1]));
    else {
        SourceExamples();
        Api();
    }
}
