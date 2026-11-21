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

constexpr std::array<const char*, static_cast<std::size_t>(DrawPipeline::DrainReason::Count)> DrainNames{"packet", "flush", "labels", "capture", "indirect", "submission"};

constexpr std::array<const char*, static_cast<std::size_t>(DrawPipeline::CommitKind::Count)> KindNames{"other", "draw", "indirect", "colormeta", "dispatch", "sampledump", "dmafill", "dmawrite", "dmacopy", "store", "endofpipe", "seal", "submitend", "completion", "bufferfill"};

std::atomic<unsigned> runningCommitters{0};
ProfileCounter drawKeys;
ProfileCounter drawKeysSameAsPrevious;
ProfileCounter drawKeysSeenRecently;
ProfileCounter drawShapesSameAsPrevious;
ProfileCounter drawShapesSeenRecently;

std::uint64_t elapsedNs(std::chrono::steady_clock::time_point since) {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - since).count());
}

ProfileCounter heldReadRanges;
ProfileCounter heldReadQueries;
ProfileCounter heldReadHits;

template<typename TItem>
struct ClearOnExit {
    std::vector<TItem>& items;
    ~ClearOnExit() { items.clear(); }
};

}

namespace {
thread_local bool finisherThread = false;
thread_local bool finisherProducer = false;
}

DrawFinisher& DrawFinisher::Get() {
    static DrawFinisher* finisher = new DrawFinisher();
    return *finisher;
}

bool DrawFinisher::Enabled() {
    return Graphics::DrawPipelineEnabled();
}

bool DrawPipeline::MetadataPassesPipelined() {
    return Graphics::DrawPipelineEnabled();
}

namespace {
ProfileCounter pipelinedMetadataPasses;
}

std::uint64_t DrawPipeline::PipelinedMetadataPasses() {
    return pipelinedMetadataPasses.Sum();
}

void DrawPipeline::CountPipelinedMetadataPass() {
    pipelinedMetadataPasses.Add(1);
}

bool DrawFinisher::Overlap() {
    return Graphics::DrawPipelineEnabled();
}

bool DrawFinisher::PendingOverlaps(std::uint64_t begin, std::uint64_t end) {
    if (!Busy() || !finisherProducer) return false;
    std::lock_guard lock(mutex);
    for (const auto& held : pending) {
        if (held.unknown) return true;
        for (const auto& [first, last] : held.writes) {
            if (begin < last && first < end) return true;
        }
    }
    return false;
}

DrawFinisher::LabelLookup DrawFinisher::PendingLabel(std::uint64_t address, std::size_t bytes, std::uint64_t& value) {
    if (!Busy() || !finisherProducer) return LabelLookup::NotHeld;
    const auto end = address + bytes;
    std::lock_guard lock(mutex);
    for (auto held = pending.rbegin(); held != pending.rend(); ++held) {
        if (held->unknown) return LabelLookup::Unknown;
        const bool overlaps = std::any_of(held->writes.begin(), held->writes.end(), [&](const Range& range) { return address < range.second && range.first < end; });
        if (!overlaps) continue;
        if (held->labelBytes.empty() || address < held->labelAddress || end > held->labelAddress + held->labelBytes.size()) return LabelLookup::Unknown;
        value = 0;
        std::memcpy(&value, held->labelBytes.data() + (address - held->labelAddress), bytes);
        return LabelLookup::Value;
    }
    return LabelLookup::NotHeld;
}

bool DrawFinisher::PendingOverlapsAny(std::span<const Range> ranges) {
    if (!Busy() || !finisherProducer) return false;
    std::lock_guard lock(mutex);
    for (const auto& held : pending) {
        if (held.unknown) return true;
        for (const auto& [first, last] : held.writes) {
            for (const auto& [begin, end] : ranges) {
                if (begin < last && first < end) return true;
            }
        }
    }
    return false;
}

bool DrawFinisher::PendingKey(std::uint64_t key, std::uint64_t shape) {
    if (!Busy() || !finisherProducer) return false;
    std::lock_guard lock(mutex);
    return std::any_of(pending.begin(), pending.end(), [&](const Pending& held) { return held.key == key || (shape != 0 && held.shape == shape); });
}

