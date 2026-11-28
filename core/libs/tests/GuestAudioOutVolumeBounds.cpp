#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace {

constexpr int audioOutErrorInvalidHandle = static_cast<int>(0x80260003);
constexpr int audioOutErrorInvalidPointer = static_cast<int>(0x80260004);
constexpr int audioOutErrorPortNotPadSpk = static_cast<int>(0x8026000A);
constexpr int audioOutErrorInvalidMixLevel = static_cast<int>(0x80260014);
constexpr int userId = 0x10000000;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestVolumeNullPointerPrecedenceAndHandleGates() {
    Require(sceAudioOutInit() == 0);

    for (const int badHandle : {-100, -1, 0, 33, 100, 999}) {
        Require(sceAudioOutSetVolume(badHandle, 0, nullptr) == audioOutErrorInvalidPointer);
        Require(sceAudioOutSetVolume(badHandle, 1, nullptr) == audioOutErrorInvalidPointer);
        Require(sceAudioOutSetVolume(badHandle, 0xFF, nullptr) == audioOutErrorInvalidPointer);
    }

    for (const int unopenedHandle : {1, 2, 8, 16, 32}) {
        Require(sceAudioOutSetVolume(unopenedHandle, 0, nullptr) == audioOutErrorInvalidPointer);
        Require(sceAudioOutSetVolume(unopenedHandle, 1, nullptr) == audioOutErrorInvalidPointer);
        Require(sceAudioOutSetVolume(unopenedHandle, 0xFF, nullptr) == audioOutErrorInvalidPointer);
    }

    int vol[8] = {32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768};

    for (const int badHandle : {-100, -1, 0, 33, 100, 999}) {
        Require(sceAudioOutSetVolume(badHandle, 0, vol) == audioOutErrorInvalidHandle);
        Require(sceAudioOutSetVolume(badHandle, 1, vol) == audioOutErrorInvalidHandle);
        Require(sceAudioOutSetVolume(badHandle, 0xFF, vol) == audioOutErrorInvalidHandle);
    }

    for (const int unopenedHandle : {1, 2, 8, 16, 32}) {
        Require(sceAudioOutSetVolume(unopenedHandle, 0, vol) == audioOutErrorInvalidHandle);
        Require(sceAudioOutSetVolume(unopenedHandle, 1, vol) == audioOutErrorInvalidHandle);
        Require(sceAudioOutSetVolume(unopenedHandle, 0xFF, vol) == audioOutErrorInvalidHandle);
    }

    Require(sceAudioOutGetPortState(0, nullptr) == audioOutErrorInvalidPointer);
    Require(sceAudioOutGetPortState(-1, nullptr) == audioOutErrorInvalidPointer);
    Require(sceAudioOutGetPortState(1, nullptr) == audioOutErrorInvalidPointer);

    AudioOutPortState state{};
    Require(sceAudioOutGetPortState(0, &state) == audioOutErrorInvalidHandle);
    Require(sceAudioOutGetPortState(-1, &state) == audioOutErrorInvalidHandle);
    Require(sceAudioOutGetPortState(1, &state) == audioOutErrorInvalidHandle);
}

