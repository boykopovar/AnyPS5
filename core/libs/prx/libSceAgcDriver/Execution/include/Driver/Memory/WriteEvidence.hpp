#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_WRITEEVIDENCE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_WRITEEVIDENCE_HPP

#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace AgcDriver::DriverDetail {

inline constexpr std::size_t WrittenBufferRing = 4096;

inline constexpr std::size_t DwordEvidenceEntries = 65536;

struct WrittenBuffer {
    std::uint64_t program;
    std::uint64_t begin;
    std::uint64_t end;
    std::uint64_t serial;
    std::uint32_t queue;
    bool atomic;
    std::shared_ptr<const std::vector<std::byte>> value;
    std::uint64_t generation = 0;
};

struct DwordEvidence {
    std::uint32_t streak = 0;
    std::uint32_t changed = 0;

    std::uint64_t program = 0;
    std::uint64_t begin = 0;
    std::uint64_t end = 0;
};

class SampledReadScope {
public:
    explicit SampledReadScope(std::atomic<std::uint64_t>& reads);
    ~SampledReadScope();
    SampledReadScope(const SampledReadScope&) = delete;
    SampledReadScope& operator=(const SampledReadScope&) = delete;

private:
    bool previous;
};

struct ValidateCounters {
    std::uint64_t pending = 0, unsyncedMisses = 0, skipped = 0, syncedNoWriter = 0, syncedForeign = 0,
                  syncedLargeRange = 0, syncedLabel = 0, syncedEvidence = 0, syncedWriterChanged = 0, syncedSample = 0,
                  syncedImage = 0, syncedShadow = 0, syncedOff = 0, verified = 0, mismatches = 0, verifiedMissesHit = 0,
                  knownValue = 0;
    double syncedWaitMs = 0, verifiedWaitMs = 0;
    std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
};

struct PendingView {
    std::shared_ptr<const Graphics::Recorder::WriteRanges> snapshot;
    std::uint64_t generation = 0;
    bool loaded = false;

    void Load();
    bool Overlaps(std::uint64_t address, std::size_t bytes) const;
};

}

#endif
