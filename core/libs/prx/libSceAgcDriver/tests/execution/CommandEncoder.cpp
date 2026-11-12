#include "prx/libSceAgcDriver/Graphics/include/CommandEncoder.hpp"
#include "prx/libSceAgcDriver/tests/AllocationFailure.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <csignal>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <new>
#include <memory>
#include <mutex>
#include <random>
#include <source_location>
#include <span>
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

const CommandEncoder::SiteWaits* SiteOf(const std::vector<CommandEncoder::SiteWaits>& sites, const CommandEncoder::WaitSite& site) {
    const auto wanted = CommandEncoder::Describe(site);
    const CommandEncoder::SiteWaits* found = nullptr;
    for (const auto& entry : sites) {
        if (CommandEncoder::Describe(entry.site) != wanted) continue;
        Require(found == nullptr, "the wait site " + wanted + " was reported twice");
        found = &entry;
    }
    return found;
}

void RequireSite(const std::vector<CommandEncoder::SiteWaits>& sites, const CommandEncoder::WaitSite& site, std::uint64_t waits, std::uint64_t flushes) {
    const auto* entry = SiteOf(sites, site);
    const auto name = CommandEncoder::Describe(site);
    Require(entry != nullptr, "no waits were attributed to " + name + "; sites:" + CommandEncoder::Report(sites, sites.size()));
    Require(entry->waits == waits, name + " waited " + std::to_string(entry->waits) + " times, expected " + std::to_string(waits));
    Require(entry->flushes == flushes, name + " flushed " + std::to_string(entry->flushes) + " chunks, expected " + std::to_string(flushes));
    Require(entry->waitNs >= waits * 1'000'000ull, name + " reported " + std::to_string(entry->waitNs) + " ns for " + std::to_string(waits) + " waits on a 2 ms replay");
}

void SetEnvironment(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, 1);
#endif
}

void SitesAreCountedOnlyWhenProfiled() {
    SetEnvironment("APS5_PROFILE_ENCODER", "0");
    Reset();
    auto table = FakeTable();
    const auto device = Handle<VkDevice>(0x55);
    const auto attached = Attached(device, table);
    auto& encoder = *attached;
    const auto commands = Handle<VkCommandBuffer>(0x1800);
    CommandEncoder::SetDelayForTesting([] { std::this_thread::sleep_for(std::chrono::milliseconds(2)); });
    encoder.Open(commands, RecordingOf(commands));
    table.cmdDraw(commands, 1, 1, 0, 0);
    encoder.Quiesce();
    encoder.Close();
    CommandEncoder::SetDelayForTesting({});
    Require(!encoder.ProfilesSites(), "the encoder profiles its wait sites with APS5_PROFILE_ENCODER=0");
    Require(encoder.Counts().waits == 1, "the encoder's wait total is not kept without APS5_PROFILE_ENCODER");
    Require(encoder.WaitsBySite().empty(), "wait sites were counted without APS5_PROFILE_ENCODER:" + CommandEncoder::Report(encoder.WaitsBySite(), CommandEncoder::TrackedSites));
}

