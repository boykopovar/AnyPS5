#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAudioOut2Initialize(void);
int APS5_VABI sceAudioOut2ContextQueryMemory(const AudioOut2ContextParam* params, size_t* memory_size);
int APS5_VABI sceAudioOut2ContextCreate(const AudioOut2ContextParam* params, void* buffer, size_t buffer_size, AudioOut2ContextHandle* ctx);
int APS5_VABI sceAudioOut2ContextDestroy(AudioOut2ContextHandle ctx);
int APS5_VABI sceAudioOut2ContextSetAttributes(AudioOut2ContextHandle ctx, const AudioOut2Attribute* attributes, uint32_t num);
int APS5_VABI sceAudioOut2ContextAdvance(AudioOut2ContextHandle ctx);
int APS5_VABI sceAudioOut2ContextPush(AudioOut2ContextHandle ctx, uint32_t blocking);
int APS5_VABI sceAudioOut2ContextGetQueueLevel(AudioOut2ContextHandle ctx, uint32_t* queue_level, uint32_t* available_queue);
int APS5_VABI sceAudioOut2PortCreate(AudioOut2ContextHandle ctx, const AudioOut2PortParam* params, AudioOut2PortHandle* port);
int APS5_VABI sceAudioOut2PortDestroy(AudioOut2PortHandle port);
int APS5_VABI sceAudioOut2PortSetAttributes(AudioOut2PortHandle port, const AudioOut2Attribute* attributes, uint32_t num);
int APS5_VABI sceAudioOut2PortGetState(AudioOut2PortHandle port, AudioOut2PortState* state);

int APS5_VABI sceAudioOut2LoInit(void) {
    return sceAudioOut2Initialize();
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
