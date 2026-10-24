#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceOpusCeltEnc/include/OpusEncoder.hpp"
#include <cstdint>
#include "prx/libc/include/General.hpp"

struct OpusCeltEncoder;

namespace {

OpusEncoderLibrary library("OpusCeltEnc", OPUS_APPLICATION_RESTRICTED_CELT, 0x81400000u);

} // namespace

extern "C" {

int APS5_VABI sceOpusCeltEncInitialize(std::uint32_t* context) { return library.Initialize(context); }

int APS5_VABI sceOpusCeltEncTerminate(const std::uint32_t* context) { return library.Terminate(context); }

int APS5_VABI sceOpusCeltEncGetSize(int channels) { return library.GetSize(channels); }

int APS5_VABI sceOpusCeltEncCreate(OpusCeltEncoder* state, int sampleRate, int channels, int application) {
    return library.Create(state, sampleRate, channels, application);
}

int APS5_VABI sceOpusCeltEncCreateEx(const std::uint32_t* context, OpusCeltEncoder* state, int sampleRate, int channels, int application) {
    return library.CreateEx(context, state, sampleRate, channels, application);
}

int APS5_VABI sceOpusCeltEncEncode(OpusCeltEncoder* state, const std::int16_t* samples, int frameSize, std::uint8_t* data, int maxDataBytes) {
    return library.Encode(state, samples, frameSize, data, maxDataBytes);
}

int APS5_VABI sceOpusCeltEncEncodeFloat(OpusCeltEncoder* state, const float* samples, int frameSize, std::uint8_t* data, int maxDataBytes) {
    return library.EncodeFloat(state, samples, frameSize, data, maxDataBytes);
}

int APS5_VABI sceOpusCeltEncDestroy(OpusCeltEncoder* state) { return library.Destroy(state); }

int APS5_VABI sceOpusCeltEncCtl(OpusCeltEncoder* state, int request, ...) {
    (void)state;
    (void)request;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusCeltEncEncodeWithPriority(OpusCeltEncoder* state, const std::int16_t* samples, int frameSize, std::uint8_t* data, int maxDataBytes, std::uint32_t priority) {
    (void)state;
    (void)samples;
    (void)frameSize;
    (void)data;
    (void)maxDataBytes;
    (void)priority;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusCeltEncEncodeFloatWithPriority(OpusCeltEncoder* state, const float* samples, int frameSize, std::uint8_t* data, int maxDataBytes, std::uint32_t priority) {
    (void)state;
    (void)samples;
    (void)frameSize;
    (void)data;
    (void)maxDataBytes;
    (void)priority;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

} // extern "C"
