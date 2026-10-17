#include <shared_mutex>
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics {
namespace {

bool sameTarget(const ColorTarget& a, const ColorTarget& b) {
    return a.surfaceAddress == b.surfaceAddress && a.format == b.format && a.extent.width == b.extent.width && a.extent.height == b.extent.height && a.samples == b.samples;
}

VkClearColorValue fastClearColor(const ColorTarget& target) {
    Require(target.elementBytes != 0 && target.elementBytes <= sizeof(target.clearWords) && 16u % target.elementBytes == 0, "the fast clear of a multisampled target over 64-bit texels is not modeled");
    std::array<std::byte, 16> texels{};
    for (std::size_t offset = 0; offset < texels.size(); offset += target.elementBytes) std::memcpy(texels.data() + offset, target.clearWords.data(), target.elementBytes);
    std::array<std::uint32_t, 4> pattern{};
    std::memcpy(pattern.data(), texels.data(), texels.size());
    VkClearColorValue clear{};
    Require(ClearColorForTexel(target.format, target.elementBytes, pattern, clear), "the fast-clear color of a multisampled target has no exact clear value in its format");
    return clear;
}

template<typename Record>
void recordCommands(const Context& context, Record&& record) {
    auto* recorder = Recorder::Active();
    std::unique_ptr<CommandBatch> batch;
    if (recorder == nullptr) batch = std::make_unique<CommandBatch>(context);
    record(recorder != nullptr ? recorder->Commands() : batch->Handle(), recorder);
    if (batch) batch->SubmitAndWait();
}

class MultisampleTarget {
public:
    MultisampleTarget(const Context& context, const ColorTarget& target) : context(context), target(target), cmask(target.cmaskAddress, target.cmaskBytes), colorFill(target) {
        this->context.bufferPool.reset();
        if (target.dccAddress != 0) metadataValid = std::make_shared<std::atomic<bool>>(true);
        const auto initial = initialDccClear(target);
        const auto samples = static_cast<VkSampleCountFlagBits>(target.samples);
        Require((context.limits.framebufferColorSampleCounts & samples) != 0, "the device cannot render color with the target's sample count");
        VkFormatProperties properties{};
        context.formatProperties(context.physical, target.format, &properties);
        Require((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) != 0, "multisampled render-target format cannot be a color attachment");
        usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        VkImageFormatProperties supported{};
        sampleable = (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0 && context.imageFormatProperties(context.physical, target.format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage | VK_IMAGE_USAGE_SAMPLED_BIT, 0, &supported) == VK_SUCCESS && (supported.sampleCounts & samples) != 0;
        if (sampleable) usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
        Check(context.imageFormatProperties(context.physical, target.format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage, 0, &supported), "vkGetPhysicalDeviceImageFormatProperties multisampled target");
        Require((supported.sampleCounts & samples) != 0 && target.extent.width <= supported.maxExtent.width && target.extent.height <= supported.maxExtent.height, "multisampled render target exceeds device image limits");
        storageFormat = HostStorageFormat(context, target.format);
        storageFlags = storageFormat != target.format ? VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT | VK_IMAGE_CREATE_EXTENDED_USAGE_BIT : 0u;
        const auto storageSupports = [&](VkFormat format, VkImageUsageFlags formatUsage, VkImageCreateFlags flags) {
            VkImageFormatProperties properties{};
            return context.imageFormatProperties(context.physical, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, formatUsage, flags, &properties) == VK_SUCCESS && (properties.sampleCounts & samples) != 0;
        };
        if (storageFormat != VK_FORMAT_UNDEFINED && (!context.storageImageMultisample || !storageSupports(storageFormat, VK_IMAGE_USAGE_STORAGE_BIT, 0u) || !storageSupports(target.format, usage | VK_IMAGE_USAGE_STORAGE_BIT, storageFlags))) storageFormat = VK_FORMAT_UNDEFINED;
        try {
            const auto created = allocate(usage, 0u);
            image = created.image;
            memory = created.memory;
            view = created.view;
            recordCommands(context, [&](VkCommandBuffer commands, Recorder* recorder) {
                recordToGeneral(commands, image);
                const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
                context.Resolved(&DeviceFunctions::cmdClearColorImage, "vkCmdClearColorImage")(commands, image, VK_IMAGE_LAYOUT_GENERAL, &initial, 1, &range);
                RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
                if (recorder != nullptr) Recorder::CountBarriers(Recorder::CommandClass::Draw, 2);
            });
        } catch (...) {
            release();
            throw;
        }
    }
    ~MultisampleTarget() { release(); }
    MultisampleTarget(const MultisampleTarget&) = delete;
    MultisampleTarget& operator=(const MultisampleTarget&) = delete;

    struct Allocation {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
    };

    Allocation allocate(VkImageUsageFlags imageUsage, VkImageCreateFlags flags) {
        Allocation created;
        try {
            VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            info.flags = flags;
            info.imageType = VK_IMAGE_TYPE_2D;
            info.format = target.format;
            info.extent = {target.extent.width, target.extent.height, 1};
            info.mipLevels = 1;
            info.arrayLayers = 1;
            info.samples = static_cast<VkSampleCountFlagBits>(target.samples);
            info.tiling = VK_IMAGE_TILING_OPTIMAL;
            info.usage = imageUsage;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &info, nullptr, &created.image), "vkCreateImage multisampled target");
            VkMemoryRequirements requirements{};
            context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, created.image, &requirements);
            VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            allocation.allocationSize = requirements.size;
            allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &created.memory), "vkAllocateMemory multisampled target");
            Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, created.image, created.memory, 0), "vkBindImageMemory multisampled target");
            VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            viewInfo.image = created.image;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = target.format;
            viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &created.view), "vkCreateImageView multisampled target");
        } catch (...) {
            destroy(created);
            throw;
        }
        return created;
    }

    void destroy(const Allocation& allocation) noexcept {
        if (allocation.view) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, allocation.view, nullptr);
        if (allocation.image) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, allocation.image, nullptr);
        if (allocation.memory) context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, allocation.memory, nullptr);
    }

    void recordToGeneral(VkCommandBuffer commands, VkImage destination) {
        VkImageMemoryBarrier toGeneral{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        toGeneral.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        toGeneral.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        toGeneral.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toGeneral.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        toGeneral.image = destination;
        toGeneral.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier")(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral);
    }

    void makeStorable(Recorder& recorder) {
        if ((usage & VK_IMAGE_USAGE_STORAGE_BIT) != 0) return;
        const auto storable = allocate(usage | VK_IMAGE_USAGE_STORAGE_BIT, storageFlags);
        const auto source = image;
        recordToGeneral(recorder.Commands(), storable.image);
        RecordMemoryBarrier(context, recorder.Commands(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        VkImageCopy region{};
        region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.dstSubresource = region.srcSubresource;
        region.extent = {target.extent.width, target.extent.height, 1};
        context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(recorder.Commands(), source, VK_IMAGE_LAYOUT_GENERAL, storable.image, VK_IMAGE_LAYOUT_GENERAL, 1, &region);
        RecordMemoryBarrier(context, recorder.Commands(), VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        Recorder::CountBarriers(Recorder::CommandClass::Draw, 3);
        for (const auto& [key, texture] : textures) recorder.Keep(texture);
        textures.clear();
        retired.push_back({image, memory, view});
        image = storable.image;
        memory = storable.memory;
        view = storable.view;
        usage |= VK_IMAGE_USAGE_STORAGE_BIT;
    }

    void ApplyFastClear(const ColorTarget& use) {
        checkDccCurrent(use);
        VkClearColorValue clear{};
        if (!TakeMultisampledClear(cmask, colorFill, use, clear)) return;
        recordCommands(context, [&](VkCommandBuffer commands, Recorder* recorder) {
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
            const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            context.Resolved(&DeviceFunctions::cmdClearColorImage, "vkCmdClearColorImage")(commands, image, VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &range);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
            if (recorder != nullptr) Recorder::CountBarriers(Recorder::CommandClass::Draw, 2);
        });
    }

    void SettleStorageWrites() {
        if (!storageWritten) return;
        storageWritten = false;
        recordCommands(context, [&](VkCommandBuffer commands, Recorder* recorder) {
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
            if (recorder != nullptr) Recorder::CountBarriers(Recorder::CommandClass::Draw, 1);
        });
    }

    VkImageView Storage(VkFormat format) {
        Require(storageFormat != VK_FORMAT_UNDEFINED, "the device cannot write the multisampled color target's format as a storage image at its sample count");
        Require(format == storageFormat, "a storage image access to a multisampled color target in another format than the target's storage form is not implemented");
        Require(cmask.Refusal().empty(), cmask.Refusal());
        Require(cmask.Keys() != DccKeys::Clear0000, "a storage image access to a multisampled color target whose CMASK fast clear no draw has applied is not implemented");
        auto* recording = Recorder::Active();
        Require(recording != nullptr, "a multisampled color target is only written by recorded dispatches");
        makeStorable(*recording);
        ApplyFastClear(target);
        if (storageView == VK_NULL_HANDLE) {
            VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            viewInfo.image = image;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = storageFormat;
            viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
            Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &storageView), "vkCreateImageView multisampled target storage");
        }
        recordCommands(context, [&](VkCommandBuffer commands, Recorder* recorder) {
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
            if (recorder != nullptr) Recorder::CountBarriers(Recorder::CommandClass::Draw, 1);
        });
        storageWritten = true;
        return storageView;
    }

    bool Storable() const {
        return (usage & VK_IMAGE_USAGE_STORAGE_BIT) != 0;
    }

    std::shared_ptr<Texture> Sampled(VkComponentMapping components) {
        Require(sampleable, "the device cannot sample the multisampled color target's format at its sample count");
        Require(cmask.Refusal().empty(), cmask.Refusal());
        Require(cmask.Keys() != DccKeys::Clear0000, "sampling a multisampled color target whose CMASK fast clear no draw has applied is not implemented");
        Require(Recorder::Active() != nullptr, "a multisampled color target is only sampled by recorded draws and dispatches");
        SettleStorageWrites();
        ApplyFastClear(target);
        recordCommands(context, [&](VkCommandBuffer commands, Recorder*) {
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
            Recorder::CountBarriers(Recorder::CommandClass::Draw, 1);
        });
        const std::array<std::uint32_t, 4> key{static_cast<std::uint32_t>(components.r), static_cast<std::uint32_t>(components.g), static_cast<std::uint32_t>(components.b), static_cast<std::uint32_t>(components.a)};
        if (const auto found = textures.find(key); found != textures.end()) return found->second;
        auto texture = std::make_shared<Texture>(context, image, target.format, VK_IMAGE_ASPECT_COLOR_BIT, components);
        textures.emplace(key, texture);
        return texture;
    }

    VkRenderPass ResolvePass() {
        if (resolvePass != VK_NULL_HANDLE) return resolvePass;
        std::array<VkAttachmentDescription, 2> attachments{};
        attachments[0].format = target.format;
        attachments[0].samples = static_cast<VkSampleCountFlagBits>(target.samples);
        attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        attachments[1].format = target.format;
        attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        for (auto& attachment : attachments) {
            attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
            attachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
        }
        const VkAttachmentReference color{0, VK_IMAGE_LAYOUT_GENERAL};
        const VkAttachmentReference resolve{1, VK_IMAGE_LAYOUT_GENERAL};
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color;
        subpass.pResolveAttachments = &resolve;
        VkRenderPassCreateInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
        info.attachmentCount = static_cast<std::uint32_t>(attachments.size());
        info.pAttachments = attachments.data();
        info.subpassCount = 1;
        info.pSubpasses = &subpass;
        Check(context.Function<PFN_vkCreateRenderPass>("vkCreateRenderPass")(context.device, &info, nullptr, &resolvePass), "vkCreateRenderPass resolve");
        return resolvePass;
    }

    Context context;
    std::shared_ptr<std::atomic<bool>> active = std::make_shared<std::atomic<bool>>(true);
    std::shared_ptr<std::atomic<bool>> metadataValid;
    ColorTarget target;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkRenderPass resolvePass = VK_NULL_HANDLE;
    MultisampledCmask cmask;
    MultisampledColorFill colorFill;
    bool fromTexture = false;

private:
    bool sampleable = false;
    VkImageUsageFlags usage = 0;
    VkFormat storageFormat = VK_FORMAT_UNDEFINED;
    VkImageCreateFlags storageFlags = 0;
    std::vector<Allocation> retired;
    VkImageView storageView = VK_NULL_HANDLE;
    bool storageWritten = false;
    std::map<std::array<std::uint32_t, 4>, std::shared_ptr<Texture>> textures;
    std::vector<GuestMemory::UnchangedQuery> dccRanges;

    VkClearColorValue initialDccClear(const ColorTarget& use) {
        if (use.dccAddress == 0) return {};
        const auto dccBytes = MultisampledDccBytes(use);
        Require((use.dccAddress & 4095u) == 0, "multisampled DCC metadata base alignment is not implemented");
        Require(use.cmaskAddress == 0 ? use.cmaskBytes == 0 : use.cmaskBytes == CmaskBytes(use.extent.width, use.extent.height), "multisampled DCC CMASK footprint is invalid");
        const auto colorBytes = MultisampledColorBytes(use);
        Require(colorBytes != 0, "multisampled DCC color footprint is not implemented");
        dccRanges.push_back({use.address, static_cast<std::size_t>(colorBytes), 0});
        dccRanges.push_back({use.dccAddress, dccBytes, 0});
        if (use.cmaskAddress != 0) dccRanges.push_back({use.cmaskAddress, use.cmaskBytes, 0});
        for (auto& range : dccRanges) {
            GuestMemory::CheckRange(reinterpret_cast<const void*>(range.address), range.bytes, 256);
            Require(GuestMemory::Watched(range.address, range.bytes), "initial multisampled DCC contents require watched guest memory");
            range.generation = GuestMemory::CollectWritesUncached(range.address, range.bytes);
            Require(range.generation != 0, "initial multisampled DCC contents have no write generation");
        }
        for (std::size_t first = 0; first < dccRanges.size(); ++first) {
            for (std::size_t second = first + 1; second < dccRanges.size(); ++second) {
                const auto& a = dccRanges[first];
                const auto& b = dccRanges[second];
                Require(a.address >= b.address + b.bytes || b.address >= a.address + a.bytes, "overlapping multisampled DCC color or metadata ranges are not implemented");
            }
        }
        std::vector<std::byte> dcc(dccRanges[1].bytes);
        GuestMemory::Read(use.dccAddress, dcc);
        std::vector<std::byte> cmask(use.cmaskBytes);
        if (!cmask.empty()) GuestMemory::Read(use.cmaskAddress, cmask);
        const auto clear = MultisampledDccClear(use, dcc, cmask);
        Require(GuestMemory::UnchangedSinceAll(dccRanges), "initial multisampled DCC contents changed during import");
        return clear;
    }

    void checkDccCurrent(const ColorTarget& use) const {
        if (dccRanges.empty()) {
            Require(use.dccAddress == 0, "adding DCC to a resident multisampled color target is not implemented");
            return;
        }
        Require(metadataValid->load(std::memory_order_acquire), "multisampled DCC metadata was released while its color target remains resident");
        static_cast<void>(MultisampledDccBytes(use));
        Require(use.address == target.address && use.dccAddress == target.dccAddress && use.dccControl == target.dccControl && use.dccAlphaOnMsb == target.dccAlphaOnMsb && use.dccPipeAligned == target.dccPipeAligned && use.cmaskAddress == target.cmaskAddress && use.cmaskBytes == target.cmaskBytes && use.tileMode == target.tileMode && use.elementBytes == target.elementBytes && use.pipeBankXor == target.pipeBankXor, "multisampled DCC target layout or metadata changed");
        for (const auto& range : dccRanges) {
            Require(GuestMemory::Watched(range.address, range.bytes), "multisampled DCC target reuse requires watched guest memory");
            GuestMemory::CollectWritesUncached(range.address, range.bytes);
            Require(GuestMemory::UnchangedSince(range.address, range.bytes, range.generation), "writes to a resident multisampled DCC target or its metadata are not implemented");
            if (auto* recorder = Recorder::Active(); recorder != nullptr) Require(!recorder->PendingWriteOverlaps(range.address, range.bytes), "pending writes to a resident multisampled DCC target or its metadata are not implemented");
        }
    }

    void release() noexcept {
        textures.clear();
        if (resolvePass) context.Function<PFN_vkDestroyRenderPass>("vkDestroyRenderPass")(context.device, resolvePass, nullptr);
        if (storageView) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, storageView, nullptr);
        destroy({image, memory, view});
        for (const auto& allocation : retired) destroy(allocation);
    }
};

