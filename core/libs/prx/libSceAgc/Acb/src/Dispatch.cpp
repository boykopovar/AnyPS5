#include "prx/libSceAgc/Acb/include/Dispatch.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

uint32_t* APS5_VABI sceAgcAcbDispatchIndirect(CommandBuffer* buf, const volatile void* indirect_args, uint32_t modifier) {
    // Compute queues take the argument address directly (DISPATCH_INDIRECT with a 64-bit address).
    const auto address = reinterpret_cast<std::uintptr_t>(indirect_args);
    Agc::Command::CheckAddress(address, 4, __func__);
    Agc::Command::CheckBits(modifier, 0xa038u | 0x41u, __func__);
    return Agc::Command::Emit(buf, 0x16u, {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), modifier | 0x41u}, __func__);
}

std::uint32_t APS5_VABI sceAgcAcbDispatchIndirectGetSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
