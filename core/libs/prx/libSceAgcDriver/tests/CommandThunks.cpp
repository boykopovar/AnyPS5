#include "prx/libSceAgcDriver/Graphics/include/CommandList.hpp"
#include "prx/libSceAgcDriver/Graphics/include/CommandStream.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <functional>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using AgcDriver::Graphics::CommandFunctions;
using AgcDriver::Graphics::CommandStream;
using AgcDriver::Graphics::StreamMaterializer;

void Require(bool condition, const std::string& message) {
    if (!condition) {
        std::fprintf(stderr, "command thunks: %s\n", message.c_str());
        throw std::runtime_error(message);
    }
}

template<typename Exception, typename Body>
bool Throws(Body&& body) {
    try {
        body();
    } catch (const Exception&) {
        return true;
    }
    return false;
}

std::vector<std::string> log;

VkCommandBuffer fakeBuffer(std::uintptr_t value) {
    return reinterpret_cast<VkCommandBuffer>(value << 4);
}

std::uintptr_t id(VkCommandBuffer commands) {
    return reinterpret_cast<std::uintptr_t>(commands) >> 4;
}

template<typename... T>
void note(const char* name, VkCommandBuffer commands, const T&... values) {
    std::ostringstream text;
    text << name << '@' << id(commands);
    ((text << ' ' << values), ...);
    log.push_back(text.str());
}

VKAPI_ATTR void VKAPI_CALL draw(VkCommandBuffer commands, std::uint32_t vertices, std::uint32_t instances, std::uint32_t firstVertex, std::uint32_t firstInstance) {
    note("draw", commands, vertices, instances, firstVertex, firstInstance);
}

VKAPI_ATTR void VKAPI_CALL dispatch(VkCommandBuffer commands, std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    note("dispatch", commands, x, y, z);
}

VKAPI_ATTR void VKAPI_CALL bindPipeline(VkCommandBuffer commands, VkPipelineBindPoint point, VkPipeline pipeline) {
    note("bindPipeline", commands, point, reinterpret_cast<std::uintptr_t>(pipeline));
}

VKAPI_ATTR void VKAPI_CALL setViewport(VkCommandBuffer commands, std::uint32_t first, std::uint32_t count, const VkViewport* viewports) {
    std::ostringstream text;
    for (std::uint32_t index = 0; index < count; ++index) text << viewports[index].x << ',' << viewports[index].width << ';';
    note("setViewport", commands, first, count, text.str());
}

VKAPI_ATTR void VKAPI_CALL setScissorWithCount(VkCommandBuffer commands, std::uint32_t count, const VkRect2D* scissors) {
    std::ostringstream text;
    for (std::uint32_t index = 0; index < count; ++index) text << scissors[index].offset.x << ',' << scissors[index].extent.width << ';';
    note("setScissorWithCount", commands, count, text.str());
}

VKAPI_ATTR void VKAPI_CALL pushConstants(VkCommandBuffer commands, VkPipelineLayout layout, VkShaderStageFlags stages, std::uint32_t offset, std::uint32_t size, const void* values) {
    std::string bytes(static_cast<const char*>(values), size);
    note("pushConstants", commands, reinterpret_cast<std::uintptr_t>(layout), stages, offset, size, bytes);
}

VKAPI_ATTR void VKAPI_CALL bindDescriptorSets(VkCommandBuffer commands, VkPipelineBindPoint point, VkPipelineLayout layout, std::uint32_t first, std::uint32_t count, const VkDescriptorSet* sets, std::uint32_t offsets, const std::uint32_t* dynamicOffsets) {
    std::ostringstream text;
    for (std::uint32_t index = 0; index < count; ++index) text << reinterpret_cast<std::uintptr_t>(sets[index]) << ';';
    for (std::uint32_t index = 0; index < offsets; ++index) text << dynamicOffsets[index] << ':';
    note("bindDescriptorSets", commands, point, reinterpret_cast<std::uintptr_t>(layout), first, count, offsets, text.str());
}

