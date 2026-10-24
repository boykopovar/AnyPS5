#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceOpusDec/include/OpusDecoder.hpp"
#include <cstdint>
#include "prx/libc/include/General.hpp"

namespace {

OpusDecoderLibrary library("OpusCeltDec", true);

} // namespace

extern "C" {

int APS5_VABI sceOpusCeltDecInitialize(std::uint32_t* context) { return library.Initialize(context); }

int APS5_VABI sceOpusCeltDecTerminate(const std::uint32_t* context) { return library.Terminate(context); }

int APS5_VABI sceOpusCeltDecGetSize(int channels) { return library.GetSize(channels); }

int APS5_VABI sceOpusCeltDecCreateEx(const std::uint32_t* context, void* state, int sampleRate, int channels) {
    return library.CreateEx(context, state, sampleRate, channels);
}

int APS5_VABI sceOpusCeltDecDecode(void* state, const std::uint8_t* data, int bytes, std::int16_t* pcm, int capacity) {
    return library.Decode(state, data, bytes, pcm, capacity);
}

int APS5_VABI sceOpusCeltDecDestroy(void* state) { return library.Destroy(state); }

int APS5_VABI sceOpusCeltDecCreate() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusCeltDecCtl() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusCeltDecDecodeFloat() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusCeltDecDecodeFloatWithPriority() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusCeltDecDecodeWithPriority() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
