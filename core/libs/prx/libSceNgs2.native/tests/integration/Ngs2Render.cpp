#include "Ngs2Test.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace Ngs2Testing;
using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;
using Testing::RequireThrows;

constexpr int invalidSystemHandle = static_cast<int>(0x804A0230u);
constexpr int invalidRackHandle = static_cast<int>(0x804A0261u);
constexpr uintptr_t unknownHandle = 0x1234;
constexpr std::uint32_t playing = SCE_NGS2_VOICE_STATE_FLAG_INUSE | SCE_NGS2_VOICE_STATE_FLAG_PLAYING;

uintptr_t Sampler(Ngs2Fixture& ngs2, uintptr_t system, const std::vector<std::int16_t>& pcm, std::uint32_t repeats) {
    const auto voice = Voice(ngs2.CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP,
            Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 1, 48000, 0, 0, 0}});
    const Ngs2WaveformBlock block{0, pcm.size() * sizeof(std::int16_t), repeats, 0, static_cast<std::uint32_t>(pcm.size()), 0, 0x55};
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), 0, 1, &block});
    return voice;
}

struct CallbackRecorder {
    std::vector<std::uint32_t> flags;
    std::vector<std::uintptr_t> userData;
};

void APS5_VABI OnBlock(const Ngs2VoiceCallbackInfo* info) {
    auto& recorder = *reinterpret_cast<CallbackRecorder*>(info->callback_data);
    recorder.flags.push_back(info->flag);
    recorder.userData.push_back(info->user_data);
}

void RecordCallbacks(uintptr_t voice, CallbackRecorder& recorder, std::uint32_t flags) {
    Control(voice, SCE_NGS2_VOICE_PARAM_CALLBACK,
            Ngs2VoiceCallbackParam{{}, OnBlock, reinterpret_cast<std::uintptr_t>(&recorder), flags, 0});
}

void RequireCallbacks(const CallbackRecorder& recorder, const std::vector<std::uint32_t>& flags) {
    RequireEqual(recorder.flags.size(), flags.size(), "callback count");
    for (std::size_t i = 0; i < flags.size(); ++i) {
        RequireEqual(recorder.flags[i], flags[i], "callback " + std::to_string(i) + " flag");
        RequireEqual(recorder.userData[i], std::uintptr_t{0x55}, "callback " + std::to_string(i) + " block user data");
    }
}

std::vector<std::int16_t> RenderI16(uintptr_t system) {
    std::vector<std::int16_t> out(Grain, -1);
    const Ngs2RenderBufferInfo info{out.data(), out.size() * sizeof(std::int16_t), SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 1};
    RequireEqual(sceNgs2SystemRender(system, &info, 1), SCE_NGS2_OK, "render");
    return out;
}

std::vector<float> RenderF32(uintptr_t system, std::uint32_t channels) {
    std::vector<float> out(Grain * channels, -1.0f);
    const Ngs2RenderBufferInfo info{out.data(), out.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, channels};
    RequireEqual(sceNgs2SystemRender(system, &info, 1), SCE_NGS2_OK, "render");
    return out;
}

template<typename TSample>
void RequireSamples(const std::vector<TSample>& out, const std::vector<TSample>& expected, const char* message) {
    RequireEqual(out.size(), expected.size(), std::string(message) + " sample count");
    for (std::size_t i = 0; i < out.size(); ++i) RequireEqual(out[i], expected[i], std::string(message) + " sample " + std::to_string(i));
}

const Case resetOption{"SystemResetOption_Default_FillsDocumentedDefaults", [] {
    Ngs2SystemOption option{};
    RequireEqual(sceNgs2SystemResetOption(&option), SCE_NGS2_OK, "reset option");
    RequireEqual(option.size, static_cast<std::uint32_t>(sizeof(option)), "size");
    RequireEqual(option.max_grain_samples, 512u, "max grain samples");
    RequireEqual(option.num_grain_samples, 256u, "grain samples");
    RequireEqual(option.sample_rate, 48000u, "sample rate");
}};

const Case queryNullOutput{"QueryBufferSize_NullOutput_FailsInvalidOutAddress", [] {
    Ngs2SystemOption option{};
    RequireEqual(sceNgs2SystemResetOption(&option), SCE_NGS2_OK, "reset option");
    RequireEqual(sceNgs2SystemQueryBufferSize(&option, nullptr), SCE_NGS2_ERROR_INVALID_OUT_ADDRESS, "system query");
    RequireEqual(sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_SAMPLER, nullptr, nullptr), SCE_NGS2_ERROR_INVALID_OUT_ADDRESS,
                 "rack query");
}};

const Case unknownHandles{"Handles_Unknown_FailWithHandleErrors", [] {
    Ngs2SystemInfo info{};
    RequireEqual(sceNgs2SystemGetInfo(unknownHandle, &info, sizeof(info)), invalidSystemHandle, "system info");
    RequireEqual(sceNgs2RackDestroy(unknownHandle, nullptr), invalidRackHandle, "rack destroy");
    Ngs2RackInfo rackInfo{};
    RequireEqual(sceNgs2RackGetInfo(unknownHandle, &rackInfo, sizeof(rackInfo)), SCE_NGS2_ERROR_INVALID_RACK_HANDLE, "rack info");
}};

const Case rackInfoErrors{"RackGetInfo_NullOrShortOutput_Fails", [] {
    Ngs2Fixture ngs2;
    const auto rack = ngs2.CreateRack(ngs2.CreateSystem(), SCE_NGS2_RACK_ID_SAMPLER);
    Ngs2RackInfo rackInfo{};
    RequireEqual(sceNgs2RackGetInfo(rack, nullptr, sizeof(rackInfo)), SCE_NGS2_ERROR_INVALID_OUT_ADDRESS, "null output");
    RequireEqual(sceNgs2RackGetInfo(rack, &rackInfo, sizeof(rackInfo) - 1), SCE_NGS2_ERROR_INVALID_OUT_SIZE, "short output");
}};

const Case defaultRackInfo{"RackGetInfo_DefaultSamplerRack_ReportsDefaults", [] {
    Ngs2Fixture ngs2;
    const auto system = ngs2.CreateSystem();
    const auto rack = ngs2.CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER);
    Ngs2RackInfo rackInfo{};
    RequireEqual(sceNgs2RackGetInfo(rack, &rackInfo, sizeof(rackInfo)), SCE_NGS2_OK, "rack info");
    RequireEqual(rackInfo.rack_handle, rack, "rack handle");
    RequireEqual(rackInfo.owner_system_handle, system, "owner system");
    RequireEqual(rackInfo.rack_id, SCE_NGS2_RACK_ID_SAMPLER, "rack id");
    RequireEqual(rackInfo.type, 1u, "type");
    Require(rackInfo.uid != 0, "uid is not zero");
    RequireEqual(rackInfo.max_voices, 256u, "max voices");
    RequireEqual(rackInfo.max_channel_works, 256u, "max channel works");
    RequireEqual(rackInfo.max_grain_samples, 512u, "max grain samples");
    RequireEqual(rackInfo.max_ports, 8u, "max ports");
    RequireEqual(rackInfo.max_matrices, 1u, "max matrices");
    RequireEqual(rackInfo.active_voice_count, 0u, "active voices");
    RequireEqual(rackInfo.name[0], '\0', "empty name");
}};

