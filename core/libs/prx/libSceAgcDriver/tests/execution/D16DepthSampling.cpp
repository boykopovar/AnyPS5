#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineLibrary.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "DepthFastClearHarness.hpp"
#include "VulkanTestDevice.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using AgcDriver::Graphics::Buffer;
using AgcDriver::Graphics::ClearDepthSurfaces;
using AgcDriver::Graphics::CommandBatch;
using AgcDriver::Graphics::Context;
using AgcDriver::Graphics::DepthSurfaceStorage;
using AgcDriver::Graphics::DepthSurfaceStoreBack;
using AgcDriver::Graphics::DepthSurfaceTexture;
using AgcDriver::Graphics::DepthTarget;
using AgcDriver::Graphics::HostDepthFormat;
using AgcDriver::Graphics::Require;
using AgcDriver::Graphics::StorageTexture;
using DepthFastClearHarness::Covered;
using DepthFastClearHarness::DrawOptions;
using DepthFastClearHarness::Height;
using DepthFastClearHarness::Pixels;
using DepthFastClearHarness::Shaders;
using DepthFastClearHarness::Width;

constexpr std::uint32_t Format8Uint = 5;
constexpr std::uint32_t Format16Unorm = 7;
constexpr std::uint32_t Format16Uint = 11;
constexpr std::uint32_t TileDepth64KB = 0x18;
constexpr std::uint32_t Type2D = 9;
constexpr std::uint32_t TypeCube = 11;
constexpr std::uint32_t Type2DArray = 13;
constexpr std::size_t PlaneBytes = 65536;
constexpr std::size_t BlockBytes = 12 * PlaneBytes;
constexpr std::uint16_t DrawnBits = 0x8000u;
constexpr std::uint16_t GuestSliceBits = 0x1234u;
constexpr std::uint16_t StoredBits = 0x2468u;

struct Filter {
    VkPhysicalDevice real = VK_NULL_HANDLE;
    PFN_vkGetPhysicalDeviceFormatProperties format = nullptr;
    PFN_vkGetPhysicalDeviceImageFormatProperties image = nullptr;
    VkFormat hiddenA = VK_FORMAT_UNDEFINED;
    VkFormat hiddenB = VK_FORMAT_UNDEFINED;
    bool refuseAliasImages = false;
};

Filter filter;

void VKAPI_CALL FilteredFormat(VkPhysicalDevice, VkFormat format, VkFormatProperties* properties) {
    filter.format(filter.real, format, properties);
    if (format == filter.hiddenA || format == filter.hiddenB) *properties = {};
}

VkResult VKAPI_CALL ForwardImageFormat(VkPhysicalDevice, VkFormat format, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties* properties) {
    if (filter.refuseAliasImages && (format == VK_FORMAT_R16_SNORM || format == VK_FORMAT_R16_SINT || format == VK_FORMAT_R16_SFLOAT)) return VK_ERROR_FORMAT_NOT_SUPPORTED;
    return filter.image(filter.real, format, type, tiling, usage, flags, properties);
}

class GuestBlock {
public:
    GuestBlock() {
#ifdef _WIN32
        block = static_cast<std::uint8_t*>(GuestArena::GuestArenaAllocate_nid_postfix(BlockBytes, 65536));
        if (block != nullptr) {
            GuestArena::GuestArenaCommit_nid_postfix(block, BlockBytes, PAGE_READWRITE, BlockBytes);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
        }
#else
        constexpr std::uintptr_t alignment = 65536;
        void* mapped = mmap(nullptr, BlockBytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (mapped != MAP_FAILED) {
            const auto begin = reinterpret_cast<std::uintptr_t>(mapped);
            const auto aligned = (begin + alignment - 1) & ~(alignment - 1);
            if (aligned != begin) munmap(mapped, aligned - begin);
            if (aligned + BlockBytes != begin + BlockBytes + alignment) munmap(reinterpret_cast<void*>(aligned + BlockBytes), begin + alignment - aligned);
            block = reinterpret_cast<std::uint8_t*>(aligned);
            GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, BlockBytes);
        }
#endif
        Require(block != nullptr, "d16 fallback sampling: cannot allocate the guest block");
        std::memset(block, 0, BlockBytes);
        GuestAllocations::Mutation().Add(block, BlockBytes, true, true, true);
        AgcDriver::GuestMemory::CollectWritesUncached(reinterpret_cast<std::uintptr_t>(block), BlockBytes);
    }

    ~GuestBlock() {
        GuestAllocations::Mutation().Remove(block);
#ifdef _WIN32
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, BlockBytes);
        GuestArena::GuestArenaReset_nid_postfix(block, BlockBytes);
        GuestArena::GuestArenaRelease_nid_postfix(block, BlockBytes);
#else
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, BlockBytes);
        munmap(block, BlockBytes);
#endif
    }

    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;
    std::uint8_t* Plane(std::size_t index) const { return block + index * PlaneBytes; }

private:
    std::uint8_t* block = nullptr;
};

std::uint64_t Address(const std::uint8_t* plane) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(plane));
}

std::array<std::uint32_t, 8> StorageViewSized(std::uint64_t address, std::uint32_t format, std::uint32_t type, std::uint32_t lastArray, std::uint32_t swizzle, std::uint32_t width, std::uint32_t height) {
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (format << 20u) | (((width - 1u) & 3u) << 30u),
        ((width - 1u) >> 2u) | ((height - 1u) << 14u),
        0xfacu | (swizzle << 20u) | (type << 28u),
        lastArray,
        0u,
        0u,
        0u,
    };
}

std::array<std::uint32_t, 8> StorageView(std::uint64_t address, std::uint32_t format, std::uint32_t type, std::uint32_t lastArray = 0, std::uint32_t swizzle = TileDepth64KB) {
    return StorageViewSized(address, format, type, lastArray, swizzle, Width, Height);
}

std::uint32_t Draw(AgcDriver::VulkanDevice& device, const Context& context, const Shaders& shaders, std::uint64_t depth, std::uint64_t stencil, VkFormat format, const DrawOptions& options, std::uint8_t clearStencil = 0, float clearDepth = 1.0f, std::uint32_t slice = 0, VkExtent2D extent = {Width, Height}, std::span<std::byte> pixels = Pixels, std::optional<VkRect2D> scissor = std::nullopt) {
    std::fill(pixels.begin(), pixels.end(), std::byte{0});
    const std::array<AgcDriver::Graphics::CompiledShader, 2> compiled{{
        {ShaderRecompiler::ShaderStage::Vertex, &shaders.vertex, 0},
        {ShaderRecompiler::ShaderStage::Fragment, &shaders.pixel, PixelPushOffset(shaders.vertexPush, device.Target())}
    }};
    AgcDriver::Graphics::State state{};
    state.stages = {AgcDriver::Graphics::ShaderPath::Vertex, 0u, 32u, 32u, std::nullopt, std::nullopt};
    state.color = {reinterpret_cast<std::uintptr_t>(pixels.data()), extent, VK_FORMAT_R8G8B8A8_UNORM, pixels.size(), 0xe4u};
    state.colors = {state.color};
    state.hasColorTarget = true;
    state.depth = DepthTarget{depth, stencil, extent, format, clearDepth, clearStencil, 0u, false};
    state.depth->firstSlice = slice;
    state.depthTest = options.depthTest;
    state.depthWrite = options.depthWrite;
    state.depthCompare = options.depthCompare;
    state.stencilTest = options.stencilTest;
    state.stencilFront = options.stencil;
    state.stencilBack = options.stencil;
    state.renderExtent = extent;
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0.0f, static_cast<float>(extent.height), static_cast<float>(extent.width), -static_cast<float>(extent.height), options.minDepth, options.maxDepth};
    state.scissor = scissor.value_or(VkRect2D{{0, 0}, extent});
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    const AgcDriver::Pm4::DrawParameters draw{0u, 3u, 0u, 1u, 0u, false};
    AgcDriver::Graphics::Draw(context, state, draw, compiled);
    device.WaitIdle();
    std::uint32_t covered = 0;
    for (std::size_t pixel = 0; pixel < pixels.size() / 4u; ++pixel) {
        if (std::to_integer<std::uint8_t>(pixels[pixel * 4u]) == 255u) ++covered;
    }
    return covered;
}

constexpr std::uint32_t Threads = 32;
constexpr std::array<std::uint32_t, 3> LoadDims{1u, 3u, 5u};

std::array<std::uint32_t, 11> LoadCode(std::uint32_t dim) {
    return {
        0x7e02020d,
        0x7e04020c,
        0xf0001100u | (dim << 3u),
        0x00010300,
        0xbf8c3f70,
        0x34080082,
        0xe0701000,
        0x80000304,
        0xbf810000,
        0xbf810000,
        0xbf810000,
    };
}

alignas(256) std::array<std::uint32_t, 11> Load2D = LoadCode(LoadDims[0]);
alignas(256) std::array<std::uint32_t, 11> LoadCube = LoadCode(LoadDims[1]);
alignas(256) std::array<std::uint32_t, 11> LoadArray = LoadCode(LoadDims[2]);

struct Shape {
    std::uint32_t format;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t type;
    std::uint32_t lastArray;
    std::uint32_t swizzle;
    std::uint32_t baseArray = 0;
};

std::array<std::uint32_t, 8> TextureDescriptor(const void* data, const Shape& shape) {
    const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
    return {
        static_cast<std::uint32_t>(address >> 8u),
        static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (shape.format << 20u) | (((shape.width - 1u) & 3u) << 30u),
        ((shape.width - 1u) >> 2u) | ((shape.height - 1u) << 14u),
        0xfacu | (shape.swizzle << 20u) | (shape.type << 28u),
        shape.lastArray | (shape.baseArray << 16u),
        0u, 0u, 0u,
    };
}

void Load(AgcDriver::VulkanDevice& device, std::uint8_t* texels, const Shape& shape, std::uint32_t slice, std::span<std::uint32_t> output, std::uint32_t shaderType = 0xffffffffu) {
    const auto loadType = shaderType == 0xffffffffu ? shape.type : shaderType;
    const auto& code = loadType == TypeCube ? LoadCube : loadType == Type2DArray ? LoadArray : Load2D;
    std::vector<std::uint32_t> userData(16, 0u);
    const auto out = std::array<std::uint32_t, 4>{static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(output.data())), static_cast<std::uint32_t>((reinterpret_cast<std::uintptr_t>(output.data()) >> 32u) & 0xffffu), static_cast<std::uint32_t>(output.size_bytes()), 0x31016facu};
    const auto texture = TextureDescriptor(texels, shape);
    std::copy(out.begin(), out.end(), userData.begin());
    std::copy(texture.begin(), texture.end(), userData.begin() + 4);
    userData[12] = slice;
    userData[13] = 0u;
    const std::array<ShaderRecompiler::MemoryRegion, 1> memory{{{reinterpret_cast<std::uintptr_t>(code.data()), std::as_bytes(std::span(code))}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{Threads, 1, 1}, 0u, {false, false, false}, false, 1};
    ShaderRecompiler::RecompileRequest request{
        {ShaderRecompiler::ShaderStage::Compute, reinterpret_cast<std::uintptr_t>(code.data()), code, 0, {}},
        {32, 0, userData, compute, std::nullopt, std::nullopt, memory},
        device.Target(),
        {0, 0, 0, 128}
    };
    request.useCache = false;
    std::fill(output.begin(), output.end(), 0xdeadbeefu);
    const auto result = ShaderRecompiler::Recompile(request);
    device.Dispatch(result, 1, 1, 1, {}, reinterpret_cast<std::uintptr_t>(code.data()));
    device.WaitIdle();
}

