#include "Ngs2Test.hpp"

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace Ngs2Testing;
using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;
using Testing::RequireThrows;

struct Calls {
    std::vector<std::uint32_t> setupVoices;
    std::vector<std::uint32_t> cleanupVoices;
    std::vector<std::uint32_t> processFlags;
    std::vector<int> order;
    int processResult = SCE_NGS2_OK;
    std::uint32_t contextMismatches = 0;
};

Calls& CallsOf(std::uintptr_t userData) {
    return *reinterpret_cast<Calls*>(userData);
}

std::int32_t APS5_VABI Setup(Ngs2UserFx2SetupContext* context) {
    auto& calls = CallsOf(context->user_data);
    if (context->common == nullptr || context->param == nullptr || context->work == nullptr || context->max_voices != 2) {
        calls.contextMismatches++;
        return SCE_NGS2_OK;
    }
    std::memset(context->work, 0x5a, 8);
    *static_cast<float*>(context->param) = 1.0f;
    *static_cast<float*>(context->common) = 0.25f;
    calls.setupVoices.push_back(context->voice_index);
    return SCE_NGS2_OK;
}

std::int32_t APS5_VABI Cleanup(Ngs2UserFx2CleanupContext* context) {
    auto& calls = CallsOf(context->user_data);
    if (context->max_voices != 2 || static_cast<std::uint8_t*>(context->work)[7] != 0x5a) calls.contextMismatches++;
    calls.cleanupVoices.push_back(context->voice_index);
    return SCE_NGS2_OK;
}

std::int32_t APS5_VABI Gain(Ngs2UserFx2ProcessContext* context) {
    auto& calls = CallsOf(context->user_data);
    if (context->num_input_channels != 2 || context->num_output_channels != 2 || context->num_grain_samples != Grain ||
        context->sample_rate != 48000 || context->state == nullptr || static_cast<const std::uint8_t*>(context->work)[0] != 0x5a) {
        calls.contextMismatches++;
    }
    const float gain = *static_cast<const float*>(context->param);
    for (std::uint32_t c = 0; c < 2; c++) {
        for (std::uint32_t i = 0; i < Grain; i++) context->channel_data[c][i] *= gain;
    }
    *static_cast<float*>(context->common) = 0.125f;
    calls.processFlags.push_back(context->flags);
    calls.order.push_back(0);
    return calls.processResult;
}

std::int32_t APS5_VABI Offset(Ngs2UserFx2ProcessContext* context) {
    const float offset = *static_cast<const float*>(context->common);
    for (std::uint32_t i = 0; i < Grain; i++) context->channel_data[1][i] += offset;
    CallsOf(context->user_data).order.push_back(1);
    return SCE_NGS2_OK;
}

std::int32_t APS5_VABI ControlHandler(Ngs2UserFx2ControlContext*) {
    return SCE_NGS2_OK;
}

Ngs2CustomUserFx2ModuleOption Module(Ngs2UserFx2ProcessHandler process, Calls& calls) {
    return {{sizeof(Ngs2CustomUserFx2ModuleOption)}, Setup, Cleanup, nullptr, process, sizeof(float), sizeof(float), 8,
            reinterpret_cast<std::uintptr_t>(&calls)};
}

Ngs2CustomSubmixerRackOption RackOption(const Ngs2CustomUserFx2ModuleOption* modules, std::uint32_t count) {
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

template<typename TException>
void RequireQueryRejected(const Ngs2CustomSubmixerRackOption& option, const char* message) {
    Ngs2ContextBufferInfo query{};
    RequireThrows<TException>([&] {
        sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, &option.custom_rack_option.rack_option, &query);
    }, message);
}

template<typename TException, typename TParam>
void RequireControlRejected(uintptr_t voice, std::uint32_t id, TParam param, const char* message) {
    param.header = {static_cast<std::uint16_t>(sizeof(TParam)), 0, id};
    RequireThrows<TException>([&] { sceNgs2VoiceControl(voice, &param.header); }, message);
}

uintptr_t Sampler(Ngs2Fixture& ngs2, uintptr_t system, const std::vector<std::int16_t>& pcm) {
    const auto voice = Voice(ngs2.CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP,
            Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 2, 48000, 0, 0, 0}});
    const Ngs2WaveformBlock block{0, pcm.size() * sizeof(std::int16_t), 0, 0, static_cast<std::uint32_t>(pcm.size() / 2), 0, 0};
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), 0, 1, &block});
    return voice;
}

