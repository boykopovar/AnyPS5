#include "prx/libSceAgc/DcbState/include/ContextState.hpp"

#include "prx/libSceAgc/Command/include/Packet.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

uint32_t* APS5_VABI sceAgcDcbContextStateOp(CommandBuffer* buf, uint32_t operation) {
 (void)buf;
 (void)operation;
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

uint64_t APS5_VABI sceAgcDcbContextStateOpGetSize(uint32_t operation) {
 (void)operation;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

APS5_EXPORT("qj7QZpgr9Uw", sceAgcDcbContextStateAnotherOp);
uint32_t* APS5_VABI sceAgcDcbContextStateAnotherOp(CommandBuffer* buf, uint32_t operation) {
    (void)buf;
    (void)operation;
    NotImplemented_nid_no_patch(__func__);
    return nullptr;
}

uint32_t APS5_VABI sceAgcDcbSetBaseDispatchIndirectArgsGetSize() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t APS5_VABI sceAgcDcbSetBaseDrawIndirectArgsGetSize() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t APS5_VABI sceAgcDcbQueueEndOfShaderActionGetSize() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

std::uint32_t APS5_VABI sceAgcDcbGetLodStatsGetSize() {
    return 20;
}

}
