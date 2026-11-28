#include "prx/libkernel/include/Api.hpp"
#include "prx/libSceVideoOut/include/Buffer.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/Event.hpp"
#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include "prx/libSceSystemService/include/SystemService.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "prx/libSceAgcDriver/Execution/include/HostHdr.hpp"
#include "prx/libSceSystemService/SystemService.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

constexpr int SystemUser = 255;
constexpr int MainBus = 0;
constexpr int OutputModeEvent = 8;

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "FAIL: %s\n", what);
    std::abort();
}

VideoOutOutputStatus Status(int handle) {
    VideoOutOutputStatus status{};
    Require(sceVideoOutGetOutputStatus(handle, &status) == 0, "sceVideoOutGetOutputStatus failed");
    std::array<std::byte, 80> withVrr{};
    Require(sceVideoOutGetOutputStatusWithVrr(handle, withVrr.data()) == 0, "sceVideoOutGetOutputStatusWithVrr failed");
    Require(std::memcmp(withVrr.data(), &status, sizeof(status)) == 0, "the output status with VRR differs from the output status");
    return status;
}

SystemServiceHdrToneMapLuminance ToneMap() {
    SystemServiceHdrToneMapLuminance luminance{-1.0f, -1.0f, -1.0f};
    Require(sceSystemServiceGetHdrToneMapLuminance(&luminance) == SYSTEM_SERVICE_OK, "sceSystemServiceGetHdrToneMapLuminance failed");
    return luminance;
}

bool OutputModeEventArrives(KernelEqueue queue, KernelUseconds timeout) {
    KernelEvent event{};
    int count = 0;
    return sceKernelWaitEqueue(queue, &event, 1, &count, &timeout) == 0 && count == 1 && sceVideoOutGetEventId(&event) == OutputModeEvent;
}

void ValidateLuminanceClampingBounds() {
    const auto qnan = std::numeric_limits<float>::quiet_NaN();
    const auto inf = std::numeric_limits<float>::infinity();

    const auto nanMax = AgcDriver::DisplayHdrFromOutput(true, qnan, 0.0f, 1000.0f);
    Require(nanMax.hdr && nanMax.maxLuminance == 1000.0f && nanMax.maxFullFrameLuminance == 1000.0f, "NaN max luminance did not fall back to undescribed display");

    const auto infMax = AgcDriver::DisplayHdrFromOutput(true, inf, 0.0f, 1000.0f);
    Require(infMax.hdr && infMax.maxLuminance == 1000.0f && infMax.maxFullFrameLuminance == 1000.0f, "inf max luminance did not fall back to undescribed display");

    const auto nanMin = AgcDriver::DisplayHdrFromOutput(true, 1000.0f, qnan, 800.0f);
    Require(nanMin.minLuminance == 0.0f, "NaN min luminance did not clamp to 0");

    const auto infMin = AgcDriver::DisplayHdrFromOutput(true, 1000.0f, inf, 800.0f);
    Require(infMin.minLuminance == 0.0f, "inf min luminance did not clamp to 0");

    const auto nanFull = AgcDriver::DisplayHdrFromOutput(true, 1000.0f, 0.005f, qnan);
    Require(nanFull.maxFullFrameLuminance == 1000.0f, "NaN full frame luminance did not clamp to max luminance");

    const auto infFull = AgcDriver::DisplayHdrFromOutput(true, 1000.0f, 0.005f, inf);
    Require(infFull.maxFullFrameLuminance == 1000.0f, "inf full frame luminance did not clamp to max luminance");

    const auto exactOne = AgcDriver::DisplayHdrFromOutput(true, 1.0f, 0.0f, 1.0f);
    Require(exactOne.hdr && exactOne.maxLuminance == 1.0f && exactOne.maxFullFrameLuminance == 1.0f, "exact 1.0f max luminance was not accepted");

    const auto subOne = AgcDriver::DisplayHdrFromOutput(true, 0.999f, 0.0f, 0.999f);
    Require(subOne.hdr && subOne.maxLuminance == 1000.0f && subOne.maxFullFrameLuminance == 1000.0f, "sub-1.0f max luminance did not fall back to undescribed display");

    const auto zeroMax = AgcDriver::DisplayHdrFromOutput(true, 0.0f, 0.0f, 0.0f);
    Require(zeroMax.hdr && zeroMax.maxLuminance == 1000.0f && zeroMax.maxFullFrameLuminance == 1000.0f, "zero max luminance did not fall back to undescribed display");

    AgcDriver::HostHdrStatus hostStatus{};
    hostStatus.available = true;
    hostStatus.display = AgcDriver::DisplayHdrFromOutput(true, 850.0f, 0.002f, 600.0f);
    hostStatus.reportedToneMapPeak = 0.0f;

    const float maxCllDisplay = AgcDriver::PqToSdrWhiteNits(hostStatus);
    Require(maxCllDisplay == 850.0f, "MaxCLL did not fall back to display peak luminance");

    hostStatus.reportedToneMapPeak = 500.0f;
    const float maxCllToneMap = AgcDriver::PqToSdrWhiteNits(hostStatus);
    Require(maxCllToneMap == 500.0f, "MaxCLL was not set from reported tone map peak");

    hostStatus.reportedToneMapPeak = 950.0f;
    const float maxCllHigh = AgcDriver::PqToSdrWhiteNits(hostStatus);
    Require(maxCllHigh == 950.0f, "high MaxCLL was not set from reported tone map peak");

    hostStatus.available = false;
    hostStatus.reportedToneMapPeak = 0.0f;
    const float sdrWhite = AgcDriver::PqToSdrWhiteNits(hostStatus);
    Require(sdrWhite == 100.0f, "SDR mode did not produce reference white for MaxCLL");
}