namespace {
std::array<std::atomic<std::uint64_t>, static_cast<std::size_t>(DrawFinisher::DrainSite::Count)> finisherDrains{}, finisherSkips{};
}

void DrawFinisher::NoteDrain(DrainSite site, bool drained) {
    (drained ? finisherDrains : finisherSkips)[static_cast<std::size_t>(site)].fetch_add(1, std::memory_order_relaxed);
}

namespace {
std::array<std::atomic<std::uint64_t>, 257> drainedOpcodes{}, drainedBeside{};
}

void DrawFinisher::NoteDrainedOpcode(std::uint32_t opcode, bool besideCommits) {
    drainedOpcodes[std::min<std::uint32_t>(opcode, 256u)].fetch_add(1, std::memory_order_relaxed);
    if (besideCommits) drainedBeside[std::min<std::uint32_t>(opcode, 256u)].fetch_add(1, std::memory_order_relaxed);
}

std::string DrawFinisher::DrainText() {
    constexpr std::array<const char*, static_cast<std::size_t>(DrainSite::Count)> names{"lookup", "reads", "packet", "kept draw"};
    std::string text = "; worker drains of the finisher (drained/skipped):";
    for (std::size_t i = 0; i < names.size(); ++i) {
        char item[64];
        std::snprintf(item, sizeof(item), " %s %llu/%llu", names[i], static_cast<unsigned long long>(finisherDrains[i].exchange(0)), static_cast<unsigned long long>(finisherSkips[i].exchange(0)));
        text += item;
    }
    text += "; drained by opcode (of them beside commits):";
    for (int shown = 0; shown < 6; ++shown) {
        std::size_t top = 0;
        for (std::size_t i = 1; i < drainedOpcodes.size(); ++i) {
            if (drainedOpcodes[i].load() > drainedOpcodes[top].load()) top = i;
        }
        const auto count = drainedOpcodes[top].exchange(0);
        if (count == 0) break;
        char item[48];
        std::snprintf(item, sizeof(item), " 0x%02zx x%llu (%llu)", top, static_cast<unsigned long long>(count), static_cast<unsigned long long>(drainedBeside[top].exchange(0)));
        text += item;
    }
    return text;
}

bool DrawFinisher::OnFinisherThread() {
    return finisherThread;
}

void DrawFinisher::Enqueue(Job job) {
    Enqueue(std::move(job), Pending{});
}

void DrawFinisher::Enqueue(Job job, Pending held) {
    finisherProducer = true;
    constexpr std::size_t depth = 16;
    struct ReclaimStorage {};
    auto& reclaim = HostThreadLocal<std::vector<Job>, ReclaimStorage>();
    const ClearOnExit<Job> clearReclaim{reclaim};
    std::unique_lock lock(mutex);
    if (!ran.empty()) std::swap(ran, reclaim);
    if (failure != nullptr) std::rethrow_exception(std::exchange(failure, nullptr));
    if (jobs.size() >= depth && GuestMemory::GpuMutex().HeldByThisThread()) throw std::runtime_error("draw finisher full while the enqueuing thread holds the GPU lock");
    idle.wait(lock, [&] { return jobs.size() < depth; });
    jobs.push_back(std::move(job));
    pending.push_back(std::move(held));
    outstanding.fetch_add(1, std::memory_order_release);
    if (!running) {
        running = true;
        thread = std::thread([this] {
            finisherThread = true;
            run();
        });
    }
    if (sleeping) wake.notify_one();
}

void DrawFinisher::run() {
    NameDriverThread("draw-finish");
    PerformanceTrace::BindThread("draw finisher");
    ReserveFinisherCore();
    for (;;) {
        Job* job = nullptr;
        bool failed = false;
        SpinUntil(HandoffSpinWindow(), [&] { return outstanding.load(std::memory_order_acquire) != 0; });
        {
            std::unique_lock lock(mutex);
            sleeping = true;
            wake.wait(lock, [&] { return !jobs.empty(); });
            sleeping = false;
            job = &jobs.front();
            failed = failure != nullptr;
        }
        std::exception_ptr error;
        try {
            if (!failed) (*job)();
        } catch (...) {
            error = std::current_exception();
        }
        std::lock_guard lock(mutex);
        if (error != nullptr && failure == nullptr) failure = error;
        ran.push_back(std::move(jobs.front()));
        jobs.pop_front();
        pending.pop_front();
        outstanding.fetch_sub(1, std::memory_order_release);
        idle.notify_all();
    }
}

