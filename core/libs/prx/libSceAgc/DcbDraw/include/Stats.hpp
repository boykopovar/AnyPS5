#ifndef CORE_LIBS_PRX_LIBSCEAGC_DCBDRAW_INCLUDE_STATS_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_DCBDRAW_INCLUDE_STATS_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcDcbGetLodStats(CommandBuffer* buf, std::uint8_t cachePolicy, const volatile void* buffer, std::uint32_t bufferSizeInBytes, std::uint32_t resetCount, std::uint8_t forceReset, std::uint8_t reportAndReset, std::uint32_t reportingIntervalIn100kClocks);
std::uint32_t APS5_VABI sceAgcDcbBeginOcclusionQueryGetSize(std::int32_t queryType);
std::uint32_t APS5_VABI sceAgcDcbEndOcclusionQueryGetSize();

}

#endif