VKAPI_ATTR void VKAPI_CALL pipelineBarrier(VkCommandBuffer commands, VkPipelineStageFlags source, VkPipelineStageFlags destination, VkDependencyFlags dependencies, std::uint32_t memory, const VkMemoryBarrier* memoryBarriers, std::uint32_t buffers, const VkBufferMemoryBarrier* bufferBarriers, std::uint32_t images, const VkImageMemoryBarrier* imageBarriers) {
    std::ostringstream text;
    for (std::uint32_t index = 0; index < memory; ++index) text << memoryBarriers[index].srcAccessMask << '>' << memoryBarriers[index].dstAccessMask << ';';
    for (std::uint32_t index = 0; index < buffers; ++index) text << reinterpret_cast<std::uintptr_t>(bufferBarriers[index].buffer) << '+' << bufferBarriers[index].size << ';';
    for (std::uint32_t index = 0; index < images; ++index) text << reinterpret_cast<std::uintptr_t>(imageBarriers[index].image) << '/' << imageBarriers[index].newLayout << ';';
    note("pipelineBarrier", commands, source, destination, dependencies, memory, buffers, images, text.str());
}

VKAPI_ATTR void VKAPI_CALL updateBuffer(VkCommandBuffer commands, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, const void* data) {
    note("updateBuffer", commands, reinterpret_cast<std::uintptr_t>(buffer), offset, size, std::string(static_cast<const char*>(data), static_cast<std::size_t>(size)));
}

VKAPI_ATTR void VKAPI_CALL clearAttachments(VkCommandBuffer commands, std::uint32_t attachments, const VkClearAttachment* clears, std::uint32_t rects, const VkClearRect* areas) {
    std::ostringstream text;
    for (std::uint32_t index = 0; index < attachments; ++index) text << clears[index].colorAttachment << '=' << clears[index].clearValue.color.uint32[0] << ';';
    for (std::uint32_t index = 0; index < rects; ++index) text << areas[index].rect.extent.width << 'x' << areas[index].rect.extent.height << ';';
    note("clearAttachments", commands, attachments, rects, text.str());
}

VKAPI_ATTR void VKAPI_CALL beginRenderPass(VkCommandBuffer commands, const VkRenderPassBeginInfo* info, VkSubpassContents contents) {
    std::ostringstream text;
    for (std::uint32_t index = 0; index < info->clearValueCount; ++index) text << info->pClearValues[index].color.float32[0] << ';';
    note("beginRenderPass", commands, reinterpret_cast<std::uintptr_t>(info->renderPass), info->renderArea.extent.width, info->clearValueCount, contents, text.str(), info->pNext != nullptr);
}

VKAPI_ATTR void VKAPI_CALL beginRendering(VkCommandBuffer commands, const VkRenderingInfoKHR* info) {
    std::ostringstream text;
    for (std::uint32_t index = 0; index < info->colorAttachmentCount; ++index) text << reinterpret_cast<std::uintptr_t>(info->pColorAttachments[index].imageView) << ';';
    note("beginRendering", commands, info->renderArea.extent.width, info->colorAttachmentCount, text.str(), info->pDepthAttachment != nullptr ? reinterpret_cast<std::uintptr_t>(info->pDepthAttachment->imageView) : 0, info->pStencilAttachment != nullptr ? reinterpret_cast<std::uintptr_t>(info->pStencilAttachment->imageView) : 0);
}

VKAPI_ATTR void VKAPI_CALL blitImage(VkCommandBuffer commands, VkImage source, VkImageLayout sourceLayout, VkImage destination, VkImageLayout destinationLayout, std::uint32_t count, const VkImageBlit* regions, VkFilter filter) {
    std::ostringstream text;
    for (std::uint32_t index = 0; index < count; ++index) text << regions[index].srcOffsets[1].x << '>' << regions[index].dstOffsets[1].x << ';';
    note("blitImage", commands, reinterpret_cast<std::uintptr_t>(source), sourceLayout, reinterpret_cast<std::uintptr_t>(destination), destinationLayout, count, filter, text.str());
}

