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
    std::fprintf(stderr, "ICU UTF-32 check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void RequireThrows(TAction action) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false);
}

struct FromResult {
    std::u16string text;
    std::int32_t length = -77;
    std::int32_t substitutions = -77;
    std::int32_t status = 0;
    bool null = false;
    char16_t after = 0;
};

FromResult From(const std::vector<std::int32_t>& source, std::int32_t sourceLength, std::int32_t capacity,
                std::int32_t substitution, std::int32_t status = 0, bool withSub = true) {
    std::array<char16_t, 16> buffer;
    buffer.fill(Fill);
    FromResult result;
    result.status = status;
    char16_t* returned = withSub
        ? u_strFromUTF32WithSub_nid_postfix(buffer.data(), capacity, &result.length, source.data(), sourceLength, substitution, &result.substitutions, &result.status)
        : u_strFromUTF32_nid_postfix(buffer.data(), capacity, &result.length, source.data(), sourceLength, &result.status);
    result.null = returned == nullptr;
    if (returned) Require(returned == buffer.data());
    const auto size = capacity < 0 ? 0 : std::min<std::size_t>(capacity, buffer.size());
    result.text.assign(buffer.data(), size);
    result.after = size < buffer.size() ? buffer[size] : Fill;
    return result;
}

void Valid() {
    const std::vector<std::int32_t> mixed{0x41, 0x1f600, 0xe000, 0xffff};
    auto result = From(mixed, 4, 8, -1);
    Require(!result.null && result.status == 0 && result.length == 5 && result.substitutions == 0);
    Require(result.text.substr(0, 6) == std::u16string({0x41, 0xd83d, 0xde00, 0xe000, 0xffff, 0}));
    Require(result.text[6] == Fill);

    result = From({0x41, 0x1f600, 0, 0x42}, -1, 8, -1);
    Require(result.status == 0 && result.length == 3 && result.text.substr(0, 4) == std::u16string({0x41, 0xd83d, 0xde00, 0}));

    result = From({0x10000, 0x10ffff, 0x7f, 0x80}, 4, 8, -1);
    Require(result.length == 6 && result.text.substr(0, 7) == std::u16string({0xd800, 0xdc00, 0xdbff, 0xdfff, 0x7f, 0x80, 0}));

    result = From({}, 0, 4, -1);
    Require(!result.null && result.status == 0 && result.length == 0 && result.text[0] == 0 && result.text[1] == Fill);
    result = From({0}, -1, 1, -1);
    Require(result.status == 0 && result.length == 0 && result.text[0] == 0);

    result = From(mixed, 4, 8, -1, 0, false);
    Require(result.status == 0 && result.length == 5 && result.substitutions == -77);
}

void Capacity() {
    const std::vector<std::int32_t> mixed{0x41, 0x1f600, 0xe000, 0xffff};
    auto result = From(mixed, 4, 5, -1);
    Require(result.status == NotTerminated && result.length == 5 && result.after == Fill);
    Require(result.text == std::u16string({0x41, 0xd83d, 0xde00, 0xe000, 0xffff}));

    result = From(mixed, 4, 4, -1);
    Require(result.status == Overflow && result.length == 5);
    Require(result.text == std::u16string({0x41, 0xd83d, 0xde00, 0xe000}));

    result = From(mixed, 4, 2, -1);
    Require(result.status == Overflow && result.length == 5 && result.text == std::u16string({0x41, 0xe000}));

    result = From(mixed, 4, 0, -1);
    Require(result.status == Overflow && result.length == 5 && result.after == Fill);

    result = From({0x1f600}, 1, 1, -1);
    Require(result.status == Overflow && result.length == 2 && result.text == std::u16string({Fill}));

    result = From({0x41, 0}, -1, 1, -1);
    Require(result.status == NotTerminated && result.length == 1 && result.text == std::u16string({0x41}));

    std::int32_t length = -77;
    std::int32_t status = 0;
    const std::vector<std::int32_t> text{0x61, 0x1f600};
    Require(u_strFromUTF32_nid_postfix(nullptr, 0, &length, text.data(), 2, &status) == nullptr);
    Require(status == Overflow && length == 3);
    status = 0;
    Require(u_strFromUTF32_nid_postfix(nullptr, 0, nullptr, nullptr, 0, &status) == nullptr);
    Require(status == NotTerminated);
}

