#include "prx/libSceAgcDriver/Execution/include/DisplayBuffer.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/HostHdr.hpp"
#include "prx/libSceAgcDriver/Execution/include/PqToSdr.hpp"
#include "prx/libSceAgcDriver/Execution/include/Presentation.hpp"
#include "prx/libSceAgcDriver/Execution/include/Screenshot.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "tests/VideoOutTestEnvironment.hpp"
#include "Decoder/Png.hpp"
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <SDL_vulkan.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

constexpr int SkipReturnCode = 77;
constexpr std::uint32_t Width = 320;
constexpr std::uint32_t Height = 180;
constexpr std::uint32_t Pitch = 384;
constexpr std::uint64_t Bgra8 = 0x8000000000000000ull;
constexpr std::uint64_t Rgb10Pq = 0x8100070422000000ull;
constexpr std::size_t PixelBytes = static_cast<std::size_t>(Width) * Height * 4u;
constexpr std::size_t BlockBytes = 2u * 1024u * 1024u;
constexpr std::size_t BufferSpacing = BlockBytes / 2u;
constexpr std::size_t BufferOffset = 32768u;

void Require(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error(reason);
}

void SetEnvironment(const char* name, const std::string& value) {
#ifdef _WIN32
    Require(_putenv_s(name, value.c_str()) == 0, std::string("cannot set ") + name);
#else
    Require(setenv(name, value.c_str(), 1) == 0, std::string("cannot set ") + name);
#endif
}

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(65536, BlockBytes));
#endif
        Require(block != nullptr, "cannot allocate the guest block");
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        VirtualFree(block, 0, MEM_RELEASE);
#else
        std::free(block);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    std::uint8_t* Data() { return block; }

private:
    std::uint8_t* block = nullptr;
};

std::uint32_t Bgra8Texel(std::uint32_t x, std::uint32_t y) {
    return (x & 255u) | ((y & 255u) << 8u) | (((x ^ y) & 255u) << 16u) | (255u << 24u);
}

std::uint32_t PqTexel(std::uint32_t x, std::uint32_t y) {
    const auto code = (x * 3u + y * 5u) % 1024u;
    return code | (((code * 7u) % 1024u) << 10u) | ((1023u - code) << 20u) | (3u << 30u);
}

AgcDriver::DisplayBuffer Prepare(std::uint8_t* memory, std::uint64_t format, const std::function<std::uint32_t(std::uint32_t, std::uint32_t)>& texel) {
    AgcDriver::DisplayBuffer buffer{reinterpret_cast<std::uint64_t>(memory), format, Width, Height, AgcDriver::DisplayTilingGcn, Pitch};
    buffer.alignment = 4096;
    const auto bytes = AgcDriver::DisplayBufferSize(buffer);
    Require(bytes + BufferOffset <= BufferSpacing, "the PS4 display buffer does not fit its part of the guest block");
    std::memset(memory, 0x55, bytes);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto word = texel(x, y);
            std::memcpy(memory + AgcDriver::DisplayBufferOffset(buffer, x, y), &word, sizeof(word));
        }
    }
    return buffer;
}

std::vector<std::byte> ExpectedBgra8() {
    std::vector<std::byte> pixels(PixelBytes);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto word = Bgra8Texel(x, y);
            std::memcpy(pixels.data() + (static_cast<std::size_t>(y) * Width + x) * 4u, &word, sizeof(word));
        }
    }
    return pixels;
}

std::vector<std::byte> ExpectedPq(float whiteNits) {
    std::vector<std::byte> pixels(PixelBytes);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto pixel = AgcDriver::PqToSdrPixel(PqTexel(x, y), true, whiteNits);
            std::memcpy(pixels.data() + (static_cast<std::size_t>(y) * Width + x) * 4u, pixel.data(), pixel.size());
        }
    }
    return pixels;
}

int LargestDifference(std::span<const std::byte> frame, const std::vector<std::byte>& expected) {
    int worst = 0;
    for (std::size_t i = 0; i < expected.size(); ++i) worst = std::max(worst, std::abs(std::to_integer<int>(expected[i]) - std::to_integer<int>(frame[i])));
    return worst;
}

void FindDump(const std::vector<std::vector<std::byte>>& dumps, const std::vector<std::byte>& expected, const char* what) {
    int best = 256;
    for (const auto& dump : dumps) best = std::min(best, LargestDifference(dump, expected));
    Require(best <= 1, std::string(what) + ": no frame dump within one level of the expected pixels (closest differs by " + std::to_string(best) + ", " + std::to_string(dumps.size()) + " dumps)");
}

