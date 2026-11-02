#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "AudioOut2Internal.hpp"

extern "C" {

int APS5_VABI sceAudioOut2LoInit(void) {
    AudioOut2MarkInitialized_nid_no_patch();
    return 0;
}

int APS5_VABI sceAudioOut2LoTerminate(void) {
    return 0;
}

int APS5_VABI sceAudioOut2LoContextQueryMemory(const AudioOut2ContextParam* params, size_t* memory_size) {
    return sceAudioOut2ContextQueryMemory(params, memory_size);
}

int APS5_VABI sceAudioOut2LoContextCreate(const AudioOut2ContextParam* params, void* buffer, size_t buffer_size, AudioOut2ContextHandle* ctx) {
    return sceAudioOut2ContextCreate(params, buffer, buffer_size, ctx);
}

int APS5_VABI sceAudioOut2LoContextDestroy(AudioOut2ContextHandle ctx) {
    return sceAudioOut2ContextDestroy(ctx);
}

int APS5_VABI sceAudioOut2LoContextSetAttributes(AudioOut2ContextHandle ctx, const AudioOut2Attribute* attributes, uint32_t num) {
    return sceAudioOut2ContextSetAttributes(ctx, attributes, num);
}

int APS5_VABI sceAudioOut2LoContextAdvance(AudioOut2ContextHandle ctx) {
    return sceAudioOut2ContextAdvance(ctx);
}

int APS5_VABI sceAudioOut2LoContextPush(AudioOut2ContextHandle ctx, uint32_t blocking) {
    return sceAudioOut2ContextPush(ctx, blocking);
}

int APS5_VABI sceAudioOut2LoContextGetQueueLevel(AudioOut2ContextHandle ctx, uint32_t* queue_level, uint32_t* available_queue) {
    return sceAudioOut2ContextGetQueueLevel(ctx, queue_level, available_queue);
}

int APS5_VABI sceAudioOut2LoPortCreate(AudioOut2ContextHandle ctx, const AudioOut2PortParam* params, AudioOut2PortHandle* port) {
    return sceAudioOut2PortCreate(ctx, params, port);
}

int APS5_VABI sceAudioOut2LoPortDestroy(AudioOut2PortHandle port) {
    return sceAudioOut2PortDestroy(port);
}

int APS5_VABI sceAudioOut2LoPortSetAttributes(AudioOut2PortHandle port, const AudioOut2Attribute* attributes, uint32_t num) {
    return sceAudioOut2PortSetAttributes(port, attributes, num);
}

int APS5_VABI sceAudioOut2LoPortGetState(AudioOut2PortHandle port, AudioOut2PortState* state) {
    return sceAudioOut2PortGetState(port, state);
}

}
