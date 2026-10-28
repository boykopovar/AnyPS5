#include "prx/libSceJsc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <algorithm>
#include <array>
#include <codecvt>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <locale>
#include <limits>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>



static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "JavaScriptCore string check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static void Unicode() {
    std::u32string scalars;
    for (char32_t codepoint = 1; codepoint <= 0x10FFFF; ++codepoint) {
        if (codepoint < 0xD800 || codepoint > 0xDFFF) scalars.push_back(codepoint);
    }
    std::codecvt_utf8<char32_t> encode;
    std::codecvt_utf8_utf16<char16_t> decode;
    std::mbstate_t state{};
    const char32_t* readScalars = nullptr;
    std::string utf8(scalars.size() * 4, '\0');
    char* writtenBytes = nullptr;
    Require(encode.out(state, scalars.data(), scalars.data() + scalars.size(), readScalars,
        utf8.data(), utf8.data() + utf8.size(), writtenBytes) == std::codecvt_base::ok);
    Require(readScalars == scalars.data() + scalars.size());
    utf8.resize(writtenBytes - utf8.data());
    state = {};
    const char* readBytes = nullptr;
    std::u16string utf16(utf8.size(), 0);
    char16_t* writtenUnits = nullptr;
    Require(decode.in(state, utf8.data(), utf8.data() + utf8.size(), readBytes,
        utf16.data(), utf16.data() + utf16.size(), writtenUnits) == std::codecvt_base::ok);
    Require(readBytes == utf8.data() + utf8.size());
    utf16.resize(writtenUnits - utf16.data());
    auto* string = JSStringCreateWithUTF8CString(utf8.c_str());
    Require(JSStringGetLength(string) == utf16.size());
    const auto* characters = JSStringGetCharactersPtr(string);
    Require(std::equal(utf16.begin(), utf16.end(), characters));
    const std::vector<std::uint16_t> units(utf16.begin(), utf16.end());
    auto* other = JSStringCreateWithCharacters(units.data(), units.size());
    Require(JSStringIsEqual(string, other));
    Require(JSStringGetMaximumUTF8CStringSize(string) == utf16.size() * 3 + 1);
    std::vector<char> buffer(utf8.size() + 2, '?');
    Require(JSStringGetUTF8CString(string, buffer.data(), buffer.size() - 1) == utf8.size() + 1);
    Require(std::memcmp(buffer.data(), utf8.c_str(), utf8.size() + 1) == 0);
    Require(buffer.back() == '?');
    JSStringRelease(other);
    JSStringRelease(string);
}

static void Boundaries() {
    const char text[] = "A\xC2\xA2\xE2\x82\xAC\xF0\x9F\x98\x80Z";
    auto* string = JSStringCreateWithUTF8CString(text);
    Require(JSStringGetLength(string) == 6);
    const std::array<std::size_t, 6> complete{0, 1, 3, 6, 10, 11};
    for (std::size_t capacity = 1; capacity <= sizeof(text) + 1; ++capacity) {
        std::array<char, 32> buffer;
        buffer.fill('?');
        std::size_t expected = 0;
        for (const auto length : complete) {
            if (length < capacity) expected = length;
        }
        Require(JSStringGetUTF8CString(string, buffer.data(), capacity) == expected + 1);
        Require(std::memcmp(buffer.data(), text, expected) == 0 && buffer[expected] == '\0');
        for (auto index = expected + 1; index < buffer.size(); ++index) Require(buffer[index] == '?');
    }
    char unchanged = '!';
    Require(JSStringGetUTF8CString(string, &unchanged, 0) == 0 && unchanged == '!');
    Require(JSStringGetUTF8CString(string, nullptr, 10) == 0);
    Require(JSStringGetUTF8CString(nullptr, &unchanged, 1) == 0 && unchanged == '!');
    Require(JSStringGetLength(nullptr) == 0 && JSStringGetCharactersPtr(nullptr) == nullptr);
    Require(JSStringIsEqual(nullptr, nullptr) && !JSStringIsEqual(nullptr, string));
    Require(JSStringIsEqualToUTF8CString(string, text));
    Require(!JSStringIsEqualToUTF8CString(string, "a"));
    JSStringRelease(string);
}

