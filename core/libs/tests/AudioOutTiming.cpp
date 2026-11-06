#include "SceTypes.hpp"
#include "prx/libkernel/Time/include/Time.hpp"

#include <array>
#include <chrono>
#include <cstdio>

extern "C" {
int APS5_VABI sceAudioOutOpen(int userId, int type, int index, std::uint32_t len, std::uint32_t freq, std::uint32_t param);
int APS5_VABI sceAudioOutClose(int handle);
int APS5_VABI sceAudioOutOutput(int handle, const void* ptr);
int APS5_VABI sceAudioOutOutputs(AudioOutOutputParam* param, std::uint32_t num);
}

int main() {
    // The first clock read must not wrap while initializing its origin.
    if (sceKernelGetProcessTime() >= 1000000ULL) return 1;
    constexpr std::uint32_t Frames = 256, Rate = 48000, Blocks = 96;
    std::array<std::int16_t, Frames * 2> samples{};
    // Vibration ports never open an SDL device: this exercises their fallback clock.
    // Simulate 2 ms of producer work per block; it must fit inside the block's deadline.
    for (const bool batch : {false, true}) {
        const int handle = sceAudioOutOpen(0, 10, 0, Frames, Rate, 1);
        if (handle < 1) return 1;
        AudioOutOutputParam param{handle, samples.data()};
        const auto output = [&] { return batch ? sceAudioOutOutputs(&param, 1) : sceAudioOutOutput(handle, samples.data()); };
        if (output() < 0) return 1;
        // Fill the virtual queue before measuring steady producer pacing.
        // Startup may enqueue up to the target latency without sleeping.
        for (unsigned warmup = 0; warmup < 32; ++warmup) {
            KernelTimespec work{};
            work.tv_nsec = 2000000;
            if (sceKernelNanosleep(&work, nullptr) != 0 || output() < 0) return 1;
        }
        const auto start = std::chrono::steady_clock::now();
        for (std::uint32_t block = 0; block < Blocks; ++block) {
            KernelTimespec work{};
            work.tv_nsec = 2000000;
            if (sceKernelNanosleep(&work, nullptr) != 0 || output() < 0) return 1;
        }
        const double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        const double expected = static_cast<double>(Blocks) * Frames / Rate;
        if (sceAudioOutClose(handle) != 0) return 1;
        std::printf("AudioOutTiming: %s %.3f s, expected %.3f s\n", batch ? "batch" : "single", elapsed, expected);
        if (elapsed < expected * 0.95 || elapsed > expected * 1.25) return 1;
    }
    return 0;
}
