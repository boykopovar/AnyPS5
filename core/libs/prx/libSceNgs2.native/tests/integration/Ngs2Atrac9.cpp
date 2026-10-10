#include "Ngs2Test.hpp"

#include "libatrac9.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <random>
#include <stdexcept>
#include <string>
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

namespace {

using namespace Ngs2Testing;
using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;
using Testing::RequireThrows;

constexpr std::uint32_t Config = 0xFE7007F0;
constexpr std::uint32_t SuperframeBytes = 256;
constexpr std::uint32_t SuperframeSamples = 1024;

const std::uint8_t Superframe[SuperframeBytes] = {
    0x21, 0xf0, 0x08, 0x42, 0x03, 0x1b, 0x99, 0x5f, 0x30, 0xf4, 0x30, 0xf5, 0xf9, 0xca, 0x41, 0x69,
    0x19, 0x6b, 0x98, 0x7d, 0x97, 0x04, 0xe0, 0x36, 0xd6, 0x2a, 0x0c, 0x98, 0x48, 0x95, 0xca, 0xa9,
    0x85, 0xd2, 0xeb, 0x88, 0xee, 0x84, 0xe7, 0x1e, 0x78, 0x17, 0x13, 0x72, 0xe1, 0x55, 0xdb, 0xfb,
    0xc0, 0x73, 0x78, 0x57, 0x78, 0x58, 0x8d, 0x4e, 0x82, 0x99, 0xe6, 0xca, 0x2e, 0xb1, 0x73, 0xb3,
    0x83, 0x5e, 0x3c, 0x71, 0xe3, 0x23, 0x8f, 0x5b, 0x3b, 0xa3, 0xcd, 0x1c, 0xf8, 0xa9, 0x17, 0xb8,
    0x1c, 0x8f, 0x0f, 0x94, 0xe8, 0x9c, 0xe2, 0x6a, 0x35, 0xc9, 0x35, 0x63, 0x7a, 0xa8, 0x16, 0xfd,
    0x07, 0x52, 0x6f, 0xea, 0x76, 0xfe, 0x45, 0xb1, 0xa1, 0x13, 0xcd, 0x99, 0xbc, 0xe5, 0x51, 0xd8,
    0x74, 0x60, 0x6d, 0x5c, 0xfc, 0x4f, 0xc7, 0x31, 0x18, 0xfc, 0x94, 0x92, 0xa4, 0xab, 0x90, 0xab,
    0xbd, 0xff, 0x95, 0x73, 0xfd, 0xdb, 0x24, 0x9f, 0xee, 0xf2, 0x38, 0xaa, 0xd7, 0x8a, 0x82, 0x4b,
    0xea, 0xef, 0xa8, 0xe7, 0x3f, 0x6f, 0x18, 0x57, 0x6b, 0xad, 0xea, 0x0f, 0x58, 0x7a, 0xbb, 0x55,
    0x79, 0x50, 0xc9, 0x76, 0x2b, 0xed, 0x15, 0x19, 0xdf, 0x4e, 0xc3, 0xb6, 0xd8, 0x46, 0x02, 0xc2,
    0x35, 0xaa, 0xa9, 0x74, 0x19, 0xc2, 0xcc, 0x84, 0x6f, 0xb6, 0x93, 0xc7, 0x30, 0x4e, 0x1c, 0xa9,
    0x9f, 0xe5, 0x1f, 0xca, 0xd3, 0x35, 0xe4, 0xb2, 0x83, 0xa7, 0x69, 0x5b, 0xed, 0x6b, 0x1d, 0x23,
    0x15, 0xc7, 0x97, 0x2e, 0x89, 0x63, 0xb2, 0xf8, 0x19, 0x45, 0xbf, 0x49, 0x94, 0xdb, 0xf2, 0x33,
    0xc1, 0x12, 0xfd, 0x94, 0x47, 0xe1, 0x46, 0xca, 0x89, 0xaf, 0x25, 0x76, 0x1a, 0x42, 0x99, 0x2f,
    0x09, 0x31, 0x5d, 0x92, 0x94, 0x0f, 0x8a, 0xae, 0x55, 0x49, 0xf7, 0xe9, 0x25, 0xea, 0x1c, 0xe6
};

std::vector<float> Reference(std::uint32_t superframes = 1) {
    std::uint8_t config[4] = {0xFE, 0x70, 0x07, 0xF0};
    void* decoder = Atrac9GetHandle();
    std::vector<float> pcm(SuperframeSamples * superframes);
    int initResult = Atrac9InitDecoder(decoder, config);
    int decodeResult = 0;
    for (std::uint32_t s = 0; initResult == 0 && decodeResult == 0 && s < superframes; ++s) {
        int offset = 0;
        for (std::uint32_t frame = 0; decodeResult == 0 && frame < 4; frame++) {
            int used = 0;
            decodeResult = Atrac9DecodeF32(decoder, Superframe + offset, static_cast<int>(SuperframeBytes) - offset,
                                           pcm.data() + s * SuperframeSamples + frame * 256, &used, 0);
            offset += used;
        }
    }
    Atrac9ReleaseHandle(decoder);
    RequireEqual(initResult, 0, "initialize the reference decoder");
    RequireEqual(decodeResult, 0, "decode the reference superframe");
    return pcm;
}

struct CallbackRecorder {
    std::vector<std::uint32_t> flags;
};

void APS5_VABI OnBlock(const Ngs2VoiceCallbackInfo* info) {
    reinterpret_cast<CallbackRecorder*>(info->callback_data)->flags.push_back(info->flag);
}

void RecordCallbacks(uintptr_t voice, CallbackRecorder& recorder, std::uint32_t flags) {
    Control(voice, SCE_NGS2_VOICE_PARAM_CALLBACK,
            Ngs2VoiceCallbackParam{{}, OnBlock, reinterpret_cast<std::uintptr_t>(&recorder), flags, 0});
}

uintptr_t Sampler(Ngs2Fixture& ngs2, uintptr_t system, std::uint32_t skip, std::uint32_t samples, std::uint32_t repeats,
                  CallbackRecorder& recorder) {
    const auto voice = Voice(ngs2.CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP,
            Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_ATRAC9, 1, 48000, Config, 0, 0}});
    const Ngs2WaveformBlock block{0, SuperframeBytes, repeats, skip, samples, 0, 0};
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, Superframe, 0, 1, &block});
    RecordCallbacks(voice, recorder, SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END | SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_REPEAT);
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    return voice;
}