void TestVolumeChannelBoundsAndRemapping() {
    const int portStd = sceAudioOutOpen(userId, 0, 0, 256, 48000, 6);
    Require(portStd > 0);

    Require(sceAudioOutSetVolume(portStd, 0, nullptr) == audioOutErrorInvalidPointer);
    Require(sceAudioOutSetVolume(portStd, 1, nullptr) == audioOutErrorInvalidPointer);
    Require(sceAudioOutSetVolume(portStd, 0xFF, nullptr) == audioOutErrorInvalidPointer);

    int testVolumes[8] = {0, 1000, 16384, 32768, 48000, 65535, 65536, 131072};
    Require(sceAudioOutSetVolume(portStd, 0, testVolumes) == 0);
    Require(sceAudioOutSetVolume(portStd, 1, testVolumes) == 0);
    Require(sceAudioOutSetVolume(portStd, 0x3, testVolumes) == 0);
    Require(sceAudioOutSetVolume(portStd, 0x0F, testVolumes) == 0);
    Require(sceAudioOutSetVolume(portStd, 0xFF, testVolumes) == 0);

    AudioOutPortState portState{};
    Require(sceAudioOutGetPortState(portStd, &portState) == 0);
    Require(portState.volume == 127);
    Require(portState.output == 1);
    Require(portState.channel == 2);

    Require(sceAudioOutClose(portStd) == 0);
    Require(sceAudioOutSetVolume(portStd, 0xFF, testVolumes) == audioOutErrorInvalidHandle);
    Require(sceAudioOutSetVolume(portStd, 0xFF, nullptr) == audioOutErrorInvalidPointer);
    Require(sceAudioOutGetPortState(portStd, &portState) == audioOutErrorInvalidHandle);

    const int portStereo = sceAudioOutOpen(userId, 0, 0, 256, 48000, 1);
    Require(portStereo > 0);
    int stereoVol[8] = {32768, 16384, 0, 0, 0, 0, 0, 0};
    Require(sceAudioOutSetVolume(portStereo, 0x3, stereoVol) == 0);
    Require(sceAudioOutClose(portStereo) == 0);

    const int portF32Std = sceAudioOutOpen(userId, 0, 0, 256, 48000, 7);
    Require(portF32Std > 0);
    Require(sceAudioOutSetVolume(portF32Std, 0xFF, testVolumes) == 0);
    Require(sceAudioOutClose(portF32Std) == 0);
}

void TestPadSpkMixLevelAndMainOutputBounds() {
    Require(sceAudioOutSetMainOutput() == 0);

    Require(sceAudioOutSetMixLevelPadSpk(-1, 1000) == audioOutErrorInvalidHandle);
    Require(sceAudioOutSetMixLevelPadSpk(0, 1000) == audioOutErrorInvalidHandle);
    Require(sceAudioOutSetMixLevelPadSpk(1, 1000) == audioOutErrorInvalidHandle);
    Require(sceAudioOutSetMixLevelPadSpk(33, 1000) == audioOutErrorInvalidHandle);

    const int mainPort = sceAudioOutOpen(userId, 0, 0, 256, 48000, 1);
    Require(mainPort > 0);
    Require(sceAudioOutSetMixLevelPadSpk(mainPort, 0) == audioOutErrorPortNotPadSpk);
    Require(sceAudioOutSetMixLevelPadSpk(mainPort, 11626) == audioOutErrorPortNotPadSpk);
    Require(sceAudioOutSetMixLevelPadSpk(mainPort, 32768) == audioOutErrorPortNotPadSpk);
    Require(sceAudioOutClose(mainPort) == 0);

    const int padPort = sceAudioOutOpen(userId, 4, 0, 256, 48000, 1);
    Require(padPort > 0);

    for (const std::uint32_t validLevel : {0u, 100u, 1000u, 11626u, 20000u, 32768u}) {
        Require(sceAudioOutSetMixLevelPadSpk(padPort, validLevel) == 0);
    }

    for (const std::uint32_t invalidLevel : {32769u, 40000u, 65536u, 100000u, 0xFFFFFFFFu}) {
        Require(sceAudioOutSetMixLevelPadSpk(padPort, invalidLevel) == audioOutErrorInvalidMixLevel);
    }

    Require(sceAudioOutClose(padPort) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(padPort, 11626) == audioOutErrorInvalidHandle);
}

} // namespace

int main() {
    TestVolumeNullPointerPrecedenceAndHandleGates();
    TestVolumeChannelBoundsAndRemapping();
    TestPadSpkMixLevelAndMainOutputBounds();

    LibcRunShutdown_nid_postfix();
    return 0;
}
