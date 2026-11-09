#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
#include <limits>
#include <memory>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU UTF-8 check failed at line %u\n", location.line());
    std::abort();
}

template<class TCall>
void Rejects(TCall call) {
    try {
        call();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false);
}

std::int32_t Error(int consumed, int strict) {
    if (strict == -3) return 0xfffd;
    if (strict < 0) return -1;
    constexpr std::array<std::int32_t, 4> legacy{0x15, 0x9f, 0xffff, 0x10ffff};
    return legacy[consumed];
}

void Sequence(std::initializer_list<std::uint8_t> bytes, int strict, std::int32_t nextValue, int nextIndex,
              std::int32_t previousValue, int previousIndex, int backIndex) {
    for (int origin : {0, 3}) {
        std::array<std::uint8_t, 12> text;
        text.fill(0x80);
        std::copy(bytes.begin(), bytes.end(), text.begin() + origin);
        const auto original = text;
        const auto length = static_cast<std::int32_t>(bytes.size());
        auto index = origin + 1;
        Require(utf8_nextCharSafeBody_nid_postfix(text.data(), &index, origin + length, text[origin], static_cast<std::int8_t>(strict)) == nextValue);
        Require(index == origin + nextIndex);
        index = origin + length - 1;
        Require(utf8_prevCharSafeBody_nid_postfix(text.data(), origin, &index, text[index], static_cast<std::int8_t>(strict)) == previousValue);
        Require(index == origin + previousIndex);
        Require(utf8_back1SafeBody_nid_postfix(text.data(), origin, origin + length - 1) == origin + backIndex);
        Require(text == original);
    }
}

void ErrorModes() {
    for (int strict = -128; strict <= 127; ++strict) {
        const auto e0 = Error(0, strict);
        const auto e1 = Error(1, strict);
        const auto e2 = Error(2, strict);
        const auto e3 = Error(3, strict);
        Sequence({0x41}, strict, e0, 1, e0, 0, 0);
        Sequence({0xc2, 0xa2}, strict, 0xa2, 2, 0xa2, 0, 0);
        Sequence({0xe2, 0x82, 0xac}, strict, 0x20ac, 3, 0x20ac, 0, 0);
        Sequence({0xf0, 0x9f, 0x98, 0x80}, strict, 0x1f600, 4, 0x1f600, 0, 0);
        Sequence({0xc0, 0x80}, strict, e0, 1, e0, 1, 1);
        Sequence({0xc1, 0xbf}, strict, e0, 1, e0, 1, 1);
        Sequence({0xe0, 0x80, 0x80}, strict, e0, 1, e0, 2, 2);
        Sequence({0xf0, 0x80, 0x80, 0x80}, strict, e0, 1, e0, 3, 3);
        Sequence({0xf4, 0x90, 0x80, 0x80}, strict, e0, 1, e0, 3, 3);
        Sequence({0xf5, 0x80, 0x80, 0x80}, strict, e0, 1, e0, 3, 3);
        Sequence({0xff, 0xbf, 0xbf, 0xbf}, strict, e0, 1, e0, 3, 3);
        Sequence({0xe1, 0xa0}, strict, e1, 2, e1, 0, 0);
        Sequence({0xf1, 0x90, 0x80}, strict, e2, 3, e2, 0, 0);
        Sequence({0xf1, 0x90}, strict, e1, 2, e1, 0, 0);
        Sequence({0xe1, 0xa0, 0x41}, strict, e1, 2, e0, 2, 2);
        Sequence({0xf0, 0x9f, 0x98, 0x41}, strict, e2, 3, e0, 3, 3);
        Sequence({0xef, 0xb7, 0x90}, strict, strict > 0 ? e2 : 0xfdd0, 3, strict > 0 ? e2 : 0xfdd0, 0, 0);
        Sequence({0xf4, 0x8f, 0xbf, 0xbf}, strict, strict > 0 ? e3 : 0x10ffff, 4, strict > 0 ? e3 : 0x10ffff, 0, 0);
        Sequence({0xed, 0xa0, 0x80}, strict, strict == -2 ? 0xd800 : e0, strict == -2 ? 3 : 1,
                 strict == -2 ? 0xd800 : e0, strict == -2 ? 0 : 2, 2);
        Sequence({0xed, 0xa0}, strict, strict == -2 ? e1 : e0, strict == -2 ? 2 : 1, e0, 1, 1);
        Sequence({0x80, 0x80, 0x80, 0x80}, strict, e0, 1, e0, 3, 3);
        Sequence({0xe0, 0xa0, 0x80}, strict, 0x800, 3, 0x800, 0, 0);
        Sequence({0xe0, 0x9f, 0xbf}, strict, e0, 1, e0, 2, 2);
        Sequence({0xef, 0xbf, 0xbe}, strict, strict > 0 ? e2 : 0xfffe, 3, strict > 0 ? e2 : 0xfffe, 0, 0);
        Sequence({0xf0, 0x9f, 0xbf, 0xbe}, strict, strict > 0 ? e3 : 0x1fffe, 4, strict > 0 ? e3 : 0x1fffe, 0, 0);
    }
}

