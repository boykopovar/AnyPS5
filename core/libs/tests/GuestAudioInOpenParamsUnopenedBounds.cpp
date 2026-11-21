#include "prx/libSceAudioIn/include/AudioIn.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

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

void Require(bool value, const std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "Require failed at line %u\n", static_cast<unsigned>(location.line()));
        std::fflush(stderr);
        std::abort();
    }
}

void SetEnvironment(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

} // namespace

int main() {
    SetEnvironment("SDL_AUDIODRIVER", "dummy");
    Require(sceAudioInInit() == 0);

    for (const std::uint32_t invalidSize : {0u, 1u, 64u, 127u, 129u, 255u, 257u, 512u, 1024u, 0xFFFFFFFFu}) {
        Require(sceAudioInOpen(UserId, 0, 0, invalidSize, 48000, 2) == ErrorInvalidSize);
    }
    Require(sceAudioInOpen(UserId, 9, 1, 512, 44100, 99) == ErrorInvalidSize);

    for (const std::uint32_t invalidFreq : {0u, 8000u, 11025u, 22050u, 32000u, 44100u, 96000u, 192000u, 0xFFFFFFFFu}) {
        Require(sceAudioInOpen(UserId, 0, 0, 256, invalidFreq, 2) == ErrorInvalidFreq);
        Require(sceAudioInOpen(UserId, 0, 0, 128, invalidFreq, 2) == ErrorInvalidFreq);
    }
    Require(sceAudioInOpen(UserId, 9, 1, 256, 44100, 99) == ErrorInvalidFreq);

    for (const std::uint32_t invalidType : {6u, 7u, 8u, 10u, 100u, 0xFFFFFFFFu}) {
        Require(sceAudioInOpen(UserId, invalidType, 0, 256, 48000, 2) == ErrorInvalidType);
        Require(sceAudioInOpen(UserId, invalidType, 0, 128, 16000, 2) == ErrorInvalidType);
    }
    Require(sceAudioInOpen(UserId, 6, 1, 256, 48000, 99) == ErrorInvalidType);

    for (const std::uint32_t invalidIndex : {1u, 2u, 3u, 100u, 0xFFFFFFFFu}) {
        Require(sceAudioInOpen(UserId, 0, invalidIndex, 256, 48000, 2) == ErrorInvalidParam);
        Require(sceAudioInOpen(UserId, 0, invalidIndex, 256, 48000, 99) == ErrorInvalidParam);
    }

    for (const std::uint32_t invalidFormat : {3u, 4u, 7u, 15u, 0x13u, 0x17u, 0x20u, 0x21u, 0x22u, 0x30u, 0x40u, 0xFFu}) {
        Require(sceAudioInOpen(UserId, 0, 0, 256, 48000, invalidFormat) == ErrorInvalidParam);
    }

    for (const std::uint32_t validFormat : {0u, 1u, 2u, 0x10u, 0x11u, 0x12u}) {
        const int port = sceAudioInOpen(UserId, 0, 0, 256, 48000, validFormat);
        Require(port > 0);
        Require(sceAudioInClose(port) == 0);
    }

    for (const std::uint32_t extra : {0u, 1u, 7u, 0x3Eu, 0x1FFFu, 0xFFFFFFFFu}) {
        Require(sceAudioInOpenEx(UserId, 6, 0, extra, 256, 48000, 2) == ErrorInvalidType);
        Require(sceAudioInOpenEx(UserId, 0, 0, extra, 512, 48000, 2) == ErrorInvalidSize);
        Require(sceAudioInOpenEx(UserId, 0, 0, extra, 256, 44100, 2) == ErrorInvalidFreq);
        Require(sceAudioInOpenEx(UserId, 0, 1, extra, 256, 48000, 2) == ErrorInvalidParam);
        Require(sceAudioInOpenEx(UserId, 0, 0, extra, 256, 48000, 3) == ErrorInvalidParam);
        Require(sceAudioInHqOpenEx(UserId, 0, 0, extra, 128, 44100, 2) == ErrorInvalidFreq);
    }

    const int portEx = sceAudioInOpenEx(UserId, 0, 0, 0, 256, 48000, 2);
    Require(portEx > 0);
    Require(sceAudioInClose(portEx) == 0);

    const int hqPort256 = sceAudioInHqOpenEx(UserId, 0, 0, 0, 256, 48000, 2);
    Require(hqPort256 > 0);
    Require(sceAudioInClose(hqPort256) == 0);

    const int hqPort128 = sceAudioInHqOpenEx(UserId, 0, 0, 0, 128, 48000, 2);
    Require(hqPort128 > 0);
    Require(sceAudioInClose(hqPort128) == 0);

    Require(sceAudioInHqOpen(UserId, 0, 0, 128, 16000, 2) == ErrorInvalidFreq);
    Require(sceAudioInHqOpen(UserId, 0, 0, 256, 48000, 2) == ErrorInvalidSize);
    Require(sceAudioInHqOpen(UserId, 0, 0, 128, 48000, 0) == ErrorInvalidParam);
    Require(sceAudioInHqOpen(UserId, 0, 0, 128, 48000, 1) == ErrorInvalidParam);
    Require(sceAudioInHqOpen(UserId, 0, 0, 128, 48000, 0x10) == ErrorInvalidParam);
    const int hq = sceAudioInHqOpen(UserId, 0, 0, 128, 48000, 2);
    Require(hq > 0);
    Require(sceAudioInClose(hq) == 0);

    Require(sceAudioInAsyncOpen(UserId, 0, 0, 128, 16000, 2) == ErrorInvalidFreq);
    Require(sceAudioInAsyncOpen(UserId, 0, 0, 256, 48000, 2) == ErrorInvalidSize);
    const int asyncPort = sceAudioInAsyncOpen(UserId, 0, 0, 128, 48000, 0x10);
    Require(asyncPort > 0);
    Require(sceAudioInClose(asyncPort) == 0);

    for (const int badHandle : {-1, -100, 0, 1, 9, static_cast<int>(0x20000000u), static_cast<int>(0x30000008u), static_cast<int>(0x300000FFu), 0x7FFFFFFF}) {
        Require(sceAudioInClose(badHandle) == ErrorInvalidHandle);
        Require(sceAudioInInput(badHandle, nullptr) == ErrorInvalidHandle);
        Require(sceAudioInGetRerouteCount(badHandle) == ErrorInvalidHandle);
        Require(sceAudioInGetSilentState(badHandle) == ErrorInvalidHandle);
        Require(sceAudioInIsPadEmbeddedMic(badHandle) == ErrorInvalidHandle);
        Require(sceAudioInGetHandleStatusInfo(badHandle, nullptr) == ErrorInvalidHandle);

        float gain = -3.0f;
        Require(sceAudioInGetGain(badHandle, &gain) == ErrorInvalidHandle);
        Require(gain == 1.0f);
        Require(sceAudioInGetGain(badHandle, nullptr) == ErrorInvalidPointer);

        if ((static_cast<std::uint32_t>(badHandle) & 0xffu) >= 8u) {
            Require(sceAudioInIsSharedDevice(badHandle) == ErrorInvalidHandle);
        }
    }

    for (std::uint32_t slot = 0; slot < 8u; ++slot) {
        const int unopened = static_cast<int>(HandleBase + slot);
        Require(sceAudioInClose(unopened) == ErrorInvalidState);
        Require(sceAudioInInput(unopened, nullptr) == ErrorInvalidState);
        Require(sceAudioInGetRerouteCount(unopened) == ErrorInvalidState);
        Require(sceAudioInGetSilentState(unopened) == ErrorInvalidState);
        Require(sceAudioInIsPadEmbeddedMic(unopened) == ErrorInvalidState);
        Require(sceAudioInGetHandleStatusInfo(unopened, nullptr) == ErrorInvalidState);
        Require(sceAudioInIsSharedDevice(unopened) == ErrorInvalidState);

        float gain = -3.0f;
        Require(sceAudioInGetGain(unopened, &gain) == ErrorInvalidState);
        Require(gain == 1.0f);
    }

    std::array<int, 8> openHandles{};
    for (std::size_t i = 0; i < openHandles.size(); ++i) {
        openHandles[i] = sceAudioInOpen(UserId, 0, 0, 256, 48000, 2);
        Require(openHandles[i] >= static_cast<int>(HandleBase));
    }

    Require(sceAudioInOpen(UserId, 0, 0, 256, 48000, 2) == ErrorPortFull);
    Require(sceAudioInOpenEx(UserId, 0, 0, 7, 256, 48000, 2) == ErrorPortFull);
    Require(sceAudioInHqOpen(UserId, 0, 0, 128, 48000, 2) == ErrorPortFull);
    Require(sceAudioInAsyncOpen(UserId, 0, 0, 128, 48000, 2) == ErrorPortFull);

    float activeGain = 0.0f;
    Require(sceAudioInGetGain(openHandles[0], &activeGain) == 0);
    Require(activeGain == 1.0f);
    Require(sceAudioInGetRerouteCount(openHandles[0]) == 0);
    Require(sceAudioInIsPadEmbeddedMic(openHandles[0]) == 0);
    Require(sceAudioInInput(openHandles[0], nullptr) == 0);

    Require(sceAudioInClose(openHandles[0]) == 0);

    Require(sceAudioInClose(openHandles[0]) == ErrorInvalidState);
    Require(sceAudioInInput(openHandles[0], nullptr) == ErrorInvalidState);
    Require(sceAudioInGetRerouteCount(openHandles[0]) == ErrorInvalidState);
    Require(sceAudioInGetSilentState(openHandles[0]) == ErrorInvalidState);
    Require(sceAudioInIsPadEmbeddedMic(openHandles[0]) == ErrorInvalidState);
    Require(sceAudioInGetHandleStatusInfo(openHandles[0], nullptr) == ErrorInvalidState);
    Require(sceAudioInIsSharedDevice(openHandles[0]) == ErrorInvalidState);

    const int reclaimed = sceAudioInOpen(UserId, 0, 0, 256, 48000, 2);
    Require(reclaimed == openHandles[0]);
    Require(sceAudioInClose(reclaimed) == 0);

    for (std::size_t i = 1; i < openHandles.size(); ++i) {
        Require(sceAudioInClose(openHandles[i]) == 0);
    }

    return 0;
}
