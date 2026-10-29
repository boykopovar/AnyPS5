#include "prx/libSceNgs2.native/tests/Ngs2Test.hpp"
#include "Ngs2Test.hpp"

#include "libatrac9.h"

#include <algorithm>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <iterator>
#include <random>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static constexpr std::uint32_t Config = 0xFE7007F0;
static constexpr std::uint32_t SuperframeBytes = 256;
static constexpr std::uint32_t SuperframeSamples = 1024;

static const std::uint8_t Superframe[SuperframeBytes] = {
    0x21, 0xf0, 0x08, 0x42, 0x03, 0x1b, 0x99, 0x5f, 0x30, 0xf4, 0x30, 0xf5, 0xf9, 0xca, 0x41, 0x69,
    0x19, 0x6b, 0x98, 0x7d, 0x97, 0x04, 0xe0, 0x36, 0xd6, 0x2a, 0x0c, 0x98, 0x48, 0x95, 0xca, 0xa9,
    0x85, 0xd2, 0xeb, 0x88, 0xee, 0x84, 0xe7, 0x1e, 0x78, 0x17, 0x13, 0x72, 0xe1, 0x55, 0xdb, 0xfb,
    0xc0, 0x73, 0x78, 0x57, 0x78, 0x58, 0x8d, 0x4e, 0x82, 0x99, 0xe6, 0xca, 0x2e, 0xb1, 0x73, 0xb3,
    0x83, 0x5e, 0x3c, 0x71, 0xe3, 0x23, 0x8f, 0x5b, 0x3b, 0xa3, 0xcd, 0x1c, 0xf8, 0x8d, 0x4e, 0x82,
    0x99, 0xe6, 0xca, 0x2e, 0xb1, 0x73, 0xb3, 0x83, 0x5e, 0x3c, 0x71, 0xe3, 0x23, 0x8f, 0x5b, 0x3b,
    0xa3, 0xcd, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
    0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01
};

static std::vector<float> Reference(std::uint32_t superframes = 1, std::vector<std::uint32_t>* offsets = nullptr, const std::uint8_t* packet = Superframe) {
    std::uint8_t config[4] = {0xFE, 0x70, 0x07, 0xF0};
    void* decoder = Atrac9GetHandle();
    Require(Atrac9InitDecoder(decoder, config) == 0);
    std::vector<float> pcm(SuperframeSamples * superframes);
    for (std::uint32_t s = 0; s < superframes; ++s) {
        int offset = 0;
        for (std::uint32_t frame = 0; frame < 4; frame++) {
            int used = 0;
            Require(Atrac9DecodeF32(decoder, packet + offset, static_cast<int>(SuperframeBytes) - offset, pcm.data() + s * SuperframeSamples + frame * 256, &used, 0) == 0);
            offset += used;
            if (offsets != nullptr) offsets->push_back(offset);
        }
    }
    Atrac9ReleaseHandle(decoder);
    for (auto& sample : pcm) sample = std::clamp(sample * (32767.0f / 32768.0f), -1.0f, 1.0f);
    return pcm;
}

static std::vector<std::uint32_t> callbackFlags;
static void APS5_VABI OnBlock(const Ngs2VoiceCallbackInfo* info) {
    callbackFlags.push_back(info->flag);
}

static uintptr_t Sampler(uintptr_t system, std::uint32_t skip, std::uint32_t samples, std::uint32_t repeats) {
    const auto voice = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_ATRAC9, 1, 48000, Config, 0, 0}});
    const Ngs2WaveformBlock block{0, SuperframeBytes, repeats, skip, samples, 0, 0};
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, Superframe, 0, 1, &block});
    Control(voice, SCE_NGS2_VOICE_PARAM_CALLBACK,
            Ngs2VoiceCallbackParam{{}, OnBlock, 0, SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END | SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_REPEAT, 0});
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    return voice;
}

static std::vector<float> Render(uintptr_t system, std::uint32_t samples, std::uint32_t grain = Grain) {
    std::vector<float> rendered;
    std::vector<float> out(grain);
    const Ngs2RenderBufferInfo info{out.data(), out.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 1};
    while (rendered.size() < samples) {
        Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_OK);
        rendered.insert(rendered.end(), out.begin(), out.end());
    }
    return rendered;
}

static void TestSkipAndBlockEnd(const std::vector<float>& reference) {
    const auto system = CreateSystem();
    Patch(Sampler(system, 100, 500, 0), Mastering(system, 1));
    callbackFlags.clear();
    const auto rendered = Render(system, 512);
    for (std::uint32_t i = 0; i < 512; i++) Require(rendered[i] == (i >= 3 && i < 503 ? reference[100 + i - 3] : 0.0f));
    Require(callbackFlags.size() == 1 && callbackFlags[0] == SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestRepeatAndState(const std::vector<float>& reference) {
    const auto system = CreateSystem();
    const auto sampler = Sampler(system, 0, 300, 2);
    Patch(sampler, Mastering(system, 1));
    callbackFlags.clear();
    auto rendered = Render(system, 304);
    Ngs2SamplerVoiceState state{};
    Require(sceNgs2VoiceGetState(sampler, &state.voice_state, sizeof(state)) == SCE_NGS2_OK);
    std::vector<std::uint32_t> offsets;
    Reference(1, &offsets);
    Require(state.num_decoded_samples == 556 && state.decoded_data_size == SuperframeBytes + offsets[0] && state.waveform_data == Superframe + offsets[0]);
    const auto second = Render(system, 304);
    rendered.insert(rendered.end(), second.begin(), second.end());
    for (std::uint32_t i = 0; i < 608; i++) Require(rendered[i] == (i >= 3 && i < 603 ? reference[(i - 3) % 300] : 0.0f));
    Require(callbackFlags.size() == 2 && callbackFlags[0] == SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_REPEAT && callbackFlags[1] == SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END);
    Require(Flags(sampler) == 0);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void Put16(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value));
    out.push_back(static_cast<std::uint8_t>(value >> 8));
}

static void Put32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    Put16(out, value & 0xffff);
    Put16(out, value >> 16);
}