void DrawFinisher::Settle() {
    if (!Busy() || OnFinisherThread()) return;
    if (GuestMemory::GpuMutex().HeldByThisThread()) throw std::runtime_error("draw finisher settle under the GPU lock");
    std::unique_lock lock(mutex);
    idle.wait(lock, [&] { return jobs.empty(); });
}

void DrawFinisher::Drain() {
    if (OnFinisherThread()) return;
    if (Busy() && GuestMemory::GpuMutex().HeldByThisThread()) throw std::runtime_error("draw finisher drain under the GPU lock");
    SpinUntil(HandoffSpinWindow() * 2, [&] { return outstanding.load(std::memory_order_acquire) == 0; });
    std::unique_lock lock(mutex);
    if (!jobs.empty()) idle.wait(lock, [&] { return jobs.empty(); });
    if (failure != nullptr) std::rethrow_exception(std::exchange(failure, nullptr));
}

void DrawFinisher::ReleaseRan() {
    std::vector<Job> done;
    std::lock_guard lock(mutex);
    std::swap(done, ran);
}

DrawPipeline& DrawPipeline::Queue0() {
    static DrawPipeline* pipeline = new DrawPipeline();
    return *pipeline;
}

std::size_t DrawPipeline::Depth() {
    static const std::size_t depth = [] {
        const char* text = std::getenv("APS5_PIPELINED_DRAWS");
        if (std::getenv("APS5_LOCKED_DRAW_PREPARE") != nullptr || std::getenv("APS5_DRAW_DRAIN") != nullptr || std::getenv("APS5_DRAIN_ALL") != nullptr || std::getenv("APS5_NO_WORDWISE_CAPTURE") != nullptr) return std::size_t{0};
        if (text == nullptr) return std::size_t{32};
        const auto value = std::strtoull(text, nullptr, 10);
        return value == 0 ? std::size_t{0} : value == 1 ? std::size_t{8} : static_cast<std::size_t>(std::min<unsigned long long>(value, 256));
    }();
    return depth;
}

std::size_t DrawPipeline::ResumeDepth() {
    return Depth() / 2;
}

std::size_t DrawPipeline::QueuedItems() {
    auto& pipeline = Queue0();
    std::lock_guard lock(pipeline.mutex);
    return pipeline.items.size();
}

bool& DrawPipeline::Active() {
    static thread_local bool active = false;
    return active;
}

std::atomic<std::uint64_t>& DrawPipeline::EpochToken() {
    static std::atomic<std::uint64_t> token{1};
    return token;
}

namespace {

struct EpochSince {
    std::atomic<std::uint64_t> token{0};
    std::atomic<std::uint64_t> since{0};
};

std::array<EpochSince, 4096>& epochSinces() {
    static std::array<EpochSince, 4096> sinces;
    return sinces;
}

}

void DrawPipeline::NoteEpochSince(std::uint64_t token, std::uint64_t since) {
    auto& slot = epochSinces()[token % epochSinces().size()];
    slot.token.store(0, std::memory_order_relaxed);
    slot.since.store(since, std::memory_order_relaxed);
    slot.token.store(token, std::memory_order_release);
}

void DrawPipeline::FollowEpoch(std::uint64_t token) {
    static thread_local std::uint64_t followed = 0;
    if (followed == token) return;
    followed = token;
    const auto& slot = epochSinces()[token % epochSinces().size()];
    const bool noted = slot.token.load(std::memory_order_acquire) == token;
    const auto since = slot.since.load(std::memory_order_relaxed);
    GuestMemory::BumpCollectEpochSince(noted && slot.token.load(std::memory_order_acquire) == token ? since : 0);
}

unsigned DrawPipeline::Committers() {
    return runningCommitters.load(std::memory_order_acquire);
}

void DrawPipeline::rethrowFailure() {
    if (failure == nullptr) return;
    auto error = std::exchange(failure, nullptr);
    std::rethrow_exception(error);
}

