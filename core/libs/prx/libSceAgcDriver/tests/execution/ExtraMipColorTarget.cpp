#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureSwizzleEquations.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Recompiler.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <optional>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using namespace AgcDriver::Graphics;

struct GuestBlock {
    std::vector<std::byte> storage;
    std::byte* registered;
    std::size_t bytes;
    GuestBlock(std::size_t surfaceBytes) : storage(surfaceBytes + 4u * 65536u, std::byte{0xa5}), bytes(((surfaceBytes + 65535u) & ~std::size_t{65535u}) + 2u * 65536u) {
        registered = reinterpret_cast<std::byte*>((reinterpret_cast<std::uintptr_t>(storage.data()) + 65535u) & ~std::uintptr_t{65535u});
        GuestAllocations::Mutation().Add(registered, bytes, true, true, true);
    }
    ~GuestBlock() { GuestAllocations::Mutation().Remove(registered); }
    GuestBlock(const GuestBlock&) = delete;
    GuestBlock& operator=(const GuestBlock&) = delete;
    std::byte* Surface() const { return registered + 65536u; }
};

std::size_t TexelOffset(const GuestTextureResource& descriptor, const SurfaceGeometry& geometry, ColorTileMode mode, std::uint32_t level, std::uint32_t layer) {
    const auto& mip = geometry.mips.at(level);
    std::uint64_t offset = geometry.GuestLayerOffset(layer) + mip.tiledOffset;
    if (descriptor.tileMode == TextureTileMode::kLinear) return static_cast<std::size_t>(offset);
    const auto bytes = BytesPerElement(descriptor.format);
    ColorTargetLayout layout(mip.tail ? 64u : mip.width, mip.tail ? 64u : mip.height, mode, bytes);
    auto within = layout.Offset(mip.tail ? mip.tailX : 0u, mip.tail ? mip.tailY : 0u);
    if (ColorTileModeIsXor(mode)) {
        const auto equation = std::ranges::find_if(kTextureSwizzleEquations, [&](const auto& item) { return item.swizzleMode == XorSwizzleMode(descriptor.tileMode) && item.elementBytes == bytes; });
        Require(equation != std::end(kTextureSwizzleEquations), "extra mip fixture has no array-slice address equation");
        for (std::uint32_t bit = 0; bit < 16u; ++bit) within ^= static_cast<std::size_t>(std::popcount(equation->bits[bit] & (layer << 24u)) & 1u) << bit;
    }
    return static_cast<std::size_t>(offset) + within;
}

