#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>

namespace {

using AgcDriver::DriverDetail::DrawFinisher;
using AgcDriver::DriverDetail::DrawPipeline;

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

}

int main() {
    try {
        if (DrawPipeline::Depth() == 0) return 77;
        RetiredItemLeavesTheEnqueuingThread();
        RanJobLeavesTheEnqueuingThread();
        std::cout << "retired draw pipeline items tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
