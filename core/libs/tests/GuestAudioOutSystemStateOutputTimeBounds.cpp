#include "prx/libc/include/Shutdown.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "prx/libSceAudioOut/include/AudioOut2.hpp"
#include "SceTypes.hpp"
#include "Testing.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

constexpr int ErrorInvalidPort = static_cast<int>(0x80260003);
constexpr int ErrorInvalidPointer = static_cast<int>(0x80260004);
constexpr int ErrorInvalidPortType = static_cast<int>(0x8026000A);
constexpr int ErrorAudioOut2InvalidArgument = static_cast<int>(0x80260502);
constexpr int ErrorAudioOut2NotInitialized = static_cast<int>(0x80268006);

constexpr int PortTypeMain = 0;
constexpr int PortTypeBgm = 1;
constexpr int PortTypeVoice = 2;
constexpr int PortTypePersonal = 3;
constexpr int PortTypePadSpk = 4;
constexpr int PortTypeVibration = 10;
constexpr int PortTypeAudio3D = 126;
constexpr int PortTypeAux = 127;

static_assert(sizeof(AudioOutSystemState) == 32);
static_assert(offsetof(AudioOutSystemState, loudness) == 0);
static_assert(offsetof(AudioOutSystemState, reserved8) == 4);
static_assert(offsetof(AudioOutSystemState, reserved) == 8);

} // namespace

