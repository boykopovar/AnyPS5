#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <vector>

namespace {

struct Bignum {
    std::uint64_t* d;
    std::int32_t top;
    std::int32_t dmax;
    std::int32_t neg;
    std::int32_t flags;
};

static_assert(sizeof(Bignum) == 24);

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 RSA_bits check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

LibreSsl3Rsa Key(Bignum* modulus) {
    LibreSsl3Rsa rsa;
    std::memset(&rsa, 0xA5, sizeof(rsa));
    rsa.n = modulus;
    return rsa;
}

int Bits(std::vector<std::uint64_t>& words, std::int32_t top) {
    Bignum modulus {words.data(), top, static_cast<std::int32_t>(words.size()), 0, 0};
    LibreSsl3Rsa rsa = Key(&modulus);
    LibreSsl3Rsa before;
    std::memcpy(&before, &rsa, sizeof(rsa));
    const int bits = RSA_bits_nid_postfix(&rsa);
    Require(std::memcmp(&before, &rsa, sizeof(rsa)) == 0);
    Require(modulus.top == top);
    return bits;
}

void TestZero() {
    std::vector<std::uint64_t> words(4, 0xFFFFFFFFFFFFFFFFull);
    Require(Bits(words, 0) == 0);
    Bignum modulus {nullptr, 0, 0, 0, 0};
    LibreSsl3Rsa rsa = Key(&modulus);
    Require(RSA_bits_nid_postfix(&rsa) == 0);
}

void TestTopWord() {
    for (std::int32_t top : {1, 2, 32, 64}) {
        std::vector<std::uint64_t> words(static_cast<std::size_t>(top) + 1, 0x5A5A5A5A5A5A5A5Aull);
        const int base = (top - 1) * 64;
        words[top - 1] = 1;
        Require(Bits(words, top) == base + 1);
        words[top - 1] = 0x8000000000000000ull;
        Require(Bits(words, top) == base + 64);
        words[top - 1] = std::numeric_limits<std::uint64_t>::max();
        Require(Bits(words, top) == base + 64);
        words[top - 1] = 0x10000;
        Require(Bits(words, top) == base + 17);
    }
}

void TestModuli() {
    std::vector<std::uint64_t> words2048(32, 0x0123456789ABCDEFull);
    words2048[31] = 0xC000000000000001ull;
    Require(Bits(words2048, 32) == 2048);
    std::vector<std::uint64_t> words4096(64, 0xFEDCBA9876543210ull);
    words4096[63] = 0x8000000000000000ull;
    Require(Bits(words4096, 64) == 4096);
    std::vector<std::uint64_t> words4095(64, 0);
    words4095[63] = 0x4000000000000000ull;
    Require(Bits(words4095, 64) == 4095);
}

void TestMalformed() {
    RequireInvalidArgument([] { RSA_bits_nid_postfix(nullptr); });
    LibreSsl3Rsa missing = Key(nullptr);
    RequireInvalidArgument([&] { RSA_bits_nid_postfix(&missing); });
    std::uint64_t word = 1;
    Bignum negative {&word, -1, 1, 0, 0};
    LibreSsl3Rsa negativeKey = Key(&negative);
    RequireInvalidArgument([&] { RSA_bits_nid_postfix(&negativeKey); });
    Bignum nullWords {nullptr, 1, 1, 0, 0};
    LibreSsl3Rsa nullWordsKey = Key(&nullWords);
    RequireInvalidArgument([&] { RSA_bits_nid_postfix(&nullWordsKey); });
    constexpr std::int32_t largeTop = 33554432;
    std::uint64_t topWord = std::numeric_limits<std::uint64_t>::max();
    auto* words = reinterpret_cast<std::uint64_t*>(reinterpret_cast<std::uintptr_t>(&topWord) - static_cast<std::uintptr_t>(largeTop - 1) * sizeof(std::uint64_t));
    Bignum overflow {words, largeTop, largeTop, 0, 0};
    LibreSsl3Rsa overflowKey = Key(&overflow);
    RequireInvalidArgument([&] { RSA_bits_nid_postfix(&overflowKey); });
    topWord = std::numeric_limits<std::uint64_t>::max() >> 1;
    Require(RSA_bits_nid_postfix(&overflowKey) == std::numeric_limits<int>::max());
}

}

int main() {
    TestZero();
    TestTopWord();
    TestModuli();
    TestMalformed();
    std::puts("LibreSSL3 RSA_bits tests passed");
}
