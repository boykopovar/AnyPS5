#ifndef CORE_LIBS_PRX_LIBSCEAGC_DCBDRAW_INCLUDE_DRAWINDEXED_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_DCBDRAW_INCLUDE_DRAWINDEXED_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcDcbDrawIndex(CommandBuffer* buf, std::uint32_t indexCount, const volatile void* indexAddress, std::uint64_t modifier);
std::uint32_t APS5_VABI sceAgcDcbDrawIndexGetSize();
std::uint32_t* APS5_VABI sceAgcDcbDrawIndexAuto(CommandBuffer* buf, std::uint32_t indexCount, std::uint64_t modifier);
std::uint32_t APS5_VABI sceAgcDcbDrawIndexAutoGetSize();
std::uint32_t* APS5_VABI sceAgcDcbDrawIndexOffset(CommandBuffer* buf, std::uint32_t indexOffset, std::uint32_t indexCount, std::uint64_t modifier);
uint32_t APS5_VABI sceAgcDcbDrawIndexOffsetGetSize(void);
uint32_t* APS5_VABI sceAgcDcbDrawIndexIndirect(CommandBuffer* buf, uint32_t dataOffsetInBytes, uint64_t modifier);
std::uint32_t APS5_VABI sceAgcDcbDrawIndexIndirectGetSize();
uint32_t* APS5_VABI sceAgcDcbDrawIndexIndirectMulti(CommandBuffer* buf, uint32_t dataOffsetInBytes, uint32_t countIndirect, uint32_t maxCountOrCount, const volatile void* countAddr, uint32_t strideInBytes, uint64_t modifier);
std::uint32_t APS5_VABI sceAgcDcbDrawIndexIndirectMultiGetSize();
uint32_t* APS5_VABI sceAgcDcbDrawIndexMultiInstanced(CommandBuffer* buf, uint32_t indexCount, const volatile void* indexAddr, const volatile void* objectIds, uint32_t instanceCount, uint64_t modifier);
uint32_t APS5_VABI sceAgcDcbDrawIndexMultiInstancedGetSize(void);

}

#endif
