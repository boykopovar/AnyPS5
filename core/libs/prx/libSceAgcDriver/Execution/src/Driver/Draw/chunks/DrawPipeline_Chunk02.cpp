#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libc/include/HostThread.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/WorkerAffinity.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/HandoffSpin.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileCounter.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/WorkerSampler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <string>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace AgcDriver::DriverDetail {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
void DrawPipeline::run() {
    NameDriverThread("draw-commit");
    PerformanceTrace::BindThread("draw committer");
    ReserveCommitterCore();
    StartCommitterSampler();
    for (;;) {
        Commit* commit = nullptr;
        bool isDraw = false;
        SpinUntil(HandoffSpinWindow(), [&] { return outstanding.load(std::memory_order_acquire) != 0; });
        {
            std::unique_lock lock(mutex);
            committerWaiting = true;
            wake.wait(lock, [&] { return !items.empty() || closing; });
            committerWaiting = false;
            if (items.empty()) break;
            commit = stopped ? nullptr : &items.front().commit;
            isDraw = items.front().draw;
        }
        const auto start = std::chrono::steady_clock::now();
        std::exception_ptr fatal;
        bool failed = false;
        bool shutdown = false;
        try {
            if (commit != nullptr) (*commit)();
            if (!isDraw) GuestMemory::BreakProofs();
        } catch (const std::exception&) {
            failed = true;
            fatal = std::current_exception();
        } catch (const ProcessShutdown&) {
            shutdown = true;
            fatal = std::current_exception();
        } catch (...) {
            fatal = std::current_exception();
        }
        const auto spent = elapsedNs(start);
        if (fatal != nullptr && !stopped) {
            stopped = true;
            if (!shutdown) Driver::Get().ReportFailure(fatal);
        }
        std::lock_guard lock(mutex);
        countWrites(items.front().writes, false);
        const auto kindIndex = static_cast<std::size_t>(items.front().kind);
        kindCommits[kindIndex].fetch_add(1, std::memory_order_relaxed);
        kindNs[kindIndex].fetch_add(spent, std::memory_order_relaxed);
        retired.push_back(std::move(items.front()));
        items.pop_front();
        retiredItems.fetch_add(1, std::memory_order_release);
        ++commits;
        commitNs += spent;
        if (failed) ++commitErrors;
        if (fatal != nullptr && failure == nullptr) failure = fatal;
        outstanding.fetch_sub(1, std::memory_order_release);
        if ((producersWaiting != 0 && items.size() <= ResumeDepth()) || (drainersWaiting != 0 && items.empty())) {
            producersWaiting = 0;
            drainersWaiting = 0;
            idle.notify_all();
        }
    }
    runningCommitters.fetch_sub(1, std::memory_order_acq_rel);
}

void DrawPipeline::Stop() {
    std::thread committer;
    {
        std::lock_guard lock(mutex);
        closing = true;
        committer = std::move(thread);
    }
    wake.notify_all();
    if (!committer.joinable()) return;
    if (committer.get_id() == std::this_thread::get_id()) committer.detach();
    else committer.join();
}

void DrawPipeline::ReleaseRetired() {
    std::vector<Item> done;
    std::lock_guard lock(mutex);
    std::swap(done, retired);
    reclaimedItems += done.size();
}

void DrawPipeline::Drain(DrainReason reason, std::uint32_t opcode) {
    drainRequests[std::min<std::uint32_t>(opcode, 0x100)].fetch_add(1, std::memory_order_relaxed);
    reasonRequests[static_cast<std::size_t>(reason)].fetch_add(1, std::memory_order_relaxed);
    const bool finisherBusy = DrawFinisher::Busy() && !DrawFinisher::OnFinisherThread();
    if ((finisherBusy || Busy()) && GuestMemory::GpuMutex().HeldByThisThread()) throw std::runtime_error("draw pipeline drain under the GPU lock (reason " + std::to_string(static_cast<unsigned>(reason)) + ", opcode " + std::to_string(opcode) + ")");
    if (finisherBusy) DrawFinisher::Get().Drain();
    if (!Busy()) return;
    const auto start = std::chrono::steady_clock::now();
    SpinUntil(HandoffSpinWindow() * 2, [&] { return !Busy(); });
    std::unique_lock lock(mutex);
    while (!items.empty()) {
        ++drainersWaiting;
        drainWaits.fetch_add(1, std::memory_order_release);
        idle.wait(lock);
    }
    const auto index = static_cast<std::size_t>(reason);
    ++drains[index];
    ++drainOpcodes[std::min<std::uint32_t>(opcode, 0x100)];
    drainWaitNs[index] += elapsedNs(start);
    rethrowFailure();
}

