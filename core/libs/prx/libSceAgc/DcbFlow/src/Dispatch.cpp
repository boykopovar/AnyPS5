#include "prx/libSceAgc/DcbFlow/include/Dispatch.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

uint32_t* APS5_VABI sceAgcDcbDispatchIndirect(CommandBuffer* buf, uint32_t data_offset_in_bytes, uint32_t flags) {
    // Graphics queues read the arguments at an offset from the base set by SetBaseDispatchIndirectArgs.
    Agc::Command::CheckBits(flags, 0xa038u | 0x41u, __func__);
    return Agc::Command::Emit(buf, 0x16u, {data_offset_in_bytes, flags | 0x41u}, __func__);
}

std::uint32_t APS5_VABI sceAgcDcbDispatchIndirectGetSize() {
    return 12;
}

}
