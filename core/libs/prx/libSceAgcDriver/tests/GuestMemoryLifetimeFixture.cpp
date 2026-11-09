#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/tests/execution/VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/tests/GuestMemoryLifetimeSupport.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>

namespace {
std::unique_ptr<AgcDriver::VulkanDevice> device;
constexpr std::array<std::uint32_t, 4> Pattern{0x12345678u, 0x89abcdefu, 0x24681357u, 0xfedcba98u};
}

extern "C" int APS5_VABI Begin_nid_no_patch() {
    try {
        device = OpenVulkanTestDevice();
        if (!device || !GuestMemoryLifetimeHasHostImports(device->Device())) {
            device.reset();
            std::fflush(stdout);
            std::exit(VulkanTestSkipped);
        }
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "converted guest Vulkan initialization failed: %s\n", error.what());
        std::exit(1);
    }
}

extern "C" int APS5_VABI Fill_nid_no_patch(void* pointer, int gpu) {
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    {
        GuestAllocations::Mutation mutation;
        const auto range = mutation.Find(pointer);
        if (!range.readable || !range.writable || range.gpu != (gpu != 0)) {
            std::fputs("converted guest mapping does not match its requested CPU and GPU access\n", stderr);
            return 1;
        }
    }
    if (!device->FillBuffer(reinterpret_cast<std::uintptr_t>(pointer), 4096, Pattern)) return 1;
    AgcDriver::Graphics::Context probe{};
    probe.device = device->Device();
    probe.hostImportAlignment = 1;
    return !AgcDriver::Graphics::HostImportCovers(probe, reinterpret_cast<std::uintptr_t>(pointer), 65536);
}

extern "C" int APS5_VABI Retired_nid_no_patch(const void* pointer) {
    AgcDriver::Graphics::Context probe{};
    probe.device = device->Device();
    probe.hostImportAlignment = 1;
    const bool imported = AgcDriver::Graphics::HostImportCovers(probe, reinterpret_cast<std::uintptr_t>(pointer), 65536);
    if (imported) std::fputs("converted guest observed a stale Vulkan host import\n", stderr);
    return imported;
}

extern "C" int APS5_VABI Verify_nid_no_patch(const void* pointer, int zero) {
    const auto* bytes = static_cast<const std::uint8_t*>(pointer);
    for (std::size_t offset = 0; offset < 4096; ++offset) {
        const auto expected = zero ? std::uint8_t{0} : reinterpret_cast<const std::uint8_t*>(Pattern.data())[offset % sizeof(Pattern)];
        if (bytes[offset] != expected) {
            std::fprintf(stderr, "converted guest memory mismatch at byte %zu: expected %u, observed %u\n",
                         offset, unsigned(expected), unsigned(bytes[offset]));
            return 1;
        }
    }
    return 0;
}

extern "C" [[noreturn]] void APS5_VABI Finish_nid_no_patch(int result) {
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        device->WaitIdle();
    }
    device.reset();
    std::printf("converted guest memory/Vulkan lifetime result=%d\n", result);
    std::fflush(stdout);
    std::exit(result);
}
