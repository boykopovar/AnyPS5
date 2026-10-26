#include "prx/libSceNgs2.native/tests/Ngs2Test.hpp"
#include "Ngs2Test.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <iterator>
#include <numbers>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

static uintptr_t Sampler(uintptr_t system, const std::vector<std::int16_t>& pcm, std::uint32_t repeats, uintptr_t* rack = nullptr) {
    const auto created = CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER);
    if (rack != nullptr) *rack = created;
    const auto voice = Voice(created);
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
    Require(sceNgs2SystemGetInfo(0x1234, &info, sizeof(info)) == static_cast<int>(0x804A8201u));
    Require(sceNgs2RackDestroy(0x1234, nullptr) == static_cast<int>(0x804A8202u));

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

static void TestPatchToRackHandle() {
    const auto system = CreateSystem();
    const auto masteringRack = CreateRack(system, SCE_NGS2_RACK_ID_MASTERING);
    const auto master = Voice(masteringRack);
    Control(master, SCE_NGS2_MASTERING_VOICE_PARAM_SETUP, Ngs2MasteringVoiceSetupParam{{}, 1, 0});
    Control(master, SCE_NGS2_MASTERING_VOICE_PARAM_OUTPUT, Ngs2MasteringVoiceOutputParam{{}, 0, 0});
    Event(master, SCE_NGS2_VOICE_EVENT_PLAY);

    const std::vector<std::int16_t> pcm(Grain, 16384);
    const auto sampler = Sampler(system, pcm, 0);
    Control(sampler, SCE_NGS2_VOICE_PARAM_PATCH, Ngs2VoicePatchParam{{}, 0, 0, masteringRack});

    Ngs2VoicePortInfo port{};
    Require(sceNgs2VoiceGetPortInfo(sampler, 0, &port, sizeof(port)) == SCE_NGS2_OK);
    Require(port.dest_handle == master && port.dest_input_id == 0);
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);
    const auto out = RenderI16(system);
    Require(out == pcm);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static bool Near(float a, float b) {
    return std::fabs(a - b) < 1e-4f;
}

static bool PanThrows(Ngs2PanWork* work, const Ngs2PanParam* params, std::uint32_t numParams, std::uint32_t format) {
    float out[64] = {};
    try { sceNgs2PanGetVolumeMatrix(work, params, numParams, format, out); } catch (const std::exception&) { return true; }
    return false;
}

static bool PanInitThrows(Ngs2PanWork* work, const float* angles, float unitAngle, std::uint32_t numSpeakers) {
    try { sceNgs2PanInit(work, angles, unitAngle, numSpeakers); } catch (const std::exception&) { return true; }
    return false;
}

struct PanCase {
    std::vector<float> angles;
    float unitAngle;
    std::uint32_t numSpeakers;
    std::vector<Ngs2PanParam> params;
    std::uint32_t format;
    std::vector<float> expected;
};

