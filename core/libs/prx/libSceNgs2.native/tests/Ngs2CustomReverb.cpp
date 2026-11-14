#include "Ngs2Test.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>
#include <stdexcept>
#include <string_view>

static constexpr std::uint32_t RenderGrain = 256;

static int APS5_VABI Impulse(Ngs2UserFxProcessContext* context) {
    auto& calls = *reinterpret_cast<std::uint32_t*>(context->user_data0);
    Require(context->num_channels == 2 && context->num_grain_samples == RenderGrain);
    for (std::uint32_t channel = 0; channel < 2; channel++) {
        std::fill_n(context->channel_data[channel], RenderGrain, 0.0f);
        if (calls == 0) context->channel_data[channel][3] = channel == 0 ? .5f : .25f;
    }
    calls++;
    return SCE_NGS2_OK;
}

static void TestReverb(std::uint32_t size, bool strongHf = false) {
    const auto firstBuffer = usedBuffers;
    const auto system = CreateSystem();
    Require(sceNgs2SystemSetGrainSamples(system, RenderGrain) == SCE_NGS2_OK);
    const auto master = Mastering(system, 2);
    const std::uint32_t moduleOption[]{12, size, 0};
    Ngs2CustomSubmixerRackOption option{};
    auto& common = option.custom_rack_option.rack_option;
    common.size = sizeof(option);
    common.max_grain_samples = 512;
    common.max_voices = 2;
    common.max_matrices = 1;
    common.max_ports = 1;
    option.custom_rack_option.state_size = 16;
    option.custom_rack_option.num_buffers = 1;
    option.custom_rack_option.num_modules = 2;
    option.max_channels = 2;
    option.max_inputs = 1;
    for (auto index : {0u, 1u}) {
        auto& module = option.custom_rack_option.module[index];
        module.module_id = SCE_NGS2_CUSTOM_MODULE_ID_REVERB;
        module.option = reinterpret_cast<const Ngs2CustomModuleOption*>(moduleOption);
    }
    Ngs2ContextBufferInfo query{};
    Require(sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, &common, &query) == SCE_NGS2_OK);
    const auto storage = Buffer(query);
    uintptr_t rack = 0;
    Require(TestRackCreate(system, SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, &common, &storage, &rack) == SCE_NGS2_OK);
    std::array<uintptr_t, 2> voices{};
    for (auto index : {0u, 1u}) {
        Require(sceNgs2RackGetVoiceHandle(rack, index, &voices[index]) == SCE_NGS2_OK);
        Control(voices[index], SCE_NGS2_CUSTOM_SUBMIXER_VOICE_PARAM_SETUP, Ngs2CustomSubmixerVoiceSetupParam{{}, 2, 2, 0, 0});
        Patch(voices[index], master);
        Event(voices[index], SCE_NGS2_VOICE_EVENT_PLAY);
    }
    Ngs2ReverbI3DL2Param cave{1, 0, -1000, 0, 0, 2.91f, 1.3f, -602, .25f, -302, .022f, 100, 100, 5000, {}};
    if (strongHf) {
        cave.reflections_delay = .015f;
        cave.room_hf = -12000;
    }
    const auto source = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SUBMIXER));
    Control(source, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, 2, 0});
    std::uint32_t calls = 0;
    Control(source, SCE_NGS2_SUBMIXER_VOICE_PARAM_USER_FX,
            Ngs2SubmixerVoiceUserFxParam{{}, Impulse, reinterpret_cast<std::uintptr_t>(&calls), 0, 0});
    Patch(source, voices[0]);
    Event(source, SCE_NGS2_VOICE_EVENT_PLAY);
    std::vector<float> out(RenderGrain * 2);
    const Ngs2RenderBufferInfo output{out.data(), out.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 2};
    Require(sceNgs2SystemRender(system, &output, 1) == SCE_NGS2_OK);
    for (std::uint32_t i = 0; i < out.size(); i++) Require(out[i] == (i == 6 ? .5f : i == 7 ? .25f : 0.0f));
    calls = 0;
    Control(voices[0], SCE_NGS2_CUSTOM_VOICE_PARAM_REVERB | 0x20, Ngs2ReverbVoiceI3DL2Param{{}, cave});
    auto dry = cave;
    dry.wet = 0;
    dry.dry = 1;
    Control(voices[0], SCE_NGS2_CUSTOM_VOICE_PARAM_REVERB | 1, Ngs2ReverbVoiceI3DL2Param{{}, dry});
    for (std::uint32_t grain = 0; grain < 80; grain++) {
        Require(sceNgs2SystemRender(system, &output, 1) == SCE_NGS2_OK);
        for (const auto frame : {6000u, 8192u, 12000u, 16000u}) {
            if (frame / RenderGrain != grain) continue;
            const float expected[3][4][2]{
                {{5.78283021e-9f, 2.58373547e-7f}, {-8.08179466e-5f, 8.2077866e-5f}, {-7.49083818e-4f, -3.76510725e-4f}, {3.18058097e-4f, 6.42870436e-4f}},
                {{0, 0}, {0, 0}, {0, 0}, {8.11674454e-6f, -8.95900448e-5f}},
                {{-1.07959904e-5f, -1.03797774e-5f}, {2.38732318e-5f, 2.75960247e-5f}, {3.50958362e-5f, 3.04597361e-5f}, {2.78984935e-5f, 2.99852363e-5f}},
            };
            const auto sample = frame % RenderGrain;
            const auto check = frame == 6000 ? 0 : frame == 8192 ? 1 : frame == 12000 ? 2 : 3;
            Require(std::abs(out[sample * 2] - expected[strongHf ? 2 : size][check][0]) < 1e-7f);
            Require(std::abs(out[sample * 2 + 1] - expected[strongHf ? 2 : size][check][1]) < 1e-7f);
        }
    }
    Control(voices[0], SCE_NGS2_CUSTOM_SUBMIXER_VOICE_PARAM_SETUP, Ngs2CustomSubmixerVoiceSetupParam{{}, 2, 2, 0, 0});
    Patch(voices[0], master);
    calls = 0;
    Event(voices[0], SCE_NGS2_VOICE_EVENT_PLAY);
    Require(sceNgs2SystemRender(system, &output, 1) == SCE_NGS2_OK);
    for (std::uint32_t i = 0; i < out.size(); i++) Require(out[i] == (i == 6 ? .5f : i == 7 ? .25f : 0.0f));
    Event(voices[0], SCE_NGS2_VOICE_EVENT_STOP_IMM);
    Event(voices[0], SCE_NGS2_VOICE_EVENT_PLAY);
    Require(sceNgs2SystemRender(system, &output, 1) == SCE_NGS2_OK);
    for (float sample : out) Require(sample == 0);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    ResetBuffers(firstBuffer);
}

