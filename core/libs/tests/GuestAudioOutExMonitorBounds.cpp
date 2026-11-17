#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "Testing.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

constexpr int userId = 0x10000000;
constexpr int audioOutErrorInvalidPort = static_cast<int>(0x80260003);
constexpr int audioOutErrorInvalidPointer = static_cast<int>(0x80260004);
constexpr int audioOutErrorInvalidSize = static_cast<int>(0x80260006);
constexpr int errorExInvalidMode = static_cast<int>(0x80260015);
constexpr int errorExInvalidControl = static_cast<int>(0x80268009);
constexpr std::uint32_t exMonitorInfoSize = 1024;

template <typename TAction>
bool Throws(TAction&& action) {
    try {
        action();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void TestAudioOutExOpenModeBounds() {
    for (const std::uint32_t invalidMode : {5u, 6u, 7u, 8u, 11u, 12u, 50u, 100u, 0x61Fu, 0xFFFFFFFFu}) {
        Require(sceAudioOutExOpen(userId, invalidMode) == errorExInvalidMode);
        Require(sceAudioOutExOpen(0, invalidMode) == errorExInvalidMode);
        Require(sceAudioOutExOpen(-1, invalidMode) == errorExInvalidMode);
    }

    for (const std::uint32_t standardMode : {0u, 1u, 2u, 9u}) {
        const int port = sceAudioOutExOpen(userId, standardMode);
        Require(port > 0 && port <= 32);

        const std::vector<std::int16_t> sampleBuf(256 * 2, 0);
        Require(sceAudioOutOutput(port, sampleBuf.data()) == 256);
        Require(sceAudioOutOutput(port, nullptr) == 256);

        Require(sceAudioOutExClose(port) == 0);
    }

    for (const std::uint32_t highRateMode : {3u, 4u, 10u}) {
        const int port = sceAudioOutExOpen(userId, highRateMode);
        Require(port > 0 && port <= 32);

        const std::vector<std::int16_t> sampleBuf(1024 * 2, 0);
        Require(sceAudioOutOutput(port, sampleBuf.data()) == 1024);
        Require(sceAudioOutOutput(port, nullptr) == 1024);

        Require(sceAudioOutExClose(port) == 0);
    }
}

void TestAudioOutExConfigureOutputValidation() {
    for (const int nonZeroControl : {-100, -1, 1, 2, 10, 100, 0x7FFFFFFF}) {
        Require(sceAudioOutExConfigureOutput(nonZeroControl, 0, 0, 1, 0) == errorExInvalidControl);
        Require(sceAudioOutExConfigureOutput(nonZeroControl, 1, 0, 1, 0) == errorExInvalidControl);
        Require(sceAudioOutExConfigureOutput(nonZeroControl, 0, 20, 1, 0) == errorExInvalidControl);
        Require(sceAudioOutExConfigureOutput(nonZeroControl, 0, 0, 5, 0) == errorExInvalidControl);
        Require(sceAudioOutExConfigureOutput(nonZeroControl, 0, 0, 1, 1) == errorExInvalidControl);
    }

    for (const std::uint32_t validMode : {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 0xFFu}) {
        for (const std::uint32_t validTarget : {1u, 2u, 3u, 0xFFu}) {
            Require(sceAudioOutExConfigureOutput(0, 0, validMode, validTarget, 0) == 0);
        }
    }

    for (const std::uint64_t invalidFlag : {1ULL, 2ULL, 0x10ULL, 0x100ULL, 0xFFFFFFFFULL}) {
        Require(Throws([&] { sceAudioOutExConfigureOutput(0, invalidFlag, 0, 1, 0); }));
    }

    for (const std::uint64_t invalidOption : {1ULL, 2ULL, 0x10ULL, 0x100ULL, 0xFFFFFFFFULL}) {
        Require(Throws([&] { sceAudioOutExConfigureOutput(0, 0, 0, 1, invalidOption); }));
    }

    for (const std::uint32_t invalidMode : {11u, 12u, 50u, 100u, 0xFEu, 0x100u, 0xFFFFFFFFu}) {
        Require(Throws([&] { sceAudioOutExConfigureOutput(0, 0, invalidMode, 1, 0); }));
    }

    for (const std::uint32_t invalidTarget : {0u, 4u, 5u, 10u, 0xFEu, 0x100u, 0xFFFFFFFFu}) {
        Require(Throws([&] { sceAudioOutExConfigureOutput(0, 0, 0, invalidTarget, 0); }));
    }
}

void TestAudioOutExGetMonitorInfoValidation() {
    std::array<std::uint8_t, exMonitorInfoSize> monitorBuffer{};

    for (const int invalidType : {-100, -1, 1, 2, 10, 100}) {
        Require(sceAudioOutExGetMonitorInfo(invalidType, 0, monitorBuffer.data(), exMonitorInfoSize) == audioOutErrorInvalidPort);
        Require(sceAudioOutExGetMonitorInfo(invalidType, 1, monitorBuffer.data(), exMonitorInfoSize) == audioOutErrorInvalidPort);
        Require(sceAudioOutExGetMonitorInfo(invalidType, -1, monitorBuffer.data(), exMonitorInfoSize) == audioOutErrorInvalidPort);
        Require(sceAudioOutExGetMonitorInfo(invalidType, 0, nullptr, exMonitorInfoSize) == audioOutErrorInvalidPort);
        Require(sceAudioOutExGetMonitorInfo(invalidType, 0, monitorBuffer.data(), 0) == audioOutErrorInvalidPort);
        Require(sceAudioOutExGetMonitorInfo(invalidType, 0, nullptr, 0) == audioOutErrorInvalidPort);
    }

    Require(sceAudioOutExGetMonitorInfo(0, 0, nullptr, exMonitorInfoSize) == audioOutErrorInvalidPointer);
    Require(sceAudioOutExGetMonitorInfo(0, 1, nullptr, exMonitorInfoSize) == audioOutErrorInvalidPointer);
    Require(sceAudioOutExGetMonitorInfo(0, -1, nullptr, exMonitorInfoSize) == audioOutErrorInvalidPointer);
    Require(sceAudioOutExGetMonitorInfo(0, 0, nullptr, 0) == audioOutErrorInvalidPointer);
    Require(sceAudioOutExGetMonitorInfo(0, 0, nullptr, 2048) == audioOutErrorInvalidPointer);

    for (const std::uint32_t invalidSize : {0u, 1u, 256u, 512u, 1023u, 1025u, 2048u, 0xFFFFFFFFu}) {
        Require(sceAudioOutExGetMonitorInfo(0, 0, monitorBuffer.data(), invalidSize) == audioOutErrorInvalidSize);
        Require(sceAudioOutExGetMonitorInfo(0, 1, monitorBuffer.data(), invalidSize) == audioOutErrorInvalidSize);
        Require(sceAudioOutExGetMonitorInfo(0, -1, monitorBuffer.data(), invalidSize) == audioOutErrorInvalidSize);
    }

    monitorBuffer.fill(0xCC);
    Require(sceAudioOutExGetMonitorInfo(0, 0, monitorBuffer.data(), exMonitorInfoSize) == 0);
    for (const auto byte : monitorBuffer) {
        Require(byte == 0);
    }

    monitorBuffer.fill(0x77);
    Require(sceAudioOutExGetMonitorInfo(0, 1, monitorBuffer.data(), exMonitorInfoSize) == 0);
    for (const auto byte : monitorBuffer) {
        Require(byte == 0);
    }

    monitorBuffer.fill(0x33);
    Require(sceAudioOutExGetMonitorInfo(0, -1, monitorBuffer.data(), exMonitorInfoSize) == 0);
    for (const auto byte : monitorBuffer) {
        Require(byte == 0);
    }
}

void TestAudioOutExCloseAndPortIsolation() {
    const int exPort = sceAudioOutExOpen(userId, 0);
    Require(exPort > 0 && exPort <= 32);

    for (const int badHandle : {-100, -1, 0, 33, 34, 100, 0x1000}) {
        Require(Throws([&] { sceAudioOutExClose(badHandle); }));
    }

    const int unopenedHandle = (exPort == 32) ? 1 : (exPort + 1);
    Require(Throws([&] { sceAudioOutExClose(unopenedHandle); }));

    const int mainPort = sceAudioOutOpen(userId, 0, 0, 256, 48000, 1);
    Require(mainPort > 0);
    Require(Throws([&] { sceAudioOutExClose(mainPort); }));
    Require(sceAudioOutClose(mainPort) == 0);

    AudioOutPortState state{};
    Require(Throws([&] { sceAudioOutGetPortState(exPort, &state); }));

    Require(sceAudioOutExClose(exPort) == 0);
    Require(Throws([&] { sceAudioOutExClose(exPort); }));

    const std::vector<std::int16_t> sampleBuf(256 * 2, 0);
    Require(sceAudioOutOutput(exPort, sampleBuf.data()) == audioOutErrorInvalidPort);
}

} // namespace

int main() {
    const int initResult = sceAudioOutInit();
    Require(initResult == 0 || initResult == static_cast<int>(0x8026000E));

    TestAudioOutExOpenModeBounds();
    TestAudioOutExConfigureOutputValidation();
    TestAudioOutExGetMonitorInfoValidation();
    TestAudioOutExCloseAndPortIsolation();

    LibcRunShutdown_nid_postfix();
    return 0;
}
