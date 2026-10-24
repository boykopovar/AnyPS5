#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

using DesBlock = std::array<unsigned char, 8>;
using DesSchedule = std::array<unsigned char, 128>;

extern "C" {
void APS5_VABI DES_ecb_encrypt_nid_postfix(const unsigned char*, unsigned char*, const unsigned char*, int);
int APS5_VABI DES_set_key_nid_postfix(const unsigned char*, unsigned char*);
void APS5_VABI DES_set_odd_parity_nid_postfix(unsigned char*);
}

static void Require(bool value) { if (!value) std::abort(); }

template<typename TAction>
static void RequireThrows(TAction action) {
    try { action(); } catch (const std::invalid_argument&) { return; }
    std::abort();
}

static DesBlock Block(std::uint64_t value) {
    DesBlock block{};
    for (int index = 0; index < 8; ++index) block[index] = static_cast<unsigned char>(value >> (56 - 8 * index));
    return block;
}

static void CheckVector(std::uint64_t key, std::uint64_t plain, std::uint64_t cipher) {
    const auto keyBlock = Block(key);
    DesSchedule schedule{};
    schedule.fill(0xaa);
    Require(DES_set_key_nid_postfix(keyBlock.data(), schedule.data()) == 0);
    const auto plainBlock = Block(plain);
    DesBlock output{};
    DES_ecb_encrypt_nid_postfix(plainBlock.data(), output.data(), schedule.data(), 1);
    Require(output == Block(cipher));
    DES_ecb_encrypt_nid_postfix(output.data(), output.data(), schedule.data(), 0);
    Require(output == plainBlock);
    DesBlock again{};
    DES_ecb_encrypt_nid_postfix(plainBlock.data(), again.data(), schedule.data(), 7);
    Require(again == Block(cipher));
}

static void CheckParity() {
    DesBlock key{0x00, 0x01, 0x02, 0x03, 0x12, 0x13, 0xfe, 0xff};
    DES_set_odd_parity_nid_postfix(key.data());
    Require(key == (DesBlock{0x01, 0x01, 0x02, 0x02, 0x13, 0x13, 0xfe, 0xfe}));
    for (int value = 0; value < 256; ++value) {
        DesBlock single{};
        single.fill(static_cast<unsigned char>(value));
        DES_set_odd_parity_nid_postfix(single.data());
        Require((single[0] & 0xfe) == (value & 0xfe) && std::popcount(single[0]) % 2 == 1);
    }
}

int main() {
    CheckVector(0x133457799bbcdff1, 0x0123456789abcdef, 0x85e813540f0ab405);
    CheckVector(0x0e329232ea6d0d73, 0x8787878787878787, 0x0000000000000000);
    CheckVector(0x0101010101010101, 0x8000000000000000, 0x95f8a5e5dd31d900);
    CheckParity();
    DesSchedule schedule{};
    const auto weak = Block(0x0101010101010101);
    Require(DES_set_key_nid_postfix(weak.data(), schedule.data()) == 0);
    const auto badParity = Block(0x0000000000000000);
    Require(DES_set_key_nid_postfix(badParity.data(), schedule.data()) == 0);
    RequireThrows([&] { DES_set_key_nid_postfix(nullptr, schedule.data()); });
    RequireThrows([&] { DES_ecb_encrypt_nid_postfix(weak.data(), schedule.data(), nullptr, 1); });
    RequireThrows([] { DES_set_odd_parity_nid_postfix(nullptr); });
    return 0;
}