VKAPI_ATTR void VKAPI_CALL pushDescriptorSet(VkCommandBuffer commands, VkPipelineBindPoint point, VkPipelineLayout layout, std::uint32_t set, std::uint32_t count, const VkWriteDescriptorSet* writes) {
    note("pushDescriptorSet", commands, point, reinterpret_cast<std::uintptr_t>(layout), set, count, count != 0 ? writes[0].dstBinding : 0);
}

CommandFunctions capturing() {
    CommandFunctions functions{};
    functions.CmdDraw = &draw;
    functions.CmdDispatch = &dispatch;
    functions.CmdBindPipeline = &bindPipeline;
    functions.CmdSetViewport = &setViewport;
    functions.CmdSetScissorWithCount = &setScissorWithCount;
    functions.CmdPushConstants = &pushConstants;
    functions.CmdBindDescriptorSets = &bindDescriptorSets;
    functions.CmdPipelineBarrier = &pipelineBarrier;
    functions.CmdUpdateBuffer = &updateBuffer;
    functions.CmdClearAttachments = &clearAttachments;
    functions.CmdBeginRenderPass = &beginRenderPass;
    functions.CmdBeginRenderingKHR = &beginRendering;
    functions.CmdBlitImage = &blitImage;
    functions.CmdPushDescriptorSetKHR = &pushDescriptorSet;
    return functions;
}

const CommandFunctions& Real() {
    static const CommandFunctions functions = capturing();
    return functions;
}

template<typename Pfn>
Pfn thunked(const char* name, Pfn real) {
    return reinterpret_cast<Pfn>(AgcDriver::Graphics::CommandThunk(name, reinterpret_cast<PFN_vkVoidFunction>(real)));
}

struct Thunks {
    PFN_vkCmdDraw draw = thunked("vkCmdDraw", Real().CmdDraw);
    PFN_vkCmdDispatch dispatch = thunked("vkCmdDispatch", Real().CmdDispatch);
    PFN_vkCmdBindPipeline bindPipeline = thunked("vkCmdBindPipeline", Real().CmdBindPipeline);
    PFN_vkCmdSetViewport setViewport = thunked("vkCmdSetViewport", Real().CmdSetViewport);
    PFN_vkCmdSetScissorWithCount setScissorWithCount = thunked("vkCmdSetScissorWithCount", Real().CmdSetScissorWithCount);
    PFN_vkCmdPushConstants pushConstants = thunked("vkCmdPushConstants", Real().CmdPushConstants);
    PFN_vkCmdBindDescriptorSets bindDescriptorSets = thunked("vkCmdBindDescriptorSets", Real().CmdBindDescriptorSets);
    PFN_vkCmdPipelineBarrier pipelineBarrier = thunked("vkCmdPipelineBarrier", Real().CmdPipelineBarrier);
    PFN_vkCmdUpdateBuffer updateBuffer = thunked("vkCmdUpdateBuffer", Real().CmdUpdateBuffer);
    PFN_vkCmdClearAttachments clearAttachments = thunked("vkCmdClearAttachments", Real().CmdClearAttachments);
    PFN_vkCmdBeginRenderPass beginRenderPass = thunked("vkCmdBeginRenderPass", Real().CmdBeginRenderPass);
    PFN_vkCmdBeginRenderingKHR beginRendering = thunked("vkCmdBeginRenderingKHR", Real().CmdBeginRenderingKHR);
    PFN_vkCmdBlitImage blitImage = thunked("vkCmdBlitImage", Real().CmdBlitImage);
    PFN_vkCmdPushDescriptorSetKHR pushDescriptorSet = thunked("vkCmdPushDescriptorSetKHR", Real().CmdPushDescriptorSetKHR);
};

const Thunks& thunks() {
    static const Thunks table;
    return table;
}

template<typename T>
VkBuffer handle(T value) {
    return reinterpret_cast<VkBuffer>(static_cast<std::uintptr_t>(value));
}

