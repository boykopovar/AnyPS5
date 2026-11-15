#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

static_assert(sizeof(AudioOutPortState) == 24);
static_assert(offsetof(AudioOutPortState, output) == 0);
static_assert(offsetof(AudioOutPortState, channel) == 2);
static_assert(offsetof(AudioOutPortState, reserved) == 3);
static_assert(offsetof(AudioOutPortState, volume) == 4);
static_assert(offsetof(AudioOutPortState, rerouteCounter) == 6);
static_assert(offsetof(AudioOutPortState, flag) == 8);
static_assert(offsetof(AudioOutPortState, activeState) == 16);

static_assert(sizeof(AudioOut2PortState) == 64);
static_assert(offsetof(AudioOut2PortState, output) == 0);
static_assert(offsetof(AudioOut2PortState, num_channels) == 2);
static_assert(offsetof(AudioOut2PortState, pad1) == 3);
static_assert(offsetof(AudioOut2PortState, volume) == 4);
static_assert(offsetof(AudioOut2PortState, reroute_counter) == 6);
static_assert(offsetof(AudioOut2PortState, flags) == 8);
static_assert(offsetof(AudioOut2PortState, pad2) == 12);
static_assert(offsetof(AudioOut2PortState, reserved) == 16);

static_assert(sizeof(AudioOutSystemState) == 32);
static_assert(offsetof(AudioOutSystemState, loudness) == 0);
static_assert(offsetof(AudioOutSystemState, reserved8) == 4);
static_assert(offsetof(AudioOutSystemState, reserved) == 8);

static constexpr int AUDIO_OUT_ERROR_INVALID_HANDLE = static_cast<int>(0x80260003u);
static constexpr int AUDIO_OUT_ERROR_INVALID_POINTER = static_cast<int>(0x80260004u);
static constexpr int AUDIO_OUT2_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80260502u);
static constexpr int AUDIO_OUT2_ERROR_INVALID_HANDLE = static_cast<int>(0x80260503u);

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

static bool ThrowsRuntimeError(auto&& action) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

