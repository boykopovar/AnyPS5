#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <thread>

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
