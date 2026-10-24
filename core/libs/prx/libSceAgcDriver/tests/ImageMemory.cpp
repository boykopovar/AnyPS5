#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ImageMemory.hpp"
#include "AllocationFailure.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

using namespace AgcDriver::Graphics;
using AgcDriver::Tests::SetAllocationFailureCountdown;

namespace {

constexpr VkDeviceSize KiB = VkDeviceSize{1} << 10u;
constexpr VkDeviceSize MiB = VkDeviceSize{1} << 20u;
constexpr std::size_t RecordCount = 32768;

void Expect(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename T>
T Handle(std::size_t value) {
    return reinterpret_cast<T>(static_cast<std::uintptr_t>(value));
}

template<typename T>
std::size_t Index(T handle) {
    return static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(handle));
}

struct MemoryRecord {
    VkDeviceSize bytes = 0;
    std::uint32_t type = 0;
    VkImage dedicated = VK_NULL_HANDLE;
    bool active = false;
};

struct ImageRecord {
    VkMemoryRequirements requirements{};
    VkBool32 required = VK_FALSE;
    VkBool32 preferred = VK_FALSE;
    VkDeviceMemory boundMemory = VK_NULL_HANDLE;
    VkDeviceSize boundOffset = 0;
};

struct MockState {
    std::array<MemoryRecord, RecordCount> memories{};
    std::array<ImageRecord, RecordCount> images{};
    std::array<VkResult, 32> results{};
    std::size_t resultCount = 0;
    std::size_t resultCursor = 0;
    std::size_t nextMemory = 1;
    std::size_t nextImage = 1;
    std::size_t attempts = 0;
    std::size_t allocations = 0;
    std::size_t frees = 0;
    std::size_t live = 0;
    std::size_t invalidFrees = 0;
    std::size_t invalidBinds = 0;
    std::size_t maps = 0;
    std::size_t requirements2 = 0;
    VkDeviceSize liveBytes = 0;
    VkDeviceSize deviceLimit = 0;
    VkDeviceSize reportedBudget = 8ull << 30u;
    VkDeviceSize reportedUsage = 0;
    VkResult refusedDevice = VK_SUCCESS;
    bool failHostAfterAllocate = false;
};

MockState mock;
std::mutex mockMutex;
std::uint64_t (*reclaimAction)(std::uint64_t, bool) = nullptr;
std::size_t reclaimCalls = 0;
std::array<ImageAllocation*, 16> reclaimImages{};
std::size_t reclaimImageCount = 0;
bool reclaimReportsBytes = true;

void Clean() {
    Expect(mock.live == 0 && mock.liveBytes == 0, "image allocator leaked driver memory");
    Expect(mock.invalidFrees == 0, "image allocator freed an undefined or already-freed handle");
    Expect(mock.invalidBinds == 0, "image allocator produced an invalid or overlapping binding");
    Expect(mock.maps == 0, "image allocation mapped image memory on the host");
    Expect(mock.allocations == mock.frees, "driver allocations and frees are unbalanced");
}

void ResetMock() {
    SetAllocationFailureCountdown(-1);
    Clean();
    mock = {};
    reclaimAction = nullptr;
    reclaimCalls = 0;
    reclaimImages = {};
    reclaimImageCount = 0;
    reclaimReportsBytes = true;
}

VKAPI_ATTR VkResult VKAPI_CALL AllocateMemory(VkDevice, const VkMemoryAllocateInfo* info, const VkAllocationCallbacks*, VkDeviceMemory* memory) {
    std::lock_guard lock(mockMutex);
    ++mock.attempts;
    VkResult result = VK_SUCCESS;
    if (mock.resultCursor != mock.resultCount) result = mock.results[mock.resultCursor++];
    if (result == VK_SUCCESS && info->memoryTypeIndex != 1 && mock.refusedDevice != VK_SUCCESS) result = mock.refusedDevice;
    if (result == VK_SUCCESS && info->memoryTypeIndex != 1 && mock.deviceLimit != 0 && info->allocationSize > mock.deviceLimit - std::min(mock.liveBytes, mock.deviceLimit)) result = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    if (result != VK_SUCCESS) {
        *memory = Handle<VkDeviceMemory>(0xdeadbeef);
        return result;
    }
    if (mock.nextMemory == RecordCount) return VK_ERROR_TOO_MANY_OBJECTS;
    VkImage dedicated = VK_NULL_HANDLE;
    for (auto* next = static_cast<const VkBaseInStructure*>(info->pNext); next != nullptr; next = next->pNext) {
        if (next->sType == VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO) dedicated = reinterpret_cast<const VkMemoryDedicatedAllocateInfo*>(next)->image;
    }
    const auto index = mock.nextMemory++;
    mock.memories[index] = {info->allocationSize, info->memoryTypeIndex, dedicated, true};
    *memory = Handle<VkDeviceMemory>(index);
    ++mock.allocations;
    ++mock.live;
    mock.liveBytes += info->allocationSize;
    if (mock.failHostAfterAllocate) {
        mock.failHostAfterAllocate = false;
        SetAllocationFailureCountdown(0);
    }
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL FreeMemory(VkDevice, VkDeviceMemory memory, const VkAllocationCallbacks*) {
    std::lock_guard lock(mockMutex);
    const auto index = Index(memory);
    if (index == 0 || index >= mock.nextMemory || !mock.memories[index].active) {
        ++mock.invalidFrees;
        return;
    }
    for (std::size_t image = 1; image < mock.nextImage; ++image) {
        if (mock.images[image].boundMemory == memory) ++mock.invalidFrees;
    }
    auto& record = mock.memories[index];
    record.active = false;
    mock.liveBytes -= record.bytes;
    --mock.live;
    ++mock.frees;
}

VKAPI_ATTR void VKAPI_CALL UnmapMemory(VkDevice, VkDeviceMemory) {}
VKAPI_ATTR void VKAPI_CALL DestroyBuffer(VkDevice, VkBuffer, const VkAllocationCallbacks*) {}

VKAPI_ATTR VkResult VKAPI_CALL MapMemory(VkDevice, VkDeviceMemory, VkDeviceSize, VkDeviceSize, VkMemoryMapFlags, void**) {
    std::lock_guard lock(mockMutex);
    ++mock.maps;
    return VK_ERROR_MEMORY_MAP_FAILED;
}

VKAPI_ATTR void VKAPI_CALL GetImageMemoryRequirements(VkDevice, VkImage image, VkMemoryRequirements* requirements) {
    std::lock_guard lock(mockMutex);
    *requirements = mock.images[Index(image)].requirements;
}

VKAPI_ATTR void VKAPI_CALL GetImageMemoryRequirements2(VkDevice, const VkImageMemoryRequirementsInfo2* info, VkMemoryRequirements2* requirements) {
    std::lock_guard lock(mockMutex);
    const auto& record = mock.images[Index(info->image)];
    requirements->memoryRequirements = record.requirements;
    ++mock.requirements2;
    for (auto* next = static_cast<VkBaseOutStructure*>(requirements->pNext); next != nullptr; next = next->pNext) {
        if (next->sType != VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS) continue;
        auto* dedicated = reinterpret_cast<VkMemoryDedicatedRequirements*>(next);
        dedicated->requiresDedicatedAllocation = record.required;
        dedicated->prefersDedicatedAllocation = record.preferred;
    }
}

VKAPI_ATTR VkResult VKAPI_CALL BindImageMemory(VkDevice, VkImage image, VkDeviceMemory memory, VkDeviceSize offset) {
    std::lock_guard lock(mockMutex);
    const auto imageIndex = Index(image);
    const auto memoryIndex = Index(memory);
    const auto invalid = [&] {
        ++mock.invalidBinds;
        return VK_ERROR_INITIALIZATION_FAILED;
    };
    if (imageIndex == 0 || imageIndex >= mock.nextImage || memoryIndex == 0 || memoryIndex >= mock.nextMemory) return invalid();
    auto& imageRecord = mock.images[imageIndex];
    const auto& memoryRecord = mock.memories[memoryIndex];
    const auto& requirements = imageRecord.requirements;
    if (!memoryRecord.active || imageRecord.boundMemory != VK_NULL_HANDLE || memoryRecord.type >= 32 || (requirements.memoryTypeBits & (1u << memoryRecord.type)) == 0) return invalid();
    if (requirements.alignment == 0 || offset % requirements.alignment != 0 || offset > memoryRecord.bytes || requirements.size > memoryRecord.bytes - offset) return invalid();
    if (imageRecord.required && memoryRecord.dedicated != image) return invalid();
    if (memoryRecord.dedicated != VK_NULL_HANDLE && (memoryRecord.dedicated != image || offset != 0)) return invalid();
    for (std::size_t index = 1; index < mock.nextImage; ++index) {
        const auto& other = mock.images[index];
        if (other.boundMemory != memory) continue;
        const auto endsBefore = offset + requirements.size <= other.boundOffset;
        const auto beginsAfter = other.boundOffset + other.requirements.size <= offset;
        if (!endsBefore && !beginsAfter) return invalid();
    }
    imageRecord.boundMemory = memory;
    imageRecord.boundOffset = offset;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL MemoryProperties2(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties2* properties) {
    auto* budget = static_cast<VkPhysicalDeviceMemoryBudgetPropertiesEXT*>(properties->pNext);
    budget->heapBudget[0] = mock.reportedBudget;
    budget->heapUsage[0] = mock.reportedUsage;
    budget->heapBudget[1] = 8ull << 30u;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL DeviceProc(VkDevice, const char* name) {
    if (std::strcmp(name, "vkAllocateMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(AllocateMemory);
    if (std::strcmp(name, "vkFreeMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(FreeMemory);
    if (std::strcmp(name, "vkUnmapMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(UnmapMemory);
    if (std::strcmp(name, "vkDestroyBuffer") == 0) return reinterpret_cast<PFN_vkVoidFunction>(DestroyBuffer);
    if (std::strcmp(name, "vkMapMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(MapMemory);
    if (std::strcmp(name, "vkGetImageMemoryRequirements") == 0) return reinterpret_cast<PFN_vkVoidFunction>(GetImageMemoryRequirements);
    if (std::strcmp(name, "vkGetImageMemoryRequirements2") == 0) return reinterpret_cast<PFN_vkVoidFunction>(GetImageMemoryRequirements2);
    if (std::strcmp(name, "vkBindImageMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(BindImageMemory);
    return nullptr;
}

std::uint64_t Reclaim(std::uint64_t bytes, bool newImage) {
    ++reclaimCalls;
    return reclaimAction != nullptr ? reclaimAction(bytes, newImage) : 0;
}

Context NewContext() {
    Context context{};
    context.device = Handle<VkDevice>(1);
    context.deviceProc = DeviceProc;
    context.limits.maxMemoryAllocationCount = 4096;
    context.memory.memoryHeapCount = 2;
    context.memory.memoryHeaps[0] = {8ull << 30u, VK_MEMORY_HEAP_DEVICE_LOCAL_BIT};
    context.memory.memoryHeaps[1] = {8ull << 30u, 0};
    context.memory.memoryTypeCount = 4;
    context.memory.memoryTypes[0] = {VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0};
    context.memory.memoryTypes[1] = {VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 1};
    context.memory.memoryTypes[2] = {VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0};
    context.memory.memoryTypes[3] = {VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 0};
    context.cacheReclaim = Reclaim;
    return context;
}

struct ImageSpec {
    VkImage image;
    VkImageCreateInfo info;
};

ImageSpec NewImage(VkDeviceSize size = 64 * KiB, VkDeviceSize alignment = 256, std::uint32_t types = 1u, bool required = false, bool preferred = false) {
    std::lock_guard lock(mockMutex);
    Expect(mock.nextImage != RecordCount, "image mock exhausted its image records");
    const auto index = mock.nextImage++;
    mock.images[index].requirements = {size, alignment, types};
    mock.images[index].required = required ? VK_TRUE : VK_FALSE;
    mock.images[index].preferred = required || preferred ? VK_TRUE : VK_FALSE;
    VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = VK_FORMAT_R8G8B8A8_UNORM;
    info.extent = {64, 64, 1};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    return {Handle<VkImage>(index), info};
}

VkDeviceMemory MemoryOf(const ImageAllocation& allocation) { return allocation.Memory(); }
VkDeviceSize OffsetOf(const ImageAllocation& allocation) { return allocation.Offset(); }

ImageAllocation Take(const Context& context, const ImageSpec& image, bool budgeted = false) {
    return AllocateImageMemory(context, image.image, image.info, "test image", true, budgeted);
}

void Bind(const ImageSpec& image, const ImageAllocation& allocation) {
    Expect(allocation.Size() == mock.images[Index(image.image)].requirements.size, "image lease size does not describe the image requirements");
    Expect(allocation.MemoryType() == mock.memories[Index(MemoryOf(allocation))].type, "image lease reports the wrong actual memory type");
    Check(BindImageMemory(VK_NULL_HANDLE, image.image, MemoryOf(allocation), OffsetOf(allocation)), "mock image binding");
}

void Unbind(const ImageSpec& image) {
    std::lock_guard lock(mockMutex);
    mock.images[Index(image.image)].boundMemory = VK_NULL_HANDLE;
}

void Drop(const ImageSpec& image, ImageAllocation& allocation) {
    Unbind(image);
    allocation.Reset();
}

void ConcurrentBurst() {
    ResetMock();
    {
        auto context = NewContext();
        std::vector<ImageSpec> images;
        std::vector<ImageAllocation> allocations;
        images.reserve(4096);
        allocations.reserve(4096);
        for (std::size_t index = 0; index < 4096; ++index) {
            images.push_back(NewImage());
            allocations.push_back(Take(context, images.back()));
            Bind(images.back(), allocations.back());
        }
        std::printf("image allocation: 4096 simultaneous 64 KiB images used %zu driver allocations and %llu bytes\n", mock.allocations, static_cast<unsigned long long>(mock.liveBytes));
        Expect(mock.allocations <= 128, "4096 small images still allocate driver memory independently");
        Expect(mock.liveBytes <= 300 * MiB, "small-image block growth wastes too much memory");
        Expect(mock.requirements2 == images.size(), "image requirements did not use dedicated-allocation queries");
        const auto before = mock.allocations;
        for (std::size_t index = 0; index < images.size(); index += 2) Drop(images[index], allocations[index]);
        for (std::size_t index = 0; index < images.size(); index += 2) {
            allocations[index] = Take(context, images[index]);
            Bind(images[index], allocations[index]);
        }
        Expect(mock.allocations == before, "returned image ranges were not reused inside live blocks");
        for (std::size_t index = 0; index < images.size(); ++index) Drop(images[index], allocations[index]);
    }
    Clean();
}

void VariableRanges() {
    ResetMock();
    {
        auto context = NewContext();
        std::array<ImageSpec, 256> images{};
        std::array<ImageAllocation, 256> allocations{};
        std::uint32_t random = 0x52f61a3du;
        const auto next = [&] {
            random ^= random << 13u;
            random ^= random >> 17u;
            random ^= random << 5u;
            return random;
        };
        for (std::size_t round = 0; round < 5; ++round) {
            for (std::size_t index = 0; index < images.size(); ++index) {
                const auto alignment = VkDeviceSize{1} << (4u + next() % 13u);
                const auto bytes = VkDeviceSize{1} + next() % (384u * 1024u);
                images[index] = NewImage(bytes, alignment);
                allocations[index] = Take(context, images[index]);
                Bind(images[index], allocations[index]);
            }
            for (std::size_t remaining = images.size(); remaining != 0; --remaining) {
                const auto chosen = static_cast<std::size_t>(next()) % remaining;
                Drop(images[chosen], allocations[chosen]);
                if (chosen != remaining - 1) {
                    images[chosen] = images[remaining - 1];
                    allocations[chosen] = std::move(allocations[remaining - 1]);
                }
            }
        }
    }
    Clean();
}

void BoundaryRanges() {
    ResetMock();
    {
        auto context = NewContext();
        constexpr std::array<VkDeviceSize, 18> sizes{1, 63, 64, 255, 256, 257, 511, 512, 513, 16383, 16384, 16385, 65535, 65536, 65537, MiB - 257, MiB - 1, MiB};
        std::array<ImageSpec, sizes.size()> images{};
        std::array<ImageAllocation, sizes.size()> allocations{};
        for (std::size_t index = 0; index < sizes.size(); ++index) {
            images[index] = NewImage(sizes[index], VkDeviceSize{1} << (index % 17));
            allocations[index] = Take(context, images[index]);
            Bind(images[index], allocations[index]);
        }
        for (std::size_t index = sizes.size(); index-- != 0;) Drop(images[index], allocations[index]);
    }
    Clean();
    ResetMock();
    {
        auto context = NewContext();
        std::array<ImageSpec, 4> images{};
        std::array<ImageAllocation, 4> allocations{};
        for (std::size_t index = 0; index < images.size(); ++index) {
            images[index] = NewImage(MiB, MiB);
            allocations[index] = Take(context, images[index]);
            Bind(images[index], allocations[index]);
        }
        Expect(mock.allocations == 1, "four aligned 1 MiB image ranges did not fit one 4 MiB block");
        const auto returnedMemory = MemoryOf(allocations[1]);
        const auto returnedOffset = OffsetOf(allocations[1]);
        Drop(images[1], allocations[1]);
        images[1] = NewImage(MiB - 1, MiB);
        allocations[1] = Take(context, images[1]);
        Bind(images[1], allocations[1]);
        Expect(MemoryOf(allocations[1]) == returnedMemory && OffsetOf(allocations[1]) == returnedOffset && mock.allocations == 1, "bitmap range spanning multiple words was not reused");
        for (std::size_t index = 0; index < images.size(); ++index) Drop(images[index], allocations[index]);
    }
    Clean();
}

void UnboundAllocationRollback() {
    ResetMock();
    {
        auto context = NewContext();
        const auto first = NewImage();
        auto allocation = Take(context, first);
        const auto memory = MemoryOf(allocation);
        const auto offset = OffsetOf(allocation);
        allocation.Reset();
        const auto second = NewImage();
        auto replacement = Take(context, second);
        Bind(second, replacement);
        Expect(MemoryOf(replacement) == memory && OffsetOf(replacement) == offset && mock.allocations == 1, "an allocation returned before binding did not release its range");
        Drop(second, replacement);
    }
    Clean();
}

void BlockAlignmentFallback() {
    ResetMock();
    {
        auto context = NewContext();
        mock.deviceLimit = 32 * MiB;
        std::array<ImageSpec, 32> images{};
        std::array<ImageAllocation, 32> allocations{};
        for (std::size_t index = 0; index < images.size(); ++index) {
            images[index] = NewImage(64 * KiB, 4 * MiB);
            allocations[index] = Take(context, images[index]);
            Bind(images[index], allocations[index]);
        }
        Expect(mock.liveBytes == images.size() * 64 * KiB, "block-aligned images wasted a whole slab block per allocation");
        Expect(context.bufferPool->ImageSlabBlocks() == 0, "one-image-per-block alignment entered the shared image arena");
        for (std::size_t index = 0; index < images.size(); ++index) Drop(images[index], allocations[index]);
    }
    Clean();
    ResetMock();
    {
        auto context = NewContext();
        const auto first = NewImage(64 * KiB, 2 * MiB);
        const auto second = NewImage(64 * KiB, 2 * MiB);
        auto firstAllocation = Take(context, first);
        auto secondAllocation = Take(context, second);
        Bind(first, firstAllocation);
        Bind(second, secondAllocation);
        Expect(mock.allocations == 1 && MemoryOf(firstAllocation) == MemoryOf(secondAllocation), "two-image alignment unnecessarily bypassed suballocation");
        Drop(first, firstAllocation);
        Drop(second, secondAllocation);
    }
    Clean();
}

void TypeIsolation() {
    ResetMock();
    {
        auto context = NewContext();
        std::array<ImageSpec, 3> images{NewImage(96 * KiB, 256, 1u), NewImage(96 * KiB, 256, 4u), NewImage(96 * KiB, 256, 8u)};
        std::array<ImageAllocation, 3> allocations{};
        for (std::size_t index = 0; index < images.size(); ++index) {
            allocations[index] = Take(context, images[index]);
            Bind(images[index], allocations[index]);
        }
        for (std::size_t first = 0; first < images.size(); ++first) {
            for (std::size_t second = first + 1; second < images.size(); ++second) Expect(MemoryOf(allocations[first]) != MemoryOf(allocations[second]), "image blocks mixed incompatible memory types");
        }
        for (std::size_t index = 0; index < images.size(); ++index) Drop(images[index], allocations[index]);
        bool rejected = false;
        try {
            auto unsupported = Take(context, NewImage(96 * KiB, 256, 2u));
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        Expect(rejected, "host-only image requirements changed the existing device-memory selection policy");
    }
    Clean();
}

void DedicatedRequirements() {
    ResetMock();
    {
        auto context = NewContext();
        std::array<ImageSpec, 4> images{NewImage(64 * KiB, 256, 1u, true), NewImage(64 * KiB, 256, 1u, true), NewImage(64 * KiB, 256, 1u, false, true), NewImage()};
        std::array<ImageAllocation, 4> allocations{};
        for (std::size_t index = 0; index < images.size(); ++index) {
            allocations[index] = Take(context, images[index]);
            Bind(images[index], allocations[index]);
        }
        Expect(MemoryOf(allocations[0]) != MemoryOf(allocations[1]), "required dedicated images share a driver allocation");
        for (std::size_t index = 0; index < 2; ++index) {
            const auto memory = MemoryOf(allocations[index]);
            Expect(mock.memories[Index(memory)].dedicated == images[index].image, "required image omitted its dedicated allocation chain");
            Drop(images[index], allocations[index]);
            Expect(!mock.memories[Index(memory)].active, "dedicated memory was retained for reuse by another image");
        }
        for (std::size_t index = 2; index < images.size(); ++index) Drop(images[index], allocations[index]);
        const auto another = NewImage(64 * KiB, 256, 1u, true);
        auto allocation = Take(context, another);
        Bind(another, allocation);
        Drop(another, allocation);
    }
    Clean();
}

void LargeAndLinearImages() {
    ResetMock();
    {
        auto context = NewContext();
        auto linear = NewImage();
        linear.info.tiling = VK_IMAGE_TILING_LINEAR;
        const auto large = NewImage(3 * MiB);
        auto first = Take(context, linear);
        auto second = Take(context, large);
        Bind(linear, first);
        Bind(large, second);
        Expect(OffsetOf(first) == 0 && OffsetOf(second) == 0, "linear or large image entered the small optimal-image arena");
        Drop(linear, first);
        Drop(large, second);
    }
    Clean();
}

void RejectedImageForms() {
    ResetMock();
    {
        auto context = NewContext();
        for (const auto flags : {VK_IMAGE_CREATE_SPARSE_BINDING_BIT, VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT, VK_IMAGE_CREATE_SPARSE_ALIASED_BIT, VK_IMAGE_CREATE_DISJOINT_BIT, VK_IMAGE_CREATE_PROTECTED_BIT}) {
            auto image = NewImage();
            image.info.flags = flags;
            bool rejected = false;
            try {
                auto allocation = Take(context, image);
            } catch (const std::runtime_error&) {
                rejected = true;
            }
            Expect(rejected, "image allocation accepted a binding mode requiring a separate allocator");
        }
        auto external = NewImage();
        VkExternalMemoryImageCreateInfo exportInfo{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO};
        exportInfo.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
        external.info.pNext = &exportInfo;
        bool rejected = false;
        try {
            auto allocation = Take(context, external);
        } catch (const std::runtime_error&) {
            rejected = true;
        }
        Expect(rejected, "external image allocation omitted its required import/export contract");
        Expect(mock.allocations == 0, "rejected image forms made driver allocations");
        const auto compatible = NewImage();
        auto withFormatList = compatible;
        VkImageFormatListCreateInfo formats{VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO};
        const VkFormat format = compatible.info.format;
        formats.viewFormatCount = 1;
        formats.pViewFormats = &format;
        withFormatList.info.pNext = &formats;
        auto allocation = Take(context, withFormatList);
        Bind(withFormatList, allocation);
        Expect(OffsetOf(allocation) == 0, "image creation metadata was passed through the ordinary slab arena");
        Drop(withFormatList, allocation);
    }
    Clean();
}

void BudgetPressureUsesLiveBlock() {
    ResetMock();
    {
        auto context = NewContext();
        context.physical = Handle<VkPhysicalDevice>(1);
        context.memoryProperties2 = MemoryProperties2;
        const auto first = NewImage(64 * KiB, 256, 3u);
        auto firstAllocation = Take(context, first, true);
        Bind(first, firstAllocation);
        Expect(firstAllocation.MemoryType() == 0, "budget fixture did not begin in device-local memory");
        const auto driverAllocations = mock.allocations;
        mock.reportedUsage = mock.reportedBudget;
        const auto second = NewImage(64 * KiB, 256, 3u);
        auto secondAllocation = Take(context, second, true);
        Bind(second, secondAllocation);
        Expect(secondAllocation.MemoryType() == 0 && MemoryOf(secondAllocation) == MemoryOf(firstAllocation), "heap pressure ignored already-allocated compatible device memory");
        Expect(mock.allocations == driverAllocations, "heap pressure allocated new host memory while a compatible device range was available");
        Drop(first, firstAllocation);
        Drop(second, secondAllocation);
    }
    Clean();
}

void SmallBudgetHeadroom() {
    ResetMock();
    {
        auto context = NewContext();
        context.physical = Handle<VkPhysicalDevice>(1);
        context.memoryProperties2 = MemoryProperties2;
        mock.reportedUsage = mock.reportedBudget - 64 * KiB;
        const auto image = NewImage(64 * KiB, 256, 3u);
        auto allocation = Take(context, image, true);
        Bind(image, allocation);
        const auto& memory = mock.memories[Index(MemoryOf(allocation))];
        Expect(memory.type == 0 && memory.bytes == 64 * KiB, "a small amount of device headroom was oversubscribed by a new image block");
        Expect(context.bufferPool->ImageSlabBlocks() == 0, "an image arena block exceeded reported heap headroom");
        Drop(image, allocation);
    }
    Clean();
}

void ReleasedMemoryAccounting() {
    ResetMock();
    {
        auto context = NewContext();
        const auto first = NewImage();
        const auto second = NewImage();
        auto firstAllocation = Take(context, first);
        auto secondAllocation = Take(context, second);
        Bind(first, firstAllocation);
        Bind(second, secondAllocation);
        const auto before = ImageMemoryReleasedBytes();
        Drop(first, firstAllocation);
        Expect(ImageMemoryReleasedBytes() == before, "returning a partial image block reported reclaimable driver bytes");
        Drop(second, secondAllocation);
        Expect(ImageMemoryReleasedBytes() - before == 4 * MiB, "returning the last image range did not report its whole reclaimable block");
    }
    Clean();
    ResetMock();
    {
        auto context = NewContext();
        const auto image = NewImage();
        auto cpuOwner = std::make_shared<ImageAllocation>(Take(context, image));
        Bind(image, *cpuOwner);
        auto pendingGpuOwner = cpuOwner;
        const auto before = ImageMemoryReleasedBytes();
        cpuOwner.reset();
        Expect(ImageMemoryReleasedBytes() == before, "a deferred image lease reported bytes before the last owner released it");
        Unbind(image);
        pendingGpuOwner.reset();
        Expect(ImageMemoryReleasedBytes() - before == 4 * MiB, "the final deferred image lease failed to report its reclaimable block");
    }
    Clean();
    ResetMock();
    {
        auto context = NewContext();
        const auto image = NewImage(64 * KiB, 256, 1u, true);
        auto allocation = Take(context, image);
        Bind(image, allocation);
        const auto before = ImageMemoryReleasedBytes();
        Drop(image, allocation);
        Expect(ImageMemoryReleasedBytes() - before == 64 * KiB, "a dedicated image release reported the wrong reclaimable byte count");
    }
    Clean();
}

void ReleasedMemoryIsThreadLocal() {
    ResetMock();
    {
        auto context = NewContext();
        const auto image = NewImage();
        auto allocation = Take(context, image);
        Bind(image, allocation);
        const auto before = ImageMemoryReleasedBytes();
        std::uint64_t workerReleased = 0;
        std::thread worker([&] {
            const auto workerBefore = ImageMemoryReleasedBytes();
            Drop(image, allocation);
            workerReleased = ImageMemoryReleasedBytes() - workerBefore;
        });
        worker.join();
        Expect(ImageMemoryReleasedBytes() == before, "another thread inflated local reclaimed-memory accounting");
        Expect(workerReleased == 4 * MiB, "worker-thread release did not account for its own returned block");
    }
    Clean();
}

void MoveAndPoolLifetime() {
    ResetMock();
    ImageAllocation survivor;
    ImageSpec image{};
    {
        auto context = NewContext();
        image = NewImage();
        auto first = Take(context, image);
        Bind(image, first);
        const auto memory = MemoryOf(first);
        survivor = std::move(first);
        first.Reset();
        Expect(MemoryOf(survivor) == memory && mock.memories[Index(memory)].active, "moving an image allocation lost its memory");
        auto second = std::move(survivor);
        survivor.Reset();
        survivor = std::move(second);
        context.bufferPool.reset();
        Expect(mock.memories[Index(memory)].active, "image allocation did not retain its pool owner");
    }
    Expect(mock.live != 0, "image lease lost memory when its creating Context went away");
    Drop(image, survivor);
    survivor.Reset();
    Clean();
}

void MoveAssignmentReleasesDestination() {
    ResetMock();
    {
        auto context = NewContext();
        const auto first = NewImage();
        const auto second = NewImage();
        auto source = Take(context, first);
        auto destination = Take(context, second);
        Bind(first, source);
        Bind(second, destination);
        const auto releasedMemory = MemoryOf(destination);
        const auto releasedOffset = OffsetOf(destination);
        Unbind(second);
        destination = std::move(source);
        source.Reset();
        const auto third = NewImage();
        auto replacement = Take(context, third);
        Bind(third, replacement);
        Expect(MemoryOf(replacement) == releasedMemory && OffsetOf(replacement) == releasedOffset, "move assignment did not release its previous destination range");
        auto& alias = destination;
        destination = std::move(alias);
        Expect(destination && MemoryOf(destination) == MemoryOf(replacement), "self move invalidated a live image allocation");
        Drop(first, destination);
        Drop(third, replacement);
    }
    Clean();
}

void ResetWithoutHostAllocation() {
    ResetMock();
    {
        auto context = NewContext();
        std::array<ImageSpec, 48> images{};
        std::array<ImageAllocation, 48> allocations{};
        for (const std::size_t blocks : {4u, 6u, 3u}) {
            const auto count = blocks * 8;
            for (std::size_t index = 0; index < count; ++index) {
                images[index] = NewImage(512 * KiB);
                allocations[index] = Take(context, images[index]);
                Bind(images[index], allocations[index]);
            }
            const auto previous = std::set_terminate([] {
                std::fputs("FAIL: image Reset terminated during host allocation failure\n", stderr);
                std::_Exit(1);
            });
            SetAllocationFailureCountdown(0);
            for (std::size_t index = 0; index < count; ++index) Drop(images[index], allocations[index]);
            SetAllocationFailureCountdown(-1);
            std::set_terminate(previous);
            for (std::size_t index = 0; index < count; ++index) {
                allocations[index] = Take(context, images[index]);
                Bind(images[index], allocations[index]);
            }
            for (std::size_t index = 0; index < count; ++index) Drop(images[index], allocations[index]);
        }
    }
    Clean();
}

void HostAllocationFailures() {
    std::size_t failures = 0;
    std::size_t successes = 0;
    for (int failAt = 0; failAt != 48; ++failAt) {
        ResetMock();
        {
            auto context = NewContext();
            const auto image = NewImage();
            ImageAllocation allocation;
            SetAllocationFailureCountdown(failAt);
            try {
                allocation = Take(context, image);
                ++successes;
            } catch (const std::bad_alloc&) {
                ++failures;
            }
            SetAllocationFailureCountdown(-1);
            if (MemoryOf(allocation) != VK_NULL_HANDLE) {
                Bind(image, allocation);
                Drop(image, allocation);
            }
        }
        Clean();
    }
    Expect(failures != 0 && successes != 0, "host-allocation fault sweep did not exercise both outcomes");
    ResetMock();
    {
        auto context = NewContext();
        const auto image = NewImage();
        mock.failHostAfterAllocate = true;
        ImageAllocation allocation;
        try {
            allocation = Take(context, image);
        } catch (const std::bad_alloc&) {
        }
        SetAllocationFailureCountdown(-1);
        if (MemoryOf(allocation) != VK_NULL_HANDLE) Drop(image, allocation);
    }
    Clean();
}

void HostFailuresPreserveLiveSiblings() {
    for (int failAt = 0; failAt < 24; ++failAt) {
        ResetMock();
        {
            auto context = NewContext();
            std::array<ImageSpec, 4> images{};
            std::array<ImageAllocation, 4> allocations{};
            for (std::size_t index = 0; index < images.size(); ++index) {
                images[index] = NewImage(MiB);
                allocations[index] = Take(context, images[index]);
                Bind(images[index], allocations[index]);
            }
            const auto memory = MemoryOf(allocations[0]);
            const auto another = NewImage();
            ImageAllocation extra;
            SetAllocationFailureCountdown(failAt);
            try {
                extra = Take(context, another);
            } catch (const std::bad_alloc&) {
            }
            SetAllocationFailureCountdown(-1);
            Expect(mock.memories[Index(memory)].active, "a failed new image block released a live sibling block");
            if (extra) {
                Bind(another, extra);
                Drop(another, extra);
            }
            for (std::size_t index = 0; index < images.size(); ++index) Drop(images[index], allocations[index]);
        }
        Clean();
    }
}

void EmptyBlockReclamation() {
    ResetMock();
    {
        auto context = NewContext();
        const auto image = NewImage();
        auto allocation = Take(context, image);
        Bind(image, allocation);
        Drop(image, allocation);
        Expect(context.bufferPool->ImageSlabBlocks() == 1, "empty image block was not retained for reuse");
        VkMemoryAllocateInfo request{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        request.allocationSize = 3 * MiB;
        request.memoryTypeIndex = 0;
        mock.results[0] = VK_ERROR_OUT_OF_DEVICE_MEMORY;
        mock.resultCount = 1;
        const auto memory = context.bufferPool->AllocateMemory(request, "test image block trim");
        Expect(context.bufferPool->ImageSlabBlocks() == 0 && mock.frees == 1, "allocation pressure did not reclaim the empty image block");
        FreeMemory(VK_NULL_HANDLE, memory, nullptr);
    }
    Clean();
    ResetMock();
    {
        auto context = NewContext();
        context.physical = Handle<VkPhysicalDevice>(1);
        context.memoryProperties2 = MemoryProperties2;
        const auto image = NewImage();
        auto allocation = Take(context, image);
        Bind(image, allocation);
        mock.reportedUsage = mock.reportedBudget + MiB;
        Drop(image, allocation);
        Expect(context.bufferPool->ImageSlabBlocks() == 0 && mock.live == 0, "over-budget device memory retained an empty image block");
    }
    Clean();
}

void VulkanFailures() {
    for (const auto result : {VK_ERROR_OUT_OF_DEVICE_MEMORY, VK_ERROR_OUT_OF_HOST_MEMORY, VK_ERROR_TOO_MANY_OBJECTS, VK_ERROR_DEVICE_LOST}) {
        ResetMock();
        {
            auto context = NewContext();
            const auto image = NewImage();
            mock.refusedDevice = result;
            bool failed = false;
            try {
                auto allocation = Take(context, image);
            } catch (const std::runtime_error&) {
                failed = true;
            }
            Expect(failed, "a failed Vulkan allocation was accepted");
            Expect(mock.allocations == 0 && mock.invalidFrees == 0, "a failed Vulkan output was treated as an owned handle");
        }
        Clean();
    }
    ResetMock();
    {
        auto context = NewContext();
        const auto image = NewImage(64 * KiB, 256, 3u);
        mock.refusedDevice = VK_ERROR_OUT_OF_DEVICE_MEMORY;
        auto allocation = Take(context, image);
        Bind(image, allocation);
        Expect(mock.memories[Index(MemoryOf(allocation))].type == 1, "refused device allocation did not select a valid host fallback");
        Drop(image, allocation);
    }
    Clean();
}

void SmallHeapFallback() {
    ResetMock();
    {
        auto context = NewContext();
        mock.deviceLimit = 512 * KiB;
        const auto image = NewImage(64 * KiB);
        auto allocation = Take(context, image);
        Bind(image, allocation);
        const auto& memory = mock.memories[Index(MemoryOf(allocation))];
        Expect(memory.type == 0 && memory.bytes <= mock.deviceLimit, "a refused arena block prevented an exact-size device allocation that fits");
        Drop(image, allocation);
    }
    Clean();
}

std::uint64_t ReturnImages(std::uint64_t, bool) {
    for (std::size_t index = 0; index < reclaimImageCount; ++index) reclaimImages[index]->Reset();
    return reclaimReportsBytes ? reclaimImageCount * 512 * KiB : 0;
}

void ReentrantPressureReclaim(bool reportsBytes) {
    ResetMock();
    {
        auto context = NewContext();
        std::array<ImageSpec, 8> images{};
        std::array<ImageAllocation, 8> allocations{};
        for (std::size_t index = 0; index < images.size(); ++index) {
            images[index] = NewImage(512 * KiB);
            allocations[index] = Take(context, images[index]);
            Bind(images[index], allocations[index]);
        }
        const auto firstBlock = MemoryOf(allocations[0]);
        for (std::size_t index = 1; index < images.size(); ++index) Expect(MemoryOf(allocations[index]) == firstBlock, "pressure fixture did not fill one image block");
        Unbind(images[0]);
        reclaimImages[0] = &allocations[0];
        reclaimImageCount = 1;
        reclaimReportsBytes = reportsBytes;
        reclaimAction = ReturnImages;
        mock.deviceLimit = mock.liveBytes;
        const auto next = NewImage(512 * KiB);
        auto allocation = Take(context, next);
        Bind(next, allocation);
        Expect(reclaimCalls != 0, "image block exhaustion did not attempt cache reclamation");
        Expect(MemoryOf(allocation) == firstBlock, "reclamation-created space was ignored before a failed new block allocation");
        Drop(next, allocation);
        for (std::size_t index = 1; index < images.size(); ++index) Drop(images[index], allocations[index]);
        reclaimAction = nullptr;
    }
    Clean();
}

constexpr std::size_t PressureBlocks = 80;
constexpr std::size_t PressureSlots = 8;
std::array<ImageSpec, PressureBlocks * PressureSlots> pressureImages{};
std::array<ImageAllocation, PressureBlocks * PressureSlots> pressureAllocations{};
bool legacyPressureAccounting = false;
std::size_t pressureDropped = 0;

std::uint64_t ReleaseInterleavedImages(std::uint64_t requested, bool) {
    const auto before = ImageMemoryReleasedBytes();
    std::uint64_t released = 0;
    for (std::size_t slot = 0; slot < PressureSlots && released < requested; ++slot) {
        for (std::size_t block = 0; block < PressureBlocks && released < requested; ++block) {
            const auto index = block * PressureSlots + slot;
            Drop(pressureImages[index], pressureAllocations[index]);
            ++pressureDropped;
            released = legacyPressureAccounting ? released + 512 * KiB : ImageMemoryReleasedBytes() - before;
        }
    }
    return released;
}

void InterleavedPressureAccounting(bool legacy) {
    ResetMock();
    legacyPressureAccounting = legacy;
    pressureDropped = 0;
    {
        auto context = NewContext();
        for (std::size_t index = 0; index < pressureImages.size(); ++index) {
            pressureImages[index] = NewImage(512 * KiB);
            pressureAllocations[index] = Take(context, pressureImages[index]);
            Bind(pressureImages[index], pressureAllocations[index]);
        }
        Expect(context.bufferPool->ImageSlabBlocks() == PressureBlocks, "interleaved pressure fixture did not fill eighty image blocks");
        mock.deviceLimit = PressureBlocks * 4 * MiB;
        reclaimAction = ReleaseInterleavedImages;
        VkMemoryAllocateInfo request{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        request.allocationSize = MiB;
        request.memoryTypeIndex = 0;
        bool succeeded = false;
        try {
            const auto memory = context.bufferPool->AllocateMemory(request, "test interleaved image reclamation");
            succeeded = true;
            FreeMemory(context.device, memory, nullptr);
        } catch (const std::runtime_error&) {
        }
        reclaimAction = nullptr;
        std::printf("image allocation: %s accounting %s after dropping %zu interleaved images\n", legacy ? "legacy per-image" : "returned-block", succeeded ? "recovered" : "failed", pressureDropped);
        Expect(succeeded != legacy, "pressure recovery did not distinguish retained slices from returned driver memory");
        for (std::size_t index = 0; index < pressureImages.size(); ++index) Drop(pressureImages[index], pressureAllocations[index]);
    }
    Clean();
}

void MultithreadedRanges() {
    ResetMock();
    {
        auto context = NewContext();
        GetBufferPool(context);
        constexpr std::size_t Threads = 6;
        constexpr std::size_t PerThread = 32;
        std::array<std::array<ImageSpec, PerThread>, Threads> images{};
        for (auto& group : images) for (auto& image : group) image = NewImage(32 * KiB, 4096);
        std::atomic<bool> failed{false};
        std::array<std::thread, Threads> workers;
        for (std::size_t thread = 0; thread < Threads; ++thread) {
            workers[thread] = std::thread([&, thread] {
                try {
                    std::array<ImageAllocation, PerThread> allocations{};
                    for (std::size_t round = 0; round < 32; ++round) {
                        for (std::size_t index = 0; index < PerThread; ++index) {
                            allocations[index] = Take(context, images[thread][index]);
                            Bind(images[thread][index], allocations[index]);
                        }
                        for (std::size_t index = 0; index < PerThread; ++index) Drop(images[thread][index], allocations[index]);
                    }
                } catch (...) {
                    failed.store(true, std::memory_order_relaxed);
                }
            });
        }
        for (auto& worker : workers) worker.join();
        Expect(!failed.load(), "concurrent image allocation or release failed");
    }
    Clean();
}

}

int main() {
    try {
        std::puts("image allocation: concurrent burst");
        ConcurrentBurst();
        std::puts("image allocation: variable and boundary ranges");
        VariableRanges();
        BoundaryRanges();
        BlockAlignmentFallback();
        UnboundAllocationRollback();
        std::puts("image allocation: types and dedicated requirements");
        TypeIsolation();
        DedicatedRequirements();
        LargeAndLinearImages();
        RejectedImageForms();
        BudgetPressureUsesLiveBlock();
        SmallBudgetHeadroom();
        ReleasedMemoryAccounting();
        ReleasedMemoryIsThreadLocal();
        std::puts("image allocation: ownership and host allocation failures");
        MoveAndPoolLifetime();
        MoveAssignmentReleasesDestination();
        ResetWithoutHostAllocation();
        HostAllocationFailures();
        HostFailuresPreserveLiveSiblings();
        EmptyBlockReclamation();
        std::puts("image allocation: Vulkan failures and reclamation");
        VulkanFailures();
        SmallHeapFallback();
        ReentrantPressureReclaim(true);
        ReentrantPressureReclaim(false);
        InterleavedPressureAccounting(true);
        InterleavedPressureAccounting(false);
        std::puts("image allocation: multithreaded ranges");
        MultithreadedRanges();
        std::puts("image allocation: range, lifetime, failure, reclamation and concurrency checks passed");
    } catch (const std::exception& error) {
        SetAllocationFailureCountdown(-1);
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