static void PutTag(std::vector<std::uint8_t>& out, const char* tag) {
    out.insert(out.end(), tag, tag + 4);
}

static std::vector<std::uint8_t> At9File(std::uint32_t sampleRate) {
    static constexpr std::uint8_t guid[16] = {0xd2, 0x42, 0xe1, 0x47, 0xba, 0x36, 0x8d, 0x4d, 0x88, 0xfc, 0x61, 0x65, 0x4f, 0x8c, 0x83, 0x6c};
    std::vector<std::uint8_t> file;
    PutTag(file, "RIFF");
    Put32(file, 0);
    PutTag(file, "WAVE");
    PutTag(file, "fmt ");
    Put32(file, 52);
    Put16(file, 0xfffe);
    Put16(file, 1);
    Put32(file, sampleRate);
    Put32(file, 12000);
    Put16(file, SuperframeBytes);
    Put16(file, 0);
    Put16(file, 34);
    Put16(file, SuperframeSamples);
    Put32(file, 4);
    file.insert(file.end(), guid, guid + sizeof(guid));
    Put32(file, 1);
    for (int i = 0; i < 4; i++) file.push_back(static_cast<std::uint8_t>(Config >> (24 - 8 * i)));
    Put32(file, 0);
    PutTag(file, "fact");
    Put32(file, 12);
    Put32(file, 900);
    Put32(file, 256);
    Put32(file, 256);
    PutTag(file, "data");
    Put32(file, SuperframeBytes);
    file.insert(file.end(), Superframe, Superframe + SuperframeBytes);
    const auto riffSize = static_cast<std::uint32_t>(file.size() - 8);
    std::memcpy(file.data() + 4, &riffSize, sizeof(riffSize));
    return file;
}

static void TestParse() {
    const auto file = At9File(48000);
    Ngs2WaveformInfo info{};
    Require(sceNgs2ParseWaveformData(file.data(), file.size(), nullptr) == SCE_NGS2_ERROR_INVALID_OUT_ADDRESS);
    Require(sceNgs2ParseWaveformData(file.data(), file.size(), &info) == SCE_NGS2_OK);
    Require(info.format.waveform_type == SCE_NGS2_WAVEFORM_TYPE_ATRAC9 && info.format.num_channels == 1 && info.format.sample_rate == 48000);
    Require(info.format.config_data == Config && info.data_offset == file.size() - SuperframeBytes && info.data_size == SuperframeBytes);
    Require(info.num_samples == 900 && info.num_delay_samples == 256 && info.audio_unit_size == 64 && info.num_audio_unit_samples == 256);
    Require(info.num_audio_unit_per_frame == 4 && info.audio_frame_size == SuperframeBytes && info.num_audio_frame_samples == SuperframeSamples);
    Require(info.num_blocks == 1 && info.block[0].data_offset == info.data_offset && info.block[0].data_size == SuperframeBytes);
    Require(info.block[0].num_skip_samples == 256 && info.block[0].num_samples == 900);
    for (const std::uint32_t factDelay : {0u, 1u, 900u, 0xffffffffu}) {
        auto other = file;
        constexpr char factTag[] = {'f', 'a', 'c', 't'};
        const auto fact = static_cast<std::size_t>(std::search(other.begin(), other.end(), std::begin(factTag), std::end(factTag)) - other.begin());
        std::memcpy(other.data() + fact + 12, &factDelay, sizeof(factDelay));
        Ngs2WaveformInfo parsed{};
        Require(sceNgs2ParseWaveformData(other.data(), other.size(), &parsed) == SCE_NGS2_OK);
        Require(parsed.num_delay_samples == 256 && parsed.block[0].num_skip_samples == 256 && parsed.num_samples == 900);
    }

    Require(sceNgs2ParseWaveformData(file.data(), 11, &info) == SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA);
    Require(sceNgs2ParseWaveformData(Superframe, SuperframeBytes, &info) == SCE_NGS2_ERROR_UNKNOWN_WAVEFORM_FORMAT);
    const auto wrongRate = At9File(44100);
    Require(sceNgs2ParseWaveformData(wrongRate.data(), wrongRate.size(), &info) == SCE_NGS2_ERROR_INVALID_WAVEFORM_FORMAT);
}

static void RequireAtrac9Block(std::uint32_t config, std::uint32_t pos, std::uint32_t count, std::uint64_t offset, std::uint64_t size, std::uint32_t skip) {
    const Ngs2WaveformFormat format{SCE_NGS2_WAVEFORM_TYPE_ATRAC9, 1, 48000, config, 0, 0};
    Ngs2WaveformBlock block;
    std::memset(&block, 0xab, sizeof(block));
    Require(sceNgs2CalcWaveformBlock(&format, pos, count, &block) == SCE_NGS2_OK);
    Require(block.data_offset == offset && block.data_size == size && block.num_skip_samples == skip && block.num_samples == count);
    Require(block.num_repeats == 0 && block.reserved == 0 && block.user_data == 0);
}

