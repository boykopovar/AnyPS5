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
#include <thread>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>

namespace {

using AgcDriver::Graphics::Require;

constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t AllocationBytes = BlockBytes * 4;

class GuestAllocation {
public:
    GuestAllocation() {
#ifdef _WIN32
        memory = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, AllocationBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
#else
        memory = static_cast<std::uint8_t*>(std::aligned_alloc(BlockBytes, AllocationBytes));
#endif
        Require(memory != nullptr, "host import at map: cannot allocate the guest memory");
        std::memset(memory, 0, AllocationBytes);
        Register();
    }

    ~GuestAllocation() {
        if (registered) Unregister();
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        std::free(memory);
#endif
    }

    GuestAllocation(const GuestAllocation&) = delete;
    GuestAllocation& operator=(const GuestAllocation&) = delete;

    std::uint64_t Address() const { return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(memory)); }

    void Register() {
        GuestAllocations::Mutation().Add(memory, AllocationBytes, true, true, true);
        registered = true;
    }

    void Unregister() {
        GuestAllocations::Mutation().Remove(memory);
        registered = false;
    }

    GuestAllocations::Mapped Ranges() const {
        GuestAllocations::Mapped ranges;
        for (const auto& range : GuestAllocations::GuestAllocationsAcquire_nid_postfix()) {
            if (range->address == Address()) ranges.push_back(range);
        }
        Require(ranges.size() == 1, "host import at map: the allocation is not registered");
        return ranges;
    }

private:
    std::uint8_t* memory = nullptr;
    bool registered = false;
};

void Reconcile(AgcDriver::VulkanDevice& device, const GuestAllocation& probe) {
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    const std::array<std::uint32_t, 4> pattern{5u, 6u, 7u, 8u};
    Require(device.FillBuffer(probe.Address(), BlockBytes, pattern), "host import at map: the probe fill was not imported");
    device.WaitIdle();
}

void Import(AgcDriver::VulkanDevice& device, const GuestAllocations::Mapped& ranges, std::uint64_t generation = GuestAllocations::GuestAllocationsGeneration_nid_postfix()) {
    if (device.ImportGuestMemory(ranges, generation, false)) return;
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    Require(device.ImportGuestMemory(ranges, generation, true), "host import at map: the import under the GPU lock refused");
}

bool Imported(VkDevice device, std::uint64_t address, std::size_t bytes) {
    AgcDriver::Graphics::Context probe{};
    probe.device = device;
    probe.hostImportAlignment = 1;
    return AgcDriver::Graphics::HostImportCovers(probe, address, bytes);
}

}

int main() {
    try {
#ifdef _WIN32
        _putenv_s("APS5_PIN_WAIT_MS", "2000");
#else
        setenv("APS5_PIN_WAIT_MS", "2000", 1);
#endif
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        GuestAllocation first;
        Import(*device, first.Ranges());
        const bool importedFirst = Imported(device->Device(), first.Address(), AllocationBytes);
        first.Unregister();
        Require(!Imported(device->Device(), first.Address(), AllocationBytes), "host import at map: unmapping an allocation imported before any lookup kept its import");
        GuestAllocation probe;
        bool filled = false;
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            const std::array<std::uint32_t, 4> pattern{1u, 2u, 3u, 4u};
            filled = device->FillBuffer(probe.Address(), BlockBytes, pattern);
            device->WaitIdle();
        }
        if (!filled) {
            std::printf("skipped, the device does not import guest memory\n");
            return VulkanTestSkipped;
        }
        Require(importedFirst, "host import at map: a mapping before any lookup was not imported");
        GuestAllocation guest;
        const auto handle = device->Device();
        Require(!Imported(handle, guest.Address(), AllocationBytes), "host import at map: the allocation was imported before anything asked for it");
        Import(*device, guest.Ranges());
        Require(Imported(handle, guest.Address(), AllocationBytes), "host import at map: the mapped allocation was not imported");

        GuestAllocation stale;
        Import(*device, stale.Ranges());
        stale.Unregister();
        GuestAllocation mapped;
        Import(*device, mapped.Ranges());
        Require(Imported(handle, mapped.Address(), AllocationBytes), "host import at map: a mapping made after another changed was not imported");

        GuestAllocation remapped;
        Import(*device, remapped.Ranges());
        remapped.Unregister();
        remapped.Register();
        Import(*device, remapped.Ranges());
        Require(Imported(handle, remapped.Address(), AllocationBytes), "host import at map: a range mapped again was not imported");
        GuestAllocation gone;
        const auto goneRanges = gone.Ranges();
        gone.Unregister();
        Import(*device, goneRanges);
        Require(!Imported(handle, gone.Address(), AllocationBytes), "host import at map: a mapping unregistered before its import was imported");
        gone.Register();

        GuestAllocation late;
        const auto lateRanges = late.Ranges();
        const auto lateAt = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
        GuestAllocation newer;
        Reconcile(*device, probe);
        Import(*device, lateRanges, lateAt);
        Require(!Imported(handle, late.Address(), AllocationBytes), "host import at map: an import older than the last reconcile was made");

        GuestAllocation contended;
        std::atomic<bool> held{false};
        std::atomic<bool> release{false};
        std::thread holder([&] {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            held = true;
            while (!release) std::this_thread::yield();
        });
        while (!held) std::this_thread::yield();
        const bool unlocked = device->ImportGuestMemory(contended.Ranges(), GuestAllocations::GuestAllocationsGeneration_nid_postfix(), false);
        release = true;
        holder.join();
        Require(unlocked && Imported(handle, contended.Address(), AllocationBytes), "host import at map: a mapping on a device that already imports needed the GPU lock");
        std::puts("host import at map tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
