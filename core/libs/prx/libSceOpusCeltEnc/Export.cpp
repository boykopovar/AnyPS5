#include "prx/libSceOpusCeltEnc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceOpusCeltEnc/include/OpusEncoder.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdarg>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <unordered_map>
#include <opus_multistream.h>
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
constexpr std::uint32_t bitrateErrorOffset = 0x09u;
constexpr std::uint32_t vbrErrorOffset = 0x0Au;
constexpr std::uint32_t vbrConstraintErrorOffset = 0x0Bu;
constexpr std::uint32_t forceChannelsErrorOffset = 0x0Cu;
constexpr std::uint32_t bandwidthErrorOffset = 0x0Eu;
constexpr std::uint32_t maxBandwidthErrorOffset = 0x0Fu;
constexpr std::uint32_t signalErrorOffset = 0x12u;
constexpr std::uint32_t complexityErrorOffset = 0x13u;
constexpr std::uint32_t packetLossErrorOffset = 0x14u;
constexpr std::uint32_t ctlNullArgErrorOffset = 0x19u;
constexpr std::uint32_t streamsErrorOffset = 0x1Bu;
constexpr std::uint32_t coupledStreamsErrorOffset = 0x1Cu;
constexpr std::uint32_t mappingErrorOffset = 0x1Du;
constexpr std::uint32_t predictionErrorOffset = 0x1Eu;
constexpr std::size_t multistreamStateSize = 0x2A0u;
constexpr std::size_t multistreamInstanceOffset = 0x00u;
constexpr std::size_t multistreamFlagsOffset = 0x08u;
constexpr std::size_t multistreamStreamsOffset = 0x10u;
constexpr std::size_t multistreamCoupledOffset = 0x14u;
constexpr std::size_t multistreamMappingOffset = 0x18u;
constexpr std::size_t multistreamRateOffset = 0x24u;
constexpr std::size_t multistreamActiveBitrateOffset = 0x28u;
constexpr std::size_t multistreamActiveVbrOffset = 0x2Cu;
constexpr std::size_t multistreamActiveBandwidthOffset = 0x30u;
constexpr std::size_t multistreamComplexityOffset = 0x34u;
constexpr std::size_t multistreamForceChannelsOffset = 0x38u;
constexpr std::size_t multistreamPacketLossOffset = 0x3Cu;
constexpr std::size_t multistreamSignalOffset = 0x40u;
constexpr std::size_t multistreamPredictionOffset = 0x48u;
constexpr std::size_t multistreamActiveFrameOffset = 0x4Cu;
constexpr std::size_t multistreamResult0Offset = 0x70u;
constexpr std::size_t multistreamResult1Offset = 0x74u;
constexpr std::size_t multistreamApplicationOffset = 0x78u;
constexpr std::size_t multistreamMaxBandwidthOffset = 0x7Cu;
constexpr std::size_t multistreamBandwidthOffset = 0x80u;
constexpr std::size_t multistreamVbrOffset = 0x84u;
constexpr std::size_t multistreamVbrConstraintOffset = 0x88u;
constexpr std::size_t multistreamBitrateOffset = 0x8Cu;
constexpr std::size_t multistreamEffectiveBitrateOffset = 0x90u;
constexpr std::size_t multistreamFrameOffset = 0x94u;
constexpr std::size_t multistreamMagicOffset = 0x98u;
constexpr std::size_t multistreamDirtyOffset = 0x9Cu;
constexpr std::size_t multistreamContextOffset = 0xA0u;
constexpr std::uint32_t lowestPriority = 20u;
constexpr std::uint32_t highestPriority = 100u;
constexpr std::uint32_t firstRequest = 4000u;
constexpr std::uint32_t requestCount = 48u;
constexpr int requestMinusOne = -1;
constexpr int requestMinusTwo = -2;
constexpr std::uint32_t maximumStreams = 12u;
constexpr std::array<std::uint32_t, 5> frameFactors{400u, 200u, 100u, 50u, 25u};

struct MultistreamEncoderEntry {
    int streams;
    int coupledStreams;
    int sampleRate;
    std::unique_ptr<OpusMSEncoder, void (*)(OpusMSEncoder*)> codec{nullptr, opus_multistream_encoder_destroy};
};

