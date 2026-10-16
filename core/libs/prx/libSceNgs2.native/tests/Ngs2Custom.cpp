#include "Ngs2Test.hpp"

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

struct Calls {
    std::vector<std::uint32_t> setupVoices;
    std::vector<std::uint32_t> cleanupVoices;
    std::vector<std::uint32_t> processFlags;
    std::vector<int> order;
    int processResult = SCE_NGS2_OK;
};
static Calls calls;

static std::int32_t APS5_VABI Setup(Ngs2UserFx2SetupContext* context) {
    Require(context->common != nullptr && context->param != nullptr && context->work != nullptr);
    Require(context->user_data == 0x77 && context->max_voices == 2);
    std::memset(context->work, 0x5a, 8);
    *static_cast<float*>(context->param) = 1.0f;
    *static_cast<float*>(context->common) = 0.25f;
    calls.setupVoices.push_back(context->voice_index);
    return SCE_NGS2_OK;
}

static std::int32_t APS5_VABI Cleanup(Ngs2UserFx2CleanupContext* context) {
    Require(context->user_data == 0x77 && context->max_voices == 2 && static_cast<std::uint8_t*>(context->work)[7] == 0x5a);
    calls.cleanupVoices.push_back(context->voice_index);
    return SCE_NGS2_OK;
}

static std::int32_t APS5_VABI Gain(Ngs2UserFx2ProcessContext* context) {
    Require(context->user_data == 0x77 && context->num_input_channels == 2 && context->num_output_channels == 2);
    Require(context->num_grain_samples == Grain && context->sample_rate == 48000 && context->state != nullptr);
    Require(static_cast<const std::uint8_t*>(context->work)[0] == 0x5a);
    const float gain = *static_cast<const float*>(context->param);
    for (std::uint32_t c = 0; c < 2; c++) {
        for (std::uint32_t i = 0; i < Grain; i++) context->channel_data[c][i] *= gain;
    }
    *static_cast<float*>(context->common) = 0.125f;
    calls.processFlags.push_back(context->flags);
    calls.order.push_back(0);
    return calls.processResult;
}

static std::int32_t APS5_VABI Offset(Ngs2UserFx2ProcessContext* context) {
    const float offset = *static_cast<const float*>(context->common);
    for (std::uint32_t i = 0; i < Grain; i++) context->channel_data[1][i] += offset;
    calls.order.push_back(1);
    return SCE_NGS2_OK;
}

static std::int32_t APS5_VABI ControlHandler(Ngs2UserFx2ControlContext*) {
    return SCE_NGS2_OK;
}

static Ngs2CustomUserFx2ModuleOption Module(Ngs2UserFx2ProcessHandler process) {
    return {{sizeof(Ngs2CustomUserFx2ModuleOption)}, Setup, Cleanup, nullptr, process, sizeof(float), sizeof(float), 8, 0x77};
}

static Ngs2CustomSubmixerRackOption RackOption(const Ngs2CustomUserFx2ModuleOption* modules, std::uint32_t count) {
    Ngs2CustomSubmixerRackOption option{};
    auto& common = option.custom_rack_option.rack_option;
    common.size = sizeof(option);
    common.max_grain_samples = 512;
    common.max_voices = 2;
    common.max_matrices = 1;
    common.max_ports = 1;
    option.custom_rack_option.num_buffers = 1;
    option.custom_rack_option.num_modules = count;
    for (std::uint32_t m = 0; m < count; m++) {
        option.custom_rack_option.module[m].option = &modules[m].custom_module_option;
        option.custom_rack_option.module[m].module_id = SCE_NGS2_CUSTOM_MODULE_ID_USER_FX2;
        option.custom_rack_option.module[m].state_size = 16;
    }
    option.max_channels = 2;
    option.max_inputs = 1;
    return option;
}

template <typename TException>
static bool CreateThrows(const Ngs2CustomSubmixerRackOption& option) {
    Ngs2ContextBufferInfo query{};
    try {
        sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, &option.custom_rack_option.rack_option, &query);
    } catch (const TException&) {
        return true;
    }
    return false;
}

template <typename TException>
static bool CreateSamplerThrows(const Ngs2CustomSamplerRackOption& option) {
    Ngs2ContextBufferInfo query{};
    try {
        sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_CUSTOM_SAMPLER, &option.custom_rack_option.rack_option, &query);
    } catch (const TException&) {
        return true;
    }
    return false;
}

