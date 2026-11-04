#ifndef CORE_LIBS_PRX_LIBSCEAGC_CB_INCLUDE_SYNC_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_CB_INCLUDE_SYNC_HPP

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {
std::uint32_t* APS5_VABI sceAgcCbMemSemaphore(CommandBuffer* buf, const volatile void* address, std::uint32_t operation, std::uint32_t modifier, std::uint32_t value);
}

#include "SceShaders.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcCbNop_nid_postfix(CommandBuffer* buf, std::uint32_t sizeInDwords);
uint32_t APS5_VABI sceAgcCbNopGetSize(uint32_t sizeInDwords);
std::uint32_t APS5_VABI sceAgcCbQueueEndOfPipeActionGetSize();
std::uint32_t* APS5_VABI sceAgcCbReleaseMem(CommandBuffer* buf, std::uint8_t action, std::uint16_t gcrControl, std::uint8_t dst, std::uint8_t cachePolicy, const volatile Label* address, std::uint8_t dataSelect, std::uint64_t data, std::uint16_t gdsOffset, std::uint16_t gdsSize, std::uint8_t interrupt, std::uint32_t interruptContextId);

}

#endif
