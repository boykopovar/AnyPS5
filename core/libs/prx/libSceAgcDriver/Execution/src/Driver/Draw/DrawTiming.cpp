#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawTiming.hpp"
#include <atomic>
#include <cstdio>
#include <string>

namespace AgcDriver::DriverDetail {

namespace {

std::atomic<std::uint64_t> countedPackets{0}, countedDrawn{0}, countedHanded{0}, countedHandedDrawn{0};

std::string phaseText(const std::array<double, DrawDriverPhaseCount>& ms, std::uint64_t count, double& total) {
    std::string report;
    total = 0;
    for (std::size_t i = 0; i < DrawDriverPhaseCount; ++i) {
        char text[64];
        std::snprintf(text, sizeof(text), " %s %.1f", DrawDriverPhaseNames[i], ms[i] * 1000 / static_cast<double>(count));
        report += text;
        total += ms[i];
    }
    return report;
}

}

DrawPhaseCounts CountedDrawPhases() {
    return {countedPackets.load(std::memory_order_relaxed), countedDrawn.load(std::memory_order_relaxed), countedHanded.load(std::memory_order_relaxed), countedHandedDrawn.load(std::memory_order_relaxed)};
}

PendingDrawPhases& Driver::pendingDrawPhases() {
    static thread_local PendingDrawPhases pending;
    return pending;
}

void Driver::addDrawPhases(const std::array<double, DrawDriverPhaseCount>& ms, bool drawn, std::uint64_t captures) {
    countedPackets.fetch_add(1, std::memory_order_relaxed);
    if (drawn) countedDrawn.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard lock(drawPhasesMutex);
    auto& totals = drawPhaseTotals;
    for (std::size_t i = 0; i < DrawDriverPhaseCount; ++i) totals.ms[i] += ms[i];
    ++totals.packets;
    if (drawn) ++totals.drawn;
    totals.captures += captures;
    const auto now = std::chrono::steady_clock::now();
    if (now - totals.lastReport < std::chrono::seconds(10)) return;
    totals.lastReport = now;
    double total = 0;
    const auto report = phaseText(totals.ms, totals.packets, total);
    std::string finisher;
    if (totals.handed != 0) {
        double handedTotal = 0;
        const auto handedReport = phaseText(totals.finisherMs, totals.handed, handedTotal);
        char text[512];
        std::snprintf(text, sizeof(text), "; draw finisher (%llu handed draws, %llu drawn, %llu captures), us per handed draw: queue wait %.1f",static_cast<unsigned long long>(totals.handed), static_cast<unsigned long long>(totals.handedDrawn), static_cast<unsigned long long>(totals.handedCaptures), totals.queueMs * 1000 / static_cast<double>(totals.handed));
        finisher = text;
        finisher += handedReport;
        std::snprintf(text, sizeof(text), ", total %.1f (%.1f ms; queue wait %.1f ms, Graphics::Draw %.1f ms, skipped %.1f ms)", handedTotal * 1000 / static_cast<double>(totals.handed), handedTotal, totals.queueMs, totals.finisherMs[DrawRowGraphics], totals.finisherMs[DrawRowSkipped]);
        finisher += text;
    }
    AgcDriver::ProfilePrint_nid_no_patch( "[draw] driver phases (10 s, %llu draw packets, %llu drawn, %llu captures), us per packet:%s, total %.1f (%.1f ms; Graphics::Draw %.1f ms, skipped %.1f ms)%s\n", static_cast<unsigned long long>(totals.packets), static_cast<unsigned long long>(totals.drawn), static_cast<unsigned long long>(totals.captures), report.c_str(), total * 1000 / static_cast<double>(totals.packets), total, totals.ms[DrawRowGraphics], totals.ms[DrawRowSkipped], finisher.c_str());
    totals.ms = {};
    totals.packets = totals.drawn = totals.captures = 0;
    totals.finisherMs = {};
    totals.handed = totals.handedDrawn = totals.handedCaptures = 0;
    totals.queueMs = 0;
}

void Driver::addFinisherDrawPhases(const std::array<double, DrawDriverPhaseCount>& ms, bool drawn, std::uint64_t captures, double queueMs) {
    countedHanded.fetch_add(1, std::memory_order_relaxed);
    if (drawn) countedHandedDrawn.fetch_add(1, std::memory_order_relaxed);
    std::lock_guard lock(drawPhasesMutex);
    auto& totals = drawPhaseTotals;
    for (std::size_t i = 0; i < DrawDriverPhaseCount; ++i) totals.finisherMs[i] += ms[i];
    ++totals.handed;
    if (drawn) ++totals.handedDrawn;
    totals.handedCaptures += captures;
    totals.queueMs += queueMs;
}

void DrawPhaseTiming::Phase(DrawDriverPhase which) {
    if (!profile) return;
    const auto now = std::chrono::steady_clock::now();
    phaseMs[which] += std::chrono::duration<double, std::milli>(now - phaseLap).count();
    phaseLap = now;
}

}