std::vector<float> Render(uintptr_t system, std::uint32_t samples) {
    std::vector<float> rendered;
    std::vector<float> out(Grain);
    const Ngs2RenderBufferInfo info{out.data(), out.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 1};
    while (rendered.size() < samples) {
        RequireEqual(sceNgs2SystemRender(system, &info, 1), SCE_NGS2_OK, "render");
        rendered.insert(rendered.end(), out.begin(), out.end());
    }
    return rendered;
}

void RequireSilence(const std::vector<float>& output, const std::string& message) {
    for (std::size_t i = 0; i < output.size(); ++i) RequireEqual(output[i], 0.0f, message + " sample " + std::to_string(i));
}

void RequireReferenceThenSilence(const std::vector<float>& output, const std::vector<float>& reference, std::size_t offset,
                                 std::size_t count, const std::string& message) {
    for (std::size_t i = 0; i < output.size(); ++i) {
        RequireEqual(output[i], i < count ? reference[offset + i] : 0.0f, message + " sample " + std::to_string(i));
    }
}

Ngs2SamplerVoiceState SamplerState(uintptr_t voice) {
    Ngs2SamplerVoiceState state{};
    RequireEqual(sceNgs2VoiceGetState(voice, &state.voice_state, sizeof(state)), SCE_NGS2_OK, "get sampler state");
    return state;
}

const Case skipAndBlockEnd{"Atrac9Sampler_SkipAndSampleCount_RendersWindowAndSignalsBlockEnd", [] {
    const auto reference = Reference();
    Ngs2Fixture ngs2;
    CallbackRecorder recorder;
    const auto system = ngs2.CreateSystem();
    Patch(Sampler(ngs2, system, 100, 500, 0, recorder), ngs2.Mastering(system, 1));
    const auto rendered = Render(system, 512);
    RequireReferenceThenSilence(rendered, reference, 100, 500, "rendered");
    RequireEqual(recorder.flags.size(), std::size_t{1}, "callback count");
    RequireEqual(recorder.flags[0], SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END, "callback flag");
}};