static void TestPan() {
    const float pi = std::numbers::pi_v<float>;
    Ngs2PanWork work{};
    Require(sceNgs2PanInit(&work, nullptr, 360.0f, 2) == SCE_NGS2_OK);
    Require(work.num_speakers == 2 && work.unit_angle == 360.0f);
    Require(work.speaker_angles[0] == 90.0f && work.speaker_angles[1] == 270.0f && work.speaker_angles[2] == 450.0f);
    Require(sceNgs2PanInit(&work, nullptr, 2.0f * pi, 2) == SCE_NGS2_OK && work.unit_angle == 2.0f * pi);
    Require(work.speaker_angles[0] == 90.0f / 180.0f * pi && work.speaker_angles[1] == 270.0f / 180.0f * pi && work.speaker_angles[2] == 450.0f);
    Require(sceNgs2PanInit(&work, nullptr, 100.0f, 4) == SCE_NGS2_OK && work.unit_angle == 360.0f && work.speaker_angles[4] == 400.0f);
    const float frontBack[] = {-30.0f, 30.0f, 0.0f, -110.0f, 470.0f};
    Require(sceNgs2PanInit(&work, frontBack, 360.0f, 5) == SCE_NGS2_OK);
    const float circle[] = {0.0f, 30.0f, 110.0f, 250.0f, 330.0f, 360.0f};
    for (std::uint32_t i = 0; i < 6; i++) Require(work.speaker_angles[i] == circle[i]);

    const std::vector<PanCase> cases = {
        {{}, 360.0f, 2, {{0, 1, 1, 0}, {45, 1, 1, 0}, {90, 1, 1, 0}, {135, 1, 1, 0}, {180, 1, 1, 0}, {225, 1, 1, 0}, {270, 1, 1, 0}, {315, 1, 1, 0}}, 2,
         {0.707106769f, 0.382683456f, 0.0f, 0.382683456f, 0.707106769f, 0.923879504f, 1.0f, 0.923879504f, 0.707106769f, 0.923879504f, 1.0f, 0.923879504f, 0.707106769f, 0.382683456f, 0.0f, 0.382683456f}},
        {{}, 360.0f, 2, {{30, 0, 1, 0}, {30, -1, 1, 0}, {30, 0.5f, 0.5f, 0}, {30, 3, 1, 0}, {-30, -0.25f, 1, 0}}, 2,
         {0.965925813f, 0.866025448f, 0.39667666f, 0.49999997f, 0.896872759f, 0.965925813f, 0.49999997f, 0.495722443f, 0.866025448f, 0.997858942f}},
        {{}, 360.0f, 2, {{60, 1, 1, 0.5f}}, 1, {0.866025388f}},
        {{}, 360.0f, 2, {{60, 1, 1, 0.5f}}, 6, {0.258819073f, 0.965925813f, 0.0f, 0.0f, 0.0f, 0.0f}},
        {{}, 2.0f * pi, 2, {{0.25f * pi, 1, 1, 0}, {1.5f * pi, 1, 1, 0}}, 2, {0.999965429f, 1.0f, 0.00831161533f, 0.0f}},
        {{}, 360.0f, 4, {{100, 1, 1, 0.25f}}, 6, {0.0f, 0.222520918f, 0.0f, 0.25f, 0.0f, 0.974927902f}},
        {{}, 360.0f, 4, {{100, 1, 1, 0.25f}}, 2, {0.0f, 0.709984899f}},
        {{}, 360.0f, 5, {{70, 1, 0.8f, 0.5f}, {200, 0.5f, 1, 0}}, 6,
         {0.0f, 0.0f, 0.625465155f, 0.270598054f, 0.0f, 0.270598054f, 0.5f, 0.0f, 0.0f, 0.782271147f, 0.498791903f, 0.491533577f}},
        {{}, 360.0f, 5, {{70, 1, 0.8f, 0.5f}}, 1, {0.691666603f}},
        {{}, 360.0f, 7, {{180, 1, 1, 0.25f}}, 8, {0.0f, 0.0f, 0.0f, 0.25f, 0.0f, 0.0f, 0.707106769f, 0.707106769f}},
        {{}, 360.0f, 7, {{180, 1, 1, 0.25f}}, 6, {0.0f, 0.0f, 0.0f, 0.25f, 0.707106769f, 0.707106769f}},
        {{-pi / 6, pi / 6, 0, -110 * pi / 180, 110 * pi / 180}, 2.0f * pi, 5, {{1.0f, 1, 1, 0}, {-2.5f, 0.2f, 1, 0.1f}}, 6,
         {0.0f, 0.0f, 0.859783232f, 0.582614243f, 0.0f, 0.0f, 0.0f, 0.100000001f, 0.0f, 0.753403723f, 0.510659218f, 0.372568965f}},
        {{-30, 30, 0, -90, 90, -150, 150}, 360.0f, 7, {{-120, 1, 1, 0}}, 1, {0.707106769f}},
        {{-45, 45}, 360.0f, 2, {{10, 1, 1, 0}, {720 + 170, 1, 1, 0}}, 2, {0.57357651f, 0.664795876f, 0.819152057f, 0.747025073f}},
        {{-40, 40, -120, 120}, 360.0f, 4, {{-80, 0.75f, 1, 0}}, 8, {0.69351995f, 0.0746578425f, 0.0f, 0.0f, 0.69351995f, 0.180239946f, 0.0f, 0.0f}},
    };
    for (const auto& panCase : cases) {
        Require(sceNgs2PanInit(&work, panCase.angles.empty() ? nullptr : panCase.angles.data(), panCase.unitAngle, panCase.numSpeakers) == SCE_NGS2_OK);
        std::vector<float> out(panCase.expected.size(), -1.0f);
        Require(sceNgs2PanGetVolumeMatrix(&work, panCase.params.data(), static_cast<std::uint32_t>(panCase.params.size()), panCase.format, out.data()) == SCE_NGS2_OK);
        for (std::size_t i = 0; i < out.size(); i++) Require(std::fabs(out[i] - panCase.expected[i]) < 1e-6f);
    }

    const Ngs2PanParam front{0.0f, 1.0f, 1.0f, 0.0f};
    float out[2] = {-1.0f, -1.0f};
    Require(sceNgs2PanInit(&work, nullptr, 360.0f, 2) == SCE_NGS2_OK);
    Require(sceNgs2PanGetVolumeMatrix(&work, &front, 1, 2, nullptr) == SCE_NGS2_ERROR_INVALID_OUT_ADDRESS);
    Require(sceNgs2PanGetVolumeMatrix(&work, &front, 0, 2, out) == SCE_NGS2_OK && out[0] == -1.0f && out[1] == -1.0f);
    Require(SCE_NGS2_ERROR_INVALID_PAN_UNIT_ANGLE == static_cast<int>(0x804A8450) && SCE_NGS2_ERROR_INVALID_PAN_SPEAKER == static_cast<int>(0x804A8451));
    Require(SCE_NGS2_ERROR_INVALID_PAN_MATRIX_FORMAT == static_cast<int>(0x804A8452) && SCE_NGS2_ERROR_INVALID_PAN_WORK == static_cast<int>(0x804A8453));
    Require(SCE_NGS2_ERROR_INVALID_PAN_PARAM == static_cast<int>(0x804A8454));
    float guard[32];
    const auto panResult = [&](Ngs2PanWork* panWork, const Ngs2PanParam* params, std::uint32_t numParams, std::uint32_t format) {
        std::fill(std::begin(guard), std::end(guard), -1.0f);
        const int result = sceNgs2PanGetVolumeMatrix(panWork, params, numParams, format, guard);
        Require(result == SCE_NGS2_OK || std::all_of(std::begin(guard), std::end(guard), [](float level) { return level == -1.0f; }));
        return result;
    };
    const Ngs2PanParam many[9] = {};
    Require(panResult(nullptr, &front, 1, 2) == SCE_NGS2_ERROR_INVALID_PAN_WORK && panResult(nullptr, nullptr, 1, 2) == SCE_NGS2_ERROR_INVALID_PAN_WORK);
    Require(sceNgs2PanGetVolumeMatrix(nullptr, &front, 1, 2, nullptr) == SCE_NGS2_ERROR_INVALID_PAN_WORK);
    Require(panResult(&work, nullptr, 0, 2) == SCE_NGS2_ERROR_INVALID_PAN_PARAM && panResult(&work, nullptr, 1, 3) == SCE_NGS2_ERROR_INVALID_PAN_PARAM);
    Require(sceNgs2PanGetVolumeMatrix(&work, nullptr, 1, 2, nullptr) == SCE_NGS2_ERROR_INVALID_PAN_PARAM);
    Require(sceNgs2PanGetVolumeMatrix(&work, &front, 1, 3, nullptr) == SCE_NGS2_ERROR_INVALID_OUT_ADDRESS);
    Require(panResult(&work, &front, 1, 4) == SCE_NGS2_ERROR_INVALID_PAN_MATRIX_FORMAT && panResult(&work, &front, 1, 3) == SCE_NGS2_ERROR_INVALID_PAN_MATRIX_FORMAT);
    Require(panResult(&work, many, 9, 3) == SCE_NGS2_ERROR_INVALID_PAN_MATRIX_FORMAT && panResult(&work, &front, 1, 8) == SCE_NGS2_OK);
    Require(panResult(&work, many, 9, 2) == SCE_NGS2_ERROR_INVALID_NUM_CHANNELS && panResult(&work, many, 8, 2) == SCE_NGS2_OK);
    Ngs2PanWork badWork = work;
    badWork.num_speakers = 3;
    Require(panResult(&badWork, &front, 1, 2) == SCE_NGS2_ERROR_INVALID_PAN_SPEAKER && panResult(&badWork, &front, 1, 3) == SCE_NGS2_ERROR_INVALID_PAN_MATRIX_FORMAT);
    Require(panResult(&badWork, many, 9, 2) == SCE_NGS2_ERROR_INVALID_PAN_SPEAKER);
    badWork.unit_angle = 100.0f;
    Require(panResult(&badWork, &front, 1, 2) == SCE_NGS2_ERROR_INVALID_PAN_SPEAKER);
    badWork.num_speakers = 2;
    Require(panResult(&badWork, &front, 1, 2) == SCE_NGS2_ERROR_INVALID_PAN_UNIT_ANGLE && panResult(&badWork, many, 9, 2) == SCE_NGS2_ERROR_INVALID_PAN_UNIT_ANGLE);
    Require(sceNgs2PanGetVolumeMatrix(&badWork, &front, 1, 2, nullptr) == SCE_NGS2_ERROR_INVALID_OUT_ADDRESS);
    const Ngs2PanParam nanAngle{NAN, 1.0f, 1.0f, 0.0f};
    Require(panResult(&work, &nanAngle, 1, 2) == SCE_NGS2_OK && guard[0] == 0.0f && guard[1] == 0.0f);
    const Ngs2PanParam laterNan[2] = {front, nanAngle};
    Require(panResult(&work, laterNan, 2, 2) == SCE_NGS2_OK);
    Require(std::fabs(guard[0] - 0.707106769f) < 1e-6f && guard[1] == 0.0f && std::fabs(guard[2] - 0.707106769f) < 1e-6f && guard[3] == 0.0f);
    Ngs2PanWork sevenWork{};
    Require(sceNgs2PanInit(&sevenWork, nullptr, 360.0f, 7) == SCE_NGS2_OK);
    const Ngs2PanParam nanEach[4] = {{NAN, 1.0f, 1.0f, 0.5f}, {30.0f, NAN, 1.0f, 0.5f}, {30.0f, 1.0f, NAN, 0.5f}, {30.0f, 1.0f, 1.0f, NAN}};
    Require(panResult(&sevenWork, nanEach, 4, 8) == SCE_NGS2_OK);
    for (std::uint32_t channel = 0; channel < 8; channel++) {
        Require(guard[channel * 4] == (channel == 3 ? 0.5f : 0.0f));
        for (std::uint32_t p = 1; p < 4; p++) Require(std::isnan(guard[channel * 4 + p]));
    }
    Ngs2PanWork radianWork{};
    const float radianPair[] = {-0.52359879f, 0.52359879f};
    Require(sceNgs2PanInit(&radianWork, radianPair, 2.0f * pi, 2) == SCE_NGS2_OK && radianWork.speaker_angles[0] == 0.52359879f);
    const Ngs2PanParam belowFirst{0.523598731f, 1.0f, 1.0f, 0.0f};
    const Ngs2PanParam aboveFirst{0.52359885f, 1.0f, 1.0f, 0.0f};
    Require(std::nextafter(0.52359879f, 0.0f) == belowFirst.angle && PanThrows(&radianWork, &belowFirst, 1, 1));
    Require(panResult(&radianWork, &aboveFirst, 1, 1) == SCE_NGS2_OK && std::fabs(guard[0] - 0.707106769f) < 1e-6f);
    const Ngs2PanParam huge{1e30f, 1.0f, 1.0f, 0.0f};
    Require(PanThrows(&work, &huge, 1, 2));
    const float hugeSpeaker[] = {1e30f, 30.0f};
    const float hugeNegative[] = {-1e30f, 30.0f};
    Require(PanInitThrows(&work, hugeSpeaker, 360.0f, 2) && PanInitThrows(&work, hugeNegative, 360.0f, 2));
    Require(sceNgs2PanInit(&work, nullptr, 360.0f, 2) == SCE_NGS2_OK);
    work.speaker_angles[0] = -INFINITY;
    Require(PanThrows(&work, &front, 1, 2));
    work.speaker_angles[0] = NAN;
    const Ngs2PanParam right{30.0f, 1.0f, 1.0f, 0.0f};
    Require(panResult(&work, &right, 1, 2) == SCE_NGS2_OK && guard[0] == 0.0f && guard[1] == 0.0f);
    Require(sceNgs2PanInit(&work, nullptr, 360.0f, 2) == SCE_NGS2_OK);
    Ngs2PanWork marked;
    std::memset(&marked, 0x5a, sizeof(marked));
    Ngs2PanWork expected;
    std::memcpy(&expected, &marked, sizeof(marked));
    const auto unchanged = [&] { return std::memcmp(&marked, &expected, sizeof(marked)) == 0; };
    const float pair[] = {-30.0f, 30.0f};
    Require(sceNgs2PanInit(nullptr, nullptr, 360.0f, 2) == static_cast<int>(0x804A8010));
    Require(sceNgs2PanInit(nullptr, pair, 180.0f, 3) == static_cast<int>(0x804A8010));
    Require(sceNgs2PanInit(nullptr, nullptr, 100.0f, 3) == static_cast<int>(0x804A8010));
    for (std::uint32_t count : {1u, 3u, 6u, 8u, 255u}) {
        Require(sceNgs2PanInit(&marked, nullptr, 360.0f, count) == static_cast<int>(0x804A8451));
        Require(unchanged());
    }
    const float sevenDegrees[] = {0.0f, 40.0f, 110.0f, 150.0f, 210.0f, 250.0f, 320.0f, 360.0f};
    for (const float unitAngle : {360.0f, 2.0f * pi, 180.0f, NAN}) {
        Ngs2PanWork seven;
        std::memset(&seven, 0x5a, sizeof(seven));
        Require(sceNgs2PanInit(&seven, nullptr, unitAngle, 0) == SCE_NGS2_OK && seven.num_speakers == 7);
        Require(seven.unit_angle == (unitAngle == 2.0f * pi ? 2.0f * pi : 360.0f));
        for (std::uint32_t i = 0; i < 8; i++) Require(seven.speaker_angles[i] == sevenDegrees[i]);
    }
    for (std::uint32_t count : {0u, 1u, 3u, 6u, 8u, 255u}) {
        Require(sceNgs2PanInit(&marked, pair, 360.0f, count) == static_cast<int>(0x804A8451));
        Require(sceNgs2PanInit(&marked, pair, 2.0f * pi, count) == static_cast<int>(0x804A8451));
        Require(unchanged());
    }
    for (const float unitAngle : {180.0f, 0.0f, 359.0f, 361.0f, 90.0f, pi}) {
        Require(sceNgs2PanInit(&marked, pair, unitAngle, 2) == static_cast<int>(0x804A8450));
        Require(sceNgs2PanInit(&marked, pair, unitAngle, 3) == static_cast<int>(0x804A8450));
        Require(unchanged());
    }
    Require(sceNgs2PanInit(&work, pair, 2.0f * pi, 2) == SCE_NGS2_OK);
    const float nanSpeaker[] = {0.0f, NAN};
    Require(sceNgs2PanInit(&work, nanSpeaker, 360.0f, 2) == SCE_NGS2_OK && work.num_speakers == 2 && work.unit_angle == 360.0f);
    Require(std::isnan(work.speaker_angles[0]) && work.speaker_angles[1] == 0.0f && std::isnan(work.speaker_angles[2]));
    const float onUnit[] = {720.0f, 30.0f};
    const float belowZero[] = {-360.0f, 30.0f};
    Require(PanInitThrows(&work, onUnit, 360.0f, 2) && !PanInitThrows(&work, belowZero, 360.0f, 2));
    work.unit_angle = 100.0f;
    Require(panResult(&work, &front, 1, 2) == SCE_NGS2_ERROR_INVALID_PAN_UNIT_ANGLE);
}

