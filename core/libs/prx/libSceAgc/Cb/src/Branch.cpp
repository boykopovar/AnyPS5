#include "prx/libSceAgc/Cb/include/Branch.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcCbBranch(CommandBuffer* buf, std::uint8_t mode, std::uint8_t compareFunction, const volatile std::uint64_t* compareAddress, std::uint64_t mask, std::uint64_t reference, std::uint8_t cachePolicy1, const volatile std::uint32_t* buffer1, std::uint32_t sizeInDwords1, std::uint8_t cachePolicy2, const volatile std::uint32_t* buffer2, std::uint32_t sizeInDwords2) {
    Agc::Command::CheckBits(mode, 3, __func__);
    Agc::Command::CheckBits(compareFunction, 7, __func__);
    Agc::Command::CheckBits(cachePolicy1, 3, __func__);
    Agc::Command::CheckBits(cachePolicy2, 3, __func__);
    Agc::Command::CheckBits(sizeInDwords1, 0xfffffu, __func__);
    Agc::Command::CheckBits(sizeInDwords2, 0xfffffu, __func__);
    const auto address = reinterpret_cast<std::uintptr_t>(compareAddress);
    const auto first = reinterpret_cast<std::uintptr_t>(buffer1);
    const auto second = reinterpret_cast<std::uintptr_t>(buffer2);
    Agc::Command::CheckAddress(address, 8, __func__);
    Agc::Command::CheckAddress(first, 4, __func__);
    Agc::Command::CheckAddress(second, 4, __func__);
    return Agc::Command::Emit(buf, 0x3fu, {mode | (static_cast<std::uint32_t>(compareFunction) << 8u), static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), static_cast<std::uint32_t>(mask), static_cast<std::uint32_t>(mask >> 32u), static_cast<std::uint32_t>(reference), static_cast<std::uint32_t>(reference >> 32u), static_cast<std::uint32_t>(first), static_cast<std::uint32_t>(first >> 32u), sizeInDwords1 | (static_cast<std::uint32_t>(cachePolicy1) << 28u), static_cast<std::uint32_t>(second), static_cast<std::uint32_t>(second >> 32u), sizeInDwords2 | (static_cast<std::uint32_t>(cachePolicy2) << 28u)}, __func__);
}

std::uint32_t APS5_VABI sceAgcCbBranchGetSize() {
    return 56;
}

uint32_t* APS5_VABI sceAgcCbCondWrite(CommandBuffer* buf, uint32_t cond, uint32_t a, uint32_t b, uint32_t c) {
    (void)buf;
    (void)cond;
    (void)a;
    (void)b;
    (void)c;
    NotImplemented_nid_no_patch(__func__);
    return nullptr;
}

}
