#include "prx/libc/include/CrashText.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

int failures = 0;

std::vector<std::uint8_t> Wide(const std::u16string& text) {
    std::vector<std::uint8_t> bytes;
    for (const char16_t unit : text) {
        bytes.push_back(static_cast<std::uint8_t>(unit & 0xff));
        bytes.push_back(static_cast<std::uint8_t>(unit >> 8));
    }
    return bytes;
}

std::vector<std::uint8_t> Narrow(const std::string& text) {
    return std::vector<std::uint8_t>(text.begin(), text.end());
}

void Expect(const char* name, const std::vector<std::uint8_t>& bytes, const std::string& expected, std::size_t capacity = 960) {
    std::vector<char> out(capacity, '#');
    const bool formatted = crash_text::Format(bytes.data(), bytes.size(), out.data(), capacity);
    const std::string actual = formatted ? std::string(out.data()) : std::string("<none>");
    if (actual == expected && actual.size() < capacity) return;
    std::fprintf(stderr, "%s: expected %s, got %s\n", name, expected.c_str(), actual.c_str());
    ++failures;
}

}

int main() {
    using namespace std::string_literals;
    Expect("terminated UTF-16", Wide(u"LowLevelFatalError [File:Unknown]\0tail"s), "u\"LowLevelFatalError [File:Unknown]\"");
    Expect("terminated ASCII with escapes", Narrow("PS5 Out of Memory:\n\t\"a\\b\"\r\0tail"s), "\"PS5 Out of Memory:\\n\\t\\\"a\\\\b\\\"\\r\"");
    Expect("UTF-16 up to the end of readable memory", Wide(u"Ran out of memory"), "u\"Ran out of memory\"...");
    Expect("ASCII up to the end of readable memory", Narrow("PAGE END"), "\"PAGE END\"...");
    Expect("UTF-16 before a non-ASCII unit", Wide(u"Memory\u00e9 more"), "u\"Memory\"...");
    Expect("short ASCII", Narrow("abc\0"s), "<none>");
    Expect("short UTF-16", Wide(u"abc\0"s), "<none>");
    Expect("three readable bytes", Narrow("abc"), "<none>");
    Expect("pointer value", std::vector<std::uint8_t>{0x08, 0xd4, 0x8e, 0x5b, 0xf6, 0x7f, 0x00, 0x00}, "<none>");
    Expect("control byte first", Narrow("\x01text\0"s), "<none>");
    Expect("character limit", Wide(std::u16string(600, u'A') + u'\0'), "u\"" + std::string(crash_text::MaximumCharacters, 'A') + "\"...");
    Expect("NUL right at the character limit", Wide(std::u16string(crash_text::MaximumCharacters, u'B') + u'\0'), "u\"" + std::string(crash_text::MaximumCharacters, 'B') + "\"");
    Expect("output capacity", Narrow(std::string(100, 'x')), "\"" + std::string(10, 'x') + "\"...", 16);
    Expect("escape at the output capacity", Narrow("abcdefghi\n" + std::string(20, 'z')), "\"abcdefghi\"...", 16);
    Expect("too small output", Narrow("long enough text"), "<none>", 7);
    if (failures != 0) return 1;
    std::puts("crash text: all cases passed");
    return 0;
}
