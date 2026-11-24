#include "prx/libSceAudioIn/include/AudioIn.hpp"
#include "prx/libSceAudioIn/include/Exports.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>

namespace {

constexpr int audioInErrorInvalidHandle = static_cast<int>(0x80260101);
constexpr int audioInErrorInvalidSize = static_cast<int>(0x80260102);
constexpr int audioInErrorInvalidFreq = static_cast<int>(0x80260103);
constexpr int audioInErrorInvalidPointer = static_cast<int>(0x80260105);
constexpr int audioInErrorInvalidParam = static_cast<int>(0x80260106);
constexpr int audioInErrorInvalidState = static_cast<int>(0x80260109);
constexpr int audioInErrorInvalidDevice = static_cast<int>(0x8026010F);
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

void TestMicLevelBounds() {
    for (const int overLevel : {101, 102, 120, 150, 200, 255, 1000, 65535}) {
        Require(sceAudioInSetMicLevel(1, 0, overLevel, 0, 0) == audioInErrorInvalidParam);
        Require(sceAudioInSetMicLevel(0, 0, overLevel, 0, 0) == audioInErrorInvalidParam);
        Require(sceAudioInSetMicLevel(5, 0, overLevel, 0, 0) == audioInErrorInvalidParam);
        Require(sceAudioInSetMicLevel(99, 0, overLevel, 0, 0) == audioInErrorInvalidParam);
        Require(sceAudioInSetMicLevel(-1, 0, overLevel, 0, 0) == audioInErrorInvalidParam);
    }

    for (const int negativeLevel : {-1, -2, -5, -50, -100}) {
        Require(sceAudioInSetMicLevel(1, 0, negativeLevel, 0, 0) == audioInErrorInvalidParam);
        Require(sceAudioInSetMicLevel(0, 0, negativeLevel, 0, 0) == audioInErrorInvalidParam);
        Require(sceAudioInSetMicLevel(5, 0, negativeLevel, 0, 0) == audioInErrorInvalidParam);
        Require(sceAudioInSetMicLevel(-1, 0, negativeLevel, 0, 0) == audioInErrorInvalidParam);
    }

    for (const int badType : {0, 5, 6, 7, 8, 10, 50, 100, 255}) {
        Require(sceAudioInSetMicLevel(badType, 0, 0, 0, 0) == audioInErrorInvalidDevice);
        Require(sceAudioInSetMicLevel(badType, 0, 50, 0, 0) == audioInErrorInvalidDevice);
        Require(sceAudioInSetMicLevel(badType, 0, 100, 0, 0) == audioInErrorInvalidDevice);
    }

    for (const int validType : {1, 2, 3, 4}) {
        Require(Throws([&] { sceAudioInSetMicLevel(validType, 0, 0, 0, 0); }));
        Require(Throws([&] { sceAudioInSetMicLevel(validType, 0, 1, 0, 0); }));
        Require(Throws([&] { sceAudioInSetMicLevel(validType, 0, 50, 0, 0); }));
        Require(Throws([&] { sceAudioInSetMicLevel(validType, 0, 99, 0, 0); }));
        Require(Throws([&] { sceAudioInSetMicLevel(validType, 0, 100, 0, 0); }));
    }

    for (const int rawDeviceId : {-1, -2, -5, -100}) {
        Require(Throws([&] { sceAudioInSetMicLevel(rawDeviceId, 0, 0, 0, 0); }));
        Require(Throws([&] { sceAudioInSetMicLevel(rawDeviceId, 0, 50, 0, 0); }));
        Require(Throws([&] { sceAudioInSetMicLevel(rawDeviceId, 0, 100, 0, 0); }));
    }

    for (const int overLevel : {101, 102, 150, 200, 500}) {
        Require(sceAudioInSetMicrophoneLevel(1, 0, overLevel, 0) == audioInErrorInvalidParam);
        Require(sceAudioInSetMicrophoneLevel(0, 0, overLevel, 0) == audioInErrorInvalidParam);
        Require(sceAudioInSetMicrophoneLevel(5, 0, overLevel, 0) == audioInErrorInvalidParam);
        Require(sceAudioInSetMicrophoneLevel(-1, 0, overLevel, 0) == audioInErrorInvalidParam);
    }

    for (const int negativeLevel : {-1, -5, -10, -50}) {
        Require(sceAudioInSetMicrophoneLevel(1, 0, negativeLevel, 0) == audioInErrorInvalidParam);
        Require(sceAudioInSetMicrophoneLevel(0, 0, negativeLevel, 0) == audioInErrorInvalidParam);
        Require(sceAudioInSetMicrophoneLevel(5, 0, negativeLevel, 0) == audioInErrorInvalidParam);
        Require(sceAudioInSetMicrophoneLevel(-1, 0, negativeLevel, 0) == audioInErrorInvalidParam);
    }

    for (const int badType : {0, 5, 6, 7, 10}) {
        Require(sceAudioInSetMicrophoneLevel(badType, 0, 0, 0) == audioInErrorInvalidDevice);
        Require(sceAudioInSetMicrophoneLevel(badType, 0, 50, 0) == audioInErrorInvalidDevice);
        Require(sceAudioInSetMicrophoneLevel(badType, 0, 100, 0) == audioInErrorInvalidDevice);
    }

    for (const int validType : {1, 2, 3, 4}) {
        Require(Throws([&] { sceAudioInSetMicrophoneLevel(validType, 0, 0, 0); }));
        Require(Throws([&] { sceAudioInSetMicrophoneLevel(validType, 0, 50, 0); }));
        Require(Throws([&] { sceAudioInSetMicrophoneLevel(validType, 0, 100, 0); }));
    }

    for (const int rawDeviceId : {-1, -2, -10}) {
        Require(Throws([&] { sceAudioInSetMicrophoneLevel(rawDeviceId, 0, 0, 0); }));
        Require(Throws([&] { sceAudioInSetMicrophoneLevel(rawDeviceId, 0, 50, 0); }));
        Require(Throws([&] { sceAudioInSetMicrophoneLevel(rawDeviceId, 0, 100, 0); }));
    }

    Require(Throws([&] { sceAudioInSetUsbGain(); }));
}

void TestVmicParameterValidation() {
    Require(sceAudioInInit() == 0);

    for (const std::uint32_t badSize : {0u, 16u, 32u, 64u, 127u, 129u, 200u, 255u, 257u, 512u, 1024u, 2048u, 4096u}) {
        Require(sceAudioInVmicCreate(userId, badSize, 48000, 0) == audioInErrorInvalidSize);
        Require(sceAudioInVmicCreate(userId, badSize, 16000, 0) == audioInErrorInvalidSize);
    }

    for (const std::uint32_t badFreq : {0u, 8000u, 11025u, 22050u, 24000u, 32000u, 44100u, 96000u, 192000u}) {
        Require(sceAudioInVmicCreate(userId, 128, badFreq, 0) == audioInErrorInvalidFreq);
        Require(sceAudioInVmicCreate(userId, 256, badFreq, 0) == audioInErrorInvalidFreq);
    }

    Require(sceAudioInVmicCreate(userId, 512, 44100, 0) == audioInErrorInvalidSize);
    Require(sceAudioInVmicCreate(userId, 0, 0, 0) == audioInErrorInvalidSize);

    for (const std::uint32_t validSize : {128u, 256u}) {
        for (const std::uint32_t validFreq : {16000u, 48000u}) {
            Require(Throws([&] { sceAudioInVmicCreate(userId, validSize, validFreq, 0); }));
            Require(Throws([&] { sceAudioInVmicCreate(userId, validSize, validFreq, 1); }));
            Require(Throws([&] { sceAudioInVmicCreate(userId, validSize, validFreq, 2); }));
            Require(Throws([&] { sceAudioInVmicCreate(0, validSize, validFreq, 0); }));
            Require(Throws([&] { sceAudioInVmicCreate(-1, validSize, validFreq, 0); }));
        }
    }
}

void TestVmicLifecycleAndHandleBoundaries() {
    Require(Throws([&] { sceAudioInVmicDestroy(); }));
    Require(Throws([&] { sceAudioInVmicWrite(); }));
    Require(Throws([&] { sceAudioInVmicGetStatus(); }));
    Require(Throws([&] { sceAudioInVmicSetMute(); }));

    for (const int vmicHandle : {0x00040000, 0x00040001, 0x00040002, 0x00040007, 0x00040008, 0x0004ffff}) {
        Require(sceAudioInClose(vmicHandle) == audioInErrorInvalidHandle);
        Require(sceAudioInInput(vmicHandle, nullptr) == audioInErrorInvalidHandle);

        float gain = -4.0f;
        Require(sceAudioInGetGain(vmicHandle, &gain) == audioInErrorInvalidHandle);
        Require(gain == 1.0f);
        Require(sceAudioInGetGain(vmicHandle, nullptr) == audioInErrorInvalidPointer);

        Require(sceAudioInGetSilentState(vmicHandle) == audioInErrorInvalidHandle);
        Require(sceAudioInGetRerouteCount(vmicHandle) == audioInErrorInvalidHandle);
        Require(sceAudioInIsPadEmbeddedMic(vmicHandle) == audioInErrorInvalidState);
        Require(sceAudioInGetHandleStatusInfo(vmicHandle, nullptr) == audioInErrorInvalidHandle);
    }

    Require(sceAudioInIsSharedDevice(0x00040008) == audioInErrorInvalidState);
    Require(sceAudioInIsSharedDevice(0x00040009) == audioInErrorInvalidState);
    Require(sceAudioInIsSharedDevice(0x000400ff) == audioInErrorInvalidState);
    Require(sceAudioInIsSharedDevice(0x00040000) == audioInErrorInvalidState);
    Require(sceAudioInIsSharedDevice(0x00040001) == audioInErrorInvalidState);
}

} // namespace

int main() {
    TestMicLevelBounds();
    TestVmicParameterValidation();
    TestVmicLifecycleAndHandleBoundaries();

    LibcRunShutdown_nid_postfix();
    return 0;
}
