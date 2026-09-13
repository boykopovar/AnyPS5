#include "prx/libSceAgc/Cb/include/Sync.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

std::uint32_t* APS5_VABI sceAgcCbNop_nid_postfix(CommandBuffer* buf, std::uint32_t sizeInDwords) {
    return Agc::Command::WriteNop(buf, sizeInDwords, __func__);
}

uint32_t APS5_VABI sceAgcCbNopGetSize(uint32_t size_in_dwords) {
 (void)size_in_dwords;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t APS5_VABI sceAgcCbQueueEndOfPipeActionGetSize(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t* APS5_VABI sceAgcCbReleaseMem(CommandBuffer* buf, uint8_t action, uint16_t gcr_cntl, uint8_t dst, uint8_t cache_policy, const volatile Label* address, uint8_t data_sel, uint64_t data, uint16_t gds_offset, uint16_t gds_size, uint8_t interrupt, uint32_t interrupt_ctx_id) {
 (void)buf;
 (void)action;
 (void)gcr_cntl;
 (void)dst;
 (void)cache_policy;
 (void)address;
 (void)data_sel;
 (void)data;
 (void)gds_offset;
 (void)gds_size;
 (void)interrupt;
 (void)interrupt_ctx_id;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

}