const Case repeatAndState{"Atrac9Sampler_RepeatedBlock_PlaysTwiceAndReportsState", [] {
    const auto reference = Reference();
    Ngs2Fixture ngs2;
    CallbackRecorder recorder;
    const auto system = ngs2.CreateSystem();
    const auto sampler = Sampler(ngs2, system, 0, 300, 1, recorder);
    Patch(sampler, ngs2.Mastering(system, 1));
    auto rendered = Render(system, 304);
    const auto state = SamplerState(sampler);
    RequireEqual(state.num_decoded_samples, 304u, "decoded samples");
    RequireEqual(state.decoded_data_size, 2 * SuperframeBytes, "decoded bytes");
    RequireEqual(static_cast<const void*>(state.waveform_data), static_cast<const void*>(Superframe + SuperframeBytes),
                 "waveform position");
    const auto second = Render(system, 304);
    rendered.insert(rendered.end(), second.begin(), second.end());
    for (std::uint32_t i = 0; i < 608; i++) {
        RequireEqual(rendered[i], i < 600 ? reference[i % 300] : 0.0f, "sample " + std::to_string(i));
    }
    RequireEqual(recorder.flags.size(), std::size_t{2}, "callback count");
    RequireEqual(recorder.flags[0], SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_REPEAT, "first callback flag");
    RequireEqual(recorder.flags[1], SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END, "second callback flag");
    RequireEqual(Flags(sampler), 0u, "stopped");
}};

void Put16(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value));
    out.push_back(static_cast<std::uint8_t>(value >> 8));
}

void Put32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    Put16(out, value & 0xffff);
    Put16(out, value >> 16);
}

void PutTag(std::vector<std::uint8_t>& out, const char* tag) {
    out.insert(out.end(), tag, tag + 4);
}

