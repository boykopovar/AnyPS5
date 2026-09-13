#include "prx/libSceAgc/Patch/include/QueueEndOfPipeAction.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAgcQueueEndOfPipeActionPatchAddress(uint32_t* cmd, const volatile Label* address) {
 (void)cmd;
 (void)address;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcQueueEndOfPipeActionPatchData(uint32_t* cmd, uint32_t context_id, uint32_t data_sel, uint64_t data) {
 (void)cmd;
 (void)context_id;
 (void)data_sel;
 (void)data;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
