#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_QUEUES_SUBMISSIONRING_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_QUEUES_SUBMISSIONRING_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace AgcDriver::DriverDetail {

inline constexpr std::uint32_t SubmissionQueueEnd = 0x59;

std::size_t SubmissionRingCapacity();

std::chrono::seconds StallWatchdogPeriod();

bool ReadSubmissionRingSettings();

void PrepareSubmissionRing(std::uint32_t queue);

void RecordSubmission(std::uint32_t queue, std::uint64_t serial, std::span<const std::uint32_t> commands, const void* guest, bool rewindTail);

void DumpSubmissionRings(std::string_view reason);

class StallWatch {
public:
    explicit StallWatch(std::uint32_t queue) : queue(queue) {}
    void Progressed() { since = {}; }
    void Check(const char* waiter, std::uint64_t value);

private:
    std::uint32_t queue;
    std::chrono::steady_clock::time_point since{};
    bool dumped = false;
};

}

#endif
