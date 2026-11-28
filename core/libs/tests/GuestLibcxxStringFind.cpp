#include "prx/libc/include/LibcxxStringSearch.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <stdexcept>

namespace {

constexpr std::uint64_t Npos = std::numeric_limits<std::uint64_t>::max();

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "%s\n", message);
        std::abort();
    }
}

std::uint64_t Find(const void* object, char value, std::uint64_t position) {
    return _ZNKSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE4findEcm_nid_postfix(object, value, position);
}

std::uint64_t Rfind(const void* object, char value, std::uint64_t position) {
    return _ZNKSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE5rfindEcm_nid_postfix(object, value, position);
}

}

int main() {
    std::array<unsigned char, 24> shortString{8, 'x', 0, 'y', 'x'};
    Require(Find(shortString.data(), 'x', 1) == 3, "find must start at the requested position");
    Require(Find(shortString.data(), '\0', 0) == 1, "find missed an embedded zero byte");
    Require(Find(shortString.data(), '\0', 2) == Npos, "find searched the terminator");
    Require(Find(shortString.data(), 'x', 4) == Npos, "find accepted a position at the end");
    Require(Rfind(shortString.data(), 'x', 2) == 0 && Rfind(shortString.data(), 'x', Npos) == 3,
            "rfind did not clamp to the last allowed position");
    Require(Rfind(shortString.data(), 'x', 0) == 0, "rfind excluded index zero");

    const std::array<unsigned char, 24> empty{};
    Require(Find(empty.data(), 'x', 0) == Npos && Rfind(empty.data(), 'x', Npos) == Npos,
            "search on an empty string must return npos");

    std::array<char, 9008> bytes{};
    bytes.fill('a');
    bytes[4098] = 'b';
    bytes[8194] = 'b';
    bytes[9000] = 0;
    const std::array<std::uint64_t, 3> longString{
        9009, 9000, reinterpret_cast<std::uintptr_t>(bytes.data()),
    };
    Require(Find(longString.data(), 'b', 0) == 4098 && Find(longString.data(), 'b', 4099) == 8194,
            "long find missed a match across a copy boundary");
    Require(Rfind(longString.data(), 'b', Npos) == 8194 && Rfind(longString.data(), 'b', 8193) == 4098,
            "long rfind missed a match across a copy boundary");

    shortString[0] = 46;
    bool rejected = false;
    try {
        Find(shortString.data(), 'x', 0);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected, "search accepted overflowing short string size");
    std::puts("libc++ string find and rfind guest ABI checks passed");
    return 0;
}
