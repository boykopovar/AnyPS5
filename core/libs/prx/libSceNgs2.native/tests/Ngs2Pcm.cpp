#include "Ngs2Test.hpp"

#include <algorithm>
#include <array>
#include <bit>
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

static constexpr std::uint32_t PcmGrain = 64;

static std::uintptr_t PcmSystem() {
    Ngs2SystemOption option{};
    option.size = sizeof(option);
    option.max_grain_samples = 512;
    option.num_grain_samples = PcmGrain;
    option.sample_rate = 48000;
    option.max_voice_channels = 8;
    Ngs2ContextBufferInfo query{};
    Require(sceNgs2SystemQueryBufferSize(&option, &query) == SCE_NGS2_OK);
    const auto buffer = Buffer(query);
    std::uintptr_t system = 0;
    Require(sceNgs2SystemCreate(&option, &buffer, &system) == SCE_NGS2_OK);
    return system;
}

static PcmFixture Canonical(const PcmFixture& fixture) {
    PcmFixture result{SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 4, {}, fixture.samples};
    for (float sample : fixture.samples) {
        const auto bits = std::bit_cast<std::uint32_t>(sample);
        for (unsigned byte = 0; byte < 4; ++byte) result.data.push_back(static_cast<std::uint8_t>(bits >> (byte * 8)));
    }
    return result;
}

