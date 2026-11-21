#include "prx/libc/include/HostThreadSlot.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Execution/include/AsyncLog.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SlimMutex.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/AtomicSharedPtr.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Execution/include/DeviceFault.hpp"
#include "prx/libc/include/HostThreadSlot.hpp"
#include "prx/libSceAgcDriver/Execution/include/ThreadScratch.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MapImportGate.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "ShaderCacheDirectory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/UnitShadow.hpp"
#include "prx/libSceAgcDriver/Graphics/include/KeptUnits.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "RdnaDecoder/include/RdnaDecoder/RdnaDescriptorFormat.hpp"
#include "IntermediateRepresentation/include/IntermediateRepresentation/IrMetadata/BufferFormat.hpp"
#include <algorithm>
#include <filesystem>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <stdexcept>
#include <vector>
#include <atomic>
#include <mutex>
#include <map>
#include <limits>
#include <exception>
#include <functional>
#include <optional>
#include <thread>
#include <unordered_map>
#include <utility>
#if defined(__APPLE__)
#include <unistd.h>
#endif

namespace AgcDriver::Graphics {
namespace {
namespace {
namespace {
namespace {
bool ClearColorForTexel(VkFormat format, std::uint32_t elementBytes, std::span<const std::uint32_t, 4> pattern, VkClearColorValue& clear) {
    return FillClearColor(format, elementBytes, pattern, clear);
}

VkImageView StorageTexture::createView(std::uint32_t mip, bool firstLayer, VkFormat format) const {
    Require(mip < descriptor.mipCount, "storage texture mip level is outside the texture");
    Require(!firstLayer || descriptor.dimension == TextureDimension::k2DArray, "a first-layer storage view needs a 2D array surface");
    const auto viewLayerCount = firstLayer ? 1u : geometry.imageLayers - descriptor.baseArray;
    VkImageViewUsageCreateInfo usage{VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO};
    const bool packed = PackedStorageViewFormat(storageFormat) != VK_FORMAT_UNDEFINED;
    usage.usage = packed && format == storageFormat ? VK_IMAGE_USAGE_SAMPLED_BIT | (attachable ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT : 0u) : VK_IMAGE_USAGE_STORAGE_BIT;
    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.pNext = format == storageFormat && !packed ? nullptr : &usage;
    viewInfo.image = image;
    viewInfo.viewType = firstLayer ? VK_IMAGE_VIEW_TYPE_2D : descriptor.dimension == TextureDimension::k1D ? VK_IMAGE_VIEW_TYPE_1D : descriptor.dimension == TextureDimension::k2D ? VK_IMAGE_VIEW_TYPE_2D : descriptor.dimension == TextureDimension::k3D ? VK_IMAGE_VIEW_TYPE_3D : descriptor.dimension == TextureDimension::k1DArray ? VK_IMAGE_VIEW_TYPE_1D_ARRAY : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
    viewInfo.format = format;
    viewInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, mip, 1u, descriptor.baseArray, viewLayerCount};
    VkImageView created = VK_NULL_HANDLE;
    Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &created), "vkCreateImageView storage");
    return created;
}

bool StorageTexture::singlePass() {
    if (singlePassState < 0) {
        const auto elementBytes = BytesPerElement(descriptor.format);
        const auto format = TextureDetiler::ImageElementFormat(elementBytes);
        bool eligible = context.singlePassStorage && format != VK_FORMAT_UNDEFINED && BlockWidth(descriptor.format) == 1 && !geometry.thick && geometry.imageDepth == 1 && (descriptor.dimension == TextureDimension::k2D || descriptor.dimension == TextureDimension::k2DArray || descriptor.dimension == TextureDimension::kCube);
        if (eligible) {
            VkFormatProperties properties{};
            context.formatProperties(context.physical, format, &properties);
            eligible = (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) != 0;
        }
        singlePassState = eligible ? 1 : 0;
    }
    return singlePassState == 1;
}

VkImageView StorageTexture::elementLayerView(std::uint32_t level, std::uint32_t layer) {
    const auto key = (level << 16) | layer;
    if (const auto found = elementLayerViews.find(key); found != elementLayerViews.end()) return found->second;
    VkImageViewUsageCreateInfo usage{VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO};
    usage.usage = VK_IMAGE_USAGE_STORAGE_BIT;
    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.pNext = &usage;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = TextureDetiler::ImageElementFormat(BytesPerElement(descriptor.format));
    viewInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, level, 1u, geometry.CopyLayer(layer), 1u};
    VkImageView created = VK_NULL_HANDLE;
    Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &created), "vkCreateImageView storage element");
    elementLayerViews.emplace(key, created);
    return created;
}

