#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceVideoOut/include/Buffer.hpp"
#include "prx/libSceVideoOut/include/VideoOut.hpp"
#include "prx/libSceVideoOut/include/VideoOutDriver.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace {

static constexpr int SYSTEM_USER = 255;
static constexpr int MAIN_BUS = 0;
static constexpr int HMD2_MAIN_BUS = 0x20;

static constexpr int INVALID_VALUE = static_cast<int>(0x80290001);
static constexpr int INVALID_ADDRESS = static_cast<int>(0x80290002);
static constexpr int RESOURCE_BUSY = static_cast<int>(0x80290009);
static constexpr int INVALID_HANDLE = static_cast<int>(0x8029000B);
static constexpr int UNSUPPORTED_OUTPUT_MODE = static_cast<int>(0x80290016);
static constexpr int UNKNOWN_OUTPUT_MODE = static_cast<int>(0x8029001E);

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

template <typename F>
bool Throws(F&& action) {
    try {
        action();
        return false;
    } catch (const std::runtime_error&) {
        return true;
    }
}

void TestPortOpenParameterValidation() {
    Require(sceVideoOutOpen(SYSTEM_USER, HMD2_MAIN_BUS, 0, nullptr) == INVALID_VALUE);
    Require(sceVideoOutOpen(0, HMD2_MAIN_BUS, 0, nullptr) == INVALID_VALUE);

    Require(Throws([] { sceVideoOutOpen(-1, MAIN_BUS, 0, nullptr); }));
    Require(Throws([] { sceVideoOutOpen(1, MAIN_BUS, 0, nullptr); }));
    Require(Throws([] { sceVideoOutOpen(100, MAIN_BUS, 0, nullptr); }));

    Require(Throws([] { sceVideoOutOpen(SYSTEM_USER, -1, 0, nullptr); }));
    Require(Throws([] { sceVideoOutOpen(SYSTEM_USER, 3, 0, nullptr); }));
    Require(Throws([] { sceVideoOutOpen(SYSTEM_USER, 4, 0, nullptr); }));
    Require(Throws([] { sceVideoOutOpen(SYSTEM_USER, 99, 0, nullptr); }));

    Require(Throws([] { sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, -1, nullptr); }));
    Require(Throws([] { sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 1, nullptr); }));
    Require(Throws([] { sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 2, nullptr); }));

    VideoOutOpenParam badFirstWord{15, 0, 0, 0, 0};
    Require(Throws([&] { sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, &badFirstWord); }));

    VideoOutOpenParam badPriorityFlag{16, 2, 256, 0, 0};
    Require(Throws([&] { sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, &badPriorityFlag); }));

    VideoOutOpenParam badPriorityLow{16, 1, 255, 0, 0};
    Require(Throws([&] { sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, &badPriorityLow); }));

    VideoOutOpenParam badPriorityHigh{16, 1, 768, 0, 0};
    Require(Throws([&] { sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, &badPriorityHigh); }));

    VideoOutOpenParam badAffinityFlag{16, 0, 0, 2, 1};
    Require(Throws([&] { sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, &badAffinityFlag); }));

    VideoOutOpenParam badAffinityZero{16, 0, 0, 1, 0};
    Require(Throws([&] { sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, &badAffinityZero); }));

    VideoOutOpenParam badAffinityMask{16, 0, 0, 1, 0x2000};
    Require(Throws([&] { sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, &badAffinityMask); }));
}

