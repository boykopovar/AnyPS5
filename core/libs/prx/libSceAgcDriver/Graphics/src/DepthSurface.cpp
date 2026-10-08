#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "RdnaDecoder/include/RdnaDecoder/RdnaDescriptorFormat.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace AgcDriver::Graphics {
namespace {

struct GuestDepthSlice {
    std::vector<std::byte> bytes;
    std::uint64_t generation = 0;
};

class DepthSurface {
public:
    DepthSurface(const Context& context, const DepthTarget& target, std::uint32_t layers = 1) : context(context), target(target) {
        this->context.bufferPool.reset();
        VkFormatProperties properties{};
        context.formatProperties(context.physical, target.format, &properties);
        Require((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0, "depth/stencil format " + std::to_string(target.format) + " cannot be an attachment on this device");
        Require((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0, "depth/stencil format " + std::to_string(target.format) + " cannot be sampled on this device");
        Require(target.extent.width <= context.limits.maxFramebufferWidth && target.extent.height <= context.limits.maxFramebufferHeight, "depth target exceeds framebuffer limits");
        const VkImageAspectFlags aspects = VK_IMAGE_ASPECT_DEPTH_BIT | (target.stencilAddress != 0 ? VK_IMAGE_ASPECT_STENCIL_BIT : 0u);
        try {
            VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            info.imageType = VK_IMAGE_TYPE_2D;
            info.format = target.format;
            info.extent = {target.extent.width, target.extent.height, 1};
            info.mipLevels = 1;
            Require(layers != 0 && layers <= context.limits.maxImageArrayLayers, "depth array exceeds device limits");
            info.arrayLayers = layers;
            info.samples = VK_SAMPLE_COUNT_1_BIT;
            info.tiling = VK_IMAGE_TILING_OPTIMAL;
            info.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &info, nullptr, &image), "vkCreateImage depth");
            VkMemoryRequirements requirements{};
            context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, image, &requirements);
            VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            allocation.allocationSize = requirements.size;
            allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &memory), "vkAllocateMemory depth target");
            Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, memory, 0), "vkBindImageMemory depth");
            VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            viewInfo.image = image;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = target.format;
            viewInfo.subresourceRange = {aspects, 0, 1, 0, 1};
            Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &view), "vkCreateImageView depth");
            auto* recorder = Recorder::Active();
            std::unique_ptr<CommandBatch> batch;
            if (recorder == nullptr) batch = std::make_unique<CommandBatch>(context);
            const auto commands = recorder != nullptr ? recorder->Commands() : batch->Handle();
            VkImageMemoryBarrier toGeneral{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            toGeneral.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            toGeneral.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            toGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            toGeneral.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toGeneral.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            toGeneral.image = image;
            toGeneral.subresourceRange = {aspects, 0, 1, 0, layers};
            const auto barrier = context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier");
            barrier(commands, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toGeneral);
            const VkClearDepthStencilValue clear{target.clearDepth, target.clearStencil};
            context.Function<PFN_vkCmdClearDepthStencilImage>("vkCmdClearDepthStencilImage")(commands, image, VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &toGeneral.subresourceRange);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
            if (batch) batch->SubmitAndWait();
            else Recorder::CountBarriers(Recorder::CommandClass::Draw, 2);
        } catch (...) {
            release();
            throw;
        }
    }
    ~DepthSurface() { release(); }
    DepthSurface(const DepthSurface&) = delete;
    DepthSurface& operator=(const DepthSurface&) = delete;

    void Clear(const DepthClearPass& pass) {
        auto* recorder = Recorder::Active();
        std::unique_ptr<CommandBatch> batch;
        if (recorder == nullptr) batch = std::make_unique<CommandBatch>(context);
        const auto commands = recorder != nullptr ? recorder->Commands() : batch->Handle();
        constexpr VkAccessFlags access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, access, VK_ACCESS_TRANSFER_WRITE_BIT);
        const VkClearDepthStencilValue clear{pass.target.clearDepth, pass.target.clearStencil};
        const VkImageSubresourceRange range{pass.aspects, 0, 1, 0, 1};
        context.Function<PFN_vkCmdClearDepthStencilImage>("vkCmdClearDepthStencilImage")(commands, image, VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &range);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, access);
        NoteWrite(pass.aspects);
        if (batch) batch->SubmitAndWait();
        else Recorder::CountBarriers(Recorder::CommandClass::Draw, 2);
    }

    void NoteWrite(VkImageAspectFlags aspects) {
        if (aspects & VK_IMAGE_ASPECT_DEPTH_BIT) ++depthVersion;
        if (aspects & VK_IMAGE_ASPECT_STENCIL_BIT) ++stencilVersion;
    }

    void CopyFrom(const DepthSurface& source, VkImageAspectFlags aspects) {
        auto* recorder = Recorder::Active();
        std::unique_ptr<CommandBatch> batch;
        if (recorder == nullptr) batch = std::make_unique<CommandBatch>(context);
        const auto commands = recorder != nullptr ? recorder->Commands() : batch->Handle();
        constexpr VkAccessFlags access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, access, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        std::array<VkImageCopy, 2> copies{};
        std::uint32_t count = 0;
        for (const auto aspect : {VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_ASPECT_STENCIL_BIT}) {
            if ((aspects & aspect) == 0) continue;
            auto& copy = copies[count++];
            copy.srcSubresource = {static_cast<VkImageAspectFlags>(aspect), 0, 0, 1};
            copy.dstSubresource = copy.srcSubresource;
            copy.extent = {target.extent.width, target.extent.height, 1};
        }
        context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, source.image, VK_IMAGE_LAYOUT_GENERAL, image, VK_IMAGE_LAYOUT_GENERAL, count, copies.data());
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, access);
        NoteWrite(aspects);
        if (batch) batch->SubmitAndWait();
        else Recorder::CountBarriers(Recorder::CommandClass::Draw, 2);
    }

    std::uint64_t Version(VkImageAspectFlags aspect) const {
        return aspect == VK_IMAGE_ASPECT_STENCIL_BIT ? stencilVersion : depthVersion;
    }

    void ValidateSampled(std::span<const std::uint32_t> words, const GuestTextureResource& resource) const {
        std::array<std::uint32_t, 8> key{};
        std::copy_n(words.begin(), std::min<std::size_t>(words.size(), key.size()), key.begin());
        const bool stencil = target.stencilAddress != 0 && resource.baseAddress == target.stencilAddress;
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        const auto expected = stencil ? VK_FORMAT_R8_UINT : d16 ? VK_FORMAT_R16_UNORM : VK_FORMAT_R32_SFLOAT;
        const auto format = ResolveTextureFormat(resource.format);
        const bool depthBits = !stencil && words.size() >= 4 && ShaderRecompiler::DepthBitsTextureWidth(words[1], words[3]) == (d16 ? 16u : 32u);
        if ((format != expected && !depthBits) || (resource.dimension != TextureDimension::k2D && resource.dimension != TextureDimension::k2DArray) || resource.width != target.extent.width || resource.height != target.extent.height || resource.baseLevel != 0 || resource.lastLevel != 0 || (resource.dimension == TextureDimension::k2D && resource.baseArray != 0)) {
            char text[448];
            std::snprintf(text, sizeof(text), "AGC graphics: sampling the %s plane of depth surface 0x%llx (%ux%u, vk format %d) as a %ux%u texture of guest format %u (vk %d), tile mode %u, dimension %d, levels %u-%u, slice %u is not implemented (T# %08x %08x %08x %08x %08x %08x %08x %08x)",
                          stencil ? "stencil" : "depth", static_cast<unsigned long long>(target.address), target.extent.width, target.extent.height, static_cast<int>(target.format), resource.width, resource.height, resource.format, static_cast<int>(format),
                          static_cast<unsigned>(resource.tileMode), static_cast<int>(resource.dimension), resource.baseLevel, resource.lastLevel, resource.baseArray, key[0], key[1], key[2], key[3], key[4], key[5], key[6], key[7]);
            throw std::runtime_error(text);
        }
    }

    std::shared_ptr<Texture> Sampled(const GuestTextureResource& resource, VkComponentMapping components, std::span<DepthSurface* const> slices, bool stencil) {
        const VkImageAspectFlags aspect = stencil ? VK_IMAGE_ASPECT_STENCIL_BIT : VK_IMAGE_ASPECT_DEPTH_BIT;
        const BackingKey backingKey{resource.baseAddress, resource.baseArray, slices.size(), static_cast<std::uint64_t>(resource.tileMode), aspect, resource.dccAddress, resource.dccAlphaOnMsb, resource.format};
        auto backing = arrays.find(backingKey);
        if (backing == arrays.end() && (slices.size() > 1 || slices.front() == nullptr)) {
            SampledDepth sampled;
            sampled.array = std::make_shared<DepthSurface>(context, target, static_cast<std::uint32_t>(slices.size()));
            sampled.versions.resize(slices.size());
            sampled.guest.resize(slices.size());
            backing = arrays.emplace(backingKey, std::move(sampled)).first;
        }
        const auto key = std::pair{backingKey, std::array<std::uint32_t, 5>{components.r, components.g, components.b, components.a, static_cast<std::uint32_t>(resource.dimension)}};
        auto found = textures.find(key);
        if (found == textures.end()) {
            const auto type = resource.dimension == TextureDimension::k2DArray ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
            std::shared_ptr<Texture> texture;
            if (backing == arrays.end()) {
                texture = std::make_shared<Texture>(context, slices.front()->image, target.format, aspect, components, type, 1);
            } else {
                auto owned = std::make_shared<SampledView>(backing->second.array, aspect, components, type, static_cast<std::uint32_t>(slices.size()));
                texture = std::shared_ptr<Texture>(owned, &owned->texture);
            }
            found = textures.emplace(key, std::move(texture)).first;
        }
        if (backing != arrays.end()) {
            auto& sampled = backing->second;
            sampled.array->CopySlices(resource, slices, aspect, sampled.versions, sampled.guest);
        }
        return found->second;
    }

    void CopySlices(const GuestTextureResource& resource, std::span<DepthSurface* const> slices, VkImageAspectFlags aspect, std::vector<std::pair<VkImage, std::uint64_t>>& versions, std::vector<GuestDepthSlice>& guest) {
        const bool d16 = target.format == VK_FORMAT_D16_UNORM || target.format == VK_FORMAT_D16_UNORM_S8_UINT;
        const auto bytes = aspect == VK_IMAGE_ASPECT_STENCIL_BIT ? 1u : d16 ? 2u : 4u;
        const auto stride = DepthSliceBytes(target.extent, bytes);
        std::vector<bool> changed(slices.size());
        std::vector<GuestDepthSlice> pending(slices.size());
        for (std::size_t i = 0; i < slices.size(); ++i) {
            if (slices[i] != nullptr) {
                changed[i] = versions[i] != std::pair{slices[i]->image, slices[i]->Version(aspect)};
                continue;
            }
            Require(resource.dccAddress == 0, "guest depth array layers with compression metadata are not implemented");
            const auto address = resource.baseAddress + (resource.baseArray + i) * stride;
            StorageTexture::FlushPending(address, stride, nullptr, "depth array layer");
            GuestMemory::FlushGpuWrites(address, stride);
            const auto generation = GuestMemory::CollectWrites(address, stride);
            if (!guest[i].bytes.empty() && (GuestMemory::UnchangedSince(address, stride, guest[i].generation) || GuestMemory::CompareMapped(address, guest[i].bytes) == GuestMemory::Compare::Equal)) {
                guest[i].generation = generation;
                continue;
            }
            pending[i].bytes.resize(stride);
            GuestMemory::Read(address, pending[i].bytes);
            pending[i].generation = generation;
            changed[i] = true;
        }
        if (std::none_of(changed.begin(), changed.end(), [](bool value) { return value; })) return;
        if (std::any_of(pending.begin(), pending.end(), [](const auto& slice) { return !slice.bytes.empty(); })) {
            Require(context.detiler != nullptr, "depth array upload requires a texture detiler");
            context.detiler->BeginBatch();
        }
        auto* recorder = Recorder::Active();
        std::unique_ptr<CommandBatch> batch;
        if (recorder == nullptr) batch = std::make_unique<CommandBatch>(context);
        const auto commands = recorder != nullptr ? recorder->Commands() : batch->Handle();
        constexpr VkAccessFlags access = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, access, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        std::vector<std::shared_ptr<Buffer>> staging;
        std::vector<std::shared_ptr<DeviceBuffer>> scratch;
        for (std::uint32_t i = 0; i < slices.size(); ++i) {
            if (!changed[i]) continue;
            if (slices[i] == nullptr) {
                Require(context.detiler != nullptr, "depth array upload requires a texture detiler");
                const auto mips = ComputeElementMipLayout(resource.tileMode, bytes, resource.width, resource.height, 1);
                Require(mips.size() == 1 && mips[0].tiledSize == stride && mips[0].tiledOffset == 0 && !mips[0].tail, "depth array upload layout disagrees with the attachment slice stride");
                const auto& mip = mips[0];
                auto upload = std::make_shared<Buffer>(context, stride, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
                auto tiled = std::make_shared<DeviceBuffer>(context, stride, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
                auto linear = std::make_shared<DeviceBuffer>(context, mip.linearSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
                std::memcpy(upload->Bytes().data(), pending[i].bytes.data(), pending[i].bytes.size());
                if (recorder != nullptr) {
                    recorder->Keep(upload);
                    recorder->Keep(tiled);
                    recorder->Keep(linear);
                }
                staging.push_back(upload);
                scratch.push_back(tiled);
                scratch.push_back(linear);
                RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
                CopyBuffer(context, commands, upload->Handle(), 0, tiled->Handle(), 0, stride);
                RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
                context.detiler->Dispatch(commands, resource.tileMode, bytes, tiled->Handle(), 0, linear->Handle(), 0, mip, false, resource.baseArray + i);
                RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
                VkBufferImageCopy copy{};
                copy.bufferRowLength = mip.pitchBytes / bytes;
                copy.imageSubresource = {aspect, 0, i, 1};
                copy.imageExtent = {resource.width, resource.height, 1};
                context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(commands, linear->Handle(), image, VK_IMAGE_LAYOUT_GENERAL, 1, &copy);
                if (recorder != nullptr) Recorder::CountBarriers(Recorder::CommandClass::Draw, 3);
            } else {
                VkImageCopy copy{};
                copy.srcSubresource = {aspect, 0, 0, 1};
                copy.dstSubresource = {aspect, 0, i, 1};
                copy.extent = {target.extent.width, target.extent.height, 1};
                context.Function<PFN_vkCmdCopyImage>("vkCmdCopyImage")(commands, slices[i]->image, VK_IMAGE_LAYOUT_GENERAL, image, VK_IMAGE_LAYOUT_GENERAL, 1, &copy);
            }
        }
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, access);
        if (batch) batch->SubmitAndWait();
        else Recorder::CountBarriers(Recorder::CommandClass::Draw, 2);
        for (std::size_t i = 0; i < slices.size(); ++i) {
            if (!changed[i]) continue;
            if (slices[i] != nullptr) {
                versions[i] = {slices[i]->image, slices[i]->Version(aspect)};
                guest[i] = {};
            } else {
                versions[i] = {};
                guest[i] = std::move(pending[i]);
            }
        }
    }

    const Context context;
    const DepthTarget target;
    VkImage image = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;

private:
    std::uint64_t depthVersion = 1;
    std::uint64_t stencilVersion = 1;
    using BackingKey = std::array<std::uint64_t, 8>;
    struct SampledDepth {
        std::shared_ptr<DepthSurface> array;
        std::vector<std::pair<VkImage, std::uint64_t>> versions;
        std::vector<GuestDepthSlice> guest;
    };
    struct SampledView {
        std::shared_ptr<DepthSurface> array;
        Texture texture;
        SampledView(std::shared_ptr<DepthSurface> source, VkImageAspectFlags aspect, VkComponentMapping components, VkImageViewType type, std::uint32_t layers)
            : array(std::move(source)), texture(array->context, array->image, array->target.format, aspect, components, type, layers) {}
    };
    std::map<BackingKey, SampledDepth> arrays;
    std::map<std::pair<BackingKey, std::array<std::uint32_t, 5>>, std::shared_ptr<Texture>> textures;

    void release() noexcept {
        textures.clear();
        arrays.clear();
        if (view) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, view, nullptr);
        if (image) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, image, nullptr);
        if (memory) context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, memory, nullptr);
        view = VK_NULL_HANDLE;
        image = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
    }
};

