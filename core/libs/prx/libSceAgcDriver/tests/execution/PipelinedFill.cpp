#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Memory/BufferFill.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

using AgcDriver::Graphics::Require;
using namespace AgcDriver::DriverDetail;
alignas(256) constexpr auto Code = PatternFillKernel;
alignas(256) std::array<std::uint32_t, 4> Pattern{};
alignas(256) std::array<std::uint32_t, 4> Control{};
alignas(256) std::array<std::uint32_t, 260> Output{};
alignas(256) std::array<std::uint32_t, 64> Labels{};
alignas(256) std::array<std::uint32_t, 4> ScratchPattern{0x0f0f0f0fu, 0, 0, 0};
alignas(256) std::array<std::uint32_t, 4> ScratchControl{256, 0, 0, 0};
alignas(256) std::array<std::uint32_t, 260> Scratch{};

std::array<std::uint32_t, 12> UserData(const void* pattern, const void* control, void* output) {
    std::array<std::uint32_t, 12> data{};
    const auto descriptor = [&](std::size_t first, const void* pointer, std::uint32_t stride, std::uint32_t count, std::uint32_t format) {
        const auto address = reinterpret_cast<std::uintptr_t>(pointer);
        data[first] = static_cast<std::uint32_t>(address);
        data[first + 1] = static_cast<std::uint32_t>(address >> 32u) | (stride << 16u);
        data[first + 2] = count;
        data[first + 3] = format;
    };
    descriptor(0, pattern, 4, 4, 0x14004u);
    descriptor(4, output, 4, 256, 0x14004u);
    descriptor(8, control, 16, 1, 0x4dfacu);
    return data;
}

void AppendFill(std::vector<std::uint32_t>& words, const void* pattern, const void* control, void* output) {
    const auto registers = [&](std::uint32_t first, std::span<const std::uint32_t> values) {
        words.push_back(0xc0007600u | (static_cast<std::uint32_t>(values.size()) << 16u));
        words.push_back(first);
        words.insert(words.end(), values.begin(), values.end());
    };
    const std::array<std::uint32_t, 3> shape{64, 1, 1};
    registers(0x207, shape);
    const auto address = reinterpret_cast<std::uintptr_t>(Code.data());
    const std::array<std::uint32_t, 2> program{static_cast<std::uint32_t>(address >> 8u), static_cast<std::uint32_t>(address >> 40u)};
    registers(0x20c, program);
    const std::array<std::uint32_t, 1> resource{(12u << 1u) | (1u << 7u)};
    registers(0x213, resource);
    registers(0x240, UserData(pattern, control, output));
    words.insert(words.end(), {0xc0031500u, 4, 1, 1, 0x41u | 0x8000u});
}

void AppendScratchFill(std::vector<std::uint32_t>& words) {
    AppendFill(words, ScratchPattern.data(), ScratchControl.data(), Scratch.data());
}

void AppendOutputFill(std::vector<std::uint32_t>& words) {
    AppendFill(words, Pattern.data(), Control.data(), Output.data());
}

void AppendLabel(std::vector<std::uint32_t>& words, std::uint32_t* target, std::uint32_t value) {
    const auto address = reinterpret_cast<std::uintptr_t>(target);
    words.insert(words.end(), {0xc0064900u, 0x514u, (1u << 29u) | (2u << 24u), static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), value, 0, 0});
}

struct Blocker {
    std::atomic<bool> release{false};
    std::uint32_t* target = nullptr;
    ~Blocker() { release = true; }
    void Enqueue(std::vector<DrawPipeline::Range> writes = {}) {
        DrawPipeline::Queue0().Enqueue([this] {
            while (!release.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            if (target != nullptr) std::atomic_ref<std::uint32_t>(*target).store(1);
            std::lock_guard gpuLock(AgcDriver::GuestMemory::GpuMutex());
        }, std::move(writes));
    }
};

void SubmitBehind(Blocker& blocker, std::vector<std::uint32_t>& words) {
    std::unique_lock gpuLock(AgcDriver::GuestMemory::GpuMutex());
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    blocker.Enqueue();
}

void ReleaseWhen(Blocker& blocker, std::size_t reached, std::size_t passed) {
    const auto start = std::chrono::steady_clock::now();
    auto reachedAt = std::chrono::steady_clock::time_point{};
    for (;;) {
        const auto outstanding = DrawPipeline::Queue0().Outstanding();
        const auto now = std::chrono::steady_clock::now();
        if (outstanding >= passed) break;
        if (outstanding >= reached && reachedAt == std::chrono::steady_clock::time_point{}) reachedAt = now;
        if (reachedAt != std::chrono::steady_clock::time_point{} && now - reachedAt > std::chrono::seconds(2)) break;
        Require(now - start < std::chrono::seconds(20), "the queue worker never reached the fill");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    blocker.release = true;
}

void Finish() {
    auto idle = std::async(std::launch::async, [] { AgcDriverWaitIdle_nid_postfix(); });
    if (idle.wait_for(std::chrono::seconds(20)) != std::future_status::ready) {
        std::fputs("pipelined fill: the queue worker and the draw pipeline deadlocked\n", stderr);
        std::fflush(stderr);
        std::_Exit(1);
    }
    idle.get();
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Output.data()), sizeof(Output));
    AgcDriver::GuestMemory::FlushGpuWrites(reinterpret_cast<std::uintptr_t>(Scratch.data()), sizeof(Scratch));
}