template<typename Handle>
Handle fake(std::uint64_t value) {
    return reinterpret_cast<Handle>(static_cast<std::uintptr_t>(value * 16 + 0x1000));
}

using Op = std::function<void(VkCommandBuffer, std::mt19937&)>;

std::vector<Op> operations(bool chainedBarriers) {
    const auto& t = thunks();
    std::vector<Op> ops;
    ops.emplace_back([&t](VkCommandBuffer commands, std::mt19937& random) { t.draw(commands, random() % 100, 1 + random() % 4, random() % 7, random() % 3); });
    ops.emplace_back([&t](VkCommandBuffer commands, std::mt19937& random) { t.dispatch(commands, random() % 64, random() % 8, 1); });
    ops.emplace_back([&t](VkCommandBuffer commands, std::mt19937& random) { t.bindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, fake<VkPipeline>(random() % 9)); });
    ops.emplace_back([&t](VkCommandBuffer commands, std::mt19937& random) {
        std::vector<VkViewport> viewports(1 + random() % 3);
        for (auto& viewport : viewports) viewport = VkViewport{static_cast<float>(random() % 50), 0, static_cast<float>(random() % 500), 10, 0, 1};
        t.setViewport(commands, random() % 2, static_cast<std::uint32_t>(viewports.size()), viewports.data());
        std::memset(viewports.data(), 0x5a, viewports.size() * sizeof(VkViewport));
    });
    ops.emplace_back([&t](VkCommandBuffer commands, std::mt19937& random) {
        std::vector<VkRect2D> scissors(random() % 3);
        for (auto& scissor : scissors) scissor = VkRect2D{{static_cast<std::int32_t>(random() % 40), 0}, {static_cast<std::uint32_t>(random() % 400), 9}};
        t.setScissorWithCount(commands, static_cast<std::uint32_t>(scissors.size()), scissors.data());
        std::memset(scissors.data(), 0x5a, scissors.size() * sizeof(VkRect2D));
    });
    ops.emplace_back([&t](VkCommandBuffer commands, std::mt19937& random) {
        std::string bytes(4 * (1 + random() % 8), 'a');
        for (auto& byte : bytes) byte = static_cast<char>('a' + random() % 26);
        t.pushConstants(commands, fake<VkPipelineLayout>(3), VK_SHADER_STAGE_ALL, 4 * (random() % 4), static_cast<std::uint32_t>(bytes.size()), bytes.data());
        std::fill(bytes.begin(), bytes.end(), '#');
    });
    ops.emplace_back([&t](VkCommandBuffer commands, std::mt19937& random) {
        std::vector<VkDescriptorSet> sets(1 + random() % 3);
        for (auto& set : sets) set = fake<VkDescriptorSet>(random() % 99);
        std::vector<std::uint32_t> offsets(random() % 3);
        for (auto& offset : offsets) offset = random() % 4096;
        t.bindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_COMPUTE, fake<VkPipelineLayout>(5), random() % 2, static_cast<std::uint32_t>(sets.size()), sets.data(), static_cast<std::uint32_t>(offsets.size()), offsets.data());
        std::memset(sets.data(), 0, sets.size() * sizeof(VkDescriptorSet));
        std::memset(offsets.data(), 0, offsets.size() * sizeof(std::uint32_t));
    });
    ops.emplace_back([&t, chainedBarriers](VkCommandBuffer commands, std::mt19937& random) {
        std::vector<VkMemoryBarrier> memory(random() % 2, VkMemoryBarrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr, static_cast<VkAccessFlags>(random() % 64), static_cast<VkAccessFlags>(random() % 64)});
        std::vector<VkBufferMemoryBarrier> buffers(random() % 3);
        for (auto& barrier : buffers) barrier = VkBufferMemoryBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, nullptr, 0, 0, 0, 0, fake<VkBuffer>(random() % 30), 0, random() % 1000};
        std::vector<VkImageMemoryBarrier> images(random() % 3);
        for (auto& barrier : images) {
            barrier = VkImageMemoryBarrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.image = fake<VkImage>(random() % 30);
            barrier.newLayout = static_cast<VkImageLayout>(random() % 8);
        }
        VkMemoryBarrier2 chain{};
        chain.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
        if (chainedBarriers && !images.empty() && random() % 4 == 0) images.front().pNext = &chain;
        t.pipelineBarrier(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, static_cast<std::uint32_t>(memory.size()), memory.data(), static_cast<std::uint32_t>(buffers.size()), buffers.data(), static_cast<std::uint32_t>(images.size()), images.data());
        std::memset(memory.data(), 0, memory.size() * sizeof(VkMemoryBarrier));
        std::memset(buffers.data(), 0, buffers.size() * sizeof(VkBufferMemoryBarrier));
        std::memset(images.data(), 0, images.size() * sizeof(VkImageMemoryBarrier));
    });
    ops.emplace_back([&t](VkCommandBuffer commands, std::mt19937& random) {
        std::string bytes(4 * (1 + random() % 6), 'q');
        for (auto& byte : bytes) byte = static_cast<char>('A' + random() % 26);
        t.updateBuffer(commands, fake<VkBuffer>(random() % 30), 4 * (random() % 8), bytes.size(), bytes.data());
        std::fill(bytes.begin(), bytes.end(), '#');
    });
    ops.emplace_back([&t](VkCommandBuffer commands, std::mt19937& random) {
        std::vector<VkClearAttachment> clears(1 + random() % 2);
        for (auto& clear : clears) {
            clear = VkClearAttachment{VK_IMAGE_ASPECT_COLOR_BIT, static_cast<std::uint32_t>(random() % 4), {}};
            clear.clearValue.color.uint32[0] = random() % 1000;
        }
        std::vector<VkClearRect> rects(1 + random() % 2, VkClearRect{{{0, 0}, {static_cast<std::uint32_t>(random() % 640), static_cast<std::uint32_t>(random() % 480)}}, 0, 1});
        t.clearAttachments(commands, static_cast<std::uint32_t>(clears.size()), clears.data(), static_cast<std::uint32_t>(rects.size()), rects.data());
        std::memset(clears.data(), 0, clears.size() * sizeof(VkClearAttachment));
        std::memset(rects.data(), 0, rects.size() * sizeof(VkClearRect));
    });
    ops.emplace_back([&t](VkCommandBuffer commands, std::mt19937& random) {
        std::vector<VkClearValue> values(random() % 3);
        for (auto& value : values) value.color.float32[0] = static_cast<float>(random() % 100);
        VkRenderPassBeginInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr, fake<VkRenderPass>(random() % 9), fake<VkFramebuffer>(1), {{0, 0}, {static_cast<std::uint32_t>(random() % 800), 600}}, static_cast<std::uint32_t>(values.size()), values.data()};
        t.beginRenderPass(commands, &info, VK_SUBPASS_CONTENTS_INLINE);
        std::memset(values.data(), 0, values.size() * sizeof(VkClearValue));
        info = {};
    });
    ops.emplace_back([&t](VkCommandBuffer commands, std::mt19937& random) {
        std::vector<VkRenderingAttachmentInfoKHR> colors(random() % 3);
        for (auto& color : colors) {
            color = VkRenderingAttachmentInfoKHR{};
            color.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
            color.imageView = fake<VkImageView>(random() % 50);
        }
        VkRenderingAttachmentInfoKHR depth{};
        depth.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
        depth.imageView = fake<VkImageView>(70 + random() % 5);
        VkRenderingInfoKHR info{};
        info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
        info.renderArea.extent = {static_cast<std::uint32_t>(random() % 900), 10};
        info.colorAttachmentCount = static_cast<std::uint32_t>(colors.size());
        info.pColorAttachments = colors.data();
        if (random() % 2 == 0) info.pDepthAttachment = &depth;
        t.beginRendering(commands, &info);
        std::memset(colors.data(), 0, colors.size() * sizeof(VkRenderingAttachmentInfoKHR));
        depth = {};
        info = {};
    });
    ops.emplace_back([&t](VkCommandBuffer commands, std::mt19937& random) {
        std::vector<VkImageBlit> regions(1 + random() % 2);
        for (auto& region : regions) {
            region = VkImageBlit{};
            region.srcOffsets[1].x = static_cast<std::int32_t>(random() % 256);
            region.dstOffsets[1].x = static_cast<std::int32_t>(random() % 128);
        }
        t.blitImage(commands, fake<VkImage>(1), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, fake<VkImage>(2), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<std::uint32_t>(regions.size()), regions.data(), random() % 2 == 0 ? VK_FILTER_LINEAR : VK_FILTER_NEAREST);
        std::memset(regions.data(), 0, regions.size() * sizeof(VkImageBlit));
    });
    return ops;
}

