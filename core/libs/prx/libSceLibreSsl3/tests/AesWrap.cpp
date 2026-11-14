#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 AES wrap check failed at line %u\n", location.line());
    std::abort();
}

std::string Hex(const unsigned char* bytes, std::size_t length) {
    static constexpr char Digits[] = "0123456789abcdef";
    std::string text;
    for (std::size_t index = 0; index < length; ++index) {
        text += Digits[bytes[index] >> 4];
        text += Digits[bytes[index] & 15];
    }
    return text;
}

AesKey Kek(int bits) {
    unsigned char bytes[32];
    for (std::size_t index = 0; index < sizeof(bytes); ++index) bytes[index] = static_cast<unsigned char>(index);
    AesKey key{};
    Require(AES_set_encrypt_key_nid_postfix(bytes, bits, &key) == 0);
    return key;
}

const unsigned char RfcData[32] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};

std::string Wrap(int bits, const unsigned char* iv, std::uint32_t length) {
    AesKey key = Kek(bits);
    unsigned char out[48];
    std::memset(out, 0x5a, sizeof(out));
    Require(AES_wrap_key_nid_postfix(&key, iv, out, RfcData, length) == static_cast<int>(length + 8));
    Require(out[length + 8] == 0x5a);
    return Hex(out, length + 8);
}

void CheckRfc3394() {
    Require(Wrap(192, nullptr, 16) == "96778b25ae6ca435f92b5b97c050aed2468ab8a17ad84e5d");
    Require(Wrap(256, nullptr, 16) == "64e8c3f9ce0f5ba263e9777905818a2a93c8191e7d6e8ae7");
    Require(Wrap(192, nullptr, 24) == "031d33264e15d33268f24ec260743edce1c6c7ddee725a936ba814915c6762d2");
    Require(Wrap(256, nullptr, 24) == "a8f9bc1612c68b3ff6e6f4fbe30e71e4769c8b80a32cb8958cd5d17d6b254da1");
    Require(Wrap(256, nullptr, 32) == "28c9f404c4b810f4cbccb35cfb87f8263f5786e2d80ed326cbc7f0e71a99f43bfb988b9b7a02dd21");
}

void CheckIv() {
    const unsigned char iv[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    Require(Wrap(128, iv, 16) == "1f0167d51c8f6e4eb1f0286f52cf9cf97d904baf9ddda8a4");
    const unsigned char defaultIv[8] = {0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6, 0xa6};
    Require(Wrap(128, defaultIv, 16) == "1fa68b0a8112b447aef34bd8fb5a7b829d3e862371d2cfe5");
}

void CheckLong(std::uint32_t length, const char* head, const char* tail) {
    std::vector<unsigned char> in(length);
    for (std::uint32_t index = 0; index < length; ++index) in[index] = static_cast<unsigned char>(index * 31 + 7);
    std::vector<unsigned char> out(length + 8);
    AesKey key = Kek(128);
    Require(AES_wrap_key_nid_postfix(&key, nullptr, out.data(), in.data(), length) == static_cast<int>(length + 8));
    Require(Hex(out.data(), 8) == head);
    Require(Hex(out.data() + length, 8) == tail);
}

void CheckInPlace() {
    AesKey key = Kek(128);
    const char* expected = "889671106535a9f86d9f9a262f674569efa38d7535aac77527cab92855bddd6e";
    unsigned char shifted[32];
    std::memcpy(shifted + 8, RfcData, 24);
    Require(AES_wrap_key_nid_postfix(&key, nullptr, shifted, shifted + 8, 24) == 32);
    Require(Hex(shifted, sizeof(shifted)) == expected);
    unsigned char same[32];
    std::memcpy(same, RfcData, 24);
    Require(AES_wrap_key_nid_postfix(&key, nullptr, same, same, 24) == 32);
    Require(Hex(same, sizeof(same)) == expected);
}

void CheckRejectedLengths() {
    AesKey key = Kek(128);
    unsigned char out[48];
    std::memset(out, 0x5a, sizeof(out));
    for (const std::uint32_t length : {0u, 8u, 15u, 17u, 31u}) Require(AES_wrap_key_nid_postfix(&key, nullptr, out, RfcData, length) == -1);
    for (const unsigned char byte : out) Require(byte == 0x5a);
}

}

int main() {
    const unsigned char kek[16] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
    const unsigned char plain[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
    const unsigned char expected[24] = {0x1f, 0xa6, 0x8b, 0x0a, 0x81, 0x12, 0xb4, 0x47, 0xae, 0xf3, 0x4b, 0xd8,
        0xfb, 0x5a, 0x7b, 0x82, 0x9d, 0x3e, 0x86, 0x23, 0x71, 0xd2, 0xcf, 0xe5};
    AesKey key{};
    Require(AES_set_encrypt_key_nid_postfix(kek, 128, &key) == 0);
    unsigned char ecb[16]{};
    AES_encrypt_nid_postfix(plain, ecb, &key);
    const unsigned char expectedEcb[16] = {0x69, 0xc4, 0xe0, 0xd8, 0x6a, 0x7b, 0x04, 0x30, 0xd8, 0xcd, 0xb7, 0x80, 0x70, 0xb4, 0xc5, 0x5a};
    for (std::size_t index = 0; index < sizeof(expectedEcb); ++index) Require(ecb[index] == expectedEcb[index]);
    unsigned char wrapped[24]{};
    Require(AES_wrap_key_nid_postfix(&key, nullptr, wrapped, plain, sizeof(plain)) == 24);
    for (std::size_t index = 0; index < sizeof(expected); ++index) Require(wrapped[index] == expected[index]);

    Require(AES_wrap_key_nid_postfix(&key, nullptr, wrapped, plain, 8) == -1);
    Require(AES_wrap_key_nid_postfix(&key, nullptr, wrapped, plain, 20) == -1);

    bool threw = false;
    try {
        AES_wrap_key_nid_postfix(&key, nullptr, nullptr, plain, sizeof(plain));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    CheckRfc3394();
    CheckIv();
    CheckLong(336, "d293d9a1178a1c56", "9c7c803cf8f7ec53");
    CheckLong(344, "e4143f2d5f1f7f87", "33237f42754a2551");
    CheckLong(87376, "f4ade4a05685a269", "e0280d1f43d0cb33");
    CheckLong(87384, "c0b1b6f333a86076", "644fac20407b6147");
    CheckInPlace();
    CheckRejectedLengths();
    std::puts("libSceLibreSsl3 AES wrap tests passed");
}
