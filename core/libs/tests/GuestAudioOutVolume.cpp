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

void TestAlternatingMaskPatterns() {
    Require(sceAudioOutInit() == 0 || sceAudioOutInit() == static_cast<int>(0x8026000e));

    const int eightPort = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 2);
    Require(eightPort > 0);

    std::array<int, 8> eightVol = {4000, 8000, 12000, 16000, 20000, 24000, 28000, 32000};
    Require(sceAudioOutSetVolume(eightPort, 0x55, eightVol.data()) == 0);
    Require(sceAudioOutSetVolume(eightPort, 0xAA, eightVol.data()) == 0);
    Require(sceAudioOutClose(eightPort) == 0);
}

void TestStandardFormatEightChannelPort() {
    const int stdPort = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 6);
    Require(stdPort > 0);

    std::array<int, 8> stdVol = {1000, 2000, 3000, 4000, 5000, 6000, 7000, 8000};
    Require(sceAudioOutSetVolume(stdPort, 0xFF, stdVol.data()) == 0);
    Require(sceAudioOutSetVolume(stdPort, 0x55, stdVol.data()) == 0);
    Require(sceAudioOutSetVolume(stdPort, 0xAA, stdVol.data()) == 0);
    Require(sceAudioOutClose(stdPort) == 0);
}

void TestPadSpeakerMixLevelBounds() {
    constexpr int invalidPort = static_cast<int>(0x80260003);
    constexpr int invalidPortType = static_cast<int>(0x8026000a);
    constexpr int invalidMixLevel = static_cast<int>(0x80260014);

    Require(sceAudioOutSetMixLevelPadSpk(-1, 16384) == invalidPort);

    const int mainPort = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1);
    Require(mainPort > 0);
    Require(sceAudioOutSetMixLevelPadSpk(mainPort, 16384) == invalidPortType);
    Require(sceAudioOutClose(mainPort) == 0);

    const int padPort = sceAudioOutOpen(0x10000000, 4, 0, 256, 48000, 0);
    Require(padPort > 0);

    Require(sceAudioOutSetMixLevelPadSpk(padPort, 0) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(padPort, 11626) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(padPort, 32768) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(padPort, 32769) == invalidMixLevel);
    Require(sceAudioOutSetMixLevelPadSpk(padPort, static_cast<std::uint32_t>(-1)) == invalidMixLevel);

    std::array<int, 8> padVol = {32768, 0, 0, 0, 0, 0, 0, 0};
    Require(sceAudioOutSetVolume(padPort, 1, padVol.data()) == 0);
    Require(sceAudioOutClose(padPort) == 0);
}

} // namespace

int main() {
    TestAlternatingMaskPatterns();
    TestStandardFormatEightChannelPort();
    TestPadSpeakerMixLevelBounds();

    LibcRunShutdown_nid_postfix();
    return 0;
}