static void TestCalcBlock() {
    const Ngs2WaveformFormat format{SCE_NGS2_WAVEFORM_TYPE_ATRAC9, 1, 48000, Config, 0, 0};
    Require(sceNgs2CalcWaveformBlock(&format, 1500, 600, nullptr) == SCE_NGS2_ERROR_INVALID_OUT_ADDRESS);
    RequireAtrac9Block(Config, 0, 768, 0, SuperframeBytes, 256);
    RequireAtrac9Block(Config, 0, 769, 0, 2 * SuperframeBytes, 256);
    RequireAtrac9Block(Config, 1500, 600, SuperframeBytes, 2 * SuperframeBytes, 732);
    RequireAtrac9Block(Config, 1500, 0, SuperframeBytes, SuperframeBytes, 732);
    RequireAtrac9Block(Config, 4294967195u, 1000, 1073741568u, 3221226240u, 1179);
    RequireAtrac9Block(0xFE7007E0, 300, 10, 64, 128, 300);
    RequireAtrac9Block(0xFE4007F0, 600, 100, SuperframeBytes, SuperframeBytes, 216);
    RequireAtrac9Block(0xFE1007F0, 0, 1, 0, SuperframeBytes, 64);
    Ngs2WaveformBlock block{};
    const Ngs2WaveformFormat stereo{SCE_NGS2_WAVEFORM_TYPE_ATRAC9, 2, 44100, Config, 0, 0};
    Require(sceNgs2CalcWaveformBlock(&stereo, 0, 1, &block) == SCE_NGS2_OK && block.data_size == SuperframeBytes && block.num_skip_samples == 256);
    const Ngs2WaveformFormat silent{SCE_NGS2_WAVEFORM_TYPE_ATRAC9, 0, 0, Config, 0, 0};
    Require(sceNgs2CalcWaveformBlock(&silent, 0, 1, &block) == SCE_NGS2_OK && block.data_size == SuperframeBytes && block.num_skip_samples == 256);
    for (const std::uint32_t rateIndex : {0u, 2u, 3u, 5u, 6u, 8u, 15u}) {
        const Ngs2WaveformFormat unknown{SCE_NGS2_WAVEFORM_TYPE_ATRAC9, 1, 48000, (Config & ~0x00F00000u) | rateIndex << 20, 0, 0};
        bool threw = false;
        try { sceNgs2CalcWaveformBlock(&unknown, 0, 1, &block); } catch (const std::runtime_error&) { threw = true; }
        Require(threw);
    }
}

static void TestFrameInfo() {
    const Ngs2WaveformFormat supported[] = {
        {SCE_NGS2_WAVEFORM_TYPE_ATRAC9, 1, 48000, Config, 0, 0},
        {SCE_NGS2_WAVEFORM_TYPE_ATRAC9, 1, 48000, Config, 7, 9},
    };
    const Ngs2WaveformFormat rejected[] = {
        {SCE_NGS2_WAVEFORM_TYPE_ATRAC9, 2, 48000, Config, 0, 0},
        {SCE_NGS2_WAVEFORM_TYPE_ATRAC9, 0, 48000, Config, 0, 0},
        {SCE_NGS2_WAVEFORM_TYPE_ATRAC9, 9, 48000, Config, 0, 0},
        {SCE_NGS2_WAVEFORM_TYPE_ATRAC9, 1, 44100, Config, 0, 0},
        {SCE_NGS2_WAVEFORM_TYPE_ATRAC9, 1, 0, Config, 0, 0},
        {SCE_NGS2_WAVEFORM_TYPE_ATRAC9, 1, 192001, Config, 0, 0},
        {SCE_NGS2_WAVEFORM_TYPE_ATRAC9, 1, 48000, 0, 0, 0},
    };
    for (const auto& format : supported) {
        Ngs2WaveformBlock block{};
        Require(sceNgs2CalcWaveformBlock(&format, 0, SuperframeSamples, &block) == SCE_NGS2_OK && block.data_size == 2 * SuperframeBytes);
        std::uint32_t values[4] = {0xa1, 0xa2, 0xa3, 0xa4};
        Require(sceNgs2GetWaveformFrameInfo(&format, &values[0], &values[1], &values[2], &values[3]) == SCE_NGS2_OK);
        Require(values[0] == SuperframeBytes && values[1] == SuperframeSamples && values[2] == 4 && values[3] == SuperframeSamples / 4);
    }
    for (const auto& format : rejected) {
        std::uint32_t values[4] = {0xa1, 0xa2, 0xa3, 0xa4};
        Require(sceNgs2GetWaveformFrameInfo(&format, &values[0], &values[1], &values[2], &values[3]) == SCE_NGS2_ERROR_INVALID_WAVEFORM_FORMAT);
        Require(values[0] == 0xa1 && values[1] == 0xa2 && values[2] == 0xa3 && values[3] == 0xa4);
    }
    std::uint32_t values[4] = {0xa1, 0xa2, 0xa3, 0xa4};
    Require(sceNgs2GetWaveformFrameInfo(nullptr, &values[0], &values[1], &values[2], &values[3]) == SCE_NGS2_ERROR_INVALID_WAVEFORM_FORMAT);
    Require(sceNgs2GetWaveformFrameInfo(&supported[0], nullptr, &values[1], &values[2], &values[3]) == SCE_NGS2_ERROR_INVALID_OUT_ADDRESS);
    Require(sceNgs2GetWaveformFrameInfo(&supported[0], &values[0], nullptr, &values[2], &values[3]) == SCE_NGS2_ERROR_INVALID_OUT_ADDRESS);
    Require(sceNgs2GetWaveformFrameInfo(&supported[0], &values[0], &values[1], nullptr, &values[3]) == SCE_NGS2_ERROR_INVALID_OUT_ADDRESS);
    Require(sceNgs2GetWaveformFrameInfo(&supported[0], &values[0], &values[1], &values[2], nullptr) == SCE_NGS2_ERROR_INVALID_OUT_ADDRESS);
    Require(sceNgs2GetWaveformFrameInfo(nullptr, nullptr, nullptr, nullptr, nullptr) == SCE_NGS2_ERROR_INVALID_OUT_ADDRESS);
    Require(values[0] == 0xa1 && values[1] == 0xa2 && values[2] == 0xa3 && values[3] == 0xa4);
}