std::vector<std::string> runProgram(std::uint32_t seed, std::size_t length, VkCommandBuffer commands, bool chainedBarriers, std::function<void()> after = {}) {
    log.clear();
    std::mt19937 random(seed);
    const auto ops = operations(chainedBarriers);
    for (std::size_t step = 0; step < length; ++step) ops[random() % ops.size()](commands, random);
    if (after) after();
    return log;
}

std::vector<std::string> retarget(std::vector<std::string> lines, VkCommandBuffer from, VkCommandBuffer to) {
    const auto source = "@" + std::to_string(id(from)) + " ";
    const auto target = "@" + std::to_string(id(to)) + " ";
    for (auto& line : lines) {
        if (const auto at = line.find(source); at != std::string::npos) line.replace(at, source.size(), target);
    }
    return lines;
}

std::string firstDifference(const std::vector<std::string>& expected, const std::vector<std::string>& actual) {
    for (std::size_t index = 0; index < std::min(expected.size(), actual.size()); ++index) {
        if (expected[index] != actual[index]) return "at " + std::to_string(index) + ": expected '" + expected[index] + "', got '" + actual[index] + "'";
    }
    return "lengths " + std::to_string(expected.size()) + " and " + std::to_string(actual.size());
}

void StreamReplayRecordsWhatDirectRecords() {
    const auto direct = fakeBuffer(3);
    const auto target = fakeBuffer(4);
    for (std::uint32_t seed = 1; seed <= 40; ++seed) {
        const auto expected = retarget(runProgram(seed, 200, direct, false), direct, target);
        CommandStream stream(Real());
        const auto streamed = runProgram(seed, 200, stream.Handle(), false);
        Require(streamed.empty(), "a streamed command reached the driver while recording: " + (streamed.empty() ? std::string() : streamed.front()));
        stream.Seal();
        log.clear();
        AgcDriver::Graphics::Replay(stream, target, Real());
        Require(log == expected, "seed " + std::to_string(seed) + ": the replay differs from direct recording " + firstDifference(expected, log));
        Require(stream.Ops() == expected.size(), "the stream counted " + std::to_string(stream.Ops()) + " ops for " + std::to_string(expected.size()) + " commands");
    }
}

