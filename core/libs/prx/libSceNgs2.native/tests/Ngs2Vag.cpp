#include "Ngs2Test.hpp"

#include "prx/libc/include/General.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

static constexpr std::uint32_t FrameSamples = 28;
static constexpr std::uint32_t FrameBytes = 16;
static constexpr std::uint32_t HeaderSize = 0x30;

static void PutBe32(std::vector<std::uint8_t>& data, std::size_t offset, std::uint32_t value) {
    for (std::size_t i = 0; i < 4; i++) data[offset + i] = static_cast<std::uint8_t>(value >> (24 - 8 * i));
}

static std::vector<std::uint8_t> Vag(std::uint32_t channelByte, std::uint32_t frames, std::uint32_t rate = 48000) {
    const std::uint32_t channels = channelByte == 0 ? 1 : channelByte;
    std::vector<std::uint8_t> data(HeaderSize + frames * channels * FrameBytes);
    std::memcpy(data.data(), "VAGp", 4);
    PutBe32(data, 4, 0x20001);
    PutBe32(data, 0x0c, frames * channels * FrameBytes);
    PutBe32(data, 0x10, rate);
    data[0x1e] = static_cast<std::uint8_t>(channelByte);
    return data;
}

static std::uint8_t* Unit(std::vector<std::uint8_t>& data, std::uint32_t channels, std::uint32_t frame, std::uint32_t channel) {
    return data.data() + HeaderSize + (frame * channels + channel) * FrameBytes;
}

static void SetUnit(std::vector<std::uint8_t>& data, std::uint32_t channels, std::uint32_t frame, std::uint32_t channel, std::uint8_t predictorShift, std::uint8_t flags, std::uint8_t nibbles) {
    auto* unit = Unit(data, channels, frame, channel);
    unit[0] = predictorShift;
    unit[1] = flags;
    std::memset(unit + 2, nibbles, FrameBytes - 2);
}

template <typename TFunction>
static void RequireThrows(TFunction function) {
    bool threw = false;
    try { function(); } catch (const std::exception&) { threw = true; }
    Require(threw);
}

static bool SameBlock(const Ngs2WaveformBlock& block, std::uint64_t offset, std::uint64_t size, std::uint32_t repeats, std::uint32_t samples) {
    return block.data_offset == offset && block.data_size == size && block.num_repeats == repeats && block.num_skip_samples == 0 && block.num_samples == samples;
}

static void TestParseHeader() {
    auto data = Vag(0, 3, 44100);
    Ngs2WaveformInfo info{};
    Require(sceNgs2ParseWaveformData(data.data(), data.size(), &info) == SCE_NGS2_OK);
    Require(info.format.waveform_type == SCE_NGS2_WAVEFORM_TYPE_VAG && info.format.num_channels == 1 && info.format.sample_rate == 44100 && info.format.config_data == 0);
    Require(info.data_offset == HeaderSize && info.data_size == 48 && info.num_samples == 56);
    Require(info.audio_unit_size == 16 && info.num_audio_unit_samples == 28 && info.num_audio_unit_per_frame == 1);
    Require(info.audio_frame_size == 16 && info.num_audio_frame_samples == 28 && info.num_delay_samples == 0);
    Require(info.loop_begin_position == 0 && info.loop_end_position == 0 && info.num_blocks == 1);
    Require(SameBlock(info.block[0], HeaderSize, 32, 0, 56));

    Unit(data, 1, 1, 0)[1] = 2;
    Require(sceNgs2ParseWaveformData(data.data(), data.size(), &info) == SCE_NGS2_OK);
    Require(info.num_samples == 84 && info.num_blocks == 1 && SameBlock(info.block[0], HeaderSize, 48, 0, 84));

    PutBe32(data, 4, 0x20);
    Require(sceNgs2ParseWaveformData(data.data(), data.size(), &info) == SCE_NGS2_OK && info.num_samples == 84);

    auto stereo = Vag(2, 3);
    Require(sceNgs2ParseWaveformData(stereo.data(), stereo.size(), &info) == SCE_NGS2_OK);
    Require(info.format.num_channels == 2 && info.audio_unit_size == 32 && info.audio_frame_size == 32 && info.num_samples == 56);
    Require(SameBlock(info.block[0], HeaderSize, 64, 0, 56));

    for (std::size_t size = 4; size < HeaderSize + 2 * FrameBytes; size++) {
        Require(sceNgs2ParseWaveformData(data.data(), size, &info) == SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA);
    }
    Require(sceNgs2ParseWaveformData(data.data(), HeaderSize + 2 * FrameBytes, &info) == SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA);

    PutBe32(data, 0x0c, 64);
    Require(sceNgs2ParseWaveformData(data.data(), data.size(), &info) == SCE_NGS2_ERROR_INVALID_WAVEFORM_DATA);
    Unit(data, 1, 1, 0)[1] = 0;
    Require(sceNgs2ParseWaveformData(data.data(), data.size(), &info) == SCE_NGS2_OK && info.data_size == 64 && info.num_samples == 84);
    PutBe32(data, 0x0c, 0);
    RequireThrows([&] { sceNgs2ParseWaveformData(data.data(), data.size(), &info); });
}

