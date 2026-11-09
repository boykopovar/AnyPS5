#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"

namespace AgcDriver::DriverDetail {

bool Driver::traceBudget() {
    static std::atomic<int> lines{0};
    return lines.fetch_add(1) < 600;
}

void Driver::traceCapture(const char* what, std::uint64_t program, std::uint32_t queue, std::span<const ShaderRecompiler::MemoryRegion> regions, double waitedMs) {
    if (waitedMs < 0.05 || !traceBudget()) return;
    std::fprintf(stderr, "[capsync] %s q0x%x program 0x%llx waited %.1f ms over %zu regions\n", what, queue, static_cast<unsigned long long>(program), waitedMs, regions.size());
    for (const auto& region : regions) {
        const auto begin = region.guestAddress;
        const auto end = begin + region.bytes.size();
        const auto pageBegin = begin & ~static_cast<std::uint64_t>(4095);
        const auto pageEnd = (end + 4095) & ~static_cast<std::uint64_t>(4095);
        if (!newestWriter(pageBegin, pageEnd)) continue;
        std::fprintf(stderr, "[capsync]   region 0x%llx+0x%zx: writers of its page(s):%s; of its dwords:%s\n", static_cast<unsigned long long>(begin), region.bytes.size(), describeWriters(pageBegin, pageEnd).c_str(), describeWriters(begin, end).c_str());
    }
}

void Driver::reportValidation(ValidateCounters& counters) {
    const auto count = [](std::uint64_t value) { return static_cast<unsigned long long>(value); };
    std::size_t tracked = 0, eligible = 0;
    {
        std::lock_guard lock(writtenBuffersMutex);
        tracked = dwordEvidence.size();
        for (const auto& [address, evidence] : dwordEvidence) {
            if (evidence.streak >= writeEvidenceAfter()) ++eligible;
        }
    }
    AgcDriver::ProfilePrint_nid_no_patch( "[validate] dispatch-cache compares over pending GPU writes (10 s): %llu; missed without the sync %llu, skipped the sync %llu, synced %llu waiting %.0f ms (no dispatch writer %llu, foreign writer %llu, larger range %llu, label %llu, evidence short %llu, writer changed %llu, sampled %llu, image %llu, shadow %llu, off %llu); known-value reads %llu; verify: %llu would-skip compares synced anyway (%.0f ms), %llu mismatches, %llu misses a sync made hits; dwords observed over waits: %llu unchanged, %llu changed (regions not observed: %llu after no GPU wait, %llu not compared); %zu tracked, %zu with a streak\n", count(counters.pending), count(counters.unsyncedMisses), count(counters.skipped), count(counters.syncedNoWriter + counters.syncedForeign + counters.syncedLargeRange + counters.syncedLabel + counters.syncedEvidence + counters.syncedWriterChanged + counters.syncedSample + counters.syncedImage + counters.syncedShadow + counters.syncedOff), counters.syncedWaitMs, count(counters.syncedNoWriter), count(counters.syncedForeign), count(counters.syncedLargeRange), count(counters.syncedLabel), count(counters.syncedEvidence), count(counters.syncedWriterChanged), count(counters.syncedSample), count(counters.syncedImage), count(counters.syncedShadow), count(counters.syncedOff), count(counters.knownValue), count(counters.verified), counters.verifiedWaitMs, count(counters.mismatches), count(counters.verifiedMissesHit), count(observedUnchanged.exchange(0)), count(observedChanged.exchange(0)), count(observationsNoWait.exchange(0)), count(observationsNotReached.exchange(0)), tracked, eligible);
    counters = ValidateCounters{};
}

}