class FakeMaterializer final : public StreamMaterializer {
public:
    explicit FakeMaterializer(VkCommandBuffer buffer) : buffer(buffer) {}
    VkCommandBuffer Begin(CommandStream&) override {
        ++begun;
        note("begin", buffer);
        return buffer;
    }
    VkCommandBuffer buffer;
    int begun = 0;
};

void DemotionReplaysThePrefixInOrder() {
    const auto direct = fakeBuffer(5);
    const auto target = fakeBuffer(6);
    std::size_t demotedSeeds = 0;
    for (std::uint32_t seed = 1; seed <= 40; ++seed) {
        auto expected = retarget(runProgram(seed, 120, direct, true), direct, target);
        FakeMaterializer materializer(target);
        CommandStream stream(Real(), nullptr, &materializer);
        const auto lines = runProgram(seed, 120, stream.Handle(), true);
        if (stream.Materialized() == VK_NULL_HANDLE) {
            Require(lines.empty(), "a stream that never demoted reached the driver");
            continue;
        }
        ++demotedSeeds;
        Require(materializer.begun == 1, "the stream began " + std::to_string(materializer.begun) + " command buffers");
        Require(!lines.empty() && lines.front() == "begin@" + std::to_string(id(target)), "the demotion did not begin its command buffer first");
        const std::vector<std::string> replayed(lines.begin() + 1, lines.end());
        Require(replayed == expected, "seed " + std::to_string(seed) + ": the demoted recording differs from direct recording " + firstDifference(expected, replayed));
        Require(std::string_view(stream.DemotedBy().command) == "vkCmdPipelineBarrier" && stream.DemotedBy().chained == VK_STRUCTURE_TYPE_MEMORY_BARRIER_2, "the demotion was not attributed to the chained barrier");
        Require(stream.Bytes() == 0 || stream.Materialized() != VK_NULL_HANDLE, "the stream kept records after its demotion");
        Require(Throws<std::logic_error>([&] {
            stream.Seal();
            AgcDriver::Graphics::Replay(stream, target, Real());
        }), "a demoted stream was replayed");
    }
    Require(demotedSeeds >= 10, "only " + std::to_string(demotedSeeds) + " of 40 programs demoted");
}

