#include "Ngs2Test.hpp"

#include "libatrac9.h"

#include <cstdint>
#include <cstring>
#include <random>
#include <stdexcept>
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

static std::vector<float> Reference(std::uint32_t superframes = 1) {
    std::uint8_t config[4] = {0xFE, 0x70, 0x07, 0xF0};
    void* decoder = Atrac9GetHandle();
    Require(Atrac9InitDecoder(decoder, config) == 0);
    std::vector<float> pcm(SuperframeSamples * superframes);
    for (std::uint32_t s = 0; s < superframes; ++s) {
        int offset = 0;
        for (std::uint32_t frame = 0; frame < 4; frame++) {
            int used = 0;
            Require(Atrac9DecodeF32(decoder, Superframe + offset, static_cast<int>(SuperframeBytes) - offset, pcm.data() + s * SuperframeSamples + frame * 256, &used, 0) == 0);
            offset += used;
        }
    }
    Atrac9ReleaseHandle(decoder);
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

static std::vector<float> Render(uintptr_t system, std::uint32_t samples) {
    std::vector<float> rendered;
    std::vector<float> out(Grain);
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
    for (std::uint32_t i = 0; i < 512; i++) Require(rendered[i] == (i < 500 ? reference[100 + i] : 0.0f));
    Require(callbackFlags.size() == 1 && callbackFlags[0] == SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestRepeatAndState(const std::vector<float>& reference) {
    const auto system = CreateSystem();
    const auto sampler = Sampler(system, 0, 300, 1);
    Patch(sampler, Mastering(system, 1));
    callbackFlags.clear();
    auto rendered = Render(system, 304);
    Ngs2SamplerVoiceState state{};
    Require(sceNgs2VoiceGetState(sampler, &state.voice_state, sizeof(state)) == SCE_NGS2_OK);
    Require(state.num_decoded_samples == 304 && state.decoded_data_size == 2 * SuperframeBytes && state.waveform_data == Superframe + SuperframeBytes);
    const auto second = Render(system, 304);
    rendered.insert(rendered.end(), second.begin(), second.end());
    for (std::uint32_t i = 0; i < 608; i++) Require(rendered[i] == (i < 600 ? reference[i % 300] : 0.0f));
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

    Require(sceNgs2ParseWaveformData(file.data(), 11, &info) == SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA);
    Require(sceNgs2ParseWaveformData(Superframe, SuperframeBytes, &info) == SCE_NGS2_ERROR_UNKNOWN_WAVEFORM_FORMAT);
    const auto wrongRate = At9File(44100);
    Require(sceNgs2ParseWaveformData(wrongRate.data(), wrongRate.size(), &info) == SCE_NGS2_ERROR_INVALID_WAVEFORM_FORMAT);
}

static void TestCalcBlock() {
    const Ngs2WaveformFormat format{SCE_NGS2_WAVEFORM_TYPE_ATRAC9, 1, 48000, Config, 0, 0};
    Ngs2WaveformBlock block{};
    Require(sceNgs2CalcWaveformBlock(&format, 1500, 600, nullptr) == SCE_NGS2_ERROR_INVALID_OUT_ADDRESS);
    Require(sceNgs2CalcWaveformBlock(&format, 1500, 600, &block) == SCE_NGS2_OK);
    Require(block.data_offset == SuperframeBytes && block.data_size == 2 * SuperframeBytes && block.num_skip_samples == 476 && block.num_samples == 600);
    Require(sceNgs2CalcWaveformBlock(&format, 1500, 0, &block) == SCE_NGS2_OK);
    Require(block.data_offset == SuperframeBytes && block.data_size == 0 && block.num_skip_samples == 0 && block.num_samples == 0);
    const Ngs2WaveformFormat stereo{SCE_NGS2_WAVEFORM_TYPE_ATRAC9, 2, 48000, Config, 0, 0};
    Require(sceNgs2CalcWaveformBlock(&stereo, 0, 1, &block) == SCE_NGS2_ERROR_INVALID_WAVEFORM_FORMAT);
    const Ngs2WaveformFormat silent{SCE_NGS2_WAVEFORM_TYPE_ATRAC9, 0, 48000, Config, 0, 0};
    Require(sceNgs2CalcWaveformBlock(&silent, 0, 1, &block) == SCE_NGS2_ERROR_INVALID_WAVEFORM_FORMAT);
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
            Require(info.block[begin != 0].num_repeats == 1);
            const auto system = CreateSystem();
            const auto voice = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
            Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, info.format});
            Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS,
                Ngs2SamplerVoiceWaveformBlocksParam{{}, file.data(), 0, info.num_blocks, info.block});
            Patch(voice, Mastering(system, 1));
            Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
            const auto rendered = Render(system, samples + end - begin + Grain);
            for (std::uint32_t i = 0; i < rendered.size(); ++i) {
                const auto index = i < end ? i : i - (end - begin);
                Require(rendered[i] == (i < samples + end - begin ? reference[256 + index] : 0.0f));
            }
            Require(Flags(voice) == 0);
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
        const auto index = i < 700 ? i : i - 600;
        Require(rendered[i] == (i < samples + 600 ? reference[256 + index] : 0.0f));
    }
    Require(Flags(voice) == 0);
    Require(sceNgs2SystemDestroy(system, nullptr) == 0);
}

static void TestMalformedLoops() {
    const auto original = LoopedFile(100, 700, 0);
    Ngs2WaveformInfo info{};
    for (const auto [offset, value] : {std::pair<std::size_t, std::uint32_t>{128, 2}, {144, 0}, {148, 255}, {148, 5000}}) {
        auto file = original;
        Set32(file, offset, value);
        Require(sceNgs2ParseWaveformData(file.data(), file.size(), &info) == SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA);
    }
    for (const auto offset : {140u, 152u}) {
        auto file = original;
        Set32(file, offset, 1);
        bool rejected = false;
        try { sceNgs2ParseWaveformData(file.data(), file.size(), &info); } catch (const std::runtime_error&) { rejected = true; }
        Require(rejected);
    }
    for (std::size_t size = 92; size < 168; ++size)
        Require(sceNgs2ParseWaveformData(original.data(), size, &info) == SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA);
    auto shortSampler = original;
    shortSampler.erase(shortSampler.begin() + 100 + 35, shortSampler.begin() + 160);
    Set32(shortSampler, 96, 35);
    Set32(shortSampler, 4, static_cast<std::uint32_t>(shortSampler.size() - 8));
    Require(sceNgs2ParseWaveformData(shortSampler.data(), shortSampler.size(), &info) == SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA);
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
        Render(system, Grain);
    } catch (const std::runtime_error& error) {
        rejected = std::strstr(error.what(), "ATRAC9 decode failed") != nullptr;
    }
    Require(rejected);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

int main() {
    const auto reference = Reference();
    TestSkipAndBlockEnd(reference);
    TestRepeatAndState(reference);
    TestParse();
    TestParseLoops();
    TestMalformedLoops();
    TestCalcBlock();
    TestCorruptSuperframeAtPageEnd();
    return 0;
}
