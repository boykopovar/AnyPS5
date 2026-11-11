#include "prx/libSceAgcDriver/Graphics/include/CommandEncoder.hpp"
#include "prx/libSceAgcDriver/tests/AllocationFailure.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <new>
#include <memory>
#include <mutex>
#include <random>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

namespace {

using AgcDriver::Graphics::CommandEncoder;
using AgcDriver::Graphics::DeviceFunctions;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::mutex logMutex;
std::map<VkCommandBuffer, std::vector<std::string>> recorded;

void Log(VkCommandBuffer commands, std::string text) {
    std::lock_guard lock(logMutex);
    recorded[commands].push_back(std::move(text));
}

template<typename T>
std::uint64_t Raw(T handle) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(handle));
}

template<typename T>
T Handle(std::uint64_t value) {
    if constexpr (std::is_pointer_v<T>) return reinterpret_cast<T>(static_cast<std::uintptr_t>(value));
    else return static_cast<T>(value);
}

VKAPI_ATTR void VKAPI_CALL FakeDraw(VkCommandBuffer commands, std::uint32_t vertices, std::uint32_t instances, std::uint32_t firstVertex, std::uint32_t firstInstance) {
    Log(commands, "draw " + std::to_string(vertices) + " " + std::to_string(instances) + " " + std::to_string(firstVertex) + " " + std::to_string(firstInstance));
}

VKAPI_ATTR void VKAPI_CALL FakeBindPipeline(VkCommandBuffer commands, VkPipelineBindPoint point, VkPipeline pipeline) {
    Log(commands, "pipeline " + std::to_string(point) + " " + std::to_string(Raw(pipeline)));
}

VKAPI_ATTR void VKAPI_CALL FakeSetViewport(VkCommandBuffer commands, std::uint32_t first, std::uint32_t count, const VkViewport* viewports) {
    std::string text = "viewport " + std::to_string(first);
    for (std::uint32_t index = 0; index < count; ++index) text += " " + std::to_string(viewports[index].x) + "," + std::to_string(viewports[index].width);
    Log(commands, text);
}

VKAPI_ATTR void VKAPI_CALL FakeSetColorWriteMask(VkCommandBuffer commands, std::uint32_t first, std::uint32_t count, const VkColorComponentFlags* masks) {
    std::string text = "mask " + std::to_string(first);
    for (std::uint32_t index = 0; index < count; ++index) text += " " + std::to_string(masks[index]);
    Log(commands, text);
}

VKAPI_ATTR void VKAPI_CALL FakePushConstants(VkCommandBuffer commands, VkPipelineLayout layout, VkShaderStageFlags stages, std::uint32_t offset, std::uint32_t size, const void* values) {
    std::string text = "push " + std::to_string(Raw(layout)) + " " + std::to_string(stages) + " " + std::to_string(offset);
    for (std::uint32_t index = 0; index < size; ++index) text += " " + std::to_string(static_cast<const std::uint8_t*>(values)[index]);
    Log(commands, text);
}

VKAPI_ATTR void VKAPI_CALL FakeBindDescriptorSets(VkCommandBuffer commands, VkPipelineBindPoint point, VkPipelineLayout layout, std::uint32_t first, std::uint32_t count, const VkDescriptorSet* sets, std::uint32_t offsets, const std::uint32_t* dynamicOffsets) {
    std::string text = "sets " + std::to_string(point) + " " + std::to_string(Raw(layout)) + " " + std::to_string(first);
    for (std::uint32_t index = 0; index < count; ++index) text += " " + std::to_string(Raw(sets[index]));
    for (std::uint32_t index = 0; index < offsets; ++index) text += " +" + std::to_string(dynamicOffsets[index]);
    Log(commands, text);
}

VKAPI_ATTR void VKAPI_CALL FakePipelineBarrier(VkCommandBuffer commands, VkPipelineStageFlags source, VkPipelineStageFlags destination, VkDependencyFlags, std::uint32_t memory, const VkMemoryBarrier* barriers, std::uint32_t, const VkBufferMemoryBarrier*, std::uint32_t, const VkImageMemoryBarrier*) {
    std::string text = "barrier " + std::to_string(source) + " " + std::to_string(destination);
    for (std::uint32_t index = 0; index < memory; ++index) text += " " + std::to_string(barriers[index].srcAccessMask) + (barriers[index].pNext != nullptr ? "+chain" : "");
    Log(commands, text);
}

VKAPI_ATTR void VKAPI_CALL FakeUpdateBuffer(VkCommandBuffer commands, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize size, const void* data) {
    std::string text = "update " + std::to_string(Raw(buffer)) + " " + std::to_string(offset);
    for (VkDeviceSize index = 0; index < size; ++index) text += " " + std::to_string(static_cast<const std::uint8_t*>(data)[index]);
    Log(commands, text);
}

VKAPI_ATTR void VKAPI_CALL FakeBeginRenderPass(VkCommandBuffer commands, const VkRenderPassBeginInfo* info, VkSubpassContents contents) {
    std::string text = "begin " + std::to_string(Raw(info->renderPass)) + " " + std::to_string(contents);
    for (std::uint32_t index = 0; index < info->clearValueCount; ++index) text += " " + std::to_string(info->pClearValues[index].color.uint32[0]);
    Log(commands, text);
}

VKAPI_ATTR void VKAPI_CALL FakeEndRenderPass(VkCommandBuffer commands) {
    Log(commands, "end");
}

VKAPI_ATTR void VKAPI_CALL FakeCopyBuffer(VkCommandBuffer commands, VkBuffer source, VkBuffer destination, std::uint32_t count, const VkBufferCopy* regions) {
    std::string text = "copy " + std::to_string(Raw(source)) + " " + std::to_string(Raw(destination));
    for (std::uint32_t index = 0; index < count; ++index) text += " " + std::to_string(regions[index].srcOffset) + ">" + std::to_string(regions[index].dstOffset) + ":" + std::to_string(regions[index].size);
    Log(commands, text);
}

std::string ImageCopies(std::uint32_t count, const VkBufferImageCopy* regions) {
    std::string text;
    for (std::uint32_t index = 0; index < count; ++index) text += " " + std::to_string(regions[index].bufferOffset) + "@" + std::to_string(regions[index].imageExtent.width) + "x" + std::to_string(regions[index].imageExtent.height);
    return text;
}

