#include "Ngs2Test.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <thread>
#include <vector>

static uintptr_t Sampler(uintptr_t system, const std::vector<std::int16_t>& pcm, std::uint32_t repeats) {
    const auto voice = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 1, 48000, 0, 0, 0}});
    const Ngs2WaveformBlock block{0, pcm.size() * sizeof(std::int16_t), repeats, 0, static_cast<std::uint32_t>(pcm.size()), 0, 0x55};
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), 0, 1, &block});
    return voice;
}

static std::vector<std::uint32_t> callbackFlags;
static void APS5_VABI OnBlock(const Ngs2VoiceCallbackInfo* info) {
    Require(info->callback_data == 7 && info->user_data == 0x55);
    callbackFlags.push_back(info->flag);
}

static std::vector<std::int16_t> RenderI16(uintptr_t system) {
    std::vector<std::int16_t> out(Grain, -1);
    const Ngs2RenderBufferInfo info{out.data(), out.size() * sizeof(std::int16_t), SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 1};
    Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_OK);
    return out;
}

static void TestErrorsAndInfo() {
    Ngs2SystemOption option{};
    Require(sceNgs2SystemResetOption(&option) == SCE_NGS2_OK);
    Require(option.size == sizeof(option) && option.max_grain_samples == 512 && option.num_grain_samples == 256 && option.sample_rate == 48000);
    Require(sceNgs2SystemQueryBufferSize(&option, nullptr) == SCE_NGS2_ERROR_INVALID_OUT_ADDRESS);
    Require(sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_SAMPLER, nullptr, nullptr) == SCE_NGS2_ERROR_INVALID_OUT_ADDRESS);

    Ngs2SystemInfo info{};
    Require(sceNgs2SystemGetInfo(0x1234, &info, sizeof(info)) == static_cast<int>(0x804A0230u));
    Require(sceNgs2RackDestroy(0x1234, nullptr) == static_cast<int>(0x804A0261u));

    const auto system = CreateSystem();
    const auto rack = CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER);
    Ngs2RackInfo rackInfo{};
    Require(sceNgs2RackGetInfo(0x1234, &rackInfo, sizeof(rackInfo)) == SCE_NGS2_ERROR_INVALID_RACK_HANDLE);
    Require(sceNgs2RackGetInfo(rack, nullptr, sizeof(rackInfo)) == SCE_NGS2_ERROR_INVALID_OUT_ADDRESS);
    Require(sceNgs2RackGetInfo(rack, &rackInfo, sizeof(rackInfo) - 1) == SCE_NGS2_ERROR_INVALID_OUT_SIZE);
    Require(sceNgs2RackGetInfo(rack, &rackInfo, sizeof(rackInfo)) == SCE_NGS2_OK);
    Require(rackInfo.rack_handle == rack && rackInfo.owner_system_handle == system && rackInfo.rack_id == SCE_NGS2_RACK_ID_SAMPLER && rackInfo.type == 1);
    Require(rackInfo.uid != 0 && rackInfo.max_voices == 256 && rackInfo.max_channel_works == 256 && rackInfo.max_grain_samples == 512);
    Require(rackInfo.max_ports == 8 && rackInfo.max_matrices == 1 && rackInfo.active_voice_count == 0 && rackInfo.name[0] == '\0');
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
    Require(sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_SUBMIXER, &named.rack_option, &namedQuery) == SCE_NGS2_OK);
    const auto namedBuffer = Buffer(namedQuery);
    uintptr_t namedRack = 0;
    Require(sceNgs2RackCreate(system, SCE_NGS2_RACK_ID_SUBMIXER, &named.rack_option, &namedBuffer, &namedRack) == SCE_NGS2_OK);
    Ngs2RackInfo namedInfo{};
    Require(sceNgs2RackGetInfo(namedRack, &namedInfo, sizeof(namedInfo)) == SCE_NGS2_OK);
    Require(namedInfo.name[0] == 'n' && namedInfo.name[62] == 'n' && namedInfo.name[63] == '\0');
    Require(namedInfo.type == 2 && namedInfo.rack_id == SCE_NGS2_RACK_ID_SUBMIXER && namedInfo.uid != rackInfo.uid && namedInfo.max_grain_samples == 256);
    Require(namedInfo.max_voices == 2 && namedInfo.max_ports == 2 && namedInfo.max_inputs == 3 && namedInfo.max_channel_works == 0);
    Require(namedInfo.buffer_info.host_buffer == namedBuffer.host_buffer && namedInfo.render_count == 0);
    uintptr_t named0 = 0;
    Require(sceNgs2RackGetVoiceHandle(namedRack, 0, &named0) == SCE_NGS2_OK);
    Control(named0, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, 2, 0});
    Event(named0, SCE_NGS2_VOICE_EVENT_PLAY);
    Require(sceNgs2RackGetInfo(namedRack, &namedInfo, sizeof(namedInfo)) == SCE_NGS2_OK && namedInfo.active_voice_count == 1);
    Ngs2VoiceChannelsInfo stereoInfo{};
    Require(sceNgs2VoiceQueryInfo(named0, SCE_NGS2_VOICE_INFO_CHANNELS, &stereoInfo, sizeof(stereoInfo)) == SCE_NGS2_OK && stereoInfo.num_channels == 2);
    Require(sceNgs2SystemGetInfo(system, nullptr, sizeof(info)) == SCE_NGS2_ERROR_INVALID_OUT_ADDRESS);
    Require(sceNgs2SystemGetInfo(system, &info, sizeof(info) - 1) == SCE_NGS2_ERROR_INVALID_OUT_SIZE);
    Require(sceNgs2SystemGetInfo(system, &info, sizeof(info)) == SCE_NGS2_OK);
    Require(info.system_handle == system && info.uid != 0 && info.rack_count == 2 && info.sample_rate == 48000);
    Require(info.num_grain_samples == Grain && info.max_grain_samples == 512 && info.render_count == 0);

    Ngs2ContextBufferInfo released{};
    Require(sceNgs2SystemDestroy(system, &released) == SCE_NGS2_OK && released.host_buffer != nullptr);
    Require(sceNgs2SystemGetInfo(system, &info, sizeof(info)) == SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE);
}