std::vector<std::uint8_t> At9File(std::uint32_t sampleRate) {
    static constexpr std::uint8_t guid[16] = {0xd2, 0x42, 0xe1, 0x47, 0xba, 0x36, 0x8d, 0x4d,
                                              0x88, 0xfc, 0x61, 0x65, 0x4f, 0x8c, 0x83, 0x6c};
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

const Case parse{"ParseWaveformData_Atrac9File_ReportsFormatAndBlock", [] {
    const auto file = At9File(48000);
    Ngs2WaveformInfo info{};
    RequireEqual(sceNgs2ParseWaveformData(file.data(), file.size(), nullptr), SCE_NGS2_ERROR_INVALID_OUT_ADDRESS, "null output");
    RequireEqual(sceNgs2ParseWaveformData(file.data(), file.size(), &info), SCE_NGS2_OK, "parse");
    RequireEqual(info.format.waveform_type, SCE_NGS2_WAVEFORM_TYPE_ATRAC9, "waveform type");
    RequireEqual(info.format.num_channels, 1u, "channels");
    RequireEqual(info.format.sample_rate, 48000u, "sample rate");
    RequireEqual(info.format.config_data, Config, "config");
    RequireEqual(info.data_offset, file.size() - SuperframeBytes, "data offset");
    RequireEqual(info.data_size, SuperframeBytes, "data size");
    RequireEqual(info.num_samples, 900u, "samples");
    RequireEqual(info.num_delay_samples, 256u, "delay samples");
    RequireEqual(info.audio_unit_size, 64u, "audio unit size");
    RequireEqual(info.num_audio_unit_samples, 256u, "audio unit samples");
    RequireEqual(info.num_audio_unit_per_frame, 4u, "audio units per frame");
    RequireEqual(info.audio_frame_size, SuperframeBytes, "audio frame size");
    RequireEqual(info.num_audio_frame_samples, SuperframeSamples, "audio frame samples");
    RequireEqual(info.num_blocks, 1u, "blocks");
    RequireEqual(info.block[0].data_offset, info.data_offset, "block data offset");
    RequireEqual(info.block[0].data_size, SuperframeBytes, "block data size");
    RequireEqual(info.block[0].num_skip_samples, 256u, "block skip samples");
    RequireEqual(info.block[0].num_samples, 900u, "block samples");
}};

const Case parseInvalid{"ParseWaveformData_TruncatedRawOrWrongRate_Fails", [] {
    const auto file = At9File(48000);
    Ngs2WaveformInfo info{};
    RequireEqual(sceNgs2ParseWaveformData(file.data(), 11, &info), SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA, "11 bytes");
    RequireEqual(sceNgs2ParseWaveformData(Superframe, SuperframeBytes, &info), SCE_NGS2_ERROR_UNKNOWN_WAVEFORM_FORMAT,
                 "raw superframe");
    const auto wrongRate = At9File(44100);
    RequireEqual(sceNgs2ParseWaveformData(wrongRate.data(), wrongRate.size(), &info), SCE_NGS2_ERROR_INVALID_WAVEFORM_FORMAT,
                 "44100 Hz");
}};

const Case calcBlock{"CalcWaveformBlock_Atrac9Range_AlignsToSuperframes", [] {
    const Ngs2WaveformFormat format{SCE_NGS2_WAVEFORM_TYPE_ATRAC9, 1, 48000, Config, 0, 0};
    Ngs2WaveformBlock block{};
    RequireEqual(sceNgs2CalcWaveformBlock(&format, 1500, 600, nullptr), SCE_NGS2_ERROR_INVALID_OUT_ADDRESS, "null output");
    RequireEqual(sceNgs2CalcWaveformBlock(&format, 1500, 600, &block), SCE_NGS2_OK, "600 samples at 1500");
    RequireEqual(block.data_offset, SuperframeBytes, "data offset");
    RequireEqual(block.data_size, 2 * SuperframeBytes, "data size");
    RequireEqual(block.num_skip_samples, 476u, "skip samples");
    RequireEqual(block.num_samples, 600u, "samples");
    RequireEqual(sceNgs2CalcWaveformBlock(&format, 1500, 0, &block), SCE_NGS2_OK, "0 samples at 1500");
    RequireEqual(block.data_offset, SuperframeBytes, "empty data offset");
    RequireEqual(block.data_size, 0u, "empty data size");
    RequireEqual(block.num_skip_samples, 0u, "empty skip samples");
    RequireEqual(block.num_samples, 0u, "empty samples");
}};

const Case calcBlockChannels{"CalcWaveformBlock_ChannelCountMismatch_FailsInvalidFormat", [] {
    Ngs2WaveformBlock block{};
    const Ngs2WaveformFormat stereo{SCE_NGS2_WAVEFORM_TYPE_ATRAC9, 2, 48000, Config, 0, 0};
    RequireEqual(sceNgs2CalcWaveformBlock(&stereo, 0, 1, &block), SCE_NGS2_ERROR_INVALID_WAVEFORM_FORMAT, "2 channels");
    const Ngs2WaveformFormat silent{SCE_NGS2_WAVEFORM_TYPE_ATRAC9, 0, 48000, Config, 0, 0};
    RequireEqual(sceNgs2CalcWaveformBlock(&silent, 0, 1, &block), SCE_NGS2_ERROR_INVALID_WAVEFORM_FORMAT, "0 channels");
}};

void Set32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) bytes[offset + i] = static_cast<std::uint8_t>(value >> (i * 8));
}

std::vector<std::uint8_t> LoopedFile(std::uint32_t begin, std::uint32_t end, std::uint32_t plays, bool trailingSampler = false) {
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

constexpr std::uint32_t loopedSamples = 4 * SuperframeSamples - 256;

const std::pair<std::uint32_t, std::uint32_t> loopRanges[] = {
    {0u, 800u}, {100u, 700u}, {768u, 2600u}, {850u, 2600u}, {1024u, 2600u}, {3072u, loopedSamples}, {0u, loopedSamples}};

std::string LoopName(std::uint32_t begin, std::uint32_t end, bool trailingSampler) {
    return "loop " + std::to_string(begin) + "-" + std::to_string(end) + (trailingSampler ? " trailing smpl" : " leading smpl");
}

uintptr_t LoopedVoice(Ngs2Fixture& ngs2, uintptr_t system, const std::vector<std::uint8_t>& file, const Ngs2WaveformInfo& info) {
    const auto voice = Voice(ngs2.CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, info.format});
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS,
            Ngs2SamplerVoiceWaveformBlocksParam{{}, file.data(), 0, info.num_blocks, info.block});
    Patch(voice, ngs2.Mastering(system, 1));
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    return voice;
}

