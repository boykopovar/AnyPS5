#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAgcDriverAddEqEvent(KernelEqueue eq, int id, void* udata) {
 (void)eq;
 (void)id;
 (void)udata;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDriverDeleteEqEvent(KernelEqueue eq, int id) {
 (void)eq;
 (void)id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t APS5_VABI sceAgcDriverGetEqContextId(const KernelEvent* ev) {
 (void)ev;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDriverGetEqEventType(const KernelEvent* ev) {
 (void)ev;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint32_t APS5_VABI sceAgcDriverInitResourceRegistration(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

bool APS5_VABI sceAgcDriverIsCaptureInProgress(void) {
 NotImplemented_nid_no_patch(__func__);
 return false;
}

uint32_t APS5_VABI sceAgcDriverQueryResourceRegistrationUserMemoryRequirements(uint64_t* size_in_bytes) {
 (void)size_in_bytes;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDriverRegisterOwner(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDriverRegisterResource(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDriverRegisterWorkloadStream(uint32_t stream_id, const void* stream) {
 (void)stream_id;
 (void)stream;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDriverSetHsOffchipParam(uint64_t value0, uint64_t value1, uint64_t value2) {
 (void)value0;
 (void)value1;
 (void)value2;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDriverSetTFRing(const volatile void* base, uint32_t size) {
 (void)base;
 (void)size;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDriverSubmitAcb(uint32_t queue, const Packet* packet) {
 (void)queue;
 (void)packet;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDriverSubmitCommandBuffer(void* queue_context, const Packet* packet) {
 (void)queue_context;
 (void)packet;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDriverSubmitDcb(const Packet* packet) {
 (void)packet;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDriverSubmitMultiAcbs(uint32_t queue, uint32_t* const* acbs, const uint32_t* sizes_in_dwords, uint32_t count) {
 (void)queue;
 (void)acbs;
 (void)sizes_in_dwords;
 (void)count;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDriverSubmitMultiCommandBuffers(void* queue_context, uint32_t* const* command_buffers, const uint32_t* sizes_in_dwords, uint32_t count) {
 (void)queue_context;
 (void)command_buffers;
 (void)sizes_in_dwords;
 (void)count;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDriverSubmitMultiDcbs(uint32_t* const* dcb_gpu_addrs, const uint32_t* dcb_sizes_in_dwords, uint32_t count) {
 (void)dcb_gpu_addrs;
 (void)dcb_sizes_in_dwords;
 (void)count;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDriverUnregisterOwnerAndResources(uint32_t owner_handle) {
 (void)owner_handle;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDriverUnregisterResource(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}


// signature from name
bool APS5_VABI sceAgcDriverIsTraceInProgress(void) {
 NotImplemented_nid_no_patch(__func__);
 return false;
}

 // signature from name
bool APS5_VABI sceAgcDriverIsSubmitValidationEnabled(void) {
 NotImplemented_nid_no_patch(__func__);
 return false;
}

 // signature from name
int APS5_VABI sceAgcDriverAgrSubmitDcb(const Packet* packet) {
 (void)packet;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
