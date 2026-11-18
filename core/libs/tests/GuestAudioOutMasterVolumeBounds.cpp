#include "prx/libc/include/Shutdown.hpp"
#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

constexpr int ErrorInvalidPort = static_cast<int>(0x80260003);
constexpr int ErrorInvalidPointer = static_cast<int>(0x80260004);
constexpr int ErrorInvalidPortType = static_cast<int>(0x8026000A);
constexpr int ErrorInvalidMixLevel = static_cast<int>(0x80260014);

constexpr int PortTypeMain = 0;
constexpr int PortTypeBgm = 1;
constexpr int PortTypeVoice = 2;
constexpr int PortTypePersonal = 3;
constexpr int PortTypePadSpk = 4;
constexpr int PortTypeVibration = 10;
constexpr int PortTypeAudio3D = 126;
constexpr int PortTypeAux = 127;

constexpr int DefaultVolume = 32768;

static_assert(sizeof(AudioOutPortState) == 24);
static_assert(offsetof(AudioOutPortState, volume) == 4);

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "Check failed at %s:%u\n", location.file_name(), location.line());
        std::abort();
    }
}

void TestVolumePointerAndHandleBounds() {
    Require(sceAudioOutInit() == 0 || sceAudioOutInit() == static_cast<int>(0x8026000e));

    std::array<int, 8> volumeBuffer = {DefaultVolume, DefaultVolume, DefaultVolume, DefaultVolume, DefaultVolume, DefaultVolume, DefaultVolume, DefaultVolume};

    for (const int unopenedHandle : {-100, -1, 0, 33, 100, 999, -2147483647 - 1, 0x7FFFFFFF}) {
        Require(sceAudioOutSetVolume(unopenedHandle, 0xFF, nullptr) == ErrorInvalidPointer);
        Require(sceAudioOutSetVolume(unopenedHandle, 0, nullptr) == ErrorInvalidPointer);
        Require(sceAudioOutSetVolume(unopenedHandle, 0xFF, volumeBuffer.data()) == ErrorInvalidPort);
        Require(sceAudioOutSetVolume(unopenedHandle, 0, volumeBuffer.data()) == ErrorInvalidPort);
    }

    for (int unopenedHandle = 1; unopenedHandle <= 32; ++unopenedHandle) {
        Require(sceAudioOutSetVolume(unopenedHandle, 0xFF, nullptr) == ErrorInvalidPointer);
        Require(sceAudioOutSetVolume(unopenedHandle, 0xFF, volumeBuffer.data()) == ErrorInvalidPort);
    }

    const int port = sceAudioOutOpen(0x10000000, PortTypeMain, 0, 256, 48000, 1);
    Require(port > 0);

    Require(sceAudioOutSetVolume(port, 0xFF, nullptr) == ErrorInvalidPointer);
    Require(sceAudioOutSetVolume(port, 0, nullptr) == ErrorInvalidPointer);
    Require(sceAudioOutSetVolume(port, 1, nullptr) == ErrorInvalidPointer);

    Require(sceAudioOutClose(port) == 0);
    Require(sceAudioOutClose(port) == ErrorInvalidPort);

    Require(sceAudioOutSetVolume(port, 0xFF, nullptr) == ErrorInvalidPointer);
    Require(sceAudioOutSetVolume(port, 0xFF, volumeBuffer.data()) == ErrorInvalidPort);
    Require(sceAudioOutSetVolume(port, 0, volumeBuffer.data()) == ErrorInvalidPort);
}

void TestChannelBitmaskAndSelectiveVolume() {
    const int monoPort = sceAudioOutOpen(0x10000000, PortTypeMain, 0, 256, 48000, 0);
    Require(monoPort > 0);

    std::array<int, 8> vol = {16384, 8192, 4096, 2048, 1024, 512, 256, 128};
    Require(sceAudioOutSetVolume(monoPort, 0, vol.data()) == 0);
    Require(sceAudioOutSetVolume(monoPort, 1, vol.data()) == 0);
    Require(sceAudioOutSetVolume(monoPort, 2, vol.data()) == 0);
    Require(sceAudioOutSetVolume(monoPort, 0xFF, vol.data()) == 0);
    Require(sceAudioOutClose(monoPort) == 0);

    const int stereoPort = sceAudioOutOpen(0x10000000, PortTypeMain, 0, 256, 48000, 1);
    Require(stereoPort > 0);

    Require(sceAudioOutSetVolume(stereoPort, 0, vol.data()) == 0);
    Require(sceAudioOutSetVolume(stereoPort, 1, vol.data()) == 0);
    Require(sceAudioOutSetVolume(stereoPort, 2, vol.data()) == 0);
    Require(sceAudioOutSetVolume(stereoPort, 3, vol.data()) == 0);
    Require(sceAudioOutSetVolume(stereoPort, 0xFF, vol.data()) == 0);
    Require(sceAudioOutSetVolume(stereoPort, 0x80000000u, vol.data()) == 0);
    Require(sceAudioOutSetVolume(stereoPort, 0xFFFFFFFFu, vol.data()) == 0);
    Require(sceAudioOutClose(stereoPort) == 0);

    const int eightChPort = sceAudioOutOpen(0x10000000, PortTypeMain, 0, 256, 48000, 2);
    Require(eightChPort > 0);

    for (std::uint32_t bit = 0; bit < 8; ++bit) {
        Require(sceAudioOutSetVolume(eightChPort, 1u << bit, vol.data()) == 0);
    }
    Require(sceAudioOutSetVolume(eightChPort, 0xFF, vol.data()) == 0);
    Require(sceAudioOutClose(eightChPort) == 0);

    const int stdPort = sceAudioOutOpen(0x10000000, PortTypeMain, 0, 256, 48000, 6);
    Require(stdPort > 0);
    Require(sceAudioOutSetVolume(stdPort, 0xFF, vol.data()) == 0);
    Require(sceAudioOutClose(stdPort) == 0);

    const int stdF32Port = sceAudioOutOpen(0x10000000, PortTypeMain, 0, 256, 48000, 7);
    Require(stdF32Port > 0);
    Require(sceAudioOutSetVolume(stdF32Port, 0xFF, vol.data()) == 0);
    Require(sceAudioOutClose(stdF32Port) == 0);
}