VkImageView StorageTexture::AttachmentView(VkFormat format, std::uint32_t mip, std::uint32_t depthSlice, std::uint32_t layers, std::uint32_t layer) {
    Require(attachable, "storage image cannot be a color attachment");
    Require(mip < descriptor.mipCount, "attachment mip exceeds the storage image");
    const bool volume = descriptor.dimension == TextureDimension::k3D;
    const bool array = !volume && descriptor.dimension == TextureDimension::k2DArray;
    const auto depth = volume ? std::max((descriptor.depthOrLastArray + 1u) >> mip, 1u) : 1u;
    Require((depthSlice == 0 && layers == 1) || (volume && layers != 0 && depthSlice < depth && layers <= depth - depthSlice) || (array && depthSlice == 0 && layers != 0), "attachment slice is outside the storage image");
    Require((layer == 0 && layers == 1) || (volume && layer == 0) || (array && descriptor.baseArray + layer + layers - 1u <= descriptor.depthOrLastArray), "attachment layer is outside the storage image");
    const auto found = attachmentViews.find({format, mip, depthSlice, layers, layer});
    if (found != attachmentViews.end()) return found->second;
    VkImageViewUsageCreateInfo usage{VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO};
    usage.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.pNext = &usage;
    viewInfo.image = image;
    viewInfo.viewType = layers > 1 ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, mip, 1u, volume ? depthSlice : descriptor.baseArray + layer, layers};
    VkImageView created = VK_NULL_HANDLE;
    Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &created), "vkCreateImageView attachment");
    attachmentViews.emplace(std::tuple{format, mip, depthSlice, layers, layer}, created);
    return created;
}

VkImageView StorageTexture::AttachmentProxyView() {
    if (proxyView != VK_NULL_HANDLE) return proxyView;
    Require(storageFormat == VK_FORMAT_R8_UNORM && descriptor.dimension == TextureDimension::k2D && descriptor.mipCount == 1 && geometry.imageLayers == 1, "an RGBA8_SRGB attachment proxy is only modeled for a single-mip 2D R8 surface");
    try {
        VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
        imageInfo.flags = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageInfo.extent = {descriptor.width, descriptor.height, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &imageInfo, nullptr, &proxyImage), "vkCreateImage attachment proxy");
        proxyAllocation = AllocateImageMemory(context, proxyImage, imageInfo, "vkAllocateMemory attachment proxy");
        Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, proxyImage, proxyAllocation.Memory(), proxyAllocation.Offset()), "vkBindImageMemory attachment proxy");
        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = proxyImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
        viewInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &viewInfo, nullptr, &proxyView), "vkCreateImageView attachment proxy");
    } catch (...) {
        if (proxyImage) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, proxyImage, nullptr);
        proxyAllocation.Reset();
        proxyImage = VK_NULL_HANDLE;
        proxyView = VK_NULL_HANDLE;
        throw;
    }
    return proxyView;
}

namespace {

void blitWhole(const Context& context, VkCommandBuffer commands, VkImage source, VkImage destination, std::uint32_t width, std::uint32_t height) {
    VkImageBlit region{};
    region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.srcOffsets[1] = {static_cast<std::int32_t>(width), static_cast<std::int32_t>(height), 1};
    region.dstSubresource = region.srcSubresource;
    region.dstOffsets[1] = region.srcOffsets[1];
    context.Function<PFN_vkCmdBlitImage>("vkCmdBlitImage")(commands, source, VK_IMAGE_LAYOUT_GENERAL, destination, VK_IMAGE_LAYOUT_GENERAL, 1, &region, VK_FILTER_NEAREST);
}

VkImageMemoryBarrier proxyBarrier(VkImage image, VkAccessFlags source, VkAccessFlags destination, VkImageLayout from, VkImageLayout to) {
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = source;
    barrier.dstAccessMask = destination;
    barrier.oldLayout = from;
    barrier.newLayout = to;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    return barrier;
}

}

void StorageTexture::RecordAttachmentProxyLoad(VkCommandBuffer commands, VkImageLayout attachmentLayout) const {
    Require(proxyView != VK_NULL_HANDLE, "the storage image has no attachment proxy");
    const auto barrier = context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier");
    const VkMemoryBarrier written{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT};
    const auto discard = proxyBarrier(proxyImage, 0, VK_ACCESS_TRANSFER_WRITE_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    barrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &written, 0, nullptr, 1, &discard);
    blitWhole(context, commands, image, proxyImage, descriptor.width, descriptor.height);
    const auto attach = proxyBarrier(proxyImage, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_IMAGE_LAYOUT_GENERAL, attachmentLayout);
    barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1, &attach);
}

void StorageTexture::RecordAttachmentProxyStore(VkCommandBuffer commands, VkImageLayout attachmentLayout) const {
    Require(proxyView != VK_NULL_HANDLE, "the storage image has no attachment proxy");
    const auto barrier = context.Resolved(&DeviceFunctions::cmdPipelineBarrier, "vkCmdPipelineBarrier");
    const auto detach = proxyBarrier(proxyImage, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT, attachmentLayout, VK_IMAGE_LAYOUT_GENERAL);
    barrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &detach);
    blitWhole(context, commands, proxyImage, image, descriptor.width, descriptor.height);
    const VkMemoryBarrier stored{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT};
    barrier(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &stored, 0, nullptr, 0, nullptr);
}

