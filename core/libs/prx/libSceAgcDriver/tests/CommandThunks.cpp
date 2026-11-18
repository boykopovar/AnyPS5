#include "prx/libSceAgcDriver/Graphics/include/CommandList.hpp"
#include "FakeHandles.hpp"
#include "prx/libSceAgcDriver/Graphics/include/CommandStream.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <algorithm>
#include <exception>
#include <functional>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <thread>
#include <utility>
#include <vector>

namespace {

template<typename T>
void fillBytes(std::vector<T>& values, int byte) {
    if (!values.empty()) std::memset(static_cast<void*>(values.data()), byte, values.size() * sizeof(T));
}

}

namespace AgcDriver::Graphics {

std::uint64_t& DeviceProcLookups() {
    thread_local std::uint64_t count = 0;
    return count;
}

}

namespace {

using AgcDriver::Graphics::CommandFunctions;
using AgcDriver::Graphics::CommandId;
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
        fillBytes(viewports, 0x5a);
    });
    ops.emplace_back([&t](VkCommandBuffer commands, std::mt19937& random) {
        std::vector<VkRect2D> scissors(random() % 3);
        for (auto& scissor : scissors) scissor = VkRect2D{{static_cast<std::int32_t>(random() % 40), 0}, {static_cast<std::uint32_t>(random() % 400), 9}};
        t.setScissorWithCount(commands, static_cast<std::uint32_t>(scissors.size()), scissors.data());
        fillBytes(scissors, 0x5a);
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
        fillBytes(sets, 0);
        fillBytes(offsets, 0);
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
        fillBytes(memory, 0);
        fillBytes(buffers, 0);
        fillBytes(images, 0);
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
        fillBytes(clears, 0);
        fillBytes(rects, 0);
    });
    ops.emplace_back([&t](VkCommandBuffer commands, std::mt19937& random) {
        std::vector<VkClearValue> values(random() % 3);
        for (auto& value : values) value.color.float32[0] = static_cast<float>(random() % 100);
        VkRenderPassBeginInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, nullptr, fake<VkRenderPass>(random() % 9), fake<VkFramebuffer>(1), {{0, 0}, {static_cast<std::uint32_t>(random() % 800), 600}}, static_cast<std::uint32_t>(values.size()), values.data()};
        t.beginRenderPass(commands, &info, VK_SUBPASS_CONTENTS_INLINE);
        fillBytes(values, 0);
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
        fillBytes(colors, 0);
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
        fillBytes(regions, 0);
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
    const auto direct = FakeCommandBuffer(3);
    const auto target = FakeCommandBuffer(4);
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
    const auto direct = FakeCommandBuffer(5);
    const auto target = FakeCommandBuffer(6);
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

void PushDescriptorsDemoteOnlyWhatTheyCannotCopy() {
    const auto target = FakeCommandBuffer(8);
    VkDescriptorImageInfo image{fake<VkSampler>(3), fake<VkImageView>(4), VK_IMAGE_LAYOUT_GENERAL};
    {
        FakeMaterializer materializer(target);
        CommandStream stream(Real(), nullptr, &materializer);
        log.clear();
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstBinding = 7;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.pImageInfo = &image;
        thunks().pushDescriptorSet(stream.Handle(), VK_PIPELINE_BIND_POINT_COMPUTE, fake<VkPipelineLayout>(2), 0, 1, &write);
        Require(log.empty() && stream.Materialized() == VK_NULL_HANDLE && stream.Ops() == 1, "a copyable push descriptor write did not stream");
    }
    VkWriteDescriptorSetInlineUniformBlock inlineBlock{};
    inlineBlock.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK;
    VkWriteDescriptorSetAccelerationStructureKHR structures{};
    structures.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    struct Case {
        const char* what;
        VkDescriptorType type;
        const void* next;
        bool switchedOff;
        VkStructureType chained;
    };
    const std::array<Case, 4> cases{{
        {"an inline uniform block", VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK, &inlineBlock, false, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK},
        {"an acceleration structure", VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, &structures, false, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR},
        {"a descriptor type with no array to copy", VK_DESCRIPTOR_TYPE_MUTABLE_EXT, nullptr, false, VK_STRUCTURE_TYPE_MAX_ENUM},
        {"APS5_NO_STREAMED_PUSH_DESCRIPTORS", VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, true, VK_STRUCTURE_TYPE_MAX_ENUM},
    }};
    for (const auto& item : cases) {
        AgcDriver::Graphics::RuntimeSwitches::Assign(AgcDriver::Graphics::RuntimeSwitch::NoStreamedPushDescriptors, item.switchedOff);
        FakeMaterializer materializer(target);
        CommandStream stream(Real(), nullptr, &materializer);
        log.clear();
        thunks().draw(stream.Handle(), 3, 1, 0, 0);
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.pNext = item.next;
        write.dstBinding = 7;
        write.descriptorCount = 1;
        write.descriptorType = item.type;
        write.pImageInfo = &image;
        Require(log.empty(), "a streamed draw reached the driver");
        thunks().pushDescriptorSet(stream.Handle(), VK_PIPELINE_BIND_POINT_COMPUTE, fake<VkPipelineLayout>(2), 0, 1, &write);
        thunks().draw(stream.Handle(), 4, 1, 0, 0);
        AgcDriver::Graphics::RuntimeSwitches::Assign(AgcDriver::Graphics::RuntimeSwitch::NoStreamedPushDescriptors, false);
        const std::vector<std::string> expected{"begin@8", "draw@8 3 1 0 0", "pushDescriptorSet@8 1 4128 0 1 7", "draw@8 4 1 0 0"};
        Require(log == expected, std::string(item.what) + ": the push descriptor demotion recorded " + firstDifference(expected, log));
        Require(std::string_view(stream.DemotedBy().command) == "vkCmdPushDescriptorSetKHR" && stream.DemotedBy().chained == item.chained, std::string(item.what) + ": the push descriptor demotion was not attributed to its command and chain");
    }
}

void DemotionWithoutAMaterializerPoisons() {
    CommandStream stream(Real());
    thunks().draw(stream.Handle(), 1, 1, 0, 0);
    VkWriteDescriptorSetInlineUniformBlock inlineBlock{};
    inlineBlock.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_INLINE_UNIFORM_BLOCK;
    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.pNext = &inlineBlock;
    write.descriptorType = VK_DESCRIPTOR_TYPE_INLINE_UNIFORM_BLOCK;
    Require(Throws<std::logic_error>([&] { thunks().pushDescriptorSet(stream.Handle(), VK_PIPELINE_BIND_POINT_COMPUTE, fake<VkPipelineLayout>(2), 0, 1, &write); }), "a stream with no materializer demoted");
    Require(stream.Poisoned(), "a failed demotion left the stream unpoisoned");
}

void DirectCallsReachTheirOwnEntryPoint() {
    log.clear();
    thunks().draw(FakeCommandBuffer(9), 1, 2, 3, 4);
    Require(log == std::vector<std::string>{"draw@9 1 2 3 4"}, "a direct call did not reach the real entry point: " + (log.empty() ? std::string("nothing") : log.front()));
}

template<int K>
VKAPI_ATTR void VKAPI_CALL stencilSlot(VkCommandBuffer commands, VkStencilFaceFlags, std::uint32_t reference) {
    note("stencil", commands, K, reference);
}

template<int... K>
std::array<PFN_vkCmdSetStencilReference, sizeof...(K)> stencils(std::integer_sequence<int, K...>) {
    return {&stencilSlot<K>...};
}

struct alignas(16) FakeDispatchable {
    const void* key;
};

std::atomic<std::uint64_t> reusedRight{0};
std::atomic<std::uint64_t> reusedWrong{0};

VKAPI_ATTR void VKAPI_CALL reusedFirst(VkCommandBuffer, VkStencilFaceFlags, std::uint32_t) {
    reusedRight.fetch_add(1, std::memory_order_relaxed);
}

VKAPI_ATTR void VKAPI_CALL reusedSecond(VkCommandBuffer, VkStencilFaceFlags, std::uint32_t) {
    reusedWrong.fetch_add(1, std::memory_order_relaxed);
}

VkDevice deviceOf(FakeDispatchable& dispatchable) {
    return reinterpret_cast<VkDevice>(&dispatchable);
}

VkCommandBuffer commandsOf(FakeDispatchable& dispatchable) {
    return reinterpret_cast<VkCommandBuffer>(&dispatchable);
}

void EntryPointsBeyondTheSlotsAreKeyedByDevice() {
    using AgcDriver::Graphics::CommandThunk;
    constexpr const char* Name = "vkCmdSetStencilReference";
    const auto reals = stencils(std::make_integer_sequence<int, 11>{});
    const auto entry = [&](std::size_t index) { return reinterpret_cast<PFN_vkVoidFunction>(reals[index]); };
    Require(AgcDriver::Graphics::CommandThunkSlots(CommandId::CmdSetStencilReference) == 0, "another test used the stencil reference slots");
    std::vector<PFN_vkVoidFunction> given;
    for (std::size_t index = 0; index < 8; ++index) {
        const auto thunk = CommandThunk(Name, entry(index));
        Require(AgcDriver::Graphics::IsCommandThunk(Name, thunk), "a slot's entry point was not a thunk");
        for (const auto earlier : given) Require(earlier != thunk, "two entry points shared a thunk slot");
        given.push_back(thunk);
        log.clear();
        reinterpret_cast<PFN_vkCmdSetStencilReference>(thunk)(FakeCommandBuffer(2), VK_STENCIL_FACE_FRONT_BIT, 9);
        Require(log == std::vector<std::string>{"stencil@2 " + std::to_string(index) + " 9"}, "slot " + std::to_string(index) + " reached the wrong entry point");
    }
    Require(CommandThunk(Name, entry(0)) == given[0], "the same entry point took a second slot");
    Require(AgcDriver::Graphics::CommandThunkSlots(CommandId::CmdSetStencilReference) == 8, "the slots in use were miscounted");
    Require(Throws<std::runtime_error>([&] { CommandThunk(Name, entry(8)); }), "a ninth entry point with no device to key it by was given a thunk");
    static const int keys[3] = {};
    FakeDispatchable first{&keys[0]};
    FakeDispatchable second{&keys[1]};
    FakeDispatchable firstCommands{&keys[0]};
    FakeDispatchable secondCommands{&keys[1]};
    FakeDispatchable strangerCommands{&keys[2]};
    const auto keyed = CommandThunk(Name, entry(8), deviceOf(first));
    Require(AgcDriver::Graphics::IsCommandThunk(Name, keyed) && std::find(given.begin(), given.end(), keyed) == given.end(), "a ninth entry point on a device was not given the keyed thunk");
    Require(CommandThunk(Name, entry(9), deviceOf(second)) == keyed, "a second device's entry point was given another thunk");
    Require(CommandThunk(Name, entry(8), deviceOf(first)) == keyed && AgcDriver::Graphics::CommandDeviceEntries(CommandId::CmdSetStencilReference) == 2, "a device's repeated entry point was registered twice");
    Require(CommandThunk(Name, entry(0), deviceOf(first)) == given[0], "an entry point that holds a slot was keyed instead");
    const auto call = reinterpret_cast<PFN_vkCmdSetStencilReference>(keyed);
    log.clear();
    call(commandsOf(firstCommands), VK_STENCIL_FACE_FRONT_BIT, 1);
    call(commandsOf(secondCommands), VK_STENCIL_FACE_BACK_BIT, 2);
    const std::vector<std::string> routed{"stencil@" + std::to_string(id(commandsOf(firstCommands))) + " 8 1", "stencil@" + std::to_string(id(commandsOf(secondCommands))) + " 9 2"};
    Require(log == routed, "the keyed thunk did not route each command buffer to its device's entry point: " + firstDifference(routed, log));
    Require(Throws<std::logic_error>([&] { call(commandsOf(strangerCommands), VK_STENCIL_FACE_FRONT_BIT, 3); }), "a command buffer of an unregistered device reached an entry point");
    Require(Throws<std::runtime_error>([&] { CommandThunk(Name, entry(10), deviceOf(first)); }), "one device gave two entry points for one command and both were kept");
    {
        CommandFunctions functions{};
        functions.CmdSetStencilReference = reals[10];
        CommandStream stream(functions);
        log.clear();
        call(stream.Handle(), VK_STENCIL_FACE_FRONT_BIT, 4);
        Require(log.empty() && stream.Ops() == 1, "the keyed thunk did not stream a stream handle");
        stream.Seal();
        AgcDriver::Graphics::Replay(stream, FakeCommandBuffer(3), functions);
        Require(log == std::vector<std::string>{"stencil@3 10 4"}, "the keyed thunk's streamed call replayed wrongly");
    }
    AgcDriver::Graphics::ForgetCommandDevice(deviceOf(first));
    Require(AgcDriver::Graphics::CommandDeviceEntries(CommandId::CmdSetStencilReference) == 1, "a forgotten device kept its entry");
    Require(Throws<std::logic_error>([&] { call(commandsOf(firstCommands), VK_STENCIL_FACE_FRONT_BIT, 5); }), "a forgotten device's command buffer still reached its entry point");
    Require(CommandThunk(Name, entry(10), deviceOf(first)) == keyed, "a device created where a forgotten one was could not register its entry point");
    log.clear();
    call(commandsOf(firstCommands), VK_STENCIL_FACE_FRONT_BIT, 6);
    call(commandsOf(secondCommands), VK_STENCIL_FACE_FRONT_BIT, 7);
    Require(log.size() == 2 && log[0].ends_with(" 10 6") && log[1].ends_with(" 9 7"), "the keyed thunk routed wrongly after a device was replaced");
    AgcDriver::Graphics::ForgetCommandDevice(deviceOf(first));
    AgcDriver::Graphics::ForgetCommandDevice(deviceOf(second));
    Require(AgcDriver::Graphics::CommandDeviceEntries(CommandId::CmdSetStencilReference) == 0, "forgetting every device left entries");
    const auto peak = AgcDriver::Graphics::CommandDeviceStorage(CommandId::CmdSetStencilReference);
    Require(peak == 2, "two live devices kept " + std::to_string(peak) + " entries of storage");
    constexpr std::size_t Cycles = 200;
    static const int cycleKeys[Cycles] = {};
    std::vector<FakeDispatchable> devices;
    std::vector<FakeDispatchable> buffers;
    for (std::size_t index = 0; index < Cycles; ++index) {
        devices.push_back(FakeDispatchable{&cycleKeys[index]});
        buffers.push_back(FakeDispatchable{&cycleKeys[index]});
    }
    for (std::size_t cycle = 0; cycle + 1 < Cycles; cycle += 2) {
        Require(CommandThunk(Name, entry(8), deviceOf(devices[cycle])) == keyed && CommandThunk(Name, entry(9), deviceOf(devices[cycle + 1])) == keyed, "a device created after others were destroyed was not given the keyed thunk");
        log.clear();
        call(commandsOf(buffers[cycle]), VK_STENCIL_FACE_FRONT_BIT, 1);
        call(commandsOf(buffers[cycle + 1]), VK_STENCIL_FACE_FRONT_BIT, 2);
        Require(log.size() == 2 && log[0].ends_with(" 8 1") && log[1].ends_with(" 9 2"), "cycle " + std::to_string(cycle) + " routed a command buffer to the wrong device's entry point");
        if (cycle > 0) Require(Throws<std::logic_error>([&] { call(commandsOf(buffers[cycle - 1]), VK_STENCIL_FACE_FRONT_BIT, 3); }), "a destroyed device's command buffer reached an entry point in cycle " + std::to_string(cycle));
        Require(AgcDriver::Graphics::CommandDeviceEntries(CommandId::CmdSetStencilReference) == 2, "cycle " + std::to_string(cycle) + " miscounted the live devices");
        AgcDriver::Graphics::ForgetCommandDevice(deviceOf(devices[cycle]));
        AgcDriver::Graphics::ForgetCommandDevice(deviceOf(devices[cycle + 1]));
        Require(AgcDriver::Graphics::CommandDeviceStorage(CommandId::CmdSetStencilReference) == peak, "creating and destroying devices grew the entries to " + std::to_string(AgcDriver::Graphics::CommandDeviceStorage(CommandId::CmdSetStencilReference)) + " after cycle " + std::to_string(cycle));
    }
    Require(AgcDriver::Graphics::CommandDeviceEntries(CommandId::CmdSetStencilReference) == 0, "forgetting every cycled device left entries");
    static const int raceKeys[2] = {};
    FakeDispatchable racedFirst{&raceKeys[0]};
    FakeDispatchable racedSecond{&raceKeys[1]};
    FakeDispatchable racedCommands{&raceKeys[0]};
    const auto firstReal = reinterpret_cast<PFN_vkVoidFunction>(&reusedFirst);
    const auto secondReal = reinterpret_cast<PFN_vkVoidFunction>(&reusedSecond);
    Require(CommandThunk(Name, firstReal, deviceOf(racedFirst)) == keyed, "the raced device was not given the keyed thunk");
    std::atomic<bool> stop{false};
    std::atomic<std::uint64_t> reuses{0};
    std::thread writer([&] {
        while (!stop.load(std::memory_order_relaxed)) {
            AgcDriver::Graphics::ForgetCommandDevice(deviceOf(racedFirst));
            CommandThunk(Name, secondReal, deviceOf(racedSecond));
            AgcDriver::Graphics::ForgetCommandDevice(deviceOf(racedSecond));
            CommandThunk(Name, firstReal, deviceOf(racedFirst));
            reuses.fetch_add(1, std::memory_order_relaxed);
        }
    });
    std::uint64_t missing = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < deadline && reusedWrong.load(std::memory_order_relaxed) == 0) {
        for (int index = 0; index < 4096; ++index) {
            try {
                call(commandsOf(racedCommands), VK_STENCIL_FACE_FRONT_BIT, 1);
            } catch (const std::logic_error&) {
                ++missing;
            }
        }
    }
    stop.store(true);
    writer.join();
    Require(reusedWrong.load() == 0, "a lookup racing a forget and reuse of its node called another device's entry point " + std::to_string(reusedWrong.load()) + " times (" + std::to_string(reusedRight.load()) + " right, " + std::to_string(reuses.load()) + " reuses)");
    Require(reusedRight.load() > 0 && reuses.load() > 0, "the forget and reuse race never ran both sides");
    Require(AgcDriver::Graphics::CommandDeviceStorage(CommandId::CmdSetStencilReference) == peak, "the forget and reuse race grew the entries");
    AgcDriver::Graphics::ForgetCommandDevice(deviceOf(racedFirst));
    std::cout << "keyed race: " << reusedRight.load() << " right, " << missing << " missing, " << reuses.load() << " reuses\n";
}

