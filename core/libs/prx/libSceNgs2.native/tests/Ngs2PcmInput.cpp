#include "Ngs2Test.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <string_view>

static uintptr_t Sampler(uintptr_t system, std::uint32_t rate = 48000, std::uint32_t channels = 1) {
    const auto voice = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP,
            Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, channels, rate, 0, 0, 0}});
    Patch(voice, Mastering(system, channels));
    return voice;
}

static void Queue(uintptr_t voice, const std::int16_t* data, std::uint32_t frames,
                  std::uint32_t flags = SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE,
                  std::uint32_t declared = 0, std::uint32_t channels = 1) {
    const Ngs2WaveformBlock block{0, frames * channels * sizeof(std::int16_t), 0, 0, declared ? declared : frames, 0, 0};
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS,
            Ngs2SamplerVoiceWaveformBlocksParam{{}, data, flags, 1, &block});
}

template <std::uint32_t Channels = 1>
static std::array<float, Grain * Channels> Render(uintptr_t system) {
    std::array<float, Grain * Channels> output{};
    const Ngs2RenderBufferInfo info{output.data(), sizeof(output), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, Channels};
    Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_OK);
    return output;
}

static Ngs2SamplerVoiceState State(uintptr_t voice) {
    Ngs2SamplerVoiceState state{};
    Require(sceNgs2VoiceGetState(voice, &state.voice_state, sizeof(state)) == SCE_NGS2_OK);
    return state;
}

static void Constant(const std::array<float, Grain>& output, std::int16_t value) {
    for (const auto sample : output) Require(std::abs(sample - value / 32768.0f) < 1e-5f);
}

static void TestLateFillAndReleasedInput() {
    const auto system = CreateSystem();
    const auto voice = Sampler(system);
    std::array<std::int16_t, Grain * 2> first, future;
    first.fill(1000); future.fill(2000);
    Queue(voice, first.data(), first.size());
    Queue(voice, future.data(), future.size());
    Require(State(voice).decoded_data_size == 0);
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    Constant(Render(system), 1000);
    const auto progress = State(voice);
    Require(progress.num_decoded_samples == Grain && progress.decoded_data_size == sizeof(first));
    Require(progress.waveform_data == first.data() + Grain);
    first.fill(-1000);
    future.fill(3000);
    Constant(Render(system), 1000);
    Require(State(voice).decoded_data_size == sizeof(first));
    future.fill(4000);
    Constant(Render(system), 4000);
    Require(State(voice).decoded_data_size == sizeof(first) + sizeof(future));
    future.fill(-4000);
    Constant(Render(system), 4000);
    Require(State(voice).waveform_data == future.data() + future.size());
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

template <std::uint32_t Channels = 1>
static void TestReleasedRegionRing(std::uint32_t rate) {
    const auto system = CreateSystem();
    const auto voice = Sampler(system, rate, Channels);
    std::array<std::array<std::int16_t, Grain * 2 * Channels>, 4> ring{};
    struct Region { std::int16_t* data; std::uint32_t frames; };
    std::deque<Region> pending;
    std::uint32_t generated = 0;
    const auto sample = [](std::uint32_t frame, std::uint32_t channel) {
        const auto value = static_cast<std::int16_t>(1000 + (frame % 512) * 32);
        return channel == 0 ? value : static_cast<std::int16_t>(-value);
    };
    const auto queue = [&](std::int16_t* data, std::uint32_t frames) {
        for (std::uint32_t i = 0; i < frames; ++i, ++generated) {
            for (std::uint32_t channel = 0; channel < Channels; ++channel) data[i * Channels + channel] = sample(generated, channel);
        }
        Queue(voice, data, frames, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE, 0, Channels);
        pending.push_back({data, frames});
    };
    for (auto& buffer : ring) queue(buffer.data(), buffer.size() / Channels);
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    double position = 0;
    std::uint64_t decoded = 0;
    for (int render = 0; render < 100; ++render) {
        const auto output = Render<Channels>(system);
        for (std::uint32_t i = 0; i < Grain; ++i) {
            const auto frame = static_cast<std::uint32_t>(position);
            for (std::uint32_t channel = 0; channel < Channels; ++channel) {
                const auto expected = std::lerp(static_cast<float>(sample(frame, channel)), static_cast<float>(sample(frame + 1, channel)), static_cast<float>(position - frame)) / 32768.0f;
                Require(std::abs(output[i * Channels + channel] - expected) < 1e-4f);
            }
            position += rate / 48000.0;
        }
        const auto state = State(voice);
        Require(state.decoded_data_size >= state.num_decoded_samples * Channels * sizeof(std::int16_t));
        Require(state.decoded_data_size <= generated * Channels * sizeof(std::int16_t));
        auto remaining = (state.decoded_data_size - decoded) / (Channels * sizeof(std::int16_t));
        while (remaining != 0) {
            Require(!pending.empty());
            const auto released = pending.front();
            const auto count = static_cast<std::uint32_t>(std::min<std::uint64_t>(remaining, released.frames));
            pending.front().data += count * Channels;
            pending.front().frames -= count;
            if (pending.front().frames == 0) pending.pop_front();
            queue(released.data, count);
            remaining -= count;
        }
        decoded = state.decoded_data_size;
    }
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestStreamPieceLifetime() {
    const auto system = CreateSystem();
    const auto voice = Sampler(system);
    std::array<std::int16_t, 6> first;
    std::array<std::int16_t, Grain * 2> next;
    first.fill(1000); next.fill(2000);
    Queue(voice, first.data(), first.size(), SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE, first.size() + next.size());
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    const auto beginning = Render(system);
    for (std::uint32_t i = 0; i < Grain; ++i) Require(beginning[i] == (i < first.size() ? 1000 / 32768.0f : 0));
    Queue(voice, next.data(), next.size(), SCE_NGS2_WAVEFORM_BLOCKS_FLAG_APPEND);
    next.fill(3000);
    Constant(Render(system), 3000);
    Require(State(voice).decoded_data_size == sizeof(first) + sizeof(next));
    next.fill(-3000);
    Constant(Render(system), 3000);
    Require(State(voice).num_decoded_samples == first.size() + next.size());
    Require(State(voice).waveform_data == next.data() + next.size());
    Require(Flags(voice) == 0);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

int main(int argc, char** argv) {
    const bool reportedRegions = argc == 2 && std::string_view(argv[1]) == "--reported-regions";
    if (argc != 1 && !reportedRegions) return 2;
    if (!reportedRegions) TestLateFillAndReleasedInput();
    TestReleasedRegionRing(48000);
    TestReleasedRegionRing(44100);
    TestReleasedRegionRing<2>(48000);
    TestReleasedRegionRing<2>(44100);
    if (!reportedRegions) TestStreamPieceLifetime();
}
