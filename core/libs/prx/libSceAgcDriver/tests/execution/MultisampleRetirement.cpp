#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using namespace AgcDriver::Graphics;

constexpr std::size_t BlockBytes = 1u << 20u;
constexpr std::uint32_t Side = 256;

struct Image {
    std::uint64_t generation;
    VkDeviceMemory memory;
};

std::mutex countsMutex;
std::condition_variable countsChanged;
std::map<VkImage, Image> images;
std::map<VkImageView, VkImage> views;
std::map<VkDeviceMemory, VkDeviceSize> allocations;
std::vector<std::pair<VkImage, std::uint64_t>> pendingImages;
std::uint64_t created = 0, destroyed = 0;
PFN_vkGetDeviceProcAddr realProc = nullptr;
PFN_vkAllocateMemory realAllocate = nullptr;
PFN_vkFreeMemory realFree = nullptr;
PFN_vkCreateImage realCreateImage = nullptr;
PFN_vkDestroyImage realDestroyImage = nullptr;
PFN_vkBindImageMemory realBindImageMemory = nullptr;
PFN_vkCreateImageView realCreateImageView = nullptr;
PFN_vkDestroyImageView realDestroyImageView = nullptr;
PFN_vkCmdClearColorImage realClearColorImage = nullptr;
PFN_vkCmdCopyImage realCopyImage = nullptr;

[[noreturn]] void FailPendingImage() {
    std::fputs("an unsubmitted MSAA image would be destroyed; stopping without submitting it\n", stderr);
    std::fflush(stderr);
#ifdef _WIN32
    TerminateProcess(GetCurrentProcess(), 1);
#endif
    std::_Exit(1);
}

VKAPI_ATTR VkResult VKAPI_CALL CountAllocate(VkDevice device, const VkMemoryAllocateInfo* info, const VkAllocationCallbacks* callbacks, VkDeviceMemory* memory) {
    std::lock_guard lock(countsMutex);
    const auto result = realAllocate(device, info, callbacks, memory);
    if (result == VK_SUCCESS) allocations.emplace(*memory, info->allocationSize);
    return result;
}

VKAPI_ATTR void VKAPI_CALL CountFree(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* callbacks) {
    {
        std::lock_guard lock(countsMutex);
        realFree(device, memory, callbacks);
        allocations.erase(memory);
    }
    countsChanged.notify_all();
}

VKAPI_ATTR VkResult VKAPI_CALL CountCreateImage(VkDevice device, const VkImageCreateInfo* info, const VkAllocationCallbacks* callbacks, VkImage* image) {
    std::lock_guard lock(countsMutex);
    const auto result = realCreateImage(device, info, callbacks, image);
    if (result == VK_SUCCESS) images.emplace(*image, Image{++created, VK_NULL_HANDLE});
    return result;
}

VKAPI_ATTR void VKAPI_CALL CountDestroyImage(VkDevice device, VkImage image, const VkAllocationCallbacks* callbacks) {
    {
        std::lock_guard lock(countsMutex);
        for (const auto& [pending, generation] : pendingImages) {
            if (pending == image && images.at(image).generation == generation) FailPendingImage();
        }
        realDestroyImage(device, image, callbacks);
        images.erase(image);
        ++destroyed;
    }
    countsChanged.notify_all();
}

VKAPI_ATTR VkResult VKAPI_CALL CountBindImageMemory(VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize offset) {
    std::lock_guard lock(countsMutex);
    const auto result = realBindImageMemory(device, image, memory, offset);
    if (result == VK_SUCCESS) images.at(image).memory = memory;
    return result;
}

VKAPI_ATTR VkResult VKAPI_CALL CountCreateImageView(VkDevice device, const VkImageViewCreateInfo* info, const VkAllocationCallbacks* callbacks, VkImageView* view) {
    std::lock_guard lock(countsMutex);
    const auto result = realCreateImageView(device, info, callbacks, view);
    if (result == VK_SUCCESS) views.emplace(*view, info->image);
    return result;
}