const Case parseLoops{"ParseWaveformData_SamplerChunk_SplitsBlocksAroundLoop", [] {
    for (bool trailingSampler : {false, true}) {
        for (const auto& [begin, end] : loopRanges) {
            const auto name = LoopName(begin, end, trailingSampler);
            const auto file = LoopedFile(begin, end, 2, trailingSampler);
            Ngs2WaveformInfo info{};
            RequireEqual(sceNgs2ParseWaveformData(file.data(), file.size(), &info), SCE_NGS2_OK, name + " parse");
            RequireEqual(info.loop_begin_position, begin, name + " loop begin");
            RequireEqual(info.loop_end_position, end, name + " loop end");
            RequireEqual(info.num_samples, loopedSamples, name + " samples");
            RequireEqual(info.num_blocks, 1u + (begin != 0) + (end != loopedSamples), name + " blocks");
            RequireEqual(info.block[begin != 0].num_repeats, 1u, name + " loop block repeats");
        }
    }
}};

const Case renderLoops{"Atrac9Sampler_ParsedLoopBlocks_PlayLoopTwiceThenTail", [] {
    const auto reference = Reference(4);
    for (bool trailingSampler : {false, true}) {
        for (const auto& [begin, end] : loopRanges) {
            const auto name = LoopName(begin, end, trailingSampler);
            const auto file = LoopedFile(begin, end, 2, trailingSampler);
            Ngs2WaveformInfo info{};
            RequireEqual(sceNgs2ParseWaveformData(file.data(), file.size(), &info), SCE_NGS2_OK, name + " parse");
            Ngs2Fixture ngs2;
            const auto system = ngs2.CreateSystem();
            const auto voice = LoopedVoice(ngs2, system, file, info);
            const auto rendered = Render(system, loopedSamples + end - begin + Grain);
            for (std::uint32_t i = 0; i < rendered.size(); ++i) {
                const auto index = i < end ? i : i - (end - begin);
                RequireEqual(rendered[i], i < loopedSamples + end - begin ? reference[256 + index] : 0.0f,
                             name + " sample " + std::to_string(i));
            }
            RequireEqual(Flags(voice), 0u, name + " stopped");
        }
    }
}};

const Case infiniteLoop{"Atrac9Sampler_InfiniteLoopExitLoop_FinishesWithTail", [] {
    const auto reference = Reference(4);
    const auto file = LoopedFile(100, 700, 0);
    Ngs2WaveformInfo info{};
    RequireEqual(sceNgs2ParseWaveformData(file.data(), 168, &info), SCE_NGS2_OK, "parse the header");
    RequireEqual(info.block[1].num_repeats, UINT32_MAX, "infinite repeats");
    Ngs2Fixture ngs2;
    const auto system = ngs2.CreateSystem();
    const auto voice = LoopedVoice(ngs2, system, file, info);
    auto rendered = Render(system, 1024);
    const Ngs2VoiceParamHeader exitLoop{sizeof(Ngs2VoiceParamHeader), 0, SCE_NGS2_SAMPLER_VOICE_PARAM_EXIT_LOOP};
    RequireEqual(sceNgs2VoiceControl(voice, &exitLoop), SCE_NGS2_OK, "exit loop");
    const auto tail = Render(system, loopedSamples + 600 - 1024 + Grain);
    rendered.insert(rendered.end(), tail.begin(), tail.end());
    for (std::uint32_t i = 0; i < rendered.size(); ++i) {
        const auto index = i < 700 ? i : i - 600;
        RequireEqual(rendered[i], i < loopedSamples + 600 ? reference[256 + index] : 0.0f, "sample " + std::to_string(i));
    }
    RequireEqual(Flags(voice), 0u, "stopped");
}};