const Case namedRackInfo{"RackGetInfo_NamedSubmixerRack_ReportsOptionsAndActiveVoices", [] {
    Ngs2Fixture ngs2;
    const auto system = ngs2.CreateSystem();
    const auto samplerRack = ngs2.CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER);
    Ngs2RackInfo samplerInfo{};
    RequireEqual(sceNgs2RackGetInfo(samplerRack, &samplerInfo, sizeof(samplerInfo)), SCE_NGS2_OK, "sampler rack info");
    Ngs2SubmixerRackOption named{};
    named.rack_option.size = sizeof(named);
    for (auto& c : named.rack_option.name) c = 'n';
    named.rack_option.max_grain_samples = 256;
    named.rack_option.max_voices = 2;
    named.rack_option.max_matrices = 1;
    named.rack_option.max_ports = 2;
    named.max_channels = 2;
    named.max_inputs = 3;
    Ngs2ContextBufferInfo namedQuery{};
    RequireEqual(sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_SUBMIXER, &named.rack_option, &namedQuery), SCE_NGS2_OK, "query");
    const auto namedBuffer = ngs2.Buffer(namedQuery);
    uintptr_t namedRack = 0;
    RequireEqual(sceNgs2RackCreate(system, SCE_NGS2_RACK_ID_SUBMIXER, &named.rack_option, &namedBuffer, &namedRack), SCE_NGS2_OK,
                 "create");
    Ngs2RackInfo namedInfo{};
    RequireEqual(sceNgs2RackGetInfo(namedRack, &namedInfo, sizeof(namedInfo)), SCE_NGS2_OK, "named rack info");
    RequireEqual(namedInfo.name[0], 'n', "name start");
    RequireEqual(namedInfo.name[62], 'n', "name end");
    RequireEqual(namedInfo.name[63], '\0', "name terminator");
    RequireEqual(namedInfo.type, 2u, "type");
    RequireEqual(namedInfo.rack_id, SCE_NGS2_RACK_ID_SUBMIXER, "rack id");
    Require(namedInfo.uid != samplerInfo.uid, "unique uid");
    RequireEqual(namedInfo.max_grain_samples, 256u, "max grain samples");
    RequireEqual(namedInfo.max_voices, 2u, "max voices");
    RequireEqual(namedInfo.max_ports, 2u, "max ports");
    RequireEqual(namedInfo.max_inputs, 3u, "max inputs");
    RequireEqual(namedInfo.max_channel_works, 0u, "max channel works");
    RequireEqual(namedInfo.buffer_info.host_buffer, namedBuffer.host_buffer, "buffer");
    RequireEqual(namedInfo.render_count, 0u, "render count");

    const auto named0 = Voice(namedRack);
    Control(named0, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, 2, 0});
    Event(named0, SCE_NGS2_VOICE_EVENT_PLAY);
    RequireEqual(sceNgs2RackGetInfo(namedRack, &namedInfo, sizeof(namedInfo)), SCE_NGS2_OK, "named rack info after play");
    RequireEqual(namedInfo.active_voice_count, 1u, "active voices after play");
    Ngs2VoiceChannelsInfo stereoInfo{};
    RequireEqual(sceNgs2VoiceQueryInfo(named0, SCE_NGS2_VOICE_INFO_CHANNELS, &stereoInfo, sizeof(stereoInfo)), SCE_NGS2_OK,
                 "channels info");
    RequireEqual(stereoInfo.num_channels, 2u, "channels");
}};

const Case systemInfo{"SystemGetInfo_SystemWithTwoRacks_ReportsState", [] {
    Ngs2Fixture ngs2;
    const auto system = ngs2.CreateSystem();
    ngs2.CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER);
    ngs2.CreateRack(system, SCE_NGS2_RACK_ID_SUBMIXER);
    Ngs2SystemInfo info{};
    RequireEqual(sceNgs2SystemGetInfo(system, nullptr, sizeof(info)), SCE_NGS2_ERROR_INVALID_OUT_ADDRESS, "null output");
    RequireEqual(sceNgs2SystemGetInfo(system, &info, sizeof(info) - 1), SCE_NGS2_ERROR_INVALID_OUT_SIZE, "short output");
    RequireEqual(sceNgs2SystemGetInfo(system, &info, sizeof(info)), SCE_NGS2_OK, "info");
    RequireEqual(info.system_handle, system, "handle");
    Require(info.uid != 0, "uid is not zero");
    RequireEqual(info.rack_count, 2u, "rack count");
    RequireEqual(info.sample_rate, 48000u, "sample rate");
    RequireEqual(info.num_grain_samples, Grain, "grain samples");
    RequireEqual(info.max_grain_samples, 512u, "max grain samples");
    RequireEqual(info.render_count, 0u, "render count");
}};

const Case systemDestroy{"SystemDestroy_Created_ReleasesBufferAndInvalidatesHandle", [] {
    Ngs2Fixture ngs2;
    const auto system = ngs2.CreateSystem();
    ngs2.CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER);
    Ngs2ContextBufferInfo released{};
    RequireEqual(sceNgs2SystemDestroy(system, &released), SCE_NGS2_OK, "destroy");
    Require(released.host_buffer != nullptr, "released buffer is reported");
    Ngs2SystemInfo info{};
    RequireEqual(sceNgs2SystemGetInfo(system, &info, sizeof(info)), SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE, "info after destroy");
}};

class PcmFixture {
public:
    PcmFixture() : system(ngs2.CreateSystem()), master(ngs2.Mastering(system, 1)), pcm(Pcm()),
                   sampler(Sampler(ngs2, system, pcm, 0)) {
        Patch(sampler, master);
    }

    static std::vector<std::int16_t> Pcm() {
        std::vector<std::int16_t> pcm;
        for (int i = 0; i < 12; i++) pcm.push_back(static_cast<std::int16_t>(i * 1000 - 4000));
        return pcm;
    }

    Ngs2Fixture ngs2;
    const uintptr_t system;
    const uintptr_t master;
    const std::vector<std::int16_t> pcm;
    const uintptr_t sampler;
};

const Case portInfo{"VoiceGetPortInfo_PatchedPort_ReportsDestinationMatrixAndVolume", [] {
    const PcmFixture fixture;
    Ngs2VoicePortInfo info{};
    RequireEqual(sceNgs2VoiceGetPortInfo(fixture.sampler, 0, nullptr, sizeof(info)), SCE_NGS2_ERROR_INVALID_OUT_ADDRESS, "null output");
    RequireEqual(sceNgs2VoiceGetPortInfo(fixture.sampler, 0, &info, sizeof(info) - 1), SCE_NGS2_ERROR_INVALID_OUT_SIZE, "short output");
    RequireEqual(sceNgs2VoiceGetPortInfo(fixture.sampler, 0, &info, sizeof(info)), SCE_NGS2_OK, "default port");
    RequireEqual(info.dest_handle, fixture.master, "destination");
    RequireEqual(info.matrix_id, -1, "no matrix");
    RequireEqual(info.volume, 1.0f, "unit volume");
    Control(fixture.sampler, SCE_NGS2_VOICE_PARAM_PORT_VOLUME, Ngs2VoicePortVolumeParam{{}, 0, 0.5f});
    Control(fixture.sampler, SCE_NGS2_VOICE_PARAM_PORT_MATRIX, Ngs2VoicePortMatrixParam{{}, 0, 0});
    RequireEqual(sceNgs2VoiceGetPortInfo(fixture.sampler, 0, &info, sizeof(info)), SCE_NGS2_OK, "changed port");
    RequireEqual(info.dest_handle, fixture.master, "changed destination");
    RequireEqual(info.matrix_id, 0, "matrix 0");
    RequireEqual(info.volume, 0.5f, "half volume");
    RequireEqual(sceNgs2VoiceGetPortInfo(fixture.sampler, 1, &info, sizeof(info)), SCE_NGS2_OK, "unpatched port");
    RequireEqual(info.dest_handle, uintptr_t{0}, "unpatched destination");
    RequireThrows<std::invalid_argument>([&] { sceNgs2VoiceGetPortInfo(fixture.sampler, 8, &info, sizeof(info)); }, "port 8");
}};

