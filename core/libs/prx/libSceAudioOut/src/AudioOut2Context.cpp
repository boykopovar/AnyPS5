#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAudioOut2ContextAdvance(AudioOut2ContextHandle ctx) {
    (void)ctx;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOut2ContextCreate(const AudioOut2ContextParam* params, void* buffer, size_t buffer_size, AudioOut2ContextHandle* ctx) {
    (void)params;
    (void)buffer;
    (void)buffer_size;
    (void)ctx;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOut2ContextDestroy(AudioOut2ContextHandle ctx) {
    (void)ctx;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOut2ContextGetQueueLevel(AudioOut2ContextHandle ctx, uint32_t* queue_level, uint32_t* available_queues) {
    (void)ctx;
    (void)queue_level;
    (void)available_queues;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOut2ContextPush(AudioOut2ContextHandle ctx, uint32_t blocking) {
    (void)ctx;
    (void)blocking;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOut2ContextQueryMemory(const AudioOut2ContextParam* params, size_t* memory_size) {
    (void)params;
    (void)memory_size;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOut2ContextResetParam(AudioOut2ContextParam* params) {
    (void)params;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceAudioOut2ContextSetAttributes(AudioOut2ContextHandle ctx, const AudioOut2Attribute* attributes, uint32_t num) {
    (void)ctx;
    (void)attributes;
    (void)num;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
