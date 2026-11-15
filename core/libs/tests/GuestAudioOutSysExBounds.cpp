#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {

constexpr int user = 0x10000000;
constexpr int errorExInvalidMode = static_cast<int>(0x80260015u);
constexpr int errorExInvalidControl = static_cast<int>(0x80268009u);
constexpr int invalidType = static_cast<int>(0x80260003);
constexpr int invalidPointer = static_cast<int>(0x80260004);
constexpr int invalidSize = static_cast<int>(0x80260006);
constexpr int invalidMonitorType = static_cast<int>(0x8026000C);
constexpr std::uint32_t exMonitorSize = 0x400;
constexpr std::uint32_t hdmiMonitorSize = 0x180;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestAudioOutExOpenAndModes() {
    Require(sceAudioOutExOpen(user, 5) == errorExInvalidMode);
    Require(sceAudioOutExOpen(user, 6) == errorExInvalidMode);
    Require(sceAudioOutExOpen(user, 7) == errorExInvalidMode);
    Require(sceAudioOutExOpen(user, 8) == errorExInvalidMode);
    Require(sceAudioOutExOpen(user, 11) == errorExInvalidMode);
    Require(sceAudioOutExOpen(user, 100) == errorExInvalidMode);

    const int ex0 = sceAudioOutExOpen(user, 0);
    Require(ex0 > 0 && ex0 <= 32);

    const int ex4 = sceAudioOutExOpen(user, 4);
    Require(ex4 > 0 && ex4 <= 32 && ex4 != ex0);

    const std::vector<std::int16_t> burst256(256 * 2, 0x1234);
    Require(sceAudioOutOutput(ex0, burst256.data()) == 256);
    Require(sceAudioOutOutput(ex0, nullptr) == 256);

    const std::vector<std::int16_t> burst1024(1024 * 2, 0x5678);
    Require(sceAudioOutOutput(ex4, burst1024.data()) == 1024);
    Require(sceAudioOutOutput(ex4, nullptr) == 1024);

    AudioOutPortState state{};
    bool threw = false;
    try {
        sceAudioOutGetPortState(ex0, &state);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);

    Require(sceAudioOutExClose(ex0) == 0);
    Require(sceAudioOutExClose(ex4) == 0);

    threw = false;
    try {
        sceAudioOutExClose(ex0);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);

    for (const int badHandle : {0, -1, 33, 1000}) {
        threw = false;
        try {
            sceAudioOutExClose(badHandle);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw);
    }
}