static void TestSamplerRestart() {
    const auto firstBuffer = usedBuffers;
    const auto system = CreateSystem();
    Require(sceNgs2SystemSetGrainSamples(system, RenderGrain) == SCE_NGS2_OK);
    const auto master = Mastering(system, 2);
    Ngs2CustomSamplerRackOption option{};
    auto& common = option.custom_rack_option.rack_option;
    common.size = sizeof(option);
    common.max_grain_samples = 512;
    common.max_voices = 1;
    common.max_matrices = 1;
    common.max_ports = 1;
    option.custom_rack_option.state_size = 48;
    option.custom_rack_option.num_buffers = 1;
    option.custom_rack_option.num_modules = 1;
    option.custom_rack_option.module[0].module_id = SCE_NGS2_CUSTOM_MODULE_ID_REVERB;
    option.max_channel_works = 2;
    option.max_waveform_blocks = 4;
    Ngs2ContextBufferInfo query{};
    Require(sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_CUSTOM_SAMPLER, &common, &query) == SCE_NGS2_OK);
    const auto storage = Buffer(query);
    uintptr_t rack = 0;
    Require(TestRackCreate(system, SCE_NGS2_RACK_ID_CUSTOM_SAMPLER, &common, &storage, &rack) == SCE_NGS2_OK);
    const auto voice = Voice(rack);
    struct SamplerSetup {
        Ngs2VoiceParamHeader header;
        Ngs2WaveformFormat format;
        std::uint32_t reserved[2];
    };
    Control(voice, SCE_NGS2_CUSTOM_SAMPLER_VOICE_PARAM_SETUP,
            SamplerSetup{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 2, 48000, 0, 0, 0}, {}});
    const Ngs2ReverbI3DL2Param cave{1, 0, -1000, 0, 0, 2.91f, 1.3f, -602, .015f, -302, .022f, 100, 100, 5000, {}};
    Control(voice, SCE_NGS2_CUSTOM_VOICE_PARAM_REVERB, Ngs2ReverbVoiceI3DL2Param{{}, cave});
    Patch(voice, master);
    std::vector<std::int16_t> pcm(RenderGrain * 2);
    pcm[6] = 16384;
    pcm[7] = 8192;
    Ngs2WaveformBlock block{0, pcm.size() * sizeof(std::int16_t), 0, 0, RenderGrain, 0, 0};
    Control(voice, SCE_NGS2_CUSTOM_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), 0, 1, &block});
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    std::vector<float> out(RenderGrain * 2);
    const Ngs2RenderBufferInfo output{out.data(), out.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 2};
    for (std::uint32_t i = 0; i < 8; i++) Require(sceNgs2SystemRender(system, &output, 1) == SCE_NGS2_OK);
    Require(Flags(voice) == 0);
    std::vector<std::int16_t> silence(RenderGrain * 2 * 20);
    block.data_size = silence.size() * sizeof(std::int16_t);
    block.num_samples = silence.size() / 2;
    Control(voice, SCE_NGS2_CUSTOM_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS,
            Ngs2SamplerVoiceWaveformBlocksParam{{}, silence.data(), SCE_NGS2_WAVEFORM_BLOCKS_FLAG_RESET, 1, &block});
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    for (std::uint32_t i = 0; i < 20; i++) {
        Require(sceNgs2SystemRender(system, &output, 1) == SCE_NGS2_OK);
        for (float sample : out) Require(sample == 0);
    }
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    ResetBuffers(firstBuffer);
}

