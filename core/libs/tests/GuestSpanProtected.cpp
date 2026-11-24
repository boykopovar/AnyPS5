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
        std::fputs("GuestSpanProtected check failed\n", stderr);
        std::abort();
    }
}

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

int main() {
    GuardedBuffer textBuffer, setBuffer;
    for (unsigned selected = 0; selected < 256; ++selected) {
        std::string members;
        std::array<bool, 256> present{};
        for (unsigned value = 1; value < 256; ++value) {
            if ((value + selected) % 7 == 0 || value == selected) {
                members.push_back(static_cast<char>(value));
                present[value] = true;
            }
        }
        if (selected == 0) { members.clear(); present.fill(false); }
        auto* set = reinterpret_cast<char*>(setBuffer.At(members.size() + 1));
        std::memcpy(set, members.c_str(), members.size() + 1);
        for (std::size_t length : std::array<std::size_t, 9>{0, 1, 7, 15, 16, 31, 32, 63, 127}) {
            std::string input;
            for (std::size_t i = 0; i < length; ++i) input.push_back(static_cast<char>(1 + (i * 37 + selected) % 255));
            auto* text = reinterpret_cast<char*>(textBuffer.At(length + 1));
            std::memcpy(text, input.c_str(), length + 1);
            std::size_t accepted = 0, rejected = 0;
            while (accepted < length && present[static_cast<unsigned char>(input[accepted])]) ++accepted;
            while (rejected < length && !present[static_cast<unsigned char>(input[rejected])]) ++rejected;
            Require(strspn_nid_postfix(text, set) == accepted);
            Require(strcspn_nid_postfix(text, set) == rejected);
            Require(std::memcmp(text, input.c_str(), length + 1) == 0);
            Require(std::memcmp(set, members.c_str(), members.size() + 1) == 0);
        }
    }
    for (std::size_t length : {256, 512, 1024}) {
        std::string allA(length, 'a');
        auto* text = reinterpret_cast<char*>(textBuffer.At(length + 1));
        std::memcpy(text, allA.c_str(), length + 1);

        auto* set = reinterpret_cast<char*>(setBuffer.At(2));
        set[0] = 'a'; set[1] = 0;
        Require(strspn_nid_postfix(text, set) == length);
        Require(strcspn_nid_postfix(text, set) == 0);

        set[0] = 'b'; set[1] = 0;
        Require(strspn_nid_postfix(text, set) == 0);
        Require(strcspn_nid_postfix(text, set) == length);

        set = reinterpret_cast<char*>(setBuffer.At(1));
        set[0] = 0;
        Require(strspn_nid_postfix(text, set) == 0);
        Require(strcspn_nid_postfix(text, set) == length);

        allA[length / 2] = 'b';
        std::memcpy(text, allA.c_str(), length + 1);

        set = reinterpret_cast<char*>(setBuffer.At(2));
        set[0] = 'a'; set[1] = 0;
        Require(strspn_nid_postfix(text, set) == length / 2);

        set[0] = 'b'; set[1] = 0;
        Require(strcspn_nid_postfix(text, set) == length / 2);
    }
}