struct GuardedRange {
    VkDevice device;
    std::uint64_t address;
    std::uint64_t bytes;
    std::uint32_t samples;
    std::shared_ptr<std::atomic<bool>> active;
    std::shared_ptr<std::atomic<bool>> metadataValid;
};

class PrimedSharedMutex : public std::shared_mutex {
public:
    PrimedSharedMutex() {
        lock();
        unlock();
    }
};

std::shared_mutex& guardedMutex() {
    static PrimedSharedMutex mutex;
    return mutex;
}

std::vector<GuardedRange>& guarded() {
    static std::vector<GuardedRange> ranges;
    return ranges;
}

std::atomic<bool>& anyGuarded() {
    static std::atomic<bool> any{false};
    return any;
}

void guardReads(std::uint64_t address, std::size_t bytes) {
    RequireNoMultisampleTarget(address, bytes, "a CPU read");
}

void retireUnmapped(std::uintptr_t address, std::size_t bytes) {
    RetireUnmappedDepth_nid_no_patch(address, bytes);
    std::unique_lock lock(guardedMutex());
    std::erase_if(guarded(), [&](const auto& range) {
        if (range.metadataValid && address < range.address + range.bytes && range.address < address + bytes) range.metadataValid->store(false, std::memory_order_release);
        if (range.address < address || range.address - address > bytes || range.bytes > bytes - (range.address - address)) return false;
        if (!range.metadataValid) range.active->store(false, std::memory_order_release);
        return true;
    });
    std::erase_if(guarded(), [](const auto& range) { return range.metadataValid && !range.active->load(std::memory_order_acquire); });
    anyGuarded().store(!guarded().empty(), std::memory_order_release);
}