int main() {
    AudioOut2SystemState systemState2{};
    Require(sceAudioOut2GetSystemStateEx(nullptr) == ErrorAudioOut2NotInitialized);
    Require(sceAudioOut2GetSystemStateEx(&systemState2) == ErrorAudioOut2NotInitialized);

    Require(sceAudioOutInit() == 0);

    Require(sceAudioOutGetSystemState(nullptr) == ErrorInvalidPointer);

    AudioOutSystemState systemState{};
    std::memset(&systemState, 0xEE, sizeof(systemState));
    Require(sceAudioOutGetSystemState(&systemState) == 0);
    Require(systemState.loudness == 0.0f);
    for (int i = 0; i < 4; ++i) {
        Require(systemState.reserved8[i] == 0);
    }
    for (int i = 0; i < 3; ++i) {
        Require(systemState.reserved[i] == 0);
    }
    const std::array<std::uint8_t, 32> expectedZeroState{};
    Require(std::memcmp(&systemState, expectedZeroState.data(), sizeof(systemState)) == 0);

    systemState.loudness = -24.0f;
    systemState.reserved8[1] = 0x5A;
    systemState.reserved[0] = 0x1234567890ABCDEFULL;
    Require(sceAudioOutGetSystemState(&systemState) == 0);
    Require(std::memcmp(&systemState, expectedZeroState.data(), sizeof(systemState)) == 0);

    Require(sceAudioOut2GetSystemState(nullptr) == ErrorAudioOut2InvalidArgument);

    systemState2.loudness = 5.0f;
    Require(sceAudioOut2GetSystemState(&systemState2) == 0);
    Require(systemState2.loudness == 0.0f);

    for (const int unopenedHandle : {-1, -100, 0, 1, 16, 32, 33, 9999}) {
        Require(sceAudioOutGetLastOutputTime(unopenedHandle, nullptr) == ErrorInvalidPointer);
    }

    std::uint64_t probeOutputTime = 0xAA55AA5511223344ULL;
    for (const int unopenedHandle : {-1, -100, 0, 33, 9999}) {
        Require(sceAudioOutGetLastOutputTime(unopenedHandle, &probeOutputTime) == ErrorInvalidPort);
        Require(probeOutputTime == 0xAA55AA5511223344ULL);
    }
    for (int unopenedHandle = 1; unopenedHandle <= 32; ++unopenedHandle) {
        Require(sceAudioOutGetLastOutputTime(unopenedHandle, &probeOutputTime) == ErrorInvalidPort);
        Require(probeOutputTime == 0xAA55AA5511223344ULL);
    }

    for (const int badType : {-1, 5, 6, 7, 8, 9, 11, 50, 100, 125, 128}) {
        Require(sceAudioOutOpen(0, badType, 0, 256, 48000, 0) == ErrorInvalidPortType);
    }

    std::array<std::int16_t, 512> pcmData{};
    pcmData.fill(100);

    for (const int validPortType : {PortTypeMain, PortTypeBgm, PortTypeVoice, PortTypePersonal, PortTypePadSpk, PortTypeVibration, PortTypeAudio3D, PortTypeAux}) {
        const int handle = sceAudioOutOpen(0, validPortType, 0, 256, 48000, 0);
        Require(handle > 0);

        std::uint64_t lastOutputTime = 0x1234ULL;
        Require(sceAudioOutGetLastOutputTime(handle, &lastOutputTime) == 0);
        Require(lastOutputTime == 0);

        Require(sceAudioOutOutput(handle, nullptr) == 256);
        Require(sceAudioOutGetLastOutputTime(handle, &lastOutputTime) == 0);
        Require(lastOutputTime == 0);

        const std::uint64_t tStart = sceKernelGetProcessTime();
        Require(sceAudioOutOutput(handle, pcmData.data()) == 256);
        const std::uint64_t tEnd = sceKernelGetProcessTime();

        Require(sceAudioOutGetLastOutputTime(handle, &lastOutputTime) == 0);
        Require(lastOutputTime >= tStart);
        Require(lastOutputTime <= tEnd);

        const std::uint64_t firstOutputTime = lastOutputTime;
        Require(sceAudioOutOutput(handle, pcmData.data()) == 256);
        Require(sceAudioOutGetLastOutputTime(handle, &lastOutputTime) == 0);
        Require(lastOutputTime >= firstOutputTime);

        Require(sceAudioOutClose(handle) == 0);
        Require(sceAudioOutGetLastOutputTime(handle, &lastOutputTime) == ErrorInvalidPort);
        Require(sceAudioOutGetLastOutputTime(handle, nullptr) == ErrorInvalidPointer);
    }

    const int h1 = sceAudioOutOpen(0, PortTypeMain, 0, 256, 48000, 0);
    const int h2 = sceAudioOutOpen(0, PortTypeBgm, 0, 256, 48000, 0);
    Require(h1 > 0 && h2 > 0);

    std::uint64_t time1 = 0x111ULL, time2 = 0x222ULL;
    Require(sceAudioOutGetLastOutputTime(h1, &time1) == 0);
    Require(sceAudioOutGetLastOutputTime(h2, &time2) == 0);
    Require(time1 == 0 && time2 == 0);

    std::array<AudioOutOutputParam, 2> batchParams{};
    batchParams[0].handle = h1;
    batchParams[0].ptr = pcmData.data();
    batchParams[1].handle = h2;
    batchParams[1].ptr = nullptr;

    const std::uint64_t batchStart = sceKernelGetProcessTime();
    Require(sceAudioOutOutputs(batchParams.data(), static_cast<std::uint32_t>(batchParams.size())) == 256);
    const std::uint64_t batchEnd = sceKernelGetProcessTime();

    Require(sceAudioOutGetLastOutputTime(h1, &time1) == 0);
    Require(time1 >= batchStart && time1 <= batchEnd);
    Require(sceAudioOutGetLastOutputTime(h2, &time2) == 0);
    Require(time2 == 0);

    batchParams[0].ptr = nullptr;
    batchParams[1].ptr = pcmData.data();
    const std::uint64_t prevTime1 = time1;
    const std::uint64_t batch2Start = sceKernelGetProcessTime();
    Require(sceAudioOutOutputs(batchParams.data(), static_cast<std::uint32_t>(batchParams.size())) == 256);
    const std::uint64_t batch2End = sceKernelGetProcessTime();

    Require(sceAudioOutGetLastOutputTime(h1, &time1) == 0);
    Require(time1 == prevTime1);
    Require(sceAudioOutGetLastOutputTime(h2, &time2) == 0);
    Require(time2 >= batch2Start && time2 <= batch2End);

    Require(sceAudioOutClose(h1) == 0);
    Require(sceAudioOutClose(h2) == 0);

    LibcRunShutdown_nid_postfix();
    return 0;
}
