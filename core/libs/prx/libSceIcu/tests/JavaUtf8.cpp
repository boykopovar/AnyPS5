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
#include <vector>

namespace {

constexpr std::int32_t Illegal = 1;
constexpr std::int32_t Invalid = 10;
constexpr std::int32_t Overflow = 15;
constexpr std::int32_t NotTerminated = -124;
constexpr char16_t Fill = 0xa55a;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU Java modified UTF-8 check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void RequireThrows(TAction action) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false);
}

std::string Bytes(std::initializer_list<int> values) {
    std::string result;
    for (auto value : values) result.push_back(static_cast<char>(value));
    return result;
}

struct FromResult {
    std::u16string text;
    std::int32_t length = -77;
    std::int32_t substitutions = -77;
    std::int32_t status = 0;
    bool null = false;
    char16_t after = 0;
};

FromResult From(const std::string& source, std::int32_t sourceLength, std::int32_t capacity, std::int32_t substitution, std::int32_t status = 0) {
    std::array<char16_t, 16> buffer;
    buffer.fill(Fill);
    FromResult result;
    result.status = status;
    char16_t* returned = u_strFromJavaModifiedUTF8WithSub_nid_postfix(buffer.data(), capacity, &result.length, source.c_str(), sourceLength, substitution, &result.substitutions, &result.status);
    result.null = returned == nullptr;
    if (returned) Require(returned == buffer.data());
    const auto size = capacity < 0 ? 0 : std::min<std::size_t>(capacity, buffer.size());
    result.text.assign(buffer.data(), size);
    result.after = size < buffer.size() ? buffer[size] : Fill;
    return result;
}

FromResult Whole(const std::string& source, std::int32_t substitution, std::int32_t capacity = 12) {
    return From(source, static_cast<std::int32_t>(source.size()), capacity, substitution);
}

void Check(const FromResult& result, std::u16string expected, std::int32_t substitutions) {
    Require(!result.null && result.status == 0 && result.length == static_cast<std::int32_t>(expected.size()) && result.substitutions == substitutions);
    Require(result.text.substr(0, expected.size()) == expected && result.text[expected.size()] == 0);
}

void Valid() {
    auto result = From("abc", -1, 8, -1);
    Check(result, u"abc", 0);
    Require(result.text[4] == Fill);

    Check(Whole(Bytes({0xc0, 0x80}), -1), std::u16string({0}), 0);
    Check(Whole(Bytes({0x41, 0, 0x42}), -1), std::u16string({0x41, 0, 0x42}), 0);
    Check(Whole(Bytes({0xe2, 0x82, 0xac, 0xc3, 0xa9, 0x7f, 0xdf, 0xbf, 0xef, 0xbf, 0xbf}), -1),
          std::u16string({0x20ac, 0xe9, 0x7f, 0x7ff, 0xffff}), 0);
    Check(Whole(Bytes({0xed, 0xa0, 0xbd, 0xed, 0xb8, 0x80}), -1), std::u16string({0xd83d, 0xde00}), 0);
    Check(Whole(Bytes({0xed, 0xb8, 0x80, 0xed, 0xa0, 0xbd}), -1), std::u16string({0xde00, 0xd83d}), 0);
    Check(Whole(Bytes({0xe0, 0x80, 0x80, 0xc1, 0xbf, 0xc0, 0x80}), -1), std::u16string({0, 0x7f, 0}), 0);
    Check(From("", -1, 4, -1), u"", 0);
    Check(From("", 0, 4, -1), u"", 0);
}

