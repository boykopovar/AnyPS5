#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "AllocationFailure.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <deque>
#include <map>
#include <memory>
#include <new>
#include <set>
#include <string>
#include <string_view>
#include <vector>

using namespace AgcDriver::Graphics;
using AgcDriver::Tests::SetAllocationFailureCountdown;

namespace {

thread_local int failAfterMap = -1;

int failures = 0;

void Expect(bool condition, const std::string& what) {
    if (condition) return;
    std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    ++failures;
}

constexpr std::size_t MiB = std::size_t{1} << 20u;
constexpr VkMemoryPropertyFlags HostVisible = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

struct MockDevice {
    std::uint64_t next = 1;
    std::map<VkBuffer, VkDeviceSize> sizes;
    std::map<VkBuffer, VkBufferUsageFlags> usages;
    std::map<VkDeviceMemory, std::vector<std::byte>> hostMemory;
    std::uint64_t allocations = 0;
    std::uint64_t frees = 0;
    std::uint64_t destroyedBuffers = 0;
    VkDeviceSize liveBytes = 0;
    std::map<VkDeviceMemory, VkDeviceSize> memoryBytes;
    std::map<VkDeviceMemory, std::uint32_t> memoryTypes;
    std::map<VkBuffer, std::pair<VkDeviceMemory, VkDeviceSize>> bound;
    std::set<VkBuffer> liveBuffers;
    std::deque<VkResult> results;
    VkDeviceSize limit = 0;
    VkResult pressureResult = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    std::uint64_t attempts = 0;
    VkResult mapResult = VK_SUCCESS;
    VkResult bindResult = VK_SUCCESS;
    unsigned binds = 0;
    unsigned maps = 0;
    unsigned unmaps = 0;
};

MockDevice mock;

VKAPI_ATTR VkResult VKAPI_CALL mockCreateBuffer(VkDevice, const VkBufferCreateInfo* info, const VkAllocationCallbacks*, VkBuffer* buffer) {
    *buffer = reinterpret_cast<VkBuffer>(static_cast<std::uintptr_t>(mock.next++));
    mock.sizes[*buffer] = info->size;
    mock.usages[*buffer] = info->usage;
    mock.liveBuffers.insert(*buffer);
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockGetBufferMemoryRequirements(VkDevice, VkBuffer buffer, VkMemoryRequirements* requirements) {
    *requirements = {mock.sizes.at(buffer), 256, 3};
}

VKAPI_ATTR VkResult VKAPI_CALL mockAllocateMemory(VkDevice, const VkMemoryAllocateInfo* info, const VkAllocationCallbacks*, VkDeviceMemory* memory) {
    ++mock.attempts;
    auto result = VK_SUCCESS;
    if (!mock.results.empty()) {
        result = mock.results.front();
        mock.results.pop_front();
    }
    if (result == VK_SUCCESS && mock.limit != 0 && mock.liveBytes + info->allocationSize > mock.limit) result = mock.pressureResult;
    if (result != VK_SUCCESS) {
        *memory = reinterpret_cast<VkDeviceMemory>(std::uintptr_t{0xdeadbeef});
        return result;
    }
    *memory = reinterpret_cast<VkDeviceMemory>(static_cast<std::uintptr_t>(mock.next++));
    if (info->memoryTypeIndex == 1) mock.hostMemory[*memory].resize(info->allocationSize);
    mock.memoryBytes[*memory] = info->allocationSize;
    mock.memoryTypes[*memory] = info->memoryTypeIndex;
    mock.liveBytes += info->allocationSize;
    ++mock.allocations;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockBindBufferMemory(VkDevice, VkBuffer buffer, VkDeviceMemory memory, VkDeviceSize offset) {
    ++mock.binds;
    if (mock.bindResult != VK_SUCCESS) return mock.bindResult;
    if (offset + mock.sizes.at(buffer) > mock.memoryBytes.at(memory)) return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    mock.bound[buffer] = {memory, offset};
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL mockMapMemory(VkDevice, VkDeviceMemory memory, VkDeviceSize, VkDeviceSize, VkMemoryMapFlags, void** data) {
    if (mock.mapResult != VK_SUCCESS) return mock.mapResult;
    *data = mock.hostMemory.at(memory).data();
    ++mock.maps;
    SetAllocationFailureCountdown(failAfterMap);
    failAfterMap = -1;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL mockUnmapMemory(VkDevice, VkDeviceMemory) { ++mock.unmaps; }

VKAPI_ATTR void VKAPI_CALL mockDestroyBuffer(VkDevice, VkBuffer buffer, const VkAllocationCallbacks*) {
    Expect(mock.liveBuffers.erase(buffer) == 1, "an unknown buffer was destroyed");
    ++mock.destroyedBuffers;
}

VKAPI_ATTR void VKAPI_CALL mockFreeMemory(VkDevice, VkDeviceMemory memory, const VkAllocationCallbacks*) {
    if (!mock.memoryBytes.contains(memory)) {
        Expect(false, "an undefined failed allocation output was freed");
        return;
    }
    mock.liveBytes -= mock.memoryBytes.at(memory);
    mock.memoryBytes.erase(memory);
    mock.hostMemory.erase(memory);
    ++mock.frees;
}

VKAPI_ATTR VkDeviceAddress VKAPI_CALL mockGetBufferDeviceAddress(VkDevice, const VkBufferDeviceAddressInfo* info) {
    return 0x100000000000ULL + reinterpret_cast<std::uintptr_t>(info->buffer) * 0x10000;
}

PFN_vkVoidFunction VKAPI_CALL mockProc(VkDevice, const char* name) {
    static const std::map<std::string_view, PFN_vkVoidFunction> table{
        {"vkCreateBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockCreateBuffer)},
        {"vkGetBufferMemoryRequirements", reinterpret_cast<PFN_vkVoidFunction>(mockGetBufferMemoryRequirements)},
        {"vkAllocateMemory", reinterpret_cast<PFN_vkVoidFunction>(mockAllocateMemory)},
        {"vkBindBufferMemory", reinterpret_cast<PFN_vkVoidFunction>(mockBindBufferMemory)},
        {"vkMapMemory", reinterpret_cast<PFN_vkVoidFunction>(mockMapMemory)},
        {"vkUnmapMemory", reinterpret_cast<PFN_vkVoidFunction>(mockUnmapMemory)},
        {"vkDestroyBuffer", reinterpret_cast<PFN_vkVoidFunction>(mockDestroyBuffer)},
        {"vkFreeMemory", reinterpret_cast<PFN_vkVoidFunction>(mockFreeMemory)},
        {"vkGetBufferDeviceAddressKHR", reinterpret_cast<PFN_vkVoidFunction>(mockGetBufferDeviceAddress)},
    };
    const auto it = table.find(name);
    return it == table.end() ? nullptr : it->second;
}

VkDeviceMemory MemoryOf(VkBuffer buffer) {
    return mock.bound.at(buffer).first;
}

VkDeviceSize ClassOf(VkBuffer buffer) {
    return mock.memoryBytes.at(MemoryOf(buffer));
}

Context mockContext() {
    Context context{};
    context.deviceProc = mockProc;
    context.memory.memoryTypeCount = 2;
    context.memory.memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    context.memory.memoryTypes[1].propertyFlags = HostVisible | VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    context.limits.maxMemoryAllocationCount = 1u << 20u;
    context.bufferDeviceAddress = true;
    return context;
}

VkDeviceSize reportedBudget = 0;
VkDeviceSize reportedUsage = 0;

VKAPI_ATTR void VKAPI_CALL mockMemoryProperties2(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties2* properties) {
    auto* reported = static_cast<VkPhysicalDeviceMemoryBudgetPropertiesEXT*>(properties->pNext);
    reported->heapBudget[0] = reportedBudget;
    reported->heapUsage[0] = reportedUsage;
}

void HostPlacement() {
    mock = MockDevice{};
    auto context = mockContext();
    context.physical = reinterpret_cast<VkPhysicalDevice>(std::uintptr_t{1});
    context.memoryProperties2 = mockMemoryProperties2;
    context.memory.memoryHeapCount = 2;
    context.memory.memoryHeaps[0] = {2048 * MiB, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT};
    context.memory.memoryHeaps[1] = {16384 * MiB, 0};
    context.memory.memoryTypes[0].heapIndex = 0;
    context.memory.memoryTypes[1].heapIndex = 1;
    constexpr auto usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    reportedBudget = 1620 * MiB;
    reportedUsage = 1600 * MiB;
    VkDeviceMemory placed = VK_NULL_HANDLE;
    {
        DeviceBuffer large(context, 176 * MiB, usage);
        placed = MemoryOf(large.Handle());
        Expect(mock.memoryTypes.at(placed) == 1, "a device buffer over the device-local budget was not placed in the host memory type");
    }
    Expect(!mock.memoryBytes.contains(placed) && mock.liveBuffers.empty(), "a device buffer placed in host memory was kept in the device pool after its release");
    reportedUsage = 200 * MiB;
    VkDeviceMemory device = VK_NULL_HANDLE;
    {
        DeviceBuffer large(context, 176 * MiB, usage);
        device = MemoryOf(large.Handle());
        Expect(mock.memoryTypes.at(device) == 0, "a device buffer within the device-local budget was not placed in device-local memory");
    }
    Expect(mock.memoryBytes.contains(device), "a device-local buffer was not kept in the device pool after its release");
    DeviceBuffer again(context, 176 * MiB, usage);
    Expect(MemoryOf(again.Handle()) == device, "a retained device-local buffer was not reused");
}

void SizeClassesAndDirections() {
    mock = MockDevice{};
    auto context = mockContext();
    VkDeviceMemory first = VK_NULL_HANDLE;
    {
        DeviceBuffer upload(context, 10 * MiB + 4096, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        first = MemoryOf(upload.Handle());
        Expect(mock.sizes.at(upload.Handle()) == 10 * MiB + 4096, "a 10 MiB + 4 KiB device buffer was created with " + std::to_string(mock.sizes.at(upload.Handle())) + " bytes, not the requested size");
        Expect(ClassOf(upload.Handle()) == 11 * MiB, "a 10 MiB + 4 KiB device buffer has " + std::to_string(ClassOf(upload.Handle())) + " bytes of memory, not its 11 MiB class");
        constexpr VkBufferUsageFlags all = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        Expect((mock.usages.at(upload.Handle()) & all) == all, "a device buffer was created without storage and both transfer directions");
        Expect(upload.Size() == 10 * MiB + 4096, "a device buffer reports its class instead of the requested size");
    }
    const auto made = mock.allocations;
    DeviceBuffer writeBack(context, 10 * MiB + 512 * 1024, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    Expect(MemoryOf(writeBack.Handle()) == first && mock.allocations == made, "a retained 11 MiB device buffer did not serve a write-back of another size and direction in its class");
    Expect(writeBack.Size() == 10 * MiB + 512 * 1024, "a reused device buffer reports the retained size");
    Expect(mock.sizes.at(writeBack.Handle()) == 10 * MiB + 512 * 1024, "a reused device buffer kept the VkBuffer of the earlier size");
}

void SmallDeviceRetention() {
    mock = MockDevice{};
    {
        auto context = mockContext();
        constexpr auto usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        const auto deviceBudget = BufferPool::DeviceBudget(context.memory);
        const auto bytes = static_cast<std::size_t>((deviceBudget != 0 ? deviceBudget : 512 * MiB) / 2) + MiB;
        VkDeviceMemory small = VK_NULL_HANDLE;
        {
            DeviceBuffer buffer(context, 256, usage);
            small = MemoryOf(buffer.Handle());
        }
        std::vector<std::unique_ptr<DeviceBuffer>> large;
        for (int i = 0; i < 2; ++i) large.push_back(std::make_unique<DeviceBuffer>(context, bytes, usage));
        large.clear();
        Expect(mock.frees != 0, "large device buffers did not exercise pool budget eviction");
        const auto made = mock.allocations;
        const bool isolated = std::getenv("APS5_BUFFER_POOL_SHARED") == nullptr;
        DeviceBuffer again(context, 256, usage);
        Expect((MemoryOf(again.Handle()) == small) == isolated, "small device buffer retention did not follow the separate or shared pool budget");
        Expect(mock.allocations == made + (isolated ? 0 : 1), "large device buffer churn changed small buffer allocation reuse");
    }
    Expect(mock.liveBytes == 0 && mock.liveBuffers.empty(), "device pool tier retention leaked Vulkan objects");
}

void LargerClass() {
    mock = MockDevice{};
    auto context = mockContext();
    constexpr auto usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkDeviceMemory twelve = VK_NULL_HANDLE;
    {
        DeviceBuffer large(context, 12 * MiB, usage);
        twelve = MemoryOf(large.Handle());
    }
    {
        DeviceBuffer smaller(context, 7 * MiB, usage);
        Expect(MemoryOf(smaller.Handle()) == twelve, "a 7 MiB request did not take the retained 12 MiB buffer");
    }
    const auto made = mock.allocations;
    {
        DeviceBuffer tooSmall(context, 5 * MiB, usage);
        Expect(MemoryOf(tooSmall.Handle()) != twelve && mock.allocations == made + 1, "a 5 MiB request took a retained buffer of more than twice its size");
        Expect(mock.sizes.at(tooSmall.Handle()) == 5 * MiB, "a 5 MiB device buffer was not created at its own class");
    }
    DeviceBuffer again(context, 12 * MiB, usage);
    Expect(MemoryOf(again.Handle()) == twelve, "a buffer served to a smaller request did not return to its own 12 MiB class");
    DeviceBuffer best(context, 4 * MiB + 1, usage);
    Expect(MemoryOf(best.Handle()) != twelve && ClassOf(best.Handle()) == 5 * MiB, "a 4 MiB + 1 request did not take the smallest retained class that fits (5 MiB)");
}

void KeptUntilFence() {
    mock = MockDevice{};
    auto context = mockContext();
    constexpr auto usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    std::vector<std::shared_ptr<DeviceBuffer>> batch;
    for (int i = 0; i < 3; ++i) batch.push_back(std::make_shared<DeviceBuffer>(context, 6 * MiB + static_cast<std::size_t>(i + 1) * 4096, usage));
    for (std::size_t i = 0; i < batch.size(); ++i) {
        for (std::size_t j = i + 1; j < batch.size(); ++j) Expect(batch[i]->Handle() != batch[j]->Handle(), "two buffers of one unfinished batch share a VkBuffer");
    }
    const auto kept = batch.front()->Handle();
    const auto made = mock.allocations;
    {
        DeviceBuffer during(context, 6 * MiB, usage);
        Expect(during.Handle() != kept && mock.allocations == made + 1, "a buffer kept by an unfinished batch was handed out again");
    }
    batch.clear();
    std::vector<std::unique_ptr<DeviceBuffer>> after;
    for (int i = 0; i < 3; ++i) after.push_back(std::make_unique<DeviceBuffer>(context, 6 * MiB + 100, usage));
    after.push_back(std::make_unique<DeviceBuffer>(context, 6 * MiB, usage));
    Expect(mock.allocations == made + 1, "the buffers released after the batch fence were not reused (" + std::to_string(mock.allocations - made - 1) + " new allocations)");
}

void Budget() {
    mock = MockDevice{};
    auto context = mockContext();
    constexpr auto usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    std::vector<std::unique_ptr<DeviceBuffer>> live;
    for (int i = 0; i < 12; ++i) live.push_back(std::make_unique<DeviceBuffer>(context, (48 + static_cast<std::size_t>(i) * 3) * MiB, usage));
    const auto first = MemoryOf(live.front()->Handle());
    VkDeviceSize madeBytes = 0;
    for (const auto& buffer : live) madeBytes += ClassOf(buffer->Handle());
    Expect(madeBytes > 512 * MiB, "the budget case does not exceed the budget");
    for (auto& buffer : live) buffer.reset();
    Expect(mock.frees != 0 && mock.liveBytes <= 512 * MiB, "the device tier retains " + std::to_string(mock.liveBytes / MiB) + " MiB after " + std::to_string(mock.frees) + " evictions, over its 512 MiB budget");
    DeviceBuffer probe(context, 48 * MiB, usage);
    Expect(MemoryOf(probe.Handle()) != first, "the least recently used device buffer survived the eviction");
}

void AddressAndHostUnchanged() {
    mock = MockDevice{};
    auto context = mockContext();
    VkDeviceMemory scratch = VK_NULL_HANDLE;
    {
        DeviceBuffer buffer(context, 2 * MiB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        scratch = MemoryOf(buffer.Handle());
    }
    Buffer addressed(context, 2 * MiB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    Expect(MemoryOf(addressed.Handle()) != scratch && addressed.DeviceAddress() != 0, "an addressable device-local buffer took a scratch buffer without a device address");
    Buffer shadow(context, 2 * MiB - 256, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    Expect(MemoryOf(shadow.Handle()) == scratch, "a device-local staging buffer did not share the scratch buffers' class");
    VkBuffer host = VK_NULL_HANDLE;
    {
        Buffer copy(context, 10 * MiB + 4096, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, HostVisible);
        host = copy.Handle();
        Expect(mock.sizes.at(host) == 10 * MiB + 4096, "a large host buffer was rounded to a class");
        Expect(mock.usages.at(host) == VK_BUFFER_USAGE_TRANSFER_SRC_BIT, "a host buffer was created with more usages than asked");
    }
    Buffer other(context, 10 * MiB + 8192, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, HostVisible);
    Expect(other.Handle() != host, "a large host buffer of another exact size was reused");
    Buffer same(context, 10 * MiB + 4096, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, HostVisible);
    Expect(same.Handle() == host, "a large host buffer of the same exact size was not reused");
}

void SmallDeviceClasses() {
    mock = MockDevice{};
    auto context = mockContext();
    constexpr auto usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkDeviceMemory mebibyte = VK_NULL_HANDLE;
    VkDeviceMemory half = VK_NULL_HANDLE;
    {
        DeviceBuffer one(context, MiB, usage);
        mebibyte = MemoryOf(one.Handle());
    }
    {
        DeviceBuffer smaller(context, 300 * 1024, usage);
        Expect(MemoryOf(smaller.Handle()) != mebibyte && ClassOf(smaller.Handle()) == 512 * 1024, "a 300 KiB request took a 1 MiB buffer or skipped its power-of-two class");
        half = MemoryOf(smaller.Handle());
    }
    DeviceBuffer again(context, 260 * 1024, usage);
    Expect(MemoryOf(again.Handle()) == half, "a 260 KiB request did not take the retained 512 KiB buffer");
    constexpr auto indirect = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkBuffer tiny = VK_NULL_HANDLE;
    {
        DeviceBuffer args(context, 20, indirect);
        tiny = args.Handle();
    }
    const auto made = mock.allocations;
    DeviceBuffer nextArgs(context, 20, indirect);
    Expect(nextArgs.Handle() == tiny && mock.allocations == made, "a 20-byte device request did not reuse the retained 256-byte buffer of its class");
}

void SlabMetadataFailure() {
    unsigned rejected = 0;
    for (int point = 0; point < 16; ++point) {
        mock = MockDevice{};
        bool failed = false;
        {
            const auto context = mockContext();
            BufferPool pool(context);
            SlabSlot slot{};
            failAfterMap = point;
            try {
                slot = pool.TakeSlot(context, 1, 512, false);
            } catch (const std::bad_alloc&) {
                failed = true;
                ++rejected;
            }
            SetAllocationFailureCountdown(-1);
            failAfterMap = -1;
            if (failed) {
                Expect(mock.allocations == 1, "metadata failure did not follow a successful Vulkan allocation");
                Expect(mock.frees == 1 && mock.liveBytes == 0, "metadata failure leaked the slab's device memory at allocation " + std::to_string(point));
                Expect(mock.maps == 1 && mock.unmaps == 1, "metadata failure did not unmap its slab");
                Expect(pool.SlabBlocks() == 0, "metadata failure published an unusable slab");
                slot = pool.TakeSlot(context, 1, 512, false);
            }
            Expect(slot.mapping != nullptr, "slab retry did not return mapped memory");
            pool.PutSlot(slot.memory, slot.offset);
        }
        Expect(mock.liveBytes == 0 && mock.allocations == mock.frees, "metadata failure retained device memory after pool destruction");
        if (!failed) {
            Expect(rejected != 0, "slab metadata allocation failures were not exercised");
            std::printf("slab metadata failures checked: %u\n", rejected);
            return;
        }
    }
    Expect(false, "slab allocation never reached the success control");
}

void SlabMapFailure() {
    mock = MockDevice{};
    {
        const auto context = mockContext();
        BufferPool pool(context);
        mock.mapResult = VK_ERROR_MEMORY_MAP_FAILED;
        bool failed = false;
        try {
            static_cast<void>(pool.TakeSlot(context, 1, 512, false));
        } catch (const std::runtime_error& error) {
            failed = std::strstr(error.what(), "vkMapMemory buffer slab") != nullptr;
        }
        Expect(failed, "the slab mapping failure was not reported");
        Expect(mock.allocations == 1 && mock.frees == 1 && mock.liveBytes == 0, "a failed slab mapping did not release exactly its allocation");
        Expect(mock.maps == 0 && mock.unmaps == 0 && pool.SlabBlocks() == 0, "a failed mapping published a slab or unmapped invalid memory");
        mock.mapResult = VK_SUCCESS;
        const auto slot = pool.TakeSlot(context, 1, 512, false);
        pool.PutSlot(slot.memory, slot.offset);
    }
    Expect(mock.liveBytes == 0 && mock.allocations == mock.frees && mock.maps == mock.unmaps, "a failed mapping made a later slab leak");
}

void Slabs() {
    Expect(BufferPool::SlabEligible(512, 256, 512, 64), "a size class with a smaller alignment and atom was refused a slab");
    Expect(!BufferPool::SlabEligible(512, 1024, 512, 64), "a size class below its alignment was given a slab");
    Expect(!BufferPool::SlabEligible(512, 256, 640, 64), "a buffer needing more than its class was given a slab");
    Expect(!BufferPool::SlabEligible(512, 256, 512, 1024), "a size class below the non-coherent atom was given a slab");
    Expect(!BufferPool::SlabEligible(MiB, 256, MiB, 64), "an exact-size buffer was given a slab");
    mock = MockDevice{};
    {
        auto context = mockContext();
        context.limits.nonCoherentAtomSize = 64;
        constexpr VkBufferUsageFlags storage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        {
            Buffer a(context, 300, storage);
            Buffer b(context, 300, storage);
            Buffer c(context, 260, storage);
            Expect(mock.allocations == 1, "buffers of one size class did not share one memory block");
            const auto block = mock.bound.at(a.Handle()).first;
            std::set<VkDeviceSize> offsets;
            for (auto* buffer : {&a, &b, &c}) {
                const auto [memory, offset] = mock.bound.at(buffer->Handle());
                Expect(memory == block, "buffers of one size class were bound to different blocks");
                Expect(offset % 512 == 0, "a slab slot is not aligned to its size class");
                Expect(offsets.insert(offset).second, "two live buffers share a slab slot");
                Expect(buffer->Bytes().data() == mock.hostMemory.at(memory).data() + offset, "a slab buffer's mapping is not its slot");
            }
            std::memset(a.Bytes().data(), 0x11, a.Bytes().size());
            std::memset(b.Bytes().data(), 0x22, b.Bytes().size());
            std::memset(c.Bytes().data(), 0x33, c.Bytes().size());
            Expect(std::all_of(a.Bytes().begin(), a.Bytes().end(), [](std::byte value) { return value == std::byte{0x11}; }), "a neighbouring slab buffer overwrote another");
            Expect(std::all_of(b.Bytes().begin(), b.Bytes().end(), [](std::byte value) { return value == std::byte{0x22}; }), "a neighbouring slab buffer overwrote another");
            Buffer larger(context, 600, storage);
            Expect(mock.allocations == 2 && mock.bound.at(larger.Handle()).first != block, "another size class did not get a block of its own");
        }
        Expect(mock.frees == 0, "releasing slab buffers freed device memory");
        Buffer again(context, 300, storage);
        Expect(mock.allocations == 2, "a released slab buffer was not reused");
    }
    Expect(mock.liveBytes == 0, "slab blocks outlived their pool");
    mock = MockDevice{};
    {
        auto context = mockContext();
        BufferPool pool(context);
        constexpr std::size_t slot = 512 * 1024;
        const auto perBlock = static_cast<std::size_t>(BufferPool::SlabBlockBytes(slot) / slot);
        std::vector<SlabSlot> slots;
        for (std::size_t i = 0; i <= perBlock; ++i) slots.push_back(pool.TakeSlot(context, 1, slot, false));
        Expect(mock.allocations == 2 && pool.SlabBlocks() == 2, "a full block did not open a second one");
        std::set<std::pair<VkDeviceMemory, VkDeviceSize>> distinct;
        for (const auto& taken : slots) distinct.insert({taken.memory, taken.offset});
        Expect(distinct.size() == slots.size(), "a slab handed out one slot twice");
        for (const auto& taken : slots) pool.PutSlot(taken.memory, taken.offset);
        Expect(mock.frees == 1 && pool.SlabBlocks() == 1, "emptied blocks were not freed down to one spare");
        const auto reused = pool.TakeSlot(context, 1, slot, false);
        Expect(mock.allocations == 2, "the spare block was not reused");
        pool.PutSlot(reused.memory, reused.offset);
    }
    Expect(mock.liveBytes == 0, "slab blocks outlived their pool");
}

void AllocationRecovery(VkResult pressureResult, bool host) {
    mock = MockDevice{};
    {
        auto context = mockContext();
        const auto make = [&](std::size_t bytes) -> std::shared_ptr<void> {
            if (host) return std::make_shared<Buffer>(context, bytes, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            return std::make_shared<DeviceBuffer>(context, bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        };
        auto active = make(2 * MiB);
        const auto activeBuffer = *mock.liveBuffers.begin();
        const auto activeMemory = mock.memoryBytes.begin()->first;
        { auto idle = make(2 * MiB); }
        mock.limit = 6 * MiB;
        mock.pressureResult = pressureResult;
        const auto before = mock.attempts;
        try {
            auto recovered = make(4 * MiB);
            Expect(mock.attempts == before + 2, "pool pressure did not trigger exactly one allocation retry");
            Expect(mock.frees == 1 && mock.liveBytes == 6 * MiB, "recovery did not release the idle allocation");
            Expect(mock.liveBuffers.contains(activeBuffer) && mock.memoryBytes.contains(activeMemory), "recovery destroyed an active allocation");
        } catch (const std::exception& error) {
            Expect(false, std::string("idle memory did not permit recovery: ") + error.what());
        }
    }
    Expect(mock.liveBytes == 0 && mock.liveBuffers.empty(), "recovery leaked a resource");
}

void SlabAllocationRecovery() {
    mock = MockDevice{};
    {
        auto context = mockContext();
        Buffer active(context, 300, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        const auto activeMemory = mock.bound.at(active.Handle()).first;
        { DeviceBuffer idle(context, 2 * MiB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT); }
        mock.limit = 4 * MiB;
        const auto before = mock.attempts;
        try {
            Buffer recovered(context, 600, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            Expect(mock.attempts == before + 2 && mock.frees == 1, "a new slab block did not retry after reclaiming an idle allocation");
            Expect(context.bufferPool->SlabBlocks() == 2 && mock.memoryBytes.contains(activeMemory), "slab allocation recovery discarded its live block");
        } catch (const std::exception& error) {
            Expect(false, std::string("slab allocation did not recover: ") + error.what());
        }
    }
    Expect(mock.liveBytes == 0 && mock.liveBuffers.empty(), "slab recovery leaked a resource");
}

void ReclaimSlabs() {
    mock = MockDevice{};
    {
        auto context = mockContext();
        Buffer active(context, 300, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        const auto activeMemory = mock.bound.at(active.Handle()).first;
        std::memset(active.Bytes().data(), 0x5a, active.Bytes().size());
        {
            Buffer sameBlock(context, 260, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
            Buffer otherBlock(context, 600, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        }
        mock.limit = 6 * MiB;
        const auto before = mock.attempts;
        try {
            Buffer recovered(context, 4 * MiB, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            Expect(mock.attempts == before + 2 && mock.frees == 1, "reclaim did not free the unused slab block");
            Expect(context.bufferPool->SlabBlocks() == 1 && mock.memoryBytes.contains(activeMemory), "reclaim freed a slab with a live slot");
            Expect(std::all_of(active.Bytes().begin(), active.Bytes().end(), [](std::byte value) { return value == std::byte{0x5a}; }), "reclaim changed a live slab buffer");
        } catch (const std::exception& error) {
            Expect(false, std::string("empty slab did not permit recovery: ") + error.what());
        }
    }
    Expect(mock.liveBytes == 0 && mock.liveBuffers.empty(), "slab reclaim leaked a resource");
    mock = MockDevice{};
    {
        auto context = mockContext();
        BufferPool pool(context);
        const auto idle = pool.TakeSlot(context, 1, 512 * 1024, false);
        pool.PutSlot(idle.memory, idle.offset);
        mock.limit = 4 * MiB;
        const auto before = mock.attempts;
        try {
            const auto recovered = pool.TakeSlot(context, 1, 256 * 1024, false);
            Expect(mock.attempts == before + 2 && mock.frees == 1 && pool.SlabBlocks() == 1, "a spare slab without cached buffers was not reclaimed");
            pool.PutSlot(recovered.memory, recovered.offset);
        } catch (const std::exception& error) {
            Expect(false, std::string("spare slab did not permit recovery: ") + error.what());
        }
    }
    Expect(mock.liveBytes == 0, "spare slab recovery leaked device memory");
}

void AllocationFailure(VkResult initial, VkResult final, bool cached) {
    mock = MockDevice{};
    {
        auto context = mockContext();
        if (cached) { DeviceBuffer idle(context, 2 * MiB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT); }
        mock.results = {initial, final};
        const auto before = mock.attempts;
        std::string failure;
        try {
            DeviceBuffer buffer(context, 4 * MiB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        } catch (const std::exception& error) {
            failure = error.what();
        }
        const bool reclaimed = cached && (initial == VK_ERROR_OUT_OF_DEVICE_MEMORY || initial == VK_ERROR_OUT_OF_HOST_MEMORY);
        Expect(mock.attempts == before + (reclaimed ? 2 : 1), "allocation retries were not bounded or a non-memory failure was retried");
        Expect(failure.find("Vulkan result " + std::to_string(reclaimed ? final : initial)) != std::string::npos, "allocation failure lost its final Vulkan result");
        Expect(mock.liveBuffers.size() == (cached && !reclaimed ? 1u : 0u), "a failed allocation retained its buffer");
        Expect(mock.liveBytes == (cached && !reclaimed ? 2 * MiB : 0), "a failed allocation mishandled cached memory");
    }
    Expect(mock.liveBytes == 0 && mock.liveBuffers.empty(), "failure cleanup leaked a resource");
}

void SuccessfulAllocationKeepsPool() {
    mock = MockDevice{};
    {
        auto context = mockContext();
        { DeviceBuffer idle(context, 2 * MiB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT); }
        DeviceBuffer buffer(context, 4 * MiB, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        Expect(mock.attempts == 2 && mock.frees == 0 && mock.liveBytes == 6 * MiB, "a successful first allocation reclaimed its cache");
    }
    Expect(mock.liveBytes == 0 && mock.liveBuffers.empty(), "normal allocation leaked a resource");
}

void PartialSlabDoesNotRetry() {
    mock = MockDevice{};
    {
        auto context = mockContext();
        Buffer active(context, 300, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        { Buffer idle(context, 260, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT); }
        mock.limit = 2 * MiB;
        const auto before = mock.attempts;
        std::string failure;
        try {
            Buffer buffer(context, 2 * MiB, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        } catch (const std::exception& error) {
            failure = error.what();
        }
        Expect(failure.find("Vulkan result -2") != std::string::npos, "a live slab's memory was treated as reclaimable");
        Expect(mock.attempts == before + 1 && mock.frees == 0, "an allocation retried without freeing device memory");
        Expect(mock.liveBuffers.size() == 1 && mock.liveBuffers.contains(active.Handle()) && context.bufferPool->SlabBlocks() == 1, "reclaim changed a live slab allocation");
    }
    Expect(mock.liveBytes == 0 && mock.liveBuffers.empty(), "partial slab failure leaked a resource");
}

void DeviceBindingFailures() {
    constexpr auto bytes = 3 * MiB;
    constexpr auto usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    for (const auto result : {VK_ERROR_OUT_OF_HOST_MEMORY, VK_ERROR_OUT_OF_DEVICE_MEMORY}) {
        mock = MockDevice{};
        {
            auto context = mockContext();
            mock.bindResult = result;
            bool threw = false;
            try {
                DeviceBuffer failed(context, bytes, usage);
            } catch (const std::runtime_error& error) {
                threw = std::string(error.what()).find("Vulkan result " + std::to_string(result)) != std::string::npos;
            }
            const auto retained = mock.liveBytes;
            Expect(threw && mock.binds == 1, "a failed device buffer bind lost its error");
            Expect(retained == 0 && mock.liveBuffers.empty(), "a failed device buffer was retained for reuse");
            mock.bindResult = VK_SUCCESS;
            unsigned invalid = 0;
            for (unsigned retry = 0; retry < 3; ++retry) {
                DeviceBuffer recovered(context, bytes, usage);
                if (!mock.bound.contains(recovered.Handle())) ++invalid;
            }
            Expect(invalid == 0, "a retry reused an unbound device buffer");
            Expect(mock.binds == 2 && mock.allocations == 2, "successfully bound device buffers stopped being reused");
            std::printf("device bind result=%d retained=%llu invalid retries=%u binds=%u\n", static_cast<int>(result), static_cast<unsigned long long>(retained), invalid, mock.binds);
        }
        Expect(mock.liveBytes == 0 && mock.liveBuffers.empty(), "device bind recovery leaked a resource");
    }
}

}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--small-device-retention") {
            SmallDeviceRetention();
            return failures == 0 ? 0 : 1;
        }
        SizeClassesAndDirections();
        LargerClass();
        KeptUntilFence();
        SmallDeviceRetention();
        Budget();
        AddressAndHostUnchanged();
        SmallDeviceClasses();
        SlabMetadataFailure();
        SlabMapFailure();
        DeviceBindingFailures();
        Slabs();
        for (const auto result : {VK_ERROR_OUT_OF_DEVICE_MEMORY, VK_ERROR_OUT_OF_HOST_MEMORY}) {
            AllocationRecovery(result, false);
            AllocationRecovery(result, true);
            AllocationFailure(result, result, true);
            AllocationFailure(result, result, false);
        }
        SlabAllocationRecovery();
        ReclaimSlabs();
        AllocationFailure(VK_ERROR_DEVICE_LOST, VK_ERROR_DEVICE_LOST, true);
        AllocationFailure(VK_ERROR_OUT_OF_DEVICE_MEMORY, VK_ERROR_DEVICE_LOST, true);
        SuccessfulAllocationKeepsPool();
        PartialSlabDoesNotRetry();
        HostPlacement();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
    if (failures != 0) return 1;
    std::puts("buffer pool tests passed");
    return 0;
}
