#include "prx/libSceAgc/Command/Memory.hpp"
#include "prx/libSceAgc/Command/Packet.hpp"
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

uint32_t* APS5_VABI sceAgcDcbCondExec(CommandBuffer* buf, const volatile uint32_t* address, uint32_t num_dwords) {
 (void)buf;
 (void)address;
 (void)num_dwords;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t APS5_VABI sceAgcDcbCondExecGetSize(void) {
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

std::uint32_t* APS5_VABI sceAgcDcbDmaData(CommandBuffer* buf, std::uint8_t engine, std::uint8_t dst, std::uint8_t dstCachePolicy, std::uint64_t dstAddress, std::uint8_t src, std::uint8_t srcCachePolicy, std::uint64_t srcAddress, std::uint32_t numBytes, std::uint8_t waitForPrevious, std::uint8_t writeConfirm, std::uint8_t blockEngine) {
    return Agc::Command::WriteDma(buf, false, engine, dst, dstCachePolicy, dstAddress, src, srcCachePolicy, srcAddress, numBytes, waitForPrevious, writeConfirm, blockEngine, __func__);
}

uint32_t* APS5_VABI sceAgcDcbDispatchIndirect(CommandBuffer* buf, uint32_t data_offset_in_bytes, uint32_t flags) {
 (void)buf;
 (void)data_offset_in_bytes;
 (void)flags;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t APS5_VABI sceAgcDcbDispatchIndirectGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

std::uint32_t* APS5_VABI sceAgcDcbEventWrite(CommandBuffer* buf, std::uint8_t eventType, const volatile void* address) {
    Agc::Command::CheckBits(eventType, 0x3fu, __func__);
    if ((eventType & 0xfeu) == 0x38u) {
        const auto guestAddress = reinterpret_cast<std::uintptr_t>(address);
        Agc::Command::CheckAddress(guestAddress, 8, __func__);
        return Agc::Command::Emit(buf, 0x46u, {0x100u | eventType, static_cast<std::uint32_t>(guestAddress), static_cast<std::uint32_t>(guestAddress >> 32u)}, __func__);
    }
    Agc::Command::Require(address == nullptr, __func__, "this event does not use an address");
    const auto eventIndex = eventType == 7 || eventType == 15 || eventType == 16 ? 0x400u : 0u;
    return Agc::Command::Emit(buf, 0x46u, {eventIndex | eventType}, __func__);
}

uint32_t* APS5_VABI sceAgcDcbJump(CommandBuffer* buf, uint8_t mode, uint8_t cache_policy, const uint32_t* target, uint32_t size_in_dwords) {
 (void)buf;
 (void)mode;
 (void)cache_policy;
 (void)target;
 (void)size_in_dwords;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t APS5_VABI sceAgcDcbJumpGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* APS5_VABI sceAgcDcbResetQueue(CommandBuffer* buf, uint32_t op, uint32_t state) {
 (void)buf;
 (void)op;
 (void)state;
 const std::array<std::uint32_t, 1> code = {0xbf810000u};
 const std::array<std::uint32_t, 1> capabilities = {1u};
 const std::array<ShaderRecompiler::RegisterValue, 3> shaderRegisters = {{{0x207u, 1u}, {0x208u, 1u}, {0x209u, 1u}}};
 const std::array<ShaderRecompiler::MemoryRegion, 1> memory = {{{0x10000u, std::as_bytes(std::span(code))}}};
 const ShaderRecompiler::RecompileRequest request{
     {ShaderRecompiler::ShaderStage::Compute, 0x10000u, code, 0, {}},
     {64, 0, {}, shaderRegisters, {}, {}, memory},
     {0x00403000u, 0x00010600u, 64, capabilities, {}, {1024, 1024, 64}, 1024, 32768},
     {0, 0, 0, 0}
 };
 (void)ShaderRecompiler::Recompile(request);
 return nullptr;
}

uint32_t* APS5_VABI sceAgcDcbRewind(CommandBuffer* buf, uint32_t initial_state) {
 (void)buf;
 (void)initial_state;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint32_t APS5_VABI sceAgcDcbRewindGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

std::uint32_t* APS5_VABI sceAgcDcbStallCommandBufferParser(CommandBuffer* buf) {
    return Agc::Command::Emit(buf, 0x42u, {0}, __func__);
}

std::uint32_t* APS5_VABI sceAgcDcbWaitRegMem(CommandBuffer* buf, std::uint8_t size, std::uint8_t compareFunction, std::uint8_t operation, std::uint8_t cachePolicy, const volatile void* address, std::uint64_t reference, std::uint64_t mask, std::uint32_t pollCycles) {
    return Agc::Command::WriteWait(buf, size, compareFunction, operation, cachePolicy, address, reference, mask, pollCycles, __func__);
}

uint32_t APS5_VABI sceAgcDcbWaitOnAddressGetSize(uint32_t size) {
 (void)size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* APS5_VABI sceAgcDcbWaitUntilSafeForRendering(CommandBuffer* buf, uint32_t video_out_handle, uint32_t display_buffer_index) {
 (void)buf;
 (void)video_out_handle;
 (void)display_buffer_index;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

std::uint32_t* APS5_VABI sceAgcDcbWriteData(CommandBuffer* buf, std::uint8_t dst, std::uint8_t cachePolicy, std::uint64_t address, const void* data, std::uint32_t numDwords, std::uint8_t increment, std::uint8_t writeConfirm) {
    return Agc::Command::WriteData(buf, false, dst, cachePolicy, address, data, numDwords, increment, writeConfirm, __func__);
}

uint32_t APS5_VABI sceAgcDcbWriteDataGetSize(uint32_t num_dwords) {
 (void)num_dwords;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}
std::uint64_t APS5_VABI sceAgcDcbEventWriteGetSize(std::uint8_t eventType) {
    (void)eventType;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint64_t APS5_VABI sceAgcDcbCopyDataGetSize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