void Capacity() {
    const auto euro = Bytes({'a', 'b', 0xe2, 0x82, 0xac});
    auto result = Whole(euro, -1, 3);
    Require(result.status == NotTerminated && result.length == 3 && result.after == Fill && result.text == std::u16string({'a', 'b', 0x20ac}));
    result = Whole(euro, -1, 2);
    Require(result.status == Overflow && result.length == 3 && result.text == u"ab");
    result = Whole(euro, -1, 0);
    Require(result.status == Overflow && result.length == 3 && result.after == Fill);
    result = From(euro, -1, 1, -1);
    Require(result.status == Overflow && result.length == 3 && result.text == u"a");
    result = From("ab", -1, 2, -1);
    Require(result.status == NotTerminated && result.length == 2 && result.after == Fill);

    std::int32_t length = -77;
    std::int32_t status = 0;
    Require(u_strFromJavaModifiedUTF8WithSub_nid_postfix(nullptr, 0, &length, euro.c_str(), 5, -1, nullptr, &status) == nullptr);
    Require(status == Overflow && length == 3);
    status = 0;
    Require(u_strFromJavaModifiedUTF8WithSub_nid_postfix(nullptr, 0, nullptr, nullptr, 0, -1, nullptr, &status) == nullptr);
    Require(status == NotTerminated);
}

void Malformed() {
    Check(Whole(Bytes({0xf0, 0x9f, 0x98, 0x80}), 0xfffd), std::u16string({0xfffd}), 1);
    Check(Whole(Bytes({0xf0, 0x9f, 0x98, 0x41}), 0xfffd), std::u16string({0xfffd, 0x41}), 1);
    Check(Whole(Bytes({0xf0, 0x9f, 0x41}), 0xfffd), std::u16string({0xfffd, 0x41}), 1);
    Check(Whole(Bytes({0xf0, 0x80, 0x80, 0x80, 0x41}), 0xfffd), std::u16string({0xfffd, 0xfffd, 0xfffd, 0xfffd, 0x41}), 4);
    Check(Whole(Bytes({0xf4, 0x90, 0x80, 0x80}), 0xfffd), std::u16string({0xfffd, 0xfffd, 0xfffd, 0xfffd}), 4);
    Check(Whole(Bytes({0xf4, 0x8f, 0xbf, 0xbf}), 0xfffd), std::u16string({0xfffd}), 1);
    Check(Whole(Bytes({0xf5, 0x80, 0x80, 0x80}), 0xfffd), std::u16string({0xfffd, 0xfffd, 0xfffd, 0xfffd}), 4);
    Check(Whole(Bytes({0xff, 0x41}), 0xfffd), std::u16string({0xfffd, 0x41}), 1);
    Check(Whole(Bytes({0x80, 0x41}), 0xfffd), std::u16string({0xfffd, 0x41}), 1);
    Check(Whole(Bytes({0xe2, 0x82, 0x41}), 0xfffd), std::u16string({0xfffd, 0x41}), 1);
    Check(Whole(Bytes({0xe2, 0x41}), 0xfffd), std::u16string({0xfffd, 0x41}), 1);
    Check(Whole(Bytes({0xe0, 0x80, 0x41}), 0xfffd), std::u16string({0xfffd, 0xfffd, 0x41}), 2);
    Check(Whole(Bytes({0xed, 0xa0}), 0xfffd), std::u16string({0xfffd, 0xfffd}), 2);
    Check(Whole(Bytes({0xe2, 0x82}), 0xfffd), std::u16string({0xfffd}), 1);
    Check(Whole(Bytes({0xe2}), 0xfffd), std::u16string({0xfffd}), 1);
    Check(Whole(Bytes({0xc3}), 0xfffd), std::u16string({0xfffd}), 1);
    Check(Whole(Bytes({0xc3, 0x41}), 0xfffd), std::u16string({0xfffd, 0x41}), 1);
    Check(Whole(Bytes({0x41, 0xc3}), 0x3f), std::u16string({0x41, 0x3f}), 1);
    Check(From(Bytes({0xe2, 0x82, 0xac, 0xe2, 0x82}), 4, 8, 0x3f), std::u16string({0x20ac, 0x3f}), 1);
    Check(Whole(Bytes({0x80}), 0x1f600), std::u16string({0xd83d, 0xde00}), 1);
    Check(Whole(Bytes({0xf0, 0x9f, 0x98, 0x80, 0x41}), 0), std::u16string({0, 0x41}), 1);

    auto result = Whole(Bytes({0x41, 0x80, 0x42}), -1);
    Require(result.null && result.status == Invalid && result.length == -77 && result.substitutions == 0);
    Require(result.text[0] == 0x41 && result.text[1] == Fill);
    result = Whole(Bytes({0xf0, 0x9f, 0x98, 0x80}), -1);
    Require(result.null && result.status == Invalid);
    result = Whole(Bytes({0x41, 0x41, 0x80}), -1, 1);
    Require(result.null && result.status == Invalid);
    result = From(Bytes({0xc3}), -1, 8, -1);
    Require(result.null && result.status == Invalid);
}

