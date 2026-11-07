#include "prx/libSceIcu/libSceIcu.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "ICU CharString check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction>
void Invalid(TAction action) {
    try { action(); } catch (const std::invalid_argument&) { return; }
    Require(false);
}

}

int main() {
    Icu::Abi::CharString text{};
    text.buffer.pointer = text.buffer.stack;
    text.buffer.capacity = 40;
    const std::array<char, 5> content{'a', 0, 'b', 'a', 0};
    std::copy(content.begin(), content.end(), text.buffer.stack);
    text.length = 4;
    Require(_ZNK6icu_6810CharString13getDirSepCharEv_nid_postfix(&text) == '/');
    Require(_ZNK6icu_6810CharString11lastIndexOfEc_nid_postfix(&text, 'a') == 3);
    Require(_ZNK6icu_6810CharString11lastIndexOfEc_nid_postfix(&text, 0) == 1);
    Require(_ZNK6icu_6810CharString11lastIndexOfEc_nid_postfix(&text, 'z') == -1);
    for (int initial : {0, -124, -128, 1, 15}) {
        for (int capacity = -1; capacity <= 6; ++capacity) {
            std::array<char, 8> output;
            output.fill('!');
            std::int32_t status = initial;
            Require(_ZNK6icu_6810CharString7extractEPciR10UErrorCode_nid_postfix(&text, output.data() + 1, capacity, &status) == 4);
            const auto expected = initial > 0 ? initial : capacity < 0 ? 1 : capacity < 4 ? 15 : capacity == 4 ? -124 : initial == -124 ? 0 : initial;
            Require(status == expected);
            Require(output.front() == '!');
            for (int index = 0; index < 7; ++index) {
                const auto byte = initial <= 0 && capacity >= 4 && index < 4 ? content[index]
                    : initial <= 0 && capacity > 4 && index == 4 ? 0 : '!';
                Require(output[index + 1] == byte);
            }
        }
    }
    for (int capacity : {0, 1, -1}) {
        std::int32_t status = 0;
        Require(_ZNK6icu_6810CharString7extractEPciR10UErrorCode_nid_postfix(&text, nullptr, capacity, &status) == 4);
        Require(status == (capacity == 0 ? 15 : 1));
    }
    std::int32_t status = 0;
    Require(_ZNK6icu_6810CharString7extractEPciR10UErrorCode_nid_postfix(&text, text.buffer.pointer, 40, &status) == 4);
    Require(status == 0 && text.buffer.pointer[4] == 0);
    Require(_ZN6icu_6810CharString8truncateEi_nid_postfix(&text, 100) == &text && text.length == 4);
    Require(_ZN6icu_6810CharString8truncateEi_nid_postfix(&text, 2) == &text && text.length == 2 && text.buffer.pointer[2] == 0);
    _ZN6icu_6810CharString8truncateEi_nid_postfix(&text, -1);
    Require(text.length == 0 && text.buffer.pointer[0] == 0);
    status = 0;
    Require(_ZNK6icu_6810CharString7extractEPciR10UErrorCode_nid_postfix(&text, nullptr, 0, &status) == 0 && status == -124);
    Invalid([&] { _ZNK6icu_6810CharString7extractEPciR10UErrorCode_nid_postfix(&text, nullptr, 0, nullptr); });
    Invalid([] { _ZN6icu_6810CharString8truncateEi_nid_postfix(nullptr, 0); });
    Invalid([] { _ZNK6icu_6810CharString11lastIndexOfEc_nid_postfix(nullptr, 'a'); });
    text.length = 2;
    text.buffer.pointer = nullptr;
    Invalid([&] { _ZN6icu_6810CharString8truncateEi_nid_postfix(&text, 0); });
    Invalid([&] { _ZNK6icu_6810CharString11lastIndexOfEc_nid_postfix(&text, 0); });
    std::puts("ICU CharString inspection tests passed");
}