void Substitution() {
    auto result = From({0xd800, 0x41, 0x110000, -5, 0xdfff}, 5, 8, 0xfffd);
    Require(result.status == 0 && result.length == 5 && result.substitutions == 4);
    Require(result.text.substr(0, 6) == std::u16string({0xfffd, 0x41, 0xfffd, 0xfffd, 0xfffd, 0}));

    result = From({0xdfff, 0x41}, 2, 8, 0x1f600);
    Require(result.status == 0 && result.length == 3 && result.substitutions == 1);
    Require(result.text.substr(0, 4) == std::u16string({0xd83d, 0xde00, 0x41, 0}));

    result = From({0xd800}, 1, 1, 0x1f600);
    Require(result.status == Overflow && result.length == 2 && result.substitutions == 1 && result.text == std::u16string({Fill}));

    result = From({0xd800, 0x0}, -1, 4, 0x3f);
    Require(result.status == 0 && result.length == 1 && result.substitutions == 1 && result.text.substr(0, 2) == std::u16string({0x3f, 0}));

    result = From({0x41, 0xd800, 0x42}, 3, 8, -1);
    Require(result.null && result.status == Invalid && result.length == -77 && result.substitutions == 0);
    Require(result.text[0] == 0x41 && result.text[1] == Fill);

    result = From({0x41, 0xd800}, 2, 8, -1, 0, false);
    Require(result.null && result.status == Invalid);
    result = From({0x110000}, 1, 8, -1, 0, false);
    Require(result.null && result.status == Invalid);
    result = From({-1}, 1, 8, -1, 0, false);
    Require(result.null && result.status == Invalid);
}

void Arguments() {
    const std::vector<std::int32_t> text{0x41};
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
    Require(!u_strFromUTF32_nid_postfix(buffer.data(), 4, &length, nullptr, 1, &status) && status == Illegal);
    status = 0;
    Require(!u_strFromUTF32_nid_postfix(buffer.data(), 4, &length, nullptr, -1, &status) && status == Illegal);
    status = 0;
    Require(!u_strFromUTF32_nid_postfix(nullptr, 1, &length, text.data(), 1, &status) && status == Illegal);
    Require(length == -77);
}

void Status() {
    auto result = From({0x41}, 1, 4, -1, Illegal);
    Require(result.null && result.status == Illegal && result.text[0] == Fill && result.length == -77);
    result = From({0x41}, 1, 4, -1, Invalid);
    Require(result.null && result.status == Invalid);

    result = From({0x41}, 1, 4, -1, NotTerminated);
    Require(!result.null && result.status == 0 && result.text.substr(0, 2) == std::u16string({0x41, 0}));
    result = From({0x41}, 1, 1, -1, NotTerminated);
    Require(result.status == NotTerminated && result.length == 1);
    result = From({0x41, 0x42}, 2, 1, -1, NotTerminated);
    Require(result.status == Overflow);
    result = From({0x41}, 1, 4, -1, -5);
    Require(result.status == -5 && result.text.substr(0, 2) == std::u16string({0x41, 0}));

    const std::vector<std::int32_t> text{0x41};
    std::array<char16_t, 4> buffer{};
    std::int32_t length = 0;
    RequireThrows([&] { u_strFromUTF32_nid_postfix(buffer.data(), 4, &length, text.data(), 1, nullptr); });
    RequireThrows([&] { u_strFromUTF32WithSub_nid_postfix(buffer.data(), 4, &length, text.data(), 1, -1, nullptr, nullptr); });
}

}

int main() {
    Valid();
    Capacity();
    Substitution();
    Arguments();
    Status();
    std::puts("ICU UTF-32 conversion tests passed");
}