VKAPI_ATTR void VKAPI_CALL FakeCopyBufferToImage(VkCommandBuffer commands, VkBuffer source, VkImage destination, VkImageLayout layout, std::uint32_t count, const VkBufferImageCopy* regions) {
    Log(commands, "to-image " + std::to_string(Raw(source)) + " " + std::to_string(Raw(destination)) + " " + std::to_string(layout) + ImageCopies(count, regions));
}

VKAPI_ATTR void VKAPI_CALL FakeCopyImageToBuffer(VkCommandBuffer commands, VkImage source, VkImageLayout layout, VkBuffer destination, std::uint32_t count, const VkBufferImageCopy* regions) {
    Log(commands, "to-buffer " + std::to_string(Raw(source)) + " " + std::to_string(layout) + " " + std::to_string(Raw(destination)) + ImageCopies(count, regions));
}

VKAPI_ATTR void VKAPI_CALL FakeClearColorImage(VkCommandBuffer commands, VkImage image, VkImageLayout layout, const VkClearColorValue* color, std::uint32_t count, const VkImageSubresourceRange* ranges) {
    std::string text = "clear " + std::to_string(Raw(image)) + " " + std::to_string(layout) + " " + std::to_string(color->uint32[0]) + "," + std::to_string(color->uint32[3]);
    for (std::uint32_t index = 0; index < count; ++index) text += " " + std::to_string(ranges[index].baseMipLevel) + "+" + std::to_string(ranges[index].levelCount);
    Log(commands, text);
}

VKAPI_ATTR void VKAPI_CALL FakeBeginQuery(VkCommandBuffer commands, VkQueryPool pool, std::uint32_t query, VkQueryControlFlags flags) {
    Log(commands, "begin-query " + std::to_string(Raw(pool)) + " " + std::to_string(query) + " " + std::to_string(flags));
}

VKAPI_ATTR void VKAPI_CALL FakeEndQuery(VkCommandBuffer commands, VkQueryPool pool, std::uint32_t query) {
    Log(commands, "end-query " + std::to_string(Raw(pool)) + " " + std::to_string(query));
}

DeviceFunctions FakeTable() {
    DeviceFunctions table{};
    table.cmdDraw = &FakeDraw;
    table.cmdBindPipeline = &FakeBindPipeline;
    table.cmdSetViewport = &FakeSetViewport;
    table.cmdSetColorWriteMask = &FakeSetColorWriteMask;
    table.cmdPushConstants = &FakePushConstants;
    table.cmdBindDescriptorSets = &FakeBindDescriptorSets;
    table.cmdPipelineBarrier = &FakePipelineBarrier;
    table.cmdUpdateBuffer = &FakeUpdateBuffer;
    table.cmdBeginRenderPass = &FakeBeginRenderPass;
    table.cmdEndRenderPass = &FakeEndRenderPass;
    table.cmdCopyBuffer = &FakeCopyBuffer;
    table.cmdCopyBufferToImage = &FakeCopyBufferToImage;
    table.cmdCopyImageToBuffer = &FakeCopyImageToBuffer;
    table.cmdClearColorImage = &FakeClearColorImage;
    table.cmdBeginQuery = &FakeBeginQuery;
    table.cmdEndQuery = &FakeEndQuery;
    return table;
}

struct Oracle {
    DeviceFunctions& table;
    std::map<VkCommandBuffer, std::vector<std::string>> expected;

    void call(std::mt19937& random, VkCommandBuffer commands) {
        const auto value = static_cast<std::uint32_t>(random() % 251);
        switch (random() % 10) {
        case 0:
            table.cmdDraw(commands, value, 1, value / 2, 0);
            FakeExpect(commands, "draw " + std::to_string(value) + " 1 " + std::to_string(value / 2) + " 0");
            break;
        case 1:
            table.cmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, Handle<VkPipeline>(value + 1));
            FakeExpect(commands, "pipeline 0 " + std::to_string(value + 1));
            break;
        case 2: {
            std::vector<VkViewport> viewports(1 + value % 3);
            std::string text = "viewport 0";
            for (std::size_t index = 0; index < viewports.size(); ++index) {
                viewports[index] = VkViewport{static_cast<float>(value + index), 0.0f, static_cast<float>(index), 1.0f, 0.0f, 1.0f};
                text += " " + std::to_string(viewports[index].x) + "," + std::to_string(viewports[index].width);
            }
            table.cmdSetViewport(commands, 0, static_cast<std::uint32_t>(viewports.size()), viewports.data());
            std::fill(viewports.begin(), viewports.end(), VkViewport{});
            FakeExpect(commands, text);
            break;
        }
        case 3: {
            std::vector<std::uint8_t> bytes(4 * (1 + value % 8));
            std::string text = "push 7 1 0";
            for (std::size_t index = 0; index < bytes.size(); ++index) {
                bytes[index] = static_cast<std::uint8_t>(value + index);
                text += " " + std::to_string(bytes[index]);
            }
            table.cmdPushConstants(commands, Handle<VkPipelineLayout>(7), VK_SHADER_STAGE_VERTEX_BIT, 0, static_cast<std::uint32_t>(bytes.size()), bytes.data());
            std::fill(bytes.begin(), bytes.end(), 0);
            FakeExpect(commands, text);
            break;
        }
        case 4: {
            VkDescriptorSet sets[2]{Handle<VkDescriptorSet>(value + 3), Handle<VkDescriptorSet>(value + 4)};
            std::uint32_t offsets[1]{value * 16};
            const std::uint32_t count = 1 + value % 2;
            const std::uint32_t dynamic = value % 2;
            std::string text = "sets 0 9 1";
            for (std::uint32_t index = 0; index < count; ++index) text += " " + std::to_string(Raw(sets[index]));
            for (std::uint32_t index = 0; index < dynamic; ++index) text += " +" + std::to_string(offsets[index]);
            table.cmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, Handle<VkPipelineLayout>(9), 1, count, sets, dynamic, offsets);
            sets[0] = VK_NULL_HANDLE;
            offsets[0] = 0;
            FakeExpect(commands, text);
            break;
        }
        case 5: {
            VkBaseInStructure link{VK_STRUCTURE_TYPE_MEMORY_BARRIER, nullptr};
            VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER, value % 5 == 0 ? &link : nullptr, value, value + 1};
            table.cmdPipelineBarrier(commands, 1, 2, 0, 1, &barrier, 0, nullptr, 0, nullptr);
            FakeExpect(commands, "barrier 1 2 " + std::to_string(value) + (value % 5 == 0 ? "+chain" : ""));
            break;
        }
        case 6: {
            std::uint8_t bytes[4]{static_cast<std::uint8_t>(value), 1, 2, 3};
            table.cmdUpdateBuffer(commands, Handle<VkBuffer>(5), value * 4, 4, bytes);
            FakeExpect(commands, "update 5 " + std::to_string(value * 4) + " " + std::to_string(bytes[0]) + " 1 2 3");
            break;
        }
        case 7: {
            VkClearValue clears[2]{};
            clears[0].color.uint32[0] = value;
            clears[1].color.uint32[0] = value + 1;
            VkRenderPassBeginInfo info{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
            info.renderPass = Handle<VkRenderPass>(value + 11);
            info.clearValueCount = value % 3;
            info.pClearValues = clears;
            std::string text = "begin " + std::to_string(value + 11) + " 0";
            for (std::uint32_t index = 0; index < info.clearValueCount; ++index) text += " " + std::to_string(clears[index].color.uint32[0]);
            table.cmdBeginRenderPass(commands, &info, VK_SUBPASS_CONTENTS_INLINE);
            clears[0].color.uint32[0] = 0;
            FakeExpect(commands, text);
            break;
        }
        case 8: {
            VkColorComponentFlags masks[3]{value & 0xfu, (value >> 1) & 0xfu, (value >> 2) & 0xfu};
            const std::uint32_t first = value % 2;
            const std::uint32_t count = 1 + value % 3;
            std::string text = "mask " + std::to_string(first);
            for (std::uint32_t index = 0; index < count; ++index) text += " " + std::to_string(masks[index]);
            table.cmdSetColorWriteMask(commands, first, count, masks);
            std::fill(std::begin(masks), std::end(masks), 0u);
            FakeExpect(commands, text);
            break;
        }
        default:
            table.cmdEndRenderPass(commands);
            FakeExpect(commands, "end");
            break;
        }
    }

    void FakeExpect(VkCommandBuffer commands, std::string text) {
        expected[commands].push_back(std::move(text));
    }
};

