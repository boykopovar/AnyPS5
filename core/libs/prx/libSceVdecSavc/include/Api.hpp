#pragma once

#include "SceTypes.hpp"
#include <cstdint>

struct SdecConfig {
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
};
static_assert(sizeof(SdecConfig) == 0x28);

struct SdecRecord {
    std::uint32_t reserved0;
    std::uint32_t instanceMode;
    std::uint32_t unknown8;
    std::int32_t extraFrames;
};
static_assert(sizeof(SdecRecord) == 0x10);

struct SdecMemoryBlock {
    std::uint64_t base;
    std::uint64_t size;
    std::uint32_t alignment;
    std::uint32_t reserved;
};

struct SdecMemory {
    SdecMemoryBlock blocks[3];
};
static_assert(sizeof(SdecMemory) == 0x48);

extern "C" {

int APS5_VABI sceSdecCreateSw(const SdecConfig* config, const void* attributes, const SdecRecord* record, const SdecMemory* memory, void** handle);
int APS5_VABI sceSdecDestroySw(void* decoder);
void APS5_VABI sceSdecGetVersionSw(std::uint32_t* version);
int APS5_VABI sceSdecQueryMemorySw(const SdecConfig* config, const SdecRecord* record, SdecMemory* memory, std::uint32_t* dpbFrames);

}