void DrawPipeline::Enqueue(Commit commit, std::vector<Range> writes, std::uint64_t labelAddress, std::vector<std::byte> labelBytes, std::vector<Range> reads, bool draw) {
    enqueue({std::move(commit), std::move(writes), labelAddress, std::move(labelBytes), std::move(reads), draw});
}

void DrawPipeline::Enqueue(CommitKind kind, Commit commit, std::vector<Range> writes, std::uint64_t labelAddress, std::vector<std::byte> labelBytes, std::vector<Range> reads, bool draw) {
    enqueue({std::move(commit), std::move(writes), labelAddress, std::move(labelBytes), std::move(reads), draw, false, kind});
}

void DrawPipeline::NoteDrawKey(std::uint64_t key, std::uint64_t shape) {
    constexpr std::size_t RecentSlots = 4096;
    thread_local std::array<std::uint64_t, RecentSlots> recent{};
    thread_local std::uint64_t previous = 0;
    thread_local std::array<std::uint64_t, RecentSlots> recentShapes{};
    thread_local std::uint64_t previousShape = 0;
    const auto shapeSlot = static_cast<std::size_t>((shape ^ (shape >> 29u)) % RecentSlots);
    const auto slot = static_cast<std::size_t>((key ^ (key >> 29u)) % RecentSlots);
    drawKeys.Add(1);
    if (key == previous) drawKeysSameAsPrevious.Add(1);
    if (recent[slot] == key) drawKeysSeenRecently.Add(1);
    if (shape == previousShape) drawShapesSameAsPrevious.Add(1);
    if (recentShapes[shapeSlot] == shape) drawShapesSeenRecently.Add(1);
    recent[slot] = key;
    previous = key;
    recentShapes[shapeSlot] = shape;
    previousShape = shape;
}

void DrawPipeline::EnqueueStore(Commit commit, std::uint64_t address, std::vector<std::byte> bytes) {
    const auto end = address + bytes.size();
    enqueue({std::move(commit), {{address, end}}, address, std::move(bytes), {}, false, true, CommitKind::Store});
}

void DrawPipeline::enqueue(Item item) {
    if (DrawFinisher::Busy() && !DrawFinisher::OnFinisherThread()) DrawFinisher::Get().Drain();
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    struct ReclaimStorage {};
    auto& reclaim = HostThreadLocal<std::vector<Item>, ReclaimStorage>();
    const ClearOnExit<Item> clearReclaim{reclaim};
    const auto lockStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    std::unique_lock lock(mutex);
    if (!retired.empty() && watchers == 0) {
        reclaimedItems += retired.size();
        std::swap(retired, reclaim);
    }
    if (profile) lockWaitNs += elapsedNs(lockStart);
    rethrowFailure();
    if (closing) {
        lock.unlock();
        item.commit();
        return;
    }
    if (Depth() != 0 && items.size() >= Depth()) {
        if (GuestMemory::GpuMutex().HeldByThisThread()) throw std::runtime_error("draw pipeline full while the enqueuing thread holds the GPU lock");
        const auto start = std::chrono::steady_clock::now();
        while (items.size() >= Depth()) {
            ++producersWaiting;
            producerWaits.fetch_add(1, std::memory_order_release);
            idle.wait(lock);
        }
        fullWaitNs += elapsedNs(start);
        rethrowFailure();
    }
    depthSum += items.size();
    ++enqueued;
    heldReadRanges.Add(item.reads.size());
    countWrites(item.writes, true);
    items.push_back(std::move(item));
    outstanding.fetch_add(1, std::memory_order_release);
    if (!thread.joinable()) {
        thread = Libc::StartHostThread([this] {
            GuestMemory::MarkProofThread();
            run();
        });
        runningCommitters.fetch_add(1, std::memory_order_acq_rel);
    }
    if (committerWaiting) {
        ++wakes;
        wake.notify_one();
    }
    static const bool stats = std::getenv("APS5_PIPELINE_STATS") != nullptr;
    if (!profile && !stats) return;
    const auto now = std::chrono::steady_clock::now();
    if (now - lastReport >= std::chrono::seconds(10)) report(now);
}