const Case queryInfo{"VoiceQueryInfo_Channels_ReportsMonoAndRejectsUnknownInfo", [] {
    const PcmFixture fixture;
    Ngs2VoiceChannelsInfo info{};
    RequireEqual(sceNgs2VoiceQueryInfo(fixture.sampler, SCE_NGS2_VOICE_INFO_CHANNELS, nullptr, sizeof(info)),
                 SCE_NGS2_ERROR_INVALID_OUT_ADDRESS, "null output");
    RequireEqual(sceNgs2VoiceQueryInfo(fixture.sampler, SCE_NGS2_VOICE_INFO_CHANNELS, &info, sizeof(info) - 1),
                 SCE_NGS2_ERROR_INVALID_OUT_SIZE, "short output");
    RequireEqual(sceNgs2VoiceQueryInfo(fixture.sampler, SCE_NGS2_VOICE_INFO_CHANNELS, &info, sizeof(info)), SCE_NGS2_OK, "channels");
    RequireEqual(info.num_channels, 1u, "mono");
    RequireEqual(info.reserved, 0u, "reserved");
    RequireThrows<std::runtime_error>([&] { sceNgs2VoiceQueryInfo(fixture.sampler, 0x4000, &info, sizeof(info)); }, "info 0x4000");
}};

const Case pcmBlockEnd{"SamplerRender_PcmBlock_PlaysDataThenSignalsBlockEnd", [] {
    PcmFixture fixture;
    CallbackRecorder recorder;
    Control(fixture.sampler, SCE_NGS2_VOICE_PARAM_PORT_VOLUME, Ngs2VoicePortVolumeParam{{}, 0, 0.5f});
    Control(fixture.sampler, SCE_NGS2_VOICE_PARAM_PORT_MATRIX, Ngs2VoicePortMatrixParam{{}, 0, 0});
    Control(fixture.sampler, SCE_NGS2_VOICE_PARAM_PORT_VOLUME, Ngs2VoicePortVolumeParam{{}, 0, 1.0f});
    Control(fixture.sampler, SCE_NGS2_VOICE_PARAM_PORT_MATRIX, Ngs2VoicePortMatrixParam{{}, 0, -1});
    RecordCallbacks(fixture.sampler, recorder, SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END);
    RequireEqual(Flags(fixture.sampler), 0u, "idle before play");
    Event(fixture.sampler, SCE_NGS2_VOICE_EVENT_PLAY);
    RequireEqual(Flags(fixture.sampler), SCE_NGS2_VOICE_STATE_FLAG_INUSE, "in use before render");

    const auto& pcm = fixture.pcm;
    RequireSamples(RenderI16(fixture.system), std::vector<std::int16_t>(pcm.begin(), pcm.begin() + Grain), "first grain");
    RequireEqual(Flags(fixture.sampler), playing, "playing after the first grain");
    RequireCallbacks(recorder, {});

    Ngs2SamplerVoiceState state{};
    RequireEqual(sceNgs2VoiceGetState(fixture.sampler, &state.voice_state, sizeof(state) - 8), SCE_NGS2_ERROR_INVALID_OUT_SIZE,
                 "short state");
    RequireEqual(sceNgs2VoiceGetState(fixture.sampler, &state.voice_state, sizeof(state)), SCE_NGS2_OK, "state");
    RequireEqual(state.num_decoded_samples, static_cast<std::uint64_t>(Grain), "decoded samples");
    RequireEqual(state.decoded_data_size, static_cast<std::uint64_t>(Grain * 2), "decoded bytes");
    RequireEqual(state.user_data, std::uintptr_t{0x55}, "block user data");
    RequireEqual(static_cast<const void*>(state.waveform_data), static_cast<const void*>(pcm.data() + Grain), "waveform position");

    std::vector<std::int16_t> tail(Grain, 0);
    std::copy(pcm.begin() + Grain, pcm.end(), tail.begin());
    RequireSamples(RenderI16(fixture.system), tail, "second grain");
    RequireCallbacks(recorder, {SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END});
    RequireEqual(Flags(fixture.sampler), 0u, "stopped");
    RequireEqual(sceNgs2VoiceGetState(fixture.sampler, &state.voice_state, sizeof(state)), SCE_NGS2_OK, "final state");
    RequireEqual(state.num_decoded_samples, static_cast<std::uint64_t>(pcm.size()), "all samples decoded");
    RequireEqual(static_cast<const void*>(state.waveform_data), static_cast<const void*>(pcm.data() + pcm.size()), "end position");
}};

const Case pitchAndRepeat{"SamplerRender_HalfPitchRepeatedBlock_InterpolatesAndSignalsRepeat", [] {
    Ngs2Fixture ngs2;
    CallbackRecorder recorder;
    const auto system = ngs2.CreateSystem();
    const auto master = ngs2.Mastering(system, 1);
    const std::vector<std::int16_t> pcm{0, 1000, 2000, 3000};
    const auto sampler = Sampler(ngs2, system, pcm, 1);
    Patch(sampler, master);
    Control(sampler, SCE_NGS2_SAMPLER_VOICE_PARAM_PITCH, Ngs2SamplerVoicePitchParam{{}, 0.5f});
    RecordCallbacks(sampler, recorder, SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END | SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_REPEAT);
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);
    RequireSamples(RenderI16(system), {0, 500, 1000, 1500, 2000, 2500, 3000, 1500}, "first grain");
    RequireCallbacks(recorder, {SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_REPEAT});
    RequireSamples(RenderI16(system), {0, 500, 1000, 1500, 2000, 2500, 3000, 3000}, "second grain");
    RequireCallbacks(recorder, {SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_REPEAT, SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END});
    RequireEqual(Flags(sampler), 0u, "stopped");
}};

const Case submixerMatrix{"SubmixerRender_MatrixAndPortVolume_ScaleChannelsThenSilence", [] {
    Ngs2Fixture ngs2;
    const auto system = ngs2.CreateSystem();
    const auto master = ngs2.Mastering(system, 2);
    const auto submixer = Voice(ngs2.CreateRack(system, SCE_NGS2_RACK_ID_SUBMIXER));
    Control(submixer, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, 2, 0});
    Patch(submixer, master);
    const Ngs2VoiceCommand play{2, 0, 4, 0, {.u = SCE_NGS2_VOICE_EVENT_PLAY}};
    RequireEqual(sceNgs2VoiceRunCommands(submixer, &play, 1), SCE_NGS2_OK, "play the submixer");
    const std::vector<std::int16_t> pcm(Grain, 16384);
    const auto sampler = Sampler(ngs2, system, pcm, 0);
    Patch(sampler, submixer);
    const float levels[2] = {1.0f, 0.5f};
    Control(sampler, SCE_NGS2_VOICE_PARAM_MATRIX_LEVELS, Ngs2VoiceMatrixLevelsParam{{}, 0, 2, levels});
    Control(sampler, SCE_NGS2_VOICE_PARAM_PORT_MATRIX, Ngs2VoicePortMatrixParam{{}, 0, 0});
    Control(sampler, SCE_NGS2_VOICE_PARAM_PORT_VOLUME, Ngs2VoicePortVolumeParam{{}, 0, 0.5f});
    RequireEqual(sceNgs2VoiceRunCommands(sampler, &play, 1), SCE_NGS2_OK, "play the sampler");

    const auto out = RenderF32(system, 2);
    for (std::uint32_t i = 0; i < Grain; i++) {
        RequireEqual(out[i * 2], 0.25f, "left frame " + std::to_string(i));
        RequireEqual(out[i * 2 + 1], 0.125f, "right frame " + std::to_string(i));
    }
    Ngs2SubmixerVoiceState state{};
    RequireEqual(sceNgs2VoiceGetState(submixer, &state.voice_state, sizeof(state)), SCE_NGS2_OK, "submixer state");
    RequireEqual(state.voice_state.state_flags, playing, "submixer playing");
    for (float sample : RenderF32(system, 2)) RequireEqual(sample, 0.0f, "silence after the data");
}};

