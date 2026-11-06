#include "prx/libwebrtc/libwebrtc.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <opus.h>
#include <source_location>
#include <vector>

namespace {

constexpr int SampleRate = 48000;
constexpr std::size_t FrameSamples = 960;
constexpr std::size_t PacketCapacity = 1500;
constexpr std::int32_t Voip = 0;
constexpr std::int32_t Audio = 1;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc Opus check failed at line %u\n", location.line());
    std::abort();
}

WebRtcOpusEncInst* Create(std::size_t channels, std::int32_t application) {
    WebRtcOpusEncInst* inst = nullptr;
    Require(WebRtcOpus_EncoderCreate_nid_postfix(&inst, channels, application, SampleRate) == 0);
    Require(inst != nullptr);
    return inst;
}

std::int16_t Tone(std::size_t index, double frequency, double amplitude) {
    return static_cast<std::int16_t>(std::lround(amplitude * std::sin(2.0 * 3.14159265358979323846 * frequency * static_cast<double>(index) / SampleRate)));
}

struct Noise {
    std::uint32_t state = 12345;

    std::int16_t Next(int amplitude) {
        state = state * 1664525u + 1013904223u;
        return static_cast<std::int16_t>(static_cast<int>(state >> 16) % (2 * amplitude + 1) - amplitude);
    }
};

std::vector<std::int16_t> ToneFrame(std::size_t frame, double frequency) {
    std::vector<std::int16_t> pcm(FrameSamples);
    for (std::size_t i = 0; i < FrameSamples; ++i) pcm[i] = Tone(frame * FrameSamples + i, frequency, 8000.0);
    return pcm;
}

std::vector<std::int16_t> NoiseFrame(Noise& noise, int amplitude) {
    std::vector<std::int16_t> pcm(FrameSamples);
    for (auto& sample : pcm) sample = noise.Next(amplitude);
    return pcm;
}

int Encode(WebRtcOpusEncInst* inst, const std::vector<std::int16_t>& pcm, std::size_t samples, std::array<std::uint8_t, PacketCapacity>& packet) {
    return WebRtcOpus_Encode_nid_postfix(inst, pcm.data(), samples, packet.size(), packet.data());
}

void TestRoundTrip() {
    auto* inst = Create(1, Voip);
    Require(WebRtcOpus_SetBitRate_nid_postfix(inst, 32000) == 0);
    Require(WebRtcOpus_SetComplexity_nid_postfix(inst, 5) == 0);
    int error = OPUS_OK;
    OpusDecoder* decoder = opus_decoder_create(SampleRate, 1, &error);
    Require(error == OPUS_OK && decoder != nullptr);
    constexpr std::size_t Frames = 50;
    std::vector<std::int16_t> input;
    std::vector<std::int16_t> output;
    std::array<std::uint8_t, PacketCapacity> packet {};
    for (std::size_t frame = 0; frame < Frames; ++frame) {
        const auto pcm = ToneFrame(frame, 440.0);
        input.insert(input.end(), pcm.begin(), pcm.end());
        const int size = Encode(inst, pcm, FrameSamples, packet);
        Require(size > 2 && size <= static_cast<int>(PacketCapacity));
        std::array<std::int16_t, FrameSamples> decoded {};
        Require(opus_decode(decoder, packet.data(), size, decoded.data(), static_cast<int>(FrameSamples), 0) == static_cast<int>(FrameSamples));
        output.insert(output.end(), decoded.begin(), decoded.end());
    }
    const std::size_t start = 10 * FrameSamples;
    const std::size_t length = 30 * FrameSamples;
    double best = 0.0;
    double inputEnergy = 0.0;
    double outputEnergy = 0.0;
    for (std::size_t lag = 0; lag < FrameSamples; ++lag) {
        double cross = 0.0;
        double in = 0.0;
        double out = 0.0;
        for (std::size_t i = start; i < start + length; ++i) {
            cross += static_cast<double>(input[i]) * output[i + lag];
            in += static_cast<double>(input[i]) * input[i];
            out += static_cast<double>(output[i + lag]) * output[i + lag];
        }
        const double correlation = cross / std::sqrt(in * out);
        if (correlation > best) {
            best = correlation;
            inputEnergy = in;
            outputEnergy = out;
        }
    }
    Require(best > 0.9);
    const double gain = std::sqrt(outputEnergy / inputEnergy);
    Require(gain > 0.7 && gain < 1.3);
    opus_decoder_destroy(decoder);
    Require(WebRtcOpus_EncoderFree_nid_postfix(inst) == 0);
}