static void TestPitchShiftAndReverbSetup() {
    const auto firstBuffer = usedBuffers;
    const auto system = CreateSystem();
    Require(sceNgs2SystemSetGrainSamples(system, RenderGrain) == SCE_NGS2_OK);
    const auto master = Mastering(system, 2);
    const std::uint32_t reverbOption[]{12, 1, 0};
    Ngs2CustomSubmixerRackOption option{};
    auto& custom = option.custom_rack_option;
    auto& common = custom.rack_option;
    common.size = sizeof(option);
    common.max_grain_samples = 512;
    common.max_voices = 1;
    common.max_matrices = 1;
    common.max_ports = 1;
    custom.state_size = 32;
    custom.num_buffers = 1;
    custom.num_modules = 2;
    option.max_channels = 2;
    option.max_inputs = 1;
    custom.module[0].module_id = SCE_NGS2_CUSTOM_MODULE_ID_PITCH_SHIFT;
    custom.module[1].module_id = SCE_NGS2_CUSTOM_MODULE_ID_REVERB;
    custom.module[1].option = reinterpret_cast<const Ngs2CustomModuleOption*>(reverbOption);
    Ngs2ContextBufferInfo query{};
    Require(sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, &common, &query) == SCE_NGS2_OK);
    const auto storage = Buffer(query);
    std::uintptr_t rack = 0;
    Require(TestRackCreate(system, SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, &common, &storage, &rack) == SCE_NGS2_OK);
    const auto voice = Voice(rack);
    for (int i = 0; i < 2; ++i) {
        Control(voice, SCE_NGS2_CUSTOM_SUBMIXER_VOICE_PARAM_SETUP, Ngs2CustomSubmixerVoiceSetupParam{{}, 2, 2, 0, 0});
        Patch(voice, master);
        Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
        std::array<float, RenderGrain * 2> samples{};
        const Ngs2RenderBufferInfo output{samples.data(), samples.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 2};
        Require(sceNgs2SystemRender(system, &output, 1) == SCE_NGS2_OK);
        Require(std::all_of(samples.begin(), samples.end(), [](float value) { return value == 0.0f; }));
        Event(voice, SCE_NGS2_VOICE_EVENT_STOP_IMM);
    }
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    ResetBuffers(firstBuffer);
}

struct RoutingSignal {
    std::uint32_t id = 0;
    std::uint32_t frame = 0;
    bool constant = false;
    bool muted = false;
    std::uint32_t activeChannels = 0xFFFFFFFFu;
    std::uintptr_t eventVoice = 0;
    std::vector<std::uint32_t> events;
};

static float RoutingSample(const RoutingSignal& signal, std::uint32_t frame, std::uint32_t channel) {
    if (signal.muted || (signal.activeChannels & (1u << channel)) == 0) return 0.0f;
    const float amplitude = static_cast<float>((signal.id + 1) * (channel + 1)) / 128.0f;
    if (signal.constant) return (channel & 1) != 0 ? -amplitude : amplitude;
    if (frame == 3 || frame == RenderGrain * 3 + 17) return amplitude;
    return frame == 97 ? -amplitude * .5f : 0.0f;
}

static int APS5_VABI GenerateRoutingSignal(Ngs2UserFxProcessContext* context) {
    auto& signal = *reinterpret_cast<RoutingSignal*>(context->user_data0);
    for (std::uint32_t channel = 0; channel < context->num_channels; ++channel)
        for (std::uint32_t frame = 0; frame < context->num_grain_samples; ++frame)
            context->channel_data[channel][frame] = RoutingSample(signal, signal.frame + frame, channel);
    signal.frame += context->num_grain_samples;
    for (const auto event : signal.events) Event(signal.eventVoice, event);
    signal.events.clear();
    return SCE_NGS2_OK;
}

struct RoutingModule {
    std::uint32_t id;
    std::uint32_t source;
    std::uint32_t dest;
    std::uint32_t extra = 0;
    std::uint32_t size = 0;
};