void WaitsAreAttributedToTheirCallSites() {
    using Wait = CommandEncoder::Wait;
    using Site = CommandEncoder::WaitSite;
    SetEnvironment("APS5_PROFILE_ENCODER", "1");
    Reset();
    auto table = FakeTable();
    const auto device = Handle<VkDevice>(0x53);
    const auto attached = Attached(device, table);
    auto& encoder = *attached;
    Require(encoder.ProfilesSites(), "the encoder does not profile its wait sites with APS5_PROFILE_ENCODER=1");
    const auto first = Handle<VkCommandBuffer>(0x1500);
    const auto second = Handle<VkCommandBuffer>(0x1580);
    const std::uint32_t word = 7;
    CommandEncoder::SetDelayForTesting([] { std::this_thread::sleep_for(std::chrono::milliseconds(2)); });
    const auto repeated = std::source_location::current();
    const auto once = std::source_location::current();
    const auto lookup = std::source_location::current();
    const auto opened = std::source_location::current();
    const auto idle = std::source_location::current();
    encoder.Open(first, RecordingOf(first));
    for (int round = 0; round < 2; ++round) {
        table.cmdDraw(first, 1, 1, 0, 0);
        encoder.Quiesce(repeated);
    }
    table.cmdDraw(first, 2, 1, 0, 0);
    encoder.Quiesce(once);
    table.cmdDraw(first, 3, 1, 0, 0);
    table.cmdUpdateBuffer(first, Handle<VkBuffer>(0x1600), 0, sizeof(word), &word);
    table.cmdDraw(first, 4, 1, 0, 0);
    AgcDriver::Graphics::BeforeCommandLookup(device, lookup);
    table.cmdDraw(first, 5, 1, 0, 0);
    encoder.Close();
    table.cmdDraw(first, 6, 1, 0, 0);
    encoder.Open(first, RecordingOf(first));
    table.cmdDraw(first, 7, 1, 0, 0);
    encoder.Close();
    encoder.Open(second, RecordingOf(second), opened);
    encoder.Close();
    encoder.Quiesce(idle);
    CommandEncoder::SetDelayForTesting({});
    const auto sites = encoder.WaitsBySite();
    RequireSite(sites, Site::At(Wait::Quiesce, repeated), 2, 2);
    RequireSite(sites, Site::At(Wait::Quiesce, once), 1, 1);
    RequireSite(sites, Site{Wait::Direct, "cmdUpdateBuffer", 0}, 1, 1);
    RequireSite(sites, Site::At(Wait::Lookup, lookup), 1, 1);
    RequireSite(sites, Site{Wait::Behind, "cmdDraw", 0}, 1, 0);
    RequireSite(sites, Site::At(Wait::Open, opened), 1, 0);
    Require(SiteOf(sites, Site::At(Wait::Quiesce, idle)) == nullptr, "a quiesce with nothing posted and nothing to flush was attributed a wait");
    std::uint64_t waits = 0;
    for (const auto& entry : sites) waits += entry.waits;
    Require(waits == encoder.Counts().waits, "the waits by site do not add up to the encoder's waits");
    Require(sites.size() == 6, "expected six wait sites, got:" + CommandEncoder::Report(sites, sites.size()));
    const auto report = CommandEncoder::Report(sites, 2);
    Require(report.find("4 more sites") != std::string::npos && report.find(CommandEncoder::Describe(sites.front().site)) != std::string::npos, "the site report does not name the costliest site and the rest:" + report);
    Require(CommandEncoder::Describe(Site::At(Wait::Lookup, lookup)).find("WaitsAreAttributedToTheirCallSites:" + std::to_string(lookup.line())) != std::string::npos, "a call site is not described by its function and line: " + CommandEncoder::Describe(Site::At(Wait::Lookup, lookup)));
}

}