void guard(const Context& context, const ColorTarget& target, const std::shared_ptr<std::atomic<bool>>& active, const std::shared_ptr<std::atomic<bool>>& metadataValid) {
    auto bytes = std::max<std::uint64_t>(target.bytes * target.samples, static_cast<std::uint64_t>(target.extent.width) * target.extent.height * target.elementBytes * target.samples);
    if (target.tileMode == ColorTileMode::RenderTarget && std::has_single_bit(target.elementBytes) && std::has_single_bit(target.samples)) {
        const auto log2Samples = std::countr_zero(target.samples);
        const auto log2Pixels = 16u - std::countr_zero(target.elementBytes) - log2Samples;
        const auto log2Width = (log2Pixels + ((log2Samples & 1u) == 0u ? 1u : 0u)) / 2u;
        const auto blockWidth = 1u << log2Width;
        const auto blockHeight = 1u << (log2Pixels - log2Width);
        const auto align = [](std::uint32_t extent, std::uint32_t block) { return (static_cast<std::uint64_t>(extent) + block - 1u) / block * block; };
        bytes = align(target.extent.width, blockWidth) * align(target.extent.height, blockHeight) * target.elementBytes * target.samples;
    }
    std::unique_lock lock(guardedMutex());
    guarded().push_back({context.device, target.address, bytes, target.samples, active});
    if (target.dccAddress != 0) {
        guarded().push_back({context.device, target.dccAddress, MultisampledDccBytes(target), target.samples, active, metadataValid});
        if (target.cmaskAddress != 0) guarded().push_back({context.device, target.cmaskAddress, target.cmaskBytes, target.samples, active, metadataValid});
    }
    anyGuarded().store(true, std::memory_order_release);
    InstallResidentImageReadGuard_nid_no_patch();
}