void Preflight() {
    auto result = Whole(Bytes({0x41, 0x80}), 0x1f600, 2);
    Require(result.status == Overflow && result.length == 3 && result.substitutions == 1 && result.text == std::u16string({0x41, 0xd83d}));

    result = Whole(Bytes({0x41, 0x80, 0x80}), 0x1f600, 1);
    Require(result.status == Overflow && result.length == 3 && result.substitutions == 2 && result.text == std::u16string({0x41}));

    result = Whole(Bytes({0x41, 0x80, 0x80, 0x80}), 0x1f600, 2);
    Require(result.status == Overflow && result.length == 5 && result.substitutions == 3 && result.text == std::u16string({0x41, 0xd83d}));

    result = Whole(Bytes({0x41, 0x80, 0x80}), 0x3f, 1);
    Require(result.status == Overflow && result.length == 3 && result.substitutions == 2);

    result = Whole(Bytes({0xe2, 0x82, 0xac, 0x61, 0xf0, 0x9f, 0x98, 0x80, 0xc0, 0x80}), 0x3f, 0);
    Require(result.status == Overflow && result.length == 4 && result.substitutions == 1);

    result = Whole(Bytes({0xe2, 0x82, 0xac, 0x61}), 0x3f, 0);
    Require(result.status == Overflow && result.length == 2 && result.substitutions == 0);

    result = Whole(Bytes({0xe2, 0x82}), 0x3f, 0);
    Require(result.status == Overflow && result.length == 1 && result.substitutions == 1);
}

void Arguments() {
    const std::string text = "a";
    for (auto substitution : {0x110000, 0xd800, 0xdfff, 0x7fffffff}) {
        const auto result = From(text, 1, 8, substitution);
        Require(result.null && result.status == Illegal && result.length == -77 && result.substitutions == -77 && result.text[0] == Fill);
    }
    for (auto length : {-2, std::numeric_limits<std::int32_t>::min()}) {
        const auto result = From(text, length, 8, -1);
        Require(result.null && result.status == Illegal && result.length == -77);
    }
    Require(From(text, 1, -1, -1).status == Illegal);
    std::int32_t status = 0;
    std::int32_t length = -77;
    std::array<char16_t, 4> buffer{};
    Require(!u_strFromJavaModifiedUTF8WithSub_nid_postfix(buffer.data(), 4, &length, nullptr, 1, -1, nullptr, &status) && status == Illegal);
    status = 0;
    Require(!u_strFromJavaModifiedUTF8WithSub_nid_postfix(buffer.data(), 4, &length, nullptr, -1, -1, nullptr, &status) && status == Illegal);
    status = 0;
    Require(!u_strFromJavaModifiedUTF8WithSub_nid_postfix(nullptr, 1, &length, text.c_str(), 1, -1, nullptr, &status) && status == Illegal);
    Require(length == -77);
}

