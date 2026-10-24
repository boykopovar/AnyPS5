#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceOpusCeltEnc/include/OpusEncoder.hpp"
#include <cstdint>
#include "prx/libc/include/General.hpp"

struct OpusSilkEncoder;

namespace {

OpusEncoderLibrary library("OpusSilkEnc", OPUS_APPLICATION_RESTRICTED_SILK, 0x81740000u);

} // namespace

extern "C" {

int APS5_VABI sceOpusSilkEncInitialize(std::uint32_t* context) { return library.Initialize(context); }

int APS5_VABI sceOpusSilkEncTerminate(const std::uint32_t* context) { return library.Terminate(context); }

int APS5_VABI sceOpusSilkEncGetSize(int channels) { return library.GetSize(channels); }

int APS5_VABI sceOpusSilkEncCreate(OpusSilkEncoder* state, int sampleRate, int channels, int application) {
    return library.Create(state, sampleRate, channels, application);
}

int APS5_VABI sceOpusSilkEncCreateEx(const std::uint32_t* context, OpusSilkEncoder* state, int sampleRate, int channels, int application) {
    return library.CreateEx(context, state, sampleRate, channels, application);
}

int APS5_VABI sceOpusSilkEncEncode(OpusSilkEncoder* state, const std::int16_t* samples, int frameSize, std::uint8_t* data, int maxDataBytes) {
    return library.Encode(state, samples, frameSize, data, maxDataBytes);
}

int APS5_VABI sceOpusSilkEncEncodeFloat(OpusSilkEncoder* state, const float* samples, int frameSize, std::uint8_t* data, int maxDataBytes) {
    return library.EncodeFloat(state, samples, frameSize, data, maxDataBytes);
}

int APS5_VABI sceOpusSilkEncDestroy(OpusSilkEncoder* state) { return library.Destroy(state); }

int APS5_VABI sceOpusSilkEncCtl(OpusSilkEncoder* state, int request, ...) {
    (void)state;
    (void)request;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusSilkEncCancel(OpusSilkEncoder* state, std::uint32_t batchId) {
    (void)state;
    (void)batchId;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusSilkEncEncodeWithPriority(OpusSilkEncoder* state, const std::int16_t* samples, int frameSize, std::uint8_t* data, int maxDataBytes, std::uint32_t priority) {
    (void)state;
    (void)samples;
    (void)frameSize;
    (void)data;
    (void)maxDataBytes;
    (void)priority;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusSilkEncEncodeFloatWithPriority(OpusSilkEncoder* state, const float* samples, int frameSize, std::uint8_t* data, int maxDataBytes, std::uint32_t priority) {
    (void)state;
    (void)samples;
    (void)frameSize;
    (void)data;
    (void)maxDataBytes;
    (void)priority;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusSilkEncEncodeWithPriorityAndTimeout(OpusSilkEncoder* state, const std::int16_t* samples, int frameSize, std::uint8_t* data, int maxDataBytes, std::uint32_t priority, std::uint32_t timeout, std::uint32_t* batchId) {
    (void)state;
    (void)samples;
    (void)frameSize;
    (void)data;
    (void)maxDataBytes;
    (void)priority;
    (void)timeout;
    (void)batchId;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceOpusSilkEncEncodeFloatWithPriorityAndTimeout(OpusSilkEncoder* state, const float* samples, int frameSize, std::uint8_t* data, int maxDataBytes, std::uint32_t priority, std::uint32_t timeout, std::uint32_t* batchId) {
    (void)state;
    (void)samples;
    (void)frameSize;
    (void)data;
    (void)maxDataBytes;
    (void)priority;
    (void)timeout;
    (void)batchId;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

} // extern "C"