std::mutex& targetsMutex() {
    static std::mutex mutex;
    return mutex;
}

std::vector<std::unique_ptr<MultisampleTarget>>& targets() {
    static std::vector<std::unique_ptr<MultisampleTarget>> list;
    return list;
}

MultisampleTarget& acquire(const Context& context, const ColorTarget& use) {
    for (const auto& target : targets()) {
        if (!target->active->load(std::memory_order_acquire)) continue;
        if (target->context.device != context.device || !sameTarget(target->target, use)) continue;
        if (target->fromTexture && use.cmaskAddress != 0) {
            target->fromTexture = false;
            target->cmask = MultisampledCmask(use.cmaskAddress, use.cmaskBytes);
        }
        target->cmask.CheckAddress(use.cmaskAddress);
        target->SettleStorageWrites();
        target->ApplyFastClear(use);
        return *target;
    }
    targets().push_back(std::make_unique<MultisampleTarget>(context, use));
    auto& created = *targets().back();
    guard(context, use, created.active, created.metadataValid);
    if (use.cmaskFastClear && use.dccAddress == 0) created.cmask.SeedCode(CurrentUniformMetadataByte(use.cmaskAddress, static_cast<std::size_t>(use.cmaskBytes)));
    created.ApplyFastClear(use);
    return created;
}