std::vector<float> Render(uintptr_t system) {
    std::vector<float> out(Grain * 2, -1.0f);
    const Ngs2RenderBufferInfo info{out.data(), out.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 2};
    RequireEqual(sceNgs2SystemRender(system, &info, 1), SCE_NGS2_OK, "render");
    return out;
}

void RequireFrames(const std::vector<float>& out, float left, float right, const char* message) {
    for (std::uint32_t i = 0; i < Grain; i++) {
        RequireEqual(out[i * 2], left, std::string(message) + " left frame " + std::to_string(i));
        RequireEqual(out[i * 2 + 1], right, std::string(message) + " right frame " + std::to_string(i));
    }
}

class ChainFixture {
public:
    ChainFixture()
        : modules{Module(Gain, calls), Module(Offset, calls)},
          system(ngs2.CreateSystem()),
          master(ngs2.Mastering(system, 2)),
          rack(ngs2.CreateRack(system, SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, &rackOption.custom_rack_option.rack_option)),
          custom(Voice(rack)),
          pcm(Grain * 2 * 8, 16384) {
        Control(custom, SCE_NGS2_CUSTOM_SUBMIXER_VOICE_PARAM_SETUP, Ngs2CustomSubmixerVoiceSetupParam{{}, 2, 2, 0, 0});
        Patch(custom, master);
        Event(custom, SCE_NGS2_VOICE_EVENT_PLAY);
        sampler = Sampler(ngs2, system, pcm);
        Patch(sampler, custom);
        Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);
    }

    Calls calls;
    const Ngs2CustomUserFx2ModuleOption modules[2];
    const Ngs2CustomSubmixerRackOption rackOption = RackOption(modules, 2);
    Ngs2Fixture ngs2;
    const uintptr_t system;
    const uintptr_t master;
    const uintptr_t rack;
    const uintptr_t custom;
    const std::vector<std::int16_t> pcm;
    uintptr_t sampler = 0;
};

const float halfGain = 0.5f;

const Case chainSetup{"CustomRack_Create_RunsSetupForEveryModuleAndVoice", [] {
    const ChainFixture fixture;
    RequireEqual(fixture.calls.setupVoices == std::vector<std::uint32_t>{0, 1, 0, 1}, true, "setup voice order 0,1,0,1");
    RequireEqual(fixture.calls.contextMismatches, 0u, "context mismatches");
}};

const Case chainRender{"CustomRack_Render_RunsModulesInOrderWithSharedCommon", [] {
    ChainFixture fixture;
    RequireFrames(Render(fixture.system), 0.5f, 0.75f, "first grain");
    RequireEqual(fixture.calls.processFlags == std::vector<std::uint32_t>{1}, true, "process flags 1");
    RequireEqual(fixture.calls.order == std::vector<int>{0, 1}, true, "module order 0,1");
    RequireEqual(fixture.calls.contextMismatches, 0u, "context mismatches");
}};

const Case chainParam{"CustomVoice_UserFx2Param_UpdatesParamAndFlagsNextGrain", [] {
    ChainFixture fixture;
    Render(fixture.system);
    Control(fixture.custom, SCE_NGS2_CUSTOM_VOICE_PARAM_USER_FX2 | 0, Ngs2CustomVoiceUserFx2Param{{}, &halfGain, sizeof(halfGain)});
    RequireFrames(Render(fixture.system), 0.25f, 0.5f, "after the param change");
    RequireEqual(fixture.calls.processFlags == std::vector<std::uint32_t>{1, 2}, true, "process flags 1,2");
    Render(fixture.system);
    RequireEqual(fixture.calls.processFlags == std::vector<std::uint32_t>{1, 2, 0}, true, "process flags 1,2,0");
}};

const Case chainRejectedControls{"CustomVoice_InvalidControls_AreRejected", [] {
    const ChainFixture fixture;
    RequireControlRejected<std::invalid_argument>(fixture.custom, SCE_NGS2_CUSTOM_VOICE_PARAM_USER_FX2 | 0,
        Ngs2CustomVoiceUserFx2Param{{}, &halfGain, sizeof(halfGain) - 1}, "short param");
    RequireControlRejected<std::invalid_argument>(fixture.custom, SCE_NGS2_CUSTOM_VOICE_PARAM_USER_FX2 | 2,
        Ngs2CustomVoiceUserFx2Param{{}, &halfGain, sizeof(halfGain)}, "module index 2");
    RequireControlRejected<std::runtime_error>(fixture.custom, SCE_NGS2_CUSTOM_SUBMIXER_VOICE_PARAM_SETUP,
        Ngs2CustomSubmixerVoiceSetupParam{{}, 2, 1, 0, 0}, "channel conversion");
    RequireControlRejected<std::runtime_error>(fixture.custom, SCE_NGS2_CUSTOM_SUBMIXER_VOICE_PARAM_SETUP,
        Ngs2CustomSubmixerVoiceSetupParam{{}, 2, 2, 1, 0}, "setup flags");
    RequireControlRejected<std::invalid_argument>(fixture.custom, SCE_NGS2_CUSTOM_SUBMIXER_VOICE_PARAM_SETUP,
        Ngs2CustomSubmixerVoiceSetupParam{{}, 3, 3, 0, 0}, "3 channels");
    RequireControlRejected<std::invalid_argument>(fixture.sampler, SCE_NGS2_CUSTOM_VOICE_PARAM_USER_FX2 | 0,
        Ngs2CustomVoiceUserFx2Param{{}, &halfGain, sizeof(halfGain)}, "user fx param on a sampler");
}};

