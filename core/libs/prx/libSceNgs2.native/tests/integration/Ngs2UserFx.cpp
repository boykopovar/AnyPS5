#include "Ngs2Test.hpp"

#include <array>
#include <stdexcept>
#include <string>

namespace {

using namespace Ngs2Testing;
using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

struct State {
    std::uint32_t calls = 0;
    std::uint32_t flags = 0;
    std::uint32_t channels = 0;
    float gain = 1.0f;
    float bias = 0.0f;
    int result = 0;
    std::uint32_t contextMismatches = 0;
};

int APS5_VABI Process(Ngs2UserFxProcessContext* context) {
    auto& state = *reinterpret_cast<State*>(context->user_data0);
    if (context->user_data1 != 123 || context->user_data2 != 456 || context->num_channels != state.channels ||
        context->num_grain_samples != Grain || context->sample_rate != 48000) {
        state.contextMismatches++;
    }
    state.calls++;
    state.flags = context->flags;
    for (std::uint32_t c = 0; c < context->num_channels; ++c)
        for (std::uint32_t i = 0; i < context->num_grain_samples; ++i)
            context->channel_data[c][i] = context->channel_data[c][i] * state.gain + state.bias * (c + 1);
    return state.result;
}

void Install(uintptr_t voice, State& state) {
    Control(voice, SCE_NGS2_SUBMIXER_VOICE_PARAM_USER_FX,
            Ngs2SubmixerVoiceUserFxParam{{}, Process, reinterpret_cast<std::uintptr_t>(&state), 123, 456});
}

std::vector<float> Render(uintptr_t system, std::uint32_t channels) {
    std::vector<float> output(Grain * channels, -1.0f);
    const Ngs2RenderBufferInfo buffer{output.data(), output.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, channels};
    RequireEqual(sceNgs2SystemRender(system, &buffer, 1), 0, "render");
    return output;
}

void RequireSilence(const std::vector<float>& output, const char* message) {
    for (std::size_t i = 0; i < output.size(); ++i) RequireEqual(output[i], 0.0f, std::string(message) + " sample " + std::to_string(i));
}

void RequireCalls(const State& state, std::uint32_t calls, std::uint32_t flags, const char* message) {
    RequireEqual(state.calls, calls, std::string(message) + " call count");
    RequireEqual(state.flags, flags, std::string(message) + " flags");
    RequireEqual(state.contextMismatches, 0u, std::string(message) + " context mismatches");
}

class GeneratorFixture {
public:
    GeneratorFixture() : system(ngs2.CreateSystem()), master(ngs2.Mastering(system, 8)),
                         voice(Voice(ngs2.CreateRack(system, SCE_NGS2_RACK_ID_SUBMIXER))) {
        Control(voice, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, 8, 0});
        Install(voice, state);
        Patch(voice, master);
        Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    }

    Ngs2Fixture ngs2;
    State state{0, 0, 8, 1.0f, 0.125f};
    const uintptr_t system;
    const uintptr_t master;
    const uintptr_t voice;
};

const Case generatorFirstGrain{"UserFx_GeneratorFirstGrain_WritesBiasAndReportsStartFlag", [] {
    GeneratorFixture fixture;
    const auto output = Render(fixture.system, 8);
    RequireCalls(fixture.state, 1, 1, "first grain");
    for (std::size_t i = 0; i < output.size(); ++i) {
        RequireEqual(output[i], 0.125f * (i % 8 + 1), "sample " + std::to_string(i));
    }
    Render(fixture.system, 8);
    RequireCalls(fixture.state, 2, 0, "second grain");
}};

const Case generatorPauseResume{"UserFx_PauseThenResume_SkipsProcessingWhilePaused", [] {
    GeneratorFixture fixture;
    Render(fixture.system, 8);
    Render(fixture.system, 8);
    Event(fixture.voice, SCE_NGS2_VOICE_EVENT_PAUSE);
    RequireSilence(Render(fixture.system, 8), "paused");
    RequireCalls(fixture.state, 2, 0, "paused");
    Event(fixture.voice, SCE_NGS2_VOICE_EVENT_RESUME);
    Render(fixture.system, 8);
    RequireCalls(fixture.state, 3, 0, "resumed");
}};

