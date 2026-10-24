#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/DisplayBuffer.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Presentation.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include <SDL_loadso.h>
#include <vulkan/vulkan.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <mutex>
#include <random>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

alignas(256) std::array<std::uint32_t, 15> SpinCode{0xf4080200u, 0xfa000000u, 0xbe840381u, 0x9304ff04u, 0x0019660du, 0x8004ff04u, 0x3c6ef35fu, 0x80828102u, 0xbf078002u, 0xbf85fff9u, 0xbf8cc07fu, 0x7e000204u, 0xe0700000u, 0x80020000u, 0xbf810000u};

constexpr std::uint32_t Width = 64;
constexpr std::uint32_t Height = 64;
constexpr std::uint64_t Bgra8 = 0x8000000000000000ull;
constexpr std::size_t BlockBytes = 1u << 20u;
constexpr std::size_t DisplayBytes = Width * Height * 4u;
constexpr std::size_t ControlOffset = 512u * 1024u;

struct Control {
    alignas(256) std::array<std::uint32_t, 4> spinDescriptor{};
    alignas(256) volatile std::uint32_t done = 0;
};

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(65536, BlockBytes));
#endif
        Require(block != nullptr, "present lock wait: cannot allocate the guest block");
        std::memset(block, 0, BlockBytes);
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

    std::uint8_t* Display() { return block; }
    volatile std::uint32_t& FirstWord() { return *reinterpret_cast<volatile std::uint32_t*>(block); }
    Control& Controls() { return *reinterpret_cast<Control*>(block + ControlOffset); }

private:
    std::uint8_t* block = nullptr;
};

std::uint32_t Low(const volatile void* pointer) {
    return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

std::uint32_t High(const volatile void* pointer) {
    return static_cast<std::uint32_t>(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer)) >> 32u);
}

void SetShaderRegister(std::vector<std::uint32_t>& words, std::uint32_t offset, std::uint32_t value) {
    words.insert(words.end(), {0xc0017600u, offset, value});
}

void Dispatch(std::vector<std::uint32_t>& words, const void* code, std::initializer_list<std::uint32_t> userData) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(code));
    SetShaderRegister(words, 0x20c, static_cast<std::uint32_t>(address >> 8u));
    SetShaderRegister(words, 0x20d, static_cast<std::uint32_t>(address >> 40u) & 0xffu);
    for (const std::uint32_t offset : {0x207u, 0x208u, 0x209u}) SetShaderRegister(words, offset, 1);
    SetShaderRegister(words, 0x212, 0);
    SetShaderRegister(words, 0x213, static_cast<std::uint32_t>(userData.size()) << 1u);
    std::uint32_t offset = 0x240;
    for (const auto value : userData) SetShaderRegister(words, offset++, value);
    words.insert(words.end(), {0xc0031500u, 1, 1, 1, 0x8041u});
}

void WriteData(std::vector<std::uint32_t>& words, volatile std::uint32_t* target, std::uint32_t value) {
    words.insert(words.end(), {0xc0033700u, 0x00100200u, Low(target), High(target), value});
}

void SubmitSpin(GuestBlock& block, std::uint32_t iterations) {
    auto& control = block.Controls();
    control.spinDescriptor = {Low(block.Display()), High(block.Display()) & 0xffffu, 16u, 0x31016facu};
    control.done = 0;
    std::vector<std::uint32_t> words;
    Dispatch(words, SpinCode.data(), {Low(control.spinDescriptor.data()), High(control.spinDescriptor.data()), iterations});
    WriteData(words, &control.done, 1);
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    Require(sceAgcDriverSubmitAcb(0x20, &packet) == 0, "present lock wait: the compute submission failed");
}

