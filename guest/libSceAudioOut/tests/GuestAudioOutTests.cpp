#include "SceTypes.hpp"
#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

void TestInvalidSampleRates() {
    constexpr int errorInvalidParam = -2144993272;

    for (const int type : {0, 1, 2, 3, 4}) {
        for (const std::uint32_t len : {256u, 512u, 1024u}) {
            for (const std::uint32_t param : {0u, 1u, 2u}) {
                Require(sceAudioOutOpen(0x10000000, type, 0, len, 0u, param) == errorInvalidParam);
                Require(sceAudioOutOpen(0, type, 0, len, 0u, param) == errorInvalidParam);
                Require(sceAudioOutOpen(-1, type, 0, len, 0u, param) == errorInvalidParam);
            }
        }
    }
}

void TestInvalidVolumeSettings() {
    constexpr int errorInvalidPort = -2144993277;
    constexpr int errorInvalidPointer = -2144993276;

    int validVolumes[8] = {32768, 32768, 32768, 32768, 32768, 32768, 32768, 32768};

    Require(sceAudioOutSetVolume(-1, 1u, validVolumes) == errorInvalidPort);
    Require(sceAudioOutSetVolume(0, 1u, validVolumes) == errorInvalidPort);
    Require(sceAudioOutSetVolume(33, 1u, validVolumes) == errorInvalidPort);
    Require(sceAudioOutSetVolume(999, 1u, validVolumes) == errorInvalidPort);

    for (const int invalidHandle : {-100, -1, 0, 33, 64, 999}) {
        for (const std::uint32_t flag : {0u, 1u, 0xFFu, 0xFFFFFFFFu}) {
            Require(sceAudioOutSetVolume(invalidHandle, flag, validVolumes) == errorInvalidPort);
            Require(sceAudioOutSetVolume(invalidHandle, flag, nullptr) == errorInvalidPointer);
        }
    }

    Require(sceAudioOutSetVolume(1, 1u, nullptr) == errorInvalidPointer);
    Require(sceAudioOutSetVolume(1, 0u, nullptr) == errorInvalidPointer);
    Require(sceAudioOutSetVolume(1, 0xFFu, nullptr) == errorInvalidPointer);

    for (const int badVolume : {-1, -100, -32768, 32769, 65536, 0x7FFFFFFF}) {
        int badVolumes[8] = {badVolume, badVolume, badVolume, badVolume, badVolume, badVolume, badVolume, badVolume};
        for (const int invalidHandle : {-1, 0, 999}) {
            Require(sceAudioOutSetVolume(invalidHandle, 1u, badVolumes) == errorInvalidPort);
        }
    }
}

}

int main() {
    TestInvalidSampleRates();
    TestInvalidVolumeSettings();
    return 0;
}