std::vector<float> MatrixLevelFrame(const std::vector<float>& levels, bool command) {
    Ngs2Fixture ngs2;
    const auto system = ngs2.CreateSystem();
    const auto master = ngs2.Mastering(system, 2);
    const std::vector<std::int16_t> pcm(Grain, 16384);
    const auto sampler = Sampler(ngs2, system, pcm, 0);
    Patch(sampler, master);
    if (command) {
        const Ngs2VoiceCommand set{5, 0, 0x11, static_cast<std::uint16_t>(levels.size()), {.levels = levels.data()}};
        RequireEqual(sceNgs2VoiceRunCommands(sampler, &set, 1), SCE_NGS2_OK, "set levels by command");
    } else {
        Control(sampler, SCE_NGS2_VOICE_PARAM_MATRIX_LEVELS,
                Ngs2VoiceMatrixLevelsParam{{}, 0, static_cast<std::uint32_t>(levels.size()), levels.data()});
    }
    Control(sampler, SCE_NGS2_VOICE_PARAM_PORT_MATRIX, Ngs2VoicePortMatrixParam{{}, 0, 0});
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);
    const auto out = RenderF32(system, 2);
    return {out[0], out[1]};
}

const Case matrixLevelClamp{"MatrixLevels_OutOfRange_AreClampedToPlusMinusFour", [] {
    const float inf = INFINITY;
    struct Clamp {
        std::vector<float> levels;
        std::vector<float> expected;
    };
    const Clamp clamps[] = {
        {{4.0f, -4.0f}, {2.0f, -2.0f}},
        {{4.0001f, -4.5f}, {2.0f, -2.0f}},
        {{100.0f, -100.0f}, {2.0f, -2.0f}},
        {{inf, -inf}, {2.0f, -2.0f}},
        {{3.5f, -0.25f}, {1.75f, -0.125f}},
    };
    for (bool command : {false, true}) {
        for (const auto& clamp : clamps) {
            const auto name = std::string(command ? "command" : "param") + " levels " + std::to_string(clamp.levels[0]) + "," +
                              std::to_string(clamp.levels[1]);
            RequireSamples(MatrixLevelFrame(clamp.levels, command), clamp.expected, name.c_str());
        }
    }
}};

const Case matrixLevelNan{"MatrixLevels_NaN_PropagatesWithoutAffectingOtherChannels", [] {
    for (bool command : {false, true}) {
        const auto frame = MatrixLevelFrame({NAN, 0.5f}, command);
        Require(std::isnan(frame[0]), std::string(command ? "command" : "param") + " NaN level stays NaN");
        RequireEqual(frame[1], 0.25f, std::string(command ? "command" : "param") + " second channel");
    }
}};

const Case matrixLevelEmpty{"MatrixLevels_ZeroLevels_AreRejected", [] {
    Ngs2Fixture ngs2;
    const auto system = ngs2.CreateSystem();
    const std::vector<std::int16_t> silence(Grain, 0);
    const auto sampler = Sampler(ngs2, system, silence, 0);
    const float levels[2] = {1.0f, 1.0f};
    RequireThrows<std::invalid_argument>(
        [&] { Control(sampler, SCE_NGS2_VOICE_PARAM_MATRIX_LEVELS, Ngs2VoiceMatrixLevelsParam{{}, 0, 0, levels}); }, "zero levels");
}};

struct ReverbFrame {
    std::uint32_t frame;
    std::vector<float> samples;
};

struct ReverbCase {
    std::uint32_t channels;
    float amplitude;
    bool noise;
    Ngs2ReverbI3DL2Param params;
    std::int32_t changeGrain;
    Ngs2ReverbI3DL2Param change;
    std::vector<ReverbFrame> expected;
};

std::vector<float> RenderReverb(const ReverbCase& reverbCase, std::uint32_t frames) {
    Ngs2Fixture ngs2;
    const auto system = ngs2.CreateSystem();
    RequireEqual(sceNgs2SystemSetGrainSamples(system, 256), SCE_NGS2_OK, "grain 256");
    const auto channels = reverbCase.channels;
    const auto master = ngs2.Mastering(system, channels);
    const auto reverb = Voice(ngs2.CreateRack(system, SCE_NGS2_RACK_ID_REVERB));
    Control(reverb, SCE_NGS2_REVERB_VOICE_PARAM_SETUP, Ngs2ReverbVoiceSetupParam{{}, channels, channels, 0, 0});
    Control(reverb, SCE_NGS2_REVERB_VOICE_PARAM_I3DL2, Ngs2ReverbVoiceI3DL2Param{{}, reverbCase.params});
    Patch(reverb, master);
    Event(reverb, SCE_NGS2_VOICE_EVENT_PLAY);
    std::vector<std::int16_t> pcm(4096 * channels, 0);
    std::uint32_t seed = 1;
    for (std::uint32_t i = 0; reverbCase.noise && i + 3 * channels < pcm.size(); i++) {
        seed = seed * 1664525u + 1013904223u;
        if (i < 3000 * channels) {
            pcm[i + 3 * channels] = static_cast<std::int16_t>(static_cast<std::int32_t>((seed >> 16) & 0x3fff) - 0x2000);
        }
    }
    for (std::uint32_t c = 0; !reverbCase.noise && c < channels; c++) {
        pcm[3 * channels + c] = static_cast<std::int16_t>((16384 - 4000 * static_cast<std::int32_t>(c)) * reverbCase.amplitude);
    }
    const auto sampler = Voice(ngs2.CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
    Control(sampler, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP,
            Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, channels, 48000, 0, 0, 0}});
    const Ngs2WaveformBlock block{0, pcm.size() * sizeof(std::int16_t), 0, 0, 4096, 0, 0};
    Control(sampler, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), 0, 1, &block});
    Patch(sampler, reverb);
    std::vector<float> identity(channels * channels, 0.0f);
    for (std::uint32_t c = 0; c < channels; c++) identity[c * channels + c] = 1.0f;
    Control(sampler, SCE_NGS2_VOICE_PARAM_MATRIX_LEVELS, Ngs2VoiceMatrixLevelsParam{{}, 0, channels * channels, identity.data()});
    Control(sampler, SCE_NGS2_VOICE_PARAM_PORT_MATRIX, Ngs2VoicePortMatrixParam{{}, 0, 0});
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);
    std::vector<float> all;
    std::vector<float> out(256 * channels);
    for (std::uint32_t grain = 0; grain * 256 < frames; grain++) {
        if (static_cast<std::int32_t>(grain) == reverbCase.changeGrain) {
            Control(reverb, SCE_NGS2_REVERB_VOICE_PARAM_I3DL2, Ngs2ReverbVoiceI3DL2Param{{}, reverbCase.change});
        }
        const Ngs2RenderBufferInfo info{out.data(), out.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, channels};
        RequireEqual(sceNgs2SystemRender(system, &info, 1), SCE_NGS2_OK, "render grain " + std::to_string(grain));
        all.insert(all.end(), out.begin(), out.end());
    }
    return all;
}

