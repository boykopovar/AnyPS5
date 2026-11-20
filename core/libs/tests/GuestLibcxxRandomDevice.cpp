#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>

extern "C" void APS5_VABI _ZNSt3__113random_deviceC2ERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEE_nid_postfix(
    void* self, const void* token);
extern "C" void APS5_VABI _ZNSt3__113random_deviceD1Ev_nid_postfix(void* self);
extern "C" std::uint32_t APS5_VABI _ZNSt3__113random_deviceclEv_nid_postfix(void* self);

static void Require(bool condition) { if (!condition) std::abort(); }

int main() {
    std::array<std::uint8_t, 16> object;
    std::array<std::uint8_t, 32> token;
    object.fill(0x5a);
    token.fill(0xa5);
    _ZNSt3__113random_deviceC2ERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEE_nid_postfix(
        object.data(), token.data());
    Require(object[0] == 0x5a && object[15] == 0x5a);
    Require(token[0] == 0xa5 && token[31] == 0xa5);

    const std::uint32_t first = _ZNSt3__113random_deviceclEv_nid_postfix(object.data());
    bool varied = false;
    for (int index = 0; index < 64; ++index)
        varied |= _ZNSt3__113random_deviceclEv_nid_postfix(object.data()) != first;
    Require(varied);

    _ZNSt3__113random_deviceD1Ev_nid_postfix(object.data());
    Require(object[0] == 0x5a && object[15] == 0x5a);
    Require(token[0] == 0xa5 && token[31] == 0xa5);
}