static void TestPanExplicitAnglesAndNanDistance() {
    Ngs2PanWork work;
    std::memset(&work, 0x5a, sizeof(work));
    Ngs2PanWork filled;
    std::memcpy(&filled, &work, sizeof(work));
    const float pair[] = {-30.0f, 30.0f};
    Require(sceNgs2PanInit(&work, pair, 360.0f, 2) == SCE_NGS2_OK && work.num_speakers == 2 && work.unit_angle == 360.0f);
    Require(work.speaker_angles[0] == 30.0f && work.speaker_angles[1] == 330.0f && work.speaker_angles[2] == 390.0f);
    Require(std::memcmp(&work.speaker_angles[3], &filled.speaker_angles[3], 5 * sizeof(float)) == 0);
    Require(sceNgs2PanInit(&work, nullptr, 360.0f, 2) == SCE_NGS2_OK);
    for (std::uint32_t i = 3; i < 8; i++) Require(work.speaker_angles[i] == 0.0f);
    const Ngs2PanParam params[2] = {{0.0f, 1.0f, 1.0f, 0.0f}, {30.0f, NAN, 1.0f, 0.0f}};
    float out[4] = {};
    Require(sceNgs2PanGetVolumeMatrix(&work, params, 2, 2, out) == SCE_NGS2_OK);
    Require(std::fabs(out[0] - 0.707106769f) < 1e-6f && std::isnan(out[1]) && std::fabs(out[2] - 0.707106769f) < 1e-6f && std::isnan(out[3]));
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

static void TestSetupClearsCallback() {
    const auto system = CreateSystem();
    const auto master = Mastering(system, 1);
    const std::vector<std::int16_t> pcm{1000, 2000, 3000, 4000};
    const auto sampler = Sampler(system, pcm, 0);
    Control(sampler, SCE_NGS2_VOICE_PARAM_CALLBACK, Ngs2VoiceCallbackParam{{}, OnBlock, 7, SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END, 0});
    Control(sampler, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 1, 48000, 0, 0, 0}});
    const Ngs2WaveformBlock oneShot{0, pcm.size() * sizeof(std::int16_t), 0, 0, static_cast<std::uint32_t>(pcm.size()), 0, 0};
    Control(sampler, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), 0, 1, &oneShot});
    Patch(sampler, master);
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);

    callbackFlags.clear();
    auto out = RenderI16(system);
    for (std::uint32_t i = 0; i < Grain; i++) Require(out[i] == (i < pcm.size() ? pcm[i] : 0));
    Require(callbackFlags.empty() && Flags(sampler) == 0);

    Control(sampler, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 1, 48000, 0, 0, 0}});
    Patch(sampler, master);
    Control(sampler, SCE_NGS2_VOICE_PARAM_CALLBACK, Ngs2VoiceCallbackParam{{}, OnBlock, 7, SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END, 0});
    const Ngs2WaveformBlock stream{0, pcm.size() * sizeof(std::int16_t), 0, 0, static_cast<std::uint32_t>(pcm.size()), 0, 0x55};
    Control(sampler, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), 0, 1, &stream});
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);
    out = RenderI16(system);
    for (std::uint32_t i = 0; i < Grain; i++) Require(out[i] == (i < pcm.size() ? pcm[i] : 0));
    Require(callbackFlags.size() == 1 && callbackFlags[0] == SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

struct PcmResetCallback {
    std::vector<std::int16_t> pcm;
    std::uint32_t flag;
    int calls = 0;
    std::uint32_t waveformFlags = SCE_NGS2_WAVEFORM_BLOCKS_FLAG_RESET;
    bool pause = false;
};

static void APS5_VABI ResetPcm(const Ngs2VoiceCallbackInfo* info) {
    auto& callback = *reinterpret_cast<PcmResetCallback*>(info->callback_data);
    if (++callback.calls != 1) return;
    Require(info->flag == callback.flag);
    const Ngs2WaveformBlock block{0, callback.pcm.size() * sizeof(std::int16_t), 0, 0, static_cast<std::uint32_t>(callback.pcm.size()), 0, 0};
    Control(info->voice_handle, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS,
            Ngs2SamplerVoiceWaveformBlocksParam{{}, callback.pcm.data(), callback.waveformFlags, 1, &block});
    if (callback.pause) Event(info->voice_handle, SCE_NGS2_VOICE_EVENT_PAUSE);
}

static void TestPcmCallbackReset() {
    for (const auto flag : {SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END, SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_REPEAT}) {
        const auto system = CreateSystem();
        const std::vector<std::int16_t> pcm{16384};
        const auto sampler = Sampler(system, pcm, flag == SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_REPEAT ? 1 : 0);
        Patch(sampler, Mastering(system, 1));
        Control(sampler, SCE_NGS2_SAMPLER_VOICE_PARAM_PITCH, Ngs2SamplerVoicePitchParam{{}, 2.0f});
        PcmResetCallback callback{{}, flag};
        for (std::uint32_t frame = 0; frame < Grain * 2 + 2; ++frame) callback.pcm.push_back(static_cast<std::int16_t>(1000 + frame * 100));
        Control(sampler, SCE_NGS2_VOICE_PARAM_CALLBACK,
                Ngs2VoiceCallbackParam{{}, ResetPcm, reinterpret_cast<std::uintptr_t>(&callback), flag, 0});
        Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);
        const auto first = RenderI16(system);
        Require(callback.calls == 1);
        Ngs2SamplerVoiceState state{};
        Require(sceNgs2VoiceGetState(sampler, &state.voice_state, sizeof(state)) == SCE_NGS2_OK);
        if (state.num_decoded_samples != 1) std::fprintf(stderr, "PCM callback reset consumed %llu samples before the next grain, expected 1\n", static_cast<unsigned long long>(state.num_decoded_samples));
        Require(state.num_decoded_samples == 1 && state.decoded_data_size == sizeof(std::int16_t) && state.waveform_data == callback.pcm.data());
        for (std::uint32_t frame = 0; frame < Grain; ++frame) Require(first[frame] == (frame == 0 ? 16384 : 0));
        const auto second = RenderI16(system);
        for (std::uint32_t frame = 0; frame < Grain; ++frame) Require(second[frame] == callback.pcm[frame * 2]);
        Require(callback.calls == 1);
        Require(sceNgs2VoiceGetState(sampler, &state.voice_state, sizeof(state)) == SCE_NGS2_OK);
        Require(state.num_decoded_samples == 1 + Grain * 2 && state.waveform_data == callback.pcm.data() + Grain * 2);
        Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    }
}

static void TestPcmCallbackRefill() {
    for (const bool pause : {false, true}) {
        const auto system = CreateSystem();
        const auto sampler = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
        Control(sampler, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 1, 48000, 0, 0, 0}});
        Patch(sampler, Mastering(system, 1));
        const std::int16_t pcm = 16384;
        const Ngs2WaveformBlock block{0, sizeof(pcm), 0, 0, 1, 0, 0};
        Control(sampler, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS,
                Ngs2SamplerVoiceWaveformBlocksParam{{}, &pcm, SCE_NGS2_WAVEFORM_BLOCKS_FLAG_CONTINUE, 1, &block});
        PcmResetCallback callback{{}, SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END, 0, 0, pause};
        for (std::uint32_t frame = 0; frame < Grain * 2; ++frame) callback.pcm.push_back(static_cast<std::int16_t>(1000 + frame * 100));
        Control(sampler, SCE_NGS2_VOICE_PARAM_CALLBACK,
                Ngs2VoiceCallbackParam{{}, ResetPcm, reinterpret_cast<std::uintptr_t>(&callback), callback.flag, 0});
        Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);
        const auto first = RenderI16(system);
        Require(callback.calls == 1 && first.front() == pcm);
        for (std::uint32_t frame = 1; frame < Grain; ++frame) Require(first[frame] == (pause ? 0 : callback.pcm[frame - 1]));
        Ngs2SamplerVoiceState state{};
        Require(sceNgs2VoiceGetState(sampler, &state.voice_state, sizeof(state)) == SCE_NGS2_OK);
        const std::uint32_t consumed = pause ? 0 : Grain - 1;
        Require(state.num_decoded_samples == consumed + 1 && state.waveform_data == callback.pcm.data() + consumed);
        if (pause) Event(sampler, SCE_NGS2_VOICE_EVENT_RESUME);
        const auto second = RenderI16(system);
        for (std::uint32_t frame = 0; frame < Grain; ++frame) Require(second[frame] == callback.pcm[consumed + frame]);
        Require(callback.calls == 1);
        Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    }
}

struct PcmSetupCallback {
    uintptr_t master;
    std::uint32_t channels;
    std::uint32_t flag;
    std::vector<std::int16_t> pcm;
    int calls = 0;
};

static void APS5_VABI SetupPcm(const Ngs2VoiceCallbackInfo* info) {
    auto& callback = *reinterpret_cast<PcmSetupCallback*>(info->callback_data);
    if (++callback.calls != 1) return;
    Require(info->flag == callback.flag);
    Control(info->voice_handle, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP,
            Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, callback.channels, 48000, 0, 0, 0}});
    const Ngs2WaveformBlock block{0, callback.pcm.size() * sizeof(std::int16_t), 0, 0, Grain * 2, 0, 0};
    Control(info->voice_handle, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS,
            Ngs2SamplerVoiceWaveformBlocksParam{{}, callback.pcm.data(), 0, 1, &block});
    Patch(info->voice_handle, callback.master);
    Event(info->voice_handle, SCE_NGS2_VOICE_EVENT_PLAY);
}

static void TestPcmCallbackSetup() {
    for (const auto channels : {2u, 8u}) {
        for (const auto flag : {SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END, SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_REPEAT}) {
            const auto system = CreateSystem();
            const std::vector<std::int16_t> pcm{16384};
            const auto sampler = Sampler(system, pcm, flag == SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_REPEAT ? 1 : 0);
            PcmSetupCallback callback{Mastering(system, channels), channels, flag, {}};
            for (std::uint32_t frame = 0; frame < Grain * 2; ++frame) {
                for (std::uint32_t channel = 0; channel < channels; ++channel) callback.pcm.push_back(static_cast<std::int16_t>(1000 + frame * 25 + channel * 100));
            }
            Patch(sampler, callback.master);
            Control(sampler, SCE_NGS2_VOICE_PARAM_CALLBACK,
                    Ngs2VoiceCallbackParam{{}, SetupPcm, reinterpret_cast<std::uintptr_t>(&callback), flag, 0});
            Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);
            std::vector<std::int16_t> output(Grain * channels, -1);
            const Ngs2RenderBufferInfo info{output.data(), output.size() * sizeof(std::int16_t), SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, channels};
            Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_OK);
            Require(callback.calls == 1 && std::all_of(output.begin(), output.end(), [](std::int16_t sample) { return sample == 0; }));
            Ngs2SamplerVoiceState state{};
            Require(sceNgs2VoiceGetState(sampler, &state.voice_state, sizeof(state)) == SCE_NGS2_OK);
            Require(state.num_decoded_samples == 0 && state.decoded_data_size == 0 && state.waveform_data == callback.pcm.data());
            Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_OK);
            Require(callback.calls == 1 && std::equal(output.begin(), output.end(), callback.pcm.begin()));
            Require(sceNgs2VoiceGetState(sampler, &state.voice_state, sizeof(state)) == SCE_NGS2_OK);
            Require(state.num_decoded_samples == Grain && state.decoded_data_size == Grain * channels * sizeof(std::int16_t));
            Require(state.waveform_data == callback.pcm.data() + Grain * channels);
            Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
        }
    }
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

