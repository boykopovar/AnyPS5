#include "prx/libSceAgc/Cb/include/Branch.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcCbBranch(CommandBuffer* buf, std::uint8_t mode, std::uint8_t compareFunction, const volatile std::uint64_t* compareAddress, std::uint64_t mask, std::uint64_t reference, std::uint8_t cachePolicy1, const volatile std::uint32_t* buffer1, std::uint32_t sizeInDwords1, std::uint8_t cachePolicy2, const volatile std::uint32_t* buffer2, std::uint32_t sizeInDwords2) {
    mode &= 3u;
    compareFunction &= 7u;
    Agc::Command::CheckBits(cachePolicy1, 3, __func__);
    Agc::Command::CheckBits(cachePolicy2, 3, __func__);
    Agc::Command::CheckBits(sizeInDwords1, 0xfffffu, __func__);
    Agc::Command::CheckBits(sizeInDwords2, 0xfffffu, __func__);
    const auto address = reinterpret_cast<std::uintptr_t>(compareAddress) & (compareFunction != 0 ? ~std::uintptr_t{7} : ~std::uintptr_t{0});
    const auto first = reinterpret_cast<std::uintptr_t>(buffer1);
    const auto second = reinterpret_cast<std::uintptr_t>(buffer2);
    if (first != 0 || sizeInDwords1 != 0) Agc::Command::CheckAddress(first, 4, __func__);
    if (second != 0 || sizeInDwords2 != 0) Agc::Command::CheckAddress(second, 4, __func__);
    return Agc::Command::Emit(buf, 0x3fu, {mode | (static_cast<std::uint32_t>(compareFunction) << 8u), static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), static_cast<std::uint32_t>(mask), static_cast<std::uint32_t>(mask >> 32u), static_cast<std::uint32_t>(reference), static_cast<std::uint32_t>(reference >> 32u), static_cast<std::uint32_t>(first), static_cast<std::uint32_t>(first >> 32u), sizeInDwords1 | (static_cast<std::uint32_t>(cachePolicy1) << 28u), static_cast<std::uint32_t>(second), static_cast<std::uint32_t>(second >> 32u), sizeInDwords2 | (static_cast<std::uint32_t>(cachePolicy2) << 28u)}, __func__);
}

std::uint32_t APS5_VABI sceAgcCbBranchGetSize() {
    return 56;
}

std::uint32_t* APS5_VABI sceAgcCbCondWrite(CommandBuffer* buf, std::uint32_t compareFunction, std::uint32_t writeSpace, const volatile void* writeAddress, std::uint32_t writeData, const volatile void* pollAddress, std::uint32_t reference, std::uint32_t mask) {
    const auto destination = reinterpret_cast<std::uintptr_t>(writeAddress);
    const auto source = reinterpret_cast<std::uintptr_t>(pollAddress);
    Agc::Command::CheckBits(source, 0xffffffffffffull, __func__);
    return Agc::Command::Emit(buf, 0x45u, {0x10u | (compareFunction & 7u) | ((writeSpace & 3u) << 8u), static_cast<std::uint32_t>(source), static_cast<std::uint32_t>(source >> 32u), reference, mask, static_cast<std::uint32_t>(destination), static_cast<std::uint32_t>(destination >> 32u), writeData}, __func__);
}

std::uint32_t APS5_VABI sceAgcCbCondWriteGetSize() {
    return 36;
}

}
