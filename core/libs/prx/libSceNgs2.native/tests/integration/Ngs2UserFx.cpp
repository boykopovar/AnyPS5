#include "Ngs2Test.hpp"

#include <array>
#include <stdexcept>

struct State {
    std::uint32_t calls = 0;
    std::uint32_t flags = 0;
    std::uint32_t channels = 0;
    float gain = 1.0f;
    float bias = 0.0f;
    int result = 0;
};

static int APS5_VABI Process(Ngs2UserFxProcessContext* context) {
    auto& state = *reinterpret_cast<State*>(context->user_data0);
    Require(context->user_data1 == 123 && context->user_data2 == 456);
    Require(context->num_channels == state.channels && context->num_grain_samples == Grain && context->sample_rate == 48000);
    state.calls++;
    state.flags = context->flags;
    for (std::uint32_t c = 0; c < context->num_channels; ++c)
        for (std::uint32_t i = 0; i < context->num_grain_samples; ++i)
            context->channel_data[c][i] = context->channel_data[c][i] * state.gain + state.bias * (c + 1);
    return state.result;
}

static void Install(uintptr_t voice, State& state) {
    Control(voice, SCE_NGS2_SUBMIXER_VOICE_PARAM_USER_FX,
            Ngs2SubmixerVoiceUserFxParam{{}, Process, reinterpret_cast<std::uintptr_t>(&state), 123, 456});
}

static std::vector<float> Render(uintptr_t system, std::uint32_t channels) {
    std::vector<float> output(Grain * channels, -1.0f);
    const Ngs2RenderBufferInfo buffer{output.data(), output.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, channels};
    Require(sceNgs2SystemRender(system, &buffer, 1) == 0);
    return output;
}

static void TestGenerator() {
    const auto system = CreateSystem();
    const auto master = Mastering(system, 8);
    const auto voice = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SUBMIXER));
    Control(voice, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, 8, 0});
    State state{0, 0, 8, 1.0f, 0.125f};
    Install(voice, state);
    Patch(voice, master);
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    auto output = Render(system, 8);
    Require(state.calls == 1 && state.flags == 1);
    for (std::size_t i = 0; i < output.size(); ++i) Require(output[i] == 0.125f * (i % 8 + 1));
    Render(system, 8);
    Require(state.calls == 2 && state.flags == 0);
    Event(voice, SCE_NGS2_VOICE_EVENT_PAUSE);
    for (float sample : Render(system, 8)) Require(sample == 0.0f);
    Require(state.calls == 2);
    Event(voice, SCE_NGS2_VOICE_EVENT_RESUME);
    Render(system, 8);
    Require(state.calls == 3 && state.flags == 0);
    Event(voice, SCE_NGS2_VOICE_EVENT_STOP_IMM);
    for (float sample : Render(system, 8)) Require(sample == 0.0f);
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    Render(system, 8);
    Require(state.calls == 4 && state.flags == 1);
    Control(voice, SCE_NGS2_SUBMIXER_VOICE_PARAM_USER_FX, Ngs2SubmixerVoiceUserFxParam{});
    for (float sample : Render(system, 8)) Require(sample == 0.0f);
    Require(state.calls == 4);
    Install(voice, state);
    state.result = -5;
    bool rejected = false;
    try { Render(system, 8); } catch (const std::runtime_error&) { rejected = true; }
    Require(rejected);
    state.result = 0;
    Render(system, 8);
    Require(state.calls == 6 && state.flags == 1);
    Control(voice, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, 8, 0});
    Patch(voice, master);
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    for (float sample : Render(system, 8)) Require(sample == 0.0f);
    Require(state.calls == 6);
    Require(sceNgs2SystemDestroy(system, nullptr) == 0);
}

static void TestEffectAndIndependentVoices() {
    const auto system = CreateSystem();
    const auto master = Mastering(system, 2);
    const auto first = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SUBMIXER));
    const auto second = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SUBMIXER));
    for (auto voice : {first, second}) {
        Control(voice, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, 2, 0});
        Patch(voice, master);
        Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    }
    State firstState{0, 0, 2, 0.5f, 0.0f};
    State secondState{0, 0, 2, 1.0f, 0.125f};
    Install(first, firstState);
    Install(second, secondState);
    const auto sampler = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
    Control(sampler, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 2, 48000, 0, 0, 0}});
    std::array<std::int16_t, Grain * 2> pcm{};
    for (std::size_t i = 0; i < pcm.size(); ++i) pcm[i] = i % 2 == 0 ? 16384 : -8192;
    const Ngs2WaveformBlock block{0, sizeof(pcm), 0, 0, Grain, 0, 0};
    Control(sampler, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), 0, 1, &block});
    Patch(sampler, first);
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);
    const auto mixed = Render(system, 2);
    for (std::size_t i = 0; i < mixed.size(); ++i) Require(mixed[i] == (i % 2 == 0 ? 0.375f : 0.125f));
    Require(firstState.calls == 1 && secondState.calls == 1 && Flags(sampler) == 0);
    const auto tail = Render(system, 2);
    for (std::size_t i = 0; i < tail.size(); ++i) Require(tail[i] == (i % 2 == 0 ? 0.125f : 0.25f));
    Require(firstState.calls == 2 && secondState.calls == 2);
    Require(sceNgs2SystemDestroy(system, nullptr) == 0);
}

int main() {
    TestGenerator();
    TestEffectAndIndependentVoices();
}