template <typename TException, typename TParam>
static bool ControlThrows(uintptr_t voice, std::uint32_t id, TParam param) {
    param.header = {static_cast<std::uint16_t>(sizeof(TParam)), 0, id};
    try {
        sceNgs2VoiceControl(voice, &param.header);
    } catch (const TException&) {
        return true;
    }
    return false;
}

static uintptr_t CustomRack(uintptr_t system, const Ngs2CustomSubmixerRackOption& option) {
    Ngs2ContextBufferInfo query{};
    Require(sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, &option.custom_rack_option.rack_option, &query) == SCE_NGS2_OK);
    const auto info = Buffer(query);
    uintptr_t rack = 0;
    Require(sceNgs2RackCreate(system, SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, &option.custom_rack_option.rack_option, &info, &rack) == SCE_NGS2_OK);
    return rack;
}

static uintptr_t Sampler(uintptr_t system, const std::vector<std::int16_t>& pcm) {
    const auto voice = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 2, 48000, 0, 0, 0}});
    const Ngs2WaveformBlock block{0, pcm.size() * sizeof(std::int16_t), 0, 0, static_cast<std::uint32_t>(pcm.size() / 2), 0, 0};
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), 0, 1, &block});
    return voice;
}

static std::vector<float> Render(uintptr_t system) {
    std::vector<float> out(Grain * 2, -1.0f);
    const Ngs2RenderBufferInfo info{out.data(), out.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 2};
    Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_OK);
    return out;
}

