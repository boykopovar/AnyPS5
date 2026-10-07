#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_WAITMEMORY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_WAITMEMORY_HPP

#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace AgcDriver::DriverDetail {

inline constexpr std::size_t LabelStoreHistory = 4;

inline constexpr const char* LabelRefusalNames[6] = {"none", "trust-off", "queued", "overwritten", "unclosed", "behind-completion"};

inline constexpr std::chrono::microseconds PollTryInterval{1000};

struct LabelStore {
    std::uint64_t stamp = 0;
    std::uint32_t value = 0;
    std::uint32_t queue = 0xffffffffu;
};

struct WriteRecord {
    std::uint64_t target;
    std::uint64_t length;
    std::uint32_t queue;
    std::uint32_t opcode;
};

struct PollStats {
    std::uint64_t ticks = 0, tableHits = 0, tries = 0, triesFailed = 0, submits = 0, reaps = 0, reapsWithWork = 0;

    std::uint64_t reapsBehindCompletion = 0, reapsOther = 0;
    std::array<std::uint64_t, 6> refusals{};
    double longestTryHoldUs = 0;
    double tryHoldUs = 0;
};

struct WaitOutcomes {
    std::uint64_t atEntry = 0, fromRecorder = 0, fromRecorderSameQueue = 0, fromRecorderPolling = 0, polled = 0, timedOut = 0, pollSubmits = 0, pollReaps = 0;
    std::uint64_t fromRecorderUnlocked = 0, entriesUnlocked = 0, entryTriesFailed = 0, fromRecorderLate = 0, lateRefusedCpuStore = 0;
};

struct EpochBumps {
    std::uint64_t submissions = 0, waits = 0, drains = 0, reaps = 0, packets = 0;
};

void PollSleep();

}

#endif