void ValidateHdrModeEnumerationAndNegativePointers(int handle) {
    constexpr int InvalidValue = static_cast<int>(0x80290001u);
    constexpr int InvalidAddress = static_cast<int>(0x80290002u);
    constexpr int UnsupportedOutputMode = static_cast<int>(0x80290016u);
    constexpr int UnknownOutputMode = static_cast<int>(0x8029001Eu);

    VideoOutOutputOptions options{};
    Require(sceVideoOutInitializeOutputOptions(nullptr) == InvalidAddress, "null options pointer was accepted by initialize");
    Require(sceVideoOutInitializeOutputOptions(&options) == 0, "failed to initialize output options");

    const int mode1Support = sceVideoOutIsOutputSupported(handle, 1, &options, nullptr, 0);
    Require(mode1Support == 1, "default 59.94Hz mode was not reported as supported under HDR");
    Require(sceVideoOutConfigureOutput(handle, 1, &options, nullptr, 0) == 0, "default mode configure failed under HDR");

    const int hfrSupport = sceVideoOutIsOutputSupported(handle, 15, &options, nullptr, 0);
    Require(hfrSupport == 0 || hfrSupport == 1, "120Hz mode support query returned invalid status");

    const int configureHfrResult = sceVideoOutConfigureOutput(handle, 15, &options, nullptr, 0);
    Require(configureHfrResult == UnsupportedOutputMode, "120Hz mode configure was accepted without attribute3 0x40");

    for (const uint64_t mode : {0ull, 2ull, 3ull, 5ull, 6ull, 9ull, 10ull, 11ull, 20ull}) {
        Require(sceVideoOutIsOutputSupported(handle, mode, &options, nullptr, 0) == UnknownOutputMode, "unknown mode was not rejected in support check");
        Require(sceVideoOutConfigureOutput(handle, mode, &options, nullptr, 0) == UnknownOutputMode, "unknown mode was not rejected in configure");
    }

    for (const uint64_t mode : {4ull, 7ull, 8ull, 12ull, 13ull, 14ull, 16ull, 17ull, 19ull}) {
        Require(sceVideoOutIsOutputSupported(handle, mode, &options, nullptr, 0) == UnsupportedOutputMode, "unsupported mode was not rejected in support check");
        Require(sceVideoOutConfigureOutput(handle, mode, &options, nullptr, 0) == UnsupportedOutputMode, "unsupported mode was not rejected in configure");
    }

    int dummy = 0;
    Require(sceVideoOutConfigureOutput(handle, 1, &options, &dummy, 0) == InvalidValue, "non-null reservedPtr in configure was accepted");
    Require(sceVideoOutConfigureOutput(handle, 1, &options, nullptr, 1) == InvalidValue, "non-zero reserved in configure was accepted");
    Require(sceVideoOutConfigureOutput(handle, 1, &options, &dummy, 1) == InvalidValue, "non-null reservedPtr and non-zero reserved in configure was accepted");
    Require(sceVideoOutIsOutputSupported(handle, 1, &options, &dummy, 0) == InvalidValue, "non-null reservedPtr in support check was accepted");
    Require(sceVideoOutIsOutputSupported(handle, 1, &options, nullptr, 1) == InvalidValue, "non-zero reserved in support check was accepted");
    Require(sceVideoOutIsOutputSupported(handle, 1, &options, &dummy, 1) == InvalidValue, "non-null reservedPtr and non-zero reserved in support check was accepted");

    VideoOutOutputOptions invalidOptions = options;
    invalidOptions.internalData[0] = 0;
    Require(sceVideoOutIsOutputSupported(handle, 1, &invalidOptions, nullptr, 0) == InvalidValue, "corrupted options word 0 was accepted in support check");
    Require(sceVideoOutConfigureOutput(handle, 1, &invalidOptions, nullptr, 0) == InvalidValue, "corrupted options word 0 was accepted in configure");

    for (size_t word = 1; word < 16; ++word) {
        if (word == 3) continue;
        invalidOptions = options;
        invalidOptions.internalData[word] = 1;
        Require(sceVideoOutIsOutputSupported(handle, 1, &invalidOptions, nullptr, 0) == InvalidValue, "corrupted options non-free word was accepted in support check");
        Require(sceVideoOutConfigureOutput(handle, 1, &invalidOptions, nullptr, 0) == InvalidValue, "corrupted options non-free word was accepted in configure");
    }

    VideoOutOutputOptions freeWordOptions = options;
    freeWordOptions.internalData[3] = 0xDEADBEEFu;
    Require(sceVideoOutIsOutputSupported(handle, 1, &freeWordOptions, nullptr, 0) == 1, "free word in options was rejected in support check");
    Require(sceVideoOutConfigureOutput(handle, 1, &freeWordOptions, nullptr, 0) == 0, "free word in options was rejected in configure");

    VideoOutColorSpaceConversionSettings csSettings{};
    auto expectRuntimeError = [](auto&& action, const char* what) {
        bool thrown = false;
        try {
            action();
        } catch (const std::runtime_error&) {
            thrown = true;
        }
        Require(thrown, what);
    };

    expectRuntimeError([&] { sceVideoOutAdjustColorSpaceConversion(handle, nullptr); }, "null settings pointer in adjust color space was accepted");
    expectRuntimeError([&] { sceVideoOutAdjustColorSpaceConversion_(handle, nullptr, sizeof(csSettings)); }, "null settings pointer in adjust color space sized was accepted");
    expectRuntimeError([&] { sceVideoOutAdjustColorSpaceConversion_(handle, &csSettings, sizeof(csSettings) - 1); }, "truncated size in adjust color space was accepted");
    expectRuntimeError([&] { sceVideoOutAdjustColorSpaceConversion(0, &csSettings); }, "handle 0 was accepted in adjust color space");
    expectRuntimeError([&] { sceVideoOutAdjustColorSpaceConversion(-1, &csSettings); }, "handle -1 was accepted in adjust color space");

    expectRuntimeError([] { sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion(nullptr); }, "null pointer in set default sdr to hdr was accepted");
    expectRuntimeError([&] { sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion_(nullptr, sizeof(csSettings)); }, "null pointer in set default sdr to hdr sized was accepted");
    expectRuntimeError([&] { sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion_(&csSettings, sizeof(csSettings) - 1); }, "truncated size in set default sdr to hdr was accepted");

    expectRuntimeError([] { sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion(nullptr, 1.0f, 0); }, "null pointer in set sdr to hdr was accepted");
    expectRuntimeError([&] { sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion_(nullptr, 1.0f, 0, sizeof(csSettings)); }, "null pointer in set sdr to hdr sized was accepted");
    expectRuntimeError([&] { sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion_(&csSettings, 1.0f, 0, sizeof(csSettings) - 1); }, "truncated size in set sdr to hdr was accepted");

    VideoOutColorSettings colorSettings{};
    expectRuntimeError([&] { sceVideoOutAdjustColor(handle, nullptr); }, "null color settings pointer was accepted");
    expectRuntimeError([&] { sceVideoOutAdjustColor_(handle, nullptr, sizeof(colorSettings)); }, "null color settings pointer sized was accepted");
    expectRuntimeError([&] { sceVideoOutAdjustColor_(handle, &colorSettings, sizeof(colorSettings) - 1); }, "truncated size in adjust color was accepted");
    expectRuntimeError([&] { sceVideoOutAdjustColor(0, &colorSettings); }, "handle 0 was accepted in adjust color");
    expectRuntimeError([&] { sceVideoOutAdjustColor(-1, &colorSettings); }, "handle -1 was accepted in adjust color");

    expectRuntimeError([] { sceVideoOutColorSettingsSetGamma(nullptr, 1.0f); }, "null pointer in set gamma was accepted");
    expectRuntimeError([&] { sceVideoOutColorSettingsSetGamma_(nullptr, 1.0f, sizeof(colorSettings)); }, "null pointer in set gamma sized was accepted");
    expectRuntimeError([&] { sceVideoOutColorSettingsSetGamma_(&colorSettings, 1.0f, sizeof(colorSettings) - 1); }, "truncated size in set gamma was accepted");
    expectRuntimeError([&] { sceVideoOutColorSettingsSetGamma(&colorSettings, 0.05f); }, "sub-0.1 gamma was accepted");
    expectRuntimeError([&] { sceVideoOutColorSettingsSetGamma(&colorSettings, 2.05f); }, "over-2.0 gamma was accepted");
    expectRuntimeError([&] { sceVideoOutColorSettingsSetGamma(&colorSettings, -1.0f); }, "negative gamma was accepted");
    expectRuntimeError([&] { sceVideoOutColorSettingsSetGamma(&colorSettings, std::numeric_limits<float>::quiet_NaN()); }, "NaN gamma was accepted");
    expectRuntimeError([&] { sceVideoOutColorSettingsSetGamma(&colorSettings, std::numeric_limits<float>::infinity()); }, "inf gamma was accepted");

    expectRuntimeError([] { sceVideoOutSetGlobalBlendSpace(0, 0, 0); }, "handle 0 was accepted in set global blend space");
    expectRuntimeError([] { sceVideoOutSetGlobalBlendSpace(-1, 0, 0); }, "handle -1 was accepted in set global blend space");
}

}

