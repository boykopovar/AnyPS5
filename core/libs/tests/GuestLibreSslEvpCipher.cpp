#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

extern "C" {
int APS5_VABI EVP_CIPHER_block_size_nid_postfix(const void*);
}

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL EVP_CIPHER check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> static void RequireThrows(TAction action, std::source_location location = std::source_location::current()) {
    try { action(); } catch (const std::exception&) { return; }
    Require(false, location);
}

constexpr std::size_t CipherSize = 88;
constexpr std::size_t BlockSizeOffset = 4;

static int BlockSize(std::int32_t value) {
    alignas(8) unsigned char cipher[CipherSize];
    std::memset(cipher, 0xAA, sizeof(cipher));
    std::memcpy(cipher + BlockSizeOffset, &value, sizeof(value));
    return EVP_CIPHER_block_size_nid_postfix(cipher);
}

static void TestBlockSize() {
    Require(BlockSize(1) == 1);
    Require(BlockSize(8) == 8);
    Require(BlockSize(16) == 16);
    Require(BlockSize(0) == 0);
    Require(BlockSize(-1) == -1);
    alignas(8) unsigned char cipher[CipherSize] {};
    const std::int32_t nid = 419;
    const std::int32_t keyLength = 16;
    const std::int32_t blockSize = 16;
    std::memcpy(cipher, &nid, sizeof(nid));
    std::memcpy(cipher + BlockSizeOffset, &blockSize, sizeof(blockSize));
    std::memcpy(cipher + 8, &keyLength, sizeof(keyLength));
    unsigned char before[CipherSize];
    std::memcpy(before, cipher, sizeof(cipher));
    Require(EVP_CIPHER_block_size_nid_postfix(cipher) == 16);
    Require(std::memcmp(before, cipher, sizeof(cipher)) == 0);
    RequireThrows([] { EVP_CIPHER_block_size_nid_postfix(nullptr); });
}

int main() {
    TestBlockSize();
    std::puts("libSceLibreSsl EVP_CIPHER tests passed");
}