static std::vector<float> MatrixLevelFrame(const std::vector<float>& levels, bool command) {
    const auto system = CreateSystem();
    const auto master = Mastering(system, 2);
    const std::vector<std::int16_t> pcm(Grain, 16384);
    const auto sampler = Sampler(system, pcm, 0);
    Patch(sampler, master);
    if (command) {
        const Ngs2VoiceCommand set{5, 0, 0x11, static_cast<std::uint16_t>(levels.size()), {.levels = levels.data()}};
        Require(sceNgs2VoiceRunCommands(sampler, &set, 1) == SCE_NGS2_OK);
    } else {
        Control(sampler, SCE_NGS2_VOICE_PARAM_MATRIX_LEVELS, Ngs2VoiceMatrixLevelsParam{{}, 0, static_cast<std::uint32_t>(levels.size()), levels.data()});
    }
    Control(sampler, SCE_NGS2_VOICE_PARAM_PORT_MATRIX, Ngs2VoicePortMatrixParam{{}, 0, 0});
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);
    std::vector<float> out(Grain * 2, -1.0f);
    const Ngs2RenderBufferInfo info{out.data(), out.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 2};
    Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_OK);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    return {out[0], out[1]};
}

static void TestMatrixLevelClamp() {
    const float inf = INFINITY;
    for (bool command : {false, true}) {
        Require(MatrixLevelFrame({4.0f, -4.0f}, command) == (std::vector<float>{2.0f, -2.0f}));
        Require(MatrixLevelFrame({4.0001f, -4.5f}, command) == (std::vector<float>{2.0f, -2.0f}));
        Require(MatrixLevelFrame({100.0f, -100.0f}, command) == (std::vector<float>{2.0f, -2.0f}));
        Require(MatrixLevelFrame({inf, -inf}, command) == (std::vector<float>{2.0f, -2.0f}));
        Require(MatrixLevelFrame({3.5f, -0.25f}, command) == (std::vector<float>{1.75f, -0.125f}));
        const auto nan = MatrixLevelFrame({NAN, 0.5f}, command);
        Require(std::isnan(nan[0]) && nan[1] == 0.25f);
    }

    const auto system = CreateSystem();
    const std::vector<std::int16_t> silence(Grain, 0);
    const auto sampler = Sampler(system, silence, 0);
    const float levels[2] = {1.0f, 1.0f};
    bool empty = false;
    try { Control(sampler, SCE_NGS2_VOICE_PARAM_MATRIX_LEVELS, Ngs2VoiceMatrixLevelsParam{{}, 0, 0, levels}); } catch (const std::invalid_argument&) { empty = true; }
    Require(empty);
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

static std::vector<float> RenderReverb(const ReverbCase& reverbCase, std::uint32_t frames) {
    const auto firstBuffer = usedBuffers;
    const auto system = CreateSystem();
    Require(sceNgs2SystemSetGrainSamples(system, 256) == SCE_NGS2_OK);
    const auto channels = reverbCase.channels;
    const auto master = Mastering(system, channels);
    const auto reverb = Voice(CreateRack(system, SCE_NGS2_RACK_ID_REVERB));
    Control(reverb, SCE_NGS2_REVERB_VOICE_PARAM_SETUP, Ngs2ReverbVoiceSetupParam{{}, channels, channels, 0, 0});
    Control(reverb, SCE_NGS2_REVERB_VOICE_PARAM_I3DL2, Ngs2ReverbVoiceI3DL2Param{{}, reverbCase.params});
    Patch(reverb, master);
    Event(reverb, SCE_NGS2_VOICE_EVENT_PLAY);
    std::vector<std::int16_t> pcm(4096 * channels, 0);
    std::uint32_t seed = 1;
    for (std::uint32_t i = 0; reverbCase.noise && i + 3 * channels < pcm.size(); i++) {
        seed = seed * 1664525u + 1013904223u;
        if (i < 3000 * channels) pcm[i + 3 * channels] = static_cast<std::int16_t>(static_cast<std::int32_t>((seed >> 16) & 0x3fff) - 0x2000);
    }
    for (std::uint32_t c = 0; !reverbCase.noise && c < channels; c++) pcm[3 * channels + c] = static_cast<std::int16_t>((16384 - 4000 * static_cast<std::int32_t>(c)) * reverbCase.amplitude);
    const auto sampler = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
    Control(sampler, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, channels, 48000, 0, 0, 0}});
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
        if (static_cast<std::int32_t>(grain) == reverbCase.changeGrain) Control(reverb, SCE_NGS2_REVERB_VOICE_PARAM_I3DL2, Ngs2ReverbVoiceI3DL2Param{{}, reverbCase.change});
        const Ngs2RenderBufferInfo info{out.data(), out.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, channels};
        Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_OK);
        all.insert(all.end(), out.begin(), out.end());
    }
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    usedBuffers = firstBuffer;
    return all;
}

static void TestReverb() {
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
    for (const auto& reverbCase : cases) {
        const auto out = RenderReverb(reverbCase, 32256);
        for (const auto& expected : reverbCase.expected) {
            for (std::uint32_t c = 0; c < reverbCase.channels; c++) {
                if (c == 3) continue;
                const float value = out[expected.frame * reverbCase.channels + c];
                Require(std::fabs(value - expected.samples[c]) <= 2e-7f + std::fabs(expected.samples[c]) * 1e-5f);
            }
        }
    }
}

static std::vector<float> RenderReverbChannels(std::uint32_t inputs, std::uint32_t outputs, const Ngs2ReverbI3DL2Param& params, const std::vector<std::int16_t>& pcm, std::uint32_t frames) {
    const auto firstBuffer = usedBuffers;
    const auto system = CreateSystem();
    Require(sceNgs2SystemSetGrainSamples(system, 256) == SCE_NGS2_OK);
    const auto master = Mastering(system, outputs);
    const auto reverb = Voice(CreateRack(system, SCE_NGS2_RACK_ID_REVERB));
    Control(reverb, SCE_NGS2_REVERB_VOICE_PARAM_SETUP, Ngs2ReverbVoiceSetupParam{{}, inputs, outputs, 0, 0});
    Control(reverb, SCE_NGS2_REVERB_VOICE_PARAM_I3DL2, Ngs2ReverbVoiceI3DL2Param{{}, params});
    Patch(reverb, master);
    Event(reverb, SCE_NGS2_VOICE_EVENT_PLAY);
    const auto sampler = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
    Control(sampler, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, inputs, 48000, 0, 0, 0}});
    const Ngs2WaveformBlock block{0, pcm.size() * sizeof(std::int16_t), 0, 0, static_cast<std::uint32_t>(pcm.size() / inputs), 0, 0};
    Control(sampler, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, pcm.data(), 0, 1, &block});
    Patch(sampler, reverb);
    std::vector<float> identity(inputs * inputs, 0.0f);
    for (std::uint32_t c = 0; c < inputs; c++) identity[c * inputs + c] = 1.0f;
    Control(sampler, SCE_NGS2_VOICE_PARAM_MATRIX_LEVELS, Ngs2VoiceMatrixLevelsParam{{}, 0, inputs * inputs, identity.data()});
    Control(sampler, SCE_NGS2_VOICE_PARAM_PORT_MATRIX, Ngs2VoicePortMatrixParam{{}, 0, 0});
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);
    std::vector<float> all;
    std::vector<float> out(256 * outputs);
    for (std::uint32_t grain = 0; grain * 256 < frames; grain++) {
        const Ngs2RenderBufferInfo info{out.data(), out.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, outputs};
        Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_OK);
        all.insert(all.end(), out.begin(), out.end());
    }
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    usedBuffers = firstBuffer;
    return all;
}

static void TestReverbConversion() {
    Ngs2ReverbI3DL2Param mixed{1.0f, 1.0f, -1000, 0, 0, 2.91f, 1.3f, -602, 0.015f, -302, 0.022f, 100.0f, 100.0f, 5000.0f, {}};
    std::vector<std::int16_t> stereo(4096 * 2, 0);
    std::uint32_t seed = 1;
    for (std::uint32_t i = 0; i < 3000 * 2; i++) {
        seed = seed * 1664525u + 1013904223u;
        stereo[i] = static_cast<std::int16_t>(static_cast<std::int32_t>((seed >> 16) & 0x3fff) - 0x2000);
    }
    const auto same = RenderReverbChannels(2, 2, mixed, stereo, 8192);
    for (std::uint32_t outputs : {6u, 8u}) {
        const auto surround = RenderReverbChannels(2, outputs, mixed, stereo, 8192);
        float sides = 0.0f;
        float lfe = 0.0f;
        for (std::uint32_t frame = 0; frame < 8192; frame++) {
            for (std::uint32_t c = 0; c < 2; c++) Require(std::fabs(surround[frame * outputs + c] - same[frame * 2 + c]) <= 1e-7f + std::fabs(same[frame * 2 + c]) * 1e-5f);
            for (std::uint32_t c = 2; c < outputs; c++) {
                if (c != 3) sides = std::max(sides, std::fabs(surround[frame * outputs + c]));
            }
            lfe = std::max(lfe, std::fabs(surround[frame * outputs + 3]));
        }
        Require(sides > 1e-4f && lfe == 0.0f);
    }

    auto dry = mixed;
    dry.wet = 0.0f;
    const std::int16_t levels[6] = {8192, 4096, 2048, 1024, 512, 256};
    std::vector<std::int16_t> six(4096 * 6);
    for (std::size_t i = 0; i < six.size(); i++) six[i] = levels[i % 6];
    const auto down = RenderReverbChannels(6, 2, dry, six, 2048);
    const float half = std::numbers::sqrt2_v<float> / 2.0f;
    const float left = (8192.0f + 2048.0f * half + 512.0f * 0.5f) / 32768.0f;
    const float right = (4096.0f + 2048.0f * half + 256.0f * 0.5f) / 32768.0f;
    Require(std::fabs(down[1024 * 2] - left) <= left * 1e-5f && std::fabs(down[1024 * 2 + 1] - right) <= right * 1e-5f);
}

