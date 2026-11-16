#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <cstdint>
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

void TestInvalidPortHandles() {
    constexpr int invalidPort = -2144993277;
    constexpr int invalidPointer = -2144993276;

    Require(sceAudioOutClose(0) == invalidPort);
    Require(sceAudioOutClose(-1) == invalidPort);
    Require(sceAudioOutClose(33) == invalidPort);
    Require(sceAudioOutClose(-2147483647 - 1) == invalidPort);

    AudioOutPortState state{};
    Require(sceAudioOutGetPortState(0, &state) == invalidPort);
    Require(sceAudioOutGetPortState(-1, &state) == invalidPort);
    Require(sceAudioOutGetPortState(33, &state) == invalidPort);
    Require(sceAudioOutGetPortState(0, nullptr) == invalidPointer);

    std::uint64_t lastTime = 0;
    Require(sceAudioOutGetLastOutputTime(0, &lastTime) == invalidPort);
    Require(sceAudioOutGetLastOutputTime(-1, &lastTime) == invalidPort);
    Require(sceAudioOutGetLastOutputTime(33, &lastTime) == invalidPort);
    Require(sceAudioOutGetLastOutputTime(0, nullptr) == invalidPointer);

    int volumes[8] = {32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768};
    Require(sceAudioOutSetVolume(0, 0xFF, volumes) == invalidPort);
    Require(sceAudioOutSetVolume(-1, 0xFF, volumes) == invalidPort);
    Require(sceAudioOutSetVolume(33, 0xFF, volumes) == invalidPort);
    Require(sceAudioOutSetVolume(0, 0xFF, nullptr) == invalidPointer);
}

void TestPortParameterBounds() {
    constexpr int invalidType = -2144993270;
    constexpr int invalidSize = -2144993274;
    constexpr int invalidFreq = -2144993272;
    constexpr int invalidFormat = static_cast<int>(0x80260007);

    Require(sceAudioOutOpen(0x10000000, -1, 0, 256, 48000, 1) == invalidType);
    Require(sceAudioOutOpen(0x10000000, 5, 0, 256, 48000, 1) == invalidType);
    Require(sceAudioOutOpen(0x10000000, 8, 0, 256, 48000, 1) == invalidType);
    Require(sceAudioOutOpen(0x10000000, 255, 0, 256, 48000, 1) == invalidType);

    Require(sceAudioOutOpen(0x10000000, 0, 0, 0, 48000, 1) == invalidSize);
    Require(sceAudioOutOpen(0x10000000, 0, 0, 256, 0, 1) == invalidFreq);

    Require(sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 8) == invalidFormat);
    Require(sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 0xFF) == invalidFormat);
}

void TestPortLifecycleAndCapacity() {
    constexpr int maxPorts = 32;
    constexpr int portFull = -2144993275;
    constexpr int invalidPort = -2144993277;

    int handles[maxPorts]{};
    for (int i = 0; i < maxPorts; ++i) {
        handles[i] = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1);
        Require(handles[i] > 0);
        Require(handles[i] <= maxPorts);
    }

    const int overflow = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1);
    Require(overflow == portFull);

    const int freed = handles[0];
    Require(sceAudioOutClose(freed) == 0);
    Require(sceAudioOutClose(freed) == invalidPort);

    const int reopened = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1);
    Require(reopened == freed);
    handles[0] = reopened;

    for (int i = 0; i < maxPorts; ++i) {
        Require(sceAudioOutClose(handles[i]) == 0);
    }
}

void TestPadSpeakerMixLevelBounds() {
    constexpr int invalidParam = -2144993260;
    constexpr int invalidType = -2144993270;
    constexpr int invalidPort = -2144993277;

    Require(sceAudioOutSetMixLevelPadSpk(0, 32768) == invalidPort);

    const int padPort = sceAudioOutOpen(0x10000000, 4, 0, 256, 48000, 1);
    Require(padPort > 0);

    Require(sceAudioOutSetMixLevelPadSpk(padPort, 32769) == invalidParam);
    Require(sceAudioOutSetMixLevelPadSpk(padPort, 0) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(padPort, 11626) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(padPort, 32768) == 0);

    Require(sceAudioOutClose(padPort) == 0);

    const int mainPort = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1);
    Require(mainPort > 0);
    Require(sceAudioOutSetMixLevelPadSpk(mainPort, 11626) == invalidType);
    Require(sceAudioOutClose(mainPort) == 0);
}

} // namespace

int main() {
    Require(sceAudioOutInit() == 0 || sceAudioOutInit() == static_cast<int>(0x8026000e));

    TestInvalidPortHandles();
    TestPortParameterBounds();
    TestPortLifecycleAndCapacity();
    TestPadSpeakerMixLevelBounds();

    LibcRunShutdown_nid_postfix();
    return 0;
}