static void RequirePcmBlock(std::uint32_t type, std::uint32_t channels, std::uint32_t pos, std::uint32_t count, std::uint64_t offset, std::uint64_t size) {
    const Ngs2WaveformFormat format{type, channels, 48000, 0, 0, 0};
    Ngs2WaveformBlock block;
    std::memset(&block, 0xab, sizeof(block));
    Require(sceNgs2CalcWaveformBlock(&format, pos, count, &block) == SCE_NGS2_OK);
    Require(block.data_offset == offset && block.data_size == size && block.num_samples == count);
    Require(block.num_repeats == 0 && block.num_skip_samples == 0 && block.reserved == 0 && block.user_data == 0);
}

static void TestCalcPcmBlock() {
    const std::uint32_t bytesPerSample[] = {1, 1, 2, 2, 3, 3, 4, 4, 4, 4, 8, 8};
    for (std::uint32_t type = 0x10; type <= 0x1b; ++type) {
        const auto stride = 100u * bytesPerSample[type - 0x10];
        RequirePcmBlock(type, 100, 3, 5, 3u * stride, 5u * stride);
    }
    RequirePcmBlock(SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 2, 100, 50, 400, 200);
    RequirePcmBlock(SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 1, 10, 0, 40, 0);
    RequirePcmBlock(SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 0, 7, 9, 0, 0);
    RequirePcmBlock(0x1a, 8, 0x4000000, 1, 0, 64);
    RequirePcmBlock(0x1a, 2, 0xffffffffu, 2, 0xfffffff0u, 32);
    const Ngs2WaveformFormat noRate{SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 2, 0, 0, 0, 0};
    Ngs2WaveformBlock block{};
    Require(sceNgs2CalcWaveformBlock(&noRate, 1, 1, &block) == SCE_NGS2_OK && block.data_offset == 4 && block.data_size == 4);
    for (std::uint32_t type : {0x0u, 0xfu, 0x1du, 0x20u, 0x30u, 0x43u}) {
        const Ngs2WaveformFormat unknown{type, 2, 48000, 0, 0, 0};
        std::memset(&block, 0xab, sizeof(block));
        Require(sceNgs2CalcWaveformBlock(&unknown, 0, 1, &block) == SCE_NGS2_ERROR_UNKNOWN_WAVEFORM_FORMAT);
        const auto* bytes = reinterpret_cast<const std::uint8_t*>(&block);
        Require(std::all_of(bytes, bytes + sizeof(block), [](std::uint8_t byte) { return byte == 0; }));
    }
    std::memset(&block, 0xab, sizeof(block));
    Require(sceNgs2CalcWaveformBlock(nullptr, 0, 1, &block) == SCE_NGS2_ERROR_INVALID_WAVEFORM_FORMAT && block.data_size == 0 && block.num_samples == 0);
}

static void RequireVagBlock(std::uint32_t channels, std::uint32_t pos, std::uint32_t count, std::uint64_t offset, std::uint64_t size, std::uint32_t samples) {
    const Ngs2WaveformFormat format{0x1c, channels, 48000, 0, 0, 0};
    Ngs2WaveformBlock block;
    std::memset(&block, 0xab, sizeof(block));
    Require(sceNgs2CalcWaveformBlock(&format, pos, count, &block) == SCE_NGS2_OK);
    Require(block.data_offset == offset && block.data_size == size && block.num_samples == samples);
    Require(block.num_repeats == 0 && block.num_skip_samples == 0 && block.reserved == 0 && block.user_data == 0);
}

static void TestCalcVagBlock() {
    RequireVagBlock(1, 0, 28, 0, 16, 28);
    RequireVagBlock(1, 27, 2, 0, 32, 56);
    RequireVagBlock(1, 28, 28, 16, 16, 28);
    RequireVagBlock(2, 30, 1, 32, 32, 28);
    RequireVagBlock(6, 56, 57, 192, 288, 84);
    RequireVagBlock(1, 0, 0, 0, 0, 0);
    RequireVagBlock(1, 5, 0, 0, 16, 28);
    RequireVagBlock(0, 100, 50, 0, 0, 84);
}

static void Set32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8));
}