void Status() {
    auto result = From("a", 1, 4, -1, Illegal);
    Require(result.null && result.status == Illegal && result.text[0] == Fill && result.length == -77);
    result = From("a", 1, 4, -1, NotTerminated);
    Require(!result.null && result.status == 0 && result.text.substr(0, 2) == std::u16string({'a', 0}));
    result = From("a", 1, 1, -1, NotTerminated);
    Require(result.status == NotTerminated && result.length == 1);
    result = From("ab", 2, 1, -1, NotTerminated);
    Require(result.status == Overflow);

    std::array<char16_t, 4> buffer{};
    std::int32_t length = 0;
    RequireThrows([&] { u_strFromJavaModifiedUTF8WithSub_nid_postfix(buffer.data(), 4, &length, "a", 1, -1, nullptr, nullptr); });
}

constexpr char FillByte = 0x5a;

struct ToResult {
    std::string bytes;
    std::int32_t length = -77;
    std::int32_t status = 0;
    bool null = false;
    char after = 0;
};

ToResult To(const std::u16string& source, std::int32_t sourceLength, std::int32_t capacity, std::int32_t status = 0) {
    std::array<char, 32> buffer;
    buffer.fill(FillByte);
    ToResult result;
    result.status = status;
    char* returned = u_strToJavaModifiedUTF8_nid_postfix(buffer.data(), capacity, &result.length, source.data(), sourceLength, &result.status);
    result.null = returned == nullptr;
    if (returned) Require(returned == buffer.data());
    const auto size = capacity < 0 ? 0 : std::min<std::size_t>(capacity, buffer.size());
    result.bytes.assign(buffer.data(), size);
    result.after = size < buffer.size() ? buffer[size] : FillByte;
    return result;
}

void CheckTo(const ToResult& result, const std::string& expected) {
    Require(!result.null && result.status == 0 && result.length == static_cast<std::int32_t>(expected.size()));
    Require(result.bytes.substr(0, expected.size()) == expected && result.bytes[expected.size()] == 0);
}

void ToValid() {
    CheckTo(To(u"abc", -1, 8), "abc");
    CheckTo(To(std::u16string({0}), 1, 8), Bytes({0xc0, 0x80}));
    CheckTo(To(std::u16string({0x41, 0, 0x42}), 3, 8), Bytes({0x41, 0xc0, 0x80, 0x42}));
    CheckTo(To(std::u16string({0x20ac, 0xe9, 0x7f, 0x80, 0x7ff, 0x800, 0xffff}), 7, 20),
            Bytes({0xe2, 0x82, 0xac, 0xc3, 0xa9, 0x7f, 0xc2, 0x80, 0xdf, 0xbf, 0xe0, 0xa0, 0x80, 0xef, 0xbf, 0xbf}));
    CheckTo(To(std::u16string({0xd83d, 0xde00}), -1, 8), Bytes({0xed, 0xa0, 0xbd, 0xed, 0xb8, 0x80}));
    CheckTo(To(std::u16string({0xde00, 0xd83d}), 2, 8), Bytes({0xed, 0xb8, 0x80, 0xed, 0xa0, 0xbd}));
    CheckTo(To(u"", -1, 4), "");
    CheckTo(To(u"", 0, 4), "");
}

void ToCapacity() {
    const std::u16string text{0x61, 0x20ac, 0x62};
    auto result = To(text, 3, 5);
    Require(result.status == NotTerminated && result.length == 5 && result.after == FillByte);
    Require(result.bytes == Bytes({0x61, 0xe2, 0x82, 0xac, 0x62}));
    result = To(text, 3, 4);
    Require(result.status == Overflow && result.length == 5 && result.bytes == Bytes({0x61, 0xe2, 0x82, 0xac}));
    result = To(text, 3, 3);
    Require(result.status == Overflow && result.length == 5 && result.bytes == std::string("a") + FillByte + FillByte);
    result = To(text, 3, 2);
    Require(result.status == Overflow && result.length == 5 && result.bytes == std::string("a") + FillByte);
    result = To(text, 3, 0);
    Require(result.status == Overflow && result.length == 5 && result.after == FillByte);
    result = To(std::u16string({0x61, 0x62, 0}), -1, 2);
    Require(result.status == NotTerminated && result.length == 2 && result.bytes == "ab");
    result = To(std::u16string({0x61, 0, 0x62}), 3, 2);
    Require(result.status == Overflow && result.length == 4 && result.bytes == std::string("a") + FillByte);
    result = To(std::u16string({0xd83d, 0xde00}), 2, 5);
    Require(result.status == Overflow && result.length == 6 && result.bytes == Bytes({0xed, 0xa0, 0xbd, FillByte, FillByte}));

    std::int32_t length = -77;
    std::int32_t status = 0;
    Require(u_strToJavaModifiedUTF8_nid_postfix(nullptr, 0, &length, text.data(), 3, &status) == nullptr);
    Require(status == Overflow && length == 5);
    status = 0;
    Require(u_strToJavaModifiedUTF8_nid_postfix(nullptr, 0, nullptr, nullptr, 0, &status) == nullptr);
    Require(status == NotTerminated);
}

