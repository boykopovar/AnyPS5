#include "Ngs2JobTest.hpp"

#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>

enum class Activation { Untouched, Late, FailBeforePlay, FailThenPlay, ProcessControl, PeerControl, Reset, Guards, Cycle, Exception };
struct ActivationState {
    Activation mode;
    std::uintptr_t voice = 0;
    std::uintptr_t peer = 0;
    std::uintptr_t system = 0;
    std::uintptr_t rack = 0;
    unsigned tick = 0;
    unsigned setups = 0;
    unsigned calls = 0;
    unsigned controls = 0;
    unsigned guards = 0;
    bool nested = false;
    bool crossThread = false;
    std::array<std::uint32_t, 8> flags{};
    std::array<float, 8> before{};
};

static int SetValue(std::uintptr_t voice, float value) {
    const Ngs2CustomVoiceUserFx2Param param{{sizeof(param), 0, SCE_NGS2_CUSTOM_VOICE_PARAM_USER_FX2}, &value, sizeof(value)};
    return sceNgs2VoiceControl(voice, &param.header);
}

template <typename TAction>
static void RejectReentry(ActivationState& state, TAction action) {
    bool rejected = false;
    try { action(); } catch (const std::runtime_error&) { rejected = true; }
    Require(rejected);
    ++state.guards;
}

static int APS5_VABI ActivationControl(Ngs2UserFx2ControlContext* context) {
    auto& state = *reinterpret_cast<ActivationState*>(context->user_data);
    Require(context->data_size == sizeof(float) && state.controls < state.before.size());
    state.before[state.controls++] = *static_cast<float*>(context->param);
    const float value = *static_cast<const float*>(context->data);
    if (value == 0.25f && state.mode == Activation::PeerControl) {
        const auto controlPeer = [&] {
            std::uint32_t flags = 99;
            Require(sceNgs2VoiceGetStateFlags(state.peer, &flags) == SCE_NGS2_OK && flags == 0);
            Require(SetValue(state.peer, 0.75f) == SCE_NGS2_OK);
        };
        if (state.crossThread) { std::thread helper(controlPeer); helper.join(); }
        else controlPeer();
        state.nested = true;
    }
    if (state.mode == Activation::Cycle && value == 0.25f) {
        Require(SetValue(state.peer, 0.75f) == SCE_NGS2_OK);
        state.nested = true;
    }
    if (state.mode == Activation::Cycle && value == 0.75f)
        RejectReentry(state, [&] { SetValue(state.peer, 0.5f); });
    if (state.mode == Activation::Guards && value == 0.25f) {
        std::uint32_t flags = 0;
        Ngs2VoiceState voiceState{};
        Ngs2VoicePortInfo port{};
        Ngs2VoiceMatrixInfo matrix{};
        std::uintptr_t owner = 0;
        Ngs2VoiceChannelsInfo channels{};
        std::array<float, 128> audio{};
        const Ngs2RenderBufferInfo output{audio.data(), sizeof(audio), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 2};
        RejectReentry(state, [&] { SetValue(state.voice, 0.75f); });
        RejectReentry(state, [&] { sceNgs2VoiceRunCommands(state.voice, nullptr, 0); });
        RejectReentry(state, [&] { sceNgs2VoiceGetStateFlags(state.voice, &flags); });
        RejectReentry(state, [&] { sceNgs2VoiceGetState(state.voice, &voiceState, sizeof(voiceState)); });
        RejectReentry(state, [&] { sceNgs2VoiceGetOwner(state.voice, &owner, &flags); });
        RejectReentry(state, [&] { sceNgs2VoiceGetPortInfo(state.voice, 0, &port, sizeof(port)); });
        RejectReentry(state, [&] { sceNgs2VoiceGetMatrixInfo(state.voice, 0, &matrix, sizeof(matrix)); });
        RejectReentry(state, [&] { sceNgs2VoiceQueryInfo(state.voice, SCE_NGS2_VOICE_INFO_CHANNELS, &channels, sizeof(channels)); });
        RejectReentry(state, [&] { sceNgs2RackDestroy(state.rack, nullptr); });
        RejectReentry(state, [&] { sceNgs2SystemDestroy(state.system, nullptr); });
        RejectReentry(state, [&] { sceNgs2SystemRender(state.system, &output, 1); });
    }
    if (state.mode == Activation::Exception && value == 0.25f) throw std::runtime_error("control callback failure");
    *static_cast<float*>(context->param) = value * 2.0f;
    return state.mode == Activation::FailBeforePlay || state.mode == Activation::FailThenPlay ? -55 : SCE_NGS2_OK;
}

