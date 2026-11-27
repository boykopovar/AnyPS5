#include "prx/libSceNgs2.native/tests/Ngs2Test.hpp"
#ifdef APS5_NGS2_PS4_LIBRARY
#include "prx/libSceNgs2/include/Api.hpp"
#include "tests/ProcessParametersFixture.hpp"
#include <cerrno>
#endif

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <numbers>
#include <utility>
#include <vector>

static Ngs2CustomSubmixerRackOption RackOption(std::uint32_t count, std::uint32_t channels = 2) {
    Ngs2CustomSubmixerRackOption option{};
    auto& custom = option.custom_rack_option;
    custom.rack_option.size = sizeof(option);
    custom.rack_option.max_grain_samples = 512;
    custom.rack_option.max_voices = 2;
    custom.rack_option.max_matrices = 1;
    custom.rack_option.max_ports = count;
    custom.num_buffers = count;
    custom.state_size = 16;
    for (std::uint32_t port = 0; port < count; port++) custom.port[port].source_buffer_id = port;
    option.max_channels = channels;
    option.max_inputs = count == 1 ? 1 : 2;
    return option;
}

static void AddGain(Ngs2CustomSubmixerRackOption& option, std::uint32_t source, std::uint32_t dest) {
    auto& custom = option.custom_rack_option;
    custom.num_modules = 1;
    custom.module[0].module_id = SCE_NGS2_CUSTOM_MODULE_ID_GAIN;
    custom.module[0].source_buffer_id = source;
    custom.module[0].dest_buffer_id = dest;
    custom.module[0].extra_buffer_id = custom.num_buffers == 1 ? 0 : 1;
}

static uintptr_t CustomRack(uintptr_t system, const Ngs2CustomSubmixerRackOption& option) {
    Ngs2ContextBufferInfo query{};
    Require(sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, &option.custom_rack_option.rack_option, &query) == SCE_NGS2_OK);
    const auto info = Buffer(query);
    uintptr_t rack = 0;
    Require(sceNgs2RackCreate(system, SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, &option.custom_rack_option.rack_option, &info, &rack) == SCE_NGS2_OK);
    return rack;
}

static uintptr_t CustomVoice(uintptr_t rack, std::uint32_t index = 0, std::uint32_t channels = 2) {
    uintptr_t voice = 0;
    Require(sceNgs2RackGetVoiceHandle(rack, index, &voice) == SCE_NGS2_OK);
    Control(voice, SCE_NGS2_CUSTOM_SUBMIXER_VOICE_PARAM_SETUP, Ngs2CustomSubmixerVoiceSetupParam{{}, channels, channels, 0, 0});
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    return voice;
}

static void SetGain(uintptr_t voice, float level) {
    Ngs2CustomVoiceGainParam param{};
    for (auto& gain : param.gain) gain = level;
    Control(voice, SCE_NGS2_CUSTOM_VOICE_PARAM_GAIN, param);
}

static void Route(uintptr_t source, std::uint32_t port, uintptr_t dest, std::uint32_t input = 0, std::uint32_t channels = 2) {
    std::array<float, 64> identity{};
    for (std::uint32_t channel = 0; channel < channels; channel++) identity[channel * channels + channel] = 1.0f;
    Control(source, SCE_NGS2_VOICE_PARAM_MATRIX_LEVELS, Ngs2VoiceMatrixLevelsParam{{}, 0, channels * channels, identity.data()});
    Control(source, SCE_NGS2_VOICE_PARAM_PORT_MATRIX, Ngs2VoicePortMatrixParam{{}, port, 0});
    Control(source, SCE_NGS2_VOICE_PARAM_PATCH, Ngs2VoicePatchParam{{}, port, input, dest});
    Ngs2VoicePortInfo info{};
    Require(sceNgs2VoiceGetPortInfo(source, port, &info, sizeof(info)) == SCE_NGS2_OK);
    Require(info.dest_handle == dest && info.dest_input_id == input);
}

static void Output(uintptr_t system, uintptr_t source, std::uint32_t port, std::uint32_t output, std::uint32_t channels = 2) {
    const auto master = Mastering(system, channels);
    Control(master, SCE_NGS2_MASTERING_VOICE_PARAM_OUTPUT, Ngs2MasteringVoiceOutputParam{{}, output, 0});
    Route(source, port, master, 0, channels);
}

