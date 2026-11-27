#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_QUEUES_SUBMISSIONRING_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_QUEUES_SUBMISSIONRING_HPP

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <string_view>

namespace AgcDriver::DriverDetail {

inline constexpr std::uint32_t SubmissionQueueEnd = 0x59;
inline constexpr std::uint32_t UnmeasuredSubmissionQueueEnd = 0x58;

std::size_t SubmissionRingCapacity();

std::chrono::seconds StallWatchdogPeriod();

using StallClock = std::chrono::steady_clock::time_point (*)();
void SetStallClock(StallClock clock);

enum class StallChoice { Wait, Quit, Unavailable };

using StallPresenter = StallChoice (*)(const char* message);
void SetStallPresenter(StallPresenter presenter);
StallChoice ShowStallPrompt(const char* message);

bool StallPromptEnabled();

bool ReadSubmissionRingSettings();

void PrepareSubmissionRing(std::uint32_t queue);

void RecordSubmission(std::uint32_t queue, std::uint64_t serial, std::span<const std::uint32_t> commands, const void* guest, bool rewindTail);

void DumpSubmissionRings(std::string_view reason, std::string_view detail = {});

void NoteWorkerPacket(std::uint32_t queue, std::uint64_t serial, std::size_t offset) noexcept;
void NoteWorkerIdle(std::uint32_t queue) noexcept;

enum class SubmitStage : std::uint8_t { Reading, FlipRoom, FlipHold, ShaderPreparation, Accepting };

class SubmitCall {
public:
    SubmitCall(std::uint32_t queue, std::size_t dwords);
    ~SubmitCall() { Accepted(); }
    SubmitCall(const SubmitCall&) = delete;
    SubmitCall& operator=(const SubmitCall&) = delete;
    void Enter(SubmitStage stage);
    void Accepted();

private:
    int slot = -1;
};

struct UnacceptedSubmit {
    std::uint32_t queue = 0;
    std::size_t dwords = 0;
    SubmitStage stage = SubmitStage::Reading;
    std::uint64_t stageTenthsOfMs = 0;
    std::uint64_t callTenthsOfMs = 0;
};

inline constexpr std::size_t MaximumUnacceptedSubmits = 32;
inline constexpr char NoUnacceptedSubmitText[] = "no submit call is waiting to be accepted";

std::size_t UnacceptedSubmits(std::span<UnacceptedSubmit> out);
void DescribeUnacceptedSubmit(const UnacceptedSubmit& submit, std::span<char> text);

class StallWatch {
public:
    explicit StallWatch(std::uint32_t queue) : queue(queue) {}
    void Progressed() {
        since = {};
        progressed = true;
    }
    void Check(const char* waiter, std::uint64_t value, std::unique_lock<std::mutex>* held = nullptr);

private:
    std::uint32_t queue;
    std::chrono::steady_clock::time_point since{};
    std::chrono::steady_clock::time_point began{};
    bool dumped = false;
    bool progressed = false;
    bool staleDumped = false;
    bool finished = false;
    int prompts = 0;
};

}

#endif