void ExpectRaw(std::span<const std::uint32_t> output, std::uint32_t expected, const std::string& what) {
    for (std::uint32_t tid = 0; tid < Threads; ++tid) {
        char text[160];
        std::snprintf(text, sizeof(text), "%s: texel %u read 0x%08x, expected 0x%08x", what.c_str(), tid, output[tid], expected);
        Require(output[tid] == expected, text);
    }
}

void WriteTexture(const Context& context, const std::shared_ptr<StorageTexture>& texture, std::uint32_t width, std::uint32_t height, std::uint16_t value) {
    std::vector<std::uint16_t> values(static_cast<std::size_t>(width) * height, value);
    Buffer upload(context, values.size() * sizeof(std::uint16_t), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    std::memcpy(upload.Bytes().data(), values.data(), values.size() * sizeof(std::uint16_t));
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    CommandBatch batch(context);
    AgcDriver::Graphics::RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {width, height, 1};
    context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(batch.Handle(), upload.Handle(), texture->Image(), VK_IMAGE_LAYOUT_GENERAL, 1, &region);
    batch.SubmitAndWait();
}

void WriteTextureValues(const Context& context, const std::shared_ptr<StorageTexture>& texture, std::uint32_t width, std::uint32_t height, std::span<const std::uint16_t> values, std::uint32_t layer = 0) {
    Buffer upload(context, values.size() * sizeof(std::uint16_t), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    std::memcpy(upload.Bytes().data(), values.data(), values.size() * sizeof(std::uint16_t));
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    CommandBatch batch(context);
    AgcDriver::Graphics::RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, layer, 1};
    region.imageExtent = {width, height, 1};
    context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(batch.Handle(), upload.Handle(), texture->Image(), VK_IMAGE_LAYOUT_GENERAL, 1, &region);
    batch.SubmitAndWait();
}

void WriteTextureValues8(const Context& context, const std::shared_ptr<StorageTexture>& texture, std::uint32_t width, std::uint32_t height, std::span<const std::uint8_t> values) {
    Buffer upload(context, values.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    std::memcpy(upload.Bytes().data(), values.data(), values.size());
    std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
    CommandBatch batch(context);
    AgcDriver::Graphics::RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
    VkBufferImageCopy region{};
    region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    region.imageExtent = {width, height, 1};
    context.Function<PFN_vkCmdCopyBufferToImage>("vkCmdCopyBufferToImage")(batch.Handle(), upload.Handle(), texture->Image(), VK_IMAGE_LAYOUT_GENERAL, 1, &region);
    batch.SubmitAndWait();
}

std::vector<std::uint8_t> ReadStorage8(const Context& context, const std::shared_ptr<StorageTexture>& texture, std::uint32_t width, std::uint32_t height) {
    Buffer readback(context, static_cast<std::size_t>(width) * height, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        CommandBatch batch(context);
        AgcDriver::Graphics::RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        VkBufferImageCopy region{};
        region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        region.imageExtent = {width, height, 1};
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(batch.Handle(), texture->Image(), VK_IMAGE_LAYOUT_GENERAL, readback.Handle(), 1, &region);
        batch.SubmitAndWait();
    }
    readback.Invalidate();
    std::vector<std::uint8_t> values(static_cast<std::size_t>(width) * height);
    std::memcpy(values.data(), readback.Bytes().data(), values.size());
    return values;
}

std::vector<std::uint16_t> ReadStorage(const Context& context, const std::shared_ptr<StorageTexture>& texture, std::uint32_t width, std::uint32_t height, std::uint32_t layers = 1) {
    const auto bytes = static_cast<std::size_t>(width) * height * layers * 2u;
    Buffer readback(context, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    {
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        CommandBatch batch(context);
        AgcDriver::Graphics::RecordMemoryBarrier(context, batch.Handle(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        std::vector<VkBufferImageCopy> regions;
        for (std::uint32_t layer = 0; layer < layers; ++layer) {
            VkBufferImageCopy region{};
            region.bufferOffset = static_cast<VkDeviceSize>(width) * height * 2u * layer;
            region.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, layer, 1};
            region.imageExtent = {width, height, 1};
            regions.push_back(region);
        }
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(batch.Handle(), texture->Image(), VK_IMAGE_LAYOUT_GENERAL, readback.Handle(), static_cast<std::uint32_t>(regions.size()), regions.data());
        batch.SubmitAndWait();
    }
    readback.Invalidate();
    std::vector<std::uint16_t> values(static_cast<std::size_t>(width) * height * layers);
    std::memcpy(values.data(), readback.Bytes().data(), values.size() * sizeof(std::uint16_t));
    return values;
}

void Expect(const std::vector<std::uint16_t>& values, std::uint16_t expected, const std::string& what, std::size_t layer = 0) {
    const auto offset = layer * static_cast<std::size_t>(Width) * Height;
    std::size_t wrong = 0;
    std::uint16_t first = 0;
    for (std::size_t index = 0; index < static_cast<std::size_t>(Width) * Height; ++index) {
        if (values[offset + index] == expected) continue;
        if (wrong++ == 0) first = values[offset + index];
    }
    Require(wrong == 0, what + ": " + std::to_string(wrong) + " texels differ, first " + std::to_string(first) + " expected " + std::to_string(expected));
}

void RequireCubeSampling(AgcDriver::VulkanDevice& device, const Context& context, const Shaders& shaders, const char* name) {
    constexpr std::uint32_t size = 64;
    GuestBlock block;
    const auto base = Address(block.Plane(0));
    const auto fillLayer = [](std::uint64_t address, const AgcDriver::Graphics::GuestTextureResource& resource, std::uint32_t layer, std::uint16_t value) {
        const auto geometry = AgcDriver::Graphics::DescribeSurface(resource);
        auto* texels = reinterpret_cast<std::uint16_t*>(address + geometry.GuestLayerOffset(layer));
        std::fill(texels, texels + static_cast<std::size_t>(geometry.layerBytes) / sizeof(std::uint16_t), value);
        AgcDriver::GuestMemory::CollectWrites(address + geometry.GuestLayerOffset(layer), static_cast<std::size_t>(geometry.layerBytes));
    };
    const auto words = StorageViewSized(base, Format16Uint, TypeCube, 5u, 0u, size, size);
    const auto resource = AgcDriver::Graphics::DecodeTextureResource(words);
    const auto geometry = AgcDriver::Graphics::DescribeSurface(resource);
    Require(geometry.layers == 6u, std::string(name) + ": the cube surface has " + std::to_string(geometry.layers) + " layers");
    for (std::uint32_t face = 1u; face < 6u; ++face) fillLayer(base, resource, face, static_cast<std::uint16_t>(0x1000u + face));
    alignas(256) std::array<std::byte, static_cast<std::size_t>(size) * size * 4u> pixels{};
    Require(Draw(device, context, shaders, base, 0, VK_FORMAT_D16_UNORM_S8_UINT, {.depthTest = true, .depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}, 0, 0.5f, 0, {size, size}, pixels) == size * size, std::string(name) + ": the cube face did not cover the target");
    const auto texture = DepthSurfaceTexture(context, words, resource, {});
    Require(texture != nullptr, std::string(name) + ": the cube view was not served");
    std::array<std::uint32_t, Threads> values{};
    for (std::uint32_t face = 0; face < 6u; ++face) {
        Load(device, reinterpret_cast<std::uint8_t*>(base), Shape{Format16Uint, size, size, TypeCube, 5u, 0u}, face, values);
        ExpectRaw(values, face == 0u ? DrawnBits : static_cast<std::uint16_t>(0x1000u + face), std::string(name) + ": cube face " + std::to_string(face));
    }

    ClearDepthSurfaces(context.device);

    GuestBlock arrayBlock;
    const auto arrayBase = Address(arrayBlock.Plane(0));
    const auto arrayWords = StorageViewSized(arrayBase, Format16Uint, TypeCube, 11u, 0u, size, size);
    const auto arrayResource = AgcDriver::Graphics::DecodeTextureResource(arrayWords);
    const auto arrayGeometry = AgcDriver::Graphics::DescribeSurface(arrayResource);
    Require(arrayGeometry.layers == 12u, std::string(name) + ": the cube-array surface has " + std::to_string(arrayGeometry.layers) + " layers");
    for (std::uint32_t layer = 1u; layer < 12u; ++layer) fillLayer(arrayBase, arrayResource, layer, static_cast<std::uint16_t>(0x2000u + layer));
    Require(Draw(device, context, shaders, arrayBase, 0, VK_FORMAT_D16_UNORM_S8_UINT, {.depthTest = true, .depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}, 0, 0.5f, 0, {size, size}, pixels) == size * size, std::string(name) + ": the cube-array face did not cover the target");
    const auto arrayTexture = DepthSurfaceTexture(context, arrayWords, arrayResource, {});
    Require(arrayTexture != nullptr, std::string(name) + ": the cube-array view was not served");
    for (std::uint32_t layer = 0; layer < 12u; ++layer) {
        Load(device, reinterpret_cast<std::uint8_t*>(arrayBase), Shape{Format16Uint, size, size, TypeCube, 11u, 0u}, layer, values, Type2DArray);
        ExpectRaw(values, layer == 0u ? DrawnBits : static_cast<std::uint16_t>(0x2000u + layer), std::string(name) + ": cube-array layer " + std::to_string(layer));
    }
    ClearDepthSurfaces(context.device);
}

void RequireAliasRawBits(AgcDriver::VulkanDevice& device, const Context& context, const char* name) {
    constexpr std::uint32_t size = 256;
    GuestBlock block;
    const auto address = Address(block.Plane(0));
    const auto target = DepthTarget{address, 0, {size, size}, VK_FORMAT_D16_UNORM_S8_UINT, 1.0f, 0u};
    Require(DepthSurfaceView(context, target) != VK_NULL_HANDLE, std::string(name) + ": the alias depth surface was not created");
    std::vector<std::uint16_t> codes(static_cast<std::size_t>(size) * size);
    for (std::uint32_t code = 0; code < codes.size(); ++code) codes[code] = static_cast<std::uint16_t>(code);
    for (const std::uint32_t format : {Format16Unorm, 8u, Format16Uint, 12u, 13u}) {
        const auto words = StorageViewSized(address, format, Type2D, 0u, 0u, size, size);
        const auto resource = AgcDriver::Graphics::DecodeTextureResource(words);
        auto storage = DepthSurfaceStorage(context, words, resource, 0);
        Require(storage != nullptr, std::string(name) + ": the " + std::to_string(format) + " alias storage view was not served");
        Require(storage->Descriptor().format == format, std::string(name) + ": the " + std::to_string(format) + " alias lost its guest descriptor");
        if (format == 8u || format == 12u || format == 13u) {
            const auto typed = AgcDriver::Graphics::ResolveTextureFormat(format);
            Require(storage->ImageFormat() == typed || storage->ImageFormat() == VK_FORMAT_R16_UINT, std::string(name) + ": the " + std::to_string(format) + " alias has an unexpected backing");
            if (storage->ImageFormat() == VK_FORMAT_R16_UINT) {
                bool typedView = false;
                try {
                    typedView = storage->StorageView(0, false, false) != VK_NULL_HANDLE;
                } catch (const std::exception&) {
                }
                Require(!typedView, std::string(name) + ": a raw-backed numeric alias bound a typed shader view");
            } else {
                Require(storage->ImageFormat() == typed, std::string(name) + ": a numeric alias selected an unexpected typed backing");
                Require(storage->StorageView(0, false, false) != VK_NULL_HANDLE, std::string(name) + ": a typed depth alias did not expose its typed view");
            }
        } else {
            Require(storage->ImageFormat() == AgcDriver::Graphics::StorageFormatForGuest(context, format), std::string(name) + ": the " + std::to_string(format) + " alias changed its regular storage backing");
        }
        WriteTextureValues(context, storage, size, size, codes);
        device.WaitIdle();
        Require(DepthSurfaceStoreBack(context, *storage), std::string(name) + ": the " + std::to_string(format) + " alias store-back was not taken");
        device.WaitIdle();
        const auto exported = DepthSurfaceStorage(context, words, resource, 0);
        Require(exported != nullptr, std::string(name) + ": the " + std::to_string(format) + " alias export was not served");
        device.WaitIdle();
        const auto values = ReadStorage(context, exported, size, size);
        for (std::uint32_t code = 0; code < codes.size(); ++code) {
            if (values[code] == codes[code]) continue;
            char text[192];
            std::snprintf(text, sizeof(text), "%s: alias %u changed code %u to %u", name, format, code, values[code]);
            Require(false, text);
        }
    }
    ClearDepthSurfaces(context.device);
}

void RequireAliasImageQueryFallback(AgcDriver::VulkanDevice& device, const Context& base, const char* name) {
    constexpr std::uint32_t size = 256;
    Context context = base;
    context.imageFormatProperties = ForwardImageFormat;
    filter.refuseAliasImages = true;
    GuestBlock block;
    const auto address = Address(block.Plane(0));
    static_cast<void>(DepthSurfaceView(context, {address, 0, {size, size}, VK_FORMAT_D16_UNORM_S8_UINT, 1.0f, 0u}));
    const auto words = StorageViewSized(address, 8u, Type2D, 0u, 0u, size, size);
    const auto resource = AgcDriver::Graphics::DecodeTextureResource(words);
    auto storage = DepthSurfaceStorage(context, words, resource, 0);
    Require(storage != nullptr, std::string(name) + ": the image-query fallback storage was not served");
    Require(storage->Descriptor().format == 8u && storage->ImageFormat() == VK_FORMAT_R16_UINT, std::string(name) + ": an image-query refusal did not fall back to the raw transfer backing");
    bool typedThrew = false;
    try {
        static_cast<void>(storage->StorageView(0, false, false));
    } catch (const std::exception&) {
        typedThrew = true;
    }
    Require(typedThrew, std::string(name) + ": an image-query-refused alias exposed a typed view");
    std::vector<std::uint16_t> codes(static_cast<std::size_t>(size) * size);
    for (std::uint32_t code = 0; code < codes.size(); ++code) codes[code] = static_cast<std::uint16_t>(code);
    WriteTextureValues(context, storage, size, size, codes);
    device.WaitIdle();
    Require(AgcDriver::Graphics::DepthSurfaceStoreBack(context, *storage), std::string(name) + ": the image-query fallback store-back was not taken");
    device.WaitIdle();
    auto exported = DepthSurfaceStorage(context, words, resource, 0);
    Require(exported != nullptr, std::string(name) + ": the image-query fallback export was not served");
    device.WaitIdle();
    const auto values = ReadStorage(context, exported, size, size);
    for (std::uint32_t code = 0; code < codes.size(); ++code) Require(values[code] == codes[code], std::string(name) + ": the image-query fallback changed raw code " + std::to_string(code));
    filter.refuseAliasImages = false;
    ClearDepthSurfaces(context.device);
}

void RequireNoFormatListViews(AgcDriver::VulkanDevice& device, const Context& base, const char* name) {
    constexpr std::uint32_t size = 256;
    Context context = base;
    context.imageFormatList = false;
    GuestBlock block;
    const auto address = Address(block.Plane(0));
    static_cast<void>(DepthSurfaceView(context, {address, 0, {size, size}, VK_FORMAT_D16_UNORM_S8_UINT, 1.0f, 0u}));
    const auto words = StorageViewSized(address, 8u, Type2D, 0u, 0u, size, size);
    const auto resource = AgcDriver::Graphics::DecodeTextureResource(words);
    auto storage = DepthSurfaceStorage(context, words, resource, 0);
    Require(storage != nullptr, std::string(name) + ": the no-format-list storage was not served");
    Require(storage->StorageView(0, false, false) != VK_NULL_HANDLE, std::string(name) + ": the no-list typed view was not created");
    Require(storage->StorageView(0, false, true) != VK_NULL_HANDLE, std::string(name) + ": the no-list raw UINT view was not created");
    std::vector<std::uint16_t> codes(static_cast<std::size_t>(size) * size);
    for (std::uint32_t code = 0; code < codes.size(); ++code) codes[code] = static_cast<std::uint16_t>(code);
    WriteTextureValues(context, storage, size, size, codes);
    device.WaitIdle();
    Require(AgcDriver::Graphics::DepthSurfaceStoreBack(context, *storage), std::string(name) + ": the no-list store-back was not taken");
    device.WaitIdle();
    auto exported = DepthSurfaceStorage(context, words, resource, 0);
    Require(exported != nullptr, std::string(name) + ": the no-list export was not served");
    device.WaitIdle();
    const auto values = ReadStorage(context, exported, size, size);
    for (std::uint32_t code = 0; code < codes.size(); ++code) Require(values[code] == codes[code], std::string(name) + ": the no-list transfer changed raw code " + std::to_string(code));
    const auto lifetimeWords = StorageViewSized(Address(block.Plane(1)), 8u, Type2D, 0u, 0u, size, size);
    const auto lifetimeResource = AgcDriver::Graphics::DecodeTextureResource(lifetimeWords);
    auto lifetime = StorageTexture::CreateRaw16Depth(context, *context.detiler, lifetimeResource, 0);
    Require(lifetime != nullptr, std::string(name) + ": the no-list lifetime storage was not created");
    Require(lifetime->StorageView(0, false, false) != VK_NULL_HANDLE && lifetime->StorageView(0, false, true) != VK_NULL_HANDLE, std::string(name) + ": the no-list lifetime views were not created");
    lifetime.reset();
    ClearDepthSurfaces(context.device);
}

void RequirePendingPrecedence(AgcDriver::VulkanDevice& device, const Context& context, const Shaders& shaders, const char* name) {
    GuestBlock block;
    const auto depth = Address(block.Plane(0));
    const auto liveWords = StorageView(depth, Format16Unorm, Type2D);
    const auto liveResource = AgcDriver::Graphics::DecodeTextureResource(liveWords);
    std::vector<std::uint16_t> liveValues(static_cast<std::size_t>(Width) * Height, 0x1111u);
    std::memcpy(reinterpret_cast<void*>(depth), liveValues.data(), liveValues.size() * sizeof(std::uint16_t));
    AgcDriver::GuestMemory::CollectWrites(depth, liveValues.size() * sizeof(std::uint16_t));
    auto live = StorageTexture::Create(context, *context.detiler, liveResource, 0);
    Require(live != nullptr, std::string(name) + ": the live storage image was not created");
    const auto liveBytes = static_cast<std::size_t>(live->GuestBytes());
    live->MarkWholeWritten();
    StorageTexture::FlushPending(depth, liveBytes, nullptr, "test");
    device.WaitIdle();
    live->SetCached(true);
    Require(StorageTexture::FindLive(depth, liveBytes) != nullptr, std::string(name) + ": the flushed image is not live");
    const auto pendingWords = StorageView(depth, Format16Unorm, Type2D);
    const auto pendingResource = AgcDriver::Graphics::DecodeTextureResource(pendingWords);
    auto pending = StorageTexture::Create(context, *context.detiler, pendingResource, 0);
    Require(pending != nullptr, std::string(name) + ": the pending storage image was not created");
    std::vector<std::uint16_t> pendingValues(static_cast<std::size_t>(Width) * Height, 0x2222u);
    WriteTextureValues(context, pending, Width, Height, pendingValues);
    pending->MarkWholeWritten();
    Require(StorageTexture::FindPending(depth, liveBytes) == pending, std::string(name) + ": the pending image is not found before the import");
    DepthSurfaceView(context, {depth, 0, {Width, Height}, VK_FORMAT_D16_UNORM_S8_UINT, 1.0f, 0u});
    std::array<std::uint32_t, Threads> sampled{};
    Load(device, reinterpret_cast<std::uint8_t*>(depth), Shape{Format16Uint, Width, Height, Type2D, 0u, 0u}, 0, sampled);
    ExpectRaw(sampled, 0x2222u, std::string(name) + ": pending storage did not win over the live image");

    const auto liveOnly = Address(block.Plane(2));
    const auto liveOnlyWords = StorageView(liveOnly, Format16Unorm, Type2D);
    const auto liveOnlyResource = AgcDriver::Graphics::DecodeTextureResource(liveOnlyWords);
    auto liveOnlyImage = StorageTexture::Create(context, *context.detiler, liveOnlyResource, 0);
    Require(liveOnlyImage != nullptr, std::string(name) + ": the live-only image was not created");
    std::vector<std::uint16_t> liveOnlyValues(static_cast<std::size_t>(Width) * Height, 0x3333u);
    WriteTextureValues(context, liveOnlyImage, Width, Height, liveOnlyValues);
    liveOnlyImage->MarkWholeWritten();
    const auto liveOnlyBytes = static_cast<std::size_t>(liveOnlyImage->GuestBytes());
    StorageTexture::FlushPending(liveOnly, liveOnlyBytes, nullptr, "test");
    device.WaitIdle();
    liveOnlyImage->SetCached(true);
    Require(StorageTexture::FindPending(liveOnly, liveOnlyBytes) == nullptr && StorageTexture::FindLive(liveOnly, liveOnlyBytes) != nullptr, std::string(name) + ": the live-only image is not live without pending results");
    DepthSurfaceView(context, {liveOnly, 0, {Width, Height}, VK_FORMAT_D16_UNORM_S8_UINT, 1.0f, 0u});
    Load(device, reinterpret_cast<std::uint8_t*>(liveOnly), Shape{Format16Uint, Width, Height, Type2D, 0u, 0u}, 0, sampled);
    ExpectRaw(sampled, 0x3333u, std::string(name) + ": the live-only storage import");

    const auto arrayBase = Address(block.Plane(3));
    const auto arrayWords = StorageView(arrayBase, Format16Unorm, Type2DArray, 0u);
    const auto arrayResource = AgcDriver::Graphics::DecodeTextureResource(arrayWords);
    auto arrayImage = StorageTexture::Create(context, *context.detiler, arrayResource, 0);
    Require(arrayImage != nullptr, std::string(name) + ": the array storage image was not created");
    std::vector<std::uint16_t> arrayValues(static_cast<std::size_t>(Width) * Height, 0x4444u);
    WriteTextureValues(context, arrayImage, Width, Height, arrayValues, 0u);
    arrayImage->MarkWholeWritten();
    Require(StorageTexture::FindPending(arrayBase, static_cast<std::size_t>(arrayImage->GuestBytes())) != nullptr, std::string(name) + ": the array layer 0 is not pending");
    DepthSurfaceView(context, {arrayBase, 0, {Width, Height}, VK_FORMAT_D16_UNORM_S8_UINT, 1.0f, 0u});
    Load(device, reinterpret_cast<std::uint8_t*>(arrayBase), Shape{Format16Uint, Width, Height, Type2D, 0u, 0u}, 0, sampled);
    ExpectRaw(sampled, 0x4444u, std::string(name) + ": the array layer 0 storage import");

    const auto htileDepth = Address(block.Plane(4));
    const auto htileMetadata = Address(block.Plane(5));
    static_cast<void>(DepthSurfaceView(context, {htileDepth, 0, {Width, Height}, VK_FORMAT_D16_UNORM_S8_UINT, 1.0f, 0u, htileMetadata, false}));
    static_cast<void>(AgcDriver::Graphics::NoteHtileDepthClear(htileMetadata, AgcDriver::Graphics::HtileMetadataBytes({Width, Height})));
    const auto htileWords = StorageView(htileDepth, Format16Unorm, Type2D);
    const auto htileResource = AgcDriver::Graphics::DecodeTextureResource(htileWords);
    auto htileImage = StorageTexture::Create(context, *context.detiler, htileResource, 0);
    Require(htileImage != nullptr, std::string(name) + ": the htile negative storage image was not created");
    std::vector<std::uint16_t> htileValues(static_cast<std::size_t>(Width) * Height, 0x5555u);
    WriteTextureValues(context, htileImage, Width, Height, htileValues);
    htileImage->MarkWholeWritten();
    static_cast<void>(DepthSurfaceView(context, {htileDepth, 0, {Width, Height}, VK_FORMAT_D16_UNORM_S8_UINT, 1.0f, 0u, htileMetadata, false}));
    Load(device, reinterpret_cast<std::uint8_t*>(htileDepth), Shape{Format16Uint, Width, Height, Type2D, 0u, 0u}, 0, sampled);
    ExpectRaw(sampled, 0xffffu, std::string(name) + ": an HTILE surface imported a pending storage image");

    const auto fillDepth = Address(block.Plane(6));
    Require(Draw(device, context, shaders, fillDepth, 0, VK_FORMAT_D16_UNORM_S8_UINT, {.depthTest = true, .depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}, 0, 0.5f) == Covered, std::string(name) + ": the pending-clear depth write did not cover the target");
    AgcDriver::Graphics::NoteDepthMetadataFill(fillDepth, static_cast<std::size_t>(AgcDriver::Graphics::DepthSliceBytes({Width, Height}, 2u)), 0u);
    const auto fillWords = StorageView(fillDepth, Format16Unorm, Type2D);
    const auto fillResource = AgcDriver::Graphics::DecodeTextureResource(fillWords);
    auto fillImage = StorageTexture::Create(context, *context.detiler, fillResource, 0);
    Require(fillImage != nullptr, std::string(name) + ": the pending-clear storage image was not created");
    std::vector<std::uint16_t> fillValues(static_cast<std::size_t>(Width) * Height, 0x6666u);
    WriteTextureValues(context, fillImage, Width, Height, fillValues);
    fillImage->MarkWholeWritten();
    DepthSurfaceView(context, {fillDepth, 0, {Width, Height}, VK_FORMAT_D16_UNORM_S8_UINT, 1.0f, 0u});
    Load(device, reinterpret_cast<std::uint8_t*>(fillDepth), Shape{Format16Uint, Width, Height, Type2D, 0u, 0u}, 0, sampled);
    ExpectRaw(sampled, 0x0000u, std::string(name) + ": a pending depth clear imported a pending storage image");
    ClearDepthSurfaces(context.device);
}

void RequireRefusals(AgcDriver::VulkanDevice& device, const Context& context, const Shaders& shaders, const char* name) {
    GuestBlock block;
    const auto depth = Address(block.Plane(0));
    Require(Draw(device, context, shaders, depth, 0, VK_FORMAT_D16_UNORM_S8_UINT, {.depthTest = true, .depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}, 0, 0.5f) == Covered, std::string(name) + ": the refusal depth write did not cover the target");
    const auto words = StorageView(depth, Format16Unorm, Type2D, 0u, 0u);
    const auto resource = AgcDriver::Graphics::DecodeTextureResource(words);
    auto cropped = resource;
    cropped.width = Width / 2u;
    Require(DepthSurfaceTexture(context, words, cropped, {}) == nullptr, std::string(name) + ": a cropped fallback view was served");
    auto baseLevel = resource;
    baseLevel.baseLevel = 1u;
    baseLevel.mipCount = 2u;
    bool threw = false;
    bool served = false;
    try {
        served = DepthSurfaceTexture(context, words, baseLevel, {}) != nullptr;
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw || !served, std::string(name) + ": a non-zero base level was served");
    auto lastLevel = resource;
    lastLevel.lastLevel = 1u;
    lastLevel.mipCount = 2u;
    threw = false;
    try {
        static_cast<void>(DepthSurfaceTexture(context, words, lastLevel, {}));
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw, std::string(name) + ": a non-zero last level was served");
    auto cube = resource;
    cube.baseAddress = depth;
    cube.dimension = AgcDriver::Graphics::TextureDimension::kCube;
    cube.width = Width;
    cube.height = Height;
    cube.depthOrLastArray = 5u;
    cube.baseArray = 1u;
    threw = false;
    try {
        static_cast<void>(DepthSurfaceTexture(context, words, cube, {}));
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw, std::string(name) + ": a cube view with a non-zero base array was served");
    auto shortCube = cube;
    shortCube.baseArray = 0u;
    shortCube.depthOrLastArray = 10u;
    threw = false;
    try {
        static_cast<void>(DepthSurfaceTexture(context, words, shortCube, {}));
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw, std::string(name) + ": a cube view with eleven layers was served");
    const auto multisampled = DepthTarget{depth, 0, {Width, Height}, VK_FORMAT_D16_UNORM_S8_UINT, 1.0f, 0u, 0u, false, 2u};
    static_cast<void>(DepthSurfaceView(context, multisampled));
    threw = false;
    try {
        static_cast<void>(DepthSurfaceStorage(context, words, resource, 0));
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw, std::string(name) + ": multisampled depth storage was served");
    threw = false;
    try {
        static_cast<void>(AgcDriver::Graphics::HostDepthBiasConstant(context, VK_FORMAT_D16_UNORM_S8_UINT, 2.0f));
    } catch (const std::exception&) {
        threw = true;
    }
    Require(threw == (HostDepthFormat(context, VK_FORMAT_D16_UNORM_S8_UINT) == VK_FORMAT_D32_SFLOAT_S8_UINT), std::string(name) + ": the constant depth bias refusal differs from the host format");
    for (const std::uint32_t format : {8u, 12u, 13u}) {
        const auto sampledWords = StorageView(depth, format, Type2D);
        const auto sampledResource = AgcDriver::Graphics::DecodeTextureResource(sampledWords);
        bool served = false;
        threw = false;
        try {
            served = DepthSurfaceTexture(context, sampledWords, sampledResource, {}) != nullptr;
        } catch (const std::exception&) {
            threw = true;
        }
        Require(threw || !served, std::string(name) + ": a numeric R16 alias was served as a raw depth sample");
    }
    ClearDepthSurfaces(context.device);
}

void RequireNonuniformStencil(AgcDriver::VulkanDevice& device, const Context& context, const Shaders& shaders, const char* name) {
    GuestBlock block;
    const auto depth = Address(block.Plane(0));
    const auto stencil = Address(block.Plane(1));
    constexpr VkStencilOpState replace11{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0xffu, 0xffu, 0x11u};
    constexpr VkStencilOpState replace22{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0xffu, 0xffu, 0x22u};
    Require(Draw(device, context, shaders, depth, stencil, VK_FORMAT_D16_UNORM_S8_UINT, {.depthTest = false, .depthWrite = true, .stencilTest = true, .stencil = replace11}, 0, 0.5f) == Covered, std::string(name) + ": the nonuniform stencil base did not cover the target");
    Require(Draw(device, context, shaders, depth, stencil, VK_FORMAT_D16_UNORM_S8_UINT, {.depthTest = false, .depthWrite = false, .stencilTest = true, .stencil = replace22}, 0, 0.5f, 0, {Width, Height}, Pixels, VkRect2D{{0, 0}, {Width / 2u, Height}}) == Covered / 2u, std::string(name) + ": the nonuniform stencil half did not cover half the target");
    const auto stencilWords = StorageView(stencil, Format8Uint, Type2D);
    const auto stencilResource = AgcDriver::Graphics::DecodeTextureResource(stencilWords);
    auto exported = DepthSurfaceStorage(context, stencilWords, stencilResource, 0);
    Require(exported != nullptr, std::string(name) + ": the nonuniform stencil export was not served");
    device.WaitIdle();
    const auto first = ReadStorage8(context, exported, Width, Height);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto expected = x < Width / 2u ? 0x22u : 0x11u;
            Require(first[y * Width + x] == expected, std::string(name) + ": stencil export at " + std::to_string(x) + ", " + std::to_string(y) + " is " + std::to_string(first[y * Width + x]));
        }
    }
    std::vector<std::uint8_t> pattern(static_cast<std::size_t>(Width) * Height);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) pattern[y * Width + x] = x < Width / 2u ? 0x33u : 0x44u;
    }
    WriteTextureValues8(context, exported, Width, Height, pattern);
    device.WaitIdle();
    Require(DepthSurfaceStoreBack(context, *exported), std::string(name) + ": the nonuniform stencil store-back was not taken");
    device.WaitIdle();
    auto stored = DepthSurfaceStorage(context, stencilWords, stencilResource, 0);
    Require(stored != nullptr, std::string(name) + ": the nonuniform stencil re-export was not served");
    device.WaitIdle();
    const auto second = ReadStorage8(context, stored, Width, Height);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto expected = x < Width / 2u ? 0x33u : 0x44u;
            Require(second[y * Width + x] == expected, std::string(name) + ": stencil store-back at " + std::to_string(x) + ", " + std::to_string(y) + " is " + std::to_string(second[y * Width + x]));
        }
    }

    const auto importDepth = Address(block.Plane(2));
    const auto importStencil = Address(block.Plane(3));
    const auto importWords = StorageView(importStencil, Format8Uint, Type2D);
    const auto importResource = AgcDriver::Graphics::DecodeTextureResource(importWords);
    auto importImage = StorageTexture::Create(context, *context.detiler, importResource, 0);
    Require(importImage != nullptr, std::string(name) + ": the stencil import image was not created");
    std::vector<std::uint8_t> importPattern(static_cast<std::size_t>(Width) * Height);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) importPattern[y * Width + x] = x < Width / 2u ? 0x55u : 0x66u;
    }
    WriteTextureValues8(context, importImage, Width, Height, importPattern);
    importImage->MarkWholeWritten();
    DepthSurfaceView(context, {importDepth, importStencil, {Width, Height}, VK_FORMAT_D16_UNORM_S8_UINT, 1.0f, 0u});
    auto imported = DepthSurfaceStorage(context, importWords, importResource, 0);
    Require(imported != nullptr, std::string(name) + ": the stencil import export was not served");
    device.WaitIdle();
    const auto third = ReadStorage8(context, imported, Width, Height);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto expected = x < Width / 2u ? 0x55u : 0x66u;
            Require(third[y * Width + x] == expected, std::string(name) + ": stencil import at " + std::to_string(x) + ", " + std::to_string(y) + " is " + std::to_string(third[y * Width + x]));
        }
    }
    ClearDepthSurfaces(context.device);
}