std::vector<std::vector<std::byte>> ReadDumps() {
    std::vector<std::vector<std::byte>> dumps;
    for (int index = 0; index < 16; ++index) {
        char name[32];
        std::snprintf(name, sizeof(name), "frame_%03d.bmp", index);
        std::ifstream file(name, std::ios::binary);
        if (!file) continue;
        std::vector<char> bmp((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        Require(bmp.size() == 54u + PixelBytes, std::string(name) + " does not hold the buffer's extent at full scale");
        dumps.emplace_back(PixelBytes);
        std::memcpy(dumps.back().data(), bmp.data() + 54, PixelBytes);
    }
    return dumps;
}

std::vector<std::filesystem::path> ListFiles(const std::filesystem::path& directory) {
    std::vector<std::filesystem::path> paths;
    for (const auto& entry : std::filesystem::directory_iterator(directory)) paths.push_back(entry.path());
    return paths;
}

void CheckScreenshot(const std::filesystem::path& path, const std::vector<std::byte>& expected, const char* what) {
    std::ifstream file(path, std::ios::binary);
    const std::vector<std::uint8_t> png((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    const auto image = Decoder::Png::Decode(png);
    Require(image.has_value() && image->width == Width && image->height == Height, std::string(what) + ": the screenshot does not decode at the buffer's extent");
    std::vector<std::byte> frame(PixelBytes);
    for (std::size_t i = 0; i + 4 <= PixelBytes; i += 4) {
        frame[i] = static_cast<std::byte>(image->pixels[i + 2]);
        frame[i + 1] = static_cast<std::byte>(image->pixels[i + 1]);
        frame[i + 2] = static_cast<std::byte>(image->pixels[i]);
        frame[i + 3] = static_cast<std::byte>(image->pixels[i + 3]);
    }
    const auto worst = LargestDifference(frame, expected);
    Require(worst <= 1, std::string(what) + ": the screenshot differs from the expected pixels by " + std::to_string(worst));
}

int Run() {
    SetEnvironment("APS5_DUMP_FRAMES", "3");
    SetEnvironment("APS5_DUMP_SCALE", "1");
    VideoOutTestEnvironment environment;
    const auto screenshots = std::filesystem::current_path() / "screenshots";
    std::filesystem::create_directories(screenshots);
    SetEnvironment("ANYPS5_SCREENSHOTS", screenshots.string());

    SDL_SetMainReady();
    const auto noDisplay = [](const char* error) {
        if (std::getenv("ANYPS5_REQUIRE_DISPLAY") != nullptr) throw std::runtime_error(error);
        std::printf("skipped, no display: %s\n", error);
        return SkipReturnCode;
    };
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return noDisplay(SDL_GetError());
    AgcDriverLockVulkanLoader_nid_postfix();
    auto* window = SDL_CreateWindow("GCN display present", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, Width, Height, SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN);
    AgcDriverUnlockVulkanLoader_nid_postfix();
    if (window == nullptr) return noDisplay(SDL_GetError());
    unsigned count = 0;
    Require(SDL_Vulkan_GetInstanceExtensions(window, &count, nullptr) == SDL_TRUE, SDL_GetError());
    std::vector<const char*> extensions(count);
    Require(SDL_Vulkan_GetInstanceExtensions(window, &count, extensions.data()) == SDL_TRUE, SDL_GetError());
    const AgcDriver::PresentationWindow target{window, extensions, [](void* context, VkInstance instance) {
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        Require(SDL_Vulkan_CreateSurface(static_cast<SDL_Window*>(context), instance, &surface) == SDL_TRUE, SDL_GetError());
        return surface;
    }, [](void* context, std::uint32_t* width, std::uint32_t* height) {
        int drawableWidth = 0;
        int drawableHeight = 0;
        SDL_Vulkan_GetDrawableSize(static_cast<SDL_Window*>(context), &drawableWidth, &drawableHeight);
        *width = drawableWidth > 0 ? static_cast<std::uint32_t>(drawableWidth) : 0;
        *height = drawableHeight > 0 ? static_cast<std::uint32_t>(drawableHeight) : 0;
    }, Width, Height};
    try {
        AgcDriverAttachWindow_nid_postfix(target);
    } catch (const AgcDriver::VulkanUnavailable& error) {
        if (std::getenv("ANYPS5_REQUIRE_VULKAN") != nullptr) throw;
        std::printf("skipped, no usable Vulkan device: %s\n", error.what());
        return SkipReturnCode;
    }

    GuestBlock block;
    const auto srgb = Prepare(block.Data() + BufferOffset, Bgra8, Bgra8Texel);
    const auto pq = Prepare(block.Data() + BufferSpacing + BufferOffset, Rgb10Pq, PqTexel);
    const auto present = [&](const AgcDriver::DisplayBuffer& buffer) {
        bool ready = false;
        AgcDriverPresentBuffer_nid_postfix(target, buffer, nullptr, [](void* context) { *static_cast<bool*>(context) = true; }, &ready);
        Require(ready, "the flip did not complete");
    };
    const auto takeScreenshot = [&](const AgcDriver::DisplayBuffer& buffer, const char* what) {
        const auto before = ListFiles(screenshots);
        AgcDriver::Screenshot::Request();
        for (int attempt = 0; attempt < 400 && ListFiles(screenshots).size() == before.size(); ++attempt) {
            present(buffer);
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        const auto after = ListFiles(screenshots);
        Require(after.size() == before.size() + 1u, std::string(what) + ": the screenshot was not written");
        return *std::find_if(after.begin(), after.end(), [&](const auto& path) { return std::find(before.begin(), before.end(), path) == before.end(); });
    };

    AgcDriverSetHostDisplayHdr_nid_postfix({});
    present(srgb);
    Require(!AgcDriverPresentingHdr10_nid_postfix(), "the SDR buffer reached an HDR10 swapchain");
    present(pq);
    Require(!AgcDriverPresentingHdr10_nid_postfix(), "a PQ buffer reached an HDR10 swapchain without HDR output");
    const auto sdrPq = ExpectedPq(AgcDriver::PqToSdrWhiteNits(AgcDriver::CurrentHostHdr()));

    const bool hdr = AgcDriver::CurrentHostHdr().surfaceHdr10 && std::getenv("APS5_FORCE_SDR") == nullptr;
    std::vector<std::byte> hdrPq;
    std::filesystem::path hdrScreenshot;
    if (hdr) {
        AgcDriverSetHostDisplayHdr_nid_postfix(AgcDriver::DisplayHdrFromOutput(true, 600.0f, 0.05f, 350.0f));
        present(pq);
        Require(AgcDriverPresentingHdr10_nid_postfix(), "the PQ buffer did not reach an HDR10 swapchain");
        hdrPq = ExpectedPq(AgcDriver::PqToSdrWhiteNits(AgcDriver::CurrentHostHdr()));
        hdrScreenshot = takeScreenshot(pq, "HDR10 presentation of the PQ buffer");
        Require(AgcDriverPresentingHdr10_nid_postfix(), "the HDR10 swapchain did not stay for its screenshot");
        AgcDriverSetHostDisplayHdr_nid_postfix({});
    } else {
        std::printf("the surface offers no HDR10 here: HDR10 presentation not exercised\n");
        present(srgb);
    }
    const auto sdrScreenshot = takeScreenshot(srgb, "SDR presentation of the 8-bit buffer");
    Require(!AgcDriverPresentingHdr10_nid_postfix(), "the SDR buffer did not bring the SDR swapchain back");

    AgcDriverReleaseWindow_nid_postfix(window);
    LibcRunShutdown_nid_postfix();
    SDL_DestroyWindow(window);
    SDL_Quit();

    const auto dumps = ReadDumps();
    FindDump(dumps, ExpectedBgra8(), "SDR presentation of the 8-bit buffer");
    FindDump(dumps, sdrPq, "SDR presentation of the PQ buffer");
    CheckScreenshot(sdrScreenshot, ExpectedBgra8(), "SDR presentation of the 8-bit buffer");
    if (hdr) {
        FindDump(dumps, hdrPq, "CPU dump of the HDR10 presentation");
        CheckScreenshot(hdrScreenshot, hdrPq, "HDR10 presentation of the PQ buffer");
    }
    std::puts(hdr ? "PS4 display presentation tests passed, SDR and HDR10" : "PS4 display presentation tests passed, SDR only");
    return 0;
}

}

int main() {
    try {
        return Run();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try {
            LibcRunShutdown_nid_postfix();
        } catch (const std::exception& shutdown) {
            std::fprintf(stderr, "shutdown: %s\n", shutdown.what());
        }
        return 1;
    }
}
