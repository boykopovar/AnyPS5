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

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::Graphics::Require;

constexpr std::size_t WindowBytes = 65536;
constexpr std::uint64_t LabelLimitBytes = std::uint64_t{16} << 20u;

class GuestBlock {
public:
    explicit GuestBlock(std::size_t bytes) : bytes(bytes) {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, bytes + WindowBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        block = static_cast<std::uint8_t*>(std::aligned_alloc(WindowBytes, bytes + WindowBytes));
#endif
        Require(block != nullptr, "label import window: cannot allocate the guest block");
        std::memset(block, 0, bytes + WindowBytes);
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
        GuestBlock small(WindowBytes);
        GuestBlock large(std::size_t{1} << 20u);
        GuestBlock many(static_cast<std::size_t>(LabelLimitBytes) + 4 * WindowBytes);
        GuestBlock fresh(WindowBytes);
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        const std::array<std::uint32_t, 4> pattern{0x11111111u, 0x22222222u, 0x33333333u, 0x44444444u};
        if (!device->FillBuffer(work.Address(), 4096, pattern)) {
            std::printf("skipped, the device does not import guest memory\n");
            return VulkanTestSkipped;
        }
        AgcDriver::Graphics::SetHostImportBudget(AgcDriver::Graphics::HostImportLiveBytes() + large.Bytes() / 4);
        const bool largeWatched = AgcDriver::GuestMemory::Watched(large.Address() + 3 * WindowBytes, WindowBytes);
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
        {
            Require(device->FillBuffer(work.Address(), 4096, pattern), "cannot record work before the small-block label");
            const auto before = LabelCounts();
            const auto live = AgcDriver::Graphics::HostImportLiveBytes();
            const std::uint32_t value = 0x5a11u;
            const auto address = small.Address() + 128;
            Require(!AgcDriver::Graphics::HostImportCovers(AgcDriver::VulkanDeviceTestAccess::ContextOf(*device), address, 4), "an earlier import already covers the small allocation");
            const int reason = Store(*device, address, value, 2);
            Require(reason == 7, "a label in an allocation no larger than a window did not keep its CPU path (reason " + std::to_string(reason) + ")");
            Require(LabelCounts().made == before.made && AgcDriver::Graphics::HostImportLiveBytes() == live, "a label in a small allocation imported it");
            device->WaitIdle();
            AgcDriver::GuestMemory::Write(address, std::as_bytes(std::span(&value, 1)), 4);
            Require(small.Word(128) == value, "the CPU label did not land");
        }
        {
            const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
            Require(device->FillBuffer(work.Address(), 4096, pattern), "cannot record work before the label windows");
            const auto before = LabelCounts();
            const auto room = static_cast<std::size_t>((LabelLimitBytes - AgcDriver::Graphics::HostImportCounters().labelBytes) / WindowBytes);
            for (std::size_t index = 0; index < room; ++index) {
                const int reason = Store(*device, many.Address() + index * WindowBytes + 16, 0xc0de0000u + static_cast<std::uint32_t>(index), 10 + index);
                Require(reason == 0, "label " + std::to_string(index) + " was not recorded on the GPU (reason " + std::to_string(reason) + ")");
            }
            Require(LabelCounts().made == before.made + room && AgcDriver::Graphics::HostImportCounters().labelBytes == LabelLimitBytes, "the labels did not fill the label windows to their limit");
            const auto held = many.Address() + room * WindowBytes + 16;
            const std::uint32_t heldValue = 0xc0de0000u + static_cast<std::uint32_t>(room);
            const int heldReason = Store(*device, held, heldValue, 10 + room);
            Require(heldReason == 7, "a label past the limit while every label window is held did not keep its CPU path (reason " + std::to_string(heldReason) + ")");
            Require(AgcDriver::Graphics::HostImportCounters().labelBytes == LabelLimitBytes, "a label past the limit grew the label windows");
            device->WaitIdle();
            AgcDriver::GuestMemory::Write(held, std::as_bytes(std::span(&heldValue, 1)), 4);
            {
                AgcDriver::Graphics::GuestBufferMemory memory(context);
                memory.AddReadable(work.Address(), 32);
                memory.UploadPrepare(false);
                memory.UploadFinish(false);
            }
            Require(device->FillBuffer(work.Address(), 4096, pattern), "cannot record work after the build");
            const auto last = many.Address() + (room + 1) * WindowBytes + 16;
            const std::uint32_t lastValue = 0xc0de0000u + static_cast<std::uint32_t>(room + 1);
            const int reason = Store(*device, last, lastValue, 11 + room);
            Require(reason == 0, "a label past the limit was not recorded on the GPU once older windows were released (reason " + std::to_string(reason) + ")");
            Require(AgcDriver::Graphics::HostImportCounters().labelBytes == LabelLimitBytes, "the label windows exceed their limit");
            Require(!AgcDriver::Graphics::HostImportCovers(context, large.Address() + 3 * WindowBytes + 64, 4), "the least recently used label window was not the one retired");
            Require(AgcDriver::GuestMemory::Watched(large.Address() + 3 * WindowBytes, WindowBytes) == largeWatched, "the retired label window did not get back the write watch it had before its import");
            Require(AgcDriver::Graphics::HostImportCovers(context, many.Address() + 16, 4) && AgcDriver::Graphics::HostImportCovers(context, last, 4), "a more recent label window was retired");
            Require(AgcDriver::Graphics::HostImportCovers(context, work.Address(), 4096), "the normal import was retired before the budget check");
            Require(device->FillBuffer(fresh.Address(), 4096, pattern), "cannot record work on a fresh allocation");
            Require(AgcDriver::Graphics::HostImportCovers(context, fresh.Address(), 4096), "a normal import within the budget was refused while the label windows are full");
            Require(AgcDriver::Graphics::HostImportCovers(context, work.Address(), 4096), "label windows consumed the normal host import budget and retired a normal import");
            Require(AgcDriver::Graphics::HostImportCounters().labelBytes == LabelLimitBytes && AgcDriver::Graphics::HostImportCovers(context, many.Address() + 16, 4), "a normal import retired a label window");
            device->WaitIdle();
            for (std::size_t index = 0; index <= room + 1; ++index) Require(many.Word(index * WindowBytes + 16) == 0xc0de0000u + index, "label " + std::to_string(index) + " did not land");
        }
        AgcDriver::Graphics::SetHostImportBudget(0);
        std::puts("label import window tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