void Compare(const std::map<VkCommandBuffer, std::vector<std::string>>& expected, const char* scenario) {
    std::lock_guard lock(logMutex);
    for (const auto& [commands, sequence] : expected) {
        const auto& actual = recorded[commands];
        Require(actual.size() == sequence.size(), std::string(scenario) + ": command buffer " + std::to_string(Raw(commands)) + " recorded " + std::to_string(actual.size()) + " commands, inline records " + std::to_string(sequence.size()));
        for (std::size_t index = 0; index < sequence.size(); ++index) Require(actual[index] == sequence[index], std::string(scenario) + ": command " + std::to_string(index) + " on command buffer " + std::to_string(Raw(commands)) + " is '" + actual[index] + "', inline records '" + sequence[index] + "'");
    }
}

std::map<VkCommandBuffer, CommandEncoder::Recording>& Recordings() {
    static std::map<VkCommandBuffer, CommandEncoder::Recording> value;
    return value;
}

CommandEncoder::Recording& RecordingOf(VkCommandBuffer commands) {
    return Recordings()[commands];
}

void Reset() {
    std::lock_guard lock(logMutex);
    recorded.clear();
    Recordings().clear();
}

std::size_t Recorded(VkCommandBuffer commands) {
    std::lock_guard lock(logMutex);
    return recorded[commands].size();
}

std::unique_ptr<CommandEncoder> Attached(VkDevice device, DeviceFunctions& table) {
    auto encoder = CommandEncoder::Attach(device, table);
    Require(encoder != nullptr, "no command encoder slot was free");
    return encoder;
}

void RandomProgramRecordsWhatInlineRecords(std::uint32_t seed, bool delays) {
    Reset();
    auto table = FakeTable();
    const auto device = Handle<VkDevice>(0x40);
    std::mt19937 random(seed);
    if (delays) {
        CommandEncoder::SetDelayForTesting([seed]() mutable {
            thread_local std::mt19937 delay(seed * 7 + 1);
            const auto pause = delay() % 4;
            if (pause == 1) std::this_thread::yield();
            if (pause == 2) std::this_thread::sleep_for(std::chrono::microseconds(delay() % 200));
        });
    }
    {
        const auto attached = Attached(device, table);
        auto& encoder = *attached;
        Oracle oracle{table, {}};
        const VkCommandBuffer buffers[2]{Handle<VkCommandBuffer>(0x100), Handle<VkCommandBuffer>(0x200)};
        std::uint32_t depth = 0;
        VkCommandBuffer open = VK_NULL_HANDLE;
        for (std::uint32_t step = 0; step < 20000; ++step) {
            const auto choice = random() % 20;
            if (choice == 0 && depth < 3) {
                if (depth == 0) open = buffers[random() % 2];
                encoder.Open(open, RecordingOf(open));
                ++depth;
            } else if (choice == 1 && depth != 0) {
                encoder.Close();
                --depth;
            } else if (choice == 2) {
                encoder.Quiesce();
            } else if (choice == 3) {
                AgcDriver::Graphics::BeforeCommandLookup(device);
            } else if (choice == 4) {
                encoder.Quiesce();
                const auto commands = buffers[random() % 2];
                encoder.Real().cmdDraw(commands, 1000 + step, 1, 0, 0);
                oracle.FakeExpect(commands, "draw " + std::to_string(1000 + step) + " 1 0 0");
            } else {
                oracle.call(random, buffers[random() % 2]);
            }
        }
        while (depth != 0) {
            encoder.Close();
            --depth;
        }
        encoder.Quiesce();
        Compare(oracle.expected, delays ? "random program with encoder delays" : "random program");
        const auto counts = encoder.Counts();
        Require(counts.commands != 0, "the encoder recorded no command: the windows deferred nothing");
    }
    CommandEncoder::SetDelayForTesting({});
    Require(table.cmdDraw == &FakeDraw, "the device table kept an encoder entry after the encoder was destroyed");
}