void RequireStoreOverwrite(AgcDriver::VulkanDevice& device, const Context& context, const Shaders& shaders, const char* name) {
    GuestBlock block;
    const auto depth = Address(block.Plane(0));
    const auto color = Address(block.Plane(1));
    constexpr std::array<std::uint32_t, 15> kernel{0xd7460004u, 0x0401060cu, 0xf4280204u, 0xfa000000u, 0xd7460005u, 0x0405060du, 0x7e0c020eu, 0xbf8cc07fu, 0x7e000208u, 0x7e020209u, 0x7e04020au, 0x7e06020bu, 0xf0200f28u, 0x00000004u, 0xbf810000u};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{8, 8, 1}, 0u, {true, true, true}, false, 2u, {0, 0, 0}};
    const std::array<std::uint32_t, 3> groups{Width / 8u, Height / 8u, 1u};
    const auto sampleWords = StorageView(depth, Format16Uint, Type2D, 0u, 9u);
    const auto sampleResource = AgcDriver::Graphics::DecodeTextureResource(sampleWords);
    const auto sampleExpected = [&](std::uint16_t expected, const std::string& what) {
        const auto texture = DepthSurfaceTexture(context, sampleWords, sampleResource, {});
        Require(texture != nullptr, std::string(name) + ": the overwrite depth plane was not served");
        std::array<std::uint32_t, Threads> sampled{};
        Load(device, reinterpret_cast<std::uint8_t*>(depth), Shape{Format16Uint, Width, Height, Type2D, 0u, 9u}, 0, sampled);
        ExpectRaw(sampled, expected, what);
    };
    struct Case {
        std::uint32_t format;
        std::uint32_t colorBits;
        std::uint16_t expected;
    };
    const std::array<Case, 5> cases{{
        {Format16Unorm, 0x00000000u, 0x0000u},
        {Format16Unorm, 0x80000000u, 0x0000u},
        {Format16Unorm, 0x3f800000u, 0xffffu},
        {Format16Uint, 0x00000000u, 0x0000u},
        {Format16Uint, 0x0000ffffu, 0xffffu},
    }};
    for (const auto& entry : cases) {
        const auto words = StorageView(depth, entry.format, Type2D);
        const auto resource = AgcDriver::Graphics::DecodeTextureResource(words);
        std::array<std::uint32_t, 12> userData{};
        std::copy(words.begin(), words.end(), userData.begin());
        userData[8] = static_cast<std::uint32_t>(color);
        userData[9] = static_cast<std::uint32_t>(color >> 32u) & 0xffffu;
        userData[10] = 16u;
        userData[11] = 0x31016facu;
        std::memcpy(reinterpret_cast<void*>(color), &entry.colorBits, sizeof(entry.colorBits));
        AgcDriver::GuestMemory::CollectWrites(color, sizeof(entry.colorBits));
        const auto clear = AgcDriver::Graphics::MatchStorageImageClear(kernel, userData, compute, groups);
        Require(clear.has_value(), std::string(name) + ": the clear kernel was not matched for the overwrite endpoint");
        for (const int mode : {0, 1}) {
            Require(Draw(device, context, shaders, depth, 0, VK_FORMAT_D16_UNORM_S8_UINT, {.depthTest = true, .depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}, 0, 0.5f) == Covered, std::string(name) + ": the overwrite producer did not cover the target");
            auto storage = DepthSurfaceStorage(context, words, resource, 0);
            Require(storage != nullptr, std::string(name) + ": the overwrite storage view was not served");
            std::vector<std::uint16_t> values(static_cast<std::size_t>(Width) * Height, 0x4000u);
            WriteTextureValues(context, storage, Width, Height, values);
            device.WaitIdle();
            const auto before = AgcDriver::Graphics::DepthClearCounts();
            AgcDriver::Graphics::SetDepthPlaneClearForTest(mode != 0);
            std::optional<AgcDriver::Graphics::DepthPlaneClear> scope;
            if (mode != 0) scope.emplace(*clear);
            Require(DepthSurfaceStoreBack(context, *storage), std::string(name) + ": the overwrite store-back was not taken");
            scope.reset();
            AgcDriver::Graphics::SetDepthPlaneClearForTest(false);
            device.WaitIdle();
            const auto now = AgcDriver::Graphics::DepthClearCounts();
            if (mode == 0) {
                Require(now.clears == before.clears, std::string(name) + ": a disabled depth plane clear took a clear");
                sampleExpected(0x4000u, std::string(name) + ": depth plane copy mode");
            } else {
                Require(now.taken == before.taken + 1u && now.clears == before.clears + 1u, std::string(name) + ": the depth plane clear did not take its overwrite path");
                sampleExpected(entry.expected, std::string(name) + ": depth plane overwrite endpoint");
            }
        }
        {
            auto partial = *clear;
            partial.width = Width / 2u;
            Require(Draw(device, context, shaders, depth, 0, VK_FORMAT_D16_UNORM_S8_UINT, {.depthTest = true, .depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}, 0, 0.5f) == Covered, std::string(name) + ": the partial overwrite producer did not cover the target");
            auto storage = DepthSurfaceStorage(context, words, resource, 0);
            std::vector<std::uint16_t> values(static_cast<std::size_t>(Width) * Height, 0x4000u);
            WriteTextureValues(context, storage, Width, Height, values);
            device.WaitIdle();
            const auto before = AgcDriver::Graphics::DepthClearCounts();
            AgcDriver::Graphics::SetDepthPlaneClearForTest(true);
            std::optional<AgcDriver::Graphics::DepthPlaneClear> scope;
            scope.emplace(partial);
            Require(DepthSurfaceStoreBack(context, *storage), std::string(name) + ": the partial overwrite store-back was not taken");
            scope.reset();
            AgcDriver::Graphics::SetDepthPlaneClearForTest(false);
            device.WaitIdle();
            const auto now = AgcDriver::Graphics::DepthClearCounts();
            Require(now.taken == before.taken + 1u && now.clears == before.clears, std::string(name) + ": a partial depth plane clear took the whole-plane overwrite");
            sampleExpected(0x4000u, std::string(name) + ": a partial depth plane clear changed the copy");
        }
    }
    ClearDepthSurfaces(context.device);
}

