#include "Ngs2Test.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

struct PcmFixture {
    std::uint32_t type;
    std::uint32_t bytes;
    std::vector<std::uint8_t> data;
    std::array<float, 8> samples;
};

static const PcmFixture Fixtures[] = {
    {SCE_NGS2_WAVEFORM_TYPE_PCM_U8, 1,
     {0x00, 0x40, 0x60, 0x80, 0xa0, 0xc0, 0xe0, 0xff},
     {-1, -0.5f, -0.25f, 0, 0.25f, 0.5f, 0.75f, 127.0f / 128}},
    {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 2,
     {0x00, 0x80, 0x00, 0xc0, 0x00, 0xe0, 0x00, 0x00, 0x00, 0x20, 0x00, 0x40, 0x00, 0x60, 0xff, 0x7f},
     {-1, -0.5f, -0.25f, 0, 0.25f, 0.5f, 0.75f, 32767.0f / 32768}},
    {SCE_NGS2_WAVEFORM_TYPE_PCM_I24L, 3,
     {0x00, 0x00, 0x80, 0x00, 0x00, 0xc0, 0x00, 0x00, 0xe0, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x20, 0x00, 0x00, 0x40, 0x00, 0x00, 0x60, 0xff, 0xff, 0x7f},
     {-1, -0.5f, -0.25f, 0, 0.25f, 0.5f, 0.75f, 8388607.0f / 8388608}},
    {SCE_NGS2_WAVEFORM_TYPE_PCM_I32L, 4,
     {0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0xe0, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x20, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x60, 0xff, 0xff, 0xff, 0x7f},
     {-1, -0.5f, -0.25f, 0, 0.25f, 0.5f, 0.75f, 1}},
    {SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 4,
     {0x00, 0x00, 0x80, 0xbf, 0x00, 0x00, 0x00, 0xbf, 0x00, 0x00, 0x80, 0xbe, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x80, 0x3e, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0x40, 0x3f, 0x00, 0x00, 0x80, 0x3f},
     {-1, -0.5f, -0.25f, 0, 0.25f, 0.5f, 0.75f, 1}},
    {SCE_NGS2_WAVEFORM_TYPE_PCM_F64L, 8,
     {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xbf, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0xbf,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd0, 0xbf, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xd0, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe0, 0x3f,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xe8, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x3f},
     {-1, -0.5f, -0.25f, 0, 0.25f, 0.5f, 0.75f, 1}},
};

class Playback {
public:
    Playback(std::uint32_t type, std::uint32_t channels = 1, std::uint32_t rate = 48000)
        : firstBuffer(usedBuffers), system(CreateSystem()), channels(channels), voice(Voice(CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER))) {
        Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {type, channels, rate, 0, 0, 0}});
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

    std::vector<float> Render() {
        std::vector<float> output(Grain * channels, 123);
        const Ngs2RenderBufferInfo info{output.data(), output.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, channels};
        Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_OK);
        return output;
    }

    Ngs2SamplerVoiceState State() {
        Ngs2SamplerVoiceState state{};
        Require(sceNgs2VoiceGetState(voice, &state.voice_state, sizeof(state)) == SCE_NGS2_OK);
        return state;
    }

    const std::size_t firstBuffer;
    const std::uintptr_t system;
    const std::uint32_t channels;
    const std::uintptr_t voice;
};

template <typename TFunction>
static void RequireThrows(TFunction function) {
    bool threw = false;
    try { function(); } catch (const std::exception&) { threw = true; }
    Require(threw);
}

