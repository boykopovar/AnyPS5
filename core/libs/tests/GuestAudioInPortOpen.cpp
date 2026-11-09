#include "prx/libSceAudioIn/include/AudioIn.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdlib>
#include <cstdint>

static void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

int main() {
    constexpr int user = 0x10000000;
    constexpr int invalidHandle = static_cast<int>(0x80260101);
    constexpr int invalidSize = static_cast<int>(0x80260102);
    constexpr int invalidFreq = static_cast<int>(0x80260103);
    constexpr int invalidType = static_cast<int>(0x80260104);
    constexpr int invalidPointer = static_cast<int>(0x80260105);
    constexpr int invalidParam = static_cast<int>(0x80260106);

    sceAudioInInit();

    Require(sceAudioInOpen(user, 6, 0, 256, 48000, 2) == invalidType);
    Require(sceAudioInOpen(user, 99, 0, 256, 48000, 2) == invalidType);

    Require(sceAudioInOpen(user, 0, 0, 256, 48000, 3) == invalidParam);
    Require(sceAudioInOpen(user, 0, 0, 256, 48000, 0x20) == invalidParam);
    for (const std::uint32_t fmt : {0u, 1u, 3u, 0x10u, 0x11u, 0x12u}) {
        Require(sceAudioInOpen(user, 0, 0, 256, 48000, fmt) == invalidParam);
    }

    Require(sceAudioInOpen(user, 0, 0, 512, 48000, 2) == invalidSize);
    Require(sceAudioInOpen(user, 0, 0, 256, 44100, 2) == invalidFreq);

    float gain = -1.0f;
    Require(sceAudioInGetGain(invalidHandle, &gain) == invalidHandle);
    Require(sceAudioInGetGain(invalidHandle, nullptr) == invalidPointer);
    Require(sceAudioInGetGain(invalidHandle, reinterpret_cast<float*>(static_cast<std::uintptr_t>(0x10))) == invalidPointer);
    Require(sceAudioInGetRerouteCount(invalidHandle) == invalidHandle);
    Require(sceAudioInClose(invalidHandle) == invalidHandle);
    Require(sceAudioInInput(invalidHandle, nullptr) == invalidHandle);
    Require(sceAudioInGetSilentState(invalidHandle) == invalidHandle);

    int port = sceAudioInOpen(user, 0, 0, 256, 48000, 2);
    Require(port > 0);
    Require(sceAudioInGetGain(port, &gain) == 0);
    Require(sceAudioInGetRerouteCount(port) == 0);
    Require(sceAudioInClose(port) == 0);

    return 0;
}