const Case malformedLoopFields{"ParseWaveformData_MalformedSamplerFields_FailsOrIsUnsupported", [] {
    const auto original = LoopedFile(100, 700, 0);
    Ngs2WaveformInfo info{};
    for (const auto& [offset, value] : {std::pair<std::size_t, std::uint32_t>{128, 2}, {144, 0}, {148, 255}, {148, 5000}}) {
        auto file = original;
        Set32(file, offset, value);
        RequireEqual(sceNgs2ParseWaveformData(file.data(), file.size(), &info), SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA,
                     "offset " + std::to_string(offset) + " value " + std::to_string(value));
    }
    for (const auto offset : {140u, 152u}) {
        auto file = original;
        Set32(file, offset, 1);
        RequireThrows<std::runtime_error>([&] { sceNgs2ParseWaveformData(file.data(), file.size(), &info); },
                                          "unsupported field at offset " + std::to_string(offset));
    }
}};

const Case malformedLoopSizes{"ParseWaveformData_TruncatedOrShortSamplerChunk_FailsInvalidData", [] {
    const auto original = LoopedFile(100, 700, 0);
    Ngs2WaveformInfo info{};
    for (std::size_t size = 92; size < 168; ++size) {
        RequireEqual(sceNgs2ParseWaveformData(original.data(), size, &info), SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA,
                     "truncated to " + std::to_string(size));
    }
    auto shortSampler = original;
    shortSampler.erase(shortSampler.begin() + 100 + 35, shortSampler.begin() + 160);
    Set32(shortSampler, 96, 35);
    Set32(shortSampler, 4, static_cast<std::uint32_t>(shortSampler.size() - 8));
    RequireEqual(sceNgs2ParseWaveformData(shortSampler.data(), shortSampler.size(), &info), SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA,
                 "35-byte sampler chunk");
}};

class GuardedSuperframe {
public:
    GuardedSuperframe() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        page = info.dwPageSize;
        region = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, 2 * page, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(region != nullptr, "allocate two pages");
        DWORD previous = 0;
        Require(VirtualProtect(region + page, page, PAGE_NOACCESS, &previous) != 0, "protect the guard page");
#else
        page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        void* mapping = mmap(nullptr, 2 * page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapping != MAP_FAILED, "map two pages");
        region = static_cast<std::uint8_t*>(mapping);
        Require(mprotect(region + page, page, PROT_NONE) == 0, "protect the guard page");
#endif
        std::mt19937 random(50);
        for (std::uint32_t i = 0; i < SuperframeBytes; i++) Data()[i] = static_cast<std::uint8_t>(random());
    }

    ~GuardedSuperframe() {
        if (region == nullptr) return;
#ifdef _WIN32
        VirtualFree(region, 0, MEM_RELEASE);
#else
        munmap(region, 2 * page);
#endif
    }

    GuardedSuperframe(const GuardedSuperframe&) = delete;
    GuardedSuperframe& operator=(const GuardedSuperframe&) = delete;

    std::uint8_t* Data() const {
        return region + page - SuperframeBytes;
    }

private:
    std::size_t page = 0;
    std::uint8_t* region = nullptr;
};

const Case corruptSuperframe{"Atrac9Sampler_CorruptSuperframeAtPageEnd_ThrowsDecodeFailure", [] {
    const GuardedSuperframe superframe;
    Ngs2Fixture ngs2;
    const auto system = ngs2.CreateSystem();
    const auto voice = Voice(ngs2.CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP,
            Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_ATRAC9, 1, 48000, Config, 0, 0}});
    const Ngs2WaveformBlock block{0, SuperframeBytes, 0, 0, SuperframeSamples, 0, 0};
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS,
            Ngs2SamplerVoiceWaveformBlocksParam{{}, superframe.Data(), 0, 1, &block});
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    Patch(voice, ngs2.Mastering(system, 1));
    const auto error = RequireThrows<std::runtime_error>([&] { Render(system, Grain); }, "corrupt superframe");
    Require(std::strstr(error.what(), "ATRAC9 decode failed") != nullptr,
            std::string("error mentions the decode failure: ") + error.what());
}};

uintptr_t EmptySampler(Ngs2Fixture& ngs2, uintptr_t system) {
    const auto voice = Voice(ngs2.CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP,
            Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_ATRAC9, 1, 48000, Config, 0, 0}});
    Patch(voice, ngs2.Mastering(system, 1));
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    return voice;
}

