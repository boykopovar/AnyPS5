#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include <atomic>
#include <chrono>
#include <iostream>
#include <semaphore>
#include <stdexcept>
#include <thread>

namespace {

void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void TestStopReleasesFullProducer() {
    using AgcDriver::DriverDetail::DrawPipeline;
    auto& pipeline = DrawPipeline::Queue0();
    const auto depth = DrawPipeline::Depth();
    Check(depth >= 2, "test requires at least two pipeline slots");
    std::binary_semaphore release{0};
    std::atomic<bool> started{false};
    pipeline.Enqueue([&] {
        started.store(true, std::memory_order_release);
        release.acquire();
    }, {}, 0, {}, {}, true);
    while (!started.load(std::memory_order_acquire)) std::this_thread::yield();
    for (std::size_t index = 1; index < depth; ++index) pipeline.Enqueue([] {}, {}, 0, {}, {}, true);
    std::atomic<bool> returned{false};
    std::thread::id ranOn;
    std::thread producer([&] {
        pipeline.Enqueue([&] { ranOn = std::this_thread::get_id(); }, {}, 0, {}, {}, true);
        returned.store(true, std::memory_order_release);
    });
    const auto producerId = producer.get_id();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const bool waitedForRoom = !returned.load(std::memory_order_acquire);
    std::thread stopper([&] { pipeline.Stop(); });
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!returned.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < deadline) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    const bool releasedByStop = returned.load(std::memory_order_acquire);
    release.release();
    producer.join();
    stopper.join();
    Check(waitedForRoom, "the producer did not wait for room in a full queue");
    Check(releasedByStop, "stopping the pipeline left a producer waiting for room until the committer drained");
    Check(ranOn == producerId, "a draw enqueued while the pipeline stopped did not run on its producer");
}

}

int main() {
    try {
        TestStopReleasesFullProducer();
        std::cout << "draw pipeline stop tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "draw pipeline stop tests failed: " << error.what() << '\n';
        return 1;
    }
}