MultisampleTarget& createFilled(const Context& context, const GuestTextureResource& resource, VkFormat format, const char* access) {
    const auto refuse = [&](const std::string& reason) {
        char text[192];
        std::snprintf(text, sizeof(text), "AGC graphics: %s the %ux%u multisampled texture 0x%llx of %u samples, which no draw rendered to and ", access, resource.width, resource.height, static_cast<unsigned long long>(resource.baseAddress), resource.samples);
        throw std::runtime_error(text + reason + ", is not implemented");
    };
    ColorTarget use{};
    switch (resource.tileMode) {
        case TextureTileMode::kStandard4KB: use.tileMode = ColorTileMode::Standard4KB; break;
        case TextureTileMode::kStandard64KB: use.tileMode = ColorTileMode::Standard64KB; break;
        case TextureTileMode::kR64KBX: use.tileMode = ColorTileMode::RenderTarget; break;
        default: refuse("whose tile mode " + std::to_string(static_cast<int>(resource.tileMode)) + " has no modeled multisampled color footprint");
    }
    use.address = resource.baseAddress;
    use.surfaceAddress = resource.baseAddress;
    use.extent = {resource.width, resource.height};
    use.surfaceExtent = use.extent;
    use.format = format;
    use.elementBytes = BytesPerElement(resource.format);
    use.pipeBankXor = resource.pipeBankXor;
    use.samples = resource.samples;
    const auto bytes = MultisampledColorBytes(use);
    if (bytes == 0) refuse("whose " + std::to_string(use.elementBytes) + "-byte elements have no modeled multisampled color footprint");
    use.bytes = ColorTargetLayout(use.extent.width, use.extent.height, use.tileMode, use.elementBytes).Bytes();
    char range[64];
    std::snprintf(range, sizeof(range), "0x%llx+0x%llx", static_cast<unsigned long long>(use.address), static_cast<unsigned long long>(bytes));
    RequireNoMultisampleTarget(use.address, static_cast<std::size_t>(bytes), "sampling a multisampled texture no draw rendered to");
    GuestMemory::FlushGpuWrites(use.address, static_cast<std::size_t>(bytes));
    if (!GuestMemory::Accessible(reinterpret_cast<const void*>(use.address), static_cast<std::size_t>(bytes))) refuse(std::string("whose color memory ") + range + " is not mapped");
    const auto* memory = reinterpret_cast<const std::byte*>(use.address);
    std::uint32_t value = 0;
    std::memcpy(&value, memory, sizeof(value));
    char held[16];
    std::snprintf(held, sizeof(held), "0x%08x", value);
    const auto repeated = std::mismatch(memory + sizeof(value), memory + bytes, memory).first;
    if (repeated != memory + bytes) {
        char offset[32];
        std::snprintf(offset, sizeof(offset), "+0x%llx", static_cast<unsigned long long>(repeated - memory));
        refuse(std::string("whose color memory ") + range + " does not hold one repeated 32-bit value (" + held + " until " + offset + ")");
    }
    VkClearColorValue clear{};
    if (!MultisampledFillClearColor(use, value, clear)) refuse(std::string("whose color memory ") + range + " holds " + held + ", which has no exact clear value in its format");
    targets().push_back(std::make_unique<MultisampleTarget>(context, use));
    auto& created = *targets().back();
    guard(context, use, created.active, created.metadataValid);
    created.colorFill.NoteFill(use.address, static_cast<std::size_t>(bytes), value);
    created.fromTexture = true;
    return created;
}

}

void InstallResidentImageReadGuard_nid_no_patch() {
    static std::once_flag installed;
    std::call_once(installed, [] { GuestMemory::SetReadGuard(&guardReads); });
    UpdateResidentImageUnmapObserver_nid_no_patch();
}

void UpdateResidentImageUnmapObserver_nid_no_patch() {
    static std::mutex mutex;
    std::lock_guard lock(mutex);
    GuestAllocations::GuestAllocationsSetUnmapObserver_nid_postfix(anyGuarded().load(std::memory_order_acquire) || HasResidentDepthGuards_nid_no_patch() ? &retireUnmapped : nullptr);
}

std::optional<DccKeys> MultisampledCmaskKeys(std::uint8_t code) {
    const auto tile = code & 0xfu;
    if ((code >> 4u) != tile) return std::nullopt;
    if (tile == 0u) return DccKeys::Clear0000;
    if ((tile >> 2u) == 3u) return DccKeys::Uncompressed;
    return std::nullopt;
}

void MultisampledCmask::NoteFill(std::uint64_t fillAddress, std::size_t fillBytes, std::uint32_t pattern) {
    if (address == 0 || fillAddress >= address + bytes || address >= fillAddress + fillBytes) return;
    if (fillAddress > address || fillAddress + fillBytes < address + bytes) {
        char text[192];
        std::snprintf(text, sizeof(text), "a fill of 0x%llx+0x%zx partly covers the CMASK 0x%llx+0x%llx of a multisampled color target, leaving its tiles mixed", static_cast<unsigned long long>(fillAddress), fillBytes, static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes));
        refusal = text;
        return;
    }
    refusal.clear();
    const auto code = static_cast<std::uint8_t>(pattern);
    if (const auto filled = MultisampledCmaskKeys(code); filled && pattern == code * 0x01010101u) {
        keys = *filled;
    } else {
        char text[224];
        std::snprintf(text, sizeof(text), "a fill of the CMASK 0x%llx+0x%llx of a multisampled color target with 0x%08x leaves its tiles mixed (neither a register color clear 0 nor a color-expanded code such as 0xcc or 0xff)", static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes), pattern);
        refusal = text;
    }
}