void ClearAttachment(const Context& context, VkImageView view, VkFormat format, const std::array<std::uint32_t, 4>& value) {
    struct Objects {
        const Context& context;
        VkRenderPass pass = VK_NULL_HANDLE;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        ~Objects() {
            if (framebuffer) context.Function<PFN_vkDestroyFramebuffer>("vkDestroyFramebuffer")(context.device, framebuffer, nullptr);
            if (pass) context.Function<PFN_vkDestroyRenderPass>("vkDestroyRenderPass")(context.device, pass, nullptr);
        }
    } objects{context};
    VkAttachmentDescription attachment{};
    attachment.format = format;
    attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachment.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
    attachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
    const VkAttachmentReference color{0u, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color;
    const std::array<VkSubpassDependency, 2> dependencies{{
        {VK_SUBPASS_EXTERNAL, 0u, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, 0u},
        {0u, VK_SUBPASS_EXTERNAL, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, 0u}
    }};
    VkRenderPassCreateInfo passInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    passInfo.attachmentCount = 1;
    passInfo.pAttachments = &attachment;
    passInfo.subpassCount = 1;
    passInfo.pSubpasses = &subpass;
    passInfo.dependencyCount = static_cast<std::uint32_t>(dependencies.size());
    passInfo.pDependencies = dependencies.data();
    Check(context.Function<PFN_vkCreateRenderPass>("vkCreateRenderPass")(context.device, &passInfo, nullptr, &objects.pass), "create extra-mip render pass");
    VkFramebufferCreateInfo framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebufferInfo.renderPass = objects.pass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = &view;
    framebufferInfo.width = 1;
    framebufferInfo.height = 1;
    framebufferInfo.layers = 1;
    Check(context.Function<PFN_vkCreateFramebuffer>("vkCreateFramebuffer")(context.device, &framebufferInfo, nullptr, &objects.framebuffer), "create extra-mip framebuffer");
    VkClearValue clear{};
    std::copy(value.begin(), value.end(), clear.color.uint32);
    CommandBatch batch(context);
    VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    begin.renderPass = objects.pass;
    begin.framebuffer = objects.framebuffer;
    begin.renderArea.extent = {1u, 1u};
    begin.clearValueCount = 1;
    begin.pClearValues = &clear;
    context.Function<PFN_vkCmdBeginRenderPass>("vkCmdBeginRenderPass")(batch.Handle(), &begin, VK_SUBPASS_CONTENTS_INLINE);
    context.Function<PFN_vkCmdEndRenderPass>("vkCmdEndRenderPass")(batch.Handle());
    batch.SubmitAndWait();
}

void CheckCase(AgcDriver::VulkanDevice& device, std::uint32_t width, std::uint32_t height, std::uint32_t levels, std::uint32_t format, TextureTileMode tile, ColorTileMode colorTile, std::uint32_t layers, bool singlePass) {
    auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    context.singlePassStorage = context.singlePassStorage && singlePass;
    GuestTextureResource descriptor{};
    descriptor.width = width;
    descriptor.height = height;
    descriptor.mipCount = levels;
    descriptor.lastLevel = levels - 1u;
    descriptor.format = format;
    descriptor.tileMode = tile;
    descriptor.dimension = layers == 1 ? TextureDimension::k2D : TextureDimension::k2DArray;
    descriptor.depthOrLastArray = layers - 1u;
    const auto geometry = DescribeSurface(descriptor);
    Require(geometry.imageLevels < levels, "extra-mip fixture needs levels past the physical chain");
    GuestBlock block(static_cast<std::size_t>(geometry.guestBytes));
    descriptor.baseAddress = reinterpret_cast<std::uintptr_t>(block.Surface());
    std::vector<std::byte> expected(block.registered, block.registered + block.bytes);
    const std::array<std::uint32_t, 3> selected{geometry.imageLevels, geometry.imageLevels - 1u, levels - 1u};
    const auto layer = layers - 1u;
    std::array<std::size_t, 3> offsets{};
    for (std::size_t index = 0; index < selected.size(); ++index) {
        const auto mip = selected[index];
        Require(geometry.mips[mip].width == 1u && geometry.mips[mip].height == 1u, "extra-mip fixture's selected level is not 1x1");
        offsets[index] = TexelOffset(descriptor, geometry, colorTile, mip, layer);
        Require(offsets[index] + BytesPerElement(format) <= geometry.guestBytes, "extra-mip fixture texel exceeds its guest surface");
    }
    Require(offsets[0] != offsets[1] && offsets[0] != offsets[2] && offsets[1] != offsets[2], "extra-mip fixture siblings alias the same guest texel");
    std::printf("extra-mip geometry: %ux%u levels=%u physical=%u tile=%u format=%u layer=%u offsets=0x%zx/0x%zx/0x%zx\n", width, height, levels, geometry.imageLevels, static_cast<unsigned>(tile), format, layer, offsets[0], offsets[1], offsets[2]);
    std::shared_ptr<StorageTexture> texture;
    const auto singlePassBefore = StorageTexture::SinglePassMoves();
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        texture = StorageTexture::Create(context, *context.detiler, descriptor, 0u);
        if (auto* recorder = Recorder::Active()) recorder->Sync();
    }
    for (std::size_t index = 0; index < selected.size(); ++index) {
        const std::array<std::uint32_t, 4> value{0x71350000u + selected[index], 0x20304050u + selected[index], 0xabcdef01u, 0x98765432u};
        {
            std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
            ClearAttachment(context, texture->AttachmentView(StorageFormatForGuest(context, format), selected[index], 0u, 1u, layer), StorageFormatForGuest(context, format), value);
            texture->MarkDirty();
            texture->WriteBack();
            if (auto* recorder = Recorder::Active()) recorder->Sync();
        }
        std::memcpy(expected.data() + 65536u + offsets[index], value.data(), BytesPerElement(format));
        Require(std::equal(expected.begin(), expected.end(), block.registered), "extra-mip rendering changed a sibling, another layer, padding or an allocation boundary");
    }
    const std::array<std::uint32_t, 4> cpuValue{0x10293847u, 0x56473829u, 0xabcdef01u, 0x12345678u};
    AgcDriver::GuestMemory::Write(descriptor.baseAddress + offsets[0], std::as_bytes(std::span(cpuValue)).first(BytesPerElement(format)));
    std::memcpy(expected.data() + 65536u + offsets[0], cpuValue.data(), BytesPerElement(format));
    const std::array<std::uint32_t, 4> laterValue{0x78654321u, 0xfedcba98u, 0x83920123u, 0x45901234u};
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        static_cast<void>(texture->Refresh());
        if (auto* recorder = Recorder::Active()) recorder->Sync();
        ClearAttachment(context, texture->AttachmentView(StorageFormatForGuest(context, format), selected[2], 0u, 1u, layer), StorageFormatForGuest(context, format), laterValue);
        texture->MarkDirty();
        texture->WriteBack();
        if (auto* recorder = Recorder::Active()) recorder->Sync();
    }
    std::memcpy(expected.data() + 65536u + offsets[2], laterValue.data(), BytesPerElement(format));
    Require(std::equal(expected.begin(), expected.end(), block.registered), "extra-mip refresh or the next writeback lost a CPU-written sibling or changed guest bounds");
    const auto singlePassMoves = StorageTexture::SinglePassMoves() - singlePassBefore;
    Require(singlePass || singlePassMoves == 0u, "the linear-copy fixture used the disabled direct-image path");
    std::printf("extra-mip transfer profile: direct-enabled=%u direct-image-moves=%llu\n", singlePass ? 1u : 0u, static_cast<unsigned long long>(singlePassMoves));
    {
        std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
        texture.reset();
    }
    std::puts("extra-mip color attachment writes and full guest bytes passed");
}