int main() {
    for (int unopened : {-1, 0, 1, 32, 33, 9999, 0x7FFFFFFF}) {
        Require(sceAudioOutGetPortState(unopened, nullptr) == AUDIO_OUT_ERROR_INVALID_POINTER);
        AudioOutPortState canaryState{};
        std::memset(&canaryState, 0xEE, sizeof(canaryState));
        AudioOutPortState expectedCanary{};
        std::memset(&expectedCanary, 0xEE, sizeof(expectedCanary));
        Require(sceAudioOutGetPortState(unopened, &canaryState) == AUDIO_OUT_ERROR_INVALID_HANDLE);
        Require(std::memcmp(&canaryState, &expectedCanary, sizeof(canaryState)) == 0);
    }

    Require(sceAudioOut2PortGetState(0, nullptr) == AUDIO_OUT2_ERROR_INVALID_HANDLE);
    Require(sceAudioOut2LoPortGetState(0, nullptr) == AUDIO_OUT2_ERROR_INVALID_HANDLE);
    AudioOut2PortState canaryState2{};
    std::memset(&canaryState2, 0xDD, sizeof(canaryState2));
    AudioOut2PortState expectedCanary2{};
    std::memset(&expectedCanary2, 0xDD, sizeof(expectedCanary2));
    Require(sceAudioOut2PortGetState(0, &canaryState2) == AUDIO_OUT2_ERROR_INVALID_HANDLE);
    Require(std::memcmp(&canaryState2, &expectedCanary2, sizeof(canaryState2)) == 0);
    Require(sceAudioOut2PortGetState(0x7FFF, &canaryState2) == AUDIO_OUT2_ERROR_INVALID_HANDLE);
    Require(std::memcmp(&canaryState2, &expectedCanary2, sizeof(canaryState2)) == 0);
    Require(sceAudioOut2LoPortGetState(0, &canaryState2) == AUDIO_OUT2_ERROR_INVALID_HANDLE);
    Require(std::memcmp(&canaryState2, &expectedCanary2, sizeof(canaryState2)) == 0);
    Require(sceAudioOut2LoPortGetState(0x7FFF, &canaryState2) == AUDIO_OUT2_ERROR_INVALID_HANDLE);
    Require(std::memcmp(&canaryState2, &expectedCanary2, sizeof(canaryState2)) == 0);

    Require(sceAudioOutGetSystemState(nullptr) == AUDIO_OUT_ERROR_INVALID_POINTER);
    AudioOutSystemState sysState{};
    std::memset(&sysState, 0xCC, sizeof(sysState));
    Require(sceAudioOutGetSystemState(&sysState) == 0);
    Require(sysState.loudness == 0.0f);
    for (auto b : sysState.reserved8) Require(b == 0);
    for (auto r : sysState.reserved) Require(r == 0);

    const int initResult = sceAudioOutInit();
    Require(initResult == 0 || initResult == static_cast<int>(0x8026000eu));

    int handleMain = sceAudioOutOpen(255, 0, 0, 256, 48000, 1);
    Require(handleMain > 0);
    AudioOutPortState stateMain{};
    Require(sceAudioOutGetPortState(handleMain, nullptr) == AUDIO_OUT_ERROR_INVALID_POINTER);
    Require(sceAudioOutGetPortState(handleMain, &stateMain) == 0);
    Require(stateMain.output == 1);
    Require(stateMain.channel == 2);
    Require(stateMain.volume == 127);
    Require(stateMain.rerouteCounter == 0);
    Require(stateMain.flag == 0);
    Require(stateMain.activeState == 0);
    Require(stateMain.reserved[0] == 0);

    int handleVoice = sceAudioOutOpen(255, 2, 0, 256, 48000, 0);
    Require(handleVoice > 0);
    AudioOutPortState stateVoice{};
    Require(sceAudioOutGetPortState(handleVoice, &stateVoice) == 0);
    Require(stateVoice.output == 0x40);
    Require(stateVoice.channel == 1);
    Require(stateVoice.volume == 127);

    int handlePad = sceAudioOutOpen(255, 4, 0, 256, 48000, 0);
    Require(handlePad > 0);
    AudioOutPortState statePad{};
    Require(sceAudioOutGetPortState(handlePad, &statePad) == 0);
    Require(statePad.output == 4);
    Require(statePad.channel == 1);
    Require(statePad.volume == 127);

    int handleAux = sceAudioOutOpen(255, 127, 0, 256, 48000, 0);
    Require(handleAux > 0);
    AudioOutPortState stateAux{};
    Require(sceAudioOutGetPortState(handleAux, &stateAux) == 0);
    Require(stateAux.output == 0x80);
    Require(stateAux.channel == 0);
    Require(stateAux.volume == 127);

    int handleBgm = sceAudioOutOpen(255, 1, 0, 256, 48000, 1);
    Require(handleBgm > 0);
    AudioOutPortState stateBgm{};
    Require(sceAudioOutGetPortState(handleBgm, &stateBgm) == 0);
    Require(stateBgm.output == 1);
    Require(stateBgm.channel == 2);

    int handlePersonal = sceAudioOutOpen(255, 3, 0, 256, 48000, 0);
    Require(handlePersonal > 0);
    AudioOutPortState statePersonal{};
    Require(sceAudioOutGetPortState(handlePersonal, &statePersonal) == 0);
    Require(statePersonal.output == 0x40);
    Require(statePersonal.channel == 1);

    int handle3D = sceAudioOutOpen(255, 126, 0, 256, 48000, 1);
    Require(handle3D > 0);
    AudioOutPortState state3D{};
    Require(sceAudioOutGetPortState(handle3D, &state3D) == 0);
    Require(state3D.output == 1);
    Require(state3D.channel == 2);

    int handleEx = sceAudioOutExOpen(255, 0);
    if (handleEx > 0) {
        AudioOutPortState stateEx{};
        Require(ThrowsRuntimeError([=, &stateEx] { sceAudioOutGetPortState(handleEx, &stateEx); }));
        Require(sceAudioOutExClose(handleEx) == 0);
    }

    int vol[2] = {32768, 32768};
    Require(sceAudioOutSetVolume(handleMain, 3, vol) == 0);
    AudioOutPortState stateAfterVolume{};
    Require(sceAudioOutGetPortState(handleMain, &stateAfterVolume) == 0);
    Require(stateAfterVolume.volume == 127);
    Require(stateAfterVolume.rerouteCounter == 0);

    Require(sceAudioOutClose(handleMain) == 0);
    Require(sceAudioOutClose(handleVoice) == 0);
    Require(sceAudioOutClose(handlePad) == 0);
    Require(sceAudioOutClose(handleAux) == 0);
    Require(sceAudioOutClose(handleBgm) == 0);
    Require(sceAudioOutClose(handlePersonal) == 0);
    Require(sceAudioOutClose(handle3D) == 0);

    AudioOutPortState postCloseCanary{};
    std::memset(&postCloseCanary, 0xBB, sizeof(postCloseCanary));
    AudioOutPortState expectedPostClose{};
    std::memset(&expectedPostClose, 0xBB, sizeof(expectedPostClose));

    Require(sceAudioOutGetPortState(handleMain, &postCloseCanary) == AUDIO_OUT_ERROR_INVALID_HANDLE);
    Require(std::memcmp(&postCloseCanary, &expectedPostClose, sizeof(postCloseCanary)) == 0);
    Require(sceAudioOutGetPortState(handleVoice, &postCloseCanary) == AUDIO_OUT_ERROR_INVALID_HANDLE);
    Require(sceAudioOutGetPortState(handlePad, &postCloseCanary) == AUDIO_OUT_ERROR_INVALID_HANDLE);
    Require(sceAudioOutGetPortState(handleAux, &postCloseCanary) == AUDIO_OUT_ERROR_INVALID_HANDLE);
    Require(sceAudioOutGetPortState(handleBgm, &postCloseCanary) == AUDIO_OUT_ERROR_INVALID_HANDLE);
    Require(sceAudioOutGetPortState(handlePersonal, &postCloseCanary) == AUDIO_OUT_ERROR_INVALID_HANDLE);
    Require(sceAudioOutGetPortState(handle3D, &postCloseCanary) == AUDIO_OUT_ERROR_INVALID_HANDLE);

    Require(sceAudioOutClose(handleMain) == AUDIO_OUT_ERROR_INVALID_HANDLE);

    AudioOut2ContextParam ctxParam{};
    ctxParam.max_ports = 2;
    AudioOut2ContextHandle ctx = 0;
    Require(sceAudioOut2ContextCreate(&ctxParam, nullptr, 0, &ctx) == 0);
    Require(ctx > 0);

    AudioOut2PortParam portParam{};
    portParam.port_type = 0;
    portParam.data_format = (2u << 16) | 1u;
    portParam.sampling_freq = 48000;
    AudioOut2PortHandle port2 = 0;
    Require(sceAudioOut2PortCreate(ctx, &portParam, &port2) == 0);
    Require(port2 > 0);

    AudioOut2PortState pState2{};
    Require(sceAudioOut2PortGetState(port2, nullptr) == AUDIO_OUT2_ERROR_INVALID_ARGUMENT);
    Require(sceAudioOut2LoPortGetState(port2, nullptr) == AUDIO_OUT2_ERROR_INVALID_ARGUMENT);

    Require(sceAudioOut2PortGetState(port2, &pState2) == 0);
    Require(pState2.output == 1);
    Require(pState2.num_channels == 2);
    Require(pState2.volume == 0x7FFF);
    Require(pState2.reroute_counter == 0);
    Require(pState2.flags == 0);

    AudioOut2PortState loState2{};
    Require(sceAudioOut2LoPortGetState(port2, &loState2) == 0);
    Require(loState2.output == 1);
    Require(loState2.num_channels == 2);
    Require(loState2.volume == 0x7FFF);

    Require(sceAudioOut2PortDestroy(port2) == 0);
    Require(sceAudioOut2PortGetState(port2, &pState2) == AUDIO_OUT2_ERROR_INVALID_HANDLE);
    Require(sceAudioOut2LoPortGetState(port2, &loState2) == AUDIO_OUT2_ERROR_INVALID_HANDLE);

    Require(sceAudioOut2ContextDestroy(ctx) == 0);

    LibcRunShutdown_nid_postfix();
    return 0;
}
