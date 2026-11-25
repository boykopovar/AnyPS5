#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace {

using AgcDriver::DriverDetail::DrawPipeline;
using namespace std::chrono_literals;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::cerr << message << '\n';
    std::_Exit(1);
}

bool WaitFor(const std::function<bool()>& done, std::chrono::milliseconds limit) {
    const auto end = std::chrono::steady_clock::now() + limit;
    while (!done()) {
        if (std::chrono::steady_clock::now() >= end) return false;
        std::this_thread::sleep_for(1ms);
    }
    return true;
}

struct Gates {
    std::mutex mutex;
    std::condition_variable changed;
    std::size_t open = 0;

    void Pass(std::size_t index) {
        std::unique_lock lock(mutex);
        changed.wait(lock, [&] { return open > index; });
    }
    void OpenThrough(std::size_t count) {
        {
            std::lock_guard lock(mutex);
            open = count;
        }
        changed.notify_all();
    }
};

std::shared_ptr<Gates> FillWithGatedCommits(DrawPipeline& pipeline) {
    auto gates = std::make_shared<Gates>();
    for (std::size_t i = 0; i < DrawPipeline::Depth(); ++i) pipeline.Enqueue([gates, i] { gates->Pass(i); }, {});
    return gates;
}

void RetireThrough(DrawPipeline& pipeline, Gates& gates, std::uint64_t base, std::size_t count) {
    gates.OpenThrough(count);
    Require(WaitFor([&] { return pipeline.Retired() >= base + count; }, 10s), "a released commit did not retire");
}

void ProducerResumesAtTheLowWatermark() {
    auto& pipeline = DrawPipeline::Queue0();
    pipeline.Drain(DrawPipeline::DrainReason::Submission);
    const auto depth = DrawPipeline::Depth();
    const auto resume = DrawPipeline::ResumeDepth();
    const auto base = pipeline.Retired();
    const auto waits = pipeline.ProducerWaits();
    auto gates = FillWithGatedCommits(pipeline);
    std::atomic<bool> returned{false};
    std::thread producer([&] {
        pipeline.Enqueue([] {}, {});
        returned.store(true, std::memory_order_release);
    });
    Require(WaitFor([&] { return pipeline.ProducerWaits() > waits; }, 10s), "the producer did not wait on a full pipeline");
    if (depth - resume > 1) {
        RetireThrough(pipeline, *gates, base, 1);
        std::this_thread::sleep_for(200ms);
        Require(!returned.load(std::memory_order_acquire), "the producer resumed after one retire instead of at the low watermark");
    }
    for (std::size_t retired = 2; retired < depth - resume; ++retired) {
        RetireThrough(pipeline, *gates, base, retired);
        Require(!returned.load(std::memory_order_acquire), "the producer resumed above the low watermark");
    }
    RetireThrough(pipeline, *gates, base, depth - resume);
    Require(WaitFor([&] { return returned.load(std::memory_order_acquire); }, 10s), "the producer did not resume at the low watermark");
    producer.join();
    gates->OpenThrough(depth);
    pipeline.Drain(DrawPipeline::DrainReason::Submission);
    Require(pipeline.Retired() == base + depth + 1, "the pipeline did not retire every item");
}

void EveryProducerResumes() {
    auto& pipeline = DrawPipeline::Queue0();
    pipeline.Drain(DrawPipeline::DrainReason::Submission);
    const auto depth = DrawPipeline::Depth();
    const auto resume = DrawPipeline::ResumeDepth();
    const auto base = pipeline.Retired();
    const auto waits = pipeline.ProducerWaits();
    auto gates = FillWithGatedCommits(pipeline);
    std::atomic<unsigned> returned{0};
    std::vector<std::thread> producers;
    for (int i = 0; i < 2; ++i) producers.emplace_back([&] {
        pipeline.Enqueue([] {}, {});
        returned.fetch_add(1, std::memory_order_acq_rel);
    });
    Require(WaitFor([&] { return pipeline.ProducerWaits() >= waits + 2; }, 10s), "both producers did not wait on a full pipeline");
    RetireThrough(pipeline, *gates, base, depth - resume);
    Require(WaitFor([&] { return returned.load(std::memory_order_acquire) == 2; }, 10s), "a producer did not resume at the low watermark");
    for (auto& producer : producers) producer.join();
    gates->OpenThrough(depth);
    pipeline.Drain(DrawPipeline::DrainReason::Submission);
    Require(pipeline.Retired() == base + depth + 2, "the pipeline did not retire every item");
}

void DrainReturnsAtEmptyBehindAWaitingProducer() {
    auto& pipeline = DrawPipeline::Queue0();
    pipeline.Drain(DrawPipeline::DrainReason::Submission);
    const auto depth = DrawPipeline::Depth();
    const auto resume = DrawPipeline::ResumeDepth();
    const auto base = pipeline.Retired();
    const auto producerWaits = pipeline.ProducerWaits();
    const auto drainWaits = pipeline.DrainWaits();
    auto gates = FillWithGatedCommits(pipeline);
    std::atomic<bool> produced{false};
    std::atomic<bool> drained{false};
    std::thread producer([&] {
        pipeline.Enqueue([] {}, {});
        produced.store(true, std::memory_order_release);
    });
    Require(WaitFor([&] { return pipeline.ProducerWaits() > producerWaits; }, 10s), "the producer did not wait on a full pipeline");
    std::thread drainer([&] {
        pipeline.Drain(DrawPipeline::DrainReason::Submission);
        drained.store(true, std::memory_order_release);
    });
    Require(WaitFor([&] { return pipeline.DrainWaits() > drainWaits; }, 10s), "the drain did not wait on a busy pipeline");
    RetireThrough(pipeline, *gates, base, depth - resume);
    Require(WaitFor([&] { return produced.load(std::memory_order_acquire); }, 10s), "the producer did not resume at the low watermark");
    producer.join();
    Require(!drained.load(std::memory_order_acquire), "the drain returned while items were queued");
    RetireThrough(pipeline, *gates, base, depth - 1);
    Require(!drained.load(std::memory_order_acquire), "the drain returned while items were queued");
    gates->OpenThrough(depth);
    Require(WaitFor([&] { return drained.load(std::memory_order_acquire); }, 10s), "the drain did not return when the pipeline emptied");
    drainer.join();
    Require(pipeline.Retired() == base + depth + 1, "the pipeline did not retire every item");
}

}

int main() {
    if (DrawPipeline::Depth() == 0) return 77;
    ProducerResumesAtTheLowWatermark();
    if (DrawPipeline::Depth() >= DrawPipeline::ResumeDepth() + 2) EveryProducerResumes();
    DrainReturnsAtEmptyBehindAWaitingProducer();
    std::cout << "draw pipeline producer wake tests passed\n";
    return 0;
}
