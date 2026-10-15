#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceOpusDec/include/OpusDecoder.hpp"
#include <cstdint>

namespace {

OpusDecoderLibrary library("OpusCeltDec", true);

} // namespace

extern "C" {

int APS5_VABI sceOpusCeltDecInitialize(std::uint32_t* context) { return library.Initialize(context); }

int APS5_VABI sceOpusCeltDecTerminate(std::uint32_t* context) { return library.Terminate(context); }

int APS5_VABI sceOpusCeltDecGetSize(int channels) { return library.GetSize(channels); }

int APS5_VABI sceOpusCeltDecCreateEx(std::uint32_t* context, void* state, int sampleRate, int channels) {
    return library.CreateEx(context, state, sampleRate, channels);
}

int APS5_VABI sceOpusCeltDecDecode(void* state, const std::uint8_t* data, int bytes, std::int16_t* pcm, int capacity) {
    return library.Decode(state, data, bytes, pcm, capacity);
}

int APS5_VABI sceOpusCeltDecDestroy(void* state) { return library.Destroy(state); }

int APS5_VABI sceOpusCeltDecCreate(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusCeltDecCtl(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusCeltDecDecodeFloat(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusCeltDecDecodeFloatWithPriority(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusCeltDecDecodeWithPriority(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusCeltDecGetAjmContext(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