struct ReverbRoutingGraph {
    std::size_t firstBuffer = usedBuffers;
    std::uintptr_t system = CreateSystem();
    std::uint32_t channels;
    std::uint32_t bufferCount;
    std::uint32_t voiceCount;
    std::uint32_t moduleOffset;
    std::array<std::uint32_t, 4> inputMap;
    std::array<std::uintptr_t, 2> voices{};
    std::array<std::array<RoutingSignal, 2>, 2> signals{};

    ReverbRoutingGraph(std::uint32_t channelCount, std::uint32_t buffersCount,
                       std::initializer_list<RoutingModule> modules, std::uint32_t voicesCount = 1,
                       std::array<std::uint32_t, 4> bufferInputs = {0, 1, 0, 1})
        : channels(channelCount), bufferCount(buffersCount), voiceCount(voicesCount),
          moduleOffset(buffersCount > 2 ? buffersCount - 2 : 0), inputMap(bufferInputs) {
        Require(sceNgs2SystemSetGrainSamples(system, RenderGrain) == SCE_NGS2_OK);
        Ngs2CustomSubmixerRackOption option{};
        auto& custom = option.custom_rack_option;
        custom.rack_option.size = sizeof(option);
        custom.rack_option.max_grain_samples = 512;
        custom.rack_option.max_voices = voiceCount;
        custom.rack_option.max_matrices = 1;
        custom.rack_option.max_ports = bufferCount;
        custom.num_buffers = bufferCount;
        custom.num_modules = modules.size() + moduleOffset;
        option.max_channels = channels;
        option.max_inputs = std::min(bufferCount, 2u);
        std::array<std::array<std::uint32_t, 3>, SCE_NGS2_CUSTOM_MAX_MODULES> reverbOptions{};
        for (std::uint32_t buffer = 2; buffer < bufferCount; ++buffer) {
            auto& copy = custom.module[buffer - 2];
            copy.module_id = SCE_NGS2_CUSTOM_MODULE_ID_USER_FX;
            copy.source_buffer_id = inputMap[buffer];
            copy.dest_buffer_id = buffer;
        }
        std::size_t index = moduleOffset;
        for (const auto& module : modules) {
            auto& item = custom.module[index];
            item.module_id = module.id;
            item.source_buffer_id = module.source;
            item.dest_buffer_id = module.dest;
            item.extra_buffer_id = module.extra;
            if (module.id == SCE_NGS2_CUSTOM_MODULE_ID_REVERB) {
                reverbOptions[index] = {12, module.size, 0};
                item.option = reinterpret_cast<const Ngs2CustomModuleOption*>(reverbOptions[index].data());
            }
            ++index;
        }
        for (std::uint32_t buffer = 0; buffer < bufferCount; ++buffer) custom.port[buffer].source_buffer_id = buffer;
        Ngs2ContextBufferInfo query{};
        Require(sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, &custom.rack_option, &query) == SCE_NGS2_OK);
        const auto storage = Buffer(query);
        std::uintptr_t rack = 0;
        Require(TestRackCreate(system, SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, &custom.rack_option, &storage, &rack) == SCE_NGS2_OK);
        for (std::uint32_t voice = 0; voice < voiceCount; ++voice) {
            Require(sceNgs2RackGetVoiceHandle(rack, voice, &voices[voice]) == SCE_NGS2_OK);
            Control(voices[voice], SCE_NGS2_CUSTOM_SUBMIXER_VOICE_PARAM_SETUP,
                    Ngs2CustomSubmixerVoiceSetupParam{{}, channels, channels, 0, 0});
            Event(voices[voice], SCE_NGS2_VOICE_EVENT_PLAY);
            for (std::uint32_t input = 0; input < option.max_inputs; ++input) {
                auto& signal = signals[voice][input];
                signal.id = voice * 4 + input;
                const auto source = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SUBMIXER));
                Control(source, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, channels, 0});
                Control(source, SCE_NGS2_SUBMIXER_VOICE_PARAM_USER_FX,
                        Ngs2SubmixerVoiceUserFxParam{{}, GenerateRoutingSignal, reinterpret_cast<std::uintptr_t>(&signal), 0, 0});
                Control(source, SCE_NGS2_VOICE_PARAM_PATCH, Ngs2VoicePatchParam{{}, 0, input, voices[voice]});
                Event(source, SCE_NGS2_VOICE_EVENT_PLAY);
            }
            for (std::uint32_t buffer = 0; buffer < bufferCount; ++buffer) {
                const auto master = Mastering(system, channels);
                Control(master, SCE_NGS2_MASTERING_VOICE_PARAM_OUTPUT,
                        Ngs2MasteringVoiceOutputParam{{}, voice * bufferCount + buffer, 0});
                Control(voices[voice], SCE_NGS2_VOICE_PARAM_PATCH, Ngs2VoicePatchParam{{}, buffer, 0, master});
            }
        }
    }

    ~ReverbRoutingGraph() {
        Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
        ResetBuffers(firstBuffer);
    }

    RoutingSignal& Signal(std::uint32_t buffer, std::uint32_t voice = 0) { return signals[voice][inputMap[buffer]]; }

    void SetReverb(const Ngs2ReverbI3DL2Param& param, std::uint32_t module = 0, std::uint32_t voice = 0) {
        Control(voices[voice], SCE_NGS2_CUSTOM_VOICE_PARAM_REVERB | (module + moduleOffset), Ngs2ReverbVoiceI3DL2Param{{}, param});
    }

    void SetGain(float value, std::uint32_t module) {
        Ngs2CustomVoiceGainParam param{};
        std::fill(std::begin(param.gain), std::end(param.gain), value);
        Control(voices[0], SCE_NGS2_CUSTOM_VOICE_PARAM_GAIN | (module + moduleOffset), param);
    }

    std::vector<std::vector<float>> Render() {
        std::vector<std::vector<float>> out(bufferCount * voiceCount, std::vector<float>(RenderGrain * channels, -123.0f));
        std::vector<Ngs2RenderBufferInfo> info;
        for (auto& buffer : out)
            info.push_back({buffer.data(), buffer.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, channels});
        Require(sceNgs2SystemRender(system, info.data(), info.size()) == SCE_NGS2_OK);
        return out;
    }
};

