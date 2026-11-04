#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include <cstdint>
#include <cstdio>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 AES wrap check failed at line %u\n", location.line());
    std::abort();
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
    std::puts("libSceLibreSsl3 AES wrap tests passed");
}
