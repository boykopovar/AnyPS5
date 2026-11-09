#ifndef CORE_LIBS_PRX_LIBSCEAGC_DCBFLOW_INCLUDE_SYNC_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_DCBFLOW_INCLUDE_SYNC_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

uint32_t* APS5_VABI sceAgcDcbCondExec(CommandBuffer* buf, const volatile uint32_t* address, uint32_t numDwords);
uint32_t APS5_VABI sceAgcDcbCondExecGetSize(void);
std::uint32_t* APS5_VABI sceAgcDcbWaitRegMem(CommandBuffer* buf, std::uint8_t size, std::uint8_t compareFunction, std::uint8_t operation, std::uint8_t cachePolicy, const volatile void* address, std::uint64_t reference, std::uint64_t mask, std::uint32_t pollCycles);
uint32_t APS5_VABI sceAgcDcbWaitOnAddressGetSize(uint32_t size);
std::uint32_t* APS5_VABI sceAgcDcbEventWrite(CommandBuffer* buf, std::uint8_t eventType, const volatile void* address);
std::uint64_t APS5_VABI sceAgcDcbEventWriteGetSize(std::uint8_t eventType);
std::uint32_t* APS5_VABI sceAgcDcbStallCommandBufferParser(CommandBuffer* buf);
std::uint32_t APS5_VABI sceAgcDcbStallCommandBufferParserGetSize();
std::uint32_t* APS5_VABI sceAgcDcbMemSemaphore(CommandBuffer* buf, const volatile void* address, std::uint8_t waitForMailbox, std::uint8_t signalType, std::uint8_t operation);

}

#endif
