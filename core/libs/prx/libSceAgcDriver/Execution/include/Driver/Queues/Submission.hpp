#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_SUBMISSION_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_SUBMISSION_HPP

#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/SubmissionProgress.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace AgcDriver::DriverDetail {

enum class AwaitKind { Wait, Semaphore, Rewind };

struct LabelValue {
    std::uint64_t dword = 0;
    std::uint32_t value = 0;
    bool known = false;
};

struct AwaitedDword {
    std::uint64_t address = 0;
    std::uint32_t bytes = 0;
    bool satisfied = false;
    bool passed = false;
    AwaitKind kind = AwaitKind::Wait;
    std::uint32_t queue = 0;
    std::uint32_t function = 0;
    std::uint64_t reference = 0;
    std::uint64_t mask = 0;
    std::array<std::optional<LabelValue>, 2> ahead{};
};

struct Submission {
    std::uint64_t serial;
    std::uint32_t queue;
    std::vector<std::uint32_t> commands;
    std::shared_ptr<SubmissionProgress> progress;

    std::shared_ptr<const ShaderRegistry> shaders;
    std::map<std::size_t, std::shared_ptr<IFlipRequest>> flips;
    std::map<std::size_t, std::shared_ptr<IRenderingWait>> renderingWaits;
    std::map<std::size_t, std::size_t> conditionalEnds;
    std::map<std::size_t, std::vector<std::uint32_t>> registerLists;
    bool suspend = false;
    bool waitFree = false;
    bool holdsFlip = false;

    std::uint64_t received = 0;
    std::vector<std::uint64_t> labelWrites;
    std::vector<LabelValue> labelValues;
    std::set<std::size_t> heldAtSubmit;
    std::vector<AwaitedDword> awaitedDwords;
    std::chrono::steady_clock::time_point receivedAt{};
    std::chrono::steady_clock::time_point copiedAt{};
    std::chrono::steady_clock::time_point validatedAt{};
    std::chrono::steady_clock::time_point roomReadyAt{};
    std::chrono::steady_clock::time_point enqueuedAt{};
    std::chrono::steady_clock::time_point dequeuedAt{};
    std::chrono::steady_clock::time_point orderedAt{};
    const std::uint32_t* rewindTail = nullptr;
    std::size_t rewindWords = 0;
};

struct QueueWorker {
    std::deque<Submission> pending;

    std::atomic<std::uint64_t> queued{0};
    std::unordered_map<std::uint64_t, std::uint32_t> unfinishedWrites;
    std::uint64_t executingWaitFree = 0;
    bool executingWaits = false;
    alignas(64) std::atomic<std::uint64_t> executingPacket{0};
    std::thread thread;
};

struct ReferencedProgramScan {
    std::vector<std::uint64_t> addresses;
    bool complete = true;
};

ReferencedProgramScan ReferencedPrograms(std::span<const std::uint32_t> commands, const std::map<std::size_t, std::vector<std::uint32_t>>& registerLists);

}

#endif