VkImageView StorageTexture::View(std::uint32_t mip) {
    if (mip == defaultMip) return view;
    const auto found = extraViews.find(mip);
    if (found != extraViews.end()) return found->second;
    const auto created = createView(mip, false, storageFormat);
    extraViews.emplace(mip, created);
    return created;
}

VkImageView StorageTexture::FirstLayerView(std::uint32_t mip) {
    const auto found = firstLayerViews.find(mip);
    if (found != firstLayerViews.end()) return found->second;
    const auto created = createView(mip, true, storageFormat);
    firstLayerViews.emplace(mip, created);
    return created;
}

VkImageView StorageTexture::StorageView(std::uint32_t mip, bool firstLayer, bool uintStorage) {
    const auto packed = PackedStorageViewFormat(storageFormat);
    const auto format = packed != VK_FORMAT_UNDEFINED ? packed : uintStorage ? UintStorageViewFormat(storageFormat) : VK_FORMAT_UNDEFINED;
    if (format == VK_FORMAT_UNDEFINED) return firstLayer ? FirstLayerView(mip) : View(mip);
    if (format == VK_FORMAT_R32_UINT) return AtomicView(mip, firstLayer);
    const auto found = uintViews.find({mip, firstLayer});
    if (found != uintViews.end()) return found->second;
    Require(StorageFormatOrUndefined(context, format) == format, "storage image of format " + std::to_string(storageFormat) + " has no storage view of its UINT format " + std::to_string(format));
    const auto created = createView(mip, firstLayer, format);
    uintViews.emplace(std::pair{mip, firstLayer}, created);
    return created;
}

VkImageView StorageTexture::ElementView() {
    if (elementView != VK_NULL_HANDLE) return elementView;
    if (descriptor.dimension != TextureDimension::k2D || geometry.imageLayers != 1) return VK_NULL_HANDLE;
    const auto bytes = BytesPerElement(descriptor.format);
    const auto format = bytes == 1 ? VK_FORMAT_R8_UINT : bytes == 2 ? VK_FORMAT_R16_UINT : bytes == 4 ? VK_FORMAT_R32_UINT : bytes == 8 ? VK_FORMAT_R32G32_UINT : VK_FORMAT_UNDEFINED;
    if (format == VK_FORMAT_UNDEFINED || StorageFormatOrUndefined(context, format) != format) return VK_NULL_HANDLE;
    elementView = createView(0, false, format);
    return elementView;
}

VkImageView StorageTexture::AtomicView(std::uint32_t mip, bool firstLayer) {
    if (storageFormat == VK_FORMAT_R32_UINT) return firstLayer ? FirstLayerView(mip) : View(mip);
    Require(storageFormat == VK_FORMAT_R32_SINT || storageFormat == VK_FORMAT_R32_SFLOAT, "storage image atomics need a surface of one 32-bit component");
    const auto found = atomicViews.find({mip, firstLayer});
    if (found != atomicViews.end()) return found->second;
    const auto created = createView(mip, firstLayer, VK_FORMAT_R32_UINT);
    atomicViews.emplace(std::pair{mip, firstLayer}, created);
    return created;
}

VkImageView StorageTexture::Atomic64View(std::uint32_t mip, bool firstLayer) {
    const auto format = context.imageInt64Atomics ? VK_FORMAT_R64_UINT : VK_FORMAT_R32G32_UINT;
    if (storageFormat == format) return firstLayer ? FirstLayerView(mip) : View(mip);
    Require(storageFormat == VK_FORMAT_R32G32_UINT || storageFormat == VK_FORMAT_R32G32_SINT || storageFormat == VK_FORMAT_R32G32_SFLOAT, "64-bit storage image atomics need a surface of two 32-bit components");
    const auto found = atomicViews.find({mip, firstLayer});
    if (found != atomicViews.end()) return found->second;
    const auto created = createView(mip, firstLayer, format);
    atomicViews.emplace(std::pair{mip, firstLayer}, created);
    return created;
}

const char* LookupOutcomes::Name(Kind kind) {
    static constexpr const char* names[Count] = {"sampled fast hit", "sampled fast miss", "sampled hit view", "sampled hit cleared view", "sampled hit snapshot", "sampled made view", "sampled made snapshot", "storage hit", "storage made", "refresh unchanged", "refresh compared", "refresh proved", "upload direct", "upload cpu", "upload clear", "dcc scan", "pending flush"};
    return kind < Count ? names[kind] : "?";
}

bool LookupOutcomes::Profiled() {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    return profile;
}

std::chrono::steady_clock::time_point LookupOutcomes::Add(Kind kind, std::chrono::steady_clock::time_point start) {
    const auto now = std::chrono::steady_clock::now();
    auto& outcomes = ThreadLookupOutcomes();
    ++outcomes.counts[kind];
    outcomes.ms[kind] += std::chrono::duration<double, std::milli>(now - start).count();
    return now;
}

