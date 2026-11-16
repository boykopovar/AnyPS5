#include "Ngs2Test.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <vector>

static std::int32_t APS5_VABI Generate(Ngs2UserFxProcessContext* context) {
    const auto* signal = reinterpret_cast<const float*>(context->user_data0);
    for (std::uint32_t c = 0; c < context->num_channels; c++)
        std::fill_n(context->channel_data[c], context->num_grain_samples, signal[c]);
    return SCE_NGS2_OK;
}

static Ngs2CustomSubmixerRackOption MixerOption(std::uint32_t channels, std::uint32_t modules, const Ngs2CustomModuleOption* moduleOption, std::uint32_t stateSize) {
    Ngs2CustomSubmixerRackOption mixerOption{};
    auto& custom = mixerOption.custom_rack_option;
    custom.rack_option.size = sizeof(mixerOption);
    custom.rack_option.max_grain_samples = 512;
    custom.rack_option.max_voices = 1;
    custom.rack_option.max_matrices = 1;
    custom.rack_option.max_ports = 1;
    custom.num_buffers = 1;
    custom.state_size = 16;
    custom.num_modules = modules;
    for (std::uint32_t m = 0; m < modules; m++) {
        custom.module[m].module_id = SCE_NGS2_CUSTOM_MODULE_ID_CHANNEL_MIXER;
        custom.module[m].option = moduleOption;
        custom.module[m].state_size = stateSize;
    }
    mixerOption.max_channels = channels;
    mixerOption.max_inputs = 1;
    return mixerOption;
}

static int CreateMixerRack(std::uint32_t channels, std::uint32_t modules, const Ngs2CustomModuleOption* moduleOption, std::uint32_t stateSize) {
    const auto system = CreateSystem();
    auto option = MixerOption(channels, modules, moduleOption, stateSize);
    Ngs2ContextBufferInfo info{};
    uintptr_t rack = 0;
    const int result = sceNgs2RackCreate(system, SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, &option.custom_rack_option.rack_option, &info, &rack);
    sceNgs2SystemDestroy(system, nullptr);
    return result;
}

static void RequireMixerRackCreates(const Ngs2CustomModuleOption* moduleOption) {
    const auto system = CreateSystem();
    auto option = MixerOption(2, 1, moduleOption, 0);
    Ngs2ContextBufferInfo query{};
    Require(sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, &option.custom_rack_option.rack_option, &query) == SCE_NGS2_OK);
    const auto info = Buffer(query);
    uintptr_t rack = 0;
    Require(sceNgs2RackCreate(system, SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, &option.custom_rack_option.rack_option, &info, &rack) == SCE_NGS2_OK);
    Require(rack != 0);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

struct MixerGraph {
    std::uint32_t channels;
    uintptr_t system = CreateSystem();
    std::array<uintptr_t, 2> voices{};
    std::array<float, 8> signal{0.5f, -0.25f, 0.125f, -0.0625f, 0.03125f, -0.015625f, 0.0078125f, -0.00390625f};

    MixerGraph(std::uint32_t channelCount = 2, std::uint32_t modules = 1, std::uint32_t voiceCount = 1) : channels(channelCount) {
        const auto master = Mastering(system, channels);
        auto mixerOption = MixerOption(channels, modules, nullptr, 0);
        auto& custom = mixerOption.custom_rack_option;
        custom.rack_option.max_voices = voiceCount;
        Ngs2ContextBufferInfo query{};
        Require(sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, &custom.rack_option, &query) == SCE_NGS2_OK);
        const auto buffer = Buffer(query);
        uintptr_t rack = 0;
        Require(sceNgs2RackCreate(system, SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, &custom.rack_option, &buffer, &rack) == SCE_NGS2_OK);
        for (std::uint32_t v = 0; v < voiceCount; v++) {
            Require(sceNgs2RackGetVoiceHandle(rack, v, &voices[v]) == SCE_NGS2_OK);
            Control(voices[v], SCE_NGS2_CUSTOM_SUBMIXER_VOICE_PARAM_SETUP,
                    Ngs2CustomSubmixerVoiceSetupParam{{}, channels, channels, 0, 0});
            const auto source = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SUBMIXER));
            Control(source, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, channels, 0});
            Control(source, SCE_NGS2_SUBMIXER_VOICE_PARAM_USER_FX,
                    Ngs2SubmixerVoiceUserFxParam{{}, Generate, reinterpret_cast<std::uintptr_t>(signal.data()), 0, 0});
            Patch(source, voices[v]);
            Event(source, SCE_NGS2_VOICE_EVENT_PLAY);
            Patch(voices[v], master);
            Event(voices[v], SCE_NGS2_VOICE_EVENT_PLAY);
        }
    }

    ~MixerGraph() { Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK); }

    void Set(const Ngs2CustomVoiceChannelMixerParam& param, std::uint32_t index = 0, std::uint32_t voice = 0) {
        Control(voices[voice], SCE_NGS2_CUSTOM_VOICE_PARAM_CHANNEL_MIXER | index, param);
    }

    std::vector<float> Render(std::uint32_t grain = Grain) {
        Require(sceNgs2SystemSetGrainSamples(system, grain) == SCE_NGS2_OK);
        std::vector<float> out(grain * channels, -123.0f);
        const Ngs2RenderBufferInfo info{out.data(), out.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, channels};
        Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_OK);
        return out;
    }
};