static uintptr_t Input(uintptr_t system, const std::vector<std::int16_t>& pcm, std::uint32_t channels = 2) {
    const auto voice = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP,
            Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, channels, 48000, 0, 0, 0}});
    const Ngs2WaveformBlock block{0, pcm.size() * sizeof(std::int16_t), 0, 0, static_cast<std::uint32_t>(pcm.size() / channels), 0, 0};
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), 0, 1, &block});
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    return voice;
}

static void Render(uintptr_t system, const std::vector<float>& expected, std::uint32_t channels = 2, std::uint32_t grain = Grain, bool startup = false, const std::vector<float>& scale = {}) {
    Require(sceNgs2SystemSetGrainSamples(system, grain) == SCE_NGS2_OK);
    std::vector<std::vector<float>> samples(expected.size(), std::vector<float>(grain * channels, 9.0f));
    std::vector<Ngs2RenderBufferInfo> info;
    for (auto& output : samples) info.push_back({output.data(), output.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, channels});
    Require(sceNgs2SystemRender(system, info.data(), static_cast<std::uint32_t>(info.size())) == SCE_NGS2_OK);
    for (std::size_t port = 0; port < expected.size(); port++) {
        for (std::size_t i = 0; i < samples[port].size(); ++i) {
            const float value = startup && i / channels < 3 ? 0.0f : expected[port] * (scale.empty() ? 1.0f : scale[i % channels]);
            Require(std::fabs(samples[port][i] - value) <= 0.000001f);
        }
    }
}

