#include "Ngs2JobTest.hpp"

#include <array>
#include <cmath>

enum class Family { Legacy, Custom, UserFx2Mixer, UserFx2Sampler, Mixer, Master };
struct FxCalls {
    unsigned count = 0;
    std::array<std::uint32_t, 8> flags{};
    std::array<float, 8> input{};
    unsigned controls = 0;
    std::array<float, 3> before{};
    bool reject = false;
    float bias = 0.0f;
};
struct UpdateFx {
    std::uintptr_t target;
    Family family;
    std::uint32_t event;
    bool commands;
    bool replace;
    FxCalls* replacement;
    unsigned tick = 0;
    unsigned controlMode = 0;
    bool remove = false;
};

static int APS5_VABI Process(Ngs2UserFxProcessContext* context) {
    auto& calls = *reinterpret_cast<FxCalls*>(context->user_data0);
    Require(calls.count < calls.flags.size());
    calls.flags[calls.count] = context->flags;
    calls.input[calls.count++] = context->channel_data[0][0];
    Require(context->num_channels == 2 && context->num_grain_samples == 64 && context->sample_rate == 48000);
    for (unsigned channel = 0; channel < 2; ++channel)
        for (unsigned i = 0; i < 64; ++i) context->channel_data[channel][i] += context->user_data1 != 0 ? 0.25f : 0.125f;
    return SCE_NGS2_OK;
}

static int APS5_VABI SetupFx(Ngs2UserFx2SetupContext* context) {
    *static_cast<float*>(context->param) = 1.0f;
    return SCE_NGS2_OK;
}

static int APS5_VABI ProcessFx(Ngs2UserFx2ProcessContext* context) {
    auto& calls = *reinterpret_cast<FxCalls*>(context->user_data);
    Require(calls.count < calls.flags.size());
    calls.flags[calls.count] = context->flags;
    calls.input[calls.count++] = context->channel_data[0][0];
    Require(context->num_input_channels == 2 && context->num_output_channels == 2 && context->num_grain_samples == 64);
    for (unsigned channel = 0; channel < 2; ++channel)
        for (unsigned i = 0; i < 64; ++i) context->channel_data[channel][i] = context->channel_data[channel][i] * *static_cast<const float*>(context->param) + calls.bias;
    return SCE_NGS2_OK;
}

static int APS5_VABI ControlFx(Ngs2UserFx2ControlContext* context) {
    auto& calls = *reinterpret_cast<FxCalls*>(context->user_data);
    Require(context->data_size == sizeof(float) && calls.controls < calls.before.size());
    calls.before[calls.controls++] = *static_cast<float*>(context->param);
    const float input = *static_cast<const float*>(context->data);
    *static_cast<float*>(context->param) = input * 2.0f;
    return calls.reject && input == 0.25f ? -55 : SCE_NGS2_OK;
}

static void SendEvent(const UpdateFx& update, std::uint32_t event) {
    if (!update.commands) Event(update.target, event);
    else {
        Ngs2VoiceCommand command{};
        command.id = 2;
        command.type = 4;
        command.value.u = event;
        Require(sceNgs2VoiceRunCommands(update.target, &command, 1) == SCE_NGS2_OK);
    }
}

static int APS5_VABI Send(Ngs2UserFxProcessContext* context) {
    auto& update = *reinterpret_cast<UpdateFx*>(context->user_data0);
    if (update.tick == 1) {
        if (!update.replace) SendEvent(update, update.event);
        else if (update.family == Family::Legacy)
            Control(update.target, SCE_NGS2_SUBMIXER_VOICE_PARAM_USER_FX,
                    Ngs2SubmixerVoiceUserFxParam{{}, update.remove ? nullptr : Process, reinterpret_cast<std::uintptr_t>(update.replacement), 1, 0});
        else if (update.family == Family::Custom)
            Control(update.target, SCE_NGS2_CUSTOM_VOICE_PARAM_USER_FX,
                    Ngs2CustomVoiceUserFxParam{{}, update.remove ? nullptr : Process, reinterpret_cast<std::uintptr_t>(update.replacement), 1, 0});
        else {
            float gain = 0.25f;
            Ngs2CustomVoiceUserFx2Param param{{sizeof(param), 0, SCE_NGS2_CUSTOM_VOICE_PARAM_USER_FX2}, &gain, sizeof(gain)};
            Require(sceNgs2VoiceControl(update.target, &param.header) == (update.controlMode == 2 ? -55 : SCE_NGS2_OK));
            gain = -1.0f;
        }
        Require(Flags(update.target) == (!update.replace && update.event == SCE_NGS2_VOICE_EVENT_PLAY ? 1u : 3u));
    }
    if (update.tick == 3 && update.controlMode == 2) {
        const float gain = 0.75f;
        Control(update.target, SCE_NGS2_CUSTOM_VOICE_PARAM_USER_FX2, Ngs2CustomVoiceUserFx2Param{{}, &gain, sizeof(gain)});
    }
    if (update.tick == 4 && !update.replace && update.event == SCE_NGS2_VOICE_EVENT_PAUSE) SendEvent(update, SCE_NGS2_VOICE_EVENT_RESUME);
    ++update.tick;
    for (unsigned channel = 0; channel < context->num_channels; ++channel)
        std::fill_n(context->channel_data[channel], context->num_grain_samples, 0.0f);
    return SCE_NGS2_OK;
}

