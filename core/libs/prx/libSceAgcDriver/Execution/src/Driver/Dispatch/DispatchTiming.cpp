#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Dispatch/DispatchTiming.hpp"

namespace AgcDriver::DriverDetail {

void Driver::addDriverPhases(DispatchClass which, const std::array<double, DriverPhaseCount>& ms, bool hit, bool validated) {
    std::lock_guard lock(driverPhasesMutex);
    auto& totals = driverPhaseTotals[which];
    for (std::size_t i = 0; i < DriverPhaseCount; ++i) totals.ms[i] += ms[i];
    ++totals.dispatches;
    if (hit) ++totals.hits;
    if (validated) ++totals.validations;
    const auto now = std::chrono::steady_clock::now();
    if (now - driverPhasesReport < std::chrono::seconds(10)) return;
    driverPhasesReport = now;
    for (std::size_t cls = 0; cls < DispatchClassCount; ++cls) {
        auto& line = driverPhaseTotals[cls];
        if (line.dispatches == 0) continue;
        std::string report;
        double total = 0;
        for (std::size_t i = 0; i < DriverPhaseCount; ++i) {
            char text[64];
            std::snprintf(text, sizeof(text), " %s %.1f", DriverPhaseNames[i], line.ms[i] * 1000 / static_cast<double>(line.dispatches));
            report += text;
            total += line.ms[i];
        }
        const double perValidation = line.validations != 0 ? 1000 / static_cast<double>(line.validations) : 0.0;
        AgcDriver::ProfilePrint_nid_no_patch("[dispatch] driver phases %s (10 s, %llu dispatches, %llu cache hits), us per dispatch:%s, total %.1f (%.1f ms); validate %.1f us per validation (%llu validations; GPU waits inside it %.1f us per validation apart)\n", DispatchClassNames[cls], static_cast<unsigned long long>(line.dispatches), static_cast<unsigned long long>(line.hits), report.c_str(), total * 1000 / static_cast<double>(line.dispatches), total, line.ms[PhaseValidate] * perValidation, static_cast<unsigned long long>(line.validations), line.ms[PhaseValidateWait] * perValidation);
        line = {};
    }
}

PendingDispatchPhases& Driver::pendingDispatchPhases() {
    static thread_local PendingDispatchPhases pending;
    return pending;
}

std::chrono::steady_clock::time_point& Driver::packetStartedAt() {
    static thread_local std::chrono::steady_clock::time_point started{};
    return started;
}

double DispatchPhaseTiming::Elapsed() {
    const auto now = std::chrono::steady_clock::now();
    const auto ms = std::chrono::duration<double, std::milli>(now - lap).count();
    lap = now;
    return ms;
}

void DispatchPhaseTiming::Phase(DriverPhase which) {
    if (!profile) return;
    const auto now = std::chrono::steady_clock::now();
    phaseMs[which] += std::chrono::duration<double, std::milli>(now - phaseLap).count();
    phaseLap = now;
}

}
