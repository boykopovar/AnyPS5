#ifndef CORE_LIBS_PRX_LIBSCEAGC_DCBFLOW_INCLUDE_MEMORY_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_DCBFLOW_INCLUDE_MEMORY_HPP

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {
std::uint32_t* APS5_VABI sceAgcDcbAcquireMem(CommandBuffer* buf, std::uint8_t engine, std::uint32_t cbDbOp, std::uint32_t gcrControl, const volatile void* base, std::uint64_t sizeBytes, std::uint32_t pollCycles);
std::uint32_t APS5_VABI sceAgcDcbAtomicGdsGetSize();
}

#include "SceShaders.hpp"

extern "C" {

uint32_t APS5_VABI sceAgcDcbAcquireMemGetSize(void);
uint32_t* APS5_VABI sceAgcDcbCopyData(CommandBuffer* buf, uint8_t dst, uint8_t dstCachePolicy, uint64_t dstAddress, uint8_t src, uint8_t srcCachePolicy, uint64_t srcAddressOrImmediate, uint8_t itemSize, uint8_t writeConfirm);
std::uint64_t APS5_VABI sceAgcDcbCopyDataGetSize();
std::uint32_t* APS5_VABI sceAgcDcbDmaData(CommandBuffer* buf, std::uint8_t engine, std::uint8_t dst, std::uint8_t dstCachePolicy, std::uint64_t dstAddress, std::uint8_t src, std::uint8_t srcCachePolicy, std::uint64_t srcAddress, std::uint32_t numBytes, std::uint8_t waitForPrevious, std::uint8_t writeConfirm, std::uint8_t blockEngine);
std::uint32_t APS5_VABI sceAgcDcbDmaDataGetSize();
std::uint32_t* APS5_VABI sceAgcDcbAtomicMem(CommandBuffer* buf, std::uint8_t atomicOp, std::uint8_t command, std::uint8_t cachePolicy, const volatile void* address, std::uint64_t srcData, std::uint64_t compareData, std::uint16_t loopInterval);
std::uint32_t APS5_VABI sceAgcDcbAtomicMemGetSize();
std::uint32_t* APS5_VABI sceAgcDcbWriteData(CommandBuffer* buf, std::uint8_t dst, std::uint8_t cachePolicy, std::uint64_t address, const void* data, std::uint32_t numDwords, std::uint8_t increment, std::uint8_t writeConfirm);
uint32_t APS5_VABI sceAgcDcbWriteDataGetSize(uint32_t numDwords);

}

#endif