static void TestFramesAndPositions(const PcmFixture& fixture, std::uint32_t channels) {
    Playback playback(fixture.type, channels);
    std::vector<std::uint8_t> data(3, 0xa5);
    constexpr std::uint32_t skip = 3;
    for (std::uint32_t frame = 0; frame < skip + Grain * 2; frame++) {
        for (std::uint32_t channel = 0; channel < channels; channel++) {
            const auto sample = (frame + channel) % fixture.samples.size();
            const auto first = fixture.data.begin() + sample * fixture.bytes;
            data.insert(data.end(), first, first + fixture.bytes);
        }
    }
    const auto frameBytes = channels * fixture.bytes;
    const Ngs2WaveformBlock block{2, data.size() - 3, 0, skip, Grain * 2, 0, 0};
    playback.Add(data.data() + 1, &block, 1);
    Require(playback.State().waveform_data == data.data() + 3 + skip * frameBytes);
    for (std::uint32_t pass = 0; pass < 2; pass++) {
        const auto output = playback.Render();
        for (std::uint32_t frame = 0; frame < Grain; frame++) {
            for (std::uint32_t channel = 0; channel < channels; channel++) {
                const auto actual = output[frame * channels + channel];
                const auto expected = fixture.samples[(skip + pass * Grain + frame + channel) % 8];
                if (actual != expected) {
                    std::fprintf(stderr, "PCM type %#x frame %u channel %u: got %a, expected %a\n", fixture.type, pass * Grain + frame, channel, actual, expected);
                }
                Require(actual == expected);
            }
        }
        const auto state = playback.State();
        Require(state.num_decoded_samples == (pass + 1) * Grain);
        Require(state.decoded_data_size == (pass + 1) * Grain * frameBytes);
        Require(state.waveform_data == data.data() + 3 + (skip + (pass + 1) * Grain) * frameBytes);
    }
    Require(Flags(playback.voice) == 0);
}

static std::vector<Ngs2VoiceCallbackInfo> callbacks;

static void APS5_VABI OnBlock(const Ngs2VoiceCallbackInfo* info) {
    callbacks.push_back(*info);
}

static void TestRepeatAndTransition(const PcmFixture& fixture) {
    Playback playback(fixture.type);
    callbacks.clear();
    Control(playback.voice, SCE_NGS2_VOICE_PARAM_CALLBACK,
            Ngs2VoiceCallbackParam{{}, OnBlock, 7, SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_REPEAT | SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END, 0});
    const Ngs2WaveformBlock blocks[] = {{0, fixture.bytes * 4, 1, 1, 3, 0, 101}, {fixture.bytes * 4, fixture.bytes * 4, 0, 0, 4, 0, 202}};
    playback.Add(fixture.data.data(), blocks, 2);
    const auto first = playback.Render();
    constexpr std::uint32_t indices[] = {1, 2, 3, 1, 2, 3, 4, 5};
    for (std::uint32_t i = 0; i < Grain; i++) Require(first[i] == fixture.samples[indices[i]]);
    auto state = playback.State();
    Require(state.num_decoded_samples == 8 && state.decoded_data_size == 8 * fixture.bytes);
    Require(state.waveform_data == fixture.data.data() + 6 * fixture.bytes);
    const auto last = playback.Render();
    Require(last[0] == fixture.samples[6] && last[1] == fixture.samples[7]);
    for (std::uint32_t i = 2; i < Grain; i++) Require(last[i] == 0);
    state = playback.State();
    Require(state.num_decoded_samples == 10 && state.decoded_data_size == 10 * fixture.bytes);
    Require(state.waveform_data == fixture.data.data() + fixture.data.size());
    Require(callbacks.size() == 3);
    Require(callbacks[0].flag == SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_REPEAT && callbacks[0].user_data == 101 && callbacks[0].num_repeated == 1);
    Require(callbacks[1].flag == SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END && callbacks[1].user_data == 101);
    Require(callbacks[2].flag == SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END && callbacks[2].user_data == 202);
    Require(callbacks[0].block_data == fixture.data.data() && callbacks[2].block_data == fixture.data.data() + fixture.bytes * 4);
}

static void TestResampling(const PcmFixture& fixture) {
    Playback playback(fixture.type, 1, 24000);
    const Ngs2WaveformBlock blocks[] = {{0, fixture.bytes * 3, 0, 0, 3, 0, 0}, {fixture.bytes * 3, fixture.bytes * 5, 0, 0, 5, 0, 0}};
    playback.Add(fixture.data.data(), blocks, 2);
    for (std::uint32_t pass = 0; pass < 2; pass++) {
        const auto output = playback.Render();
        for (std::uint32_t i = 0; i < Grain; i++) {
            const auto sample = (pass * Grain + i) / 2;
            const auto next = sample < 7 ? sample + 1 : sample;
            const auto expected = i % 2 == 0 ? fixture.samples[sample] : std::lerp(fixture.samples[sample], fixture.samples[next], 0.5f);
            Require(output[i] == expected);
        }
        Require(playback.State().decoded_data_size == (pass + 1) * 4 * fixture.bytes);
    }
    Require(playback.State().num_decoded_samples == 8);
}

