#include "prx/libSceAudioIn/include/AudioIn.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
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

void SetEnvironment(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

void Require(bool value, const std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "Require failed at line %u\n", static_cast<unsigned>(location.line()));
        std::fflush(stderr);
        std::abort();
    }
}

void TestPrecedenceAndBounds() {
    for (const std::uint32_t invalidType : {6u, 7u, 10u, 100u, 0xFFFFFFFFu}) {
        Require(sceAudioInDeviceIdOpen(UserId, invalidType, 0, 0, 0, 99) == ErrorInvalidType);
        Require(sceAudioInDeviceIdOpen(UserId, invalidType, 1, 256, 48000, 2) == ErrorInvalidType);
        Require(sceAudioInDeviceIdAsyncOpen(UserId, invalidType, 0, 0, 0, 99) == ErrorInvalidType);
        Require(sceAudioInDeviceIdAsyncOpen(UserId, invalidType, 1, 128, 16000, 0) == ErrorInvalidType);
        Require(sceAudioInDeviceIdHqOpen(UserId, invalidType, 0, 0, 0, 99) == ErrorInvalidType);
        Require(sceAudioInDeviceIdHqOpen(UserId, invalidType, 2, 256, 48000, 2) == ErrorInvalidType);
        Require(sceAudioInDeviceOpenEx(UserId, invalidType, 0, 0, 0, 0, 99) == ErrorInvalidType);
    }

    for (const std::uint32_t validType : {0u, 1u, 2u, 3u, 4u, 5u}) {
        for (const std::uint32_t invalidFreq : {0u, 8000u, 22050u, 32000u, 44100u, 48001u, 96000u}) {
            Require(sceAudioInDeviceIdOpen(UserId, validType, 0, 0, invalidFreq, 99) == ErrorInvalidFreq);
            Require(sceAudioInDeviceIdOpen(UserId, validType, 10, 256, invalidFreq, 2) == ErrorInvalidFreq);
            Require(sceAudioInDeviceIdAsyncOpen(UserId, validType, 0, 128, invalidFreq, 0) == ErrorInvalidFreq);
            Require(sceAudioInDeviceIdHqOpen(UserId, validType, 0, 256, invalidFreq, 2) == ErrorInvalidFreq);
            Require(sceAudioInDeviceOpenEx(UserId, validType, 0, 0, 128, invalidFreq, 2) == ErrorInvalidFreq);
        }

        for (const std::uint32_t validFreq : {16000u, 48000u}) {
            for (const std::uint32_t invalidLen : {0u, 64u, 96u, 127u, 129u, 255u, 257u, 512u, 1024u}) {
                Require(sceAudioInDeviceIdOpen(UserId, validType, 0, invalidLen, validFreq, 99) == ErrorInvalidSize);
                Require(sceAudioInDeviceIdAsyncOpen(UserId, validType, 5, invalidLen, validFreq, 0) == ErrorInvalidSize);
                Require(sceAudioInDeviceIdHqOpen(UserId, validType, 8, invalidLen, validFreq, 2) == ErrorInvalidSize);
                Require(sceAudioInDeviceOpenEx(UserId, validType, 0, 0, invalidLen, validFreq, 2) == ErrorInvalidSize);
            }

            for (const std::uint32_t invalidParam : {3u, 4u, 15u, 19u, 32u, 99u}) {
                Require(sceAudioInDeviceIdOpen(UserId, validType, 0, 128, validFreq, invalidParam) == ErrorInvalidParam);
                Require(sceAudioInDeviceIdAsyncOpen(UserId, validType, 0, 256, validFreq, invalidParam) == ErrorInvalidParam);
                Require(sceAudioInDeviceIdHqOpen(UserId, validType, 0, 128, validFreq, invalidParam) == ErrorInvalidParam);
                Require(sceAudioInDeviceOpenEx(UserId, validType, 0, 0, 256, validFreq, invalidParam) == ErrorInvalidParam);
            }
        }
    }

    for (const std::uint32_t invalidFreq : {0u, 8000u, 22050u, 32000u, 44100u, 96000u}) {
        Require(sceAudioInDeviceHqOpen(UserId, 0, 0, invalidFreq, 99) == ErrorInvalidFreq);
        Require(sceAudioInDeviceHqOpen(UserId, 1, 128, invalidFreq, 2) == ErrorInvalidFreq);
    }

    for (const std::uint32_t invalidLen : {0u, 64u, 96u, 255u, 512u}) {
        Require(sceAudioInDeviceHqOpen(UserId, 0, invalidLen, 48000, 2) == ErrorInvalidSize);
        Require(sceAudioInDeviceHqOpen(UserId, 2, invalidLen, 16000, 0) == ErrorInvalidSize);
    }

    for (const std::uint32_t invalidParam : {3u, 4u, 15u, 99u}) {
        Require(sceAudioInDeviceHqOpen(UserId, 0, 128, 48000, invalidParam) == ErrorInvalidParam);
    }
}

