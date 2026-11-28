#include "prx/libSceAudioIn/include/AudioIn.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace {

constexpr int ErrorInvalidHandle = static_cast<int>(0x80260101);
constexpr int ErrorInvalidSize = static_cast<int>(0x80260102);
constexpr int ErrorInvalidFreq = static_cast<int>(0x80260103);
constexpr int ErrorInvalidType = static_cast<int>(0x80260104);
constexpr int ErrorInvalidPointer = static_cast<int>(0x80260105);
constexpr int ErrorInvalidParam = static_cast<int>(0x80260106);
constexpr int ErrorPortFull = static_cast<int>(0x80260107);
constexpr int ErrorInvalidState = static_cast<int>(0x80260109);

constexpr int UserId = 0x10000000;
constexpr std::uint32_t HandleBase = 0x30000000;

void Require(bool value) {
    if (!value) std::abort();
}

void TestUnopenedPortGates() {
    for (const int badHandle : {-100, -1, 0, 1, 7, 8, 15, static_cast<int>(0x20000000u), static_cast<int>(0x30000008u), static_cast<int>(0x300000FFu)}) {
        Require(sceAudioInClose(badHandle) == ErrorInvalidHandle);
        Require(sceAudioInInput(badHandle, nullptr) == ErrorInvalidHandle);
        Require(sceAudioInGetSilentState(badHandle) == ErrorInvalidHandle);
        Require(sceAudioInGetRerouteCount(badHandle) == ErrorInvalidHandle);
        Require(sceAudioInIsPadEmbeddedMic(badHandle) == ErrorInvalidHandle);

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
        Require(sceAudioInGetRerouteCount(unopenedHandle) == ErrorInvalidState);
        Require(sceAudioInIsPadEmbeddedMic(unopenedHandle) == ErrorInvalidState);

        float gain = -1.0f;
        Require(sceAudioInGetGain(unopenedHandle, &gain) == ErrorInvalidState);
        Require(gain == 1.0f);
        Require(sceAudioInGetGain(unopenedHandle, nullptr) == ErrorInvalidPointer);
    }
}

void TestHqOpenValidation() {
    for (const std::uint32_t invalidFreq : {0u, 8000u, 16000u, 22050u, 32000u, 44100u, 48001u, 96000u}) {
        Require(sceAudioInHqOpen(UserId, 0, 0, 128, invalidFreq, 2) == ErrorInvalidFreq);
        Require(sceAudioInHqOpen(UserId, 0, 0, 256, invalidFreq, 2) == ErrorInvalidFreq);
        Require(sceAudioInHqOpen(UserId, 0, 0, 0, invalidFreq, 0) == ErrorInvalidFreq);
        Require(sceAudioInHqOpen(UserId, 10, 5, 512, invalidFreq, 99) == ErrorInvalidFreq);
    }

    for (const std::uint32_t invalidLen : {0u, 64u, 96u, 127u, 129u, 256u, 512u, 1024u}) {
        Require(sceAudioInHqOpen(UserId, 0, 0, invalidLen, 48000, 2) == ErrorInvalidSize);
        Require(sceAudioInHqOpen(UserId, 0, 0, invalidLen, 48000, 0) == ErrorInvalidSize);
        Require(sceAudioInHqOpen(UserId, 10, 5, invalidLen, 48000, 99) == ErrorInvalidSize);
    }

    for (const std::uint32_t invalidParam : {0u, 1u, 3u, 4u, 15u, 16u, 18u, 99u}) {
        Require(sceAudioInHqOpen(UserId, 0, 0, 128, 48000, invalidParam) == ErrorInvalidParam);
        Require(sceAudioInHqOpen(UserId, 10, 5, 128, 48000, invalidParam) == ErrorInvalidParam);
    }

    for (const std::uint32_t invalidType : {6u, 7u, 10u, 100u, 0xFFFFFFFFu}) {
        Require(sceAudioInHqOpen(UserId, invalidType, 0, 128, 48000, 2) == ErrorInvalidType);
    }

    for (const std::uint32_t invalidIndex : {1u, 2u, 5u, 10u, 0xFFFFFFFFu}) {
        Require(sceAudioInHqOpen(UserId, 0, invalidIndex, 128, 48000, 2) == ErrorInvalidParam);
    }
}

void TestAsyncOpenValidation() {
    for (const std::uint32_t invalidFreq : {0u, 8000u, 16000u, 22050u, 32000u, 44100u, 48001u, 96000u}) {
        Require(sceAudioInAsyncOpen(UserId, 0, 0, 128, invalidFreq, 0) == ErrorInvalidFreq);
        Require(sceAudioInAsyncOpen(UserId, 0, 0, 256, invalidFreq, 2) == ErrorInvalidFreq);
        Require(sceAudioInAsyncOpen(UserId, 10, 5, 0, invalidFreq, 99) == ErrorInvalidFreq);
    }

    for (const std::uint32_t invalidLen : {0u, 64u, 96u, 127u, 129u, 256u, 512u, 1024u}) {
        Require(sceAudioInAsyncOpen(UserId, 0, 0, invalidLen, 48000, 0) == ErrorInvalidSize);
        Require(sceAudioInAsyncOpen(UserId, 0, 0, invalidLen, 48000, 2) == ErrorInvalidSize);
        Require(sceAudioInAsyncOpen(UserId, 10, 5, invalidLen, 48000, 99) == ErrorInvalidSize);
    }

    for (const std::uint32_t invalidType : {6u, 7u, 10u, 100u, 0xFFFFFFFFu}) {
        Require(sceAudioInAsyncOpen(UserId, invalidType, 0, 128, 48000, 0) == ErrorInvalidType);
        Require(sceAudioInAsyncOpen(UserId, invalidType, 0, 128, 48000, 2) == ErrorInvalidType);
    }

    for (const std::uint32_t invalidIndex : {1u, 2u, 5u, 10u, 0xFFFFFFFFu}) {
        Require(sceAudioInAsyncOpen(UserId, 0, invalidIndex, 128, 48000, 0) == ErrorInvalidParam);
        Require(sceAudioInAsyncOpen(UserId, 0, invalidIndex, 128, 48000, 2) == ErrorInvalidParam);
    }

    for (const std::uint32_t invalidParam : {3u, 4u, 15u, 19u, 32u, 99u}) {
        Require(sceAudioInAsyncOpen(UserId, 0, 0, 128, 48000, invalidParam) == ErrorInvalidParam);
    }
}