void PointerCommandsWithoutACopyDemote() {
    const auto target = fakeBuffer(8);
    FakeMaterializer materializer(target);
    CommandStream stream(Real(), nullptr, &materializer);
    log.clear();
    thunks().draw(stream.Handle(), 3, 1, 0, 0);
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstBinding = 7;
    Require(log.empty(), "a streamed draw reached the driver");
    thunks().pushDescriptorSet(stream.Handle(), VK_PIPELINE_BIND_POINT_COMPUTE, fake<VkPipelineLayout>(2), 0, 1, &write);
    thunks().draw(stream.Handle(), 4, 1, 0, 0);
    const std::vector<std::string> expected{"begin@8", "draw@8 3 1 0 0", "pushDescriptorSet@8 1 4128 0 1 7", "draw@8 4 1 0 0"};
    Require(log == expected, "the push descriptor demotion recorded " + firstDifference(expected, log));
    Require(std::string_view(stream.DemotedBy().command) == "vkCmdPushDescriptorSetKHR" && stream.DemotedBy().chained == VK_STRUCTURE_TYPE_MAX_ENUM, "the push descriptor demotion was not attributed to its command");
}

void DemotionWithoutAMaterializerPoisons() {
    CommandStream stream(Real());
    thunks().draw(stream.Handle(), 1, 1, 0, 0);
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    Require(Throws<std::logic_error>([&] { thunks().pushDescriptorSet(stream.Handle(), VK_PIPELINE_BIND_POINT_COMPUTE, fake<VkPipelineLayout>(2), 0, 1, &write); }), "a stream with no materializer demoted");
    Require(stream.Poisoned(), "a failed demotion left the stream unpoisoned");
}

void DirectCallsReachTheirOwnEntryPoint() {
    log.clear();
    thunks().draw(fakeBuffer(9), 1, 2, 3, 4);
    Require(log == std::vector<std::string>{"draw@9 1 2 3 4"}, "a direct call did not reach the real entry point: " + (log.empty() ? std::string("nothing") : log.front()));
}

template<int K>
VKAPI_ATTR void VKAPI_CALL markerSlot(VkCommandBuffer commands, const void*) {
    note("marker", commands, K);
}

template<int... K>
std::array<PFN_vkCmdSetCheckpointNV, sizeof...(K)> markers(std::integer_sequence<int, K...>) {
    return {&markerSlot<K>...};
}