static std::uintptr_t CreateFx(std::uintptr_t system, Family family, FxCalls& calls, unsigned controlMode) {
    const auto user = reinterpret_cast<std::uintptr_t>(&calls);
    if (family == Family::Legacy || family == Family::Mixer) {
        const auto voice = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SUBMIXER));
        Control(voice, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, 2, 0});
        if (family == Family::Legacy)
            Control(voice, SCE_NGS2_SUBMIXER_VOICE_PARAM_USER_FX, Ngs2SubmixerVoiceUserFxParam{{}, Process, user, 0, 0});
        return voice;
    }
    const bool sampler = family == Family::UserFx2Sampler;
    Ngs2CustomSubmixerRackOption mixerOption{};
    Ngs2CustomSamplerRackOption samplerOption{};
    auto& custom = sampler ? samplerOption.custom_rack_option : mixerOption.custom_rack_option;
    auto& common = custom.rack_option;
    common.size = sampler ? sizeof(samplerOption) : sizeof(mixerOption);
    common.max_grain_samples = 512;
    common.max_voices = 1;
    common.max_matrices = 1;
    common.max_ports = 1;
    custom.state_size = sampler ? 48 : 16;
    custom.num_buffers = 1;
    custom.num_modules = 1;
    custom.module[0].module_id = family == Family::Custom ? SCE_NGS2_CUSTOM_MODULE_ID_USER_FX : SCE_NGS2_CUSTOM_MODULE_ID_USER_FX2;
    const Ngs2CustomUserFx2ModuleOption module{{sizeof(module)}, SetupFx, nullptr, controlMode != 0 ? ControlFx : nullptr, ProcessFx, 0, sizeof(float), 0, user};
    if (family != Family::Custom) custom.module[0].option = &module.custom_module_option;
    mixerOption.max_channels = 2;
    mixerOption.max_inputs = 1;
    samplerOption.max_channel_works = 2;
    samplerOption.max_waveform_blocks = 4;
    const auto id = sampler ? SCE_NGS2_RACK_ID_CUSTOM_SAMPLER : SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER;
    Ngs2ContextBufferInfo query{};
    Require(sceNgs2RackQueryBufferSize(id, &common, &query) == SCE_NGS2_OK);
    const auto buffer = Buffer(query);
    std::uintptr_t rack = 0;
    Require(TestRackCreate(system, id, &common, &buffer, &rack) == SCE_NGS2_OK);
    const auto voice = Voice(rack);
    if (!sampler) Control(voice, SCE_NGS2_CUSTOM_SUBMIXER_VOICE_PARAM_SETUP, Ngs2CustomSubmixerVoiceSetupParam{{}, 2, 2, 0});
    if (family == Family::Custom)
        Control(voice, SCE_NGS2_CUSTOM_VOICE_PARAM_USER_FX, Ngs2CustomVoiceUserFxParam{{}, Process, user, 0, 0});
    return voice;
}

