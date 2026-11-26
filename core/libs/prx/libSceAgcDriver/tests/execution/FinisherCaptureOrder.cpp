#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "VulkanTestDevice.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {

using AgcDriver::Graphics::Require;
using AgcDriver::ShaderMemory;
using AgcDriver::DriverDetail::DrawFinisher;
using AgcDriver::DriverDetail::DrawPipeline;
using AgcDriver::DriverDetail::Driver;

alignas(256) std::array<std::uint32_t, 128> Memory{};
std::atomic<bool> Started{false};
std::atomic<bool> Release{false};
std::atomic<bool> Answered{false};
std::atomic<bool> OnFinisher{false};
std::atomic<int> Overlapping{-1};
std::atomic<int> Disjoint{-1};

template <class Condition>
bool WaitUntil(Condition condition) {
    const auto start = std::chrono::steady_clock::now();
    while (!condition()) {
        if (std::chrono::steady_clock::now() - start > std::chrono::seconds(10)) return false;
        std::this_thread::yield();
    }
    return true;
}

std::string Name(int policy) {
    switch (static_cast<ShaderMemory::PendingWrite>(policy)) {
    case ShaderMemory::PendingWrite::None: return "None";
    case ShaderMemory::PendingWrite::Sync: return "Sync";
    default: return "policy " + std::to_string(policy);
    }
}

}

int main() {
    try {
        {
            const auto device = OpenVulkanTestDevice();
            if (!device) return VulkanTestSkipped;
        }
        if (DrawPipeline::Depth() == 0 || !DrawFinisher::Enabled()) return VulkanTestSkipped;
        const auto address = reinterpret_cast<std::uintptr_t>(Memory.data());
        DrawPipeline::Queue0().Enqueue([] {
            Started.store(true);
            WaitUntil([] { return Release.load(); });
        }, {{address, address + 64}});
        const bool started = WaitUntil([] { return Started.load(); });
        if (started) {
            DrawFinisher::Get().Enqueue([address] {
                OnFinisher.store(DrawFinisher::OnFinisherThread());
                Overlapping.store(static_cast<int>(Driver::QueryPendingSpan(address + 16, 16, {})));
                Disjoint.store(static_cast<int>(Driver::QueryPendingSpan(address + 256, 16, {})));
                Answered.store(true);
            });
        }
        const bool answered = started && WaitUntil([] { return Answered.load(); });
        Release.store(true);
        DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Submission);
        DrawFinisher::Get().Drain();
        Require(started, "the queued item did not start");
        Require(answered, "the finisher did not answer the span queries");
        Require(OnFinisher.load(), "the span queries did not run on the draw finisher");
        Require(Overlapping.load() == static_cast<int>(ShaderMemory::PendingWrite::Sync), "a span over a write still queued in the pipeline answered " + Name(Overlapping.load()) + " on the draw finisher, expected Sync");
        Require(Disjoint.load() == static_cast<int>(ShaderMemory::PendingWrite::None), "a span clear of the queued write answered " + Name(Disjoint.load()) + " on the draw finisher, expected None");
        AgcDriverShutdown_nid_postfix();
        std::cout << "a draw finisher span query over a write queued in the pipeline answers Sync\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        Release.store(true);
        try { AgcDriverShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
