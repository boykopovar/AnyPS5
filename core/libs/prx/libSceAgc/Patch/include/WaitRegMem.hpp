#ifndef CORE_LIBS_PRX_LIBSCEAGC_PATCH_INCLUDE_WAITREGMEM_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_PATCH_INCLUDE_WAITREGMEM_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

int APS5_VABI sceAgcWaitRegMemPatchAddress(std::uint32_t* cmd, const volatile void* address);
int APS5_VABI sceAgcWaitRegMemPatchReference(std::uint32_t* cmd, std::uint32_t reference);
int APS5_VABI sceAgcWaitRegMemPatchMask(std::uint32_t* cmd, std::uint32_t mask);
int APS5_VABI sceAgcWaitRegMemPatchCompareFunction(std::uint32_t* cmd, std::uint8_t compareFunction);
int APS5_VABI sceAgcWriteDataPatchSetAddressOrOffset(std::uint32_t* cmd, std::uint64_t address);
int APS5_VABI sceAgcAsyncWriteDataPatchSetAddressOrOffset(std::uint32_t* cmd, std::uint64_t address);
int APS5_VABI sceAgcWriteDataPatchSetDst(std::uint32_t* cmd, std::uint8_t dst);
int APS5_VABI sceAgcAsyncWriteDataPatchSetDst(std::uint32_t* cmd, std::uint8_t dst);
int APS5_VABI sceAgcWriteDataPatchSetCachePolicy(std::uint32_t* cmd, std::uint8_t cachePolicy);
int APS5_VABI sceAgcAsyncWriteDataPatchSetCachePolicy(std::uint32_t* cmd, std::uint8_t cachePolicy);

}

#endif