static void TestNoModules(std::uint32_t count, std::vector<float> expected) {
    const auto system = CreateSystem();
    auto option = RackOption(count);
    for (std::uint32_t port = 0; port < count; port++) option.custom_rack_option.port[port].source_buffer_id = count - port - 1;
    const auto custom = CustomVoice(CustomRack(system, option));
    for (std::uint32_t port = 0; port < count; port++) Output(system, custom, port, port);
    const std::vector<std::int16_t> pcm0(Grain * 2 * 16, 8192);
    const std::vector<std::int16_t> pcm1(Grain * 2 * 16, -24576);
    const auto input0 = Input(system, pcm0);
    Route(input0, 0, custom);
    uintptr_t input1 = 0;
    if (count > 1) {
        input1 = Input(system, pcm1);
        Route(input1, 0, custom, 1);
    }
    Render(system, expected, 2, Grain, true);
    if (count > 1) {
        Route(input1, 0, custom, 0);
        expected[count - 1] = -0.5f;
        expected[count - 2] = 0.0f;
        Render(system, expected);
        Route(input1, 0, 0);
        expected[count - 1] = 0.25f;
        Render(system, expected);
    }
    Route(input0, 0, 0);
    Render(system, std::vector<float>(count, 0.0f));
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestGainRouting() {
    struct Case {
        std::uint32_t count, source, dest;
        std::vector<float> expected;
    };
    const Case cases[] = {
        {1, 0, 0, {0.5f}},
        {2, 0, 1, {0.25f, 0.5f}},
        {2, 1, 0, {-1.5f, -0.75f}},
        {3, 0, 2, {0.25f, -0.75f, 0.5f}},
        {3, 2, 1, {0.25f, 0.0f, 0.0f}},
        {4, 1, 3, {0.25f, -0.75f, 0.0f, -1.5f}},
        {4, 3, 0, {0.0f, -0.75f, 0.0f, 0.0f}},
        {4, 1, 1, {0.25f, -1.5f, 0.0f, 0.0f}},
    };
    for (const auto& test : cases) {
        const auto system = CreateSystem();
        auto option = RackOption(test.count);
        AddGain(option, test.source, test.dest);
        const auto custom = CustomVoice(CustomRack(system, option));
        SetGain(custom, 2.0f);
        for (std::uint32_t port = 0; port < test.count; port++) Output(system, custom, port, port);
        const std::vector<float> silence(test.count, 0.0f);
        Render(system, silence);
        const std::vector<std::int16_t> pcm0(Grain * 2 * 16, 8192);
        const std::vector<std::int16_t> pcm1(Grain * 2 * 16, -24576);
        const auto input0 = Input(system, pcm0);
        Route(input0, 0, custom);
        uintptr_t input1 = 0;
        if (test.count > 1) {
            input1 = Input(system, pcm1);
            Route(input1, 0, custom, 1);
        }
        Render(system, test.expected, 2, Grain, true);
        Render(system, test.expected, 2, 64);
        Route(input0, 0, 0);
        if (input1 != 0) Route(input1, 0, 0);
        Render(system, silence);
        Render(system, silence);
        Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    }
}

static void TestUnityCopy() {
    const auto system = CreateSystem();
    auto option = RackOption(2);
    AddGain(option, 0, 1);
    const auto custom = CustomVoice(CustomRack(system, option));
    SetGain(custom, 1.0f);
    Output(system, custom, 0, 0);
    Output(system, custom, 1, 1);
    Render(system, {0.0f, 0.0f});
    std::vector<std::int16_t> source(Grain * 2), dest(Grain * 2);
    for (std::uint32_t i = 0; i < Grain; i++) {
        source[i * 2] = 8192;
        source[i * 2 + 1] = -16384;
        dest[i * 2] = -24576;
        dest[i * 2 + 1] = 4096;
    }
    Route(Input(system, source), 0, custom);
    Route(Input(system, dest), 0, custom, 1);
    std::array<std::array<float, Grain * 2>, 2> outputs{};
    std::array<Ngs2RenderBufferInfo, 2> info{};
    for (std::size_t i = 0; i < outputs.size(); i++) {
        info[i] = {outputs[i].data(), sizeof(outputs[i]), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 2};
    }
    Require(sceNgs2SystemRender(system, info.data(), info.size()) == SCE_NGS2_OK);
    for (const auto& output : outputs) {
        for (std::uint32_t i = 0; i < Grain; i++) Require(output[i * 2] == (i < 3 ? 0.0f : 0.25f) && output[i * 2 + 1] == (i < 3 ? 0.0f : -0.5f));
    }
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

#ifdef APS5_NGS2_PS4_LIBRARY
static constexpr bool Ps4Library = true;
#else
static constexpr bool Ps4Library = false;
#endif

static std::vector<float> MasteringScale(std::uint32_t channels) {
    if (Ps4Library) return {};
    constexpr float half = std::numbers::sqrt2_v<float> / 2.0f;
    if (channels == 3) return {1.0f + half, 1.0f + half, 0.0f};
    if (channels == 5) return {1.0f, 1.0f, 1.0f, 0.0f, 1.0f};
    if (channels == 7) return {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, half};
    return {};
}

static void TestIndependentVoicesAndSilentRamp(std::uint32_t channels) {
    const auto system = CreateSystem();
    const auto scale = MasteringScale(channels);
    auto option = RackOption(4, channels);
    AddGain(option, 0, 3);
    const auto rack = CustomRack(system, option);
    const auto first = CustomVoice(rack, 0, channels);
    const auto second = CustomVoice(rack, 1, channels);
    SetGain(first, 2.0f);
    SetGain(second, -1.0f);
    Output(system, first, 3, 0, channels);
    Output(system, second, 3, 1, channels);
    Render(system, {0.0f, 0.0f}, channels);
    const std::vector<std::int16_t> pcm0(Grain * channels * 16, 8192);
    const std::vector<std::int16_t> pcm1(Grain * channels * 16, -24576);
    const auto input0 = Input(system, pcm0, channels);
    const auto input1 = Input(system, pcm1, channels);
    Route(input0, 0, first, 0, channels);
    Route(input1, 0, second, 0, channels);
    Render(system, {0.5f, 0.75f}, channels, Grain, true, scale);
    Route(input0, 0, 0, 0, channels);
    Render(system, {0.0f, 0.75f}, channels, Grain, false, scale);
    SetGain(first, 4.0f);
    Render(system, {0.0f, 0.75f}, channels, Grain, false, scale);
    Route(input0, 0, first, 0, channels);
    Render(system, {1.0f, 0.75f}, channels, Grain, false, scale);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestSubmixerKeyInput() {
    const auto system = CreateSystem();
    Ngs2SubmixerRackOption option{};
    option.rack_option.size = sizeof(option);
    option.rack_option.max_grain_samples = 512;
    option.rack_option.max_voices = 1;
    option.rack_option.max_matrices = 1;
    option.rack_option.max_ports = 1;
    option.max_channels = 2;
    option.max_envelope_points = 1;
    option.max_inputs = 3;
    option.num_peak_meter_blocks = 1;
    Ngs2ContextBufferInfo query{};
    Require(sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_SUBMIXER, &option.rack_option, &query) == SCE_NGS2_OK);
    const auto info = Buffer(query);
    uintptr_t rack = 0;
    Require(sceNgs2RackCreate(system, SCE_NGS2_RACK_ID_SUBMIXER, &option.rack_option, &info, &rack) == SCE_NGS2_OK);
    uintptr_t submixer = 0;
    Require(sceNgs2RackGetVoiceHandle(rack, 0, &submixer) == SCE_NGS2_OK);
    Control(submixer, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, 2, 0});
    Event(submixer, SCE_NGS2_VOICE_EVENT_PLAY);
    Output(system, submixer, 0, 0);
    const std::vector<std::int16_t> pcm0(Grain * 2 * 16, 8192);
    const std::vector<std::int16_t> pcm1(Grain * 2 * 16, -24576);
    const auto input0 = Input(system, pcm0);
    const auto input1 = Input(system, pcm1);
    Route(input0, 0, submixer);
    Route(input1, 0, submixer, 1);
    Render(system, {0.25f}, 2, Grain, true);
    Route(input1, 0, submixer, 0);
    Render(system, {-0.5f});
    Route(input1, 0, submixer, 1);
    Render(system, {0.25f});
#ifndef APS5_NGS2_PS4_LIBRARY
    Ngs2SubmixerVoiceCompressorParam param{{}, 0x8, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0};
    Control(submixer, SCE_NGS2_SUBMIXER_VOICE_PARAM_COMPRESSOR, param);
    Render(system, {0.25f * 0.5f / 0.75f});
    Route(input0, 1, submixer, 2);
    Render(system, {0.25f * 0.5f / 0.75f});
    param.flags = 0xa;
    Control(submixer, SCE_NGS2_SUBMIXER_VOICE_PARAM_COMPRESSOR, param);
    Render(system, {0.25f * 0.5f / 0.75f});
    param.flags = 0;
    Control(submixer, SCE_NGS2_SUBMIXER_VOICE_PARAM_COMPRESSOR, param);
    Render(system, {0.25f});
    param.flags = 0xa;
    Control(submixer, SCE_NGS2_SUBMIXER_VOICE_PARAM_COMPRESSOR, param);
    Route(input1, 0, 0);
    Render(system, {0.25f});
    Route(input1, 0, submixer, 1);
    param.flags = 0;
    Control(submixer, SCE_NGS2_SUBMIXER_VOICE_PARAM_COMPRESSOR, param);
    Render(system, {0.25f});
    param.flags = 0x8;
    Control(submixer, SCE_NGS2_SUBMIXER_VOICE_PARAM_COMPRESSOR, param);
    Route(input0, 1, submixer, 1);
    Render(system, {0.25f});
    Route(input0, 0, 0);
    Render(system, {0.0f});
    Route(input1, 0, submixer);
    Render(system, {-0.75f});
#endif
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestSubmixerFilterOff() {
    const auto system = CreateSystem();
    Ngs2SubmixerRackOption option{};
    option.rack_option.size = sizeof(option);
    option.rack_option.max_grain_samples = 512;
    option.rack_option.max_voices = 1;
    option.rack_option.max_matrices = 1;
    option.rack_option.max_ports = 1;
    option.max_channels = 2;
    option.max_envelope_points = 1;
    option.max_filters = 2;
    option.max_inputs = 1;
    option.num_peak_meter_blocks = 1;
    Ngs2ContextBufferInfo query{};
    Require(sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_SUBMIXER, &option.rack_option, &query) == SCE_NGS2_OK);
    const auto info = Buffer(query);
    uintptr_t rack = 0;
    Require(sceNgs2RackCreate(system, SCE_NGS2_RACK_ID_SUBMIXER, &option.rack_option, &info, &rack) == SCE_NGS2_OK);
    uintptr_t submixer = 0;
    Require(sceNgs2RackGetVoiceHandle(rack, 0, &submixer) == SCE_NGS2_OK);
    Control(submixer, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, 2, 0});
    Event(submixer, SCE_NGS2_VOICE_EVENT_PLAY);
    Output(system, submixer, 0, 0);
    const std::vector<std::int16_t> pcm(Grain * 2 * 16, 8192);
    Route(Input(system, pcm), 0, submixer);
    for (const std::uint32_t index : {0u, 1u}) {
        Ngs2SamplerVoiceFilterParam filter{};
        filter.index = index;
        filter.q = 1.0f;
        filter.level = 1.0f;
        Control(submixer, SCE_NGS2_SUBMIXER_VOICE_PARAM_FILTER, filter);
    }
    Control(submixer, SCE_NGS2_SUBMIXER_VOICE_PARAM_COMPRESSOR, Ngs2SubmixerVoiceCompressorParam{{}, 1, 0.1f, 0.01f, 0.0f, 0.0f, 0.0f, 1.0f, 0});
    Render(system, {0.25f}, 2, Grain, true);
    Render(system, {0.25f});
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static std::int32_t APS5_VABI Steady(Ngs2UserFxProcessContext* context) {
    const auto* signal = reinterpret_cast<const float*>(context->user_data0);
    for (std::uint32_t c = 0; c < context->num_channels; c++) {
        for (std::uint32_t i = 0; i < context->num_grain_samples; i++) context->channel_data[c][i] = context->user_data1 != 0 && i % 2 != 0 ? 0.0f : signal[c];
    }
    return SCE_NGS2_OK;
}

struct CompressorGraph {
    std::vector<float> signal;
    std::uint32_t channels;
    uintptr_t system = CreateSystem();
    uintptr_t compressor = 0;

    explicit CompressorGraph(std::vector<float> input, bool alternate = false)
        : signal(std::move(input)), channels(static_cast<std::uint32_t>(signal.size())) {
        const auto master = Mastering(system, channels);
        compressor = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SUBMIXER));
        Control(compressor, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, channels, 0});
        const auto source = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SUBMIXER));
        Control(source, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, channels, 0});
        Control(source, SCE_NGS2_SUBMIXER_VOICE_PARAM_USER_FX,
                Ngs2SubmixerVoiceUserFxParam{{}, Steady, reinterpret_cast<std::uintptr_t>(signal.data()), alternate ? 1u : 0u, 0});
        Patch(source, compressor);
        Event(source, SCE_NGS2_VOICE_EVENT_PLAY);
        Patch(compressor, master);
        Event(compressor, SCE_NGS2_VOICE_EVENT_PLAY);
    }

    ~CompressorGraph() { Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK); }

    void Set(std::uint32_t flags, float attack = 0.0f, float level = 1.0f, float knee = 0.0f) {
        Control(compressor, SCE_NGS2_SUBMIXER_VOICE_PARAM_COMPRESSOR, Ngs2SubmixerVoiceCompressorParam{{}, flags, 0.5f, 0.5f, knee, attack, 0.0f, level, 0});
    }

    std::vector<float> Render() {
        std::vector<float> out(Grain * channels, -9.0f);
        const Ngs2RenderBufferInfo info{out.data(), out.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, channels};
        Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_OK);
        return out;
    }

    void Expect(const std::vector<float>& frame, bool alternate = false) {
        const auto out = Render();
        for (std::uint32_t i = 0; i < Grain; i++) {
            for (std::uint32_t c = 0; c < channels; c++) Require(std::fabs(out[i * channels + c] - (alternate && i % 2 != 0 ? 0.0f : frame[c])) <= 0.00001f);
        }
    }
};