// A command the encoder does not interpose runs on the producing thread at once, ahead of everything
// queued before it: a state it sets lands on the wrong draws and races the encoder's own recording.
// Every command of the device table must therefore be replaced, so the next command added to the
// table without an encoder entry fails here instead of in a draw. The three listed members are the
// table's non-command entry points.
void EveryCommandOfTheTableIsInterposed() {
    static_assert(std::is_standard_layout_v<DeviceFunctions> && sizeof(DeviceFunctions) % sizeof(void*) == 0, "the table must be a plain array of entry points");
    constexpr std::size_t entries = sizeof(DeviceFunctions) / sizeof(void*);
    const std::size_t notCommands[]{offsetof(DeviceFunctions, updateDescriptorSets) / sizeof(void*), offsetof(DeviceFunctions, allocateDescriptorSets) / sizeof(void*), offsetof(DeviceFunctions, getFenceStatus) / sizeof(void*)};
    DeviceFunctions table{};
    void* before[entries];
    for (std::size_t index = 0; index < entries; ++index) before[index] = reinterpret_cast<void*>(&FakeDraw);
    std::memcpy(&table, before, sizeof(table));
    {
        const auto attached = Attached(Handle<VkDevice>(0x4f), table);
        void* after[entries];
        std::memcpy(after, &table, sizeof(table));
        for (std::size_t index = 0; index < entries; ++index) {
            const bool command = std::find(std::begin(notCommands), std::end(notCommands), index) == std::end(notCommands);
            Require(!command || after[index] != before[index], "entry " + std::to_string(index) + " of the device table is a command the encoder does not interpose; add it to install() in CommandEncoder.cpp, or list it as a non-command here");
            Require(command || after[index] == before[index], "entry " + std::to_string(index) + " of the device table is not a command but the encoder replaced it");
        }
    }
    Require(std::memcmp(&table, before, sizeof(table)) == 0, "destroying the encoder did not restore the device table");
}

void OtherThreadsRecordDirectlyWhileAWindowIsOpen() {
    Reset();
    auto table = FakeTable();
    const auto device = Handle<VkDevice>(0x41);
    const auto attached = Attached(device, table);
    auto& encoder = *attached;
    const auto mine = Handle<VkCommandBuffer>(0x300);
    const auto theirs = Handle<VkCommandBuffer>(0x400);
    std::map<VkCommandBuffer, std::vector<std::string>> expected;
    std::thread other([&] {
        for (std::uint32_t index = 0; index < 5000; ++index) table.cmdDraw(theirs, index, 2, 0, 0);
    });
    encoder.Open(mine, RecordingOf(mine));
    for (std::uint32_t index = 0; index < 5000; ++index) {
        table.cmdDraw(mine, index, 1, 0, 0);
        expected[mine].push_back("draw " + std::to_string(index) + " 1 0 0");
        expected[theirs].push_back("draw " + std::to_string(index) + " 2 0 0");
    }
    encoder.Close();
    other.join();
    encoder.Quiesce();
    Compare(expected, "another thread's command buffer");
}

void HookDuringUnencodedOpsWaits() {
    Reset();
    auto table = FakeTable();
    const auto device = Handle<VkDevice>(0x42);
    const auto attached = Attached(device, table);
    auto& encoder = *attached;
    const auto commands = Handle<VkCommandBuffer>(0x500);
    std::atomic<bool> release{false};
    CommandEncoder::SetDelayForTesting([&] {
        while (!release.load()) std::this_thread::yield();
    });
    encoder.Open(commands, RecordingOf(commands));
    table.cmdDraw(commands, 1, 1, 0, 0);
    encoder.Close();
    std::thread releaser([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        release.store(true);
    });
    table.cmdDraw(commands, 2, 1, 0, 0);
    releaser.join();
    CommandEncoder::SetDelayForTesting({});
    encoder.Quiesce();
    Compare({{commands, {"draw 1 1 0 0", "draw 2 1 0 0"}}}, "a direct call after a closed window");
}

void NestedWindowKeepsOrder() {
    Reset();
    auto table = FakeTable();
    const auto device = Handle<VkDevice>(0x43);
    const auto attached = Attached(device, table);
    auto& encoder = *attached;
    const auto outer = Handle<VkCommandBuffer>(0x600);
    const auto inner = Handle<VkCommandBuffer>(0x700);
    encoder.Open(outer, RecordingOf(outer));
    table.cmdDraw(outer, 1, 1, 0, 0);
    encoder.Open(outer, RecordingOf(outer));
    table.cmdDraw(outer, 2, 1, 0, 0);
    encoder.Close();
    table.cmdDraw(outer, 3, 1, 0, 0);
    encoder.Open(inner, RecordingOf(inner));
    table.cmdDraw(inner, 4, 1, 0, 0);
    table.cmdDraw(outer, 5, 1, 0, 0);
    encoder.Close();
    table.cmdDraw(outer, 6, 1, 0, 0);
    encoder.Close();
    table.cmdDraw(outer, 7, 1, 0, 0);
    encoder.Quiesce();
    Compare({{outer, {"draw 1 1 0 0", "draw 2 1 0 0", "draw 3 1 0 0", "draw 5 1 0 0", "draw 6 1 0 0", "draw 7 1 0 0"}}, {inner, {"draw 4 1 0 0"}}}, "nested windows");
}

void LargeRecordsSpanChunks() {
    Reset();
    auto table = FakeTable();
    const auto device = Handle<VkDevice>(0x44);
    const auto attached = Attached(device, table);
    auto& encoder = *attached;
    const auto commands = Handle<VkCommandBuffer>(0x800);
    std::map<VkCommandBuffer, std::vector<std::string>> expected;
    encoder.Open(commands, RecordingOf(commands));
    for (std::uint32_t round = 0; round < 64; ++round) {
        std::vector<std::uint8_t> bytes(4096 + round);
        std::string text = "push 1 1 0";
        for (std::size_t index = 0; index < bytes.size(); ++index) {
            bytes[index] = static_cast<std::uint8_t>(round + index);
            text += " " + std::to_string(bytes[index]);
        }
        table.cmdPushConstants(commands, Handle<VkPipelineLayout>(1), VK_SHADER_STAGE_VERTEX_BIT, 0, static_cast<std::uint32_t>(bytes.size()), bytes.data());
        expected[commands].push_back(text);
    }
    std::vector<std::uint8_t> huge(256 * 1024, 7);
    table.cmdPushConstants(commands, Handle<VkPipelineLayout>(1), VK_SHADER_STAGE_VERTEX_BIT, 0, static_cast<std::uint32_t>(huge.size()), huge.data());
    std::string text = "push 1 1 0";
    for (const auto byte : huge) text += " " + std::to_string(byte);
    expected[commands].push_back(text);
    encoder.Close();
    encoder.Quiesce();
    Compare(expected, "records larger than a chunk");
}

