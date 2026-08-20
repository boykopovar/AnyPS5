#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"

extern "C" {

int sceAgcDriverAddEqEvent(KernelEqueue eq, int id, void* udata) {
 (void)eq;
 (void)id;
 (void)udata;
 return 0;
}

int sceAgcDriverDeleteEqEvent(KernelEqueue eq, int id) {
 (void)eq;
 (void)id;
 return 0;
}

uint32_t sceAgcDriverGetEqContextId(const KernelEvent* ev) {
 (void)ev;
 return 0;
}

int sceAgcDriverGetEqEventType(const KernelEvent* ev) {
 (void)ev;
 return 0;
}

uint32_t sceAgcDriverInitResourceRegistration(void) {
 return 0;
}

bool sceAgcDriverIsCaptureInProgress(void) {
 return false;
}

uint32_t sceAgcDriverQueryResourceRegistrationUserMemoryRequirements(uint64_t* size_in_bytes) {
 (void)size_in_bytes;
 return 0;
}

int sceAgcDriverRegisterOwner(void) {
 return 0;
}

int sceAgcDriverRegisterResource(void) {
 return 0;
}

int sceAgcDriverRegisterWorkloadStream(uint32_t stream_id, const void* stream) {
 (void)stream_id;
 (void)stream;
 return 0;
}

int sceAgcDriverSetHsOffchipParam(uint64_t value0, uint64_t value1, uint64_t value2) {
 (void)value0;
 (void)value1;
 (void)value2;
 return 0;
}

int sceAgcDriverSetTFRing(const volatile void* base, uint32_t size) {
 (void)base;
 (void)size;
 return 0;
}

int sceAgcDriverSubmitAcb(uint32_t queue, const Packet* packet) {
 (void)queue;
 (void)packet;
 return 0;
}

int sceAgcDriverSubmitCommandBuffer(void* queue_context, const Packet* packet) {
 (void)queue_context;
 (void)packet;
 return 0;
}

int sceAgcDriverSubmitDcb(const Packet* packet) {
 (void)packet;
 return 0;
}

int sceAgcDriverSubmitMultiAcbs(uint32_t queue, uint32_t* const* acbs, const uint32_t* sizes_in_dwords, uint32_t count) {
 (void)queue;
 (void)acbs;
 (void)sizes_in_dwords;
 (void)count;
 return 0;
}

int sceAgcDriverSubmitMultiCommandBuffers(void* queue_context, uint32_t* const* command_buffers, const uint32_t* sizes_in_dwords, uint32_t count) {
 (void)queue_context;
 (void)command_buffers;
 (void)sizes_in_dwords;
 (void)count;
 return 0;
}

int sceAgcDriverSubmitMultiDcbs(uint32_t* const* dcb_gpu_addrs, const uint32_t* dcb_sizes_in_dwords, uint32_t count) {
 (void)dcb_gpu_addrs;
 (void)dcb_sizes_in_dwords;
 (void)count;
 return 0;
}

int sceAgcDriverUnregisterOwnerAndResources(uint32_t owner_handle) {
 (void)owner_handle;
 return 0;
}

int sceAgcDriverUnregisterResource(void) {
 return 0;
}

}
