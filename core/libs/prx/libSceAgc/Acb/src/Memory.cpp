#include "prx/libSceAgc/Acb/include/Memory.hpp"
#include "prx/libSceAgc/DcbFlow/include/Memory.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcAcbAcquireMem(CommandBuffer* buf, std::uint32_t gcrControl, const volatile void* base, std::uint64_t sizeBytes, std::uint32_t pollCycles) {
    return sceAgcDcbAcquireMem(buf, 1, 0, gcrControl, base, sizeBytes, pollCycles);
}

uint32_t APS5_VABI sceAgcAcbAcquireMemGetSize(void) {
    return 32; // ACQUIRE_MEM: header + 7 dwords
}

uint32_t* APS5_VABI sceAgcAcbCopyData(CommandBuffer* buf, uint8_t dst, uint8_t dst_cache_policy, uint64_t dst_address, uint8_t src, uint8_t src_cache_policy, uint64_t src_address_or_immediate, uint8_t item_size, uint8_t write_confirm) {
    return Agc::Command::WriteCopyData(buf, true, dst, dst_cache_policy, dst_address, src, src_cache_policy, src_address_or_immediate, item_size, write_confirm, __func__);
}

std::uint64_t APS5_VABI sceAgcAcbCopyDataGetSize() {
    return 24; // COPY_DATA: header + 5 dwords
}

std::uint32_t* APS5_VABI sceAgcAcbDmaData(CommandBuffer* buf, std::uint8_t dst, std::uint8_t dstCachePolicy, std::uint64_t dstAddress, std::uint8_t src, std::uint8_t srcCachePolicy, std::uint64_t srcAddress, std::uint32_t numBytes, std::uint8_t waitForPrevious, std::uint8_t writeConfirm) {
    return Agc::Command::WriteDma(buf, true, 0, dst, dstCachePolicy, dstAddress, src, srcCachePolicy, srcAddress, numBytes, waitForPrevious, writeConfirm, 0, __func__);
}

std::uint32_t APS5_VABI sceAgcAcbDmaDataGetSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint32_t* APS5_VABI sceAgcAcbAtomicMem(CommandBuffer* buf, std::uint8_t atomicOp, std::uint8_t command, std::uint8_t cachePolicy, const volatile void* address, std::uint64_t srcData, std::uint64_t compareData, std::uint16_t loopInterval) {
    return Agc::Command::WriteAtomicMem(buf, atomicOp, command, cachePolicy, address, srcData, compareData, loopInterval, __func__);
}

std::uint32_t APS5_VABI sceAgcAcbAtomicMemGetSize() {
    return 36; // ATOMIC_MEM: header + 8 dwords
}

std::uint32_t APS5_VABI sceAgcAcbAtomicGdsGetSize() {
    return 36; // upper bound: ATOMIC_GDS is not emitted by this port (header + 8 dwords reserved)
}

std::uint32_t* APS5_VABI sceAgcAcbPrimeUtcl2(CommandBuffer* buf, const volatile void* address, std::uint32_t sizeInBytes) {
    // Page-table prefetch hint: a NOP of the reserved size that keeps the request readable in dumps.
    const auto guestAddress = reinterpret_cast<std::uintptr_t>(address);
    auto* packet = Agc::Command::Allocate(buf, 5, __func__);
    packet[0] = Agc::Command::Header(0x10u, 5);
    packet[1] = 0;
    packet[2] = static_cast<std::uint32_t>(guestAddress);
    packet[3] = static_cast<std::uint32_t>(guestAddress >> 32u);
    packet[4] = sizeInBytes;
    return packet;
}

std::uint32_t APS5_VABI sceAgcAcbPrimeUtcl2GetSize() {
    return 20;
}

std::uint32_t* APS5_VABI sceAgcAcbWriteData(CommandBuffer* buf, std::uint8_t dst, std::uint8_t cachePolicy, std::uint64_t address, const void* data, std::uint32_t numDwords, std::uint8_t increment, std::uint8_t writeConfirm) {
    return Agc::Command::WriteData(buf, true, dst, cachePolicy, address, data, numDwords, increment, writeConfirm, __func__);
}

}
