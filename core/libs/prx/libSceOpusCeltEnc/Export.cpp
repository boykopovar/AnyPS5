#include "prx/libSceOpusCeltEnc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceOpusCeltEnc/include/OpusEncoder.hpp"
#include <cstdint>
#include <cstring>
#include "prx/libc/include/General.hpp"

namespace {

std::uint32_t bitrate(int rate, int channels) {
    if (rate == 8000) return channels == 1 ? 11000u : 19000u;
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
    writeWord(bytes, 0x0Cu, 2u);
    writeWord(bytes, 0x10u, channelWord);
    writeWord(bytes, 0x14u, rateWord);
    writeWord(bytes, 0x1Cu, bandwidth);
    writeWord(bytes, 0x20u, 2u);
    writeWord(bytes, 0x24u, 0x451u);
    writeWord(bytes, 0x28u, 0x5E0u);
    writeWord(bytes, 0x2Cu, 10u);
    writeWord(bytes, 0x40u, frameWord);
    writeWord(bytes, 0xA8u, 0x803u);
    writeWord(bytes, 0xACu, 0x451u);
    writeWord(bytes, 0xB0u, 0x451u);
    writeWord(bytes, 0xB4u, 1u);
    writeWord(bytes, 0xB8u, 1u);
    writeWord(bytes, 0xBCu, 0xFFFFFC18u);
    writeWord(bytes, 0xC0u, bandwidth);
    writeWord(bytes, 0xC4u, 0x10u);
    writeWord(bytes, 0xC8u, frameWord);
    writeWord(bytes, 0xCCu, 0x17B2u);
}

constexpr EncoderCodes codes{0x813F0000u, 1u, 2u, 3u, 4u, 5u, 0x10u, 0x0Du, 0x11u, 0x16u, 0x19u, 728, 0xCCu, 0x00u, 0x14u, 0x10u, 0x17B2u, {1u, 2u, 4u, 8u, 16u, 24u}, 6, writeState};

OpusEncoderLibrary library("OpusCeltEnc", OPUS_APPLICATION_RESTRICTED_LOWDELAY, OPUS_APPLICATION_RESTRICTED_CELT, codes);

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