static std::vector<std::uint8_t> LoopedFile(std::uint32_t begin, std::uint32_t end, std::uint32_t plays, bool trailingSampler = false) {
    auto file = At9File(48000);
    Set32(file, 80, 4 * SuperframeSamples - 256);
    Set32(file, 96, 4 * SuperframeBytes);
    for (int i = 0; i < 3; ++i) file.insert(file.end(), Superframe, Superframe + SuperframeBytes);
    std::vector<std::uint8_t> sampler;
    PutTag(sampler, "smpl");
    Put32(sampler, 60);
    for (int i = 0; i < 7; ++i) Put32(sampler, 0);
    Put32(sampler, 1);
    Put32(sampler, 24);
    Put32(sampler, 0);
    Put32(sampler, 0);
    Put32(sampler, begin + 256);
    Put32(sampler, end + 256 - 1);
    Put32(sampler, 0);
    Put32(sampler, plays);
    file.insert(trailingSampler ? file.end() : file.begin() + 92, sampler.begin(), sampler.end());
    Set32(file, 4, static_cast<std::uint32_t>(file.size() - 8));
    return file;
}

static void TestParseLoops() {
    constexpr std::uint32_t samples = 4 * SuperframeSamples - 256;
    const auto reference = Reference(4);
    for (bool trailingSampler : {false, true}) {
        for (const auto [begin, end] : {std::pair{0u, 800u}, {100u, 700u}, {768u, 2600u}, {850u, 2600u}, {1024u, 2600u}, {3072u, samples}, {0u, samples}}) {
            const auto file = LoopedFile(begin, end, 2, trailingSampler);
            Ngs2WaveformInfo info{};
            Require(sceNgs2ParseWaveformData(file.data(), file.size(), &info) == SCE_NGS2_OK);
            Require(info.loop_begin_position == begin && info.loop_end_position == end && info.num_samples == samples);
            Require(info.num_blocks == 1 + (begin != 0) + (end != samples));
            Require(info.block[begin != 0].num_repeats == UINT32_MAX);
            const auto system = CreateSystem();
            const auto voice = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
            Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, info.format});
            Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS,
                Ngs2SamplerVoiceWaveformBlocksParam{{}, file.data(), 0, info.num_blocks, info.block});
            Patch(voice, Mastering(system, 1));
            Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
            const auto loop = end - begin;
            const auto rendered = Render(system, end + 2 * loop);
            for (std::uint32_t i = 0; i < rendered.size(); ++i) {
                const auto frame = i >= 3 ? i - 3 : 0;
                const auto index = frame < end ? frame : begin + (frame - end) % loop;
                Require(rendered[i] == (i < 3 ? 0.0f : reference[256 + index]));
            }
            Require(sceNgs2SystemDestroy(system, nullptr) == 0);
        }
    }
    const auto file = LoopedFile(100, 700, 0);
    Ngs2WaveformInfo info{};
    Require(sceNgs2ParseWaveformData(file.data(), 168, &info) == SCE_NGS2_OK);
    Require(info.block[1].num_repeats == UINT32_MAX);
    const auto system = CreateSystem();
    const auto voice = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, info.format});
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS,
        Ngs2SamplerVoiceWaveformBlocksParam{{}, file.data(), 0, info.num_blocks, info.block});
    Patch(voice, Mastering(system, 1));
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    auto rendered = Render(system, 1024);
    const Ngs2VoiceParamHeader exitLoop{sizeof(Ngs2VoiceParamHeader), 0, SCE_NGS2_SAMPLER_VOICE_PARAM_EXIT_LOOP};
    Require(sceNgs2VoiceControl(voice, &exitLoop) == SCE_NGS2_OK);
    const auto tail = Render(system, samples + 600 - 1024 + Grain);
    rendered.insert(rendered.end(), tail.begin(), tail.end());
    for (std::uint32_t i = 0; i < rendered.size(); ++i) {
        const auto frame = i >= 3 ? i - 3 : 0;
        const auto index = frame < 700 ? frame : frame - 600;
        Require(rendered[i] == (i >= 3 && frame < samples + 600 ? reference[256 + index] : 0.0f));
    }
    Require(Flags(voice) == 0);
    Require(sceNgs2SystemDestroy(system, nullptr) == 0);
}