static void TestUserFxChain() {
    calls = {};
    const auto system = CreateSystem();
    const auto master = Mastering(system, 2);
    const Ngs2CustomUserFx2ModuleOption modules[2] = {Module(Gain), Module(Offset)};
    const auto rack = CustomRack(system, RackOption(modules, 2));
    Require(calls.setupVoices == (std::vector<std::uint32_t>{0, 1, 0, 1}));

    const auto custom = Voice(rack);
    Control(custom, SCE_NGS2_CUSTOM_SUBMIXER_VOICE_PARAM_SETUP, Ngs2CustomSubmixerVoiceSetupParam{{}, 2, 2, 0, 0});
    Patch(custom, master);
    Event(custom, SCE_NGS2_VOICE_EVENT_PLAY);

    std::vector<std::int16_t> pcm(Grain * 2 * 8, 16384);
    const auto sampler = Sampler(system, pcm);
    Patch(sampler, custom);
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);

    auto out = Render(system);
    for (std::uint32_t i = 0; i < Grain; i++) Require(out[i * 2] == 0.5f && out[i * 2 + 1] == 0.75f);
    Require(calls.processFlags == (std::vector<std::uint32_t>{1}) && calls.order == (std::vector<int>{0, 1}));

    const float gain = 0.5f;
    Control(custom, SCE_NGS2_CUSTOM_VOICE_PARAM_USER_FX2 | 0, Ngs2CustomVoiceUserFx2Param{{}, &gain, sizeof(gain)});
    out = Render(system);
    for (std::uint32_t i = 0; i < Grain; i++) Require(out[i * 2] == 0.25f && out[i * 2 + 1] == 0.5f);
    Require(calls.processFlags == (std::vector<std::uint32_t>{1, 2}));
    out = Render(system);
    Require(calls.processFlags == (std::vector<std::uint32_t>{1, 2, 0}));

    Require(ControlThrows<std::invalid_argument>(custom, SCE_NGS2_CUSTOM_VOICE_PARAM_USER_FX2 | 0, Ngs2CustomVoiceUserFx2Param{{}, &gain, sizeof(gain) - 1}));
    Require(ControlThrows<std::invalid_argument>(custom, SCE_NGS2_CUSTOM_VOICE_PARAM_USER_FX2 | 2, Ngs2CustomVoiceUserFx2Param{{}, &gain, sizeof(gain)}));
    Require(ControlThrows<std::runtime_error>(custom, SCE_NGS2_CUSTOM_SUBMIXER_VOICE_PARAM_SETUP, Ngs2CustomSubmixerVoiceSetupParam{{}, 2, 1, 0, 0}));
    Require(ControlThrows<std::runtime_error>(custom, SCE_NGS2_CUSTOM_SUBMIXER_VOICE_PARAM_SETUP, Ngs2CustomSubmixerVoiceSetupParam{{}, 2, 2, 1, 0}));
    Require(ControlThrows<std::invalid_argument>(custom, SCE_NGS2_CUSTOM_SUBMIXER_VOICE_PARAM_SETUP, Ngs2CustomSubmixerVoiceSetupParam{{}, 3, 3, 0, 0}));
    Require(ControlThrows<std::invalid_argument>(sampler, SCE_NGS2_CUSTOM_VOICE_PARAM_USER_FX2 | 0, Ngs2CustomVoiceUserFx2Param{{}, &gain, sizeof(gain)}));

    calls.processResult = -1;
    bool failed = false;
    try {
        Render(system);
    } catch (const std::runtime_error&) {
        failed = true;
    }
    Require(failed);
    calls.processResult = SCE_NGS2_OK;

    Require(sceNgs2RackDestroy(rack, nullptr) == SCE_NGS2_OK);
    Require(calls.cleanupVoices == (std::vector<std::uint32_t>{0, 0, 1, 1}));
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestRackOptions() {
    calls = {};
    const auto system = CreateSystem();
    const Ngs2CustomUserFx2ModuleOption modules[1] = {Module(Gain)};
    const auto good = RackOption(modules, 1);
    Ngs2ContextBufferInfo query{};
    Require(sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, &good.custom_rack_option.rack_option, &query) == SCE_NGS2_OK);

    auto option = good;
    option.custom_rack_option.num_buffers = 2;
    Require(CreateThrows<std::runtime_error>(option));
    option = good;
    option.custom_rack_option.module[0].module_id = 0x10;
    Require(CreateThrows<std::runtime_error>(option));
    option = good;
    option.custom_rack_option.module[0].dest_buffer_id = 1;
    Require(CreateThrows<std::invalid_argument>(option));
    option = good;
    option.custom_rack_option.port[0].source_buffer_id = 1;
    Require(CreateThrows<std::invalid_argument>(option));
    option = good;
    option.custom_rack_option.num_modules = SCE_NGS2_CUSTOM_MAX_MODULES + 1;
    Require(CreateThrows<std::invalid_argument>(option));
    auto noProcess = Module(nullptr);
    option = RackOption(&noProcess, 1);
    Require(CreateThrows<std::invalid_argument>(option));
    auto badSize = Module(Gain);
    badSize.custom_module_option.size = 8;
    option = RackOption(&badSize, 1);
    Require(CreateThrows<std::invalid_argument>(option));
    bool failed = false;
    try {
        sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, nullptr, &query);
    } catch (const std::runtime_error&) {
        failed = true;
    }
    Require(failed && calls.setupVoices.empty());

    auto withControl = Module(Gain);
    withControl.control_handler = ControlHandler;
    const auto voice = Voice(CustomRack(system, RackOption(&withControl, 1)));
    const float gain = 0.5f;
    Require(ControlThrows<std::runtime_error>(voice, SCE_NGS2_CUSTOM_VOICE_PARAM_USER_FX2 | 0, Ngs2CustomVoiceUserFx2Param{{}, &gain, sizeof(gain)}));
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    Require(calls.cleanupVoices == (std::vector<std::uint32_t>{0, 1}));
}