static Ngs2ReverbI3DL2Param RoutingReverb(float wet = .75f, float dry = .25f) {
    return {wet, dry, -1000, 0, 0, 2.91f, 1.3f, -602, .015f, -302, .022f, 100, 100, 5000, {}};
}

static void ExpectSame(const std::vector<float>& actual, const std::vector<float>& expected) {
    Require(actual.size() == expected.size());
    for (std::size_t i = 0; i < actual.size(); ++i) Require(std::abs(actual[i] - expected[i]) < 1e-7f);
}

static void ExpectSignal(const std::vector<float>& actual, const RoutingSignal& signal,
                         std::uint32_t firstFrame, std::uint32_t channels) {
    for (std::uint32_t frame = 0; frame < RenderGrain; ++frame)
        for (std::uint32_t channel = 0; channel < channels; ++channel)
            Require(actual[frame * channels + channel] == RoutingSample(signal, firstFrame + frame, channel));
}

static void TestRoutedReverb(std::uint32_t buffersCount, std::uint32_t source, std::uint32_t dest,
                             std::uint32_t channels, std::uint32_t size) {
    ReverbRoutingGraph reference(channels, 1, {{SCE_NGS2_CUSTOM_MODULE_ID_REVERB, 0, 0, 0, size}});
    ReverbRoutingGraph routed(channels, buffersCount,
                             {{SCE_NGS2_CUSTOM_MODULE_ID_REVERB, source, dest, (source + 1) % buffersCount, size}});
    reference.Signal(0).id = routed.Signal(source).id;
    for (std::uint32_t input = 0; input < 2; ++input) routed.signals[0][input].constant = input != routed.inputMap[source];
    auto param = RoutingReverb();
    if (channels > 1) param.reserved[0] = 1u << (channels - 1);
    reference.SetReverb(param);
    routed.SetReverb(param);
    float tailEnergy = 0;
    for (std::uint32_t grain = 0; grain < 80; ++grain) {
        if (grain == 17 || grain == 43) {
            param.wet = grain == 17 ? .35f : 1.0f;
            param.dry = grain == 17 ? -.125f : 0.0f;
            param.room_hf = -6000;
            reference.SetReverb(param);
            routed.SetReverb(param);
        }
        const auto expected = reference.Render();
        const auto actual = routed.Render();
        ExpectSame(actual[dest], expected[0]);
        for (std::uint32_t buffer = 0; buffer < buffersCount; ++buffer)
            if (buffer != dest) ExpectSignal(actual[buffer], routed.Signal(buffer), grain * RenderGrain, channels);
        if (grain > 43) for (const float sample : actual[dest]) tailEnergy += std::abs(sample);
    }
    Require(tailEnergy > 1e-5f);
}

