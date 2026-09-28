#include "prx/libSceAgc/Misc/include/PacketInfo.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"

#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

uint32_t APS5_VABI sceAgcGetPacketSize(uint32_t* packet) {
 (void)packet;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

APS5_EXPORT("V++UgBtQhn0", sceAgcGetDataPacketPayloadAddressUnk);
int APS5_VABI sceAgcGetDataPacketPayloadAddressUnk(uint32_t** addr, uint32_t* cmd, int type) {
    Agc::Command::CheckAddress(reinterpret_cast<std::uintptr_t>(addr), alignof(uint32_t*), __func__);
    Agc::Command::CheckAddress(reinterpret_cast<std::uintptr_t>(cmd), alignof(uint32_t), __func__);
    if (type != 0) {
        *addr = cmd + 2;
    } else {
        *addr = (~cmd[0] & 0x3fff0000u) != 0 ? cmd + 1 : nullptr;
    }
    return 0;
}

// SDK 9 Dcb ABI: (destination, packet). Reads the AGC system-software version stamped into the
// first payload dword of an AGC data packet and copies it to the caller's destination.
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
