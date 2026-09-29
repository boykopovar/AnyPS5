#include "prx/libSceAgc/Patch/include/Rewind.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <atomic>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

// The valid bit (31) releases a queue stalled on the REWIND; the release store publishes the commands
// the title wrote behind it before setting the bit.
int SetRewindState(std::uint32_t* cmd, std::uint8_t state, const char* function) {
    Agc::Command::ValidatePacket(cmd, 0x59u, 2, function);
    Agc::Command::CheckBits(state, 1, function);
    std::atomic_ref<std::uint32_t>(cmd[1]).store(static_cast<std::uint32_t>(state) << 31u, std::memory_order_release);
    return 0;
}

}

extern "C" {

int APS5_VABI sceAgcRewindPatchSetRewindState(uint32_t* cmd, uint8_t state) {
    return SetRewindState(cmd, state, __func__);
}

int APS5_VABI sceAgcAsyncRewindPatchSetRewindState(std::uint32_t* cmd, std::uint8_t state) {
    return SetRewindState(cmd, state, __func__);
}

}
