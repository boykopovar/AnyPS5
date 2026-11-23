#pragma once

#include "SceTypes.hpp"
#include "prx/libSceVdecSavc/include/Api.hpp"
#include <cstdint>

struct SdecConfig2 {
    std::uint32_t reserved0;
    std::uint32_t unknown4;
    std::uint32_t level;
    std::uint32_t reservedC;
    std::uint32_t unknown10;
    std::uint32_t reserved14;
    std::uint32_t unknown18;
    std::int32_t frameCount;
    std::int32_t widthInMbs;
    std::int32_t heightInMbs;
    std::uint32_t unknown28;
    std::uint32_t unknown2C;
    std::uint8_t depthLuma;
    std::uint8_t depthChroma;
    std::uint16_t reserved32;
    std::uint32_t fieldMode;
};
static_assert(sizeof(SdecConfig2) == 0x38);

struct SdecRecord2 {
    std::uint32_t reserved0;
    std::uint32_t instanceMode;
    std::uint32_t instanceCount;
    std::int32_t extraFrames;
    std::uint64_t unknown10;
    std::int32_t unknown18;
    std::uint32_t reserved1C;
    std::uint64_t unknown20;
};
static_assert(sizeof(SdecRecord2) == 0x28);

extern "C" {

int APS5_VABI sceSdecCreateSw2(const SdecConfig2* config, const void* attributes, const SdecRecord2* record, const SdecMemory* memory, void** handle);
int APS5_VABI sceSdecDestroySw2(void* decoder);
void APS5_VABI sceSdecGetVersionSw2(std::uint32_t* version);
int APS5_VABI sceSdecQueryMemorySw2(const SdecConfig2* config, const SdecRecord* record, std::int32_t mode, SdecMemory* memory, std::uint32_t* dpbFrames, std::int32_t flag, std::uint8_t* tooBig);

}