void Queue(uintptr_t voice, const void* data, std::uint32_t bytes, std::uint32_t flags, std::uint32_t samples = 0,
           std::uint32_t skip = 0) {
    const Ngs2WaveformBlock block{0, bytes, 0, skip, samples, 0, bytes};
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, data, flags, 1, &block});
}

const Case streamPartitions{"Atrac9Stream_SuperframesSplitAcrossBlocks_DecodeLikeContiguousData", [] {
    const auto reference = Reference(2);
    std::vector<std::uint8_t> data(Superframe, Superframe + SuperframeBytes);
    data.insert(data.end(), Superframe, Superframe + SuperframeBytes);
    for (const auto skip : {100u, 1100u}) {
        for (const float pitch : {0.5f, 1.0f, 1.5f}) {
            const auto name = "skip " + std::to_string(skip) + " pitch " + std::to_string(pitch);
            Ngs2Fixture ngs2;
            CallbackRecorder recorder;
            const auto system = ngs2.CreateSystem();
            const auto voice = EmptySampler(ngs2, system);
            RecordCallbacks(voice, recorder, SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END);
            Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_PITCH, Ngs2SamplerVoicePitchParam{{}, pitch});
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
                const float position = i * pitch;
                float expected = 0.0f;
                if (position < 800) {
                    const auto index = static_cast<std::uint32_t>(position);
                    const auto next = std::min(index + 1, 799u);
                    const auto fraction = position - index;
                    expected = reference[skip + index] * (1 - fraction) + reference[skip + next] * fraction;
                }
                Require(std::abs(output[i] - expected) < 1e-6f, name + " sample " + std::to_string(i) + ": expected " +
                                                                std::to_string(expected) + ", got " + std::to_string(output[i]));
            }
            RequireEqual(recorder.flags.size(), std::size_t{5}, name + " block end callbacks");
            RequireEqual(Flags(voice), 0u, name + " stopped");
            RequireEqual(SamplerState(voice).num_decoded_samples, 800u, name + " decoded samples");
        }
    }
}};

struct RefillState {
    bool pause = false;
    bool reset = false;
    int calls = 0;
    int unexpectedFlags = 0;
    int userDataMismatches = 0;
    std::uint32_t firstBlockSize = 0;
    const void* firstBlockData = nullptr;
};

void APS5_VABI Refill(const Ngs2VoiceCallbackInfo* info) {
    auto& state = *reinterpret_cast<RefillState*>(info->callback_data);
    ++state.calls;
    if (info->flag != SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END) ++state.unexpectedFlags;
    if (info->user_data != info->block_size) ++state.userDataMismatches;
    if (state.calls != 1) return;
    state.firstBlockSize = info->block_size;
    state.firstBlockData = info->block_data;
    if (state.reset) Queue(info->voice_handle, Superframe, SuperframeBytes, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_RESET, 500, 100);
    else Queue(info->voice_handle, Superframe + 13, SuperframeBytes - 13, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND, 0xffffffffu, 0xffffffffu);
    if (state.pause) Event(info->voice_handle, SCE_NGS2_VOICE_EVENT_PAUSE);
}

void RequireRefill(bool pause, bool reset) {
    const auto reference = Reference();
    Ngs2Fixture ngs2;
    const auto system = ngs2.CreateSystem();
    const auto voice = EmptySampler(ngs2, system);
    RefillState state{pause, reset};
    Control(voice, SCE_NGS2_VOICE_PARAM_CALLBACK,
            Ngs2VoiceCallbackParam{{}, Refill, reinterpret_cast<std::uintptr_t>(&state), SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END, 0});
    Queue(voice, Superframe, 13, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE, 500, 100);
    if (pause || reset) {
        RequireSilence(Render(system, Grain), "grain with the partial superframe");
        RequireEqual(state.calls, 1, "refill callback ran once");
        if (pause) Event(voice, SCE_NGS2_VOICE_EVENT_RESUME);
    }
    RequireReferenceThenSilence(Render(system, 512), reference, 100, 500, "refilled output");
    RequireEqual(state.calls, 2, "callback count");
    RequireEqual(state.firstBlockSize, 13u, "first block size");
    RequireEqual(state.firstBlockData, static_cast<const void*>(Superframe), "first block data");
    RequireEqual(state.unexpectedFlags, 0, "callback flags other than block end");
    RequireEqual(state.userDataMismatches, 0, "callback user data differs from the block size");
    RequireEqual(Flags(voice), 0u, "stopped");
}