void TestUnopenedPortOperations() {
    VideoOutOutputOptions options{};
    Require(sceVideoOutInitializeOutputOptions(&options) == 0);

    for (int unopened : {-1, 0, 1, 2, 0x7FFFFFFF}) {
        Require(sceVideoOutClose(unopened) == INVALID_HANDLE);
        Require(sceVideoOutIsOutputSupported(unopened, 1, &options, nullptr, 0) == INVALID_HANDLE);
        Require(sceVideoOutConfigureOutput(unopened, 1, &options, nullptr, 0) == INVALID_HANDLE);

        VideoOutFlipStatus flipStatus{};
        Require(sceVideoOutGetFlipStatus(unopened, &flipStatus) == INVALID_HANDLE);
        Require(sceVideoOutIsFlipPending(unopened) == INVALID_HANDLE);

        std::array<std::byte, 96> vrr{};
        Require(sceVideoOutGetOutputStatusWithVrr(unopened, vrr.data()) == INVALID_HANDLE);

        uintptr_t labelAddr = 0;
        Require(sceVideoOutGetBufferLabelAddress(unopened, &labelAddr) == INVALID_HANDLE);

        std::uint64_t cap = 0;
        Require(sceVideoOutGetDeviceCapabilityInfo_(unopened, &cap, sizeof(cap)) == INVALID_HANDLE);

        Require(Throws([&] { sceVideoOutSetFlipRate(unopened, 0); }));
        Require(Throws([&] { sceVideoOutSetWindowModeMargins(unopened, 0, 0); }));
        Require(Throws([&] { sceVideoOutAllowOutputResolutionWqhdDetection(unopened); }));
        Require(Throws([&] { sceVideoOutLatencyControlWaitBeforeInput(unopened); }));
        Require(Throws([&] { sceVideoOutLatencyMeasureSetStartPoint(unopened, 0); }));
        Require(Throws([&] { sceVideoOutVrrPegToFixedRate(unopened, 0, 0); }));
        Require(Throws([&] { sceVideoOutVrrUnpegFromFixedRate(unopened); }));

        VideoOutColorSettings color{};
        Require(Throws([&] { sceVideoOutAdjustColor(unopened, &color); }));

        VideoOutColorSpaceConversionSettings cs{};
        Require(Throws([&] { sceVideoOutAdjustColorSpaceConversion(unopened, &cs); }));
        Require(Throws([&] { sceVideoOutSetGlobalBlendSpace(unopened, 0, 0); }));

        VideoOutResolutionStatus res{};
        Require(Throws([&] { sceVideoOutGetResolutionStatus(unopened, &res); }));

        VideoOutOutputStatus out{};
        Require(Throws([&] { sceVideoOutGetOutputStatus(unopened, &out); }));

        VideoOutVblankStatus vb{};
        Require(Throws([&] { sceVideoOutGetVblankStatus(unopened, &vb); }));

        VideoOutBufferAttribute ba{};
        Require(Throws([&] { sceVideoOutSubmitChangeBufferAttribute(unopened, 0, &ba); }));

        VideoOutBufferAttribute2 ba2{};
        Require(Throws([&] { sceVideoOutSubmitChangeBufferAttribute2(unopened, 0, &ba2, nullptr); }));
    }
}

void TestOptionsAndCapabilityHelpers() {
    Require(sceVideoOutInitializeOutputOptions(nullptr) == INVALID_ADDRESS);

    VideoOutOutputOptions options{};
    std::memset(&options, 0xFF, sizeof(options));
    Require(sceVideoOutInitializeOutputOptions(&options) == 0);
    Require(options.internalData[0] == 0x00FF0000u);
    for (size_t i = 1; i < 16; ++i) {
        Require(options.internalData[i] == 0);
    }

    std::array<std::uint8_t, 16> modeBuf{};
    modeBuf.fill(0);
    Require(Throws([] { sceVideoOutModeSetAny_(nullptr, 16); }));

    sceVideoOutModeSetAny_(modeBuf.data(), 8);
    std::uint32_t reportedSize = 0;
    std::memcpy(&reportedSize, modeBuf.data(), sizeof(reportedSize));
    Require(reportedSize == 8);
    for (size_t i = 4; i < 8; ++i) {
        Require(modeBuf[i] == 0xFF);
    }
    for (size_t i = 8; i < 16; ++i) {
        Require(modeBuf[i] == 0);
    }
}

void TestColorAndGammaAttributes() {
    Require(Throws([] { sceVideoOutColorSettingsSetGamma(nullptr, 1.0f); }));

    VideoOutColorSettings color{};
    Require(Throws([&] { sceVideoOutColorSettingsSetGamma(&color, 0.05f); }));
    Require(Throws([&] { sceVideoOutColorSettingsSetGamma(&color, 2.5f); }));
    Require(Throws([&] { sceVideoOutColorSettingsSetGamma(&color, -1.0f); }));
    Require(Throws([&] { sceVideoOutColorSettingsSetGamma(&color, std::numeric_limits<float>::quiet_NaN()); }));
    Require(Throws([&] { sceVideoOutColorSettingsSetGamma(&color, std::numeric_limits<float>::infinity()); }));

    Require(sceVideoOutColorSettingsSetGamma(&color, 0.1f) == 0);
    Require(color.gamma == 0.1f);
    Require(sceVideoOutColorSettingsSetGamma(&color, 2.0f) == 0);
    Require(color.gamma == 2.0f);
    Require(sceVideoOutColorSettingsSetGamma(&color, 1.4f) == 0);
    Require(color.gamma == 1.4f);

    Require(Throws([&] { sceVideoOutColorSettingsSetGamma_(&color, 1.0f, sizeof(color) - 1); }));
    Require(sceVideoOutColorSettingsSetGamma_(&color, 1.2f, sizeof(color)) == 0);
    Require(color.gamma == 1.2f);
}

