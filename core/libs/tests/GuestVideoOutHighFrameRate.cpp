#include "prx/libSceVideoOut/include/Output.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <stdexcept>

static constexpr int Unavailable = static_cast<int>(0x80290019u);

static void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

static double Measure(int handle) {
    Require(sceVideoOutWaitVblank(handle) == 0, "initial vblank failed");
    const auto start = std::chrono::steady_clock::now();
    for (int frame = 0; frame < 60; ++frame) Require(sceVideoOutWaitVblank(handle) == 0, "vblank failed");
    return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
}

int main(int argc, char** argv) {
    const bool requireDisplay = argc > 1 && std::strcmp(argv[1], "--require-high-frame-rate") == 0;
    VideoOutTestEnvironment environment;
    {
        std::ofstream param("app0/sce_sys/param.json", std::ios::binary);
        param << R"({"titleId":"PPSA99007","localizedParameters":{"en-US":{"titleName":"High frame rate test"}},"attribute3":64})";
        Require(static_cast<bool>(param), "cannot write high-frame-rate fixture");
    }
    int handle;
    try {
        handle = sceVideoOutOpen(255, 0, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (requireDisplay) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }
    const int supported = sceVideoOutIsOutputSupported(handle, 15, nullptr, nullptr, 0);
    if (supported == 0 && !requireDisplay) {
        Require(sceVideoOutConfigureOutput(handle, 15, nullptr, nullptr, 0) == Unavailable, "an output without 120 Hz accepted high-frame-rate output");
        Require(sceVideoOutClose(handle) == 0, "close failed");
        LibcRunShutdown_nid_postfix();
        std::puts("skipped, current display does not provide 120 Hz");
        return 77;
    }
    Require(supported == 1, "120 Hz output not reported as supported");
    Require(sceVideoOutConfigureOutput(handle, 1, nullptr, nullptr, 0) == 0, "default mode failed");
    const double normal = Measure(handle);
    Require(sceVideoOutConfigureOutput(handle, 15, nullptr, nullptr, 0) == 0, "permitted high-frame-rate mode failed");
    VideoOutOutputStatus output{};
    VideoOutResolutionStatus resolution{};
    Require(sceVideoOutGetOutputStatus(handle, &output) == 0 && output.refreshRate == 13, "wrong output refresh rate");
    Require(sceVideoOutGetResolutionStatus(handle, &resolution) == 0 && resolution.refreshRate == 13, "wrong resolution refresh rate");
    const double fast = Measure(handle);
    Require(sceVideoOutConfigureOutput(handle, 1, nullptr, nullptr, 0) == 0, "default mode restore failed");
    Require(sceVideoOutGetOutputStatus(handle, &output) == 0 && output.refreshRate == 3, "default output refresh rate not restored");
    const double restored = Measure(handle);
    std::printf("60 vblanks: default=%.3f s high=%.3f s restored=%.3f s\n", normal, fast, restored);
    Require(normal > 0.8 && normal < 1.3 && fast > 0.4 && fast < 0.7 && restored > 0.8 && restored < 1.3, "incorrect vblank periods");
    Require(normal / fast > 1.7 && restored / fast > 1.7, "high-frame-rate mode did not double vblank frequency");
    Require(sceVideoOutClose(handle) == 0, "close failed");
    LibcRunShutdown_nid_postfix();
}