static void TestReverbSetup() {
    const auto system = CreateSystem();
    const auto master = Mastering(system, 2);
    const auto reverb = Voice(CreateRack(system, SCE_NGS2_RACK_ID_REVERB));
    for (std::uint32_t channels : {3u, 4u, 5u, 7u}) {
        bool rejected = false;
        try { Control(reverb, SCE_NGS2_REVERB_VOICE_PARAM_SETUP, Ngs2ReverbVoiceSetupParam{{}, channels, channels, 0, 0}); } catch (const std::invalid_argument&) { rejected = true; }
        Require(rejected);
    }
    for (const auto& counts : {std::array<std::uint32_t, 2>{3, 2}, std::array<std::uint32_t, 2>{2, 7}}) {
        bool rejected = false;
        try { Control(reverb, SCE_NGS2_REVERB_VOICE_PARAM_SETUP, Ngs2ReverbVoiceSetupParam{{}, counts[0], counts[1], 0, 0}); } catch (const std::invalid_argument&) { rejected = true; }
        Require(rejected);
    }
    Control(reverb, SCE_NGS2_REVERB_VOICE_PARAM_SETUP, Ngs2ReverbVoiceSetupParam{{}, 1, 2, 0, 0});
    Control(reverb, SCE_NGS2_REVERB_VOICE_PARAM_SETUP, Ngs2ReverbVoiceSetupParam{{}, 2, 2, 0, 0});
    Ngs2ReverbI3DL2Param invalid{1.0f, 0.0f, 0, 0, 0, 1.0f, 1.0f, 0, 0.0f, 0, 0.0f, 100.0f, 100.0f, 5000.0f, {}};
    invalid.decay_time = NAN;
    bool nan = false;
    try { Control(reverb, SCE_NGS2_REVERB_VOICE_PARAM_I3DL2, Ngs2ReverbVoiceI3DL2Param{{}, invalid}); } catch (const std::invalid_argument&) { nan = true; }
    Require(nan);
    Patch(reverb, master);
    Event(reverb, SCE_NGS2_VOICE_EVENT_PLAY);
    const std::vector<std::int16_t> pcm(Grain * 64, 16384);
    const auto sampler = Sampler(system, pcm, 0);
    Patch(sampler, reverb);
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);
    std::vector<float> out(Grain * 2);
    const Ngs2RenderBufferInfo info{out.data(), out.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 2};
    float loudest = 0.0f;
    for (std::uint32_t grain = 0; grain < 600; grain++) {
        Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_OK);
        for (float sample : out) loudest = std::max(loudest, std::fabs(sample));
    }
    Require(loudest > 0.0f && loudest < 1e-8f);
    Ngs2VoiceState state{};
    Require(sceNgs2VoiceGetState(reverb, &state, sizeof(state)) == SCE_NGS2_OK && (state.state_flags & SCE_NGS2_VOICE_STATE_FLAG_INUSE) != 0);
    Require(sceNgs2SystemSetSampleRate(system, 7000) == SCE_NGS2_ERROR_INVALID_SAMPLE_RATE);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);

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
    Require(sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_REVERB, &option.rack_option, &query) == SCE_NGS2_OK);
    for (std::uint32_t size : {0u, 2u}) {
        option.reverb_size = size;
        bool unsupported = false;
        try { sceNgs2RackQueryBufferSize(SCE_NGS2_RACK_ID_REVERB, &option.rack_option, &query); } catch (const std::runtime_error&) { unsupported = true; }
        Require(unsupported);
    }
}