void TestColorSpaceConversionAttributes() {
    Require(Throws([] { sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion(nullptr); }));

    VideoOutColorSpaceConversionSettings cs{};
    std::memset(&cs, 0xCC, sizeof(cs));
    Require(sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion(&cs) == 0);
    for (size_t i = 0; i < 8; ++i) {
        Require(cs.internalData[i] == 0);
    }

    Require(Throws([&] { sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion_(&cs, sizeof(cs) - 1); }));
    Require(sceVideoOutColorSpaceConversionSettingsSetDefaultSdrToHdrConversion_(&cs, sizeof(cs)) == 0);

    Require(Throws([] { sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion(nullptr, 1.0f, 0); }));
    Require(sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion(&cs, 1.5f, 1) == 0);
    Require(Throws([&] { sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion_(&cs, 1.5f, 1, sizeof(cs) - 1); }));
    Require(sceVideoOutColorSpaceConversionSettingsSetSdrToHdrConversion_(&cs, 1.5f, 1, sizeof(cs)) == 0);
}

void TestBufferAttributeManipulation() {
    Require(Throws([] { sceVideoOutSetBufferAttribute(nullptr, 0x80000000u, 0, 0, 1920, 1080, 1920); }));

    VideoOutBufferAttribute attr{};
    sceVideoOutSetBufferAttribute(&attr, 0x80000000u, 1, 0, 1920, 1080, 1920);
    Require(attr.pixel_format == 0x80000000u);
    Require(attr.tiling_mode == 1);
    Require(attr.aspect_ratio == 0);
    Require(attr.width == 1920);
    Require(attr.height == 1080);
    Require(attr.pitch_in_pixel == 1920);
    Require(attr.option == 0);

    Require(Throws([] { sceVideoOutSetBufferAttribute2(nullptr, 0x8000000000000000ull, 0, 1920, 1080, 0, 0, 0); }));

    VideoOutBufferAttribute2 attr2{};
    sceVideoOutSetBufferAttribute2(&attr2, 0x8000000000000000ull, 0, 3840, 2160, 0, 0x10026c, 0);
    Require(attr2.pixel_format == 0x8000000000000000ull);
    Require(attr2.tiling_mode == 0);
    Require(attr2.aspect_ratio == 0);
    Require(attr2.width == 3840);
    Require(attr2.height == 2160);
    Require(attr2.pitch_in_pixel == 0);
    Require(attr2.option == 0);
    Require(attr2.dcc_control == 0x10026c);

    Require(Throws([] { sceVideoOutAddBufferAttributeOption(nullptr, 8); }));
    sceVideoOutAddBufferAttributeOption(&attr2, 8);
    Require(attr2.option == 8);

    Require(Throws([] { sceVideoOutRemoveBufferAttributeOption(nullptr, 8); }));
    sceVideoOutRemoveBufferAttributeOption(&attr2, 8);
    Require(attr2.option == 0);
}

