#include "prx/libc/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

static void Require(bool condition) {
    if (!condition) {
        std::fputs("GuestCaseCompareRandom check failed\n", stderr);
        std::abort();
    }
}

static int Fold(char c) {
    const auto value = static_cast<unsigned char>(c);
    return value >= 'A' && value <= 'Z' ? value + ('a' - 'A') : value;
}

static int ReferenceCompare(const char* s1, const char* s2, std::size_t n) {
    for (; n != 0; ++s1, ++s2, --n) {
        const int a = Fold(*s1);
        const int b = Fold(*s2);
        if (a != b) return a < b ? -1 : 1;
        if (*s1 == 0) break;
    }
    return 0;
}

int main() {
    std::mt19937 random(0xca5e);
    const char alphabet[] = {'a', 'A', 'z', 'Z', '@', '[', '`', '{', '_', '0', '\x7f', '\x80', '\xc4', '\xe4', '\xff'};
    for (int round = 0; round < 100000; ++round) {
        const std::size_t length = random() % 4 == 0 ? random() % 8 : random() % 80;
        std::vector<char> left(length + 1), right(length + 1);
        for (std::size_t index = 0; index < length; ++index) {
            left[index] = alphabet[random() % sizeof(alphabet)];
            const char same = left[index];
            const int choice = static_cast<int>(random() % 8);
            right[index] = choice == 0 ? alphabet[random() % sizeof(alphabet)]
                : choice == 1 && same >= 'a' && same <= 'z' ? static_cast<char>(same - ('a' - 'A'))
                : choice == 2 && same >= 'A' && same <= 'Z' ? static_cast<char>(same + ('a' - 'A')) : same;
        }
        if (length != 0 && random() % 6 == 0) right[random() % length] = 0;
        if (length != 0 && random() % 6 == 0) left[random() % length] = 0;
        const std::size_t limit = random() % 3 == 0 ? SIZE_MAX : random() % (length + 2);
        Require(strcasecmp_nid_postfix(left.data(), right.data()) == ReferenceCompare(left.data(), right.data(), SIZE_MAX));
        Require(strcasecmp_nid_postfix(right.data(), left.data()) == ReferenceCompare(right.data(), left.data(), SIZE_MAX));
        Require(strncasecmp_nid_postfix(left.data(), right.data(), limit) == ReferenceCompare(left.data(), right.data(), limit));
        Require(strncasecmp_nid_postfix(right.data(), left.data(), limit) == ReferenceCompare(right.data(), left.data(), limit));
    }
    for (int c = 0; c < 256; ++c) Require(tolower_nid_postfix(c) == Fold(static_cast<char>(c)));
}
