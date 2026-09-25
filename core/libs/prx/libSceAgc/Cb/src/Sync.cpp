#include "prx/libSceAgc/Cb/include/Sync.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcCbNop_nid_postfix(CommandBuffer* buf, std::uint32_t sizeInDwords) {
    return Agc::Command::WriteNop(buf, sizeInDwords, __func__);
}

uint32_t APS5_VABI sceAgcCbNopGetSize(uint32_t size_in_dwords) {
 (void)size_in_dwords;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

std::uint32_t APS5_VABI sceAgcCbQueueEndOfPipeActionGetSize() {
    return 32;
}

std::uint32_t* APS5_VABI sceAgcCbReleaseMem(CommandBuffer* buf, std::uint8_t action, std::uint16_t gcrControl, std::uint8_t dst, std::uint8_t cachePolicy, const volatile Label* address, std::uint8_t dataSelect, std::uint64_t data, std::uint16_t gdsOffset, std::uint16_t gdsSize, std::uint8_t interrupt, std::uint32_t interruptContextId) {
    Agc::Command::CheckBits(action, 0x3fu, __func__);
    Agc::Command::CheckBits(gcrControl, 0xfffu, __func__);
    Agc::Command::CheckBits(dst, 1, __func__);
    Agc::Command::CheckBits(cachePolicy, 3, __func__);
    Agc::Command::Require(dataSelect <= 3 || dataSelect == 5, __func__, "invalid release data selector");
    Agc::Command::Require(interrupt <= 4, __func__, "invalid release interrupt selector");
    Agc::Command::CheckBits(interruptContextId, 0x7ffffffu, __func__);
    auto guestAddress = reinterpret_cast<std::uintptr_t>(address);
    auto value = data;
    if (interrupt == 4) {
        Agc::Command::Require(guestAddress == 0 && value == 0, __func__, "interrupt-only release cannot write data");
    } else if (dataSelect == 5) {
        Agc::Command::Require(data == 0, __func__, "GDS release cannot use immediate data");
        value = gdsOffset | (static_cast<std::uint64_t>(gdsSize) << 16u);
    }
    // A null label is accepted: games emit releases whose destination is never read.
    if (dataSelect != 0 && interrupt != 4 && guestAddress != 0) {
        Agc::Command::CheckAddress(guestAddress, dataSelect == 2 || dataSelect == 3 ? 8 : 4, __func__);
    }
    if (dataSelect == 1) {
        Agc::Command::CheckBits(data, 0xffffffffu, __func__);
    }
    std::uint32_t control = gcrControl;
    if ((control & 0x300u) == 0x100u) {
        control |= 0x200u;
    }
    const auto eventIndex = action >= 0x2fu ? 6u : 5u;
    auto* packet = Agc::Command::Emit(buf, 0x49u, {action | (eventIndex << 8u) | (control << 12u) | (static_cast<std::uint32_t>(cachePolicy) << 25u), (static_cast<std::uint32_t>(dst) << 16u) | (static_cast<std::uint32_t>(interrupt) << 24u) | (static_cast<std::uint32_t>(dataSelect) << 29u), static_cast<std::uint32_t>(guestAddress), static_cast<std::uint32_t>(guestAddress >> 32u), static_cast<std::uint32_t>(value), static_cast<std::uint32_t>(value >> 32u), interruptContextId}, __func__);
    static const std::uint64_t traced = [] { const char* value = std::getenv("APS5_TRACE_LABEL"); return value ? std::strtoull(value, nullptr, 16) : 0ull; }();
    if (traced != 0 && guestAddress == traced) std::fprintf(stderr, "[agc] RELEASE_MEM for 0x%llx recorded at %p"  "\n", static_cast<unsigned long long>(guestAddress), static_cast<void*>(packet));
    return packet;
}

}
