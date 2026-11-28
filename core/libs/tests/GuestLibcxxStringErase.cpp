#include "prx/libc/include/Api.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

constexpr auto Erase = _ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE5eraseEmm_nid_postfix;
constexpr auto Npos = std::numeric_limits<std::uint64_t>::max();

struct ShortString {
    unsigned char flags;
    std::array<char, 23> data;
};

struct LongString {
    std::uint64_t capacity;
    std::uint64_t size;
    char* data;
};

static_assert(sizeof(ShortString) == 24 && sizeof(LongString) == 24);

void Require(bool condition, const char* failure) {
    if (condition) return;
    std::fputs(failure, stderr);
    std::fputc('\n', stderr);
    std::abort();
}

}

int main() {
    ShortString small{12, {'a', 'b', '\0', 'c', 'd', 'e'}};
    Require(Erase(&small, 1, 1) == &small, "erase must return the original guest object");
    constexpr char expected[] = {'a', '\0', 'c', 'd', 'e', '\0'};
    Require(small.flags == 10 && std::memcmp(small.data.data(), expected, sizeof(expected)) == 0,
            "short erase did not preserve the embedded NUL and terminator");
    const auto unchanged = small;
    Require(Erase(&small, 2, 0) == &small && std::memcmp(&unchanged, &small, sizeof(small)) == 0,
            "zero-length erase changed the short string");
    try {
        Erase(&small, 6, 0);
        Require(false, "position past size must throw even when count is zero");
    } catch (const std::out_of_range&) {
        Require(std::memcmp(&unchanged, &small, sizeof(small)) == 0,
                "out-of-range erase modified the string");
    }
    Require(Erase(&small, 2, Npos) == &small && small.flags == 4 && small.data[2] == '\0',
            "short npos erase did not leave exactly the prefix");

    std::array<char, 6256> storage{};
    for (std::size_t i = 0; i < storage.size() - 1; ++i) {
        storage[i] = static_cast<char>('A' + i % 23);
    }
    const auto original = storage;
    LongString large{storage.size() | 1u, storage.size() - 1, storage.data()};
    const auto capacity = large.capacity;
    Require(Erase(&large, 117, 3) == &large, "long erase did not return this");
    Require(large.size == storage.size() - 4 && large.capacity == capacity && large.data == storage.data(),
            "long erase changed the guest allocation or reported the wrong size");
    Require(std::equal(storage.begin(), storage.begin() + 117, original.begin()) &&
                std::equal(storage.begin() + 117, storage.begin() + large.size, original.begin() + 120) &&
                storage[large.size] == '\0',
            "long erase did not move the complete overlapping suffix");
    Require(Erase(&large, 4, Npos - 1) == &large && large.capacity == capacity && large.size == 4 &&
                large.data == storage.data() && storage[4] == '\0',
            "oversized count did not clip to the end while keeping the long allocation");
    Require(Erase(&large, large.size, Npos) == &large && large.size == 4,
            "erase at exactly size must succeed as a no-op");
    std::puts("libc++ string erase ABI checks passed");
    return 0;
}
