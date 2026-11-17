#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Testing.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

static const LibreSslEvpCipher* Cipher(const unsigned char* memory) {
    return reinterpret_cast<const LibreSslEvpCipher*>(memory);
}

constexpr std::size_t CipherSize = 88;
constexpr std::size_t BlockSizeOffset = 4;

static int BlockSize(std::int32_t value) {
    alignas(8) unsigned char cipher[CipherSize];
    std::memset(cipher, 0xAA, sizeof(cipher));
    std::memcpy(cipher + BlockSizeOffset, &value, sizeof(value));
    return EVP_CIPHER_block_size_nid_postfix(Cipher(cipher));
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
    Require(EVP_CIPHER_block_size_nid_postfix(Cipher(cipher)) == 16);
    Require(std::memcmp(before, cipher, sizeof(cipher)) == 0);
    ExpectErr([] { EVP_CIPHER_block_size_nid_postfix(nullptr); });
}

int main() {
    TestBlockSize();
    std::puts("libSceLibreSsl EVP_CIPHER tests passed");
}
