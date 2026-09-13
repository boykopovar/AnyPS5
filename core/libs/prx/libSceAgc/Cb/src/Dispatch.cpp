#include "prx/libSceAgc/Cb/include/Dispatch.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcCbDispatch(CommandBuffer* buf, std::uint32_t threadGroupX, std::uint32_t threadGroupY, std::uint32_t threadGroupZ, std::uint32_t modifier) {
    Agc::Command::CheckBits(modifier, 0xa038u, __func__);
    return Agc::Command::Emit(buf, 0x15u, {threadGroupX, threadGroupY, threadGroupZ, modifier | 0x41u}, __func__);
}

uint32_t APS5_VABI sceAgcCbDispatchGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
