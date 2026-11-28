#include "prx/libc/include/Api.hpp"
#include "tests/GuardedBuffer.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static void Require(bool condition) {
    if (!condition) {
        std::fputs("GuestWideTokenBoundary check failed\n", stderr);
        std::abort();
    }
}

template<class T>
static void Prefix(const T* data) {
    for (int index = 1; index <= 8; ++index) Require(data[-index] == static_cast<T>(sizeof(T) == 1 ? 0xa5 : 0xa5a5));
}

int main() {
    GuardedBuffer input, delimiters, other;
    auto* set = delimiters.At<char16_t>(3);
    set[0] = 0x100; set[1] = 0xd800; set[2] = 0;
    const std::u16string separator(set, 2);
    for (std::size_t length = 0; length <= 128; ++length) {
        auto* source = input.At<char16_t>(length + 1);
        const std::array<char16_t, 7> values{0x100, 1, 0x8000, 0xd800, 0xffff, 0x100, 0xdc00};
        for (std::size_t i = 0; i < length; ++i) source[i] = values[(i + length) % values.size()];
        source[length] = 0;
        const std::u16string saved(source, length + 1);
        std::vector<std::u16string> expected;
        std::u16string field;
        for (std::size_t i = 0; i < length; ++i) {
            if (separator.find(saved[i]) != std::u16string::npos) {
                if (!field.empty()) { expected.push_back(field); field.clear(); }
            } else field.push_back(saved[i]);
        }
        if (!field.empty()) expected.push_back(field);
        auto* alternate = other.At<char16_t>(4);
        alternate[0] = 7; alternate[1] = 0x100; alternate[2] = 9; alternate[3] = 0;
        char16_t* state = nullptr;
        char16_t* otherState = nullptr;
        Require(wcstok_nid_postfix(alternate, set, &otherState) == alternate && alternate[0] == 7 && alternate[1] == 0);
        for (std::size_t i = 0; i < expected.size(); ++i) {
            auto* token = wcstok_nid_postfix(i == 0 ? source : nullptr, set, &state);
            Require(token != nullptr && std::u16string(token) == expected[i]);
        }
        Require(wcstok_nid_postfix(expected.empty() ? source : nullptr, set, &state) == nullptr);
        Require(wcstok_nid_postfix(nullptr, set, &state) == nullptr);
        auto* last = wcstok_nid_postfix(nullptr, set, &otherState);
        Require(last == alternate + 2 && last[0] == 9 && last[1] == 0);
        for (std::size_t i = 0; i <= length; ++i) if (separator.find(saved[i]) == std::u16string::npos) Require(source[i] == saved[i]);
        Require(set[0] == 0x100 && set[1] == 0xd800 && set[2] == 0);
    }
}