static void TestSampleRate() {
    const auto system = CreateSystem();
    Require(sceNgs2SystemSetSampleRate(0x1234, 96000) == SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE);
    Require(sceNgs2SystemSetSampleRate(system, 0) == SCE_NGS2_ERROR_INVALID_SAMPLE_RATE);
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

static bool SystemOptionThrows(const Ngs2SystemOption& option) {
    Ngs2ContextBufferInfo query{};
    try {
        sceNgs2SystemQueryBufferSize(&option, &query);

    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

static void TestSystemOptionFlags() {
    Ngs2SystemOption option{};
    Require(sceNgs2SystemResetOption(&option) == SCE_NGS2_OK && option.flags == 0);
    option.flags = 0x2;
    Ngs2ContextBufferInfo query{};
    Require(sceNgs2SystemQueryBufferSize(&option, &query) == SCE_NGS2_OK && query.host_buffer_size != 0);
    const auto info = Buffer(query);
    uintptr_t system = 0;
    Require(sceNgs2SystemCreate(&option, &info, &system) == SCE_NGS2_OK && system != 0);
    Require(sceNgs2SystemSetGrainSamples(system, Grain) == SCE_NGS2_OK);
    const auto master = Mastering(system, 1);
    const std::vector<std::int16_t> pcm{1000, 2000, 3000, 4000};
    const auto sampler = Sampler(system, pcm, 1);
    Patch(sampler, master);
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);
    const auto out = RenderI16(system);
    Require(std::equal(pcm.begin(), pcm.end(), out.begin()));
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);

    for (const std::uint32_t flags : {0x1u, 0x3u, 0x4u, 0x80000000u, 0x80000002u}) {
        option.flags = flags;
        Require(SystemOptionThrows(option));
    }
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

    Require(sceNgs2RackSetUserData(0x1234, 5) == SCE_NGS2_ERROR_INVALID_RACK_HANDLE);
    Require(sceNgs2RackSetUserData(system, 5) == SCE_NGS2_ERROR_INVALID_RACK_HANDLE);
    value = 1;
    Require(sceNgs2RackGetUserData(0x1234, &value) == SCE_NGS2_ERROR_INVALID_RACK_HANDLE && value == 1);
    Require(sceNgs2RackGetUserData(system, &value) == SCE_NGS2_ERROR_INVALID_RACK_HANDLE && value == 1);
    Require(sceNgs2RackGetUserData(0x1234, nullptr) == SCE_NGS2_ERROR_INVALID_RACK_HANDLE);
    const auto rack = CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER);
    const auto other = CreateRack(system, SCE_NGS2_RACK_ID_MASTERING);
    Require(sceNgs2RackGetUserData(rack, &value) == SCE_NGS2_OK && value == 0);
    Require(sceNgs2RackSetUserData(rack, 0x0123456789abcdef) == SCE_NGS2_OK);
    Require(sceNgs2RackGetUserData(rack, &value) == SCE_NGS2_OK && value == 0x0123456789abcdef);
    Require(sceNgs2RackGetUserData(other, &value) == SCE_NGS2_OK && value == 0);
    Require(sceNgs2SystemGetUserData(system, &value) == SCE_NGS2_OK && value == 0xfedcba9876543210);
    Require(sceNgs2RackSetUserData(other, 0) == SCE_NGS2_OK);
    Require(sceNgs2RackGetUserData(rack, &value) == SCE_NGS2_OK && value == 0x0123456789abcdef);
    threw = false;
    try {
        sceNgs2RackGetUserData(rack, nullptr);

    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    Require(sceNgs2RackDestroy(rack, nullptr) == SCE_NGS2_OK);
    value = 1;
    Require(sceNgs2RackGetUserData(rack, &value) == SCE_NGS2_ERROR_INVALID_RACK_HANDLE && value == 1);
    Require(sceNgs2RackSetUserData(rack, 5) == SCE_NGS2_ERROR_INVALID_RACK_HANDLE);
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

static void TestMasteringGainClampedLikePs5() {
    std::deque<std::vector<std::int16_t>> waveforms;
    const auto chain = [&waveforms](std::uint32_t channels, std::uint32_t grains, uintptr_t& master) {
        const auto system = CreateSystem();
        master = Mastering(system, channels);
        const auto& pcm = waveforms.emplace_back(Grain * grains, 16384);
        const auto source = Sampler(system, pcm, 0);
        const std::vector<float> levels(channels, 1.0f);
        Control(source, SCE_NGS2_VOICE_PARAM_MATRIX_LEVELS, Ngs2VoiceMatrixLevelsParam{{}, 0, channels, levels.data()});
        Control(source, SCE_NGS2_VOICE_PARAM_PORT_MATRIX, Ngs2VoicePortMatrixParam{{}, 0, 0});
        Patch(source, master);
        Event(source, SCE_NGS2_VOICE_EVENT_PLAY);
        return system;
    };
    const auto render = [](uintptr_t system, std::uint32_t channels) {
        std::vector<float> out(Grain * channels, -1.0f);
        const Ngs2RenderBufferInfo info{out.data(), out.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, channels};
        Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_OK);
        return out;
    };
    const auto expect = [](const std::vector<float>& out, std::uint32_t channels, float fbwOut, float lfeOut) {
        for (std::uint32_t i = 0; i < Grain; i++) {
            for (std::uint32_t channel = 0; channel < channels; channel++) {
                const float level = channel == 3 && channels >= 6 ? lfeOut : fbwOut;
                const float sample = out[i * channels + channel];
                Require(std::isnan(level) ? std::isnan(sample) : sample == 0.5f * level);
            }
        }
    };
    const auto gain = [&](std::uint32_t channels, float fbw, float lfe, float fbwOut, float lfeOut) {
        uintptr_t master = 0;
        const auto system = chain(channels, 1, master);
        Control(master, SCE_NGS2_MASTERING_VOICE_PARAM_GAIN, Ngs2MasteringVoiceGainParam{{}, fbw, lfe});
        expect(render(system, channels), channels, fbwOut, lfeOut);
        Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    };
    const float inf = INFINITY;
    for (const std::uint32_t channels : {6u, 8u}) {
        gain(channels, 32.0f, 1.0f, 32.0f, 1.0f);
        gain(channels, 33.0f, 1.0f, 32.0f, 1.0f);
        gain(channels, 64.0f, 1.0f, 32.0f, 1.0f);
        gain(channels, 1e30f, 1.0f, 32.0f, 1.0f);
        gain(channels, inf, 1.0f, 32.0f, 1.0f);
        gain(channels, -32.0f, 1.0f, -32.0f, 1.0f);
        gain(channels, -33.0f, 1.0f, -32.0f, 1.0f);
        gain(channels, -inf, 1.0f, -32.0f, 1.0f);
        gain(channels, NAN, 1.0f, NAN, 1.0f);
        gain(channels, 1.0f, 32.0f, 1.0f, 32.0f);
        gain(channels, 1.0f, 33.0f, 1.0f, 32.0f);
        gain(channels, 1.0f, 1e30f, 1.0f, 32.0f);
        gain(channels, 1.0f, inf, 1.0f, 32.0f);
        gain(channels, 1.0f, -33.0f, 1.0f, -32.0f);
        gain(channels, 1.0f, -inf, 1.0f, -32.0f);
        gain(channels, 1.0f, NAN, 1.0f, NAN);
    }
    gain(1, 0.5f, 2.0f, 0.5f, 0.5f);
    gain(1, inf, NAN, 32.0f, 32.0f);
    gain(2, -inf, 1e30f, -32.0f, -32.0f);
    gain(2, 2.0f, 0.5f, 2.0f, 2.0f);

    uintptr_t master = 0;
    const auto system = chain(6, 4, master);
    expect(render(system, 6), 6, 1.0f, 1.0f);
    Control(master, SCE_NGS2_MASTERING_VOICE_PARAM_GAIN, Ngs2MasteringVoiceGainParam{{}, 0.5f, 0.25f});
    expect(render(system, 6), 6, 0.5f, 0.25f);
    Event(master, SCE_NGS2_VOICE_EVENT_STOP_IMM);
    Event(master, SCE_NGS2_VOICE_EVENT_PLAY);
    expect(render(system, 6), 6, 0.5f, 0.25f);
    Control(master, SCE_NGS2_MASTERING_VOICE_PARAM_SETUP, Ngs2MasteringVoiceSetupParam{{}, 6, 0});
    Event(master, SCE_NGS2_VOICE_EVENT_PLAY);
    expect(render(system, 6), 6, 1.0f, 1.0f);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
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

static bool AnsweredWithin(const std::atomic<bool>& answered, std::chrono::milliseconds limit = std::chrono::seconds(2)) {
    const auto deadline = std::chrono::steady_clock::now() + limit;
    while (!answered && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return answered;
}

static void TestRackLockLeavesQueries() {
    const auto system = CreateSystem();
    const auto rack = CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER);
    const auto voice = Voice(rack);
    Require(sceNgs2RackLock(0x1234) == SCE_NGS2_ERROR_INVALID_RACK_HANDLE);
    Require(sceNgs2RackLock(rack) == SCE_NGS2_OK);
    Require(sceNgs2RackLock(rack) == SCE_NGS2_OK);
    std::atomic<bool> answered = false;
    std::thread query([&] {
        std::uint32_t flags = 0;
        Require(sceNgs2VoiceGetStateFlags(voice, &flags) == SCE_NGS2_OK);
        answered = true;
    });
    const bool answeredWhileLocked = AnsweredWithin(answered);
    std::atomic<bool> rendered = false;
    std::thread render([&] {
        RenderI16(system);
        rendered = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const bool renderedWhileLocked = rendered;
    Require(sceNgs2RackUnlock(rack) == SCE_NGS2_OK);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const bool renderedWhileLockedOnce = rendered;
    Require(sceNgs2RackUnlock(rack) == SCE_NGS2_OK);
    query.join();
    render.join();
    Require(answeredWhileLocked);
    Require(!renderedWhileLocked && !renderedWhileLockedOnce && rendered);
    Require(sceNgs2RackUnlock(rack) == SCE_NGS2_ERROR_INVALID_RACK_HANDLE);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

struct QueryInCallback {
    uintptr_t voice = 0;
    std::atomic<bool> answered = false;
    bool answeredInCallback = false;
    std::thread query;
};

static void APS5_VABI QueryFromAnotherThread(const Ngs2VoiceCallbackInfo* info) {
    auto& callback = *reinterpret_cast<QueryInCallback*>(info->callback_data);
    callback.query = std::thread([&callback] {
        std::uint32_t flags = 0;
        Require(sceNgs2VoiceGetStateFlags(callback.voice, &flags) == SCE_NGS2_OK);
        callback.answered = true;
    });
    callback.answeredInCallback = AnsweredWithin(callback.answered);
}

static void TestVoiceCallbackLeavesQueries() {
    const auto system = CreateSystem();
    const auto master = Mastering(system, 1);
    const std::vector<std::int16_t> pcm(Grain / 2, 1000);
    const auto sampler = Sampler(system, pcm, 0);
    Patch(sampler, master);
    QueryInCallback callback;
    callback.voice = sampler;
    Control(sampler, SCE_NGS2_VOICE_PARAM_CALLBACK, Ngs2VoiceCallbackParam{{}, QueryFromAnotherThread, reinterpret_cast<std::uintptr_t>(&callback), SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END, 0});
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);
    RenderI16(system);
    Require(callback.query.joinable());
    callback.query.join();
    Require(callback.answered && callback.answeredInCallback);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

struct RackLockInCallback {
    uintptr_t rack = 0;
    std::atomic<bool> locked = false;
    std::atomic<bool> callbackReturned = false;
    std::atomic<bool> release = false;
    bool lockedDuringCallback = false;
    std::thread locker;
};

static void APS5_VABI LockFromAnotherThread(const Ngs2VoiceCallbackInfo* info) {
    auto& callback = *reinterpret_cast<RackLockInCallback*>(info->callback_data);
    callback.locker = std::thread([&callback] {
        Require(sceNgs2RackLock(callback.rack) == SCE_NGS2_OK);
        callback.lockedDuringCallback = !callback.callbackReturned;
        callback.locked = true;
        while (!callback.release) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        Require(sceNgs2RackUnlock(callback.rack) == SCE_NGS2_OK);
    });
    static_cast<void>(AnsweredWithin(callback.locked, std::chrono::milliseconds(200)));
    callback.callbackReturned = true;
}

static void TestRackLockWaitsForRender() {
    const auto system = CreateSystem();
    const auto master = Mastering(system, 1);
    const std::vector<std::int16_t> pcm(Grain / 2, 1000);
    RackLockInCallback callback;
    const auto sampler = Sampler(system, pcm, 0, &callback.rack);
    Patch(sampler, master);
    Control(sampler, SCE_NGS2_VOICE_PARAM_CALLBACK, Ngs2VoiceCallbackParam{{}, LockFromAnotherThread, reinterpret_cast<std::uintptr_t>(&callback), SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END, 0});
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);
    RenderI16(system);
    const bool lockedAfterRender = AnsweredWithin(callback.locked);
    callback.release = true;
    Require(callback.locker.joinable());
    callback.locker.join();
    Require(lockedAfterRender && !callback.lockedDuringCallback);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

struct CrossSystemCallback {
    uintptr_t otherRack = 0;
    std::atomic<bool>* entered = nullptr;
    const std::atomic<bool>* awaited = nullptr;
};

static void APS5_VABI LockAnotherSystem(const Ngs2VoiceCallbackInfo* info) {
    auto& callback = *reinterpret_cast<CrossSystemCallback*>(info->callback_data);
    *callback.entered = true;
    static_cast<void>(AnsweredWithin(*callback.awaited, std::chrono::milliseconds(200)));
    Require(sceNgs2RackLock(callback.otherRack) == SCE_NGS2_OK);
    Require(sceNgs2RackUnlock(callback.otherRack) == SCE_NGS2_OK);
}

static uintptr_t CallbackSampler(uintptr_t system, CrossSystemCallback& callback, uintptr_t* rack) {
    static const std::vector<std::int16_t> pcm(Grain / 2, 1000);
    const auto sampler = Sampler(system, pcm, 0, rack);
    Patch(sampler, Mastering(system, 1));
    Control(sampler, SCE_NGS2_VOICE_PARAM_CALLBACK, Ngs2VoiceCallbackParam{{}, LockAnotherSystem, reinterpret_cast<std::uintptr_t>(&callback), SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END, 0});
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);
    return sampler;
}

static void TestCallbacksLockEachOthersSystem() {
    const auto first = CreateSystem();
    const auto second = CreateSystem();
    std::atomic<bool> firstEntered = false;
    std::atomic<bool> secondEntered = false;
    CrossSystemCallback firstCallback{0, &firstEntered, &secondEntered};
    CrossSystemCallback secondCallback{0, &secondEntered, &firstEntered};
    uintptr_t firstRack = 0;
    uintptr_t secondRack = 0;
    CallbackSampler(first, firstCallback, &firstRack);
    CallbackSampler(second, secondCallback, &secondRack);
    firstCallback.otherRack = secondRack;
    secondCallback.otherRack = firstRack;
    std::atomic<bool> firstRendered = false;
    std::atomic<bool> secondRendered = false;
    std::thread firstRender([&] {
        RenderI16(first);
        firstRendered = true;
    });
    std::thread secondRender([&] {
        RenderI16(second);
        secondRendered = true;
    });
    Require(AnsweredWithin(firstRendered) && AnsweredWithin(secondRendered));
    firstRender.join();
    secondRender.join();
    Require(firstEntered && secondEntered);
    Require(sceNgs2SystemDestroy(first, nullptr) == SCE_NGS2_OK);
    Require(sceNgs2SystemDestroy(second, nullptr) == SCE_NGS2_OK);
}

static void TestRackLocksAcrossSystemsAndRender() {
    const auto first = CreateSystem();
    const auto second = CreateSystem();
    const auto heldRack = CreateRack(second, SCE_NGS2_RACK_ID_SAMPLER);
    std::atomic<bool> entered = false;
    const std::atomic<bool> never = false;
    CrossSystemCallback callback{heldRack, &entered, &never};
    uintptr_t renderedRack = 0;
    CallbackSampler(first, callback, &renderedRack);
    std::atomic<bool> holding = false;
    std::atomic<bool> released = false;
    std::thread locker([&] {
        Require(sceNgs2RackLock(heldRack) == SCE_NGS2_OK);
        holding = true;
        static_cast<void>(AnsweredWithin(entered, std::chrono::milliseconds(200)));
        Require(sceNgs2RackLock(renderedRack) == SCE_NGS2_OK);
        Require(sceNgs2RackUnlock(renderedRack) == SCE_NGS2_OK);
        Require(sceNgs2RackUnlock(heldRack) == SCE_NGS2_OK);
        released = true;
    });
    Require(AnsweredWithin(holding));
    std::atomic<bool> rendered = false;
    std::thread render([&] {
        RenderI16(first);
        rendered = true;
    });
    Require(AnsweredWithin(released) && AnsweredWithin(rendered));
    locker.join();
    render.join();
    Require(entered);
    Require(sceNgs2SystemDestroy(first, nullptr) == SCE_NGS2_OK);
    Require(sceNgs2SystemDestroy(second, nullptr) == SCE_NGS2_OK);
}

static void TestRackDestroyWaitsForLock() {
    const auto system = CreateSystem();
    const auto rack = CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER);
    Require(sceNgs2RackLock(rack) == SCE_NGS2_OK);
    std::atomic<bool> destroyed = false;
    std::thread destroyer([&] {
        Require(sceNgs2RackDestroy(rack, nullptr) == SCE_NGS2_OK);
        destroyed = true;
    });
    const bool destroyedWhileLocked = AnsweredWithin(destroyed, std::chrono::milliseconds(200));
    Require(!destroyedWhileLocked && sceNgs2RackUnlock(rack) == SCE_NGS2_OK);
    Require(AnsweredWithin(destroyed));
    destroyer.join();
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

struct DestroyInCallback {
    uintptr_t handle = 0;
    bool system = false;
};

static void APS5_VABI DestroyFromCallback(const Ngs2VoiceCallbackInfo* info) {
    const auto& destroy = *reinterpret_cast<const DestroyInCallback*>(info->callback_data);
    static_cast<void>(destroy.system ? sceNgs2SystemDestroy(destroy.handle, nullptr) : sceNgs2RackDestroy(destroy.handle, nullptr));
}

static void APS5_VABI RenderFromCallback(const Ngs2VoiceCallbackInfo* info) {
    static_cast<void>(RenderI16(info->callback_data));
}

static void TestDestroyDuringOwnRenderThrows() {
    for (const bool destroySystem : {false, true}) {
        const auto system = CreateSystem();
        uintptr_t rack = 0;
        const std::vector<std::int16_t> pcm(Grain / 2, 1000);
        const auto sampler = Sampler(system, pcm, 0, &rack);
        Patch(sampler, Mastering(system, 1));
        const DestroyInCallback destroy{destroySystem ? system : rack, destroySystem};
        Control(sampler, SCE_NGS2_VOICE_PARAM_CALLBACK, Ngs2VoiceCallbackParam{{}, DestroyFromCallback, reinterpret_cast<std::uintptr_t>(&destroy), SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END, 0});
        Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);
        bool refused = false;
        try {
            RenderI16(system);
        } catch (const std::runtime_error& error) {
            refused = std::string(error.what()).find("is not implemented") != std::string::npos;
        }
        Require(refused);
        Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    }
    for (const bool destroySystem : {false, true}) {
        const auto outer = CreateSystem();
        const auto inner = CreateSystem();
        uintptr_t outerRack = 0;
        const std::vector<std::int16_t> pcm(Grain / 2, 1000);
        const auto outerSampler = Sampler(outer, pcm, 0, &outerRack);
        Patch(outerSampler, Mastering(outer, 1));
        const auto innerSampler = Sampler(inner, pcm, 0);
        Patch(innerSampler, Mastering(inner, 1));
        const DestroyInCallback destroy{destroySystem ? outer : outerRack, destroySystem};
        Control(outerSampler, SCE_NGS2_VOICE_PARAM_CALLBACK, Ngs2VoiceCallbackParam{{}, RenderFromCallback, inner, SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END, 0});
        Control(innerSampler, SCE_NGS2_VOICE_PARAM_CALLBACK, Ngs2VoiceCallbackParam{{}, DestroyFromCallback, reinterpret_cast<std::uintptr_t>(&destroy), SCE_NGS2_VOICE_CALLBACK_FLAG_BLOCK_END, 0});
        Event(outerSampler, SCE_NGS2_VOICE_EVENT_PLAY);
        Event(innerSampler, SCE_NGS2_VOICE_EVENT_PLAY);
        bool refused = false;
        try {
            RenderI16(outer);
        } catch (const std::runtime_error& error) {
            refused = std::string(error.what()).find("is not implemented") != std::string::npos;
        }
        Require(refused);
        Require(sceNgs2SystemDestroy(inner, nullptr) == SCE_NGS2_OK);
        Require(sceNgs2SystemDestroy(outer, nullptr) == SCE_NGS2_OK);
    }
}

static void TestLock() {
    const auto system = CreateSystem();
    const auto voice = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
    Require(sceNgs2SystemLock(0x1234) == SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE);
    Require(sceNgs2SystemUnlock(0x1234) == SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE);
    Require(sceNgs2SystemUnlock(system) == SCE_NGS2_OK);

    Ngs2SystemInfo info{};
    Require(sceNgs2SystemLock(system) == SCE_NGS2_OK);
    Require(sceNgs2SystemLock(system) == SCE_NGS2_OK);
    std::atomic<bool> otherDone = false;
    std::thread other([&] {
        Ngs2SystemInfo otherInfo{};
        std::uint32_t flags = 0;
        Require(sceNgs2SystemGetInfo(system, &otherInfo, sizeof(otherInfo)) == SCE_NGS2_OK);
        Require(sceNgs2VoiceGetStateFlags(voice, &flags) == SCE_NGS2_OK);
        std::vector<std::int16_t> out(Grain * 2, -1);
        const Ngs2RenderBufferInfo buffer{out.data(), out.size() * sizeof(std::int16_t), SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 1};
        Require(sceNgs2SystemRender(system, &buffer, 1) == SCE_NGS2_OK);
        Require(std::all_of(out.begin(), out.begin() + Grain, [](std::int16_t sample) { return sample == 0; }));
        Require(std::all_of(out.begin() + Grain, out.end(), [](std::int16_t sample) { return sample == -1; }));
        Require(sceNgs2SystemGetInfo(system, &otherInfo, sizeof(otherInfo)) == SCE_NGS2_OK && otherInfo.render_count == 0);
        otherDone = true;
    });
    for (int i = 0; i < 200 && !otherDone; i++) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    Require(otherDone);
    other.join();
    RenderI16(system);
    Require(sceNgs2SystemGetInfo(system, &info, sizeof(info)) == SCE_NGS2_OK && info.render_count == 1);

    std::atomic<bool> done = false;
    std::thread blocked([&] {
        Require(sceNgs2SystemLock(system) == SCE_NGS2_OK);
        done = true;
        RenderI16(system);
        Require(sceNgs2SystemUnlock(system) == SCE_NGS2_OK);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Require(!done);
    Require(sceNgs2SystemUnlock(system) == SCE_NGS2_OK);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    Require(!done);
    Require(sceNgs2SystemUnlock(system) == SCE_NGS2_OK);
    blocked.join();
    Require(done);
    Require(sceNgs2SystemGetInfo(system, &info, sizeof(info)) == SCE_NGS2_OK && info.render_count == 2);

    Require(sceNgs2SystemLock(system) == SCE_NGS2_OK);
    std::thread unlocker([&] { Require(sceNgs2SystemUnlock(system) == SCE_NGS2_OK); });
    unlocker.join();
    std::thread renderer([&] { RenderI16(system); });
    renderer.join();
    Require(sceNgs2SystemGetInfo(system, &info, sizeof(info)) == SCE_NGS2_OK && info.render_count == 3);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static constexpr float HalfPower = std::numbers::sqrt2_v<float> / 2.0f;

static std::vector<float> MixFrame(std::uint32_t sourceChannels, std::uint32_t sourceChannel, std::uint32_t destChannels) {
    const auto firstBuffer = usedBuffers;
    const auto system = CreateSystem();
    const auto master = Mastering(system, destChannels);
    const auto voice = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SAMPLER));
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, sourceChannels, 48000, 0, 0, 0}});
    std::vector<std::int16_t> frames(Grain * sourceChannels, 0);
    for (std::uint32_t i = 0; i < Grain; i++) frames[i * sourceChannels + sourceChannel] = 16384;
    const Ngs2WaveformBlock block{0, frames.size() * sizeof(std::int16_t), 0, 0, Grain, 0, 0};
    Control(voice, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, frames.data(), 0, 1, &block});
    Patch(voice, master);
    Event(voice, SCE_NGS2_VOICE_EVENT_PLAY);
    std::vector<float> out(Grain * destChannels, -1.0f);
    const Ngs2RenderBufferInfo info{out.data(), out.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, destChannels};
    Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_OK);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
    usedBuffers = firstBuffer;
    return std::vector<float>(out.begin(), out.begin() + destChannels);
}