static bool ParseThrows(const std::vector<std::uint8_t>& file) {
    Ngs2WaveformInfo info{};
    try {
        sceNgs2ParseWaveformData(file.data(), file.size(), &info);

    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static std::vector<std::uint8_t> ShortSampler(std::vector<std::uint8_t> file, std::uint32_t size) {
    file.erase(file.begin() + 100 + size, file.begin() + 160);
    Set32(file, 96, size);
    Set32(file, 4, static_cast<std::uint32_t>(file.size() - 8));
    return file;
}

static void TestMalformedLoops() {
    const auto original = LoopedFile(100, 700, 0);
    Ngs2WaveformInfo expected{};
    Require(sceNgs2ParseWaveformData(original.data(), original.size(), &expected) == SCE_NGS2_OK && expected.num_blocks == 3);
    for (const auto [offset, value] : {std::pair<std::size_t, std::uint32_t>{128, 2}, {140, 1}, {140, 2}, {152, 0x80000000u}, {156, 5}}) {
        auto file = original;
        Set32(file, offset, value);
        Ngs2WaveformInfo info{};
        Require(sceNgs2ParseWaveformData(file.data(), file.size(), &info) == SCE_NGS2_OK);
        Require(info.loop_begin_position == 100 && info.loop_end_position == 700 && info.num_blocks == 3);
        for (std::uint32_t block = 0; block < 3; ++block) Require(std::memcmp(&info.block[block], &expected.block[block], sizeof(Ngs2WaveformBlock)) == 0);
    }
    for (const auto [start, last, begin, end] : {std::tuple<std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t>{256, 255, 0, 0}, {356, 255, 100, 0}, {956, 955, 700, 700}, {956, 700, 700, 445}}) {
        auto file = original;
        Set32(file, 144, start);
        Set32(file, 148, last);
        Ngs2WaveformInfo info{};
        Require(sceNgs2ParseWaveformData(file.data(), file.size(), &info) == SCE_NGS2_OK);
        Require(info.loop_begin_position == begin && info.loop_end_position == end && info.num_blocks == 1);
        Require(info.block[0].num_repeats == 0 && info.block[0].num_samples == info.num_samples);
    }
    for (const std::uint32_t last : {4096u, 5000u, 0xffffffffu}) {
        auto file = original;
        Set32(file, 148, last);
        Require(ParseThrows(file));
    }
    for (const std::uint32_t start : {0u, 255u}) {
        auto file = original;
        Set32(file, 144, start);
        Require(ParseThrows(file));
    }
    Ngs2WaveformInfo info{};
    auto pastData = original;
    Set32(pastData, 80, 4 * SuperframeSamples - 255);
    Require(ParseThrows(pastData));
    Set32(pastData, 80, 4 * SuperframeSamples - 256);
    Require(sceNgs2ParseWaveformData(pastData.data(), pastData.size(), &info) == SCE_NGS2_OK && info.num_blocks == 3);
    for (std::size_t size = 92; size < 168; ++size)
        Require(sceNgs2ParseWaveformData(original.data(), size, &info) == SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA);
    for (const std::uint32_t size : {28u, 36u, 50u}) Require(ParseThrows(ShortSampler(original, size)));
    const auto minimal = ShortSampler(original, 52);
    Require(sceNgs2ParseWaveformData(minimal.data(), minimal.size(), &info) == SCE_NGS2_OK && info.num_blocks == 3);
}

static std::uint8_t* SuperframeBeforeGuardPage() {
#ifdef _WIN32
    SYSTEM_INFO info{};
    GetSystemInfo(&info);
    const std::size_t page = info.dwPageSize;
    auto* region = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 2 * page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    DWORD previous = 0;
    Require(region != nullptr && VirtualProtect(region + page, page, PAGE_NOACCESS, &previous) != 0);
#else
    const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    void* mapping = mmap(nullptr, 2 * page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(mapping != MAP_FAILED);
    auto* region = static_cast<std::uint8_t*>(mapping);
    Require(mprotect(region + page, page, PROT_NONE) == 0);
#endif
    auto* superframe = region + page - SuperframeBytes;
    std::mt19937 random(50);
    for (std::uint32_t i = 0; i < SuperframeBytes; i++) superframe[i] = static_cast<std::uint8_t>(random());
    return superframe;
}

static void TestCorruptSuperframeAtPageEnd() {
    const auto* superframe = SuperframeBeforeGuardPage();
    const auto system = CreateSystem();
    const auto voice = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_ATRAC9, 1, 48000, Config, 0, 0}});
    const Ngs2WaveformBlock block{0, SuperframeBytes, 0, 0, SuperframeSamples, 0, 0};
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, superframe, 0, 1, &block});
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    Patch(voice, Mastering(system, 1));
    bool rejected = false;
    try {
        Render(system, SuperframeSamples);
    } catch (const std::runtime_error& error) {
        rejected = std::strstr(error.what(), "ATRAC9 decode failed") != nullptr;
    }
    Require(rejected);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static uintptr_t EmptySampler(uintptr_t system) {
    const auto voice = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_ATRAC9, 1, 48000, Config, 0, 0}});
    Patch(voice, Mastering(system, 1));
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    return voice;
}

static void Queue(uintptr_t voice, const void* data, std::uint32_t bytes, std::uint32_t flags, std::uint32_t samples = 0, std::uint32_t skip = 0) {
    const Ngs2WaveformBlock block{0, bytes, 0, skip, samples, 0, 0};
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, data, flags, 1, &block});
}

static void TestStreamPartitions() {
    const auto reference = Reference(2);
    std::vector<std::uint8_t> data(Superframe, Superframe + SuperframeBytes);
    data.insert(data.end(), Superframe, Superframe + SuperframeBytes);
    for (const auto skip : {100u, 1100u}) {
        for (const float pitch : {0.5f, 1.0f, 1.5f}) {
            const auto system = CreateSystem();
            const auto voice = EmptySampler(system);
            Control(voice, SCE_NGS2_VOICE_PARAM_CALLBACK,
                Ngs2VoiceCallbackParam{{}, OnBlock, 0, SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END, 0});
            Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_PITCH, Ngs2SamplerVoicePitchParam{{}, pitch});
            callbackFlags.clear();
            const std::uint32_t sizes[]{1, 17, 61, 93, 340};
            std::uint32_t offset = 0;
            for (const auto size : sizes) {
                const auto flags = offset == 0 ? SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE
                    : SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND | (offset + size == data.size() ? 0 : SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE);
                Queue(voice, data.data() + offset, size, flags, 800, skip);
                offset += size;
            }
            const auto output = Render(system, 1664);
            for (std::size_t i = 0; i < output.size(); ++i) {
                const float expected = SamplerReference(i, pitch, [&](std::uint32_t index) {
                    return index < 800 ? reference[skip + index] : 0.0f;
                });
                Require(std::abs(output[i] - expected) < 1e-6f);
            }
            Require(callbackFlags.size() == 1 && Flags(voice) == 0);
            Ngs2SamplerVoiceState state{};
            Require(sceNgs2VoiceGetState(voice, &state.voice_state, sizeof(state)) == SCE_NGS2_OK);
            Require(state.num_decoded_samples == 0);
            Require(sceNgs2SystemDestroy(system, nullptr) == 0);
        }
    }
}