static void TestFx(Family family, std::uint32_t event, unsigned order, bool commands = false, bool replace = false, unsigned controlMode = 0, bool remove = false, bool generate = false) {
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
    option.job_scheduler_options[0] = reinterpret_cast<std::uintptr_t>(&scheduler);
    Ngs2ContextBufferInfo query{};
    Require(sceNgs2SystemQueryBufferSize(&option, &query) == SCE_NGS2_OK);
    const auto buffer = Buffer(query);
    std::uintptr_t system = 0;
    Require(sceNgs2SystemCreate(&option, &buffer, &system) == SCE_NGS2_OK);
    const auto master = Mastering(system, 2);
    const auto sender = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SUBMIXER));
    Control(sender, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, 2, 0});
    FxCalls original{}, replacement{};
    original.reject = controlMode == 2;
    original.bias = generate ? 0.125f : 0.0f;
    const auto target = family == Family::Master ? master : CreateFx(system, family, original, controlMode);
    UpdateFx update{target, family, event, commands, replace, &replacement, 0, controlMode, remove};
    Control(sender, SCE_NGS2_SUBMIXER_VOICE_PARAM_USER_FX,
            Ngs2SubmixerVoiceUserFxParam{{}, Send, reinterpret_cast<std::uintptr_t>(&update), 0, 0});
    const bool sampler = family == Family::UserFx2Sampler;
    const auto source = sampler ? target : Voice(CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
    Control(source, sampler ? SCE_NGS2_CUSTOM_SAMPLER_VOICE_PARAM_SETUP : SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP,
            Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 2, 48000, 0, 0, 0}});
    std::array<std::int16_t, 4096> pcm{};
    pcm.fill(generate ? 0 : 4096);
    const Ngs2WaveformBlock block{0, sizeof(pcm), 0, 0, pcm.size() / 2, 0, 0};
    Control(source, sampler ? SCE_NGS2_CUSTOM_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS : SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS,
            Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), 0, 1, &block});
    const bool fx2 = family == Family::UserFx2Mixer || sampler;
    if (fx2) {
        const float gain = 1.0f;
        Control(target, SCE_NGS2_CUSTOM_VOICE_PARAM_USER_FX2, Ngs2CustomVoiceUserFx2Param{{}, &gain, sizeof(gain)});
    }
    if (!sampler) Patch(source, target);
    if (family != Family::Master) Patch(target, master);
    Patch(sender, master);
    Event(source, SCE_NGS2_VOICE_EVENT_PLAY);
    if (!sampler) Event(target, SCE_NGS2_VOICE_EVENT_PLAY);
    Event(sender, SCE_NGS2_VOICE_EVENT_PLAY);
    for (unsigned grain = 0; grain < 8; ++grain) {
        std::array<float, 130> samples{};
        samples.fill(-1234.0f);
        const Ngs2RenderBufferInfo output{samples.data(), 128 * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 2};
        Require(sceNgs2SystemRender(system, &output, 1) == SCE_NGS2_OK && manager.batches.empty());
        const bool ending = !replace && (event == SCE_NGS2_VOICE_EVENT_STOP || event == SCE_NGS2_VOICE_EVENT_STOP_IMM || event == SCE_NGS2_VOICE_EVENT_KILL);
        const bool paused = !replace && event == SCE_NGS2_VOICE_EVENT_PAUSE;
        const auto expectedFlags = ending && grain >= 2 ? 0u : !replace && event == SCE_NGS2_VOICE_EVENT_PLAY && grain == 1 ? 1u :
            paused && grain >= 2 && grain <= 4 ? (sampler && grain >= 3 ? 7u : 5u) : 3u;
        Require(Flags(target) == expectedFlags);
        if (remove && grain >= 2) Require(original.count == 2 && replacement.count == 0);
        else if (family != Family::Mixer && family != Family::Master) {
            const bool replaced = replace && !fx2 && grain >= 2;
            const auto& calls = replaced ? replacement : original;
            const unsigned expectedCalls = replaced ? grain - 1 : replace && !fx2 ? std::min(grain + 1, 2u) : ending ? std::min(grain + 1, 3u) : grain + 1;
            Require(calls.count == expectedCalls);
            if (!ending || grain <= 2) {
                const auto flags = grain == 0 || (!replace && event == SCE_NGS2_VOICE_EVENT_PLAY && grain == 2) ? 3u :
                    replace && ((grain == 2 && controlMode != 2) || (grain == 4 && controlMode == 2)) ? 2u : ending && grain == 2 ? (event == SCE_NGS2_VOICE_EVENT_KILL ? 32u : 8u) :
                    paused && grain >= 3 && grain <= 4 ? 4u : 0u;
                Require(calls.flags[calls.count - 1] == flags);
                const float input = generate || grain == 0 || (sampler && ((ending && event == SCE_NGS2_VOICE_EVENT_KILL && grain == 2) || (paused && grain == 4) || (!replace && event == SCE_NGS2_VOICE_EVENT_PLAY && grain == 2))) ? 0.0f : 0.125f;
                Require(calls.input[calls.count - 1] == input);
            }
        }
        for (unsigned i = 0; i < 64; ++i) for (unsigned channel = 0; channel < 2; ++channel) {
            const bool initialDelay = (grain == 0 || (sampler && !replace && event == SCE_NGS2_VOICE_EVENT_PLAY && grain == 2)) && i < 3;
            float expected = generate ? 0.0f : initialDelay ? 0.0f : 0.125f;
            if ((family == Family::Legacy || family == Family::Custom) && (!remove || grain < 2)) expected += replace && grain >= 2 ? 0.25f : 0.125f;
            if (fx2) {
                float gain = controlMode != 0 ? 2.0f : 1.0f;
                if (replace && grain >= 2 && controlMode != 2) gain = controlMode != 0 ? 0.5f : 0.25f;
                if (controlMode == 2 && grain >= 4) gain = 1.5f;
                expected = expected * gain + original.bias;
            }
            if (family != Family::Master && ending && event == SCE_NGS2_VOICE_EVENT_KILL && grain >= 1) expected = 0;
            else if (family != Family::Master && ending && grain == 2) expected *= 1.0f - static_cast<float>(i) / 64.0f;
            else if (ending && grain > 2) expected = 0;
            if (paused && family != Family::Master) {
                if (grain == 2) expected *= 1.0f - static_cast<float>(i) / 64.0f;
                if (grain == 3 || grain == 4) expected = 0;
                if (grain == 5) expected *= static_cast<float>(i) / 64.0f;
            }
            if (std::fabs(samples[i * 2 + channel] - expected) > 1.0e-7f) std::fprintf(stderr,
                "family=%u event=%u order=%u replace=%d grain=%u sample=%u actual=%g expected=%g\n",
                static_cast<unsigned>(family), event, order, replace, grain, i, samples[i * 2 + channel], expected);
            Require(std::fabs(samples[i * 2 + channel] - expected) <= 1.0e-7f);
        }
        Require(samples[128] == -1234.0f && samples[129] == -1234.0f);
    }
    if (controlMode != 0) {
        Require(original.controls == (controlMode == 2 ? 3u : 2u));
        Require(original.before[0] == 1.0f && original.before[1] == 2.0f);
        if (controlMode == 2) Require(original.before[2] == 0.5f);
    }
    Require(update.tick == 8 && sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    ResetBuffers(firstBuffer);
}

