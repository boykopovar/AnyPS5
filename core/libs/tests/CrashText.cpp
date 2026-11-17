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

void ExpectInstruction(const char* name, const std::vector<std::uint8_t>& bytes, const char* expected) {
    const char* found = crash_text::AmdOnlyInstruction(bytes.data(), bytes.size());
    const std::string actual = found != nullptr ? found : "<none>";
    if (actual == (expected != nullptr ? expected : "<none>")) return;
    std::fprintf(stderr, "%s: expected %s, got %s\n", name, expected != nullptr ? expected : "<none>", actual.c_str());
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
    ExpectInstruction("MONITORX", {0x0f, 0x01, 0xfa}, "MONITORX");
    ExpectInstruction("MWAITX", {0x0f, 0x01, 0xfb}, "MWAITX");
    ExpectInstruction("CLZERO", {0x0f, 0x01, 0xfc}, "CLZERO");
    ExpectInstruction("CLZERO with an address-size prefix", {0x67, 0x0f, 0x01, 0xfc}, "CLZERO");
    ExpectInstruction("RDPRU", {0x0f, 0x01, 0xfd}, "RDPRU");
    ExpectInstruction("MONITORX behind a lock prefix", {0xf0, 0x0f, 0x01, 0xfa}, "MONITORX");
    ExpectInstruction("MCOMMIT", {0xf3, 0x0f, 0x01, 0xfa}, "MCOMMIT");
    ExpectInstruction("EXTRQ with immediates", {0x66, 0x0f, 0x78, 0xc0, 0x08, 0x10}, "EXTRQ");
    ExpectInstruction("EXTRQ register form with REX", {0x66, 0x41, 0x0f, 0x79, 0xc1}, "EXTRQ");
    ExpectInstruction("INSERTQ with immediates", {0xf2, 0x0f, 0x78, 0xc1, 0x08, 0x10}, "INSERTQ");
    ExpectInstruction("INSERTQ register form", {0xf2, 0x0f, 0x79, 0xc1}, "INSERTQ");
    ExpectInstruction("MOVNTSS", {0xf3, 0x0f, 0x2b, 0x07}, "MOVNTSS");
    ExpectInstruction("MOVNTSD with REX", {0xf2, 0x44, 0x0f, 0x2b, 0x07}, "MOVNTSD");
    ExpectInstruction("MOVNTPS", {0x0f, 0x2b, 0x07}, nullptr);
    ExpectInstruction("MOVNTPD", {0x66, 0x0f, 0x2b, 0x07}, nullptr);
    ExpectInstruction("VMREAD", {0x0f, 0x78, 0xc1}, nullptr);
    ExpectInstruction("RDTSCP", {0x0f, 0x01, 0xf9}, nullptr);
    ExpectInstruction("MONITOR", {0x0f, 0x01, 0xc8}, nullptr);
    ExpectInstruction("UD2", {0x0f, 0x0b}, nullptr);
    ExpectInstruction("two mandatory prefixes", {0x66, 0xf3, 0x0f, 0x01, 0xfa}, nullptr);
    ExpectInstruction("MONITORX cut after two bytes", {0x0f, 0x01}, nullptr);
    ExpectInstruction("a prefix alone", {0xf3}, nullptr);
    ExpectInstruction("no readable bytes", {}, nullptr);
    if (failures != 0) return 1;
    std::puts("crash text: all cases passed");
    return 0;
}
