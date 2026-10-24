#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string_view>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;

constexpr std::size_t ImageCount = 8;
constexpr std::uint32_t Side = 64;
constexpr std::uint32_t Format32UInt = 20;
constexpr std::size_t GuestStride = 65536;
constexpr std::size_t PixelBytes = std::size_t{Side} * Side * sizeof(std::uint32_t);

bool FitsSharedPair(const VkMemoryRequirements& memory) {
    constexpr VkDeviceSize blockBytes = VkDeviceSize{4} << 20u;
    if (memory.size > (VkDeviceSize{1} << 20u) || memory.alignment == 0 || memory.alignment > blockBytes) return false;
    const auto next = (memory.size + memory.alignment - 1u) / memory.alignment * memory.alignment;
    return next <= blockBytes - memory.size;
}

class Hooks;
Hooks* hooks = nullptr;

class Hooks {
public:
    struct Binding {
        VkDeviceMemory memory;
        VkDeviceSize offset;
    };

    struct Pressure {
        VkDeviceSize bytes = 0;
        VkDeviceMemory waitForFree = VK_NULL_HANDLE;
        std::uint64_t previousFrees = 0;
        std::uint64_t released = 0;
        unsigned failures = 0;
        unsigned requests = 0;
        bool armed = false;
    };

    explicit Hooks(Context& context) : context(context), original(context.deviceProc) {
        Require(hooks == nullptr, "image memory: native hooks are already active");
        hooks = this;
        context.deviceProc = Resolve;
    }

    ~Hooks() {
        context.deviceProc = original;
        hooks = nullptr;
    }

    VkImage ImageOf(VkImageView view) {
        std::lock_guard lock(mutex);
        return views.at(view);
    }

    bool Destroyed(VkImage image) {
        std::lock_guard lock(mutex);
        return destroyed.contains(image);
    }

    Binding BindingOf(VkImage image) {
        std::lock_guard lock(mutex);
        return bindings.at(image);
    }

    VkDeviceSize AllocationBytes(VkDeviceMemory memory) {
        std::lock_guard lock(mutex);
        return allocations.at(memory);
    }

    std::uint64_t Frees(VkDeviceMemory memory) {
        std::lock_guard lock(mutex);
        const auto found = frees.find(memory);
        return found == frees.end() ? 0 : found->second;
    }

    VkMemoryRequirements Requirements(VkImage image) {
        VkMemoryRequirements requirements{};
        reinterpret_cast<PFN_vkGetImageMemoryRequirements>(original(context.device, "vkGetImageMemoryRequirements"))(context.device, image, &requirements);
        return requirements;
    }

    void BeginPressure(VkDeviceSize bytes, VkDeviceMemory waitForFree = VK_NULL_HANDLE) {
        const auto previous = Frees(waitForFree);
        std::lock_guard lock(mutex);
        pressure = {bytes, waitForFree, previous, 0, 0, 0, true};
    }

    Pressure EndPressure() {
        std::lock_guard lock(mutex);
        pressure.armed = false;
        return pressure;
    }

    static VkDeviceSize Reclaim(VkDeviceSize bytes, bool newImage) {
        Require(newImage, "image memory: image pressure lost its safe same-cache permission");
        Require(ReleaseTextureCaches(bytes, false) == 0, "image memory: an ordinary allocation reclaimed the sampled cache during image creation");
        const auto released = ReleaseTextureCaches(bytes, newImage);
        auto& self = *hooks;
        std::lock_guard lock(self.mutex);
        ++self.pressure.requests;
        self.pressure.released += released;
        if (self.pressure.waitForFree == VK_NULL_HANDLE) self.pressure.armed = false;
        return released;
    }