void TestForceChannels() {
    auto* inst = Create(2, Audio);
    Require(WebRtcOpus_SetBitRate_nid_postfix(inst, 64000) == 0);
    std::vector<std::int16_t> pcm(FrameSamples * 2);
    std::array<std::uint8_t, PacketCapacity> packet {};
    auto encodeStereo = [&](std::size_t frame) {
        for (std::size_t i = 0; i < FrameSamples; ++i) {
            pcm[i * 2] = Tone(frame * FrameSamples + i, 440.0, 8000.0);
            pcm[i * 2 + 1] = Tone(frame * FrameSamples + i, 660.0, 8000.0);
        }
        const int size = WebRtcOpus_Encode_nid_postfix(inst, pcm.data(), FrameSamples, packet.size(), packet.data());
        Require(size > 2);
        return opus_packet_get_nb_channels(packet.data());
    };
    Require(WebRtcOpus_SetForceChannels_nid_postfix(inst, 1) == 0);
    for (std::size_t frame = 0; frame < 10; ++frame) Require(encodeStereo(frame) == 1);
    Require(WebRtcOpus_SetForceChannels_nid_postfix(inst, 0) == 0);
    int channels = 0;
    for (std::size_t frame = 10; frame < 20; ++frame) channels = encodeStereo(frame);
    Require(channels == 2);
    Require(WebRtcOpus_SetForceChannels_nid_postfix(inst, 2) == 0);
    Require(WebRtcOpus_SetForceChannels_nid_postfix(inst, 3) == -1);
    Require(WebRtcOpus_EncoderFree_nid_postfix(inst) == 0);
}

void TestConstantBitrate() {
    auto* inst = Create(1, Voip);
    Noise noise;
    std::array<std::uint8_t, PacketCapacity> packet {};
    Require(WebRtcOpus_EnableCbr_nid_postfix(inst) == 0);
    for (const std::int32_t rate : {16000, 32000, 64000}) {
        Require(WebRtcOpus_SetBitRate_nid_postfix(inst, rate) == 0);
        const int expected = rate / 8 / 50;
        for (std::size_t frame = 0; frame < 10; ++frame) {
            const auto pcm = frame % 2 == 0 ? ToneFrame(frame, 300.0) : NoiseFrame(noise, 12000);
            Require(Encode(inst, pcm, FrameSamples, packet) == expected);
        }
    }
    Require(WebRtcOpus_DisableCbr_nid_postfix(inst) == 0);
    Require(WebRtcOpus_SetBitRate_nid_postfix(inst, 32000) == 0);
    std::vector<int> sizes;
    for (std::size_t frame = 0; frame < 20; ++frame) {
        const auto pcm = frame % 2 == 0 ? ToneFrame(frame, 300.0) : NoiseFrame(noise, 12000);
        sizes.push_back(Encode(inst, pcm, FrameSamples, packet));
    }
    Require(std::all_of(sizes.begin(), sizes.end(), [](int size) { return size > 2; }));
    Require(*std::min_element(sizes.begin(), sizes.end()) != *std::max_element(sizes.begin(), sizes.end()));
    Require(WebRtcOpus_EncoderFree_nid_postfix(inst) == 0);
}

void TestDtx() {
    auto* inst = Create(1, Voip);
    Require(WebRtcOpus_SetBitRate_nid_postfix(inst, 32000) == 0);
    Require(WebRtcOpus_EnableDtx_nid_postfix(inst) == 0);
    std::array<std::uint8_t, PacketCapacity> packet {};
    const std::vector<std::int16_t> silence(FrameSamples, 0);
    std::vector<int> results;
    for (std::size_t frame = 0; frame < 100; ++frame) results.push_back(Encode(inst, silence, FrameSamples, packet));
    Require(std::none_of(results.begin(), results.end(), [](int result) { return result < 0; }));
    const auto entered = std::find_if(results.begin(), results.end(), [](int result) { return result == 1 || result == 2; });
    Require(entered != results.end());
    Require(std::find(entered + 1, results.end(), 0) != results.end());
    Require(std::find(results.begin(), entered, 0) == entered);
    Noise noise;
    bool active = false;
    for (std::size_t frame = 0; frame < 5 && !active; ++frame) active = Encode(inst, NoiseFrame(noise, 12000), FrameSamples, packet) > 2;
    Require(active);
    Require(WebRtcOpus_DisableDtx_nid_postfix(inst) == 0);
    Require(WebRtcOpus_EncoderFree_nid_postfix(inst) == 0);
}

void TestBandwidth() {
    auto* inst = Create(1, Voip);
    Require(WebRtcOpus_SetBitRate_nid_postfix(inst, 32000) == 0);
    Noise noise;
    std::array<std::uint8_t, PacketCapacity> packet {};
    Require(WebRtcOpus_SetMaxPlaybackRate_nid_postfix(inst, 8000) == 0);
    for (std::size_t frame = 0; frame < 10; ++frame) Require(Encode(inst, NoiseFrame(noise, 8000), FrameSamples, packet) > 2);
    Require(WebRtcOpus_GetBandwidth_nid_postfix(inst) == OPUS_BANDWIDTH_NARROWBAND);
    Require(WebRtcOpus_SetMaxPlaybackRate_nid_postfix(inst, 48000) == 0);
    Require(WebRtcOpus_SetBandwidth_nid_postfix(inst, OPUS_BANDWIDTH_WIDEBAND) == 0);
    for (std::size_t frame = 0; frame < 10; ++frame) Require(Encode(inst, NoiseFrame(noise, 8000), FrameSamples, packet) > 2);
    Require(WebRtcOpus_GetBandwidth_nid_postfix(inst) == OPUS_BANDWIDTH_WIDEBAND);
    Require(WebRtcOpus_SetBandwidth_nid_postfix(inst, 5) == OPUS_BAD_ARG);
    Require(WebRtcOpus_EncoderFree_nid_postfix(inst) == 0);
}