void MultisampledCmask::SeedCode(std::optional<std::uint8_t> code) {
    if (const auto seeded = code ? MultisampledCmaskKeys(*code) : std::nullopt) {
        keys = *seeded;
        return;
    }
    char text[192];
    if (code) std::snprintf(text, sizeof(text), "the CMASK 0x%llx of a multisampled color target reads as code 0x%02x, which is not modeled", static_cast<unsigned long long>(address), *code);
    else std::snprintf(text, sizeof(text), "the CMASK 0x%llx of a multisampled color target reads as mixed, which is not modeled", static_cast<unsigned long long>(address));
    refusal = text;
}

void MultisampledCmask::CheckAddress(std::uint64_t cmaskAddress) const {
    if (cmaskAddress == address) return;
    char text[192];
    std::snprintf(text, sizeof(text), "a multisampled color target used CMASK 0x%llx after its first use with CMASK 0x%llx, which is not modeled", static_cast<unsigned long long>(cmaskAddress), static_cast<unsigned long long>(address));
    Require(false, std::string(text));
}

bool MultisampledCmask::TakeClear() {
    Require(refusal.empty(), refusal);
    if (keys != DccKeys::Clear0000) return false;
    keys = DccKeys::Uncompressed;
    return true;
}

std::uint64_t MultisampledColorBytes(const ColorTarget& target) {
    const auto blockLog2 = target.tileMode == ColorTileMode::Standard4KB ? 12u : target.tileMode == ColorTileMode::Standard64KB || target.tileMode == ColorTileMode::RenderTarget ? 16u : 0u;
    if (blockLog2 == 0 || !std::has_single_bit(target.elementBytes) || !std::has_single_bit(target.samples)) return 0;
    const auto samplesLog2 = static_cast<std::uint32_t>(std::countr_zero(target.samples));
    const auto elementsLog2 = blockLog2 - static_cast<std::uint32_t>(std::countr_zero(target.elementBytes)) - samplesLog2;
    const auto widthLog2 = (elementsLog2 + ((samplesLog2 & 1u) == 0 ? 1u : 0u)) / 2u;
    const std::uint64_t blockWidth = std::uint64_t{1} << widthLog2;
    const std::uint64_t blockHeight = std::uint64_t{1} << (elementsLog2 - widthLog2);
    return ((target.extent.width + blockWidth - 1) / blockWidth) * ((target.extent.height + blockHeight - 1) / blockHeight) << blockLog2;
}

std::size_t MultisampledDccBytes(const ColorTarget& target) {
    Require(target.samples == 2 || target.samples == 4 || target.samples == 8, "unsupported multisampled DCC fragment count");
    Require(target.tileMode == ColorTileMode::RenderTarget && target.dccPipeAligned, "multisampled DCC requires pipe-aligned SW_64KB_R_X");
    Require(std::has_single_bit(target.elementBytes) && target.elementBytes <= 16, "unsupported multisampled DCC element size");
    Require(target.extent.width != 0 && target.extent.width <= 16384 && target.extent.height != 0 && target.extent.height <= 16384, "invalid multisampled DCC extent");
    Require(target.mipCount == 1 && target.mip == 0 && !target.mipTail && target.depth == 1 && target.depthSlice == 0 && target.arrayLastSlice == 0 && target.arraySlice == 0 && target.layers == 1, "multisampled DCC array, volume or mip contents are not implemented");
    const auto bits = 19u - static_cast<std::uint32_t>(std::countr_zero(target.elementBytes));
    const std::uint64_t width = std::uint64_t{1} << ((bits + 1u) / 2u);
    const std::uint64_t height = std::uint64_t{1} << (bits / 2u);
    return static_cast<std::size_t>(((target.extent.width + width - 1) / width) * ((target.extent.height + height - 1) / height) * 4096u);
}

VkClearColorValue MultisampledDccClear(const ColorTarget& target, std::span<const std::byte> dcc, std::span<const std::byte> cmask) {
    Require(target.dccAddress != 0 && (target.dccControl & 0x3ff80u) == 0 && (target.pipeBankXor & ~0xcf00u) == 0, "multisampled DCC address, color transform, lossy mode or XOR is not implemented");
    Require(dcc.size() == MultisampledDccBytes(target), "multisampled DCC metadata snapshot size mismatch");
    Require(std::ranges::all_of(dcc, [&](std::byte key) { return key == dcc.front(); }), "mixed multisampled DCC metadata is not implemented");
    Require(target.cmaskAddress == 0 ? cmask.empty() && target.cmaskBytes == 0 : cmask.size() == target.cmaskBytes && target.cmaskBytes == CmaskBytes(target.extent.width, target.extent.height), "multisampled DCC CMASK snapshot size mismatch");
    if (!cmask.empty()) {
        const auto code = std::to_integer<std::uint32_t>(cmask.front());
        const auto expanded = target.samples == 2 ? 0xddu : target.samples == 4 ? 0xeeu : 0xffu;
        Require(std::ranges::all_of(cmask, [&](std::byte key) { return key == cmask.front(); }) && (code == 0xccu || code == expanded), "multisampled DCC CMASK compression or register clear is not implemented");
    }
    DccKeys keys;
    switch (std::to_integer<std::uint32_t>(dcc.front())) {
        case 0x00u: keys = DccKeys::Clear0000; break;
        case 0x40u: keys = DccKeys::Clear0001; break;
        case 0x80u: keys = DccKeys::Clear1110; break;
        case 0xc0u: keys = DccKeys::Clear1111; break;
        default: throw std::runtime_error("initial multisampled DCC contents are not a supported uniform fast clear");
    }
    std::array<std::byte, 16> texels{};
    Require(FillDccClear(target.format, keys, target.dccAlphaOnMsb, texels), "multisampled DCC clear format is not implemented");
    std::array<std::uint32_t, 4> pattern{};
    std::memcpy(pattern.data(), texels.data(), texels.size());
    VkClearColorValue clear{};
    Require(ClearColorForTexel(target.format, target.elementBytes, pattern, clear), "multisampled DCC fast clear has no exact host value");
    return clear;
}

