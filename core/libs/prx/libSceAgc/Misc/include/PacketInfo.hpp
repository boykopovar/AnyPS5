#ifndef CORE_LIBS_PRX_LIBSCEAGC_MISC_INCLUDE_PACKETINFO_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_MISC_INCLUDE_PACKETINFO_HPP

#include "SceTypes.hpp"
#include <cstdint>

struct SceAgcMemoryRange {
    void* base;
    uint64_t size;
};

extern "C" {
int APS5_VABI sceAgcGetDataPacketPayloadAddress(uint32_t** addr, uint32_t* cmd, int type);
int APS5_VABI sceAgcGetDataPacketPayloadAddress_0090(uint32_t** addr, uint32_t* cmd, int type);
}

#endif