void TestVolumeAmplitudeExtremesAndAudioScaling() {
    const int port = sceAudioOutOpen(0x10000000, PortTypeMain, 0, 256, 48000, 1);
    Require(port > 0);

    std::array<std::int16_t, 512> pcm16{};
    pcm16.fill(1000);

    std::array<int, 8> volMute = {0, 0, 0, 0, 0, 0, 0, 0};
    Require(sceAudioOutSetVolume(port, 3, volMute.data()) == 0);
    Require(sceAudioOutOutput(port, pcm16.data()) == 256);

    std::array<int, 8> volUnity = {DefaultVolume, DefaultVolume, 0, 0, 0, 0, 0, 0};
    Require(sceAudioOutSetVolume(port, 3, volUnity.data()) == 0);
    Require(sceAudioOutOutput(port, pcm16.data()) == 256);

    std::array<int, 8> volFractional = {1, 16384, 0, 0, 0, 0, 0, 0};
    Require(sceAudioOutSetVolume(port, 3, volFractional.data()) == 0);
    Require(sceAudioOutOutput(port, pcm16.data()) == 256);

    std::array<int, 8> volBoost = {65536, 131072, 0, 0, 0, 0, 0, 0};
    Require(sceAudioOutSetVolume(port, 3, volBoost.data()) == 0);
    Require(sceAudioOutOutput(port, pcm16.data()) == 256);

    std::array<int, 8> volMaxInt = {0x7FFFFFFF, 0x7FFFFFFF, 0, 0, 0, 0, 0, 0};
    Require(sceAudioOutSetVolume(port, 3, volMaxInt.data()) == 0);
    Require(sceAudioOutOutput(port, pcm16.data()) == 256);

    std::array<int, 8> volNegative = {-1, -100, -32768, -65536, 0, 0, 0, 0};
    Require(sceAudioOutSetVolume(port, 3, volNegative.data()) == 0);
    Require(sceAudioOutOutput(port, pcm16.data()) == 256);

    Require(sceAudioOutClose(port) == 0);

    const int floatPort = sceAudioOutOpen(0x10000000, PortTypeMain, 0, 256, 48000, 4);
    Require(floatPort > 0);

    std::array<float, 512> pcmFloat{};
    pcmFloat.fill(0.5f);

    Require(sceAudioOutSetVolume(floatPort, 3, volMute.data()) == 0);
    Require(sceAudioOutOutput(floatPort, pcmFloat.data()) == 256);

    Require(sceAudioOutSetVolume(floatPort, 3, volUnity.data()) == 0);
    Require(sceAudioOutOutput(floatPort, pcmFloat.data()) == 256);

    Require(sceAudioOutSetVolume(floatPort, 3, volBoost.data()) == 0);
    Require(sceAudioOutOutput(floatPort, pcmFloat.data()) == 256);

    Require(sceAudioOutSetVolume(floatPort, 3, volNegative.data()) == 0);
    Require(sceAudioOutOutput(floatPort, pcmFloat.data()) == 256);

    Require(sceAudioOutClose(floatPort) == 0);
}

void TestPortTypesVolumeSupport() {
    std::array<int, 8> vol = {DefaultVolume, DefaultVolume, DefaultVolume, DefaultVolume, DefaultVolume, DefaultVolume, DefaultVolume, DefaultVolume};

    for (const int portType : {PortTypeMain, PortTypeBgm, PortTypeVoice, PortTypePersonal, PortTypePadSpk, PortTypeVibration, PortTypeAudio3D, PortTypeAux}) {
        const int handle = sceAudioOutOpen(0x10000000, portType, 0, 256, 48000, 1);
        Require(handle > 0);

        Require(sceAudioOutSetVolume(handle, 0xFF, vol.data()) == 0);
        Require(sceAudioOutSetVolume(handle, 0, vol.data()) == 0);
        Require(sceAudioOutSetVolume(handle, 0xFF, nullptr) == ErrorInvalidPointer);

        Require(sceAudioOutClose(handle) == 0);
        Require(sceAudioOutSetVolume(handle, 0xFF, vol.data()) == ErrorInvalidPort);
    }
}