LookupOutcomes& ThreadLookupOutcomes() {
    thread_local LookupOutcomes outcomes;
    return outcomes;
}

namespace {
std::atomic<void (*)(std::uint64_t, std::size_t)> refreshUnchangedHookForTest{nullptr};
}

void StorageTexture::SetRefreshUnchangedHookForTest(void (*hook)(std::uint64_t address, std::size_t bytes)) {
    refreshUnchangedHookForTest.store(hook, std::memory_order_release);
}

bool StorageTexture::Refresh() {
    return refresh(0, trackedLayers);
}

std::pair<std::uint32_t, std::uint32_t> StorageTexture::refreshUnits(std::uint32_t first, std::uint32_t count) const {
    const std::pair whole{std::uint32_t{0}, trackedLayers};
    if (count == 0 || geometry.thick || geometry.imageLayers <= 1 || first >= geometry.imageLayers || count > geometry.imageLayers - first || geometry.layerBytes == 0 || trackedLayerBytes == 0 || descriptor.dccAddress != 0) return whole;
    const auto begin = geometry.GuestLayerOffset(first);
    const auto end = geometry.GuestLayerOffset(first + count - 1) + geometry.layerBytes;
    const auto firstLayer = static_cast<std::uint32_t>(begin / trackedLayerBytes);
    const auto lastLayer = static_cast<std::uint32_t>(std::min<std::uint64_t>((end + trackedLayerBytes - 1) / trackedLayerBytes, trackedLayers));
    if (firstLayer >= lastLayer) return whole;
    for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
        if (layerPending[layer] && (layer < firstLayer || layer >= lastLayer)) return whole;
    }
    if (pendingAlias() != nullptr) return whole;
    return {firstLayer, lastLayer};
}

bool StorageTexture::RefreshLayers(std::uint32_t first, std::uint32_t count) {
    const auto [firstLayer, lastLayer] = refreshUnits(first, count);
    if (firstLayer == 0 && lastLayer == trackedLayers) {
        static_cast<void>(FlushDepthPlanes(descriptor.baseAddress, static_cast<std::size_t>(guestBytes)));
        return Refresh();
    }
    const auto scopeBegin = layerBegin(firstLayer);
    static_cast<void>(FlushDepthPlanes(scopeBegin, static_cast<std::size_t>(layerBegin(lastLayer - 1) + layerBytes(lastLayer - 1) - scopeBegin)));
    return refresh(firstLayer, lastLayer);
}

namespace {

bool& GpuOrderedFlush();
bool& GpuOrderedSkipped();
std::atomic<std::uint64_t> orderedFlushSkips{0};
std::atomic<std::uint64_t> orderedRefreshSkips{0};
std::atomic<bool> flushFailureForTest{false};
std::atomic<int> flushFailureFiredForTest{0};

}