bool sameSurface(const DepthTarget& a, const DepthTarget& b) {
    return a.address == b.address && a.stencilAddress == b.stencilAddress && a.extent.width == b.extent.width && a.extent.height == b.extent.height && a.format == b.format;
}

std::mutex& surfacesMutex() {
    static std::mutex mutex;
    return mutex;
}

std::vector<std::unique_ptr<DepthSurface>>& surfaces() {
    static auto* list = new std::vector<std::unique_ptr<DepthSurface>>();
    return *list;
}

}

std::uint64_t DepthSliceBytes(VkExtent2D extent, std::uint32_t bytesPerTexel) {
    const std::uint32_t blockWidth = bytesPerTexel == 4 ? 128u : 256u;
    const std::uint32_t blockHeight = bytesPerTexel == 1 ? 256u : 128u;
    const auto width = static_cast<std::uint64_t>((extent.width + blockWidth - 1) / blockWidth * blockWidth);
    const auto height = static_cast<std::uint64_t>((extent.height + blockHeight - 1) / blockHeight * blockHeight);
    return width * height * bytesPerTexel;
}

VkImageView DepthSurfaceView(const Context& context, const DepthTarget& target) {
    std::lock_guard lock(surfacesMutex());
    for (const auto& surface : surfaces()) {
        if (surface->context.device == context.device && sameSurface(surface->target, target)) {
            return surface->view;
        }
    }
    surfaces().push_back(std::make_unique<DepthSurface>(context, target));
    return surfaces().back()->view;
}