void DrawTarget(const Context& context, AgcDriver::VulkanDevice& device, const ColorTarget& color) {
    using ShaderRecompiler::ShaderStage;
    alignas(256) static constexpr std::array<std::uint32_t, 6> vertexCode{0xe0382000u, 0x80000005u, 0xbf8c3f70u, 0xf80008cfu, 0x03020100u, 0xbf810000u};
    alignas(256) static constexpr std::array<std::uint32_t, 4> pixelCode{0x7e0e02f2u, 0xf800180fu, 0x07070707u, 0xbf810000u};
    alignas(256) static constexpr std::array<std::array<float, 4>, 3> triangle{{{-1.0f, -1.0f, 0.5f, 1.0f}, {3.0f, -1.0f, 0.5f, 1.0f}, {-1.0f, 3.0f, 0.5f, 1.0f}}};
    const auto address = reinterpret_cast<std::uintptr_t>(triangle.data());
    const std::vector<std::uint32_t> vertexData{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (16u << 16u), 3u, 0x01016facu};
    const std::array<ShaderRecompiler::MemoryRegion, 1> vertexMemory{{{reinterpret_cast<std::uintptr_t>(vertexCode.data()), std::as_bytes(std::span(vertexCode))}}};
    ShaderRecompiler::RecompileRequest vertex{
        {ShaderStage::Vertex, reinterpret_cast<std::uintptr_t>(vertexCode.data()), vertexCode, 0, {}},
        {64u, 0, vertexData, std::nullopt, std::nullopt, ShaderRecompiler::ShaderVertexStageInfo{}, vertexMemory},
        device.Target(), {0, 0, 0, 64u}
    };
    vertex.useCache = false;
    const auto compiledVertex = ShaderRecompiler::Recompile(vertex);
    const auto push = static_cast<std::uint32_t>(compiledVertex.pushConstants.size());
    State state{};
    state.stages = {ShaderPath::Vertex, 0u, 64u, 64u, std::nullopt, std::nullopt};
    state.color = color;
    state.colors = {color};
    state.hasColorTarget = true;
    state.renderExtent = color.extent;
    state.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    state.viewport = {0, static_cast<float>(color.extent.height), static_cast<float>(color.extent.width), -static_cast<float>(color.extent.height), 0, 1};
    state.scissor = {{0, 0}, color.extent};
    state.cullMode = VK_CULL_MODE_NONE;
    state.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    state.blend.colorWriteMask = 15;
    state.blends = {state.blend};
    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.targetOutputMode[0] = 9;
    pixel.targetExportMapping = ExportMappings(state);
    const std::array<ShaderRecompiler::MemoryRegion, 1> pixelMemory{{{reinterpret_cast<std::uintptr_t>(pixelCode.data()), std::as_bytes(std::span(pixelCode))}}};
    ShaderRecompiler::RecompileRequest fragment{
        {ShaderStage::Fragment, reinterpret_cast<std::uintptr_t>(pixelCode.data()), pixelCode, 0, {}},
        {64u, 0, {}, std::nullopt, pixel, std::nullopt, pixelMemory},
        device.Target(), PixelPushLayout(push, device.Target())
    };
    fragment.useCache = false;
    const auto compiledPixel = ShaderRecompiler::Recompile(fragment);
    const std::array<CompiledShader, 2> shaders{{{ShaderStage::Vertex, &compiledVertex, 0}, {ShaderStage::Fragment, &compiledPixel, PixelPushOffset(push, device.Target())}}};
    const AgcDriver::Pm4::DrawParameters draw{0, 3, 0, 1, 0, false};
    Draw(context, state, draw, shaders);
}

