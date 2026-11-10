#include "prx/libSceAgcDriver/Graphics/include/CommandEncoder.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
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
        switch (random() % 9) {
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

void Reset() {
    std::lock_guard lock(logMutex);
    recorded.clear();
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
                encoder.Open(open);
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
    encoder.Open(mine);
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
    encoder.Open(commands);
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
    encoder.Open(outer);
    table.cmdDraw(outer, 1, 1, 0, 0);
    encoder.Open(outer);
    table.cmdDraw(outer, 2, 1, 0, 0);
    encoder.Close();
    table.cmdDraw(outer, 3, 1, 0, 0);
    encoder.Open(inner);
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
    encoder.Open(commands);
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
        encoder.Open(commands);
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
        encoder.Open(commands);
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
    encoder.Open(commands);
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
    encoder.Open(commands);
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
    encoder.Open(commands);
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

void FailedReplayStaysWithItsCommandBuffer() {
    Reset();
    auto table = FakeTable();
    const auto device = Handle<VkDevice>(0x4a);
    const auto failing = Handle<VkCommandBuffer>(0xe00);
    const auto healthy = Handle<VkCommandBuffer>(0xe80);
    std::size_t foreignReports = 0;
    std::size_t ownerReports = 0;
    {
        const auto attached = Attached(device, table);
        auto& encoder = *attached;
        std::atomic<int> failures{1};
        CommandEncoder::SetDelayForTesting([&] {
            if (failures.fetch_sub(1) > 0) throw std::runtime_error("injected replay failure");
        });
        encoder.Open(failing);
        table.cmdDraw(failing, 1, 1, 0, 0);
        encoder.Close();
        encoder.Drain();
        std::thread other([&] {
            if (Reports([&] { encoder.Settle(); })) ++foreignReports;
            if (Reports([&] { AgcDriver::Graphics::BeforeCommandLookup(device); })) ++foreignReports;
        });
        other.join();
        for (int attempt = 0; attempt < 2; ++attempt) {
            if (Reports([&] { encoder.Quiesce(); })) ++ownerReports;
        }
        if (Reports([&] {
                encoder.Open(healthy);
                table.cmdDraw(healthy, 2, 1, 0, 0);
                encoder.Close();
                encoder.Quiesce();
            }))
            ++ownerReports;
        if (Reports([&] {
                encoder.Open(failing);
                table.cmdDraw(failing, 3, 1, 0, 0);
                encoder.Close();
                encoder.Quiesce();
            }))
            ++ownerReports;
        CommandEncoder::SetDelayForTesting({});
    }
    Require(foreignReports == 0, "a failed replay was reported to " + std::to_string(foreignReports) + " settles on a thread that did not record the failed commands");
    Require(ownerReports == 1, "a failed replay was reported " + std::to_string(ownerReports) + " times to the thread that recorded it, not once");
    Require(Recorded(failing) == 0, "the encoder recorded " + std::to_string(Recorded(failing)) + " commands into a command buffer after a replay into it failed");
    Compare({{healthy, {"draw 2 1 0 0"}}}, "another command buffer after a failed replay");
}

bool Refuses(const std::function<void()>& finish) {
    try {
        finish();
        return false;
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) != "injected replay failure";
    }
}

void FinishRefusesAFailedCommandBuffer() {
    Reset();
    auto table = FakeTable();
    const auto device = Handle<VkDevice>(0x4b);
    const auto failing = Handle<VkCommandBuffer>(0xf00);
    const auto healthy = Handle<VkCommandBuffer>(0xf80);
    const auto later = Handle<VkCommandBuffer>(0xfc0);
    const auto attached = Attached(device, table);
    auto& encoder = *attached;
    std::atomic<int> failures{1};
    CommandEncoder::SetDelayForTesting([&] {
        if (failures.fetch_sub(1) > 0) throw std::runtime_error("injected replay failure");
    });
    encoder.Open(failing);
    table.cmdDraw(failing, 1, 1, 0, 0);
    encoder.Close();
    encoder.Drain();
    bool foreignRefused = false;
    std::thread other([&] { foreignRefused = Refuses([&] { encoder.Finish(failing); }); });
    other.join();
    Require(foreignRefused, "another thread finishing a command buffer whose replay failed was not refused, or took the producer's report");
    Require(Reports([&] { encoder.Quiesce(); }), "a failed replay was not reported to the thread that recorded it after another thread finished the buffer");
    Require(!Reports([&] { encoder.Finish(healthy); }), "finishing a healthy command buffer reported another buffer's failed replay");
    Require(Refuses([&] { encoder.Finish(failing); }), "finishing a command buffer whose reported replay failure was not refused, or reported it again");
    encoder.Forget(failing);
    encoder.Open(failing);
    table.cmdDraw(failing, 2, 1, 0, 0);
    encoder.Close();
    Require(!Reports([&] { encoder.Finish(failing); }), "a reused command buffer handle inherited the failure of the freed buffer");
    failures.store(1);
    encoder.Open(later);
    table.cmdDraw(later, 3, 1, 0, 0);
    encoder.Close();
    Require(Reports([&] { encoder.Finish(later); }), "the producer finishing a command buffer whose replay failed did not get the failure");
    Require(!Reports([&] { encoder.Quiesce(); }), "a failed replay delivered by finishing its command buffer was reported again");
    encoder.Forget(later);
    CommandEncoder::SetDelayForTesting({});
    Compare({{failing, {"draw 2 1 0 0"}}, {later, {}}}, "a reused command buffer handle");
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

int main() {
    try {
        for (std::uint32_t seed = 1; seed <= 6; ++seed) RandomProgramRecordsWhatInlineRecords(seed, seed % 2 == 0);
        OtherThreadsRecordDirectlyWhileAWindowIsOpen();
        HookDuringUnencodedOpsWaits();
        NestedWindowKeepsOrder();
        LargeRecordsSpanChunks();
        UnwindingSettlesQueuedRecording();
        OwnersOutliveTheUnwindDrain();
        ForeignThreadCannotProduceIntoAnOpenWindow();
        ForeignLookupWhileAWindowIsOpenProceeds();
        RawCallAfterLookupKeepsOrder();
        FailedReplayStaysWithItsCommandBuffer();
        FinishRefusesAFailedCommandBuffer();
        DevicesPastTheSlotsRecordInline();
        std::cout << "command encoder tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