void RecordEveryPacked(DeviceFunctions& table, VkCommandBuffer commands, std::uint32_t round, std::vector<std::string>& expected) {
    VkBufferCopy copies[2]{{round, 16 + round, 4}, {32, 48, 8 + round}};
    table.cmdCopyBuffer(commands, Handle<VkBuffer>(1), Handle<VkBuffer>(2), 2, copies);
    expected.push_back("copy 1 2 " + std::to_string(round) + ">" + std::to_string(16 + round) + ":4 32>48:" + std::to_string(8 + round));
    std::uint8_t bytes[3]{static_cast<std::uint8_t>(round), 9, 8};
    table.cmdUpdateBuffer(commands, Handle<VkBuffer>(3), 4 * round, 3, bytes);
    expected.push_back("update 3 " + std::to_string(4 * round) + " " + std::to_string(round) + " 9 8");
    VkBufferImageCopy regions[1]{};
    regions[0].bufferOffset = 256 * round;
    regions[0].imageExtent = {8 + round, 4, 1};
    table.cmdCopyBufferToImage(commands, Handle<VkBuffer>(4), Handle<VkImage>(5), VK_IMAGE_LAYOUT_GENERAL, 1, regions);
    expected.push_back("to-image 4 5 1 " + std::to_string(256 * round) + "@" + std::to_string(8 + round) + "x4");
    table.cmdCopyImageToBuffer(commands, Handle<VkImage>(5), VK_IMAGE_LAYOUT_GENERAL, Handle<VkBuffer>(6), 1, regions);
    expected.push_back("to-buffer 5 1 6 " + std::to_string(256 * round) + "@" + std::to_string(8 + round) + "x4");
    VkClearColorValue color{};
    color.uint32[0] = round;
    color.uint32[3] = 7;
    const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, round, 1, 0, 1};
    table.cmdClearColorImage(commands, Handle<VkImage>(7), VK_IMAGE_LAYOUT_GENERAL, &color, 1, &range);
    expected.push_back("clear 7 1 " + std::to_string(round) + ",7 " + std::to_string(round) + "+1");
    table.cmdBeginQuery(commands, Handle<VkQueryPool>(8), round, 1);
    expected.push_back("begin-query 8 " + std::to_string(round) + " 1");
    table.cmdEndQuery(commands, Handle<VkQueryPool>(8), round);
    expected.push_back("end-query 8 " + std::to_string(round));
    copies[0] = {};
    bytes[0] = 0;
    regions[0] = {};
    color = {};
}

void TransfersAndQueriesInAWindowNeverWait() {
    Reset();
    auto table = FakeTable();
    const auto device = Handle<VkDevice>(0x46);
    const auto attached = Attached(device, table);
    auto& encoder = *attached;
    const auto commands = Handle<VkCommandBuffer>(0xa00);
    CommandEncoder::SetDelayForTesting([] { std::this_thread::sleep_for(std::chrono::milliseconds(5)); });
    std::vector<std::string> expected;
    for (std::uint32_t round = 0; round < 4; ++round) {
        encoder.Open(commands, RecordingOf(commands));
        table.cmdDraw(commands, round, 1, 0, 0);
        expected.push_back("draw " + std::to_string(round) + " 1 0 0");
        RecordEveryPacked(table, commands, round, expected);
        Require(encoder.Counts().waits == 0, "a transfer or query command in a window waited for the encoder");
        encoder.Close();
    }
    encoder.Quiesce();
    CommandEncoder::SetDelayForTesting({});
    Compare({{commands, expected}}, "transfers and queries in a window");
}

void UnwindingSettlesQueuedRecording() {
    Reset();
    auto table = FakeTable();
    const auto device = Handle<VkDevice>(0x45);
    const auto attached = Attached(device, table);
    auto& encoder = *attached;
    const auto commands = Handle<VkCommandBuffer>(0x900);
    CommandEncoder::SetDelayForTesting([] { std::this_thread::sleep_for(std::chrono::milliseconds(30)); });
    std::size_t recordedAtCatch = 0;
    try {
        encoder.Open(commands, RecordingOf(commands));
        const CommandEncoder::UnwindQuiesce guard(attached.get());
        table.cmdDraw(commands, 1, 1, 0, 0);
        table.cmdDraw(commands, 2, 1, 0, 0);
        encoder.Close();
        throw std::runtime_error("injected failure after the window closed");
    } catch (const std::runtime_error&) {
        std::lock_guard lock(logMutex);
        recordedAtCatch = recorded[commands].size();
    }
    CommandEncoder::SetDelayForTesting({});
    Require(recordedAtCatch == 2, "unwinding released the owners of a queued draw while the encoder had recorded " + std::to_string(recordedAtCatch) + " of 2 commands");
    encoder.Quiesce();
}

struct RecordedAtDestruction {
    VkCommandBuffer commands;
    std::size_t& seen;
    ~RecordedAtDestruction() {
        std::lock_guard lock(logMutex);
        seen = recorded[commands].size();
    }
};

void OwnersOutliveTheUnwindDrain() {
    Reset();
    auto table = FakeTable();
    const auto device = Handle<VkDevice>(0x46);
    const auto attached = Attached(device, table);
    auto& encoder = *attached;
    const auto commands = Handle<VkCommandBuffer>(0xa00);
    CommandEncoder::SetDelayForTesting([] { std::this_thread::sleep_for(std::chrono::milliseconds(30)); });
    std::size_t recordedAtRelease = 0;
    try {
        encoder.Open(commands, RecordingOf(commands));
        RecordedAtDestruction owner{commands, recordedAtRelease};
        const CommandEncoder::UnwindQuiesce guard(attached.get());
        table.cmdDraw(commands, 1, 1, 0, 0);
        table.cmdDraw(commands, 2, 1, 0, 0);
        encoder.Close();
        throw std::runtime_error("injected failure after the window closed");
    } catch (const std::runtime_error&) {
    }
    CommandEncoder::SetDelayForTesting({});
    Require(recordedAtRelease == 2, "an owner of a queued draw was released while the encoder had recorded " + std::to_string(recordedAtRelease) + " of 2 commands");
    encoder.Quiesce();
}