void CheckDraw(AgcDriver::VulkanDevice& device, bool recorded, std::uint32_t layers) {
    auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    GuestTextureResource descriptor{};
    descriptor.width = 32;
    descriptor.height = 16;
    descriptor.mipCount = 8;
    descriptor.lastLevel = 7;
    descriptor.format = 22;
    descriptor.tileMode = TextureTileMode::kR64KBX;
    descriptor.dimension = layers == 1 ? TextureDimension::k2D : TextureDimension::k2DArray;
    descriptor.depthOrLastArray = layers - 1u;
    const auto geometry = DescribeSurface(descriptor);
    GuestBlock block(static_cast<std::size_t>(geometry.guestBytes));
    descriptor.baseAddress = reinterpret_cast<std::uintptr_t>(block.Surface());
    auto expected = std::vector<std::byte>(block.registered, block.registered + block.bytes);
    const auto layer = layers - 1u;
    const auto mip = descriptor.lastLevel;
    const auto offset = TexelOffset(descriptor, geometry, ColorTileMode::RenderTarget, mip, layer);
    ColorTarget color{descriptor.baseAddress + offset, {1, 1}, VK_FORMAT_R32_SFLOAT, 65536, 0xe4u};
    color.tileMode = ColorTileMode::RenderTarget;
    color.surfaceAddress = descriptor.baseAddress + geometry.GuestLayerOffset(layer);
    color.surfaceExtent = {descriptor.width, descriptor.height};
    color.mipCount = descriptor.mipCount;
    color.mip = mip;
    color.mipTail = geometry.mips[mip].tail;
    if (layers > 1) {
        color.arrayAddress = descriptor.baseAddress;
        color.arrayLastSlice = layers - 1u;
        color.arraySlice = layer;
    }
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    std::optional<Recorder> recorder;
    if (recorded) {
        recorder.emplace(context);
        recorder->Activate();
    }
    DrawTarget(context, device, color);
    if (recorded) {
        Require(recorder->Recording(), "extra-mip draw did not leave recorded GPU work");
        recorder->Sync();
    }
    StorageTexture::FlushAllPending("idle");
    if (recorded) recorder->Sync();
    device.WaitIdle();
    const std::uint32_t value = std::bit_cast<std::uint32_t>(1.0f);
    std::memcpy(expected.data() + 65536u + offset, &value, sizeof(value));
    for (std::size_t index = 0; index < expected.size(); ++index) {
        Require(expected[index] == block.registered[index], "extra-mip draw mismatch: layers=" + std::to_string(layers) + " offset=" + std::to_string(offset) + " byte=" + std::to_string(index) + " actual=" + std::to_string(std::to_integer<unsigned>(block.registered[index])) + " expected=" + std::to_string(std::to_integer<unsigned>(expected[index])));
    }
    ClearCachedTextures(context.device);
    std::printf("extra-mip actual draw passed: recorded=%u first-array-slice=%u mip=%u physical=%u\n", recorded ? 1u : 0u, layer, mip, geometry.imageLevels);
}

