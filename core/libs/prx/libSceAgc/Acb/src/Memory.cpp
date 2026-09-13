#include "prx/libSceAgc/Acb/include/Memory.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

uint32_t* APS5_VABI sceAgcAcbAcquireMem(CommandBuffer* buf, uint32_t gcr_cntl, const volatile void* base, uint64_t size_bytes, uint32_t poll_cycles) {
 (void)buf;
 (void)gcr_cntl;
 (void)base;
 (void)size_bytes;
 (void)poll_cycles;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t APS5_VABI sceAgcAcbAcquireMemGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* APS5_VABI sceAgcAcbCopyData(CommandBuffer* buf, uint8_t dst, uint8_t dst_cache_policy, uint64_t dst_address, uint8_t src, uint8_t src_cache_policy, uint64_t src_address_or_immediate, uint8_t item_size, uint8_t write_confirm) {
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

std::uint64_t APS5_VABI sceAgcAcbCopyDataGetSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint32_t* APS5_VABI sceAgcAcbDmaData(CommandBuffer* buf, std::uint8_t dst, std::uint8_t dstCachePolicy, std::uint64_t dstAddress, std::uint8_t src, std::uint8_t srcCachePolicy, std::uint64_t srcAddress, std::uint32_t numBytes, std::uint8_t waitForPrevious, std::uint8_t writeConfirm) {
    return Agc::Command::WriteDma(buf, true, 0, dst, dstCachePolicy, dstAddress, src, srcCachePolicy, srcAddress, numBytes, waitForPrevious, writeConfirm, 0, __func__);
}

std::uint32_t* APS5_VABI sceAgcAcbWriteData(CommandBuffer* buf, std::uint8_t dst, std::uint8_t cachePolicy, std::uint64_t address, const void* data, std::uint32_t numDwords, std::uint8_t increment, std::uint8_t writeConfirm) {
    return Agc::Command::WriteData(buf, true, dst, cachePolicy, address, data, numDwords, increment, writeConfirm, __func__);
}

}