void InputsAfterQueuedLabels() {
    Output.fill(0xcafebabeu);
    Pattern = {0x40404040u, 0, 0, 0};
    Control = {256, 0, 0, 0};
    std::vector<std::uint32_t> words;
    AppendScratchFill(words);
    AppendLabel(words, &Control[0], 128);
    AppendLabel(words, &Pattern[0], 0xababababu);
    AppendOutputFill(words);
    Blocker blocker;
    SubmitBehind(blocker, words);
    ReleaseWhen(blocker, 4, 5);
    Finish();
    for (std::size_t i = 0; i < Output.size(); ++i) {
        const auto expected = i < 128 ? 0xababababu : 0xcafebabeu;
        Require(Output[i] == expected, "a fill read its control or pattern before the queued labels that write them, at " + std::to_string(i));
    }
}

void FillIntoFullPipeline() {
    Output.fill(0xcafebabeu);
    Pattern = {0x5a5a5a5au, 0, 0, 0};
    Control = {256, 0, 0, 0};
    Labels.fill(0);
    const auto depth = DrawPipeline::Depth();
    Require(depth >= 3 && depth - 2 <= Labels.size(), "unexpected draw pipeline depth " + std::to_string(depth));
    std::vector<std::uint32_t> words;
    AppendScratchFill(words);
    for (std::size_t i = 0; i + 2 < depth; ++i) AppendLabel(words, &Labels[i], static_cast<std::uint32_t>(i + 1));
    AppendOutputFill(words);
    Blocker blocker;
    SubmitBehind(blocker, words);
    ReleaseWhen(blocker, depth, depth + 1);
    Finish();
    for (std::size_t i = 0; i + 2 < depth; ++i) Require(Labels[i] == i + 1, "a queued label was not stored");
    for (std::size_t i = 0; i < Output.size(); ++i) Require(Output[i] == (i < 256 ? 0x5a5a5a5au : 0xcafebabeu), "a fill behind a full draw pipeline did not land at " + std::to_string(i));
}

void SatisfiedWaitKeepsUnrelatedQueuedWork() {
    alignas(8) static std::uint32_t gate = 1;
    alignas(8) static std::uint32_t marker = 0;
    marker = 0;

    Blocker blocker;
    blocker.Enqueue();

    const auto address = reinterpret_cast<std::uintptr_t>(&gate);
    std::vector<std::uint32_t> words{
        0xc0053c00u, 0x13u, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u),
        1u, 0xffffffffu, 0x19u};
    AppendLabel(words, &marker, 1);
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    AgcDriver::Submit(&packet, 0);

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (DrawPipeline::Queue0().Outstanding() < 2 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const bool keptQueued = DrawPipeline::Queue0().Outstanding() >= 2;
    blocker.release = true;
    Finish();
    Require(keptQueued, "a satisfied WAIT_REG_MEM drained unrelated queued work");
    Require(marker == 1, "a label after a satisfied WAIT_REG_MEM did not execute");
}

void SatisfiedWaitDrainsOverlappingQueuedWork() {
    alignas(8) static std::uint32_t gate = 1;
    alignas(8) static std::uint32_t marker = 0;
    std::atomic_ref<std::uint32_t>(gate).store(1);
    marker = 0;

    Blocker blocker;
    blocker.target = &gate;
    const auto address = reinterpret_cast<std::uintptr_t>(&gate);
    blocker.Enqueue({{address, address + sizeof(gate)}});

    std::vector<std::uint32_t> words{
        0xc0053c00u, 0x13u, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u),
        1u, 0xffffffffu, 0x19u};
    AppendLabel(words, &marker, 1);
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    const auto drainsBefore = DrawPipeline::DrainRequestsByOpcode(0x3c);
    auto submitted = std::async(std::launch::async, [&] { AgcDriver::Submit(&packet, 0); });

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (DrawPipeline::DrainRequestsByOpcode(0x3c) == drainsBefore &&
           DrawPipeline::Queue0().Outstanding() < 2 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const bool drained = DrawPipeline::DrainRequestsByOpcode(0x3c) > drainsBefore;
    const bool labelQueuedEarly = DrawPipeline::Queue0().Outstanding() >= 2;
    blocker.release = true;
    Require(submitted.wait_for(std::chrono::seconds(20)) == std::future_status::ready,
            "WAIT_REG_MEM submission did not finish after the overlapping write");
    submitted.get();
    Finish();
    Require(drained && !labelQueuedEarly, "a satisfied WAIT_REG_MEM skipped an overlapping queued write");
    Require(marker == 1, "a label after an overlapping WAIT_REG_MEM did not execute");
}

}

int main() {
#ifdef _WIN32
    _putenv_s("APS5_PIPELINED_DRAWS", "1");
#else
    setenv("APS5_PIPELINED_DRAWS", "1", 1);
#endif
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        InputsAfterQueuedLabels();
        FillIntoFullPipeline();
        SatisfiedWaitKeepsUnrelatedQueuedWork();
        SatisfiedWaitDrainsOverlappingQueuedWork();
        AgcDriverShutdown_nid_postfix();
        std::puts("pipelined fill tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
