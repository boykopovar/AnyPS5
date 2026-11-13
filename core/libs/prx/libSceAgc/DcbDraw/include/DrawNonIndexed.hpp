#ifndef CORE_LIBS_PRX_LIBSCEAGC_DCBDRAW_INCLUDE_DRAWNONINDEXED_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_DCBDRAW_INCLUDE_DRAWNONINDEXED_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcDcbDrawIndirect(CommandBuffer* buf, std::uint32_t dataOffsetInBytes, std::uint64_t modifier);
std::uint32_t APS5_VABI sceAgcDcbDrawIndirectGetSize();
std::uint32_t* APS5_VABI sceAgcDcbDrawIndirectMulti(CommandBuffer* buf, std::uint32_t dataOffsetInBytes, std::uint32_t countIndirect, std::uint32_t maxCountOrCount, const volatile void* countAddress, std::uint32_t strideInBytes, std::uint64_t modifier);
std::uint32_t APS5_VABI sceAgcDcbDrawIndirectMultiGetSize();

}

#endif