void RequireLayeredStencilPreservation(AgcDriver::VulkanDevice& device, const Context& context, const Shaders& shaders, const char* name) {
    GuestBlock block;
    const auto depth0 = Address(block.Plane(0));
    const auto stencil0 = Address(block.Plane(2));
    const auto stencil1 = Address(block.Plane(3));
    const auto words = StorageView(depth0, Format16Unorm, Type2DArray, 1u, 0);
    const auto resource = AgcDriver::Graphics::DecodeTextureResource(words);
    const auto geometry = AgcDriver::Graphics::DescribeSurface(resource);
    const auto depth1 = depth0 + geometry.GuestLayerOffset(1);
    const auto drawStencil = [&](std::uint64_t depth, std::uint64_t stencil, float value, std::uint8_t left, std::uint8_t right) {
        const VkStencilOpState leftOp{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0xffu, 0xffu, left};
        const VkStencilOpState rightOp{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_REPLACE, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_ALWAYS, 0xffu, 0xffu, right};
        Require(Draw(device, context, shaders, depth, stencil, VK_FORMAT_D16_UNORM_S8_UINT, {.depthTest = true, .depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS, .stencilTest = true, .stencil = leftOp, .minDepth = value, .maxDepth = value}, 0, value) == Covered, std::string(name) + ": the layered depth/stencil base did not cover the target");
        Require(Draw(device, context, shaders, depth, stencil, VK_FORMAT_D16_UNORM_S8_UINT, {.depthTest = true, .depthWrite = false, .depthCompare = VK_COMPARE_OP_ALWAYS, .stencilTest = true, .stencil = rightOp}, 0, value, 0, {Width, Height}, Pixels, VkRect2D{{0, 0}, {Width / 2u, Height}}) == Covered / 2u, std::string(name) + ": the layered stencil half did not cover half the target");
    };
    drawStencil(depth0, stencil0, 0.5f, 0x11u, 0x22u);
    drawStencil(depth1, stencil1, 0.25f, 0x33u, 0x44u);
    auto layered = DepthSurfaceStorage(context, words, resource, 0);
    Require(layered != nullptr && layered->ImageLayers() == 2u, std::string(name) + ": the layered depth storage was not served");
    device.WaitIdle();
    const auto before = ReadStorage(context, layered, Width, Height, 2);
    Expect(before, 0x8000u, std::string(name) + ": layered depth layer 0", 0);
    Expect(before, 0x4000u, std::string(name) + ": layered depth layer 1", 1);
    std::vector<std::uint16_t> layer0(static_cast<std::size_t>(Width) * Height, 0x2000u);
    std::vector<std::uint16_t> layer1(static_cast<std::size_t>(Width) * Height, 0x6000u);
    WriteTextureValues(context, layered, Width, Height, layer0, 0);
    WriteTextureValues(context, layered, Width, Height, layer1, 1);
    device.WaitIdle();
    Require(DepthSurfaceStoreBack(context, *layered), std::string(name) + ": the layered depth store-back was not taken");
    device.WaitIdle();
    auto afterStorage = DepthSurfaceStorage(context, words, resource, 0);
    Require(afterStorage != nullptr, std::string(name) + ": the layered depth re-export was not served");
    device.WaitIdle();
    const auto after = ReadStorage(context, afterStorage, Width, Height, 2);
    Expect(after, 0x2000u, std::string(name) + ": layered depth store-back layer 0", 0);
    Expect(after, 0x6000u, std::string(name) + ": layered depth store-back layer 1", 1);
    for (const std::uint32_t format : {7u, 8u, 11u, 12u, 13u}) {
        const auto aliasWords = StorageView(depth0, format, Type2DArray, 1u, 0u);
        const auto aliasResource = AgcDriver::Graphics::DecodeTextureResource(aliasWords);
        auto aliasStorage = DepthSurfaceStorage(context, aliasWords, aliasResource, 0);
        Require(aliasStorage != nullptr && aliasStorage->ImageLayers() == 2u, std::string(name) + ": the layered alias storage was not served");
        device.WaitIdle();
        const auto aliasBefore = ReadStorage(context, aliasStorage, Width, Height, 2);
        const auto expectedBefore0 = format == 7u ? 0x2000u : 0x3000u;
        const auto expectedBefore1 = format == 7u ? 0x6000u : 0x7000u;
        Expect(aliasBefore, expectedBefore0, std::string(name) + ": layered alias export layer 0", 0);
        Expect(aliasBefore, expectedBefore1, std::string(name) + ": layered alias export layer 1", 1);
        std::vector<std::uint16_t> alias0(static_cast<std::size_t>(Width) * Height, 0x3000u);
        std::vector<std::uint16_t> alias1(static_cast<std::size_t>(Width) * Height, 0x7000u);
        WriteTextureValues(context, aliasStorage, Width, Height, alias0, 0);
        WriteTextureValues(context, aliasStorage, Width, Height, alias1, 1);
        device.WaitIdle();
        Require(DepthSurfaceStoreBack(context, *aliasStorage), std::string(name) + ": the layered alias store-back was not taken");
        device.WaitIdle();
        auto aliasAfter = DepthSurfaceStorage(context, aliasWords, aliasResource, 0);
        Require(aliasAfter != nullptr, std::string(name) + ": the layered alias re-export was not served");
        device.WaitIdle();
        const auto exported = ReadStorage(context, aliasAfter, Width, Height, 2);
        Expect(exported, 0x3000u, std::string(name) + ": layered alias store-back layer 0", 0);
        Expect(exported, 0x7000u, std::string(name) + ": layered alias store-back layer 1", 1);
    }
    const auto stencilWords0 = StorageView(stencil0, Format8Uint, Type2D);
    const auto stencilWords1 = StorageView(stencil1, Format8Uint, Type2D);
    const auto stencilResource0 = AgcDriver::Graphics::DecodeTextureResource(stencilWords0);
    const auto stencilResource1 = AgcDriver::Graphics::DecodeTextureResource(stencilWords1);
    auto stencilView0 = DepthSurfaceStorage(context, stencilWords0, stencilResource0, 0);
    auto stencilView1 = DepthSurfaceStorage(context, stencilWords1, stencilResource1, 0);
    Require(stencilView0 != nullptr && stencilView1 != nullptr, std::string(name) + ": a layered stencil plane was not served");
    device.WaitIdle();
    const auto stencilBytes0 = ReadStorage8(context, stencilView0, Width, Height);
    const auto stencilBytes1 = ReadStorage8(context, stencilView1, Width, Height);
    for (std::uint32_t y = 0; y < Height; ++y) {
        for (std::uint32_t x = 0; x < Width; ++x) {
            const auto expected0 = x < Width / 2u ? 0x22u : 0x11u;
            const auto expected1 = x < Width / 2u ? 0x44u : 0x33u;
            Require(stencilBytes0[y * Width + x] == expected0, std::string(name) + ": layered stencil layer 0 changed at " + std::to_string(x) + ", " + std::to_string(y));
            Require(stencilBytes1[y * Width + x] == expected1, std::string(name) + ": layered stencil layer 1 changed at " + std::to_string(x) + ", " + std::to_string(y));
        }
    }
    ClearDepthSurfaces(context.device);
}