static void TestPcmBlockEnd() {
    const auto system = CreateSystem();
    const auto master = Mastering(system, 1);
    std::vector<std::int16_t> pcm;
    for (int i = 0; i < 12; i++) pcm.push_back(static_cast<std::int16_t>(i * 1000 - 4000));
    const auto sampler = Sampler(system, pcm, 0);
    Patch(sampler, master);
    Ngs2VoicePortInfo portInfo{};
    Require(sceNgs2VoiceGetPortInfo(sampler, 0, nullptr, sizeof(portInfo)) == SCE_NGS2_ERROR_INVALID_OUT_ADDRESS);
    Require(sceNgs2VoiceGetPortInfo(sampler, 0, &portInfo, sizeof(portInfo) - 1) == SCE_NGS2_ERROR_INVALID_OUT_SIZE);
    Require(sceNgs2VoiceGetPortInfo(sampler, 0, &portInfo, sizeof(portInfo)) == SCE_NGS2_OK);
    Require(portInfo.dest_handle == master && portInfo.matrix_id == -1 && portInfo.volume == 1.0f);
    Control(sampler, SCE_NGS2_VOICE_PARAM_PORT_VOLUME, Ngs2VoicePortVolumeParam{{}, 0, 0.5f});
    Control(sampler, SCE_NGS2_VOICE_PARAM_PORT_MATRIX, Ngs2VoicePortMatrixParam{{}, 0, 0});
    Require(sceNgs2VoiceGetPortInfo(sampler, 0, &portInfo, sizeof(portInfo)) == SCE_NGS2_OK);
    Require(portInfo.dest_handle == master && portInfo.matrix_id == 0 && portInfo.volume == 0.5f);
    Control(sampler, SCE_NGS2_VOICE_PARAM_PORT_VOLUME, Ngs2VoicePortVolumeParam{{}, 0, 1.0f});
    Control(sampler, SCE_NGS2_VOICE_PARAM_PORT_MATRIX, Ngs2VoicePortMatrixParam{{}, 0, -1});
    Require(sceNgs2VoiceGetPortInfo(sampler, 1, &portInfo, sizeof(portInfo)) == SCE_NGS2_OK && portInfo.dest_handle == 0);
    bool portOutOfRange = false;
    try { sceNgs2VoiceGetPortInfo(sampler, 8, &portInfo, sizeof(portInfo)); } catch (const std::invalid_argument&) { portOutOfRange = true; }
    Require(portOutOfRange);
    Ngs2VoiceChannelsInfo channelsInfo{};
    Require(sceNgs2VoiceQueryInfo(sampler, SCE_NGS2_VOICE_INFO_CHANNELS, nullptr, sizeof(channelsInfo)) == SCE_NGS2_ERROR_INVALID_OUT_ADDRESS);
    Require(sceNgs2VoiceQueryInfo(sampler, SCE_NGS2_VOICE_INFO_CHANNELS, &channelsInfo, sizeof(channelsInfo) - 1) == SCE_NGS2_ERROR_INVALID_OUT_SIZE);
    Require(sceNgs2VoiceQueryInfo(sampler, SCE_NGS2_VOICE_INFO_CHANNELS, &channelsInfo, sizeof(channelsInfo)) == SCE_NGS2_OK);
    Require(channelsInfo.num_channels == 1 && channelsInfo.reserved == 0);
    bool unknownInfo = false;
    try { sceNgs2VoiceQueryInfo(sampler, 0x4000, &channelsInfo, sizeof(channelsInfo)); } catch (const std::runtime_error&) { unknownInfo = true; }
    Require(unknownInfo);
    Control(sampler, SCE_NGS2_VOICE_PARAM_CALLBACK, Ngs2VoiceCallbackParam{{}, OnBlock, 7, SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END, 0});
    Require(Flags(sampler) == 0);
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);
    Require(Flags(sampler) == SCE_NGS2_VOICE_STATE_FLAG_INUSE);

    callbackFlags.clear();
    auto out = RenderI16(system);
    for (std::uint32_t i = 0; i < Grain; i++) Require(out[i] == pcm[i]);
    Require(Flags(sampler) == (SCE_NGS2_VOICE_STATE_FLAG_INUSE | SCE_NGS2_VOICE_STATE_FLAG_PLAYING) && callbackFlags.empty());

    Ngs2SamplerVoiceState state{};
    Require(sceNgs2VoiceGetState(sampler, &state.voice_state, sizeof(state) - 8) == SCE_NGS2_ERROR_INVALID_OUT_SIZE);
    Require(sceNgs2VoiceGetState(sampler, &state.voice_state, sizeof(state)) == SCE_NGS2_OK);
    Require(state.num_decoded_samples == Grain && state.decoded_data_size == Grain * 2 && state.user_data == 0x55);
    Require(state.waveform_data == pcm.data() + Grain);

    out = RenderI16(system);
    for (std::uint32_t i = 0; i < Grain; i++) Require(out[i] == (i < 4 ? pcm[Grain + i] : 0));
    Require(callbackFlags.size() == 1 && callbackFlags[0] == SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END);
    Require(Flags(sampler) == 0);
    Require(sceNgs2VoiceGetState(sampler, &state.voice_state, sizeof(state)) == SCE_NGS2_OK);
    Require(state.num_decoded_samples == pcm.size() && state.waveform_data == pcm.data() + pcm.size());
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestPitchAndRepeat() {
    const auto system = CreateSystem();
    const auto master = Mastering(system, 1);
    const std::vector<std::int16_t> pcm{0, 1000, 2000, 3000};
    const auto sampler = Sampler(system, pcm, 1);
    Patch(sampler, master);
    Control(sampler, SCE_NGS2_SAMPLER_VOICE_PARAM_PITCH, Ngs2SamplerVoicePitchParam{{}, 0.5f});
    Control(sampler, SCE_NGS2_VOICE_PARAM_CALLBACK,
            Ngs2VoiceCallbackParam{{}, OnBlock, 7, SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END | SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_REPEAT, 0});
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);

    callbackFlags.clear();
    const std::int16_t first[Grain] = {0, 500, 1000, 1500, 2000, 2500, 3000, 1500};
    auto out = RenderI16(system);
    for (std::uint32_t i = 0; i < Grain; i++) Require(out[i] == first[i]);
    Require(callbackFlags.size() == 1 && callbackFlags[0] == SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_REPEAT);

    const std::int16_t second[Grain] = {0, 500, 1000, 1500, 2000, 2500, 3000, 3000};
    out = RenderI16(system);
    for (std::uint32_t i = 0; i < Grain; i++) Require(out[i] == second[i]);
    Require(callbackFlags.size() == 2 && callbackFlags[1] == SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END);
    Require(Flags(sampler) == 0);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestSubmixerMatrix() {
    const auto system = CreateSystem();
    const auto master = Mastering(system, 2);
    const auto submixer = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SUBMIXER));
    Control(submixer, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, 2, 0});
    Patch(submixer, master);
    const Ngs2VoiceCommand play{2, 0, 4, 0, {.u = SCE_NGS2_VOICE_EVENT_PLAY}};
    Require(sceNgs2VoiceRunCommands(submixer, &play, 1) == SCE_NGS2_OK);

    const std::vector<std::int16_t> pcm(Grain, 16384);
    const auto sampler = Sampler(system, pcm, 0);
    Patch(sampler, submixer);
    const float levels[2] = {1.0f, 0.5f};
    Control(sampler, SCE_NGS2_VOICE_PARAM_MATRIX_LEVELS, Ngs2VoiceMatrixLevelsParam{{}, 0, 2, levels});
    Control(sampler, SCE_NGS2_VOICE_PARAM_PORT_MATRIX, Ngs2VoicePortMatrixParam{{}, 0, 0});
    Control(sampler, SCE_NGS2_VOICE_PARAM_PORT_VOLUME, Ngs2VoicePortVolumeParam{{}, 0, 0.5f});
    Require(sceNgs2VoiceRunCommands(sampler, &play, 1) == SCE_NGS2_OK);

    std::vector<float> out(Grain * 2, -1.0f);
    const Ngs2RenderBufferInfo info{out.data(), out.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 2};
    Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_OK);
    for (std::uint32_t i = 0; i < Grain; i++) Require(out[i * 2] == 0.25f && out[i * 2 + 1] == 0.125f);

    Ngs2SubmixerVoiceState state{};
    Require(sceNgs2VoiceGetState(submixer, &state.voice_state, sizeof(state)) == SCE_NGS2_OK);
    Require(state.voice_state.state_flags == (SCE_NGS2_VOICE_STATE_FLAG_INUSE | SCE_NGS2_VOICE_STATE_FLAG_PLAYING));

    Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_OK);
    for (float sample : out) Require(sample == 0.0f);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static int allocations = 0;
