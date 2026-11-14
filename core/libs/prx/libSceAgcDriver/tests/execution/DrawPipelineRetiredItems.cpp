#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <cstdint>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>

namespace {

using AgcDriver::DriverDetail::DrawFinisher;
using AgcDriver::DriverDetail::DrawPipeline;
namespace GuestMemory = AgcDriver::GuestMemory;

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void RetiredItemLeavesTheEnqueuingThread() {
    auto& pipeline = DrawPipeline::Queue0();
    auto held = std::make_shared<int>(1);
    const std::weak_ptr<int> watched = held;
    pipeline.Enqueue([held = std::move(held)] {}, {});
    pipeline.Drain(DrawPipeline::DrainReason::Submission);
    pipeline.Enqueue([] {}, {});
    Require(watched.expired(), "a retired draw pipeline item stayed in the enqueuing thread's storage after Enqueue returned");
    pipeline.Drain(DrawPipeline::DrainReason::Submission);
}

void RanJobLeavesTheEnqueuingThread() {
    auto& finisher = DrawFinisher::Get();
    auto held = std::make_shared<int>(1);
    const std::weak_ptr<int> watched = held;
    finisher.Enqueue([held = std::move(held)] {});
    finisher.Drain();
    finisher.Enqueue([] {});
    Require(watched.expired(), "a finished draw finisher job stayed in the enqueuing thread's storage after Enqueue returned");
    finisher.Drain();
}

void QueuedWorkRunsInTheQueuingEpoch() {
    auto& pipeline = DrawPipeline::Queue0();
    auto& finisher = DrawFinisher::Get();
    const bool shared = DrawPipeline::SharedCollectEpoch();
    GuestMemory::BumpCollectEpoch();
    const auto queued = GuestMemory::HeldCollectEpoch();
    std::uint64_t committed = 0;
    pipeline.Enqueue([&committed] { committed = GuestMemory::HeldCollectEpoch(); }, {});
    pipeline.Drain(DrawPipeline::DrainReason::Submission);
    Require(shared ? committed == queued : committed != queued, shared ? "the committer did not run an item in the collect epoch it was queued in" : "the committer took the queuing thread's collect epoch with APS5_NO_SHARED_COLLECT_EPOCH");

    GuestMemory::BumpCollectEpoch();
    const auto handed = GuestMemory::HeldCollectEpoch();
    std::uint64_t finished = 0;
    std::uint64_t recommitted = 0;
    finisher.Enqueue([&] {
        finished = GuestMemory::HeldCollectEpoch();
        pipeline.Enqueue([&recommitted] { recommitted = GuestMemory::HeldCollectEpoch(); }, {});
    });
    finisher.Drain();
    pipeline.Drain(DrawPipeline::DrainReason::Submission);
    if (shared) Require(finished == handed && recommitted == handed, "a finisher job or the item it queued did not run in the collect epoch the job was handed over in");
    else Require(finished != handed && recommitted != handed, "the finisher or the committer took the queuing thread's collect epoch with APS5_NO_SHARED_COLLECT_EPOCH");

    std::promise<void> ready;
    std::promise<void> go;
    auto readied = ready.get_future();
    auto released = go.get_future();
    std::uint64_t older = 0;
    std::uint64_t kept = 0;
    std::thread late([&] {
        GuestMemory::BumpCollectEpoch();
        older = GuestMemory::HeldCollectEpoch();
        ready.set_value();
        released.wait();
        pipeline.Enqueue([&kept] { kept = GuestMemory::HeldCollectEpoch(); }, {});
        pipeline.Drain(DrawPipeline::DrainReason::Submission);
    });
    readied.wait();
    GuestMemory::BumpCollectEpoch();
    const auto newer = GuestMemory::HeldCollectEpoch();
    pipeline.Enqueue([] {}, {});
    pipeline.Drain(DrawPipeline::DrainReason::Submission);
    go.set_value();
    late.join();
    if (shared) Require(older < newer && kept == newer, "an item queued in an older collect epoch moved the committer's epoch back");
}

}

int main() {
    try {
        if (DrawPipeline::Depth() == 0) return 77;
        RetiredItemLeavesTheEnqueuingThread();
        RanJobLeavesTheEnqueuingThread();
        QueuedWorkRunsInTheQueuingEpoch();
        std::cout << "retired draw pipeline items tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
