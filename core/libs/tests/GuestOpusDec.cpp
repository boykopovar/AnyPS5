#include "prx/libSceOpusDec/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <span>
#include <stdexcept>
#include <opus.h>

namespace {

void Require(bool value) { if (!value) std::abort(); }

template <typename TFunction>
void RequireThrows(TFunction function) {
    try { function(); } catch (const std::runtime_error&) { return; }
    std::abort();
}

const std::uint8_t OPUS_STEREO[] = {
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

const std::uint8_t OPUS_NARROWBAND[] = {
    11, 0, 0x08, 0x83, 0x9C, 0xE1, 0x7B, 0x2F, 0x52, 0xCA, 0x9E, 0x75, 0xD8,
    13, 0, 0x08, 0x96, 0x51, 0x3A, 0x33, 0x4A, 0xF8, 0x29, 0xF5, 0xBE, 0x40, 0x15, 0x98,
    10, 0, 0x08, 0x85, 0x89, 0x9D, 0x73, 0x0F, 0xC2, 0xB1, 0xE2, 0x48,
};

void CheckReference(int rate, int channels) {
    std::uint32_t context = 0;
    std::array<std::uint8_t, 640> state{};
    Require(sceOpusDecInitialize(&context) == 0);
    Require(sceOpusDecCreateEx(&context, state.data(), rate, channels) == 0);
    int error = OPUS_OK;
    auto* reference = opus_decoder_create(rate, channels, &error);
    Require(reference && error == OPUS_OK);
    std::array<std::uint8_t, 1500> longPacket{};
    auto* repacketizer = opus_repacketizer_create();
    Require(repacketizer != nullptr);
    for (int i = 0; i < 6; ++i) Require(opus_repacketizer_cat(repacketizer, OPUS_STEREO + 2, OPUS_STEREO[0]) == OPUS_OK);
    const int longSize = opus_repacketizer_out(repacketizer, longPacket.data() + 2, static_cast<opus_int32>(longPacket.size() - 2));
    Require(longSize > 0);
    opus_repacketizer_destroy(repacketizer);
    longPacket[0] = static_cast<std::uint8_t>(longSize);
    longPacket[1] = static_cast<std::uint8_t>(longSize >> 8);
    for (auto packets : {std::span<const std::uint8_t>(OPUS_NARROWBAND), std::span<const std::uint8_t>(OPUS_STEREO),
             std::span<const std::uint8_t>(OPUS_NARROWBAND), std::span<const std::uint8_t>(longPacket.data(), longSize + 2)}) {
        for (std::size_t offset = 0; offset < packets.size();) {
            const int bytes = packets[offset] | (packets[offset + 1] << 8);
            const auto* packet = packets.data() + offset + 2;
            std::array<std::int16_t, 11520> expected{};
            const int samples = opus_decode(reference, packet, bytes, expected.data(), 5760, 0);
            Require(samples > 0);
            const int count = samples * channels;
            std::array<std::int16_t, 11522> actual{};
            actual.fill(12345);
            Require(sceOpusDecDecode(state.data(), packet, bytes, actual.data() + 1, count * 2 + 1) == count * 2);
            if (!std::equal(expected.begin(), expected.begin() + count, actual.begin() + 1)) {
                std::fprintf(stderr, "Opus PCM differs from libopus at %d Hz, %d channels, packet offset %zu\n", rate, channels, offset);
                std::abort();
            }
            Require(actual.front() == 12345 && std::all_of(actual.begin() + count + 1, actual.end(), [](auto value) { return value == 12345; }));
            offset += bytes + 2;
        }
    }
    opus_decoder_destroy(reference);
    Require(sceOpusDecDestroy(state.data()) == 0);
    Require(sceOpusDecTerminate(&context) == 0);
}

} // namespace

int main() {
    constexpr std::uint32_t errorBase = 0x81740000u;
    Require(sceOpusDecInitialize(nullptr) == static_cast<int>(errorBase + 13));
    Require(sceOpusDecDestroy(nullptr) == static_cast<int>(errorBase));
    for (int rate : {8000, 12000, 16000, 24000, 48000})
        for (int channels : {1, 2}) CheckReference(rate, channels);
    std::uint32_t context = 0, otherContext = 0;
    Require(sceOpusDecInitialize(&context) == 0);
    std::array<std::uint8_t, 640> invalidState{};
    std::int16_t output = 12345;
    const std::uint8_t packet = 0;
    Require(sceOpusDecCreateEx(nullptr, invalidState.data(), 0, 0) == static_cast<int>(errorBase));
    Require(sceOpusDecCreateEx(&context, invalidState.data(), 0, 0) == static_cast<int>(errorBase + 2));
    Require(sceOpusDecCreateEx(&context, invalidState.data(), 48000, 0) == static_cast<int>(errorBase + 3));
    Require(sceOpusDecDecode(nullptr, nullptr, -1, nullptr, -1) == static_cast<int>(errorBase));
    Require(sceOpusDecDecode(invalidState.data(), nullptr, -1, nullptr, -1) == static_cast<int>(errorBase + 6));
    Require(sceOpusDecDecode(invalidState.data(), &packet, -1, nullptr, -1) == static_cast<int>(errorBase + 7));
    Require(sceOpusDecDecode(invalidState.data(), &packet, 1, nullptr, -1) == static_cast<int>(errorBase + 8));
    Require(sceOpusDecDecode(invalidState.data(), &packet, 1, &output, -1) == static_cast<int>(errorBase + 9));
    Require(sceOpusDecDecode(invalidState.data(), &packet, 1, &output, 2) == static_cast<int>(errorBase + 1));
    Require(output == 12345);

    Require(sceOpusDecInitialize(&otherContext) == 0 && context != otherContext);
    RequireThrows([&] { sceOpusDecInitialize(&context); });
    Require(sceOpusDecGetSize(1) == 640 && sceOpusDecGetSize(2) == 640);
    std::array<std::uint8_t, 640> state{}, otherState{}, monoState{};
    Require(sceOpusDecCreateEx(&context, state.data(), 48000, 2) == 0);
    Require(sceOpusDecCreateEx(&otherContext, otherState.data(), 48000, 2) == 0);
    Require(sceOpusDecCreateEx(&context, monoState.data(), 48000, 1) == 0);
    RequireThrows([&] { sceOpusDecTerminate(&context); });
    RequireThrows([&] { sceOpusDecCreateEx(&context, state.data(), 48000, 2); });
    Require(sceOpusDecGetSize(3) == static_cast<int>(errorBase + 3));
    std::array<std::uint8_t, 640> unsupported{};
    for (int rate : {0, 22050, 44100, 96000})
        Require(sceOpusDecCreateEx(&context, unsupported.data(), rate, 2) == static_cast<int>(errorBase + 2));
    std::array<std::int16_t, 1922> pcm{}, reference{};
    pcm.fill(12345);
    RequireThrows([&] { sceOpusDecDecode(state.data(), OPUS_STEREO + 2, 70, pcm.data() + 1, 3839); });
    Require(std::all_of(pcm.begin(), pcm.end(), [](auto value) { return value == 12345; }));
    const std::uint8_t malformed[] = {0x7f};
    RequireThrows([&] { sceOpusDecDecode(state.data(), malformed, 1, pcm.data() + 1, 3840); });
    RequireThrows([&] { sceOpusDecDecode(state.data(), nullptr, 0, pcm.data() + 1, 3840); });
    Require(std::all_of(pcm.begin(), pcm.end(), [](auto value) { return value == 12345; }));
    std::size_t offset = 0;
    for (int frame = 0; frame < 3; ++frame) {
        const int bytes = OPUS_STEREO[offset] | (OPUS_STEREO[offset + 1] << 8);
        const auto* packet = OPUS_STEREO + offset + 2;
        Require(sceOpusDecDecode(state.data(), packet, bytes, pcm.data() + 1, 3841) == 3840);
        Require(pcm.front() == 12345 && pcm.back() == 12345);
        Require(sceOpusDecDecode(otherState.data(), packet, bytes, reference.data() + 1, 3840) == 3840);
        Require(std::equal(pcm.begin() + 1, pcm.end() - 1, reference.begin() + 1));
        if (frame == 1) {
            Require(sceOpusDecCreateEx(&context, unsupported.data(), 48000, 2) == 0);
            std::array<std::int16_t, 1920> fresh{};
            Require(sceOpusDecDecode(unsupported.data(), packet, bytes, fresh.data(), 3840) == 3840);
            Require(!std::equal(fresh.begin(), fresh.end(), pcm.begin() + 1));
            Require(sceOpusDecDestroy(unsupported.data()) == 0);
        }
        if (frame > 0) {
            int peak = 0;
            for (auto it = pcm.begin() + 1; it != pcm.end() - 1; ++it) peak = std::max(peak, std::abs(static_cast<int>(*it)));
            Require(peak > 900 && peak < 1200);
        }
        std::array<std::int16_t, 962> mono{};
        mono.front() = mono.back() = 12345;
        Require(sceOpusDecDecode(monoState.data(), packet, bytes, mono.data() + 1, 1921) == 1920);
        Require(mono.front() == 12345 && mono.back() == 12345);
        Require(std::any_of(mono.begin() + 1, mono.end() - 1, [](auto value) { return value != 0; }));
        offset += bytes + 2;
    }
    for (int rate : {8000, 12000, 16000, 24000}) {
        std::array<std::uint8_t, 640> rateState{};
        Require(sceOpusDecCreateEx(&context, rateState.data(), rate, 2) == 0);
        const int samples = 960 * rate / 48000;
        std::array<std::int16_t, 962> resampled{};
        int peak = 0;
        for (std::size_t at = 0, frame = 0; frame < 3; ++frame) {
            const int bytes = OPUS_STEREO[at] | (OPUS_STEREO[at + 1] << 8);
            resampled.fill(12345);
            RequireThrows([&] { sceOpusDecDecode(rateState.data(), OPUS_STEREO + at + 2, bytes, resampled.data() + 1, samples * 4 - 1); });
            Require(sceOpusDecDecode(rateState.data(), OPUS_STEREO + at + 2, bytes, resampled.data() + 1, samples * 4) == samples * 4);
            Require(resampled[0] == 12345 && resampled[samples * 2 + 1] == 12345);
            for (int index = 1; index <= samples * 2; ++index) peak = std::max(peak, std::abs(static_cast<int>(resampled[index])));
            at += bytes + 2;
        }
        Require(peak > 900 && peak < 1200);
        Require(sceOpusDecDestroy(rateState.data()) == 0);
    }

    Require(sceOpusDecDestroy(state.data()) == 0);
    Require(sceOpusDecDestroy(state.data()) == static_cast<int>(errorBase + 1));
    Require(sceOpusDecDecode(state.data(), OPUS_STEREO + 2, 70, pcm.data(), 3840) == static_cast<int>(errorBase + 1));
    Require(sceOpusDecCreateEx(&context, state.data(), 48000, 2) == 0);
    Require(sceOpusDecDecode(state.data(), OPUS_STEREO + 2, 70, pcm.data(), 3840) == 3840);
    std::array<std::int16_t, 3842> rejected;
    rejected.fill(12345);
    const std::uint8_t invalidFraming[] = {0x7d, 1};
    RequireThrows([&] { sceOpusDecDecode(state.data(), invalidFraming, 2, rejected.data() + 1, 7680); });
    Require(std::all_of(rejected.begin(), rejected.end(), [](auto value) { return value == 12345; }));
    Require(sceOpusDecDestroy(state.data()) == 0);
    Require(sceOpusDecDestroy(monoState.data()) == 0);
    Require(sceOpusDecTerminate(&context) == 0);
    RequireThrows([&] { sceOpusDecTerminate(&context); });
    RequireThrows([&] { sceOpusDecCreateEx(&context, state.data(), 48000, 2); });
    Require(sceOpusDecDecode(otherState.data(), OPUS_STEREO + offset + 2, OPUS_STEREO[offset], reference.data(), 3840) == 3840);
    Require(sceOpusDecDestroy(otherState.data()) == 0);
    Require(sceOpusDecTerminate(&otherContext) == 0);

    std::uint32_t narrowContext = 0;
    std::array<std::uint8_t, 640> narrowState{};
    Require(sceOpusDecInitialize(&narrowContext) == 0);
    Require(sceOpusDecCreateEx(&narrowContext, narrowState.data(), 48000, 1) == 0);
    std::array<std::int16_t, 962> narrow{};
    bool audible = false;
    for (std::size_t at = 0; at < sizeof(OPUS_NARROWBAND);) {
        const int bytes = OPUS_NARROWBAND[at] | (OPUS_NARROWBAND[at + 1] << 8);
        narrow.fill(12345);
        Require(sceOpusDecDecode(narrowState.data(), OPUS_NARROWBAND + at + 2, bytes, narrow.data() + 1, 1920) == 1920);
        Require(narrow.front() == 12345 && narrow.back() == 12345);
        audible = audible || std::any_of(narrow.begin() + 1, narrow.end() - 1, [](auto value) { return value != 0 && value != 12345; });
        at += bytes + 2;
    }
    Require(audible);
    narrow.fill(12345);
    Require(sceOpusDecDecode(narrowState.data(), OPUS_STEREO + 2, 70, narrow.data() + 1, 1920) == 1920);
    Require(narrow.front() == 12345 && narrow.back() == 12345);
    Require(sceOpusDecDestroy(narrowState.data()) == 0);
    Require(sceOpusDecTerminate(&narrowContext) == 0);
}
