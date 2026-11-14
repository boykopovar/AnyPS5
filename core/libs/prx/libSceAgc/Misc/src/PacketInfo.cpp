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
    uint32_t* payload = nullptr;
    if (type != 0) {
        payload = cmd + 2;
    } else {
        payload = (~cmd[0] & 0x3fff0000u) != 0 ? cmd + 1 : nullptr;
    }
    Agc::Command::Require(GuestCopyOut_nid_no_patch(addr, &payload, sizeof(payload)), __func__, "destination pointer is not writable");
    return 0;
}

int APS5_VABI sceAgcGetDataPacketPayloadAddress(uint32_t** addr, uint32_t* cmd, int type) {
    return sceAgcGetDataPacketPayloadAddress_0090(addr, cmd, type);
}

int APS5_VABI sceAgcGetDataPacketPayloadRange(SceAgcMemoryRange* range, uint32_t* cmd, int type) {
    Agc::Command::CheckAddress(reinterpret_cast<std::uintptr_t>(range), alignof(SceAgcMemoryRange), __func__);
    Agc::Command::CheckAddress(reinterpret_cast<std::uintptr_t>(cmd), alignof(uint32_t), __func__);
    const uint64_t count_bytes = (cmd[0] >> 14u) & 0xfffcu;
    SceAgcMemoryRange result{};
    if (type != 0) {
        result.base = cmd + 2;
        result.size = count_bytes;
    } else if ((~cmd[0] & 0x3fff0000u) == 0) {
        result.base = nullptr;
        result.size = 0;
    } else {
        result.base = cmd + 1;
        result.size = count_bytes + sizeof(uint32_t);
    }
    Agc::Command::Require(GuestCopyOut_nid_no_patch(range, &result, sizeof(result)), __func__, "destination pointer is not writable");
    return 0;
}

int APS5_VABI sceAgcDcbGetSystemSoftwareVersion(uint32_t* destination, const uint32_t* packet) {
    Agc::Command::CheckAddress(reinterpret_cast<std::uintptr_t>(destination), alignof(uint32_t), __func__);
    Agc::Command::CheckAddress(reinterpret_cast<std::uintptr_t>(packet), alignof(uint32_t), __func__);
    Agc::Command::Require((packet[0] & 0xc0000000u) == 0xc0000000u, __func__, "source pointer is not a PM4 packet");
    const auto size = ((packet[0] >> 16u) & 0x3fffu) + 2u;
    Agc::Command::Require(size >= 3u, __func__, "PM4 packet is too short to carry a software version");
    const auto version = packet[1];
    Agc::Command::Require(GuestCopyOut_nid_no_patch(destination, &version, sizeof(version)), __func__, "destination pointer is not writable");
    return 0;
}

}
