#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTextureLayout.hpp"
#include "VulkanTestDevice.hpp"
#include "MsaaReadbackVertex_spv.h"
#include "MsaaReadbackUint_spv.h"
#include "MsaaReadbackDepth_spv.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <limits>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>

namespace AgcDriver {

struct VulkanDeviceTestAccess {
    static Graphics::Context ContextOf(const VulkanDevice& device) { return device.graphicsContext(); }
};

}

namespace {

using namespace AgcDriver::Graphics;

PFN_vkGetDeviceProcAddr originalDeviceProc = nullptr;
PipelineCacheSynchronization* pipelineCacheSynchronization = nullptr;
std::uint32_t checkedPipelineCacheUses = 0u;

VKAPI_ATTR VkResult VKAPI_CALL CheckedCreateComputePipelines(VkDevice device, VkPipelineCache cache, std::uint32_t count, const VkComputePipelineCreateInfo* infos, const VkAllocationCallbacks* allocator, VkPipeline* pipelines) {
    bool snapshotAvailable = false;
    std::thread probe([&] {
        const auto snapshot = pipelineCacheSynchronization->TryLockForSnapshot();
        snapshotAvailable = snapshot.owns_lock();
    });
    probe.join();
    Require(!snapshotAvailable, "sample reader pipeline creation did not hold the cache use lock");
    ++checkedPipelineCacheUses;
    const auto create = reinterpret_cast<PFN_vkCreateComputePipelines>(originalDeviceProc(device, "vkCreateComputePipelines"));
    Require(create != nullptr, "readback fixture cannot resolve compute pipeline creation");
    return create(device, cache, count, infos, allocator, pipelines);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL CheckedDeviceProc(VkDevice device, const char* name) {
    if (std::strcmp(name, "vkCreateComputePipelines") == 0) return reinterpret_cast<PFN_vkVoidFunction>(CheckedCreateComputePipelines);
    return originalDeviceProc(device, name);
}

constexpr bool SurfaceFits(const VkImageFormatProperties& supported, std::uint32_t samples, std::uint32_t width = 17u, std::uint32_t height = 5u, std::uint32_t layers = 2u) {
    return (supported.sampleCounts & samples) != 0u && width <= supported.maxExtent.width && height <= supported.maxExtent.height && layers <= supported.maxArrayLayers;
}

void CheckSupportSelection() {
    constexpr VkImageFormatProperties supported{{128u, 64u, 1u}, 1u, 2u, VK_SAMPLE_COUNT_1_BIT | VK_SAMPLE_COUNT_4_BIT | VK_SAMPLE_COUNT_8_BIT, 0u};
    static_assert(!SurfaceFits(supported, 2u));
    static_assert(SurfaceFits(supported, 4u) && SurfaceFits(supported, 8u));
    static_assert(SurfaceFits(supported, 4u, 128u, 64u, 1u));
    static_assert(!SurfaceFits(supported, 4u, 129u, 64u, 1u));
    static_assert(!SurfaceFits(supported, 4u, 128u, 65u, 1u));
    static_assert(!SurfaceFits(supported, 4u, 17u, 5u, 3u));
    static_assert(!SurfaceFits(VkImageFormatProperties{}, 4u));
}

bool SurfaceSupported(const Context& context, VkFormat format, std::uint32_t samples, std::uint32_t width = 17u, std::uint32_t height = 5u, std::uint32_t layers = 2u, bool writable = false) {
    const VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | (format == VK_FORMAT_D32_SFLOAT ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) | (writable ? VK_IMAGE_USAGE_STORAGE_BIT : 0u);
    VkImageFormatProperties supported{};
    const auto result = context.imageFormatProperties(context.physical, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage, 0u, &supported);
    if (result != VK_ERROR_FORMAT_NOT_SUPPORTED) Check(result, "readback fixture format query");
    if (result == VK_SUCCESS && SurfaceFits(supported, samples, width, height, layers)) return true;
    std::printf("skipped, the device does not support readback fixture format=%u samples=%u extent=%ux%u layers=%u\n", static_cast<std::uint32_t>(format), samples, width, height, layers);
    return false;
}

struct Surface {
    Context context;
    VkFormat format;
    std::uint32_t elementBytes;
    std::uint32_t samples;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t layers;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView arrayView = VK_NULL_HANDLE;
    VkRenderPass pass = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    std::array<VkShaderModule, 2> modules{};
    std::vector<VkImageView> views;
    std::vector<VkFramebuffer> framebuffers;

    Surface(const Context& context, VkFormat format, std::uint32_t elementBytes, std::uint32_t samples, std::uint32_t width = 17u, std::uint32_t height = 5u, std::uint32_t layers = 2u, bool writable = false) : context(context), format(format), elementBytes(elementBytes), samples(samples), width(width), height(height), layers(layers) {
        try {
            const bool depth = format == VK_FORMAT_D32_SFLOAT;
            const auto aspect = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
            const VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | (depth ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) | (writable ? VK_IMAGE_USAGE_STORAGE_BIT : 0u);
            VkImageFormatProperties supported{};
            Check(context.imageFormatProperties(context.physical, format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage, 0u, &supported), "readback fixture format query");
            Require(SurfaceFits(supported, samples, width, height, layers), "readback fixture format does not support its sample count or extent");
            Require(context.sampleRateShading, "readback fixture needs per-sample shading");
            VkImageCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
            info.imageType = VK_IMAGE_TYPE_2D;
            info.format = format;
            info.extent = {width, height, 1u};
            info.mipLevels = 1u;
            info.arrayLayers = layers;
            info.samples = static_cast<VkSampleCountFlagBits>(samples);
            info.tiling = VK_IMAGE_TILING_OPTIMAL;
            info.usage = usage;
            info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            Check(context.Function<PFN_vkCreateImage>("vkCreateImage")(context.device, &info, nullptr, &image), "readback fixture image");
            VkMemoryRequirements requirements{};
            context.Function<PFN_vkGetImageMemoryRequirements>("vkGetImageMemoryRequirements")(context.device, image, &requirements);
            VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
            allocation.allocationSize = requirements.size;
            allocation.memoryTypeIndex = context.MemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            Check(context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &memory), "readback fixture memory");
            Check(context.Function<PFN_vkBindImageMemory>("vkBindImageMemory")(context.device, image, memory, 0u), "readback fixture bind");
            VkImageViewCreateInfo view{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            view.image = image;
            view.viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            view.format = format;
            view.subresourceRange = {aspect, 0u, 1u, 0u, layers};
            Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &view, nullptr, &arrayView), "readback fixture array view");
            VkAttachmentDescription attachment{};
            attachment.format = format;
            attachment.samples = info.samples;
            attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            attachment.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
            const VkAttachmentReference reference{0u, VK_IMAGE_LAYOUT_GENERAL};
            VkSubpassDescription subpass{};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            if (depth) subpass.pDepthStencilAttachment = &reference;
            else { subpass.colorAttachmentCount = 1u; subpass.pColorAttachments = &reference; }
            VkRenderPassCreateInfo renderPass{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
            renderPass.attachmentCount = 1u;
            renderPass.pAttachments = &attachment;
            renderPass.subpassCount = 1u;
            renderPass.pSubpasses = &subpass;
            Check(context.Function<PFN_vkCreateRenderPass>("vkCreateRenderPass")(context.device, &renderPass, nullptr, &pass), "readback fixture render pass");
            views.resize(layers);
            framebuffers.resize(layers);
            for (std::uint32_t layer = 0u; layer < layers; ++layer) {
                view.viewType = VK_IMAGE_VIEW_TYPE_2D;
                view.subresourceRange.baseArrayLayer = layer;
                view.subresourceRange.layerCount = 1u;
                Check(context.Function<PFN_vkCreateImageView>("vkCreateImageView")(context.device, &view, nullptr, &views[layer]), "readback fixture layer view");
                VkFramebufferCreateInfo framebuffer{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
                framebuffer.renderPass = pass;
                framebuffer.attachmentCount = 1u;
                framebuffer.pAttachments = &views[layer];
                framebuffer.width = width;
                framebuffer.height = height;
                framebuffer.layers = 1u;
                Check(context.Function<PFN_vkCreateFramebuffer>("vkCreateFramebuffer")(context.device, &framebuffer, nullptr, &framebuffers[layer]), "readback fixture framebuffer");
            }
            const auto makeModule = [&](std::span<const std::uint32_t> words, VkShaderModule& module) {
                VkShaderModuleCreateInfo shader{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
                shader.codeSize = words.size_bytes();
                shader.pCode = words.data();
                Check(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(context.device, &shader, nullptr, &module), "readback fixture shader");
            };
            makeModule(MSAA_READBACK_VERTEX_SPV, modules[0]);
            makeModule(depth ? std::span<const std::uint32_t>(MSAA_READBACK_DEPTH_SPV) : std::span<const std::uint32_t>(MSAA_READBACK_UINT_SPV), modules[1]);
            const VkPushConstantRange push{VK_SHADER_STAGE_FRAGMENT_BIT, 0u, 5u * sizeof(std::uint32_t)};
            VkPipelineLayoutCreateInfo layout{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
            layout.pushConstantRangeCount = 1u;
            layout.pPushConstantRanges = &push;
            Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &layout, nullptr, &pipelineLayout), "readback fixture pipeline layout");
            std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
            for (std::uint32_t index = 0; index < stages.size(); ++index) {
                stages[index] = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
                stages[index].stage = index == 0u ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
                stages[index].module = modules[index];
                stages[index].pName = "main";
            }
            const VkPipelineVertexInputStateCreateInfo vertices{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
            VkPipelineInputAssemblyStateCreateInfo assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
            assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            const VkViewport viewport{0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f};
            const VkRect2D scissor{{0, 0}, {width, height}};
            VkPipelineViewportStateCreateInfo viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
            viewportState.viewportCount = viewportState.scissorCount = 1u;
            viewportState.pViewports = &viewport;
            viewportState.pScissors = &scissor;
            VkPipelineRasterizationStateCreateInfo raster{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
            raster.polygonMode = VK_POLYGON_MODE_FILL;
            raster.cullMode = VK_CULL_MODE_NONE;
            raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            raster.lineWidth = 1.0f;
            VkPipelineMultisampleStateCreateInfo multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
            multisample.rasterizationSamples = info.samples;
            multisample.sampleShadingEnable = VK_TRUE;
            multisample.minSampleShading = 1.0f;
            VkPipelineDepthStencilStateCreateInfo depthState{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
            depthState.depthTestEnable = depthState.depthWriteEnable = depth ? VK_TRUE : VK_FALSE;
            depthState.depthCompareOp = VK_COMPARE_OP_ALWAYS;
            VkPipelineColorBlendAttachmentState blend{};
            blend.colorWriteMask = 15u;
            VkPipelineColorBlendStateCreateInfo color{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
            color.attachmentCount = depth ? 0u : 1u;
            color.pAttachments = &blend;
            VkGraphicsPipelineCreateInfo graphics{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
            graphics.stageCount = static_cast<std::uint32_t>(stages.size());
            graphics.pStages = stages.data();
            graphics.pVertexInputState = &vertices;
            graphics.pInputAssemblyState = &assembly;
            graphics.pViewportState = &viewportState;
            graphics.pRasterizationState = &raster;
            graphics.pMultisampleState = &multisample;
            graphics.pDepthStencilState = &depthState;
            graphics.pColorBlendState = &color;
            graphics.layout = pipelineLayout;
            graphics.renderPass = pass;
            Check(context.Function<PFN_vkCreateGraphicsPipelines>("vkCreateGraphicsPipelines")(context.device, context.pipelineCache, 1u, &graphics, nullptr, &pipeline), "readback fixture graphics pipeline");
        } catch (...) { Release(); throw; }
    }

    ~Surface() { Release(); }
    Surface(const Surface&) = delete;
    Surface& operator=(const Surface&) = delete;

    void Release() noexcept {
        if (pipeline) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pipeline, nullptr);
        if (pipelineLayout) context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, pipelineLayout, nullptr);
        for (const auto module : modules) if (module) context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
        for (const auto framebuffer : framebuffers) if (framebuffer) context.Function<PFN_vkDestroyFramebuffer>("vkDestroyFramebuffer")(context.device, framebuffer, nullptr);
        if (pass) context.Function<PFN_vkDestroyRenderPass>("vkDestroyRenderPass")(context.device, pass, nullptr);
        for (const auto view : views) if (view) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, view, nullptr);
        if (arrayView) context.Function<PFN_vkDestroyImageView>("vkDestroyImageView")(context.device, arrayView, nullptr);
        if (image) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, image, nullptr);
        if (memory) context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, memory, nullptr);
    }

    void Render() {
        CommandBatch batch(context);
        for (std::uint32_t layer = 0; layer < layers; ++layer) {
            const VkClearValue clear{};
            VkRenderPassBeginInfo begin{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
            begin.renderPass = pass;
            begin.framebuffer = framebuffers[layer];
            begin.renderArea = {{0, 0}, {width, height}};
            begin.clearValueCount = 1u;
            begin.pClearValues = &clear;
            const std::array<std::uint32_t, 5> push{layer, width, height, samples, elementBytes};
            context.Function<PFN_vkCmdBeginRenderPass>("vkCmdBeginRenderPass")(batch.Handle(), &begin, VK_SUBPASS_CONTENTS_INLINE);
            context.Function<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(batch.Handle(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            context.Function<PFN_vkCmdPushConstants>("vkCmdPushConstants")(batch.Handle(), pipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0u, sizeof(push), push.data());
            context.Function<PFN_vkCmdDraw>("vkCmdDraw")(batch.Handle(), 3u, 1u, 0u, 0u);
            context.Function<PFN_vkCmdEndRenderPass>("vkCmdEndRenderPass")(batch.Handle());
        }
        batch.SubmitAndWait();
    }

    std::vector<std::byte> Expected(std::uint32_t first, std::uint32_t elements) const {
        std::vector<std::byte> result(static_cast<std::size_t>(elements) * elementBytes);
        for (std::uint32_t index = 0u; index < elements; ++index) {
            const auto element = first + index;
            const auto sample = element % samples;
            const auto pixel = element / samples;
            const auto x = pixel % width;
            const auto y = (pixel / width) % height;
            const auto layer = pixel / (width * height);
            const auto value = x + y * width + layer * width * height + sample * 31u;
            std::array<std::uint32_t, 4> words{0x91352700u ^ value, 0x45219b00u ^ (value * 3u), 0xc7391d00u ^ (value * 5u), 0x6b43e100u ^ (value * 7u)};
            if (format == VK_FORMAT_D32_SFLOAT) words[0] = std::bit_cast<std::uint32_t>(0.125f + static_cast<float>(element + 1u) / 1048576.0f);
            std::memcpy(result.data() + static_cast<std::size_t>(index) * elementBytes, words.data(), elementBytes);
        }
        return result;
    }
};

std::vector<std::byte> CheckReadback(TextureDetiler& reader, const Surface& surface, std::uint32_t first, std::uint32_t elements, std::uint32_t repetitions, std::size_t extraBytes = 0u, std::span<const std::byte> supplied = {}) {
    const auto expected = supplied.empty() ? surface.Expected(first, elements) : std::vector<std::byte>(supplied.begin(), supplied.end());
    const auto bytes = ((expected.size() + 3u) & ~std::size_t{3u}) + extraBytes;
    const auto alignment = std::max<std::size_t>(4u, surface.context.limits.minStorageBufferOffsetAlignment);
    const auto offset = (16u + alignment - 1u) / alignment * alignment;
    Buffer output(surface.context, offset + bytes + 16u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    std::fill(output.Bytes().begin(), output.Bytes().end(), std::byte{0xa5});
    reader.BeginBatch();
    CommandBatch batch(surface.context);
    RecordMemoryBarrier(surface.context, batch.Handle(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    for (std::uint32_t repeat = 0u; repeat < repetitions; ++repeat) {
        reader.DispatchMultisampleReadback(batch.Handle(), surface.format, output.Handle(), offset, bytes, surface.arrayView, surface.width, surface.height, surface.layers, surface.samples, first, elements);
        if (repeat + 1u < repetitions) RecordMemoryBarrier(surface.context, batch.Handle(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    }
    RecordMemoryBarrier(surface.context, batch.Handle(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
    batch.SubmitAndWait();
    output.Invalidate();
    const auto data = output.Bytes();
    const auto mismatch = std::mismatch(expected.begin(), expected.end(), data.begin() + offset);
    if (mismatch.first != expected.end()) std::printf("readback mismatch: format=%u first=%u byte=%zu expected=0x%02x actual=0x%02x\n", static_cast<std::uint32_t>(surface.format), first, static_cast<std::size_t>(mismatch.first - expected.begin()), std::to_integer<unsigned>(*mismatch.first), std::to_integer<unsigned>(*mismatch.second));
    Require(std::equal(expected.begin(), expected.end(), data.begin() + offset), "multisample readback changed a rendered sample, component, layer or pixel");
    for (std::size_t index = 0u; index < data.size(); ++index) Require((index >= offset && index < offset + expected.size()) || data[index] == std::byte{0xa5}, "multisample readback changed a buffer sentinel or packed tail");
    return {data.begin() + offset, data.begin() + offset + expected.size()};
}

void CheckRetile(TextureDetiler& reader, const Surface& surface) {
    const auto elements = surface.width * surface.height * surface.layers * surface.samples;
    const auto linear = CheckReadback(reader, surface, 0u, elements, 1u);
    const auto expected = surface.Expected(0u, elements);
    constexpr std::array<std::array<std::uint32_t, 3>, 7> coordinates{{
        {0u, 0u, 0u}, {128u, 66u, 16u}, {64u, 32u, 1u}, {32u, 64u, 2u},
        {63u, 31u, 7u}, {127u, 65u, 15u}, {1u, 1u, 3u}
    }};
    constexpr std::array<TextureTileMode, 2> modes{TextureTileMode::kR64KBX, TextureTileMode::kZ64KBX};
    constexpr std::array<std::array<std::uint64_t, 7>, 2> references{{
        {0ull, 6621216ull, 493568ull, 1016832ull, 2766588ull, 6170780ull, 1215508ull},
        {0ull, 6637700ull, 460808ull, 1000460ull, 2815728ull, 6219892ull, 1182776ull}
    }};
    for (std::size_t mode = 0u; mode < modes.size(); ++mode) {
        for (const auto pipeBankXor : {0u, 0x8100u}) {
            const MultisampleTextureLayout layout(modes[mode], surface.elementBytes, surface.samples, surface.width, surface.height, surface.layers, pipeBankXor);
            std::vector<std::byte> tiled(static_cast<std::size_t>(layout.Bytes()) + 32u, std::byte{0xa5});
            auto payload = std::span(tiled).subspan(16u, layout.Bytes());
            layout.Tile(linear, payload);
            std::vector<bool> covered(payload.size());
            std::size_t position = 0u;
            for (std::uint32_t layer = 0u; layer < surface.layers; ++layer) {
                for (std::uint32_t y = 0u; y < surface.height; ++y) {
                    for (std::uint32_t x = 0u; x < surface.width; ++x) {
                        for (std::uint32_t fragment = 0u; fragment < surface.samples; ++fragment) {
                            const auto address = layout.Offset(x, y, layer, fragment);
                            Require(address + surface.elementBytes <= payload.size(), "readback retile exceeds its allocation");
                            for (std::uint32_t byte = 0u; byte < surface.elementBytes; ++byte) {
                                Require(!covered[address + byte], "readback retile elements overlap");
                                covered[address + byte] = true;
                                Require(payload[address + byte] == expected[position++], "readback and retile disagree on layer, pixel or fragment order");
                            }
                        }
                    }
                }
            }
            for (std::size_t index = 0u; index < covered.size(); ++index) Require(covered[index] || payload[index] == std::byte{0xa5}, "readback retile changed tile padding");
            for (std::size_t index = 0u; index < coordinates.size(); ++index) {
                const auto& coordinate = coordinates[index];
                const auto fragment = static_cast<std::uint32_t>(index) % surface.samples;
                const auto source = (((coordinate[2] * surface.height + coordinate[1]) * surface.width + coordinate[0]) * surface.samples + fragment) * surface.elementBytes;
                const auto reference = references[mode][index];
                const auto address = (reference & ~65535ull) | ((reference & 65535ull) ^ pipeBankXor);
                Require(address + surface.elementBytes <= payload.size() && std::equal(expected.begin() + source, expected.begin() + source + surface.elementBytes, payload.begin() + address), "rendered readback differs at an AMD reference fragment address");
            }
            std::vector<std::byte> restored(expected.size());
            layout.Detile(payload, restored);
            Require(restored == expected && linear == expected, "readback retile roundtrip changed rendered element bits");
            for (std::size_t index = 0u; index < tiled.size(); ++index) Require((index >= 16u && index < 16u + payload.size()) || tiled[index] == std::byte{0xa5}, "readback retile changed an allocation sentinel");
            std::printf("multisample readback retile: format=%u mode=%s xor=0x%x samples=%u layers=%u elements=%u reference-points=7 padding/sentinels/roundtrip=passed\n", static_cast<std::uint32_t>(surface.format), mode == 0u ? "R_X" : "Z_X", pipeBankXor, surface.samples, surface.layers, elements);
        }
    }
}

template<typename TAction>
void Refused(TAction action, const char* expected) {
    std::string reason;
    try { action(); } catch (const std::exception& error) { reason = error.what(); }
    Require(reason.find(expected) != std::string::npos, "multisample readback missed an exact invalid-input refusal: " + reason);
}

void CheckRefusals(TextureDetiler& reader, const Surface& surface) {
    Buffer output(surface.context, 4096u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    CommandBatch batch(surface.context);
    const auto call = [&](VkFormat format, std::uint64_t offset, std::uint64_t bytes, std::uint32_t width, std::uint32_t height, std::uint32_t layers, std::uint32_t fragments, std::uint32_t first, std::uint32_t elements) {
        reader.DispatchMultisampleReadback(batch.Handle(), format, output.Handle(), offset, bytes, surface.arrayView, width, height, layers, fragments, first, elements);
    };
    for (auto format : {VK_FORMAT_UNDEFINED, VK_FORMAT_R32_SFLOAT, VK_FORMAT_D16_UNORM, VK_FORMAT_D32_SFLOAT_S8_UINT}) Refused([&] { call(format, 0u, 4u, 1u, 1u, 1u, 4u, 0u, 1u); }, "requires an unsigned element view or D32 depth");
    for (auto fragments : {0u, 1u, 3u, 16u}) Refused([&] { call(surface.format, 0u, 4u, 1u, 1u, 1u, fragments, 0u, 1u); }, "fragment count is unsupported");
    for (auto extent : {std::array{0u, 1u, 1u}, std::array{1u, 0u, 1u}, std::array{1u, 1u, 0u}, std::array{16385u, 1u, 1u}, std::array{1u, 16385u, 1u}, std::array{1u, 1u, 8193u}}) Refused([&] { call(surface.format, 0u, 4u, extent[0], extent[1], extent[2], 4u, 0u, 1u); }, "extent is unsupported");
    for (auto range : {std::array{0u, 0u}, std::array{4u, 1u}, std::array{3u, 2u}, std::array{0xffffffffu, 2u}}) Refused([&] { call(surface.format, 0u, 4u, 1u, 1u, 1u, 4u, range[0], range[1]); }, "element range is invalid");
    for (auto range : {std::array<std::uint64_t, 2>{1u, 4u}, std::array<std::uint64_t, 2>{0u, 0u}, std::array<std::uint64_t, 2>{0u, 3u}, std::array<std::uint64_t, 2>{0u, static_cast<std::uint64_t>(surface.context.limits.maxStorageBufferRange) + 4u}, std::array<std::uint64_t, 2>{~std::uint64_t{0} - 3u, 8u}}) Refused([&] { call(surface.format, range[0], range[1], 1u, 1u, 1u, 4u, 0u, 1u); }, "buffer range is invalid");
    Refused([&] { reader.DispatchMultisampleReadback(VK_NULL_HANDLE, surface.format, output.Handle(), 0u, 4u, surface.arrayView, 1u, 1u, 1u, 4u, 0u, 1u); }, "requires commands, buffer and array view");
    Refused([&] { reader.DispatchMultisampleReadback(batch.Handle(), surface.format, VK_NULL_HANDLE, 0u, 4u, surface.arrayView, 1u, 1u, 1u, 4u, 0u, 1u); }, "requires commands, buffer and array view");
    Refused([&] { reader.DispatchMultisampleReadback(batch.Handle(), surface.format, output.Handle(), 0u, 4u, VK_NULL_HANDLE, 1u, 1u, 1u, 4u, 0u, 1u); }, "requires commands, buffer and array view");
    Refused([&] { call(surface.format, 0u, 4u, 16384u, 16384u, 8192u, 8u, 0u, 1u); }, "element range is invalid");
    std::puts("multisample readback invalid-input refusals passed");
}

std::vector<std::byte> Upload(TextureDetiler& transfer, TextureDetiler& reader, const Surface& surface, std::vector<std::byte>& expected, std::uint32_t first, std::uint32_t elements, std::uint32_t repetitions = 1u, std::span<const std::byte> supplied = {}) {
    const auto bytes = static_cast<std::size_t>(elements) * surface.elementBytes;
    const auto padded = (bytes + 3u) & ~std::size_t{3};
    const auto alignment = std::max<std::size_t>({4u, surface.elementBytes, surface.context.limits.minStorageBufferOffsetAlignment});
    const auto offset = (16u + alignment - 1u) / alignment * alignment;
    Buffer source(surface.context, offset + padded + 16u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    std::fill(source.Bytes().begin(), source.Bytes().end(), std::byte{0x5a});
    Require(supplied.empty() || supplied.size() == bytes, "multisample upload fixture source size mismatch");
    for (std::size_t index = 0; index < bytes; ++index) source.Bytes()[offset + index] = supplied.empty() ? static_cast<std::byte>((index * 29u + first * 17u + elements * 7u) & 255u) : supplied[index];
    const std::vector<std::byte> retained(source.Bytes().begin(), source.Bytes().end());
    std::copy_n(retained.begin() + offset, bytes, expected.begin() + static_cast<std::size_t>(first) * surface.elementBytes);
    const auto stride = 256u / alignment * alignment;
    Require(stride != 0u, "upload fixture cannot hold an aligned chunk");
    transfer.BeginBatch();
    CommandBatch batch(surface.context);
    RecordMemoryBarrier(surface.context, batch.Handle(), VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    for (std::uint32_t repeat = 0; repeat < repetitions; ++repeat) {
        for (std::size_t done = 0; done < bytes; done += stride) {
            const auto count = std::min(stride, bytes - done);
            transfer.DispatchMultisampleImage(batch.Handle(), surface.elementBytes, source.Handle(), offset + done, (count + 3u) & ~std::size_t{3}, surface.arrayView, surface.width, surface.height, surface.layers, surface.samples, first + static_cast<std::uint32_t>(done / surface.elementBytes), static_cast<std::uint32_t>(count / surface.elementBytes));
        }
        RecordMemoryBarrier(surface.context, batch.Handle(), VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    }
    batch.SubmitAndWait();
    source.Invalidate();
    Require(std::equal(retained.begin(), retained.end(), source.Bytes().begin()), "multisample upload changed source bytes or sentinels");
    return CheckReadback(reader, surface, 0u, surface.width * surface.height * surface.layers * surface.samples, 1u, 0u, expected);
}

void CheckUploadRefusals(TextureDetiler& transfer, const Surface& surface) {
    Buffer source(surface.context, 256u, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    CommandBatch batch(surface.context);
    const auto call = [&](std::uint32_t bytes, std::uint32_t fragments, std::uint32_t first, std::uint32_t elements, std::uint64_t offset = 0u, std::uint64_t range = 4u) {
        transfer.DispatchMultisampleImage(batch.Handle(), bytes, source.Handle(), offset, range, surface.arrayView, 1u, 1u, 1u, fragments, first, elements);
    };
    for (const auto bytes : {0u, 3u, 32u}) Refused([&] { call(bytes, 4u, 0u, 1u); }, "element size is unsupported");
    for (const auto fragments : {0u, 1u, 3u, 16u}) Refused([&] { call(4u, fragments, 0u, 1u); }, "fragment count is unsupported");
    for (const auto range : {std::array{0u, 0u}, std::array{4u, 1u}, std::array{3u, 2u}, std::array{0xffffffffu, 2u}}) Refused([&] { call(4u, 4u, range[0], range[1]); }, "element range is invalid");
    for (const auto range : {std::array<std::uint64_t, 2>{1u, 4u}, std::array<std::uint64_t, 2>{0u, 0u}, std::array<std::uint64_t, 2>{0u, 3u}, std::array<std::uint64_t, 2>{0u, 260u}, std::array<std::uint64_t, 2>{~std::uint64_t{0} - 3u, 8u}}) Refused([&] { call(4u, 4u, 0u, 1u, range[0], range[1]); }, "buffer range is invalid");
    Refused([&] { transfer.DispatchMultisampleImage(VK_NULL_HANDLE, 4u, source.Handle(), 0u, 4u, surface.arrayView, 1u, 1u, 1u, 4u, 0u, 1u); }, "requires commands, buffer and array view");
    Refused([&] { transfer.DispatchMultisampleImage(batch.Handle(), 4u, VK_NULL_HANDLE, 0u, 4u, surface.arrayView, 1u, 1u, 1u, 4u, 0u, 1u); }, "requires commands, buffer and array view");
    Refused([&] { transfer.DispatchMultisampleImage(batch.Handle(), 4u, source.Handle(), 0u, 4u, VK_NULL_HANDLE, 1u, 1u, 1u, 4u, 0u, 1u); }, "requires commands, buffer and array view");
    for (const auto extent : {std::array{0u, 1u, 1u}, std::array{1u, 0u, 1u}, std::array{1u, 1u, 0u}, std::array{16385u, 1u, 1u}, std::array{1u, 16385u, 1u}, std::array{1u, 1u, 8193u}}) Refused([&] { transfer.DispatchMultisampleImage(batch.Handle(), 4u, source.Handle(), 0u, 4u, surface.arrayView, extent[0], extent[1], extent[2], 4u, 0u, 1u); }, "extent is unsupported");
    Refused([&] { transfer.DispatchMultisampleImage(batch.Handle(), 4u, source.Handle(), 0u, 4u, surface.arrayView, 16384u, 16384u, 8192u, 8u, 0u, 1u); }, "element range is invalid");
    for (const bool multisample : {false, true}) {
        auto unsupported = surface.context;
        if (multisample) unsupported.storageImageMultisample = false;
        else unsupported.storageImageWriteWithoutFormat = false;
        TextureDetiler unavailable(unsupported);
        Refused([&] { unavailable.DispatchMultisampleImage(batch.Handle(), 4u, source.Handle(), 0u, 4u, surface.arrayView, 1u, 1u, 1u, 4u, 0u, 1u); }, "does not enable multisampled storage image writes without format");
    }
}

void CheckUpload(const Context& context) {
    Require(context.storageImageMultisample && context.storageImageWriteWithoutFormat, "upload fixture requires enabled multisampled formatless storage writes");
    auto limited = context;
    limited.limits.maxComputeWorkGroupCount[0] = 1u;
    limited.limits.maxStorageBufferRange = 256u;
    TextureDetiler transfer(limited);
    TextureDetiler reader(context);
    std::uint32_t exercised = 0u;
    for (const auto bytes : {1u, 2u, 4u, 8u, 16u}) {
        const auto format = TextureDetiler::ImageElementFormat(bytes);
        for (const auto samples : {2u, 4u, 8u}) {
            if (!SurfaceSupported(context, format, samples, 17u, 5u, 2u, true)) continue;
            Surface surface(context, format, bytes, samples, 17u, 5u, 2u, true);
            surface.Render();
            const auto elements = surface.width * surface.height * surface.layers * samples;
            auto expected = surface.Expected(0u, elements);
            Upload(transfer, reader, surface, expected, 3u, 1u);
            Upload(transfer, reader, surface, expected, 1u, elements - 3u);
            Upload(transfer, reader, surface, expected, 0u, elements);
            Upload(transfer, reader, surface, expected, elements - 1u, 1u, samples == 4u ? 65u : 1u);
            if (bytes == 4u && samples == 4u) CheckUploadRefusals(transfer, surface);
            std::printf("multisample upload: bytes=%u samples=%u layers=2 full/partial/packed/source-sentinels/chunks/reuse=passed\n", bytes, samples);
            ++exercised;
        }
    }
    Require(exercised != 0u, "no supported multisample upload subcase was exercised");
    const MultisampleTextureLayout active(TextureTileMode::kR64KBX, 8u, 4u, 17u, 5u, 2u);
    if (SurfaceSupported(context, VK_FORMAT_R32G32_UINT, 4u, active.Pitch(), active.PaddedHeight(), 2u, true)) {
        for (const auto xorValue : {0u, 0x8100u}) {
            const MultisampleTextureLayout layout(TextureTileMode::kR64KBX, 8u, 4u, active.Pitch(), active.PaddedHeight(), 2u, xorValue);
            std::vector<std::byte> tiled(static_cast<std::size_t>(layout.Bytes()));
            for (std::size_t byte = 0; byte < tiled.size(); ++byte) tiled[byte] = static_cast<std::byte>((byte * 37u + (byte >> 8u) * 13u + (byte >> 15u) * 71u + (byte >> 16u) * 101u) & 255u);
            std::vector<std::byte> linear(static_cast<std::size_t>(layout.LinearBytes()));
            layout.Detile(tiled, linear);
            Surface surface(context, VK_FORMAT_R32G32_UINT, 8u, 4u, layout.Pitch(), layout.PaddedHeight(), 2u, true);
            surface.Render();
            const auto elements = surface.width * surface.height * surface.layers * surface.samples;
            auto expected = surface.Expected(0u, elements);
            const auto actual = Upload(transfer, reader, surface, expected, 0u, elements, 1u, linear);
            std::vector<std::byte> retiled(tiled.size(), std::byte{0xa5});
            layout.Tile(actual, retiled);
            Require(retiled == tiled, "multisample padded initialization changed color bytes, fragments, layers, XOR or padding");
            std::printf("multisample padded upload: active=17x5 backing=%ux%u layers=2 fragments=4 bytes=%zu xor=0x%x exact roundtrip passed\n", surface.width, surface.height, retiled.size(), xorValue);
        }
    }
}

}

int main(int argc, char** argv) {
    try {
        CheckSupportSelection();
        const std::string mode = argc == 1 ? "uint32" : argv[1];
        VkFormat format = VK_FORMAT_R32_UINT;
        std::uint32_t bytes = 4u;
        if (mode == "uint8") { format = VK_FORMAT_R8_UINT; bytes = 1u; }
        else if (mode == "uint16") { format = VK_FORMAT_R16_UINT; bytes = 2u; }
        else if (mode == "uint64") { format = VK_FORMAT_R32G32_UINT; bytes = 8u; }
        else if (mode == "uint128") { format = VK_FORMAT_R32G32B32A32_UINT; bytes = 16u; }
        else Require(mode == "uint32" || mode == "depth" || mode == "upload" || mode == "retile" || mode == "depth_retile", "unknown multisample readback test mode");
        if (mode == "depth" || mode == "depth_retile") format = VK_FORMAT_D32_SFLOAT;
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        std::lock_guard gpu(AgcDriver::GuestMemory::GpuMutex());
        auto context = AgcDriver::VulkanDeviceTestAccess::ContextOf(*device);
        originalDeviceProc = context.deviceProc;
        pipelineCacheSynchronization = context.pipelineCacheSynchronization;
        Require(originalDeviceProc != nullptr && pipelineCacheSynchronization != nullptr, "readback fixture requires device functions and pipeline cache synchronization");
        context.deviceProc = CheckedDeviceProc;
        if (!context.sampleRateShading) {
            std::puts("skipped, the device does not support per-sample shading");
            return VulkanTestSkipped;
        }
        if (mode == "upload") {
            if (!context.storageImageMultisample || !context.storageImageWriteWithoutFormat) {
                std::puts("skipped, the device does not enable multisampled formatless storage writes");
                return VulkanTestSkipped;
            }
            CheckUpload(context);
            Require(checkedPipelineCacheUses != 0u, "upload fixture did not check pipeline cache use");
            std::printf("multisample upload guarded pipeline creation checks=%u passed\n", checkedPipelineCacheUses);
            return 0;
        }
        auto limited = context;
        limited.limits.maxComputeWorkGroupCount[0] = 1u;
        TextureDetiler reader(limited);
        if (mode == "retile" || mode == "depth_retile") {
            if (!SurfaceSupported(context, format, 4u, 129u, 67u, 17u)) return VulkanTestSkipped;
            Surface surface(context, format, bytes, 4u, 129u, 67u, 17u);
            surface.Render();
            CheckRetile(reader, surface);
            Require(checkedPipelineCacheUses != 0u, "retile fixture did not check pipeline cache use");
            std::printf("multisample retile guarded pipeline creation checks=%u passed\n", checkedPipelineCacheUses);
            return 0;
        }
        std::uint32_t checked = 0u;
        for (const auto samples : {2u, 4u, 8u}) {
            if (!SurfaceSupported(context, format, samples)) continue;
            Surface surface(context, format, bytes, samples);
            surface.Render();
            const auto elements = surface.width * surface.height * surface.layers * samples;
            CheckReadback(reader, surface, 1u, 1u, 1u, 64u * std::max(4u, bytes));
            CheckReadback(reader, surface, 0u, elements, samples == 4u ? 65u : 1u);
            CheckReadback(reader, surface, 1u, elements - 3u, 1u);
            CheckReadback(reader, surface, 0u, elements, 1u);
            if (samples == 4u && bytes == 4u) CheckRefusals(reader, surface);
            std::printf("multisample readback %s: samples=%u elements=%u layers=%u full/partial/sentinels/reuse/2d=passed storage-usage=0\n", mode.c_str(), samples, elements, surface.layers);
            ++checked;
        }
        if (checked == 0u) return VulkanTestSkipped;
        if (mode == "depth" && SurfaceSupported(context, format, 4u, 128u, 64u, 1u)) {
            Surface surface(context, format, bytes, 4u, 128u, 64u, 1u);
            surface.Render();
            CheckReadback(reader, surface, 0u, 128u * 64u * 4u, 1u);
            std::puts("multisample readback D32 128x64 4x: 32768 distinct D32 sample values passed");
        }
        if (mode == "uint32" && SurfaceSupported(context, VK_FORMAT_D32_SFLOAT, 4u) && SurfaceSupported(context, VK_FORMAT_R32_UINT, 4u)) {
            Surface depth(context, VK_FORMAT_D32_SFLOAT, 4u, 4u);
            depth.Render();
            CheckReadback(reader, depth, 0u, depth.width * depth.height * depth.layers * depth.samples, 1u);
            Surface color(context, VK_FORMAT_R32_UINT, 4u, 4u);
            color.Render();
            CheckReadback(reader, color, 0u, color.width * color.height * color.layers * color.samples, 1u);
            std::puts("multisample readback uint/depth pipeline transitions passed");
        }
        Require(checkedPipelineCacheUses != 0u, "readback fixture did not check pipeline cache use");
        std::printf("multisample reader guarded pipeline creation checks=%u passed\n", checkedPipelineCacheUses);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
