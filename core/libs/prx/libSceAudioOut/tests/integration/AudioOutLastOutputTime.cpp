#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "AudioOutTestSupport.hpp"

#include <Testing/Test.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

extern "C" {
int APS5_VABI sceAudioOutOpen(int, int, int, std::uint32_t, std::uint32_t, std::uint32_t);
int APS5_VABI sceAudioOutClose(int);
int APS5_VABI sceAudioOutOutput(int, const void*);
int APS5_VABI sceAudioOutOutputs(AudioOutOutputParam*, std::uint32_t);
int APS5_VABI sceAudioOutGetPortState(int, AudioOutPortState*);
int APS5_VABI sceAudioOutGetLastOutputTime(int, std::uint64_t*);
std::uint64_t APS5_VABI sceKernelGetProcessTime();
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr int user = 0x10000000;
constexpr int portTypeMain = 0;
constexpr int portTypeVibration = 10;
constexpr std::uint32_t formatS16Mono = 0;
constexpr std::uint32_t frames = 256;
constexpr std::uint32_t frequency = 48000;
constexpr std::uint64_t untouched = 0xA5A5A5A5A5A5A5A5ull;
constexpr int invalidPort = static_cast<int>(0x80260003);
constexpr int invalidPointer = static_cast<int>(0x80260004);

class DummyAudio {
public:
    DummyAudio() {
        sceKernelGetProcessTime();
    }

private:
    const ScopedEnvironment driver{"SDL_AUDIODRIVER", "dummy"};
};

class Port {
public:
    explicit Port(int type, std::uint32_t length = frames) : handle(sceAudioOutOpen(user, type, 0, length, frequency, formatS16Mono)) {
        Require(handle > 0, "port must open: " + std::to_string(handle));
    }

    ~Port() {
        if (open) sceAudioOutClose(handle);
    }

    Port(const Port&) = delete;
    Port& operator=(const Port&) = delete;

    void Close() {
        RequireEqual(Release(), 0, "port must close");
    }

    int Release() {
        open = false;
        return sceAudioOutClose(handle);
    }

    const int handle;

private:
    bool open = true;
};

const std::vector<std::int16_t> block(frames, 256);

std::uint64_t LastOutputTime(int handle) {
    std::uint64_t time = untouched;
    RequireEqual(sceAudioOutGetLastOutputTime(handle, &time), 0, "an open port must report its last output time");
    return time;
}

void Pause() {
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
}

std::uint64_t OutputAndCheck(int handle) {
    Pause();
    const std::uint64_t before = sceKernelGetProcessTime();
    RequireEqual(sceAudioOutOutput(handle, block.data()), static_cast<int>(frames), "output must accept the block");
    const std::uint64_t after = sceKernelGetProcessTime();
    const std::uint64_t time = LastOutputTime(handle);
    Require(time >= before && time <= after, "last output time must be the process time of the output");
    return time;
}

void RequireOutputTimes(int type) {
    const DummyAudio audio;
    Port port(type);
    RequireEqual(LastOutputTime(port.handle), std::uint64_t{0}, "a port that never output must report 0");
    const std::uint64_t first = OutputAndCheck(port.handle);
    Pause();
    RequireEqual(LastOutputTime(port.handle), first, "the time must not move without an output");
    const std::uint64_t second = OutputAndCheck(port.handle);
    Require(second > first, "a later output must report a later time");
    Pause();
    RequireEqual(sceAudioOutOutput(port.handle, nullptr), static_cast<int>(frames), "waiting for the output must succeed");
    RequireEqual(LastOutputTime(port.handle), second, "waiting without data must not move the time");
    port.Close();
}

const Case mainPort{"GetLastOutputTime_MainPortOutputs_ReportsEachOutputTime", [] {
    RequireOutputTimes(portTypeMain);
}};

const Case vibrationPort{"GetLastOutputTime_VibrationPortOutputs_ReportsEachOutputTime", [] {
    RequireOutputTimes(portTypeVibration);
}};

const Case independent{"GetLastOutputTime_OtherPortOutputs_StaysZero", [] {
    const DummyAudio audio;
    Port played(portTypeMain);
    Port silent(portTypeMain);
    Require(played.handle != silent.handle, "two open ports must have different handles");
    OutputAndCheck(played.handle);
    RequireEqual(LastOutputTime(silent.handle), std::uint64_t{0}, "an output must not move the time of another port");
}};

const Case outputs{"Outputs_SeveralPorts_SetsTimeOnlyForPortsWithData", [] {
    const DummyAudio audio;
    Port first(portTypeMain);
    Port second(portTypeMain);
    Port waiting(portTypeMain);
    AudioOutOutputParam params[3] = {{first.handle, block.data()}, {second.handle, block.data()}, {waiting.handle, nullptr}};
    Pause();
    const std::uint64_t before = sceKernelGetProcessTime();
    RequireEqual(sceAudioOutOutputs(params, 3), static_cast<int>(frames), "outputs must accept the blocks");
    const std::uint64_t after = sceKernelGetProcessTime();
    for (const int handle : {first.handle, second.handle}) {
        const std::uint64_t time = LastOutputTime(handle);
        Require(time >= before && time <= after, "outputs must set the time of port " + std::to_string(handle));
    }
    RequireEqual(LastOutputTime(waiting.handle), std::uint64_t{0}, "outputs must not move the time of a port that received no data");
}};

const Case reopened{"GetLastOutputTime_ReopenedPort_StartsAtZero", [] {
    const DummyAudio audio;
    {
        Port port(portTypeMain);
        OutputAndCheck(port.handle);
        port.Close();
    }
    Port reopenedPort(portTypeMain);
    RequireEqual(LastOutputTime(reopenedPort.handle), std::uint64_t{0},
                 "a port opened after a close must not keep the time of the closed one");
}};

const Case errors{"GetLastOutputTime_InvalidHandleOrPointer_FailsWithoutWriting", [] {
    const DummyAudio audio;
    Port port(portTypeMain);
    const int handle = port.handle;
    const std::uint64_t time = OutputAndCheck(handle);
    RequireEqual(sceAudioOutGetLastOutputTime(handle, nullptr), invalidPointer, "a null destination must be rejected");
    RequireEqual(LastOutputTime(handle), time, "a rejected call must not move the time");
    std::uint64_t destination = untouched;
    RequireEqual(sceAudioOutGetLastOutputTime(0, &destination), invalidPort, "handle 0 must be rejected");
    RequireEqual(sceAudioOutGetLastOutputTime(-1, &destination), invalidPort, "a negative handle must be rejected");
    RequireEqual(sceAudioOutGetLastOutputTime(handle + 1, &destination), invalidPort, "a port that is not open must be rejected");
    RequireEqual(sceAudioOutGetLastOutputTime(1000, &destination), invalidPort, "an out of range handle must be rejected");
    port.Close();
    RequireEqual(sceAudioOutGetLastOutputTime(handle, &destination), invalidPort, "a closed port must be rejected");
    RequireEqual(destination, untouched, "a rejected call must not write the destination");
}};

const Case realTime{"Output_PortWithoutDevice_ConsumesBlocksInRealTime", [] {
    const DummyAudio audio;
    Port port(portTypeVibration);
    const std::uint64_t blockUs = 1000000ull * frames / frequency;
    const std::uint64_t start = sceKernelGetProcessTime();
    for (int i = 0; i < 7; i++) RequireEqual(sceAudioOutOutput(port.handle, block.data()), static_cast<int>(frames), "output block " + std::to_string(i));
    Require(sceKernelGetProcessTime() - start < 3 * blockUs, "a port without a device must take blocks up to its latency without waiting");
    for (int i = 7; i < 60; i++) RequireEqual(sceAudioOutOutput(port.handle, block.data()), static_cast<int>(frames), "output block " + std::to_string(i));
    const std::uint64_t elapsed = sceKernelGetProcessTime() - start;
    Require(elapsed + 50000 >= 60 * blockUs && elapsed < 60 * blockUs + 200000,
            "a port without a device must consume blocks in real time, took " + std::to_string(elapsed) + " us");
    RequireEqual(sceAudioOutOutput(port.handle, nullptr), static_cast<int>(frames), "waiting for the output must succeed");
    Require(sceKernelGetProcessTime() - start + 2000 >= 60 * blockUs, "waiting on a port without a device must last until its queue has played");
    port.Close();
}};

void RequireQueriesDuringOutput(bool batch, bool drain) {
    const DummyAudio audio;
    const std::vector<std::int16_t> second(frequency, 256);
    Port port(portTypeVibration, frequency);
    const int handle = port.handle;
    RequireEqual(sceAudioOutOutput(handle, second.data()), static_cast<int>(frequency), "output must accept the block");
    const auto initialTime = LastOutputTime(handle);
    std::atomic<bool> started = false;
    std::atomic<int> result = 0;
    std::thread output([&] {
        AudioOutOutputParam param{handle, drain ? nullptr : second.data()};
        started = true;
        result = batch ? sceAudioOutOutputs(&param, 1) : sceAudioOutOutput(handle, param.ptr);
    });
    while (!started) std::this_thread::yield();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const auto before = std::chrono::steady_clock::now();
    AudioOutPortState state{};
    const int stateResult = sceAudioOutGetPortState(handle, &state);
    std::uint64_t time = untouched;
    const int timeResult = sceAudioOutGetLastOutputTime(handle, &time);
    const auto queryDuration = std::chrono::steady_clock::now() - before;
    const int closeResult = port.Release();
    output.join();
    RequireEqual(closeResult, 0, "close must safely wait for pending output");
    RequireEqual(stateResult, 0, "pending output must allow port state queries");
    RequireEqual(timeResult, 0, "pending output must allow last output time queries");
    RequireEqual(time, initialTime, "pending output must not change its timestamp yet");
    Require(queryDuration < std::chrono::milliseconds(300), "audio queries must not wait for output pacing or draining");
    RequireEqual(result.load(), static_cast<int>(frequency), "pending output must complete before close");
}

const Case querySingle{"Queries_DuringPendingOutput_DoNotWait", [] {
    RequireQueriesDuringOutput(false, false);
}};

const Case querySingleDrain{"Queries_DuringPendingDrain_DoNotWait", [] {
    RequireQueriesDuringOutput(false, true);
}};

const Case queryBatch{"Queries_DuringPendingBatchOutput_DoNotWait", [] {
    RequireQueriesDuringOutput(true, false);
}};

const Case queryBatchDrain{"Queries_DuringPendingBatchDrain_DoNotWait", [] {
    RequireQueriesDuringOutput(true, true);
}};

} // namespace
