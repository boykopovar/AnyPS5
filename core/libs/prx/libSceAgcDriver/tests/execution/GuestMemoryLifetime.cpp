#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceAgcDriver/tests/execution/VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/tests/GuestMemoryLifetimeSupport.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string_view>

extern "C" {
void* APS5_VABI mmap_nid_postfix(void*, std::size_t, int, int, int, std::int64_t) noexcept;
int APS5_VABI munmap_nid_postfix(void*, std::size_t) noexcept;
int APS5_VABI mprotect_nid_postfix(void*, std::size_t, int) noexcept;
void* APS5_VABI memalign_nid_postfix(std::size_t, std::size_t);
void* APS5_VABI realloc_nid_postfix(void*, std::size_t);
void APS5_VABI free_nid_postfix(void*);
int APS5_VABI sceKernelMapFlexibleMemory(void**, std::size_t, int, int);
int APS5_VABI sceKernelAllocateDirectMemory(std::int64_t, std::int64_t, std::size_t, std::size_t, int, std::int64_t*);
int APS5_VABI sceKernelMapDirectMemory(void**, std::size_t, int, int, std::int64_t, std::size_t);
int APS5_VABI sceKernelReleaseDirectMemory(std::int64_t, std::size_t);
}

namespace {

constexpr std::size_t BlockBytes = 65536;
constexpr std::size_t FillBytes = 4096;
constexpr std::array<std::uint32_t, 4> Pattern{0x12345678u, 0x89abcdefu, 0x24681357u, 0xfedcba98u};

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::uint64_t Address(const void* pointer) {
    return reinterpret_cast<std::uintptr_t>(pointer);
}

bool Imported(const AgcDriver::VulkanDevice& device, const void* pointer, std::size_t bytes = BlockBytes) {
    AgcDriver::Graphics::Context probe{};
    probe.device = device.Device();
    probe.hostImportAlignment = 1;
    return AgcDriver::Graphics::HostImportCovers(probe, Address(pointer), bytes);
}

void* Map(std::size_t bytes = BlockBytes) {
    auto* result = mmap_nid_postfix(nullptr, bytes, 3, 0x1002, -1, 0);
    Require(result != reinterpret_cast<void*>(~std::uintptr_t{0}), "guest mmap failed");
    {
        GuestAllocations::Mutation mutation;
        const auto range = mutation.Find(result);
        Require(range.readable && range.writable && !range.gpu, "guest mmap did not register CPU-only read/write access");
    }
    std::memset(result, 0, bytes);
    return result;
}

void CheckPattern(const void* pointer) {
    const auto* data = static_cast<const std::byte*>(pointer);
    for (std::size_t offset = 0; offset < FillBytes; offset += sizeof(Pattern))
        Require(std::memcmp(data + offset, Pattern.data(), sizeof(Pattern)) == 0, "guest mutation lost the pending GPU fill");
}

void Fill(AgcDriver::VulkanDevice& device, void* pointer) {
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    Require(device.FillBuffer(Address(pointer), FillBytes, Pattern), "Vulkan device did not import the guest allocation");
    Require(Imported(device, pointer), "GPU fill did not create a host import");
}

void CheckMappings(AgcDriver::VulkanDevice& device) {
    auto* mapping = Map();
    Fill(device, mapping);
    Require(mprotect_nid_postfix(mapping, BlockBytes, 3) == 0, "guest writable protection refresh failed");
    Require(!Imported(device, mapping), "host import survived guest writable protection refresh");
    CheckPattern(mapping);
    Fill(device, mapping);
    Require(mprotect_nid_postfix(mapping, BlockBytes, 1) == 0, "guest read-only protection failed");
    Require(!Imported(device, mapping), "host import survived guest protection change");
    CheckPattern(mapping);
    Require(mprotect_nid_postfix(mapping, BlockBytes, 0) == 0, "guest inaccessible protection failed");
    Require(mprotect_nid_postfix(mapping, BlockBytes, 3) == 0, "guest writable protection restore failed");
    CheckPattern(mapping);
    Fill(device, mapping);
    void* replaced = mapping;
    Require(sceKernelMapFlexibleMemory(&replaced, BlockBytes, 0x33, 0x10) == 0 && replaced == mapping, "guest fixed mapping replacement failed");
    Require(!Imported(device, mapping), "host import survived guest fixed mapping replacement");
    {
        GuestAllocations::Mutation mutation;
        const auto range = mutation.Find(mapping);
        Require(range.readable && range.writable && range.gpu, "guest fixed mapping did not register its requested CPU and GPU access");
    }
    const auto* data = static_cast<const std::uint8_t*>(mapping);
    for (std::size_t offset = 0; offset < BlockBytes; ++offset)
        Require(data[offset] == 0, "GPU work reached replacement guest pages");
    Fill(device, mapping);
    Require(munmap_nid_postfix(mapping, BlockBytes) == 0, "guest munmap failed");
    Require(!Imported(device, mapping), "host import survived guest munmap");

    mapping = Map(3 * BlockBytes);
    Fill(device, mapping);
    auto* middle = static_cast<std::uint8_t*>(mapping) + BlockBytes;
    Require(mprotect_nid_postfix(middle, BlockBytes, 1) == 0, "guest partial protection failed");
    Require(!Imported(device, mapping), "whole host import survived partial protection change");
    CheckPattern(mapping);
    Fill(device, mapping);
    Require(munmap_nid_postfix(middle, BlockBytes) == 0, "guest middle unmap failed");
    Require(Imported(device, mapping), "unrelated prefix import was retired by middle unmap");
    Require(munmap_nid_postfix(mapping, BlockBytes) == 0, "guest prefix unmap failed");
    Require(!Imported(device, mapping), "prefix host import survived guest munmap");
    Require(munmap_nid_postfix(middle + BlockBytes, BlockBytes) == 0, "guest suffix unmap failed");
}

void CheckHeap(AgcDriver::VulkanDevice& device) {
    const std::array<void*, 10> api{};
    ApplicationHeapRegister_nid_no_patch(api.data());
    auto* allocation = memalign_nid_postfix(BlockBytes, BlockBytes);
    Require(allocation != nullptr, "guest aligned allocation failed");
    std::memset(allocation, 0, BlockBytes);
    Fill(device, allocation);
    auto* replacement = realloc_nid_postfix(allocation, 2 * BlockBytes);
    Require(replacement != nullptr, "guest realloc failed");
    Require(!Imported(device, allocation), "host import survived guest realloc");
    CheckPattern(replacement);
    free_nid_postfix(replacement);
    for (int iteration = 0; iteration < 3; ++iteration) {
        allocation = memalign_nid_postfix(BlockBytes, BlockBytes);
        Require(allocation != nullptr, "guest heap reuse allocation failed");
        std::memset(allocation, 0, BlockBytes);
        Fill(device, allocation);
        free_nid_postfix(allocation);
        Require(!Imported(device, allocation), "host import survived guest free or heap cache eviction");
    }
}

void CheckLockedMutation(AgcDriver::VulkanDevice& device) {
    auto* mapping = Map();
    {
        std::lock_guard outer(AgcDriver::GuestMemory::GpuMutex());
        Fill(device, mapping);
        Require(mprotect_nid_postfix(mapping, BlockBytes, 3) == 0, "writable protection refresh under the GPU lock did not finish");
        Require(!Imported(device, mapping), "locked protection refresh retained the import");
        CheckPattern(mapping);
        Fill(device, mapping);
        Require(mprotect_nid_postfix(mapping, BlockBytes, 1) == 0, "protection change under the GPU lock did not finish");
        Require(!Imported(device, mapping), "locked protection change retained the import");
        CheckPattern(mapping);
        Require(munmap_nid_postfix(mapping, BlockBytes) == 0, "unmap under the GPU lock did not finish");
    }

    mapping = Map();
    {
        std::lock_guard outer(AgcDriver::GuestMemory::GpuMutex());
        auto lease = std::make_shared<GuestAllocations::Lease>(GuestAllocations::GuestAllocationsAcquire_nid_postfix());
        auto* recorder = AgcDriver::Graphics::Recorder::Active();
        Require(recorder != nullptr, "Vulkan device has no active recorder");
        recorder->Keep(std::move(lease));
        Require(munmap_nid_postfix(mapping, BlockBytes) == 0, "completed batch kept its lease until the outer GPU lock was released");
    }
}

void CheckCopy(AgcDriver::VulkanDevice& device) {
    auto* source = Map();
    auto* destination = Map();
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        Fill(device, source);
        const auto copied = device.CopyBuffer(Address(destination), Address(source), FillBytes, 0, BlockBytes, 0, 0, 0, [](std::span<const std::byte>, std::uint64_t) {});
        Require(copied.path == 1, "guest source was not read by a Vulkan host-import transfer");
        Require(Imported(device, source) && Imported(device, destination), "GPU copy did not import both guest ranges");
    }
    Require(mprotect_nid_postfix(source, BlockBytes, 3) == 0, "copied source protection refresh failed");
    Require(!Imported(device, source), "GPU copy source remained imported after guest protection refresh");
    CheckPattern(destination);
    Require(Imported(device, destination), "source retirement unnecessarily retired the copy destination");
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        Fill(device, source);
        const auto copied = device.CopyBuffer(Address(destination), Address(source), FillBytes, 0, BlockBytes, 0, 0, 0, [](std::span<const std::byte>, std::uint64_t) {});
        Require(copied.path == 1, "second source transfer did not use host imports");
    }
    Require(munmap_nid_postfix(source, BlockBytes) == 0, "copied source unmap failed");
    Require(!Imported(device, source), "GPU copy source remained imported after guest unmap");
    CheckPattern(destination);
    Require(Imported(device, destination), "source unmap retired the unrelated copy destination");
    Require(munmap_nid_postfix(destination, BlockBytes) == 0, "copy destination unmap failed");
    Require(!Imported(device, destination), "GPU copy destination remained imported after guest unmap");
}