void CheckReadback(AgcDriver::VulkanDevice& device) {
    auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(device);
    GuestTextureResource descriptor{};
    descriptor.width = 4;
    descriptor.height = 2;
    descriptor.mipCount = 5;
    descriptor.lastLevel = 4;
    descriptor.format = 20;
    descriptor.tileMode = TextureTileMode::kLinear;
    descriptor.dimension = TextureDimension::k2DArray;
    descriptor.depthOrLastArray = 1;
    const auto geometry = DescribeSurface(descriptor);
    GuestBlock block(static_cast<std::size_t>(geometry.guestBytes));
    descriptor.baseAddress = reinterpret_cast<std::uintptr_t>(block.Surface());
    std::lock_guard lock(AgcDriver::GuestMemory::GpuMutex());
    auto texture = StorageTexture::Create(context, *context.detiler, descriptor, 0);
    if (auto* recorder = Recorder::Active()) recorder->Sync();
    char name[80];
    std::snprintf(name, sizeof(name), "storage_direct_%llx_0.raw", static_cast<unsigned long long>(descriptor.baseAddress));
    Require(!std::filesystem::exists(name), "extra-mip readback file already exists");
    StorageTexture::FlushDumpSelected("closed extra-mip fixture gate");
    Require(!std::filesystem::exists(name), "extra-mip readback ignored its closed gate");
    { std::ofstream gate("extra-mip-readback.ready"); Require(gate.good(), "cannot create extra-mip readback fixture gate"); }
    StorageTexture::FlushDumpSelected("extra-mip fixture readback");
    std::filesystem::remove("extra-mip-readback.ready");
    std::ifstream file(name, std::ios::binary);
    std::array<std::uint32_t, 3> header{};
    file.read(reinterpret_cast<char*>(header.data()), sizeof(header));
    Require(file.good() && header[0] == geometry.mips[0].pitchBytes / 4u && header[1] == descriptor.height && header[2] == VK_FORMAT_R32_UINT, "extra-mip readback header mismatch");
    std::vector<std::byte> pixels(static_cast<std::size_t>(geometry.mips[0].linearSize * geometry.layers));
    file.read(reinterpret_cast<char*>(pixels.data()), static_cast<std::streamsize>(pixels.size()));
    Require(file.good() && file.peek() == std::char_traits<char>::eof(), "extra-mip readback file size mismatch");
    for (std::uint32_t layer = 0; layer < geometry.layers; ++layer) {
        for (std::uint32_t y = 0; y < descriptor.height; ++y) {
            for (std::uint32_t x = 0; x < descriptor.width * 4u; ++x) {
                const auto offset = layer * geometry.mips[0].linearSize + y * geometry.mips[0].pitchBytes + x;
                Require(pixels[offset] == std::byte{0xa5}, "extra-mip diagnostic readback changed a base-level pixel");
            }
        }
    }
    file.close();
    std::filesystem::remove(name);
    texture.reset();
    std::puts("extra-mip physical/extra-image diagnostic readback passed");
}

}

int main(int argc, char** argv) {
    try {
        const std::string mode = argc == 2 ? argv[1] : "";
        Require(argc == 1 || mode == "--recorded" || mode == "--immediate" || mode == "--readback", "unknown extra-mip fixture mode");
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (mode == "--readback") { CheckReadback(*device); return 0; }
        if (mode == "--recorded" || mode == "--immediate") {
            CheckDraw(*device, mode == "--recorded", 1u);
            CheckDraw(*device, mode == "--recorded", 3u);
            return 0;
        }
        for (const auto singlePass : {true, false}) {
            CheckCase(*device, 512u, 256u, 12u, 75u, TextureTileMode::kD64KBX, ColorTileMode::D64KBX, 1u, singlePass);
            CheckCase(*device, 32u, 16u, 8u, 20u, TextureTileMode::kR64KBX, ColorTileMode::RenderTarget, 2u, singlePass);
            CheckCase(*device, 7u, 3u, 6u, 20u, TextureTileMode::kLinear, ColorTileMode::Linear, 2u, singlePass);
            CheckCase(*device, 1u, 1u, 16u, 20u, TextureTileMode::kLinear, ColorTileMode::Linear, 2u, singlePass);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