void TestPadSpeakerMixLevelBounds() {
    for (const int unopenedHandle : {-100, -1, 0, 33, 999}) {
        Require(sceAudioOutSetMixLevelPadSpk(unopenedHandle, 0) == ErrorInvalidPort);
        Require(sceAudioOutSetMixLevelPadSpk(unopenedHandle, 32768) == ErrorInvalidPort);
        Require(sceAudioOutSetMixLevelPadSpk(unopenedHandle, 65536) == ErrorInvalidPort);
    }

    const int mainPort = sceAudioOutOpen(0x10000000, PortTypeMain, 0, 256, 48000, 1);
    Require(mainPort > 0);
    Require(sceAudioOutSetMixLevelPadSpk(mainPort, 16384) == ErrorInvalidPortType);
    Require(sceAudioOutSetMixLevelPadSpk(mainPort, 32768) == ErrorInvalidPortType);
    Require(sceAudioOutSetMixLevelPadSpk(mainPort, 32769) == ErrorInvalidMixLevel);
    Require(sceAudioOutClose(mainPort) == 0);

    const int padPort = sceAudioOutOpen(0x10000000, PortTypePadSpk, 0, 256, 48000, 1);
    Require(padPort > 0);

    Require(sceAudioOutSetMixLevelPadSpk(padPort, 0) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(padPort, 1) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(padPort, 16384) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(padPort, 32767) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(padPort, 32768) == 0);

    Require(sceAudioOutSetMixLevelPadSpk(padPort, 32769) == ErrorInvalidMixLevel);
    Require(sceAudioOutSetMixLevelPadSpk(padPort, 40000) == ErrorInvalidMixLevel);
    Require(sceAudioOutSetMixLevelPadSpk(padPort, 65536) == ErrorInvalidMixLevel);
    Require(sceAudioOutSetMixLevelPadSpk(padPort, 0xFFFFFFFFu) == ErrorInvalidMixLevel);

    std::array<int, 8> vol = {16384, 16384, 0, 0, 0, 0, 0, 0};
    Require(sceAudioOutSetVolume(padPort, 3, vol.data()) == 0);

    std::array<std::int16_t, 512> pcm16{};
    pcm16.fill(2000);
    Require(sceAudioOutOutput(padPort, pcm16.data()) == 256);

    Require(sceAudioOutClose(padPort) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(padPort, 32768) == ErrorInvalidPort);
}

void TestPortStateVolumeBounds() {
    for (const int unopenedHandle : {-100, -1, 0, 33, 999}) {
        Require(sceAudioOutGetPortState(unopenedHandle, nullptr) == ErrorInvalidPointer);
        AudioOutPortState state{};
        Require(sceAudioOutGetPortState(unopenedHandle, &state) == ErrorInvalidPort);
    }

    const int port = sceAudioOutOpen(0x10000000, PortTypeMain, 0, 256, 48000, 1);
    Require(port > 0);

    Require(sceAudioOutGetPortState(port, nullptr) == ErrorInvalidPointer);

    AudioOutPortState state{};
    std::memset(&state, 0xEE, sizeof(state));
    Require(sceAudioOutGetPortState(port, &state) == 0);
    Require(state.volume == 127);
    Require(state.output == PortTypeMain);
    Require(state.channel == 2);
    Require(state.rerouteCounter == 0);
    Require(state.flag == 0);
    Require(state.activeState == 0);

    std::array<int, 8> newVol = {1000, 2000, 0, 0, 0, 0, 0, 0};
    Require(sceAudioOutSetVolume(port, 3, newVol.data()) == 0);

    Require(sceAudioOutGetPortState(port, &state) == 0);
    Require(state.volume == 127);

    Require(sceAudioOutClose(port) == 0);
    Require(sceAudioOutGetPortState(port, &state) == ErrorInvalidPort);
}

void TestMasteringAndAuxiliaryVolumeApis() {
    Require(sceAudioOutMasteringInit(0) == 0);
    Require(sceAudioOutMasteringTerm() == 0);
    Require(sceAudioOutSetMainOutput() == 0);
}

} // namespace

int main() {
    TestVolumePointerAndHandleBounds();
    TestChannelBitmaskAndSelectiveVolume();
    TestVolumeAmplitudeExtremesAndAudioScaling();
    TestPortTypesVolumeSupport();
    TestPadSpeakerMixLevelBounds();
    TestPortStateVolumeBounds();
    TestMasteringAndAuxiliaryVolumeApis();

    LibcRunShutdown_nid_postfix();
    return 0;
}