#ifdef APS5_NGS2_PS4_LIBRARY
alignas(16) static std::array<std::byte, 0x40> processParameters{};

static void SetCompiledSdkVersion(std::uint32_t version) {
    std::memcpy(processParameters.data() + 0x10, &version, sizeof(version));
}
#endif

static void TestSubmixerCompressor() {
    {
        CompressorGraph graph({0.8f, 0.8f});
        graph.Set(0);
        graph.Expect({0.65f, 0.65f});
        graph.Expect({0.65f, 0.65f});
        graph.Set(0x1);
        graph.Expect({0.8f, 0.8f});
        graph.Set(0, 0.0f, 2.0f);
        graph.Expect({1.3f, 1.3f});
        graph.Set(0x1, 0.0f, 2.0f);
        graph.Expect({0.8f, 0.8f});
    }
    {
        CompressorGraph graph({0.3f, 0.3f});
        graph.Set(0);
        graph.Expect({0.3f, 0.3f});
    }
    {
        CompressorGraph graph({0.8f, 0.8f}, true);
        graph.Set(0x2);
        graph.Expect({0.8f * 0.941942f, 0.8f * 0.941942f}, true);
        graph.Set(0);
        graph.Expect({0.65f, 0.65f}, true);
    }
    {
        CompressorGraph graph({0.8f, 0.2f});
        graph.Set(0);
        graph.Expect({0.65f, 0.1625f});
    }
    {
        CompressorGraph graph({0.2f, 0.2f, 0.2f, 0.8f, 0.2f, 0.2f});
        graph.Set(0);
        graph.Expect({0.1625f, 0.1625f, 0.1625f, 0.65f, 0.1625f, 0.1625f});
    }
    {
        CompressorGraph graph({0.8f, 0.8f});
        graph.Set(0, 0.0f, 1.0f, 0.4f);
        graph.Expect({0.65f, 0.65f});
    }
    {
        CompressorGraph graph({0.3f, 0.3f});
        graph.Set(0, 0.0f, 1.0f, 0.4f);
        graph.Expect({0.3f, 0.3f});
    }
    {
        CompressorGraph graph({0.8f, 0.8f});
        graph.Set(0, 0.01f);
        const auto out = graph.Render();
        const float coefficient = std::exp(-1.0f / 480.0f);
        float gain = 1.0f;
        for (std::uint32_t i = 0; i < 32; i++) {
            gain = coefficient * gain + (1.0f - coefficient) * 0.8125f;
            Require(std::fabs(out[i * 2] - 0.8f * gain) <= 0.00001f);
        }
        Require(std::fabs(gain - 0.987908f) <= 0.00001f);
    }
#ifdef APS5_NGS2_PS4_LIBRARY
    {
        CompressorGraph graph({0.2f, 0.2f, 0.2f, 0.8f, 0.2f, 0.2f});
        graph.Set(0x8);
        graph.Expect({0.2f, 0.2f, 0.2f, 0.65f, 0.2f, 0.2f});
        graph.Set(0xa);
        graph.Expect({0.2f, 0.2f, 0.2f, 0.65f, 0.2f, 0.2f});
    }
    {
        CompressorGraph graph({0.8f, 0.2f});
        graph.Set(0x20);
        graph.Expect({0.65f, 0.1625f});
        graph.Set(0x10);
        graph.Expect({0.65f, 0.1625f});
    }
    {
        CompressorGraph graph({0.45f, 0.45f});
        SetCompiledSdkVersion(0x1700000);
        graph.Set(0, 0.0f, 1.0f, 0.4f);
        graph.Expect({0.44375f, 0.44375f});
        SetCompiledSdkVersion(0x16fffff);
        graph.Expect({0.45f, 0.45f});
    }
#else
    {
        CompressorGraph graph({0.8f, 0.8f});
        graph.Set(0x8);
        graph.Expect({0.65f, 0.65f});
    }
    {
        CompressorGraph graph({0.8f, 0.8f}, true);
        graph.Set(0x4);
        graph.Expect({0.65f, 0.65f}, true);
    }
    {
        CompressorGraph graph({0.8f, 0.2f});
        graph.Set(0x20);
        graph.Expect({0.65f, 0.2f});
        graph.Set(0x10);
        graph.Expect({0.65f, 0.1625f});
    }
    {
        CompressorGraph graph({0.2f, 0.2f, 0.2f, 0.8f, 0.2f, 0.2f});
        graph.Set(0x10);
        graph.Expect({0.2f, 0.2f, 0.2f, 0.65f, 0.2f, 0.2f});
    }
    {
        CompressorGraph graph({0.2f, 0.2f, 0.2f, 0.2f, 0.8f, 0.2f});
        graph.Set(0x10);
        graph.Expect({0.1625f, 0.1625f, 0.1625f, 0.2f, 0.65f, 0.1625f});
    }
    {
        CompressorGraph graph({0.45f, 0.45f});
        graph.Set(0, 0.0f, 1.0f, 0.4f);
        graph.Expect({0.44375f, 0.44375f});
    }
#endif
}