static void Invalid() {
    const auto* unavailable = reinterpret_cast<const std::uint16_t*>(std::uintptr_t{1});
    for (const auto count : {static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()) + 1,
        static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()), std::numeric_limits<std::size_t>::max()}) {
        bool threw = false;
        try {
            JSStringCreateWithCharacters(unavailable, count);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw);
    }
    auto* empty = JSStringCreateWithUTF8CString("");
    Require(JSStringGetLength(empty) == 0);
    auto* nullString = JSStringCreateWithCharacters(nullptr, 0);
    Require(JSStringGetLength(nullString) == 0 && JSStringGetCharactersPtr(nullString) == nullptr);
    Require(!JSStringIsEqual(empty, nullString));
    Require(JSStringIsEqualToUTF8CString(nullString, "\xFF"));
    JSStringRelease(nullString);
    for (const auto* invalid : {"\x80", "\xC0\x80", "\xC1\xBF", "\xE0\x80\x80", "\xED\xA0\x80", "\xF0\x80\x80\x80", "\xF4\x90\x80\x80", "\xF5\x80\x80\x80", "\xFF", "x\xC2", "x\xE2\x82", "x\xF0\x9F\x98", "\xE2!"}) {
        auto* string = JSStringCreateWithUTF8CString(invalid);
        Require(JSStringGetLength(string) == 0 && JSStringGetCharactersPtr(string) == nullptr);
        Require(!JSStringIsEqual(string, empty));
        Require(JSStringIsEqualToUTF8CString(string, "\xFF"));
        char buffer = '!';
        Require(JSStringGetUTF8CString(string, &buffer, 1) == 1 && buffer == '\0');
        JSStringRelease(string);
    }
    for (const auto& units : std::vector<std::vector<std::uint16_t>>{{0xD800}, {0xDC00}, {'x', 0xD800}, {'x', 0xDC00, 'y'}, {'x', 0xD800, 'y'}}) {
        auto* string = JSStringCreateWithCharacters(units.data(), units.size());
        Require(JSStringGetLength(string) == units.size());
        Require(std::equal(units.begin(), units.end(), JSStringGetCharactersPtr(string)));
        std::array<char, 16> buffer;
        buffer.fill('?');
        const std::size_t prefix = units[0] == 'x' ? 1 : 0;
        Require(JSStringGetUTF8CString(string, buffer.data(), buffer.size()) == prefix + 1);
        Require(buffer[prefix] == '\0' && buffer[prefix + 1] == '?');
        JSStringRelease(string);
    }
    JSStringRelease(empty);
}

static void Ownership() {
    std::array<std::uint16_t, 4> units{'A', 0, 0x03A9, 'B'};
    auto* string = JSStringCreateWithCharacters(units.data(), units.size());
    units.fill('?');
    const auto* pointer = JSStringGetCharactersPtr(string);
    Require(pointer[0] == 'A' && pointer[1] == 0 && pointer[2] == 0x03A9 && pointer[3] == 'B');
    std::array<char, 10> buffer{};
    Require(JSStringGetUTF8CString(string, buffer.data(), buffer.size()) == 6);
    const std::array<char, 6> expected{'A', 0, static_cast<char>(0xCE), static_cast<char>(0xA9), 'B', 0};
    Require(std::equal(expected.begin(), expected.end(), buffer.begin()));
    auto* retained = JSStringRetain(string);
    Require(retained == string);
    JSStringRelease(string);
    Require(JSStringGetCharactersPtr(retained) == pointer && JSStringGetLength(retained) == 4);
    std::vector<std::thread> threads;
    for (unsigned index = 0; index < 4; ++index) {
        threads.emplace_back([retained] {
            for (unsigned count = 0; count < 5000; ++count) {
                Require(JSStringRetain(retained) == retained);
                Require(JSStringGetLength(retained) == 4);
                JSStringRelease(retained);
            }
        });
    }
    for (auto& thread : threads) thread.join();
    JSStringRelease(retained);
}

int main() {
    Unicode();
    Boundaries();
    Invalid();
    Ownership();
}
