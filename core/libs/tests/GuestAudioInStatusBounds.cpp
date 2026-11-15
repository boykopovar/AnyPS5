#include "prx/libSceAudioIn/include/AudioIn.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>

namespace {

constexpr int invalidHandle = static_cast<int>(0x80260101);
constexpr int invalidSize = static_cast<int>(0x80260102);
constexpr int invalidFreq = static_cast<int>(0x80260103);
constexpr int invalidType = static_cast<int>(0x80260104);
constexpr int invalidParam = static_cast<int>(0x80260106);
constexpr int portFull = static_cast<int>(0x80260107);
constexpr int invalidState = static_cast<int>(0x80260109);
constexpr std::uint32_t handleBase = 0x30000000;
constexpr int user = 0x10000000;

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

void TestOpenPortTypesAndParameters() {
    Require(sceAudioInInit() == 0);

    for (const std::uint32_t type : {0u, 1u, 2u, 3u, 4u, 5u}) {
        const int h = sceAudioInOpen(user, type, 0, 256, 48000, 2);
        Require(h >= static_cast<int>(handleBase));
        Require(sceAudioInClose(h) == 0);
    }

    for (const std::uint32_t badType : {6u, 7u, 8u, 10u, 100u, 255u}) {
        Require(sceAudioInOpen(user, badType, 0, 256, 48000, 2) == invalidType);
    }

    Require(sceAudioInOpen(user, 0, 1, 256, 48000, 2) == invalidParam);
    Require(sceAudioInOpen(user, 0, 5, 256, 48000, 2) == invalidParam);

    for (const std::uint32_t badSize : {0u, 64u, 127u, 129u, 255u, 257u, 512u, 1024u}) {
        Require(sceAudioInOpen(user, 0, 0, badSize, 48000, 2) == invalidSize);
    }

    for (const std::uint32_t badFreq : {0u, 8000u, 11025u, 22050u, 32000u, 44100u, 96000u, 192000u}) {
        Require(sceAudioInOpen(user, 0, 0, 256, badFreq, 2) == invalidFreq);
    }
}

void TestChannelLayoutLimitsAndFormats() {
    for (const std::uint32_t validParam : {0u, 1u, 2u, 0x10u, 0x11u, 0x12u}) {
        const int h = sceAudioInOpen(user, 0, 0, 128, 16000, validParam);
        Require(h >= static_cast<int>(handleBase));
        Require(sceAudioInClose(h) == 0);
    }

    for (const std::uint32_t badChannel : {3u, 4u, 5u, 7u, 8u, 15u, 0x13u, 0x14u, 0x1fu}) {
        Require(sceAudioInOpen(user, 0, 0, 256, 48000, badChannel) == invalidParam);
    }

    for (const std::uint32_t badEncoding : {0x20u, 0x30u, 0x40u, 0x50u, 0x80u, 0xf0u, 0x22u, 0x32u, 0x82u}) {
        Require(sceAudioInOpen(user, 0, 0, 256, 48000, badEncoding) == invalidParam);
    }
}

void TestPortCapacityAndSlotReuse() {
    std::array<int, 8> handles{};
    for (int i = 0; i < 8; ++i) {
        handles[i] = sceAudioInOpen(user, 0, 0, 256, 48000, 2);
        Require(handles[i] >= static_cast<int>(handleBase));
    }

    const int overflow = sceAudioInOpen(user, 0, 0, 256, 48000, 2);
    Require(overflow == portFull);

    Require(sceAudioInClose(handles[0]) == 0);
    Require(sceAudioInClose(handles[0]) == invalidState);

    const int reopened = sceAudioInOpen(user, 0, 0, 256, 48000, 2);
    Require(reopened == handles[0]);
    handles[0] = reopened;

    for (int i = 0; i < 8; ++i) {
        Require(sceAudioInClose(handles[i]) == 0);
    }
}

void TestHandleStatusAndQueryBoundaries() {
    for (const int badHandle : {0, -1, 1, 33, static_cast<int>(0x20000000), static_cast<int>(0x30000008)}) {
        Require(sceAudioInGetHandleStatusInfo(badHandle, nullptr) == invalidHandle);
    }

    const int h = sceAudioInOpen(user, 0, 0, 256, 48000, 2);
    Require(h >= static_cast<int>(handleBase));
    Require(sceAudioInClose(h) == 0);
    Require(sceAudioInGetHandleStatusInfo(h, nullptr) == invalidState);

    Require(sceAudioInIsSharedDevice(8) == invalidHandle);
    Require(sceAudioInIsSharedDevice(-1) == invalidHandle);
    Require(sceAudioInIsSharedDevice(0) == invalidState);

    const int sh = sceAudioInOpen(user, 0, 0, 256, 48000, 2);
    Require(sh >= static_cast<int>(handleBase));
    Require(sceAudioInClose(sh) == 0);
    Require(sceAudioInIsSharedDevice(sh) == invalidState);

    Require(sceAudioInGetSilentState(0) == invalidHandle);
    Require(sceAudioInGetSilentState(-1) == invalidHandle);
    Require(sceAudioInGetSilentState(static_cast<int>(0x30000008)) == invalidHandle);

    const int silPort = sceAudioInOpen(user, 0, 0, 256, 48000, 2);
    Require(silPort >= static_cast<int>(handleBase));
    const int silentState = sceAudioInGetSilentState(silPort);
    Require(silentState == 0 || silentState == 1);
    Require(sceAudioInClose(silPort) == 0);
    Require(sceAudioInGetSilentState(silPort) == invalidState);
}

} // namespace

int main() {
    TestOpenPortTypesAndParameters();
    TestChannelLayoutLimitsAndFormats();
    TestPortCapacityAndSlotReuse();
    TestHandleStatusAndQueryBoundaries();

    LibcRunShutdown_nid_postfix();
    return 0;
}