void ForeignThreadCannotProduceIntoAnOpenWindow() {
    Reset();
    auto table = FakeTable();
    const auto device = Handle<VkDevice>(0x47);
    const auto attached = Attached(device, table);
    auto& encoder = *attached;
    const auto commands = Handle<VkCommandBuffer>(0xb00);
    encoder.Open(commands, RecordingOf(commands));
    table.cmdDraw(commands, 1, 1, 0, 0);
    std::atomic<bool> refused{false};
    std::thread other([&] {
        try {
            table.cmdDraw(commands, 9, 1, 0, 0);
        } catch (const std::logic_error&) {
            refused.store(true);
        }
    });
    other.join();
    Require(refused.load(), "a call on a command buffer with an open window from another thread was not refused");
    table.cmdDraw(commands, 2, 1, 0, 0);
    encoder.Close();
    std::thread later([&] { table.cmdDraw(commands, 3, 1, 0, 0); });
    later.join();
    encoder.Quiesce();
    Compare({{commands, {"draw 1 1 0 0", "draw 2 1 0 0", "draw 3 1 0 0"}}}, "a foreign producer");
}

void ForeignLookupWhileAWindowIsOpenProceeds() {
    Reset();
    auto table = FakeTable();
    const auto device = Handle<VkDevice>(0x48);
    const auto attached = Attached(device, table);
    auto& encoder = *attached;
    const auto commands = Handle<VkCommandBuffer>(0xc00);
    const auto own = Handle<VkCommandBuffer>(0xc80);
    encoder.Open(commands, RecordingOf(commands));
    table.cmdDraw(commands, 1, 1, 0, 0);
    std::atomic<bool> refused{false};
    std::thread other([&] {
        try {
            AgcDriver::Graphics::BeforeCommandLookup(device);
            table.cmdDraw(own, 7, 1, 0, 0);
        } catch (const std::logic_error&) {
            refused.store(true);
        }
    });
    other.join();
    Require(!refused.load(), "a command lookup from another thread for its own command buffer was refused while a window was open");
    table.cmdDraw(commands, 2, 1, 0, 0);
    encoder.Close();
    encoder.Quiesce();
    Compare({{commands, {"draw 1 1 0 0", "draw 2 1 0 0"}}, {own, {"draw 7 1 0 0"}}}, "a foreign lookup");
}

void RawCallAfterLookupKeepsOrder() {
    Reset();
    auto table = FakeTable();
    const auto raw = table;
    const auto device = Handle<VkDevice>(0x49);
    const auto attached = Attached(device, table);
    auto& encoder = *attached;
    const auto commands = Handle<VkCommandBuffer>(0xd00);
    CommandEncoder::SetDelayForTesting([] { std::this_thread::sleep_for(std::chrono::milliseconds(5)); });
    encoder.Open(commands, RecordingOf(commands));
    table.cmdDraw(commands, 1, 1, 0, 0);
    table.cmdDraw(commands, 2, 1, 0, 0);
    AgcDriver::Graphics::BeforeCommandLookup(device);
    raw.cmdDraw(commands, 3, 1, 0, 0);
    table.cmdDraw(commands, 4, 1, 0, 0);
    encoder.Close();
    encoder.Quiesce();
    CommandEncoder::SetDelayForTesting({});
    Compare({{commands, {"draw 1 1 0 0", "draw 2 1 0 0", "draw 3 1 0 0", "draw 4 1 0 0"}}}, "a raw call after a lookup inside a window");
}

}

bool Reports(const std::function<void()>& settle) {
    try {
        settle();
        return false;
    } catch (const std::runtime_error& error) {
        Require(std::string(error.what()) == "injected replay failure", std::string("the encoder reported '") + error.what() + "' for a failed replay");
        return true;
    }
}

bool Refuses(const std::function<void()>& finish) {
    try {
        finish();
        return false;
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) != "injected replay failure";
    }
}

void FailedReplayStaysWithItsRecording() {
    Reset();
    auto table = FakeTable();
    const auto device = Handle<VkDevice>(0x4a);
    const auto failing = Handle<VkCommandBuffer>(0xe00);
    const auto healthy = Handle<VkCommandBuffer>(0xe80);
    {
        const auto attached = Attached(device, table);
        auto& encoder = *attached;
        std::atomic<int> failures{1};
        CommandEncoder::SetDelayForTesting([&] {
            if (failures.fetch_sub(1) > 0) throw std::runtime_error("injected replay failure");
        });
        std::thread producer([&] {
            encoder.Open(failing, RecordingOf(failing));
            table.cmdDraw(failing, 1, 1, 0, 0);
            encoder.Close();
            encoder.Quiesce();
        });
        producer.join();
        std::thread other([&] {
            encoder.Settle();
            AgcDriver::Graphics::BeforeCommandLookup(device);
        });
        other.join();
        encoder.Open(healthy, RecordingOf(healthy));
        table.cmdDraw(healthy, 2, 1, 0, 0);
        encoder.Close();
        encoder.Open(failing, RecordingOf(failing));
        table.cmdDraw(failing, 3, 1, 0, 0);
        encoder.Close();
        encoder.Quiesce();
        CommandEncoder::SetDelayForTesting({});
    }
    Require(RecordingOf(failing).Failed() && !RecordingOf(healthy).Failed(), "a replay failure was not kept with the recording it happened in");
    Require(Reports([&] { RecordingOf(failing).Report(); }), "a failed replay recorded by a thread that has exited was not reported to the next settle of its recording");
    Require(!Reports([&] { RecordingOf(failing).Report(); }), "a failed replay was reported twice");
    Require(Refuses([&] { RecordingOf(failing).Finish(); }), "finishing a recording whose reported replay failure was not refused, or reported it again");
    Require(!Reports([&] { RecordingOf(healthy).Finish(); }), "finishing a healthy recording reported another recording's failed replay");
    Require(Recorded(failing) == 0, "the encoder recorded " + std::to_string(Recorded(failing)) + " commands into a recording after a replay into it failed");
    Compare({{healthy, {"draw 2 1 0 0"}}}, "another recording after a failed replay");
}

void FinishCarriesAnUnreportedFailure() {
    Reset();
    auto table = FakeTable();
    const auto device = Handle<VkDevice>(0x4b);
    const auto failing = Handle<VkCommandBuffer>(0xf00);
    const auto attached = Attached(device, table);
    auto& encoder = *attached;
    std::atomic<int> failures{1};
    CommandEncoder::SetDelayForTesting([&] {
        if (failures.fetch_sub(1) > 0) throw std::runtime_error("injected replay failure");
    });
    encoder.Open(failing, RecordingOf(failing));
    table.cmdDraw(failing, 1, 1, 0, 0);
    encoder.Close();
    encoder.Quiesce();
    CommandEncoder::SetDelayForTesting({});
    std::atomic<bool> carried{false};
    std::thread submitter([&] { carried.store(Reports([&] { RecordingOf(failing).Finish(); })); });
    submitter.join();
    Require(carried.load(), "finishing a recording on another thread did not carry its unreported replay failure");
    Require(!Reports([&] { RecordingOf(failing).Report(); }), "a failed replay carried by finishing its recording was reported again");
}

