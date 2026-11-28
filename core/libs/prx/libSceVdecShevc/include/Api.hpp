#pragma once

#include "SceTypes.hpp"
#include "prx/libSceVdecSavc/include/Api.hpp"
#include <cstdint>

struct SdecHevcConfig {
    std::uint32_t magic;
    std::uint32_t defaultDepthMode;
    std::uint8_t levelIdc;
    std::uint8_t reserved9[3];
    std::uint32_t reservedC;
    std::uint32_t unknown10;
    std::uint32_t reserved14;
    std::uint32_t unknown18;
    std::int32_t frameCount;
    std::int32_t widthInMbs;
    std::int32_t heightInMbs;
    std::uint32_t extraWidthPixels;
    std::uint32_t extraHeightPixels;
    std::uint8_t depthLuma;
    std::uint8_t depthChroma;
    std::uint16_t reserved32;
    std::uint32_t unknown34;
};
static_assert(sizeof(SdecHevcConfig) == 0x38);

extern "C" {

int APS5_VABI sceSdecCreateSwHevc(const void* param);
int APS5_VABI sceSdecDestroySwHevc(void* handle);
int APS5_VABI sceSdecExecuteSwHevc(void* handle);
int APS5_VABI sceSdecGetFrameSwHevc(void* handle);
void APS5_VABI sceSdecGetVersionSwHevc(std::uint32_t* version);
int APS5_VABI sceSdecQueryMemorySwHevc(const SdecHevcConfig* config, const SdecRecord* record, std::int32_t mode, SdecMemory* memory, std::uint32_t* dpbFrames, std::uint8_t* tooBig);
int APS5_VABI sceSdecResetSwHevc(void* handle);

}