void TestLivePortOperations(int handle) {
    Require(sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr) == RESOURCE_BUSY);

    std::uint64_t cap = 0;
    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, nullptr, sizeof(cap)) == INVALID_VALUE);
    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, &cap, sizeof(cap) - 1) == INVALID_VALUE);
    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, &cap, sizeof(cap) + 1) == INVALID_VALUE);
    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, &cap, sizeof(cap)) == 0);
    Require(cap == 1);

    uintptr_t labelAddr = 0;
    Require(sceVideoOutGetBufferLabelAddress(handle, nullptr) == INVALID_ADDRESS);
    Require(sceVideoOutGetBufferLabelAddress(handle, &labelAddr) == 16);
    Require(labelAddr != 0);

    VideoOutOutputOptions options{};
    Require(sceVideoOutInitializeOutputOptions(&options) == 0);
    Require(sceVideoOutIsOutputSupported(handle, 1, &options, nullptr, 0) == 1);
    Require(sceVideoOutConfigureOutput(handle, 1, &options, nullptr, 0) == 0);

    VideoOutOutputOptions badWord0 = options;
    badWord0.internalData[0] = 0;
    Require(sceVideoOutIsOutputSupported(handle, 1, &badWord0, nullptr, 0) == INVALID_VALUE);
    Require(sceVideoOutConfigureOutput(handle, 1, &badWord0, nullptr, 0) == INVALID_VALUE);

    for (size_t w = 1; w < 16; ++w) {
        if (w == 3) continue;
        VideoOutOutputOptions badW = options;
        badW.internalData[w] = 1;
        Require(sceVideoOutIsOutputSupported(handle, 1, &badW, nullptr, 0) == INVALID_VALUE);
        Require(sceVideoOutConfigureOutput(handle, 1, &badW, nullptr, 0) == INVALID_VALUE);
    }

    VideoOutOutputOptions freeWord = options;
    freeWord.internalData[3] = 0xCAFEBABE;
    Require(sceVideoOutIsOutputSupported(handle, 1, &freeWord, nullptr, 0) == 1);
    Require(sceVideoOutConfigureOutput(handle, 1, &freeWord, nullptr, 0) == 0);

    Require(sceVideoOutIsOutputSupported(handle, 1, &options, &options, 0) == INVALID_VALUE);
    Require(sceVideoOutIsOutputSupported(handle, 1, &options, nullptr, 1) == INVALID_VALUE);
    Require(sceVideoOutConfigureOutput(handle, 1, &options, &options, 0) == INVALID_VALUE);
    Require(sceVideoOutConfigureOutput(handle, 1, &options, nullptr, 1) == INVALID_VALUE);

    const int hfr = sceVideoOutIsOutputSupported(handle, 15, &options, nullptr, 0);
    Require(hfr == 0 || hfr == 1);
    Require(sceVideoOutConfigureOutput(handle, 15, &options, nullptr, 0) == UNSUPPORTED_OUTPUT_MODE);

    for (uint64_t mode : {4ull, 7ull, 8ull, 12ull, 13ull, 14ull, 16ull, 17ull, 19ull}) {
        Require(sceVideoOutIsOutputSupported(handle, mode, &options, nullptr, 0) == UNSUPPORTED_OUTPUT_MODE);
        Require(sceVideoOutConfigureOutput(handle, mode, &options, nullptr, 0) == UNSUPPORTED_OUTPUT_MODE);
    }

    for (uint64_t mode : {0ull, 2ull, 3ull, 5ull, 6ull, 9ull, 10ull, 11ull, 20ull, 0x100ull, 0xD000000Aull}) {
        Require(sceVideoOutIsOutputSupported(handle, mode, &options, nullptr, 0) == UNKNOWN_OUTPUT_MODE);
        Require(sceVideoOutConfigureOutput(handle, mode, &options, nullptr, 0) == UNKNOWN_OUTPUT_MODE);
    }

    Require(Throws([&] { sceVideoOutIsOutputSupported(handle, 18, &options, nullptr, 0); }));
    Require(Throws([&] { sceVideoOutConfigureOutput(handle, 18, &options, nullptr, 0); }));

    VideoOutResolutionStatus res{};
    Require(sceVideoOutGetResolutionStatus(handle, &res) == 0);
    Require(res.fullWidth == 1920 && res.fullHeight == 1080);
    Require(res.paneWidth == 1920 && res.paneHeight == 1080);
    Require(res.refreshRate == 3);
    Require(res.screenSizeInInch == 0.0f && res.flags == 0 && res.reserved0 == 0);
    Require(res.reserved1[0] == 0 && res.reserved1[1] == 0 && res.reserved1[2] == 0);

    VideoOutOutputStatus out{};
    Require(sceVideoOutGetOutputStatus(handle, &out) == 0);
    Require(out.refreshRate == res.refreshRate);
    Require(out.refreshRate == 3);
    Require(out.resolution == 1 || out.resolution == 2);
    Require(out.dynamicRange == 1 || out.dynamicRange == 2);
    Require(out.flags == 0);

    std::array<std::byte, 96> vrr{};
    vrr.fill(std::byte{0xEE});
    Require(sceVideoOutGetOutputStatusWithVrr(handle, vrr.data()) == 0);
    Require(std::memcmp(vrr.data(), &out, sizeof(out)) == 0);
    for (size_t i = sizeof(out); i < 80; ++i) {
        Require(vrr[i] == std::byte{0});
    }
    for (size_t i = 80; i < vrr.size(); ++i) {
        Require(vrr[i] == std::byte{0xEE});
    }
    Require(sceVideoOutGetOutputStatusWithVrr(handle, nullptr) == INVALID_ADDRESS);

    Require(sceVideoOutSetFlipRate(handle, -1) == INVALID_VALUE);
    Require(sceVideoOutSetFlipRate(handle, 3) == INVALID_VALUE);
    Require(sceVideoOutSetFlipRate(handle, 100) == INVALID_VALUE);
    Require(sceVideoOutSetFlipRate(handle, 0) == 0);
    Require(sceVideoOutSetFlipRate(handle, 1) == 0);
    Require(sceVideoOutSetFlipRate(handle, 2) == 0);

    Require(sceVideoOutAllowOutputResolutionWqhdDetection(handle) == 0);
    Require(sceVideoOutSetWindowModeMargins(handle, 0, 0) == 0);
    Require(sceVideoOutLatencyControlWaitBeforeInput(handle) == 0);
    Require(sceVideoOutLatencyMeasureSetStartPoint(handle, 0) == 0);
    Require(sceVideoOutAddVrrStatusFlagsPrivilege() == 0);
    Require(sceVideoOutVrrPegToFixedRate(handle, 0, 0) == 0);
    Require(Throws([&] { sceVideoOutVrrPegToFixedRate(handle, 1, 0); }));
    Require(Throws([&] { sceVideoOutVrrPegToFixedRate(handle, 0, 1); }));
    Require(sceVideoOutVrrUnpegFromFixedRate(handle) == 0);

    VideoOutColorSettings color{};
    color.gamma = 1.2f;
    Require(sceVideoOutAdjustColor(handle, &color) == 0);
    Require(sceVideoOutAdjustColor_(handle, &color, sizeof(color)) == 0);
    Require(Throws([&] { sceVideoOutAdjustColor_(handle, &color, sizeof(color) - 1); }));

    VideoOutColorSpaceConversionSettings cs{};
    Require(sceVideoOutAdjustColorSpaceConversion(handle, &cs) == 0);
    Require(sceVideoOutAdjustColorSpaceConversion_(handle, &cs, sizeof(cs)) == 0);
    Require(Throws([&] { sceVideoOutAdjustColorSpaceConversion_(handle, &cs, sizeof(cs) - 1); }));
    Require(sceVideoOutSetGlobalBlendSpace(handle, 0, 0) == 0);

    Require(Throws([&] { sceVideoOutGetResolutionStatus(handle, nullptr); }));
    Require(Throws([&] { sceVideoOutGetOutputStatus(handle, nullptr); }));
    Require(Throws([&] { sceVideoOutGetVblankStatus(handle, nullptr); }));
    Require(sceVideoOutGetFlipStatus(handle, nullptr) == INVALID_ADDRESS);

    Require(sceVideoOutClose(handle) == 0);
    Require(sceVideoOutClose(handle) == INVALID_HANDLE);
    Require(sceVideoOutIsOutputSupported(handle, 1, &options, nullptr, 0) == INVALID_HANDLE);
    Require(sceVideoOutConfigureOutput(handle, 1, &options, nullptr, 0) == INVALID_HANDLE);
    Require(sceVideoOutGetOutputStatusWithVrr(handle, vrr.data()) == INVALID_HANDLE);

    VideoOutFlipStatus postFlip{};
    Require(sceVideoOutGetFlipStatus(handle, &postFlip) == INVALID_HANDLE);
    Require(sceVideoOutIsFlipPending(handle) == INVALID_HANDLE);

    uintptr_t postLabel = 0;
    Require(sceVideoOutGetBufferLabelAddress(handle, &postLabel) == INVALID_HANDLE);

    std::uint64_t postCap = 0;
    Require(sceVideoOutGetDeviceCapabilityInfo_(handle, &postCap, sizeof(postCap)) == INVALID_HANDLE);

    Require(Throws([&] { sceVideoOutGetResolutionStatus(handle, &res); }));
    Require(Throws([&] { sceVideoOutGetOutputStatus(handle, &out); }));
}

} // namespace

int main() {
    VideoOutTestEnvironment environment;

    TestPortOpenParameterValidation();
    TestUnopenedPortOperations();
    TestOptionsAndCapabilityHelpers();
    TestColorAndGammaAttributes();
    TestColorSpaceConversionAttributes();
    TestBufferAttributeManipulation();

    int handle = 0;
    try {
        handle = sceVideoOutOpen(SYSTEM_USER, MAIN_BUS, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        LibcRunShutdown_nid_postfix();
        return 77;
    }

    Require(handle > 0);
    TestLivePortOperations(handle);

    LibcRunShutdown_nid_postfix();
    return 0;
}