std::uint64_t DrawPipeline::DrainRequestsByOpcode(std::uint32_t opcode) {
    return Queue0().drainRequests[std::min<std::uint32_t>(opcode, 0x100)].load(std::memory_order_relaxed);
}

bool DrawPipeline::HeldReads(std::uint64_t address, std::size_t bytes) {
    static const bool unordered = std::getenv("APS5_NO_QUEUED_DRAW_READ_ORDER") != nullptr;
    if (bytes == 0 || unordered) return false;
    const auto end = address + bytes;
    auto& pipeline = Queue0();
    std::lock_guard lock(pipeline.mutex);
    heldReadQueries.Add(1);
    for (const auto& item : pipeline.items) {
        for (const auto& [begin, limit] : item.reads) {
            if (address >= limit || begin >= end) continue;
            heldReadHits.Add(1);
            return true;
        }
    }
    return false;
}

std::uint64_t DrawPipeline::HeldReadQueries() {
    return heldReadQueries.Sum();
}

void DrawPipeline::ReleaseRunningReads() {
    std::lock_guard lock(mutex);
    if (!items.empty()) items.front().reads.clear();
}

std::uint64_t DrawPipeline::DrainRequestsByReason(DrainReason reason) {
    return Queue0().reasonRequests[static_cast<std::size_t>(reason)].load(std::memory_order_relaxed);
}

std::optional<std::uint64_t> DrawPipeline::PendingLabel(std::uint64_t address, std::size_t bytes) {
    if (bytes == 0 || bytes > 8) return std::nullopt;
    if (DrawFinisher::Overlap()) {
        std::uint64_t held = 0;
        const auto lookup = DrawFinisher::Get().PendingLabel(address, bytes, held);
        if (lookup == DrawFinisher::LabelLookup::Value) return held;
        if (lookup == DrawFinisher::LabelLookup::Unknown) return std::nullopt;
    }
    const auto end = address + bytes;
    std::lock_guard lock(mutex);
    for (auto item = items.rbegin(); item != items.rend(); ++item) {
        const bool overlaps = std::any_of(item->writes.begin(), item->writes.end(), [&](const Range& range) { return address < range.second && range.first < end; });
        if (!overlaps) continue;
        if (item->labelBytes.empty() || address < item->labelAddress || end > item->labelAddress + item->labelBytes.size()) return std::nullopt;
        std::uint64_t value = 0;
        std::memcpy(&value, item->labelBytes.data() + (address - item->labelAddress), bytes);
        return value;
    }
    return std::nullopt;
}

DrawPipeline::QueuedWrite DrawPipeline::StoredBytes(std::uint64_t address, std::size_t bytes, std::span<std::byte> out) {
    const auto end = address + bytes;
    if (DrawFinisher::Overlap() && DrawFinisher::Get().PendingOverlaps(address, end)) return QueuedWrite::Unknown;
    auto answer = QueuedWrite::None;
    std::lock_guard lock(mutex);
    for (auto item = items.rbegin(); item != items.rend(); ++item) {
        if (std::none_of(item->writes.begin(), item->writes.end(), [&](const Range& range) { return address < range.second && range.first < end; })) continue;
        if (!item->store) return QueuedWrite::Unknown;
        if (out.empty()) {
            answer = QueuedWrite::Stored;
            continue;
        }
        if (out.size() != bytes || address < item->labelAddress || end > item->labelAddress + item->labelBytes.size()) return QueuedWrite::Unknown;
        std::memcpy(out.data(), item->labelBytes.data() + (address - item->labelAddress), bytes);
        return QueuedWrite::Stored;
    }
    return answer;
}

void DrawPipeline::countWrites(const std::vector<Range>& writes, bool added) {
    for (const auto& range : writes) {
        const auto found = std::find_if(queuedWrites.begin(), queuedWrites.end(), [&](const auto& entry) { return entry.first == range; });
        if (added) {
            if (found != queuedWrites.end()) ++found->second;
            else queuedWrites.emplace_back(range, 1u);
        } else if (found != queuedWrites.end() && --found->second == 0) {
            *found = queuedWrites.back();
            queuedWrites.pop_back();
        }
    }
}

