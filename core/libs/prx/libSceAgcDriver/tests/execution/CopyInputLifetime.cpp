#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <new>
#include <vector>

namespace {

namespace Graphics = AgcDriver::Graphics;
using Graphics::Require;

PFN_vkGetDeviceProcAddr originalProc = nullptr;
PFN_vkAllocateMemory originalAllocate = nullptr;
PFN_vkFreeMemory originalFree = nullptr;
PFN_vkCmdCopyBuffer originalCopy = nullptr;
bool refuseReadback = false;
bool copied = false;
unsigned refusals = 0;
unsigned copies = 0;
unsigned allocations = 0;
VkBuffer destination = VK_NULL_HANDLE;

VKAPI_ATTR VkResult VKAPI_CALL Allocate(VkDevice device, const VkMemoryAllocateInfo* info, const VkAllocationCallbacks* callbacks, VkDeviceMemory* memory) {
    if (refuseReadback && copied) {
        ++refusals;
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    }
    const auto result = originalAllocate(device, info, callbacks, memory);
    if (result == VK_SUCCESS) ++allocations;
    return result;
}

VKAPI_ATTR void VKAPI_CALL Free(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* callbacks) {
    if (memory != VK_NULL_HANDLE) --allocations;
    originalFree(device, memory, callbacks);
}

VKAPI_ATTR void VKAPI_CALL Copy(VkCommandBuffer commands, VkBuffer source, VkBuffer target, std::uint32_t count, const VkBufferCopy* regions) {
    originalCopy(commands, source, target, count, regions);
    if (!copied) {
        Require(count == 1 && regions[0].size == 32 && regions[0].srcOffset == 0 && regions[0].dstOffset == 0, "expected one exact 32-byte input copy");
        destination = target;
        copied = true;
    }
    ++copies;
}

PFN_vkVoidFunction VKAPI_CALL Resolve(VkDevice device, const char* name) {
    if (std::strcmp(name, "vkAllocateMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&Allocate);
    if (std::strcmp(name, "vkFreeMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&Free);
    if (std::strcmp(name, "vkCmdCopyBuffer") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&Copy);
    return originalProc(device, name);
}

void Scenario(Graphics::Context context, bool fail) {
    constexpr std::size_t blockBytes = 65536;
    constexpr auto usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    constexpr auto properties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    auto block = std::unique_ptr<std::byte[], void(*)(std::byte*)>(static_cast<std::byte*>(::operator new(blockBytes, std::align_val_t{blockBytes})), [](std::byte* value) { ::operator delete(value, std::align_val_t{blockBytes}); });
    std::fill_n(block.get(), blockBytes, std::byte{0x11});
    std::fill_n(block.get() + 4, 32, std::byte{0x5a});
    GuestAllocations::Mutation().Add(block.get(), blockBytes, true, false, true);
    const auto unregister = [&](std::byte*) { GuestAllocations::Mutation().Remove(block.get()); };
    std::unique_ptr<std::byte, decltype(unregister)> registered(block.get(), unregister);
    context.functions = nullptr;
    context.deviceProc = &Resolve;
    context.limits.minStorageBufferOffsetAlignment = 256;
    context.limits.nonCoherentAtomSize = blockBytes;
    context.bufferPool = std::make_shared<Graphics::BufferPool>(context);
    copied = false;
    copies = refusals = allocations = 0;
    destination = VK_NULL_HANDLE;
    refuseReadback = fail;
    bool caught = false;
    bool earlyReuse = false;
    {
        Graphics::Recorder recorder(context);
        recorder.Activate();
        try {
            Graphics::GuestBufferMemory memory(context);
            memory.AddReadable(reinterpret_cast<std::uintptr_t>(block.get()) + 4, 32);
            memory.Upload(false);
        } catch (const std::exception&) {
            caught = true;
        }
        refuseReadback = false;
        Require(copied && destination != VK_NULL_HANDLE, "the fixture did not record an input copy");
        Require(fail ? caught && refusals != 0 && copies == 1 : !caught && refusals == 0 && copies == 2, "the diagnostic readback did not take the expected allocation path");
        std::vector<Graphics::BufferAllocation> taken;
        while (const auto allocation = context.bufferPool->Take(32, usage, properties)) {
            earlyReuse |= allocation->buffer == destination;
            taken.push_back(*allocation);
        }
        for (const auto& allocation : taken) context.bufferPool->Put(allocation);
        VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(recorder.Commands(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
        recorder.Sync();
    }
    bool checked = false;
    std::vector<Graphics::BufferAllocation> completed;
    while (const auto allocation = context.bufferPool->Take(32, usage, properties)) {
        if (allocation->buffer == destination) {
            Require(allocation->mapping != nullptr, "the completed destination is not mapped");
            const auto* bytes = static_cast<const std::byte*>(allocation->mapping);
            Require(std::all_of(bytes, bytes + 32, [](std::byte value) { return value == std::byte{0x5a}; }), "GPU input copy data changed");
            checked = true;
        }
        completed.push_back(*allocation);
    }
    for (const auto& allocation : completed) context.bufferPool->Put(allocation);
    for (std::size_t index = 0; index < blockBytes; ++index) Require(block[index] == (index >= 4 && index < 36 ? std::byte{0x5a} : std::byte{0x11}), "the copy changed guest memory or surrounding guards");
    registered.reset();
    Graphics::ClearHostImports(context.device);
    context.bufferPool.reset();
    Require(allocations == 0, "the input copy leaked Vulkan allocations");
    std::printf("copy-input lifetime: fail=%d refusals=%u early_reuse=%d GPU_data_checked=%d\n", fail, refusals, earlyReuse, checked);
    Require(checked && !earlyReuse, "a recorded copy destination became reusable before its fence");
}

}

int main() {
    try {
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        auto context = device->graphicsContext();
        if (context.hostImportAlignment == 0 || context.hostImportAlignment > 65536) {
            Require(std::getenv("ANYPS5_REQUIRE_VULKAN") == nullptr, "the lifetime test requires host imports with at most 64 KiB alignment");
            std::puts("skipped, suitable host imports are unavailable");
            return VulkanTestSkipped;
        }
        Graphics::ClearHostImports(context.device);
        originalProc = context.deviceProc;
        originalAllocate = context.Function<PFN_vkAllocateMemory>("vkAllocateMemory");
        originalFree = context.Function<PFN_vkFreeMemory>("vkFreeMemory");
        originalCopy = context.Function<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer");
        Scenario(context, false);
        Scenario(context, true);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
