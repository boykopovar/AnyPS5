#include "prx/libSceAudioOut/include/AudioOut.hpp"
#include "SDL.h"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string_view>
#include <thread>

namespace {

constexpr std::uint32_t Grain = 256;
std::atomic<SDL_AudioDeviceID> nextDevice{2};
std::atomic<bool> blocked{false};
std::atomic<bool> released{false};
std::atomic<unsigned> clockQueries{0};
std::atomic<unsigned> clears{0};
std::array<std::atomic<unsigned>, 4> queued{};
std::array<std::atomic<bool>, 4> closed{};
std::mutex stateMutex;
std::condition_variable changed;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "AudioOut port locks: %s\n", message);
    std::abort();
}

bool WaitFor(const std::atomic<bool>& condition, std::chrono::milliseconds timeout) {
    std::unique_lock lock(stateMutex);
    return changed.wait_for(lock, timeout, [&] { return condition.load(std::memory_order_acquire); });
}

}

extern "C" std::uint64_t APS5_VABI sceKernelGetProcessTime(void) {
    clockQueries.fetch_add(1, std::memory_order_relaxed);
    return 1;
}

extern "C" int SDLCALL SDL_InitSubSystem(Uint32) { return 0; }
extern "C" Uint32 SDLCALL SDL_WasInit(Uint32) { return SDL_INIT_AUDIO; }
extern "C" void SDLCALL SDL_PauseAudioDevice(SDL_AudioDeviceID, int) {}
extern "C" SDL_AudioStatus SDLCALL SDL_GetAudioDeviceStatus(SDL_AudioDeviceID) { return SDL_AUDIO_PLAYING; }

extern "C" SDL_AudioDeviceID SDLCALL SDL_OpenAudioDevice(const char*, int, const SDL_AudioSpec* desired, SDL_AudioSpec* obtained, int) {
    *obtained = *desired;
    return nextDevice.fetch_add(1, std::memory_order_relaxed);
}

extern "C" Uint32 SDLCALL SDL_GetQueuedAudioSize(SDL_AudioDeviceID device) {
    Require(device == 2 || device == 3, "unknown device polled");
    if (device == 2 && !released.load(std::memory_order_acquire)) {
        blocked.store(true, std::memory_order_release);
        changed.notify_all();
        return 1u << 20;
    }
    return 0;
}

extern "C" void SDLCALL SDL_ClearQueuedAudio(SDL_AudioDeviceID device) {
    if (device == 2 && !released.load(std::memory_order_acquire)) {
        clears.fetch_add(1, std::memory_order_relaxed);
        released.store(true, std::memory_order_release);
    }
}

extern "C" int SDLCALL SDL_QueueAudio(SDL_AudioDeviceID device, const void* data, Uint32 bytes) {
    Require(device == 2 || device == 3, "unknown device queued");
    Require(!closed[device].load(std::memory_order_acquire), "output used a closed device");
    Require(bytes == Grain * sizeof(std::int16_t), "output size changed");
    const auto* samples = static_cast<const std::int16_t*>(data);
    const auto expected = device == 2 ? 1000 : 2000;
    for (std::uint32_t i = 0; i < Grain; ++i) Require(samples[i] == expected, "output samples changed");
    queued[device].fetch_add(1, std::memory_order_relaxed);
    return 0;
}

extern "C" void SDLCALL SDL_CloseAudioDevice(SDL_AudioDeviceID device) {
    Require(device == 2 || device == 3, "unknown device closed");
    Require(!closed[device].exchange(true, std::memory_order_acq_rel), "device closed twice");
}

int main(int argc, char** argv) {
    Require(argc == 2, "expected a test mode");
    const std::string_view mode(argv[1]);
    const bool unrelatedOutput = mode == "output";
    const bool unrelatedClose = mode == "close";
    const bool samePortClose = mode == "same-close";
    Require(unrelatedOutput || unrelatedClose || samePortClose, "unknown test mode");
    const int first = sceAudioOutOpen(0, 0, 0, Grain, 48000, 0);
    const int second = sceAudioOutOpen(0, 0, 0, Grain, 48000, 0);
    Require(first > 0 && second > 0 && first != second, "port creation failed");
    std::array<std::int16_t, Grain> firstSamples;
    std::array<std::int16_t, Grain> secondSamples;
    firstSamples.fill(1000);
    secondSamples.fill(2000);
    int firstResult = -1;
    int secondResult = -1;
    std::atomic<bool> secondStarted{false};
    std::atomic<bool> secondDone{false};
    std::thread producer([&] { firstResult = sceAudioOutOutput(first, firstSamples.data()); });
    Require(WaitFor(blocked, std::chrono::seconds(2)), "first port did not enter backpressure");
    std::thread other([&] {
        secondStarted.store(true, std::memory_order_release);
        changed.notify_all();
        secondResult = unrelatedOutput ? sceAudioOutOutput(second, secondSamples.data()) : sceAudioOutClose(samePortClose ? first : second);
        secondDone.store(true, std::memory_order_release);
        changed.notify_all();
    });
    Require(WaitFor(secondStarted, std::chrono::seconds(2)), "second operation did not start");
    const bool completedWhileBlocked = WaitFor(secondDone, std::chrono::milliseconds(samePortClose ? 150 : 2000));
    released.store(true, std::memory_order_release);
    producer.join();
    other.join();
    Require(clockQueries.load() != 0 && clears.load() == 0, "drain timeout invalidated the controlled wait");
    Require(firstResult == static_cast<int>(Grain), "blocked output failed");
    Require(secondResult == (unrelatedOutput ? static_cast<int>(Grain) : 0), "second operation failed");
    Require(completedWhileBlocked != samePortClose, "port output/close lock scope is wrong");
    Require(queued[2].load() == 1 && queued[3].load() == (unrelatedOutput ? 1u : 0u), "unexpected queued output count");
    if (!samePortClose) Require(sceAudioOutClose(first) == 0, "first port close failed");
    if (!unrelatedClose) Require(sceAudioOutClose(second) == 0, "second port close failed");
    Require(closed[2].load() && closed[3].load(), "a device remained open");
    return 0;
}