static void TestDefaultChannelMap() {
    const float half = 0.5f * HalfPower;
    const float quarter = 0.5f * (std::numbers::sqrt2_v<float> / 4.0f);
    Require(MixFrame(1, 0, 1) == std::vector<float>{0.5f});
    Require(MixFrame(1, 0, 2) == (std::vector<float>{half, half}));
    Require(MixFrame(1, 0, 6) == (std::vector<float>{0, 0, 0.5f, 0, 0, 0}));
    Require(MixFrame(1, 0, 8) == (std::vector<float>{0, 0, 0.5f, 0, 0, 0, 0, 0}));
    Require(MixFrame(2, 0, 1) == std::vector<float>{half} && MixFrame(2, 1, 1) == std::vector<float>{half});
    Require(MixFrame(2, 1, 2) == (std::vector<float>{0, 0.5f}));
    Require(MixFrame(2, 1, 6) == (std::vector<float>{0, 0.5f, 0, 0, 0, 0}));
    Require(MixFrame(3, 2, 2) == (std::vector<float>{0, 0}));
    Require(MixFrame(3, 2, 6) == (std::vector<float>{0, 0, 0, 0.5f, 0, 0}));
    Require(MixFrame(4, 2, 1) == std::vector<float>{0.25f});
    Require(MixFrame(4, 2, 2) == (std::vector<float>{half, 0}));
    Require(MixFrame(4, 3, 6) == (std::vector<float>{0, 0, 0, 0, 0, 0.5f}));
    Require(MixFrame(5, 2, 2) == (std::vector<float>{half, half}));
    Require(MixFrame(5, 3, 1) == std::vector<float>{0.25f});
    Require(MixFrame(5, 3, 2) == (std::vector<float>{0, 0}));
    Require(MixFrame(6, 3, 1) == std::vector<float>{0});
    Require(MixFrame(6, 4, 2) == (std::vector<float>{0.25f, 0}));
    Require(MixFrame(6, 5, 8) == (std::vector<float>{0, 0, 0, 0, 0, 0.5f, 0, 0}));
    Require(MixFrame(7, 6, 1) == std::vector<float>{quarter});
    Require(MixFrame(7, 6, 2) == (std::vector<float>{quarter, quarter}));
    Require(MixFrame(7, 6, 6) == (std::vector<float>{0, 0, 0, 0, half, half}));
    Require(MixFrame(7, 6, 8) == (std::vector<float>{0, 0, 0, 0, 0, 0, half, half}));
    Require(MixFrame(8, 6, 2) == (std::vector<float>{0.25f, 0}));
    Require(MixFrame(8, 7, 6) == (std::vector<float>{0, 0, 0, 0, 0, 0.5f}));
    Require(MixFrame(8, 7, 8) == (std::vector<float>{0, 0, 0, 0, 0, 0, 0, 0.5f}));

    const auto system = CreateSystem();
    const auto master = Mastering(system, 2);
    const auto submixer = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SUBMIXER));
    Control(submixer, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, 4, 0});
    Patch(submixer, master);
    Event(submixer, SCE_NGS2_VOICE_EVENT_PLAY);
    const std::vector<std::int16_t> pcm(Grain, 16384);
    const auto sampler = Sampler(system, pcm, 0);
    Patch(sampler, submixer);
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);
    std::vector<float> out(Grain * 2, -1.0f);
    const Ngs2RenderBufferInfo info{out.data(), out.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 2};
    bool unsupported = false;
    try { sceNgs2SystemRender(system, &info, 1); } catch (const std::runtime_error&) { unsupported = true; }
    Require(unsupported);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestUnsetMatrix() {
    const auto system = CreateSystem();
    const auto master = Mastering(system, 2);
    const auto submixer = Voice(CreateRack(system, SCE_NGS2_RACK_ID_SUBMIXER));
    Control(submixer, SCE_NGS2_SUBMIXER_VOICE_PARAM_SETUP, Ngs2SubmixerVoiceSetupParam{{}, 2, 0});
    Patch(submixer, master);
    Event(submixer, SCE_NGS2_VOICE_EVENT_PLAY);

    const std::vector<std::int16_t> pcm(Grain, 16384);
    const auto sampler = Sampler(system, pcm, 0);
    Patch(sampler, submixer);
    Control(sampler, SCE_NGS2_VOICE_PARAM_PORT_MATRIX, Ngs2VoicePortMatrixParam{{}, 0, 0});
    Control(sampler, SCE_NGS2_VOICE_PARAM_PORT_VOLUME, Ngs2VoicePortVolumeParam{{}, 0, 0.5f});
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);

    std::vector<float> out(Grain * 2, -1.0f);
    const Ngs2RenderBufferInfo info{out.data(), out.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 2};
    Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_OK);
    const float spread = 0.25f * HalfPower;
    for (std::uint32_t i = 0; i < Grain; i++) Require(out[i * 2] == spread && out[i * 2 + 1] == spread);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);

    const auto stereoSystem = CreateSystem();
    const auto stereoMaster = Mastering(stereoSystem, 2);
    const auto stereo = Voice(CreateRack(stereoSystem, SCE_NGS2_RACK_ID_SAMPLER));
    Control(stereo, SCE_NGS2_SAMPLER_VOICE_PARAM_SETUP, Ngs2SamplerVoiceSetupParam{{}, {SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 2, 48000, 0, 0, 0}});
    std::vector<std::int16_t> frames;
    for (std::uint32_t i = 0; i < Grain; i++) {
        frames.push_back(16384);
        frames.push_back(8192);
    }
    const Ngs2WaveformBlock block{0, frames.size() * sizeof(std::int16_t), 0, 0, Grain, 0, 0};
    Control(stereo, SCE_NGS2_SAMPLER_VOICE_PARAM_ADD_WAVEFORM_BLOCKS, Ngs2SamplerVoiceWaveformBlocksParam{{}, frames.data(), 0, 1, &block});
    Patch(stereo, stereoMaster);
    Control(stereo, SCE_NGS2_VOICE_PARAM_PORT_MATRIX, Ngs2VoicePortMatrixParam{{}, 0, 0});
    Event(stereo, SCE_NGS2_VOICE_EVENT_PLAY);
    Require(sceNgs2SystemRender(stereoSystem, &info, 1) == SCE_NGS2_OK);
    for (std::uint32_t i = 0; i < Grain; i++) Require(out[i * 2] == 0.5f && out[i * 2 + 1] == 0.25f);
    Require(sceNgs2SystemDestroy(stereoSystem, nullptr) == SCE_NGS2_OK);
}