void TestFec() {
    auto* inst = Create(1, Voip);
    Require(WebRtcOpus_SetBitRate_nid_postfix(inst, 32000) == 0);
    Require(WebRtcOpus_EnableFec_nid_postfix(inst) == 0);
    Require(WebRtcOpus_SetPacketLossRate_nid_postfix(inst, 20) == 0);
    Noise noise;
    std::array<std::uint8_t, PacketCapacity> packet {};
    int withLbrr = 0;
    for (std::size_t frame = 0; frame < 30; ++frame) {
        const int size = Encode(inst, NoiseFrame(noise, 8000), FrameSamples, packet);
        Require(size > 2);
        withLbrr += opus_packet_has_lbrr(packet.data(), size) == 1;
    }
    Require(withLbrr > 0);
    Require(WebRtcOpus_DisableFec_nid_postfix(inst) == 0);
    for (std::size_t frame = 0; frame < 5; ++frame) Require(Encode(inst, NoiseFrame(noise, 8000), FrameSamples, packet) > 2);
    for (std::size_t frame = 0; frame < 20; ++frame) {
        const int size = Encode(inst, NoiseFrame(noise, 8000), FrameSamples, packet);
        Require(size > 2 && opus_packet_has_lbrr(packet.data(), size) == 0);
    }
    Require(WebRtcOpus_EncoderFree_nid_postfix(inst) == 0);
}

void TestErrors() {
    Require(WebRtcOpus_EncoderCreate_nid_postfix(nullptr, 1, Voip, SampleRate) == -1);
    auto* const untouched = reinterpret_cast<WebRtcOpusEncInst*>(static_cast<std::uintptr_t>(0x10));
    WebRtcOpusEncInst* inst = untouched;
    Require(WebRtcOpus_EncoderCreate_nid_postfix(&inst, 1, 2, SampleRate) == -1 && inst == untouched);
    Require(WebRtcOpus_EncoderCreate_nid_postfix(&inst, 1, Voip, 44100) == -1 && inst == untouched);
    Require(WebRtcOpus_EncoderCreate_nid_postfix(&inst, 3, Audio, SampleRate) == -1 && inst == untouched);
    Require(WebRtcOpus_EncoderFree_nid_postfix(nullptr) == -1);
    Require(WebRtcOpus_SetBitRate_nid_postfix(nullptr, 32000) == -1);
    Require(WebRtcOpus_SetPacketLossRate_nid_postfix(nullptr, 10) == -1);
    Require(WebRtcOpus_SetMaxPlaybackRate_nid_postfix(nullptr, 8000) == -1);
    Require(WebRtcOpus_EnableFec_nid_postfix(nullptr) == -1 && WebRtcOpus_DisableFec_nid_postfix(nullptr) == -1);
    Require(WebRtcOpus_EnableDtx_nid_postfix(nullptr) == -1 && WebRtcOpus_DisableDtx_nid_postfix(nullptr) == -1);
    Require(WebRtcOpus_EnableCbr_nid_postfix(nullptr) == -1 && WebRtcOpus_DisableCbr_nid_postfix(nullptr) == -1);
    Require(WebRtcOpus_SetComplexity_nid_postfix(nullptr, 5) == -1);
    Require(WebRtcOpus_GetBandwidth_nid_postfix(nullptr) == -1);
    Require(WebRtcOpus_SetBandwidth_nid_postfix(nullptr, OPUS_BANDWIDTH_WIDEBAND) == -1);
    Require(WebRtcOpus_SetForceChannels_nid_postfix(nullptr, 1) == -1);

    inst = Create(1, Audio);
    Require(WebRtcOpus_SetComplexity_nid_postfix(inst, 11) == OPUS_BAD_ARG);
    Require(WebRtcOpus_SetBitRate_nid_postfix(inst, -5) == OPUS_BAD_ARG);
    Require(WebRtcOpus_SetPacketLossRate_nid_postfix(inst, 101) == OPUS_BAD_ARG);
    std::vector<std::int16_t> pcm(48 * 120 + 1, 0);
    std::array<std::uint8_t, PacketCapacity> packet {};
    Require(Encode(inst, pcm, 48 * 120 + 1, packet) == -1);
    Require(Encode(inst, pcm, 100, packet) == -1);
    Require(WebRtcOpus_Encode_nid_postfix(inst, pcm.data(), FrameSamples, 0, packet.data()) == -1);
    Require(Encode(inst, pcm, 48 * 60, packet) > 0);
    Require(WebRtcOpus_EncoderFree_nid_postfix(inst) == 0);
}

}

int main() {
    TestRoundTrip();
    TestForceChannels();
    TestConstantBitrate();
    TestDtx();
    TestBandwidth();
    TestFec();
    TestErrors();
    std::puts("libwebrtc Opus tests passed");
}