const Case reverb{"ReverbRender_I3dl2Presets_MatchReferenceFrames", [] {
    const Ngs2ReverbI3DL2Param cave{1.0f, 0.0f, -1000, 0, 0, 2.91f, 1.3f, -602, 0.015f, -302, 0.022f, 100.0f, 100.0f, 5000.0f, {}};
    auto changed = cave;
    changed.reflection_pattern = 5;
    changed.decay_time = 0.7f;
    changed.decay_hf_ratio = 0.6f;
    changed.reflections_delay = 0.004f;
    changed.reverb_delay = 0.03f;
    changed.room_hf = -900;
    changed.wet = 0.6f;
    changed.dry = 0.4f;
    auto denser = cave;
    denser.density = 99.999f;
    const std::vector<ReverbCase> cases = {
        {2, 1.0f, false, cave, -1, {}, {{100, {0, 0}}, {723, {0.0687032491f, 0.0687032491f}}, {1055, {0.0392590016f, 0.0392590016f}}, {1417, {0.0131517649f, 0.0131517649f}},
            {1779, {0.0098147504f, 0.0098147504f}}, {2500, {0, 0}}, {4000, {1.45101382e-07f, -1.15403088e-06f}}, {6000, {0.000655628915f, 3.03641864e-05f}},
            {9000, {0.000400578661f, -0.000684358703f}}, {12000, {0.000191548112f, 0.000220489033f}}, {16000, {0.000210488462f, 0.000146183695f}},
            {20000, {2.41150738e-05f, -0.000864534522f}}, {26000, {0.000125532999f, -0.000234790568f}}, {32000, {-0.000462158496f, 4.56995949e-05f}}}},
        {2, 1.0f, false, {1.0f, 0.0f, -500, -800, 3, 1.2f, 0.5f, -300, 0.01f, 0, 0.03f, 80.0f, 60.0f, 3000.0f, {}}, -1, {},
            {{100, {0, 0}}, {723, {0.000166983227f, 0.000166983227f}}, {1055, {-3.41919585e-05f, -3.41919585e-05f}}, {1417, {0, 0}}, {1779, {0, 0}},
            {2500, {0, 1.55222626e-08f}}, {4000, {-0.000106364969f, 9.90879998e-05f}}, {6000, {0.00013573296f, 7.22560799e-06f}},
            {9000, {-7.91876082e-05f, 0.000225067721f}}, {12000, {0.000150469248f, 0.00036288987f}}, {16000, {-7.86288219e-05f, -0.000195921995f}},
            {20000, {1.01910318e-05f, 5.79599109e-05f}}, {26000, {2.14232614e-06f, -2.05506512e-05f}}, {32000, {-4.31467561e-06f, 2.34100444e-05f}}}},
        {6, 1.0f, false, {1.0f, 0.2f, -200, -300, 12, 1.0f, 0.9f, -100, 0.03f, -100, 0.04f, 70.0f, 90.0f, 4000.0f, {}}, -1, {},
            {{6000, {-0.000167909253f, -0.000179917421f, -0.000647808949f, 0, 3.22720189e-05f, 0.000213803389f}},
            {9000, {8.47857882e-05f, 8.62322413e-05f, -0.000100753634f, 0, 0.000109430795f, -0.000122895101f}},
            {12000, {4.37544441e-05f, 3.58103216e-06f, -0.00013746327f, 0, 9.3237948e-05f, -0.00013529828f}},
            {16000, {8.63046807e-08f, 1.62214419e-05f, 8.89167641e-05f, 0, 1.02178528e-05f, 0.000201705057f}},
            {20000, {-9.6526619e-06f, -2.73812566e-05f, -1.92096536e-07f, 0, -0.000116049101f, 6.94858772e-06f}},
            {26000, {1.27750207e-06f, -1.59169917e-06f, 5.30674515e-05f, 0, -3.59285696e-05f, 5.16466662e-06f}},
            {32000, {3.5737969e-06f, 4.19655044e-06f, -2.30460955e-07f, 0, 1.16596939e-05f, 1.14889262e-05f}}}},
        {2, 1.0f, false, cave, 6, changed, {{723, {0.0687032491f, 0.0687032491f}}, {1055, {0.0392590016f, 0.0392590016f}}, {1417, {0.0131517649f, 0.0131517649f}},
            {1779, {4.38156894e-05f, 4.38156894e-05f}}, {2500, {0, 0}}, {4000, {-1.60370582e-05f, -4.44955149e-05f}}, {6000, {1.02755057e-05f, 8.34046841e-06f}},
            {9000, {1.1641363e-05f, -4.69685165e-06f}}, {12000, {3.57917179e-06f, -2.34550089e-06f}}, {16000, {-8.00212234e-08f, 3.19533115e-06f}},
            {20000, {5.66619519e-07f, 4.26969848e-07f}}, {26000, {1.46052287e-07f, -2.44572629e-08f}}, {32000, {-4.45668853e-08f, 1.51349102e-08f}}}},
        {2, 0.02f, false, {40.0f, -3.0f, 5000, 500, 20, 50.0f, 5.0f, 3000, 0.9f, 5000, 0.9f, 300.0f, 300.0f, 50000.0f, {}}, -1, {},
            {{14403, {0.877262115f, 0.877262115f}}, {14500, {0, 0}}, {15000, {0, 0}}, {15500, {0, 7.50572099e-07f}}, {16000, {4.95207075e-07f, 6.37215771e-06f}},
            {20000, {0.00482429564f, -0.00149071764f}}, {26000, {-0.0096597122f, 0.00718426611f}}, {32000, {0.0107425917f, -0.000738018774f}}}},
        {2, 1.0f, false, {1.0f, 0.3f, 0, -200, 15, 0.8f, 1.0f, 0, 0.002f, -500, 0.004f, 60.0f, 80.0f, 6000.0f, {}}, -1, {},
            {{60, {0, 0}}, {99, {0.0947210863f, 0.0947210863f}}, {100, {0.0472047739f, 0.0472047739f}}, {150, {0, 0}}, {200, {0, 0}}, {300, {0, 0}},
            {2000, {-9.86956729e-06f, 7.96923473e-07f}}, {3000, {-0.000152820328f, -4.59704825e-06f}}, {5000, {0.000203833857f, -0.000208990226f}},
            {8000, {-3.69777154e-05f, -0.000259475899f}}}},
        {2, 1.0f, true, cave, 6, changed, {{1540, {0.0222691782f, 0.0242232569f}}, {1600, {0.0201471522f, 0.0186609477f}}, {1700, {-0.0211425349f, -0.0235042125f}},
            {1780, {-0.0168117173f, 0.00417749817f}}, {1800, {-0.0866222829f, -0.0945812687f}}, {2000, {0.0131965755f, -0.0707511827f}},
            {2500, {0.0942443311f, -0.100531064f}}, {3500, {0.000985965831f, 0.000946713379f}}, {8000, {-0.000968122098f, 0.000556072395f}},
            {15000, {0.000227724231f, 0.000151095577f}}}},
        {2, 1.0f, true, cave, 20, denser, {{5200, {-0.00942458585f, -0.0064059021f}}, {6000, {0.00248076022f, 0.00317249796f}},
            {8000, {0.00102439919f, 0.00105210941f}}, {12000, {1.63059376e-05f, 6.10309735e-06f}}, {20000, {-1.08853078e-06f, -2.99162899e-08f}}}},
    };
    for (std::size_t index = 0; index < cases.size(); ++index) {
        const auto& reverbCase = cases[index];
        const auto out = RenderReverb(reverbCase, 32256);
        for (const auto& expected : reverbCase.expected) {
            for (std::uint32_t c = 0; c < reverbCase.channels; c++) {
                if (c == 3) continue;
                const float value = out[expected.frame * reverbCase.channels + c];
                Require(std::fabs(value - expected.samples[c]) <= 2e-7f + std::fabs(expected.samples[c]) * 1e-5f,
                        "case " + std::to_string(index) + " frame " + std::to_string(expected.frame) + " channel " +
                        std::to_string(c) + ": expected " + std::to_string(expected.samples[c]) + ", got " + std::to_string(value));
            }
        }
    }
}};

const Case reverbSetupRejected{"ReverbVoice_UnsupportedSetupOrNaNParams_AreRejected", [] {
    Ngs2Fixture ngs2;
    const auto system = ngs2.CreateSystem();
    const auto reverb = Voice(ngs2.CreateRack(system, SCE_NGS2_RACK_ID_REVERB));
    for (std::uint32_t channels : {3u, 4u, 5u, 7u}) {
        RequireThrows<std::invalid_argument>(
            [&] { Control(reverb, SCE_NGS2_REVERB_VOICE_PARAM_SETUP, Ngs2ReverbVoiceSetupParam{{}, channels, channels, 0, 0}); },
            std::to_string(channels) + " channels");
    }
    RequireThrows<std::runtime_error>(
        [&] { Control(reverb, SCE_NGS2_REVERB_VOICE_PARAM_SETUP, Ngs2ReverbVoiceSetupParam{{}, 1, 2, 0, 0}); }, "channel conversion");
    RequireThrows<std::runtime_error>(
        [&] { Control(reverb, SCE_NGS2_REVERB_VOICE_PARAM_SETUP, Ngs2ReverbVoiceSetupParam{{}, 2, 2, 1, 0}); }, "setup flags");
    Control(reverb, SCE_NGS2_REVERB_VOICE_PARAM_SETUP, Ngs2ReverbVoiceSetupParam{{}, 2, 2, 0, 0});
    Ngs2ReverbI3DL2Param invalid{1.0f, 0.0f, 0, 0, 0, 1.0f, 1.0f, 0, 0.0f, 0, 0.0f, 100.0f, 100.0f, 5000.0f, {}};
    invalid.decay_time = NAN;
    RequireThrows<std::invalid_argument>(
        [&] { Control(reverb, SCE_NGS2_REVERB_VOICE_PARAM_I3DL2, Ngs2ReverbVoiceI3DL2Param{{}, invalid}); }, "NaN decay time");
}};