int main() {
    unsigned cases = 0;
    for (auto family : {Family::Legacy, Family::Custom, Family::UserFx2Mixer, Family::UserFx2Sampler, Family::Mixer, Family::Master}) {
        for (auto event : {SCE_NGS2_VOICE_EVENT_PLAY, SCE_NGS2_VOICE_EVENT_STOP, SCE_NGS2_VOICE_EVENT_STOP_IMM, SCE_NGS2_VOICE_EVENT_KILL, SCE_NGS2_VOICE_EVENT_PAUSE}) {
            TestFx(family, event, 0);
            ++cases;
        }
        for (unsigned order : {1u, 2u}) {
            TestFx(family, SCE_NGS2_VOICE_EVENT_STOP, order, true);
            TestFx(family, SCE_NGS2_VOICE_EVENT_PAUSE, order, true);
            cases += 2;
        }
        if (family != Family::Mixer && family != Family::Master) for (unsigned order = 0; order < 3; ++order) {
            TestFx(family, 0, order, false, true);
            ++cases;
        }
    }
    for (auto family : {Family::UserFx2Mixer, Family::UserFx2Sampler}) for (unsigned mode : {1u, 2u}) {
        TestFx(family, 0, 2, false, true, mode);
        ++cases;
    }
    for (auto family : {Family::Legacy, Family::Custom}) {
        TestFx(family, 0, 2, false, true, 0, true);
        ++cases;
    }
    for (auto family : {Family::UserFx2Mixer, Family::UserFx2Sampler}) {
        TestFx(family, SCE_NGS2_VOICE_EVENT_PAUSE, 2, false, false, 0, false, true);
        ++cases;
    }
    std::printf("NGS2 worker effects: %u scenarios passed\n", cases);
    return 0;
}