void TestPortExhaustionAndLifecycle() {
    std::vector<int> openedHandles;

    for (std::uint32_t type = 0; type < 4u; ++type) {
        const int hqHandle = sceAudioInHqOpen(UserId, type, 0, 128, 48000, 2);
        Require(hqHandle >= static_cast<int>(HandleBase));
        openedHandles.push_back(hqHandle);
    }

    for (std::uint32_t param : {0u, 1u, 2u, 0x10u}) {
        const int asyncHandle = sceAudioInAsyncOpen(UserId, 0, 0, 128, 48000, param);
        Require(asyncHandle >= static_cast<int>(HandleBase));
        openedHandles.push_back(asyncHandle);
    }

    Require(openedHandles.size() == 8u);

    for (std::size_t i = 0; i < openedHandles.size(); ++i) {
        for (std::size_t j = i + 1; j < openedHandles.size(); ++j) {
            Require(openedHandles[i] != openedHandles[j]);
        }
    }

    Require(sceAudioInHqOpen(UserId, 0, 0, 128, 16000, 2) == ErrorInvalidFreq);
    Require(sceAudioInHqOpen(UserId, 0, 0, 256, 48000, 2) == ErrorInvalidSize);
    Require(sceAudioInHqOpen(UserId, 0, 0, 128, 48000, 1) == ErrorInvalidParam);
    Require(sceAudioInHqOpen(UserId, 7, 0, 128, 48000, 2) == ErrorInvalidType);
    Require(sceAudioInHqOpen(UserId, 0, 3, 128, 48000, 2) == ErrorInvalidParam);

    Require(sceAudioInAsyncOpen(UserId, 0, 0, 128, 16000, 2) == ErrorInvalidFreq);
    Require(sceAudioInAsyncOpen(UserId, 0, 0, 256, 48000, 2) == ErrorInvalidSize);
    Require(sceAudioInAsyncOpen(UserId, 7, 0, 128, 48000, 2) == ErrorInvalidType);
    Require(sceAudioInAsyncOpen(UserId, 0, 3, 128, 48000, 2) == ErrorInvalidParam);
    Require(sceAudioInAsyncOpen(UserId, 0, 0, 128, 48000, 99) == ErrorInvalidParam);

    Require(sceAudioInHqOpen(UserId, 0, 0, 128, 48000, 2) == ErrorPortFull);
    Require(sceAudioInAsyncOpen(UserId, 0, 0, 128, 48000, 2) == ErrorPortFull);
    Require(sceAudioInOpen(UserId, 0, 0, 256, 48000, 2) == ErrorPortFull);

    for (const int handle : openedHandles) {
        Require(sceAudioInInput(handle, nullptr) == 0);
        Require(sceAudioInGetSilentState(handle) == 0);
        Require(sceAudioInGetRerouteCount(handle) == 0);
        Require(sceAudioInIsPadEmbeddedMic(handle) == 0);

        float gain = -1.0f;
        Require(sceAudioInGetGain(handle, &gain) == 0);
        Require(gain == 1.0f);
    }

    const int released = openedHandles.back();
    openedHandles.pop_back();
    Require(sceAudioInClose(released) == 0);

    Require(sceAudioInClose(released) == ErrorInvalidState);
    Require(sceAudioInInput(released, nullptr) == ErrorInvalidState);
    Require(sceAudioInGetSilentState(released) == ErrorInvalidState);
    Require(sceAudioInGetRerouteCount(released) == ErrorInvalidState);
    Require(sceAudioInIsPadEmbeddedMic(released) == ErrorInvalidState);

    const int newAsyncHandle = sceAudioInAsyncOpen(UserId, 0, 0, 128, 48000, 0x12u);
    Require(newAsyncHandle == released);
    Require(sceAudioInHqOpen(UserId, 0, 0, 128, 48000, 2) == ErrorPortFull);

    Require(sceAudioInClose(newAsyncHandle) == 0);

    const int newHqHandle = sceAudioInHqOpen(UserId, 1, 0, 128, 48000, 2);
    Require(newHqHandle == released);
    Require(sceAudioInAsyncOpen(UserId, 0, 0, 128, 48000, 2) == ErrorPortFull);

    for (const int handle : openedHandles) {
        Require(sceAudioInClose(handle) == 0);
        Require(sceAudioInClose(handle) == ErrorInvalidState);
    }
    Require(sceAudioInClose(newHqHandle) == 0);
    Require(sceAudioInClose(newHqHandle) == ErrorInvalidState);
}

} // namespace

int main() {
    Require(sceAudioInInit() == 0);

    TestUnopenedPortGates();
    TestHqOpenValidation();
    TestAsyncOpenValidation();
    TestPortExhaustionAndLifecycle();

    return 0;
}