class DefaultReverbFixture {
public:
    DefaultReverbFixture() : system(ngs2.CreateSystem()), master(ngs2.Mastering(system, 2)),
                             reverb(Voice(ngs2.CreateRack(system, SCE_NGS2_RACK_ID_REVERB))), pcm(Grain * 64, 16384) {
        Control(reverb, SCE_NGS2_REVERB_VOICE_PARAM_SETUP, Ngs2ReverbVoiceSetupParam{{}, 2, 2, 0, 0});
        Patch(reverb, master);
        Event(reverb, SCE_NGS2_VOICE_EVENT_PLAY);
        const auto sampler = Sampler(ngs2, system, pcm, 0);
        Patch(sampler, reverb);
        Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);
    }

    Ngs2Fixture ngs2;
    const uintptr_t system;
    const uintptr_t master;
    const uintptr_t reverb;
    const std::vector<std::int16_t> pcm;
};

const Case reverbDefaults{"ReverbRender_DefaultParams_ProducesFaintTail", [] {
    const DefaultReverbFixture fixture;
    std::vector<float> out(Grain * 2);
    const Ngs2RenderBufferInfo info{out.data(), out.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 2};
    float loudest = 0.0f;
    for (std::uint32_t grain = 0; grain < 600; grain++) {
        RequireEqual(sceNgs2SystemRender(fixture.system, &info, 1), SCE_NGS2_OK, "render grain " + std::to_string(grain));
        for (float sample : out) loudest = std::max(loudest, std::fabs(sample));
    }
    Require(loudest > 0.0f && loudest < 1e-8f, "loudest sample in (0, 1e-8): " + std::to_string(loudest));
    Ngs2VoiceState state{};
    RequireEqual(sceNgs2VoiceGetState(fixture.reverb, &state, sizeof(state)), SCE_NGS2_OK, "reverb state");
    Require((state.state_flags & SCE_NGS2_VOICE_STATE_FLAG_INUSE) != 0, "reverb in use");
}};

const Case reverbLowRate{"ReverbRender_SampleRateBelowSupported_Throws", [] {
    const DefaultReverbFixture fixture;
    RequireEqual(sceNgs2SystemSetSampleRate(fixture.system, 7000), SCE_NGS2_OK, "set 7000 Hz");
    std::vector<float> out(Grain * 2);
    const Ngs2RenderBufferInfo info{out.data(), out.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 2};
    RequireThrows<std::runtime_error>([&] { sceNgs2SystemRender(fixture.system, &info, 1); }, "render at 7000 Hz");
}};

const Case reverbRackSize{"ReverbRackQueryBufferSize_UnsupportedReverbSize_Throws", [] {
    Ngs2ReverbRackOption option{};
    option.rack_option.size = sizeof(option);
    option.rack_option.max_grain_samples = 512;
    option.rack_option.max_voices = 1;
    option.rack_option.max_input_delay_blocks = 1;
    option.rack_option.max_matrices = 1;
    option.rack_option.max_ports = 8;
    option.max_channels = 8;
    option.reverb_size = 1;
    Ngs2ContextBufferInfo query{};
    RequireEqual(sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_REVERB, &option.rack_option, &query), SCE_NGS2_OK, "reverb size 1");
    for (std::uint32_t size : {0u, 2u}) {
        option.reverb_size = size;
        RequireThrows<std::runtime_error>([&] { sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_REVERB, &option.rack_option, &query); },
                                          "reverb size " + std::to_string(size));
    }
}};

const Case sampleRateErrors{"SystemSetSampleRate_UnknownHandleOrZero_Fails", [] {
    Ngs2Fixture ngs2;
    const auto system = ngs2.CreateSystem();
    RequireEqual(sceNgs2SystemSetSampleRate(unknownHandle, 96000), SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE, "unknown handle");
    RequireThrows<std::invalid_argument>([&] { sceNgs2SystemSetSampleRate(system, 0); }, "zero rate");
}};

const Case sampleRate{"SystemSetSampleRate_DoubleRate_HalvesSamplerStep", [] {
    Ngs2Fixture ngs2;
    const auto system = ngs2.CreateSystem();
    RequireEqual(sceNgs2SystemSetSampleRate(system, 96000), SCE_NGS2_OK, "set 96000 Hz");
    Ngs2SystemInfo info{};
    RequireEqual(sceNgs2SystemGetInfo(system, &info, sizeof(info)), SCE_NGS2_OK, "info");
    RequireEqual(info.sample_rate, 96000u, "reported rate");
    const auto master = ngs2.Mastering(system, 1);
    const std::vector<std::int16_t> pcm{0, 1000, 2000, 3000};
    const auto sampler = Sampler(ngs2, system, pcm, 1);
    Patch(sampler, master);
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);
    RequireSamples(RenderI16(system), {0, 500, 1000, 1500, 2000, 2500, 3000, 1500}, "rendered");
}};

const Case userDataUnknown{"SystemUserData_UnknownHandle_FailsAndKeepsOutput", [] {
    uintptr_t value = 1;
    RequireEqual(sceNgs2SystemSetUserData(unknownHandle, 5), SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE, "set");
    RequireEqual(sceNgs2SystemGetUserData(unknownHandle, &value), SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE, "get");
    RequireEqual(value, uintptr_t{1}, "output untouched");
}};

const Case userData{"SystemUserData_SetThenGet_RoundTrips", [] {
    Ngs2Fixture ngs2;
    const auto system = ngs2.CreateSystem();
    uintptr_t value = 1;
    RequireEqual(sceNgs2SystemGetUserData(system, &value), SCE_NGS2_OK, "get default");
    RequireEqual(value, uintptr_t{0}, "default user data");
    RequireEqual(sceNgs2SystemSetUserData(system, 0xfedcba9876543210), SCE_NGS2_OK, "set");
    RequireEqual(sceNgs2SystemGetUserData(system, &value), SCE_NGS2_OK, "get");
    RequireEqual(value, uintptr_t{0xfedcba9876543210}, "stored user data");
    RequireThrows<std::invalid_argument>([&] { sceNgs2SystemGetUserData(system, nullptr); }, "null output");
}};

