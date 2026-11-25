#pragma once

#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libSceVdecCore/include/Api.hpp"

struct VdecCoreSvpInputMeta {
    std::int64_t pts;
    std::int64_t dts;
    std::uint64_t userData;
    std::uint32_t flags;
    std::uint32_t streamId;
};

static_assert(sizeof(VdecCoreSvpInputMeta) == 0x20);

struct VdecCoreSvpInputBuffer {
    std::uint64_t reserved0[2];
    const void* auData;
    std::uint64_t auSize;
    std::uint64_t reserved1[4];
};

static_assert(sizeof(VdecCoreSvpInputBuffer) == 0x40);
static_assert(offsetof(VdecCoreSvpInputBuffer, auData) == 0x10);
static_assert(offsetof(VdecCoreSvpInputBuffer, auSize) == 0x18);

extern "C" {

int APS5_VABI sceVdecCoreCreateDecoderBid(std::int32_t bid, const VdecCoreConfigInfo* config, const VdecCoreMemoryInfo* memory, void** decoder);
int APS5_VABI sceVdecCoreCreateDecoderSvp(std::int32_t bid, const VdecCoreConfigInfo* config, const VdecCoreMemoryInfo* memory, void** decoder);
int APS5_VABI sceVdecCoreMapMemory(void* decoder, void* address, std::uint64_t size);
int APS5_VABI sceVdecCoreSetDecodeInputSvp(void* decoder, const VdecCoreSvpInputMeta* meta, const VdecCoreSvpInputBuffer* svpBuffer, std::uint32_t* status);

}
