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

void TestLifecycle() {
    Require(sceAudioOutInit() == 0 || sceAudioOutInit() == static_cast<int>(0x8026000e));
    const int port = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1);
    Require(port > 0);
    Require(sceAudioOutClose(port) == 0);
}

void TestInvalidHandles() {
    constexpr int invalidHandle = static_cast<int>(0x80260001);
    constexpr int closedPort = 1;
    Require(sceAudioOutClose(-1) == static_cast<int>(0x80260003) || sceAudioOutClose(-1) == invalidHandle);
    Require(sceAudioOutClose(0) == static_cast<int>(0x80260003) || sceAudioOutClose(0) == invalidHandle);
    Require(sceAudioOutClose(999) == static_cast<int>(0x80260003) || sceAudioOutClose(999) == invalidHandle);
    Require(sceAudioOutClose(closedPort) == static_cast<int>(0x80260003) || sceAudioOutClose(closedPort) == invalidHandle);

    std::array<int, 8> volume = {32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768};
    Require(sceAudioOutSetVolume(-1, 1, volume.data()) == static_cast<int>(0x80260003) || sceAudioOutSetVolume(-1, 1, volume.data()) == invalidHandle);
    Require(sceAudioOutSetVolume(0, 1, volume.data()) == static_cast<int>(0x80260003) || sceAudioOutSetVolume(0, 1, volume.data()) == invalidHandle);
    Require(sceAudioOutSetVolume(999, 1, volume.data()) == static_cast<int>(0x80260003) || sceAudioOutSetVolume(999, 1, volume.data()) == invalidHandle);

    std::uint64_t outputTime = 0;
    Require(sceAudioOutGetLastOutputTime(-1, &outputTime) == static_cast<int>(0x80260003) || sceAudioOutGetLastOutputTime(-1, &outputTime) == invalidHandle);
    Require(sceAudioOutGetLastOutputTime(0, &outputTime) == static_cast<int>(0x80260003) || sceAudioOutGetLastOutputTime(0, &outputTime) == invalidHandle);
    Require(sceAudioOutGetLastOutputTime(999, &outputTime) == static_cast<int>(0x80260003) || sceAudioOutGetLastOutputTime(999, &outputTime) == invalidHandle);

    AudioOutPortState state{};
    Require(sceAudioOutGetPortState(-1, &state) == static_cast<int>(0x80260003) || sceAudioOutGetPortState(-1, &state) == invalidHandle);
    Require(sceAudioOutGetPortState(0, &state) == static_cast<int>(0x80260003) || sceAudioOutGetPortState(0, &state) == invalidHandle);
    Require(sceAudioOutGetPortState(999, &state) == static_cast<int>(0x80260003) || sceAudioOutGetPortState(999, &state) == invalidHandle);

    std::array<std::int16_t, 512> samples{};
    Require(sceAudioOutOutput(-1, samples.data()) == static_cast<int>(0x80260003) || sceAudioOutOutput(-1, samples.data()) == invalidHandle);
    Require(sceAudioOutOutput(0, samples.data()) == static_cast<int>(0x80260003) || sceAudioOutOutput(0, samples.data()) == invalidHandle);
    Require(sceAudioOutOutput(999, samples.data()) == static_cast<int>(0x80260003) || sceAudioOutOutput(999, samples.data()) == invalidHandle);
}

void TestInvalidPointers() {
    const int port = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1);
    Require(port > 0);

    constexpr int invalidPointer = static_cast<int>(0x80260004);
    constexpr int errorInvalidParam = static_cast<int>(0x80260005);
    Require(sceAudioOutSetVolume(port, 1, nullptr) == invalidPointer || sceAudioOutSetVolume(port, 1, nullptr) == errorInvalidParam);
    Require(sceAudioOutGetLastOutputTime(port, nullptr) == invalidPointer || sceAudioOutGetLastOutputTime(port, nullptr) == errorInvalidParam);
    Require(sceAudioOutGetPortState(port, nullptr) == invalidPointer || sceAudioOutGetPortState(port, nullptr) == errorInvalidParam);
    Require(sceAudioOutOutputs(nullptr, 1) == invalidPointer || sceAudioOutOutputs(nullptr, 1) == errorInvalidParam);

    Require(sceAudioOutClose(port) == 0);
}

void TestPortLimits() {
    std::array<int, 32> ports{};
    for (std::size_t i = 0; i < ports.size(); ++i) {
        ports[i] = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1);
        Require(ports[i] > 0);
    }

    const int excessPort = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1);
    Require(excessPort == static_cast<int>(0x80260005) || excessPort == static_cast<int>(0x80260007) || excessPort < 0);

    Require(sceAudioOutClose(ports[0]) == 0);
    const int recycledPort = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1);
    Require(recycledPort > 0);
    Require(sceAudioOutClose(recycledPort) == 0);

    for (std::size_t i = 1; i < ports.size(); ++i) {
        Require(sceAudioOutClose(ports[i]) == 0);
    }
}

void TestSampleFrequencies() {
    const int zeroFreq = sceAudioOutOpen(0x10000000, 0, 0, 256, 0, 1);
    Require(zeroFreq == static_cast<int>(0x80260008) || zeroFreq == static_cast<int>(0x80260001) || zeroFreq < 0);

    const int validFreq = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 1);
    Require(validFreq > 0);
    Require(sceAudioOutClose(validFreq) == 0);

    const int zeroLen = sceAudioOutOpen(0x10000000, 0, 0, 0, 48000, 1);
    Require(zeroLen == static_cast<int>(0x80260006) || zeroLen == static_cast<int>(0x80260003) || zeroLen < 0);

    const int invalidType = sceAudioOutOpen(0x10000000, 99, 0, 256, 48000, 1);
    Require(invalidType == static_cast<int>(0x8026000A) || invalidType == static_cast<int>(0x80260002) || invalidType < 0);

    const int invalidFormat = sceAudioOutOpen(0x10000000, 0, 0, 256, 48000, 0xFF);
    Require(invalidFormat == static_cast<int>(0x80260007) || invalidFormat < 0);
}

} // namespace

int main() {
    TestLifecycle();
    TestInvalidHandles();
    TestInvalidPointers();
    TestPortLimits();
    TestSampleFrequencies();

    LibcRunShutdown_nid_postfix();
    return 0;
}
