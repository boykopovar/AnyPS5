#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/SynchronizationStatistics.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"

namespace AgcDriver::DriverDetail {

std::atomic<std::uint64_t> gpuLabels{0}, completionLabels{0}, noOpLabels{0}, unlockedDrains{0};

std::atomic<std::uint64_t> labelFallbacks[5] = {};

std::atomic<std::uint64_t> queuedLabels{0}, labelGroups{0}, immediateLabels{0};

std::atomic<std::uint64_t> packetLockRecords{0}, packetLockSubmits{0}, packetLockDeferred{0}, packetSubmitDeferred{0}, captureTryRecords{0}, captureRetries{0}, suspendPoints{0};

std::atomic<std::uint64_t> recordTriesSkipped{0};

std::atomic<bool> queue0Dormant{false};

std::atomic<std::uint64_t> packetLockRecordUs{0}, packetLockSubmitUs{0};

std::atomic<std::uint64_t> triesFailed[TrySites] = {};

std::atomic<std::uint64_t> storesOnGpu{0}, storesBehindCompletions{0}, storesOnCpu{0}, storesDrained{0};

std::map<std::uint32_t, std::uint64_t> drainCounts;

std::uint64_t drainTotal = 0;

std::chrono::steady_clock::time_point lastSyncReport = std::chrono::steady_clock::now();

std::atomic<std::uint64_t> pollLabelRecords{0}, hookLabelRecords{0};

SubmissionCosts& submissionCosts(std::uint32_t queue) {
    static std::array<SubmissionCosts, 64> costs;
    return costs[queue == 0 ? 0 : std::min<std::uint32_t>(queue - 0x20 + 1, 63)];
}

void reportSync() {
    std::string report;
    for (const auto& [code, count] : drainCounts) report += " " + (code == 0xffffu ? std::string("flip") : Pm4::Name(code << 8u)) + "=" + std::to_string(count);
    AgcDriver::ProfilePrint_nid_no_patch("[sync] %llu device drains by packet:%s (%llu waited without the GPU mutex); %llu labels written on the GPU, %llu deferred behind completions, %llu no-op, fallbacks: idle %llu, completions %llu, not imported %llu, undecodable %llu; %llu labels queued per worker, recorded in %llu groups, %llu recorded at once; stores: %llu recorded on the GPU, %llu behind completions, %llu on the CPU (idle), %llu synced (drained)\n", static_cast<unsigned long long>(drainTotal), report.c_str(), static_cast<unsigned long long>(unlockedDrains.load()), static_cast<unsigned long long>(gpuLabels.load()), static_cast<unsigned long long>(completionLabels.load()), static_cast<unsigned long long>(noOpLabels.load()), static_cast<unsigned long long>(labelFallbacks[1].load()), static_cast<unsigned long long>(labelFallbacks[2].load()), static_cast<unsigned long long>(labelFallbacks[3].load()), static_cast<unsigned long long>(labelFallbacks[4].load()), static_cast<unsigned long long>(queuedLabels.load()), static_cast<unsigned long long>(labelGroups.load()), static_cast<unsigned long long>(immediateLabels.load()), static_cast<unsigned long long>(storesOnGpu.load()), static_cast<unsigned long long>(storesBehindCompletions.load()), static_cast<unsigned long long>(storesOnCpu.load()), static_cast<unsigned long long>(storesDrained.load()));
}

void countLabelOutcome(int reason) {
    if (reason == 0) ++gpuLabels;
    else if (reason == 5) ++completionLabels;
    else ++labelFallbacks[std::min(reason, 4)];
}

}