bool MultisampledFillClearColor(const ColorTarget& target, std::uint32_t fill, VkClearColorValue& clear) {
    const std::array<std::uint32_t, 4> pattern{fill, fill, fill, fill};
    VkFormat unorm = VK_FORMAT_UNDEFINED;
    switch (target.format) {
        case VK_FORMAT_R8G8B8A8_SRGB: unorm = VK_FORMAT_R8G8B8A8_UNORM; break;
        case VK_FORMAT_B8G8R8A8_SRGB: unorm = VK_FORMAT_B8G8R8A8_UNORM; break;
        case VK_FORMAT_A8B8G8R8_SRGB_PACK32: unorm = VK_FORMAT_A8B8G8R8_UNORM_PACK32; break;
        default: return ClearColorForTexel(target.format, target.elementBytes, pattern, clear);
    }
    if (!ClearColorForTexel(unorm, target.elementBytes, pattern, clear)) return false;
    return std::all_of(clear.float32, clear.float32 + 3, [](float value) { return value == 0.0f || value == 1.0f; });
}

MultisampledColorFill::MultisampledColorFill(const ColorTarget& target) : address(target.address), bytes(MultisampledColorBytes(target)) {}

void MultisampledColorFill::NoteFill(std::uint64_t fillAddress, std::size_t fillBytes, std::uint32_t pattern) {
    if (bytes == 0 ? fillAddress > address || address >= fillAddress + fillBytes : fillAddress >= address + bytes || address >= fillAddress + fillBytes) return;
    char text[192];
    if (bytes == 0) {
        std::snprintf(text, sizeof(text), "a fill of 0x%llx+0x%zx covers the color memory at 0x%llx of a multisampled target whose footprint is not modeled", static_cast<unsigned long long>(fillAddress), fillBytes, static_cast<unsigned long long>(address));
        refusal = text;
        return;
    }
    if (fillAddress > address || fillAddress + fillBytes < address + bytes) {
        std::snprintf(text, sizeof(text), "a fill of 0x%llx+0x%zx partly covers the color memory 0x%llx+0x%llx of a multisampled target, leaving its samples mixed", static_cast<unsigned long long>(fillAddress), fillBytes, static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes));
        refusal = text;
        return;
    }
    refusal.clear();
    filled = pattern;
}

std::optional<std::uint32_t> MultisampledColorFill::TakeFill() {
    Require(refusal.empty(), refusal);
    return std::exchange(filled, std::nullopt);
}

void MultisampledColorFill::Discard() {
    refusal.clear();
    filled.reset();
}

bool TakeMultisampledClear(MultisampledCmask& cmask, MultisampledColorFill& colorFill, const ColorTarget& use, VkClearColorValue& clear) {
    if (cmask.TakeClear()) {
        colorFill.Discard();
        clear = fastClearColor(use);
        return true;
    }
    const auto fill = colorFill.TakeFill();
    if (!fill) return false;
    Require(MultisampledFillClearColor(use, *fill, clear), "a fill over the color memory of a multisampled target has no exact clear value in its format");
    return true;
}

VkImageView MultisampleTargetView(const Context& context, const ColorTarget& target) {
    std::lock_guard lock(targetsMutex());
    return acquire(context, target).view;
}

std::shared_ptr<Texture> MultisampleTargetTexture(const Context& context, const GuestTextureResource& resource, VkComponentMapping components) {
    std::lock_guard lock(targetsMutex());
    const auto format = ResolveTextureFormat(resource.format);
    const MultisampleTarget* other = nullptr;
    for (const auto& target : targets()) {
        if (!target->active->load(std::memory_order_acquire)) continue;
        if (target->context.device != context.device || target->target.surfaceAddress != resource.baseAddress) continue;
        const auto& rendered = target->target;
        if (rendered.format == format && rendered.extent.width == resource.width && rendered.extent.height == resource.height && rendered.samples == resource.samples && rendered.mip == 0) return target->Sampled(components);
        other = target.get();
    }
    if (other == nullptr) {
        Require(Recorder::Active() != nullptr, "a multisampled texture is only sampled by recorded draws and dispatches");
        return createFilled(context, resource, format, "sampling").Sampled(components);
    }
    char text[320];
    std::snprintf(text, sizeof(text), "AGC graphics: sampling multisampled color target 0x%llx (%ux%u, %u samples, vk format %d) as a %ux%u texture of %u samples and guest format %u (vk %d) is not implemented", static_cast<unsigned long long>(resource.baseAddress), other->target.extent.width, other->target.extent.height, other->target.samples, static_cast<int>(other->target.format), resource.width, resource.height, resource.samples, resource.format, static_cast<int>(format));
    throw std::runtime_error(text);
}

void MaterializeMultisampleTargetClear(const Context& context, const ColorTarget& target) {
    std::lock_guard lock(targetsMutex());
    acquire(context, target);
}