void CheckDirectRelease(AgcDriver::VulkanDevice& device) {
    std::int64_t physical = 0;
    Require(sceKernelAllocateDirectMemory(0, 0x7fffffffffll, BlockBytes, BlockBytes, 0, &physical) == 0, "guest direct allocation failed");
    void* first = nullptr;
    void* second = nullptr;
    Require(sceKernelMapDirectMemory(&first, BlockBytes, 0x33, 0, physical, BlockBytes) == 0, "first direct view mapping failed");
    Require(sceKernelMapDirectMemory(&second, BlockBytes, 0x33, 0, physical, BlockBytes) == 0 && second != first, "second direct view mapping failed");
    {
        GuestAllocations::Mutation mutation;
        const auto firstRange = mutation.Find(first);
        const auto secondRange = mutation.Find(second);
        Require(firstRange.readable && firstRange.writable && firstRange.gpu && secondRange.readable && secondRange.writable && secondRange.gpu, "direct views did not register their requested CPU and GPU access");
    }
    std::memset(first, 0, BlockBytes);
    static_cast<std::uint8_t*>(first)[BlockBytes - 1] = 0x5a;
    Require(static_cast<const std::uint8_t*>(second)[BlockBytes - 1] == 0x5a, "direct views do not share their physical backing");
    auto* destination = Map();
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        Fill(device, first);
        Require(device.FillBuffer(Address(second) + FillBytes, FillBytes, Pattern), "second direct view was not imported for its GPU fill");
        const auto firstCopy = device.CopyBuffer(Address(destination), Address(first), FillBytes, 0, BlockBytes, 0, 0, 0, [](std::span<const std::byte>, std::uint64_t) {});
        const auto secondCopy = device.CopyBuffer(Address(destination) + FillBytes, Address(second) + FillBytes, FillBytes, 0, BlockBytes, 0, 0, 0, [](std::span<const std::byte>, std::uint64_t) {});
        Require(firstCopy.path == 1 && secondCopy.path == 1, "direct views were not read through GPU import transfers");
        Require(Imported(device, first) && Imported(device, second), "both direct view imports were not retained before release");
    }
    Require(sceKernelReleaseDirectMemory(physical, BlockBytes) == 0, "guest direct release failed");
    Require(!Imported(device, first) && !Imported(device, second), "direct release retained an imported virtual view");
    Require(!AgcDriver::Graphics::RegisteredReadableCovers(Address(first), BlockBytes) && !AgcDriver::Graphics::RegisteredReadableCovers(Address(second), BlockBytes), "direct release left a virtual view registered");
    CheckPattern(destination);
    CheckPattern(static_cast<const std::byte*>(destination) + FillBytes);
    Require(Imported(device, destination), "direct release retired the unrelated copy destination");
    Require(munmap_nid_postfix(destination, BlockBytes) == 0, "direct release copy destination unmap failed");
    Require(!Imported(device, destination), "direct release copy destination remained imported after unmap");
}

}

int main(int argc, char** argv) {
    try {
        Require(argc <= 2, "expected mappings, heap, locked, copy or direct test mode");
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!GuestMemoryLifetimeHasHostImports(device->Device())) return VulkanTestSkipped;
        const std::string_view mode = argc == 2 ? argv[1] : "mappings";
        if (mode == "mappings") CheckMappings(*device);
        else if (mode == "heap") CheckHeap(*device);
        else if (mode == "locked") CheckLockedMutation(*device);
        else if (mode == "copy") CheckCopy(*device);
        else if (mode == "direct") CheckDirectRelease(*device);
        else throw std::runtime_error("unknown guest memory lifetime test mode");
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            device->WaitIdle();
        }
        device.reset();
        std::puts("guest memory host import lifetime passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "guest memory host import lifetime: %s\n", error.what());
        return 1;
    }
}
