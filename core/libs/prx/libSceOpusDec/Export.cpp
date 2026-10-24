#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceOpusDec/include/OpusDecoder.hpp"
#include "prx/libSceOpusDec/include/OpusDecodeVariants.hpp"
#include <cstdint>
#include "prx/libc/include/General.hpp"

namespace {

OpusDecoderLibrary library("OpusDec", false);

constexpr std::uint32_t ErrorBase = 0x81740000u;

} // namespace

extern "C" {

int APS5_VABI sceOpusDecInitialize(std::uint32_t* context) { return library.Initialize(context); }

int APS5_VABI sceOpusDecTerminate(const std::uint32_t* context) { return library.Terminate(context); }

int APS5_VABI sceOpusDecGetSize(int channels) { return library.GetSize(channels); }

int APS5_VABI sceOpusDecCreateEx(const std::uint32_t* context, void* state, int sampleRate, int channels) {
    return library.CreateEx(context, state, sampleRate, channels);
}

int APS5_VABI sceOpusDecDecode(void* state, const std::uint8_t* data, int bytes, std::int16_t* pcm, int capacity) {
    return library.Decode(state, data, bytes, pcm, capacity);
}

int APS5_VABI sceOpusDecDestroy(void* state) { return library.Destroy(state); }

int APS5_VABI sceOpusDecCancel() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusDecCreate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusDecCtl() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusDecDecodeFloat(void* state, const std::uint8_t* data, int bytes, float* pcm, int capacity) {
    return OpusDecodeVariant(library, ErrorBase, state, data, bytes, pcm, capacity, OpusDefaultDecodePriority, true);
}

int APS5_VABI sceOpusDecDecodeFloatWithPriority(void* state, const std::uint8_t* data, int bytes, float* pcm, int capacity, int priority) {
    return OpusDecodeVariant(library, ErrorBase, state, data, bytes, pcm, capacity, priority, true);
}

int APS5_VABI sceOpusDecDecodeFloatWithPriorityAndTimeout(void* state, const std::uint8_t* data, int bytes, float* pcm, int capacity, int priority,
                                                         std::uint32_t timeout, std::uint32_t*) {
    return OpusDecodeVariantWithTimeout(library, ErrorBase, state, data, bytes, pcm, capacity, priority, true, timeout, __func__);
}

int APS5_VABI sceOpusDecDecodeWithPriority(void* state, const std::uint8_t* data, int bytes, std::int16_t* pcm, int capacity, int priority) {
    return OpusDecodeVariant(library, ErrorBase, state, data, bytes, pcm, capacity, priority, false);
}

int APS5_VABI sceOpusDecDecodeWithPriorityAndTimeout(void* state, const std::uint8_t* data, int bytes, std::int16_t* pcm, int capacity, int priority,
                                                    std::uint32_t timeout, std::uint32_t*) {
    return OpusDecodeVariantWithTimeout(library, ErrorBase, state, data, bytes, pcm, capacity, priority, false, timeout, __func__);
}

}