    void CheckBindings(const std::vector<VkImage>& images, bool sharing) {
        std::lock_guard lock(mutex);
        std::size_t eligible = 0;
        std::size_t shared = 0;
        for (std::size_t i = 0; i < images.size(); ++i) {
            VkMemoryDedicatedRequirements dedicated{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS};
            VkMemoryRequirements2 requirements{VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, &dedicated};
            VkImageMemoryRequirementsInfo2 info{VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2};
            info.image = images[i];
            reinterpret_cast<PFN_vkGetImageMemoryRequirements2>(original(context.device, "vkGetImageMemoryRequirements2"))(context.device, &info, &requirements);
            const auto& memory = requirements.memoryRequirements;
            const auto binding = bindings.at(images[i]);
            Require(binding.offset % memory.alignment == 0, "image memory: an image is bound at an unaligned offset");
            if (!dedicated.requiresDedicatedAllocation && !dedicated.prefersDedicatedAllocation && FitsSharedPair(memory)) ++eligible;
            for (std::size_t j = 0; j < i; ++j) {
                const auto other = bindings.at(images[j]);
                if (other.memory != binding.memory) continue;
                ++shared;
                VkMemoryRequirements previous{};
                reinterpret_cast<PFN_vkGetImageMemoryRequirements>(original(context.device, "vkGetImageMemoryRequirements"))(context.device, images[j], &previous);
                Require(binding.offset + memory.size <= other.offset || other.offset + previous.size <= binding.offset, "image memory: live production images overlap inside a memory block");
            }
        }
        if (sharing && eligible > 1) Require(shared != 0, "image memory: simultaneous small production images each allocate separate memory");
        std::printf("image memory: %zu images, %zu eligible, %zu shared-memory pairs\n", images.size(), eligible, shared);
    }