static std::int32_t APS5_VABI Allocate(Ngs2ContextBufferInfo* info) {
    Require(info->host_buffer == nullptr && info->host_buffer_size != 0 && info->user_data == 9);
    info->host_buffer = std::calloc(1, info->host_buffer_size);
    allocations++;
    return SCE_NGS2_OK;
}
static std::int32_t APS5_VABI Release(Ngs2ContextBufferInfo* info) {
    Require(info->host_buffer != nullptr && info->user_data == 9);
    std::free(info->host_buffer);
    allocations--;
    return SCE_NGS2_OK;
}

static void TestSampleRate() {
    const auto system = CreateSystem();
    Require(sceNgs2SystemSetSampleRate(0x1234, 96000) == SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE);
    bool threw = false;
    try {
        sceNgs2SystemSetSampleRate(system, 0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    Require(sceNgs2SystemSetSampleRate(system, 96000) == SCE_NGS2_OK);
    Ngs2SystemInfo info{};
    Require(sceNgs2SystemGetInfo(system, &info, sizeof(info)) == SCE_NGS2_OK && info.sample_rate == 96000);

    const auto master = Mastering(system, 1);
    const std::vector<std::int16_t> pcm{0, 1000, 2000, 3000};
    const auto sampler = Sampler(system, pcm, 1);
    Patch(sampler, master);
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);
    const std::int16_t expected[Grain] = {0, 500, 1000, 1500, 2000, 2500, 3000, 1500};
    const auto out = RenderI16(system);
    for (std::uint32_t i = 0; i < Grain; i++) Require(out[i] == expected[i]);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestUserData() {
    uintptr_t value = 1;
    Require(sceNgs2SystemSetUserData(0x1234, 5) == SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE);
    Require(sceNgs2SystemGetUserData(0x1234, &value) == SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE && value == 1);
    const auto system = CreateSystem();
    Require(sceNgs2SystemGetUserData(system, &value) == SCE_NGS2_OK && value == 0);
    Require(sceNgs2SystemSetUserData(system, 0xfedcba9876543210) == SCE_NGS2_OK);
    Require(sceNgs2SystemGetUserData(system, &value) == SCE_NGS2_OK && value == 0xfedcba9876543210);
    bool threw = false;
    try {
        sceNgs2SystemGetUserData(system, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestMasteringGain() {
    const auto system = CreateSystem();
    const auto stereo = Mastering(system, 2);
    const auto surround = Mastering(system, 6);
    const auto quad = Mastering(system, 4);
    Control(stereo, SCE_NGS2_MASTERING_VOICE_PARAM_GAIN, Ngs2MasteringVoiceGainParam{{}, 0.5f, 1.0f});
    Control(surround, SCE_NGS2_MASTERING_VOICE_PARAM_GAIN, Ngs2MasteringVoiceGainParam{{}, 0.5f, 0.25f});
    Control(surround, SCE_NGS2_MASTERING_VOICE_PARAM_OUTPUT, Ngs2MasteringVoiceOutputParam{{}, 1});
    Control(quad, SCE_NGS2_MASTERING_VOICE_PARAM_GAIN, Ngs2MasteringVoiceGainParam{{}, 0.5f, 0.25f});
    Control(quad, SCE_NGS2_MASTERING_VOICE_PARAM_OUTPUT, Ngs2MasteringVoiceOutputParam{{}, 2});
    const std::vector<std::int16_t> pcm(Grain, 16384);
    const auto toStereo = Sampler(system, pcm, 0);
    const float stereoLevels[2] = {1.0f, 0.5f};
    Control(toStereo, SCE_NGS2_VOICE_PARAM_MATRIX_LEVELS, Ngs2VoiceMatrixLevelsParam{{}, 0, 2, stereoLevels});
    Control(toStereo, SCE_NGS2_VOICE_PARAM_PORT_MATRIX, Ngs2VoicePortMatrixParam{{}, 0, 0});
    Patch(toStereo, stereo);
    Event(toStereo, SCE_NGS2_VOICE_EVENT_PLAY);
    const auto toSurround = Sampler(system, pcm, 0);
    const float surroundLevels[6] = {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
    Control(toSurround, SCE_NGS2_VOICE_PARAM_MATRIX_LEVELS, Ngs2VoiceMatrixLevelsParam{{}, 0, 6, surroundLevels});
    Control(toSurround, SCE_NGS2_VOICE_PARAM_PORT_MATRIX, Ngs2VoicePortMatrixParam{{}, 0, 0});
    Patch(toSurround, surround);
    Event(toSurround, SCE_NGS2_VOICE_EVENT_PLAY);
    const auto toQuad = Sampler(system, pcm, 0);
    Control(toQuad, SCE_NGS2_VOICE_PARAM_MATRIX_LEVELS, Ngs2VoiceMatrixLevelsParam{{}, 0, 4, surroundLevels});
    Control(toQuad, SCE_NGS2_VOICE_PARAM_PORT_MATRIX, Ngs2VoicePortMatrixParam{{}, 0, 0});
    Patch(toQuad, quad);
    Event(toQuad, SCE_NGS2_VOICE_EVENT_PLAY);
    bool rejected = false;
    try { Control(stereo, SCE_NGS2_MASTERING_VOICE_PARAM_GAIN, Ngs2MasteringVoiceGainParam{{}, NAN, 1.0f}); } catch (const std::exception&) { rejected = true; }
    Require(rejected);

    std::vector<float> outStereo(Grain * 2, -1.0f);
    std::vector<float> outSurround(Grain * 6, -1.0f);
    std::vector<float> outQuad(Grain * 4, -1.0f);
    const Ngs2RenderBufferInfo info[3] = {
        {outStereo.data(), outStereo.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 2},
        {outSurround.data(), outSurround.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 6},
        {outQuad.data(), outQuad.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 4},
    };
    Require(sceNgs2SystemRender(system, info, 3) == SCE_NGS2_OK);
    for (std::uint32_t i = 0; i < Grain; i++) {
        Require(outStereo[i * 2] == 0.25f && outStereo[i * 2 + 1] == 0.125f);
        for (std::uint32_t channel = 0; channel < 6; channel++) Require(outSurround[i * 6 + channel] == (channel == 3 ? 0.125f : 0.25f));
        for (std::uint32_t channel = 0; channel < 4; channel++) Require(outQuad[i * 4 + channel] == 0.25f);
    }
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);

    const auto loudSystem = CreateSystem();
    const auto loud = Mastering(loudSystem, 1);
    Control(loud, SCE_NGS2_MASTERING_VOICE_PARAM_GAIN, Ngs2MasteringVoiceGainParam{{}, 2.0f, 2.0f});
    const std::vector<std::int16_t> loudPcm(Grain * 2, 24576);
    const auto toLoud = Sampler(loudSystem, loudPcm, 0);
    Patch(toLoud, loud);
    Event(toLoud, SCE_NGS2_VOICE_EVENT_PLAY);
    auto clipped = RenderI16(loudSystem);
    for (std::int16_t sample : clipped) Require(sample == 32767);
    Control(loud, SCE_NGS2_MASTERING_VOICE_PARAM_SETUP, Ngs2MasteringVoiceSetupParam{{}, 1, 0});
    Event(loud, SCE_NGS2_VOICE_EVENT_PLAY);
    auto reset = RenderI16(loudSystem);
    for (std::int16_t sample : reset) Require(sample == 24576);
    Require(sceNgs2SystemDestroy(loudSystem, nullptr) == SCE_NGS2_OK);
}

static uintptr_t RackVoice(uintptr_t rack, std::uint32_t index) {
    uintptr_t voice = 0;
    Require(sceNgs2RackGetVoiceHandle(rack, index, &voice) == SCE_NGS2_OK && voice != 0);
    return voice;
}

static uintptr_t MasteringRack(uintptr_t system, std::uint32_t voices) {
    Ngs2MasteringRackOption option{};
    option.rack_option.size = sizeof(option);
    option.rack_option.max_grain_samples = 512;
    option.rack_option.max_voices = voices;
    option.rack_option.max_input_delay_blocks = 1;
    option.rack_option.max_matrices = 1;
    option.rack_option.max_ports = 8;
    option.max_channels = 8;
    Ngs2ContextBufferInfo query{};
    Require(sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_MASTERING, &option.rack_option, &query) == SCE_NGS2_OK);
    const auto buffer = Buffer(query);
    uintptr_t rack = 0;
    Require(sceNgs2RackCreate(system, SCE_NGS2_RACK_ID_MASTERING, &option.rack_option, &buffer, &rack) == SCE_NGS2_OK && rack != 0);
    return rack;
}

static void StereoSource(uintptr_t voice, const std::vector<std::int16_t>& pcm, uintptr_t master) {
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 1, 48000, 0, 0, 0}});
    const Ngs2WaveformBlock block{0, pcm.size() * sizeof(std::int16_t), 0, 0, static_cast<std::uint32_t>(pcm.size()), 0, 0};
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), 0, 1, &block});
    const float levels[2] = {1.0f, 0.5f};
    Control(voice, SCE_NGS2_VOICE_PARAM_MATRIX_LEVELS, Ngs2VoiceMatrixLevelsParam{{}, 0, 2, levels});
    Control(voice, SCE_NGS2_VOICE_PARAM_PORT_MATRIX, Ngs2VoicePortMatrixParam{{}, 0, 0});
    Patch(voice, master);
}