VkImageView MultisampleTargetStorageView(const Context& context, const GuestTextureResource& resource) {
    std::lock_guard lock(targetsMutex());
    const auto format = ResolveTextureFormat(resource.format);
    const MultisampleTarget* other = nullptr;
    for (const auto& target : targets()) {
        if (!target->active->load(std::memory_order_acquire)) continue;
        if (target->context.device != context.device || target->target.surfaceAddress != resource.baseAddress) continue;
        const auto& rendered = target->target;
        if (rendered.format == format && rendered.extent.width == resource.width && rendered.extent.height == resource.height && rendered.samples == resource.samples && rendered.mip == 0) return target->Storage(StorageFormatForGuest(context, resource.format));
        other = target.get();
    }
    if (other == nullptr) {
        Require(Recorder::Active() != nullptr, "a multisampled color target is only written by recorded dispatches");
        return createFilled(context, resource, format, "writing").Storage(StorageFormatForGuest(context, resource.format));
    }
    char text[320];
    std::snprintf(text, sizeof(text), "AGC graphics: storage image access to multisampled color target 0x%llx (%ux%u, %u samples, vk format %d) as a %ux%u texture of %u samples and guest format %u (vk %d) is not implemented", static_cast<unsigned long long>(resource.baseAddress), other->target.extent.width, other->target.extent.height, other->target.samples, static_cast<int>(other->target.format), resource.width, resource.height, resource.samples, resource.format, static_cast<int>(format));
    throw std::runtime_error(text);
}

bool MultisampleTargetStorable(const Context& context, const ColorTarget& target) {
    std::lock_guard lock(targetsMutex());
    return std::any_of(targets().begin(), targets().end(), [&](const auto& candidate) {
        return candidate->active->load(std::memory_order_acquire) && candidate->context.device == context.device && sameTarget(candidate->target, target) && candidate->Storable();
    });
}

void ResolveMultisampleTarget(const Context& context, const ColorTarget& source, StorageTexture& destination, VkImageView destinationView) {
    std::lock_guard lock(targetsMutex());
    auto& target = acquire(context, source);
    const std::array<VkImageView, 2> views{target.view, destinationView};
    auto framebuffer = std::make_shared<Framebuffer>(context, target.ResolvePass(), views, source.extent);
    recordCommands(context, [&](VkCommandBuffer commands, Recorder* recorder) {
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
        VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        begin.renderPass = target.ResolvePass();
        begin.framebuffer = framebuffer->Handle();
        begin.renderArea = {{0, 0}, source.extent};
        context.Resolved(&DeviceFunctions::cmdBeginRenderPass, "vkCmdBeginRenderPass")(commands, &begin, VK_SUBPASS_CONTENTS_INLINE);
        context.Resolved(&DeviceFunctions::cmdEndRenderPass, "vkCmdEndRenderPass")(commands);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT);
        if (recorder != nullptr) {
            Recorder::CountBarriers(Recorder::CommandClass::Draw, 2);
            recorder->Keep(framebuffer);
        }
    });
    destination.MarkDirty();
}

bool MultisampledColorFillAbsorbs(std::uint64_t address, std::size_t bytes, std::uint32_t pattern) {
    if (bytes == 0) return false;
    std::lock_guard lock(targetsMutex());
    return std::any_of(targets().begin(), targets().end(), [&](const auto& target) {
        VkClearColorValue clear{};
        return target->active->load(std::memory_order_acquire) && target->target.address == address && MultisampledColorBytes(target->target) == bytes && MultisampledFillClearColor(target->target, pattern, clear);
    });
}

void NoteColorMetadataFill(std::uint64_t address, std::size_t bytes, std::uint32_t pattern) {
    std::lock_guard lock(targetsMutex());
    for (const auto& target : targets()) {
        if (!target->active->load(std::memory_order_acquire)) continue;
        target->colorFill.NoteFill(address, bytes, pattern);
        const auto before = target->cmask.Keys();
        target->cmask.NoteFill(address, bytes, pattern);
        if (before == DccKeys::Clear0000 || target->cmask.Keys() != DccKeys::Clear0000) continue;
        static std::atomic<int> reports{0};
        if (reports.fetch_add(1) < 8) std::fprintf(stderr, "[gpu] multisampled color target 0x%llx fast-cleared through its CMASK 0x%llx (fill 0x%llx+0x%zx)\n", static_cast<unsigned long long>(target->target.surfaceAddress), static_cast<unsigned long long>(target->target.cmaskAddress), static_cast<unsigned long long>(address), bytes);
    }
}

void ClearMultisampleTargets(VkDevice device) {
    std::lock_guard lock(targetsMutex());
    std::erase_if(targets(), [&](const auto& target) { return target->context.device == device; });
    std::unique_lock guardLock(guardedMutex());
    std::erase_if(guarded(), [&](const auto& range) { return range.device == device; });
    anyGuarded().store(!guarded().empty(), std::memory_order_release);
    UpdateResidentImageUnmapObserver_nid_no_patch();
}

void RequireNoMultisampleTarget(std::uint64_t address, std::size_t bytes, const char* access) {
    RequireNoResidentDepthRead_nid_no_patch(address, bytes);
    if (bytes == 0 || !anyGuarded().load(std::memory_order_acquire)) return;
    std::shared_lock lock(guardedMutex());
    for (const auto& range : guarded()) {
        if (address >= range.address + range.bytes || range.address >= address + bytes) continue;
        char text[256];
        std::snprintf(text, sizeof(text), "AGC graphics: %s of 0x%llx+0x%zx overlaps multisampled color target 0x%llx+0x%llx (%u samples), whose samples are only on the host: guest access to it is not implemented", access, static_cast<unsigned long long>(address), bytes, static_cast<unsigned long long>(range.address), static_cast<unsigned long long>(range.bytes), range.samples);
        throw std::runtime_error(text);
    }
}

}
