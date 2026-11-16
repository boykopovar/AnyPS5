#include "prx/libSceAudioIn/include/AudioIn.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace {

constexpr int ErrorInvalidHandle = static_cast<int>(0x80260101);
constexpr int ErrorInvalidSize = static_cast<int>(0x80260102);
constexpr int ErrorInvalidFreq = static_cast<int>(0x80260103);
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

void TestSparkAndDebtStubs() {
    Require(Throws([] { sceAudioInSetSparkParam(); }));
    Require(Throws([] { sceAudioInSetSparkSideTone(); }));
    Require(Throws([] { sceAudioInSetDevConnection(); }));
    Require(Throws([] { sceAudioInSetConnections(); }));
    Require(Throws([] { sceAudioInSetPortConnections(); }));
    Require(Throws([] { sceAudioInSetPortStatuses(); }));
    Require(Throws([] { sceAudioInChangeAppModuleState(); }));
    Require(Throws([] { sceAudioInSetUsbGain(); }));
    Require(Throws([] { sceAudioInExtCtrl(); }));
    Require(Throws([] { sceAudioInInputs(); }));
    Require(Throws([] { sceAudioInCountPorts(1, 0, 0, 0); }));
    Require(Throws([] { sceAudioInVmicDestroy(); }));
    Require(Throws([] { sceAudioInVmicWrite(); }));
    Require(Throws([] { sceAudioInVmicGetStatus(); }));
    Require(Throws([] { sceAudioInVmicSetMute(); }));

    Require(sceAudioInSetCompressorPreGain() == 0);
    Require(sceAudioInSetNoiseGateThreshold() == 0);
}

void TestParameterBoundsAndErrorPrecedence() {
    for (const int invalidLevel : {101, 102, 200, 1000, -1, -50}) {
        Require(sceAudioInSetMicLevel(1, 0, invalidLevel, 0, 0) == ErrorInvalidParam);
        Require(sceAudioInSetMicLevel(0, 0, invalidLevel, 0, 0) == ErrorInvalidParam);
        Require(sceAudioInSetMicrophoneLevel(1, 0, invalidLevel, 0) == ErrorInvalidParam);
        Require(sceAudioInSetMicrophoneLevel(0, 0, invalidLevel, 0) == ErrorInvalidParam);
    }

    for (const int invalidType : {0, 5, 6, 7, 10, 100}) {
        Require(sceAudioInSetMicLevel(invalidType, 0, 50, 0, 0) == ErrorInvalidDevice);
        Require(sceAudioInSetMicrophoneLevel(invalidType, 0, 50, 0) == ErrorInvalidDevice);
    }

    for (const int validType : {1, 2, 3, 4, -1, -2}) {
        Require(Throws([&] { sceAudioInSetMicLevel(validType, 0, 50, 0, 0); }));
        Require(Throws([&] { sceAudioInSetMicrophoneLevel(validType, 0, 50, 0); }));
    }

    for (const int invalidMuteType : {0, 5, 8, 9, 100, static_cast<int>(0x80000000u), static_cast<int>(0x80000005u)}) {
        Require(sceAudioInSetDeviceMuteStatus(invalidMuteType, 0, 1) == ErrorInvalidDevice);
        Require(sceAudioInSetUserMute(invalidMuteType, 0, 1) == ErrorInvalidDevice);
    }

    for (const int validMuteType : {1, 2, 3, 4, 6, 7, static_cast<int>(0x80000001u), static_cast<int>(0x80000007u)}) {
        Require(Throws([&] { sceAudioInSetDeviceMuteStatus(validMuteType, 0, 1); }));
        Require(Throws([&] { sceAudioInSetUserMute(validMuteType, 0, 1); }));
    }

    for (const std::uint32_t invalidLen : {0u, 64u, 96u, 255u, 257u, 512u}) {
        Require(sceAudioInVmicCreate(UserId, invalidLen, 48000, 2) == ErrorInvalidSize);
    }

    for (const std::uint32_t invalidFreq : {0u, 8000u, 22050u, 32000u, 44100u, 96000u}) {
        Require(sceAudioInVmicCreate(UserId, 128, invalidFreq, 2) == ErrorInvalidFreq);
        Require(sceAudioInVmicCreate(UserId, 256, invalidFreq, 2) == ErrorInvalidFreq);
    }

    Require(Throws([] { sceAudioInVmicCreate(UserId, 128, 48000, 2); }));
    Require(Throws([] { sceAudioInVmicCreate(UserId, 256, 16000, 2); }));

    Require(Throws([] { sceAudioInSetMode(0, 1); }));
    Require(Throws([] { sceAudioInSetMode2(0, 2, nullptr); }));
}

