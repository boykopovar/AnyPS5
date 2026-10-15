#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceOpusDec/include/OpusDecoder.hpp"
#include <cstdint>

namespace {

OpusDecoderLibrary library("OpusDec", false);

} // namespace

extern "C" {

int APS5_VABI sceOpusDecInitialize(std::uint32_t* context) { return library.Initialize(context); }

int APS5_VABI sceOpusDecTerminate(std::uint32_t* context) { return library.Terminate(context); }

int APS5_VABI sceOpusDecGetSize(int channels) { return library.GetSize(channels); }

int APS5_VABI sceOpusDecCreateEx(std::uint32_t* context, void* state, int sampleRate, int channels) {
    return library.CreateEx(context, state, sampleRate, channels);
}

int APS5_VABI sceOpusDecDecode(void* state, const std::uint8_t* data, int bytes, std::int16_t* pcm, int capacity) {
    return library.Decode(state, data, bytes, pcm, capacity);
}

int APS5_VABI sceOpusDecDestroy(void* state) { return library.Destroy(state); }

int APS5_VABI sceOpusDecCancel(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusDecCreate(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusDecCtl(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusDecDecodeCancelForTimeout(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusDecDecodeFloat(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusDecDecodeFloatWithPriority(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusDecDecodeFloatWithPriorityAndTimeout(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusDecDecodeWithPriority(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusDecDecodeWithPriorityAndTimeout(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusDecGetAjmContext(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
