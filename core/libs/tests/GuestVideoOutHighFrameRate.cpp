#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceAgcDriver/Execution/include/HostHdr.hpp"
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
    Require(sceVideoOutConfigureOutput(handle, 1, nullptr, nullptr, 0) == 0, "default mode failed");
    VideoOutOutputStatus baselineOutput{};
    VideoOutResolutionStatus baselineResolution{};
    Require(sceVideoOutGetOutputStatus(handle, &baselineOutput) == 0 && baselineOutput.refreshRate == 3, "wrong baseline output refresh rate");
    Require(sceVideoOutGetResolutionStatus(handle, &baselineResolution) == 0 && baselineResolution.refreshRate == 3, "wrong baseline resolution refresh rate");
    Require(baselineResolution.fullWidth == 1920 && baselineResolution.fullHeight == 1080, "wrong baseline dimensions");
    Require(sceVideoOutConfigureOutput(handle, 0, nullptr, nullptr, 0) == static_cast<int>(0x8029001Eu), "unknown mode 0 did not fail");
    Require(sceVideoOutConfigureOutput(handle, 4, nullptr, nullptr, 0) == static_cast<int>(0x80290016u), "unsupported mode 4 did not fail");
    Require(sceVideoOutConfigureOutput(handle, 1, nullptr, &baselineResolution, 0) == static_cast<int>(0x80290001u), "non-null reservedPtr in configure did not fail");
    Require(sceVideoOutConfigureOutput(handle, 1, nullptr, nullptr, 1) == static_cast<int>(0x80290001u), "non-zero reserved in configure did not fail");
    Require(sceVideoOutIsOutputSupported(handle, 1, nullptr, &baselineResolution, 0) == static_cast<int>(0x80290001u), "non-null reservedPtr in support check did not fail");
    Require(sceVideoOutIsOutputSupported(handle, 1, nullptr, nullptr, 1) == static_cast<int>(0x80290001u), "non-zero reserved in support check did not fail");
    Require(sceVideoOutConfigureOutput(handle, 15, nullptr, &baselineResolution, 0) == static_cast<int>(0x80290001u), "non-null reservedPtr in 120Hz configure did not fail");
    Require(sceVideoOutConfigureOutput(handle, 15, nullptr, nullptr, 1) == static_cast<int>(0x80290001u), "non-zero reserved in 120Hz configure did not fail");
    Require(sceVideoOutIsOutputSupported(handle, 15, nullptr, &baselineResolution, 0) == static_cast<int>(0x80290001u), "non-null reservedPtr in 120Hz support check did not fail");
    Require(sceVideoOutIsOutputSupported(handle, 15, nullptr, nullptr, 1) == static_cast<int>(0x80290001u), "non-zero reserved in 120Hz support check did not fail");
    if (supported == 0 && !requireDisplay) {
        AgcDriverSetHostDisplayHdr_nid_postfix(AgcDriver::DisplayHdrFromOutput(true, 1000.0f, 0.005f, 600.0f));
        AgcDriverSetSurfaceHdr10_nid_postfix(true);
        Require(sceVideoOutIsOutputSupported(handle, 15, nullptr, nullptr, 0) == 0, "120Hz output without display support was accepted under HDR10");
        Require(sceVideoOutConfigureOutput(handle, 15, nullptr, nullptr, 0) == Unavailable, "an output without 120 Hz accepted high-frame-rate output under HDR10");
        AgcDriverSetHostDisplayHdr_nid_postfix({});
        AgcDriverSetSurfaceHdr10_nid_postfix(false);
        Require(sceVideoOutConfigureOutput(handle, 15, nullptr, nullptr, 0) == Unavailable, "an output without 120 Hz accepted high-frame-rate output");
        Require(sceVideoOutClose(handle) == 0, "close failed");
        LibcRunShutdown_nid_postfix();
        std::puts("skipped, current display does not provide 120 Hz");
        return 77;
    }
    Require(supported == 1, "120 Hz output not reported as supported");
    const double normal = Measure(handle);
    Require(sceVideoOutConfigureOutput(handle, 15, nullptr, nullptr, 0) == 0, "permitted high-frame-rate mode failed");
    VideoOutOutputStatus output{};
    VideoOutResolutionStatus resolution{};
    Require(sceVideoOutGetOutputStatus(handle, &output) == 0 && output.refreshRate == 13, "wrong output refresh rate");
    Require(sceVideoOutGetResolutionStatus(handle, &resolution) == 0 && resolution.refreshRate == 13, "wrong resolution refresh rate");
    Require(resolution.fullWidth == 1920 && resolution.fullHeight == 1080, "wrong high-frame-rate dimensions");
    AgcDriverSetHostDisplayHdr_nid_postfix(AgcDriver::DisplayHdrFromOutput(true, 1000.0f, 0.005f, 600.0f));
    AgcDriverSetSurfaceHdr10_nid_postfix(true);
    VideoOutOutputStatus hdrOutput{};
    Require(sceVideoOutGetOutputStatus(handle, &hdrOutput) == 0, "failed to get HDR output status");
    Require(hdrOutput.dynamicRange == 2 && hdrOutput.refreshRate == 13, "120Hz under HDR10 did not report HDR dynamic range and 119.88Hz refresh rate");
    VideoOutResolutionStatus hdrResolution{};
    Require(sceVideoOutGetResolutionStatus(handle, &hdrResolution) == 0, "failed to get HDR resolution status");
    Require(hdrResolution.refreshRate == 13 && hdrResolution.fullWidth == 1920 && hdrResolution.fullHeight == 1080, "120Hz under HDR10 resolution status mismatch");
    AgcDriverSetHostDisplayHdr_nid_postfix({});
    AgcDriverSetSurfaceHdr10_nid_postfix(false);
    const double fast = Measure(handle);
    Require(sceVideoOutConfigureOutput(handle, 1, nullptr, nullptr, 0) == 0, "default mode restore failed");
    Require(sceVideoOutGetOutputStatus(handle, &output) == 0 && output.refreshRate == 3, "default output refresh rate not restored");
    Require(sceVideoOutGetResolutionStatus(handle, &resolution) == 0 && resolution.refreshRate == 3, "default resolution refresh rate not restored");
    const double restored = Measure(handle);
    std::printf("60 vblanks: default=%.3f s high=%.3f s restored=%.3f s\n", normal, fast, restored);
    Require(normal > 0.8 && normal < 1.3 && fast > 0.4 && fast < 0.7 && restored > 0.8 && restored < 1.3, "incorrect vblank periods");
    Require(normal / fast > 1.7 && restored / fast > 1.7, "high-frame-rate mode did not double vblank frequency");
    Require(sceVideoOutClose(handle) == 0, "close failed");
    LibcRunShutdown_nid_postfix();
}