const Case refillAppend{"Atrac9Stream_BlockEndCallbackAppends_ContinuesSeamlessly", [] {
    RequireRefill(false, false);
}};

const Case refillPause{"Atrac9Stream_BlockEndCallbackAppendsAndPauses_ResumesSeamlessly", [] {
    RequireRefill(true, false);
}};

const Case refillReset{"Atrac9Stream_BlockEndCallbackResets_RestartsFromNewBlock", [] {
    RequireRefill(false, true);
}};

const Case starvation{"Atrac9Stream_StarvedThenAppended_ResumesDecoding", [] {
    const auto reference = Reference();
    Ngs2Fixture ngs2;
    const auto system = ngs2.CreateSystem();
    const auto voice = EmptySampler(ngs2, system);
    Queue(voice, Superframe, 13, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE, 500, 100);
    RequireSilence(Render(system, Grain * 3), "starved");
    Require((Flags(voice) & SCE_NGS2_VOICE_STATE_FLAG_PLAYING) != 0, "playing while starved");
    Queue(voice, Superframe + 13, SuperframeBytes - 13, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND);
    RequireReferenceThenSilence(Render(system, 512), reference, 100, 500, "after append");
    RequireEqual(Flags(voice), 0u, "stopped");
}};

const Case truncation{"Atrac9Stream_StarvedThenCleared_RenderRejectsPartialSuperframe", [] {
    Ngs2Fixture ngs2;
    const auto system = ngs2.CreateSystem();
    const auto voice = EmptySampler(ngs2, system);
    Queue(voice, Superframe, 13, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE, 500, 100);
    RequireSilence(Render(system, Grain * 3), "starved");
    Require((Flags(voice) & SCE_NGS2_VOICE_STATE_FLAG_PLAYING) != 0, "playing while starved");
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, nullptr, 0, 0, nullptr});
    RequireThrows<std::invalid_argument>([&] { Render(system, Grain); }, "render a truncated stream");
}};

const Case appendBoundaries{"Atrac9Stream_AppendAtEverySplit_DecodesWholeSuperframe", [] {
    const auto reference = Reference();
    for (std::uint32_t split = 1; split < SuperframeBytes; ++split) {
        const auto name = "split " + std::to_string(split);
        Ngs2Fixture ngs2;
        const auto system = ngs2.CreateSystem();
        const auto voice = EmptySampler(ngs2, system);
        Queue(voice, Superframe, split, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE, 500, 100);
        RequireSilence(Render(system, Grain), name + " before append");
        const Ngs2WaveformBlock block{split, SuperframeBytes - split, UINT32_MAX, UINT32_MAX, UINT32_MAX, 0, 0};
        Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS,
                Ngs2SamplerVoiceWaveformBlocksParam{{}, Superframe, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND, 1, &block});
        RequireReferenceThenSilence(Render(system, 512), reference, 100, 500, name);
        const auto state = SamplerState(voice);
        RequireEqual(state.num_decoded_samples, 500u, name + " decoded samples");
        RequireEqual(state.decoded_data_size, SuperframeBytes, name + " decoded bytes");
        RequireEqual(static_cast<const void*>(state.waveform_data), static_cast<const void*>(Superframe + SuperframeBytes),
                     name + " waveform position");
        RequireEqual(Flags(voice), 0u, name + " stopped");
    }
}};

const Case appendWithoutWaveform{"Atrac9Stream_AppendWithoutWaveform_IsRejected", [] {
    Ngs2Fixture ngs2;
    const auto system = ngs2.CreateSystem();
    const auto voice = EmptySampler(ngs2, system);
    RequireThrows<std::invalid_argument>([&] { Queue(voice, Superframe, SuperframeBytes, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND, 500); },
                                         "append to an empty voice");
    Queue(voice, Superframe, SuperframeBytes, 0, 500);
    Render(system, 512);
    RequireEqual(Flags(voice), 0u, "stopped after a plain block");
}};

} // namespace