VKAPI_ATTR void VKAPI_CALL CountDestroyImageView(VkDevice device, VkImageView view, const VkAllocationCallbacks* callbacks) {
    std::lock_guard lock(countsMutex);
    realDestroyImageView(device, view, callbacks);
    views.erase(view);
}

VKAPI_ATTR void VKAPI_CALL CountClearColorImage(VkCommandBuffer commands, VkImage image, VkImageLayout layout, const VkClearColorValue* clear, std::uint32_t count, const VkImageSubresourceRange* ranges) {
    {
        std::lock_guard lock(countsMutex);
        pendingImages.emplace_back(image, images.at(image).generation);
    }
    realClearColorImage(commands, image, layout, clear, count, ranges);
}

VKAPI_ATTR void VKAPI_CALL CountCopyImage(VkCommandBuffer commands, VkImage source, VkImageLayout sourceLayout, VkImage destination, VkImageLayout destinationLayout, std::uint32_t count, const VkImageCopy* regions) {
    {
        std::lock_guard lock(countsMutex);
        pendingImages.emplace_back(source, images.at(source).generation);
        pendingImages.emplace_back(destination, images.at(destination).generation);
    }
    realCopyImage(commands, source, sourceLayout, destination, destinationLayout, count, regions);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL CountingDeviceProc(VkDevice device, const char* name) {
    const auto function = realProc(device, name);
    if (function == nullptr) return nullptr;
    if (std::strcmp(name, "vkAllocateMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(CountAllocate);
    if (std::strcmp(name, "vkFreeMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(CountFree);
    if (std::strcmp(name, "vkCreateImage") == 0) return reinterpret_cast<PFN_vkVoidFunction>(CountCreateImage);
    if (std::strcmp(name, "vkDestroyImage") == 0) return reinterpret_cast<PFN_vkVoidFunction>(CountDestroyImage);
    if (std::strcmp(name, "vkBindImageMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(CountBindImageMemory);
    if (std::strcmp(name, "vkCreateImageView") == 0) return reinterpret_cast<PFN_vkVoidFunction>(CountCreateImageView);
    if (std::strcmp(name, "vkDestroyImageView") == 0) return reinterpret_cast<PFN_vkVoidFunction>(CountDestroyImageView);
    if (std::strcmp(name, "vkCmdClearColorImage") == 0) return reinterpret_cast<PFN_vkVoidFunction>(CountClearColorImage);
    if (std::strcmp(name, "vkCmdCopyImage") == 0) return reinterpret_cast<PFN_vkVoidFunction>(CountCopyImage);
    return function;
}

void CheckPendingImages() {
    std::lock_guard lock(countsMutex);
    for (const auto& [image, generation] : pendingImages) {
        const auto found = images.find(image);
        if (found == images.end() || found->second.generation != generation) {
            FailPendingImage();
        }
    }
}

Image ImageFor(VkImageView view, VkImage* image = nullptr) {
    std::lock_guard lock(countsMutex);
    const auto handle = views.at(view);
    if (image) *image = handle;
    return images.at(handle);
}

bool Alive(VkImage image, const Image& expected) {
    std::lock_guard lock(countsMutex);
    const auto found = images.find(image);
    return found != images.end() && found->second.generation == expected.generation && allocations.contains(expected.memory);
}

VkDeviceSize AllocationBytesOf(const Image& image) {
    std::lock_guard lock(countsMutex);
    return allocations.at(image.memory);
}

void AwaitImages(std::size_t limit, const std::string& what) {
    std::unique_lock lock(countsMutex);
    const bool released = countsChanged.wait_for(lock, std::chrono::seconds(2), [&] { return images.size() <= limit && allocations.size() <= limit; });
    Require(released, what + ": " + std::to_string(images.size()) + " images and " + std::to_string(allocations.size()) + " allocations remain");
}

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        data = VirtualAlloc(nullptr, BlockBytes, MEM_RESERVE, PAGE_NOACCESS);
        Require(data != nullptr, "cannot reserve the MSAA guest block");
#else
        data = mmap(nullptr, BlockBytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(data != MAP_FAILED, "cannot reserve the MSAA guest block");
#endif
        Map();
    }

    ~GuestBlock() {
        if (mapped) Unmap();
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        munmap(data, BlockBytes);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;

    void Map() {
        Require(!mapped, "the MSAA guest block is already mapped");
#ifdef _WIN32
        Require(VirtualAlloc(data, BlockBytes, MEM_COMMIT, PAGE_READWRITE) == data, "cannot commit the MSAA guest block");
#else
        Require(mmap(data, BlockBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) == data, "cannot map the MSAA guest block");
#endif
        GuestAllocations::Mutation().Add(data, BlockBytes, true, true, true);
        mapped = true;
    }

    void Unmap() {
        Require(mapped, "the MSAA guest block is not mapped");
        GuestAllocations::Mutation().Unmap(data, BlockBytes, [&](const void* begin, std::size_t bytes, const void*, bool last) {
            Require(begin == data && bytes == BlockBytes && last, "the MSAA allocation was not released as one range");
#ifdef _WIN32
            Require(VirtualFree(data, BlockBytes, MEM_DECOMMIT) != 0, "cannot decommit the MSAA guest block");
#else
            Require(mmap(data, BlockBytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) == data, "cannot unmap the MSAA guest block");
#endif
        });
        mapped = false;
        RequireNoMultisampleTarget(Address(), BlockBytes, "the released guest range");
    }

    std::uint64_t Address() const { return reinterpret_cast<std::uintptr_t>(data); }

private:
    void* data = nullptr;
    bool mapped = false;
};

ColorTarget Target(const GuestBlock& block) {
    ColorTarget target{};
    target.address = target.surfaceAddress = block.Address();
    target.extent = target.surfaceExtent = {Side, Side};
    target.format = VK_FORMAT_R8G8B8A8_UNORM;
    target.elementBytes = 4;
    target.samples = 4;
    target.tileMode = ColorTileMode::RenderTarget;
    target.bytes = ColorTargetLayout(Side, Side, target.tileMode, target.elementBytes).Bytes();
    Require(MultisampledColorBytes(target) == BlockBytes, "the MSAA guest footprint differs from its allocation");
    return target;
}

GuestTextureResource Resource(const GuestBlock& block) {
    GuestTextureResource resource{};
    resource.baseAddress = block.Address();
    resource.width = resource.height = Side;
    resource.mipCount = resource.allocatedMipCount = 1;
    resource.dimension = TextureDimension::k2DMsaa;
    resource.tileMode = TextureTileMode::kR64KBX;
    resource.format = 56;
    resource.samples = 4;
    return resource;
}

class Run {
public:
    explicit Run(AgcDriver::VulkanDevice& device) : device(device), context(AgcDriver::VulkanDeviceTestAccess::ContextOf(device)) {
        realProc = context.deviceProc;
        realAllocate = context.Function<PFN_vkAllocateMemory>("vkAllocateMemory");
        realFree = context.Function<PFN_vkFreeMemory>("vkFreeMemory");
        realCreateImage = context.Function<PFN_vkCreateImage>("vkCreateImage");
        realDestroyImage = context.Function<PFN_vkDestroyImage>("vkDestroyImage");
        realBindImageMemory = context.Function<PFN_vkBindImageMemory>("vkBindImageMemory");
        realCreateImageView = context.Function<PFN_vkCreateImageView>("vkCreateImageView");
        realDestroyImageView = context.Function<PFN_vkDestroyImageView>("vkDestroyImageView");
        realClearColorImage = context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage");
        realCopyImage = context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage");
        context.deviceProc = CountingDeviceProc;
        context.functions = nullptr;
    }

    ~Run() { Clear(); }

    void Sync() {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        CheckPendingImages();
        {
            std::lock_guard countsLock(countsMutex);
            pendingImages.clear();
        }
        device.SubmitRecorded(true);
        device.WaitIdle();
        context.recorder->FlushDeferredReleases();
    }

    void Clear() {
        Sync();
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        ClearMultisampleTargets(context.device);
    }

    void IdleRemaps() {
        GuestBlock block;
        const auto target = Target(block);
        for (unsigned cycle = 0; cycle < 32; ++cycle) {
            {
                std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
                Require(MultisampleTargetView(context, target) != VK_NULL_HANDLE, "an idle MSAA target has no view");
            }
            Sync();
            block.Unmap();
            if (cycle != 31) block.Map();
        }
        {
            std::lock_guard lock(countsMutex);
            std::printf("32 idle MSAA remaps: created=%llu destroyed=%llu live=%zu\n", static_cast<unsigned long long>(created), static_cast<unsigned long long>(destroyed), images.size());
        }
        AwaitImages(1, "unmapped idle MSAA targets accumulate");
        Clear();
        AwaitImages(0, "clearing idle MSAA targets did not release their allocations");
    }

    void RecordedRemap() {
        GuestBlock block;
        const auto target = Target(block);
        VkImage oldImage = VK_NULL_HANDLE;
        Image old{};
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            const auto keptBefore = context.recorder->OpenKeptBytes();
            std::shared_ptr<void> lookupOwner;
            VkImage lookupImage = VK_NULL_HANDLE;
            old = ImageFor(MultisampleTargetView(context, target, lookupOwner, &lookupImage), &oldImage);
            Require(lookupOwner != nullptr && lookupImage == oldImage, "the MSAA lookup did not return its owner and hazard image together");
            Require(context.recorder->Recording(), "MSAA initialization was not recorded");
            const auto kept = context.recorder->OpenKeptBytes();
            Require(kept >= keptBefore + AllocationBytesOf(old), "the pending MSAA allocation is missing from the recorder budget");
            Require(ImageFor(MultisampleTargetView(context, target)).generation == old.generation, "the repeated MSAA lookup changed its owner");
            Require(context.recorder->OpenKeptBytes() == kept, "a repeated MSAA lookup double-counts its allocation in one batch");
        }
        block.Unmap();
        block.Map();
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            const auto keptBefore = context.recorder->OpenKeptBytes();
            const auto replacement = ImageFor(MultisampleTargetView(context, target));
            CheckPendingImages();
            Require(replacement.generation != old.generation && Alive(oldImage, old), "a pending MSAA clear lost its image during remap");
            Require(context.recorder->OpenKeptBytes() >= keptBefore + AllocationBytesOf(replacement), "the replacement MSAA allocation is missing from the recorder budget");
        }
        Sync();
        AwaitImages(1, "the completed retired MSAA target was not released");
        Require(!Alive(oldImage, old), "the retired recorded MSAA allocation remains live");
        block.Unmap();
        Clear();
        AwaitImages(0, "recorded MSAA targets were not fully released");
    }

    void SampledRemap() {
        GuestBlock block;
        const auto target = Target(block);
        std::shared_ptr<Texture> held;
        VkImage oldImage = VK_NULL_HANDLE;
        Image old{};
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            MultisampleTargetView(context, target);
            held = MultisampleTargetTexture(context, Resource(block), {});
            old = ImageFor(held->View(), &oldImage);
        }
        const std::weak_ptr<Texture> lifetime = held;
        Sync();
        block.Unmap();
        block.Map();
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            const auto replacement = ImageFor(MultisampleTargetView(context, target));
            CheckPendingImages();
            Require(replacement.generation != old.generation, "a remapped sampled MSAA target reused its retired owner");
            Require(Alive(oldImage, old) && ImageFor(held->View()).generation == old.generation, "a held sampled MSAA view lost its parent allocation");
            context.recorder->Keep(held);
            held.reset();
            Require(!lifetime.expired() && Alive(oldImage, old), "a recorded sampled MSAA view lost its last alias");
        }
        Sync();
        AwaitImages(1, "a sampled MSAA owner or cached view forms a retention cycle");
        Require(lifetime.expired() && !Alive(oldImage, old), "a retired sampled MSAA parent survives its last alias and batch");
        block.Unmap();
        Clear();
        AwaitImages(0, "sampled MSAA targets were not fully released");
    }

    void StorageRemap() {
        GuestBlock block;
        const auto target = Target(block);
        const auto resource = Resource(block);
        std::shared_ptr<Texture> held;
        VkImageView sampledView = VK_NULL_HANDLE, storageView = VK_NULL_HANDLE;
        VkImage oldImage = VK_NULL_HANDLE, promotedImage = VK_NULL_HANDLE;
        Image old{}, promoted{};
        VkDeviceSize totalBytes = 0;
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            MultisampleTargetView(context, target);
            held = MultisampleTargetTexture(context, resource, {});
            sampledView = held->View();
            old = ImageFor(sampledView, &oldImage);
        }
        const std::weak_ptr<Texture> lifetime = held;
        Sync();
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            const auto keptBefore = context.recorder->OpenKeptBytes();
            storageView = MultisampleTargetStorageView(context, resource);
            promoted = ImageFor(storageView, &promotedImage);
            Require(promoted.generation != old.generation && Alive(oldImage, old), "MSAA storage promotion did not retain its source image");
            Require(ImageFor(sampledView).generation == old.generation, "MSAA storage promotion destroyed a held sampled view");
            Require(held->View() == sampledView, "MSAA storage promotion changed a held sampled alias");
            totalBytes = AllocationBytesOf(old) + AllocationBytesOf(promoted);
            const auto kept = context.recorder->OpenKeptBytes();
            Require(kept == keptBefore + totalBytes, "MSAA storage promotion did not account for exactly both allocations");
            Require(MultisampleTargetStorageView(context, resource) == storageView, "a repeated MSAA storage lookup changed its view");
            Require(context.recorder->OpenKeptBytes() == kept, "a repeated MSAA storage lookup double-counts its allocations");
        }
        Sync();
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            Require(Alive(oldImage, old) && ImageFor(sampledView).generation == old.generation, "a held sampled MSAA view did not survive storage promotion completion");
            Require(held->View() == sampledView, "a held sampled MSAA alias changed after promotion completion");
            held.reset();
            const auto keptBefore = context.recorder->OpenKeptBytes();
            Require(MultisampleTargetStorageView(context, resource) == storageView, "a later MSAA storage lookup changed its promoted image");
            const auto kept = context.recorder->OpenKeptBytes();
            Require(kept == keptBefore + totalBytes, "an already-storable MSAA target is missing allocations from the new batch budget");
            Require(MultisampleTargetStorageView(context, resource) == storageView, "a repeated promoted MSAA lookup changed its view");
            Require(context.recorder->OpenKeptBytes() == kept, "an already-storable MSAA target double-counts its allocations in one batch");
        }
        block.Unmap();
        block.Map();
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            const auto replacement = ImageFor(MultisampleTargetView(context, target));
            CheckPendingImages();
            Require(replacement.generation != promoted.generation, "a remapped MSAA target reused its promoted storage owner");
            Require(!lifetime.expired() && Alive(oldImage, old) && Alive(promotedImage, promoted), "a pending MSAA storage use lost its promoted or retired allocation");
        }
        Sync();
        AwaitImages(1, "the completed promoted MSAA owner was not released");
        Require(lifetime.expired() && !Alive(oldImage, old) && !Alive(promotedImage, promoted), "a retired MSAA storage owner retained its allocations after completion");
        block.Unmap();
        Clear();
        AwaitImages(0, "promoted MSAA targets were not fully released");
    }

private:
    AgcDriver::VulkanDevice& device;
    Context context;
};

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
        Require((context.limits.framebufferColorSampleCounts & VK_SAMPLE_COUNT_4_BIT) != 0, "the retirement fixture needs 4x color targets");
        Run run(*device);
        run.IdleRemaps();
        run.RecordedRemap();
        run.SampledRemap();
        run.StorageRemap();
        std::puts("retired MSAA allocations stay bounded, recorded clears and storage promotions retain their images, and sampled aliases release their parents after the last batch");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