static int APS5_VABI ActivationSetup(Ngs2UserFx2SetupContext* context) {
    ++reinterpret_cast<ActivationState*>(context->user_data)->setups;
    *static_cast<float*>(context->param) = 1.0f;
    return SCE_NGS2_OK;
}

static int APS5_VABI ActivationProcess(Ngs2UserFx2ProcessContext* context) {
    auto& state = *reinterpret_cast<ActivationState*>(context->user_data);
    Require(state.calls < state.flags.size());
    state.flags[state.calls++] = context->flags;
    const float value = *static_cast<const float*>(context->param);
    if (state.mode == Activation::ProcessControl && state.calls == 2) {
        const auto controlSelf = [&] { Require(SetValue(state.voice, 0.25f) == SCE_NGS2_OK); };
        if (state.crossThread) { std::thread helper(controlSelf); helper.join(); }
        else controlSelf();
        Require(*static_cast<const float*>(context->param) == value);
    }
    for (unsigned channel = 0; channel < context->num_input_channels; ++channel)
        for (unsigned i = 0; i < context->num_grain_samples; ++i) context->channel_data[channel][i] *= value;
    return SCE_NGS2_OK;
}

static std::uintptr_t ActivationVoice(std::uintptr_t system, ActivationState& state, bool handler, bool sampler = false) {
    const Ngs2CustomUserFx2ModuleOption module{{sizeof(module)}, ActivationSetup, nullptr, handler ? ActivationControl : nullptr,
        ActivationProcess, 0, sizeof(float), 0, reinterpret_cast<std::uintptr_t>(&state)};
    Ngs2CustomSubmixerRackOption option{};
    Ngs2CustomSamplerRackOption samplerOption{};
    auto& custom = sampler ? samplerOption.custom_rack_option : option.custom_rack_option;
    auto& common = custom.rack_option;
    common.size = sampler ? sizeof(samplerOption) : sizeof(option);
    common.max_grain_samples = 512;
    common.max_voices = 1;
    common.max_matrices = 1;
    common.max_ports = 1;
    custom.state_size = sampler ? 48 : 16;
    custom.num_buffers = 1;
    custom.num_modules = 1;
    custom.module[0].module_id = SCE_NGS2_CUSTOM_MODULE_ID_USER_FX2;
    custom.module[0].option = &module.custom_module_option;
    option.max_channels = 2;
    option.max_inputs = 1;
    samplerOption.max_channel_works = 2;
    samplerOption.max_waveform_blocks = 4;
    const auto id = sampler ? SCE_NGS2_RACK_ID_CUSTOM_SAMPLER : SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER;
    Ngs2ContextBufferInfo query{};
    Require(sceNgs2RackQueryBufferSize(id, &common, &query) == SCE_NGS2_OK);
    const auto buffer = Buffer(query);
    Require(TestRackCreate(system, id, &common, &buffer, &state.rack) == SCE_NGS2_OK);
    state.system = system;
    state.voice = Voice(state.rack);
    if (!sampler) Control(state.voice, SCE_NGS2_CUSTOM_SUBMIXER_VOICE_PARAM_SETUP, Ngs2CustomSubmixerVoiceSetupParam{{}, 2, 2, 0});
    return state.voice;
}

static int APS5_VABI ActivationSend(Ngs2UserFxProcessContext* context) {
    auto& state = *reinterpret_cast<ActivationState*>(context->user_data0);
    if (state.tick == 1 && state.mode != Activation::Untouched && state.mode != Activation::ProcessControl && state.mode != Activation::Reset && state.mode != Activation::Guards && state.mode != Activation::Cycle && state.mode != Activation::Exception)
        Require(SetValue(state.voice, 0.25f) == (state.mode == Activation::FailBeforePlay || state.mode == Activation::FailThenPlay ? -55 : SCE_NGS2_OK));
    if (state.tick == 3 && (state.mode == Activation::FailBeforePlay || state.mode == Activation::FailThenPlay)) Event(state.voice, SCE_NGS2_VOICE_EVENT_PLAY);
    ++state.tick;
    for (unsigned channel = 0; channel < context->num_channels; ++channel)
        std::fill_n(context->channel_data[channel], context->num_grain_samples, 0.0f);
    return SCE_NGS2_OK;
}

