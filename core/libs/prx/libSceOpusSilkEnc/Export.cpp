#include "prx/libSceOpusSilkEnc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceOpusCeltEnc/include/OpusEncoder.hpp"
#include <cstdint>
#include <cstring>
#include "prx/libc/include/General.hpp"

namespace {

std::uint32_t bitrate(int rate, int channels) {
    if (rate == 8000) return channels == 1 ? 12000u : 19000u;
    if (rate == 12000) return channels == 1 ? 15000u : 27000u;
    if (rate == 16000) return channels == 1 ? 19000u : 35000u;
    if (rate == 24000) return channels == 1 ? 27000u : 51000u;
    return channels == 1 ? 51000u : 99000u;
}

void writeWord(std::uint8_t* state, std::size_t offset, std::uint32_t value) {
    std::memcpy(state + offset, &value, sizeof value);
}

void writeState(void* state, std::uint32_t instanceId, int rate, int channels) {
    auto* bytes = static_cast<std::uint8_t*>(state);
    const auto rateWord = static_cast<std::uint32_t>(rate);
    const auto frameWord = static_cast<std::uint32_t>(rate / 50);
    const auto channelWord = static_cast<std::uint32_t>(channels);
    const auto bandwidth = bitrate(rate, channels);
    writeWord(bytes, 0x00u, instanceId);
    writeWord(bytes, 0x08u, 8u * channelWord);
    writeWord(bytes, 0x10u, 0x800u);
    writeWord(bytes, 0x14u, rateWord);
    writeWord(bytes, 0x18u, channelWord);
    writeWord(bytes, 0x1Cu, bandwidth);
    writeWord(bytes, 0x28u, 0x44Fu);
    writeWord(bytes, 0x2Cu, 0x44Fu);
    writeWord(bytes, 0x30u, 0x5E0u);
    writeWord(bytes, 0x34u, 10u);
    writeWord(bytes, 0x48u, 0x3E8u);
    writeWord(bytes, 0x58u, frameWord);
    writeWord(bytes, 0xE4u, frameWord);
    writeWord(bytes, 0xD8u, 0xFFFFFC18u);
    writeWord(bytes, 0xDCu, bandwidth);
    writeWord(bytes, 0xE0u, 0x10u);
    writeWord(bytes, 0xE8u, 0x17B2u);
}

constexpr EncoderCodes codes{0x81770000u, 1u, 2u, 3u, 4u, 5u, 0x0Fu, 0x0Cu, 0x10u, 0x15u, 0x18u, 752, 0xE8u, 0x00u, 0x14u, 0x18u, 0x17B2u, {4u, 8u, 16u, 24u, 0u, 0u}, 4, writeState};

OpusEncoderLibrary library("OpusSilkEnc", OPUS_APPLICATION_VOIP, OPUS_APPLICATION_RESTRICTED_SILK, codes);

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