static void TestDryRouting(std::uint32_t buffersCount, std::uint32_t source, std::uint32_t dest, std::uint32_t channels) {
    ReverbRoutingGraph graph(channels, buffersCount, {{SCE_NGS2_CUSTOM_MODULE_ID_REVERB, source, dest, buffersCount - 1}});
    for (auto& signal : graph.signals[0]) signal.constant = true;
    auto param = RoutingReverb(0, .25f);
    param.reserved[0] = 1u << (channels - 1);
    graph.SetReverb(param);
    for (std::uint32_t grain = 0; grain < 4; ++grain) {
        if (grain == 1) param.dry = -.5f;
        if (grain == 3) param.dry = 0;
        graph.SetReverb(param);
        const auto actual = graph.Render();
        for (std::uint32_t frame = 0; frame < RenderGrain; ++frame) {
            const float fraction = static_cast<float>(frame) / RenderGrain;
            const float dry = grain == 0 ? .25f : grain == 1 ? .25f - .75f * fraction : grain == 2 ? -.5f : -.5f + .5f * fraction;
            for (std::uint32_t channel = 0; channel < channels; ++channel) {
                const float expected = RoutingSample(graph.Signal(source), grain * RenderGrain + frame, channel) * dry;
                Require(std::abs(actual[dest][frame * channels + channel] - expected) < 1e-7f);
            }
        }
        for (std::uint32_t buffer = 0; buffer < buffersCount; ++buffer)
            if (buffer != dest) ExpectSignal(actual[buffer], graph.Signal(buffer), grain * RenderGrain, channels);
    }
    const auto silent = graph.Render();
    for (float sample : silent[dest]) Require(sample == 0);
    for (std::uint32_t buffer = 0; buffer < buffersCount; ++buffer)
        if (buffer != dest) ExpectSignal(silent[buffer], graph.Signal(buffer), 4 * RenderGrain, channels);
}

static void TestReverbModuleIsolation() {
    ReverbRoutingGraph first(2, 1, {{SCE_NGS2_CUSTOM_MODULE_ID_REVERB, 0, 0}});
    ReverbRoutingGraph second(2, 1, {{SCE_NGS2_CUSTOM_MODULE_ID_REVERB, 0, 0, 0, 1}});
    ReverbRoutingGraph routed(2, 4, {{SCE_NGS2_CUSTOM_MODULE_ID_REVERB, 0, 1, 3},
                                    {SCE_NGS2_CUSTOM_MODULE_ID_REVERB, 2, 3, 1, 1}}, 1, {0, 1, 1, 0});
    second.Signal(0).id = routed.Signal(2).id;
    const auto firstParam = RoutingReverb(.8f, -.25f);
    auto secondParam = RoutingReverb(.3f, .75f);
    secondParam.room_hf = -8000;
    first.SetReverb(firstParam);
    second.SetReverb(secondParam);
    routed.SetReverb(firstParam, 0);
    routed.SetReverb(secondParam, 1);
    for (std::uint32_t grain = 0; grain < 80; ++grain) {
        const auto actual = routed.Render();
        ExpectSame(actual[1], first.Render()[0]);
        ExpectSame(actual[3], second.Render()[0]);
        ExpectSignal(actual[0], routed.Signal(0), grain * RenderGrain, 2);
        ExpectSignal(actual[2], routed.Signal(2), grain * RenderGrain, 2);
    }
}

static void TestReverbVoiceIsolation() {
    ReverbRoutingGraph reference(2, 1, {{SCE_NGS2_CUSTOM_MODULE_ID_REVERB, 0, 0}});
    ReverbRoutingGraph routed(2, 2, {{SCE_NGS2_CUSTOM_MODULE_ID_REVERB, 1, 0}}, 2);
    reference.signals[0][0].id = 5;
    auto param = RoutingReverb(1, 0);
    reference.SetReverb(param);
    routed.SetReverb(RoutingReverb(.25f, .75f), 0, 0);
    routed.SetReverb(param, 0, 1);
    float tailEnergy = 0;
    for (std::uint32_t grain = 0; grain < 80; ++grain) {
        if (grain == 40) {
            for (auto& signal : routed.signals[0]) signal.muted = true;
            Event(routed.voices[0], SCE_NGS2_VOICE_EVENT_STOP_IMM);
            Event(routed.voices[0], SCE_NGS2_VOICE_EVENT_PLAY);
        }
        const auto actual = routed.Render();
        ExpectSame(actual[2], reference.Render()[0]);
        if (grain >= 40) {
            for (float sample : actual[0]) Require(sample == 0);
            for (float sample : actual[2]) tailEnergy += std::abs(sample);
        }
    }
    Require(tailEnergy > 1e-5f);
}