int main() {
    VideoOutTestEnvironment environment;
    int handle = 0;
    try {
        handle = sceVideoOutOpen(SystemUser, MainBus, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return 77;
    }
    Require(handle > 0, "sceVideoOutOpen failed");
    std::array<std::byte, 80> fresh{};
    Require(sceVideoOutGetOutputStatusWithVrr(handle, fresh.data()) == static_cast<int>(0x8029001Cu), "the output status with VRR of a handle without buffers is not refused");
    std::vector<std::byte> allocation(16u * 1024u * 1024u);
    const auto aligned = (reinterpret_cast<std::uintptr_t>(allocation.data()) + 65535u) & ~static_cast<std::uintptr_t>(65535u);
    VideoOutBuffers buffers{reinterpret_cast<void*>(aligned), nullptr, {nullptr, nullptr}};
    VideoOutBufferAttribute2 attribute{};
    attribute.width = 1920;
    attribute.height = 1080;
    attribute.pixel_format = 0x8000000022000000ull;
    Require(sceVideoOutRegisterBuffers2(handle, 0, 0, &buffers, 1, &attribute, 0, nullptr) == 0, "the buffer registration failed");
    AgcDriverSetHostDisplayHdr_nid_postfix({});
    AgcDriverSetSurfaceHdr10_nid_postfix(false);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    auto status = Status(handle);
    Require(status.dynamicRange == 1 && status.flags == 0, "an SDR host reports HDR output");
    Require(status.refreshRate == 3, "default SDR refresh rate must be 59.94Hz");
    auto luminance = ToneMap();
    Require(luminance.max_tone_map_luminance == 100.0f && luminance.max_full_frame_tone_map_luminance == 100.0f && luminance.min_tone_map_luminance == 0.0f, "an SDR host does not report SDR reference white");

    const auto nonHdr = AgcDriver::DisplayHdrFromOutput(false, 1000.0f, 0.001f, 800.0f);
    Require(!nonHdr.hdr && nonHdr.maxLuminance == 0.0f && nonHdr.minLuminance == 0.0f && nonHdr.maxFullFrameLuminance == 0.0f, "non-HDR color space must produce empty display HDR");

    const auto invalidMax = AgcDriver::DisplayHdrFromOutput(true, 0.5f, 0.0f, 0.5f);
    Require(invalidMax.hdr && invalidMax.maxLuminance == 1000.0f && invalidMax.minLuminance == 0.0f && invalidMax.maxFullFrameLuminance == 1000.0f, "invalid max luminance must fall back to UndescribedHdrDisplay");

    const auto negativeMax = AgcDriver::DisplayHdrFromOutput(true, -50.0f, 0.0f, 0.0f);
    Require(negativeMax.hdr && negativeMax.maxLuminance == 1000.0f && negativeMax.maxFullFrameLuminance == 1000.0f, "negative max luminance must fall back to UndescribedHdrDisplay");

    const auto badMin = AgcDriver::DisplayHdrFromOutput(true, 600.0f, 700.0f, 500.0f);
    Require(badMin.minLuminance == 0.0f, "min luminance >= max luminance must clamp to 0");

    const auto negativeMin = AgcDriver::DisplayHdrFromOutput(true, 600.0f, -0.5f, 500.0f);
    Require(negativeMin.minLuminance == 0.0f, "negative min luminance must clamp to 0");

    const auto overFull = AgcDriver::DisplayHdrFromOutput(true, 600.0f, 0.01f, 800.0f);
    Require(overFull.maxFullFrameLuminance == 600.0f, "full frame luminance > max luminance must clamp to max luminance");

    const auto underFull = AgcDriver::DisplayHdrFromOutput(true, 600.0f, 0.01f, 0.5f);
    Require(underFull.maxFullFrameLuminance == 600.0f, "full frame luminance < 1.0f must clamp to max luminance");

    ValidateLuminanceClampingBounds();

    KernelEqueue queue = 0;
    Require(sceKernelCreateEqueue(&queue, "HDR test") == 0, "event queue creation failed");
    Require(sceVideoOutAddOutputModeEvent(queue, handle, nullptr) == 0, "output mode subscription failed");
    Require(OutputModeEventArrives(queue, 0), "the output mode subscription is not triggered once");

    AgcDriverSetHostDisplayHdr_nid_postfix(AgcDriver::DisplayHdrFromOutput(true, 1015.0f, 0.005f, 640.0f));
    status = Status(handle);
    Require(status.dynamicRange == 1 && status.flags == 0, "an HDR10 display without an HDR10 surface reports HDR output");
    Require(ToneMap().max_tone_map_luminance == 100.0f, "an HDR10 display without an HDR10 surface reports its luminance");
    Require(!OutputModeEventArrives(queue, 200'000), "an HDR10 display without an HDR10 surface fires the output mode event");

    AgcDriverSetSurfaceHdr10_nid_postfix(true);
    Require(OutputModeEventArrives(queue, 2'000'000), "HDR output becoming available does not fire the output mode event");
    status = Status(handle);
    Require(status.dynamicRange == 2 && status.flags == 0, "HDR output is not reported with flags 0");
    Require(status.resolution != 0 && status.refreshRate == 3 && status.reserved[0] == 0 && status.reserved[1] == 0 && status.reserved[2] == 0, "the rest of the output status changed with HDR");
    luminance = ToneMap();
    Require(luminance.max_tone_map_luminance == 1015.0f && luminance.max_full_frame_tone_map_luminance == 640.0f && luminance.min_tone_map_luminance == 0.005f, "the display's luminance is not the tone-map luminance");
    Require(AgcDriver::CurrentHostHdr().reportedToneMapPeak == 1015.0f, "the tone-map peak given to the title is not recorded for the SDR conversion");

    AgcDriverSetHostDisplayHdr_nid_postfix(AgcDriver::DisplayHdrFromOutput(true, 400.0f, 0.05f, 300.0f));
    Require(OutputModeEventArrives(queue, 2'000'000), "a new display luminance does not fire the output mode event");
    Require(ToneMap().max_tone_map_luminance == 400.0f && Status(handle).dynamicRange == 2, "the new display luminance is not reported");

    AgcDriverSetHostDisplayHdr_nid_postfix(invalidMax);
    Require(OutputModeEventArrives(queue, 2'000'000), "an undescribed HDR display luminance does not fire the output mode event");
    Require(ToneMap().max_tone_map_luminance == 1000.0f && Status(handle).dynamicRange == 2, "the undescribed display luminance is not reported");
    VideoOutResolutionStatus hdrResolution{};
    Require(sceVideoOutGetResolutionStatus(handle, &hdrResolution) == 0, "sceVideoOutGetResolutionStatus failed in HDR mode");
    Require(hdrResolution.fullWidth == 1920 && hdrResolution.fullHeight == 1080, "resolution dimensions changed in HDR mode");
    Require(hdrResolution.refreshRate == 3, "resolution refresh rate changed in HDR mode");

    ValidateHdrModeEnumerationAndNegativePointers(handle);

    AgcDriverSetHostDisplayHdr_nid_postfix({});
    Require(OutputModeEventArrives(queue, 2'000'000), "HDR output going away does not fire the output mode event");
    status = Status(handle);
    Require(status.dynamicRange == 1 && status.flags == 0, "HDR output is reported after the display left HDR mode");
    Require(ToneMap().max_tone_map_luminance == 100.0f && AgcDriver::CurrentHostHdr().reportedToneMapPeak == 100.0f, "SDR reference white is not reported after the display left HDR mode");

    Require(sceVideoOutClose(handle) == 0, "sceVideoOutClose failed");
    Require(sceKernelDeleteEqueue(queue) == 0, "event queue deletion failed");
    std::printf("guest VideoOut HDR tests passed\n");
    LibcRunShutdown_nid_postfix();
    return 0;
}
