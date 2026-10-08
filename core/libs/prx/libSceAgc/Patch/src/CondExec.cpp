#include "prx/libSceAgc/Patch/include/CondExec.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

int SetCommandAddress(std::uint32_t* cmd, const volatile std::uint32_t* command, const char* function) {
    Agc::Command::ValidatePacket(cmd, 0x22u, 5, function);
    const auto address = reinterpret_cast<std::uintptr_t>(command);
    Agc::Command::CheckAddress(address, 4, function);
    cmd[1] = (cmd[1] & 3u) | (static_cast<std::uint32_t>(address) & ~3u);
    cmd[2] = static_cast<std::uint32_t>(address >> 32u);
    return 0;
}

int SetEnd(std::uint32_t* cmd, const volatile std::uint32_t* buffer, const char* function) {
    Agc::Command::ValidatePacket(cmd, 0x22u, 5, function);
    const auto end = reinterpret_cast<std::uintptr_t>(buffer);
    const auto packetEnd = reinterpret_cast<std::uintptr_t>(cmd + 5);
    Agc::Command::CheckAddress(end, 4, function);
    const auto numDwords = (end - packetEnd) / sizeof(std::uint32_t);
    Agc::Command::Require(numDwords <= 0x3fffu, function, "conditional execution range is too large");
    cmd[4] = (cmd[4] & ~0x3fffu) | static_cast<std::uint32_t>(numDwords);
    return 0;
}

}

extern "C" {

int APS5_VABI sceAgcCondExecPatchSetCommandAddress(uint32_t* cmd, const volatile uint32_t* command) {
    return SetCommandAddress(cmd, command, __func__);
}

int APS5_VABI sceAgcCondExecPatchSetEnd(uint32_t* cmd, const volatile uint32_t* buffer) {
    return SetEnd(cmd, buffer, __func__);
}

int APS5_VABI sceAgcAsyncCondExecPatchSetCommandAddress(std::uint32_t* cmd, const volatile std::uint32_t* command) {
    return SetCommandAddress(cmd, command, __func__);
}

int APS5_VABI sceAgcAsyncCondExecPatchSetEnd(std::uint32_t* cmd, const volatile std::uint32_t* buffer) {
    return SetEnd(cmd, buffer, __func__);
}

}