static void TestGainReverbGainOrder() {
    ReverbRoutingGraph reference(2, 1, {{SCE_NGS2_CUSTOM_MODULE_ID_GAIN, 0, 0},
                                       {SCE_NGS2_CUSTOM_MODULE_ID_REVERB, 0, 0},
                                       {SCE_NGS2_CUSTOM_MODULE_ID_GAIN, 0, 0}});
    ReverbRoutingGraph routed(2, 4, {{SCE_NGS2_CUSTOM_MODULE_ID_GAIN, 0, 1},
                                    {SCE_NGS2_CUSTOM_MODULE_ID_REVERB, 1, 2, 3},
                                    {SCE_NGS2_CUSTOM_MODULE_ID_GAIN, 2, 3}});
    routed.Signal(1).constant = true;
    reference.SetGain(1.5f, 0);
    routed.SetGain(1.5f, 0);
    reference.SetGain(-.5f, 2);
    routed.SetGain(-.5f, 2);
    reference.SetReverb(RoutingReverb(), 1);
    routed.SetReverb(RoutingReverb(), 1);
    for (std::uint32_t grain = 0; grain < 80; ++grain) {
        if (grain == 28) {
            reference.SetGain(-.25f, 0);
            routed.SetGain(-.25f, 0);
            reference.SetGain(2, 2);
            routed.SetGain(2, 2);
        }
        const auto actual = routed.Render();
        ExpectSame(actual[3], reference.Render()[0]);
        ExpectSignal(actual[0], routed.Signal(0), grain * RenderGrain, 2);
    }
}

