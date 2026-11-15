#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace {

constexpr int audioOutErrorInvalidPort = static_cast<int>(0x80260003);
constexpr int audioOutErrorPortNotPadSpk = static_cast<int>(0x8026000A);
constexpr int audioOutErrorInvalidMixLevel = static_cast<int>(0x80260014);
constexpr int userId = 0x10000000;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename TAction>
bool Throws(TAction&& action) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void TestUnopenedAndInvalidHandleGates() {
    Require(sceAudioOutInit() == 0);

    for (const int badHandle : {-1000, -100, -2, -1, 0, 33, 34, 50, 100, 1024, 0x10000, 0x7FFFFFFF}) {
        Require(sceAudioOutSetMixLevelPadSpk(badHandle, 0u) == audioOutErrorInvalidPort);
        Require(sceAudioOutSetMixLevelPadSpk(badHandle, 1u) == audioOutErrorInvalidPort);
        Require(sceAudioOutSetMixLevelPadSpk(badHandle, 11626u) == audioOutErrorInvalidPort);
        Require(sceAudioOutSetMixLevelPadSpk(badHandle, 32768u) == audioOutErrorInvalidPort);
        Require(sceAudioOutSetMixLevelPadSpk(badHandle, 32769u) == audioOutErrorInvalidPort);
        Require(sceAudioOutSetMixLevelPadSpk(badHandle, 50000u) == audioOutErrorInvalidPort);
        Require(sceAudioOutSetMixLevelPadSpk(badHandle, 65536u) == audioOutErrorInvalidPort);
        Require(sceAudioOutSetMixLevelPadSpk(badHandle, 0x80000000u) == audioOutErrorInvalidPort);
        Require(sceAudioOutSetMixLevelPadSpk(badHandle, 0xFFFFFFFFu) == audioOutErrorInvalidPort);
    }

    for (int unopenedHandle = 1; unopenedHandle <= 32; ++unopenedHandle) {
        Require(sceAudioOutSetMixLevelPadSpk(unopenedHandle, 0u) == audioOutErrorInvalidPort);
        Require(sceAudioOutSetMixLevelPadSpk(unopenedHandle, 11626u) == audioOutErrorInvalidPort);
        Require(sceAudioOutSetMixLevelPadSpk(unopenedHandle, 32768u) == audioOutErrorInvalidPort);
        Require(sceAudioOutSetMixLevelPadSpk(unopenedHandle, 32769u) == audioOutErrorInvalidPort);
        Require(sceAudioOutSetMixLevelPadSpk(unopenedHandle, 0xFFFFFFFFu) == audioOutErrorInvalidPort);
    }
}

void TestNonPadSpkPortTypeRejectionAndPrecedence() {
    for (const int portType : {0, 1, 2, 3, 126, 127}) {
        const int port = sceAudioOutOpen(userId, portType, 0, 256, 48000, 1);
        Require(port > 0);

        for (const std::uint32_t validLevel : {0u, 1u, 100u, 5000u, 11626u, 16384u, 32767u, 32768u}) {
            Require(sceAudioOutSetMixLevelPadSpk(port, validLevel) == audioOutErrorPortNotPadSpk);
        }

        for (const std::uint32_t invalidLevel : {32769u, 32770u, 40000u, 65535u, 65536u, 100000u, 0x80000000u, 0xFFFFFFFFu}) {
            Require(sceAudioOutSetMixLevelPadSpk(port, invalidLevel) == audioOutErrorInvalidMixLevel);
        }

        Require(sceAudioOutClose(port) == 0);
        Require(sceAudioOutSetMixLevelPadSpk(port, 11626u) == audioOutErrorInvalidPort);
        Require(sceAudioOutSetMixLevelPadSpk(port, 50000u) == audioOutErrorInvalidPort);
    }
}

void TestPadSpkMixLevelBoundsAndTransitions() {
    const int padPort = sceAudioOutOpen(userId, 4, 0, 256, 48000, 1);
    Require(padPort > 0);

    AudioOutPortState state{};
    Require(sceAudioOutGetPortState(padPort, &state) == 0);
    Require(state.output == 4);
    Require(state.channel == 1);
    Require(state.volume == 127);

    for (const std::uint32_t validLevel : {0u, 1u, 2u, 100u, 1000u, 11626u, 16384u, 24000u, 32767u, 32768u}) {
        Require(sceAudioOutSetMixLevelPadSpk(padPort, validLevel) == 0);
    }

    for (const std::uint32_t invalidLevel : {32769u, 32770u, 35000u, 48000u, 65535u, 65536u, 100000u, 0x80000000u, 0xFFFFFFFFu}) {
        Require(sceAudioOutSetMixLevelPadSpk(padPort, invalidLevel) == audioOutErrorInvalidMixLevel);
    }

    Require(sceAudioOutSetMixLevelPadSpk(padPort, 0u) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(padPort, 32768u) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(padPort, 11626u) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(padPort, 50000u) == audioOutErrorInvalidMixLevel);
    Require(sceAudioOutSetMixLevelPadSpk(padPort, 100u) == 0);

    Require(sceAudioOutClose(padPort) == 0);
    Require(sceAudioOutClose(padPort) == audioOutErrorInvalidPort);
    Require(sceAudioOutSetMixLevelPadSpk(padPort, 11626u) == audioOutErrorInvalidPort);
    Require(sceAudioOutSetMixLevelPadSpk(padPort, 40000u) == audioOutErrorInvalidPort);
}

void TestMultiPortPadSpkManagement() {
    const int padPort1 = sceAudioOutOpen(userId, 4, 0, 256, 48000, 1);
    Require(padPort1 > 0);
    const int padPort2 = sceAudioOutOpen(userId, 4, 0, 256, 48000, 1);
    Require(padPort2 > 0);
    Require(padPort1 != padPort2);

    const int mainPort = sceAudioOutOpen(userId, 0, 0, 256, 48000, 1);
    Require(mainPort > 0);

    Require(sceAudioOutSetMixLevelPadSpk(padPort1, 5000u) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(padPort2, 25000u) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(mainPort, 5000u) == audioOutErrorPortNotPadSpk);
    Require(sceAudioOutSetMixLevelPadSpk(mainPort, 50000u) == audioOutErrorInvalidMixLevel);

    Require(sceAudioOutClose(padPort1) == 0);
    Require(sceAudioOutSetMixLevelPadSpk(padPort1, 5000u) == audioOutErrorInvalidPort);
    Require(sceAudioOutSetMixLevelPadSpk(padPort1, 50000u) == audioOutErrorInvalidPort);

    Require(sceAudioOutSetMixLevelPadSpk(padPort2, 32768u) == 0);
    Require(sceAudioOutClose(padPort2) == 0);
    Require(sceAudioOutClose(mainPort) == 0);
}

void TestUnimplementedPadVolumeStubs() {
    Require(Throws([&] { sceAudioOutSetPadSpkVolume(1, 100); }));
    Require(Throws([&] { sceAudioOutSetPadJackVolume(); }));
    Require(Throws([&] { sceAudioOutSetUsbVolume(); }));
    Require(Throws([&] { sceAudioOutSetCaesarVolume(); }));
}

} // namespace

int main() {
    TestUnopenedAndInvalidHandleGates();
    TestNonPadSpkPortTypeRejectionAndPrecedence();
    TestPadSpkMixLevelBoundsAndTransitions();
    TestMultiPortPadSpkManagement();
    TestUnimplementedPadVolumeStubs();

    LibcRunShutdown_nid_postfix();
    return 0;
}