int Encode(std::uint32_t value, std::uint8_t* output) {
    if (value <= 0x7f) {
        output[0] = static_cast<std::uint8_t>(value);
        return 1;
    }
    if (value <= 0x7ff) {
        output[0] = static_cast<std::uint8_t>(0xc0 + value / 64);
        output[1] = static_cast<std::uint8_t>(0x80 + value % 64);
        return 2;
    }
    if (value <= 0xffff) {
        output[0] = static_cast<std::uint8_t>(0xe0 + value / 4096);
        output[1] = static_cast<std::uint8_t>(0x80 + value / 64 % 64);
        output[2] = static_cast<std::uint8_t>(0x80 + value % 64);
        return 3;
    }
    output[0] = static_cast<std::uint8_t>(0xf0 + value / 262144);
    output[1] = static_cast<std::uint8_t>(0x80 + value / 4096 % 64);
    output[2] = static_cast<std::uint8_t>(0x80 + value / 64 % 64);
    output[3] = static_cast<std::uint8_t>(0x80 + value % 64);
    return 4;
}

void ScalarRoundTrips() {
    for (std::int32_t value = 0x80; value <= 0x10ffff; ++value) {
        if (value >= 0xd800 && value <= 0xdfff) continue;
        std::array<std::uint8_t, 6> encoded{};
        const int length = Encode(value, encoded.data() + 1);
        auto index = std::int32_t{2};
        Require(utf8_nextCharSafeBody_nid_postfix(encoded.data(), &index, length + 1, encoded[1], -1) == value);
        Require(index == length + 1);
        index = length;
        Require(utf8_prevCharSafeBody_nid_postfix(encoded.data(), 1, &index, encoded[length], -1) == value);
        Require(index == 1);
        for (int last = 2; last <= length; ++last) {
            Require(utf8_back1SafeBody_nid_postfix(encoded.data(), 1, last) == 1);
            Require(utf8_back1SafeBody_nid_postfix(encoded.data(), 2, last) == last);
        }
        std::array<std::uint8_t, 6> actual;
        actual.fill(0x55);
        std::int8_t error = -37;
        Require(utf8_appendCharSafeBody_nid_postfix(actual.data(), 1, length + 1, value, &error) == length + 1);
        Require(error == -37);
        Require(std::equal(encoded.begin() + 1, encoded.begin() + 1 + length, actual.begin() + 1));
        Require(actual[0] == 0x55 && actual[length + 1] == 0x55);
    }
    for (std::int32_t value = 0; value <= 0x7f; ++value) {
        std::array<std::uint8_t, 4> text{0x55, 0x55, 0x55, 0x55};
        std::int8_t error = 29;
        Require(utf8_appendCharSafeBody_nid_postfix(text.data(), 1, 3, value, &error) == 3);
        Require(text[0] == 0x55 && text[1] == 0xc0 + value / 64 && text[2] == 0x80 + value % 64 && text[3] == 0x55);
        Require(error == 29);
    }
}

void AppendFailures() {
    constexpr std::array<std::int32_t, 12> values{-1, INT32_MIN, 0, 0x7f, 0x80, 0x7ff, 0x800, 0xd800, 0xdfff, 0x10000, 0x110000, INT32_MAX};
    constexpr std::array<std::array<std::uint8_t, 3>, 4> replacements{{{}, {0x15}, {0xc2, 0x9f}, {0xef, 0xbf, 0xbf}}};
    for (const auto value : values) {
        const bool valid = value >= 0 && value <= 0x10ffff && !(value >= 0xd800 && value <= 0xdfff);
        const int width = value <= 0x7ff ? 2 : value <= 0xffff ? 3 : 4;
        for (int space = -2; space <= 5; ++space) {
            if (valid && space >= width) continue;
            for (bool hasError : {false, true}) {
                std::array<std::uint8_t, 10> text;
                text.fill(0x55);
                std::int8_t error = 47;
                const int count = hasError ? 0 : std::clamp(space, 0, 3);
                Require(utf8_appendCharSafeBody_nid_postfix(text.data(), 2, 2 + space, value, hasError ? &error : nullptr) == 2 + count);
                Require(error == (hasError ? 1 : 47));
                for (int index = 0; index < static_cast<int>(text.size()); ++index) {
                    Require(text[index] == (index >= 2 && index < 2 + count ? replacements[count][index - 2] : 0x55));
                }
            }
        }
    }
}