bool DrawPipeline::CountedWatch() {
    static const bool counted = std::getenv("APS5_NO_DECODE_WATCH_COUNTS") == nullptr;
    return counted;
}

DrawPipeline::Watch::~Watch() {
    if (pinned != nullptr) pinned->releaseWatch(*this);
}

void DrawPipeline::unpinWatch(Watch& watch) {
    if (watch.pinned == nullptr) return;
    --watchers;
    watch.pinned = nullptr;
}

void DrawPipeline::releaseWatch(Watch& watch) {
    std::lock_guard lock(mutex);
    unpinWatch(watch);
}

DrawPipeline::Watch DrawPipeline::WatchWrites() {
    if (!Busy()) return Watch{Retired(), 0};
    std::lock_guard lock(mutex);
    Watch watch{retiredItems.load(std::memory_order_relaxed), items.size()};
    if (CountedWatch()) {
        if (watch.items != 0) {
            ++watchers;
            watch.pinned = this;
        }
        return watch;
    }
    for (const auto& item : items) watch.writes.insert(watch.writes.end(), item.writes.begin(), item.writes.end());
    return watch;
}

bool DrawPipeline::WrittenSince(Watch& watch, const std::vector<Range>& reads) {
    auto hullBegin = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t hullEnd = 0;
    for (const auto& read : reads) {
        hullBegin = std::min(hullBegin, read.first);
        hullEnd = std::max(hullEnd, read.second);
    }
    const auto overlaps = [&](const std::vector<Range>& writes) {
        for (const auto& write : writes) {
            if (write.second <= hullBegin || hullEnd <= write.first) continue;
            if (std::any_of(reads.begin(), reads.end(), [&write](const Range& read) { return read.first < write.second && write.first < read.second; })) return true;
        }
        return false;
    };
    if (DrawFinisher::Overlap() && DrawFinisher::Get().PendingOverlapsAny(reads)) {
        releaseWatch(watch);
        return true;
    }
    std::lock_guard lock(mutex);
    struct Unpin {
        DrawPipeline& queue;
        Watch& watch;
        ~Unpin() { queue.unpinWatch(watch); }
    } unpin{*this, watch};
    if (std::any_of(items.begin(), items.end(), [&](const Item& item) { return overlaps(item.writes); })) return true;
    const auto retiredNow = retiredItems.load(std::memory_order_relaxed);
    if (retiredNow == watch.retired) return false;
    const auto since = retiredNow - watch.retired;
    if (since > watch.items) return true;
    if (watch.pinned == nullptr) return overlaps(watch.writes);
    if (watch.retired < reclaimedItems || watch.retired - reclaimedItems + since > retired.size()) return true;
    countedLookups.fetch_add(1, std::memory_order_relaxed);
    const auto first = static_cast<std::size_t>(watch.retired - reclaimedItems);
    for (std::size_t index = first; index < first + static_cast<std::size_t>(since); ++index) {
        if (overlaps(retired[index].writes)) return true;
    }
    return false;
}

bool DrawPipeline::Busy() const {
    return outstanding.load(std::memory_order_acquire) != 0 || (finisherProducer && DrawFinisher::Busy() && DrawFinisher::Overlap());
}

bool DrawPipeline::OverlapsAny(std::span<const std::pair<std::uint64_t, std::uint64_t>> ranges) {
    if (DrawFinisher::Overlap() && DrawFinisher::Get().PendingOverlapsAny(ranges)) return true;
    std::lock_guard lock(mutex);
    for (const auto& [range, count] : queuedWrites) {
        for (const auto& [address, end] : ranges) {
            if (address < range.second && range.first < end) return true;
        }
    }
    return false;
}

bool DrawPipeline::Overlaps(std::uint64_t address, std::size_t bytes) {
    const auto end = address + bytes;
    if (DrawFinisher::Overlap() && DrawFinisher::Get().PendingOverlaps(address, end)) return true;
    std::lock_guard lock(mutex);
    for (const auto& [range, count] : queuedWrites) {
        if (address < range.second && range.first < end) return true;
    }
    return false;
}

}
}
}
}
}
}
}