std::mutex multistreamMutex;
std::unordered_map<std::uint32_t, std::unique_ptr<MultistreamEncoderEntry>> multistreamEncoders;
std::uint32_t nextMultistreamInstanceId = 1;

int encoderError(std::uint32_t offset) {
    return static_cast<int>(codes.baseError + offset);
}

void copyInPreserveErrno(void* dst, const void* src, std::size_t size, const char* message) {
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(dst, src, size);
    errno = savedErrno;
    if (!ok) throw std::runtime_error(message);
}

void copyOutPreserveErrno(void* dst, const void* src, std::size_t size, const char* message) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(dst, src, size);
    errno = savedErrno;
    if (!ok) throw std::runtime_error(message);
}

std::uint32_t guestStateWord(const void* state, std::size_t offset) {
    std::uint32_t word = 0;
    copyInPreserveErrno(&word, static_cast<const std::uint8_t*>(state) + offset, sizeof word, "OpusCeltEnc: unreadable state");
    return word;
}

void writeGuestStateWord(void* state, std::size_t offset, std::uint32_t value) {
    copyOutPreserveErrno(static_cast<std::uint8_t*>(state) + offset, &value, sizeof value, "OpusCeltEnc: unwritable state");
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

bool acceptedMultistreamBandwidth(int value) {
    return value == OPUS_BANDWIDTH_NARROWBAND || value == OPUS_BANDWIDTH_WIDEBAND ||
           value == OPUS_BANDWIDTH_SUPERWIDEBAND || value == OPUS_BANDWIDTH_FULLBAND;
}

int multistreamCreateError(const void* state, int sampleRate, int streams, int coupledStreams, const std::uint8_t* mapping, int application, std::array<std::uint8_t, maximumStreams>& entries) {
    if (state == nullptr) return encoderError(codes.nullErrorOffset);
    if (!acceptedMultistreamRate(sampleRate)) return encoderError(codes.rateErrorOffset);
    const auto streamCount = static_cast<std::uint32_t>(streams);
    const auto coupledCount = static_cast<std::uint32_t>(coupledStreams);
    if (streamCount - 1u >= maximumStreams) return encoderError(streamsErrorOffset);
    if (coupledCount > streamCount) return encoderError(coupledStreamsErrorOffset);
    const std::uint32_t channels = streamCount + coupledCount;
    if (channels > maximumStreams) return encoderError(codes.channelsErrorOffset);
    entries.fill(0);
    copyInPreserveErrno(entries.data(), mapping, channels, "OpusCeltEnc: unreadable channel mapping");
    for (std::uint32_t channel = 0; channel < channels; ++channel)
        if (std::count(entries.begin(), entries.begin() + channels, channel) != 1) return encoderError(mappingErrorOffset);
    if (application != OPUS_APPLICATION_RESTRICTED_LOWDELAY) return encoderError(codes.applicationErrorOffset);
    return 0;
}

void writeMultistreamStateWords(std::array<std::uint8_t, multistreamStateSize>& stateBytes, std::uint32_t instanceId, std::uint32_t contextId, int sampleRate, int streams, int coupledStreams, const std::array<std::uint8_t, maximumStreams>& mapping) {
    stateBytes.fill(0);
    const int channels = streams + coupledStreams;
    const int flagChannels = (streams < 6) ? channels : (channels >= 10 ? channels : 9);
    const std::uint64_t flags = 0x100000001ULL | (static_cast<std::uint64_t>(flagChannels) << 3);
    const std::uint32_t computedBitrate = static_cast<std::uint32_t>(channels * sampleRate + 0xBB8);
    const std::uint32_t frameWord = static_cast<std::uint32_t>(sampleRate / 50);
    writeWord(stateBytes.data(), multistreamInstanceOffset, instanceId);
    std::memcpy(stateBytes.data() + multistreamFlagsOffset, &flags, sizeof flags);
    writeWord(stateBytes.data(), multistreamStreamsOffset, static_cast<std::uint32_t>(streams));
    writeWord(stateBytes.data(), multistreamCoupledOffset, static_cast<std::uint32_t>(coupledStreams));
    std::memcpy(stateBytes.data() + multistreamMappingOffset, mapping.data(), static_cast<std::size_t>(channels));
    writeWord(stateBytes.data(), multistreamRateOffset, static_cast<std::uint32_t>(sampleRate));
    writeWord(stateBytes.data(), multistreamActiveBitrateOffset, computedBitrate);
    writeWord(stateBytes.data(), multistreamActiveVbrOffset, 2u);
    writeWord(stateBytes.data(), multistreamActiveBandwidthOffset, static_cast<std::uint32_t>(OPUS_BANDWIDTH_FULLBAND));
    writeWord(stateBytes.data(), multistreamComplexityOffset, 10u);
    writeWord(stateBytes.data(), multistreamActiveFrameOffset, frameWord);
    writeWord(stateBytes.data(), multistreamApplicationOffset, static_cast<std::uint32_t>(OPUS_APPLICATION_RESTRICTED_LOWDELAY));
    writeWord(stateBytes.data(), multistreamMaxBandwidthOffset, static_cast<std::uint32_t>(OPUS_BANDWIDTH_FULLBAND));
    writeWord(stateBytes.data(), multistreamBandwidthOffset, static_cast<std::uint32_t>(OPUS_BANDWIDTH_FULLBAND));
    writeWord(stateBytes.data(), multistreamVbrOffset, 1u);
    writeWord(stateBytes.data(), multistreamVbrConstraintOffset, 1u);
    writeWord(stateBytes.data(), multistreamBitrateOffset, static_cast<std::uint32_t>(OPUS_AUTO));
    writeWord(stateBytes.data(), multistreamEffectiveBitrateOffset, computedBitrate);
    writeWord(stateBytes.data(), multistreamFrameOffset, frameWord);
    writeWord(stateBytes.data(), multistreamMagicOffset, codes.stateMagic);
    writeWord(stateBytes.data(), multistreamDirtyOffset, 0u);
    writeWord(stateBytes.data(), multistreamContextOffset, contextId);
}

int createMultistreamEncoder(std::uint32_t contextId, OpusCeltMultistreamEncoder* state, int sampleRate, int streams, int coupledStreams, const std::array<std::uint8_t, maximumStreams>& mapping) {
    const int channels = streams + coupledStreams;
    std::lock_guard lock(multistreamMutex);
    if (hasStateMarker(state, multistreamMagicOffset)) {
        multistreamEncoders.erase(guestStateWord(state, multistreamInstanceOffset));
    }
    const std::uint32_t instanceId = nextMultistreamInstanceId++;
    auto entry = std::make_unique<MultistreamEncoderEntry>();
    entry->streams = streams;
    entry->coupledStreams = coupledStreams;
    entry->sampleRate = sampleRate;
    int opusError = OPUS_OK;
    entry->codec.reset(opus_multistream_encoder_create(sampleRate, channels, streams, coupledStreams, mapping.data(), OPUS_APPLICATION_RESTRICTED_CELT, &opusError));
    if (opusError == OPUS_ALLOC_FAIL) {
        std::array<std::uint8_t, multistreamStateSize> cleared{};
        copyOutPreserveErrno(state, cleared.data(), cleared.size(), "OpusCeltEnc: unwritable multistream state");
        throw std::bad_alloc();
    }
    if (!entry->codec || opusError != OPUS_OK) {
        std::array<std::uint8_t, multistreamStateSize> cleared{};
        copyOutPreserveErrno(state, cleared.data(), cleared.size(), "OpusCeltEnc: unwritable multistream state");
        throw std::runtime_error("OpusCeltEnc: cannot create libopus multistream encoder");
    }
    std::array<std::uint8_t, multistreamStateSize> stateBytes{};
    writeMultistreamStateWords(stateBytes, instanceId, contextId, sampleRate, streams, coupledStreams, mapping);
    copyOutPreserveErrno(state, stateBytes.data(), stateBytes.size(), "OpusCeltEnc: unwritable multistream state");
    multistreamEncoders.emplace(instanceId, std::move(entry));
    return 0;
}

void resetMultistreamState(OpusCeltMultistreamEncoder* state) {
    const int streams = static_cast<int>(guestStateWord(state, multistreamStreamsOffset));
    const int coupledStreams = static_cast<int>(guestStateWord(state, multistreamCoupledOffset));
    const int sampleRate = static_cast<int>(guestStateWord(state, multistreamRateOffset));
    const int channels = streams + coupledStreams;
    const std::uint32_t computedBitrate = static_cast<std::uint32_t>(channels * sampleRate + 0xBB8);
    const std::uint32_t frameWord = static_cast<std::uint32_t>(sampleRate / 50);
    writeGuestStateWord(state, multistreamDirtyOffset, 0u);
    writeGuestStateWord(state, multistreamComplexityOffset, 10u);
    writeGuestStateWord(state, multistreamForceChannelsOffset, 0u);
    writeGuestStateWord(state, multistreamPacketLossOffset, 0u);
    writeGuestStateWord(state, multistreamSignalOffset, 0u);
    writeGuestStateWord(state, multistreamSignalOffset + 4u, 0u);
    writeGuestStateWord(state, multistreamPredictionOffset, 0u);
    writeGuestStateWord(state, multistreamMaxBandwidthOffset, static_cast<std::uint32_t>(OPUS_BANDWIDTH_FULLBAND));
    writeGuestStateWord(state, multistreamBandwidthOffset, static_cast<std::uint32_t>(OPUS_BANDWIDTH_FULLBAND));
    writeGuestStateWord(state, multistreamVbrOffset, 1u);
    writeGuestStateWord(state, multistreamVbrConstraintOffset, 1u);
    writeGuestStateWord(state, multistreamBitrateOffset, static_cast<std::uint32_t>(OPUS_AUTO));
    writeGuestStateWord(state, multistreamEffectiveBitrateOffset, computedBitrate);
    writeGuestStateWord(state, multistreamFrameOffset, frameWord);
    writeGuestStateWord(state, multistreamActiveBandwidthOffset, static_cast<std::uint32_t>(OPUS_BANDWIDTH_FULLBAND));
    writeGuestStateWord(state, multistreamActiveVbrOffset, 2u);
    writeGuestStateWord(state, multistreamActiveBitrateOffset, computedBitrate);
    writeGuestStateWord(state, multistreamActiveFrameOffset, frameWord);
}

OpusMSEncoder* findMultistreamCodecLocked(const OpusCeltMultistreamEncoder* state) {
    const auto found = multistreamEncoders.find(guestStateWord(state, multistreamInstanceOffset));
    if (found == multistreamEncoders.end()) return nullptr;
    return found->second->codec.get();
}

int applyMultistreamSetter(OpusCeltMultistreamEncoder* state, int request, std::size_t offset, std::uint32_t storedValue, int codecArg, std::uint32_t errorOffset) {
    std::lock_guard lock(multistreamMutex);
    if (!hasStateMarker(state, multistreamMagicOffset)) return encoderError(codes.destroyStateErrorOffset);
    if (OpusMSEncoder* codec = findMultistreamCodecLocked(state)) {
        if (opus_multistream_encoder_ctl(codec, request, static_cast<opus_int32>(codecArg)) != OPUS_OK) {
            return encoderError(errorOffset);
        }
    }
    writeGuestStateWord(state, offset, storedValue);
    writeGuestStateWord(state, multistreamDirtyOffset, 1u);
    return 0;
}

int dispatchMultistreamCtl(OpusCeltMultistreamEncoder* state, int request, std::uintptr_t arg0, std::uintptr_t arg1) {
    if (request == requestMinusOne) {
        auto* out0 = reinterpret_cast<std::int32_t*>(arg0);
        auto* out1 = reinterpret_cast<std::int32_t*>(arg1);
        if (out0 == nullptr || out1 == nullptr) return encoderError(ctlNullArgErrorOffset);
        const std::uint32_t v0 = guestStateWord(state, multistreamResult0Offset);
        const std::uint32_t v1 = guestStateWord(state, multistreamResult1Offset);
        copyOutPreserveErrno(out0, &v0, sizeof v0, "OpusCeltEnc: unwritable ctl output");
        copyOutPreserveErrno(out1, &v1, sizeof v1, "OpusCeltEnc: unwritable ctl output");
        return 0;
    }

    const auto readGuestOutput = [&](std::size_t offset) -> int {
        auto* out = reinterpret_cast<std::int32_t*>(arg0);
        if (out == nullptr) return encoderError(ctlNullArgErrorOffset);
        const std::uint32_t word = guestStateWord(state, offset);
        copyOutPreserveErrno(out, &word, sizeof word, "OpusCeltEnc: unwritable ctl output");
        return 0;
    };

    const int intArg = static_cast<int>(static_cast<std::intptr_t>(arg0));

    switch (request) {
    case OPUS_SET_APPLICATION_REQUEST:
        if (intArg != OPUS_APPLICATION_RESTRICTED_LOWDELAY) return encoderError(codes.applicationErrorOffset);
        writeGuestStateWord(state, multistreamApplicationOffset, static_cast<std::uint32_t>(OPUS_APPLICATION_RESTRICTED_LOWDELAY));
        return 0;
    case OPUS_GET_APPLICATION_REQUEST:
        return readGuestOutput(multistreamApplicationOffset);
    case OPUS_SET_BITRATE_REQUEST: {
        if (intArg != OPUS_AUTO && intArg != OPUS_BITRATE_MAX) {
            const int streams = static_cast<int>(guestStateWord(state, multistreamStreamsOffset));
            const int coupledStreams = static_cast<int>(guestStateWord(state, multistreamCoupledOffset));
            const int maxBitrate = (streams <= 5 && streams + coupledStreams <= 8) ? 720000 : 1080000;
            if (intArg < 6000 || intArg > maxBitrate) return encoderError(bitrateErrorOffset);
        }
        return applyMultistreamSetter(state, OPUS_SET_BITRATE_REQUEST, multistreamBitrateOffset, static_cast<std::uint32_t>(intArg), intArg, bitrateErrorOffset);
    }
    case OPUS_GET_BITRATE_REQUEST:
        return readGuestOutput(multistreamEffectiveBitrateOffset);
    case OPUS_SET_MAX_BANDWIDTH_REQUEST:
        if (!acceptedMultistreamBandwidth(intArg)) return encoderError(maxBandwidthErrorOffset);
        return applyMultistreamSetter(state, OPUS_SET_MAX_BANDWIDTH_REQUEST, multistreamMaxBandwidthOffset, static_cast<std::uint32_t>(intArg), intArg, maxBandwidthErrorOffset);
    case OPUS_GET_MAX_BANDWIDTH_REQUEST:
        return readGuestOutput(multistreamMaxBandwidthOffset);
    case OPUS_SET_VBR_REQUEST:
        if (static_cast<std::uint32_t>(intArg) > 1u) return encoderError(vbrErrorOffset);
        return applyMultistreamSetter(state, OPUS_SET_VBR_REQUEST, multistreamVbrOffset, static_cast<std::uint32_t>(intArg), intArg, vbrErrorOffset);
    case OPUS_GET_VBR_REQUEST:
        return readGuestOutput(multistreamVbrOffset);
    case OPUS_SET_BANDWIDTH_REQUEST:
        if (intArg != OPUS_AUTO && !acceptedMultistreamBandwidth(intArg)) return encoderError(bandwidthErrorOffset);
        return applyMultistreamSetter(state, OPUS_SET_BANDWIDTH_REQUEST, multistreamBandwidthOffset, static_cast<std::uint32_t>(intArg), intArg, bandwidthErrorOffset);
    case OPUS_GET_BANDWIDTH_REQUEST:
        return readGuestOutput(multistreamBandwidthOffset);
    case OPUS_SET_COMPLEXITY_REQUEST:
        if (static_cast<std::uint32_t>(intArg) > 10u) return encoderError(complexityErrorOffset);
        return applyMultistreamSetter(state, OPUS_SET_COMPLEXITY_REQUEST, multistreamComplexityOffset, static_cast<std::uint32_t>(intArg), intArg, complexityErrorOffset);
    case OPUS_GET_COMPLEXITY_REQUEST:
        return readGuestOutput(multistreamComplexityOffset);
    case OPUS_SET_PACKET_LOSS_PERC_REQUEST:
        if (static_cast<std::uint32_t>(intArg) > 100u) return encoderError(packetLossErrorOffset);
        return applyMultistreamSetter(state, OPUS_SET_PACKET_LOSS_PERC_REQUEST, multistreamPacketLossOffset, static_cast<std::uint32_t>(intArg), intArg, packetLossErrorOffset);
    case OPUS_GET_PACKET_LOSS_PERC_REQUEST:
        return readGuestOutput(multistreamPacketLossOffset);
    case OPUS_SET_VBR_CONSTRAINT_REQUEST:
        if (static_cast<std::uint32_t>(intArg) > 1u) return encoderError(vbrConstraintErrorOffset);
        return applyMultistreamSetter(state, OPUS_SET_VBR_CONSTRAINT_REQUEST, multistreamVbrConstraintOffset, static_cast<std::uint32_t>(intArg), intArg, vbrConstraintErrorOffset);
    case OPUS_GET_VBR_CONSTRAINT_REQUEST:
        return readGuestOutput(multistreamVbrConstraintOffset);
    case OPUS_SET_FORCE_CHANNELS_REQUEST: {
        if (intArg != 1 && intArg != 2 && intArg != OPUS_AUTO) return encoderError(forceChannelsErrorOffset);
        const std::uint32_t stored = (intArg == OPUS_AUTO) ? 0u : static_cast<std::uint32_t>(intArg);
        return applyMultistreamSetter(state, OPUS_SET_FORCE_CHANNELS_REQUEST, multistreamForceChannelsOffset, stored, intArg, forceChannelsErrorOffset);
    }
    case OPUS_GET_FORCE_CHANNELS_REQUEST:
        return readGuestOutput(multistreamForceChannelsOffset);
    case OPUS_SET_SIGNAL_REQUEST: {
        if (intArg != OPUS_SIGNAL_VOICE && intArg != OPUS_SIGNAL_MUSIC && intArg != OPUS_AUTO) return encoderError(signalErrorOffset);
        const std::uint32_t stored = (intArg == OPUS_AUTO) ? 0u : static_cast<std::uint32_t>(intArg);
        return applyMultistreamSetter(state, OPUS_SET_SIGNAL_REQUEST, multistreamSignalOffset, stored, intArg, signalErrorOffset);
    }
    case OPUS_GET_SIGNAL_REQUEST:
        return readGuestOutput(multistreamSignalOffset);
    case OPUS_GET_LOOKAHEAD_REQUEST: {
        auto* out = reinterpret_cast<std::int32_t*>(arg0);
        if (out == nullptr) return encoderError(ctlNullArgErrorOffset);
        const std::uint32_t lookahead = guestStateWord(state, multistreamRateOffset) / 400u;
        copyOutPreserveErrno(out, &lookahead, sizeof lookahead, "OpusCeltEnc: unwritable ctl output");
        return 0;
    }
    case OPUS_RESET_STATE: {
        std::lock_guard lock(multistreamMutex);
        if (!hasStateMarker(state, multistreamMagicOffset)) return encoderError(codes.destroyStateErrorOffset);
        if (OpusMSEncoder* codec = findMultistreamCodecLocked(state)) {
            if (opus_multistream_encoder_ctl(codec, OPUS_RESET_STATE) != OPUS_OK) {
                throw std::runtime_error("OpusCeltEnc: libopus did not reset the multistream encoder");
            }
        }
        resetMultistreamState(state);
        return 0;
    }
    case OPUS_GET_SAMPLE_RATE_REQUEST:
        return readGuestOutput(multistreamRateOffset);
    case OPUS_SET_PREDICTION_DISABLED_REQUEST:
        if (static_cast<std::uint32_t>(intArg) > 1u) return encoderError(predictionErrorOffset);
        return applyMultistreamSetter(state, OPUS_SET_PREDICTION_DISABLED_REQUEST, multistreamPredictionOffset, static_cast<std::uint32_t>(intArg), intArg, predictionErrorOffset);
    case OPUS_GET_PREDICTION_DISABLED_REQUEST:
        return readGuestOutput(multistreamPredictionOffset);
    default:
        return encoderError(requestErrorOffset);
    }
}

bool multistreamCtlNeedsTwoArgs(int request) {
    return request == requestMinusOne;
}

bool multistreamCtlNeedsOneArg(int request) {
    switch (request) {
    case OPUS_SET_APPLICATION_REQUEST:
    case OPUS_GET_APPLICATION_REQUEST:
    case OPUS_SET_BITRATE_REQUEST:
    case OPUS_GET_BITRATE_REQUEST:
    case OPUS_SET_MAX_BANDWIDTH_REQUEST:
    case OPUS_GET_MAX_BANDWIDTH_REQUEST:
    case OPUS_SET_VBR_REQUEST:
    case OPUS_GET_VBR_REQUEST:
    case OPUS_SET_BANDWIDTH_REQUEST:
    case OPUS_GET_BANDWIDTH_REQUEST:
    case OPUS_SET_COMPLEXITY_REQUEST:
    case OPUS_GET_COMPLEXITY_REQUEST:
    case OPUS_SET_PACKET_LOSS_PERC_REQUEST:
    case OPUS_GET_PACKET_LOSS_PERC_REQUEST:
    case OPUS_SET_VBR_CONSTRAINT_REQUEST:
    case OPUS_GET_VBR_CONSTRAINT_REQUEST:
    case OPUS_SET_FORCE_CHANNELS_REQUEST:
    case OPUS_GET_FORCE_CHANNELS_REQUEST:
    case OPUS_SET_SIGNAL_REQUEST:
    case OPUS_GET_SIGNAL_REQUEST:
    case OPUS_GET_LOOKAHEAD_REQUEST:
    case OPUS_GET_SAMPLE_RATE_REQUEST:
    case OPUS_SET_PREDICTION_DISABLED_REQUEST:
    case OPUS_GET_PREDICTION_DISABLED_REQUEST:
        return true;
    default:
        return false;
    }
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
#ifdef _WIN32
    __builtin_sysv_va_list list;
    __builtin_sysv_va_start(list, request);
    const std::int32_t value = OpusEncoderLibrary::ReadCtlValue(request, [&] { return __builtin_va_arg(list, int); });
    __builtin_sysv_va_end(list);
#else
    std::va_list list;
    va_start(list, request);
    const std::int32_t value = OpusEncoderLibrary::ReadCtlValue(request, [&] { return va_arg(list, int); });
    va_end(list);
#endif
    return library.Ctl(state, request, value);
}

int APS5_VABI sceOpusCeltEncEncodeWithPriority(OpusCeltEncoder* state, const std::int16_t* samples, int frameSize, std::uint8_t* data, int maxDataBytes, std::uint32_t priority) {
    if (state == nullptr) return encoderError(codes.nullErrorOffset);
    if (samples == nullptr) return encoderError(codes.nullSamplesErrorOffset);
    if (data == nullptr) return encoderError(codes.nullDataErrorOffset);
    if (maxDataBytes == 0) return encoderError(codes.sizeErrorOffset);
    if (priority < lowestPriority || priority > highestPriority) return encoderError(priorityErrorOffset);
    if (!hasStateMarker(state, codes.stateMagicOffset)) return encoderError(codes.destroyStateErrorOffset);
    if (!acceptedPriorityFrame(guestStateWord(state, codes.stateRateOffset), static_cast<std::uint32_t>(frameSize))) return encoderError(codes.frameErrorOffset);
    return library.Encode(state, samples, frameSize, data, maxDataBytes);
}

int APS5_VABI sceOpusCeltEncEncodeFloatWithPriority(OpusCeltEncoder* state, const float* samples, int frameSize, std::uint8_t* data, int maxDataBytes, std::uint32_t priority) {
    if (state == nullptr) return encoderError(codes.nullErrorOffset);
    if (samples == nullptr) return encoderError(codes.nullSamplesErrorOffset);
    if (frameSize == 0) return encoderError(codes.frameErrorOffset);
    if (!hasStateMarker(state, codes.stateMagicOffset)) return encoderError(codes.destroyStateErrorOffset);
    if (frameSize < 0 || static_cast<std::uint64_t>(frameSize) * guestStateWord(state, codes.stateChannelsOffset) * 4u > 0x800000u) {
        return library.EncodeFloat(state, samples, frameSize, data, maxDataBytes);
    }
    if (data == nullptr) return encoderError(codes.nullDataErrorOffset);
    if (maxDataBytes == 0) return encoderError(codes.sizeErrorOffset);
    if (priority < lowestPriority || priority > highestPriority) return encoderError(priorityErrorOffset);
    if (!acceptedPriorityFrame(guestStateWord(state, codes.stateRateOffset), static_cast<std::uint32_t>(frameSize))) return encoderError(codes.frameErrorOffset);
    return library.EncodeFloat(state, samples, frameSize, data, maxDataBytes);
}

int APS5_VABI sceOpusCeltEncGetAjmContext(const OpusCeltEncoder* state, std::uint32_t* context) {
    if (state == nullptr) return encoderError(codes.nullErrorOffset);
    if (context == nullptr) return 0;
    std::uint32_t value = 0;
    if (!GuestCopyIn_nid_no_patch(&value, reinterpret_cast<const std::uint8_t*>(state) + 0xD4, sizeof(value))) throw std::runtime_error("OpusCeltEnc: unreadable AJM context");
    if (!GuestCopyOut_nid_no_patch(context, &value, sizeof(value))) throw std::runtime_error("OpusCeltEnc: unwritable AJM context");
    return 0;
}

int APS5_VABI sceOpusCeltMultistreamEncCreate(OpusCeltMultistreamEncoder* state, int sampleRate, int streams, int coupledStreams, const std::uint8_t* mapping, int application) {
    std::array<std::uint8_t, maximumStreams> validatedMapping{};
    if (const int result = multistreamCreateError(state, sampleRate, streams, coupledStreams, mapping, application, validatedMapping); result != 0) return result;
    return createMultistreamEncoder(0u, state, sampleRate, streams, coupledStreams, validatedMapping);
}

int APS5_VABI sceOpusCeltMultistreamEncCtl(OpusCeltMultistreamEncoder* state, int request, ...) {
    if (state == nullptr) return encoderError(codes.nullErrorOffset);
    if (!hasStateMarker(state, multistreamMagicOffset)) return encoderError(codes.destroyStateErrorOffset);
    if (outsideRequests(request) && request != requestMinusOne) return encoderError(requestErrorOffset);
    std::uintptr_t arg0 = 0;
    std::uintptr_t arg1 = 0;
#ifdef _WIN32
    __builtin_sysv_va_list list;
    __builtin_sysv_va_start(list, request);
    if (multistreamCtlNeedsTwoArgs(request)) {
        arg0 = __builtin_va_arg(list, std::uintptr_t);
        arg1 = __builtin_va_arg(list, std::uintptr_t);
    } else if (multistreamCtlNeedsOneArg(request)) {
        arg0 = __builtin_va_arg(list, std::uintptr_t);
    }
    __builtin_sysv_va_end(list);
#else
    std::va_list list;
    va_start(list, request);
    if (multistreamCtlNeedsTwoArgs(request)) {
        arg0 = va_arg(list, std::uintptr_t);
        arg1 = va_arg(list, std::uintptr_t);
    } else if (multistreamCtlNeedsOneArg(request)) {
        arg0 = va_arg(list, std::uintptr_t);
    }
    va_end(list);
#endif
    return dispatchMultistreamCtl(state, request, arg0, arg1);
}

int APS5_VABI sceOpusCeltMultistreamEncCreateEx(const std::uint32_t* context, OpusCeltMultistreamEncoder* state, int sampleRate, int streams, int coupledStreams, const std::uint8_t* mapping, int application) {
    if (context == nullptr) return encoderError(codes.nullErrorOffset);
    std::array<std::uint8_t, maximumStreams> validatedMapping{};
    if (const int result = multistreamCreateError(state, sampleRate, streams, coupledStreams, mapping, application, validatedMapping); result != 0) return result;
    std::uint32_t contextId = 0;
    copyInPreserveErrno(&contextId, context, sizeof contextId, "OpusCeltEnc: unreadable context");
    return createMultistreamEncoder(contextId, state, sampleRate, streams, coupledStreams, validatedMapping);
}

} // extern "C"
