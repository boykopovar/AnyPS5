#include "prx/libSceAgc/DcbFlow/include/Memory.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "shader/recompilier/Recompiler.hpp"

extern "C" {

uint32_t* APS5_VABI sceAgcDcbAcquireMem(CommandBuffer* buf, uint8_t engine, uint32_t cb_db_op, uint32_t gcr_cntl, const volatile void* base, uint64_t size_bytes, uint32_t poll_cycles) {
 (void)buf;
 (void)engine;
 (void)cb_db_op;
 (void)gcr_cntl;
 (void)base;
 (void)size_bytes;
 (void)poll_cycles;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
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

std::uint32_t* APS5_VABI sceAgcDcbWriteData(CommandBuffer* buf, std::uint8_t dst, std::uint8_t cachePolicy, std::uint64_t address, const void* data, std::uint32_t numDwords, std::uint8_t increment, std::uint8_t writeConfirm) {
    return Agc::Command::WriteData(buf, false, dst, cachePolicy, address, data, numDwords, increment, writeConfirm, __func__);
}

uint32_t APS5_VABI sceAgcDcbWriteDataGetSize(uint32_t num_dwords) {
 (void)num_dwords;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