void WaitOnTheEncoderThread() {
    Reset();
    auto table = FakeTable();
    const auto device = Handle<VkDevice>(0x54);
    const auto attached = Attached(device, table);
    auto& encoder = *attached;
    const auto commands = Handle<VkCommandBuffer>(0x1700);
    CommandEncoder::SetDelayForTesting([device] { AgcDriver::Graphics::BeforeCommandLookup(device); });
    encoder.Open(commands, RecordingOf(commands));
    table.cmdDraw(commands, 1, 1, 0, 0);
    encoder.Close();
    encoder.Quiesce();
    CommandEncoder::SetDelayForTesting({});
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
    const std::vector<std::byte> oversized(128 * 1024);
    bool refused = false;
    try {
        AgcDriver::Tests::SetAllocationFailureCountdown(0);
        encoder.Emplace([](const std::byte*, VkCommandBuffer, const DeviceFunctions&) {}, std::uint32_t{0}, std::span<const std::byte>(oversized));
    } catch (const std::bad_alloc&) {
        refused = true;
    }
    AgcDriver::Tests::SetAllocationFailureCountdown(-1);
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
    for (std::uint32_t index = 0; index < 12; ++index) {
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
    table.cmdDraw(commands, 12, 1, 0, 0);
    release.store(true);
    AgcDriver::Tests::SetAllocationFailureCountdown(0);
    encoder.Quiesce();
    AgcDriver::Tests::SetAllocationFailureCountdown(-1);
    encoder.Close();
    CommandEncoder::SetDelayForTesting({});
    Require(Recorded(commands) == 13, "the encoder recorded " + std::to_string(Recorded(commands)) + " of 13 commands posted without allocating");
}

void PostOne(CommandEncoder& encoder, DeviceFunctions& table, VkCommandBuffer commands, std::uint32_t index) {
    encoder.Open(commands, RecordingOf(commands));
    table.cmdDraw(commands, index, 1, 0, 0);
    encoder.Close();
}

void BusyFor(std::chrono::nanoseconds span) {
    const auto until = std::chrono::steady_clock::now() + span;
    while (std::chrono::steady_clock::now() < until) {}
}

void EncoderParksBetweenSpacedPosts() {
    Reset();
    auto table = FakeTable();
    const auto device = Handle<VkDevice>(0x52);
    const auto commands = Handle<VkCommandBuffer>(0x1500);
    constexpr std::uint32_t Posts = 400;
    CommandEncoder::Statistics counts{};
    {
        const auto attached = Attached(device, table);
        auto& encoder = *attached;
        for (std::uint32_t index = 0; index < Posts; ++index) {
            PostOne(encoder, table, commands, index);
            encoder.Quiesce();
            BusyFor(std::chrono::microseconds(30));
        }
        counts = encoder.Counts();
    }
    Require(Recorded(commands) == Posts, "the encoder recorded " + std::to_string(Recorded(commands)) + " of " + std::to_string(Posts) + " spaced posts");
    Require(counts.parks * 2 >= Posts, "the encoder parked " + std::to_string(counts.parks) + " times between " + std::to_string(Posts) + " posts 30 us apart instead of after each one");
}

void ProducerAheadOfTheChunkPoolWaits() {
    Reset();
    auto table = FakeTable();
    const auto device = Handle<VkDevice>(0x4e);
    const auto commands = Handle<VkCommandBuffer>(0x1400);
    constexpr std::uint32_t Windows = 64;
    std::atomic<bool> release{false};
    std::atomic<bool> producerDone{false};
    bool doneWhileHeld = true;
    CommandEncoder::Statistics counts{};
    CommandEncoder::SetDelayForTesting([&] {
        while (!release.load()) std::this_thread::yield();
    });
    {
        const auto attached = Attached(device, table);
        auto& encoder = *attached;
        std::thread producer([&] {
            for (std::uint32_t index = 0; index < Windows; ++index) {
                encoder.Open(commands, RecordingOf(commands));
                table.cmdDraw(commands, index, 1, 0, 0);
                encoder.Close();
            }
            producerDone.store(true);
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        doneWhileHeld = producerDone.load();
        release.store(true);
        producer.join();
        encoder.Quiesce();
        counts = encoder.Counts();
    }
    CommandEncoder::SetDelayForTesting({});
    Require(!doneWhileHeld, "a producer posted " + std::to_string(Windows) + " windows ahead of a held encoder without waiting for its chunk pool");
    Require(Recorded(commands) == Windows && counts.waits != 0, "a producer that waited for the chunk pool recorded " + std::to_string(Recorded(commands)) + " of " + std::to_string(Windows) + " commands after " + std::to_string(counts.waits) + " waits");
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

double NanosecondsSince(std::chrono::steady_clock::time_point start) {
    return static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count());
}

std::atomic<std::uint64_t> replayNs{0};

VKAPI_ATTR void VKAPI_CALL BurnDraw(VkCommandBuffer, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) {
    BusyFor(std::chrono::nanoseconds(replayNs.load(std::memory_order_relaxed)));
}

DeviceFunctions BurnTable() {
    DeviceFunctions table{};
    table.cmdDraw = &BurnDraw;
    return table;
}

struct Pattern {
    const char* name;
    std::uint32_t batch;
    std::uint64_t workNs;
    std::uint64_t encodeNs;
};

double RunPattern(const Pattern& pattern, CommandEncoder::Handoff handoff, std::uint32_t draws, CommandEncoder::Statistics& counts) {
    const auto commands = Handle<VkCommandBuffer>(0x1700);
    auto table = BurnTable();
    replayNs.store(pattern.encodeNs, std::memory_order_relaxed);
    const auto attached = Attached(Handle<VkDevice>(0x57), table);
    auto& encoder = *attached;
    const auto start = std::chrono::steady_clock::now();
    for (std::uint32_t index = 0; index < draws; ++index) {
        BusyFor(std::chrono::nanoseconds(pattern.workNs));
        const bool encoded = encoder.Admit(handoff);
        if (encoded) encoder.Open(commands, RecordingOf(commands));
        table.cmdDraw(commands, 3, 1, 0, 0);
        if (encoded) encoder.Close();
        if ((index + 1) % pattern.batch == 0) encoder.Quiesce();
    }
    encoder.Quiesce();
    const double ns = NanosecondsSince(start) / draws;
    counts = encoder.Counts();
    return ns;
}

int HandoffBench() {
    const Pattern patterns[] = {
        {"few-draws-per-batch", 3, 1000, 3300},
        {"few-draws-heavy-encode", 3, 1000, 20000},
        {"many-draws-per-batch", 512, 4000, 3300},
    };
    constexpr std::uint32_t Draws = 40000;
    for (const auto& pattern : patterns) {
        for (const auto handoff : {CommandEncoder::Handoff::Never, CommandEncoder::Handoff::Always, CommandEncoder::Handoff::Adaptive}) {
            const char* name = handoff == CommandEncoder::Handoff::Never ? "inline" : handoff == CommandEncoder::Handoff::Always ? "always" : "adaptive";
            double best = 0;
            CommandEncoder::Statistics counts{};
            for (std::uint32_t round = 0; round < 3; ++round) {
                Reset();
                const double ns = RunPattern(pattern, handoff, Draws, counts);
                if (round == 0 || ns < best) best = ns;
            }
            std::cout << "bench " << pattern.name << " " << name << " ns_per_draw=" << best << " inlined_share=" << static_cast<double>(counts.inlined) / Draws << " parks_per_draw=" << static_cast<double>(counts.parks) / Draws << " wait_ns_per_draw=" << static_cast<double>(counts.waitNs) / Draws << "\n";
        }
    }
    return 0;
}

void HandoffPolicyFollowsItsMeasuredCost() {
    constexpr std::uint32_t Draws = 6000;
    CommandEncoder::Statistics counts{};
    Reset();
    RunPattern({"never", 2, 1000, 0}, CommandEncoder::Handoff::Never, Draws, counts);
    Require(counts.posts == 0 && counts.inlined == Draws, "a draw went through the encoder with the hand-off refused");
    Reset();
    RunPattern({"always", 2, 1000, 0}, CommandEncoder::Handoff::Always, Draws, counts);
    Require(counts.inlined == 0 && counts.posts == Draws, "an always-handoff draw was recorded inline");
    Reset();
    RunPattern({"short batches", 2, 1000, 0}, CommandEncoder::Handoff::Adaptive, Draws, counts);
    Require(counts.inlined * 10 >= static_cast<std::uint64_t>(Draws) * 7, "the adaptive hand-off kept " + std::to_string(Draws - counts.inlined) + " of " + std::to_string(Draws) + " draws on the encoder although each batch cost more to wait for than to record");
    Reset();
    RunPattern({"long batches", 64, 40000, 20000}, CommandEncoder::Handoff::Adaptive, Draws, counts);
    Require(counts.inlined * 10 <= Draws, "the adaptive hand-off recorded " + std::to_string(counts.inlined) + " of " + std::to_string(Draws) + " draws inline although the encoder overlapped them with the drawing thread's work");
}

int main(int argc, char** argv) {
    try {
        if (argc > 1 && std::string(argv[1]) == "--bench") return HandoffBench();
        if (argc > 1 && std::string(argv[1]) == "--wait-on-the-encoder-thread") {
            std::signal(SIGABRT, [](int) { std::_Exit(0); });
            WaitOnTheEncoderThread();
            std::cerr << "a wait on the encoder thread returned instead of stopping the process\n";
            return 1;
        }
#ifdef _WIN32
        _putenv_s("APS5_HANDOFF_SPIN_US", "150");
#else
        setenv("APS5_HANDOFF_SPIN_US", "150", 1);
#endif
        for (std::uint32_t seed = 1; seed <= 6; ++seed) RandomProgramRecordsWhatInlineRecords(seed, seed % 2 == 0);
        OtherThreadsRecordDirectlyWhileAWindowIsOpen();
        HookDuringUnencodedOpsWaits();
        EveryCommandOfTheTableIsInterposed();
        NestedWindowKeepsOrder();
        LargeRecordsSpanChunks();
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
        ProducerAheadOfTheChunkPoolWaits();
        EncoderParksBetweenSpacedPosts();
        ReplayFailureNeedsNoAllocation();
        WaitsAreAttributedToTheirCallSites();
        SitesAreCountedOnlyWhenProfiled();
        HandoffPolicyFollowsItsMeasuredCost();
        std::cout << "command encoder tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