void ToArguments() {
    const std::u16string text{0x41};
    for (auto length : {-2, std::numeric_limits<std::int32_t>::min()}) {
        const auto result = To(text, length, 8);
        Require(result.null && result.status == Illegal && result.length == -77 && result.bytes[0] == FillByte);
    }
    Require(To(text, 1, -1).status == Illegal);
    std::int32_t status = 0;
    std::int32_t length = -77;
    std::array<char, 4> buffer{};
    Require(!u_strToJavaModifiedUTF8_nid_postfix(buffer.data(), 4, &length, nullptr, 1, &status) && status == Illegal);
    status = 0;
    Require(!u_strToJavaModifiedUTF8_nid_postfix(buffer.data(), 4, &length, nullptr, -1, &status) && status == Illegal);
    status = 0;
    Require(!u_strToJavaModifiedUTF8_nid_postfix(nullptr, 1, &length, text.data(), 1, &status) && status == Illegal);
    Require(length == -77);

    auto result = To(text, 1, 4, Illegal);
    Require(result.null && result.status == Illegal && result.bytes[0] == FillByte && result.length == -77);
    result = To(text, 1, 4, NotTerminated);
    Require(!result.null && result.status == 0 && result.bytes.substr(0, 2) == std::string("A\0", 2));
    result = To(text, 1, 1, NotTerminated);
    Require(result.status == NotTerminated && result.length == 1);
    result = To(std::u16string({0x41, 0x42}), 2, 1, NotTerminated);
    Require(result.status == Overflow);
    RequireThrows([&] { u_strToJavaModifiedUTF8_nid_postfix(buffer.data(), 4, &length, text.data(), 1, nullptr); });
}

void RoundTrip() {
    for (std::uint32_t unit = 0; unit <= 0xffff; ++unit) {
        const char16_t text[2] = {static_cast<char16_t>(unit), 0};
        std::array<char, 8> encoded;
        std::int32_t encodedLength = 0;
        std::int32_t status = 0;
        Require(u_strToJavaModifiedUTF8_nid_postfix(encoded.data(), 8, &encodedLength, text, 1, &status) == encoded.data());
        Require(status == 0 && encodedLength == (unit != 0 && unit <= 0x7f ? 1 : unit <= 0x7ff ? 2 : 3));
        std::array<char16_t, 4> decoded;
        std::int32_t decodedLength = 0;
        std::int32_t substitutions = -1;
        Require(u_strFromJavaModifiedUTF8WithSub_nid_postfix(decoded.data(), 4, &decodedLength, encoded.data(), encodedLength, -1, &substitutions, &status) == decoded.data());
        Require(status == 0 && decodedLength == 1 && substitutions == 0 && decoded[0] == unit && decoded[1] == 0);
    }
}

}

int main() {
    Valid();
    Capacity();
    Malformed();
    Preflight();
    Arguments();
    Status();
    ToValid();
    ToCapacity();
    ToArguments();
    RoundTrip();
    std::puts("ICU Java modified UTF-8 tests passed");
}