void RequireAliasCoveringStoreBack(AgcDriver::VulkanDevice& device, const Context& context, const Shaders& shaders, const char* name) {
    GuestBlock block;
    const auto depth = Address(block.Plane(0));
    const auto sampleWords = StorageView(depth, Format16Uint, Type2D, 0u, 9u);
    const auto sampleResource = AgcDriver::Graphics::DecodeTextureResource(sampleWords);
    for (const std::uint32_t format : {Format16Unorm, 8u, Format16Uint, 12u, 13u}) {
        Require(Draw(device, context, shaders, depth, 0, VK_FORMAT_D16_UNORM_S8_UINT, {.depthTest = true, .depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}, 0, 0.5f) == Covered, std::string(name) + ": the covering producer did not cover the target");
        const auto words = StorageView(depth, format, Type2D);
        const auto resource = AgcDriver::Graphics::DecodeTextureResource(words);
        const bool rawAlias = format == 8u || format == 12u || format == 13u;
        auto storage = rawAlias ? StorageTexture::CreateRaw16Depth(context, *context.detiler, resource, 0) : StorageTexture::Create(context, *context.detiler, resource, 0);
        Require(storage != nullptr, std::string(name) + ": the covering storage view was not created");
        std::vector<std::uint16_t> values(static_cast<std::size_t>(Width) * Height, 0x2000u);
        WriteTextureValues(context, storage, Width, Height, values);
        device.WaitIdle();
        Require(AgcDriver::Graphics::DepthSurfaceStoreBack(context, *storage), std::string(name) + ": the covering alias store-back was not taken");
        device.WaitIdle();
        const auto texture = DepthSurfaceTexture(context, sampleWords, sampleResource, {});
        Require(texture != nullptr, std::string(name) + ": the covering sample view was not served");
        std::array<std::uint32_t, Threads> sampled{};
        Load(device, reinterpret_cast<std::uint8_t*>(depth), Shape{Format16Uint, Width, Height, Type2D, 0u, 9u}, 0, sampled);
        ExpectRaw(sampled, 0x2000u, std::string(name) + ": covering store-back format " + std::to_string(format));
    }
    ClearDepthSurfaces(context.device);
}

