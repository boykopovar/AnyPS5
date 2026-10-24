#include "SceTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <random>
#include <string>

extern "C" {
int APS5_VABI sceAudioOut2ContextResetParam(AudioOut2ContextParam*);
int APS5_VABI sceAudioOut2ContextCreate(const AudioOut2ContextParam*, void*, std::size_t, AudioOut2ContextHandle*);
int APS5_VABI sceAudioOut2ContextDestroy(AudioOut2ContextHandle);
int APS5_VABI sceAudioOut2PortCreate(AudioOut2ContextHandle, const AudioOut2PortParam*, AudioOut2PortHandle*);
int APS5_VABI sceAudioOut2PortDestroy(AudioOut2PortHandle);
int APS5_VABI sceAudioOut2PortGetState(AudioOut2PortHandle, AudioOut2PortState*);
int APS5_VABI sceAudioOut2GetSpeakerInfo(AudioOut2SpeakerInfo*, std::uint32_t);
int APS5_VABI sceAudioOut2GetSystemState(AudioOut2SystemState*);
}

static void Require(bool value) { if (!value) std::abort(); }

namespace {

constexpr int errorInvalidArgument = static_cast<int>(0x80260502);
constexpr int errorInvalidHandle = static_cast<int>(0x80260503);
constexpr std::uint16_t portTypeMain = 0;
constexpr std::uint16_t outputMain = 1;
constexpr std::int16_t volumeMax = 127;
constexpr std::uint32_t formatFloat = 0;
constexpr std::uint32_t formatS16 = 1;
constexpr std::uint32_t frequency = 48000;

std::uint32_t Format(std::uint32_t channels, std::uint32_t type) {
    return channels << 8 | type;
}

void SetEnvironment(const char* name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

AudioOut2PortState StateOfPort(AudioOut2ContextHandle context, std::uint32_t format) {
    AudioOut2PortParam params{};
    params.port_type = portTypeMain;
    params.data_format = format;
    params.sampling_freq = frequency;
    AudioOut2PortHandle port = 0;
    Require(sceAudioOut2PortCreate(context, &params, &port) == 0);
    AudioOut2PortState state;
    std::memset(&state, 0xAB, sizeof(state));
    Require(sceAudioOut2PortGetState(port, &state) == 0);
    Require(sceAudioOut2PortDestroy(port) == 0);
    return state;
}

void PortStateFollowsTheDataFormat(AudioOut2ContextHandle context) {
    struct Case {
        std::uint32_t channels;
        std::uint32_t sampleType;
    };
    for (const Case entry : {Case{1, formatFloat}, Case{2, formatFloat}, Case{2, formatS16}, Case{8, formatFloat}, Case{12, formatS16}}) {
        const auto state = StateOfPort(context, Format(entry.channels, entry.sampleType));
        Require(state.output == outputMain);
        Require(state.num_channels == entry.channels);
        Require(state.volume == volumeMax);
        Require(state.reroute_counter == 0);
        Require(state.flags == 0);
        Require(state.pad1 == 0 && state.pad2 == 0);
        for (const auto word : state.reserved) Require(word == 0);
    }
}

void PortStateRejectsBadArguments(AudioOut2ContextHandle context) {
    AudioOut2PortState state{};
    Require(sceAudioOut2PortGetState(0, &state) == errorInvalidHandle);
    Require(sceAudioOut2PortGetState(0x7fff, &state) == errorInvalidHandle);

    AudioOut2PortParam params{};
    params.port_type = portTypeMain;
    params.data_format = Format(2, formatFloat);
    params.sampling_freq = frequency;
    AudioOut2PortHandle port = 0;
    Require(sceAudioOut2PortCreate(context, &params, &port) == 0);
    Require(sceAudioOut2PortGetState(port, nullptr) == errorInvalidArgument);
    Require(sceAudioOut2PortDestroy(port) == 0);
    Require(sceAudioOut2PortGetState(port, &state) == errorInvalidHandle);
}

void SpeakerInfoReportsTheFrontPair() {
    AudioOut2SpeakerInfo info;
    std::memset(&info, 0xAB, sizeof(info));
    Require(sceAudioOut2GetSpeakerInfo(&info, 0) == 0);
    Require(info.type == 0);
    Require(info.available_bits == 0x3);
    Require(info.flags == 0);
    Require(info.speaker_angle[0].azimuth == 30 && info.speaker_angle[0].elevation == 0);
    Require(info.speaker_angle[1].azimuth == -30 && info.speaker_angle[1].elevation == 0);
    for (int index = 2; index < 16; index++) Require(info.speaker_angle[index].azimuth == -360 && info.speaker_angle[index].elevation == -360);
    Require(sceAudioOut2GetSpeakerInfo(nullptr, 0) == errorInvalidArgument);
}

void SystemStateIsSilentAndRejectsNull() {
    AudioOut2SystemState state;
    std::memset(&state, 0xAB, sizeof(state));
    Require(sceAudioOut2GetSystemState(&state) == 0);
    Require(state.loudness == 0.0f);
    Require(sceAudioOut2GetSystemState(nullptr) == errorInvalidArgument);
}

}

int main() {
    const auto path = std::filesystem::temp_directory_path() / ("anyps5_audio_out2_device_state-" + std::to_string(std::random_device{}()) + ".raw");
    std::filesystem::remove(path);
    SetEnvironment("SDL_DISKAUDIOFILE", path.string());

    AudioOut2ContextParam params{};
    Require(sceAudioOut2ContextResetParam(&params) == 0);
    params.num_grains = 256;
    params.queue_depth = 1;
    AudioOut2ContextHandle context = 0;
    Require(sceAudioOut2ContextCreate(&params, nullptr, 0, &context) == 0);

    PortStateFollowsTheDataFormat(context);
    PortStateRejectsBadArguments(context);
    SpeakerInfoReportsTheFrontPair();
    SystemStateIsSilentAndRejectsNull();

    Require(sceAudioOut2ContextDestroy(context) == 0);
    std::filesystem::remove(path);
    return 0;
}