void EntryPointsBeyondTheSlotsAreRefused() {
    const auto reals = markers(std::make_integer_sequence<int, 9>{});
    std::vector<PFN_vkVoidFunction> given;
    for (std::size_t index = 0; index + 1 < reals.size(); ++index) {
        const auto thunk = AgcDriver::Graphics::CommandThunk("vkCmdSetCheckpointNV", reinterpret_cast<PFN_vkVoidFunction>(reals[index]));
        Require(AgcDriver::Graphics::IsCommandThunk("vkCmdSetCheckpointNV", thunk), "a slot's entry point was not a thunk");
        for (const auto earlier : given) Require(earlier != thunk, "two entry points shared a thunk slot");
        given.push_back(thunk);
        log.clear();
        reinterpret_cast<PFN_vkCmdSetCheckpointNV>(thunk)(fakeBuffer(2), nullptr);
        Require(log == std::vector<std::string>{"marker@2 " + std::to_string(index)}, "slot " + std::to_string(index) + " reached the wrong entry point");
    }
    Require(AgcDriver::Graphics::CommandThunk("vkCmdSetCheckpointNV", reinterpret_cast<PFN_vkVoidFunction>(reals[0])) == given[0], "the same entry point took a second slot");
    Require(Throws<std::runtime_error>([&] { AgcDriver::Graphics::CommandThunk("vkCmdSetCheckpointNV", reinterpret_cast<PFN_vkVoidFunction>(reals.back())); }), "a ninth entry point was given a thunk");
    Require(AgcDriver::Graphics::CommandThunkSlots(AgcDriver::Graphics::CommandId::CmdSetCheckpointNV) == 8, "the slots in use were miscounted");
}

void EveryListedCommandHasAThunk() {
    std::size_t names = 0;
    for (const auto name : AgcDriver::Graphics::CommandNames()) {
        ++names;
        Require(AgcDriver::Graphics::IsCommandName(name), std::string(name) + " is listed but is not a command name");
        Require(AgcDriver::Graphics::CommandNames()[static_cast<std::size_t>(AgcDriver::Graphics::CommandFor(name))] == name, std::string(name) + " maps to another command");
    }
    Require(names == AgcDriver::Graphics::CommandCount, "the name list and the command ids differ in length");
    Require(Throws<std::logic_error>([] { static_cast<void>(AgcDriver::Graphics::CommandFor("vkCmdNotACommand")); }), "an unknown command resolved");
    Require(AgcDriver::Graphics::CommandThunk("vkCmdDraw", nullptr) == nullptr, "a missing entry point was given a thunk");
    Require(!AgcDriver::Graphics::IsCommandName("vkCreateBuffer"), "a non-command was taken for a command");
}

VKAPI_ATTR void VKAPI_CALL emptyDraw(VkCommandBuffer, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) {}

void DirectModeCost() {
    const auto thunk = thunked("vkCmdDraw", &emptyDraw);
    volatile PFN_vkCmdDraw raw = &emptyDraw;
    volatile PFN_vkCmdDraw through = thunk;
    constexpr int Calls = 20'000'000;
    const auto time = [&](volatile PFN_vkCmdDraw& function) {
        const auto start = std::chrono::steady_clock::now();
        for (int call = 0; call < Calls; ++call) function(fakeBuffer(1), static_cast<std::uint32_t>(call), 1, 0, 0);
        return std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count() / Calls;
    };
    double rawNs = 1e9;
    double thunkNs = 1e9;
    for (int round = 0; round < 5; ++round) {
        rawNs = std::min(rawNs, time(raw));
        thunkNs = std::min(thunkNs, time(through));
    }
    std::printf("direct-mode cost: raw %.2f ns/call, thunk %.2f ns/call, added %.2f ns/call (best of 5 x %d calls)\n", rawNs, thunkNs, thunkNs - rawNs, Calls);
}

}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--bench") {
            DirectModeCost();
            return 0;
        }
        EveryListedCommandHasAThunk();
        DirectCallsReachTheirOwnEntryPoint();
        StreamReplayRecordsWhatDirectRecords();
        DemotionReplaysThePrefixInOrder();
        PointerCommandsWithoutACopyDemote();
        DemotionWithoutAMaterializerPoisons();
        EntryPointsBeyondTheSlotsAreRefused();
        std::cout << "command thunks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "command thunks: %s\n", error.what());
        return 1;
    }
}