void RequireDepthBitsArrayGuest(AgcDriver::VulkanDevice& device, const Context& context, const Shaders& shaders, const char* name) {
    GuestBlock block;
    const auto address = Address(block.Plane(0));
    const auto words = StorageView(address, Format16Uint, Type2DArray, 1u, 24u);
    const auto resource = AgcDriver::Graphics::DecodeTextureResource(words);
    const auto geometry = AgcDriver::Graphics::DescribeSurface(resource);
    const auto secondSlice = address + geometry.GuestLayerOffset(1);
    std::vector<std::uint16_t> values(static_cast<std::size_t>(geometry.layerBytes) / sizeof(std::uint16_t), 0x4000u);
    std::memcpy(reinterpret_cast<void*>(secondSlice), values.data(), values.size() * sizeof(std::uint16_t));
    AgcDriver::GuestMemory::CollectWrites(secondSlice, values.size() * sizeof(std::uint16_t));
    Require(Draw(device, context, shaders, address, 0, VK_FORMAT_D16_UNORM_S8_UINT, {.depthTest = true, .depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}, 0, 0.5f) == Covered, std::string(name) + ": the depth-bits array producer did not cover the target");
    const auto texture = DepthSurfaceTexture(context, words, resource, {});
    Require(texture != nullptr, std::string(name) + ": the depth-bits array view was not served");
    std::array<std::uint32_t, Threads> sampled{};
    Load(device, reinterpret_cast<std::uint8_t*>(address), Shape{Format16Uint, Width, Height, Type2DArray, 1u, 24u}, 0, sampled);
    ExpectRaw(sampled, DrawnBits, std::string(name) + ": depth-bits array resident slice");
    Load(device, reinterpret_cast<std::uint8_t*>(address), Shape{Format16Uint, Width, Height, Type2DArray, 1u, 24u}, 1, sampled);
    ExpectRaw(sampled, 0x4000u, std::string(name) + ": depth-bits array guest slice");
    ClearDepthSurfaces(context.device);
}

void RequireDepthBitsCubeGuest(AgcDriver::VulkanDevice& device, const Context& context, const Shaders& shaders, const char* name) {
    constexpr std::uint32_t size = 64;
    GuestBlock block;
    const auto base = Address(block.Plane(0));
    const auto words = StorageViewSized(base, Format16Uint, TypeCube, 5u, 24u, size, size);
    const auto resource = AgcDriver::Graphics::DecodeTextureResource(words);
    const auto geometry = AgcDriver::Graphics::DescribeSurface(resource);
    for (std::uint32_t face = 1u; face < 6u; ++face) {
        auto* texels = reinterpret_cast<std::uint16_t*>(base + geometry.GuestLayerOffset(face));
        std::fill(texels, texels + static_cast<std::size_t>(geometry.layerBytes) / sizeof(std::uint16_t), static_cast<std::uint16_t>(0x1000u + face));
        AgcDriver::GuestMemory::CollectWrites(base + geometry.GuestLayerOffset(face), static_cast<std::size_t>(geometry.layerBytes));
    }
    alignas(256) std::array<std::byte, static_cast<std::size_t>(size) * size * 4u> pixels{};
    Require(Draw(device, context, shaders, base, 0, VK_FORMAT_D16_UNORM_S8_UINT, {.depthTest = true, .depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}, 0, 0.5f, 0, {size, size}, pixels) == size * size, std::string(name) + ": the depth-bits cube producer did not cover the target");
    const auto texture = DepthSurfaceTexture(context, words, resource, {});
    Require(texture != nullptr, std::string(name) + ": the depth-bits cube view was not served");
    std::array<std::uint32_t, Threads> values{};
    for (std::uint32_t face = 0; face < 6u; ++face) {
        Load(device, reinterpret_cast<std::uint8_t*>(base), Shape{Format16Uint, size, size, TypeCube, 5u, 24u}, face, values);
        ExpectRaw(values, face == 0u ? DrawnBits : static_cast<std::uint16_t>(0x1000u + face), std::string(name) + ": depth-bits cube face " + std::to_string(face));
    }
    ClearDepthSurfaces(context.device);
}