#ifdef APS5_NGS2_PS4_LIBRARY
static std::vector<std::complex<double>> ReferenceDft(const std::vector<float>& input) {
    const std::size_t n = input.size();
    std::vector<std::complex<double>> out(n);
    for (std::size_t k = 0; k < n; ++k) {
        std::complex<double> sum(0.0, 0.0);
        for (std::size_t idx = 0; idx < n; ++idx) {
            const double angle = (-2.0 * std::numbers::pi * static_cast<double>(k) * static_cast<double>(idx)) / static_cast<double>(n);
            sum += static_cast<double>(input[idx]) * std::complex<double>(std::cos(angle), std::sin(angle));
        }
        out[k] = sum;
    }
    return out;
}

static bool ApproxEqual(float actual, float expected, float tolerance) {
    return std::fabs(actual - expected) <= tolerance;
}

static void TestFft() {
    std::uint64_t size = 0;
    std::vector<std::uint8_t> smallWork(64);
    void* handle = nullptr;
    Require(sceNgs2FftInit(smallWork.data(), smallWork.size(), 7, 2, &handle) == -1);
    Require(sceNgs2FftInit(smallWork.data(), smallWork.size(), 13, 2, &handle) == -1);
    Require(sceNgs2FftQuerySize(8, 2, &size) == 0 && size == 1088);
    Require(sceNgs2FftInit(smallWork.data(), size - 1, 8, 2, &handle) == -1);
    Require(sceNgs2FftInit(nullptr, size, 8, 2, &handle) == -1);
    Require(sceNgs2FftInit(smallWork.data(), size, 8, 2, nullptr) == -1);
    errno = 71;
    Require(sceNgs2FftInit(reinterpret_cast<void*>(std::uintptr_t{0x10}), size, 8, 2, &handle) == -1 && errno == 71);
    std::vector<std::uint8_t> initWork(size);
    Require(sceNgs2FftInit(initWork.data(), size, 8, 2, reinterpret_cast<void**>(std::uintptr_t{0x10})) == -1 && errno == 71);

    Require(sceNgs2FftQuerySize(8, 0, &size) == 0 && size == 2112);
    std::vector<std::uint8_t> flags0Work(size);
    handle = nullptr;
    Require(sceNgs2FftInit(flags0Work.data(), size, 8, 0, &handle) == 0 && handle == flags0Work.data());
    std::int32_t readLength = 0;
    std::uint32_t readFlags = 1;
    std::memcpy(&readLength, flags0Work.data() + 0x10, sizeof(readLength));
    std::memcpy(&readFlags, flags0Work.data() + 0x14, sizeof(readFlags));
    Require(readLength == 8 && readFlags == 0);
    std::vector<float> dummyIn(512, 1.0f);
    std::vector<float> scratch0(512);
    for (std::size_t i = 0; i < scratch0.size(); ++i) scratch0[i] = static_cast<float>(i) * 0.25f - 10.0f;
    std::vector<float> out0(512, -1.0f);
    Require(sceNgs2FftProcess(handle, dummyIn.data(), out0.data(), scratch0.data(), 0, 0) == 0);
    Require(out0 == scratch0);

    for (int length = 8; length <= 12; ++length) {
        Require(sceNgs2FftQuerySize(static_cast<std::uint32_t>(length), 2, &size) == 0);
        std::vector<std::uint8_t> fftWork(size);
        void* fftHandle = nullptr;
        Require(sceNgs2FftInit(fftWork.data(), size, length, 2, &fftHandle) == 0 && fftHandle == fftWork.data());
        std::uint64_t p0 = 0;
        std::uint64_t p1 = 0;
        std::memcpy(&p0, fftWork.data() + 0x00, sizeof(p0));
        std::memcpy(&p1, fftWork.data() + 0x08, sizeof(p1));
        std::memcpy(&readLength, fftWork.data() + 0x10, sizeof(readLength));
        std::memcpy(&readFlags, fftWork.data() + 0x14, sizeof(readFlags));
        Require(p0 >= reinterpret_cast<std::uintptr_t>(fftWork.data()) && (p0 & 0x3fu) == 0 && p1 > p0);
        Require(readLength == length && readFlags == 2);

        const std::size_t n = std::size_t{1} << length;
        const std::size_t half = n / 2;
        const std::size_t quarterBins = n / 8;
        std::vector<float> timeIn(n);
        for (std::size_t i = 0; i < n; ++i) {
            timeIn[i] = static_cast<float>(std::sin(0.13 * static_cast<double>(i)) + 0.5 * std::cos(0.37 * static_cast<double>(i)));
        }
        const auto dft = ReferenceDft(timeIn);

        std::vector<float> scratch(n, 0.0f);
        std::vector<float> nat1(n * 2, 0.0f);
        std::vector<float> nat1Null(n * 2, 0.0f);
        Require(sceNgs2FftProcess(fftHandle, timeIn.data(), nat1.data(), scratch.data(), 0, 0x1) == 0);
        Require(sceNgs2FftProcess(fftHandle, timeIn.data(), nat1Null.data(), nullptr, 0, 0x1) == 0);
        Require(nat1 == nat1Null);

        std::vector<float> nat9(n * 2, 0.0f);
        Require(sceNgs2FftProcess(fftHandle, timeIn.data(), nat9.data(), nullptr, 0, 0x9) == 0);

        const float scale = 1.0f / static_cast<float>(n);
        std::vector<float> packedSpectrum(n, 0.0f);
        for (std::size_t k = 0; k < half; ++k) {
            const float expRe = static_cast<float>(dft[k].real());
            const float expIm = static_cast<float>(k == 0 ? dft[half].real() : dft[k].imag());
            packedSpectrum[k * 2] = expRe;
            packedSpectrum[k * 2 + 1] = expIm;

            const std::size_t q = k / quarterBins;
            const std::size_t r = k % quarterBins;
            const std::size_t dstOffset = ((q & 1u) != 0 && r > 0) ? (quarterBins - r) : r;
            const std::size_t floatIdx = q * half + dstOffset * 2;
            Require(ApproxEqual(nat1[floatIdx], expRe, 2e-2f));
            Require(ApproxEqual(nat1[floatIdx + 1], expIm, 2e-2f));
            if (floatIdx < n) {
                Require(ApproxEqual(nat9[floatIdx], expRe * scale, 1e-4f));
                Require(ApproxEqual(nat9[floatIdx + 1], expIm * scale, 1e-4f));
            } else {
                Require(nat9[floatIdx] == nat1[floatIdx] && nat9[floatIdx + 1] == nat1[floatIdx + 1]);
            }
        }

        std::vector<float> inv0(n, 0.0f);
        std::vector<float> inv8(n, 0.0f);
        Require(sceNgs2FftProcess(fftHandle, packedSpectrum.data(), inv0.data(), nullptr, 0, 0x0) == 0);
        Require(sceNgs2FftProcess(fftHandle, packedSpectrum.data(), inv8.data(), nullptr, 0, 0x8) == 0);
        for (std::size_t i = 0; i < n; ++i) {
            Require(ApproxEqual(inv0[i], timeIn[i] * static_cast<float>(n), 1e-1f));
            Require(ApproxEqual(inv8[i], timeIn[i], 1e-4f));
        }

        std::vector<float> int5(n, 0.0f);
        std::vector<float> intD(n, 0.0f);
        std::vector<float> round4(n, 0.0f);
        std::vector<float> roundC(n, 0.0f);
        Require(sceNgs2FftProcess(fftHandle, timeIn.data(), int5.data(), scratch.data(), 0, 0x5) == 0);
        Require(sceNgs2FftProcess(fftHandle, timeIn.data(), intD.data(), nullptr, 0, 0xd) == 0);
        Require(sceNgs2FftProcess(fftHandle, int5.data(), round4.data(), nullptr, 0, 0x4) == 0);
        Require(sceNgs2FftProcess(fftHandle, int5.data(), roundC.data(), nullptr, 0, 0xc) == 0);
        for (std::size_t i = 0; i < n; ++i) {
            Require(ApproxEqual(intD[i], int5[i] * scale, 1e-5f));
            Require(ApproxEqual(round4[i], timeIn[i] * static_cast<float>(n), 1e-1f));
            Require(ApproxEqual(roundC[i], timeIn[i], 1e-4f));
        }
    }
}
#endif

int main() {
#ifdef APS5_NGS2_PS4_LIBRARY
    const std::uint64_t processSize = processParameters.size();
    const std::uint32_t processMagic = 0x4942524f;
    std::memcpy(processParameters.data(), &processSize, sizeof(processSize));
    std::memcpy(processParameters.data() + 8, &processMagic, sizeof(processMagic));
    Require(ProcessParametersFixture::Install(processParameters.data(), processParameters.size()));
    TestFft();
#endif
    TestNoModules(1, {0.25f});
    TestNoModules(2, {-0.75f, 0.25f});
    TestNoModules(3, {0.0f, -0.75f, 0.25f});
    TestNoModules(4, {0.0f, 0.0f, -0.75f, 0.25f});
    TestGainRouting();
    TestSubmixerKeyInput();
    TestSubmixerFilterOff();
    TestSubmixerCompressor();
    TestUnityCopy();
    for (std::uint32_t channels = 1; channels <= 8; channels++) {
        if (Ps4Library && (channels == 3 || channels == 4 || channels == 5 || channels == 7)) continue;
        TestIndependentVoicesAndSilentRamp(channels);
    }
    return 0;
}
