#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>
#include <vector>

extern "C" {
int APS5_VABI sceAudioOutOpen(int, int, int, std::uint32_t, std::uint32_t, std::uint32_t);
int APS5_VABI sceAudioOutClose(int);
int APS5_VABI sceAudioOutOutput(int, const void*);
int APS5_VABI sceAudioOutOutputs(AudioOutOutputParam*, std::uint32_t);
int APS5_VABI sceAudioOutGetLastOutputTime(int, std::uint64_t*);
std::uint64_t APS5_VABI sceKernelGetProcessTime();
}

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

namespace {

constexpr int user = 0x10000000;
constexpr int portTypeMain = 0;
constexpr int portTypeVibration = 10;
constexpr std::uint32_t formatS16Mono = 0;
constexpr std::uint32_t frames = 256;
constexpr std::uint32_t frequency = 48000;
constexpr std::uint64_t untouched = 0xA5A5A5A5A5A5A5A5ull;
constexpr int invalidPort = static_cast<int>(0x80260003);
constexpr int invalidPointer = static_cast<int>(0x80260004);

int Open(int type) {
    const int handle = sceAudioOutOpen(user, type, 0, frames, frequency, formatS16Mono);
    Require(handle > 0, "port must open");
    return handle;
}

std::uint64_t LastOutputTime(int handle) {
    std::uint64_t time = untouched;
    Require(sceAudioOutGetLastOutputTime(handle, &time) == 0, "an open port must report its last output time");
    return time;
}

void Pause() {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
}

std::uint64_t OutputAndCheck(int handle, const std::vector<std::int16_t>& block) {
    Pause();
    const std::uint64_t before = sceKernelGetProcessTime();
    Require(sceAudioOutOutput(handle, block.data()) == static_cast<int>(frames), "output must accept the block");
    const std::uint64_t after = sceKernelGetProcessTime();
    const std::uint64_t time = LastOutputTime(handle);
    Require(time >= before && time <= after, "last output time must be the process time of the output");
    return time;
}

void TestOutput(int type) {
    const std::vector<std::int16_t> block(frames, 256);
    const int handle = Open(type);
    Require(LastOutputTime(handle) == 0, "a port that never output must report 0");

    const std::uint64_t first = OutputAndCheck(handle, block);
    Pause();
    Require(LastOutputTime(handle) == first, "the time must not move without an output");

    const std::uint64_t second = OutputAndCheck(handle, block);
    Require(second > first, "a later output must report a later time");

    Pause();
    Require(sceAudioOutOutput(handle, nullptr) == static_cast<int>(frames), "waiting for the output must succeed");
    Require(LastOutputTime(handle) == second, "waiting without data must not move the time");

    Require(sceAudioOutClose(handle) == 0, "port must close");
}

void TestPortsAreIndependent() {
    const std::vector<std::int16_t> block(frames, 256);
    const int played = Open(portTypeMain);
    const int silent = Open(portTypeMain);
    Require(played != silent, "two open ports must have different handles");
    OutputAndCheck(played, block);
    Require(LastOutputTime(silent) == 0, "an output must not move the time of another port");
    Require(sceAudioOutClose(played) == 0, "port must close");
    Require(sceAudioOutClose(silent) == 0, "port must close");
}

void TestOutputs() {
    const std::vector<std::int16_t> block(frames, 256);
    const int first = Open(portTypeMain);
    const int second = Open(portTypeMain);
    const int waiting = Open(portTypeMain);
    AudioOutOutputParam params[3] = {{first, block.data()}, {second, block.data()}, {waiting, nullptr}};

    Pause();
    const std::uint64_t before = sceKernelGetProcessTime();
    Require(sceAudioOutOutputs(params, 3) == static_cast<int>(frames), "outputs must accept the blocks");
    const std::uint64_t after = sceKernelGetProcessTime();

    for (const int handle : {first, second}) {
        const std::uint64_t time = LastOutputTime(handle);
        Require(time >= before && time <= after, "outputs must set the time of every port that received data");
    }
    Require(LastOutputTime(waiting) == 0, "outputs must not move the time of a port that received no data");

    for (const int handle : {first, second, waiting}) Require(sceAudioOutClose(handle) == 0, "port must close");
}

void TestReopenedPort() {
    const std::vector<std::int16_t> block(frames, 256);
    const int handle = Open(portTypeMain);
    OutputAndCheck(handle, block);
    Require(sceAudioOutClose(handle) == 0, "port must close");
    const int reopened = Open(portTypeMain);
    Require(LastOutputTime(reopened) == 0, "a port opened after a close must not keep the time of the closed one");
    Require(sceAudioOutClose(reopened) == 0, "port must close");
}

void TestErrors() {
    const std::vector<std::int16_t> block(frames, 256);
    const int handle = Open(portTypeMain);
    const std::uint64_t time = OutputAndCheck(handle, block);
    Require(sceAudioOutGetLastOutputTime(handle, nullptr) == invalidPointer, "a null destination must be rejected");
    Require(LastOutputTime(handle) == time, "a rejected call must not move the time");

    std::uint64_t destination = untouched;
    Require(sceAudioOutGetLastOutputTime(0, &destination) == invalidPort, "handle 0 must be rejected");
    Require(sceAudioOutGetLastOutputTime(-1, &destination) == invalidPort, "a negative handle must be rejected");
    Require(sceAudioOutGetLastOutputTime(handle + 1, &destination) == invalidPort, "a port that is not open must be rejected");
    Require(sceAudioOutGetLastOutputTime(1000, &destination) == invalidPort, "an out of range handle must be rejected");
    Require(sceAudioOutClose(handle) == 0, "port must close");
    Require(sceAudioOutGetLastOutputTime(handle, &destination) == invalidPort, "a closed port must be rejected");
    Require(destination == untouched, "a rejected call must not write the destination");
}

}

int main() {
#ifdef _WIN32
    _putenv_s("SDL_AUDIODRIVER", "dummy");
#else
    setenv("SDL_AUDIODRIVER", "dummy", 1);
#endif
    sceKernelGetProcessTime();
    TestOutput(portTypeMain);
    TestOutput(portTypeVibration);
    TestPortsAreIndependent();
    TestOutputs();
    TestReopenedPort();
    TestErrors();
    return 0;
}
