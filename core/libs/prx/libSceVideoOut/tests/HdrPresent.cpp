#include "prx/libSceVideoOut/include/VideoOutDriver.hpp"
#include "prx/libSceVideoOut/include/Buffer.hpp"
#include "prx/libSceVideoOut/include/Output.hpp"
#include "prx/libSceAgcDriver/Execution/include/DisplayBuffer.hpp"
#include "prx/libSceAgcDriver/Execution/include/HostHdr.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <algorithm>
#include <thread>
#include <vector>

#undef main

namespace {

constexpr int SkipReturnCode = 77;
constexpr std::uint64_t Bgr10Pq = 0x8100070400000000ull;
constexpr std::uint64_t Rgb10Pq = 0x8100070422000000ull;
constexpr std::uint64_t Bgra8 = 0x8000000000000000ull;
constexpr std::uint32_t Width = 259;
constexpr std::uint32_t Height = 137;
constexpr std::uint32_t ClearWord = 600u | (500u << 10u) | (300u << 20u) | (3u << 30u);

void check(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error(reason);
}

std::span<std::byte> alignedBuffer(std::vector<std::byte>& allocation, std::size_t bytes) {
    const auto address = reinterpret_cast<std::uintptr_t>(allocation.data());
    const auto aligned = (address + 65535u) & ~static_cast<std::uintptr_t>(65535u);
    return {allocation.data() + (aligned - address), bytes};
}

void fillPq(std::span<std::byte> storage) {
    for (std::size_t i = 0; i + 4 <= storage.size(); i += 4) {
        const auto code = static_cast<std::uint32_t>((i / 4) % 1024u);
        const std::uint32_t word = code | (((code * 7u) % 1024u) << 10u) | ((1023u - code) << 20u) | (3u << 30u);
        std::memcpy(storage.data() + i, &word, 4);
    }
}

void flip(int handle, const std::shared_ptr<VideoOutConfig>& cfg, int index) {
    std::uint64_t target = 0;
    {
        std::lock_guard lock(cfg->mutex);
        target = cfg->flipStatus.count + 1;
    }
    check(sceVideoOutSubmitFlip(handle, index, 1, index) == 0, "flip submission failed");
    std::unique_lock lock(cfg->mutex);
    check(cfg->vblankCond.wait_for(lock, std::chrono::seconds(15), [&] { return cfg->failure != nullptr || cfg->flipStatus.count == target; }), "presentation did not complete");
    if (cfg->failure) std::rethrow_exception(cfg->failure);
    check(cfg->flipStatus.currentBuffer == index && cfg->flipStatus.flipPendingNum == 0, "presentation status is wrong");
}

std::vector<std::vector<std::byte>> readDumps(std::size_t bytes) {
    std::vector<std::vector<std::byte>> dumps;
    for (int index = 0; index < 64; ++index) {
        char name[32];
        std::snprintf(name, sizeof(name), "frame_%03d.bmp", index);
        if (!std::filesystem::exists(name)) continue;
        std::ifstream file(name, std::ios::binary);
        std::vector<char> bmp((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        if (bmp.size() != 54 + bytes) continue;
        dumps.emplace_back(bytes);
        std::memcpy(dumps.back().data(), bmp.data() + 54, bytes);
    }
    return dumps;
}

void findDump(const std::vector<std::vector<std::byte>>& dumps, const std::vector<std::byte>& expected, const char* what) {
    int best = 256;
    for (const auto& dump : dumps) {
        int worst = 0;
        for (std::size_t i = 0; i < expected.size() && worst <= best; ++i) worst = std::max(worst, std::abs(std::to_integer<int>(expected[i]) - std::to_integer<int>(dump[i])));
        best = std::min(best, worst);
    }
    std::printf("%s: largest difference from the CPU conversion %d\n", what, best);
    check(best <= 1, std::string(what) + " has no frame dump within one level of the CPU conversion");
}

int run() {
#ifdef _WIN32
    _putenv_s("APS5_DUMP_FRAMES", "64");
    _putenv_s("APS5_DUMP_SCALE", "1");
#else
    setenv("APS5_DUMP_FRAMES", "64", 1);
    setenv("APS5_DUMP_SCALE", "1", 1);
#endif
    VideoOutTestEnvironment environment;
    int handle = 0;
    try {
        handle = sceVideoOutOpen(255, 0, 0, nullptr);
    } catch (const std::runtime_error& error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw;
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        return SkipReturnCode;
    }
    check(handle > 0, "sceVideoOutOpen failed");
    auto cfg = VideoOutDriver::Get().GetConfig(handle);
    const std::size_t tiledBytes = static_cast<std::size_t>((Width + 127u) / 128u) * ((Height + 127u) / 128u) * 65536u;
    const std::size_t linearBytes = static_cast<std::size_t>(Width) * Height * 4u;
    std::vector<std::byte> pqAllocation(tiledBytes + 65535u), linearAllocation(linearBytes + 65535u), sdrAllocation(tiledBytes + 65535u), clearAllocation(tiledBytes + 65535u);
    const auto pq = alignedBuffer(pqAllocation, tiledBytes);
    const auto linear = alignedBuffer(linearAllocation, linearBytes);
    const auto sdr = alignedBuffer(sdrAllocation, tiledBytes);
    const auto cleared = alignedBuffer(clearAllocation, tiledBytes);
    fillPq(pq);
    fillPq(linear);
    std::memset(sdr.data(), 0x40, sdr.size());
    std::vector<std::uint8_t> keys(4096, 0x20);

    VideoOutBufferAttribute2 attribute{};
    VideoOutBuffers buffer{pq.data(), nullptr, {nullptr, nullptr}};
    sceVideoOutSetBufferAttribute2(&attribute, Bgr10Pq, 0, Width, Height, 0, 0, 0);
    check(sceVideoOutRegisterBuffers2(handle, 0, 0, &buffer, 1, &attribute, 0, nullptr) == 0, "the tiled PQ buffer was not registered");
    buffer = {linear.data(), nullptr, {nullptr, nullptr}};
    sceVideoOutSetBufferAttribute2(&attribute, Rgb10Pq, 1, Width, Height, 0, 0, 0);
    check(sceVideoOutRegisterBuffers2(handle, 1, 1, &buffer, 1, &attribute, 0, nullptr) == 0, "the linear PQ buffer was not registered");
    buffer = {sdr.data(), nullptr, {nullptr, nullptr}};
    sceVideoOutSetBufferAttribute2(&attribute, Bgra8, 0, Width, Height, 0, 0, 0);
    check(sceVideoOutRegisterBuffers2(handle, 2, 2, &buffer, 1, &attribute, 0, nullptr) == 0, "the SDR buffer was not registered");
    buffer = {cleared.data(), keys.data(), {nullptr, nullptr}};
    sceVideoOutSetBufferAttribute2(&attribute, Rgb10Pq, 0, Width, Height, 0, 0x208, ClearWord);
    check(sceVideoOutRegisterBuffers2(handle, 3, 3, &buffer, 1, &attribute, VIDEO_OUT_BUFFER_ATTRIBUTE_CATEGORY_COMPRESSED, nullptr) == 0, "the DCC PQ buffer was not registered");

    const auto present = [&](int index) { flip(handle, cfg, index); };
    const auto host = AgcDriver::CurrentHostHdr();
    VideoOutOutputStatus status{};
    check(sceVideoOutGetOutputStatus(handle, &status) == 0 && status.dynamicRange == (host.available ? 2u : 1u), "the output status does not follow the host's HDR output");
    if (host.available) {
        for (const int index : {0, 1, 3, -1}) {
            present(index);
            check(AgcDriverPresentingHdr10_nid_postfix(), "PQ display buffer " + std::to_string(index) + " did not reach an HDR10 swapchain");
        }
        present(2);
        check(!AgcDriverPresentingHdr10_nid_postfix(), "the SDR buffer did not bring the SDR swapchain back");
        present(0);
        check(AgcDriverPresentingHdr10_nid_postfix(), "the HDR10 swapchain did not come back for a PQ buffer");
        AgcDriverSetHostDisplayHdr_nid_postfix(AgcDriver::DisplayHdrFromOutput(true, 600.0f, 0.05f, 350.0f));
        present(0);
        check(AgcDriverPresentingHdr10_nid_postfix() && AgcDriver::CurrentHostHdr().display.maxLuminance == 600.0f, "a new monitor luminance left the HDR10 swapchain");
        std::printf("HDR10 presentation passed (peak %.1f nits)\n", host.display.maxLuminance);
    } else {
        std::printf("HDR output is unavailable on this host (display in HDR: %s, surface offers HDR10: %s): HDR10 presentation not exercised\n", host.display.hdr ? "yes" : "no", host.surfaceHdr10 ? "yes" : "no");
    }
    AgcDriverSetHostDisplayHdr_nid_postfix({});
    AgcDriverNoteReportedToneMapPeak_nid_postfix(437.0f);
    for (const int index : {0, 1, 3}) {
        present(index);
        present(index);
        check(!AgcDriverPresentingHdr10_nid_postfix(), "PQ display buffer " + std::to_string(index) + " reached an HDR10 swapchain without HDR output");
    }
    const auto whiteNits = AgcDriver::PqToSdrWhiteNits(AgcDriver::CurrentHostHdr());
    check(whiteNits == 437.0f, "the SDR conversion does not use the peak the title was told");
    const auto tiledExpected = AgcDriver::DecodeDisplayBuffer({reinterpret_cast<std::uint64_t>(pq.data()), Bgr10Pq, Width, Height}, pq);
    const auto linearExpected = AgcDriver::DecodeDisplayBuffer({reinterpret_cast<std::uint64_t>(linear.data()), Rgb10Pq, Width, Height, 1, 0}, linear);
    const auto clearPixel = AgcDriver::PqToSdrPixel(ClearWord, true, whiteNits);
    std::vector<std::byte> clearExpected(linearBytes);
    for (std::size_t i = 0; i < clearExpected.size(); ++i) clearExpected[i] = static_cast<std::byte>(clearPixel[i % 4]);
    for (int set = 0; set < 4; ++set) sceVideoOutUnregisterBuffers(handle, set);
    sceVideoOutClose(handle);
    LibcRunShutdown_nid_postfix();
    const auto dumps = readDumps(linearBytes);
    findDump(dumps, tiledExpected, "SDR presentation of the tiled PQ buffer at 437 nits (GPU detile conversion)");
    findDump(dumps, linearExpected, "SDR presentation of the linear PQ buffer at 437 nits (CPU conversion)");
    findDump(dumps, clearExpected, "SDR presentation of the DCC-cleared PQ buffer at 437 nits (clear value conversion)");
    return 0;
}

}

int main() {
    try {
        return run();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        try { LibcRunShutdown_nid_postfix(); }
        catch (const std::exception& shutdown) { std::fprintf(stderr, "shutdown: %s\n", shutdown.what()); }
        return 1;
    }
}