void DirectCallsSkipAFailedRecording() {
    Reset();
    auto table = FakeTable();
    const auto device = Handle<VkDevice>(0x4e);
    const auto failing = Handle<VkCommandBuffer>(0x1200);
    const auto healthy = Handle<VkCommandBuffer>(0x1280);
    const auto attached = Attached(device, table);
    auto& encoder = *attached;
    std::atomic<int> failures{1};
    CommandEncoder::SetDelayForTesting([&] {
        if (failures.fetch_sub(1) > 0) throw std::runtime_error("injected replay failure");
    });
    const std::uint8_t bytes[] = {7};
    const VkMemoryBarrier chainedLink{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    const VkMemoryBarrier chained{VK_STRUCTURE_TYPE_MEMORY_BARRIER, &chainedLink, 1, 2};
    encoder.Open(failing, RecordingOf(failing));
    table.cmdDraw(failing, 1, 1, 0, 0);
    table.cmdUpdateBuffer(failing, Handle<VkBuffer>(0x30), 0, 1, bytes);
    table.cmdPipelineBarrier(failing, 1, 2, 0, 1, &chained, 0, nullptr, 0, nullptr);
    encoder.Close();
    encoder.Open(healthy, RecordingOf(healthy));
    table.cmdUpdateBuffer(healthy, Handle<VkBuffer>(0x31), 0, 1, bytes);
    encoder.Close();
    encoder.Quiesce();
    CommandEncoder::SetDelayForTesting({});
    Require(Recorded(failing) == 0, "a window recorded " + std::to_string(Recorded(failing)) + " direct calls into a recording after a replay into it failed");
    Require(Reports([&] { RecordingOf(failing).Report(); }), "a failed replay followed by direct calls was not reported");
    Compare({{healthy, {"update 49 0 7"}}}, "a direct call into another recording after a failed replay");
}

void AppendFailureKeepsTheWindow() {
    Reset();
    auto table = FakeTable();
    const auto device = Handle<VkDevice>(0x4f);
    const auto commands = Handle<VkCommandBuffer>(0x1300);
    const auto attached = Attached(device, table);
    auto& encoder = *attached;
    encoder.Open(commands, RecordingOf(commands));
    table.cmdDraw(commands, 1, 1, 0, 0);
    bool refused = false;
    try {
        static_cast<void>(encoder.Append([](const std::byte*, VkCommandBuffer, const DeviceFunctions&) {}, static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max() / 2)));
    } catch (const std::exception&) {
        refused = true;
    }
    table.cmdDraw(commands, 2, 1, 0, 0);
    encoder.Close();
    encoder.Quiesce();
    Require(refused, "a record too large to allocate was appended");
    Compare({{commands, {"draw 1 1 0 0", "draw 2 1 0 0"}}}, "a window after a record failed to allocate");
}

void DevicesPastTheSlotsRecordInline() {
    Reset();
    std::vector<DeviceFunctions> tables(CommandEncoder::Slots + 1, FakeTable());
    std::vector<std::unique_ptr<CommandEncoder>> encoders;
    for (std::size_t index = 0; index < tables.size(); ++index) encoders.push_back(CommandEncoder::Attach(Handle<VkDevice>(0x60 + index), tables[index]));
    Require(encoders.back() == nullptr, "a device past the encoder slots got an encoder");
    Require(std::all_of(encoders.begin(), encoders.end() - 1, [](const auto& encoder) { return encoder != nullptr; }), "a device within the encoder slots got no encoder");
    Require(tables.back().cmdDraw == &FakeDraw, "a device without an encoder had its table patched");
    encoders.front().reset();
    Require(CommandEncoder::Attach(Handle<VkDevice>(0x70), tables.back()) != nullptr, "a slot freed by a destroyed encoder was not reused");
    Require(tables.back().cmdDraw == &FakeDraw, "the reused slot's encoder left its table patched");
}

void PostingNeverAllocates() {
    Reset();
    auto table = FakeTable();
    const auto device = Handle<VkDevice>(0x4c);
    const auto commands = Handle<VkCommandBuffer>(0x1000);
    const auto attached = Attached(device, table);
    auto& encoder = *attached;
    std::atomic<bool> release{false};
    CommandEncoder::SetDelayForTesting([&] {
        while (!release.load()) std::this_thread::yield();
    });
    for (std::uint32_t index = 0; index < 200; ++index) {
        encoder.Open(commands, RecordingOf(commands));
        table.cmdDraw(commands, index, 1, 0, 0);
        bool allocated = false;
        AgcDriver::Tests::SetAllocationFailureCountdown(0);
        try {
            encoder.Close();
        } catch (const std::bad_alloc&) {
            allocated = true;
        }
        AgcDriver::Tests::SetAllocationFailureCountdown(-1);
        Require(!allocated, "posting chunk " + std::to_string(index) + " allocated memory");
    }
    encoder.Open(commands, RecordingOf(commands));
    table.cmdDraw(commands, 200, 1, 0, 0);
    release.store(true);
    AgcDriver::Tests::SetAllocationFailureCountdown(0);
    encoder.Quiesce();
    AgcDriver::Tests::SetAllocationFailureCountdown(-1);
    encoder.Close();
    CommandEncoder::SetDelayForTesting({});
    Require(Recorded(commands) == 201, "the encoder recorded " + std::to_string(Recorded(commands)) + " of 201 commands posted without allocating");
}