static void TestActivation(Activation mode, unsigned order, bool handler = true, bool sampler = false, bool external = true, bool crossThread = false) {
    const auto firstBuffer = usedBuffers;
    Manager manager{{}, order};
    Ngs2JobSchedulerOption scheduler{};
    scheduler.size = sizeof(scheduler);
    scheduler.user_data = reinterpret_cast<std::uintptr_t>(&manager);
    scheduler.query_handler = Query;
    scheduler.init_handler = Initialize;
    scheduler.destroy_handler = Destroy;
    scheduler.submit_handler = Submit;
    scheduler.flush_handler = Complete;
    scheduler.join_handler = Complete;
    scheduler.num_workers = 2;
    scheduler.max_jobs = 16;
    Ngs2SystemOption option{};
    option.size = sizeof(option);
    option.max_grain_samples = 512;
    option.num_grain_samples = 64;
    option.sample_rate = 48000;
    option.max_voice_channels = 8;
    if (external) option.job_scheduler_options[0] = reinterpret_cast<std::uintptr_t>(&scheduler);
    Ngs2ContextBufferInfo query{};
    Require(sceNgs2SystemQueryBufferSize(&option, &query) == SCE_NGS2_OK);
    const auto buffer = Buffer(query);
    std::uintptr_t system = 0;
    Require(sceNgs2SystemCreate(&option, &buffer, &system) == SCE_NGS2_OK);
    const auto master = Mastering(system, 2);
    ActivationState state{mode}, peer{mode};
    state.crossThread = crossThread;
    const auto target = ActivationVoice(system, state, handler, sampler);
    if (mode == Activation::PeerControl || mode == Activation::Cycle) {
        state.peer = ActivationVoice(system, peer, true);
        peer.peer = target;
    }
    const auto sender = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SUBMIXER));
    Control(sender, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, 2, 0});
    Control(sender, SCE_NGS2_SUBMIXER_VOICE_PARAM_USER_FX,
        Ngs2SubmixerVoiceUserFxParam{{}, ActivationSend, reinterpret_cast<std::uintptr_t>(&state), 0, 0});
    const auto source = sampler ? target : Voice(CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
    Control(source, sampler ? SCE_NGS2_CUSTOM_SAMPLER_VOICE_PARAM_SETUP : SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 2, 48000, 0, 0, 0}});
    std::array<std::int16_t, 2048> pcm{};
    pcm.fill(4096);
    const Ngs2WaveformBlock block{0, sizeof(pcm), 0, 0, pcm.size() / 2, 0, 0};
    Control(source, sampler ? SCE_NGS2_CUSTOM_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS : SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), 0, 1, &block});
    if (!sampler) Patch(source, target);
    Patch(target, master);
    Patch(sender, master);
    if (mode == Activation::FailBeforePlay) Require(SetValue(target, 1.0f) == -55);
    if (mode == Activation::ProcessControl || mode == Activation::PeerControl || mode == Activation::Reset || mode == Activation::Guards || mode == Activation::Cycle || mode == Activation::Exception)
        Require(SetValue(target, 1.0f) == SCE_NGS2_OK);
    if (mode == Activation::Guards || mode == Activation::Cycle) Require(SetValue(target, 0.25f) == SCE_NGS2_OK);
    if (mode == Activation::Exception) {
        RejectReentry(state, [&] { SetValue(target, 0.25f); });
        Require(SetValue(target, 0.5f) == SCE_NGS2_OK);
    }
    Event(source, SCE_NGS2_VOICE_EVENT_PLAY);
    if (!sampler) Event(target, SCE_NGS2_VOICE_EVENT_PLAY);
    Event(sender, SCE_NGS2_VOICE_EVENT_PLAY);
    for (unsigned grain = 0; grain < 6; ++grain) {
        std::array<float, 130> audio{};
        audio.fill(-1234.0f);
        const Ngs2RenderBufferInfo output{audio.data(), 128 * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 2};
        Require(sceNgs2SystemRender(system, &output, 1) == SCE_NGS2_OK && manager.batches.empty());
        const bool restart = (mode == Activation::FailBeforePlay || mode == Activation::FailThenPlay) && grain == 3;
        Require(Flags(target) == (restart ? 1u : 3u));
        const bool active = mode == Activation::Reset ? (grain < 3 || grain >= 5) : mode == Activation::Late ? grain >= 2 : mode == Activation::FailThenPlay ? grain >= 4 : mode != Activation::Untouched;
        const unsigned calls = mode == Activation::Reset ? (grain < 3 ? grain + 1 : grain < 5 ? 3 : 4) : active ? mode == Activation::Late ? grain - 1 : mode == Activation::FailThenPlay ? grain - 3 : grain + 1 : 0;
        Require(state.calls == calls);
        if (active) {
            const bool first = calls == 1 || (mode == Activation::FailBeforePlay && grain == 4) || (mode == Activation::Reset && grain == 5);
            const bool changed = grain == 2 && (mode == Activation::ProcessControl || mode == Activation::PeerControl);
            Require(state.flags[calls - 1] == (first ? 3u : changed ? 2u : 0u));
        }
        float gain = 1.0f;
        if (active) {
            if (mode == Activation::Exception) gain = 1.0f;
            else if (mode == Activation::Reset) gain = grain < 3 ? 2.0f : 1.5f;
            else if (mode == Activation::Guards || mode == Activation::Cycle) gain = 0.5f;
            else if (mode == Activation::FailBeforePlay) gain = grain >= 4 ? 0.5f : 2.0f;
            else if (mode == Activation::ProcessControl || mode == Activation::PeerControl) gain = grain >= 2 ? 0.5f : 2.0f;
            else gain = handler ? 0.5f : 0.25f;
        }
        const bool restarting = grain == 0 || (sampler && mode == Activation::Reset && grain == 3) || (sampler && grain == 4 && (mode == Activation::FailBeforePlay || mode == Activation::FailThenPlay));
        for (unsigned i = 0; i < 128; ++i) Require(std::fabs(audio[i] - (restarting && i < 6 ? 0.0f : 0.125f * gain)) <= 1.0e-7f);
        Require(audio[128] == -1234.0f && audio[129] == -1234.0f);
        if (mode == Activation::Reset && grain == 2) {
            if (sampler) {
                Control(target, SCE_NGS2_CUSTOM_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 2, 48000, 0, 0, 0}});
                Control(target, SCE_NGS2_CUSTOM_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), 0, 1, &block});
            } else Control(target, SCE_NGS2_CUSTOM_SUBMIXER_VOICE_PARAM_SETUP, Ngs2CustomSubmixerVoiceSetupParam{{}, 2, 2, 0});
            Patch(target, master);
            Event(target, SCE_NGS2_VOICE_EVENT_PLAY);
        }
        if (mode == Activation::Reset && grain == 4) Require(SetValue(target, 0.75f) == SCE_NGS2_OK);
    }
    if (handler && mode != Activation::Untouched) {
        Require(state.controls == (mode == Activation::Exception ? 3u : mode == Activation::Late || mode == Activation::FailThenPlay ? 1u : 2u));
        Require(state.before[0] == 1.0f);
        if (state.controls == 2) Require(state.before[1] == (mode == Activation::Reset ? 1.0f : 2.0f));
    }
    if (mode == Activation::PeerControl || mode == Activation::Cycle) Require(state.nested && peer.controls == 1 && peer.before[0] == 1.0f);
    if (mode == Activation::Exception) Require(state.guards == 1 && state.before[1] == 2.0f && state.before[2] == 2.0f);
    if (mode == Activation::Guards) Require(state.guards == 11);
    if (mode == Activation::Cycle) Require(peer.guards == 1);
    Require(state.setups == 1);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    ResetBuffers(firstBuffer);
}

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "guards") == 0) {
        TestActivation(Activation::Guards, 0);
        TestActivation(Activation::Cycle, 0);
        TestActivation(Activation::Exception, 0);
        std::puts("NGS2 control reentry guards passed");
        return 0;
    }
    Require(argc == 1);
    unsigned cases = 0;
    for (auto mode : {Activation::Untouched, Activation::Late, Activation::FailBeforePlay, Activation::FailThenPlay, Activation::ProcessControl, Activation::PeerControl})
        for (unsigned order = 0; order < 3; ++order) { TestActivation(mode, order); ++cases; }
    for (auto mode : {Activation::Untouched, Activation::Late, Activation::FailBeforePlay, Activation::FailThenPlay, Activation::ProcessControl, Activation::PeerControl}) {
        TestActivation(mode, 2, true, true);
        ++cases;
    }
    for (auto mode : {Activation::ProcessControl, Activation::PeerControl}) {
        TestActivation(mode, 2, true, false, true, true);
        TestActivation(mode, 0, true, false, false, true);
        TestActivation(mode, 2, true, true, true, true);
        cases += 3;
    }
    TestActivation(Activation::Reset, 2);
    TestActivation(Activation::Reset, 2, true, true);
    cases += 2;
    TestActivation(Activation::Late, 0, true, false, false);
    TestActivation(Activation::ProcessControl, 0, true, false, false);
    cases += 2;
    TestActivation(Activation::Untouched, 2, false);
    TestActivation(Activation::Late, 2, false);
    std::printf("NGS2 activation: %u scenarios passed\n", cases + 2);
    return 0;
}
