#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <string>

extern "C" {
int APS5_VABI sceOpusDecInitialize(std::uint32_t*);
int APS5_VABI sceOpusDecTerminate(std::uint32_t*);
int APS5_VABI sceOpusDecGetSize(int);
int APS5_VABI sceOpusDecCreateEx(std::uint32_t*, void*, int, int);
int APS5_VABI sceOpusDecDecode(void*, const std::uint8_t*, int, std::int16_t*, int);
int APS5_VABI sceOpusDecDestroy(void*);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;
using Testing::RequireThrows;

constexpr std::uint8_t opusStereo[] = {
    0x46, 0x00, 0x7C, 0x87, 0xFC, 0xB1, 0x1D, 0xC0, 0xE3, 0x07, 0xD5, 0x9C, 0x4D, 0x91, 0x62, 0x6B, 0x73, 0x86, 0x05, 0xE8, 0xDB, 0xC6, 0x23, 0x6D,
    0x5E, 0x6F, 0x73, 0xF8, 0xF2, 0x47, 0xD9, 0x5E, 0xEA, 0xB3, 0xD2, 0x74, 0x6C, 0xE0, 0xCE, 0xE2, 0x3C, 0xFA, 0x59, 0x4A, 0xBA, 0x8F, 0x76, 0x0F,
    0x15, 0xE1, 0x3E, 0x60, 0xDF, 0x80, 0xC5, 0x44, 0xB1, 0x1B, 0xEF, 0x43, 0x03, 0x4F, 0xF2, 0xDE, 0xE3, 0xAD, 0x8B, 0x20, 0xFF, 0x68, 0x0C, 0x0A,
    0x3F, 0x00, 0x7C, 0x87, 0xFD, 0x45, 0xBD, 0x12, 0x00, 0xA5, 0xB1, 0xA0, 0x0A, 0x62, 0x24, 0x6F, 0x63, 0x70, 0xA5, 0x35, 0x13, 0x03, 0x74, 0x0D,
    0x83, 0xC9, 0x74, 0x1B, 0x79, 0xED, 0xA3, 0x09, 0x83, 0xA5, 0x02, 0xC4, 0x60, 0x49, 0xC9, 0xB6, 0x9A, 0x60, 0xAD, 0x6C, 0x77, 0x84, 0xE5, 0xC2,
    0xCE, 0x2E, 0x71, 0x07, 0x7E, 0x40, 0xCB, 0xDC, 0x13, 0xDF, 0xCF, 0x84, 0x5D, 0x55, 0x3B, 0x6B, 0x6F, 0x44, 0x00, 0x7C, 0x88, 0x01, 0xE8, 0xA1,
    0x2A, 0x12, 0x7D, 0x5E, 0xF6, 0x74, 0x78, 0xB1, 0x27, 0x2B, 0x8C, 0xF0, 0x7F, 0x95, 0x51, 0x71, 0x28, 0x18, 0xEA, 0x66, 0xEB, 0xCA, 0xAC, 0xDF,
    0x6C, 0x9C, 0xFD, 0xED, 0x88, 0x8E, 0x66, 0xD7, 0xDA, 0x36, 0xD7, 0xEB, 0x5F, 0xA6, 0x05, 0x72, 0xDA, 0x52, 0x14, 0x7F, 0xF5, 0x84, 0x54, 0xA1,
    0xA9, 0xF1, 0xAA, 0xA8, 0x73, 0x9A, 0xCC, 0x91, 0x83, 0x92, 0xD0, 0x7F, 0x3B, 0x6B, 0x65, 0x43, 0x00, 0x7C, 0x88, 0x01, 0xE8, 0xA1, 0x2A, 0x12,
    0x7D, 0x5F, 0x04, 0xF8, 0xF5, 0x1F, 0xA0, 0x85, 0x89, 0xED, 0xBA, 0x7C, 0x5F, 0x63, 0x2A, 0x9E, 0x05, 0xE8, 0x63, 0xD1, 0x73, 0x0C, 0xAF, 0xC8,
    0xC9, 0x22, 0xF7, 0x11, 0x1D, 0x8B, 0xA9, 0x9A, 0x90, 0x77, 0xF2, 0x86, 0x49, 0x15, 0x6E, 0xD6, 0x34, 0x1F, 0x50, 0x15, 0xEC, 0x85, 0xC7, 0xF5,
    0xA7, 0xFA, 0x99, 0xF9, 0x47, 0x32, 0x07, 0xAF, 0x24, 0xBF, 0x14, 0xC5,
};

constexpr int stereoFrameBytes = 3840;
constexpr int monoFrameBytes = 1920;
constexpr std::int16_t guard = 12345;
constexpr std::size_t stateSize = 640;

struct Packet {
    const std::uint8_t* data;
    int bytes;
};

int PacketBytes(std::size_t offset) {
    return opusStereo[offset] | (opusStereo[offset + 1] << 8);
}

Packet PacketAt(int index) {
    std::size_t offset = 0;
    for (int skipped = 0; skipped < index; ++skipped) offset += PacketBytes(offset) + 2;
    return {opusStereo + offset + 2, PacketBytes(offset)};
}

class OpusContext {
public:
    OpusContext() {
        RequireEqual(sceOpusDecInitialize(&id), 0, "initialize an Opus context");
    }

    ~OpusContext() {
        try {
            sceOpusDecTerminate(&id);
        } catch (const std::exception&) {
        }
    }

    OpusContext(const OpusContext&) = delete;
    OpusContext& operator=(const OpusContext&) = delete;

    std::uint32_t* Get() noexcept { return &id; }

private:
    std::uint32_t id = 0;
};

class OpusState {
public:
    OpusState() = default;

    ~OpusState() {
        try {
            sceOpusDecDestroy(bytes.data());
        } catch (const std::exception&) {
        }
    }

    OpusState(const OpusState&) = delete;
    OpusState& operator=(const OpusState&) = delete;

    void* Get() noexcept { return bytes.data(); }

private:
    std::array<std::uint8_t, stateSize> bytes{};
};

class OpusDecoder {
public:
    OpusDecoder(OpusContext& context, int channels) {
        RequireEqual(sceOpusDecCreateEx(context.Get(), state.Get(), 48000, channels), 0,
                     "create a decoder with " + std::to_string(channels) + " channels");
    }

    OpusDecoder(const OpusDecoder&) = delete;
    OpusDecoder& operator=(const OpusDecoder&) = delete;

    void* Get() noexcept { return state.Get(); }

private:
    OpusState state;
};

template<std::size_t TSize>
bool AllGuard(const std::array<std::int16_t, TSize>& samples) {
    return std::all_of(samples.begin(), samples.end(), [](std::int16_t value) { return value == guard; });
}

const Case initializeTwoContexts{"OpusDecInitialize_TwoContexts_ReturnsDistinctIdentifiers", [] {
    OpusContext context;
    OpusContext otherContext;
    Require(*context.Get() != *otherContext.Get(), "two live contexts have distinct identifiers");
}};

const Case initializeTwice{"OpusDecInitialize_AlreadyInitializedContext_Throws", [] {
    OpusContext context;
    RequireThrows<std::runtime_error>([&] { sceOpusDecInitialize(context.Get()); }, "initialize the same context twice");
}};

const Case getSizeSupported{"OpusDecGetSize_MonoAndStereo_Returns640", [] {
    RequireEqual(sceOpusDecGetSize(1), 640, "mono state size");
    RequireEqual(sceOpusDecGetSize(2), 640, "stereo state size");
}};

const Case getSizeUnsupported{"OpusDecGetSize_ThreeChannels_Throws", [] {
    RequireThrows<std::runtime_error>([] { sceOpusDecGetSize(3); }, "three channels");
}};

const Case createStereoAndMono{"OpusDecCreateEx_StereoAndMonoAt48000_ReturnsZero", [] {
    OpusContext context;
    OpusContext otherContext;
    OpusState state;
    OpusState otherState;
    OpusState monoState;
    RequireEqual(sceOpusDecCreateEx(context.Get(), state.Get(), 48000, 2), 0, "stereo decoder");
    RequireEqual(sceOpusDecCreateEx(otherContext.Get(), otherState.Get(), 48000, 2), 0, "stereo decoder on the other context");
    RequireEqual(sceOpusDecCreateEx(context.Get(), monoState.Get(), 48000, 1), 0, "mono decoder on a context with a stereo decoder");
}};

const Case terminateWithLiveDecoder{"OpusDecTerminate_ContextWithLiveDecoder_Throws", [] {
    OpusContext context;
    OpusDecoder decoder(context, 2);
    RequireThrows<std::runtime_error>([&] { sceOpusDecTerminate(context.Get()); }, "terminate a context with a live decoder");
}};

const Case createOnOwnedState{"OpusDecCreateEx_StateThatOwnsDecoder_Throws", [] {
    OpusContext context;
    OpusDecoder decoder(context, 2);
    RequireThrows<std::runtime_error>([&] { sceOpusDecCreateEx(context.Get(), decoder.Get(), 48000, 2); },
                                      "create a second decoder on the same state");
}};

const Case createUnsupportedRate{"OpusDecCreateEx_SampleRateOtherThan48000_Throws", [] {
    OpusContext context;
    for (const int rate : {8000, 12000, 16000, 24000}) {
        OpusState state;
        RequireThrows<std::runtime_error>([&] { sceOpusDecCreateEx(context.Get(), state.Get(), rate, 2); },
                                          "sample rate " + std::to_string(rate));
    }
}};

const Case decodeTooSmall{"OpusDecDecode_BufferOneByteTooSmall_ThrowsAndLeavesPcmUntouched", [] {
    OpusContext context;
    OpusDecoder decoder(context, 2);
    std::array<std::int16_t, 1922> pcm{};
    pcm.fill(guard);
    RequireThrows<std::runtime_error>(
        [&] { sceOpusDecDecode(decoder.Get(), opusStereo + 2, 70, pcm.data() + 1, stereoFrameBytes - 1); }, "capacity 3839");
    Require(AllGuard(pcm), "PCM untouched after a too small buffer");
}};

const Case decodeMalformed{"OpusDecDecode_MalformedPacket_ThrowsAndLeavesPcmUntouched", [] {
    OpusContext context;
    OpusDecoder decoder(context, 2);
    std::array<std::int16_t, 1922> pcm{};
    pcm.fill(guard);
    const std::uint8_t malformed[] = {0x7f};
    RequireThrows<std::runtime_error>(
        [&] { sceOpusDecDecode(decoder.Get(), malformed, 1, pcm.data() + 1, stereoFrameBytes); }, "packet {0x7f}");
    Require(AllGuard(pcm), "PCM untouched after a malformed packet");
}};

const Case decodeNullPacket{"OpusDecDecode_NullEmptyPacket_ThrowsAndLeavesPcmUntouched", [] {
    OpusContext context;
    OpusDecoder decoder(context, 2);
    std::array<std::int16_t, 1922> pcm{};
    pcm.fill(guard);
    RequireThrows<std::runtime_error>(
        [&] { sceOpusDecDecode(decoder.Get(), nullptr, 0, pcm.data() + 1, stereoFrameBytes); }, "null packet with zero bytes");
    Require(AllGuard(pcm), "PCM untouched after a null packet");
}};

const Case decodeStereoStream{"OpusDecDecode_StereoStream_MatchesIndependentDecoderWithinBounds", [] {
    OpusContext context;
    OpusContext otherContext;
    OpusDecoder decoder(context, 2);
    OpusDecoder otherDecoder(otherContext, 2);
    std::array<std::int16_t, 1922> pcm{};
    std::array<std::int16_t, 1922> reference{};
    pcm.fill(guard);
    for (int frame = 0; frame < 3; ++frame) {
        const auto packet = PacketAt(frame);
        const auto where = "frame " + std::to_string(frame);
        RequireEqual(sceOpusDecDecode(decoder.Get(), packet.data, packet.bytes, pcm.data() + 1, stereoFrameBytes + 1),
                     stereoFrameBytes, where + " decoded bytes");
        RequireEqual(pcm.front(), guard, where + " leading guard sample");
        RequireEqual(pcm.back(), guard, where + " trailing guard sample");
        RequireEqual(sceOpusDecDecode(otherDecoder.Get(), packet.data, packet.bytes, reference.data() + 1, stereoFrameBytes),
                     stereoFrameBytes, where + " reference decoded bytes");
        Require(std::equal(pcm.begin() + 1, pcm.end() - 1, reference.begin() + 1), where + " matches the independent decoder");
    }
}};

const Case decodeFreshMidStream{"OpusDecDecode_FreshDecoderMidStream_DiffersFromContinuedDecoder", [] {
    OpusContext context;
    OpusDecoder decoder(context, 2);
    std::array<std::int16_t, 1922> pcm{};
    for (int frame = 0; frame < 2; ++frame) {
        const auto packet = PacketAt(frame);
        RequireEqual(sceOpusDecDecode(decoder.Get(), packet.data, packet.bytes, pcm.data() + 1, stereoFrameBytes + 1),
                     stereoFrameBytes, "frame " + std::to_string(frame) + " decoded bytes");
    }
    const auto packet = PacketAt(1);
    OpusState freshState;
    RequireEqual(sceOpusDecCreateEx(context.Get(), freshState.Get(), 48000, 2), 0, "create a fresh decoder mid stream");
    std::array<std::int16_t, 1920> fresh{};
    RequireEqual(sceOpusDecDecode(freshState.Get(), packet.data, packet.bytes, fresh.data(), stereoFrameBytes), stereoFrameBytes,
                 "fresh decoder decoded bytes");
    Require(!std::equal(fresh.begin(), fresh.end(), pcm.begin() + 1), "fresh decoder output differs from the continued decoder");
    RequireEqual(sceOpusDecDestroy(freshState.Get()), 0, "destroy the fresh decoder");
}};

const Case decodeStereoPeak{"OpusDecDecode_StereoFramesAfterFirst_PeakWithinExpectedRange", [] {
    OpusContext context;
    OpusDecoder decoder(context, 2);
    std::array<std::int16_t, 1922> pcm{};
    for (int frame = 0; frame < 3; ++frame) {
        const auto packet = PacketAt(frame);
        RequireEqual(sceOpusDecDecode(decoder.Get(), packet.data, packet.bytes, pcm.data() + 1, stereoFrameBytes + 1),
                     stereoFrameBytes, "frame " + std::to_string(frame) + " decoded bytes");
        if (frame == 0) continue;
        int peak = 0;
        for (auto it = pcm.begin() + 1; it != pcm.end() - 1; ++it) peak = std::max(peak, std::abs(static_cast<int>(*it)));
        Require(peak > 900 && peak < 1200,
                "frame " + std::to_string(frame) + " peak " + std::to_string(peak) + " within (900, 1200)");
    }
}};

const Case decodeMonoStream{"OpusDecDecode_MonoStream_ReturnsNonSilentFrameWithinBounds", [] {
    OpusContext context;
    OpusDecoder decoder(context, 1);
    for (int frame = 0; frame < 3; ++frame) {
        const auto packet = PacketAt(frame);
        const auto where = "frame " + std::to_string(frame);
        std::array<std::int16_t, 962> mono{};
        mono.front() = mono.back() = guard;
        RequireEqual(sceOpusDecDecode(decoder.Get(), packet.data, packet.bytes, mono.data() + 1, monoFrameBytes + 1),
                     monoFrameBytes, where + " mono decoded bytes");
        RequireEqual(mono.front(), guard, where + " leading guard sample");
        RequireEqual(mono.back(), guard, where + " trailing guard sample");
        Require(std::any_of(mono.begin() + 1, mono.end() - 1, [](std::int16_t value) { return value != 0; }),
                where + " mono output is not silent");
    }
}};

const Case destroyTwice{"OpusDecDestroy_AlreadyDestroyedState_Throws", [] {
    OpusContext context;
    OpusDecoder decoder(context, 2);
    RequireEqual(sceOpusDecDestroy(decoder.Get()), 0, "first destroy");
    RequireThrows<std::runtime_error>([&] { sceOpusDecDestroy(decoder.Get()); }, "second destroy");
}};

const Case decodeAfterDestroy{"OpusDecDecode_DestroyedState_Throws", [] {
    OpusContext context;
    OpusDecoder decoder(context, 2);
    std::array<std::int16_t, 1922> pcm{};
    RequireEqual(sceOpusDecDestroy(decoder.Get()), 0, "destroy the decoder");
    RequireThrows<std::runtime_error>(
        [&] { sceOpusDecDecode(decoder.Get(), opusStereo + 2, 70, pcm.data(), stereoFrameBytes); }, "decode with a destroyed state");
}};

const Case recreateAfterDestroy{"OpusDecCreateEx_DestroyedState_RecreatesWorkingDecoder", [] {
    OpusContext context;
    OpusState state;
    std::array<std::int16_t, 1922> pcm{};
    RequireEqual(sceOpusDecCreateEx(context.Get(), state.Get(), 48000, 2), 0, "create the decoder");
    RequireEqual(sceOpusDecDestroy(state.Get()), 0, "destroy the decoder");
    RequireEqual(sceOpusDecCreateEx(context.Get(), state.Get(), 48000, 2), 0, "recreate the decoder on the same state");
    RequireEqual(sceOpusDecDecode(state.Get(), opusStereo + 2, 70, pcm.data(), stereoFrameBytes), stereoFrameBytes,
                 "decode the first frame");
}};

const Case decodeInvalidFraming{"OpusDecDecode_InvalidFraming_ThrowsAndLeavesPcmUntouched", [] {
    OpusContext context;
    OpusDecoder decoder(context, 2);
    std::array<std::int16_t, 3842> rejected;
    rejected.fill(guard);
    const std::uint8_t invalidFraming[] = {0x7d, 1};
    RequireThrows<std::runtime_error>(
        [&] { sceOpusDecDecode(decoder.Get(), invalidFraming, 2, rejected.data() + 1, 7680); }, "packet {0x7d, 0x01}");
    Require(AllGuard(rejected), "PCM untouched after invalid framing");
}};

const Case terminateWithoutDecoders{"OpusDecTerminate_ContextWithoutDecoders_ReturnsZero", [] {
    OpusContext context;
    OpusState state;
    OpusState monoState;
    RequireEqual(sceOpusDecCreateEx(context.Get(), state.Get(), 48000, 2), 0, "create the stereo decoder");
    RequireEqual(sceOpusDecCreateEx(context.Get(), monoState.Get(), 48000, 1), 0, "create the mono decoder");
    RequireEqual(sceOpusDecDestroy(state.Get()), 0, "destroy the stereo decoder");
    RequireEqual(sceOpusDecDestroy(monoState.Get()), 0, "destroy the mono decoder");
    RequireEqual(sceOpusDecTerminate(context.Get()), 0, "terminate the context");
}};

const Case terminateTwice{"OpusDecTerminate_TerminatedContext_Throws", [] {
    OpusContext context;
    RequireEqual(sceOpusDecTerminate(context.Get()), 0, "first terminate");
    RequireThrows<std::runtime_error>([&] { sceOpusDecTerminate(context.Get()); }, "second terminate");
}};

const Case createOnTerminated{"OpusDecCreateEx_TerminatedContext_Throws", [] {
    OpusContext context;
    OpusState state;
    RequireEqual(sceOpusDecTerminate(context.Get()), 0, "terminate the context");
    RequireThrows<std::runtime_error>([&] { sceOpusDecCreateEx(context.Get(), state.Get(), 48000, 2); },
                                      "create on a terminated context");
}};

const Case decodeAfterOtherTerminated{"OpusDecDecode_AfterOtherContextTerminated_KeepsDecoding", [] {
    OpusContext context;
    OpusContext otherContext;
    OpusState otherState;
    std::array<std::int16_t, 1922> reference{};
    RequireEqual(sceOpusDecCreateEx(otherContext.Get(), otherState.Get(), 48000, 2), 0, "create the decoder on the other context");
    for (int frame = 0; frame < 3; ++frame) {
        const auto packet = PacketAt(frame);
        RequireEqual(sceOpusDecDecode(otherState.Get(), packet.data, packet.bytes, reference.data() + 1, stereoFrameBytes),
                     stereoFrameBytes, "frame " + std::to_string(frame) + " decoded bytes");
    }
    RequireEqual(sceOpusDecTerminate(context.Get()), 0, "terminate the first context");
    const auto last = PacketAt(3);
    RequireEqual(sceOpusDecDecode(otherState.Get(), last.data, last.bytes, reference.data(), stereoFrameBytes), stereoFrameBytes,
                 "decode the last frame after the first context terminated");
    RequireEqual(sceOpusDecDestroy(otherState.Get()), 0, "destroy the other decoder");
    RequireEqual(sceOpusDecTerminate(otherContext.Get()), 0, "terminate the other context");
}};

} // namespace
