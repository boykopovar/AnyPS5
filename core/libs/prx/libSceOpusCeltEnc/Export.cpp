#include "prx/libSceOpusCeltEnc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceOpusCeltEnc/include/OpusEncoder.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
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

constexpr std::uint32_t priorityErrorOffset = 0x15u;
constexpr std::uint32_t requestErrorOffset = 0x08u;
constexpr std::uint32_t streamsErrorOffset = 0x1Bu;
constexpr std::uint32_t coupledStreamsErrorOffset = 0x1Cu;
constexpr std::uint32_t mappingErrorOffset = 0x1Du;
constexpr std::size_t multistreamMagicOffset = 0x98u;
constexpr std::uint32_t lowestPriority = 20u;
constexpr std::uint32_t highestPriority = 100u;
constexpr std::uint32_t firstRequest = 4000u;
constexpr std::uint32_t requestCount = 48u;
constexpr int requestMinusOne = -1;
constexpr int requestMinusTwo = -2;
constexpr std::uint32_t maximumStreams = 12u;
constexpr std::array<std::uint32_t, 5> frameFactors{400u, 200u, 100u, 50u, 25u};

int encoderError(std::uint32_t offset) {
    return static_cast<int>(codes.baseError + offset);
}

std::uint32_t guestStateWord(const void* state, std::size_t offset) {
    std::uint32_t word = 0;
    if (!GuestCopyIn_nid_no_patch(&word, static_cast<const std::uint8_t*>(state) + offset, sizeof word)) throw std::runtime_error("OpusCeltEnc: unreadable state");
    return word;
}

bool hasStateMarker(const void* state, std::size_t offset) {
    return guestStateWord(state, offset) == codes.stateMagic;
}

bool acceptedPriorityFrame(std::uint32_t rate, std::uint32_t frameSize) {
    for (const std::uint32_t factor : frameFactors)
        if (frameSize * factor == rate) return true;
    return frameSize * 50u == rate * 3u;
}

bool outsideRequests(int request) {
    return static_cast<std::uint32_t>(request) - firstRequest >= requestCount;
}

bool acceptedMultistreamRate(int rate) {
    return rate == 8000 || rate == 12000 || rate == 16000 || rate == 24000 || rate == 48000;
}

int multistreamCreateError(const void* state, int sampleRate, int streams, int coupledStreams, const std::uint8_t* mapping, int application) {
    if (state == nullptr) return encoderError(codes.nullErrorOffset);
    if (!acceptedMultistreamRate(sampleRate)) return encoderError(codes.rateErrorOffset);
    const auto streamCount = static_cast<std::uint32_t>(streams);
    const auto coupledCount = static_cast<std::uint32_t>(coupledStreams);
    if (streamCount - 1u >= maximumStreams) return encoderError(streamsErrorOffset);
    if (coupledCount > streamCount) return encoderError(coupledStreamsErrorOffset);
    const std::uint32_t channels = streamCount + coupledCount;
    if (channels > maximumStreams) return encoderError(codes.channelsErrorOffset);
    std::array<std::uint8_t, maximumStreams> entries{};
    if (!GuestCopyIn_nid_no_patch(entries.data(), mapping, channels)) throw std::runtime_error("OpusCeltEnc: unreadable channel mapping");
    for (std::uint32_t channel = 0; channel < channels; ++channel)
        if (std::count(entries.begin(), entries.begin() + channels, channel) != 1) return encoderError(mappingErrorOffset);
    if (application != OPUS_APPLICATION_RESTRICTED_LOWDELAY) return encoderError(codes.applicationErrorOffset);
    return 0;
}

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
    if (state == nullptr) return encoderError(codes.nullErrorOffset);
    if (!hasStateMarker(state, codes.stateMagicOffset)) return encoderError(codes.destroyStateErrorOffset);
    if (outsideRequests(request) && request != requestMinusOne && request != requestMinusTwo) return encoderError(requestErrorOffset);
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceOpusCeltEncEncodeWithPriority(OpusCeltEncoder* state, const std::int16_t* samples, int frameSize, std::uint8_t* data, int maxDataBytes, std::uint32_t priority) {
    if (state == nullptr) return encoderError(codes.nullErrorOffset);
    if (samples == nullptr) return encoderError(codes.nullSamplesErrorOffset);
    if (data == nullptr) return encoderError(codes.nullDataErrorOffset);
    if (maxDataBytes == 0) return encoderError(codes.sizeErrorOffset);
    if (priority < lowestPriority || priority > highestPriority) return encoderError(priorityErrorOffset);
    if (!hasStateMarker(state, codes.stateMagicOffset)) return encoderError(codes.destroyStateErrorOffset);
    if (!acceptedPriorityFrame(guestStateWord(state, codes.stateRateOffset), static_cast<std::uint32_t>(frameSize))) return encoderError(codes.frameErrorOffset);
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceOpusCeltEncEncodeFloatWithPriority(OpusCeltEncoder* state, const float* samples, int frameSize, std::uint8_t* data, int maxDataBytes, std::uint32_t priority) {
    (void)data;
    (void)maxDataBytes;
    (void)priority;
    if (state == nullptr) return encoderError(codes.nullErrorOffset);
    if (samples == nullptr) return encoderError(codes.nullSamplesErrorOffset);
    if (frameSize == 0) return encoderError(codes.frameErrorOffset);
    if (!hasStateMarker(state, codes.stateMagicOffset)) return encoderError(codes.destroyStateErrorOffset);
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceOpusCeltEncGetAjmContext(const OpusCeltEncoder* state, std::uint32_t* context) {
    if (state == nullptr) return encoderError(codes.nullErrorOffset);
    if (context == nullptr) return 0;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceOpusCeltMultistreamEncCreate(OpusCeltMultistreamEncoder* state, int sampleRate, int streams, int coupledStreams, const std::uint8_t* mapping, int application) {
    if (const int result = multistreamCreateError(state, sampleRate, streams, coupledStreams, mapping, application); result != 0) return result;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceOpusCeltMultistreamEncCtl(OpusCeltMultistreamEncoder* state, int request, ...) {
    if (state == nullptr) return encoderError(codes.nullErrorOffset);
    if (!hasStateMarker(state, multistreamMagicOffset)) return encoderError(codes.destroyStateErrorOffset);
    if (outsideRequests(request) && request != requestMinusOne) return encoderError(requestErrorOffset);
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceOpusCeltMultistreamEncCreateEx(const std::uint32_t* context, OpusCeltMultistreamEncoder* state, int sampleRate, int streams, int coupledStreams, const std::uint8_t* mapping, int application) {
    if (context == nullptr) return encoderError(codes.nullErrorOffset);
    if (const int result = multistreamCreateError(state, sampleRate, streams, coupledStreams, mapping, application); result != 0) return result;
    NotImplemented_nid_no_patch(__func__);
}

} // extern "C"
