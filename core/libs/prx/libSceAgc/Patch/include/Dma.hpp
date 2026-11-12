#ifndef CORE_LIBS_PRX_LIBSCEAGC_PATCH_INCLUDE_DMA_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_PATCH_INCLUDE_DMA_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

int APS5_VABI sceAgcDmaDataPatchSetDstAddressOrOffset(std::uint32_t* cmd, std::uint64_t address);
int APS5_VABI sceAgcDmaDataPatchSetSrcAddressOrOffsetOrImmediate(std::uint32_t* cmd, std::uint64_t address);

}

#endif