static void ExpectMix(const MixerGraph& graph, const std::vector<float>& out, const Ngs2CustomVoiceChannelMixerParam& param) {
    Require(out.size() == Grain * graph.channels);
    for (std::size_t i = 0; i < Grain; i++) {
        for (std::uint32_t o = 0; o < graph.channels; o++) {
            float expected = 0.0f;
            for (std::uint32_t c = 0; c < graph.channels; c++)
                expected += std::clamp(param.level[o][c], -32.0f, 32.0f) * graph.signal[c];
            const float actual = out[i * graph.channels + o];
            if (std::isnan(expected)) Require(std::isnan(actual));
            else Require(actual == expected);
        }
    }
}

static void TestIdentityPassthrough() {
    MixerGraph graph(4);
    const auto out = graph.Render();
    for (std::size_t i = 0; i < Grain; i++)
        for (std::uint32_t c = 0; c < graph.channels; c++) Require(out[i * graph.channels + c] == graph.signal[c]);
}

static void TestMix() {
    MixerGraph graph(2);
    Ngs2CustomVoiceChannelMixerParam param{};
    param.level[0][0] = 0.5f;
    param.level[0][1] = 1.0f;
    param.level[1][0] = -2.0f;
    param.level[1][1] = 0.25f;
    graph.Set(param);
    ExpectMix(graph, graph.Render(), param);
}

static void TestNoRamp() {
    MixerGraph graph(2);
    Ngs2CustomVoiceChannelMixerParam param{};
    param.level[0][0] = 8.0f;
    param.level[1][1] = -8.0f;
    graph.Set(param);
    ExpectMix(graph, graph.Render(), param);
    ExpectMix(graph, graph.Render(), param);
}

static void TestClampAndNaN() {
    MixerGraph graph(2);
    Ngs2CustomVoiceChannelMixerParam param{};
    param.level[0][0] = 64.0f;
    param.level[0][1] = -64.0f;
    param.level[1][0] = std::numeric_limits<float>::quiet_NaN();
    param.level[1][1] = 0.0f;
    graph.Set(param);
    ExpectMix(graph, graph.Render(), param);
}

static void TestShortParamSize() {
    MixerGraph graph(2);
    Ngs2CustomVoiceChannelMixerParam param{};
    param.header = {sizeof(Ngs2VoiceParamHeader) + 2 * sizeof(float), 0, SCE_NGS2_CUSTOM_VOICE_PARAM_CHANNEL_MIXER};
    param.level[0][0] = 2.0f;
    param.level[0][1] = -1.0f;
    Require(sceNgs2VoiceControl(graph.voices[0], &param.header) == SCE_NGS2_OK);
    ExpectMix(graph, graph.Render(), param);
}

static void TestParameterPositions() {
    MixerGraph graph(1);
    Ngs2CustomVoiceChannelMixerParam param{};
    param.header = {sizeof(param), 0, 0};
    param.level[0][0] = 4.0f;
    for (std::uint32_t position = 1; position < 256; position++) {
        param.header.id = SCE_NGS2_CUSTOM_VOICE_PARAM_CHANNEL_MIXER | position;
        Require(sceNgs2VoiceControl(graph.voices[0], &param.header) == SCE_NGS2_ERROR_INVALID_PARAM_ID);
    }
    param.header.id = SCE_NGS2_CUSTOM_VOICE_PARAM_CHANNEL_MIXER;
    Require(sceNgs2VoiceControl(graph.voices[0], &param.header) == SCE_NGS2_OK);
    ExpectMix(graph, graph.Render(), param);
}

static void TestModuleIndex() {
    MixerGraph graph(2, 2);
    Ngs2CustomVoiceChannelMixerParam silence{};
    graph.Set(silence, 1);
    const auto out = graph.Render();
    for (const auto sample : out) Require(sample == 0.0f);
}

static void TestOptionSizes() {
    for (const std::uint32_t size : {4u, 8u, 16u}) {
        const std::array<std::uint32_t, 4> storage{size, 0, 0, 0};
        RequireMixerRackCreates(reinterpret_cast<const Ngs2CustomModuleOption*>(storage.data()));
    }
    for (const std::uint32_t size : {12u, 20u, 0x100u}) {
        const std::array<std::uint32_t, 5> storage{size, 0, 0, 0, 0};
        const auto* moduleOption = reinterpret_cast<const Ngs2CustomModuleOption*>(storage.data());
        Require(CreateMixerRack(2, 1, moduleOption, 0) == SCE_NGS2_ERROR_INVALID_OPTION_SIZE);
    }
}

static void TestModuleStateSize() {
    Require(CreateMixerRack(2, 1, nullptr, 8) == SCE_NGS2_ERROR_INVALID_STATE_SIZE);
    RequireMixerRackCreates(nullptr);
}

int main() {
    TestIdentityPassthrough();
    TestMix();
    TestNoRamp();
    TestClampAndNaN();
    TestShortParamSize();
    TestParameterPositions();
    TestModuleIndex();
    TestOptionSizes();
    TestModuleStateSize();
    return 0;
}