void NoteDepthSurfaceWrite(const Context& context, const DepthTarget& target, VkImageAspectFlags aspects) {
    if (aspects == 0) return;
    std::lock_guard lock(surfacesMutex());
    for (const auto& surface : surfaces()) {
        if (surface->context.device != context.device || !sameSurface(surface->target, target)) continue;
        surface->NoteWrite(aspects);
        return;
    }
    throw std::runtime_error("AGC graphics: recording a write to an unknown depth surface");
}

void NoteDepthSurfaceWrite(const Context& context, const State& state) {
    if (!state.depth) return;
    VkImageAspectFlags aspects = state.depthTest && state.depthWrite ? VK_IMAGE_ASPECT_DEPTH_BIT : 0u;
    const auto writesStencil = [](const VkStencilOpState& stencil) {
        return stencil.writeMask != 0 && (stencil.failOp != VK_STENCIL_OP_KEEP || stencil.passOp != VK_STENCIL_OP_KEEP || stencil.depthFailOp != VK_STENCIL_OP_KEEP);
    };
    if (state.stencilTest && (writesStencil(state.stencilFront) || writesStencil(state.stencilBack))) aspects |= VK_IMAGE_ASPECT_STENCIL_BIT;
    NoteDepthSurfaceWrite(context, *state.depth, aspects);
}

