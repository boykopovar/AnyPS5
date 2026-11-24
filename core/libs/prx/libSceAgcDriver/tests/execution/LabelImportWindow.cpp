#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <span>
#include <string>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::size_t WindowBytes = 65536;

class GuestBlock {
public:
    explicit GuestBlock(std::size_t bytes) : bytes(bytes) {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(WindowBytes, bytes));
#endif
        Require(block != nullptr, "label import window: cannot allocate the guest block");
        std::memset(block, 0, bytes);
        GuestAllocations::Mutation().Add(block, bytes, true, true, true);
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

    std::uint32_t Word(std::size_t offset) const {
        std::uint32_t value = 0;
        std::memcpy(&value, block + offset, sizeof(value));
        return value;
    }

    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(block)); }

    std::size_t Bytes() const { return bytes; }

private:
    std::size_t bytes = 0;
    std::uint8_t* block = nullptr;
};

AgcDriver::Graphics::HostImportRequesterCounts LabelCounts() {
    return AgcDriver::Graphics::HostImportCounters().requesters[static_cast<std::size_t>(AgcDriver::Graphics::ImportRequester::Label)];
}

int Store(AgcDriver::VulkanDevice& device, std::uint64_t address, std::uint32_t value, std::uint64_t stamp) {
    return device.WriteLabelOnGpu(address, std::as_bytes(std::span(&value, 1)), stamp, 0);
}

}

int main() {
    try {
        GuestBlock work(WindowBytes);
        GuestBlock large(std::size_t{1} << 20u);
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        const std::array<std::uint32_t, 4> pattern{0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u};
        if (!device->FillBuffer(work.Address(), 4096, pattern)) {
            std::printf("skipped, the device does not import guest memory\n");
            return VulkanTestSkipped;
        }
        AgcDriver::Graphics::SetHostImportBudget(AgcDriver::Graphics::HostImportLiveBytes() + large.Bytes() / 4);
        {
            const auto before = LabelCounts();
            const auto live = AgcDriver::Graphics::HostImportLiveBytes();
            const int reason = Store(*device, large.Address() + 3 * WindowBytes + 64, 0x1abe1u, 1);
            const auto after = LabelCounts();
            Require(reason == 0, "a label in an allocation over the import budget was not recorded on the GPU (reason " + std::to_string(reason) + ")");
            Require(after.made == before.made + 1 && after.madeBytes == before.madeBytes + WindowBytes, "the label did not import one window of its allocation");
            Require(AgcDriver::Graphics::HostImportLiveBytes() == live + WindowBytes, "the label import did not pin exactly its window");
            Require(AgcDriver::Graphics::HostImportCounters().labelBytes == WindowBytes, "the label window was not counted as a label import");
            device->WaitIdle();
            Require(large.Word(3 * WindowBytes + 64) == 0x1abe1u, "the label recorded through its window did not land");
            Require(large.Word(3 * WindowBytes + 60) == 0 && large.Word(3 * WindowBytes + 68) == 0, "the label changed neighbouring words");
        }
        AgcDriver::Graphics::SetHostImportBudget(0);
        std::puts("label import window tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
