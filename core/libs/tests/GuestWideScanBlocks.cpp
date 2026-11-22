#include "prx/libc/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <vector>

static void Require(bool condition) {
    if (!condition) {
        std::fputs("GuestWideScanBlocks check failed\n", stderr);
        std::abort();
    }
}

static const char16_t* ReferenceChr(const char16_t* s, char16_t c) {
    for (;; ++s) {
        if (*s == c) return s;
        if (*s == 0) return nullptr;
    }
}

static const char16_t* ReferenceRchr(const char16_t* s, char16_t c) {
    const char16_t* found = nullptr;
    for (;; ++s) {
        if (*s == c) found = s;
        if (*s == 0) return found;
    }
}

static const char16_t* ReferenceMemchr(const char16_t* s, char16_t c, std::size_t n) {
    for (; n != 0; ++s, --n) {
        if (*s == c) return s;
    }
    return nullptr;
}

int main() {
    std::mt19937 random(0x5eed);
    const char16_t alphabet[] = {1, 0x41, 0xff, 0x100, 0x7fff, 0x8000, 0xd800, 0xffff};
    std::vector<unsigned char> storage(1024 + 64);
    for (int round = 0; round < 20000; ++round) {
        const std::size_t offset = random() % 33 / 2 * 2;
        auto* base = storage.data() + (32 - reinterpret_cast<std::uintptr_t>(storage.data()) % 32);
        auto* s = reinterpret_cast<char16_t*>(base + offset);
        const std::size_t capacity = (storage.size() - 64 - offset) / sizeof(char16_t);
        for (std::size_t index = 0; index < (offset + 1) / 2 + 16; ++index) {
            reinterpret_cast<char16_t*>(base)[index] = alphabet[random() % 8];
        }
        const std::size_t length = random() % 4 == 0 ? random() % 24 : random() % (capacity - 16);
        for (std::size_t index = 0; index < length; ++index) s[index] = alphabet[random() % 8];
        s[length] = 0;
        for (std::size_t index = length + 1; index < length + 16; ++index) s[index] = alphabet[random() % 8];
        const char16_t target = random() % 6 == 0 ? char16_t{0} : alphabet[random() % 8];
        Require(wcslen_nid_postfix(s) == length);
        Require(wcschr_nid_postfix(s, target) == ReferenceChr(s, target));
        Require(wcsrchr_nid_postfix(s, target) == ReferenceRchr(s, target));
        const std::size_t limit = random() % (length + 17);
        Require(wmemchr_nid_postfix(s, target, limit) == ReferenceMemchr(s, target, limit));
        const char16_t* end = ReferenceMemchr(s, 0, limit);
        Require(wcsnlen_s_nid_postfix(s, limit) == (end == nullptr ? limit : static_cast<std::size_t>(end - s)));
    }
    char16_t text[] = u"abc";
    Require(wmemchr_nid_postfix(text, u'c', SIZE_MAX) == text + 2);
    Require(wcsnlen_s_nid_postfix(text, SIZE_MAX) == 3);
    Require(wmemchr_nid_postfix(nullptr, u'a', 0) == nullptr);
    Require(wcsnlen_s_nid_postfix(text, 0) == 0);
}