static void TestStereoIntoSurround() {
    const auto system = CreateSystem();
    const auto masteringRack = MasteringRack(system, 3);
    const auto samplerRack = CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER);
    uintptr_t masters[3];
    for (std::uint32_t i = 0; i < 3; i++) {
        masters[i] = RackVoice(masteringRack, i);
        Control(masters[i], SCE_NGS2_MASTERING_VOICE_PARAM_SETUP, Ngs2MasteringVoiceSetupParam{{}, 2, 0});
        Control(masters[i], SCE_NGS2_MASTERING_VOICE_PARAM_OUTPUT, Ngs2MasteringVoiceOutputParam{{}, i, 0});
        Event(masters[i], SCE_NGS2_VOICE_EVENT_PLAY);
    }
    const std::vector<std::int16_t> pcm(Grain, 16384);
    for (std::uint32_t i = 0; i < 2; i++) {
        const auto source = RackVoice(samplerRack, i);
        StereoSource(source, pcm, masters[i]);
        Event(source, SCE_NGS2_VOICE_EVENT_PLAY);
    }
    std::vector<float> outFive(Grain * 6, -1.0f);
    std::vector<float> outSeven(Grain * 8, -1.0f);
    std::vector<float> outQuad(Grain * 4, -1.0f);
    const Ngs2RenderBufferInfo info[3] = {
        {outFive.data(), outFive.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 6},
        {outSeven.data(), outSeven.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 8},
        {outQuad.data(), outQuad.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 4},
    };
    Require(sceNgs2SystemRender(system, info, 3) == SCE_NGS2_OK);
    for (std::uint32_t i = 0; i < Grain; i++) {
        Require(outFive[i * 6] == 0.5f && outFive[i * 6 + 1] == 0.25f);
        for (std::uint32_t channel = 2; channel < 6; channel++) Require(outFive[i * 6 + channel] == 0.0f);
        Require(outSeven[i * 8] == 0.5f && outSeven[i * 8 + 1] == 0.25f);
        for (std::uint32_t channel = 2; channel < 8; channel++) Require(outSeven[i * 8 + channel] == 0.0f);
    }

    const auto rejectedSource = RackVoice(samplerRack, 2);
    StereoSource(rejectedSource, pcm, masters[2]);
    Event(rejectedSource, SCE_NGS2_VOICE_EVENT_PLAY);
    bool threw = false;
    try {
        sceNgs2SystemRender(system, info, 3);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestLock() {
    const auto system = CreateSystem();
    Require(sceNgs2SystemLock(0x1234) == SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE);
    Require(sceNgs2SystemUnlock(0x1234) == SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE);
    Ngs2SystemInfo info{};
    std::thread free([&] { Require(sceNgs2SystemGetInfo(system, &info, sizeof(info)) == SCE_NGS2_OK); });
    free.join();

    Require(sceNgs2SystemLock(system) == SCE_NGS2_OK);
    std::atomic<bool> done = false;
    std::thread blocked([&] {
        Require(sceNgs2SystemGetInfo(system, &info, sizeof(info)) == SCE_NGS2_OK);
        done = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Require(!done);
    Require(sceNgs2SystemUnlock(system) == SCE_NGS2_OK);
    blocked.join();
    Require(done);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestAllocator() {
    const Ngs2BufferAllocator allocator{Allocate, Release, 9};
    uintptr_t system = 0;
    Require(sceNgs2SystemCreateWithAllocator(nullptr, &allocator, &system) == SCE_NGS2_OK && allocations == 1);
    uintptr_t sampler = 0;
    uintptr_t master = 0;
    Require(sceNgs2RackCreateWithAllocator(system, SCE_NGS2_RACK_ID_SAMPLER, nullptr, &allocator, &sampler) == SCE_NGS2_OK);
    Require(sceNgs2RackCreateWithAllocator(system, SCE_NGS2_RACK_ID_MASTERING, nullptr, &allocator, &master) == SCE_NGS2_OK && allocations == 3);
    Ngs2ContextBufferInfo released{};
    Require(sceNgs2RackDestroy(sampler, &released) == SCE_NGS2_OK && allocations == 2 && released.host_buffer == nullptr);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK && allocations == 0);
    Require(sceNgs2RackDestroy(master, nullptr) == SCE_NGS2_ERROR_INVALID_RACK_HANDLE);
}

static uintptr_t exitSystem = 0;

static void RenderAfterStaticTeardown() {
    RenderI16(exitSystem);
    Require(sceNgs2SystemDestroy(exitSystem, nullptr) == SCE_NGS2_OK);
}

int main() {
    Require(std::atexit(RenderAfterStaticTeardown) == 0);
    TestErrorsAndInfo();
    TestPcmBlockEnd();
    TestPitchAndRepeat();
    TestSubmixerMatrix();
    TestSampleRate();
    TestUserData();
    TestMasteringGain();
    TestStereoIntoSurround();
    TestLock();
    TestAllocator();
    exitSystem = CreateSystem();
    return 0;
}
