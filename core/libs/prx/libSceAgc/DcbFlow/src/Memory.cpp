#include "prx/libSceAgc/DcbFlow/include/Memory.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcDcbAcquireMem(CommandBuffer* buf, std::uint8_t engine, std::uint32_t cbDbOp, std::uint32_t gcrControl, const volatile void* base, std::uint64_t sizeBytes, std::uint32_t pollCycles) {
    Agc::Command::CheckBits(engine, 1, __func__);
    Agc::Command::CheckBits(cbDbOp, 0x7fffffffu, __func__);
    Agc::Command::CheckBits(gcrControl, 0x7ffffu, __func__);
    // The range only scopes cache maintenance, so round it outward to the 256-byte granularity the
    // packet encodes instead of rejecting unaligned ranges.
    const auto requested = reinterpret_cast<std::uintptr_t>(base);
    auto address = requested & ~static_cast<std::uintptr_t>(0xffu);
    const auto wholeAddressSpace = sizeBytes == 0xffffffffffffffffull || (address >> 40u) != 0 || sizeBytes > (1ull << 40u);
    if (wholeAddressSpace) address = 0;
    else sizeBytes = (sizeBytes + (requested - address) + 0xffu) & ~0xffull;
    Agc::Command::Require(pollCycles / 40u <= 0xffffu, __func__, "acquire poll interval overflow");
    return Agc::Command::Emit(buf, 0x58u, {(static_cast<std::uint32_t>(engine) << 31u) | cbDbOp, wholeAddressSpace ? 0u : static_cast<std::uint32_t>(sizeBytes >> 8u), 0, static_cast<std::uint32_t>(address >> 8u), 0, pollCycles / 40u, gcrControl}, __func__);
}

uint32_t APS5_VABI sceAgcDcbAcquireMemGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* APS5_VABI sceAgcDcbCopyData(CommandBuffer* buf, uint8_t dst, uint8_t dst_cache_policy, uint64_t dst_address, uint8_t src, uint8_t src_cache_policy, uint64_t src_address_or_immediate, uint8_t item_size, uint8_t write_confirm) {
 (void)buf;
 (void)dst;
 (void)dst_cache_policy;
 (void)dst_address;
 (void)src;
 (void)src_cache_policy;
 (void)src_address_or_immediate;
 (void)item_size;
 (void)write_confirm;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

std::uint64_t APS5_VABI sceAgcDcbCopyDataGetSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint32_t* APS5_VABI sceAgcDcbDmaData(CommandBuffer* buf, std::uint8_t engine, std::uint8_t dst, std::uint8_t dstCachePolicy, std::uint64_t dstAddress, std::uint8_t src, std::uint8_t srcCachePolicy, std::uint64_t srcAddress, std::uint32_t numBytes, std::uint8_t waitForPrevious, std::uint8_t writeConfirm, std::uint8_t blockEngine) {
    return Agc::Command::WriteDma(buf, false, engine, dst, dstCachePolicy, dstAddress, src, srcCachePolicy, srcAddress, numBytes, waitForPrevious, writeConfirm, blockEngine, __func__);
}

std::uint32_t APS5_VABI sceAgcDcbDmaDataGetSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint32_t* APS5_VABI sceAgcDcbAtomicMem(CommandBuffer* buf, std::uint8_t atomicOp, std::uint8_t command, std::uint8_t cachePolicy, const volatile void* address, std::uint64_t srcData, std::uint64_t compareData, std::uint16_t loopInterval) {
    (void)buf;
    (void)atomicOp;
    (void)command;
    (void)cachePolicy;
    (void)address;
    (void)srcData;
    (void)compareData;
    (void)loopInterval;
    NotImplemented_nid_no_patch(__func__);
    return nullptr;
}

std::uint32_t APS5_VABI sceAgcDcbAtomicMemGetSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint32_t APS5_VABI sceAgcDcbAtomicGdsGetSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint32_t* APS5_VABI sceAgcDcbWriteData(CommandBuffer* buf, std::uint8_t dst, std::uint8_t cachePolicy, std::uint64_t address, const void* data, std::uint32_t numDwords, std::uint8_t increment, std::uint8_t writeConfirm) {
    return Agc::Command::WriteData(buf, false, dst, cachePolicy, address, data, numDwords, increment, writeConfirm, __func__);
}

uint32_t APS5_VABI sceAgcDcbWriteDataGetSize(uint32_t num_dwords) {
 (void)num_dwords;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