static void TestCustomSampler() {
    calls = {};
    const auto system = CreateSystem();
    const auto master = Mastering(system, 2);
    const Ngs2CustomUserFx2ModuleOption modules[2] = {Module(Gain), Module(Offset)};

    Ngs2CustomSamplerRackOption option{};
    auto& common = option.custom_rack_option.rack_option;
    common.size = sizeof(option);
    common.max_grain_samples = 512;
    common.max_voices = 2;
    common.max_matrices = 1;
    common.max_ports = 1;
    option.custom_rack_option.num_buffers = 1;
    option.custom_rack_option.num_modules = 2;
    for (std::uint32_t m = 0; m < 2; m++) {
        option.custom_rack_option.module[m].option = &modules[m].custom_module_option;
        option.custom_rack_option.module[m].module_id = SCE_NGS2_CUSTOM_MODULE_ID_USER_FX2;
        option.custom_rack_option.module[m].state_size = 16;
    }
    option.max_channel_works = 4;

    Ngs2ContextBufferInfo query{};
    Require(sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_CUSTOM_SAMPLER, &option.custom_rack_option.rack_option, &query) == SCE_NGS2_OK);
    const auto info = Buffer(query);
    uintptr_t rack = 0;
    Require(sceNgs2RackCreate(system, SCE_NGS2_RACK_ID_CUSTOM_SAMPLER, &option.custom_rack_option.rack_option, &info, &rack) == SCE_NGS2_OK);

    Ngs2RackInfo rackInfo{};
    Require(sceNgs2RackGetInfo(rack, &rackInfo, sizeof(rackInfo)) == SCE_NGS2_OK);
    Require(rackInfo.rack_id == SCE_NGS2_RACK_ID_CUSTOM_SAMPLER && rackInfo.type == 4 && rackInfo.max_channel_works == 4);

    const auto voice = Voice(rack);
    Control(voice, SCE_NGS2_CUSTOM_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 2, 48000, 0, 0, 0}});
    std::vector<std::int16_t> pcm(Grain * 2 * 4, 16384);
    const Ngs2WaveformBlock block{0, pcm.size() * sizeof(std::int16_t), 1, 0, static_cast<std::uint32_t>(pcm.size() / 2), 0, 0};
    Control(voice, SCE_NGS2_CUSTOM_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), 0, 1, &block});
    Control(voice, SCE_NGS2_CUSTOM_SAMPLER_VOICE_PARAM_PITCH, Ngs2SamplerVoicePitchParam{{}, 1.0f});
    const Ngs2VoiceParamHeader exitLoop{sizeof(Ngs2VoiceParamHeader), 0, SCE_NGS2_CUSTOM_SAMPLER_VOICE_PARAM_EXIT_LOOP};
    Require(sceNgs2VoiceControl(voice, &exitLoop) == SCE_NGS2_OK);
    Patch(voice, master);
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);

    const auto out = Render(system);
    for (std::uint32_t i = 0; i < Grain; i++) Require(out[i * 2] == 0.5f && out[i * 2 + 1] == 0.75f);

    Ngs2CustomSamplerVoiceState state{};
    Require(sceNgs2VoiceGetState(voice, &state.voice_state, sizeof(Ngs2SamplerVoiceState)) == SCE_NGS2_ERROR_INVALID_OUT_SIZE);
    Require(sceNgs2VoiceGetState(voice, &state.voice_state, sizeof(state)) == SCE_NGS2_OK);
    Require(state.voice_state.state_flags == (SCE_NGS2_VOICE_STATE_FLAG_INUSE | SCE_NGS2_VOICE_STATE_FLAG_PLAYING) && state.num_decoded_samples == Grain);
    Render(system);
    Require(sceNgs2VoiceGetState(voice, &state.voice_state, sizeof(state)) == SCE_NGS2_OK && state.num_decoded_samples == Grain * 2);

    Require(sceNgs2RackDestroy(rack, nullptr) == SCE_NGS2_OK);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestCustomFilter() {
    const auto system = CreateSystem();
    const auto master = Mastering(system, 1);

    Ngs2CustomSamplerRackOption option{};
    option.custom_rack_option.rack_option.size = sizeof(option);
    option.custom_rack_option.rack_option.max_grain_samples = 512;
    option.custom_rack_option.rack_option.max_voices = 1;
    option.custom_rack_option.rack_option.max_matrices = 1;
    option.custom_rack_option.rack_option.max_ports = 1;
    option.custom_rack_option.num_buffers = 1;
    option.custom_rack_option.num_modules = 1;
    option.custom_rack_option.module[0].module_id = SCE_NGS2_CUSTOM_MODULE_ID_FILTER;
    option.max_channel_works = 1;

    auto invalid = option;
    invalid.custom_rack_option.module[0].state_size = 4;
    Require(CreateSamplerThrows<std::invalid_argument>(invalid));
    invalid = option;
    invalid.custom_rack_option.module[0].option = reinterpret_cast<const Ngs2CustomModuleOption*>(1);
    Require(CreateSamplerThrows<std::invalid_argument>(invalid));

    Ngs2ContextBufferInfo query{};
    Require(sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_CUSTOM_SAMPLER, &option.custom_rack_option.rack_option, &query) == SCE_NGS2_OK);
    const auto info = Buffer(query);
    uintptr_t rack = 0;
    Require(sceNgs2RackCreate(system, SCE_NGS2_RACK_ID_CUSTOM_SAMPLER, &option.custom_rack_option.rack_option, &info, &rack) == SCE_NGS2_OK);

    const auto voice = Voice(rack);
    Control(voice, SCE_NGS2_CUSTOM_SAMPLER_VOICE_PARAM_SETUP,
            Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 1, 48000, 0, 0, 0}});
    const std::vector<std::int16_t> pcm(Grain, 16384);
    const Ngs2WaveformBlock block{0, pcm.size() * sizeof(std::int16_t), 0, 0, static_cast<std::uint32_t>(pcm.size()), 0, 0};
    Control(voice, SCE_NGS2_CUSTOM_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS,
            Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), 0, 1, &block});

    Ngs2CustomVoiceFilterParam filter{};
    filter.type = 1;
    filter.param.fcq.frequency = 10000.0f;
    filter.param.fcq.q = 0.70710678f;
    filter.param.fcq.level = 1.0f;
    Control(voice, SCE_NGS2_CUSTOM_VOICE_PARAM_FILTER, filter);
    Require(ControlThrows<std::invalid_argument>(voice, SCE_NGS2_CUSTOM_VOICE_PARAM_FILTER | 1, filter));
    Patch(voice, master);
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);

    std::vector<float> out(Grain, -1.0f);
    const Ngs2RenderBufferInfo renderInfo{out.data(), out.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 1};
    Require(sceNgs2SystemRender(system, &renderInfo, 1) == SCE_NGS2_OK);
    Require(out[0] > 0.0f && out[0] < 0.2f && out.back() > out[0]);

    Ngs2CustomVoiceFilterParam unsupported{};
    unsupported.type = 2;
    Require(ControlThrows<std::runtime_error>(voice, SCE_NGS2_CUSTOM_VOICE_PARAM_FILTER, unsupported));
    Control(voice, SCE_NGS2_CUSTOM_VOICE_PARAM_FILTER, Ngs2CustomVoiceFilterParam{});
    Control(voice, SCE_NGS2_CUSTOM_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS,
            Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), SCE_NGS2_WAVEFORM_BLOCKS_FLAG_RESET, 1, &block});
    Require(sceNgs2SystemRender(system, &renderInfo, 1) == SCE_NGS2_OK);
    Require(out[0] > 0.4f);
    Require(sceNgs2RackDestroy(rack, nullptr) == SCE_NGS2_OK);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}