static void TestUnconfiguredReverbRouting() {
    ReverbRoutingGraph graph(2, 2, {{SCE_NGS2_CUSTOM_MODULE_ID_REVERB, 1, 0}});
    bool threw = false;
    try {
        graph.Render();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
    graph.SetReverb(RoutingReverb(0, 1));
    const auto actual = graph.Render();
    ExpectSignal(actual[0], graph.signals[0][1], graph.signals[0][1].frame - RenderGrain, 2);
    std::array<std::uintptr_t, 2> masters{};
    for (std::uint32_t port = 0; port < 2; ++port) {
        Ngs2VoicePortInfo info{};
        Require(sceNgs2VoiceGetPortInfo(graph.voices[0], port, &info, sizeof(info)) == SCE_NGS2_OK);
        masters[port] = info.dest_handle;
    }
    Control(graph.voices[0], SCE_NGS2_CUSTOM_SUBMIXER_VOICE_PARAM_SETUP, Ngs2CustomSubmixerVoiceSetupParam{{}, 2, 2, 0, 0});
    for (std::uint32_t port = 0; port < 2; ++port)
        Control(graph.voices[0], SCE_NGS2_VOICE_PARAM_PATCH, Ngs2VoicePatchParam{{}, port, 0, masters[port]});
    Event(graph.voices[0], SCE_NGS2_VOICE_EVENT_PLAY);
    threw = false;
    try {
        graph.Render();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
    graph.SetReverb(RoutingReverb(0, 1));
    const auto reset = graph.Render();
    ExpectSignal(reset[0], graph.signals[0][1], graph.signals[0][1].frame - RenderGrain, 2);
}

static void TestUnconfiguredReverbAlias() {
    ReverbRoutingGraph graph(2, 4, {{SCE_NGS2_CUSTOM_MODULE_ID_REVERB, 3, 3, 1}});
    for (auto& signal : graph.signals[0]) signal.constant = true;
    const auto actual = graph.Render();
    for (std::uint32_t buffer = 0; buffer < graph.bufferCount; ++buffer)
        ExpectSignal(actual[buffer], graph.Signal(buffer), 0, 2);
}

static void TestRoutedReverbMasks() {
    for (const auto channels : {1u, 2u, 6u, 8u}) {
        for (const auto size : {0u, 1u}) {
            for (const auto mask : {0u, 1u << (channels - 1)}) {
                for (const bool maskedInputOnly : {false, true}) {
                    if (mask == 0 && maskedInputOnly) continue;
                    ReverbRoutingGraph graph(channels, 4, {{SCE_NGS2_CUSTOM_MODULE_ID_REVERB, 1, 3, 2, size}});
                    graph.Signal(1).activeChannels = maskedInputOnly ? mask : (1u << channels) - 1;
                    auto param = RoutingReverb(1, 0);
                    param.reserved[0] = mask;
                    graph.SetReverb(param);
                    std::array<float, 8> energy{};
                    for (std::uint32_t grain = 0; grain < 80; ++grain) {
                        const auto actual = graph.Render();
                        for (std::uint32_t frame = 0; frame < RenderGrain; ++frame) {
                            for (std::uint32_t channel = 0; channel < channels; ++channel) {
                                const auto sample = actual[3][frame * channels + channel];
                                if (maskedInputOnly || (mask & (1u << channel)) != 0) Require(sample == 0.0f);
                                energy[channel] += std::abs(sample);
                            }
                        }
                        ExpectSignal(actual[1], graph.Signal(1), grain * RenderGrain, channels);
                    }
                    for (std::uint32_t channel = 0; channel < channels; ++channel)
                        if (!maskedInputOnly && (mask & (1u << channel)) == 0) Require(energy[channel] > 1e-5f);
                }
            }
        }
    }
}

static void ExpectTransitionRefused(ReverbRoutingGraph& graph) {
    bool threw = false;
    try {
        graph.Render();
    } catch (const std::runtime_error& error) {
        threw = std::string_view(error.what()).find("custom Reverb voice transitions with multiple buffers") != std::string_view::npos;
    }
    Require(threw);
}

static void TestRoutedReverbTransitions() {
    for (const auto event : {SCE_NGS2_VOICE_EVENT_PAUSE, SCE_NGS2_VOICE_EVENT_RESUME, SCE_NGS2_VOICE_EVENT_STOP}) {
        ReverbRoutingGraph reference(2, 1, {{SCE_NGS2_CUSTOM_MODULE_ID_REVERB, 0, 0}});
        ReverbRoutingGraph routed(2, 4, {{SCE_NGS2_CUSTOM_MODULE_ID_REVERB, 1, 2}});
        reference.Signal(0).id = 1;
        reference.Signal(0).constant = true;
        routed.Signal(0).constant = true;
        routed.Signal(1).constant = true;
        reference.SetReverb(RoutingReverb());
        routed.SetReverb(RoutingReverb());
        ExpectSame(routed.Render()[2], reference.Render()[0]);
        if (event == SCE_NGS2_VOICE_EVENT_RESUME) {
            Event(routed.voices[0], SCE_NGS2_VOICE_EVENT_PAUSE);
            ExpectTransitionRefused(routed);
        }
        Event(routed.voices[0], event);
        ExpectTransitionRefused(routed);
        for (const auto voice : {reference.voices[0], routed.voices[0]}) {
            Event(voice, SCE_NGS2_VOICE_EVENT_STOP_IMM);
            Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
        }
        ExpectSame(routed.Render()[2], reference.Render()[0]);
    }
}

static void TestDeferredReverbTransitions() {
    for (const bool playPause : {false, true}) {
        ReverbRoutingGraph routed(2, 4, {{SCE_NGS2_CUSTOM_MODULE_ID_REVERB, 1, 2}});
        routed.Signal(0).constant = true;
        routed.Signal(1).constant = true;
        routed.SetReverb(RoutingReverb());
        routed.Render();
        auto& signal = routed.Signal(0);
        signal.eventVoice = routed.voices[0];
        if (playPause) signal.events = {SCE_NGS2_VOICE_EVENT_PLAY, SCE_NGS2_VOICE_EVENT_PAUSE};
        else signal.events = {SCE_NGS2_VOICE_EVENT_KILL};
        routed.Render();
        Require(signal.events.empty());
        ExpectTransitionRefused(routed);
        if (playPause) {
            Require((Flags(routed.voices[0]) & SCE_NGS2_VOICE_STATE_FLAG_PAUSED) != 0);
            Event(routed.voices[0], SCE_NGS2_VOICE_EVENT_STOP_IMM);
            Event(routed.voices[0], SCE_NGS2_VOICE_EVENT_PLAY);
            ExpectTransitionRefused(routed);
        }
        Event(routed.voices[0], SCE_NGS2_VOICE_EVENT_STOP_IMM);
        for (const auto& buffer : routed.Render()) for (const auto sample : buffer) Require(sample == 0.0f);
        Require(Flags(routed.voices[0]) == 0);
        Event(routed.voices[0], SCE_NGS2_VOICE_EVENT_PLAY);
        ReverbRoutingGraph reference(2, 1, {{SCE_NGS2_CUSTOM_MODULE_ID_REVERB, 0, 0}});
        reference.Signal(0).id = routed.Signal(1).id;
        reference.Signal(0).constant = true;
        reference.SetReverb(RoutingReverb());
        ExpectSame(routed.Render()[2], reference.Render()[0]);
    }
}

int main() {
    TestReverb(0);
    TestReverb(1);
    TestReverb(1, true);
    TestSamplerRestart();
    TestPitchShiftAndReverbSetup();
    TestRoutedReverb(2, 0, 1, 1, 0);
    TestRoutedReverb(2, 1, 0, 2, 1);
    TestRoutedReverb(3, 2, 1, 6, 0);
    TestRoutedReverb(4, 1, 3, 8, 1);
    TestRoutedReverb(4, 3, 3, 2, 0);
    TestDryRouting(2, 1, 0, 1);
    TestDryRouting(3, 0, 2, 2);
    TestDryRouting(4, 2, 3, 6);
    TestDryRouting(4, 3, 3, 8);
    TestReverbModuleIsolation();
    TestReverbVoiceIsolation();
    TestGainReverbGainOrder();
    TestUnconfiguredReverbRouting();
    TestUnconfiguredReverbAlias();
    TestRoutedReverbMasks();
    TestRoutedReverbTransitions();
    TestDeferredReverbTransitions();
    return 0;
}
