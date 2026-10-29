#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <limits>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>
#include <vector>


namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU unescape check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void RequireThrows(TAction action) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false);
}

struct Input {
    std::u16string text;
    std::vector<std::int32_t> positions;
    std::int32_t* offset = nullptr;
    std::int32_t base = 0;
    std::vector<std::pair<std::int32_t, std::int32_t>> observations;
};

char16_t APS5_VABI Read_nid_no_patch(std::int32_t position, void* context) {
    auto& input = *static_cast<Input*>(context);
    const auto relative = static_cast<std::int64_t>(position) - input.base;
    Require(relative >= 0 && static_cast<std::size_t>(relative) < input.text.size());
    input.positions.push_back(position);
    if (input.offset) input.observations.emplace_back(static_cast<std::int32_t>(relative), *input.offset - input.base);
    return input.text[relative];
}

void At(std::u16string text, std::int32_t expected, std::int32_t consumed) {
    Input input{std::move(text), {}};
    std::int32_t offset = 0;
    Require(u_unescapeAt_nid_postfix(Read_nid_no_patch, &offset, input.text.size(), &input) == expected);
    Require(offset == consumed);
}

void CallbackOrder() {
    const auto check = [](std::u16string text, std::int32_t expected, std::int32_t consumed,
                          std::vector<std::pair<std::int32_t, std::int32_t>> observations, std::int32_t base = 0) {
        auto offset = base;
        Input input{std::move(text), {}, &offset, base, {}};
        const auto length = static_cast<std::int32_t>(static_cast<std::int64_t>(base) + input.text.size());
        Require(u_unescapeAt_nid_postfix(Read_nid_no_patch, &offset, length, &input) == expected);
        Require(offset - base == consumed && input.observations == observations);
    };
    check(u"u0041!", 0x41, 5, {{0, 1}, {1, 1}, {2, 2}, {3, 3}, {4, 4}});
    check(u"x{41}", 0x41, 5, {{0, 1}, {1, 1}, {2, 2}, {3, 3}, {4, 4}});
    check(u"u12", -1, 0, {{0, 1}, {1, 1}, {2, 2}});
    check(u"uD83D\\uDE00", 0x1f600, 11,
          {{0, 1}, {1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}, {6, 5}, {7, 5}, {8, 5}, {9, 5}, {10, 5}});
    check(u"uD800\\u12", 0xd800, 5,
          {{0, 1}, {1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}, {6, 5}, {7, 5}, {8, 5}});
    check(std::u16string(u"uD83D") + char16_t{0xde00}, 0x1f600, 6,
          {{0, 1}, {1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}}, std::numeric_limits<std::int32_t>::max() - 6);
    check(u"u", -1, 0, {{0, 1}}, std::numeric_limits<std::int32_t>::max() - 1);
}

void SingleEscapes() {
    At(u"u0041!", 0x41, 5);
    At(u"U0010ffff!", 0x10ffff, 9);
    At(u"xF!", 15, 2);
    At(u"xFF0", 255, 3);
    At(u"x{10FfFf}!", 0x10ffff, 9);
    At(u"x{010ffff}!", 0x10ffff, 10);
    At(u"x{0010ffff}!", -1, 0);
    At(u"7777", 511, 3);
    At(u"078", 7, 2);
    At(u"8", u'8', 1);
    At(u"cA", 1, 2);
    At(u"c", u'c', 1);
    At(u"c\U0001f600", 0, 3);
    At(u"\U0001f600", 0x1f600, 2);
    At(u"uD83D\\uDE00!", 0x1f600, 11);
    At(std::u16string(u"uD83D") + char16_t{0xde00} + u"!", 0x1f600, 6);
    At(u"uD83D\\x{0DE00}!", 0x1f600, 14);
    At(u"uD800\\uD801\\uDC00", 0xd800, 5);
    At(u"uD800\\u12", 0xd800, 5);
    At(u"uDC00", 0xdc00, 5);
    At(u"\\", u'\\', 1);
    At(u"-", u'-', 1);
    At(u"a", 7, 1);
    At(u"b", 8, 1);
    At(u"e", 27, 1);
    At(u"f", 12, 1);
    At(u"n", 10, 1);
    At(u"r", 13, 1);
    At(u"t", 9, 1);
    At(u"v", 11, 1);
    for (const auto* text : {u"", u"u", u"u123", u"U0000000", u"U00110000", u"Uffffffff", u"x", u"x{}", u"x{1", u"x{110000}"})
        At(text, -1, 0);
    Input input{u"prefixu0041suffix", {}};
    std::int32_t offset = 6;
    Require(u_unescapeAt_nid_postfix(Read_nid_no_patch, &offset, input.text.size(), &input) == 0x41 && offset == 11);
    for (const auto invalid : {-1, 17, std::numeric_limits<std::int32_t>::max()}) {
        offset = invalid;
        const auto calls = input.positions.size();
        Require(u_unescapeAt_nid_postfix(Read_nid_no_patch, &offset, input.text.size(), &input) == -1);
        Require(offset == invalid && input.positions.size() == calls);
    }
    offset = 0;
    Require(u_unescapeAt_nid_postfix(nullptr, &offset, -1, nullptr) == -1 && offset == 0);
    RequireThrows([&] { u_unescapeAt_nid_postfix(nullptr, &offset, 1, nullptr); });
    RequireThrows([] { u_unescapeAt_nid_postfix(Read_nid_no_patch, nullptr, 1, nullptr); });
}