static void TestParseLoop() {
    auto data = Vag(1, 4);
    Unit(data, 1, 1, 0)[1] = 6;
    Unit(data, 1, 2, 0)[1] = 3;
    Ngs2WaveformInfo info{};
    Require(sceNgs2ParseWaveformData(data.data(), data.size(), &info) == SCE_NGS2_OK);
    Require(info.num_samples == 112 && info.loop_begin_position == 28 && info.loop_end_position == 84 && info.num_blocks == 3);
    Require(SameBlock(info.block[0], HeaderSize, 16, 0, 28));
    Require(SameBlock(info.block[1], HeaderSize + 16, 32, UINT32_MAX, 56));
    Require(SameBlock(info.block[2], HeaderSize + 48, 16, 0, 28));

    Unit(data, 1, 0, 0)[1] = 6;
    Unit(data, 1, 1, 0)[1] = 2;
    Unit(data, 1, 2, 0)[1] = 0;
    Unit(data, 1, 3, 0)[1] = 3;
    Require(sceNgs2ParseWaveformData(data.data(), data.size(), &info) == SCE_NGS2_OK);
    Require(info.loop_begin_position == 0 && info.loop_end_position == 112 && info.num_blocks == 1);
    Require(SameBlock(info.block[0], HeaderSize, 64, UINT32_MAX, 112));

    Unit(data, 1, 0, 0)[1] = 3;
    Unit(data, 1, 3, 0)[1] = 6;
    Require(sceNgs2ParseWaveformData(data.data(), data.size(), &info) == SCE_NGS2_OK);
    Require(info.loop_begin_position == 84 && info.loop_end_position == 28 && info.num_blocks == 1);
    Require(SameBlock(info.block[0], HeaderSize, 64, 0, 112));

    auto repeated = Vag(1, 10);
    for (std::uint32_t frame = 0; frame < 8; frame++) Unit(repeated, 1, frame, 0)[1] = frame % 2 == 0 ? 6 : 3;
    Require(sceNgs2ParseWaveformData(repeated.data(), repeated.size(), &info) == SCE_NGS2_OK);
    Require(info.num_samples == 280 && info.loop_begin_position == 168 && info.loop_end_position == 224 && info.num_blocks == 3);
    Require(SameBlock(info.block[0], HeaderSize, 96, 0, 168));
    Require(SameBlock(info.block[1], HeaderSize + 96, 32, UINT32_MAX, 56));
    Require(SameBlock(info.block[2], HeaderSize + 128, 32, 0, 56));
    Require(SameBlock(info.block[3], 0, 0, 0, 0));
}

static void TestParseFile() {
    auto data = Vag(2, 3);
    Unit(data, 2, 1, 0)[1] = 2;
    const char* const guestPath = "/aps5_ngs2_parse_file.vag";
    const auto path = ResolvePath_nid_no_patch(guestPath);
    {
        std::ofstream out(path, std::ios::binary);
        const char pad[3]{};
        out.write(pad, sizeof(pad));
        out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    }
    Ngs2WaveformInfo info{};
    Require(sceNgs2ParseWaveformFile(guestPath, 3, &info) == SCE_NGS2_OK);
    Require(info.format.waveform_type == SCE_NGS2_WAVEFORM_TYPE_VAG && info.format.num_channels == 2 && info.num_samples == 84);
    Require(info.data_offset == 3 + HeaderSize && SameBlock(info.block[0], 3 + HeaderSize, 96, 0, 84));
    std::filesystem::remove(path);
}

class Playback {
public:
    explicit Playback(std::uint32_t channels) : firstBuffer(usedBuffers), system(CreateSystem()), channels(channels), voice(Voice(CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER))) {
        Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_VAG, channels, 48000, 0, 0, 0}});
        Patch(voice, Mastering(system, channels));
        Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    }

    ~Playback() {
        Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
        usedBuffers = firstBuffer;
    }

    void Add(const void* data, const Ngs2WaveformBlock* blocks, std::uint32_t count, std::uint32_t flags = 0) {
        Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, data, flags, count, blocks});
    }

    std::vector<std::int32_t> Render() {
        std::vector<float> output(Grain * channels);
        const Ngs2RenderBufferInfo info{output.data(), output.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, channels};
        Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_OK);
        std::vector<std::int32_t> samples;
        for (const float value : output) {
            Require(value * 32768.0f == static_cast<float>(static_cast<std::int32_t>(value * 32768.0f)));
            samples.push_back(static_cast<std::int32_t>(value * 32768.0f));
        }
        return samples;
    }

    Ngs2SamplerVoiceState State() const {
        Ngs2SamplerVoiceState state{};
        Require(sceNgs2VoiceGetState(voice, &state.voice_state, sizeof(state)) == SCE_NGS2_OK);
        return state;
    }

    std::size_t firstBuffer;
    uintptr_t system;
    std::uint32_t channels;
    uintptr_t voice;
};

