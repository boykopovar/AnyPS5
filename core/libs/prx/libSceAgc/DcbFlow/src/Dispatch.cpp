#include "prx/libSceAgc/DcbFlow/include/Dispatch.hpp"

#include "prx/libSceAgc/Command/include/Memory.hpp"
#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "shader/recompilier/Recompiler.hpp"

extern "C" {

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

}
