#include "Ngs2Test.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <limits>

static std::int32_t APS5_VABI Generate(Ngs2UserFxProcessContext* context) {
    const auto* signal = reinterpret_cast<const float*>(context->user_data0);
    for (std::uint32_t c = 0; c < context->num_channels; c++)
        std::fill_n(context->channel_data[c], context->num_grain_samples, signal[c]);
    return SCE_NGS2_OK;
}

struct GainGraph {
    std::uint32_t channels;
    uintptr_t system = CreateSystem();
    std::array<uintptr_t, 2> voices{};
    std::array<float, 8> signal{0.5f, -0.25f, 0.125f, -0.0625f, 0.03125f, -0.015625f, 0.0078125f, -0.00390625f};

    GainGraph(std::uint32_t channelCount = 2, std::uint32_t voiceCount = 1)
        : channels(channelCount) {
        const auto master = Mastering(system, channels);
        Ngs2CustomSubmixerRackOption mixerOption{};
        auto& custom = mixerOption.custom_rack_option;
        custom.rack_option.size = sizeof(mixerOption);
        custom.rack_option.max_grain_samples = 512;
        custom.rack_option.max_voices = voiceCount;
        custom.rack_option.max_matrices = 1;
        custom.rack_option.max_ports = 1;
        custom.num_buffers = 1;
        custom.num_modules = 1;
        custom.module[0].module_id = SCE_NGS2_CUSTOM_MODULE_ID_GAIN;
        mixerOption.max_channels = channels;
        mixerOption.max_inputs = 1;
        const auto rackId = SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER;
        Ngs2ContextBufferInfo query{};
        Require(sceNgs2RackQueryBufferSize(rackId, &custom.rack_option, &query) == SCE_NGS2_OK);
        const auto buffer = Buffer(query);
        uintptr_t rack = 0;
        Require(sceNgs2RackCreate(system, rackId, &custom.rack_option, &buffer, &rack) == SCE_NGS2_OK);
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

    ~GainGraph() { Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK); }

    void Set(std::initializer_list<float> values, std::uint32_t voice = 0) {
        Ngs2CustomVoiceGainParam param{};
        std::fill_n(param.gain, 37, std::numeric_limits<float>::quiet_NaN());
        std::copy(values.begin(), values.end(), param.gain);
        Control(voices[voice], SCE_NGS2_CUSTOM_VOICE_PARAM_GAIN, param);
    }

    std::vector<float> Render(std::uint32_t grain = Grain) {
        Require(sceNgs2SystemSetGrainSamples(system, grain) == SCE_NGS2_OK);
        std::vector<float> out(grain * channels, -123.0f);
        const Ngs2RenderBufferInfo info{out.data(), out.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, channels};
        Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_OK);
        return out;
    }
};

static void ExpectChannels(const std::vector<float>& out, std::initializer_list<std::vector<float>> channels) {
    std::size_t c = 0;
    for (const auto& samples : channels) {
        Require(out.size() == samples.size() * channels.size());
        for (std::size_t i = 0; i < samples.size(); i++) Require(out[i * channels.size() + c] == samples[i]);
        c++;
    }
}

static void ExpectFrame(const std::vector<float>& out, std::size_t index, std::initializer_list<float> frame) {
    Require((index + 1) * frame.size() <= out.size());
    Require(std::equal(frame.begin(), frame.end(), out.begin() + index * frame.size()));
}

static void ExpectConstant(const std::vector<float>& out, std::initializer_list<float> frame) {
    Require(out.size() % frame.size() == 0);
    for (std::size_t i = 0; i < out.size() / frame.size(); i++) ExpectFrame(out, i, frame);
}

static void TestRamps() {
    GainGraph graph;
    graph.Set({64.0f, -64.0f});
    graph.Set({8.0f, -16.0f});
    ExpectChannels(graph.Render(), {{0, 0.5f, 1, 1.5f, 2, 2.5f, 3, 3.5f}, {0, 0.5f, 1, 1.5f, 2, 2.5f, 3, 3.5f}});
    ExpectConstant(graph.Render(), {4, 4});
    graph.Set({8.0f, -16.0f});
    ExpectConstant(graph.Render(), {4, 4});
    graph.Set({16.0f, 16.0f});
    graph.Set({-80.0f, 96.0f});
    ExpectChannels(graph.Render(), {{4, -1.5f, -7, -12.5f, -18, -23.5f, -29, -34.5f}, {4, 0.5f, -3, -6.5f, -10, -13.5f, -17, -20.5f}});
    ExpectConstant(graph.Render(), {-40, -24});
    graph.Set({0, 0});
    ExpectChannels(graph.Render(), {{-40, -35, -30, -25, -20, -15, -10, -5}, {-24, -21, -18, -15, -12, -9, -6, -3}});
    ExpectConstant(graph.Render(), {0, 0});
}

static void TestEightChannels() {
    GainGraph graph(8);
    graph.Set({2, 8, 24, 64, 160, 384, 896, 2048});
    const auto out = graph.Render();
    ExpectFrame(out, 0, {0, 0, 0, 0, 0, 0, 0, 0});
    ExpectFrame(out, 4, {0.5f, -1, 1.5f, -2, 2.5f, -3, 3.5f, -4});
    ExpectFrame(out, 7, {7.0f / 8, -7.0f / 4, 21.0f / 8, -7.0f / 2, 35.0f / 8, -21.0f / 4, 49.0f / 8, -7});
    ExpectConstant(graph.Render(), {1, -2, 3, -4, 5, -6, 7, -8});
}