static void TestPlayback() {
    auto data = Vag(2, 2);
    for (std::uint32_t frame = 0; frame < 2; frame++) {
        SetUnit(data, 2, frame, 0, 0x0c, 0, 0x21);
        SetUnit(data, 2, frame, 1, 0x0c, 0, 0xfe);
    }
    Unit(data, 2, 1, 0)[1] = 2;
    Ngs2WaveformInfo info{};
    Require(sceNgs2ParseWaveformData(data.data(), data.size(), &info) == SCE_NGS2_OK && info.num_samples == 56);
    Playback playback(2);
    auto block = info.block[0];
    block.num_skip_samples = 3;
    block.num_samples = 53;
    block.num_repeats = 2;
    playback.Add(data.data(), &block, 1);
    Require(playback.State().waveform_data == nullptr);
    for (std::uint32_t grain = 0; grain < 15; grain++) {
        const auto output = playback.Render();
        for (std::uint32_t i = 0; i < Grain; i++) {
            const std::uint32_t sample = grain * Grain + i;
            const bool playing = sample >= 3 && sample - 3 < 112;
            const std::uint32_t position = (sample - 3) % 56;
            Require(output[i * 2] == (playing ? (position % 2 == 0 ? 1 : 2) : 0));
            Require(output[i * 2 + 1] == (playing ? (position % 2 == 0 ? -2 : -1) : 0));
        }
        if (grain == 3) Require(playback.State().waveform_data == data.data() + HeaderSize + 32);
        if (grain == 13) {
            const auto state = playback.State();
            Require(state.num_decoded_samples == 106 && state.decoded_data_size == 128);
        }
    }
    const auto state = playback.State();
    Require(state.num_decoded_samples == 0 && state.waveform_data == nullptr);
}

static void TestHistory() {
    auto data = Vag(1, 2);
    SetUnit(data, 1, 0, 0, 0x0c, 0, 0x77);
    SetUnit(data, 1, 1, 0, 0x1c, 0, 0x00);
    const std::array<Ngs2WaveformBlock, 2> blocks{{{HeaderSize, 16, 0, 0, 28, 0, 0}, {HeaderSize + 16, 16, 0, 0, 28, 0, 0}}};
    constexpr std::array<std::int32_t, 8> decay{6, 5, 4, 3, 2, 1, 0, 0};
    {
        Playback playback(1);
        playback.Add(data.data(), blocks.data(), 2);
        for (std::uint32_t grain = 0; grain < 5; grain++) {
            const auto output = playback.Render();
            for (std::uint32_t i = 0; i < Grain; i++) {
                const std::uint32_t sample = grain * Grain + i;
                const std::uint32_t position = sample - 3;
                Require(output[i] == (sample < 3 ? 0 : position < FrameSamples ? 7 : position - FrameSamples < decay.size() ? decay[position - FrameSamples] : 0));
            }
        }
    }
    {
        Playback playback(1);
        playback.Add(data.data(), &blocks[0], 1, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE);
        playback.Add(data.data(), &blocks[1], 1, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_RESET);
        for (std::uint32_t grain = 0; grain < 4; grain++) {
            for (const auto sample : playback.Render()) Require(sample == 0);
        }
    }
}

static void TestEndFlag() {
    auto data = Vag(1, 1);
    SetUnit(data, 1, 0, 0, 0x0c, 7, 0x77);
    const Ngs2WaveformBlock block{HeaderSize, 16, 0, 0, 28, 0, 0};
    Playback playback(1);
    playback.Add(data.data(), &block, 1);
    for (const auto sample : playback.Render()) Require(sample == 0);
}

static void TestErrors() {
    auto data = Vag(1, 1);
    const Ngs2WaveformBlock block{HeaderSize, 16, 0, 0, 28, 0, 0};
    Playback playback(1);
    SetUnit(data, 1, 0, 0, 0x0c, 0x80, 0x11);
    RequireThrows([&] { playback.Add(data.data(), &block, 1, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_RESET); });
    SetUnit(data, 1, 0, 0, 0x0d, 0, 0x11);
    RequireThrows([&] { playback.Add(data.data(), &block, 1, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_RESET); });
    Require(playback.State().waveform_data == nullptr);
    SetUnit(data, 1, 0, 0, 0x0c, 0, 0x11);
    const Ngs2WaveformBlock longer{HeaderSize, 16, 0, 0, 29, 0, 0};
    RequireThrows([&] { playback.Add(data.data(), &longer, 1, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_RESET); });
    playback.Add(data.data(), &block, 1, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_RESET);
    const auto output = playback.Render();
    for (std::uint32_t i = 0; i < Grain; i++) Require(output[i] == (i < 3 ? 0 : 1));
    RequireThrows([&] { Control(playback.voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_VAG, 1, 48000, 1, 0, 0}}); });
}

int main() {
    TestParseHeader();
    TestParseLoop();
    TestParseFile();
    TestPlayback();
    TestHistory();
    TestEndFlag();
    TestErrors();
}
