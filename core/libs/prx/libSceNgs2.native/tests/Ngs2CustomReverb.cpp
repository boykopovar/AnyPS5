#include "Ngs2Test.hpp"

#include <algorithm>
#include <array>
#include <cmath>

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
    Require(sceNgs2RackCreate(system, SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, &common, &storage, &rack) == SCE_NGS2_OK);
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
    usedBuffers = firstBuffer;
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
    Require(sceNgs2RackCreate(system, SCE_NGS2_RACK_ID_CUSTOM_SAMPLER, &common, &storage, &rack) == SCE_NGS2_OK);
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
    usedBuffers = firstBuffer;
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
    Require(sceNgs2RackCreate(system, SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, &common, &storage, &rack) == SCE_NGS2_OK);
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
    usedBuffers = firstBuffer;
}

int main() {
    TestReverb(0);
    TestReverb(1);
    TestReverb(1, true);
    TestSamplerRestart();
    TestPitchShiftAndReverbSetup();
    return 0;
}
