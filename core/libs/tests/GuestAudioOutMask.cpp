#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include <cstdint>
#include <cstdio>
#include <stdexcept>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void TestPortChannelMaskAndAttributes() {
    constexpr int errorInvalidHandle = static_cast<int>(0x80260503);
    constexpr int errorInvalidArgument = static_cast<int>(0x80268001);
    constexpr int errorUnsupportedFormat = static_cast<int>(0x8026800E);
    constexpr int errorFormatFlagWithout8Channels = static_cast<int>(0x80268001);

    AudioOut2ContextParam contextParams{};
    Require(sceAudioOut2ContextResetParam(&contextParams) == 0, "reset param failed");
    AudioOut2ContextHandle context = 0;
    Require(sceAudioOut2ContextCreate(&contextParams, nullptr, 0, &context) == 0 && context != 0, "context create failed");

    AudioOut2PortParam invalidChannelParam{};
    invalidChannelParam.port_type = 0;
    invalidChannelParam.data_format = 6u << 8;
    invalidChannelParam.sampling_freq = 48000;
    AudioOut2PortHandle invalidPort = 0;
    Require(sceAudioOut2PortCreate(context, &invalidChannelParam, &invalidPort) == errorUnsupportedFormat, "6 channels rejected");

    invalidChannelParam.data_format = 3u << 8;
    Require(sceAudioOut2PortCreate(context, &invalidChannelParam, &invalidPort) == errorUnsupportedFormat, "3 channels rejected");

    invalidChannelParam.data_format = (2u << 8) | 0x80u;
    Require(sceAudioOut2PortCreate(context, &invalidChannelParam, &invalidPort) == errorFormatFlagWithout8Channels, "invalid format flag without 8 channels rejected");

    AudioOut2PortParam portParams{};
    portParams.port_type = 0;
    portParams.data_format = 2u << 8;
    portParams.sampling_freq = 48000;
    AudioOut2PortHandle port = 0;
    Require(sceAudioOut2PortCreate(context, &portParams, &port) == 0 && port != 0, "port create failed");

    AudioOut2Attribute attribute{999, 0, nullptr, 0};
    Require(sceAudioOut2PortSetAttributes(0, &attribute, 1) == errorInvalidHandle, "invalid handle zero rejected");
    Require(sceAudioOut2PortSetAttributes(-1, &attribute, 1) == errorInvalidHandle, "invalid handle negative rejected");
    Require(sceAudioOut2PortSetAttributes(port, nullptr, 1) == errorInvalidArgument, "null attributes rejected");
    Require(sceAudioOut2PortSetAttributes(port, &attribute, 1) == 0, "unknown attribute ignored");

    Require(sceAudioOut2LoPortSetAttributes(0, &attribute, 1) == errorInvalidHandle, "lo invalid handle zero rejected");
    Require(sceAudioOut2LoPortSetAttributes(-1, &attribute, 1) == errorInvalidHandle, "lo invalid handle negative rejected");
    Require(sceAudioOut2LoPortSetAttributes(port, nullptr, 1) == errorInvalidArgument, "lo null attributes rejected");
    Require(sceAudioOut2LoPortSetAttributes(port, &attribute, 1) == 0, "lo unknown attribute ignored");

    int volumes[8] = {32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768};
    constexpr int errorInvalidVolumeHandle = -2144993277;
    constexpr int errorInvalidVolumePointer = -2144993276;
    Require(sceAudioOutSetVolume(-1, 0x1u, volumes) == errorInvalidVolumeHandle, "negative handle rejected");
    Require(sceAudioOutSetVolume(0, 0x1u, volumes) == errorInvalidVolumeHandle, "zero handle rejected");
    Require(sceAudioOutSetVolume(999, 0x1u, volumes) == errorInvalidVolumeHandle, "out of bounds handle rejected");
    Require(sceAudioOutSetVolume(1, 0x1u, nullptr) == errorInvalidVolumePointer, "null volume pointer rejected");

    Require(sceAudioOut2PortDestroy(port) == 0, "port destroy failed");
    Require(sceAudioOut2ContextDestroy(context) == 0, "context destroy failed");
}
}

int InitializeSdlAudio_nid_no_patch() { return 0; }
bool WasapiDefaultOutputAbsent_nid_no_patch() { return false; }
extern "C" unsigned int SDL_WasInit(unsigned int) { return 0x00000010u; }
extern "C" int SDL_GetNumAudioDevices(int) { return 1; }
extern "C" const char* SDL_GetAudioDeviceName(int, int) { return "test"; }
extern "C" int SDL_GetAudioDeviceSpec(int, int, void*) { return 0; }
extern "C" const char* SDL_GetCurrentAudioDriver() { return "dummy"; }
extern "C" const char* SDL_GetError() { return "error"; }
extern "C" unsigned int SDL_OpenAudioDevice(const char*, int, const void*, void*, int) { return 1; }
extern "C" void SDL_CloseAudioDevice(unsigned int) {}
extern "C" void SDL_QueueAudio(unsigned int, const void*, unsigned int) {}
extern "C" void SDL_ClearQueuedAudio(unsigned int) {}
extern "C" unsigned int SDL_GetQueuedAudioSize(unsigned int) { return 0; }
extern "C" void SDL_PauseAudioDevice(unsigned int, int) {}

int main() try {
    TestPortChannelMaskAndAttributes();
    std::puts("guest audio out mask tests passed");
    return 0;
} catch (const std::exception& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
}
