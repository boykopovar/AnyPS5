#include "prx/libSceNgs2.native/tests/Ngs2Test.hpp"

#include <array>
#include <algorithm>
#include <string_view>

static std::uint32_t processCalls = 0;

static int APS5_VABI Generate(Ngs2UserFxProcessContext* context) {
    Require(++processCalls == 1);
    std::fill_n(context->channel_data[0], context->num_grain_samples, 1.0f);
    std::fill_n(context->channel_data[1], context->num_grain_samples, -0.5f);
    return 0;
}

static uintptr_t Custom(uintptr_t system, std::uint32_t count, bool chain, std::uint32_t outputBuffer = 0) {
    Ngs2CustomSubmixerRackOption option{};
    auto& custom = option.custom_rack_option;
    custom.rack_option.size = sizeof(option);
    custom.rack_option.max_grain_samples = 512;
    custom.rack_option.max_voices = 1;
    custom.rack_option.max_ports = 1;
    custom.num_buffers = count;
    custom.port[0].source_buffer_id = outputBuffer;
    custom.num_modules = chain ? 2 : 0;
    custom.module[0].module_id = SCE_NGS2_CUSTOM_MODULE_ID_USER_FX;
    custom.module[1].module_id = SCE_NGS2_CUSTOM_MODULE_ID_GAIN;
    option.max_channels = 8;
    option.max_inputs = count;
    Ngs2ContextBufferInfo query{};
    Require(sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, &custom.rack_option, &query) == 0);
    const auto storage = Buffer(query);
    uintptr_t rack = 0;
    Require(sceNgs2RackCreate(system, SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, &custom.rack_option, &storage, &rack) == 0);
    const auto voice = Voice(rack);
    Control(voice, SCE_NGS2_CUSTOM_SUBMIXER_VOICE_PARAM_SETUP, Ngs2CustomSubmixerVoiceSetupParam{{}, 2, 2, 0, 0});
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    return voice;
}

static void ExpectOutput(uintptr_t system, float left, float right) {
    std::array<float, Grain * 2> out{};
    const Ngs2RenderBufferInfo info{out.data(), sizeof(out), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 2};
    Require(sceNgs2SystemRender(system, &info, 1) == 0);
    for (std::uint32_t i = 0; i < Grain; i++) {
        Require(out[i * 2] == left && out[i * 2 + 1] == right);
    }
}

static void TestCustomCallback() {
    processCalls = 0;
    const auto system = CreateSystem();
    const auto master = Mastering(system, 2);
    const auto target = Custom(system, 1, true);
    Patch(target, master);
    Control(target, SCE_NGS2_CUSTOM_VOICE_PARAM_USER_FX, Ngs2CustomVoiceUserFxParam{{}, Generate, 0, 0, 0});
    Control(target, SCE_NGS2_CUSTOM_VOICE_PARAM_GAIN | 1, Ngs2CustomVoiceGainParam{});
    ExpectOutput(system, 0.0f, 0.0f);
    Require(processCalls == 1);
    Require(sceNgs2SystemDestroy(system, nullptr) == 0);
}

static void TestUpstreamCallback() {
    processCalls = 0;
    const auto system = CreateSystem();
    const auto master = Mastering(system, 2);
    const auto target = Custom(system, 2, false, 1);
    Patch(target, master);
    const auto source = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SUBMIXER));
    Control(source, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, 2, 0});
    Control(source, SCE_NGS2_SUBMIXER_VOICE_PARAM_USER_FX, Ngs2SubmixerVoiceUserFxParam{{}, Generate, 0, 0, 0});
    Control(source, SCE_NGS2_VOICE_PARAM_PATCH, Ngs2VoicePatchParam{{}, 0, 1, target});
    Event(source, SCE_NGS2_VOICE_EVENT_PLAY);
    ExpectOutput(system, 1.0f, -0.5f);
    Require(processCalls == 1);
    Require(sceNgs2SystemDestroy(system, nullptr) == 0);
}

static void TestConfiguredGain() {
    const auto system = CreateSystem();
    const auto master = Mastering(system, 2);
    const auto target = Custom(system, 1, true);
    Patch(target, master);
    Control(target, SCE_NGS2_CUSTOM_VOICE_PARAM_GAIN | 1, Ngs2CustomVoiceGainParam{});
    ExpectOutput(system, 0.0f, 0.0f);
    Require(sceNgs2SystemDestroy(system, nullptr) == 0);
}

struct CallbackContext {
    std::uint32_t calls = 0;
};

static void APS5_VABI CountBlockEnd(const Ngs2VoiceCallbackInfo* info) {
    auto& context = *reinterpret_cast<CallbackContext*>(info->callback_data);
    Require(++context.calls == 1);
}

static void TestRouting(std::uint32_t oldBuffers) {
    const auto system = CreateSystem();
    const auto firstMaster = Mastering(system, 2);
    const auto secondMaster = Mastering(system, 2);
    Control(secondMaster, SCE_NGS2_MASTERING_VOICE_PARAM_OUTPUT, Ngs2MasteringVoiceOutputParam{{}, 1, 0});
    const auto first = Custom(system, oldBuffers, false, oldBuffers - 1);
    const auto second = Custom(system, 2, false, 1);
    Patch(first, firstMaster);
    Patch(second, secondMaster);
    const auto source = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
    Control(source, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP,
            Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 2, 48000, 0, 0, 0}});
    std::array<std::int16_t, Grain * 2> pcm{};
    pcm.fill(8192);
    const Ngs2WaveformBlock block{0, sizeof(pcm), 0, 0, Grain, 0, 0};
    Control(source, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS,
            Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), 0, 1, &block});
    CallbackContext context;
    Control(source, SCE_NGS2_VOICE_PARAM_CALLBACK,
            Ngs2VoiceCallbackParam{{}, CountBlockEnd, reinterpret_cast<std::uintptr_t>(&context), SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END, 0});
    Patch(source, first);
    Control(source, SCE_NGS2_VOICE_PARAM_PATCH, Ngs2VoicePatchParam{{}, 0, 1, second});
    Event(source, SCE_NGS2_VOICE_EVENT_PLAY);
    std::array<std::array<float, Grain * 2>, 2> outputs{};
    std::array<Ngs2RenderBufferInfo, 2> info{};
    for (std::size_t i = 0; i < outputs.size(); i++) {
        outputs[i].fill(9.0f);
        info[i] = {outputs[i].data(), sizeof(outputs[i]), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 2};
    }
    Require(sceNgs2SystemRender(system, info.data(), info.size()) == SCE_NGS2_OK);
    Require(context.calls == 1);
    Ngs2VoicePortInfo port{};
    Require(sceNgs2VoiceGetPortInfo(source, 0, &port, sizeof(port)) == SCE_NGS2_OK);
    Require(port.dest_handle == second && port.dest_input_id == 1);
    for (const auto sample : outputs[0]) Require(sample == 0.0f);
    for (std::size_t i = 0; i < outputs[1].size(); ++i) Require(outputs[1][i] == (i < 6 ? 0.0f : 0.25f));
    Require(sceNgs2SystemDestroy(system, nullptr) == 0);
}

int main(int argc, char** argv) {
    Require(argc == 2);
    const std::string_view test = argv[1];
    if (test == "custom_callback") TestCustomCallback();
    else if (test == "upstream_callback") TestUpstreamCallback();
    else if (test == "configured_gain") TestConfiguredGain();
    else if (test == "routing_one_buffer") TestRouting(1);
    else if (test == "routing_two_buffers") TestRouting(2);
    else return 1;
    return 0;
}