class Playback {
public:
    Playback(std::uint32_t type, std::uint32_t channels = 1, std::uint32_t rate = 48000)
        : firstBuffer(usedBuffers), system(PcmSystem()), channels(channels), voice(Voice(CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER))) {
        Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {type, channels, rate, 0, 0, 0}});
        Patch(voice, Mastering(system, channels));
        Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    }

    ~Playback() {
        Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
        ResetBuffers(firstBuffer);
    }

    void Add(const void* data, const Ngs2WaveformBlock* blocks, std::uint32_t count, std::uint32_t flags = 0) {
        Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, data, flags, count, blocks});
    }

    std::vector<float> Render() {
        std::vector<float> output(PcmGrain * channels, 123);
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

static void SameState(Playback& actual, Playback& reference, const std::uint8_t* actualData,
                      const std::uint8_t* referenceData, std::uint32_t bytes) {
    const auto state = actual.State();
    const auto expected = reference.State();
    Require(state.voice_state.state_flags == expected.voice_state.state_flags && state.num_decoded_samples == expected.num_decoded_samples);
    Require(state.decoded_data_size % bytes == 0 && expected.decoded_data_size % 4 == 0);
    Require(state.decoded_data_size / bytes == expected.decoded_data_size / 4);
    if (state.waveform_data == nullptr || expected.waveform_data == nullptr) Require(state.waveform_data == nullptr && expected.waveform_data == nullptr);
    else {
        const auto offset = reinterpret_cast<std::uintptr_t>(state.waveform_data) - reinterpret_cast<std::uintptr_t>(actualData);
        const auto expectedOffset = reinterpret_cast<std::uintptr_t>(expected.waveform_data) - reinterpret_cast<std::uintptr_t>(referenceData);
        Require(offset % bytes == 0 && expectedOffset % 4 == 0 && offset / bytes == expectedOffset / 4);
    }
}

static void TestFramesAndPositions(const PcmFixture& fixture, std::uint32_t channels) {
    const auto canonical = Canonical(fixture);
    constexpr std::uint32_t skip = 3;
    const auto dataFor = [&](const PcmFixture& input) {
        std::vector<std::uint8_t> data(3, 0xa5);
        for (std::uint32_t frame = 0; frame < skip + PcmGrain * 2; ++frame) {
            for (std::uint32_t channel = 0; channel < channels; ++channel) {
                const auto index = (frame + channel) % input.samples.size();
                const auto first = input.data.begin() + index * input.bytes;
                data.insert(data.end(), first, first + input.bytes);
            }
        }
        return data;
    };
    const auto data = dataFor(fixture);
    const auto expectedData = dataFor(canonical);
    Playback playback(fixture.type, channels), reference(canonical.type, channels);
    const Ngs2WaveformBlock block{2, data.size() - 3, 0, skip, PcmGrain * 2, 0, 0};
    const Ngs2WaveformBlock expectedBlock{2, expectedData.size() - 3, 0, skip, PcmGrain * 2, 0, 0};
    playback.Add(data.data() + 1, &block, 1);
    reference.Add(expectedData.data() + 1, &expectedBlock, 1);
    Control(playback.voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{});
    Control(reference.voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{});
    SameState(playback, reference, data.data() + 3, expectedData.data() + 3, fixture.bytes);
    for (unsigned pass = 0; pass < 3; ++pass) {
        Require(playback.Render() == reference.Render());
        SameState(playback, reference, data.data() + 3, expectedData.data() + 3, fixture.bytes);
    }
}

static void TestConstantConversions(const PcmFixture& fixture) {
    for (unsigned index = 0; index < fixture.samples.size(); ++index) {
        std::vector<std::uint8_t> data;
        for (unsigned frame = 0; frame < PcmGrain * 3; ++frame) {
            const auto first = fixture.data.begin() + index * fixture.bytes;
            data.insert(data.end(), first, first + fixture.bytes);
        }
        Playback playback(fixture.type);
        const Ngs2WaveformBlock block{0, data.size(), 0, 0, PcmGrain * 3, 0, 0};
        playback.Add(data.data(), &block, 1);
        const auto first = playback.Render();
        for (unsigned frame = 0; frame < PcmGrain; ++frame) Require(first[frame] == (frame < 3 ? 0.0f : fixture.samples[index]));
        const auto settled = playback.Render();
        for (float value : settled) Require(value == fixture.samples[index]);
    }
}

static std::vector<Ngs2VoiceCallbackInfo> callbacks;

static void APS5_VABI OnBlock(const Ngs2VoiceCallbackInfo* info) {
    callbacks.push_back(*info);
}

static void TestRepeatAndTransition(const PcmFixture& fixture) {
    for (unsigned repeats : {0u, 1u, 2u, 3u}) for (unsigned skip : {0u, 1u}) {
        Playback window(fixture.type);
        const Ngs2WaveformBlock block{0, fixture.data.size(), repeats, skip, 7, 0, 0};
        window.Add(fixture.data.data(), &block, 1);
        const auto output = window.Render();
        const auto length = 7 + skip;
        const auto passes = std::max(1u, repeats);
        for (unsigned frame = 0; frame < PcmGrain; ++frame) {
            const float expected = frame >= 3 && frame - 3 < length * passes ? fixture.samples[(frame - 3) % length] : 0;
            Require(output[frame] == expected);
        }
    }
    const auto canonical = Canonical(fixture);
    Playback playback(fixture.type), reference(canonical.type);
    callbacks.clear();
    Control(playback.voice, SCE_NGS2_VOICE_PARAM_CALLBACK,
            Ngs2VoiceCallbackParam{{}, OnBlock, 7, SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_REPEAT | SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END, 0});
    const Ngs2WaveformBlock blocks[] = {{0, fixture.bytes * 4, 2, 1, 3, 0, 101}, {fixture.bytes * 4, fixture.bytes * 4, 0, 0, 4, 0, 202}};
    const Ngs2WaveformBlock expectedBlocks[] = {{0, 16, 2, 1, 3, 0, 101}, {16, 16, 0, 0, 4, 0, 202}};
    playback.Add(fixture.data.data(), blocks, 2);
    reference.Add(canonical.data.data(), expectedBlocks, 2);
    for (unsigned pass = 0; pass < 2; ++pass) {
        Require(playback.Render() == reference.Render());
        SameState(playback, reference, fixture.data.data(), canonical.data.data(), fixture.bytes);
    }
    if (callbacks.size() != 3) {
        std::fprintf(stderr, "PCM type %#x callback count %zu\n", fixture.type, callbacks.size());
        for (const auto& event : callbacks) std::fprintf(stderr, "event flag=%u user=%llu repeats=%u\n", event.flag, static_cast<unsigned long long>(event.user_data), event.num_repeated);
    }
    Require(callbacks.size() == 3);
    Require(callbacks[0].flag == SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_REPEAT && callbacks[0].user_data == 101 && callbacks[0].num_repeated == 0);
    Require(callbacks[1].flag == SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END && callbacks[1].user_data == 101);
    Require(callbacks[2].flag == SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END && callbacks[2].user_data == 202);
    Require(callbacks[0].block_data == fixture.data.data() && callbacks[2].block_data == fixture.data.data() + fixture.bytes * 4);
}

static void TestSkipWindowContract() {
    Playback playback(SCE_NGS2_WAVEFORM_TYPE_PCM_I16L);
    std::vector<std::int16_t> pcm(256, 8192);
    pcm[0] = -32768; pcm[1] = -16384; pcm[2] = -8192; pcm[3] = 0; pcm[4] = 16384;
    callbacks.clear();
    Control(playback.voice, SCE_NGS2_VOICE_PARAM_CALLBACK,
            Ngs2VoiceCallbackParam{{}, OnBlock, 7, SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END, 0});
    const Ngs2WaveformBlock blocks[] = {{0, 10, 0, 2, 3, 0, 101}, {10, 502, 0, 0, 251, 0, 202}};
    playback.Add(pcm.data(), blocks, 2);
    const auto output = playback.Render();
    const std::array<float, 8> prefix{0, 0, 0, -1, -0.5f, -0.25f, 0, 0.5f};
    Require(std::equal(prefix.begin(), prefix.end(), output.begin()));
    for (std::size_t i = prefix.size(); i < output.size(); ++i) Require(output[i] == 0.25f);
    const auto state = playback.State();
    Require(state.num_decoded_samples == 62 && state.decoded_data_size == 128);
    Require(callbacks.size() == 1 && callbacks.front().flag == SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END && callbacks.front().user_data == 101);
}

static void TestResampling(const PcmFixture& fixture) {
    const auto canonical = Canonical(fixture);
    Playback playback(fixture.type, 1, 24000), reference(canonical.type, 1, 24000);
    const Ngs2WaveformBlock blocks[] = {{0, fixture.bytes * 3, 0, 0, 3, 0, 0}, {fixture.bytes * 3, fixture.bytes * 5, 0, 0, 5, 0, 0}};
    const Ngs2WaveformBlock expectedBlocks[] = {{0, 12, 0, 0, 3, 0, 0}, {12, 20, 0, 0, 5, 0, 0}};
    playback.Add(fixture.data.data(), blocks, 2);
    reference.Add(canonical.data.data(), expectedBlocks, 2);
    for (unsigned pass = 0; pass < 2; ++pass) {
        Require(playback.Render() == reference.Render());
        SameState(playback, reference, fixture.data.data(), canonical.data.data(), fixture.bytes);
    }
}

static void TestPitch(const PcmFixture& fixture) {
    const auto canonical = Canonical(fixture);
    Playback playback(fixture.type), reference(canonical.type);
    for (auto voice : {playback.voice, reference.voice})
        Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_PITCH, Ngs2SamplerVoicePitchParam{{}, 2});
    const Ngs2WaveformBlock block{0, fixture.data.size(), 1, 0, 8, 0, 0};
    const Ngs2WaveformBlock expectedBlock{0, canonical.data.size(), 1, 0, 8, 0, 0};
    playback.Add(fixture.data.data(), &block, 1);
    reference.Add(canonical.data.data(), &expectedBlock, 1);
    Require(playback.Render() == reference.Render());
    SameState(playback, reference, fixture.data.data(), canonical.data.data(), fixture.bytes);
    Require(Flags(playback.voice) == Flags(reference.voice));
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
    TestSkipWindowContract();
    for (const auto& fixture : Fixtures) {
        TestConstantConversions(fixture);
        for (const auto channels : {1u, 2u, 6u, 8u}) TestFramesAndPositions(fixture, channels);
        TestRepeatAndTransition(fixture);
        TestResampling(fixture);
        TestPitch(fixture);
        TestBounds(fixture);
    }
    TestNumericEdges();
    return 0;
}
