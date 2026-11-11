#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include "prx/libSceAudioOut/include/InventoryExports.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "AudioOut2Internal.hpp"

static int AudioOut2QueryMemoryLo(const AudioOut2ContextParam* params, const std::uint32_t* exParams, std::size_t* memory_size) {
    constexpr int ErrorInvalidParam = static_cast<int>(0x80268001);
    constexpr int ErrorInvalidGrains = static_cast<int>(0x8026800d);
    if (!params) return ErrorInvalidParam;
    if (params->max_object_ports > 0x80) return ErrorInvalidParam;
    if (params->max_ports > 0x20) return ErrorInvalidParam;
    if (params->guarantee_object_ports > params->max_object_ports) return ErrorInvalidParam;
    if (params->num_grains < 0x100 || params->num_grains > 0x800 || (params->num_grains & 0xff) != 0) return ErrorInvalidGrains;
    const std::uint32_t extra_ports = exParams ? exParams[0] : 0;
    if (exParams && extra_ports > 0x15) return ErrorInvalidParam;
    if (params->queue_depth > (exParams ? 0x40u : 0x10u)) return ErrorInvalidParam;
    if (memory_size) {
        const auto slot_words = [](std::uint32_t n) { return 0xc + ((n + 0x1f) >> 5) * 4; };
        const std::size_t required = static_cast<std::size_t>(params->max_ports + extra_ports + params->max_object_ports) * 0xb60
            + static_cast<std::size_t>(slot_words(params->max_ports) + slot_words(params->max_object_ports))
            + (exParams ? static_cast<std::size_t>(slot_words(extra_ports)) : 0)
            + ((static_cast<std::size_t>(params->max_object_ports) * 24 + 0x7f) & ~static_cast<std::size_t>(0x7f));
        const int savedErrno = errno;
        const bool copied = GuestCopyOut_nid_no_patch(memory_size, &required, sizeof(required));
        errno = savedErrno;
        if (!copied) return ErrorInvalidParam;
    }
    return 0;
}

static int AudioOut2CreateContextLo(const AudioOut2ContextParam* params, const std::uint32_t* exParams, void* buffer, std::size_t buffer_size, AudioOut2ContextHandle* ctx) {
    constexpr int ErrorInvalidParam = static_cast<int>(0x80268001);
    if (!params || !buffer || !ctx) return ErrorInvalidParam;
    *ctx = static_cast<AudioOut2ContextHandle>(-1);
    std::size_t required = 0;
    const int result = AudioOut2QueryMemoryLo(params, exParams, &required);
    if (result < 0) return result;
    if (exParams && exParams[1] > params->max_object_ports) return ErrorInvalidParam;
    if (buffer_size < required) return ErrorInvalidParam;
    return sceAudioOut2ContextCreate(params, buffer, buffer_size, ctx);
}

extern "C" {

int APS5_VABI sceAudioOut2LoInit(void) {
    AudioOut2MarkInitialized_nid_no_patch();
    return 0;
}

int APS5_VABI sceAudioOut2LoTerminate(void) {
    return 0;
}

int APS5_VABI sceAudioOut2LoContextQueryMemory(const AudioOut2ContextParam* params, size_t* memory_size) {
    return AudioOut2QueryMemoryLo(params, nullptr, memory_size);
}

int APS5_VABI sceAudioOut2LoContextQueryMemoryEx(const AudioOut2ContextParam* params, const void* exParams, size_t* memory_size) {
    return AudioOut2QueryMemoryLo(params, static_cast<const std::uint32_t*>(exParams), memory_size);
}

int APS5_VABI sceAudioOut2LoContextCreate(const AudioOut2ContextParam* params, void* buffer, size_t buffer_size, AudioOut2ContextHandle* ctx) {
    return AudioOut2CreateContextLo(params, nullptr, buffer, buffer_size, ctx);
}

int APS5_VABI sceAudioOut2LoContextCreateEx(const AudioOut2ContextParam* params, const void* exParams, void* buffer, size_t buffer_size, AudioOut2ContextHandle* ctx) {
    return AudioOut2CreateContextLo(params, static_cast<const std::uint32_t*>(exParams), buffer, buffer_size, ctx);
}

int APS5_VABI sceAudioOut2LoContextDestroy(AudioOut2ContextHandle ctx) {
    return sceAudioOut2ContextDestroy(ctx);
}

int APS5_VABI sceAudioOut2LoContextSetAttributes(AudioOut2ContextHandle ctx, const AudioOut2Attribute* attributes, uint32_t num) {
    return sceAudioOut2ContextSetAttributes(ctx, attributes, num);
}

int APS5_VABI sceAudioOut2LoContextGetAttributes(AudioOut2ContextHandle ctx, AudioOut2Attribute* attributes, uint32_t num) {
    constexpr int ErrorInvalidParam = static_cast<int>(0x80268001);
    if (!AudioOut2FindContext(ctx)) return ErrorInvalidParam;
    if (!attributes && num != 0) return ErrorInvalidParam;
    for (uint32_t index = 0; index < num; index++) {
        const auto id = attributes[index].attribute_id;
        if (id != 0x1e && id != 0x1f) return ErrorInvalidParam;
        NotImplemented_nid_no_patch(__func__);
    }
    return 0;
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

int APS5_VABI sceAudioOut2LoPortCreateEx(AudioOut2ContextHandle ctx, const AudioOut2PortParam* params, AudioOut2PortHandle* port) {
    return sceAudioOut2PortCreate(ctx, params, port);
}

int APS5_VABI sceAudioOut2LoPortDestroy(AudioOut2PortHandle port) {
    return sceAudioOut2PortDestroy(port);
}

int APS5_VABI sceAudioOut2LoPortSetAttributes(AudioOut2PortHandle port, const AudioOut2Attribute* attributes, uint32_t num) {
    return sceAudioOut2PortSetAttributes(port, attributes, num);
}

int APS5_VABI sceAudioOut2LoPortSetAttributesEx(AudioOut2PortHandle port, const AudioOut2Attribute* attributes, uint32_t num) {
    return sceAudioOut2PortSetAttributes(port, attributes, num);
}

int APS5_VABI sceAudioOut2LoPortGetState(AudioOut2PortHandle port, AudioOut2PortState* state) {
    return sceAudioOut2PortGetState(port, state);
}

int APS5_VABI sceAudioOut2LoPortGetStateEx(AudioOut2PortHandle port, AudioOut2PortState* state, uint8_t* extra) {
    if (extra != nullptr) {
        uint8_t zero = 0;
        if (!GuestCopyOut_nid_no_patch(extra, &zero, sizeof(zero))) return static_cast<int>(0x80268001);
    }
    return sceAudioOut2PortGetState(port, state);
}

}