void TestPortExhaustionAndLifecycle() {
    std::vector<int> handles;

    for (std::uint32_t deviceId : {0u, 1u}) {
        const int handle = sceAudioInDeviceIdOpen(UserId, 0, deviceId, 128, 48000, 2);
        Require(handle >= static_cast<int>(HandleBase));
        handles.push_back(handle);
    }

    for (std::uint32_t deviceId : {2u, 3u}) {
        const int handle = sceAudioInDeviceIdAsyncOpen(UserId, 1, deviceId, 256, 16000, 0);
        Require(handle >= static_cast<int>(HandleBase));
        handles.push_back(handle);
    }

    for (std::uint32_t deviceId : {4u, 5u}) {
        const int handle = sceAudioInDeviceIdHqOpen(UserId, 2, deviceId, 128, 48000, 0x10u);
        Require(handle >= static_cast<int>(HandleBase));
        handles.push_back(handle);
    }

    for (std::uint32_t deviceId : {6u, 7u}) {
        const int handle = sceAudioInDeviceHqOpen(UserId, deviceId, 256, 48000, 2);
        Require(handle >= static_cast<int>(HandleBase));
        handles.push_back(handle);
    }

    Require(handles.size() == 8u);

    for (std::size_t i = 0; i < handles.size(); ++i) {
        for (std::size_t j = i + 1; j < handles.size(); ++j) {
            Require(handles[i] != handles[j]);
        }
    }

    Require(sceAudioInDeviceIdOpen(UserId, 6, 0, 128, 48000, 2) == ErrorInvalidType);
    Require(sceAudioInDeviceIdOpen(UserId, 0, 0, 128, 8000, 2) == ErrorInvalidFreq);
    Require(sceAudioInDeviceIdOpen(UserId, 0, 0, 64, 48000, 2) == ErrorInvalidSize);
    Require(sceAudioInDeviceIdOpen(UserId, 0, 0, 128, 48000, 99) == ErrorInvalidParam);

    Require(sceAudioInDeviceIdAsyncOpen(UserId, 6, 0, 128, 48000, 2) == ErrorInvalidType);
    Require(sceAudioInDeviceIdHqOpen(UserId, 6, 0, 128, 48000, 2) == ErrorInvalidType);
    Require(sceAudioInDeviceHqOpen(UserId, 0, 128, 8000, 2) == ErrorInvalidFreq);

    Require(sceAudioInDeviceIdOpen(UserId, 0, 0, 128, 48000, 2) == ErrorPortFull);
    Require(sceAudioInDeviceIdAsyncOpen(UserId, 0, 0, 128, 48000, 2) == ErrorPortFull);
    Require(sceAudioInDeviceIdHqOpen(UserId, 0, 0, 128, 48000, 2) == ErrorPortFull);
    Require(sceAudioInDeviceHqOpen(UserId, 0, 128, 48000, 2) == ErrorPortFull);
    Require(sceAudioInDeviceOpenEx(UserId, 0, 0, 0, 128, 48000, 2) == ErrorPortFull);

    for (const int handle : handles) {
        Require(sceAudioInInput(handle, nullptr) == 0);
        const int silentState = sceAudioInGetSilentState(handle);
        Require(silentState == 0 || silentState == 1);
        Require(sceAudioInGetRerouteCount(handle) == 0);
        Require(sceAudioInIsPadEmbeddedMic(handle) == 0);

        float gain = -1.0f;
        Require(sceAudioInGetGain(handle, &gain) == 0);
        Require(gain == 1.0f);
        Require(sceAudioInGetGain(handle, nullptr) == ErrorInvalidPointer);
    }

    const int released = handles.back();
    handles.pop_back();
    Require(sceAudioInClose(released) == 0);

    Require(sceAudioInClose(released) == ErrorInvalidState);
    Require(sceAudioInInput(released, nullptr) == ErrorInvalidState);
    Require(sceAudioInGetSilentState(released) == ErrorInvalidState);
    Require(sceAudioInGetRerouteCount(released) == ErrorInvalidState);

    const int reacquired = sceAudioInDeviceOpenEx(UserId, 3, 0x9999, 0, 128, 48000, 2);
    Require(reacquired == released);
    Require(sceAudioInDeviceIdOpen(UserId, 0, 0, 128, 48000, 2) == ErrorPortFull);

    Require(sceAudioInClose(reacquired) == 0);
    for (const int handle : handles) {
        Require(sceAudioInClose(handle) == 0);
    }
}

} // namespace

int main() {
    SetEnvironment("SDL_AUDIODRIVER", "dummy");
    Require(sceAudioInInit() == 0);

    TestPrecedenceAndBounds();
    TestPortExhaustionAndLifecycle();

    return 0;
}
