#include "prx/libSceAgc/Patch/include/CondExec.hpp"

#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

namespace {

constexpr int InvalidPacket = static_cast<int>(0x8a6c000cu);

bool IsCondExecPacket(const std::uint32_t* cmd) {
    return cmd != nullptr && ((cmd[0] >> 8u) & 0xffu) == 0x22u;
}

int SetCommandAddress(std::uint32_t* cmd, const volatile std::uint32_t* command) {
    if (!IsCondExecPacket(cmd)) {
        return InvalidPacket;
    }
    const auto address = reinterpret_cast<std::uintptr_t>(command);
    cmd[1] = (cmd[1] & 3u) | (static_cast<std::uint32_t>(address) & ~3u);
    cmd[2] = static_cast<std::uint32_t>(address >> 32u);
    return 0;
}

int SetEnd(std::uint32_t* cmd, const volatile std::uint32_t* buffer) {
    if (!IsCondExecPacket(cmd)) {
        return InvalidPacket;
    }
    const std::uint32_t byteDelta = static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(buffer) - reinterpret_cast<std::uintptr_t>(cmd));
    const std::uint32_t numDwords = ((byteDelta >> 2u) - 5u) & 0x3fffu;
    cmd[4] = (cmd[4] & ~0x3fffu) | numDwords;
    return 0;
}

}

extern "C" {

int APS5_VABI sceAgcCondExecPatchSetCommandAddress(uint32_t* cmd, const volatile uint32_t* command) {
    return SetCommandAddress(cmd, command);
}

int APS5_VABI sceAgcCondExecPatchSetEnd(uint32_t* cmd, const volatile uint32_t* buffer) {
    return SetEnd(cmd, buffer);
}

int APS5_VABI sceAgcAsyncCondExecPatchSetCommandAddress(std::uint32_t* cmd, const volatile std::uint32_t* command) {
    return SetCommandAddress(cmd, command);
}

int APS5_VABI sceAgcAsyncCondExecPatchSetEnd(std::uint32_t* cmd, const volatile std::uint32_t* buffer) {
    return SetEnd(cmd, buffer);
}

}