static void TestRefill(const std::vector<float>& reference) {
    for (const bool contiguous : {false, true}) {
        const auto system = CreateSystem();
        const auto voice = EmptySampler(system);
        std::vector<std::uint8_t> tail(Superframe + 13, Superframe + SuperframeBytes);
        callbackFlags.clear();
        Control(voice, SCE_NGS2_VOICE_PARAM_CALLBACK,
            Ngs2VoiceCallbackParam{{}, OnBlock, 0, SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END, 0});
        Queue(voice, Superframe, 13, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE, 500, 100);
        for (const float value : Render(system, Grain)) Require(value == 0.0f);
        Require(callbackFlags.empty());
        Ngs2SamplerVoiceState state{};
        Require(sceNgs2VoiceGetState(voice, &state.voice_state, sizeof(state)) == SCE_NGS2_OK);
        Require(state.num_decoded_samples == 0 && state.decoded_data_size == 0 && state.waveform_data == nullptr);
        Queue(voice, contiguous ? Superframe + 13 : tail.data(), SuperframeBytes - 13, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND);
        const auto output = Render(system, 512);
        for (std::uint32_t i = 0; i < output.size(); ++i) Require(std::abs(output[i] - (i < 499 ? reference[101 + i] : 0.0f)) < 1e-6f);
        Require(callbackFlags.size() == (contiguous ? 1u : 2u) && Flags(voice) == 0);
        Require(sceNgs2SystemDestroy(system, nullptr) == 0);
    }
}

static void TestStarvationAndTruncation(const std::vector<float>& reference) {
    for (bool truncate : {false, true}) {
        const auto system = CreateSystem();
        const auto voice = EmptySampler(system);
        Queue(voice, Superframe, 13, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE, 500, 100);
        for (float value : Render(system, Grain * 3)) Require(value == 0.0f);
        Require((Flags(voice) & SCE_NGS2_VOICE_STATE_FLAG_PLAYING) != 0);
        if (truncate) {
            Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, nullptr, 0, 0, nullptr});
            bool rejected = false;
            try { Render(system, Grain); } catch (const std::invalid_argument&) { rejected = true; }
            Require(rejected);
        } else {
            Queue(voice, Superframe + 13, SuperframeBytes - 13, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND);
            const auto output = Render(system, 512);
            for (std::size_t i = 0; i < output.size(); ++i) Require(output[i] == (i < 499 ? reference[101 + i] : 0.0f));
            Require(Flags(voice) == 0);
        }
        Require(sceNgs2SystemDestroy(system, nullptr) == 0);
    }
}

static void TestAppendBoundaries(const std::vector<float>& reference) {
    for (std::uint32_t split = 1; split < SuperframeBytes; ++split) {
        const auto system = CreateSystem();
        const auto voice = EmptySampler(system);
        Queue(voice, Superframe, split, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE, 500, 100);
        for (float value : Render(system, Grain)) Require(value == 0.0f);
        const Ngs2WaveformBlock block{split, SuperframeBytes - split, UINT32_MAX, UINT32_MAX, UINT32_MAX, 0, 0};
        Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS,
            Ngs2SamplerVoiceWaveformBlocksParam{{}, Superframe, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND, 1, &block});
        const auto output = Render(system, 512);
        for (std::size_t i = 0; i < output.size(); ++i) Require(output[i] == (i < 499 ? reference[101 + i] : 0.0f));
        Ngs2SamplerVoiceState state{};
        Require(sceNgs2VoiceGetState(voice, &state.voice_state, sizeof(state)) == SCE_NGS2_OK);
        Require(state.num_decoded_samples == 0 && state.decoded_data_size == 0);
        Require(state.waveform_data == nullptr && Flags(voice) == 0);
        Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    }
}