void DrawPipeline::report(std::chrono::steady_clock::time_point now) {
    lastReport = now;
    std::string drainText;
    for (std::size_t i = 0; i < drains.size(); ++i) {
        char item[64];
        std::snprintf(item, sizeof(item), " %s %llu (%.1f ms)", DrainNames[i], static_cast<unsigned long long>(drains[i]), static_cast<double>(drainWaitNs[i]) / 1e6);
        drainText += item;
    }
    drainText += "; packet drains by opcode:";
    for (int shown = 0; shown < 5; ++shown) {
        const auto top = std::max_element(drainOpcodes.begin(), drainOpcodes.begin() + 0x100);
        if (*top == 0) break;
        char item[32];
        std::snprintf(item, sizeof(item), " 0x%02x x%llu", static_cast<unsigned>(top - drainOpcodes.begin()), static_cast<unsigned long long>(*top));
        drainText += item;
        *top = 0;
    }
    drainOpcodes = {};
    const auto perCommit = commits == 0 ? 0.0 : static_cast<double>(commitNs) / 1e3 / static_cast<double>(commits);
    const auto perDraw = enqueued == 0 ? 0.0 : static_cast<double>(fullWaitNs) / 1e3 / static_cast<double>(enqueued);
    const auto queued = enqueued == 0 ? 0.0 : static_cast<double>(depthSum) / static_cast<double>(enqueued);
    const auto decodeDrainCount = decodeDrains.load(std::memory_order_relaxed);
    char reads[96];
    std::snprintf(reads, sizeof(reads), " %llu read ranges, %llu stores held, %llu decode drains", static_cast<unsigned long long>(heldReadRanges.Sum()), static_cast<unsigned long long>(heldReadHits.Take()), static_cast<unsigned long long>(decodeDrainCount - reportedDecodeDrains));
    reportedDecodeDrains = decodeDrainCount;
    AgcDriver::ProfilePrint_nid_no_patch("[draw] pipelined (10 s, depth %zu): %llu commits at %.1f us each on the committer, %llu failed; %llu enqueued behind %.2f on average, waiting %.1f us per draw for room, %.2f us for the lock; %llu committer wakes;%s; drains:%s\n", Depth(), static_cast<unsigned long long>(commits), perCommit, static_cast<unsigned long long>(commitErrors), static_cast<unsigned long long>(enqueued), queued, perDraw, enqueued == 0 ? 0.0 : static_cast<double>(lockWaitNs) / 1e3 / static_cast<double>(enqueued), static_cast<unsigned long long>(wakes), reads, drainText.c_str());
    std::string kindText;
    for (std::size_t i = 0; i < kindCommits.size(); ++i) {
        const auto count = kindCommits[i].exchange(0, std::memory_order_relaxed);
        const auto ns = kindNs[i].exchange(0, std::memory_order_relaxed);
        if (count == 0) continue;
        char item[64];
        std::snprintf(item, sizeof(item), " %s %llu (%.1f us)", KindNames[i], static_cast<unsigned long long>(count), static_cast<double>(ns) / 1e3 / static_cast<double>(count));
        kindText += item;
    }
    AgcDriver::ProfilePrint_nid_no_patch("[commits] by kind (10 s):%s\n", kindText.c_str());
    const auto vk = Graphics::TakeVkDrawCounts();
    AgcDriver::ProfilePrint_nid_no_patch("[vkdraw] (10 s): %llu vkCmdDraw, %llu vkCmdDrawIndexed, %llu vkCmdDrawIndirect calls, %llu mesh; draw keys %llu, same as the previous draw %llu, seen within the last 4096 draws %llu; draw shapes (state without per-draw user words) same as the previous %llu, seen within the last 4096 %llu\n", static_cast<unsigned long long>(vk.direct), static_cast<unsigned long long>(vk.indexed), static_cast<unsigned long long>(vk.indirect), static_cast<unsigned long long>(vk.mesh), static_cast<unsigned long long>(drawKeys.Take()), static_cast<unsigned long long>(drawKeysSameAsPrevious.Take()), static_cast<unsigned long long>(drawKeysSeenRecently.Take()), static_cast<unsigned long long>(drawShapesSameAsPrevious.Take()), static_cast<unsigned long long>(drawShapesSeenRecently.Take()));
    commits = commitNs = commitErrors = enqueued = fullWaitNs = depthSum = wakes = lockWaitNs = 0;
    drains = {};
    drainWaitNs = {};
}


}