void String(const char* escaped, std::u16string_view expected) {
    Require(u_unescape_nid_postfix(escaped, nullptr, std::numeric_limits<std::int32_t>::min()) == static_cast<int>(expected.size()));
    for (std::int32_t capacity = 0; capacity <= static_cast<std::int32_t>(expected.size()) + 2; ++capacity) {
        std::vector<char16_t> output(expected.size() + 4, 0xa55a);
        Require(u_unescape_nid_postfix(escaped, output.data() + 1, capacity) == static_cast<int>(expected.size()));
        auto prefix = std::min<std::size_t>(capacity, expected.size());
        if (prefix > 0 && prefix < expected.size() && expected[prefix - 1] >= 0xd800 && expected[prefix - 1] <= 0xdbff
            && expected[prefix] >= 0xdc00 && expected[prefix] <= 0xdfff) --prefix;
        Require(std::equal(expected.begin(), expected.begin() + prefix, output.begin() + 1));
        for (auto index = prefix; index < std::min<std::size_t>(capacity, expected.size()); ++index)
            Require(output[index + 1] == 0xa55a);
        if (capacity > static_cast<int>(expected.size())) Require(output[expected.size() + 1] == 0);
        Require(output.front() == 0xa55a);
        for (std::size_t index = capacity + 1; index < output.size(); ++index) Require(output[index] == 0xa55a);
    }
}

void Strings() {
    String("", u"");
    String("plain text", u"plain text");
    String("A\\u0042", u"AB");
    String("\\a\\b\\t\\n\\v\\f\\r\\e\\\"\\'\\?\\\\", u"\a\b\t\n\v\f\r\x1b\"'?\\");
    String("[a\\-z]\\cA\\777", u"[a-z]\x01\u01ff");
    String("A\\u0042\\U0001F600Z", u"AB\U0001f600Z");
    String("\\uD83D\\uDE00", u"\U0001f600");
    const char16_t unpaired[] = {0xd800, 0xd801, 0xdc00};
    String("\\uD800\\uD801\\uDC00", std::u16string_view(unpaired, 3));
    String("\\x{10ffff}\\xA\\123", u"\U0010ffff\nS");
    const char16_t embedded[] = {u'A', 0, u'B'};
    String("A\\0B", std::u16string_view(embedded, 3));
    for (unsigned int byte = 1; byte <= 255; ++byte) {
        if (byte == '\\') continue;
        const char source[] = {static_cast<char>(byte), 0};
        const char16_t expected[] = {static_cast<char16_t>(byte)};
        String(source, std::u16string_view(expected, 1));
    }
    char16_t destination[] = {0xa55a, 0xa55a, 0xa55a};
    Require(u_unescape_nid_postfix("\\U0001f600Z", destination, 1) == 3);
    Require(destination[0] == 0xa55a && destination[1] == 0xa55a);
    Require(u_unescape_nid_postfix("a\\u12", destination, 3) == 0 && destination[0] == 0 && destination[1] == 0xa55a);
    destination[0] = 0xa55a;
    Require(u_unescape_nid_postfix("abc\\u12", destination, 3) == 0 && destination[0] == 0 && destination[1] == u'b' && destination[2] == u'c');
    for (auto capacity : {-1, std::numeric_limits<std::int32_t>::min()}) {
        std::fill(std::begin(destination), std::end(destination), 0xa55a);
        RequireThrows([&] { u_unescape_nid_postfix("a\\U0001f600", destination, capacity); });
        Require(destination[0] == 0xa55a && destination[1] == 0xa55a && destination[2] == 0xa55a);
    }
    RequireThrows([] { u_unescape_nid_postfix(nullptr, nullptr, 0); });
    for (std::uint32_t value = 0; value <= 0x10ffff; value += 17) {
        char escaped[16];
        std::snprintf(escaped, sizeof(escaped), "\\U%08x", value);
        std::array<char16_t, 3> output;
        output.fill(0xa55a);
        const auto size = u_unescape_nid_postfix(escaped, output.data(), output.size());
        if (value <= 0xffff) Require(size == 1 && output[0] == value && output[1] == 0);
        else Require(size == 2 && output[0] == 0xd800 + ((value - 0x10000) >> 10) && output[1] == 0xdc00 + ((value - 0x10000) & 0x3ff) && output[2] == 0);
    }
}

} // namespace

int main() {
    SingleEscapes();
    CallbackOrder();
    Strings();
    std::puts("ICU escape decoding tests passed");
}