struct LegacyUserFxState {
    std::uint32_t calls = 0;
    std::uint32_t flags = 0;
    std::int32_t result = SCE_NGS2_OK;
    float val0 = 0.25f;
    float val1 = 0.5f;
};

static std::int32_t APS5_VABI ProcessLegacy(Ngs2UserFxProcessContext* context) {
    auto& state = *reinterpret_cast<LegacyUserFxState*>(context->user_data0);
    state.calls++;
    state.flags = context->flags;
    if (state.result != SCE_NGS2_OK) return state.result;
    for (std::uint32_t i = 0; i < context->num_grain_samples; i++) {
        context->channel_data[0][i] = state.val0;
        context->channel_data[1][i] = state.val1;
    }
    return SCE_NGS2_OK;
}

static Ngs2CustomSubmixerRackOption LegacyRackOption(const Ngs2CustomModuleOption* option) {
    Ngs2CustomSubmixerRackOption rack{};
    auto& common = rack.custom_rack_option.rack_option;
    common.size = sizeof(rack);
    common.max_grain_samples = 512;
    common.max_voices = 2;
    common.max_matrices = 1;
    common.max_ports = 1;
    rack.custom_rack_option.num_buffers = 1;
    rack.custom_rack_option.num_modules = 1;
    rack.custom_rack_option.module[0].option = option;
    rack.custom_rack_option.module[0].module_id = SCE_NGS2_CUSTOM_MODULE_ID_USER_FX;
    rack.custom_rack_option.module[0].state_size = 16;
    rack.max_channels = 2;
    rack.max_inputs = 1;
    return rack;
}

