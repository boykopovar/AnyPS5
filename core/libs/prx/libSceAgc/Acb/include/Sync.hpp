#ifndef CORE_LIBS_PRX_LIBSCEAGC_ACB_INCLUDE_SYNC_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_ACB_INCLUDE_SYNC_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

uint32_t* APS5_VABI sceAgcAcbCondExec(CommandBuffer* buf, const volatile uint32_t* address, uint32_t numDwords);
uint32_t APS5_VABI sceAgcAcbCondExecGetSize(void);
std::uint32_t* APS5_VABI sceAgcAcbWaitRegMem(CommandBuffer* buf, std::uint8_t size, std::uint8_t compareFunction, std::uint8_t cachePolicy, const volatile void* address, std::uint64_t reference, std::uint64_t mask, std::uint32_t pollCycles);
std::uint64_t APS5_VABI sceAgcAcbWaitOnAddressGetSize(std::uint32_t size);
std::uint32_t* APS5_VABI sceAgcAcbEventWrite(CommandBuffer* buf, std::uint8_t eventType, const volatile void* address);
std::uint64_t APS5_VABI sceAgcAcbEventWriteGetSize(std::uint8_t eventType);
std::uint32_t APS5_VABI sceAgcAcbQueueEndOfShaderActionGetSize();
std::uint32_t* APS5_VABI sceAgcAcbMemSemaphore(CommandBuffer* buf, const volatile void* address, std::uint8_t waitForMailbox, std::uint8_t signalType, std::uint8_t operation);

}

#endif
