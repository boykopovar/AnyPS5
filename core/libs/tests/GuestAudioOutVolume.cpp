#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

void TestPortHandleValidity() {
    Require(sceAudioOutInit() == 0 || sceAudioOutInit() == static_cast<int>(0x8026000e));

    constexpr int invalidPort = static_cast<int>(0x80260001);
    constexpr int internalInvalidPort = static_cast<int>(0x80260003);
    constexpr int invalidPortSpec = static_cast<int>(0x80260006);

    const std::array<int, 6> badHandles = {-100, -1, 0, 33, 64, 999};
    std::array<int, 8> volume = {32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768};

    for (int handle : badHandles) {
        const int res = sceAudioOutSetVolume(handle, 1, volume.data());
        Require(res == invalidPort || res == internalInvalidPort || res == invalidPortSpec || res < 0);
    }

    const int port = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1);
    Require(port > 0);
    Require(sceAudioOutClose(port) == 0);

    const int closedRes = sceAudioOutSetVolume(port, 1, volume.data());
    Require(closedRes == invalidPort || closedRes == internalInvalidPort || closedRes == invalidPortSpec || closedRes < 0);
}

void TestNullPointerValidation() {
    constexpr int invalidPointer = static_cast<int>(0x80260004);
    constexpr int internalInvalidPointer = static_cast<int>(0x80260005);

    const int port = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1);
    Require(port > 0);

    const int nullRes = sceAudioOutSetVolume(port, 1, nullptr);
    Require(nullRes == invalidPointer || nullRes == internalInvalidPointer || nullRes < 0);

    const int badPortNullRes = sceAudioOutSetVolume(-1, 1, nullptr);
    Require(badPortNullRes == invalidPointer || badPortNullRes == internalInvalidPointer || badPortNullRes < 0);

    Require(sceAudioOutClose(port) == 0);
}

void TestVolumeLevelsAndBounds() {
    const int port = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1);
    Require(port > 0);

    std::array<int, 8> volMute = {0, 0, 0, 0, 0, 0, 0, 0};
    Require(sceAudioOutSetVolume(port, 3, volMute.data()) == 0);

    std::array<int, 8> volHalf = {16384, 16384, 0, 0, 0, 0, 0, 0};
    Require(sceAudioOutSetVolume(port, 3, volHalf.data()) == 0);

    std::array<int, 8> volDefault = {32768, 32768, 0, 0, 0, 0, 0, 0};
    Require(sceAudioOutSetVolume(port, 3, volDefault.data()) == 0);

    std::array<int, 8> volThreeHalves = {49152, 49152, 0, 0, 0, 0, 0, 0};
    Require(sceAudioOutSetVolume(port, 3, volThreeHalves.data()) == 0);

    std::array<int, 8> volMax = {65536, 65536, 0, 0, 0, 0, 0, 0};
    Require(sceAudioOutSetVolume(port, 3, volMax.data()) == 0);

    std::array<int, 8> volOver = {65537, 100000, 0, 0, 0, 0, 0, 0};
    const int overRes = sceAudioOutSetVolume(port, 3, volOver.data());
    Require(overRes == 0 || overRes < 0);

    std::array<int, 8> volNegative = {-1, -32768, 0, 0, 0, 0, 0, 0};
    const int negRes = sceAudioOutSetVolume(port, 3, volNegative.data());
    Require(negRes == 0 || negRes < 0);

    Require(sceAudioOutClose(port) == 0);
}

