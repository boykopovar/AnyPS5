#include "prx/libSceOpusSilkEnc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceOpusCeltEnc/include/OpusEncoder.hpp"
#include <array>
#include <cstdint>
#include <cstring>
#include <stdexcept>
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

constexpr std::uint32_t requestErrorOffset = 0x07u;
constexpr std::uint32_t priorityErrorOffset = 0x14u;
constexpr std::uint32_t lowestPriority = 20u;
constexpr std::uint32_t highestPriority = 100u;
constexpr std::uint32_t firstRequest = 4000u;
constexpr std::uint32_t requestCount = 32u;
constexpr std::array<int, 4> otherRequests{-2, -1, 11002, 11003};
constexpr std::array<std::uint32_t, 3> frameFactors{100u, 50u, 25u};

int encoderError(std::uint32_t offset) {
    return static_cast<int>(codes.baseError + offset);
}

std::uint32_t guestStateWord(const void* state, std::size_t offset) {
    std::uint32_t word = 0;
    if (!GuestCopyIn_nid_no_patch(&word, static_cast<const std::uint8_t*>(state) + offset, sizeof word)) throw std::runtime_error("OpusSilkEnc: unreadable state");
    return word;
}

bool hasStateMarker(const void* state) {
    return guestStateWord(state, codes.stateMagicOffset) == codes.stateMagic;
}

bool acceptedPriorityFrame(std::uint32_t rate, std::uint32_t frameSize) {
    for (const std::uint32_t factor : frameFactors)
        if (frameSize * factor == rate) return true;
    return frameSize * 50u == rate * 3u;
}

bool knownRequest(int request) {
    if (static_cast<std::uint32_t>(request) - firstRequest < requestCount) return true;
    for (const int other : otherRequests)
        if (request == other) return true;
    return false;
}

int priorityEncodeError(const void* state, const void* samples, int frameSize, const void* data, int maxDataBytes, std::uint32_t priority) {
    if (state == nullptr) return encoderError(codes.nullErrorOffset);
    if (samples == nullptr) return encoderError(codes.nullSamplesErrorOffset);
    if (data == nullptr) return encoderError(codes.nullDataErrorOffset);
    if (maxDataBytes == 0) return encoderError(codes.sizeErrorOffset);
    if (priority < lowestPriority || priority > highestPriority) return encoderError(priorityErrorOffset);
    if (!hasStateMarker(state)) return encoderError(codes.destroyStateErrorOffset);
    if (!acceptedPriorityFrame(guestStateWord(state, codes.stateRateOffset), static_cast<std::uint32_t>(frameSize))) return encoderError(codes.frameErrorOffset);
    return 0;
}

int priorityEncodeFloatError(const void* state, const void* samples, int frameSize) {
    if (state == nullptr) return encoderError(codes.nullErrorOffset);
    if (samples == nullptr) return encoderError(codes.nullSamplesErrorOffset);
    if (frameSize == 0) return encoderError(codes.frameErrorOffset);
    if (!hasStateMarker(state)) return encoderError(codes.destroyStateErrorOffset);
    return 0;
}

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
    if (state == nullptr) return encoderError(codes.nullErrorOffset);
    if (!hasStateMarker(state)) return encoderError(codes.destroyStateErrorOffset);
    if (!knownRequest(request)) return encoderError(requestErrorOffset);
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceOpusSilkEncCancel(OpusSilkEncoder* state, std::uint32_t batchId) {
    (void)batchId;
    if (state == nullptr) return encoderError(codes.nullErrorOffset);
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceOpusSilkEncEncodeWithPriority(OpusSilkEncoder* state, const std::int16_t* samples, int frameSize, std::uint8_t* data, int maxDataBytes, std::uint32_t priority) {
    if (const int result = priorityEncodeError(state, samples, frameSize, data, maxDataBytes, priority); result != 0) return result;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceOpusSilkEncEncodeFloatWithPriority(OpusSilkEncoder* state, const float* samples, int frameSize, std::uint8_t* data, int maxDataBytes, std::uint32_t priority) {
    (void)data;
    (void)maxDataBytes;
    (void)priority;
    if (const int result = priorityEncodeFloatError(state, samples, frameSize); result != 0) return result;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceOpusSilkEncEncodeWithPriorityAndTimeout(OpusSilkEncoder* state, const std::int16_t* samples, int frameSize, std::uint8_t* data, int maxDataBytes, std::uint32_t priority, std::uint32_t timeout, std::uint32_t* batchId) {
    (void)timeout;
    (void)batchId;
    if (const int result = priorityEncodeError(state, samples, frameSize, data, maxDataBytes, priority); result != 0) return result;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceOpusSilkEncEncodeFloatWithPriorityAndTimeout(OpusSilkEncoder* state, const float* samples, int frameSize, std::uint8_t* data, int maxDataBytes, std::uint32_t priority, std::uint32_t timeout, std::uint32_t* batchId) {
    (void)data;
    (void)maxDataBytes;
    (void)priority;
    (void)timeout;
    (void)batchId;
    if (const int result = priorityEncodeFloatError(state, samples, frameSize); result != 0) return result;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceOpusSilkEncGetAjmContext(const OpusSilkEncoder* state, std::uint32_t* context) {
    if (state == nullptr) return encoderError(codes.nullErrorOffset);
    if (context == nullptr) return 0;
    NotImplemented_nid_no_patch(__func__);
}

} // extern "C"