static void TestPitch(const PcmFixture& fixture) {
    Playback playback(fixture.type);
    Control(playback.voice, SCE_NGS2_SAMPLER_VOICE_PARAM_PITCH, Ngs2SamplerVoicePitchParam{{}, 2});
    const Ngs2WaveformBlock block{0, fixture.data.size(), 1, 0, 8, 0, 0};
    playback.Add(fixture.data.data(), &block, 1);
    const auto output = playback.Render();
    for (std::uint32_t i = 0; i < Grain; i++) Require(output[i] == fixture.samples[(i * 2) % 8]);
    Require(playback.State().num_decoded_samples == 16 && playback.State().decoded_data_size == 16 * fixture.bytes);
    Require(Flags(playback.voice) == 0);
}

static void TestBounds(const PcmFixture& fixture) {
    Playback playback(fixture.type, 2);
    const Ngs2WaveformBlock shortBlock{0, fixture.data.size() - 1, 1, 0, 4, 0, 0};
    RequireThrows([&] { playback.Add(fixture.data.data(), &shortBlock, 1); });
    const Ngs2WaveformBlock overflow{0, fixture.data.size(), 0, UINT32_MAX, UINT32_MAX, 0, 0};
    RequireThrows([&] { playback.Add(fixture.data.data(), &overflow, 1, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_RESET); });
}

static void TestNumericEdges() {
    const PcmFixture signed24{SCE_NGS2_WAVEFORM_TYPE_PCM_I24L, 3,
        {0x01, 0x00, 0x00, 0xff, 0xff, 0xff, 0x01, 0x00, 0x80, 0xfe, 0xff, 0x7f,
         0x56, 0x34, 0x12, 0xaa, 0xcb, 0xed, 0x80, 0x00, 0x00, 0x80, 0xff, 0xff},
        {0x1p-23f, -0x1p-23f, -8388607.0f / 8388608, 8388606.0f / 8388608,
         1193046.0f / 8388608, -1193046.0f / 8388608, 0x1p-16f, -0x1p-16f}};
    TestFramesAndPositions(signed24, 1);
    const PcmFixture signed32{SCE_NGS2_WAVEFORM_TYPE_PCM_I32L, 4,
        {0x01, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0x00, 0x80, 0xfe, 0xff, 0xff, 0x7f,
         0x01, 0x00, 0x00, 0x01, 0x02, 0x00, 0x00, 0x01, 0x03, 0x00, 0x00, 0x01, 0xfd, 0xff, 0xff, 0xfe},
        {0x1p-31f, -0x1p-31f, -1, 1, 0x1p-7f, 0x1.000002p-7f, 0x1.000004p-7f, -0x1.000004p-7f}};
    TestFramesAndPositions(signed32, 1);
    const PcmFixture floating64{SCE_NGS2_WAVEFORM_TYPE_PCM_F64L, 8,
        {0x9a, 0x99, 0x99, 0x99, 0x99, 0x99, 0xb9, 0x3f, 0x9a, 0x99, 0x99, 0x99, 0x99, 0x99, 0xb9, 0xbf,
         0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0xe0, 0x3f, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0xe0, 0x3f,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x38, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0xb8,
         0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80},
        {0.1f, -0.1f, 0.5f, 0x1.000004p-1f, 0x1p-126f, -0x1p-126f, 0, 0}};
    TestFramesAndPositions(floating64, 1);
}

int main() {
    for (const auto& fixture : Fixtures) {
        for (const auto channels : {1u, 2u, 6u, 8u}) TestFramesAndPositions(fixture, channels);
        TestRepeatAndTransition(fixture);
        TestResampling(fixture);
        TestPitch(fixture);
        TestBounds(fixture);
    }
    TestNumericEdges();
    return 0;
}