bool StorageTexture::refresh(std::uint32_t firstLayer, std::uint32_t lastLayer) {
    const bool profile = LookupOutcomes::Profiled();
    auto start = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const bool scoped = firstLayer != 0 || lastLayer != trackedLayers;
    const bool epochProofs = ShaderResources::EpochProofs();
    const auto epoch = epochProofs ? GuestMemory::CollectEpoch() : 0;
    if (epoch != 0 && provedEpoch.load(std::memory_order_relaxed) == epoch && provedProof.load(std::memory_order_relaxed) == GuestMemory::ProofSerial() && provedPending.load(std::memory_order_relaxed) == PendingSerial()) {
        NoteProved();
        refreshesProved.fetch_add(1, std::memory_order_relaxed);
        epochRefreshes.fetch_add(1, std::memory_order_relaxed);
        return true;
    }
    const auto proofBefore = GuestMemory::ProofSerial();
    const auto pendingBefore = PendingSerial();
    CompletePendingCpuWrites(context, descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
    if (descriptor.dccAddress != 0) CompletePendingCpuWrites(context, descriptor.dccAddress, DccKeyBytes(guestBytes));
    if (refreshProved()) {
        layerBehind.clear();
        if (epoch != 0) {
            provedEpoch.store(epoch, std::memory_order_relaxed);
            provedProof.store(proofBefore, std::memory_order_relaxed);
            provedPending.store(pendingBefore, std::memory_order_relaxed);
        }
        NoteProved();
        refreshesProved.fetch_add(1, std::memory_order_relaxed);
        if (profile && keyProof.pendingNote != 0) refreshesProvedPendingKeys.fetch_add(1, std::memory_order_relaxed);
        if (profile) LookupOutcomes::Add(LookupOutcomes::RefreshProved, start);
        return true;
    }
    refreshProof = {};
    struct Exempt {
        const StorageTexture* previous;
        ~Exempt() { Refreshing::Set(previous); }
    } exempt{Refreshing::Get()};
    Refreshing::Set(this);
    NoteProved();
    auto alias = scoped ? std::shared_ptr<StorageTexture>{} : pendingAlias();
    if (alias != nullptr && !(ProvedClearKeys(descriptor, guestBytes, keyProof) == DccKeys::Uncompressed && HostImportFor(context, descriptor.baseAddress, static_cast<std::size_t>(guestBytes)) != nullptr)) alias = nullptr;
    if (alias != nullptr) {
        std::vector<std::uint8_t> aliasStamped(trackedLayers);
        if (!GuestMemory::ChangedBlocks(descriptor.baseAddress, static_cast<std::size_t>(guestBytes), alias->layerGeneration, aliasStamped)) alias = nullptr;
        for (std::uint32_t unit = 0; alias != nullptr && unit < trackedLayers; ++unit) {
            if (!alias->layerPending[unit]) continue;
            if (alias->layerGeneration[unit] == 0 || (aliasStamped[unit] == GuestMemory::BlockUnchanged && layerPending[unit])) {
                alias = nullptr;
                break;
            }
        }
    }
    const auto scopeBegin = scoped ? layerBegin(firstLayer) : descriptor.baseAddress;
    const auto scopeBytes = scoped ? static_cast<std::size_t>(layerBegin(lastLayer - 1) + layerBytes(lastLayer - 1) - scopeBegin) : static_cast<std::size_t>(guestBytes);
    handedOver.clear();
    handedUnits.clear();
    const auto handOverKeys = alias == nullptr && !scoped ? clearHandOver() : std::nullopt;
    static const bool refreshOrdered = std::getenv("APS5_NO_REFRESH_GPU_ORDERED") == nullptr;
    const bool orderedEligible = refreshOrdered && !scoped && alias == nullptr && !handOverKeys.has_value() && descriptor.dccAddress == 0 && !layerPending.Any() && HostImportFor(context, descriptor.baseAddress, static_cast<std::size_t>(guestBytes)) != nullptr;
    bool orderedSkipped = false;
    bool flushed = false;
    {
        struct OrderedRefreshScope {
            bool previousOrdered;
            bool previousSkipped;
            explicit OrderedRefreshScope(bool eligible) : previousOrdered(GpuOrderedFlush()), previousSkipped(GpuOrderedSkipped()) {
                GpuOrderedFlush() = eligible;
                GpuOrderedSkipped() = false;
            }
            ~OrderedRefreshScope() {
                const bool skipped = GpuOrderedSkipped();
                GpuOrderedFlush() = previousOrdered;
                GpuOrderedSkipped() = previousSkipped || skipped;
            }
        } ordered{orderedEligible};
        flushed = !handOverKeys.has_value() && FlushPending(scopeBegin, scopeBytes, alias.get(), "storage refresh", PublishScope::None);
        orderedSkipped = orderedEligible && GpuOrderedSkipped();
    }
    if (orderedSkipped) orderedRefreshSkips.fetch_add(1, std::memory_order_relaxed);
    pruneAdoptedSources();
    if (profile && flushed) start = LookupOutcomes::Add(LookupOutcomes::PendingFlush, start);
    const auto current = GuestMemory::CollectWrites(descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
    const auto scopedCurrent = current != 0 || !scoped ? current : GuestMemory::CollectWrites(scopeBegin, scopeBytes);
    std::vector<std::uint64_t> collected;
    if (scopedCurrent == 0) {
        collected.resize(trackedLayers);
        for (auto layer = firstLayer; layer < lastLayer; ++layer) collected[layer] = collectedLayerGeneration(layer, 0);
    }
    const auto currentFor = [&](std::uint32_t layer) {
        if (scoped && (layer < firstLayer || layer >= lastLayer)) return current;
        return scopedCurrent != 0 ? scopedCurrent : collected[layer];
    };
    std::vector<bool> changed(trackedLayers, false);
    std::vector<std::uint64_t> generations;
    std::vector<std::uint8_t> stampedBlocks, cpuBlocks;
    std::vector<bool> rewatchCompared(trackedLayers, false);
    std::vector<bool> outside;
    std::vector<bool> borrowed;
    bool anyBorrowed = false;
    bool unchanged = true;
    bool stamped = true;
    bool tracked = true;
    bool compared = false;
    std::vector<bool> unitTracked;
    std::vector<bool> unitCompared;
    bool keysChanged = false;
    bool cpuWrote = false;
    bool direct = false;
    DccKeys keys = uploadedKeys;
    {
        const auto equalsOriginal = [&] {
            const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::TextureCompare);
            if (scoped) return GuestMemory::EqualsCommitted(scopeBegin, std::span<const std::byte>(original).subspan(static_cast<std::size_t>(scopeBegin - descriptor.baseAddress), scopeBytes));
            return GuestMemory::EqualsCommitted(descriptor.baseAddress, original);
        };
        const auto scanBlocks = [&] {
            blockGenerations(generations);
            stampedBlocks.assign(generations.size(), 0);
            cpuBlocks.assign(generations.size(), 0);
            tracked = GuestMemory::ChangedBlocks(descriptor.baseAddress, static_cast<std::size_t>(guestBytes), generations, stampedBlocks, cpuBlocks);
            if (tracked && !keysChanged) confirmRewatched(descriptor.baseAddress & ~std::uint64_t{0xffff}, stampedBlocks, cpuBlocks, &rewatchCompared);
            if (!tracked) awaitStoredSave(nullptr);
            const auto firstBlock = static_cast<std::size_t>(scopeBegin / 65536 - descriptor.baseAddress / 65536);
            const auto blocks = static_cast<std::size_t>((scopeBegin + scopeBytes - 1) / 65536 - scopeBegin / 65536 + 1);
            compared = !tracked && (scoped ? compareUntracked(scopeBegin, scopeBytes, std::span(stampedBlocks).subspan(firstBlock, blocks), true) : compareUntracked(descriptor.baseAddress, static_cast<std::size_t>(guestBytes), stampedBlocks, true));
            unitTracked.clear();
            unitCompared.clear();
            if (!tracked && !compared) {
                unitTracked.assign(trackedLayers, false);
                unitCompared.assign(trackedLayers, false);
                compared = true;
                for (auto unit = firstLayer; unit < lastLayer; ++unit) {
                    const auto begin = layerBegin(unit);
                    const auto unitBytes = static_cast<std::size_t>(layerBytes(unit));
                    const auto offset = static_cast<std::size_t>(begin / 65536 - descriptor.baseAddress / 65536);
                    const auto count = static_cast<std::size_t>((begin + unitBytes - 1) / 65536 - begin / 65536 + 1);
                    auto changed = std::span(stampedBlocks).subspan(offset, count);
                    auto cpu = std::span(cpuBlocks).subspan(offset, count);
                    GuestMemory::CollectWritesUncached(begin, unitBytes);
                    unitTracked[unit] = GuestMemory::ChangedBlocks(begin, unitBytes, std::span(generations).subspan(offset, count), changed, cpu);
                    if (unitTracked[unit] && !keysChanged) confirmRewatched(begin & ~std::uint64_t{0xffff}, changed, cpu, &rewatchCompared);
                    if (!unitTracked[unit]) {
                        unitCompared[unit] = compareUntracked(begin, unitBytes, changed, true);
                        if (unitCompared[unit]) std::copy(changed.begin(), changed.end(), cpu.begin());
                    }
                    compared = compared && (unitTracked[unit] || unitCompared[unit]);
                }
            } else if (compared) {
                cpuBlocks = stampedBlocks;
            }
            if (compared) {
                for (const auto& [from, unitGeneration] : keptAtUnwatch) {
                    const auto unit = static_cast<std::uint32_t>((from - descriptor.baseAddress) / trackedLayerBytes);
                    if (keptAtUnwatchHolds(from, unit)) stampedBlocks[static_cast<std::size_t>(from / 65536 - descriptor.baseAddress / 65536)] = GuestMemory::BlockWritten;
                }
                if (scoped) {
                    std::fill(stampedBlocks.begin(), stampedBlocks.begin() + static_cast<std::ptrdiff_t>(firstBlock), GuestMemory::BlockWritten);
                    std::fill(stampedBlocks.begin() + static_cast<std::ptrdiff_t>(firstBlock + blocks), stampedBlocks.end(), GuestMemory::BlockWritten);
                }
            }
        };
        const auto keysStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        keys = handOverKeys.has_value() ? *handOverKeys : ProvedKeys();
        if (profile && descriptor.dccAddress != 0) LookupOutcomes::Add(LookupOutcomes::DccScan, keysStart);
        if (keys != uploadedKeys) {
            changed.assign(trackedLayers, true);
            unchanged = false;
            keysChanged = true;
            if (blockUnits) scanBlocks();
        } else if (blockUnits) {
            if (!GuestMemory::UnchangedSince(descriptor.baseAddress, static_cast<std::size_t>(guestBytes), generation)) {
                scanBlocks();
                for (std::uint32_t unit = 0; unit < trackedLayers; ++unit) {
                    if (stampedBlocks[unit] == 0) {
                        layerGeneration[unit] = advanceUnchanged(unit, currentFor(unit));
                        continue;
                    }
                    changed[unit] = true;
                    unchanged = false;
                }
                refreshGeneration();
            }
        } else {
            for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
                if (GuestMemory::UnchangedSince(layerBegin(layer), static_cast<std::size_t>(layerBytes(layer)), layerGeneration[layer])) continue;
                if (!keysChanged && rewatchProof(layerBegin(layer), layerBegin(layer) + layerBytes(layer), false) == RewatchProof::Unchanged) {
                    rewatchedConfirms.fetch_add(1, std::memory_order_relaxed);
                    layerGeneration[layer] = currentFor(layer);
                    continue;
                }
                changed[layer] = true;
                unchanged = false;
            }
            if (!unchanged) scanBlocks();
        }
        if (compared && !keysChanged) {
            for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
                const auto first = layerBegin(layer) / 65536 - descriptor.baseAddress / 65536;
                const auto last = (layerBegin(layer) + layerBytes(layer) - 1) / 65536 - descriptor.baseAddress / 65536;
                changed[layer] = std::any_of(stampedBlocks.begin() + first, stampedBlocks.begin() + last + 1, [](auto value) { return value != GuestMemory::BlockUnchanged; });
            }
            unchanged = std::none_of(changed.begin(), changed.end(), [](bool value) { return value; });
        }
        if (scoped && !keysChanged) {
            outside.assign(trackedLayers, false);
            for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
                if ((layer >= firstLayer && layer < lastLayer) || (!changed[layer] && !compared)) continue;
                outside[layer] = true;
                changed[layer] = false;
            }
            unchanged = std::none_of(changed.begin(), changed.end(), [](bool value) { return value; });
        }
        for (std::size_t k = 0; k < cpuBlocks.size(); ++k) {
            if (cpuBlocks[k] != 0 && (compared || (tracked && generations[k] != 0))) cpuWrote = true;
        }
        bool pendingChanged = false;
        for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
            if (changed[layer] && layerPending[layer]) pendingChanged = true;
        }
        if (!orderedSkipped && !unchanged && !keysChanged && !pendingChanged && (scoped ? originalHolds(scopeBegin, scopeBytes) : originalHolds(descriptor.baseAddress, static_cast<std::size_t>(guestBytes))) && equalsOriginal()) {
            unchanged = true;
            stamped = false;
            if (!scoped) outside.clear();
        }
        direct = keys == DccKeys::Uncompressed && (HostImportFor(context, descriptor.baseAddress, static_cast<std::size_t>(guestBytes)) != nullptr || (blockUnits && !importSegments().empty()));
        if (alias != nullptr && direct) {
            const bool sameBorrow = borrowedFrom.lock() == alias && borrowedVersion == alias->version && borrowedUnits.size() == trackedLayers;
            std::vector<std::uint8_t> aliasStamped(trackedLayers);
            const bool aliasTracked = GuestMemory::ChangedBlocks(descriptor.baseAddress, static_cast<std::size_t>(guestBytes), alias->layerGeneration, aliasStamped);
            borrowed.assign(trackedLayers, false);
            for (std::uint32_t unit = 0; unit < trackedLayers; ++unit) {
                if (!aliasTracked || !alias->layerPending[unit] || aliasStamped[unit] != 0 || alias->layerGeneration[unit] == 0 || layerPending[unit]) continue;
                if (sameBorrow && borrowedUnits[unit]) continue;
                borrowed[unit] = true;
                anyBorrowed = true;
                changed[unit] = false;
                unchanged = false;
            }
            if (!sameBorrow) {
                borrowedFrom.reset();
                borrowedUnits.clear();
            }
        }
    }
    if (orderedSkipped) {
        changed.assign(trackedLayers, true);
        unchanged = false;
        anyBorrowed = false;
        borrowed.clear();
    }
    const bool outsideChanged = std::any_of(outside.begin(), outside.end(), [](bool value) { return value; });
    if (scoped) noteLayersBehind(firstLayer, lastLayer, outside);
    else layerBehind.clear();
    if (unchanged) {
        ++Profile().storageReused;
        if (const auto hook = refreshUnchangedHookForTest.load(std::memory_order_acquire)) hook(descriptor.baseAddress, static_cast<std::size_t>(guestBytes));
        for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
            if (!outsideChanged || !outside[layer]) layerGeneration[layer] = advanceUnchanged(layer, currentFor(layer));
        }
        refreshGeneration();
        if (layerBehind.empty()) takeRefreshProof(alias != nullptr);
        if (profile) LookupOutcomes::Add(stamped ? LookupOutcomes::RefreshUnchanged : LookupOutcomes::RefreshCompared, start);
        return true;
    }
    CaptureTrace::Log("refresh image=%llx bytes=%llu generation=%llu dirty=%d keysChanged=%d cpuWrote=%d", static_cast<unsigned long long>(descriptor.baseAddress), static_cast<unsigned long long>(guestBytes), static_cast<unsigned long long>(generation), dirty, keysChanged, cpuWrote);
    static const bool traceUpload = std::getenv("APS5_TRACE_UPLOAD") != nullptr;
    if (traceUpload) {
        const auto keys = TextureClearKeys(descriptor, guestBytes);
        std::fprintf(stderr, "[upload] 0x%llx+0x%llx %ux%u mips %u: %s (keys %s -> %s, originalValid %d, dirty %d)\n", static_cast<unsigned long long>(descriptor.baseAddress), static_cast<unsigned long long>(guestBytes), descriptor.width, descriptor.height, descriptor.mipCount, keys != uploadedKeys ? "DCC keys changed" : "guest memory changed", DccKeysName(uploadedKeys), DccKeysName(keys), originalHolds(descriptor.baseAddress, static_cast<std::size_t>(guestBytes)) ? 1 : 0, dirty ? 1 : 0);
    }
    std::vector<bool> stored(trackedLayers, false);
    bool pendingResults = false;
    bool anyStored = false;
    bool dropped = false;
    const auto droppable = [&](std::uint32_t unit) {
        if (keysChanged && IsDccClear(keys)) return true;
        if ((unitCompared.empty() ? compared : unitCompared[unit]) || (unit < rewatchCompared.size() && rewatchCompared[unit])) return stampedBlocks.at(unit) == GuestMemory::BlockWritten;
        if (!(unitTracked.empty() ? tracked : unitTracked[unit]) || layerGeneration[unit] == 0 || unit >= stampedBlocks.size() || stampedBlocks[unit] != GuestMemory::BlockWritten) return false;
        const auto begin = layerBegin(unit);
        const auto bytes = layerBytes(unit);
        const bool edge = begin % 65536 != 0 || bytes != 65536;
        return !edge || GuestMemory::StoredOver(begin, static_cast<std::size_t>(bytes), layerGeneration[unit]);
    };
    static const bool keyFlipDrop = std::getenv("APS5_KEYFLIP_DROP") != nullptr;
    static const bool traceKeys = std::getenv("APS5_TRACE_DCC_KEYS") != nullptr;
    const bool keyFlip = keysChanged && blockUnits && !keyFlipDrop && (IsDccClear(uploadedKeys) || uploadedKeys == DccKeys::ClearSingle) && keys == DccKeys::Uncompressed && stampedBlocks.size() >= trackedLayers;
    if (traceKeys && keysChanged) {
        std::size_t pendingUnits = 0, stampedUnits = 0;
        for (std::uint32_t unit = 0; unit < trackedLayers; ++unit) {
            if (layerPending[unit]) ++pendingUnits;
            if (unit < cpuBlocks.size() && cpuBlocks[unit] != 0 && generations[unit] != 0) ++stampedUnits;
        }
        std::fprintf(stderr, "[dcc-keys] refresh 0x%llx+0x%llx keys %s -> %s (dirty %d, %zu pending units, %zu cpu-stamped units%s)\n", static_cast<unsigned long long>(descriptor.baseAddress), static_cast<unsigned long long>(guestBytes), DccKeysName(uploadedKeys), DccKeysName(keys), dirty ? 1 : 0, pendingUnits, stampedUnits, keyFlip ? ", flip keeps unstamped results" : "");
    }
    const bool selectedCpu = !direct && keys == DccKeys::Uncompressed && !cpuUploadLayers(changed).empty();
    for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
        if (!layerPending[layer]) continue;
        if (keyFlip && layerGeneration[layer] != 0 && stampedBlocks[layer] != GuestMemory::BlockWritten) {
            changed[layer] = false;
            keyFlipKept.fetch_add(1, std::memory_order_relaxed);
            if (direct) continue;
            stored[layer] = true;
            anyStored = true;
            continue;
        }
        if (changed[layer]) {
            pendingResults = true;
            if (blockUnits && droppable(layer)) {
                CaptureTrace::Log("drop-unit image=%llx unit=%u generation=%llu stamped=%d keys=%d", static_cast<unsigned long long>(descriptor.baseAddress), layer, static_cast<unsigned long long>(layerGeneration[layer]), layer < stampedBlocks.size() && stampedBlocks[layer] != 0, static_cast<int>(keys));
                layerPending[layer] = false;
                dropped = true;
                continue;
            }
        } else if (direct || selectedCpu) {
            continue;
        }
        stored[layer] = true;
        anyStored = true;
    }
    if (anyStored && !keysChanged && descriptor.dccAddress != 0 && (IsDccClear(uploadedKeys) || uploadedKeys == DccKeys::ClearSingle)) stored.assign(trackedLayers, true);
    if (pendingResults) {
        static std::atomic<int> reports{0};
        if (reports.fetch_add(1) < 8) std::fprintf(stderr, "[gpu] storage image 0x%llx: guest memory changed while GPU results were pending; keeping the CPU's blocks\n", static_cast<unsigned long long>(descriptor.baseAddress));
    }
    if (anyStored) {
        const auto previous = std::exchange(flushReason, "refresh");
        writeBackLayers(stored);
        flushReason = previous;
    } else if (dropped) {
        reconcilePending();
    }
    if (anyBorrowed) {
        NoteRefreshWork();
        uploadReason = "alias";
        borrowUnits(*alias, borrowed);
        for (std::uint32_t unit = 0; unit < trackedLayers; ++unit) {
            if (borrowed[unit]) {
                layerGeneration[unit] = currentFor(unit);
                forgetUnwatchHash(unit, 1);
            }
        }
        refreshGeneration();
    }
    if (std::any_of(changed.begin(), changed.end(), [](bool selected) { return selected; })) {
        NoteRefreshWork();
        uploadReason = keysChanged ? "keys" : cpuWrote ? "cpu" : generation == 0 ? "untracked" : flushed ? "flushed" : "store";
        upload(&changed);
    } else {
        uploadedKeys = keys;
    }
    return false;
}

}
}
}
}
}