void RequireStorageAdmission(const Context& context, const char* name) {
    GuestBlock block;
    const auto address = Address(block.Plane(0));
    const auto check = [&](std::uint32_t guestFormat, VkFormat format, const char* label) {
        VkFormatProperties properties{};
        context.formatProperties(context.physical, format, &properties);
        const bool storage = (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT) != 0;
        Require(AgcDriver::Graphics::StorageFormatAvailable(context, guestFormat) == storage, std::string(name) + ": " + label + " storage admission changed");
        Require(AgcDriver::Graphics::HostStorageFormat(context, format) == (storage ? format : VK_FORMAT_UNDEFINED), std::string(name) + ": " + label + " host storage format changed");
        const auto words = StorageView(address, guestFormat, Type2D);
        const auto resource = AgcDriver::Graphics::DecodeTextureResource(words);
        if (storage) {
            const auto created = StorageTexture::Create(context, *context.detiler, resource, 0);
            Require(created != nullptr && created->ImageFormat() == AgcDriver::Graphics::HostStorageFormat(context, format), std::string(name) + ": " + label + " regular storage backing changed");
            return;
        }
        bool threw = false;
        try {
            static_cast<void>(StorageTexture::Create(context, *context.detiler, resource, 0));
        } catch (const std::exception&) {
            threw = true;
        }
        Require(threw, std::string(name) + ": " + label + " unsupported regular storage creation was admitted");
    };
    check(8u, VK_FORMAT_R16_SNORM, "R16_SNORM");
    check(12u, VK_FORMAT_R16_SINT, "R16_SINT");
    check(13u, VK_FORMAT_R16_SFLOAT, "R16_SFLOAT");
    check(56u, VK_FORMAT_R8G8B8A8_UNORM, "R8G8B8A8_UNORM");
}

void RequireNativeControl(AgcDriver::VulkanDevice& device, const Context& context, const Shaders& shaders) {
    if (HostDepthFormat(context, VK_FORMAT_D16_UNORM_S8_UINT) != VK_FORMAT_D16_UNORM_S8_UINT) {
        std::puts("native D16_UNORM_S8_UINT is not supported on this device: the native control is skipped");
        return;
    }
    GuestBlock block;
    const auto depth = Address(block.Plane(0));
    Require(Draw(device, context, shaders, depth, 0, VK_FORMAT_D16_UNORM_S8_UINT, {.depthTest = true, .depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}, 0, 0.5f) == Covered, "native D16: the depth write did not cover the target");
    const auto words = StorageView(depth, Format16Unorm, Type2D, 0u, 0u);
    const auto resource = AgcDriver::Graphics::DecodeTextureResource(words);
    const auto texture = DepthSurfaceTexture(context, words, resource, {});
    Require(texture != nullptr, "native D16: the depth plane was not served");
    std::array<std::uint32_t, Threads> values{};
    Load(device, reinterpret_cast<std::uint8_t*>(depth), Shape{Format16Unorm, Width, Height, Type2D, 0u, 0u}, 0, values);
    std::uint32_t drawnFloatBits = 0u;
    const float drawnFloat = static_cast<float>(DrawnBits) / 65535.0f;
    std::memcpy(&drawnFloatBits, &drawnFloat, sizeof(drawnFloatBits));
    ExpectRaw(values, drawnFloatBits, "native D16: the sampled depth plane holds the drawn normalized value");
    Load(device, reinterpret_cast<std::uint8_t*>(depth), Shape{Format16Uint, Width, Height, Type2D, 0u, 0u}, 0, values);
    ExpectRaw(values, DrawnBits, "native D16: the depth plane holds the drawn code");
    ClearDepthSurfaces(context.device);
}

