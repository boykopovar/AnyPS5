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

#include "SceShaders.hpp"

extern "C" {

uint32_t APS5_VABI sceAgcGetPacketSize(uint32_t* packet);
int APS5_VABI sceAgcGetDataPacketPayloadRange(SceAgcMemoryRange* range, uint32_t* cmd, int type);
int APS5_VABI sceAgcDcbGetSystemSoftwareVersion(uint32_t* destination, const uint32_t* packet);

}

#endif
