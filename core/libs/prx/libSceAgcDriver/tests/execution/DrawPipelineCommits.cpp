#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <mutex>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using AgcDriver::DriverDetail::DrawPipeline;
using Range = DrawPipeline::Range;

void check(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("draw pipeline commits: " + what);
}

class Log {
public:
    void Add(const char* entry) {
        std::lock_guard lock(mutex);
        entries.emplace_back(entry);
    }
    std::vector<std::string> Take() {
        std::lock_guard lock(mutex);
        return std::exchange(entries, {});
    }

private:
    std::mutex mutex;
    std::vector<std::string> entries;
};

void waitFor(const std::atomic<bool>& flag) {
    while (!flag.load(std::memory_order_acquire)) std::this_thread::yield();
}

std::thread releaseLater(std::atomic<bool>& flag) {
    return std::thread([&flag] {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        flag.store(true, std::memory_order_release);
    });
}

void indirectArgumentsOverPendingDraw(DrawPipeline& pipeline, Log& log) {
    std::atomic<bool> release{false};
    pipeline.Enqueue([&] { waitFor(release); log.Add("draw"); }, {{0x100000, 0x200000}});
    AgcDriver::Pm4::DrawParameters::IndirectDraw elsewhere{};
    elsewhere.arguments = 0x300000;
    elsewhere.recordBytes = 20;
    elsewhere.stride = 20;
    elsewhere.count = 4;
    const std::array<Range, 1> apart{Range{elsewhere.arguments, elsewhere.arguments + elsewhere.RangeBytes()}};
    check(!pipeline.DrainIfOverlaps(std::span<const Range>(apart), DrawPipeline::DrainReason::Indirect), "arguments outside every pending write drained");
    check(pipeline.Busy(), "the pending draw finished before it was released");
    AgcDriver::Pm4::DrawParameters::IndirectDraw inside = elsewhere;
    inside.arguments = 0x1ffff0;
    const std::array<Range, 1> overlapping{Range{inside.arguments, inside.arguments + inside.RangeBytes()}};
    auto releaser = releaseLater(release);
    const bool drained = pipeline.DrainIfOverlaps(std::span<const Range>(overlapping), DrawPipeline::DrainReason::Indirect);
    releaser.join();
    check(drained, "arguments under a pending draw's write did not drain");
    check(!pipeline.Busy(), "the drain returned with the draw still pending");
    check(log.Take() == std::vector<std::string>{"draw"}, "the draw writing the arguments did not commit before the read");
}

void labelBetweenDispatchAndDraws(DrawPipeline& pipeline, Log& log) {
    std::atomic<bool> release{false};
    constexpr std::uint64_t label = 0x800000;
    const std::uint64_t value = 0x0000beef00001234ull;
    std::vector<std::byte> bytes(8);
    std::memcpy(bytes.data(), &value, sizeof(value));
    pipeline.Enqueue([&] { waitFor(release); log.Add("draw 1"); }, {{0x600000, 0x700000}});
    pipeline.Enqueue([&] { log.Add("dispatch"); }, {{0x700000, 0x701000}});
    pipeline.Enqueue([&] { log.Add("label"); }, {{label, label + 8}}, label, bytes);
    pipeline.Enqueue([&] { log.Add("draw 2"); }, {{0x600000, 0x700000}});
    const auto pending = pipeline.PendingLabel(label, 4);
    check(pending.has_value() && *pending == 0x1234u, "a same-queue wait did not see the pending label behind the dispatch");
    check(!pipeline.PendingLabel(0x700000, 4).has_value(), "a dispatch write was answered as a label");
    check(pipeline.Busy(), "the first draw finished before it was released");
    release.store(true, std::memory_order_release);
    pipeline.Drain(DrawPipeline::DrainReason::Packet);
    check(log.Take() == std::vector<std::string>{"draw 1", "dispatch", "label", "draw 2"}, "commits ran out of packet order");
}

void hookReadOverPendingDispatch(DrawPipeline& pipeline, Log& log) {
    std::atomic<bool> release{false};
    DrawPipeline::Active() = true;
    pipeline.Enqueue([&] { waitFor(release); log.Add("dispatch"); }, {{0x900000, 0x901000}});
    check(!DrawPipeline::DrainBeforeRead(0x902000, 16), "a read outside the dispatch's writes drained");
    check(pipeline.Busy(), "the dispatch finished before it was released");
    auto releaser = releaseLater(release);
    const bool drained = DrawPipeline::DrainBeforeRead(0x900ff0, 32);
    releaser.join();
    DrawPipeline::Active() = false;
    check(drained, "a read over the dispatch's writes did not drain");
    check(log.Take() == std::vector<std::string>{"dispatch"}, "the read ran before the dispatch writing it committed");
    check(!DrawPipeline::DrainBeforeRead(0x900ff0, 32), "a read drained on a thread that is not the queue-0 worker");
}

}

int main() {
    try {
        auto& pipeline = DrawPipeline::Queue0();
        Log log;
        indirectArgumentsOverPendingDraw(pipeline, log);
        labelBetweenDispatchAndDraws(pipeline, log);
        hookReadOverPendingDispatch(pipeline, log);
        std::puts("draw pipeline commit tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