static void TestVoicesAndPause() {
    GainGraph graph(1, 2);
    graph.Set({0}, 0);
    graph.Set({0}, 1);
    ExpectConstant(graph.Render(64), {0});
    graph.Set({8}, 0);
    graph.Set({-4}, 1);
    auto out = graph.Render(64);
    for (std::uint32_t i = 0; i < 64; ++i) Require(out[i] == 2.0f * i / 64);
    ExpectConstant(graph.Render(64), {2});
    Event(graph.voices[0], SCE_NGS2_VOICE_EVENT_PAUSE);
    graph.Set({16}, 0);
    out = graph.Render(64);
    for (std::uint32_t i = 0; i < 64; ++i) Require(out[i] == (4.0f + 4.0f * i / 64) * (1.0f - static_cast<float>(i) / 64) - 2.0f);
    ExpectConstant(graph.Render(64), {-2});
    Event(graph.voices[0], SCE_NGS2_VOICE_EVENT_RESUME);
    out = graph.Render(64);
    for (std::uint32_t i = 0; i < 64; ++i) Require(out[i] == 8.0f * i / 64 - 2.0f);
    ExpectConstant(graph.Render(64), {6});
    graph.Set({-12}, 1);
    out = graph.Render(64);
    for (std::uint32_t i = 0; i < 64; ++i) Require(out[i] == 6.0f - 4.0f * i / 64);
    ExpectConstant(graph.Render(64), {2});
}

static void TestGrainChanges() {
    GainGraph graph(1);
    graph.Set({8});
    auto out = graph.Render(64);
    Require(out.front() == 0 && out[1] == 1.0f / 16 && out[32] == 2 && out.back() == 63.0f / 16);
    ExpectConstant(graph.Render(64), {4});
    graph.Set({-8});
    out = graph.Render(128);
    Require(out.front() == 4 && out[1] == 63.0f / 16 && out[64] == 0 && out.back() == -63.0f / 16);
    ExpectConstant(graph.Render(), {-4});
}

static void TestNonfiniteGains() {
    GainGraph graph(1);
    graph.Set({std::numeric_limits<float>::infinity()});
    graph.Render();
    graph.Set({std::numeric_limits<float>::infinity()});
    for (float sample : graph.Render()) Require(std::isinf(sample) && sample > 0);
    graph.Set({std::numeric_limits<float>::quiet_NaN()});
    for (float sample : graph.Render()) Require(std::isnan(sample));
    for (float sample : graph.Render()) Require(std::isnan(sample));
    graph.Set({2});
    for (float sample : graph.Render()) Require(std::isnan(sample));
    ExpectConstant(graph.Render(), {1});
}

static void TestZeroClearsNanSource() {
    GainGraph graph(1);
    graph.signal[0] = std::numeric_limits<float>::quiet_NaN();
    graph.Set({1});
    for (float sample : graph.Render()) Require(std::isnan(sample));
    graph.Set({0});
    for (float sample : graph.Render()) Require(std::isnan(sample));
    ExpectConstant(graph.Render(), {0});
}

static void TestParameterPositions() {
    GainGraph graph(1);
    graph.Set({8});
    Ngs2CustomVoiceGainParam param{};
    param.header = {sizeof(param), 0, 0};
    param.gain[0] = 16;
    for (std::uint32_t position = 0; position < 256; position++) {
        if (position == 0) continue;
        param.header.id = SCE_NGS2_CUSTOM_VOICE_PARAM_GAIN | position;
        Require(sceNgs2VoiceControl(graph.voices[0], &param.header) == SCE_NGS2_ERROR_INVALID_PARAM_ID);
    }
    ExpectChannels(graph.Render(), {{0, 0.5f, 1, 1.5f, 2, 2.5f, 3, 3.5f}});
    ExpectConstant(graph.Render(), {4});
}

static void TestLinkedParameters() {
    GainGraph graph(1);
    std::array<Ngs2CustomVoiceGainParam, 3> params{};
    for (auto& param : params) param.header = {sizeof(param), sizeof(param), SCE_NGS2_CUSTOM_VOICE_PARAM_GAIN};
    params[0].gain[0] = 8;
    params[1].header.id |= 255;
    params[2].gain[0] = 16;
    params[2].header.next = 0;
    Require(sceNgs2VoiceControl(graph.voices[0], &params[0].header) == SCE_NGS2_ERROR_INVALID_PARAM_ID);
    ExpectChannels(graph.Render(), {{0, 0.5f, 1, 1.5f, 2, 2.5f, 3, 3.5f}});
    ExpectConstant(graph.Render(), {4});
}

static void TestIntegerOutput() {
    for (const float gain : {1.0f, -1.0f}) {
        GainGraph graph(1);
        graph.Set({gain});
        graph.Render();
        std::array<std::int16_t, Grain> out{};
        const Ngs2RenderBufferInfo info{out.data(), sizeof(out), SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 1};
        Require(sceNgs2SystemRender(graph.system, &info, 1) == SCE_NGS2_OK);
        for (const auto sample : out) Require(sample == (gain > 0.0f ? 16384 : -16384));
    }
}

int main() {
    TestRamps();
    TestEightChannels();
    TestVoicesAndPause();
    TestGrainChanges();
    TestNonfiniteGains();
    TestParameterPositions();
    TestZeroClearsNanSource();
    TestLinkedParameters();
    TestIntegerOutput();
    return 0;
}