template <typename TSample>
static void TestRenderCapacity(std::uint32_t waveformType, std::uint32_t channels) {
    const auto system = CreateSystem();
    const auto samples = Grain * channels;
    const TSample sentinel = 123;
    std::vector<TSample> out(samples * 3, sentinel);
    const Ngs2RenderBufferInfo info{out.data(), out.size() * sizeof(TSample), waveformType, channels};
    Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_OK);
    Require(std::all_of(out.begin(), out.begin() + samples, [](TSample value) { return value == 0; }));
    Require(std::all_of(out.begin() + samples, out.end(), [sentinel](TSample value) { return value == sentinel; }));

    const auto master = Mastering(system, channels);
    const std::vector<std::int16_t> pcm(Grain, 16384);
    const auto sampler = Sampler(system, pcm, 0);
    Patch(sampler, master);
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);
    std::fill(out.begin(), out.end(), sentinel);
    Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_OK);
    Require(std::all_of(out.begin(), out.begin() + samples, [sentinel](TSample value) { return value > 0 && value != sentinel; }));
    Require(std::all_of(out.begin() + samples, out.end(), [sentinel](TSample value) { return value == sentinel; }));

    Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_OK);
    Require(std::all_of(out.begin(), out.begin() + samples, [](TSample value) { return value == 0; }));
    Require(std::all_of(out.begin() + samples, out.end(), [sentinel](TSample value) { return value == sentinel; }));
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestRenderBufferCount() {
    const auto system = CreateSystem();
    std::vector<int16_t> out(Grain);
    Ngs2RenderBufferInfo infos[32];
    for (int i = 0; i < 32; i++) {
        infos[i] = Ngs2RenderBufferInfo{out.data(), out.size() * sizeof(int16_t), SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 1};
    }
    Require(sceNgs2SystemRender(system, infos, 31) == SCE_NGS2_OK);
    Require(sceNgs2SystemRender(system, infos, 32) == SCE_NGS2_ERROR_INVALID_NUM_RENDER_BUFFERS);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestRenderBufferTypeValidation() {
    const auto system = CreateSystem();
    std::vector<int16_t> out(Grain);
    Ngs2RenderBufferInfo info0{out.data(), out.size() * sizeof(int16_t), 0x50, 1};
    Require(sceNgs2SystemRender(system, &info0, 1) == SCE_NGS2_ERROR_INVALID_WAVEFORM_TYPE);
    Ngs2RenderBufferInfo valid{out.data(), out.size() * sizeof(int16_t), SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 1};
    Ngs2RenderBufferInfo infos[2];
    infos[0] = valid;
    infos[1] = Ngs2RenderBufferInfo{out.data(), out.size() * sizeof(int16_t), 0x50, 1};
    Require(sceNgs2SystemRender(system, infos, 2) == SCE_NGS2_ERROR_INVALID_WAVEFORM_TYPE);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestRenderBufferTypeBeforeNull() {
    const auto system = CreateSystem();
    std::vector<int16_t> out(Grain);
    Ngs2RenderBufferInfo info{nullptr, out.size() * sizeof(int16_t), 0x50, 1};
    Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_ERROR_INVALID_WAVEFORM_TYPE);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestRenderBufferNull() {
    const auto system = CreateSystem();
    std::vector<int16_t> out(Grain);
    Ngs2RenderBufferInfo info{nullptr, out.size() * sizeof(int16_t), SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 1};
    Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_ERROR_INVALID_RENDER_BUFFER);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestRenderRefusedDoesNotRender() {
    const auto system = CreateSystem();
    const auto master = Mastering(system, 1);
    std::vector<std::int16_t> pcm(Grain, 16384);
    const auto sampler = Sampler(system, pcm, 0);
    Patch(sampler, master);
    Event(sampler, SCE_NGS2_VOICE_EVENT_PLAY);
    std::vector<int16_t> out(Grain * 2, 123);
    Ngs2RenderBufferInfo infos[2];
    infos[0] = Ngs2RenderBufferInfo{out.data(), out.size() * sizeof(int16_t), SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 1};
    infos[1] = Ngs2RenderBufferInfo{out.data() + Grain, out.size() * sizeof(int16_t), 0x50, 1};
    Require(sceNgs2SystemRender(system, infos, 2) == SCE_NGS2_ERROR_INVALID_WAVEFORM_TYPE);
    Require(std::all_of(out.begin(), out.end(), [](int16_t value) { return value == 123; }));
    Ngs2SamplerVoiceState state{};
    Require(sceNgs2VoiceGetState(sampler, &state.voice_state, sizeof(state)) == SCE_NGS2_OK);
    Require(state.num_decoded_samples == 0);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestRenderBigEndianThrows() {
    const auto system = CreateSystem();
    std::vector<int16_t> out(Grain);
    Ngs2RenderBufferInfo infoI16B{out.data(), out.size() * sizeof(int16_t), SCE_NGS2_WAVEFORM_TYPE_PCM_I16B, 1};
    bool i16bThrows = false;
    try { sceNgs2SystemRender(system, &infoI16B, 1); } catch (const std::runtime_error&) { i16bThrows = true; }
    Require(i16bThrows);
    std::vector<float> outF(Grain);
    Ngs2RenderBufferInfo infoF32B{outF.data(), outF.size() * sizeof(float), SCE_NGS2_WAVEFORM_TYPE_PCM_F32B, 1};
    bool f32bThrows = false;
    try { sceNgs2SystemRender(system, &infoF32B, 1); } catch (const std::runtime_error&) { f32bThrows = true; }
    Require(f32bThrows);
    Require(sceNgs2SystemDestroy(system, nullptr) == SCE_NGS2_OK);
}

static void TestSilentRenderValidation() {
    const auto system = CreateSystem();
    std::vector<float> out(Grain, 123.0f);
    Ngs2RenderBufferInfo info{out.data(), out.size() * sizeof(float) - 1, SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 1};
    const auto invalid = [&] {
        bool rejected = false;
        try { sceNgs2SystemRender(system, &info, 1); } catch (const std::invalid_argument&) { rejected = true; }
        Require(rejected);
        Require(std::all_of(out.begin(), out.end(), [](float value) { return value == 123.0f; }));
    };
    invalid();
    info.buffer_size++;
    info.num_channels = 0;
    invalid();
    info.num_channels = 9;
    invalid();
    info.num_channels = 1;
    info.waveform_type = UINT32_MAX;
    Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_ERROR_INVALID_WAVEFORM_TYPE);
    Require(std::all_of(out.begin(), out.end(), [](float value) { return value == 123.0f; }));
    info.waveform_type = SCE_NGS2_WAVEFORM_TYPE_PCM_F32L;
    info.buffer = nullptr;
    Require(sceNgs2SystemRender(system, &info, 1) == SCE_NGS2_ERROR_INVALID_RENDER_BUFFER);
    Require(std::all_of(out.begin(), out.end(), [](float value) { return value == 123.0f; }));
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
    TestPatchToRackHandle();
    TestPan();
    TestPanExplicitAnglesAndNanDistance();
    TestPitchAndRepeat();
    TestSubmixerMatrix();
    TestReverb();
    TestReverbSetup();
    TestReverbConversion();
    TestSampleRate();

    TestMasteringGainClampedLikePs5();

    TestSystemOptionFlags();
    TestUserData();
    TestPcmCallbackReset();
    TestPcmCallbackRefill();
    TestPcmCallbackSetup();
    TestSetupClearsCallback();
    TestMasteringGain();
    TestMatrixLevelClamp();
    TestStereoIntoSurround();
    TestLock();
    TestRackLockLeavesQueries();
    TestVoiceCallbackLeavesQueries();
    TestRackLockWaitsForRender();
    TestCallbacksLockEachOthersSystem();
    TestRackLocksAcrossSystemsAndRender();
    TestRackDestroyWaitsForLock();
    TestDestroyDuringOwnRenderThrows();
    TestDefaultChannelMap();
    TestUnsetMatrix();
    TestRenderCapacity<float>(SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 1);
    TestRenderCapacity<float>(SCE_NGS2_WAVEFORM_TYPE_PCM_F32L, 2);
    TestRenderCapacity<std::int16_t>(SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 1);
    TestRenderCapacity<std::int16_t>(SCE_NGS2_WAVEFORM_TYPE_PCM_I16L, 2);
    TestRenderBufferCount();
    TestRenderBufferTypeValidation();
    TestRenderBufferTypeBeforeNull();
    TestRenderBufferNull();
    TestRenderRefusedDoesNotRender();
    TestRenderBigEndianThrows();
    TestSilentRenderValidation();
    TestAllocator();
    exitSystem = CreateSystem();
    return 0;
}
