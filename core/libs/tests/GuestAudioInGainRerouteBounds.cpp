#include "prx/libSceAudioIn/include/AudioIn.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace {

constexpr int ErrorInvalidHandle = static_cast<int>(0x80260101);
constexpr int ErrorInvalidPointer = static_cast<int>(0x80260105);
constexpr int ErrorInvalidParam = static_cast<int>(0x80260106);
constexpr int ErrorInvalidState = static_cast<int>(0x80260109);
constexpr int ErrorInvalidDevice = static_cast<int>(0x8026010F);

constexpr int UserId = 0x10000000;
constexpr std::uint32_t HandleBase = 0x30000000;

void Require(bool value) {
    if (!value) std::abort();
}

template <typename Fn>
bool Throws(Fn&& fn) {
    try {
        fn();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

void TestUnopenedPortGainAndReroute() {
    for (const int badHandle : {-100, -1, 0, 1, 7, 8, 15, static_cast<int>(0x20000000u), static_cast<int>(0x30000008u), static_cast<int>(0x300000FFu), 0x7FFFFFFF}) {
        Require(sceAudioInGetRerouteCount(badHandle) == ErrorInvalidHandle);

        float gain = -42.0f;
        Require(sceAudioInGetGain(badHandle, &gain) == ErrorInvalidHandle);
        Require(gain == 1.0f);
        Require(sceAudioInGetGain(badHandle, nullptr) == ErrorInvalidPointer);
    }

    for (std::uint32_t slot = 0; slot < 8u; ++slot) {
        const int unopenedHandle = static_cast<int>(HandleBase + slot);
        Require(sceAudioInGetRerouteCount(unopenedHandle) == ErrorInvalidState);

        float gain = -42.0f;
        Require(sceAudioInGetGain(unopenedHandle, &gain) == ErrorInvalidState);
        Require(gain == 1.0f);
        Require(sceAudioInGetGain(unopenedHandle, nullptr) == ErrorInvalidPointer);
    }
}

void TestMicLevelBoundsAndPrecedence() {
    for (const int invalidLevel : {101, 102, 105, 150, 200, 500, 1000, -1, -2, -50, -100}) {
        Require(sceAudioInSetMicLevel(1, 0, invalidLevel, 0, 0) == ErrorInvalidParam);
        Require(sceAudioInSetMicLevel(0, 0, invalidLevel, 0, 0) == ErrorInvalidParam);
        Require(sceAudioInSetMicLevel(5, 0, invalidLevel, 0, 0) == ErrorInvalidParam);
        Require(sceAudioInSetMicLevel(10, 0, invalidLevel, 0, 0) == ErrorInvalidParam);
        Require(sceAudioInSetMicLevel(-1, 0, invalidLevel, 0, 0) == ErrorInvalidParam);

        Require(sceAudioInSetMicrophoneLevel(1, 0, invalidLevel, 0) == ErrorInvalidParam);
        Require(sceAudioInSetMicrophoneLevel(0, 0, invalidLevel, 0) == ErrorInvalidParam);
        Require(sceAudioInSetMicrophoneLevel(5, 0, invalidLevel, 0) == ErrorInvalidParam);
        Require(sceAudioInSetMicrophoneLevel(10, 0, invalidLevel, 0) == ErrorInvalidParam);
        Require(sceAudioInSetMicrophoneLevel(-1, 0, invalidLevel, 0) == ErrorInvalidParam);
    }

    for (const int validLevel : {0, 1, 10, 25, 50, 75, 99, 100}) {
        for (const int invalidType : {0, 5, 6, 7, 8, 10, 100}) {
            Require(sceAudioInSetMicLevel(invalidType, 0, validLevel, 0, 0) == ErrorInvalidDevice);
            Require(sceAudioInSetMicrophoneLevel(invalidType, 0, validLevel, 0) == ErrorInvalidDevice);
        }

        for (const int validType : {1, 2, 3, 4, -1, -2, -100}) {
            Require(Throws([&] { sceAudioInSetMicLevel(validType, 0, validLevel, 0, 0); }));
            Require(Throws([&] { sceAudioInSetMicrophoneLevel(validType, 0, validLevel, 0); }));
        }
    }
}

void TestPortLifecycleGainAndReroute() {
    std::vector<int> ports;
    for (std::uint32_t slot = 0; slot < 4u; ++slot) {
        const int handle = sceAudioInOpen(UserId, 0, 0, 256, 48000, 2);
        Require(handle >= static_cast<int>(HandleBase));
        ports.push_back(handle);
    }

    for (const int handle : ports) {
        Require(sceAudioInGetRerouteCount(handle) == 0);

        float gain = -5.0f;
        Require(sceAudioInGetGain(handle, &gain) == 0);
        Require(gain == 1.0f);
        Require(sceAudioInGetGain(handle, nullptr) == ErrorInvalidPointer);
    }

    for (const int handle : ports) {
        Require(sceAudioInClose(handle) == 0);

        Require(sceAudioInGetRerouteCount(handle) == ErrorInvalidState);

        float gain = -5.0f;
        Require(sceAudioInGetGain(handle, &gain) == ErrorInvalidState);
        Require(gain == 1.0f);
        Require(sceAudioInGetGain(handle, nullptr) == ErrorInvalidPointer);
    }
}

} // namespace

int main() {
    Require(sceAudioInInit() == 0);

    TestUnopenedPortGainAndReroute();
    TestMicLevelBoundsAndPrecedence();
    TestPortLifecycleGainAndReroute();

    return 0;
}