void ClearDepthSurfaces(VkDevice device) {
    std::lock_guard lock(surfacesMutex());
    std::erase_if(surfaces(), [&](const auto& surface) { return surface->context.device == device; });
}

void RunDepthClearPass(const Context& context, const DepthClearPass& pass) {
    Require(pass.aspects != 0 && (pass.aspects & ~(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) == 0, "invalid depth clear aspects");
    Require((pass.aspects & VK_IMAGE_ASPECT_DEPTH_BIT) == 0 || pass.target.address != 0, "depth clear has no depth plane");
    Require((pass.aspects & VK_IMAGE_ASPECT_STENCIL_BIT) == 0 || pass.target.stencilAddress != 0, "stencil clear has no stencil plane");
    std::lock_guard lock(surfacesMutex());
    for (const auto& surface : surfaces()) {
        if (surface->context.device != context.device || !sameSurface(surface->target, pass.target)) continue;
        surface->Clear(pass);
        return;
    }
    surfaces().push_back(std::make_unique<DepthSurface>(context, pass.target));
    surfaces().back()->Clear(pass);
}

void RunDepthCopyPass(const Context& context, const DepthCopyPass& pass) {
    Require(pass.aspects != 0 && (pass.aspects & ~(VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) == 0, "invalid depth copy aspects");
    Require(pass.source.format == pass.destination.format && pass.source.extent.width == pass.destination.extent.width && pass.source.extent.height == pass.destination.extent.height, "depth copy surfaces have incompatible formats or extents");
    Require((pass.aspects & VK_IMAGE_ASPECT_DEPTH_BIT) == 0 || (pass.source.address != 0 && pass.destination.address != 0), "depth copy has an absent depth plane");
    Require((pass.aspects & VK_IMAGE_ASPECT_STENCIL_BIT) == 0 || (pass.source.stencilAddress != 0 && pass.destination.stencilAddress != 0), "depth copy has an absent stencil plane");
    std::lock_guard lock(surfacesMutex());
    DepthSurface* source = nullptr;
    DepthSurface* destination = nullptr;
    for (const auto& surface : surfaces()) {
        if (surface->context.device != context.device) continue;
        if (sameSurface(surface->target, pass.source)) source = surface.get();
        if (sameSurface(surface->target, pass.destination)) destination = surface.get();
    }
    Require(source != nullptr, "depth copy source has no resident surface");
    if (source == destination) return;
    Require(pass.source.address == 0 || pass.source.address != pass.destination.address, "depth copies with a shared depth plane are unsupported");
    Require(pass.source.stencilAddress == 0 || pass.source.stencilAddress != pass.destination.stencilAddress, "depth copies with a shared stencil plane are unsupported");
    if (destination == nullptr) {
        Require(((pass.aspects & VK_IMAGE_ASPECT_DEPTH_BIT) != 0 || pass.destination.address == 0) && ((pass.aspects & VK_IMAGE_ASPECT_STENCIL_BIT) != 0 || pass.destination.stencilAddress == 0), "partial depth copy destination has no resident surface");
        surfaces().push_back(std::make_unique<DepthSurface>(context, pass.destination));
        destination = surfaces().back().get();
    }
    destination->CopyFrom(*source, pass.aspects);
}

std::shared_ptr<Texture> DepthSurfaceTexture(const Context& context, std::span<const std::uint32_t> words, const GuestTextureResource& resource, VkComponentMapping components) {
    std::lock_guard lock(surfacesMutex());
    const auto& list = surfaces();
    const auto find = [&](std::uint64_t address) -> DepthSurface* {
        const auto found = std::find_if(list.rbegin(), list.rend(), [&](const auto& surface) {
            return surface->context.device == context.device && (surface->target.address == address || (surface->target.stencilAddress != 0 && surface->target.stencilAddress == address));
        });
        return found == list.rend() ? nullptr : found->get();
    };
    auto* base = find(resource.baseAddress);
    auto matchedAddress = resource.baseAddress;
    const bool array = resource.dimension == TextureDimension::k2DArray;
    if (base == nullptr && array) {
        for (auto it = list.rbegin(); it != list.rend(); ++it) {
            const auto& surface = **it;
            if (surface.context.device != context.device || surface.target.extent.width != resource.width || surface.target.extent.height != resource.height) continue;
            const auto matches = [&](std::uint64_t address, std::uint32_t bytes) {
                if (address == 0 || address < resource.baseAddress) return false;
                const auto stride = DepthSliceBytes(surface.target.extent, bytes);
                const auto offset = address - resource.baseAddress;
                return offset % stride == 0 && offset / stride >= resource.baseArray && offset / stride <= resource.depthOrLastArray;
            };
            const bool d16 = surface.target.format == VK_FORMAT_D16_UNORM || surface.target.format == VK_FORMAT_D16_UNORM_S8_UINT;
            if (matches(surface.target.address, d16 ? 2u : 4u)) matchedAddress = surface.target.address;
            else if (matches(surface.target.stencilAddress, 1u)) matchedAddress = surface.target.stencilAddress;
            else continue;
            base = it->get();
            break;
        }
    }
    if (base == nullptr) return nullptr;
    auto viewed = resource;
    viewed.baseAddress = matchedAddress;
    base->ValidateSampled(words, viewed);
    const bool stencil = base->target.stencilAddress != 0 && matchedAddress == base->target.stencilAddress;
    const auto last = array ? resource.depthOrLastArray : 0u;
    Require(resource.baseArray <= last && last < 8192u && last - resource.baseArray < context.limits.maxImageArrayLayers, "depth array layer range is invalid");
    if (array) Require(resource.tileMode == TextureTileMode::kZ64KBX, "depth arrays outside the 64 KiB Z layout are not implemented");
    const bool d16 = base->target.format == VK_FORMAT_D16_UNORM || base->target.format == VK_FORMAT_D16_UNORM_S8_UINT;
    const auto stride = DepthSliceBytes(base->target.extent, stencil ? 1u : d16 ? 2u : 4u);
    std::vector<DepthSurface*> slices;
    for (auto layer = resource.baseArray; layer <= last; ++layer) {
        const auto address = resource.baseAddress + layer * stride;
        auto* slice = find(address);
        if (slice != nullptr && (slice->target.format != base->target.format || slice->target.extent.width != resource.width || slice->target.extent.height != resource.height || (stencil ? slice->target.stencilAddress : slice->target.address) != address)) {
            char text[192];
            std::snprintf(text, sizeof(text), "AGC graphics: depth array 0x%llx layer %u at 0x%llx has no matching resident %s surface", static_cast<unsigned long long>(resource.baseAddress), layer, static_cast<unsigned long long>(address), stencil ? "stencil" : "depth");
            throw std::runtime_error(text);
        }
        slices.push_back(slice);
    }
    return base->Sampled(resource, components, slices, stencil);
}

bool DepthSurfaceAt(std::uint64_t address) {
    std::lock_guard lock(surfacesMutex());
    return std::any_of(surfaces().begin(), surfaces().end(), [&](const auto& surface) { return surface->target.address == address || surface->target.stencilAddress == address; });
}

}