void WaitDone(GuestBlock& block) {
    const auto start = Clock::now();
    while (block.Controls().done == 0) {
        Require(Clock::now() - start < std::chrono::seconds(30), "present lock wait: the spin dispatch did not finish");
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
}

std::uint32_t SpinResult(std::uint32_t iterations) {
    std::uint32_t value = 1;
    for (std::uint32_t iteration = 0; iteration < iterations; ++iteration) value = value * 0x19660du + 0x3c6ef35fu;
    return value;
}

void SetEnvironment(const char* name, const char* value) {
#ifdef _WIN32
    Require(_putenv_s(name, value) == 0, std::string("present lock wait: cannot set ") + name);
#else
    Require(setenv(name, value, 1) == 0, std::string("present lock wait: cannot set ") + name);
#endif
}

class FrameDumps {
public:
    FrameDumps() : previous(std::filesystem::current_path()), directory(std::filesystem::temp_directory_path() / ("aps5-present-lock-wait-" + std::to_string(std::random_device{}()))) {
        std::filesystem::create_directories(directory);
        std::filesystem::current_path(directory);
        SetEnvironment("APS5_DUMP_FRAMES", "64");
        SetEnvironment("APS5_NO_GPU_DUMP", "1");
    }

    ~FrameDumps() {
        std::error_code error;
        std::filesystem::current_path(previous, error);
        std::filesystem::remove_all(directory, error);
    }

    FrameDumps(const FrameDumps&) = delete;
    FrameDumps& operator=(const FrameDumps&) = delete;

    std::uint32_t FirstPixel(std::uint32_t index) const {
        const auto name = std::string("frame_") + (index < 10 ? "00" : index < 100 ? "0" : "") + std::to_string(index) + ".bmp";
        std::ifstream file(directory / name, std::ios::binary);
        Require(file.good(), "present lock wait: the present did not dump " + name);
        file.seekg(54);
        std::uint32_t pixel = 0;
        file.read(reinterpret_cast<char*>(&pixel), sizeof(pixel));
        Require(file.gcount() == sizeof(pixel), "present lock wait: " + name + " holds no pixel");
        return pixel;
    }

private:
    std::filesystem::path previous;
    std::filesystem::path directory;
};

std::string Hex(std::uint32_t value) {
    char text[11];
    std::snprintf(text, sizeof(text), "0x%08x", value);
    return text;
}

double Ms(Clock::duration duration) {
    return std::chrono::duration<double, std::milli>(duration).count();
}

PFN_vkGetInstanceProcAddr InstanceProc = nullptr;

VkSurfaceKHR CreateHeadlessSurface(void*, VkInstance instance) {
    const auto create = reinterpret_cast<PFN_vkCreateHeadlessSurfaceEXT>(InstanceProc(instance, "vkCreateHeadlessSurfaceEXT"));
    Require(create != nullptr, "present lock wait: vkCreateHeadlessSurfaceEXT is missing");
    VkHeadlessSurfaceCreateInfoEXT info{VK_STRUCTURE_TYPE_HEADLESS_SURFACE_CREATE_INFO_EXT};
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    Require(create(instance, &info, nullptr, &surface) == VK_SUCCESS, "present lock wait: the headless surface was not created");
    return surface;
}

void DrawableSize(void*, std::uint32_t* width, std::uint32_t* height) {
    *width = Width;
    *height = Height;
}

bool HeadlessSurfaceAvailable() {
#ifdef _WIN32
    void* loader = SDL_LoadObject("vulkan-1.dll");
#else
    void* loader = SDL_LoadObject("libvulkan.so.1");
#endif
    if (loader == nullptr) return false;
    InstanceProc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_LoadFunction(loader, "vkGetInstanceProcAddr"));
    if (InstanceProc == nullptr) return false;
    const auto enumerate = reinterpret_cast<PFN_vkEnumerateInstanceExtensionProperties>(InstanceProc(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties"));
    if (enumerate == nullptr) return false;
    std::uint32_t count = 0;
    if (enumerate(nullptr, &count, nullptr) != VK_SUCCESS) return false;
    std::vector<VkExtensionProperties> extensions(count);
    if (enumerate(nullptr, &count, extensions.data()) != VK_SUCCESS) return false;
    return std::any_of(extensions.begin(), extensions.end(), [](const VkExtensionProperties& extension) { return std::strcmp(extension.extensionName, VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME) == 0; });
}

int ReadyContext = 0;

void NoGpuReady(void*) {}

struct Measured {
    double present = 0;
    double longestLockWait = 0;
};

Measured PresentBesideLockWaits(GuestBlock& block, const AgcDriver::PresentationWindow& window, const AgcDriver::DisplayBuffer& buffer, std::uint32_t iterations) {
    block.FirstWord() = 0;
    SubmitSpin(block, iterations);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    std::atomic<bool> presenting{true};
    Measured measured;
    std::exception_ptr failure;
    std::thread presenter([&] {
        const auto start = Clock::now();
        try {
            AgcDriverPresentBuffer_nid_postfix(window, buffer, NoGpuReady, &ReadyContext);
        } catch (...) {
            failure = std::current_exception();
        }
        measured.present = Ms(Clock::now() - start);
        presenting.store(false, std::memory_order_release);
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    while (presenting.load(std::memory_order_acquire)) {
        const auto start = Clock::now();
        {
            AgcDriver::GuestMemory::TagGpuLockSite(AgcDriver::GuestMemory::GpuLockSite::Other);
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        }
        measured.longestLockWait = std::max(measured.longestLockWait, Ms(Clock::now() - start));
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    presenter.join();
    if (failure) std::rethrow_exception(failure);
    WaitDone(block);
    AgcDriverWaitIdle_nid_postfix();
    return measured;
}

int Run() {
    if (!HeadlessSurfaceAvailable()) {
        std::cout << "skipped, the device has no VK_EXT_headless_surface for a presentation window\n";
        return VulkanTestSkipped;
    }
    FrameDumps dumps;
    GuestBlock block;
    static constexpr std::array<const char*, 2> extensions{VK_KHR_SURFACE_EXTENSION_NAME, VK_EXT_HEADLESS_SURFACE_EXTENSION_NAME};
    static int context = 0;
    const AgcDriver::PresentationWindow window{&context, extensions, CreateHeadlessSurface, DrawableSize, Width, Height, nullptr};
    AgcDriverAttachWindow_nid_postfix(window);
    const AgcDriver::DisplayBuffer buffer{reinterpret_cast<std::uintptr_t>(block.Display()), Bgra8, Width, Height, 1u, Width};
    AgcDriverPresentBuffer_nid_postfix(window, buffer, NoGpuReady, &ReadyContext);
    AgcDriverWaitIdle_nid_postfix();
    Require(dumps.FirstPixel(0) == 0, "present lock wait: the first present read " + Hex(dumps.FirstPixel(0)) + " from a cleared display buffer");
    std::uint32_t iterations = 1u << 20u;
    double spin = 0;
    for (std::uint32_t attempt = 0; attempt < 8; ++attempt) {
        const auto start = Clock::now();
        SubmitSpin(block, iterations);
        WaitDone(block);
        spin = Ms(Clock::now() - start);
        if (spin >= 300) break;
        iterations *= 2u;
    }
    Require(spin >= 300, "present lock wait: the spin dispatch finished in " + std::to_string(spin) + " ms, too quickly to measure");
    std::cout << "spin dispatch of " << iterations << " iterations takes " << spin << " ms\n";
    const auto written = SpinResult(iterations);
    Measured best;
    best.longestLockWait = 1e9;
    for (std::uint32_t attempt = 0; attempt < 3; ++attempt) {
        const auto measured = PresentBesideLockWaits(block, window, buffer, iterations);
        const auto read = dumps.FirstPixel(attempt + 1u);
        std::cout << "attempt " << attempt << ": present " << measured.present << " ms, longest device lock wait beside it " << measured.longestLockWait << " ms, read " << Hex(read) << "\n";
        Require(read == written, "present lock wait: the present read " + Hex(read) + " from the display buffer before its GPU writer stored " + Hex(written));
        if (measured.longestLockWait < best.longestLockWait) best = measured;
    }
    Require(best.present >= spin / 3, "present lock wait: the present did not wait for the display buffer's GPU writer (" + std::to_string(best.present) + " ms against a " + std::to_string(spin) + " ms writer)");
    Require(best.longestLockWait < spin / 4, "present lock wait: the device lock was held for " + std::to_string(best.longestLockWait) + " ms while the present waited for the display buffer's GPU writer (" + std::to_string(spin) + " ms)");
    std::cout << "present lock wait test passed\n";
    return 0;
}

}

int main() {
    try {
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        device.reset();
        const int result = Run();
        AgcDriverShutdown_nid_postfix();
        return result;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try {
            AgcDriverShutdown_nid_postfix();
        } catch (const std::exception& shutdownError) {
            std::cerr << shutdownError.what() << '\n';
        }
        return 1;
    }
}
