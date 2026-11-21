#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 DES_set_key_unchecked check failed at line %u\n", location.line());
    std::abort();
}

using Block = std::array<unsigned char, 8>;
using Schedule = std::array<unsigned char, 128>;

Block Encrypt(const Block& key, const Block& plain) {
    Schedule schedule;
    schedule.fill(0xA5);
    DES_set_key_unchecked_nid_postfix(key.data(), schedule.data());
    Schedule expected;
    expected.fill(0x5A);
    Require(DES_set_key_nid_postfix(key.data(), expected.data()) == 0);
    Require(schedule == expected);
    Block cipher{};
    DES_ecb_encrypt_nid_postfix(plain.data(), cipher.data(), schedule.data(), 1);
    Block decrypted{};
    DES_ecb_encrypt_nid_postfix(cipher.data(), decrypted.data(), schedule.data(), 0);
    Require(decrypted == plain);
    return cipher;
}

void TestSessionGetMasterKey() {
    std::array<unsigned char, 64> session{};
    const std::uint64_t masterKeyLength = 48;
    std::memcpy(session.data() + 8, &masterKeyLength, sizeof(masterKeyLength));
    for (std::size_t i = 0; i < 48; ++i) {
        session[16 + i] = static_cast<unsigned char>(0x10 + i);
    }

    std::array<unsigned char, 48> out;
    out.fill(0xA5);
    Require(SSL_SESSION_get_master_key_nid_postfix(session.data(), out.data(), 0) == 48);
    for (unsigned char byte : out) {
        Require(byte == 0xA5);
    }

    Require(SSL_SESSION_get_master_key_nid_postfix(session.data(), nullptr, 48) == 48);

    out.fill(0xA5);
    Require(SSL_SESSION_get_master_key_nid_postfix(session.data(), out.data(), 16) == 16);
    for (std::size_t i = 0; i < 16; ++i) {
        Require(out[i] == static_cast<unsigned char>(0x10 + i));
    }
    for (std::size_t i = 16; i < out.size(); ++i) {
        Require(out[i] == 0xA5);
    }
}

}

int main() {
    const Block plain{0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
    Require(Encrypt({0x13, 0x34, 0x57, 0x79, 0x9B, 0xBC, 0xDF, 0xF1}, plain) == Block{0x85, 0xE8, 0x13, 0x54, 0x0F, 0x0A, 0xB4, 0x05});
    Require(Encrypt({0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01}, plain) == Block{0x61, 0x7B, 0x3A, 0x0C, 0xE8, 0xF0, 0x71, 0x00});
    Require(Encrypt({0x12, 0x34, 0x57, 0x79, 0x9B, 0xBC, 0xDF, 0xF0}, plain) == Block{0x85, 0xE8, 0x13, 0x54, 0x0F, 0x0A, 0xB4, 0x05});
    TestSessionGetMasterKey();
}