const Case generatorStopReplay{"UserFx_StopThenPlay_RestartsWithStartFlag", [] {
    GeneratorFixture fixture;
    Render(fixture.system, 8);
    Event(fixture.voice, SCE_NGS2_VOICE_EVENT_STOP_IMM);
    RequireSilence(Render(fixture.system, 8), "stopped");
    RequireCalls(fixture.state, 1, 1, "stopped");
    Event(fixture.voice, SCE_NGS2_VOICE_EVENT_PLAY);
    Render(fixture.system, 8);
    RequireCalls(fixture.state, 2, 1, "replayed");
}};

const Case generatorRemoved{"UserFx_HandlerRemoved_RendersSilenceWithoutCalls", [] {
    GeneratorFixture fixture;
    Render(fixture.system, 8);
    Control(fixture.voice, SCE_NGS2_SUBMIXER_VOICE_PARAM_USER_FX, Ngs2SubmixerVoiceUserFxParam{});
    RequireSilence(Render(fixture.system, 8), "removed");
    RequireCalls(fixture.state, 1, 1, "removed");
}};

const Case generatorFailure{"UserFx_HandlerReturnsError_RenderThrowsAndRecovers", [] {
    GeneratorFixture fixture;
    Render(fixture.system, 8);
    Control(fixture.voice, SCE_NGS2_SUBMIXER_VOICE_PARAM_USER_FX, Ngs2SubmixerVoiceUserFxParam{});
    Install(fixture.voice, fixture.state);
    fixture.state.result = -5;
    Testing::RequireThrows<std::runtime_error>([&] { Render(fixture.system, 8); }, "handler error");
    fixture.state.result = 0;
    Render(fixture.system, 8);
    RequireCalls(fixture.state, 3, 1, "after recovery");
}};

const Case generatorResetBySetup{"UserFx_VoiceSetupAgain_ClearsHandler", [] {
    GeneratorFixture fixture;
    Render(fixture.system, 8);
    Control(fixture.voice, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, 8, 0});
    Patch(fixture.voice, fixture.master);
    Event(fixture.voice, SCE_NGS2_VOICE_EVENT_PLAY);
    RequireSilence(Render(fixture.system, 8), "after setup");
    RequireCalls(fixture.state, 1, 1, "after setup");
}};

const Case effectChain{"UserFx_EffectsOnTwoVoices_ProcessIndependently", [] {
    Ngs2Fixture ngs2;
    const auto system = ngs2.CreateSystem();
    const auto master = ngs2.Mastering(system, 2);
    const auto first = Voice(ngs2.CreateRack(system, SCE_NGS2_RACK_ID_SUBMIXER));
    const auto second = Voice(ngs2.CreateRack(system, SCE_NGS2_RACK_ID_SUBMIXER));
    for (auto voice : {first, second}) {
        Control(voice, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, 2, 0});
        Patch(voice, master);
        Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    }
    State firstState{0, 0, 2, 0.5f, 0.0f};
    State secondState{0, 0, 2, 1.0f, 0.125f};
    Install(first, firstState);
    Install(second, secondState);
    const auto sampler = Voice(ngs2.CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
    Control(sampler, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP,
            Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 2, 48000, 0, 0, 0}});
    std::array<std::int16_t, Grain * 2> pcm{};
    for (std::size_t i = 0; i < pcm.size(); ++i) pcm[i] = i % 2 == 0 ? 16384 : -8192;
    const Ngs2WaveformBlock block{0, sizeof(pcm), 0, 0, Grain, 0, 0};
    Control(sampler, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS,
            Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), 0, 1, &block});
    Patch(sampler, first);
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);

    const auto mixed = Render(system, 2);
    for (std::size_t i = 0; i < mixed.size(); ++i) {
        RequireEqual(mixed[i], i % 2 == 0 ? 0.375f : 0.125f, "mixed sample " + std::to_string(i));
    }
    RequireEqual(firstState.calls, 1u, "first effect calls");
    RequireEqual(secondState.calls, 1u, "second effect calls");
    RequireEqual(Flags(sampler), 0u, "sampler finished");
    const auto tail = Render(system, 2);
    for (std::size_t i = 0; i < tail.size(); ++i) {
        RequireEqual(tail[i], i % 2 == 0 ? 0.125f : 0.25f, "tail sample " + std::to_string(i));
    }
    RequireEqual(firstState.calls, 2u, "first effect calls after tail");
    RequireEqual(secondState.calls, 2u, "second effect calls after tail");
    RequireEqual(firstState.contextMismatches + secondState.contextMismatches, 0u, "context mismatches");
}};

} // namespace