void TestAudioOutExConfigureOutput() {
    Require(sceAudioOutExConfigureOutput(1, 0, 0, 1, 0) == errorExInvalidControl);
    Require(sceAudioOutExConfigureOutput(-1, 0, 0, 1, 0) == errorExInvalidControl);

    Require(sceAudioOutExConfigureOutput(0, 0, 0, 1, 0) == 0);
    Require(sceAudioOutExConfigureOutput(0, 0, 4, 2, 0) == 0);
    Require(sceAudioOutExConfigureOutput(0, 0, 9, 3, 0) == 0);
    Require(sceAudioOutExConfigureOutput(0, 0, 0, 0xFF, 0) == 0);
    Require(sceAudioOutExConfigureOutput(0, 0, 0xFF, 0xFF, 0) == 0);

    bool threw = false;
    try {
        sceAudioOutExConfigureOutput(0, 1, 0, 1, 0);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        sceAudioOutExConfigureOutput(0, 0, 0, 1, 1);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        sceAudioOutExConfigureOutput(0, 0, 11, 1, 0);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        sceAudioOutExConfigureOutput(0, 0, 0, 0, 0);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        sceAudioOutExConfigureOutput(0, 0, 0, 4, 0);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
}

void TestAudioOutSysLifecycleAndRouting() {
    for (const std::uint32_t badMode : {0u, 1u, 2u, 3u, 4u, 6u, 7u, 10u}) {
        bool threw = false;
        try {
            sceAudioOutSysOpen(user, badMode);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw);
    }

    const int sysHandle = sceAudioOutSysOpen(user, 5);
    Require(sysHandle > 0 && sysHandle <= 32);

    const std::vector<std::int16_t> sysGrain(1024 * 8, 0x1111);
    Require(sceAudioOutOutput(sysHandle, sysGrain.data()) == 1024);
    Require(sceAudioOutOutput(sysHandle, nullptr) == 1024);

    AudioOutPortState state{};
    bool threw = false;
    try {
        sceAudioOutGetPortState(sysHandle, &state);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);

    Require(sceAudioOutSysConfigureOutput(1, 0, 5, 1, 0) == 0);

    threw = false;
    try {
        sceAudioOutSysConfigureOutput(0, 0, 5, 1, 0);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        sceAudioOutSysConfigureOutput(2, 0, 5, 1, 0);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        sceAudioOutSysConfigureOutput(1, 1, 5, 1, 0);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        sceAudioOutSysConfigureOutput(1, 0, 5, 1, 1);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        sceAudioOutSysConfigureOutput(1, 0, 4, 1, 0);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        sceAudioOutSysConfigureOutput(1, 0, 5, 2, 0);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);

    Require(sceAudioOutSysClose(sysHandle) == 0);

    threw = false;
    try {
        sceAudioOutSysClose(sysHandle);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);

    for (const int badHandle : {0, -1, 33, 500}) {
        threw = false;
        try {
            sceAudioOutSysClose(badHandle);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw);
    }

    const int exPort = sceAudioOutExOpen(user, 0);
    Require(exPort > 0);
    threw = false;
    try {
        sceAudioOutSysClose(exPort);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);
    Require(sceAudioOutExClose(exPort) == 0);
}

void TestAudioOutMonitorInfoAndState() {
    Require(sceAudioOutSetMainOutput() == 0);

    std::array<std::uint8_t, exMonitorSize> exBuffer{};
    Require(sceAudioOutExGetMonitorInfo(1, 0, exBuffer.data(), exMonitorSize) == invalidType);
    Require(sceAudioOutExGetMonitorInfo(-1, 0, exBuffer.data(), exMonitorSize) == invalidType);
    Require(sceAudioOutExGetMonitorInfo(1, 0, nullptr, exMonitorSize) == invalidType);
    Require(sceAudioOutExGetMonitorInfo(0, 0, nullptr, exMonitorSize) == invalidPointer);

    for (const std::uint32_t sz : {0u, 0x180u, exMonitorSize - 1, exMonitorSize + 1, 0xFFFFFFFFu}) {
        Require(sceAudioOutExGetMonitorInfo(0, 0, exBuffer.data(), sz) == invalidSize);
    }

    exBuffer.fill(0xAA);
    Require(sceAudioOutExGetMonitorInfo(0, 0, exBuffer.data(), exMonitorSize) == 0);
    for (const auto b : exBuffer) {
        Require(b == 0);
    }
    Require(sceAudioOutExGetMonitorInfo(0, 1, exBuffer.data(), exMonitorSize) == 0);
    Require(sceAudioOutExGetMonitorInfo(0, -1, exBuffer.data(), exMonitorSize) == 0);
    Require(sceAudioOutExGetMonitorInfo(0, 42, exBuffer.data(), exMonitorSize) == 0);

    std::array<std::uint8_t, hdmiMonitorSize> hdmiBuffer{};
    Require(sceAudioOutSysGetHdmiMonitorInfo(1, nullptr, hdmiMonitorSize) == invalidPointer);
    Require(sceAudioOutSysGetHdmiMonitorInfo(0, nullptr, hdmiMonitorSize) == invalidPointer);

    for (const std::uint32_t sz : {0u, 0x100u, hdmiMonitorSize - 1, hdmiMonitorSize + 1, 0xFFFFFFFFu}) {
        Require(sceAudioOutSysGetHdmiMonitorInfo(1, hdmiBuffer.data(), sz) == invalidSize);
    }

    for (const int t : {0, 2, -1, 17}) {
        Require(sceAudioOutSysGetHdmiMonitorInfo(t, hdmiBuffer.data(), hdmiMonitorSize) == invalidMonitorType);
    }

    bool threw = false;
    try {
        sceAudioOutSysGetHdmiMonitorInfo(0x10, hdmiBuffer.data(), hdmiMonitorSize);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw);

    hdmiBuffer.fill(0x55);
    Require(sceAudioOutSysGetHdmiMonitorInfo(1, hdmiBuffer.data(), hdmiMonitorSize) == 0);
    for (const auto b : hdmiBuffer) {
        Require(b == 0);
    }

    AudioOutSystemState sysState{};
    Require(sceAudioOutGetSystemState(nullptr) == invalidPointer);
    sysState.loudness = 5.0f;
    Require(sceAudioOutGetSystemState(&sysState) == 0);
    Require(sysState.loudness == 0.0f);
}

} // namespace

int main() {
    const int initResult = sceAudioOutInit();
    Require(initResult == 0 || initResult == static_cast<int>(0x8026000e));

    TestAudioOutExOpenAndModes();
    TestAudioOutExConfigureOutput();
    TestAudioOutSysLifecycleAndRouting();
    TestAudioOutMonitorInfoAndState();

    LibcRunShutdown_nid_postfix();
    return 0;
}
