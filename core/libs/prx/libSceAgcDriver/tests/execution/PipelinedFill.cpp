#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Memory/BufferFill.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <span>
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
    void Enqueue() {
        DrawPipeline::Queue0().Enqueue([this] {
            while (!release.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
            std::lock_guard gpuLock(AgcDriver::GuestMemory::GpuMutex());
        }, {});
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

void QueuedWritesAnswerRunLists() {
    std::atomic<bool> release{false};
    const auto base = reinterpret_cast<std::uint64_t>(Scratch.data());
    DrawPipeline::Queue0().Enqueue([&release] {
        while (!release.load()) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }, {{base + 256, base + 512}});
    const std::array<std::pair<std::uint64_t, std::uint64_t>, 3> runs{{{base, base + 256}, {base + 512, base + 768}, {base + 508, base + 512}}};
    const bool lastRun = DrawPipeline::Queue0().OverlapsAny(runs);
    const bool outside = DrawPipeline::Queue0().OverlapsAny(std::span(runs).first(2));
    const bool single = DrawPipeline::Queue0().Overlaps(base + 508, 4);
    release = true;
    DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Flush);
    Require(lastRun && single && !outside, "a list of runs does not overlap a queued commit's writes exactly when one of its runs does");
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
        QueuedWritesAnswerRunLists();
        InputsAfterQueuedLabels();
        FillIntoFullPipeline();
        AgcDriverShutdown_nid_postfix();
        std::puts("pipelined fill tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        AgcDriverShutdown_nid_postfix();
        return 1;
    }
}