const Case chainProcessFailure{"CustomRack_ProcessHandlerFails_RenderThrows", [] {
    ChainFixture fixture;
    fixture.calls.processResult = -1;
    RequireThrows<std::runtime_error>([&] { Render(fixture.system); }, "failing process handler");
}};

const Case chainCleanup{"CustomRack_Destroy_RunsCleanupForEveryModuleAndVoice", [] {
    ChainFixture fixture;
    Render(fixture.system);
    RequireEqual(sceNgs2RackDestroy(fixture.rack, nullptr), SCE_NGS2_OK, "destroy the rack");
    RequireEqual(fixture.calls.cleanupVoices == std::vector<std::uint32_t>{0, 0, 1, 1}, true, "cleanup voice order 0,0,1,1");
    RequireEqual(fixture.calls.contextMismatches, 0u, "context mismatches");
}};

const Case rackOptions{"CustomRack_UnsupportedOrInvalidOptions_AreRejected", [] {
    Calls calls;
    const Ngs2CustomUserFx2ModuleOption modules[1] = {Module(Gain, calls)};
    const auto good = RackOption(modules, 1);
    Ngs2ContextBufferInfo query{};
    RequireEqual(sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, &good.custom_rack_option.rack_option, &query),
                 SCE_NGS2_OK, "valid option");

    auto option = good;
    option.custom_rack_option.num_buffers = 2;
    RequireQueryRejected<std::runtime_error>(option, "2 buffers");
    option = good;
    option.custom_rack_option.module[0].module_id = 0x10;
    RequireQueryRejected<std::runtime_error>(option, "module id 0x10");
    option = good;
    option.custom_rack_option.module[0].dest_buffer_id = 1;
    RequireQueryRejected<std::invalid_argument>(option, "destination buffer 1");
    option = good;
    option.custom_rack_option.port[0].source_buffer_id = 1;
    RequireQueryRejected<std::invalid_argument>(option, "port source buffer 1");
    option = good;
    option.custom_rack_option.num_modules = SCE_NGS2_CUSTOM_MAX_MODULES + 1;
    RequireQueryRejected<std::invalid_argument>(option, "too many modules");
    const auto noProcess = Module(nullptr, calls);
    RequireQueryRejected<std::invalid_argument>(RackOption(&noProcess, 1), "missing process handler");
    auto badSize = Module(Gain, calls);
    badSize.custom_module_option.size = 8;
    RequireQueryRejected<std::invalid_argument>(RackOption(&badSize, 1), "module option size 8");
    RequireThrows<std::runtime_error>([&] { sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, nullptr, &query); },
                                      "missing rack option");
    Require(calls.setupVoices.empty(), "no setup handler ran");
}};

const Case controlHandler{"CustomVoice_ModuleWithControlHandler_RejectsParamAndCleansUp", [] {
    Calls calls;
    auto withControl = Module(Gain, calls);
    withControl.control_handler = ControlHandler;
    const auto option = RackOption(&withControl, 1);
    {
        Ngs2Fixture ngs2;
        const auto system = ngs2.CreateSystem();
        const auto voice = Voice(ngs2.CreateRack(system, SCE_NGS2_RACK_ID_CUSTOM_SUBMIXER, &option.custom_rack_option.rack_option));
        RequireControlRejected<std::runtime_error>(voice, SCE_NGS2_CUSTOM_VOICE_PARAM_USER_FX2 | 0,
            Ngs2CustomVoiceUserFx2Param{{}, &halfGain, sizeof(halfGain)}, "param with a control handler");
        RequireDestroyed(system);
    }
    RequireEqual(calls.cleanupVoices == std::vector<std::uint32_t>{0, 1}, true, "cleanup voice order 0,1");
}};

} // namespace