static void TestAppendWithoutWaveform() {
    const auto system = CreateSystem();
    const auto voice = EmptySampler(system);
    bool rejected = false;
    try {
        Queue(voice, Superframe, SuperframeBytes, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND, 500);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected);
    Queue(voice, Superframe, SuperframeBytes, 0, 500);
    Render(system, 512);
    Require(Flags(voice) == 0);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestSilenceFlagRejected() {
    const auto system = CreateSystem();
    const auto voice = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_ATRAC9, 1, 48000, Config, 0, 0}});
    const Ngs2WaveformBlock block{0, 0, 0, 0, SuperframeSamples, 0, 0};
    bool rejected = false;
    try {
        Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, Superframe, 0x10, 1, &block});
    } catch (const std::runtime_error& error) {
        rejected = std::strstr(error.what(), "silence flag") != nullptr;
    }
    Require(rejected);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestReplacement() {
    const auto reference = Reference(3);
    Require(std::any_of(reference.begin(), reference.end(), [](float sample) { return std::abs(sample) > 0.01f; }));
    const auto system = CreateSystem();
    Require(sceNgs2SystemSetGrainSamples(system, 64) == SCE_NGS2_OK);
    const auto voice = EmptySampler(system);
    std::vector<std::uint8_t> source;
    for (std::uint32_t i = 0; i < 8; ++i) source.insert(source.end(), Superframe, Superframe + SuperframeBytes);
    const std::vector<std::uint8_t> replacement(source.begin(), source.begin() + 3 * SuperframeBytes);
    Queue(voice, source.data(), source.size(), 0, 2048, 256);
    callbackFlags.clear();
    Control(voice, SCE_NGS2_VOICE_PARAM_CALLBACK, Ngs2VoiceCallbackParam{{}, OnBlock, 0, SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END, 0});
    for (std::uint32_t grain = 0; grain < 43; ++grain) {
        if (grain == 10) {
            Queue(voice, replacement.data(), replacement.size(), SCE_NGS2_WAVEFORM_BLOCKS_FLAG_RESET, 256, 512);
            std::fill(source.begin(), source.end(), 0xff);
        }
        if (grain == 36) {
            Event(voice, SCE_NGS2_VOICE_EVENT_STOP_IMM);
            Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
        }
        const auto output = Render(system, 64, 64);
        for (std::uint32_t i = 0; i < output.size(); ++i) {
            const auto frame = (grain < 36 ? grain : grain - 36) * 64 + i;
            const auto length = grain < 36 ? 2048u : 256u;
            const auto skip = grain < 36 ? 256u : 512u;
            const float expected = frame >= 3 && frame < length + 3 ? reference[skip + frame - 3] : 0.0f;
            Require(std::abs(output[i] - expected) < 1e-6f);
        }
        Require(callbackFlags.size() == (grain < 28 ? 0u : grain < 36 ? 1u : 2u));
    }
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestStreamingReplay() {
    const auto reference = Reference(3);
    for (const bool split : {false, true}) {
        const auto system = CreateSystem();
        Require(sceNgs2SystemSetGrainSamples(system, 64) == SCE_NGS2_OK);
        const auto voice = EmptySampler(system);
        std::vector<std::uint8_t> source;
        for (std::uint32_t i = 0; i < 3; ++i) source.insert(source.end(), Superframe, Superframe + SuperframeBytes);
        const std::vector<std::uint8_t> middle(source.begin() + SuperframeBytes, source.end());
        const std::vector<std::uint8_t> tail(source.begin() + 2 * SuperframeBytes, source.end());
        callbackFlags.clear();
        Control(voice, SCE_NGS2_VOICE_PARAM_CALLBACK, Ngs2VoiceCallbackParam{{}, OnBlock, 0, SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END, 0});
        Queue(voice, source.data(), SuperframeBytes, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE, 2048, 256);
        for (std::uint32_t grain = 0; grain < 71; ++grain) {
            if (grain == 13) Queue(voice, middle.data(), split ? SuperframeBytes : middle.size(),
                SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND | (split ? SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE : 0));
            if (grain == 29 && split) Queue(voice, tail.data(), tail.size(), SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND);
            if (grain == 36) {
                Event(voice, SCE_NGS2_VOICE_EVENT_STOP_IMM);
                Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
            }
            const auto output = Render(system, 64, 64);
            for (std::uint32_t i = 0; i < output.size(); ++i) {
                float expected = 0.0f;
                if (grain < 13) {
                    const auto frame = grain * 64 + i;
                    if (frame >= 3 && frame < 771) expected = reference[256 + frame - 3];
                } else if (grain < 36) {
                    const auto frame = (grain - 13) * 64 + i;
                    if (!split && frame < 1279) expected = reference[1025 + frame];
                    if (split && grain < 29 && frame < 1023) expected = reference[1025 + frame];
                    const auto last = (grain - 29) * 64 + i;
                    if (split && grain >= 29 && last < 255) expected = reference[2049 + last];
                } else {
                    const auto frame = (grain - 36) * 64 + i;
                    if (frame >= 3 && frame < 2051) expected = reference[256 + frame - 3];
                }
                Require(std::abs(output[i] - expected) < 1e-6f);
            }
            const auto expectedCallbacks = split ? (grain < 13 ? 0u : grain < 29 ? 1u : grain < 44 ? 3u : grain < 60 ? 4u : grain < 64 ? 5u : 6u)
                : grain < 13 ? 0u : grain < 28 ? 1u : grain < 44 ? 2u : grain < 64 ? 3u : 4u;
            Require(callbackFlags.size() == expectedCallbacks);
        }
        Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    }
}

int main() {
    const auto reference = Reference();
    TestSkipAndBlockEnd(reference);
    TestRepeatAndState(reference);
    TestParse();
    TestParseLoops();
    TestMalformedLoops();
    TestCalcBlock();
    TestFrameInfo();
    TestCalcPcmBlock();
    TestCalcVagBlock();
    TestCorruptSuperframeAtPageEnd();
    TestStreamPartitions();
    TestRefill(reference);
    TestReplacement();
    TestStreamingReplay();
    TestStarvationAndTruncation(reference);
    TestAppendBoundaries(reference);
    TestAppendWithoutWaveform();
    TestSilenceFlagRejected();
    return 0;
}
