#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_SUBMISSION_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_SUBMISSION_HPP

#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/SubmissionProgress.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <set>
#include <span>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace AgcDriver::DriverDetail {

class RegisterLists {
public:
    void clear() { entries.clear(); }
    void append(std::size_t cursor, std::vector<std::uint32_t> pairs) {
        if (entries.empty()) entries.reserve(16);
        entries.emplace_back(cursor, std::move(pairs));
    }
    const std::vector<std::uint32_t>& at(std::size_t cursor) const {
        const auto* found = find(cursor);
        if (found == nullptr) throw std::out_of_range("map::at:  key not found");
        return *found;
    }
    const std::vector<std::uint32_t>* find(std::size_t cursor) const {
        const auto found = std::lower_bound(entries.begin(), entries.end(), cursor, [](const auto& entry, std::size_t key) { return entry.first < key; });
        return found == entries.end() || found->first != cursor ? nullptr : &found->second;
    }
    std::size_t size() const { return entries.size(); }

private:
    std::vector<std::pair<std::size_t, std::vector<std::uint32_t>>> entries;
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
    RegisterLists registerLists;
    std::vector<std::size_t> scanCursors;
    bool scanAll = true;
    bool suspend = false;
    bool waitFree = false;
    bool holdsFlip = false;

    std::uint64_t received = 0;
    std::vector<std::uint64_t> labelWrites;
    std::set<std::size_t> heldAtSubmit;
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

ReferencedProgramScan ReferencedPrograms(std::span<const std::uint32_t> commands, const RegisterLists& registerLists);

}

#endif