static void TestCustomLegacyUserFx() {
    const auto system = CreateSystem();
    const auto master = Mastering(system, 2);
    const Ngs2CustomModuleOption header{sizeof(Ngs2CustomModuleOption)};
    const auto rack = CustomRack(system, LegacyRackOption(&header));
    const auto custom = Voice(rack);
    Control(custom, SCE_NGS2_CUSTOM_SUBMIXER_VOICE_PARAM_SETUP, Ngs2CustomSubmixerVoiceSetupParam{{}, 2, 2, 0, 0});
    Patch(custom, master);
    Event(custom, SCE_NGS2_VOICE_EVENT_PLAY);

    LegacyUserFxState state{};
    Control(custom, SCE_NGS2_CUSTOM_VOICE_PARAM_USER_FX | 0,
            Ngs2CustomVoiceUserFxParam{{}, ProcessLegacy, reinterpret_cast<std::uintptr_t>(&state), 123, 456});

    auto out = Render(system);
    Require(state.calls == 1 && state.flags == 1);
    for (std::uint32_t i = 0; i < Grain; i++) Require(out[i * 2] == 0.25f && out[i * 2 + 1] == 0.5f);

    out = Render(system);
    Require(state.calls == 2 && state.flags == 0);
    for (std::uint32_t i = 0; i < Grain; i++) Require(out[i * 2] == 0.25f && out[i * 2 + 1] == 0.5f);

    struct BadSizeParam {
        Ngs2VoiceParamHeader header;
        std::uint8_t dummy[10];
    };
    Require(ControlThrows<std::invalid_argument>(custom, SCE_NGS2_CUSTOM_VOICE_PARAM_USER_FX | 0, BadSizeParam{}));
    Require(ControlThrows<std::invalid_argument>(custom, SCE_NGS2_CUSTOM_VOICE_PARAM_USER_FX | 2, Ngs2CustomVoiceUserFxParam{{}, ProcessLegacy, 0, 0, 0}));

    Ngs2CustomModuleOption badSize{2};
    Require(CreateThrows<std::invalid_argument>(LegacyRackOption(&badSize)));

    struct ExtendedOption {
        std::uint32_t size = 20;
        std::uint32_t reserved = 0;
        std::uint32_t channels = 4;
        float param0 = 1000.0f;
        float param1 = 0.0f;
    } extendedOption;
    const auto rackExtended = CustomRack(system, LegacyRackOption(reinterpret_cast<const Ngs2CustomModuleOption*>(&extendedOption)));
    Require(sceNgs2RackDestroy(rackExtended, nullptr) == SCE_NGS2_OK);

    state.result = -1;
    bool failed = false;
    try {
        Render(system);
    } catch (const std::runtime_error&) {
        failed = true;
    }
    Require(failed);
    state.result = SCE_NGS2_OK;

    Require(sceNgs2RackDestroy(rack, nullptr) == SCE_NGS2_OK);

    const auto rackNullOpt = CustomRack(system, LegacyRackOption(nullptr));
    const auto voiceNullOpt = Voice(rackNullOpt);
    Control(voiceNullOpt, SCE_NGS2_CUSTOM_SUBMIXER_VOICE_PARAM_SETUP, Ngs2CustomSubmixerVoiceSetupParam{{}, 2, 2, 0, 0});
    Patch(voiceNullOpt, master);
    Event(voiceNullOpt, SCE_NGS2_VOICE_EVENT_PLAY);

    LegacyUserFxState state2{};
    state2.val0 = 0.125f;
    state2.val1 = 0.75f;
    Control(voiceNullOpt, SCE_NGS2_CUSTOM_VOICE_PARAM_USER_FX | 0,
            Ngs2CustomVoiceUserFxParam{{}, ProcessLegacy, reinterpret_cast<std::uintptr_t>(&state2), 123, 456});
    out = Render(system);
    Require(state2.calls == 1 && state2.flags == 1);
    for (std::uint32_t i = 0; i < Grain; i++) Require(out[i * 2] == 0.125f && out[i * 2 + 1] == 0.75f);

    Require(sceNgs2RackDestroy(rackNullOpt, nullptr) == SCE_NGS2_OK);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

int main() {
    TestUserFxChain();
    TestRackOptions();
    TestCustomSampler();
    TestCustomFilter();
    TestCustomLegacyUserFx();
    return 0;
}