void TerminatedAndBounded() {
    constexpr std::array<std::uint8_t, 4> sequence{0xf0, 0x90, 0x80, 0x80};
    for (int size = 1; size <= 4; ++size) {
        auto text = std::make_unique<std::uint8_t[]>(size + 1);
        std::copy_n(sequence.begin(), size, text.get());
        text[size] = 0;
        for (std::int32_t length : {-1, -7, INT32_MIN, size}) {
            auto index = std::int32_t{1};
            Require(utf8_nextCharSafeBody_nid_postfix(text.get(), &index, length, sequence[0], -3) == (size == 4 ? 0x10000 : 0xfffd));
            Require(index == size);
        }
        auto index = static_cast<std::int32_t>(size - 1);
        const auto last = index;
        Require(utf8_prevCharSafeBody_nid_postfix(text.get(), last, &index, text[last], -1) == -1);
        Require(index == last);
        Require(utf8_back1SafeBody_nid_postfix(text.get(), last, last) == last);
    }
    for (int byte = 0; byte <= 255; ++byte) {
        std::array<std::uint8_t, 2> text{static_cast<std::uint8_t>(byte), 0x80};
        auto index = std::int32_t{1};
        Require(utf8_nextCharSafeBody_nid_postfix(text.data(), &index, 1, byte, -1) == -1 && index == 1);
    }
}

void InvalidArguments() {
    std::array<std::uint8_t, 4> text{0xf0, 0x90, 0x80, 0x80};
    std::int32_t index = 1;
    Rejects([&] { utf8_nextCharSafeBody_nid_postfix(text.data(), nullptr, 4, 0xf0, -1); });
    Rejects([&] { utf8_prevCharSafeBody_nid_postfix(text.data(), 0, nullptr, 0x80, -1); });
    Rejects([&] { utf8_nextCharSafeBody_nid_postfix(nullptr, &index, 4, 0xf0, -1); });
    Rejects([&] { utf8_prevCharSafeBody_nid_postfix(nullptr, 0, &index, 0x80, -1); });
    Rejects([&] { utf8_back1SafeBody_nid_postfix(nullptr, 0, 0); });
    Rejects([&] { utf8_appendCharSafeBody_nid_postfix(nullptr, 0, 4, 0x10000, nullptr); });
    Rejects([&] { utf8_appendCharSafeBody_nid_postfix(text.data(), -1, 4, 0x80, nullptr); });
    index = -1;
    Rejects([&] { utf8_nextCharSafeBody_nid_postfix(text.data(), &index, 4, 0xf0, -1); });
    index = 3;
    Rejects([&] { utf8_nextCharSafeBody_nid_postfix(text.data(), &index, 2, 0xf0, -1); });
    Rejects([&] { utf8_prevCharSafeBody_nid_postfix(text.data(), 4, &index, 0x80, -1); });
    Rejects([&] { utf8_back1SafeBody_nid_postfix(text.data(), -1, 0); });
    index = 0;
    Require(utf8_nextCharSafeBody_nid_postfix(nullptr, &index, 0, 0xf0, -1) == -1 && index == 0);
    Require(utf8_prevCharSafeBody_nid_postfix(nullptr, 0, &index, 0x80, -1) == -1 && index == 0);
    for (const auto value : {0, 0x80, 0x800, 0xd800, 0x10000}) {
        const int width = value <= 0x7ff ? 2 : value <= 0xffff ? 3 : 4;
        const int position = INT32_MAX - (width - 1);
        std::int8_t error = 0;
        Require(utf8_appendCharSafeBody_nid_postfix(nullptr, position, INT32_MAX, value, &error) == position && error == 1);
        Rejects([&] { utf8_appendCharSafeBody_nid_postfix(nullptr, position + 1, INT32_MAX, value, &error); });
    }
    Rejects([] { utf8_appendCharSafeBody_nid_postfix(nullptr, INT32_MAX, INT32_MIN, -1, nullptr); });
    std::int8_t error = 0;
    Require(utf8_appendCharSafeBody_nid_postfix(nullptr, INT32_MAX, INT32_MIN, -1, &error) == INT32_MAX && error == 1);
    Require(utf8_appendCharSafeBody_nid_postfix(nullptr, 0, INT32_MIN, -1, nullptr) == 0);
}

}

int main() {
    ErrorModes();
    ScalarRoundTrips();
    AppendFailures();
    TerminatedAndBounded();
    InvalidArguments();
}
