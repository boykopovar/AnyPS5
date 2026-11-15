#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <utility>

int main(int argc, char** argv) {
    try {
        using namespace AgcDriver;
        using DriverDetail::DrawPipeline;
        using DriverDetail::DrawFinisher;
        if (DrawPipeline::Depth() == 0) return 77;
        const auto mode = argc > 1 ? std::string_view(argv[1]) : std::string_view{};
        const bool disabled = mode == "disabled" || mode == "finisher-disabled";
        const bool finisher = mode.starts_with("finisher");
        const char* worker = finisher ? "draw finisher" : "draw committer";
        if (finisher && !DrawFinisher::Enabled()) throw std::runtime_error("draw finisher is disabled");
        if (mode == "finisher-overlap") {
            constexpr std::uint64_t Begin = 0x10000;
            std::mutex gate;
            std::condition_variable opened;
            bool open = false;
            DrawFinisher::Pending held;
            held.unknown = false;
            held.writes = {{Begin, Begin + 4096}};
            std::thread([&] {
                DrawFinisher::Get().Enqueue([&] {
                    std::unique_lock lock(gate);
                    opened.wait(lock, [&] { return open; });
                }, held);
            }).join();
            const std::array inside{std::pair<std::uint64_t, std::uint64_t>{Begin, Begin + 4096}};
            const std::array outside{std::pair<std::uint64_t, std::uint64_t>{Begin + 4096, Begin + 8192}};
            const bool seen = DrawPipeline::Queue0().OverlapsAny(inside);
            const bool unrelated = DrawPipeline::Queue0().OverlapsAny(outside);
            {
                std::lock_guard lock(gate);
                open = true;
            }
            opened.notify_all();
            DrawFinisher::Get().Drain();
            if (!seen) throw std::runtime_error("a thread that hands no work to the draw finisher does not see a write the finisher still holds");
            if (unrelated) throw std::runtime_error("the draw finisher's held write overlaps a range it does not write");
            std::cout << "draw finisher overlap checked\n";
            return 0;
        }
        if (PerformanceTrace::Configured() == disabled) throw std::runtime_error("unexpected trace configuration");
        if (!disabled) PerformanceTrace::Present(1, 1, nullptr);
        bool checked = false;
        const char* failure = nullptr;
        const auto checkWorker = [&] {
            auto* trace = PerformanceTrace::Thread();
            if (disabled) {
                if (trace != nullptr) { failure = "unconfigured draw worker allocated a trace buffer"; return; }
                checked = true;
                return;
            }
            if (trace == nullptr) { failure = "draw worker CPU spans are absent from the trace"; return; }
            {
                PerformanceTimer timer("DrawPipeline.TraceRegression");
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
            const auto snapshot = trace->Collect();
            if (snapshot.name != worker || snapshot.dropped) { failure = "draw worker trace identity or capacity is incorrect"; return; }
            const auto found = [](const auto& event) { return event.scope != nullptr && std::string_view(event.scope) == "DrawPipeline.TraceRegression" && event.stage == nullptr && event.length > std::chrono::steady_clock::duration::zero(); };
            if (std::none_of(snapshot.slices.begin(), snapshot.slices.end(), found) || std::none_of(snapshot.totals.begin(), snapshot.totals.end(), found))
                { failure = "draw work was missing from trace slices or totals"; return; }
            checked = true;
        };
        if (finisher) {
            DrawFinisher::Get().Enqueue(checkWorker);
            DrawFinisher::Get().Drain();
        } else {
            DrawPipeline::Queue0().Enqueue(checkWorker, {});
            DrawPipeline::Queue0().Drain(DrawPipeline::DrainReason::Submission);
        }
        if (failure != nullptr) throw std::runtime_error(failure);
        if (!checked) throw std::runtime_error("draw worker did not run the trace check");
        std::cout << worker << " tracing checked\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
