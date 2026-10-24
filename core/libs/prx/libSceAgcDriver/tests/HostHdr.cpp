#include "prx/libSceAgcDriver/Execution/include/DisplayBuffer.hpp"
#include "prx/libSceAgcDriver/Execution/include/DisplayFormat.hpp"
#include "prx/libSceAgcDriver/Execution/include/HostHdr.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>

namespace {

int failures = 0;

void Expect(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
}

void resetHost() {
    AgcDriverSetHostDisplayHdr_nid_postfix({});
    AgcDriverSetSurfaceHdr10_nid_postfix(false);
    AgcDriverNoteReportedToneMapPeak_nid_postfix(0.0f);
}

void dynamicRangeChoice() {
    using AgcDriver::DynamicRangeSurfaceFormat;
    constexpr std::uint64_t rgbPq = 0x8100070422000000ull;
    constexpr std::uint64_t bgrPq = 0x8100070400000000ull;
    const std::array<VkSurfaceFormatKHR, 3> hdr{{{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}, {VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT}, {VK_FORMAT_A2R10G10B10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT}}};
    const auto sdr = [](VkSurfaceFormatKHR format) { return format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR; };
    Expect(sdr(DynamicRangeSurfaceFormat(hdr, rgbPq, false)), "a PQ buffer without HDR output does not get the SDR swapchain");
    for (const auto pixelFormat : {0x8000000000000000ull, 0x8100000022000000ull, 0xc001000622000000ull}) Expect(sdr(DynamicRangeSurfaceFormat(hdr, pixelFormat, true)), "an SDR buffer with HDR output does not get the SDR swapchain");
    const auto red = DynamicRangeSurfaceFormat(hdr, rgbPq, true);
    const auto blue = DynamicRangeSurfaceFormat(hdr, bgrPq, true);
    Expect(red.format == VK_FORMAT_A2B10G10R10_UNORM_PACK32 && red.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT && blue.format == VK_FORMAT_A2R10G10B10_UNORM_PACK32 && blue.colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT, "a PQ buffer with HDR output does not get the HDR10 swapchain in its own order");
    bool refused = false;
    try {
        static_cast<void>(DynamicRangeSurfaceFormat(std::span(hdr).first(1), rgbPq, true));
    } catch (const std::exception&) {
        refused = true;
    }
    Expect(refused, "HDR output without an HDR10 surface format is presented SDR silently");
    const std::uint32_t word = 600u | (500u << 10u) | (300u << 20u) | (3u << 30u);
    const auto low = AgcDriver::PqClearColor(word, true);
    const auto high = AgcDriver::PqClearColor(word, false);
    Expect(low == std::array<float, 4>{600.0f / 1023.0f, 500.0f / 1023.0f, 300.0f / 1023.0f, 1.0f} && high == std::array<float, 4>{300.0f / 1023.0f, 500.0f / 1023.0f, 600.0f / 1023.0f, 1.0f}, "a PQ clear texel does not give its 10-bit codes in RGBA order");
}

void displayDescription() {
    using AgcDriver::DisplayHdrFromOutput;
    const auto hdr = DisplayHdrFromOutput(true, 1015.0f, 0.005f, 640.0f);
    Expect(hdr.hdr && hdr.maxLuminance == 1015.0f && hdr.minLuminance == 0.005f && hdr.maxFullFrameLuminance == 640.0f, "an HDR10 output's luminance is not kept");
    Expect(!DisplayHdrFromOutput(false, 1015.0f, 0.005f, 640.0f).hdr, "an SDR output is described as HDR");
    for (const float peak : {0.0f, 0.5f, std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity()}) {
        const auto undescribed = DisplayHdrFromOutput(true, peak, 0.005f, 640.0f);
        Expect(undescribed.hdr && undescribed.maxLuminance == 1000.0f && undescribed.minLuminance == 0.0f && undescribed.maxFullFrameLuminance == 1000.0f, "an HDR10 output without a usable peak luminance does not get the default luminance");
    }
    const auto partial = DisplayHdrFromOutput(true, 600.0f, -1.0f, 0.0f);
    Expect(partial.hdr && partial.minLuminance == 0.0f && partial.maxFullFrameLuminance == 600.0f, "an unusable black level or full-frame peak is kept");
    const auto inverted = DisplayHdrFromOutput(true, 400.0f, 500.0f, 900.0f);
    Expect(inverted.hdr && inverted.minLuminance == 0.0f && inverted.maxFullFrameLuminance == 400.0f, "a black level or full-frame peak above the peak is kept");
}

void surfaceFormats() {
    using AgcDriver::Hdr10SurfaceFormat;
    const std::array<VkSurfaceFormatKHR, 2> sdr{{{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}, {VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}}};
    Expect(!Hdr10SurfaceFormat(sdr, VK_FORMAT_A2B10G10R10_UNORM_PACK32), "an SDR surface offers HDR10");
    const std::array<VkSurfaceFormatKHR, 1> floatOnly{{{VK_FORMAT_R16G16B16A16_SFLOAT, VK_COLOR_SPACE_HDR10_ST2084_EXT}}};
    Expect(!Hdr10SurfaceFormat(floatOnly, VK_FORMAT_UNDEFINED), "a float HDR10 surface offers a 10-bit HDR10 swapchain");
    const std::array<VkSurfaceFormatKHR, 3> both{{{VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}, {VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT}, {VK_FORMAT_A2R10G10B10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT}}};
    for (const auto preferred : {VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_A2R10G10B10_UNORM_PACK32}) {
        const auto chosen = Hdr10SurfaceFormat(both, preferred);
        Expect(chosen && chosen->format == preferred && chosen->colorSpace == VK_COLOR_SPACE_HDR10_ST2084_EXT, "the display buffer's own 10-bit order is not preferred");
    }
    const auto first = Hdr10SurfaceFormat(both, VK_FORMAT_B8G8R8A8_UNORM);
    Expect(first && first->format == VK_FORMAT_A2B10G10R10_UNORM_PACK32, "a preferred format that is not 10-bit HDR10 is chosen");
    const std::array<VkSurfaceFormatKHR, 1> blueLow{{{VK_FORMAT_A2R10G10B10_UNORM_PACK32, VK_COLOR_SPACE_HDR10_ST2084_EXT}}};
    const auto only = Hdr10SurfaceFormat(blueLow, VK_FORMAT_A2B10G10R10_UNORM_PACK32);
    Expect(only && only->format == VK_FORMAT_A2R10G10B10_UNORM_PACK32, "the only 10-bit HDR10 format is not chosen");
}

void whiteLevels() {
    AgcDriver::HostHdrStatus status{};
    Expect(AgcDriver::ToneMapPeakNits(status) == 100.0f && AgcDriver::PqToSdrWhiteNits(status) == 100.0f, "an SDR host does not use SDR reference white");
    status.available = true;
    status.display = AgcDriver::DisplayHdrFromOutput(true, 800.0f, 0.01f, 400.0f);
    Expect(AgcDriver::ToneMapPeakNits(status) == 800.0f && AgcDriver::PqToSdrWhiteNits(status) == 800.0f, "an HDR host that was never asked does not use the display's peak");
    status.available = false;
    status.reportedToneMapPeak = 800.0f;
    Expect(AgcDriver::ToneMapPeakNits(status) == 100.0f && AgcDriver::PqToSdrWhiteNits(status) == 800.0f, "the peak the title was told is not the white level after HDR went away");
}

void hostState() {
    resetHost();
    const auto initial = AgcDriver::CurrentHostHdr();
    Expect(!initial.available && !initial.display.hdr && !initial.surfaceHdr10 && initial.reportedToneMapPeak == 0.0f, "HDR output is available after the host was reset");
    AgcDriverSetSurfaceHdr10_nid_postfix(true);
    auto status = AgcDriver::CurrentHostHdr();
    Expect(!status.available && status.generation == initial.generation, "a surface offering HDR10 makes HDR output available on an SDR display");
    AgcDriverSetHostDisplayHdr_nid_postfix(AgcDriver::DisplayHdrFromOutput(true, 800.0f, 0.01f, 400.0f));
    status = AgcDriver::CurrentHostHdr();
    Expect(status.available && status.generation != initial.generation && status.displayGeneration != initial.displayGeneration, "an HDR10 display and surface do not make HDR output available");
    Expect(status.display.maxLuminance == 800.0f && status.display.minLuminance == 0.01f && status.display.maxFullFrameLuminance == 400.0f, "the display's luminance is not published");
    const auto available = status.generation;
    AgcDriverSetHostDisplayHdr_nid_postfix(AgcDriver::DisplayHdrFromOutput(true, 800.0f, 0.01f, 400.0f));
    Expect(AgcDriver::CurrentHostHdr().generation == available, "an unchanged display report changes the generation");
    AgcDriverSetHostDisplayHdr_nid_postfix(AgcDriver::DisplayHdrFromOutput(true, 1000.0f, 0.01f, 400.0f));
    status = AgcDriver::CurrentHostHdr();
    Expect(status.available && status.generation != available && status.display.maxLuminance == 1000.0f, "a new peak luminance is not announced");
    const auto moved = status.generation;
    AgcDriverSetSurfaceHdr10_nid_postfix(false);
    status = AgcDriver::CurrentHostHdr();
    Expect(!status.available && status.generation != moved, "a surface without HDR10 leaves HDR output available");
    const auto lost = status.generation;
    AgcDriverSetHostDisplayHdr_nid_postfix(AgcDriver::DisplayHdrFromOutput(true, 600.0f, 0.01f, 400.0f));
    status = AgcDriver::CurrentHostHdr();
    Expect(!status.available && status.generation == lost && status.displayGeneration != initial.displayGeneration, "a display change without HDR output is announced as an output change");
    AgcDriverSetSurfaceHdr10_nid_postfix(true);
    AgcDriverSetHostDisplayHdr_nid_postfix({});
    Expect(!AgcDriver::CurrentHostHdr().available, "a display switched to SDR leaves HDR output available");
}

void forceSdr() {
    resetHost();
#ifdef _WIN32
    _putenv_s("APS5_FORCE_SDR", "1");
#else
    setenv("APS5_FORCE_SDR", "1", 1);
#endif
    AgcDriverSetSurfaceHdr10_nid_postfix(true);
    AgcDriverSetHostDisplayHdr_nid_postfix(AgcDriver::DisplayHdrFromOutput(true, 800.0f, 0.01f, 400.0f));
    const auto forced = AgcDriver::CurrentHostHdr();
    Expect(!forced.available && forced.display.hdr && forced.surfaceHdr10, "forcing SDR does not suppress HDR availability while preserving host capabilities");
    Expect(AgcDriver::ToneMapPeakNits(forced) == 100.0f, "forcing SDR does not report SDR reference white");
#ifdef _WIN32
    _putenv_s("APS5_FORCE_SDR", "");
#else
    unsetenv("APS5_FORCE_SDR");
#endif
    AgcDriverSetSurfaceHdr10_nid_postfix(true);
    const auto restored = AgcDriver::CurrentHostHdr();
    Expect(restored.available && restored.generation != forced.generation, "removing the SDR override does not restore HDR availability");
    resetHost();
}

void reportedWhiteLevel() {
    resetHost();
    constexpr std::uint64_t rgbPq = 0x8100070422000000ull;
    const std::uint32_t word = 700u | (700u << 10u) | (700u << 20u) | (3u << 30u);
    std::array<std::byte, 4> source{};
    std::memcpy(source.data(), &word, 4);
    const AgcDriver::DisplayBuffer linear{65536, rgbPq, 1, 1, 1, 0};
    const auto sdr = AgcDriver::DecodeDisplayBuffer(linear, source);
    Expect(std::to_integer<unsigned>(sdr[0]) == 255, "a 537 nit grey is not white for a title told 100 nits");
    AgcDriverNoteReportedToneMapPeak_nid_postfix(1000.0f);
    const auto told = AgcDriver::DecodeDisplayBuffer(linear, source);
    const auto expected = AgcDriver::PqToSdrPixel(word, true, 1000.0f);
    Expect(std::memcmp(told.data(), expected.data(), 4) == 0 && std::to_integer<unsigned>(told[0]) == 252, "a linear PQ display buffer is not converted for the peak the title was told");
    const AgcDriver::DisplayBuffer cleared{65536, rgbPq, 128, 128, 0, 0, 0x20000, word};
    const auto clear = AgcDriver::DisplayBufferClearPixel(cleared, AgcDriver::Graphics::DccKeys::ClearRegister);
    Expect(std::memcmp(clear.data(), expected.data(), 4) == 0, "a PQ DCC clear value is not converted for the peak the title was told");
    const auto raw = AgcDriver::DisplayBufferClearTexel(cleared, AgcDriver::Graphics::DccKeys::ClearRegister);
    Expect(std::memcmp(raw.data(), &word, 4) == 0, "the raw PQ clear texel is not kept for the HDR10 swapchain");
    std::array<std::byte, 16> texels{};
    for (std::size_t i = 0; i < texels.size(); ++i) texels[i] = static_cast<std::byte>(i * 37u + 5u);
    const auto rows = AgcDriver::DisplayBufferTexels({65536, rgbPq, 1, 2, 1, 2}, texels);
    Expect(rows.size() == 8 && std::memcmp(rows.data(), texels.data(), 4) == 0 && std::memcmp(rows.data() + 4, texels.data() + 8, 4) == 0, "raw linear PQ texels are not read row by row unchanged");
}

}

int main() {
    dynamicRangeChoice();
    displayDescription();
    surfaceFormats();
    whiteLevels();
    hostState();
    reportedWhiteLevel();
    forceSdr();
    if (failures != 0) return 1;
    std::puts("host HDR tests passed");
    return 0;
}
