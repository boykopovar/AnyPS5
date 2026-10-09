#include "prx/libSceAgc/Misc/include/PacketInfo.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

uint32_t APS5_VABI sceAgcGetPacketSize(uint32_t* packet) {
    Agc::Command::CheckAddress(reinterpret_cast<std::uintptr_t>(packet), alignof(uint32_t), __func__);
    Agc::Command::Require((packet[0] & 0xc0000000u) == 0xc0000000u, __func__, "source pointer is not a PM4 packet");
    if ((packet[0] & 0x3fffff00u) == 0x3fff1000u) {
        return 1;
    }
    return ((packet[0] >> 16u) & 0x3fffu) + 2u;
}

int APS5_VABI sceAgcGetDataPacketPayloadAddress_0090(uint32_t** addr, uint32_t* cmd, int type) {
    Agc::Command::CheckAddress(reinterpret_cast<std::uintptr_t>(addr), alignof(uint32_t*), __func__);
    Agc::Command::CheckAddress(reinterpret_cast<std::uintptr_t>(cmd), alignof(uint32_t), __func__);
    if (type != 0) {
        *addr = cmd + 2;
    } else {
        *addr = (~cmd[0] & 0x3fff0000u) != 0 ? cmd + 1 : nullptr;
    }
    return 0;
}

int APS5_VABI sceAgcGetDataPacketPayloadRange(SceAgcMemoryRange* range, uint32_t* cmd, int type) {
    Agc::Command::CheckAddress(reinterpret_cast<std::uintptr_t>(range), alignof(SceAgcMemoryRange), __func__);
    Agc::Command::CheckAddress(reinterpret_cast<std::uintptr_t>(cmd), alignof(uint32_t), __func__);
    const uint64_t count_bytes = (cmd[0] >> 14u) & 0xfffcu;
    if (type != 0) {
        range->base = cmd + 2;
        range->size = count_bytes;
    } else if ((~cmd[0] & 0x3fff0000u) == 0) {
        range->base = nullptr;
        range->size = 0;
    } else {
        range->base = cmd + 1;
        range->size = count_bytes + sizeof(uint32_t);
    }
    return 0;
}

int APS5_VABI sceAgcDcbGetSystemSoftwareVersion(uint32_t* destination, const uint32_t* packet) {
    Agc::Command::CheckAddress(reinterpret_cast<std::uintptr_t>(destination), alignof(uint32_t), __func__);
    Agc::Command::CheckAddress(reinterpret_cast<std::uintptr_t>(packet), alignof(uint32_t), __func__);
    Agc::Command::Require((packet[0] & 0xc0000000u) == 0xc0000000u, __func__, "source pointer is not a PM4 packet");
    const auto size = ((packet[0] >> 16u) & 0x3fffu) + 2u;
    Agc::Command::Require(size >= 3u, __func__, "PM4 packet is too short to carry a software version");
    *destination = packet[1];
    return 0;
}

}