void ReplayFailureNeedsNoAllocation() {
    Reset();
    auto table = FakeTable();
    const auto device = Handle<VkDevice>(0x4d);
    const auto failing = Handle<VkCommandBuffer>(0x1100);
    const auto healthy = Handle<VkCommandBuffer>(0x1180);
    const auto attached = Attached(device, table);
    auto& encoder = *attached;
    const std::runtime_error injected("injected replay failure");
    std::atomic<int> calls{0};
    CommandEncoder::SetDelayForTesting([&] {
        if (calls.fetch_add(1) == 0) {
            AgcDriver::Tests::SetAllocationFailureCountdown(0);
            throw injected;
        }
        AgcDriver::Tests::SetAllocationFailureCountdown(-1);
    });
    encoder.Open(failing, RecordingOf(failing));
    table.cmdDraw(failing, 1, 1, 0, 0);
    encoder.Close();
    encoder.Quiesce();
    encoder.Open(healthy, RecordingOf(healthy));
    table.cmdDraw(healthy, 2, 1, 0, 0);
    encoder.Close();
    encoder.Quiesce();
    CommandEncoder::SetDelayForTesting({});
    Require(Reports([&] { RecordingOf(failing).Finish(); }), "a failed replay was not kept after its failure was stored without allocating");
    Compare({{healthy, {"draw 2 1 0 0"}}}, "a replay after a failure stored without allocating");
}

void PackedRecordsSkipAFailedRecording() {
    Reset();
    auto table = FakeTable();
    const auto device = Handle<VkDevice>(0x50);
    const auto failing = Handle<VkCommandBuffer>(0x1400);
    const auto healthy = Handle<VkCommandBuffer>(0x1480);
    const auto attached = Attached(device, table);
    auto& encoder = *attached;
    std::atomic<int> failures{1};
    CommandEncoder::SetDelayForTesting([&] {
        if (failures.fetch_sub(1) > 0) throw std::runtime_error("injected replay failure");
    });
    encoder.Open(failing, RecordingOf(failing));
    table.cmdDraw(failing, 1, 1, 0, 0);
    encoder.Close();
    encoder.Quiesce();
    Require(RecordingOf(failing).Failed(), "the injected replay failure was not stored in its recording");
    std::vector<std::string> expected;
    std::vector<std::string> dropped;
    for (std::uint32_t round = 0; round < 3; ++round) {
        encoder.Open(failing, RecordingOf(failing));
        RecordEveryPacked(table, failing, round, dropped);
        encoder.Open(healthy, RecordingOf(healthy));
        RecordEveryPacked(table, healthy, round, expected);
        encoder.Close();
        RecordEveryPacked(table, failing, round + 8, dropped);
        encoder.Close();
    }
    encoder.Quiesce();
    CommandEncoder::SetDelayForTesting({});
    Require(Recorded(failing) == 0, "the encoder recorded " + std::to_string(Recorded(failing)) + " of " + std::to_string(dropped.size()) + " packed transfer and query commands into a recording after a replay into it failed");
    Require(Reports([&] { RecordingOf(failing).Report(); }), "a failed replay followed by packed records was not reported");
    Require(!RecordingOf(healthy).Failed(), "another recording's failed replay marked a healthy recording failed");
    Compare({{healthy, expected}}, "packed records in another recording after a failed replay");
}

void QueriesKeepOrderWithTheirRecording() {
    Reset();
    auto table = FakeTable();
    const auto device = Handle<VkDevice>(0x51);
    const auto first = Handle<VkCommandBuffer>(0x1500);
    const auto second = Handle<VkCommandBuffer>(0x1580);
    CommandEncoder::SetDelayForTesting([] {
        thread_local std::mt19937 delay(17);
        const auto pause = delay() % 4;
        if (pause == 1) std::this_thread::yield();
        if (pause == 2) std::this_thread::sleep_for(std::chrono::microseconds(delay() % 300));
    });
    std::map<VkCommandBuffer, std::vector<std::string>> expected;
    {
        const auto attached = Attached(device, table);
        auto& encoder = *attached;
        const auto query = [&](VkCommandBuffer commands, std::uint32_t slot) {
            table.cmdBeginQuery(commands, Handle<VkQueryPool>(9), slot, 0);
            expected[commands].push_back("begin-query 9 " + std::to_string(slot) + " 0");
            table.cmdDraw(commands, slot, 1, 0, 0);
            expected[commands].push_back("draw " + std::to_string(slot) + " 1 0 0");
            table.cmdEndQuery(commands, Handle<VkQueryPool>(9), slot);
            expected[commands].push_back("end-query 9 " + std::to_string(slot));
        };
        for (std::uint32_t round = 0; round < 200; ++round) {
            const auto outer = round % 2 == 0 ? first : second;
            const auto inner = round % 2 == 0 ? second : first;
            encoder.Open(outer, RecordingOf(outer));
            table.cmdBeginQuery(outer, Handle<VkQueryPool>(8), round, 1);
            expected[outer].push_back("begin-query 8 " + std::to_string(round) + " 1");
            table.cmdDraw(outer, 1000 + round, 1, 0, 0);
            expected[outer].push_back("draw " + std::to_string(1000 + round) + " 1 0 0");
            encoder.Open(inner, RecordingOf(inner));
            query(inner, 2 * round);
            encoder.Close();
            table.cmdEndQuery(outer, Handle<VkQueryPool>(8), round);
            expected[outer].push_back("end-query 8 " + std::to_string(round));
            encoder.Close();
            query(outer, 2 * round + 1);
        }
        encoder.Quiesce();
        Require(!RecordingOf(first).Failed() && !RecordingOf(second).Failed(), "a recording with queries was marked failed");
    }
    CommandEncoder::SetDelayForTesting({});
    Compare(expected, "queries around nested windows of two recordings");
}

int main() {
    try {
        for (std::uint32_t seed = 1; seed <= 6; ++seed) RandomProgramRecordsWhatInlineRecords(seed, seed % 2 == 0);
        OtherThreadsRecordDirectlyWhileAWindowIsOpen();
        HookDuringUnencodedOpsWaits();
        EveryCommandOfTheTableIsInterposed();
        NestedWindowKeepsOrder();
        LargeRecordsSpanChunks();
        TransfersAndQueriesInAWindowNeverWait();
        PackedRecordsSkipAFailedRecording();
        QueriesKeepOrderWithTheirRecording();
        UnwindingSettlesQueuedRecording();
        OwnersOutliveTheUnwindDrain();
        ForeignThreadCannotProduceIntoAnOpenWindow();
        ForeignLookupWhileAWindowIsOpenProceeds();
        RawCallAfterLookupKeepsOrder();
        FailedReplayStaysWithItsRecording();
        FinishCarriesAnUnreportedFailure();
        DirectCallsSkipAFailedRecording();
        AppendFailureKeepsTheWindow();
        DevicesPastTheSlotsRecordInline();
        PostingNeverAllocates();
        ReplayFailureNeedsNoAllocation();
        std::cout << "command encoder tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
