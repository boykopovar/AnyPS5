#include "prx/libSceAudioIn/include/AudioIn.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

constexpr int ErrorInvalidHandle = static_cast<int>(0x80260101);
constexpr int ErrorInvalidPointer = static_cast<int>(0x80260105);
constexpr int ErrorInvalidState = static_cast<int>(0x80260109);

constexpr int UserId = 0x10000000;
constexpr std::uint32_t HandleBase = 0x30000000;

void Require(bool value, const std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "Require failed at line %u\n", static_cast<unsigned>(location.line()));
        std::abort();
    }
}

} // namespace

int main() {
    Require(sceAudioInInit() == 0);

    for (const int badHandle : {-1, -100, 0, 1, 7, 8, 9, 15, static_cast<int>(0x20000000u), static_cast<int>(0x30000008u), static_cast<int>(0x300000FFu), 0x7FFFFFFF}) {
        Require(sceAudioInGetRerouteCount(badHandle) == ErrorInvalidHandle);
        Require(sceAudioInGetHandleStatusInfo(badHandle, nullptr) == ErrorInvalidHandle);

        std::array<std::uint8_t, 32> buffer{};
        Require(sceAudioInGetHandleStatusInfo(badHandle, buffer.data()) == ErrorInvalidHandle);

        float gain = -2.0f;
        Require(sceAudioInGetGain(badHandle, &gain) == ErrorInvalidHandle);
        Require(gain == 1.0f);
        Require(sceAudioInGetGain(badHandle, nullptr) == ErrorInvalidPointer);

        Require(sceAudioInIsSharedDevice(badHandle) == ErrorInvalidState);
    }

    for (std::uint32_t slot = 0; slot < 8u; ++slot) {
        const int unopenedHandle = static_cast<int>(HandleBase + slot);
        Require(sceAudioInGetRerouteCount(unopenedHandle) == ErrorInvalidState);
        Require(sceAudioInGetHandleStatusInfo(unopenedHandle, nullptr) == ErrorInvalidState);

        std::array<std::uint8_t, 32> buffer{};
        Require(sceAudioInGetHandleStatusInfo(unopenedHandle, buffer.data()) == ErrorInvalidState);

        Require(sceAudioInIsSharedDevice(unopenedHandle) == ErrorInvalidState);
        Require(sceAudioInIsSharedDevice(static_cast<int>(slot)) == ErrorInvalidState);
        Require(sceAudioInIsSharedDevice(static_cast<int>(0x45670000u | slot)) == ErrorInvalidState);

        Require(sceAudioInGetSilentState(unopenedHandle) == ErrorInvalidState);
    }

    const int handle1 = sceAudioInOpen(UserId, 0, 0, 256, 48000, 2);
    Require(handle1 >= static_cast<int>(HandleBase));
    const std::uint32_t slot1 = static_cast<std::uint32_t>(handle1) & 0xffu;
    Require(slot1 < 8u);

    Require(sceAudioInGetRerouteCount(handle1) == 0);

    const int handle2 = sceAudioInOpen(UserId, 1, 0, 128, 16000, 0);
    Require(handle2 >= static_cast<int>(HandleBase) && handle2 != handle1);
    const std::uint32_t slot2 = static_cast<std::uint32_t>(handle2) & 0xffu;
    Require(slot2 < 8u && slot2 != slot1);

    Require(sceAudioInGetRerouteCount(handle2) == 0);

    Require(sceAudioInClose(handle1) == 0);

    Require(sceAudioInGetRerouteCount(handle1) == ErrorInvalidState);
    Require(sceAudioInGetHandleStatusInfo(handle1, nullptr) == ErrorInvalidState);
    std::array<std::uint8_t, 64> statusBuffer{};
    Require(sceAudioInGetHandleStatusInfo(handle1, statusBuffer.data()) == ErrorInvalidState);
    Require(sceAudioInIsSharedDevice(handle1) == ErrorInvalidState);
    Require(sceAudioInIsSharedDevice(static_cast<int>(slot1)) == ErrorInvalidState);

    Require(sceAudioInGetRerouteCount(handle2) == 0);

    Require(sceAudioInClose(handle2) == 0);

    Require(sceAudioInGetRerouteCount(handle2) == ErrorInvalidState);
    Require(sceAudioInGetHandleStatusInfo(handle2, nullptr) == ErrorInvalidState);
    Require(sceAudioInIsSharedDevice(handle2) == ErrorInvalidState);
    Require(sceAudioInIsSharedDevice(static_cast<int>(slot2)) == ErrorInvalidState);

    return 0;
}