void TestUnopenedPortGates() {
    for (const int badHandle : {-100, -1, 0, 1, 7, 8, 15, static_cast<int>(0x20000000u), static_cast<int>(0x30000008u), static_cast<int>(0x300000FFu), 0x7FFFFFFF}) {
        Require(sceAudioInClose(badHandle) == ErrorInvalidHandle);
        Require(sceAudioInInput(badHandle, nullptr) == ErrorInvalidHandle);
        Require(sceAudioInGetSilentState(badHandle) == ErrorInvalidHandle);

        float gain = -1.0f;
        Require(sceAudioInGetGain(badHandle, &gain) == ErrorInvalidHandle);
        Require(gain == 1.0f);
        Require(sceAudioInGetGain(badHandle, nullptr) == ErrorInvalidPointer);
    }

    for (std::uint32_t slot = 0; slot < 8u; ++slot) {
        const int unopenedHandle = static_cast<int>(HandleBase + slot);
        Require(sceAudioInClose(unopenedHandle) == ErrorInvalidState);
        Require(sceAudioInInput(unopenedHandle, nullptr) == ErrorInvalidState);
        Require(sceAudioInGetSilentState(unopenedHandle) == ErrorInvalidState);

        float gain = -1.0f;
        Require(sceAudioInGetGain(unopenedHandle, &gain) == ErrorInvalidState);
        Require(gain == 1.0f);
        Require(sceAudioInGetGain(unopenedHandle, nullptr) == ErrorInvalidPointer);
    }
}

void TestPortLifecycleAndPostCloseValidation() {
    const int handle = sceAudioInOpen(UserId, 0, 0, 256, 48000, 2);
    Require(handle >= static_cast<int>(HandleBase));

    Require(sceAudioInInput(handle, nullptr) == 0);
    Require(sceAudioInGetSilentState(handle) == 0);

    float gain = -1.0f;
    Require(sceAudioInGetGain(handle, &gain) == 0);
    Require(gain == 1.0f);

    Require(Throws([] { sceAudioInSetSparkParam(); }));
    Require(Throws([] { sceAudioInSetSparkSideTone(); }));
    Require(Throws([] { sceAudioInSetDevConnection(); }));

    Require(sceAudioInInput(handle, nullptr) == 0);
    Require(sceAudioInGetSilentState(handle) == 0);

    Require(sceAudioInClose(handle) == 0);

    Require(sceAudioInClose(handle) == ErrorInvalidState);
    Require(sceAudioInInput(handle, nullptr) == ErrorInvalidState);
    Require(sceAudioInGetSilentState(handle) == ErrorInvalidState);
    gain = -1.0f;
    Require(sceAudioInGetGain(handle, &gain) == ErrorInvalidState);
    Require(gain == 1.0f);
    Require(sceAudioInGetGain(handle, nullptr) == ErrorInvalidPointer);
}

} // namespace

int main() {
    Require(sceAudioInInit() == 0);

    TestSparkAndDebtStubs();
    TestParameterBoundsAndErrorPrecedence();
    TestUnopenedPortGates();
    TestPortLifecycleAndPostCloseValidation();

    return 0;
}