const Case masteringGain{"MasteringRender_GainAndLfeOutputs_ScaleEachLayout", [] {
    Ngs2Fixture ngs2;
    const auto system = ngs2.CreateSystem();
    const auto stereo = ngs2.Mastering(system, 2);
    const auto surround = ngs2.Mastering(system, 6);
    const auto quad = ngs2.Mastering(system, 4);
    Control(stereo, SCE_NGS2_MASTERING_VOICE_PARAM_GAIN, Ngs2MasteringVoiceGainParam{{}, 0.5f, 1.0f});
    Control(surround, SCE_NGS2_MASTERING_VOICE_PARAM_GAIN, Ngs2MasteringVoiceGainParam{{}, 0.5f, 0.25f});
    Control(surround, SCE_NGS2_MASTERING_VOICE_PARAM_OUTPUT, Ngs2MasteringVoiceOutputParam{{}, 1});
    Control(quad, SCE_NGS2_MASTERING_VOICE_PARAM_GAIN, Ngs2MasteringVoiceGainParam{{}, 0.5f, 0.25f});
    Control(quad, SCE_NGS2_MASTERING_VOICE_PARAM_OUTPUT, Ngs2MasteringVoiceOutputParam{{}, 2});
    const std::vector<std::int16_t> pcm(Grain, 16384);
    const float stereoLevels[2] = {1.0f, 0.5f};
    const float surroundLevels[6] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    const struct {
        uintptr_t master;
        const float* levels;
        std::uint32_t count;
    } routes[] = {{stereo, stereoLevels, 2}, {surround, surroundLevels, 6}, {quad, surroundLevels, 4}};
    for (const auto& route : routes) {
        const auto source = Sampler(ngs2, system, pcm, 0);
        Control(source, SCE_NGS2_VOICE_PARAM_MATRIX_LEVELS, Ngs2VoiceMatrixLevelsParam{{}, 0, route.count, route.levels});
        Control(source, SCE_NGS2_VOICE_PARAM_PORT_MATRIX, Ngs2VoicePortMatrixParam{{}, 0, 0});
        Patch(source, route.master);
        Event(source, SCE_NGS2_VOICE_EVENT_PLAY);
    }
    bool rejected = false;
    try {
        Control(stereo, SCE_NGS2_MASTERING_VOICE_PARAM_GAIN, Ngs2MasteringVoiceGainParam{{}, NAN, 1.0f});
    } catch (const Testing::Failure&) {
        throw;
    } catch (const std::exception&) {
        rejected = true;
    }
    Require(rejected, "NaN gain is rejected");

    std::vector<float> outStereo(Grain * 2, -1.0f);
    std::vector<float> outSurround(Grain * 6, -1.0f);
    std::vector<float> outQuad(Grain * 4, -1.0f);
    const Ngs2RenderBufferInfo info[3] = {
        {outStereo.data(), outStereo.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 2},
        {outSurround.data(), outSurround.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 6},
        {outQuad.data(), outQuad.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 4},
    };
    RequireEqual(sceNgs2SystemRender(system, info, 3), SCE_NGS2_OK, "render three outputs");
    for (std::uint32_t i = 0; i < Grain; i++) {
        const auto frame = " frame " + std::to_string(i);
        RequireEqual(outStereo[i * 2], 0.25f, "stereo left" + frame);
        RequireEqual(outStereo[i * 2 + 1], 0.125f, "stereo right" + frame);
        for (std::uint32_t channel = 0; channel < 6; channel++) {
            RequireEqual(outSurround[i * 6 + channel], channel == 3 ? 0.125f : 0.25f, "surround channel " + std::to_string(channel) + frame);
        }
        for (std::uint32_t channel = 0; channel < 4; channel++) {
            RequireEqual(outQuad[i * 4 + channel], 0.25f, "quad channel " + std::to_string(channel) + frame);
        }
    }
}};

const Case masteringClip{"MasteringRender_GainAboveUnity_ClipsUntilSetupResetsGain", [] {
    Ngs2Fixture ngs2;
    const auto system = ngs2.CreateSystem();
    const auto loud = ngs2.Mastering(system, 1);
    Control(loud, SCE_NGS2_MASTERING_VOICE_PARAM_GAIN, Ngs2MasteringVoiceGainParam{{}, 2.0f, 2.0f});
    const std::vector<std::int16_t> pcm(Grain * 2, 24576);
    const auto source = Sampler(ngs2, system, pcm, 0);
    Patch(source, loud);
    Event(source, SCE_NGS2_VOICE_EVENT_PLAY);
    RequireSamples(RenderI16(system), std::vector<std::int16_t>(Grain, 32767), "clipped");
    Control(loud, SCE_NGS2_MASTERING_VOICE_PARAM_SETUP, Ngs2MasteringVoiceSetupParam{{}, 1, 0});
    Event(loud, SCE_NGS2_VOICE_EVENT_PLAY);
    RequireSamples(RenderI16(system), std::vector<std::int16_t>(Grain, 24576), "after setup");
}};

uintptr_t MasteringRack(Ngs2Fixture& ngs2, uintptr_t system, std::uint32_t voices) {
    Ngs2MasteringRackOption option{};
    option.rack_option.size = sizeof(option);
    option.rack_option.max_grain_samples = 512;
    option.rack_option.max_voices = voices;
    option.rack_option.max_input_delay_blocks = 1;
    option.rack_option.max_matrices = 1;
    option.rack_option.max_ports = 8;
    option.max_channels = 8;
    return ngs2.CreateRack(system, SCE_NGS2_RACK_ID_MASTERING, &option.rack_option);
}

void StereoSource(uintptr_t voice, const std::vector<std::int16_t>& pcm, uintptr_t master) {
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP,
            Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 1, 48000, 0, 0, 0}});
    const Ngs2WaveformBlock block{0, pcm.size() * sizeof(std::int16_t), 0, 0, static_cast<std::uint32_t>(pcm.size()), 0, 0};
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), 0, 1, &block});
    const float levels[2] = {1.0f, 0.5f};
    Control(voice, SCE_NGS2_VOICE_PARAM_MATRIX_LEVELS, Ngs2VoiceMatrixLevelsParam{{}, 0, 2, levels});
    Control(voice, SCE_NGS2_VOICE_PARAM_PORT_MATRIX, Ngs2VoicePortMatrixParam{{}, 0, 0});
    Patch(voice, master);
}

class StereoMastersFixture {
public:
    StereoMastersFixture() : system(ngs2.CreateSystem()), masteringRack(MasteringRack(ngs2, system, 3)),
                             samplerRack(ngs2.CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER)), pcm(Grain, 16384) {
        for (std::uint32_t i = 0; i < 3; i++) {
            masters[i] = RackVoice(masteringRack, i);
            Control(masters[i], SCE_NGS2_MASTERING_VOICE_PARAM_SETUP, Ngs2MasteringVoiceSetupParam{{}, 2, 0});
            Control(masters[i], SCE_NGS2_MASTERING_VOICE_PARAM_OUTPUT, Ngs2MasteringVoiceOutputParam{{}, i, 0});
            Event(masters[i], SCE_NGS2_VOICE_EVENT_PLAY);
        }
        for (std::uint32_t i = 0; i < 2; i++) {
            const auto source = RackVoice(samplerRack, i);
            StereoSource(source, pcm, masters[i]);
            Event(source, SCE_NGS2_VOICE_EVENT_PLAY);
        }
        info[0] = {outFive.data(), outFive.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 6};
        info[1] = {outSeven.data(), outSeven.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 8};
        info[2] = {outQuad.data(), outQuad.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 4};
    }

    Ngs2Fixture ngs2;
    const uintptr_t system;
    const uintptr_t masteringRack;
    const uintptr_t samplerRack;
    const std::vector<std::int16_t> pcm;
    uintptr_t masters[3]{};
    std::vector<float> outFive = std::vector<float>(Grain * 6, -1.0f);
    std::vector<float> outSeven = std::vector<float>(Grain * 8, -1.0f);
    std::vector<float> outQuad = std::vector<float>(Grain * 4, -1.0f);
    Ngs2RenderBufferInfo info[3]{};
};

const Case stereoIntoSurround{"MasteringRender_StereoMastersIntoSurroundOutputs_FillFrontPair", [] {
    StereoMastersFixture fixture;
    RequireEqual(sceNgs2SystemRender(fixture.system, fixture.info, 3), SCE_NGS2_OK, "render");
    for (std::uint32_t i = 0; i < Grain; i++) {
        const auto frame = " frame " + std::to_string(i);
        RequireEqual(fixture.outFive[i * 6], 0.5f, "5.1 left" + frame);
        RequireEqual(fixture.outFive[i * 6 + 1], 0.25f, "5.1 right" + frame);
        for (std::uint32_t channel = 2; channel < 6; channel++) {
            RequireEqual(fixture.outFive[i * 6 + channel], 0.0f, "5.1 channel " + std::to_string(channel) + frame);
        }
        RequireEqual(fixture.outSeven[i * 8], 0.5f, "7.1 left" + frame);
        RequireEqual(fixture.outSeven[i * 8 + 1], 0.25f, "7.1 right" + frame);
        for (std::uint32_t channel = 2; channel < 8; channel++) {
            RequireEqual(fixture.outSeven[i * 8 + channel], 0.0f, "7.1 channel " + std::to_string(channel) + frame);
        }
    }
}};

