#include "prx/libSceAgc/Patch/include/WaitRegMem.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAgcWaitRegMemPatchAddress(std::uint32_t* cmd, const volatile void* address) {
    auto* wait = Agc::Command::ValidateWait(cmd, __func__);
    const auto guestAddress = reinterpret_cast<std::uintptr_t>(address);
    const auto alignment = ((wait[0] >> 8u) & 0xffu) == 0x3cu ? 4u : 8u;
    Agc::Command::CheckAddress(guestAddress, alignment, __func__);
    Agc::Command::CheckBits(guestAddress, 0xffffffffffffull, __func__);
    cmd[2] = (cmd[2] & 0xffff0000u) | static_cast<std::uint32_t>(guestAddress >> 32u);
    cmd[3] = static_cast<std::uint32_t>(guestAddress);
    wait[2] = static_cast<std::uint32_t>(guestAddress);
    wait[3] = static_cast<std::uint32_t>(guestAddress >> 32u);
    return 0;
}

int APS5_VABI sceAgcWaitRegMemPatchReference(std::uint32_t* cmd, std::uint64_t reference) {
    auto* wait = Agc::Command::ValidateWait(cmd, __func__);
    Agc::Command::CheckBits(reference, 0xffffffffu, __func__);
    wait[4] = static_cast<std::uint32_t>(reference);
    return 0;
}

}