void TestMultiChannelAssignment() {
    const int monoPort = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 0);
    Require(monoPort > 0);
    std::array<int, 8> monoVol = {20000, 0, 0, 0, 0, 0, 0, 0};
    Require(sceAudioOutSetVolume(monoPort, 0, monoVol.data()) == 0);
    Require(sceAudioOutSetVolume(monoPort, 1, monoVol.data()) == 0);
    Require(sceAudioOutClose(monoPort) == 0);

    const int stereoPort = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1);
    Require(stereoPort > 0);
    std::array<int, 8> stereoVol = {15000, 25000, 0, 0, 0, 0, 0, 0};
    Require(sceAudioOutSetVolume(stereoPort, 0, stereoVol.data()) == 0);
    Require(sceAudioOutSetVolume(stereoPort, 1, stereoVol.data()) == 0);
    Require(sceAudioOutSetVolume(stereoPort, 2, stereoVol.data()) == 0);
    Require(sceAudioOutSetVolume(stereoPort, 3, stereoVol.data()) == 0);
    Require(sceAudioOutClose(stereoPort) == 0);

    const int eightPort = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 2);
    Require(eightPort > 0);
    std::array<int, 8> eightVol = {4000, 8000, 12000, 16000, 20000, 24000, 28000, 32000};
    for (std::uint32_t ch = 0; ch < 8; ++ch) {
        Require(sceAudioOutSetVolume(eightPort, 1u << ch, eightVol.data()) == 0);
    }
    Require(sceAudioOutSetVolume(eightPort, 0x55, eightVol.data()) == 0);
    Require(sceAudioOutSetVolume(eightPort, 0xAA, eightVol.data()) == 0);
    Require(sceAudioOutSetVolume(eightPort, 0x0F, eightVol.data()) == 0);
    Require(sceAudioOutSetVolume(eightPort, 0xF0, eightVol.data()) == 0);
    Require(sceAudioOutSetVolume(eightPort, 0xFF, eightVol.data()) == 0);
    Require(sceAudioOutSetVolume(eightPort, 0xFFFF, eightVol.data()) == 0);
    Require(sceAudioOutClose(eightPort) == 0);

    const int eightStdPort = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 6);
    Require(eightStdPort > 0);
    Require(sceAudioOutSetVolume(eightStdPort, 0xFF, eightVol.data()) == 0);
    Require(sceAudioOutClose(eightStdPort) == 0);
}

void TestPortTypes() {
    const std::array<int, 5> portTypes = {0, 1, 2, 3, 4};
    std::array<int, 8> vol = {32768, 32768, 0, 0, 0, 0, 0, 0};

    for (int type : portTypes) {
        const int port = sceAudioOutOpen(0x10000000, type, 0, 256, 48000, 1);
        if (port > 0) {
            Require(sceAudioOutSetVolume(port, 3, vol.data()) == 0);
            Require(sceAudioOutClose(port) == 0);
        }
    }
}

void TestPadSpeakerVolume() {
    const int padPort = sceAudioOutOpen(0x10000000, 4, 0, 256, 48000, 0);
    if (padPort > 0) {
        Require(sceAudioOutSetMixLevelPadSpk(padPort, 0) == 0);
        Require(sceAudioOutSetMixLevelPadSpk(padPort, 11626) == 0);
        Require(sceAudioOutSetMixLevelPadSpk(padPort, 32768) == 0);

        constexpr int invalidMixLevel = static_cast<int>(0x80260014);
        const int overMix = sceAudioOutSetMixLevelPadSpk(padPort, 32769);
        Require(overMix == invalidMixLevel || overMix < 0);

        std::array<int, 8> padVol = {32768, 0, 0, 0, 0, 0, 0, 0};
        Require(sceAudioOutSetVolume(padPort, 1, padVol.data()) == 0);
        Require(sceAudioOutSetPadSpkVolume(padPort, 32768) == 0 || sceAudioOutSetPadSpkVolume(padPort, 32768) < 0);

        Require(sceAudioOutClose(padPort) == 0);
    }
}

} // namespace

int main() {
    TestPortHandleValidity();
    TestNullPointerValidation();
    TestVolumeLevelsAndBounds();
    TestMultiChannelAssignment();
    TestPortTypes();
    TestPadSpeakerVolume();

    LibcRunShutdown_nid_postfix();
    return 0;
}
