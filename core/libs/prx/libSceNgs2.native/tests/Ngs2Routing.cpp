#include "prx/libSceNgs2.native/tests/Ngs2Test.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
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

int main() {
    TestNoModules(1, {0.25f});
    TestNoModules(2, {-0.75f, 0.25f});
    TestNoModules(3, {0.0f, -0.75f, 0.25f});
    TestNoModules(4, {0.0f, 0.0f, -0.75f, 0.25f});
    TestGainRouting();
    TestUnityCopy();
    for (std::uint32_t channels = 1; channels <= 8; channels++) TestIndependentVoicesAndSilentRamp(channels);
    return 0;
}