void EveryListedCommandStreams() {
    for (std::size_t index = 0; index < AgcDriver::Graphics::CommandCount; ++index) {
        Require(AgcDriver::Graphics::CommandStreams(static_cast<CommandId>(index)), std::string(AgcDriver::Graphics::CommandNames()[index]) + " is listed but has no streamer, so every batch that records it demotes");
    }
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

std::string captured;
std::vector<std::pair<std::size_t, std::string>> capturedCommands;

template<typename T>
void put(const T& value) {
    static_assert(std::is_trivially_copyable_v<T>);
    captured.append(reinterpret_cast<const char*>(&value), sizeof(T));
}

template<typename T>
void putArray(const T* values, std::uint32_t count) {
    put(count);
    put(values != nullptr);
    for (std::uint32_t index = 0; index < count; ++index) put(values[index]);
}

void putNext(const void* next) {
    put(next == nullptr);
}

void putTag(const char* name) {
    capturedCommands.emplace_back(captured.size(), name);
    captured += '|';
    captured += name;
    captured += '|';
}

void putMemory(const VkMemoryBarrier& barrier) {
    put(barrier.sType);
    putNext(barrier.pNext);
    put(barrier.srcAccessMask);
    put(barrier.dstAccessMask);
}

void putBuffer(const VkBufferMemoryBarrier& barrier) {
    put(barrier.sType);
    putNext(barrier.pNext);
    put(barrier.srcAccessMask);
    put(barrier.dstAccessMask);
    put(barrier.srcQueueFamilyIndex);
    put(barrier.dstQueueFamilyIndex);
    put(barrier.buffer);
    put(barrier.offset);
    put(barrier.size);
}

void putImage(const VkImageMemoryBarrier& barrier) {
    put(barrier.sType);
    putNext(barrier.pNext);
    put(barrier.srcAccessMask);
    put(barrier.dstAccessMask);
    put(barrier.oldLayout);
    put(barrier.newLayout);
    put(barrier.srcQueueFamilyIndex);
    put(barrier.dstQueueFamilyIndex);
    put(barrier.image);
    put(barrier.subresourceRange);
}

void putAttachment(const VkRenderingAttachmentInfoKHR* attachment) {
    put(attachment != nullptr);
    if (attachment == nullptr) return;
    put(attachment->sType);
    putNext(attachment->pNext);
    put(attachment->imageView);
    put(attachment->imageLayout);
    put(attachment->resolveMode);
    put(attachment->resolveImageView);
    put(attachment->resolveImageLayout);
    put(attachment->loadOp);
    put(attachment->storeOp);
    put(attachment->clearValue);
}

template<CommandId Id>
struct Exact;

#define APS5_EXACT_RANGE(name, T) \
    template<> \
    struct Exact<CommandId::name> { \
        static VKAPI_ATTR void VKAPI_CALL Run(VkCommandBuffer commands, std::uint32_t first, std::uint32_t count, const T* values) { \
            putTag(#name); \
            put(commands); \
            put(first); \
            putArray(values, count); \
        } \
    };
APS5_EXACT_RANGE(CmdSetViewport, VkViewport)
APS5_EXACT_RANGE(CmdSetScissor, VkRect2D)
APS5_EXACT_RANGE(CmdSetColorWriteMaskEXT, VkColorComponentFlags)
#undef APS5_EXACT_RANGE

#define APS5_EXACT_COUNT(name, T) \
    template<> \
    struct Exact<CommandId::name> { \
        static VKAPI_ATTR void VKAPI_CALL Run(VkCommandBuffer commands, std::uint32_t count, const T* values) { \
            putTag(#name); \
            put(commands); \
            putArray(values, count); \
        } \
    };
APS5_EXACT_COUNT(CmdSetViewportWithCount, VkViewport)
APS5_EXACT_COUNT(CmdSetViewportWithCountEXT, VkViewport)
APS5_EXACT_COUNT(CmdSetScissorWithCount, VkRect2D)
APS5_EXACT_COUNT(CmdSetScissorWithCountEXT, VkRect2D)
APS5_EXACT_COUNT(CmdExecuteCommands, VkCommandBuffer)
#undef APS5_EXACT_COUNT

template<>
struct Exact<CommandId::CmdBindDescriptorSets> {
    static VKAPI_ATTR void VKAPI_CALL Run(VkCommandBuffer commands, VkPipelineBindPoint point, VkPipelineLayout layout, std::uint32_t first, std::uint32_t count, const VkDescriptorSet* sets, std::uint32_t offsets, const std::uint32_t* dynamicOffsets) {
        putTag("CmdBindDescriptorSets");
        put(commands);
        put(point);
        put(layout);
        put(first);
        putArray(sets, count);
        putArray(dynamicOffsets, offsets);
    }
};

template<>
struct Exact<CommandId::CmdPushConstants> {
    static VKAPI_ATTR void VKAPI_CALL Run(VkCommandBuffer commands, VkPipelineLayout layout, VkShaderStageFlags stages, std::uint32_t offset, std::uint32_t size, const void* values) {
        putTag("CmdPushConstants");
        put(commands);
        put(layout);
        put(stages);
        put(offset);
        putArray(static_cast<const std::uint8_t*>(values), size);
    }
};

template<>
struct Exact<CommandId::CmdBindVertexBuffers> {
    static VKAPI_ATTR void VKAPI_CALL Run(VkCommandBuffer commands, std::uint32_t first, std::uint32_t count, const VkBuffer* buffers, const VkDeviceSize* offsets) {
        putTag("CmdBindVertexBuffers");
        put(commands);
        put(first);
        putArray(buffers, count);
        putArray(offsets, count);
    }
};

template<>
struct Exact<CommandId::CmdPipelineBarrier> {
    static VKAPI_ATTR void VKAPI_CALL Run(VkCommandBuffer commands, VkPipelineStageFlags source, VkPipelineStageFlags destination, VkDependencyFlags dependencies, std::uint32_t memory, const VkMemoryBarrier* memoryBarriers, std::uint32_t buffers, const VkBufferMemoryBarrier* bufferBarriers, std::uint32_t images, const VkImageMemoryBarrier* imageBarriers) {
        putTag("CmdPipelineBarrier");
        put(commands);
        put(source);
        put(destination);
        put(dependencies);
        put(memory);
        for (std::uint32_t index = 0; index < memory; ++index) putMemory(memoryBarriers[index]);
        put(buffers);
        for (std::uint32_t index = 0; index < buffers; ++index) putBuffer(bufferBarriers[index]);
        put(images);
        for (std::uint32_t index = 0; index < images; ++index) putImage(imageBarriers[index]);
    }
};

template<>
struct Exact<CommandId::CmdCopyBuffer> {
    static VKAPI_ATTR void VKAPI_CALL Run(VkCommandBuffer commands, VkBuffer source, VkBuffer destination, std::uint32_t count, const VkBufferCopy* regions) {
        putTag("CmdCopyBuffer");
        put(commands);
        put(source);
        put(destination);
        putArray(regions, count);
    }
};

template<>
struct Exact<CommandId::CmdUpdateBuffer> {
    static VKAPI_ATTR void VKAPI_CALL Run(VkCommandBuffer commands, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, const void* data) {
        putTag("CmdUpdateBuffer");
        put(commands);
        put(buffer);
        put(offset);
        put(size);
        putArray(static_cast<const std::uint8_t*>(data), static_cast<std::uint32_t>(size));
    }
};

template<>
struct Exact<CommandId::CmdCopyBufferToImage> {
    static VKAPI_ATTR void VKAPI_CALL Run(VkCommandBuffer commands, VkBuffer source, VkImage destination, VkImageLayout layout, std::uint32_t count, const VkBufferImageCopy* regions) {
        putTag("CmdCopyBufferToImage");
        put(commands);
        put(source);
        put(destination);
        put(layout);
        putArray(regions, count);
    }
};

template<>
struct Exact<CommandId::CmdCopyImageToBuffer> {
    static VKAPI_ATTR void VKAPI_CALL Run(VkCommandBuffer commands, VkImage source, VkImageLayout layout, VkBuffer destination, std::uint32_t count, const VkBufferImageCopy* regions) {
        putTag("CmdCopyImageToBuffer");
        put(commands);
        put(source);
        put(layout);
        put(destination);
        putArray(regions, count);
    }
};

template<>
struct Exact<CommandId::CmdCopyImage> {
    static VKAPI_ATTR void VKAPI_CALL Run(VkCommandBuffer commands, VkImage source, VkImageLayout sourceLayout, VkImage destination, VkImageLayout destinationLayout, std::uint32_t count, const VkImageCopy* regions) {
        putTag("CmdCopyImage");
        put(commands);
        put(source);
        put(sourceLayout);
        put(destination);
        put(destinationLayout);
        putArray(regions, count);
    }
};

template<>
struct Exact<CommandId::CmdBlitImage> {
    static VKAPI_ATTR void VKAPI_CALL Run(VkCommandBuffer commands, VkImage source, VkImageLayout sourceLayout, VkImage destination, VkImageLayout destinationLayout, std::uint32_t count, const VkImageBlit* regions, VkFilter filter) {
        putTag("CmdBlitImage");
        put(commands);
        put(source);
        put(sourceLayout);
        put(destination);
        put(destinationLayout);
        putArray(regions, count);
        put(filter);
    }
};

template<>
struct Exact<CommandId::CmdClearColorImage> {
    static VKAPI_ATTR void VKAPI_CALL Run(VkCommandBuffer commands, VkImage image, VkImageLayout layout, const VkClearColorValue* color, std::uint32_t count, const VkImageSubresourceRange* ranges) {
        putTag("CmdClearColorImage");
        put(commands);
        put(image);
        put(layout);
        put(*color);
        putArray(ranges, count);
    }
};

template<>
struct Exact<CommandId::CmdClearDepthStencilImage> {
    static VKAPI_ATTR void VKAPI_CALL Run(VkCommandBuffer commands, VkImage image, VkImageLayout layout, const VkClearDepthStencilValue* value, std::uint32_t count, const VkImageSubresourceRange* ranges) {
        putTag("CmdClearDepthStencilImage");
        put(commands);
        put(image);
        put(layout);
        put(*value);
        putArray(ranges, count);
    }
};

template<>
struct Exact<CommandId::CmdClearAttachments> {
    static VKAPI_ATTR void VKAPI_CALL Run(VkCommandBuffer commands, std::uint32_t attachments, const VkClearAttachment* clears, std::uint32_t rects, const VkClearRect* areas) {
        putTag("CmdClearAttachments");
        put(commands);
        putArray(clears, attachments);
        putArray(areas, rects);
    }
};

template<>
struct Exact<CommandId::CmdBeginRenderPass> {
    static VKAPI_ATTR void VKAPI_CALL Run(VkCommandBuffer commands, const VkRenderPassBeginInfo* info, VkSubpassContents contents) {
        putTag("CmdBeginRenderPass");
        put(commands);
        put(info->sType);
        putNext(info->pNext);
        put(info->renderPass);
        put(info->framebuffer);
        put(info->renderArea);
        putArray(info->pClearValues, info->clearValueCount);
        put(contents);
    }
};

template<>
struct Exact<CommandId::CmdBeginRenderingKHR> {
    static VKAPI_ATTR void VKAPI_CALL Run(VkCommandBuffer commands, const VkRenderingInfoKHR* info) {
        putTag("CmdBeginRenderingKHR");
        put(commands);
        put(info->sType);
        putNext(info->pNext);
        put(info->flags);
        put(info->renderArea);
        put(info->layerCount);
        put(info->viewMask);
        put(info->colorAttachmentCount);
        put(info->pColorAttachments != nullptr);
        for (std::uint32_t index = 0; index < info->colorAttachmentCount; ++index) putAttachment(&info->pColorAttachments[index]);
        putAttachment(info->pDepthAttachment);
        putAttachment(info->pStencilAttachment);
    }
};

template<>
struct Exact<CommandId::CmdSetCheckpointNV> {
    static VKAPI_ATTR void VKAPI_CALL Run(VkCommandBuffer commands, const void* marker) {
        putTag("CmdSetCheckpointNV");
        put(commands);
        put(marker);
    }
};

template<>
struct Exact<CommandId::CmdSetDepthBias2EXT> {
    static VKAPI_ATTR void VKAPI_CALL Run(VkCommandBuffer commands, const VkDepthBiasInfoEXT* info) {
        putTag("CmdSetDepthBias2EXT");
        put(commands);
        put(info->sType);
        putNext(info->pNext);
        put(info->depthBiasConstantFactor);
        put(info->depthBiasClamp);
        put(info->depthBiasSlopeFactor);
    }
};

template<>
struct Exact<CommandId::CmdBindShadersEXT> {
    static VKAPI_ATTR void VKAPI_CALL Run(VkCommandBuffer commands, std::uint32_t count, const VkShaderStageFlagBits* stages, const VkShaderEXT* shaders) {
        putTag("CmdBindShadersEXT");
        put(commands);
        putArray(stages, count);
        put(shaders != nullptr);
        if (shaders != nullptr) putArray(shaders, count);
    }
};

enum class WriteArray { Images, Buffers, Views };

WriteArray writeArray(VkDescriptorType type) {
    switch (type) {
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
    case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
    case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        return WriteArray::Buffers;
    case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
    case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        return WriteArray::Views;
    default:
        return WriteArray::Images;
    }
}

template<>
struct Exact<CommandId::CmdPushDescriptorSetKHR> {
    static VKAPI_ATTR void VKAPI_CALL Run(VkCommandBuffer commands, VkPipelineBindPoint point, VkPipelineLayout layout, std::uint32_t set, std::uint32_t count, const VkWriteDescriptorSet* writes) {
        putTag("CmdPushDescriptorSetKHR");
        put(commands);
        put(point);
        put(layout);
        put(set);
        put(count);
        for (std::uint32_t index = 0; index < count; ++index) {
            const auto& write = writes[index];
            put(write.sType);
            putNext(write.pNext);
            put(write.dstSet);
            put(write.dstBinding);
            put(write.dstArrayElement);
            put(write.descriptorType);
            switch (writeArray(write.descriptorType)) {
            case WriteArray::Images:
                putArray(write.pImageInfo, write.descriptorCount);
                break;
            case WriteArray::Buffers:
                putArray(write.pBufferInfo, write.descriptorCount);
                break;
            case WriteArray::Views:
                putArray(write.pTexelBufferView, write.descriptorCount);
                break;
            }
        }
    }
};

CommandFunctions exactFunctions() {
    CommandFunctions functions{};
#define APS5_EXACT(name) functions.name = &Exact<CommandId::name>::Run;
    APS5_EXACT(CmdSetViewport)
    APS5_EXACT(CmdSetScissor)
    APS5_EXACT(CmdSetColorWriteMaskEXT)
    APS5_EXACT(CmdSetViewportWithCount)
    APS5_EXACT(CmdSetViewportWithCountEXT)
    APS5_EXACT(CmdSetScissorWithCount)
    APS5_EXACT(CmdSetScissorWithCountEXT)
    APS5_EXACT(CmdBindDescriptorSets)
    APS5_EXACT(CmdPushConstants)
    APS5_EXACT(CmdBindVertexBuffers)
    APS5_EXACT(CmdPipelineBarrier)
    APS5_EXACT(CmdCopyBuffer)
    APS5_EXACT(CmdUpdateBuffer)
    APS5_EXACT(CmdCopyBufferToImage)
    APS5_EXACT(CmdCopyImageToBuffer)
    APS5_EXACT(CmdCopyImage)
    APS5_EXACT(CmdBlitImage)
    APS5_EXACT(CmdClearColorImage)
    APS5_EXACT(CmdClearDepthStencilImage)
    APS5_EXACT(CmdClearAttachments)
    APS5_EXACT(CmdBeginRenderPass)
    APS5_EXACT(CmdBeginRenderingKHR)
    APS5_EXACT(CmdExecuteCommands)
    APS5_EXACT(CmdSetCheckpointNV)
    APS5_EXACT(CmdSetDepthBias2EXT)
    APS5_EXACT(CmdBindShadersEXT)
    APS5_EXACT(CmdPushDescriptorSetKHR)
#undef APS5_EXACT
    return functions;
}

template<CommandId Id>
decltype(&Exact<Id>::Run) exactThunk() {
    return reinterpret_cast<decltype(&Exact<Id>::Run)>(AgcDriver::Graphics::CommandThunk(AgcDriver::Graphics::CommandNames()[static_cast<std::size_t>(Id)], reinterpret_cast<PFN_vkVoidFunction>(&Exact<Id>::Run)));
}

struct Sources {
    std::array<VkViewport, 3> viewports;
    std::array<VkRect2D, 3> rects;
    std::array<VkColorComponentFlags, 3> masks;
    std::array<VkDescriptorSet, 3> sets;
    std::array<std::uint32_t, 4> dynamicOffsets;
    std::array<std::uint8_t, 20> constants;
    std::array<VkBuffer, 3> vertexBuffers;
    std::array<VkDeviceSize, 3> vertexOffsets;
    std::array<VkMemoryBarrier, 2> memoryBarriers;
    std::array<VkBufferMemoryBarrier, 2> bufferBarriers;
    std::array<VkImageMemoryBarrier, 2> imageBarriers;
    std::array<VkBufferCopy, 2> bufferCopies;
    std::array<std::uint8_t, 12> update;
    std::array<VkBufferImageCopy, 2> bufferImageCopies;
    std::array<VkImageCopy, 2> imageCopies;
    std::array<VkImageBlit, 2> blits;
    VkClearColorValue color;
    VkClearDepthStencilValue depthValue;
    std::array<VkImageSubresourceRange, 2> ranges;
    std::array<VkClearAttachment, 2> clears;
    std::array<VkClearRect, 2> clearRects;
    std::array<VkClearValue, 3> clearValues;
    VkRenderPassBeginInfo pass;
    std::array<VkRenderingAttachmentInfoKHR, 2> colors;
    VkRenderingAttachmentInfoKHR depth;
    VkRenderingAttachmentInfoKHR stencil;
    VkRenderingInfoKHR rendering;
    VkRenderingInfoKHR renderingWithoutStencil;
    std::array<VkCommandBuffer, 3> secondaries;
    VkDepthBiasInfoEXT depthBias;
    std::array<VkShaderStageFlagBits, 3> stages;
    std::array<VkShaderEXT, 3> shaders;
    std::array<VkDescriptorImageInfo, 4> imageInfos;
    std::array<VkDescriptorBufferInfo, 3> bufferInfos;
    std::array<VkBufferView, 2> texelViews;
    std::array<VkWriteDescriptorSet, 5> writes;
};

template<typename T>
void fill(T& value, std::uint32_t& next) {
    auto* bytes = reinterpret_cast<std::uint8_t*>(&value);
    for (std::size_t index = 0; index < sizeof(T); ++index) bytes[index] = static_cast<std::uint8_t>(next++ * 37 + 11);
}

template<typename T>
void structured(T& value, VkStructureType type, std::uint32_t& next) {
    fill(value, next);
    value.sType = type;
    value.pNext = nullptr;
}

void prepare(Sources& sources) {
    std::memset(&sources, 0, sizeof(sources));
    std::uint32_t next = 1;
    for (auto& value : sources.viewports) fill(value, next);
    for (auto& value : sources.rects) fill(value, next);
    for (auto& value : sources.masks) fill(value, next);
    for (auto& value : sources.sets) fill(value, next);
    for (auto& value : sources.dynamicOffsets) fill(value, next);
    fill(sources.constants, next);
    for (auto& value : sources.vertexBuffers) fill(value, next);
    for (auto& value : sources.vertexOffsets) fill(value, next);
    for (auto& value : sources.memoryBarriers) structured(value, VK_STRUCTURE_TYPE_MEMORY_BARRIER, next);
    for (auto& value : sources.bufferBarriers) structured(value, VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER, next);
    for (auto& value : sources.imageBarriers) structured(value, VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER, next);
    for (auto& value : sources.bufferCopies) fill(value, next);
    fill(sources.update, next);
    for (auto& value : sources.bufferImageCopies) fill(value, next);
    for (auto& value : sources.imageCopies) fill(value, next);
    for (auto& value : sources.blits) fill(value, next);
    fill(sources.color, next);
    fill(sources.depthValue, next);
    for (auto& value : sources.ranges) fill(value, next);
    for (auto& value : sources.clears) fill(value, next);
    for (auto& value : sources.clearRects) fill(value, next);
    for (auto& value : sources.clearValues) fill(value, next);
    structured(sources.pass, VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO, next);
    sources.pass.clearValueCount = static_cast<std::uint32_t>(sources.clearValues.size());
    sources.pass.pClearValues = sources.clearValues.data();
    for (auto& value : sources.colors) structured(value, VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR, next);
    structured(sources.depth, VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR, next);
    structured(sources.stencil, VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR, next);
    structured(sources.rendering, VK_STRUCTURE_TYPE_RENDERING_INFO_KHR, next);
    sources.rendering.colorAttachmentCount = static_cast<std::uint32_t>(sources.colors.size());
    sources.rendering.pColorAttachments = sources.colors.data();
    sources.rendering.pDepthAttachment = &sources.depth;
    sources.rendering.pStencilAttachment = &sources.stencil;
    sources.renderingWithoutStencil = sources.rendering;
    sources.renderingWithoutStencil.pStencilAttachment = nullptr;
    for (auto& value : sources.secondaries) fill(value, next);
    structured(sources.depthBias, VK_STRUCTURE_TYPE_DEPTH_BIAS_INFO_EXT, next);
    sources.stages = {VK_SHADER_STAGE_VERTEX_BIT, VK_SHADER_STAGE_GEOMETRY_BIT, VK_SHADER_STAGE_FRAGMENT_BIT};
    for (auto& value : sources.shaders) fill(value, next);
    for (auto& value : sources.imageInfos) fill(value, next);
    for (auto& value : sources.bufferInfos) fill(value, next);
    for (auto& value : sources.texelViews) fill(value, next);
    for (auto& value : sources.writes) structured(value, VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, next);
    const auto ignored = [](auto& pointer) { pointer = reinterpret_cast<std::remove_reference_t<decltype(pointer)>>(static_cast<std::uintptr_t>(0x40)); };
    for (auto& write : sources.writes) {
        ignored(write.pImageInfo);
        ignored(write.pBufferInfo);
        ignored(write.pTexelBufferView);
    }
    sources.writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    sources.writes[0].descriptorCount = 3;
    sources.writes[0].pImageInfo = sources.imageInfos.data();
    sources.writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sources.writes[1].descriptorCount = 2;
    sources.writes[1].pBufferInfo = sources.bufferInfos.data();
    sources.writes[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
    sources.writes[2].descriptorCount = 2;
    sources.writes[2].pTexelBufferView = sources.texelViews.data();
    sources.writes[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    sources.writes[3].descriptorCount = 1;
    sources.writes[3].pImageInfo = sources.imageInfos.data() + 3;
    sources.writes[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    sources.writes[4].descriptorCount = 1;
    sources.writes[4].pBufferInfo = sources.bufferInfos.data() + 2;
}

void recordEverySpecializedCommand(VkCommandBuffer commands, const Sources& sources) {
    exactThunk<CommandId::CmdSetViewport>()(commands, 1, 3, sources.viewports.data());
    exactThunk<CommandId::CmdSetScissor>()(commands, 2, 3, sources.rects.data());
    exactThunk<CommandId::CmdSetColorWriteMaskEXT>()(commands, 1, 3, sources.masks.data());
    exactThunk<CommandId::CmdSetViewportWithCount>()(commands, 2, sources.viewports.data());
    exactThunk<CommandId::CmdSetViewportWithCountEXT>()(commands, 3, sources.viewports.data());
    exactThunk<CommandId::CmdSetScissorWithCount>()(commands, 3, sources.rects.data());
    exactThunk<CommandId::CmdSetScissorWithCountEXT>()(commands, 1, sources.rects.data());
    exactThunk<CommandId::CmdBindDescriptorSets>()(commands, VK_PIPELINE_BIND_POINT_COMPUTE, fake<VkPipelineLayout>(41), 2, 3, sources.sets.data(), 4, sources.dynamicOffsets.data());
    exactThunk<CommandId::CmdBindDescriptorSets>()(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, fake<VkPipelineLayout>(42), 0, 1, sources.sets.data(), 0, nullptr);
    exactThunk<CommandId::CmdPushConstants>()(commands, fake<VkPipelineLayout>(43), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 12, 20, sources.constants.data());
    exactThunk<CommandId::CmdBindVertexBuffers>()(commands, 4, 3, sources.vertexBuffers.data(), sources.vertexOffsets.data());
    exactThunk<CommandId::CmdPipelineBarrier>()(commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 2, sources.memoryBarriers.data(), 2, sources.bufferBarriers.data(), 2, sources.imageBarriers.data());
    exactThunk<CommandId::CmdPipelineBarrier>()(commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 1, sources.bufferBarriers.data() + 1, 0, nullptr);
    exactThunk<CommandId::CmdCopyBuffer>()(commands, handle(51), handle(52), 2, sources.bufferCopies.data());
    exactThunk<CommandId::CmdUpdateBuffer>()(commands, handle(53), 24, 12, sources.update.data());
    exactThunk<CommandId::CmdCopyBufferToImage>()(commands, handle(54), fake<VkImage>(55), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 2, sources.bufferImageCopies.data());
    exactThunk<CommandId::CmdCopyImageToBuffer>()(commands, fake<VkImage>(56), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, handle(57), 2, sources.bufferImageCopies.data());
    exactThunk<CommandId::CmdCopyImage>()(commands, fake<VkImage>(58), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, fake<VkImage>(59), VK_IMAGE_LAYOUT_GENERAL, 2, sources.imageCopies.data());
    exactThunk<CommandId::CmdBlitImage>()(commands, fake<VkImage>(60), VK_IMAGE_LAYOUT_GENERAL, fake<VkImage>(61), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 2, sources.blits.data(), VK_FILTER_LINEAR);
    exactThunk<CommandId::CmdClearColorImage>()(commands, fake<VkImage>(62), VK_IMAGE_LAYOUT_GENERAL, &sources.color, 2, sources.ranges.data());
    exactThunk<CommandId::CmdClearDepthStencilImage>()(commands, fake<VkImage>(63), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &sources.depthValue, 2, sources.ranges.data());
    exactThunk<CommandId::CmdClearAttachments>()(commands, 2, sources.clears.data(), 2, sources.clearRects.data());
    exactThunk<CommandId::CmdBeginRenderPass>()(commands, &sources.pass, VK_SUBPASS_CONTENTS_INLINE);
    exactThunk<CommandId::CmdBeginRenderingKHR>()(commands, &sources.rendering);
    exactThunk<CommandId::CmdBeginRenderingKHR>()(commands, &sources.renderingWithoutStencil);
    exactThunk<CommandId::CmdExecuteCommands>()(commands, 3, sources.secondaries.data());
    exactThunk<CommandId::CmdSetCheckpointNV>()(commands, &sources.secondaries[1]);
    exactThunk<CommandId::CmdSetDepthBias2EXT>()(commands, &sources.depthBias);
    exactThunk<CommandId::CmdBindShadersEXT>()(commands, 3, sources.stages.data(), sources.shaders.data());
    exactThunk<CommandId::CmdBindShadersEXT>()(commands, 2, sources.stages.data(), nullptr);
    exactThunk<CommandId::CmdPushDescriptorSetKHR>()(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, fake<VkPipelineLayout>(64), 1, 5, sources.writes.data());
}

std::string firstByteDifference(const std::string& expected, const std::string& actual, const std::vector<std::pair<std::size_t, std::string>>& commands) {
    const auto length = std::min(expected.size(), actual.size());
    std::size_t index = 0;
    while (index < length && expected[index] == actual[index]) ++index;
    std::string command = "?";
    for (const auto& [offset, name] : commands) {
        if (offset <= index) command = name;
    }
    return "first difference at byte " + std::to_string(index) + " of " + std::to_string(expected.size()) + " (" + std::to_string(actual.size()) + " replayed), in " + command;
}

void SpecializedStreamersKeepEveryArgument() {
    const auto exact = exactFunctions();
    const auto direct = FakeCommandBuffer(70);
    Sources sources;
    prepare(sources);
    captured.clear();
    capturedCommands.clear();
    recordEverySpecializedCommand(direct, sources);
    const auto expected = captured;
    const auto commandsAt = capturedCommands;
    Require(expected.size() > 2000, "the direct recording captured only " + std::to_string(expected.size()) + " bytes");
    const auto commands = commandsAt.size();
    Require(commands == 31, "the direct recording captured " + std::to_string(commands) + " commands");
    CommandStream stream(exact);
    captured.clear();
    recordEverySpecializedCommand(stream.Handle(), sources);
    Require(captured.empty(), "a specialized command reached the driver while recording");
    Require(stream.Materialized() == VK_NULL_HANDLE && !stream.Poisoned(), "a specialized command demoted the stream");
    std::memset(static_cast<void*>(&sources), 0xA5, sizeof(sources));
    stream.Seal();
    AgcDriver::Graphics::Replay(stream, direct, exact);
    Require(captured == expected, "the replay after the sources were overwritten differs: " + firstByteDifference(expected, captured, commandsAt));
    Require(stream.Ops() == commands, "the stream counted " + std::to_string(stream.Ops()) + " ops for " + std::to_string(commands) + " commands");
}

VKAPI_ATTR void VKAPI_CALL emptyDraw(VkCommandBuffer, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) {}

struct CommandCosts {
    double raw = 0;
    double thunk = 0;
    double typedDirect = 0;
    double thunkStreamed = 0;
    double typedStreamed = 0;
};

template<typename Body>
double nanosPerCall(int calls, Body&& body) {
    const auto start = std::chrono::steady_clock::now();
    for (int call = 0; call < calls; ++call) body(static_cast<std::uint32_t>(call));
    return std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count() / calls;
}

double median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    return values[values.size() / 2];
}

CommandCosts MeasureCommandCosts(int directCalls, int streamedCalls, int rounds) {
    CommandFunctions direct{};
    direct.CmdDraw = &emptyDraw;
    AgcDriver::Graphics::DeviceFunctions deviceTable{};
    deviceTable.cmdDraw = thunked("vkCmdDraw", &emptyDraw);
    AgcDriver::Graphics::Context device{};
    device.functions = &deviceTable;
    const CommandFunctions* volatile rawDevice = &direct;
    const AgcDriver::Graphics::Context* volatile context = &device;
    const CommandFunctions* volatile directBatch = &direct;
    const CommandFunctions* volatile streamedBatch = &AgcDriver::Graphics::StreamedCommands();
    const auto target = FakeCommandBuffer(1);
    std::vector<double> raw, thunk, typed, thunkStreamed, typedStreamed;
    const std::array<std::function<void()>, 3> direct3{
        [&] { raw.push_back(nanosPerCall(directCalls, [&](std::uint32_t call) { rawDevice->CmdDraw(target, call, 1, 0, 0); })); },
        [&] { thunk.push_back(nanosPerCall(directCalls, [&](std::uint32_t call) { context->Resolved(&AgcDriver::Graphics::DeviceFunctions::cmdDraw, "vkCmdDraw")(target, call, 1, 0, 0); })); },
        [&] { typed.push_back(nanosPerCall(directCalls, [&](std::uint32_t call) { context->ForBatch(directBatch, &CommandFunctions::CmdDraw, &AgcDriver::Graphics::DeviceFunctions::cmdDraw, "vkCmdDraw")(target, call, 1, 0, 0); })); },
    };
    for (int round = 0; round < rounds; ++round) {
        for (std::size_t step = 0; step < direct3.size(); ++step) direct3[(step + static_cast<std::size_t>(round)) % direct3.size()]();
        {
            CommandStream stream(direct);
            const auto handle = stream.Handle();
            thunkStreamed.push_back(nanosPerCall(streamedCalls, [&](std::uint32_t call) { context->Resolved(&AgcDriver::Graphics::DeviceFunctions::cmdDraw, "vkCmdDraw")(handle, call, 1, 0, 0); }));
        }
        {
            CommandStream stream(direct);
            const auto handle = stream.Handle();
            typedStreamed.push_back(nanosPerCall(streamedCalls, [&](std::uint32_t call) { context->ForBatch(streamedBatch, &CommandFunctions::CmdDraw, &AgcDriver::Graphics::DeviceFunctions::cmdDraw, "vkCmdDraw")(handle, call, 1, 0, 0); }));
        }
    }
    std::vector<double> typedOver, thunkOver;
    for (std::size_t round = 0; round < raw.size(); ++round) {
        typedOver.push_back(typed[round] - raw[round]);
        thunkOver.push_back(thunk[round] - raw[round]);
    }
    CommandCosts costs;
    costs.raw = median(raw);
    costs.thunk = costs.raw + median(thunkOver);
    costs.typedDirect = costs.raw + median(typedOver);
    costs.thunkStreamed = median(thunkStreamed);
    costs.typedStreamed = median(typedStreamed);
    return costs;
}

VKAPI_ATTR void VKAPI_CALL emptyPush(VkCommandBuffer, VkPipelineBindPoint, VkPipelineLayout, std::uint32_t, std::uint32_t, const VkWriteDescriptorSet*) {}

void PushDescriptorCost(int calls, int rounds) {
    CommandFunctions direct{};
    direct.CmdPushDescriptorSetKHR = &emptyPush;
    const auto streamedPush = AgcDriver::Graphics::StreamedCommands().CmdPushDescriptorSetKHR;
    std::array<VkDescriptorImageInfo, 2> images{};
    VkDescriptorBufferInfo buffer{fake<VkBuffer>(1), 0, 256};
    std::array<VkWriteDescriptorSet, 2> writes{};
    for (auto& write : writes) write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[0].descriptorCount = 2;
    writes[0].pImageInfo = images.data();
    writes[1].dstBinding = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[1].descriptorCount = 1;
    writes[1].pBufferInfo = &buffer;
    std::vector<double> raw, append, replay;
    for (int round = 0; round < rounds; ++round) {
        raw.push_back(nanosPerCall(calls, [&](std::uint32_t call) { direct.CmdPushDescriptorSetKHR(FakeCommandBuffer(1), VK_PIPELINE_BIND_POINT_GRAPHICS, fake<VkPipelineLayout>(call & 7), 0, 2, writes.data()); }));
        CommandStream stream(direct);
        const auto handle = stream.Handle();
        append.push_back(nanosPerCall(calls, [&](std::uint32_t call) { streamedPush(handle, VK_PIPELINE_BIND_POINT_GRAPHICS, fake<VkPipelineLayout>(call & 7), 0, 2, writes.data()); }));
        stream.Seal();
        const auto start = std::chrono::steady_clock::now();
        AgcDriver::Graphics::Replay(stream, FakeCommandBuffer(1), direct);
        replay.push_back(std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count() / calls);
    }
    std::printf("push descriptors (2 writes, 3 descriptors; medians of %d rounds): direct %.2f ns, streamed append %.2f ns, replay %.2f ns per call\n", rounds, median(raw), median(append), median(replay));
}

VKAPI_ATTR void VKAPI_CALL emptyStencil(VkCommandBuffer, VkStencilFaceFlags, std::uint32_t) {}

void KeyedLookupCost(int calls, int rounds) {
    using AgcDriver::Graphics::CommandThunk;
    constexpr const char* Name = "vkCmdSetStencilReference";
    const auto reals = stencils(std::make_integer_sequence<int, 8>{});
    for (const auto real : reals) CommandThunk(Name, reinterpret_cast<PFN_vkVoidFunction>(real));
    static const int keys[4] = {};
    std::array<FakeDispatchable, 4> devices{FakeDispatchable{&keys[0]}, FakeDispatchable{&keys[1]}, FakeDispatchable{&keys[2]}, FakeDispatchable{&keys[3]}};
    std::array<FakeDispatchable, 4> buffers = devices;
    PFN_vkCmdSetStencilReference keyed = nullptr;
    for (auto& device : devices) keyed = reinterpret_cast<PFN_vkCmdSetStencilReference>(CommandThunk(Name, reinterpret_cast<PFN_vkVoidFunction>(&emptyStencil), deviceOf(device)));
    volatile PFN_vkCmdSetStencilReference raw = &emptyStencil;
    std::vector<double> direct, newest, oldest;
    for (int round = 0; round < rounds; ++round) {
        direct.push_back(nanosPerCall(calls, [&](std::uint32_t call) { raw(commandsOf(buffers[3]), VK_STENCIL_FACE_FRONT_BIT, call); }));
        newest.push_back(nanosPerCall(calls, [&](std::uint32_t call) { keyed(commandsOf(buffers[3]), VK_STENCIL_FACE_FRONT_BIT, call); }));
        oldest.push_back(nanosPerCall(calls, [&](std::uint32_t call) { keyed(commandsOf(buffers[0]), VK_STENCIL_FACE_FRONT_BIT, call); }));
    }
    for (auto& device : devices) AgcDriver::Graphics::ForgetCommandDevice(deviceOf(device));
    std::printf("keyed lookup past the slots, 4 devices (medians of %d rounds): raw %.2f ns, first node %.2f ns, fourth node %.2f ns per call\n", rounds, median(direct), median(newest), median(oldest));
}

bool InstrumentedBuild() {
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
    return true;
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
    return true;
#else
    return false;
#endif
#else
    return false;
#endif
}

int DirectModeCost() {
    constexpr int DirectCalls = 5'000'000;
    constexpr int StreamedCalls = 1'000'000;
    constexpr int Rounds = 21;
    PushDescriptorCost(StreamedCalls / 4, Rounds);
    KeyedLookupCost(StreamedCalls, Rounds);
    const auto costs = MeasureCommandCosts(DirectCalls, StreamedCalls, Rounds);
    const double thunkOver = costs.thunk - costs.raw;
    const double typedOver = costs.typedDirect - costs.raw;
    std::printf("command cost (medians of %d interleaved rounds, each through its table): direct batch: raw %.2f ns, thunk %.2f ns (+%.2f), typed %.2f ns (+%.2f); streamed batch: thunk %.2f ns, typed %.2f ns\n", Rounds, costs.raw, costs.thunk, thunkOver, costs.typedDirect, typedOver, costs.thunkStreamed, costs.typedStreamed);
    if (thunkOver < 0.2) {
        std::printf("the thunk adds %.2f ns here, too little to tell a typed call from it; not gated\n", thunkOver);
        return 0;
    }
    if (typedOver <= thunkOver * 0.75) return 0;
    if (InstrumentedBuild()) {
        std::printf("typed direct adds %.2f ns, over three quarters of the thunk's %.2f ns; not gated in a sanitizer build\n", typedOver, thunkOver);
        return 0;
    }
    std::fprintf(stderr, "command thunks: a direct batch's typed command adds %.2f ns over a raw table call, more than three quarters of the thunk's %.2f ns\n", typedOver, thunkOver);
    return 1;
}

}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--bench") return DirectModeCost();
        EveryListedCommandHasAThunk();
        EveryListedCommandStreams();
        DirectCallsReachTheirOwnEntryPoint();
        StreamReplayRecordsWhatDirectRecords();
        DemotionReplaysThePrefixInOrder();
        SpecializedStreamersKeepEveryArgument();
        PushDescriptorsDemoteOnlyWhatTheyCannotCopy();
        DemotionWithoutAMaterializerPoisons();
        EntryPointsBeyondTheSlotsAreKeyedByDevice();
        std::cout << "command thunks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "command thunks: %s\n", error.what());
        return 1;
    }
}
