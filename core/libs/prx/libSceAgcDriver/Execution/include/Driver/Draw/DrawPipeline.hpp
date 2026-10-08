#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_DRAWPIPELINE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_DRAWPIPELINE_HPP

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#include <optional>
#include <span>
#include <thread>
#include <utility>
#include <vector>

namespace AgcDriver::DriverDetail {

class DrawPipeline {
public:
    using Commit = std::function<void()>;
    using Range = std::pair<std::uint64_t, std::uint64_t>;
    enum class DrainReason : std::uint8_t { Packet, Flush, Labels, Capture, Indirect, Submission, Count };
    enum class Event : std::uint8_t { IndirectCommit, IndirectArgsDrain, Count };

    static DrawPipeline& Queue0();
    static std::size_t Depth();
    static bool PipelineIndirect();
    static bool DrainBeforeRead(std::uint64_t address, std::size_t bytes);
    static bool& Active();
    static std::atomic<std::uint64_t>& EpochToken();
    static void FollowEpoch(std::uint64_t token);

    void Enqueue(Commit commit, std::vector<Range> writes, std::uint64_t labelAddress = 0, std::vector<std::byte> labelBytes = {});
    void Drain(DrainReason reason, std::uint32_t opcode = 0x100);
    bool Busy() const { return outstanding.load(std::memory_order_acquire) != 0; }
    bool Overlaps(std::uint64_t address, std::size_t bytes);
    bool DrainIfOverlaps(std::span<const Range> ranges, DrainReason reason, std::uint32_t opcode = 0x100) {
        return DrainIfOverlaps(ranges, [](const Range& range) { return range; }, reason, opcode);
    }
    template<typename TRanges, typename TBounds>
    bool DrainIfOverlaps(const TRanges& ranges, TBounds&& bounds, DrainReason reason, std::uint32_t opcode = 0x100) {
        if (!Busy()) return false;
        if (!pendingWrites(ranges, bounds)) return false;
        Drain(reason, opcode);
        return true;
    }
    void Note(Event event) { events[static_cast<std::size_t>(event)].fetch_add(1, std::memory_order_relaxed); }
    std::uint64_t Events(Event event) const { return events[static_cast<std::size_t>(event)].load(std::memory_order_relaxed); }
    std::optional<std::uint64_t> PendingLabel(std::uint64_t address, std::size_t bytes);

private:
    struct Item {
        Commit commit;
        std::vector<Range> writes;
        std::uint64_t labelAddress = 0;
        std::vector<std::byte> labelBytes;
    };
    DrawPipeline() = default;
    template<typename TRanges, typename TBounds>
    bool pendingWrites(const TRanges& ranges, TBounds& bounds) {
        std::uint64_t lowest = ~std::uint64_t{0}, highest = 0;
        for (const auto& range : ranges) {
            const auto [first, end] = bounds(range);
            if (first >= end) continue;
            lowest = std::min(lowest, first);
            highest = std::max(highest, end);
        }
        if (lowest >= highest) return false;
        std::lock_guard lock(mutex);
        for (const auto& item : items) {
            for (const auto& [begin, limit] : item.writes) {
                if (begin >= highest || limit <= lowest) continue;
                for (const auto& range : ranges) {
                    const auto [first, end] = bounds(range);
                    if (first < limit && begin < end) return true;
                }
            }
        }
        return false;
    }
    void run();
    void rethrowFailure();
    void report(std::chrono::steady_clock::time_point now);

    std::mutex mutex;
    std::condition_variable wake;
    std::condition_variable idle;
    std::deque<Item> items;
    std::exception_ptr failure;
    std::atomic<std::size_t> outstanding{0};
    std::uint32_t idleWaiters = 0;
    bool committerWaiting = false;
    std::thread thread;
    std::uint64_t commits = 0;
    std::uint64_t commitNs = 0;
    std::uint64_t commitErrors = 0;
    std::uint64_t enqueued = 0;
    std::uint64_t fullWaitNs = 0;
    std::array<std::uint64_t, static_cast<std::size_t>(DrainReason::Count)> drains{};
    std::array<std::uint64_t, static_cast<std::size_t>(DrainReason::Count)> drainWaitNs{};
    std::array<std::uint64_t, 257> drainOpcodes{};
    std::uint64_t depthSum = 0;
    std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(Event::Count)> events{};
    std::array<std::uint64_t, static_cast<std::size_t>(Event::Count)> reportedEvents{};
    std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
};

}

#endif