void RunMode(AgcDriver::VulkanDevice& device, const Context& base, const Shaders& shaders, VkFormat expectedHost, VkFormat hiddenA, VkFormat hiddenB, const char* name) {
    VkFormatProperties properties{};
    base.formatProperties(base.physical, expectedHost, &properties);
    if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0u) {
        std::printf("%s: host format %d is not supported on this device: the mode is skipped\n", name, static_cast<int>(expectedHost));
        return;
    }
    AgcDriver::Graphics::ClearCachedPipelines(base.device);
    AgcDriver::Graphics::ClearPipelineLibraries(base.device);
    ClearDepthSurfaces(base.device);
    Context context = base;
    context.physical = reinterpret_cast<VkPhysicalDevice>(static_cast<std::uintptr_t>(hiddenA ^ hiddenB ^ 0x44u));
    context.formatProperties = FilteredFormat;
    context.imageFormatProperties = ForwardImageFormat;
    context.memoryProperties2 = nullptr;
    filter.real = base.physical;
    filter.format = base.formatProperties;
    filter.image = base.imageFormatProperties;
    filter.hiddenA = hiddenA;
    filter.hiddenB = hiddenB;
    const auto host = HostDepthFormat(context, VK_FORMAT_D16_UNORM_S8_UINT);
    Require(host == expectedHost, std::string(name) + ": selected host format " + std::to_string(static_cast<int>(host)) + ", expected " + std::to_string(static_cast<int>(expectedHost)));
    std::printf("=== %s: D16_UNORM_S8_UINT as %d ===\n", name, static_cast<int>(host));

    GuestBlock block;
    const auto depth = Address(block.Plane(0));
    const auto stencil = Address(block.Plane(1));
    std::vector<std::uint16_t> staleDepth(static_cast<std::size_t>(AgcDriver::Graphics::DepthSliceBytes({Width, Height}, 2u)) / sizeof(std::uint16_t), 0x1234u);
    std::memcpy(reinterpret_cast<void*>(depth), staleDepth.data(), staleDepth.size() * sizeof(std::uint16_t));
    AgcDriver::GuestMemory::CollectWrites(depth, staleDepth.size() * sizeof(std::uint16_t));
    Require(Draw(device, context, shaders, depth, stencil, VK_FORMAT_D16_UNORM_S8_UINT, {.depthTest = true, .depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}, 0, 0.5f) == Covered, std::string(name) + ": the depth write did not cover the target");

    const auto integerWords = StorageView(depth, Format16Uint, Type2D, 0, 9);
    const auto integerResource = AgcDriver::Graphics::DecodeTextureResource(integerWords);
    const auto integer = DepthSurfaceTexture(context, integerWords, integerResource, {});
    Require(integer != nullptr, std::string(name) + ": the R16_UINT view was not served");
    std::array<std::uint32_t, Threads> integerValues{};
    Load(device, reinterpret_cast<std::uint8_t*>(depth), Shape{Format16Uint, Width, Height, Type2D, 0, 9}, 0, integerValues);
    ExpectRaw(integerValues, DrawnBits, std::string(name) + ": R16_UINT single slice");
    const auto depthBitsWords = StorageView(depth, Format16Uint, Type2D, 0, 24);
    const auto depthBitsResource = AgcDriver::Graphics::DecodeTextureResource(depthBitsWords);
    Require(DepthSurfaceTexture(context, depthBitsWords, depthBitsResource, {}) != nullptr, std::string(name) + ": the depth-bits single-slice view was not served");
    std::array<std::uint32_t, Threads> depthBitsValues{};
    Load(device, reinterpret_cast<std::uint8_t*>(depth), Shape{Format16Uint, Width, Height, Type2D, 0, 24}, 0, depthBitsValues);
    ExpectRaw(depthBitsValues, DrawnBits, std::string(name) + ": depth-bits single slice");

    const auto arrayDepth = Address(block.Plane(2));
    const auto arrayWords = StorageView(arrayDepth, Format16Uint, Type2DArray, 1u, 0);
    const auto arrayResource = AgcDriver::Graphics::DecodeTextureResource(arrayWords);
    const auto arrayGeometry = AgcDriver::Graphics::DescribeSurface(arrayResource);
    const auto secondSlice = arrayDepth + arrayGeometry.GuestLayerOffset(1);
    std::vector<std::uint16_t> secondSliceValues(static_cast<std::size_t>(arrayGeometry.layerBytes) / sizeof(std::uint16_t), 0x4000u);
    std::memcpy(reinterpret_cast<void*>(secondSlice), secondSliceValues.data(), secondSliceValues.size() * sizeof(std::uint16_t));
    AgcDriver::GuestMemory::CollectWrites(secondSlice, secondSliceValues.size() * sizeof(std::uint16_t));
    Require(Draw(device, context, shaders, arrayDepth, 0, VK_FORMAT_D16_UNORM_S8_UINT, {.depthTest = true, .depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}, 0, 0.5f) == Covered, std::string(name) + ": the array depth write did not cover the target");
    std::array<std::uint32_t, Threads> arrayValues{};
    Load(device, reinterpret_cast<std::uint8_t*>(arrayDepth), Shape{Format16Uint, Width, Height, Type2DArray, 1u, 0}, 0, arrayValues);
    ExpectRaw(arrayValues, DrawnBits, std::string(name) + ": array slice 0");
    Load(device, reinterpret_cast<std::uint8_t*>(arrayDepth), Shape{Format16Uint, Width, Height, Type2DArray, 1u, 0}, 1, arrayValues);
    ExpectRaw(arrayValues, 0x4000u, std::string(name) + ": array slice 1");


    const auto layeredWords = StorageView(arrayDepth, Format16Unorm, Type2DArray, 1u, 0);
    const auto layeredResource = AgcDriver::Graphics::DecodeTextureResource(layeredWords);
    auto layered = DepthSurfaceStorage(context, layeredWords, layeredResource, 0);
    Require(layered != nullptr, std::string(name) + ": the layered storage view was not served");
    device.WaitIdle();
    const auto layeredValues = ReadStorage(base, layered, Width, Height, 2);
    Expect(layeredValues, DrawnBits, std::string(name) + ": layered storage layer 0", 0);
    Expect(layeredValues, 0x4000u, std::string(name) + ": layered storage layer 1", 1);
    WriteTexture(base, layered, Width, Height, StoredBits);
    device.WaitIdle();
    Require(DepthSurfaceStoreBack(context, *layered), std::string(name) + ": the layered depth storage write-back was not taken");
    device.WaitIdle();
    const auto afterWords = StorageView(arrayDepth, Format16Uint, Type2DArray, 1u, 0);
    const auto afterResource = AgcDriver::Graphics::DecodeTextureResource(afterWords);
    static_cast<void>(afterResource);
    std::array<std::uint32_t, Threads> afterValues{};
    Load(device, reinterpret_cast<std::uint8_t*>(arrayDepth), Shape{Format16Uint, Width, Height, Type2DArray, 1u, 0}, 0, afterValues);
    ExpectRaw(afterValues, StoredBits, std::string(name) + ": layered depth write-back layer 0");
    Load(device, reinterpret_cast<std::uint8_t*>(arrayDepth), Shape{Format16Uint, Width, Height, Type2DArray, 1u, 0}, 1, afterValues);
    ExpectRaw(afterValues, 0x4000u, std::string(name) + ": layered depth write-back layer 1");

    const auto guestDepth = Address(block.Plane(5));
    const auto guestWords = StorageView(guestDepth, Format16Uint, Type2DArray, 1u, 0);
    const auto guestResource = AgcDriver::Graphics::DecodeTextureResource(guestWords);
    const auto guestGeometry = AgcDriver::Graphics::DescribeSurface(guestResource);
    const auto guestSliceAddress = guestDepth + guestGeometry.GuestLayerOffset(1);
    std::vector<std::uint16_t> guestValues(static_cast<std::size_t>(guestGeometry.layerBytes) / 2u, GuestSliceBits);
    Require(!AgcDriver::GuestMemory::CommittedRanges(guestSliceAddress, static_cast<std::size_t>(guestGeometry.layerBytes)).empty(), "the mixed array guest layer is not committed");
    std::memcpy(reinterpret_cast<void*>(guestSliceAddress), guestValues.data(), guestValues.size() * sizeof(std::uint16_t));
    AgcDriver::GuestMemory::CollectWrites(guestSliceAddress, guestValues.size() * sizeof(std::uint16_t));
    Require(Draw(device, context, shaders, guestDepth, 0, VK_FORMAT_D16_UNORM_S8_UINT, {.depthTest = true, .depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}, 0, 0.5f) == Covered, std::string(name) + ": the mixed array depth write did not cover the target");
    std::array<std::uint32_t, Threads> mixedValues{};
    Load(device, reinterpret_cast<std::uint8_t*>(guestDepth), Shape{Format16Uint, Width, Height, Type2DArray, 1u, 0}, 0, mixedValues);
    ExpectRaw(mixedValues, DrawnBits, std::string(name) + ": mixed array resident slice");
    Load(device, reinterpret_cast<std::uint8_t*>(guestDepth), Shape{Format16Uint, Width, Height, Type2DArray, 1u, 0}, 1, mixedValues);
    ExpectRaw(mixedValues, GuestSliceBits, std::string(name) + ": mixed array guest slice");

    const auto pendingDepth = Address(block.Plane(7));
    const auto pendingWords = StorageView(pendingDepth, Format16Unorm, Type2D, 0, 24);
    const auto pendingResource = AgcDriver::Graphics::DecodeTextureResource(pendingWords);
    const auto pendingGeometry = AgcDriver::Graphics::DescribeSurface(pendingResource);
    std::vector<std::uint16_t> pendingValues(static_cast<std::size_t>(pendingGeometry.guestBytes) / 2u, 0x5a5au);
    std::memcpy(reinterpret_cast<void*>(pendingDepth), pendingValues.data(), pendingValues.size() * sizeof(std::uint16_t));
    AgcDriver::GuestMemory::CollectWrites(pendingDepth, pendingValues.size() * sizeof(std::uint16_t));
    auto pendingStorage = StorageTexture::Create(context, *context.detiler, pendingResource, 0);
    Require(pendingStorage != nullptr, std::string(name) + ": the pending storage image was not created");
    pendingStorage->MarkWholeWritten();
    Require(StorageTexture::FindPending(pendingDepth, static_cast<std::size_t>(pendingStorage->GuestBytes())) != nullptr || StorageTexture::FindLive(pendingDepth, static_cast<std::size_t>(pendingStorage->GuestBytes())) != nullptr, std::string(name) + ": the storage image is neither pending nor live");
    DepthSurfaceView(context, {pendingDepth, Address(block.Plane(8)), {Width, Height}, VK_FORMAT_D16_UNORM_S8_UINT, 1.0f, 0x3});
    std::array<std::uint32_t, Threads> pendingSampled{};
    Load(device, reinterpret_cast<std::uint8_t*>(pendingDepth), Shape{Format16Uint, Width, Height, Type2D, 0, 9}, 0, pendingSampled);
    ExpectRaw(pendingSampled, 0x5a5au, std::string(name) + ": pending storage import");
    constexpr VkStencilOpState pendingStencil{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_EQUAL, 0xffu, 0u, 0x3u};
    Require(Draw(device, context, shaders, pendingDepth, Address(block.Plane(8)), VK_FORMAT_D16_UNORM_S8_UINT, {.depthTest = false, .depthWrite = false, .stencilTest = true, .stencil = pendingStencil}, 0x3u, 1.0f) == Covered, std::string(name) + ": pending storage import changed stencil");

    const auto baseDepth = Address(block.Plane(9));
    const auto baseWords = StorageView(baseDepth, Format16Uint, Type2DArray, 2u, 0);
    const auto baseResource = AgcDriver::Graphics::DecodeTextureResource(baseWords);
    const auto baseGeometry = AgcDriver::Graphics::DescribeSurface(baseResource);
    for (const auto layer : {1u, 2u}) {
        const auto address = baseDepth + baseGeometry.GuestLayerOffset(layer);
        std::vector<std::uint16_t> values(static_cast<std::size_t>(baseGeometry.layerBytes) / 2u, static_cast<std::uint16_t>(0x1111u * layer));
        std::memcpy(reinterpret_cast<void*>(address), values.data(), values.size() * sizeof(std::uint16_t));
        AgcDriver::GuestMemory::CollectWrites(address, values.size() * sizeof(std::uint16_t));
    }
    Require(Draw(device, context, shaders, baseDepth, 0, VK_FORMAT_D16_UNORM_S8_UINT, {.depthTest = true, .depthWrite = true, .depthCompare = VK_COMPARE_OP_ALWAYS}, 0, 0.5f) == Covered, std::string(name) + ": the base-array depth write did not cover the target");
    std::array<std::uint32_t, Threads> baseValues{};
    Load(device, reinterpret_cast<std::uint8_t*>(baseDepth), Shape{Format16Uint, Width, Height, Type2DArray, 2u, 0, 1u}, 0, baseValues);
    ExpectRaw(baseValues, 0x1111u, std::string(name) + ": base-array slice 1");
    Load(device, reinterpret_cast<std::uint8_t*>(baseDepth), Shape{Format16Uint, Width, Height, Type2DArray, 2u, 0, 1u}, 1, baseValues);
    ExpectRaw(baseValues, 0x2222u, std::string(name) + ": base-array slice 2");

    constexpr VkStencilOpState equalStencil{VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_STENCIL_OP_KEEP, VK_COMPARE_OP_EQUAL, 0xffu, 0u, 0x5au};
    const auto stencilDepth = Address(block.Plane(3));
    const auto stencilPlane = Address(block.Plane(4));
    Require(Draw(device, context, shaders, stencilDepth, stencilPlane, VK_FORMAT_D16_UNORM_S8_UINT, {.depthTest = false, .depthWrite = false, .stencilTest = true, .stencil = equalStencil}, 0x5au, 0.5f) == Covered, std::string(name) + ": the stencil clear did not survive the first draw");
    const auto storageWords = StorageView(stencilDepth, Format16Unorm, Type2D, 0, 9);
    const auto storageResource = AgcDriver::Graphics::DecodeTextureResource(storageWords);
    auto storage = DepthSurfaceStorage(context, storageWords, storageResource, 0);
    Require(storage != nullptr, std::string(name) + ": the storage view of the fallback depth plane was not served");
    device.WaitIdle();
    Require(Draw(device, context, shaders, stencilDepth, stencilPlane, VK_FORMAT_D16_UNORM_S8_UINT, {.depthTest = false, .depthWrite = false, .stencilTest = true, .stencil = equalStencil}, 0x5au, 0.5f) == Covered, std::string(name) + ": the depth storage copy changed stencil");
    const auto storageValues = ReadStorage(base, storage, Width, Height);
    Expect(storageValues, DrawnBits, std::string(name) + ": depth storage export");
    WriteTexture(base, storage, Width, Height, StoredBits);
    device.WaitIdle();
    Require(DepthSurfaceStoreBack(context, *storage), std::string(name) + ": the depth storage write-back was not taken");
    device.WaitIdle();
    std::array<std::uint32_t, Threads> storedValues{};
    Load(device, reinterpret_cast<std::uint8_t*>(stencilDepth), Shape{Format16Uint, Width, Height, Type2D, 0, 9}, 0, storedValues);
    ExpectRaw(storedValues, StoredBits, std::string(name) + ": depth storage write-back");
    RequireNonuniformStencil(device, context, shaders, name);
    RequireCubeSampling(device, context, shaders, name);
    RequireAliasRawBits(device, context, name);
    RequireAliasImageQueryFallback(device, context, name);
    RequireNoFormatListViews(device, base, name);
    RequirePendingPrecedence(device, context, shaders, name);
    RequireAliasCoveringStoreBack(device, context, shaders, name);
    RequireDepthBitsArrayGuest(device, context, shaders, name);
    RequireDepthBitsCubeGuest(device, context, shaders, name);
    RequireLayeredStencilPreservation(device, context, shaders, name);
    RequireStoreOverwrite(device, context, shaders, name);
    RequireRefusals(device, context, shaders, name);
    ClearDepthSurfaces(base.device);
}

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        const auto base = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
        Require(base.formatProperties != nullptr && base.imageFormatProperties != nullptr, "the device exposes no format queries");
        const auto shaders = DepthFastClearHarness::Compile(*device);
        RequireStorageAdmission(base, "device");
        RequireNativeControl(*device, base, shaders);
        RunMode(*device, base, shaders, VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_UNDEFINED, "ForcedD24S8Fallback");
        RunMode(*device, base, shaders, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D16_UNORM_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT, "ForcedD32S8Fallback");
        device->WaitIdle();
        std::puts("d16 fallback depth sampling tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
