#include "prx/libSceAgc/Patch/include/Nop.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcSetNop(CommandBuffer* buf, std::uint32_t sizeDw) {
    return Agc::Command::WriteNop(buf, sizeDw, __func__);
}

}