    void CheckReleased() {
        std::lock_guard lock(mutex);
        Require(bindings.empty() && views.empty(), "image memory: production image objects outlive their final GPU use");
        Require(!freedLiveImage, "image memory: a memory block was freed while a sibling image remained live");
    }

private:
    static VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL Resolve(VkDevice device, const char* name) {
        if (std::strcmp(name, "vkBindImageMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(Bind);
        if (std::strcmp(name, "vkDestroyImage") == 0) return reinterpret_cast<PFN_vkVoidFunction>(Destroy);
        if (std::strcmp(name, "vkCreateImageView") == 0) return reinterpret_cast<PFN_vkVoidFunction>(CreateView);
        if (std::strcmp(name, "vkDestroyImageView") == 0) return reinterpret_cast<PFN_vkVoidFunction>(DestroyView);
        if (std::strcmp(name, "vkAllocateMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(Allocate);
        if (std::strcmp(name, "vkFreeMemory") == 0) return reinterpret_cast<PFN_vkVoidFunction>(Free);
        return hooks->original(device, name);
    }

    static VKAPI_ATTR VkResult VKAPI_CALL Allocate(VkDevice device, const VkMemoryAllocateInfo* info, const VkAllocationCallbacks* allocator, VkDeviceMemory* memory) {
        auto& self = *hooks;
        {
            std::lock_guard lock(self.mutex);
            const auto found = self.frees.find(self.pressure.waitForFree);
            const auto freed = found == self.frees.end() ? 0 : found->second;
            if (self.pressure.armed && info->allocationSize == self.pressure.bytes && (self.pressure.waitForFree == VK_NULL_HANDLE || freed == self.pressure.previousFrees)) {
                ++self.pressure.failures;
                *memory = VK_NULL_HANDLE;
                return VK_ERROR_OUT_OF_DEVICE_MEMORY;
            }
        }
        const auto result = reinterpret_cast<PFN_vkAllocateMemory>(self.original(device, "vkAllocateMemory"))(device, info, allocator, memory);
        if (result == VK_SUCCESS) {
            std::lock_guard lock(self.mutex);
            self.allocations[*memory] = info->allocationSize;
        }
        return result;
    }

    static VKAPI_ATTR void VKAPI_CALL Free(VkDevice device, VkDeviceMemory memory, const VkAllocationCallbacks* allocator) {
        auto& self = *hooks;
        {
            std::lock_guard lock(self.mutex);
            for (const auto& [image, binding] : self.bindings) {
                static_cast<void>(image);
                if (binding.memory == memory) self.freedLiveImage = true;
            }
            self.allocations.erase(memory);
            ++self.frees[memory];
        }
        reinterpret_cast<PFN_vkFreeMemory>(self.original(device, "vkFreeMemory"))(device, memory, allocator);
    }

    static VKAPI_ATTR VkResult VKAPI_CALL Bind(VkDevice device, VkImage image, VkDeviceMemory memory, VkDeviceSize offset) {
        auto& self = *hooks;
        const auto result = reinterpret_cast<PFN_vkBindImageMemory>(self.original(device, "vkBindImageMemory"))(device, image, memory, offset);
        if (result == VK_SUCCESS) {
            std::lock_guard lock(self.mutex);
            self.bindings[image] = {memory, offset};
            self.destroyed.erase(image);
        }
        return result;
    }

    static VKAPI_ATTR void VKAPI_CALL Destroy(VkDevice device, VkImage image, const VkAllocationCallbacks* allocator) {
        auto& self = *hooks;
        reinterpret_cast<PFN_vkDestroyImage>(self.original(device, "vkDestroyImage"))(device, image, allocator);
        std::lock_guard lock(self.mutex);
        self.bindings.erase(image);
        self.destroyed.insert(image);
    }

    static VKAPI_ATTR VkResult VKAPI_CALL CreateView(VkDevice device, const VkImageViewCreateInfo* info, const VkAllocationCallbacks* allocator, VkImageView* view) {
        auto& self = *hooks;
        const auto result = reinterpret_cast<PFN_vkCreateImageView>(self.original(device, "vkCreateImageView"))(device, info, allocator, view);
        if (result == VK_SUCCESS) {
            std::lock_guard lock(self.mutex);
            self.views[*view] = info->image;
        }
        return result;
    }

    static VKAPI_ATTR void VKAPI_CALL DestroyView(VkDevice device, VkImageView view, const VkAllocationCallbacks* allocator) {
        auto& self = *hooks;
        reinterpret_cast<PFN_vkDestroyImageView>(self.original(device, "vkDestroyImageView"))(device, view, allocator);
        std::lock_guard lock(self.mutex);
        self.views.erase(view);
    }

    Context& context;
    PFN_vkGetDeviceProcAddr original;
    std::mutex mutex;
    std::map<VkImage, Binding> bindings;
    std::map<VkImageView, VkImage> views;
    std::set<VkImage> destroyed;
    std::map<VkDeviceMemory, VkDeviceSize> allocations;
    std::map<VkDeviceMemory, std::uint64_t> frees;
    Pressure pressure;
    bool freedLiveImage = false;
};

GuestTextureResource Resource(std::uint64_t address) {
    GuestTextureResource resource{};
    resource.baseAddress = address;
    resource.width = Side;
    resource.height = Side;
    resource.mipCount = 1;
    resource.tileMode = TextureTileMode::kLinear;
    resource.dimension = TextureDimension::k2D;
    resource.format = Format32UInt;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    return resource;
}

void SampledImages(const Context& context, TextureDetiler& detiler, Hooks& native, bool sharing) {
    const auto resource = Resource(0x100000);
    const auto geometry = DescribeSurface(resource);
    std::vector<std::byte> snapshot(static_cast<std::size_t>(geometry.guestBytes), std::byte{0x35});
    const VkComponentMapping identity{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    std::vector<VkImage> pending;
    std::vector<VkImage> all;
    std::vector<std::unique_ptr<Texture>> textures;
    {
        Recorder recorder(context);
        recorder.Activate();
        for (std::size_t i = 0; i < ImageCount; ++i) {
            auto texture = std::make_unique<Texture>(context, detiler, resource, identity, snapshot);
            pending.push_back(native.ImageOf(texture->View()));
            textures.push_back(std::move(texture));
        }
        native.CheckBindings(pending, sharing);
        textures.clear();
        for (const auto image : pending) Require(!native.Destroyed(image), "image memory: a texture released its image while a recorded upload still uses it");
        all = pending;
        for (std::size_t i = 0; i < ImageCount; ++i) {
            auto texture = std::make_unique<Texture>(context, detiler, resource, identity, snapshot);
            all.push_back(native.ImageOf(texture->View()));
            textures.push_back(std::move(texture));
        }
        native.CheckBindings(all, sharing);
        recorder.Sync();
    }
    for (const auto image : pending) Require(native.Destroyed(image), "image memory: recorder teardown still holds its released texture image");
    for (std::size_t i = pending.size(); i < all.size(); ++i) Require(!native.Destroyed(all[i]), "image memory: recorder teardown destroyed a texture that is still live");
    textures.clear();
    native.CheckReleased();
}

class GuestBlock {
public:
    explicit GuestBlock(std::size_t bytes = ImageCount * GuestStride) {
#ifdef _WIN32
        data = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
        data = std::aligned_alloc(GuestStride, bytes);
#endif
        Require(data != nullptr, "image memory: cannot allocate guest texture bytes");
        std::memset(data, 0, bytes);
        GuestAllocations::Mutation().Add(data, bytes, true, true, true);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(data);
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        std::free(data);
#endif
    }

    std::uint64_t Address(std::size_t i) const {
        return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data)) + i * GuestStride;
    }

private:
    void* data = nullptr;
};

void StorageImages(const Context& context, TextureDetiler& detiler, Hooks& native, bool sharing) {
    GuestBlock guest;
    std::vector<std::shared_ptr<StorageTexture>> textures;
    std::vector<VkImage> images;
    for (std::size_t i = 0; i < ImageCount; ++i) {
        auto texture = StorageTexture::Create(context, detiler, Resource(guest.Address(i)), 0);
        images.push_back(texture->Image());
        textures.push_back(std::move(texture));
    }
    native.CheckBindings(images, sharing);
    Buffer readback(context, ImageCount * PixelBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    CommandBatch batch(context);
    const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    for (std::size_t i = 0; i < ImageCount; ++i) {
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = images[i];
        barrier.subresourceRange = range;
        context.Function<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(batch.Handle(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        VkClearColorValue clear{};
        clear.uint32[0] = 0x13579bdfu + static_cast<std::uint32_t>(i) * 0x1020304u;
        context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(batch.Handle(), images[i], VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &range);
    }
    RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    for (std::size_t i = 0; i < ImageCount; ++i) {
        VkBufferImageCopy region{};
        region.bufferOffset = i * PixelBytes;
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent = {Side, Side, 1};
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(batch.Handle(), images[i], VK_IMAGE_LAYOUT_GENERAL, readback.Handle(), 1, &region);
    }
    RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    batch.SubmitAndWait();
    readback.Invalidate();
    const auto bytes = readback.Bytes();
    for (std::size_t i = 0; i < ImageCount; ++i) {
        const auto expected = 0x13579bdfu + static_cast<std::uint32_t>(i) * 0x1020304u;
        for (std::size_t pixel = 0; pixel < Side * Side; ++pixel) {
            std::uint32_t actual = 0;
            std::memcpy(&actual, bytes.data() + i * PixelBytes + pixel * sizeof(actual), sizeof(actual));
            Require(actual == expected, "image memory: writing one storage image changed a sibling's pixels");
        }
    }
    textures.clear();
    native.CheckReleased();
}

void CachePressure(Context context, Hooks& native) {
    constexpr std::size_t MaxImages = 64;
    auto resource = Resource(0);
    resource.width = 512;
    resource.height = 256;
    VkImageCreateInfo smallInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    smallInfo.imageType = VK_IMAGE_TYPE_2D;
    smallInfo.format = VK_FORMAT_R32_UINT;
    smallInfo.extent = {resource.width, resource.height, 1};
    smallInfo.mipLevels = 1;
    smallInfo.arrayLayers = 1;
    smallInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    smallInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    smallInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    VkImage smallProbe = VK_NULL_HANDLE;
    Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &smallInfo, nullptr, &smallProbe), "vkCreateImage shared pressure probe");
    VkMemoryDedicatedRequirements dedicated{VK_STRUCTURE_TYPE_MEMORY_DEDICATED_REQUIREMENTS};
    VkMemoryRequirements2 smallRequirements{VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2, &dedicated};
    VkImageMemoryRequirementsInfo2 request{VK_STRUCTURE_TYPE_IMAGE_MEMORY_REQUIREMENTS_INFO_2};
    request.image = smallProbe;
    context.Function<PFN_vkGetImageMemoryRequirements2>("vkGetImageMemoryRequirements2")(context.device, &request, &smallRequirements);
    context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, smallProbe, nullptr);
    const auto& needed = smallRequirements.memoryRequirements;
    if (dedicated.requiresDedicatedAllocation || dedicated.prefersDedicatedAllocation || !FitsSharedPair(needed)) {
        std::printf("image memory: cache-pressure subcase skipped: dedicated=%u preferred=%u size=%llu alignment=%llu require standalone memory\n", dedicated.requiresDedicatedAllocation, dedicated.prefersDedicatedAllocation, static_cast<unsigned long long>(needed.size), static_cast<unsigned long long>(needed.alignment));
        return;
    }
    const auto geometry = DescribeSurface(resource);
    const auto stride = (geometry.guestBytes + GuestStride - 1) / GuestStride * GuestStride;
    GuestBlock guest(static_cast<std::size_t>(MaxImages * stride + 8 * 1024 * 1024));
    context.bufferPool.reset();
    context.cacheReclaim = Hooks::Reclaim;
    ClearCachedTextures(context.device);
    OverrideCacheBudgetForTest(128ull << 20u);
    const auto sample = [&](const GuestTextureResource& described) {
        const auto words = EncodeTextureResource(described);
        return CachedSampledTexture(context, words);
    };
    const auto cleanup = [&] {
        native.EndPressure();
        ClearCachedTextures(context.device);
        context.bufferPool.reset();
        OverrideCacheBudgetForTest(0);
    };
    try {
        VkDeviceMemory reclaimMemory = VK_NULL_HANDLE;
        VkDeviceMemory heldMemory = VK_NULL_HANDLE;
        VkDeviceSize blockBytes = 0;
        VkDeviceSize previousOffset = 0;
        VkDeviceSize bindingStride = 0;
        bool full = false;
        std::size_t count = 0;
        std::vector<std::weak_ptr<Texture>> evicted;
        std::vector<std::shared_ptr<Texture>> held;
        std::vector<GuestTextureResource> heldResources;
        std::vector<VkImage> heldImages;
        for (; count < MaxImages - 1; ++count) {
            resource.baseAddress = guest.Address(0) + count * stride;
            auto texture = sample(resource);
            Require(!texture->ViewsStorageImage(), "image memory: cache pressure needs independently owned sampled images");
            const auto image = native.ImageOf(texture->View());
            const auto binding = native.BindingOf(image);
            const auto needed = native.Requirements(image);
            if (reclaimMemory == VK_NULL_HANDLE) {
                reclaimMemory = binding.memory;
                blockBytes = native.AllocationBytes(reclaimMemory);
                Require(blockBytes > needed.size, "image memory: cache pressure did not allocate an eligible shared image block");
            }
            if (binding.memory == reclaimMemory) {
                Require(heldMemory == VK_NULL_HANDLE, "image memory: the fixture returned to a full first block");
                evicted.push_back(texture);
                continue;
            }
            if (heldMemory == VK_NULL_HANDLE) heldMemory = binding.memory;
            Require(binding.memory == heldMemory, "image memory: the fixture allocated a third block before filling the held one");
            if (!held.empty()) {
                Require(binding.offset > previousOffset, "image memory: identical fixture images are not bound in increasing ranges");
                const auto step = binding.offset - previousOffset;
                Require(bindingStride == 0 || bindingStride == step, "image memory: identical fixture images have inconsistent binding strides");
                bindingStride = step;
            }
            previousOffset = binding.offset;
            held.push_back(std::move(texture));
            heldResources.push_back(resource);
            heldImages.push_back(image);
            if (bindingStride != 0 && binding.offset + bindingStride + needed.size > native.AllocationBytes(heldMemory)) {
                ++count;
                full = true;
                break;
            }
        }
        Require(full && !evicted.empty() && !held.empty(), "image memory: bounded cache-pressure fixture did not fill two shared blocks");
        Require(count + 2 < MaxImages, "image memory: the partial-return fixtures exceed the bounded guest allocation");
        native.CheckBindings(heldImages, true);
        const auto heldFrees = native.Frees(heldMemory);
        native.BeginPressure(blockBytes);
        resource.baseAddress = guest.Address(0) + count * stride;
        auto made = sample(resource);
        const auto smallPressure = native.EndPressure();
        Require(smallPressure.failures != 0 && smallPressure.requests != 0, "image memory: small cache pressure did not reach driver failure and reentrant reclamation");
        Require(smallPressure.released == blockBytes, "image memory: cache reclamation reported image bytes instead of one whole reclaimable block");
        Require(std::all_of(evicted.begin(), evicted.end(), [](const auto& image) { return image.expired(); }), "image memory: releasing a shared block left one of its cache-only textures alive");
        Require(native.BindingOf(native.ImageOf(made->View())).memory == reclaimMemory, "image memory: a recovered small image did not reuse the cache's returned block");
        Require(native.Frees(heldMemory) == heldFrees, "image memory: recovering a small allocation freed a live sibling block");
        for (std::size_t i = 0; i < held.size(); ++i) {
            Require(!native.Destroyed(heldImages[i]), "image memory: cache pressure destroyed an externally held image");
            Require(sample(heldResources[i]) == held[i], "image memory: cache pressure evicted an externally held sampled entry");
        }
        made.reset();

        auto large = Resource(guest.Address(0) + MaxImages * stride);
        large.width = 1024;
        large.height = 512;
        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VK_FORMAT_R32_UINT;
        imageInfo.extent = {large.width, large.height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        VkImage probe = VK_NULL_HANDLE;
        Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &imageInfo, nullptr, &probe), "vkCreateImage large pressure probe");
        const auto largeBytes = native.Requirements(probe).size;
        context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, probe, nullptr);
        Require(largeBytes > 1024 * 1024, "image memory: the standalone pressure fixture is not larger than the shared-image limit");
        const auto beforeFree = native.Frees(reclaimMemory);
        native.BeginPressure(largeBytes, reclaimMemory);
        auto standalone = sample(large);
        const auto largePressure = native.EndPressure();
        Require(largePressure.failures != 0 && largePressure.requests != 0, "image memory: standalone cache pressure did not exercise driver failure and reclamation");
        Require(largePressure.released == blockBytes, "image memory: standalone recovery did not report the returned whole image block");
        Require(native.Frees(reclaimMemory) > beforeFree, "image memory: standalone recovery retried before freeing the returned empty block");
        Require(native.Frees(heldMemory) == heldFrees, "image memory: standalone recovery freed a block with live sibling images");
        for (std::size_t i = 0; i < held.size(); ++i) Require(!native.Destroyed(heldImages[i]) && sample(heldResources[i]) == held[i], "image memory: standalone recovery changed a held cached image");

        resource.baseAddress = guest.Address(0) + (count + 1) * stride;
        auto disposable = sample(resource);
        const auto disposableImage = native.ImageOf(disposable->View());
        const auto partialMemory = native.BindingOf(disposableImage).memory;
        const auto partialBytes = native.AllocationBytes(partialMemory);
        std::weak_ptr<Texture> disposableWeak = disposable;
        disposable.reset();
        resource.baseAddress = guest.Address(0) + (count + 2) * stride;
        auto sibling = sample(resource);
        const auto siblingImage = native.ImageOf(sibling->View());
        Require(native.BindingOf(siblingImage).memory == partialMemory, "image memory: the partial-return fixture did not create shared siblings");
        const auto partialFrees = native.Frees(partialMemory);
        Require(ReleaseTextureCaches(1) == 0, "image memory: evicting an image from a partially live block reported reclaimable memory");
        Require(disposableWeak.expired() && native.Destroyed(disposableImage), "image memory: a zero-byte cache release did not evict the unused sibling");
        Require(!native.Destroyed(siblingImage) && native.Frees(partialMemory) == partialFrees, "image memory: evicting one cached image freed a held sibling's memory block");
        Require(sample(resource) == sibling, "image memory: a partial block release evicted its held cached sibling");
        std::weak_ptr<Texture> siblingWeak = sibling;
        sibling.reset();
        Require(ReleaseTextureCaches(1) == partialBytes, "image memory: evicting the last sibling did not report its whole reclaimable block");
        Require(siblingWeak.expired() && native.Destroyed(siblingImage), "image memory: a whole-block release left the last cached sibling alive");
        std::printf("image memory: cache pressure filled %zu images, reclaimed %llu-byte blocks; small %u failures/%u reclaims, standalone %u failures/%u reclaims\n", count, static_cast<unsigned long long>(blockBytes), smallPressure.failures, smallPressure.requests, largePressure.failures, largePressure.requests);
        std::printf("image memory: partial shared-block cache release returned 0 bytes, final sibling returned %llu bytes\n", static_cast<unsigned long long>(partialBytes));
    } catch (...) {
        cleanup();
        throw;
    }
    cleanup();
    native.CheckReleased();
}

}

int main(int argc, char** argv) {
    try {
        bool sharing = true;
        bool lifetimesOnly = false;
        if (argc == 2 && std::string_view(argv[1]) == "--no-sharing-check") sharing = false;
        else if (argc == 2 && std::string_view(argv[1]) == "--lifetimes-only") {
            sharing = false;
            lifetimesOnly = true;
        } else Require(argc == 1, "image memory: unknown arguments");
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::unique_lock gpu(AgcDriver::GuestMemory::GpuMutex());
        auto context = device->graphicsContext();
        Hooks native(context);
        TextureDetiler detiler(context);
        context.detiler = &detiler;
        SampledImages(context, detiler, native, sharing);
        if (!lifetimesOnly) StorageImages(context, detiler, native, sharing);
        if (sharing) CachePressure(context, native);
        std::puts("production image memory tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