const Case stereoIntoQuad{"MasteringRender_StereoMasterIntoQuadOutput_Throws", [] {
    StereoMastersFixture fixture;
    RequireEqual(sceNgs2SystemRender(fixture.system, fixture.info, 3), SCE_NGS2_OK, "render without the quad source");
    const auto rejectedSource = RackVoice(fixture.samplerRack, 2);
    StereoSource(rejectedSource, fixture.pcm, fixture.masters[2]);
    Event(rejectedSource, SCE_NGS2_VOICE_EVENT_PLAY);
    RequireThrows<std::runtime_error>([&] { sceNgs2SystemRender(fixture.system, fixture.info, 3); }, "stereo into quad");
}};

const Case lockUnknown{"SystemLock_UnknownHandle_FailsInvalidSystemHandle", [] {
    RequireEqual(sceNgs2SystemLock(unknownHandle), SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE, "lock");
    RequireEqual(sceNgs2SystemUnlock(unknownHandle), SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE, "unlock");
}};

const Case lockUnlocked{"SystemGetInfo_FromOtherThreadWhileUnlocked_Succeeds", [] {
    Ngs2Fixture ngs2;
    const auto system = ngs2.CreateSystem();
    Ngs2SystemInfo info{};
    int result = -1;
    std::thread other([&] { result = sceNgs2SystemGetInfo(system, &info, sizeof(info)); });
    other.join();
    RequireEqual(result, SCE_NGS2_OK, "info from another thread");
}};

class LockedSystem {
public:
    explicit LockedSystem(uintptr_t system) : system(system) {
        RequireEqual(sceNgs2SystemLock(system), SCE_NGS2_OK, "lock");
        locked = true;
    }

    ~LockedSystem() {
        if (locked) sceNgs2SystemUnlock(system);
    }

    LockedSystem(const LockedSystem&) = delete;
    LockedSystem& operator=(const LockedSystem&) = delete;

    void Unlock() {
        locked = false;
        RequireEqual(sceNgs2SystemUnlock(system), SCE_NGS2_OK, "unlock");
    }

private:
    const uintptr_t system;
    bool locked = false;
};

const Case lockBlocks{"SystemLock_Held_BlocksOtherThreadUntilUnlock", [] {
    Ngs2Fixture ngs2;
    const auto system = ngs2.CreateSystem();
    Ngs2SystemInfo info{};
    std::atomic<int> result = -1;
    std::atomic<bool> done = false;
    LockedSystem lock(system);
    std::thread blocked([&] {
        result = sceNgs2SystemGetInfo(system, &info, sizeof(info));
        done = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const bool finishedWhileLocked = done;
    lock.Unlock();
    blocked.join();
    Require(!finishedWhileLocked, "other thread waits while the system is locked");
    Require(done, "other thread finishes after unlock");
    RequireEqual(result.load(), SCE_NGS2_OK, "info after unlock");
}};

struct AllocatorState {
    int allocations = 0;
    int badAllocateCalls = 0;
    int badReleaseCalls = 0;
};

constexpr std::uintptr_t allocatorTag = 9;

AllocatorState& AllocatorStateOf(const Ngs2ContextBufferInfo* info) {
    return *reinterpret_cast<AllocatorState*>(info->user_data);
}

std::int32_t APS5_VABI Allocate(Ngs2ContextBufferInfo* info) {
    auto& state = AllocatorStateOf(info);
    if (info->host_buffer != nullptr || info->host_buffer_size == 0) state.badAllocateCalls++;
    info->host_buffer = std::calloc(1, info->host_buffer_size);
    state.allocations++;
    return SCE_NGS2_OK;
}

std::int32_t APS5_VABI Release(Ngs2ContextBufferInfo* info) {
    auto& state = AllocatorStateOf(info);
    if (info->host_buffer == nullptr) state.badReleaseCalls++;
    std::free(info->host_buffer);
    state.allocations--;
    return SCE_NGS2_OK;
}

const Case allocator{"Allocator_CreateAndDestroy_BalancesAllocations", [] {
    AllocatorState state;
    const Ngs2BufferAllocator allocator{Allocate, Release, reinterpret_cast<std::uintptr_t>(&state)};
    Ngs2Fixture ngs2;
    uintptr_t system = 0;
    RequireEqual(sceNgs2SystemCreateWithAllocator(nullptr, &allocator, &system), SCE_NGS2_OK, "create the system");
    ngs2.Track(system);
    RequireEqual(state.allocations, 1, "allocations after the system");
    uintptr_t sampler = 0;
    uintptr_t master = 0;
    RequireEqual(sceNgs2RackCreateWithAllocator(system, SCE_NGS2_RACK_ID_SAMPLER, nullptr, &allocator, &sampler), SCE_NGS2_OK,
                 "create the sampler rack");
    RequireEqual(sceNgs2RackCreateWithAllocator(system, SCE_NGS2_RACK_ID_MASTERING, nullptr, &allocator, &master), SCE_NGS2_OK,
                 "create the mastering rack");
    RequireEqual(state.allocations, 3, "allocations after the racks");
    Ngs2ContextBufferInfo released{};
    RequireEqual(sceNgs2RackDestroy(sampler, &released), SCE_NGS2_OK, "destroy the sampler rack");
    RequireEqual(state.allocations, 2, "allocations after the rack destroy");
    RequireEqual(released.host_buffer, static_cast<void*>(nullptr), "allocator buffers are not reported as released");
    RequireDestroyed(system);
    RequireEqual(state.allocations, 0, "allocations after the system destroy");
    RequireEqual(sceNgs2RackDestroy(master, nullptr), SCE_NGS2_ERROR_INVALID_RACK_HANDLE, "rack destroyed with its system");
    RequireEqual(state.badAllocateCalls, 0, "allocate handler received a bad buffer");
    RequireEqual(state.badReleaseCalls, 0, "release handler received a null buffer");
}};

std::atomic<uintptr_t> exitSystem = 0;
std::vector<std::uint64_t> exitSystemStorage;

void RenderAfterStaticTeardown() {
    const auto system = exitSystem.load();
    if (system == 0) return;
    std::int16_t out[Grain]{};
    const Ngs2RenderBufferInfo info{out, sizeof(out), SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 1};
    try {
        const int rendered = sceNgs2SystemRender(system, &info, 1);
        const int destroyed = sceNgs2SystemDestroy(system, nullptr);
        if (rendered == SCE_NGS2_OK && destroyed == SCE_NGS2_OK) return;
        std::fprintf(stderr, "[ FAILED  ] render after static teardown returned %d, destroy returned %d\n", rendered, destroyed);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "[ FAILED  ] render after static teardown threw: %s\n", error.what());
    }
    std::fflush(stderr);
    std::_Exit(1);
}

const Case renderAtExit{"SystemRender_AfterStaticTeardown_StillSucceeds", [] {
    Ngs2ContextBufferInfo query{};
    RequireEqual(sceNgs2SystemQueryBufferSize(nullptr, &query), SCE_NGS2_OK, "query");
    exitSystemStorage.assign(query.host_buffer_size / sizeof(std::uint64_t) + 1, 0);
    Ngs2ContextBufferInfo info{};
    info.host_buffer = exitSystemStorage.data();
    info.host_buffer_size = query.host_buffer_size;
    uintptr_t system = 0;
    RequireEqual(sceNgs2SystemCreate(nullptr, &info, &system), SCE_NGS2_OK, "create the system kept until exit");
    RequireEqual(sceNgs2SystemSetGrainSamples(system, Grain), SCE_NGS2_OK, "set the grain size");
    exitSystem = system;
}};

} // namespace

int main(int argc, char** argv) {
    if (std::atexit(RenderAfterStaticTeardown) != 0) {
        std::fprintf(stderr, "cannot register the exit handler\n");
        return 1;
    }
    return Testing::Run(argc, argv);
}
