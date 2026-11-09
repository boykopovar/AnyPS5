#ifndef CORE_LIBS_PRX_LIBSCEAGC_ACB_INCLUDE_MEMORY_HPP
#define CORE_LIBS_PRX_LIBSCEAGC_ACB_INCLUDE_MEMORY_HPP

#include "SceTypes.hpp"
#include "SceShaders.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcAcbAcquireMem(CommandBuffer* buf, std::uint32_t gcrControl, const volatile void* base, std::uint64_t sizeBytes, std::uint32_t pollCycles);
uint32_t APS5_VABI sceAgcAcbAcquireMemGetSize(void);
uint32_t* APS5_VABI sceAgcAcbCopyData(CommandBuffer* buf, uint8_t dst, uint8_t dstCachePolicy, uint64_t dstAddress, uint8_t src, uint8_t srcCachePolicy, uint64_t srcAddressOrImmediate, uint8_t itemSize, uint8_t writeConfirm);
std::uint64_t APS5_VABI sceAgcAcbCopyDataGetSize();
std::uint32_t* APS5_VABI sceAgcAcbDmaData(CommandBuffer* buf, std::uint8_t dst, std::uint8_t dstCachePolicy, std::uint64_t dstAddress, std::uint8_t src, std::uint8_t srcCachePolicy, std::uint64_t srcAddress, std::uint32_t numBytes, std::uint8_t waitForPrevious, std::uint8_t writeConfirm);
std::uint32_t APS5_VABI sceAgcAcbDmaDataGetSize();
std::uint32_t* APS5_VABI sceAgcAcbAtomicMem(CommandBuffer* buf, std::uint8_t atomicOp, std::uint8_t command, std::uint8_t cachePolicy, const volatile void* address, std::uint64_t srcData, std::uint64_t compareData, std::uint16_t loopInterval);
std::uint32_t APS5_VABI sceAgcAcbAtomicMemGetSize();
std::uint32_t APS5_VABI sceAgcAcbAtomicGdsGetSize();
std::uint32_t* APS5_VABI sceAgcAcbPrimeUtcl2(CommandBuffer* buf, const volatile void* address, std::uint32_t sizeInBytes);
std::uint32_t APS5_VABI sceAgcAcbPrimeUtcl2GetSize();
std::uint32_t* APS5_VABI sceAgcAcbWriteData(CommandBuffer* buf, std::uint8_t dst, std::uint8_t cachePolicy, std::uint64_t address, const void* data, std::uint32_t numDwords, std::uint8_t increment, std::uint8_t writeConfirm);

}

#endif
