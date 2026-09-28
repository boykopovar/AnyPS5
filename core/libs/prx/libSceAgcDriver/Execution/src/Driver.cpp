#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/QueueState.hpp"
#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/WorkerSampler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libc/include/CpuTopology.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#include "CacheKey.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "Optimization/ShaderStageInputInfo.hpp"
#ifdef _WIN32
#include <windows.h>
#endif
#include <immintrin.h>
#include <bit>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <set>
#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <cstring>
#include <deque>
#include <exception>
#include <limits>
#include <list>
#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <unordered_map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace AgcDriver {
namespace {

// Milliseconds since the first trace line, for the APS5_TRACE_GPU timeline.
double TraceMs() {
    static const auto origin = std::chrono::steady_clock::now();
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - origin).count();
}

// A short sleep between polls of a label another queue or the CPU writes. std::this_thread::sleep_for
// goes through winpthreads' nanosleep and rounds up to the scheduler tick (15.6 ms unless raised),
// which multiplied by the dozens of queue-to-queue hand-offs in a frame; a high-resolution waitable
// timer sleeps for the 200 us asked.
void PollSleep() {
#ifdef _WIN32
    thread_local HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (timer != nullptr) {
        LARGE_INTEGER due{};
        due.QuadPart = -2000;  // 200 us in 100 ns units
        if (SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE)) {
            WaitForSingleObject(timer, INFINITE);
            return;
        }
    }
#endif
    std::this_thread::sleep_for(std::chrono::microseconds(200));
}

void require(bool condition, const char* reason) {
    if (!condition) {
        throw std::runtime_error(std::string("AGC driver: ") + reason);
    }
}

// The recompiler's resolved sources for programs of one registered shader (design13 step 3), keyed
// by the code offset, the device and the request's context (RecompileCacheKey::ContextHash): a
// capture then skips the stage input validation, the code hash, the key build and the source table.
// A snapshot object is replaced on re-registration, so the memo dies with it; a few entries per
// shader (the same program is requested with the same registers), the oldest replaced. Several
// queues dispatch one snapshot: mutexed. A compute resolve that threw (a plan failure, static per
// program and context) is kept as a poisoned entry (no handle, the message) so the dispatch skips
// the program without resolving, capturing or throwing again; `poisoned` counts the entries ever
// poisoned (the common path's test). A draw stage's resolve is never poisoned: its stage input
// validation throws on V# words (base, record count) the key does not cover, so the same key can
// resolve on the next draw.
struct HandleMemos {
    struct Entry {
        std::uint64_t key = 0;
        std::shared_ptr<const ShaderRecompiler::SourceHandle> handle;
        std::shared_ptr<const std::string> failure;
    };
    std::mutex mutex;
    std::array<Entry, 8> entries;
    std::size_t next = 0;
    std::atomic<std::uint32_t> poisoned{0};
};

// The recompiler's switch (APS5_NO_FAILURE_MEMO=1 rebuilds a failed plan every time) disables the
// poisoned source-handle entries too.
static bool FailureMemo() {
    static const bool memo = std::getenv("APS5_NO_FAILURE_MEMO") == nullptr;
    return memo;
}

struct ShaderSnapshot {
    std::uint64_t codeAddress;
    std::uint64_t headerAddress;
    std::uint8_t type;
    std::vector<std::uint32_t> code;
    std::vector<std::byte> header;
    std::unique_ptr<HandleMemos> handles = std::make_unique<HandleMemos>();
};

using ShaderRegistry = std::map<std::uint64_t, std::shared_ptr<const ShaderSnapshot>>;

// The source handle for `request` (a program of `snapshot` at `codeOffset`, compiled for the device
// of `deviceSerial`), from the snapshot's memo or resolved and memoized. Null when the memo is
// bypassed: `bypass` (the register probe, whose flag the recompiler's own key carries),
// APS5_NO_SOURCE_HANDLE_CACHE=1 (every capture resolves, as before) or APS5_NO_CAPTURE_REUSE=1
// (Recompile(request) resolves by itself). Debug aid: APS5_VERIFY_SOURCE_HANDLE=1 resolves on
// every hit as well and fails the dispatch when the memo answered a different source.
// A poisoned entry (see HandleMemos) answers null and, with `poisoned` given, its stored message
// (owned by the memo: valid while the snapshot lives); without `poisoned` it throws the message,
// as the resolve did.
std::shared_ptr<const ShaderRecompiler::SourceHandle> SourceHandleFor(const ShaderSnapshot& snapshot, std::size_t codeOffset, std::uint64_t deviceSerial, const ShaderRecompiler::RecompileRequest& request, bool bypass, const std::string** poisoned = nullptr) {
    static const bool enabled = std::getenv("APS5_NO_SOURCE_HANDLE_CACHE") == nullptr && std::getenv("APS5_NO_CAPTURE_REUSE") == nullptr;
    static const bool verify = std::getenv("APS5_VERIFY_SOURCE_HANDLE") != nullptr;
    if (!enabled || bypass || ShaderRecompiler::DebugProbeActive() || !request.useCache) return nullptr;
    std::uint64_t key = 0xcbf29ce484222325ull;
    for (const auto value : {static_cast<std::uint64_t>(codeOffset), deviceSerial, ShaderRecompiler::RecompileCacheKey::ContextHash(request)}) {
        key ^= value;
        key *= 0x100000001b3ull;
    }
    auto& memos = *snapshot.handles;
    {
        std::lock_guard lock(memos.mutex);
        for (const auto& entry : memos.entries) {
            if (entry.key != key || (entry.handle == nullptr && entry.failure == nullptr)) continue;
            if (entry.handle == nullptr) {
                if (verify) {
                    bool resolved = true;
                    try {
                        static_cast<void>(ShaderRecompiler::ResolveSource(request));
                    } catch (const std::exception&) {
                        resolved = false;
                    }
                    if (resolved) throw std::runtime_error("AGC driver: source handle memo poisoned but the source resolved");
                }
                ShaderMemory::CountHandleMemo(true);
                if (poisoned == nullptr) throw std::runtime_error(*entry.failure);
                *poisoned = entry.failure.get();
                return nullptr;
            }
            if (verify && ShaderRecompiler::ResolveSource(request)->source != entry.handle->source) throw std::runtime_error("AGC driver: source handle memo answered a different source");
            ShaderMemory::CountHandleMemo(true);
            return entry.handle;
        }
    }
    std::shared_ptr<const ShaderRecompiler::SourceHandle> handle;
    try {
        handle = ShaderRecompiler::ResolveSource(request);
    } catch (const std::exception& error) {
        if (FailureMemo() && request.shader.stage == ShaderRecompiler::ShaderStage::Compute) {
            std::lock_guard lock(memos.mutex);
            memos.entries[memos.next] = {key, nullptr, std::make_shared<const std::string>(error.what())};
            memos.next = (memos.next + 1) % memos.entries.size();
            memos.poisoned.fetch_add(1, std::memory_order_relaxed);
        }
        throw;
    }
    ShaderMemory::CountHandleMemo(false);
    if (handle == nullptr) return nullptr;
    std::lock_guard lock(memos.mutex);
    memos.entries[memos.next] = {key, handle, nullptr};
    memos.next = (memos.next + 1) % memos.entries.size();
    return handle;
}

struct Submission {
    std::uint64_t serial;
    std::uint32_t queue;
    std::vector<std::uint32_t> commands;
    // The registry as of the submit call (shared, never mutated once shared: Driver::RegisterShader
    // copies it first when a submission still holds it).
    std::shared_ptr<const ShaderRegistry> shaders;
    std::map<std::size_t, std::shared_ptr<IFlipRequest>> flips;
    std::map<std::size_t, std::shared_ptr<IRenderingWait>> renderingWaits;
    bool suspend = false;
    // Record-order stamp (Driver::eventSerial) taken when the game submitted: a WAIT_REG_MEM of this
    // submission trusts only labels the recorder noted with a newer stamp (see Recorder::NoteLabel).
    std::uint64_t received = 0;
    std::chrono::steady_clock::time_point enqueuedAt{};
};

std::uint32_t readRegister(const Registers& registers, std::uint32_t offset) {
    const auto it = registers.find(offset);
    require(it != registers.end(), "required shader register has not been written");
    return it->second;
}

// The last packets of a submission, for the WAIT_REG_MEM timeout report and the draw-failure dump.
// Only offsets are kept per packet: formatting every packet's line cost more than most packets
// themselves, and the reports are rare. The commands outlive the history (both live in execute).
struct PacketHistory {
    std::span<const std::uint32_t> commands;
    std::array<std::size_t, 64> offsets{};
    std::size_t count = 0;

    void Record(std::size_t offset) {
        offsets[count % offsets.size()] = offset;
        ++count;
    }

    std::string Format(std::size_t offset) const {
        const auto header = commands[offset];
        const auto words = std::min(Pm4::PacketWords(header), commands.size() - offset);
        char line[160];
        int length = std::snprintf(line, sizeof(line), "%s", Pm4::Name(header).c_str());
        for (std::size_t i = 1; i < words && i < 9 && length < 140; ++i) length += std::snprintf(line + length, sizeof(line) - length, " %08x", commands[offset + i]);
        return line;
    }

    // Visits the recorded packets' lines oldest to newest.
    template <typename Visit>
    void Each(Visit&& visit) const {
        const auto shown = std::min(count, offsets.size());
        const auto first = count > shown ? count % offsets.size() : 0;
        for (std::size_t i = 0; i < shown; ++i) visit(Format(offsets[(first + i) % offsets.size()]));
    }
};

// The driver's device. It is created, replaced and reset under GuestMemory::GpuMutex only; the
// atomic lets a dispatch's prologue (capture, cache validation, recompile), which runs without that
// lock, fetch it without waiting behind another queue's device phase. Arrow access is for holders of
// the GpuMutex, under which the pointee cannot be released.
class DevicePointer {
public:
    std::shared_ptr<VulkanDevice> load() const { return pointer.load(std::memory_order_acquire); }
    operator std::shared_ptr<VulkanDevice>() const { return load(); }
    DevicePointer& operator=(std::shared_ptr<VulkanDevice> value) {
        pointer.store(std::move(value), std::memory_order_release);
        return *this;
    }
    void reset() { *this = nullptr; }
    VulkanDevice* operator->() const { return load().get(); }
    explicit operator bool() const { return load() != nullptr; }
    bool operator==(std::nullptr_t) const { return load() == nullptr; }

private:
    std::atomic<std::shared_ptr<VulkanDevice>> pointer;
};

// [sync] statistics (APS5_PROFILE_DRAW): device drains by packet, and the label outcomes of
// VulkanDevice::WriteLabelOnGpu (reason 0 GPU, 5 GPU store plus a completion action behind
// write-backs, 6 completion action because the memory is not host-imported, 1-4 CPU fallbacks by
// reason). Not-imported labels
// are counted apart: a title whose CPU-side label polling stalls looks there first.
// Counted from the label path and the drain sites of Driver::execute; the report itself is printed
// under GuestMemory::GpuMutex only (lastSyncReport is plain).
std::atomic<std::uint64_t> gpuLabels{0}, completionLabels{0}, notImportedLabels{0}, noOpLabels{0}, unlockedDrains{0};
std::atomic<std::uint64_t> labelFallbacks[5] = {};
// Labels queued in a worker's deferred list, the groups they were recorded in, and labels that
// took the locked path at once (too large for the list, or APS5_LABEL_LOCK_EACH=1).
std::atomic<std::uint64_t> queuedLabels{0}, labelGroups{0}, immediateLabels{0};
// Label groups recorded and label batches submitted at the following packet's own GPU mutex
// acquisition (recordLabelsForPacket), groups and submits a failed try in flushBetweenPackets left
// to that acquisition (counted apart: a compute worker's try is often submit-only), groups a
// self-locking packet recorded right after its capture (a try that succeeded there, or a queued
// label overlapping the capture, which the packet then redoes), and suspend points (all in the
// [labels] line).
std::atomic<std::uint64_t> packetLockRecords{0}, packetLockSubmits{0}, packetLockDeferred{0}, packetSubmitDeferred{0}, captureTryRecords{0}, captureRetries{0}, suspendPoints{0};
// Boundary record-tries not made because the next packet is self-locking and the group's deadline
// has not passed (Driver::flushBetweenPackets, [labels] line).
std::atomic<std::uint64_t> recordTriesSkipped{0};
// Queue 0's worker sleeps in its untimed idle wait (Driver::run): nothing pending when it went to
// sleep, so a completion registered since has no reaper until work arrives, unless a poller reaps.
std::atomic<bool> queue0Dormant{false};
// APS5_PROFILE_DRAW: the time (microseconds) the record and the submit take at the packet's lock.
std::atomic<std::uint64_t> packetLockRecordUs{0}, packetLockSubmitUs{0};
// Failed try_locks of the GPU mutex by site ([labels] line): flushBetweenPackets' label record,
// its flush (deadline submit or size cap) and its boundary reap, a wait's poll service, the idle
// reap, the record after a capture and the record from a poll loop.
enum TrySite { TryLabel = 0, TryFlush, TryReap, TryPoll, TryIdle, TryCapture, TryPollRecord, TrySites };
std::atomic<std::uint64_t> triesFailed[TrySites] = {};
// Per-submission fixed costs by queue (APS5_PROFILE_DRAW; nanoseconds, printed as microseconds
// per submission on the queue's [packets] line and reset there): Driver::Submit's validation and
// copy on the game thread, the worker's dequeue latency (enqueue to pop) and its completion
// (markCompleted + notify), the suspend point's label record + submit under the GPU mutex (or
// skipped, nothing open), the submission-end submits made and skipped, the notifies skipped.
struct SubmissionCosts {
    std::atomic<std::uint64_t> submissions{0}, validateNs{0}, copyNs{0}, dequeueNs{0}, completeNs{0}, suspendNs{0}, suspends{0}, suspendsSkipped{0}, endSubmits{0}, endSkipped{0}, notifiesSkipped{0};
};
SubmissionCosts& submissionCosts(std::uint32_t queue) {
    static std::array<SubmissionCosts, 64> costs;
    return costs[queue == 0 ? 0 : std::min<std::uint32_t>(queue - 0x20 + 1, 63)];
}
// Non-label stores the driver makes for the title (COPY_DATA, DMA_DATA, DUMP_CONST_RAM; see the
// store path of Driver::execute): recorded on the GPU, recorded on the GPU and stored again by a
// completion action (behind a pending write-back, or memory the GPU has no view of: reasons 5 and
// 6, kept apart so a stale GPU read of a store is looked for there first), stored by the CPU under
// the mutex because the recorder was idle (nothing to order after), or stored by the CPU behind a
// device drain as before (too large, misaligned, not decodable, no device, or APS5_CPU_STORES=1).
std::atomic<std::uint64_t> storesOnGpu{0}, storesBehindCompletions{0}, storesOnCpu{0}, storesDrained{0};
std::map<std::uint32_t, std::uint64_t> drainCounts;
std::uint64_t drainTotal = 0;
std::chrono::steady_clock::time_point lastSyncReport = std::chrono::steady_clock::now();
void reportSync() {
    std::string report;
    for (const auto& [code, count] : drainCounts) report += " " + (code == 0xffffu ? std::string("flip") : Pm4::Name(code << 8u)) + "=" + std::to_string(count);
    std::fprintf(stderr, "[sync] %llu device drains by packet:%s (%llu waited without the GPU mutex); %llu labels written on the GPU, %llu deferred behind completions, %llu deferred not imported, %llu no-op, fallbacks: idle %llu, completions %llu, not imported %llu, undecodable %llu; %llu labels queued per worker, recorded in %llu groups, %llu recorded at once; stores: %llu recorded on the GPU, %llu behind completions, %llu on the CPU (idle), %llu synced (drained)\n", static_cast<unsigned long long>(drainTotal), report.c_str(), static_cast<unsigned long long>(unlockedDrains.load()), static_cast<unsigned long long>(gpuLabels.load()), static_cast<unsigned long long>(completionLabels.load()), static_cast<unsigned long long>(notImportedLabels.load()), static_cast<unsigned long long>(noOpLabels.load()), static_cast<unsigned long long>(labelFallbacks[1].load()), static_cast<unsigned long long>(labelFallbacks[2].load()), static_cast<unsigned long long>(labelFallbacks[3].load()), static_cast<unsigned long long>(labelFallbacks[4].load()), static_cast<unsigned long long>(queuedLabels.load()), static_cast<unsigned long long>(labelGroups.load()), static_cast<unsigned long long>(immediateLabels.load()), static_cast<unsigned long long>(storesOnGpu.load()), static_cast<unsigned long long>(storesBehindCompletions.load()), static_cast<unsigned long long>(storesOnCpu.load()), static_cast<unsigned long long>(storesDrained.load()));
}
void countLabelOutcome(int reason) {
    if (reason == 0) ++gpuLabels;
    else if (reason == 5) ++completionLabels;
    else if (reason == 6) ++notImportedLabels;
    else ++labelFallbacks[std::min(reason, 4)];
}

// A worker's deferred labels. A label packet (RELEASE_MEM, WRITE_DATA) no longer takes the GPU
// mutex to record its store: the label is queued here, on the worker's own thread, and the whole
// group is recorded under the mutex the next time this worker needs it anyway (before any packet
// of its queue that touches memory or the device: the label still precedes that work in queue
// order), or at the label flush deadline (APS5_LABEL_FLUSH_US) checked between packets. Other
// queues and game threads poll memory for the value, so their latency stays bounded by the
// deadline. Debug aid: APS5_LABEL_LOCK_EACH=1 records every label under the mutex as before.
struct DeferredLabel {
    static constexpr std::size_t Capacity = 32;
    std::uint64_t address;
    std::size_t size;
    std::array<std::byte, Capacity> bytes;
};
struct DeferredLabels {
    std::vector<DeferredLabel> labels;
    // When the first label of the group was queued (the deadline counts from here).
    std::chrono::steady_clock::time_point since;
};
DeferredLabels& deferredLabels() {
    static thread_local DeferredLabels deferred;
    return deferred;
}
// APS5_WORKER_AFFINITY=1 sends the queue workers and the presenter to the performance cores of a
// hybrid CPU (with libkernel's APS5_JOB_AFFINITY=1 for the title's spinning job workers); pinned
// by default the video stage measured 0 to -9 % (the presenter's GPU wait doubled), so the default
// leaves them free. APS5_WORKER_AFFINITY_MASK=<hex> picks the cores; APS5_TRACE_AFFINITY=1 names
// each pinned thread, otherwise the first pin prints one summary line.
std::uint64_t WorkerAffinityMask() {
    static const std::uint64_t mask = [] {
        if (std::getenv("APS5_NO_WORKER_AFFINITY") != nullptr) return std::uint64_t{0};
        const auto requested = CpuTopology::MaskFromEnvironment("APS5_WORKER_AFFINITY_MASK");
        if (requested != 0) return requested;
        if (std::getenv("APS5_WORKER_AFFINITY") == nullptr) return std::uint64_t{0};
        const auto& layout = CpuTopology::Get();
        return layout.hybrid ? layout.performant : std::uint64_t{0};
    }();
    return mask;
}
void PinWorkerThread(const char* role) {
    const auto mask = WorkerAffinityMask();
    if (mask == 0) return;
    static std::once_flag summary;
    std::call_once(summary, [mask] {
        const auto& layout = CpuTopology::Get();
        std::fprintf(stderr, "[affinity] queue workers and presenter -> 0x%llx (hybrid=%d efficient=0x%llx performant=0x%llx process=0x%llx)\n", static_cast<unsigned long long>(mask), layout.hybrid ? 1 : 0, static_cast<unsigned long long>(layout.efficient), static_cast<unsigned long long>(layout.performant), static_cast<unsigned long long>(layout.process));
    });
    CpuTopology::PinTraced(role, nullptr, mask);
}
bool DeferLabels() {
    // APS5_DRAIN_COMPLETION_LABELS=1 implies the locked path too: a label that must drain then
    // keeps the packet path's unlocked timeline wait instead of a WaitIdle under the mutex, so
    // that switch bisects the same way it did before labels were deferred.
    static const bool defer = std::getenv("APS5_LABEL_LOCK_EACH") == nullptr && std::getenv("APS5_DRAIN_COMPLETION_LABELS") == nullptr;
    return defer;
}
// Queued labels stay queued past the packets that follow them: a packet that records device work
// under its own hold records them first (Driver::recordLabelsForPacket), any other packet records
// them only if the mutex happens to be free, and the deadline still bounds the group. Meanwhile a
// WAIT_REG_MEM of the same queue takes a queued label's value from the label table
// (Recorder::NoteQueuedLabel, entered by flushBetweenPackets right after the label packet), a CPU
// access of a queued label's range by this worker has the group recorded by the flush hook
// (Driver::recordQueuedLabelsFromHook), and a worker polling in a wait records its group as soon
// as the mutex is free (Driver::LabelFlushDue). Before, every packet that could touch memory,
// waits included, took the mutex outright to record the group first: queue 0's first acquisition
// after a run of unlocked packets, absorbing the compute queues' whole critical sections ([lock]
// site 'label'). Debug aid: APS5_NO_QUEUED_LABELS=1 restores the outright record;
// APS5_NO_LABEL_BATCHING=1 restores it together with the other label-batching changes (the
// per-packet submit below and the recorder's store runs).
bool QueuedLabelTable() {
    static const bool enabled = std::getenv("APS5_NO_QUEUED_LABELS") == nullptr && std::getenv("APS5_NO_LABEL_BATCHING") == nullptr;
    return enabled;
}
// A recorded label no longer makes the worker submit the open batch at its next packet: the batch
// goes out at the deadline (APS5_LABEL_FLUSH_US), after APS5_BATCH_CAP dispatches or draws, or when
// a consumer needs it (a wait on its range, a flush-hook sync, the submission's end, a flip). Tens
// of thousands of one-label submissions per 10 s went away with it. Debug aid:
// APS5_NO_LABEL_BATCH_SUBMIT=1 (or APS5_NO_LABEL_BATCHING=1) submits at the next packet as before
// (with the 250 us deadline).
bool LabelBatchSubmit() {
    static const bool enabled = std::getenv("APS5_NO_LABEL_BATCH_SUBMIT") == nullptr && std::getenv("APS5_NO_LABEL_BATCHING") == nullptr;
    return enabled;
}
// How many of this worker's queued labels are in the label table (the group's first entries).
std::size_t& queuedLabelsNoted() {
    static thread_local std::size_t noted = 0;
    return noted;
}
// [labels] line: queued labels recorded from a wait's poll loop and from the flush hook.
std::atomic<std::uint64_t> pollLabelRecords{0}, hookLabelRecords{0};
// Whether a packet needs this worker's deferred labels recorded before it runs: everything except
// pure register and state writes, NOP, and the events the driver does not execute. A wait, dispatch,
// draw, copy, indirect register load, constant RAM dump or flip either reads memory a label may
// write (through the flush hook, which only knows recorded stores) or records device work that must
// follow the label in queue order.
bool NeedsRecordedLabels(std::uint32_t header) {
    if (header == FlipPacketHeader) return true;
    switch ((header >> 8u) & 0xffu) {
        case 0x10: case 0x11: case 0x12: case 0x13: case 0x26: case 0x28: case 0x2a: case 0x2f:
        case 0x42: case 0x46: case 0x58: case 0x68: case 0x69: case 0x76: case 0x78: case 0x79: case 0x7a: case 0x81:
            return false;
        default: return true;
    }
}
// Packets whose own execution takes GuestMemory::GpuMutex and records this worker's deferred labels
// first thing inside it (Driver::recordLabelsForPacket): dispatches (direct and indirect, including
// the fill HLE), draws and the flip. Their unlocked prologue (capture, resource stage A) reads guest
// memory through a flush hook that does not know a still-queued label, so a capture is checked
// against the queued labels once it is known what it read (recordQueuedLabelsAfterCapture: an
// overlap records them and the packet starts over), and a CPU read of dispatch arguments records
// the queue's labels first (recordQueuedLabelsBeforeRead). Stores (COPY_DATA, DMA_DATA,
// DUMP_CONST_RAM) lock too but resolve their source before that lock, so they keep the record
// ahead of them: a copy of a label value this queue just wrote must see it.
bool PacketLocksItself(std::uint32_t header) {
    if (header == FlipPacketHeader) return true;
    switch ((header >> 8u) & 0xffu) {
        case 0x15: case 0x16: case 0x27: case 0x2d: case 0x35: case 0x24: case 0x25: case 0x2c: case 0x38: return true;
        default: return false;
    }
}

class Driver {
public:
    static Driver& Get() {
        static Driver driver;
        return driver;
    }

    ~Driver() {
        stop();
    }

    void Shutdown() {
        stop();
        CheckFailure();
    }

private:
    void stop() {
        require(!OnWorkerThread(), "worker cannot stop itself");
        std::lock_guard shutdownLock(shutdownMutex);
        {
            std::lock_guard lock(mutex);
            stopping = true;
        }
        changed.notify_all();
        // No worker is added once `stopping` is set, so the map is stable while the threads finish.
        for (auto& [queue, worker] : workers) {
            if (worker.thread.joinable()) worker.thread.join();
        }
        std::lock_guard gpuLock(GuestMemory::GpuMutex());
        device.reset();
    }

public:

    void Submit(const Packet* packet, std::uint32_t queue) {
        CheckFailure();
        require(queue == 0 || (queue >= 0x20 && queue < 0x58), "unsupported compute queue");
        GuestMemory::CheckRange(packet, sizeof(Packet), alignof(Packet));
        const auto descriptor = *packet;
        require(descriptor.flags == 0, "nonzero submission flags are not implemented");
        Submission submission{};
        submission.queue = queue;
        static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        auto& costs = submissionCosts(queue);
        const auto start = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        if (descriptor.dw_num != 0) {
            require(descriptor.dw_num <= std::numeric_limits<std::size_t>::max() / sizeof(std::uint32_t), "command size overflow");
            GuestMemory::CheckRange(descriptor.addr, static_cast<std::size_t>(descriptor.dw_num) * sizeof(std::uint32_t), alignof(std::uint32_t));
            submission.commands.assign(descriptor.addr, descriptor.addr + descriptor.dw_num);
        }
        const auto copied = profile ? std::chrono::steady_clock::now() : start;
        validate(submission.commands, queue, descriptor.addr);
        static const bool trace = std::getenv("APS5_TRACE_GPU") != nullptr;
        if (trace) std::fprintf(stderr, "[gpu] %.1f submit queue=0x%x dwords=%zu at %p\n", TraceMs(), queue, submission.commands.size(), static_cast<const void*>(descriptor.addr));
        const auto validated = profile ? std::chrono::steady_clock::now() : start;
        {
            std::lock_guard lock(mutex);
            rethrowFailure();
            require(!stopping, "submission during shutdown");
            require(accepted != std::numeric_limits<std::uint64_t>::max(), "submission serial overflow");
            for (std::size_t cursor = 0; cursor < submission.commands.size();) {
                const auto* words = submission.commands.data() + cursor;
                if (words[0] == RenderingWaitPacketHeader) {
                    const auto output = outputs.find(words[1]);
                    require(output != outputs.end(), "rendering wait references an unregistered video output");
                    auto wait = output->second->CaptureRenderingWait(words[2]);
                    require(wait != nullptr, "video output returned a null rendering wait");
                    submission.renderingWaits.emplace(cursor, std::move(wait));
                }
                if (words[0] == FlipPacketHeader) {
                    const auto output = outputs.find(words[1]);
                    require(output != outputs.end(), "flip references an unregistered video output");
                    const FlipInfo info{words[1], std::bit_cast<std::int32_t>(words[2]), words[3], std::bit_cast<std::int64_t>(static_cast<std::uint64_t>(words[4]) | (static_cast<std::uint64_t>(words[5]) << 32u))};
                    auto request = output->second->Reserve(info);
                    require(request != nullptr, "video output returned a null flip reservation");
                    submission.flips.emplace(cursor, std::move(request));
                }
                cursor += Pm4::PacketWords(words[0]);
            }
            submission.shaders = shaders;
            submission.serial = accepted + 1;
            // Every CPU store the game made before this call precedes the stamp; labels recorded
            // later carry a larger one.
            submission.received = ++eventSerial;
            if (profile) {
                const auto now = std::chrono::steady_clock::now();
                submission.enqueuedAt = now;
                ++costs.submissions;
                costs.validateNs += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(validated - copied).count());
                costs.copyNs += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>((copied - start) + (now - validated)).count());
            }
            enqueue(std::move(submission));
            ++accepted;
        }
        changed.notify_all();
    }

    void WaitIdle() {
        require(!OnWorkerThread(), "worker cannot wait for itself");
        std::unique_lock lock(mutex);
        const auto target = accepted;
        ++idleWaiters;
        changed.wait(lock, [&] { return failure != nullptr || completed >= target; });
        --idleWaiters;
        rethrowFailure();
    }

    void SuspendPoint() {
        require(!OnWorkerThread(), "worker cannot suspend itself");
        std::unique_lock lock(mutex);
        rethrowFailure();
        require(!stopping, "suspend during shutdown");
        require(accepted != std::numeric_limits<std::uint64_t>::max(), "submission serial overflow");
        Submission boundary{};
        boundary.serial = accepted + 1;
        boundary.suspend = true;
        // The boundary resets graphics state, so it runs in order with the graphics queue.
        boundary.queue = 0;
        boundary.enqueuedAt = std::chrono::steady_clock::now();
        enqueue(std::move(boundary));
        ++accepted;
        // The suspend point only marks where the system may suspend the title; it does not wait for
        // the GPU. Blocking here deadlocks frames whose GPU work waits on labels the CPU writes later.
        changed.notify_all();
    }

    void RegisterVideoOutput(std::uint32_t handle, const std::shared_ptr<IVideoOutput>& output) {
        require(output != nullptr, "null video output");
        std::lock_guard lock(mutex);
        rethrowFailure();
        require(!stopping, "video output registration during shutdown");
        require(outputs.emplace(handle, output).second, "video output already registered");
    }

    void UnregisterVideoOutput(std::uint32_t handle, const std::shared_ptr<IVideoOutput>& output) {
        std::lock_guard lock(mutex);
        const auto it = outputs.find(handle);
        require(it != outputs.end() && it->second == output, "video output registration mismatch");
        outputs.erase(it);
    }

    // Called per packet and per poll of a wait: the flag keeps the driver mutex out of those loops
    // until a failure exists, and `failure` itself is still read under the mutex.
    void CheckFailure() {
        if (!failed.load(std::memory_order_acquire)) return;
        std::lock_guard lock(mutex);
        rethrowFailure();
    }

    void ReportFailure(std::exception_ptr error) {
        require(error != nullptr, "null asynchronous failure");
        {
            std::lock_guard lock(mutex);
            if (!failure) failure = error;
            failed.store(true, std::memory_order_release);
            for (const auto& [handle, output] : outputs) output->Fail(failure);
            for (auto& [queue, worker] : workers) {
                for (const auto& item : worker.pending) {
                    for (const auto& [offset, flip] : item.flips) flip->Fail(failure);
                }
                worker.pending.clear();
                worker.queued.store(0, std::memory_order_release);
            }
        }
        changed.notify_all();
    }

    void Present(const PresentationWindow& window, const DisplayBuffer* buffer, bool opaque, void (*gpuReady)(void*), void* context) {
        // The presenter's frame record travels in the window descriptor: PerformanceContext is a
        // thread_local per DLL, so the one the video-out library set is invisible here.
        PerformanceContext timingContext(window.timing.get());
        PerformanceTimer timing("Driver.Present");
        static thread_local bool pinned = false;
        if (!pinned) {
            pinned = true;
            PinWorkerThread("presenter");
            GuestMemory::MarkPresenterThread();
        }
        CheckFailure();
        require(gpuReady != nullptr && context != nullptr, "missing GPU completion callback");
        require(window.getDrawableSize != nullptr, "missing window drawable size query");
        // The device is held only while the presentation is recorded, submitted and queued; the
        // swapchain acquire (where FIFO mode waits for the vblank) and the wait for older
        // presentations happen outside GpuMutex so the queue workers are not blocked for the
        // present's GPU or display time. The flip completes once the blit is queued behind the
        // frame's batches on the one VkQueue (queue order makes the displayed image complete);
        // the CPU waits only when more than APS5_FLIP_INFLIGHT presentations trail on the GPU
        // (VulkanDevice::RetirePresents). APS5_FLIP_INFLIGHT=0 waits for the frame's own fence
        // before completing the flip, as before; APS5_SYNC_FLIP=1 also drains the device and
        // presents synchronously under one mutex hold.
        static const bool syncFlip = std::getenv("APS5_SYNC_FLIP") != nullptr;
        static const std::size_t inFlight = VulkanDevice::FlipInFlight();
        static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        std::shared_ptr<VulkanDevice> presenting;
        timing.Mark("validate");
        try {
            bool submitted = false;
            bool presentable = false;
            double waitedMs = 0;
            {
                GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Present);
                std::lock_guard lock(GuestMemory::GpuMutex());
                timing.Mark("gpu_mutex_wait");
                if (device == nullptr || device->Window() == nullptr) {
                    if (device) {
                        device->PrepareForReplacement();
                        static std::vector<std::shared_ptr<VulkanDevice>> replacedDevices;
                        replacedDevices.push_back(device);
                    }
                    device = std::make_shared<VulkanDevice>(&window);
                }
                require(device->Window() == window.context, "presentation window does not match device surface");
                presenting = device;
                timing.Mark("device_setup");
                std::uint32_t drawableWidth = 0;
                std::uint32_t drawableHeight = 0;
                window.getDrawableSize(window.context, &drawableWidth, &drawableHeight);
                presenting->Resize(drawableWidth, drawableHeight);
                timing.Mark("resize");
                presentable = presenting->Presentable();
                if (buffer != nullptr) require(buffer->width == window.width && buffer->height == window.height, "display buffer extent differs from output");
            }
            // The swapchain, its fences and semaphores are the presenter's own: only the VkQueue needs
            // the mutex, so the older presentations' retirement and the acquire (with its vblank
            // wait) are taken first without it.
            if (presentable && !syncFlip) {
                if (inFlight != 0) {
                    // A present through the single scaler/staging objects (a frame dump, the
                    // guest-memory path) waits for every slot: here, not under the mutex.
                    waitedMs = presenting->RetirePresents(presenting->PresentWaitsForSlots(buffer) ? 0 : inFlight);
                    timing.Mark("inflight_wait");
                }
                presentable = presenting->AcquireImage();
                timing.Mark("acquire_image");
            }
            if (presentable) {
                GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Present);
                std::lock_guard lock(GuestMemory::GpuMutex());
                timing.Mark("gpu_mutex_wait");
                if (buffer != nullptr) {
                    if (syncFlip) {
                        presenting->WaitIdle();
                        timing.Mark("device_idle_wait");
                    }
                    submitted = presenting->PresentDisplayBuffer(*buffer);
                    timing.Mark("present_display_buffer");
                } else {
                    submitted = presenting->PresentClear(window.width, window.height, opaque);
                    timing.Mark("present_clear");
                }
                if (submitted && syncFlip) {
                    waitedMs = presenting->FinishPresent();
                    timing.Mark("render_fence_wait");
                }
                if (submitted && (syncFlip || inFlight != 0)) {
                    // vkQueuePresentKHR only waits GPU-side on the blit's semaphore.
                    presenting->QueuePresent();
                    timing.Mark("queue_present");
                    submitted = false;
                }
            }
            if (submitted) {
                waitedMs = presenting->FinishPresent();
                timing.Mark("render_fence_wait");
                // The single VkQueue is shared with the recorder: presenting on it needs the mutex.
                GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Present);
                std::lock_guard lock(GuestMemory::GpuMutex());
                presenting->QueuePresent();
                timing.Mark("queue_present");
            }
            gpuReady(context);
            timing.Mark("release_and_callback");
            if (profile) reportPresents(waitedMs, inFlight);
            CheckFailure();
        } catch (...) {
            ReportFailure(std::current_exception());
            throw;
        }
    }

    // The [present] line every 10 s (APS5_PROFILE_DRAW, presenter thread): presentations and the
    // p50/p90 of the presenter's one CPU wait (inflight_wait, or render_fence_wait with
    // APS5_FLIP_INFLIGHT=0), the GPU's busy time and idle gaps over the batches the retired blits
    // waited for, the batches per flip (the recorder serial's advance over the window's flips) and
    // per present (ahead of the blit since the flip packet, behind it), and the in-place read check.
    void reportPresents(double waitedMs, std::size_t inFlight) {
        static std::vector<double> waits;
        static VulkanDevice::PresentStatistics previous{};
        static std::uint64_t previousFlips = 0, windowSerial = 0, previousUnsignaled = 0;
        static auto lastReport = std::chrono::steady_clock::now();
        waits.push_back(waitedMs);
        if (waits.size() == 1) windowSerial = flipSerial.load();
        const auto now = std::chrono::steady_clock::now();
        if (now - lastReport < std::chrono::seconds(10)) return;
        lastReport = now;
        std::sort(waits.begin(), waits.end());
        const auto percentile = [&](double fraction) { return waits.empty() ? 0.0 : waits[std::min(waits.size() - 1, static_cast<std::size_t>(fraction * static_cast<double>(waits.size())))]; };
        const auto counts = VulkanDevice::PresentCounts();
        const auto presents = counts.presents - previous.presents;
        const double perPresent = presents != 0 ? 1.0 / static_cast<double>(presents) : 0.0;
        const auto flips = flipsCounted.load() - previousFlips;
        const double perFlip = flips != 0 ? 1.0 / static_cast<double>(flips) : 0.0;
        const auto serial = flipSerial.load();
        std::fprintf(stderr, "[present] %llu presents over 10 s (%zu may trail on the GPU); %s p50 %.2f ms, p90 %.2f ms, max %.2f ms over %zu; per present: GPU busy %.1f ms, idle gaps %.1f ms (%llu batches without a completion record); per flip: %.1f batches recorded (%.1f unsignaled at the flip); per present: %.1f batches ahead of the blit, %.1f after it; after the flip packet: last submit %.1f ms, blit submit %.1f ms; in-place reads overwritten before execution: %llu of %llu checked\n", static_cast<unsigned long long>(presents), inFlight, inFlight != 0 ? "inflight_wait" : "render_fence_wait", percentile(0.5), percentile(0.9), waits.empty() ? 0.0 : waits.back(), waits.size(), (counts.gpuBusyMs - previous.gpuBusyMs) * perPresent, (counts.gpuGapMs - previous.gpuGapMs) * perPresent, static_cast<unsigned long long>(counts.gpuUnread - previous.gpuUnread), static_cast<double>(serial - windowSerial) * perFlip, static_cast<double>(flipBatchesUnsignaled.load() - previousUnsignaled) * perFlip, static_cast<double>(counts.batchesAheadOfBlit - previous.batchesAheadOfBlit) * perPresent, static_cast<double>(counts.batchesAfterFlip - previous.batchesAfterFlip) * perPresent, (counts.lastSubmitAfterFlipMs - previous.lastSubmitAfterFlipMs) * perPresent, (counts.blitSubmitAfterFlipMs - previous.blitSubmitAfterFlipMs) * perPresent, static_cast<unsigned long long>(counts.readsOverwritten - previous.readsOverwritten), static_cast<unsigned long long>(counts.readsChecked - previous.readsChecked));
        previous = counts;
        previousFlips = flipsCounted.load();
        previousUnsignaled = flipBatchesUnsignaled.load();
        waits.clear();
    }

    void ReleaseWindow(void* window) {
        std::lock_guard lock(GuestMemory::GpuMutex());
        if (device && device->Window() == window) device.reset();
    }

    void RegisterShader(const Shader* shader) {
        CheckFailure();
        GuestMemory::CheckRange(shader, sizeof(Shader), alignof(Shader));
        require(shader->file_header == 0x34333231u && shader->version == 0x18u, "invalid shader header");
        require(shader->header_size >= sizeof(Shader), "shader header is smaller than its fixed fields");
        require(shader->shader_size != 0 && (shader->shader_size & 3u) == 0, "invalid shader size");
        GuestMemory::CheckRange(shader, shader->header_size, alignof(Shader));
        const auto* code = const_cast<const void*>(shader->code);
        GuestMemory::CheckRange(code, shader->shader_size, 256);
        ShaderSnapshot snapshot{reinterpret_cast<std::uintptr_t>(code), reinterpret_cast<std::uintptr_t>(shader), shader->type, {}, {}};
        snapshot.code.resize(shader->shader_size / sizeof(std::uint32_t));
        std::memcpy(snapshot.code.data(), code, shader->shader_size);
        snapshot.header.resize(shader->header_size);
        std::memcpy(snapshot.header.data(), shader, shader->header_size);
        // Debug aid: APS5_TRACE_SHADER_REGS=<hex code address> (or "all") prints a shader's register lists.
        static const char* traceRegs = std::getenv("APS5_TRACE_SHADER_REGS");
        if (traceRegs != nullptr && (std::string(traceRegs) == "all" || std::strtoull(traceRegs, nullptr, 16) == snapshot.codeAddress)) {
            std::fprintf(stderr, "[shader] 0x%llx type %u cx", static_cast<unsigned long long>(snapshot.codeAddress), shader->type);
            for (std::uint32_t i = 0; i < shader->num_cx_registers && shader->cx_registers != nullptr; ++i) std::fprintf(stderr, " %x=%08x", shader->cx_registers[i].offset, shader->cx_registers[i].value);
            std::fprintf(stderr, " sh");
            for (std::uint32_t i = 0; i < shader->num_sh_registers && shader->sh_registers != nullptr; ++i) std::fprintf(stderr, " %x=%08x", shader->sh_registers[i].offset, shader->sh_registers[i].value);
            std::fprintf(stderr, "\n");
        }
        std::lock_guard lock(mutex);
        rethrowFailure();
        const auto address = snapshot.codeAddress;
        // Submissions hold the registry by reference (a copy per submission cost a map clone):
        // one they still reference is replaced by a copy, one only the driver holds (registration
        // bursts at load time) is updated in place.
        if (shaders == nullptr) shaders = std::make_shared<ShaderRegistry>();
        else if (shaders.use_count() != 1) shaders = std::make_shared<ShaderRegistry>(*shaders);
        shaders->insert_or_assign(address, std::make_shared<const ShaderSnapshot>(std::move(snapshot)));
    }

private:
    std::mutex mutex;
    std::mutex shutdownMutex;
    std::condition_variable changed;
    // Each queue runs on its own thread, so a WAIT_REG_MEM blocks only its own queue as on the GPU.
    // Draws and dispatches take GuestMemory::GpuMutex, so device work stays serialized.
    struct QueueWorker {
        std::deque<Submission> pending;
        // pending.size(), readable without `mutex` (the worker's submission-end decision).
        std::atomic<std::uint64_t> queued{0};
        std::thread thread;
    };
    std::map<std::uint32_t, QueueWorker> workers;
    std::uint64_t frameSerial = 0;
    // Flip packets seen, the recorder serial at the latest one that sampled it (0 until then; the
    // drain paths do not) and, summed over the flips, the batches not signaled then (the [present]
    // line; written by the queue-0 worker, read by the presenter).
    std::atomic<std::uint64_t> flipsCounted{0};
    std::atomic<std::uint64_t> flipSerial{0};
    std::atomic<std::uint64_t> flipBatchesUnsignaled{0};
    // Packets executing now and packets finished, across all queues; waits time out only when neither
    // moves. On a cache line of their own: the waiting queues poll them while the others update them.
    alignas(64) std::atomic<int> packetsInFlight{0};
    std::atomic<std::uint64_t> packetsDone{0};
    // Orders the game's submit calls against the labels the workers record (Submission::received).
    std::atomic<std::uint64_t> eventSerial{0};
    alignas(64) std::shared_ptr<ShaderRegistry> shaders = std::make_shared<ShaderRegistry>();
    // Dispatch cache: a program with the same user data and shader registers whose captured memory
    // (SRT chains, descriptors) is unchanged reuses its capture and recompile result. Guarded by
    // dispatchCacheMutex; entries are evicted least recently used. An entry is validated by value:
    // the dwords its capture read outside the registered code and header (the key names that
    // registration, so those bytes are never compared) are compared in place with guest memory
    // under the coherence gate of validateEntry, and nothing is stamped or collected on a hit.
    // Debug aid: APS5_DISPATCH_STAMP_VALIDATE=1 validates by write-watch stamps over merged spans
    // and EqualsCommitted per captured region as before (the entry then keeps the capture's pages).
    // A key holds up to DispatchVariants() value sets (a variant each, most recently hit first):
    // a program whose SRT alternates between a few value sets hits the variant that matches
    // instead of recapturing. A variant is immutable once inserted (a re-stamp is an atomic
    // store); an entry is replaced whole under dispatchCacheMutex, so a validator holding the old
    // one validates a consistent object.
    struct DispatchVariant {
        // The captured dwords outside the code and header: [begin, end) byte ranges in address
        // order, and their values in that order.
        std::vector<std::pair<std::uint64_t, std::uint64_t>> runs;
        std::vector<std::uint32_t> words;
        std::uint64_t forgetSerial = 0;
        std::shared_ptr<const ShaderRecompiler::RecompileResult> compiled;
        // The registered shader whose code and header the capture references.
        std::shared_ptr<const ShaderSnapshot> shader;
        // The dispatch recipe (design_cpu_final M4), null until the first successful device call
        // of a reusable object for this variant; stored atomically, read by every later hit
        // outside dispatchCacheMutex.
        std::atomic<std::shared_ptr<const Recipe>> recipe;
        // Data-only hits: the positions in `words` of the pure flat-SRT leaves the capture read
        // (sorted; DataWordPositions), the FlattenedSrt binding offset each one feeds, and that
        // binding's index in `compiled->bindings`. Empty: the compare is exact, as for draws.
        static constexpr std::uint32_t NoFlatBinding = std::numeric_limits<std::uint32_t>::max();
        std::vector<std::uint32_t> dataPositions;
        std::vector<std::uint32_t> dataSlots;
        std::uint32_t flatBinding = NoFlatBinding;
        // Draw entries only: the push constant offset the stage was compiled at (the earlier
        // stages' push sizes), which a hit must reproduce, and the vertex stage info the stage
        // was compiled with (null for a fragment stage): DecodeVertexStageInfo's guest reads are
        // among the runs, so a hit reproduces the info without the decode (design_cpu_final 8a).
        std::uint32_t pushOffset = 0;
        std::shared_ptr<const ShaderRecompiler::ShaderVertexStageInfo> vertexInfo;
        // Stamp validation only: the capture's pages, every captured region (code and header
        // included), the watched spans and the generation they were last found unchanged at.
        std::shared_ptr<ShaderMemory> memory;
        std::vector<ShaderRecompiler::MemoryRegion> captured;
        std::vector<std::pair<std::uint64_t, std::size_t>> spans;
        std::atomic<std::uint64_t> generation{0};
    };
    struct DispatchEntry {
        std::vector<std::shared_ptr<DispatchVariant>> variants;
        // The hit count when this entry last moved to the front of dispatchOrder (dispatchCacheMutex).
        std::uint64_t touched = 0;
        // This entry's position in dispatchOrder.
        std::list<std::uint64_t>::iterator order;
    };
    static bool StampValidate() {
        static const bool stamp = std::getenv("APS5_DISPATCH_STAMP_VALIDATE") != nullptr;
        return stamp;
    }
    // Data-only hits: a variant whose words differ from guest memory only at pure flat-SRT
    // leaves (IrResourcePlan::pureFlatSlots: words the CPU walk never consumes) is a hit whose
    // compiled result takes the live words and whose template refreshes its data buffers, so
    // per-frame constants no longer cost a capture, a recompile and an insert. Debug aids:
    // APS5_NO_DATA_HITS=1 keeps every compare exact; APS5_VERIFY_DATA_HITS=1 captures beside
    // every data hit and aborts when the read set, its bytes, the variant or the bindings differ.
    // Off with APS5_NO_TEMPLATE_DATA_REFRESH=1 too: a hit's live words reach the GPU only through
    // the template refresh (a recipe's template is never re-keyed).
    static bool DataHits() {
        static const bool hits = std::getenv("APS5_NO_DATA_HITS") == nullptr && VulkanDevice::TemplateDataRefresh();
        return hits;
    }
    static bool VerifyDataHits() {
        static const bool verify = std::getenv("APS5_VERIFY_DATA_HITS") != nullptr;
        return verify;
    }
    static constexpr std::size_t MaxDispatchVariants = 8;
    // APS5_DISPATCH_VARIANTS=<k>: value sets kept per key (default 4, at most
    // MaxDispatchVariants; 1 = one per key as before). Stamp validation keeps one.
    static std::size_t DispatchVariants() {
        static const std::size_t variants = [] {
            if (StampValidate()) return std::size_t{1};
            const char* text = std::getenv("APS5_DISPATCH_VARIANTS");
            const auto parsed = text != nullptr ? std::strtoull(text, nullptr, 10) : 4ull;
            return static_cast<std::size_t>(std::clamp<unsigned long long>(parsed, 1, MaxDispatchVariants));
        }();
        return variants;
    }
    // APS5_INSERT_COMPARE=1: a fresh entry's captured words are compared with guest memory before
    // the insert (captureStable) as before. By default a capture is kept as read: a CPU write
    // racing it is used once, as hardware would, and the value compare catches it at the next use.
    static bool InsertCompare() {
        static const bool compare = std::getenv("APS5_INSERT_COMPARE") != nullptr;
        return compare;
    }
    // 16384 entries by value (about 1 KB per variant; 8192 with more than two variants per key),
    // 4096 by stamps (two 4 KiB pages each); APS5_DISPATCH_CACHE_ENTRIES=<n> overrides either.
    static std::size_t DispatchCacheEntries() {
        static const std::size_t entries = [] {
            const char* text = std::getenv("APS5_DISPATCH_CACHE_ENTRIES");
            const auto parsed = text != nullptr ? std::strtoull(text, nullptr, 10) : 0ull;
            if (parsed != 0) return static_cast<std::size_t>(parsed);
            if (StampValidate()) return std::size_t{4096};
            return DispatchVariants() > 2 ? std::size_t{8192} : std::size_t{16384};
        }();
        return entries;
    }
    std::unordered_map<std::uint64_t, std::shared_ptr<DispatchEntry>> dispatchCache;
    // Keys most recently used first.
    std::list<std::uint64_t> dispatchOrder;
    std::mutex dispatchCacheMutex;
    std::uint64_t dispatchCacheHits = 0;
    std::uint64_t dispatchCacheEvictions = 0;
    // Variants held over every entry and their estimated bytes (under dispatchCacheMutex).
    std::uint64_t dispatchCacheVariants = 0;
    std::uint64_t dispatchCacheVariantBytes = 0;
    static std::uint64_t variantBytes(const DispatchVariant& variant) {
        return sizeof(DispatchVariant) + variant.words.size() * sizeof(std::uint32_t) + variant.runs.size() * sizeof(std::pair<std::uint64_t, std::uint64_t>) + variant.captured.size() * sizeof(ShaderRecompiler::MemoryRegion) + (variant.vertexInfo != nullptr ? sizeof(ShaderRecompiler::ShaderVertexStageInfo) : 0);
    }
    void accountVariant(const DispatchVariant& variant, bool added) {
        if (added) {
            ++dispatchCacheVariants;
            dispatchCacheVariantBytes += variantBytes(variant);
        } else {
            --dispatchCacheVariants;
            dispatchCacheVariantBytes -= variantBytes(variant);
        }
    }
    void eraseDispatchEntry(std::unordered_map<std::uint64_t, std::shared_ptr<DispatchEntry>>::iterator it) {
        for (const auto& variant : it->second->variants) accountVariant(*variant, false);
        dispatchOrder.erase(it->second->order);
        dispatchCache.erase(it);
    }
    // Lookup outcomes for the [dispatch-cache] line (APS5_PROFILE_DRAW, every 10 s); under
    // dispatchCacheMutex.
    struct EntryCounters {
        std::uint64_t lookups = 0, absent = 0, equal = 0, differing = 0, inaccessible = 0, queuedLabel = 0, flushingImage = 0, publishMoved = 0, pendingMoved = 0, forgetMoved = 0, imagesFlushed = 0, runsSynced = 0, forgetSinceInsert = 0, replaced = 0, inserts = 0, unstable = 0, touches = 0;
        // Runs (compared spans) over the validated entries and over the inserted ones, and the
        // gate retries after a moved serial by their outcome.
        std::uint64_t runsValidated = 0, runsInserted = 0, retriesEqual = 0, retriesMoved = 0;
        double validateUs = 0;
        // The 'differing' misses whose fresh capture was compared with the entry it replaced
        // (classifyDiffering): by class of the differing dwords (every one a V#/T# base word of the
        // fresh capture: address; every one a flattened-SRT word: data; none: walk; else mixed;
        // the walk read other addresses: runs changed), those whose fresh words equal one of the
        // key's last three replaced value sets, the differing dwords and their positions.
        std::uint64_t differingClassified = 0, differingAddress = 0, differingData = 0, differingWalk = 0, differingMixed = 0, differingRunsChanged = 0, differingMatchedPrior = 0, differingWords = 0;
        std::array<std::uint64_t, 4> differingWordBuckets{};
        // Variants: hits by the rank of the matching variant (front first), variants compared over
        // the validations, variants inserted into an existing entry and evicted beyond the limit.
        std::array<std::uint64_t, MaxDispatchVariants> variantHitsByRank{};
        std::uint64_t variantsCompared = 0, variantsInserted = 0, variantsEvicted = 0;
        // Data-only hits: hits, words refreshed, by rank, verified (APS5_VERIFY_DATA_HITS);
        // inserts with data positions, positions inserted, leaves skipped by reason.
        std::uint64_t dataHits = 0, dataWordsRefreshed = 0, dataVerified = 0, dataInserts = 0, dataPositionsInserted = 0, dataLeavesUnmapped = 0, dataLeavesMismatched = 0, dataLeavesAliased = 0;
        std::array<std::uint64_t, MaxDispatchVariants> dataHitsByRank{};
        std::set<std::size_t> differingPositions;
        std::size_t differingFirstPosition = std::numeric_limits<std::size_t>::max(), differingLastPosition = 0;
        std::map<std::uint64_t, std::uint64_t> differingByProgram;
        std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
    };
    EntryCounters entryCounters;
    // Misses of variants with data positions on a pending, known-value or stamp-compared run (the
    // exact compare there): the data hits the pending-write policy left on the table.
    std::atomic<std::uint64_t> dataPendingMisses{0};
    // Per key, the runs and words of the last three entries a 'differing' miss replaced
    // (APS5_PROFILE_DRAW only; under dispatchCacheMutex): whether a variant ring would have held
    // the fresh values.
    using ValueSet = std::pair<std::vector<std::pair<std::uint64_t, std::uint64_t>>, std::vector<std::uint32_t>>;
    std::unordered_map<std::uint64_t, std::deque<ValueSet>> priorValueSets;
    // Draw cache (design_cpu_final M7): per draw key one variant list per compiled program, each
    // variant a DispatchVariant validated by value over its own runs exactly as a dispatch's is,
    // so a hit needs every stage equal and skips both captures and both recompiles (rule RD: R0-R3
    // per stage, the queued-label test and the writer notes over the union). The key (8a, the
    // register key) is built before any decode from the register banks and the registry alone:
    // every present register of Graphics::DrawKeyRegisters with its offset, the registration
    // identities and code offsets the program address registers name, and the device serial. The
    // decoded state, pixel stage info, programs and roles are pure functions of it and are kept
    // with the entry (DrawDecode), so a hit skips DecodeState, DecodeShaderStages,
    // DecodePixelStageInfo and the program prepare; the vertex stage infos read attribute and V#
    // tables from guest memory, so each stage variant keeps the info it was compiled with and its
    // reads among its value-validated runs. Under APS5_NO_DRAW_KEY=1 the key is M7's (the decoded
    // bytes hashed after the decode, which then runs on hits too) and no decode is kept. Guarded by
    // drawCacheMutex; entries are immutable and replaced whole, least recently used first out. The
    // CPU path of an indirect draw (patched user words) bypasses it. Debug aids:
    // APS5_NO_DRAW_SRT_ENTRIES=1 captures every draw as before; APS5_VERIFY_DRAW_ENTRIES=1
    // captures on every hit as well and counts a stage whose fresh capture differs from the
    // matched variant (the draw then uses the fresh results); APS5_VERIFY_DRAW_RECIPE=1 decodes
    // on every register-key hit and compares the state, pixel and vertex infos, programs and
    // roles with the entry's (the draw then uses the fresh decode), and on every miss checks the
    // decoders' register facade log against the table; APS5_DRAW_CACHE_ENTRIES=<n> is the entry
    // budget (default 4096). Stamp validation (APS5_DISPATCH_STAMP_VALIDATE) has no draw form and
    // disables the entries.
    struct DrawProgram {
        ShaderRecompiler::ShaderBinary binary;
        std::uint32_t userDataBase;
        std::uint32_t firstUserSgpr = 8;
        std::vector<std::uint32_t> userData;
        std::array<ShaderRecompiler::MemoryRegion, 2> memory;
        // The registered shader the binary is a program of, and the program's offset in it.
        std::shared_ptr<const ShaderSnapshot> snapshot;
        std::size_t codeOffset = 0;
    };
    // The products of a draw's decode that the register key fixes.
    struct DrawDecode {
        Graphics::State state;
        ShaderRecompiler::ShaderPixelStageInfo pixel;
        std::vector<DrawProgram> programs;
        std::vector<ShaderRecompiler::ProgramRole> roles;
    };
    // A draw recipe (8b) with the stage variants (by identity, per program index) whose compiled
    // results it was recorded with: valid for a hit that matched exactly those. The identity is
    // the variant's control block (weak: a variant evicted beyond k dies while its record may
    // still be listed, and a fresh variant may reuse its address; a weak reference keeps the
    // block, so an evicted variant never matches again).
    struct DrawRecipeRecord {
        std::vector<std::weak_ptr<const DispatchVariant>> stages;
        std::shared_ptr<const DrawRecipe> recipe;
        bool Matches(const std::vector<std::shared_ptr<DispatchVariant>>& variants) const {
            if (stages.size() != variants.size()) return false;
            for (std::size_t i = 0; i < stages.size(); ++i) {
                if (stages[i].owner_before(variants[i]) || variants[i].owner_before(stages[i])) return false;
            }
            return true;
        }
        bool Expired() const {
            return std::any_of(stages.begin(), stages.end(), [](const std::weak_ptr<const DispatchVariant>& stage) { return stage.expired(); });
        }
    };
    struct DrawEntry {
        // The decode the register key fixes (null under the decoded-bytes key).
        std::shared_ptr<const DrawDecode> decode;
        // Per program index of the draw (a GeometryBack slot stays empty), most recently hit first.
        std::vector<std::vector<std::shared_ptr<DispatchVariant>>> stages;
        // The recipes attached to this key's matched variant sets, newest first (an immutable
        // list replaced whole; carried over when the entry is replaced).
        std::atomic<std::shared_ptr<const std::vector<DrawRecipeRecord>>> recipes;
        std::uint64_t touched = 0;
        std::list<std::uint64_t>::iterator order;
    };
    static bool DrawEntries() {
        static const bool entries = std::getenv("APS5_NO_DRAW_SRT_ENTRIES") == nullptr && !StampValidate();
        return entries;
    }
    static bool VerifyDrawEntries() {
        static const bool verify = std::getenv("APS5_VERIFY_DRAW_ENTRIES") != nullptr;
        return verify;
    }
    static bool RegisterKey() {
        static const bool registerKey = std::getenv("APS5_NO_DRAW_KEY") == nullptr;
        return registerKey;
    }
    static bool VerifyDrawRecipe() {
        static const bool verify = std::getenv("APS5_VERIFY_DRAW_RECIPE") != nullptr;
        return verify;
    }
    static std::size_t DrawCacheEntries() {
        static const std::size_t entries = [] {
            const char* text = std::getenv("APS5_DRAW_CACHE_ENTRIES");
            const auto parsed = text != nullptr ? std::strtoull(text, nullptr, 10) : 0ull;
            return parsed != 0 ? static_cast<std::size_t>(parsed) : std::size_t{4096};
        }();
        return entries;
    }
    std::unordered_map<std::uint64_t, std::shared_ptr<DrawEntry>> drawCache;
    std::list<std::uint64_t> drawOrder;
    std::mutex drawCacheMutex;
    std::uint64_t drawCacheHits = 0, drawCacheEvictions = 0, drawCacheVariants = 0, drawCacheVariantBytes = 0;
    // Why a validated draw entry missed, by the first stage in program order without a match:
    // its front variant differing (the front program, the fragment program, another stage), no
    // variant compiled at the current push offset (layout), a gate outcome (page not mapped,
    // queued label, image being stored, a serial moved), a stage count unlike the key's.
    enum class DrawMiss : std::size_t { FrontDiffering, FragmentDiffering, OtherDiffering, Layout, Gate, Stages, Count };
    struct DrawEntryCounters {
        std::uint64_t lookups = 0, absent = 0, hits = 0, stageValidations = 0, stageEqual = 0, variantsCompared = 0, inserts = 0, variantsInserted = 0, variantsEvicted = 0, present = 0, unstable = 0, touches = 0, verifyHits = 0, verifyMismatches = 0;
        std::array<std::uint64_t, static_cast<std::size_t>(DrawMiss::Count)> misses{};
        std::array<std::uint64_t, MaxDispatchVariants> variantHitsByRank{};
        double validateUs = 0;
        // The register key (8a): lookups made with it, hits (every stage equal), lookups whose
        // entry carried the decode (skipped: the whole decode; partial: the state, pixel and
        // programs of a miss), the key builds' time, and the verify counts: decoders' reads the
        // table lacks ('facade log != table', must be 0) and decodes that differed from the
        // entry's (must be 0).
        std::uint64_t registerKeyLookups = 0, registerKeyHits = 0, decodeSkipped = 0, decodePartial = 0, facadeMismatches = 0, verifyDecodes = 0, verifyDecodeMismatches = 0;
        double keyUs = 0;
        std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
    };
    DrawEntryCounters drawEntryCounters;
    void reportDrawCache(DrawEntryCounters& counters) {
        const auto count = [](std::uint64_t value) { return static_cast<unsigned long long>(value); };
        const auto validated = counters.lookups - counters.absent;
        const auto miss = [&](DrawMiss reason) { return count(counters.misses[static_cast<std::size_t>(reason)]); };
        std::string ranks;
        for (std::size_t rank = 0; rank < DispatchVariants(); ++rank) {
            char text[32];
            std::snprintf(text, sizeof(text), "%s%llu", rank == 0 ? "" : " / ", count(counters.variantHitsByRank[rank]));
            ranks += text;
        }
        std::fprintf(stderr, "[draw-cache] %llu lookups (10 s): %llu no entry, %llu validated in %.1f us each (key, lookup and every stage): %llu hits (every stage equal; stage hits by variant rank 1..k %s), misses by reason: front stage differing %llu, fragment differing %llu, other stage differing %llu, layout (push offset) %llu, gate %llu, stage count %llu; %llu stage validations (%llu equal, %.2f variants compared each); %llu inserts (%llu variants inserted, %llu evicted beyond k, %llu already present, %llu unstable), %zu entries (%llu variants, ~%.1f MiB), %llu evictions in total, %llu LRU moves; verify: %llu hits captured again, %llu stages differed\n", count(counters.lookups), count(counters.absent), count(validated), validated != 0 ? counters.validateUs / static_cast<double>(validated) : 0.0, count(counters.hits), ranks.c_str(), miss(DrawMiss::FrontDiffering), miss(DrawMiss::FragmentDiffering), miss(DrawMiss::OtherDiffering), miss(DrawMiss::Layout), miss(DrawMiss::Gate), miss(DrawMiss::Stages), count(counters.stageValidations), count(counters.stageEqual), counters.stageValidations != 0 ? static_cast<double>(counters.variantsCompared) / static_cast<double>(counters.stageValidations) : 0.0, count(counters.inserts), count(counters.variantsInserted), count(counters.variantsEvicted), count(counters.present), count(counters.unstable), drawCache.size(), count(drawCacheVariants), static_cast<double>(drawCacheVariantBytes) / (1024.0 * 1024.0), count(drawCacheEvictions), count(counters.touches), count(counters.verifyHits), count(counters.verifyMismatches));
        std::fprintf(stderr, "[draw-cache] register key (10 s): %llu lookups, %llu hits, key build %.1f us each; decode skipped %llu, partial (state/pixel/programs from the entry) %llu; facade log != table %llu; verify: %llu decodes compared, %llu differed\n", count(counters.registerKeyLookups), count(counters.registerKeyHits), counters.registerKeyLookups != 0 ? counters.keyUs / static_cast<double>(counters.registerKeyLookups) : 0.0, count(counters.decodeSkipped), count(counters.decodePartial), count(counters.facadeMismatches), count(counters.verifyDecodes), count(counters.verifyDecodeMismatches));
        counters = DrawEntryCounters{};
    }
    void accountDrawVariant(const DispatchVariant& variant, bool added) {
        if (added) {
            ++drawCacheVariants;
            drawCacheVariantBytes += variantBytes(variant);
        } else {
            --drawCacheVariants;
            drawCacheVariantBytes -= variantBytes(variant);
        }
    }
    // A miss's fresh variants (per program index, null where nothing was compiled) go in front of
    // the key's stage lists, each list trimmed to DispatchVariants(); a variant equal to one the
    // list holds (another worker's insert, a stage that had matched) is not inserted again, and
    // `fresh[i]` then becomes the kept one (the identity a recipe is attached to). `decode` is
    // kept with the entry under the register key (an entry that has one keeps its own: equal by
    // construction).
    void insertDrawEntry(std::uint64_t key, std::vector<std::shared_ptr<DispatchVariant>>& fresh, std::shared_ptr<const DrawDecode> decode) {
        std::lock_guard cacheLock(drawCacheMutex);
        auto& counters = drawEntryCounters;
        ++counters.inserts;
        const auto found = drawCache.find(key);
        auto replacement = std::make_shared<DrawEntry>();
        replacement->stages.resize(fresh.size());
        replacement->decode = std::move(decode);
        if (found != drawCache.end()) {
            if (found->second->stages.size() == fresh.size()) replacement->stages = found->second->stages;
            if (found->second->decode != nullptr) replacement->decode = found->second->decode;
            replacement->recipes.store(found->second->recipes.load());
        }
        for (std::size_t i = 0; i < fresh.size(); ++i) {
            if (fresh[i] == nullptr) continue;
            auto& variants = replacement->stages[i];
            const auto present = std::find_if(variants.begin(), variants.end(), [&](const std::shared_ptr<DispatchVariant>& kept) { return kept->pushOffset == fresh[i]->pushOffset && kept->runs == fresh[i]->runs && kept->words == fresh[i]->words; });
            if (present != variants.end()) {
                ++counters.present;
                fresh[i] = *present;
                continue;
            }
            accountDrawVariant(*fresh[i], true);
            variants.insert(variants.begin(), fresh[i]);
            ++counters.variantsInserted;
            while (variants.size() > DispatchVariants()) {
                accountDrawVariant(*variants.back(), false);
                variants.pop_back();
                ++counters.variantsEvicted;
            }
        }
        replacement->touched = drawCacheHits;
        if (found == drawCache.end()) {
            drawOrder.push_front(key);
            replacement->order = drawOrder.begin();
            drawCache.emplace(key, std::move(replacement));
        } else {
            replacement->order = found->second->order;
            drawOrder.splice(drawOrder.begin(), drawOrder, replacement->order);
            found->second = std::move(replacement);
        }
        while (drawCache.size() > DrawCacheEntries()) {
            const auto last = drawCache.find(drawOrder.back());
            for (const auto& variants : last->second->stages) {
                for (const auto& variant : variants) accountDrawVariant(*variant, false);
            }
            drawOrder.erase(last->second->order);
            drawCache.erase(last);
            ++drawCacheEvictions;
        }
    }
    // The recipe a draw hit that matched exactly `stages` (by identity) can record from, if one
    // was attached to the key's current entry; null otherwise.
    std::shared_ptr<const DrawRecipe> findDrawRecipe(std::uint64_t key, const std::vector<std::shared_ptr<DispatchVariant>>& stages) {
        std::shared_ptr<const std::vector<DrawRecipeRecord>> records;
        {
            std::lock_guard cacheLock(drawCacheMutex);
            const auto found = drawCache.find(key);
            if (found == drawCache.end()) return nullptr;
            records = found->second->recipes.load();
        }
        if (records == nullptr) return nullptr;
        for (const auto& record : *records) {
            if (record.Matches(stages)) return record.recipe;
        }
        return nullptr;
    }
    // Attaches (or replaces) the recipe of the matched variant set on the key's current entry: a
    // fresh immutable list, newest first, at most DispatchVariants() records (an entry's stage
    // lists hold that many variants each), records of an evicted variant dropped. A racing attach
    // may lose one record: a recipe is a cache, rebuilt at the next miss.
    void attachDrawRecipe(std::uint64_t key, const std::vector<std::shared_ptr<DispatchVariant>>& stages, std::shared_ptr<const DrawRecipe> recipe) {
        std::shared_ptr<DrawEntry> entry;
        {
            std::lock_guard cacheLock(drawCacheMutex);
            const auto found = drawCache.find(key);
            if (found == drawCache.end()) return;
            entry = found->second;
        }
        const auto old = entry->recipes.load();
        auto records = std::make_shared<std::vector<DrawRecipeRecord>>();
        records->push_back({std::vector<std::weak_ptr<const DispatchVariant>>(stages.begin(), stages.end()), std::move(recipe)});
        if (old != nullptr) {
            for (const auto& record : *old) {
                if (record.Matches(stages) || record.Expired() || records->size() >= DispatchVariants()) continue;
                records->push_back(record);
            }
        }
        entry->recipes.store(std::move(records));
        VulkanDevice::NoteRecipe(VulkanDevice::RecipeEvent::Attach, VulkanDevice::RecipeKind::Draw);
    }
    // The register key of a draw (design_cpu_final 8a, design_cpu_0 3.5 (a), (b), (e)): FNV over
    // every present register of the DrawKeyRegisters ranges with its offset (an absent register
    // is simply not mixed: the sequence differs), the registered shader each program address
    // register present names (its identity and the program's code offset: a re-registration at
    // the same address is another object) and the device serial. No guest memory is read.
    static std::uint64_t drawRegisterKey(const QueueState& queue, const ShaderRegistry& registry, std::uint64_t deviceSerial) {
        std::uint64_t key = 0xcbf29ce484222325ull;
        const auto mix = [&](std::uint64_t value) {
            key ^= value;
            key *= 0x100000001b3ull;
        };
        mix(deviceSerial);
        for (const auto& range : Graphics::DrawKeyRegisters) {
            const auto& bank = range.bank == Graphics::RegisterBank::Context ? queue.context : range.bank == Graphics::RegisterBank::Shader ? queue.shader : queue.userConfig;
            mix((static_cast<std::uint64_t>(range.bank) << 32u) | range.first);
            const auto end = range.first + range.count;
            for (auto it = bank.lower_bound(range.first); it != bank.end() && it->first < end; ++it) {
                mix(it->first);
                mix(it->second);
            }
        }
        for (const auto base : {0x008u, 0x088u, 0x0c8u, 0x108u, 0x148u}) {
            const auto low = queue.shader.find(base);
            const auto high = queue.shader.find(base + 1);
            if (low == queue.shader.end() || high == queue.shader.end()) {
                mix(0);
                continue;
            }
            const auto address = (static_cast<std::uint64_t>(low->second) << 8u) | (static_cast<std::uint64_t>(high->second & 0xffu) << 40u);
            auto it = registry.upper_bound(address);
            if (it == registry.begin()) {
                mix(1);
                continue;
            }
            --it;
            mix(reinterpret_cast<std::uintptr_t>(it->second.get()));
            mix(address - it->second->codeAddress);
        }
        return key;
    }
    // APS5_VERIFY_DRAW_RECIPE: whether two decodes (the entry's and a fresh one) agree, field by
    // field (the structs carry padding, so no memcmp).
    static bool sameVertexInfo(const ShaderRecompiler::ShaderVertexStageInfo& a, const ShaderRecompiler::ShaderVertexStageInfo& b) {
        if (a.resourcesNum != b.resourcesNum || a.fetchAttribReg != b.fetchAttribReg || a.fetchBufferReg != b.fetchBufferReg || a.fetchEmbedded != b.fetchEmbedded) return false;
        for (std::uint32_t i = 0; i < a.resourcesNum && i < a.resources.size(); ++i) {
            if (a.resources[i].fields != b.resources[i].fields) return false;
            const auto& x = a.resourcesDst[i];
            const auto& y = b.resourcesDst[i];
            if (x.registerStart != y.registerStart || x.registersNum != y.registersNum || x.attrId != y.attrId || x.fetchIndex != y.fetchIndex) return false;
        }
        return true;
    }
    static bool sameDecode(const DrawDecode& a, const DrawDecode& b) {
        const auto& s = a.state;
        const auto& t = b.state;
        const auto sameColor = [](const Graphics::ColorTarget& x, const Graphics::ColorTarget& y) {
            return x.address == y.address && x.extent.width == y.extent.width && x.extent.height == y.extent.height && x.format == y.format && x.bytes == y.bytes && x.componentMapping == y.componentMapping && x.tileMode == y.tileMode && x.elementBytes == y.elementBytes && x.dccAddress == y.dccAddress && x.dccAlphaOnMsb == y.dccAlphaOnMsb;
        };
        const auto sameBlend = [](const VkPipelineColorBlendAttachmentState& x, const VkPipelineColorBlendAttachmentState& y) {
            return x.blendEnable == y.blendEnable && x.srcColorBlendFactor == y.srcColorBlendFactor && x.dstColorBlendFactor == y.dstColorBlendFactor && x.colorBlendOp == y.colorBlendOp && x.srcAlphaBlendFactor == y.srcAlphaBlendFactor && x.dstAlphaBlendFactor == y.dstAlphaBlendFactor && x.alphaBlendOp == y.alphaBlendOp && x.colorWriteMask == y.colorWriteMask;
        };
        const auto sameMesh = [](const std::optional<ShaderRecompiler::MeshConfiguration>& x, const std::optional<ShaderRecompiler::MeshConfiguration>& y) {
            if (x.has_value() != y.has_value()) return false;
            if (!x) return true;
            return x->inputPrimitive == y->inputPrimitive && x->primitivesPerGroup == y->primitivesPerGroup && x->verticesPerGroup == y->verticesPerGroup && x->maxVertices == y->maxVertices && x->maxPrimitives == y->maxPrimitives && x->threadsPerGroup == y->threadsPerGroup && x->ldsSizeDwords == y->ldsSizeDwords && x->provokingVertex == y->provokingVertex;
        };
        const auto sameTess = [](const std::optional<ShaderRecompiler::TessellationConfiguration>& x, const std::optional<ShaderRecompiler::TessellationConfiguration>& y) {
            if (x.has_value() != y.has_value()) return false;
            if (!x) return true;
            return x->inputControlPoints == y->inputControlPoints && x->outputControlPoints == y->outputControlPoints && x->domain == y->domain && x->partitioning == y->partitioning && x->outputTopology == y->outputTopology;
        };
        if (s.stages.path != t.stages.path || s.stages.registerValue != t.stages.registerValue || s.stages.vertexWaveSize != t.stages.vertexWaveSize || s.stages.fragmentWaveSize != t.stages.fragmentWaveSize || !sameMesh(s.stages.mesh, t.stages.mesh) || !sameTess(s.stages.tessellation, t.stages.tessellation)) return false;
        if (!sameColor(s.color, t.color) || s.colors.size() != t.colors.size() || s.blends.size() != t.blends.size()) return false;
        for (std::size_t i = 0; i < s.colors.size(); ++i) {
            if (!sameColor(s.colors[i], t.colors[i])) return false;
        }
        for (std::size_t i = 0; i < s.blends.size(); ++i) {
            if (!sameBlend(s.blends[i], t.blends[i])) return false;
        }
        if (s.hasColorTarget != t.hasColorTarget || s.rectList != t.rectList || s.renderExtent.width != t.renderExtent.width || s.renderExtent.height != t.renderExtent.height || s.topology != t.topology || s.negativeOneToOne != t.negativeOneToOne || s.depthClamp != t.depthClamp || s.cullMode != t.cullMode || s.frontFace != t.frontFace || !sameBlend(s.blend, t.blend) || s.blendConstants != t.blendConstants) return false;
        if (std::memcmp(&s.viewport, &t.viewport, sizeof(VkViewport)) != 0 || std::memcmp(&s.scissor, &t.scissor, sizeof(VkRect2D)) != 0) return false;
        const auto& p = a.pixel;
        const auto& q = b.pixel;
        if (p.interpolatorCount != q.interpolatorCount || p.interpolatorSettings != q.interpolatorSettings || p.wave32 != q.wave32 || p.perspectiveCenterVgpr != q.perspectiveCenterVgpr || p.hasPerspectiveCenterVgpr != q.hasPerspectiveCenterVgpr || p.posX != q.posX || p.posY != q.posY || p.posZ != q.posZ || p.posW != q.posW || p.frontFace != q.frontFace || p.ancillary != q.ancillary || p.sampleShading != q.sampleShading || p.noPerspective != q.noPerspective || p.pixelKillEnable != q.pixelKillEnable || p.depthExportEnable != q.depthExportEnable || p.sampleMaskExportEnable != q.sampleMaskExportEnable || p.earlyZ != q.earlyZ || p.executeOnNoop != q.executeOnNoop || p.targetOutputMode != q.targetOutputMode || p.targetExportMapping != q.targetExportMapping) return false;
        if (a.roles != b.roles || a.programs.size() != b.programs.size()) return false;
        for (std::size_t i = 0; i < a.programs.size(); ++i) {
            const auto& x = a.programs[i];
            const auto& y = b.programs[i];
            if (x.binary.stage != y.binary.stage || x.binary.codeAddress != y.binary.codeAddress || x.userDataBase != y.userDataBase || x.firstUserSgpr != y.firstUserSgpr || x.userData != y.userData || x.snapshot != y.snapshot || x.codeOffset != y.codeOffset) return false;
        }
        return true;
    }
    // The driver's own phases of a dispatch, around VulkanDevice::dispatch's phase rows, for the
    // "[dispatch] driver phases" line (APS5_PROFILE_DRAW, every 10 s): what the [gpu] capture and
    // device totals are made of. Summed under driverPhasesMutex once per dispatch.
    // "validate" is the validation's CPU work; the fence and timeline waits its syncs made (the
    // flush hook inside syncPendingRuns and the hook compares) are moved to "validate GPU wait",
    // so the two compare paths (APS5_VALIDATE_LEGACY) can be compared by CPU cost.
    // The packet bracket: 'prologue' is the packet's start (the worker's timer) to the phase
    // timers' start (register reads, DecodeComputeStageInfo, the HLE matching, the device load),
    // 'epilogue' the tail to the packet's end (in the packet loop), so the rows sum to the [packets]
    // DISPATCH_DIRECT 'real' row; 'prepare:' splits PrepareDispatch (VulkanDevice::PreparePhaseMs;
    // 'other' is what the call spent outside its parts); 'recipe pre-check' is reserved.
    enum DriverPhase { PhasePrologue, PhaseKey, PhaseLookup, PhaseValidate, PhaseValidateWait, PhaseRelock, PhaseCapture, PhaseRecompile, PhaseInsert, PhaseQueuedLabels, PhaseSnapshots, PhasePrepareKey, PhasePrepareFind, PhasePreparePrecollect, PhasePreparePresync, PhasePrepareStageA, PhasePrepareOther, PhaseRecipePrecheck, PhaseLockWait, PhaseLabels, PhaseNoteWriters, PhaseDevice, PhaseTail, PhaseEpilogue, DriverPhaseCount };
    static constexpr const char* DriverPhaseNames[DriverPhaseCount] = {"prologue", "key", "lookup", "validate", "validate GPU wait", "relock", "capture", "recompile", "insert", "queued-label check", "snapshots", "prepare: key", "prepare: find", "prepare: precollect", "prepare: presync", "prepare: stage A (miss)", "prepare: other", "recipe pre-check", "lock wait", "labels", "note writers", "device call", "tail", "epilogue"};
    // The lines are split by queue and packet: queue 0's direct dispatches (the [packets]
    // reconciliation), queue 0's indirect ones, and every other queue's.
    enum DispatchClass : std::size_t { Queue0Direct, Queue0Indirect, OtherQueues, DispatchClassCount };
    static constexpr const char* DispatchClassNames[DispatchClassCount] = {"queue 0 direct", "queue 0 indirect", "other queues"};
    struct DriverPhaseTotals {
        std::array<double, DriverPhaseCount> ms{};
        std::uint64_t dispatches = 0, hits = 0, validations = 0;
    };
    std::mutex driverPhasesMutex;
    std::array<DriverPhaseTotals, DispatchClassCount> driverPhaseTotals{};
    std::chrono::steady_clock::time_point driverPhasesReport = std::chrono::steady_clock::now();
    // `validated`: the lookup found an entry and validated it (hit or not), what the validate
    // phase is spent on.
    void addDriverPhases(DispatchClass which, const std::array<double, DriverPhaseCount>& ms, bool hit, bool validated) {
        std::lock_guard lock(driverPhasesMutex);
        auto& totals = driverPhaseTotals[which];
        for (std::size_t i = 0; i < DriverPhaseCount; ++i) totals.ms[i] += ms[i];
        ++totals.dispatches;
        if (hit) ++totals.hits;
        if (validated) ++totals.validations;
        const auto now = std::chrono::steady_clock::now();
        if (now - driverPhasesReport < std::chrono::seconds(10)) return;
        driverPhasesReport = now;
        for (std::size_t cls = 0; cls < DispatchClassCount; ++cls) {
            auto& line = driverPhaseTotals[cls];
            if (line.dispatches == 0) continue;
            std::string report;
            double total = 0;
            for (std::size_t i = 0; i < DriverPhaseCount; ++i) {
                char text[64];
                std::snprintf(text, sizeof(text), " %s %.1f", DriverPhaseNames[i], line.ms[i] * 1000 / static_cast<double>(line.dispatches));
                report += text;
                total += line.ms[i];
            }
            const double perValidation = line.validations != 0 ? 1000 / static_cast<double>(line.validations) : 0.0;
            std::fprintf(stderr, "[dispatch] driver phases %s (10 s, %llu dispatches, %llu cache hits), us per dispatch:%s, total %.1f (%.1f ms); validate %.1f us per validation (%llu validations; GPU waits inside it %.1f us per validation apart)\n", DispatchClassNames[cls], static_cast<unsigned long long>(line.dispatches), static_cast<unsigned long long>(line.hits), report.c_str(), total * 1000 / static_cast<double>(line.dispatches), total, line.ms[PhaseValidate] * perValidation, static_cast<unsigned long long>(line.validations), line.ms[PhaseValidateWait] * perValidation);
            line = {};
        }
    }
    // What a dispatch packet became, for the [packets] DISPATCH_DIRECT outcome rows and the phase
    // bracket: the dispatch fills the calling worker's slot (the phases at its tail, or the HLE
    // outcome when it returned early; a tolerated failure leaves it 'skipped'), and the packet
    // loop reads it after the call, adds the epilogue and files the phases by class.
    enum class DispatchOutcome : std::size_t { Skipped = 0, Real, CopyHle, FillHle, SkippedMemo, Count };
    static constexpr const char* DispatchOutcomeNames[static_cast<std::size_t>(DispatchOutcome::Count)] = {"skipped", "real", "copy HLE", "fill HLE", "skipped (memo)"};
    struct PendingDispatchPhases {
        DispatchOutcome outcome = DispatchOutcome::Skipped;
        bool phases = false;
        bool hit = false;
        bool validated = false;
        std::array<double, DriverPhaseCount> ms{};
        std::chrono::steady_clock::time_point tailAt{};
    };
    static PendingDispatchPhases& pendingDispatchPhases() {
        static thread_local PendingDispatchPhases pending;
        return pending;
    }
    // The start of the packet the calling worker executes (its [packets] timer), for the prologue rows.
    static std::chrono::steady_clock::time_point& packetStartedAt() {
        static thread_local std::chrono::steady_clock::time_point started{};
        return started;
    }
    // The [draw] driver phases line: Driver::draw's own work around Graphics::Draw (the [draws]
    // line), bracketed like a dispatch's: prologue (packet start to the decode), precheck, decode
    // (State, pixel and vertex infos), program prepare (the programs' registers and user words),
    // capture and the hook waits inside it (kept apart, as the validate row does), recompile,
    // rect-list, vectors (snapshots, linked programs, the queued-label check, the writer notes),
    // key/lookup/validate (the draw cache's key, lookup, per-stage validations and a miss's
    // insert) with the hook waits inside the validations as 'validate GPU wait', lock wait,
    // labels, Graphics::Draw, the epilogue; a packet that drew nothing (Nothing, Rejected, a
    // tolerated throw) is one 'skipped' row. The rows sum to the [packets] DRAW_INDEX_AUTO time;
    // less Graphics::Draw they are the prologue the [draws] line does not see.
    enum DrawDriverPhase { DrawRowPrologue, DrawRowPrecheck, DrawRowDecode, DrawRowProgramPrepare, DrawRowCapture, DrawRowCaptureHookWaits, DrawRowRecompile, DrawRowRectList, DrawRowVectors, DrawRowKeyLookupValidate, DrawRowValidateWait, DrawRowLockWait, DrawRowLabels, DrawRowGraphics, DrawRowSkipped, DrawRowEpilogue, DrawDriverPhaseCount };
    static constexpr const char* DrawDriverPhaseNames[DrawDriverPhaseCount] = {"prologue", "precheck", "decode", "program prepare", "capture", "capture hook waits", "recompile", "rect-list", "vectors", "key/lookup/validate", "validate GPU wait", "lock wait", "labels", "Graphics::Draw", "skipped", "epilogue"};
    struct DrawPhaseTotals {
        std::array<double, DrawDriverPhaseCount> ms{};
        std::uint64_t packets = 0, drawn = 0, captures = 0;
        std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
    };
    std::mutex drawPhasesMutex;
    DrawPhaseTotals drawPhaseTotals;
    struct PendingDrawPhases {
        bool phases = false;
        std::uint64_t captures = 0;
        std::array<double, DrawDriverPhaseCount> ms{};
        std::chrono::steady_clock::time_point tailAt{};
    };
    static PendingDrawPhases& pendingDrawPhases() {
        static thread_local PendingDrawPhases pending;
        return pending;
    }
    void addDrawPhases(const std::array<double, DrawDriverPhaseCount>& ms, bool drawn, std::uint64_t captures) {
        std::lock_guard lock(drawPhasesMutex);
        auto& totals = drawPhaseTotals;
        for (std::size_t i = 0; i < DrawDriverPhaseCount; ++i) totals.ms[i] += ms[i];
        ++totals.packets;
        if (drawn) ++totals.drawn;
        totals.captures += captures;
        const auto now = std::chrono::steady_clock::now();
        if (now - totals.lastReport < std::chrono::seconds(10)) return;
        totals.lastReport = now;
        std::string report;
        double total = 0;
        for (std::size_t i = 0; i < DrawDriverPhaseCount; ++i) {
            char text[64];
            std::snprintf(text, sizeof(text), " %s %.1f", DrawDriverPhaseNames[i], totals.ms[i] * 1000 / static_cast<double>(totals.packets));
            report += text;
            total += totals.ms[i];
        }
        std::fprintf(stderr, "[draw] driver phases (10 s, %llu draw packets, %llu drawn, %llu captures), us per packet:%s, total %.1f (%.1f ms; Graphics::Draw %.1f ms, skipped %.1f ms)\n", static_cast<unsigned long long>(totals.packets), static_cast<unsigned long long>(totals.drawn), static_cast<unsigned long long>(totals.captures), report.c_str(), total * 1000 / static_cast<double>(totals.packets), total, totals.ms[DrawRowGraphics], totals.ms[DrawRowSkipped]);
        totals.ms = {};
        totals.packets = totals.drawn = totals.captures = 0;
    }
    std::map<std::uint32_t, QueueState> queues;
    std::map<std::uint32_t, std::shared_ptr<IVideoOutput>> outputs;
    DevicePointer device;
    std::uint64_t accepted = 0;
    std::uint64_t completed = 0;
    std::set<std::uint64_t> completedOutOfOrder;
    std::exception_ptr failure;
    // Set once `failure` is, so hot loops can check for one without the mutex.
    std::atomic<bool> failed{false};
    bool stopping = false;
    bool resetGraphics = false;
    // Threads in WaitIdle (under `mutex`): a worker's completion notifies only while one waits.
    std::uint32_t idleWaiters = 0;

    static bool& OnWorkerThread() {
        static thread_local bool worker = false;
        return worker;
    }

    // Caller holds `mutex`.
    void enqueue(Submission submission) {
        const auto queue = submission.queue;
        auto& worker = workers[queue];
        worker.pending.push_back(std::move(submission));
        worker.queued.fetch_add(1, std::memory_order_acq_rel);
        if (!worker.thread.joinable()) worker.thread = std::thread([this, queue] { run(queue); });
    }

    Driver() {
        ShaderMemory::SetWaitedMsProvider(&Graphics::Recorder::ThreadWaitedMs);
        try {
            LibcRegisterShutdown_nid_postfix([] { Driver::Get().Shutdown(); });
        } catch (...) {
            stop();
            throw;
        }
    }

    void rethrowFailure() const {
        if (failure != nullptr) {
            std::rethrow_exception(failure);
        }
    }

    // Summarizes a rejected submission (packet names with counts and the first rejection reason per name).
    static void dumpPackets(std::span<const std::uint32_t> commands, const std::uint32_t* guest = nullptr) {
        std::map<std::string, std::pair<std::size_t, std::string>> summary;
        std::vector<std::pair<std::size_t, std::uint32_t>> walk;
        for (std::size_t cursor = 0; cursor < commands.size();) {
            const auto header = commands[cursor];
            if (Pm4::FillerPacket(header)) { ++cursor; continue; }
            if ((header & 0xc0000000u) != 0xc0000000u) break;
            const auto count = Pm4::PacketWords(header);
            if (count > commands.size() - cursor) break;
            walk.emplace_back(cursor, header);
            auto& entry = summary[Pm4::Name(header)];
            if (entry.first++ == 0) {
                try {
                    Pm4::Validate(commands.subspan(cursor, count), 0);
                } catch (const std::exception& error) {
                    entry.second = error.what();
                }
            }
            cursor += count;
        }
        std::fprintf(stderr, "[gpu] rejected submission of %zu dwords at guest %p:\n", commands.size(), static_cast<const void*>(guest));
        for (const auto& [name, entry] : summary) std::fprintf(stderr, "[gpu]   %-28s x%-5zu %s\n", name.c_str(), entry.first, entry.second.c_str());
        // The last packets walked before the parse broke and, with APS5_DUMP_REJECTED=1, the whole
        // stream for offline analysis.
        for (std::size_t i = walk.size() > 24 ? walk.size() - 24 : 0; i < walk.size(); ++i) {
            const auto [offset, header] = walk[i];
            std::fprintf(stderr, "[gpu]   at DWORD %-6zu header 0x%08x %-24s %zu dwords:", offset, header, Pm4::Name(header).c_str(), Pm4::PacketWords(header));
            for (std::size_t j = offset + 1; j < std::min(commands.size(), offset + std::min<std::size_t>(Pm4::PacketWords(header), 12)); ++j) std::fprintf(stderr, " %08x", commands[j]);
            std::fprintf(stderr, "\n");
        }
        static const bool dumpRejected = std::getenv("APS5_DUMP_REJECTED") != nullptr;
        if (!dumpRejected) return;
        static int dumps = 0;
        char fileName[64];
        std::snprintf(fileName, sizeof(fileName), "rejected_submission_%d.bin", dumps++);
        if (FILE* file = std::fopen(fileName, "wb")) {
            std::fwrite(commands.data(), sizeof(std::uint32_t), commands.size(), file);
            std::fclose(file);
            std::fprintf(stderr, "[gpu] rejected submission written to %s\n", fileName);
        }
    }

    static void validate(std::span<const std::uint32_t> commands, std::uint32_t queue, const std::uint32_t* guest = nullptr) {
        for (std::size_t cursor = 0; cursor < commands.size();) {
            const auto header = commands[cursor];
            if (Pm4::FillerPacket(header)) { ++cursor; continue; }
            if ((header & 0xc0000000u) != 0xc0000000u) {
                char what[96];
                std::snprintf(what, sizeof(what), "unsupported PM4 packet type: header 0x%08x at DWORD %zu of %zu", header, cursor, commands.size());
                // The packets before it (and the dwords around it) show which packet was mis-sized.
                dumpPackets(commands, guest);
                std::fprintf(stderr, "[gpu] dwords %zu..%zu:", cursor >= 8 ? cursor - 8 : 0, std::min(commands.size(), cursor + 8));
                for (std::size_t i = cursor >= 8 ? cursor - 8 : 0; i < std::min(commands.size(), cursor + 8); ++i) std::fprintf(stderr, " %08x", commands[i]);
                std::fprintf(stderr, "\n");
                throw std::runtime_error(std::string("AGC driver: ") + what);
            }
            const auto count = Pm4::PacketWords(header);
            require(count <= commands.size() - cursor, "truncated PM4 packet");
            try {
                Pm4::Validate(commands.subspan(cursor, count), queue);
            } catch (const std::exception& error) {
                dumpPackets(commands, guest);
                throw std::runtime_error("AGC driver: " + Pm4::Name(header) + " at DWORD " + std::to_string(cursor) + ": " + error.what());
            }
            cursor += count;
        }
    }

    // Worker time per packet class, reported every 10 s (APS5_PROFILE_DRAW) so throughput problems
    // show where time goes.
    struct WorkerProfile {
        double dispatchMs = 0;
        double drawMs = 0;
        double waitMs = 0;
        std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point reported = start;
    };
    template <typename Work>
    static void timed(double WorkerProfile::*bucket, Work&& work) {
        static thread_local WorkerProfile profile;
        const auto begin = std::chrono::steady_clock::now();
        work();
        const auto end = std::chrono::steady_clock::now();
        profile.*bucket += std::chrono::duration<double, std::milli>(end - begin).count();
        static const bool report = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        if (report && end - profile.reported > std::chrono::seconds(10)) {
            profile.reported = end;
            std::fprintf(stderr, "[gpu] worker at %.0f s: dispatch %.1f s, draw %.1f s, wait %.1f s\n", std::chrono::duration<double>(end - profile.start).count(), profile.dispatchMs / 1000, profile.drawMs / 1000, profile.waitMs / 1000);
        }
    }

    // A draw or dispatch the translator cannot handle is skipped so the rest of the frame still runs;
    // each distinct failure is reported once.
    static void reportSkip(const char* kind, const std::string& what) {
        static std::mutex reportedMutex;
        static std::set<std::string> reported;
        std::lock_guard lock(reportedMutex);
        // APS5_TRACE_SKIPS reports every skip (shortened), not only the first per reason.
        static const bool traceSkips = std::getenv("APS5_TRACE_SKIPS") != nullptr;
        if (reported.insert(what).second) std::fprintf(stderr, "[gpu] skipped %s: %s\n", kind, what.c_str());
        else if (traceSkips) std::fprintf(stderr, "[gpu] skipped %s again: %.100s\n", kind, what.c_str());
    }
    template <typename Work>
    void tolerate(const char* kind, Work&& work) {
        try {
            work();
        } catch (const std::exception& error) {
            reportSkip(kind, error.what());
        }
    }

    // Saves a request once per shader address so it can be replayed with agc_shader_replay.
    static std::string dumpRequest(std::uint64_t address, const ShaderRecompiler::RecompileRequest& request) {
        static std::mutex dumpMutex;
        static std::set<std::uint64_t> dumped;
        char name[64];
        std::snprintf(name, sizeof(name), "shader_%llx.req", static_cast<unsigned long long>(address));
        std::lock_guard lock(dumpMutex);
        if (!dumped.insert(address).second) return name;
        try {
            const auto text = ShaderRecompiler::RequestSerializer{}.Serialize(request);
            if (std::FILE* file = std::fopen(name, "wb")) {
                std::fwrite(text.data(), 1, text.size(), file);
                std::fclose(file);
            }
        } catch (const std::exception& error) {
            std::fprintf(stderr, "[gpu] could not serialize request for 0x%llx: %s\n", static_cast<unsigned long long>(address), error.what());
        }
        return name;
    }

    // Capture and recompile run without the GPU lock, so the compute queues prepare their dispatches
    // in parallel; only resource building and recording are serialized.
    // The engine clears and initializes buffers with a nine-dword compute kernel (every thread stores one
    // 16-byte record of a 32_32_32_32_UINT typed buffer: v_lshl_add_u32 v4, s8, 6, v0; four v_mov from
    // the pattern in user data; buffer_store_format_xyzw v[0:3], v4, s[0:3] idxen). Demon's Souls runs
    // it ~13 times per frame over ~80 MB, and typed 16-byte stores into host-imported system memory took
    // ~22 ms each on the GPU while every drain waited for them. The fill is done as a transfer instead
    // (or a CPU store when the memory is not imported). Debug aid: APS5_NO_FILL_HLE=1 runs the kernel.
    // Whether `code` is that kernel with a V# the transfer can reproduce: everything fillBuffer
    // decides on except the group count, so an indirect dispatch can tell before reading its count.
    static bool matchesFillKernel(std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData, const ShaderRecompiler::ShaderComputeStageInfo& compute) {
        static const bool enabled = std::getenv("APS5_NO_FILL_HLE") == nullptr;
        if (!enabled || userData.size() < 8 || compute.numThreads[0] != 64 || compute.numThreads[1] != 1 || compute.numThreads[2] != 1) return false;
        static constexpr std::array<std::uint32_t, 9> fillKernel{0xd7460004u, 0x04010c08u, 0x7e000204u, 0x7e020205u, 0x7e040206u, 0x7e060207u, 0xe01c2000u, 0x80000004u, 0xbf810000u};
        if (code.size() < fillKernel.size() || !std::equal(fillKernel.begin(), fillKernel.end(), code.begin())) return false;
        // V# in user[0..3]: base, stride 16, record count, 32_32_32_32_UINT with an identity swizzle.
        const auto stride = (userData[1] >> 16u) & 0x3fffu;
        const bool swizzled = ((userData[1] >> 31u) & 1u) != 0;
        const auto dstSel = userData[3] & 0xfffu;
        const bool addTid = ((userData[3] >> 23u) & 1u) != 0;
        const auto type = userData[3] >> 30u;
        const auto format = (userData[3] >> 12u) & 0x7fu;
        return type == 0 && stride == 16 && !swizzled && !addTid && dstSel == 0xfacu && format == 0x4bu;
    }

    // A fill covering exactly one resident storage image's surface is done as a GPU clear of that
    // image (StorageTexture::FillClear): nothing is stored into host memory and the surface is not
    // re-uploaded at its next use; the texels reach guest memory through the deferred write-back
    // like a shader's. Debug aid: APS5_NO_FILL_CLEAR=1 stores every fill as before.
    static bool fillClearEnabled() {
        static const bool enabled = std::getenv("APS5_NO_FILL_CLEAR") == nullptr;
        return enabled;
    }

    // Debug aid: APS5_FILL_CLEAR_EXACT_ONLY=1 clears only an image no other image overlaps (the
    // first form of the conversion): no layer clears, no stale images over the range.
    static bool fillClearExactOnly() {
        static const bool exactOnly = std::getenv("APS5_FILL_CLEAR_EXACT_ONLY") != nullptr;
        return exactOnly;
    }

    // APS5_PROFILE_DRAW, every 10 s: [fill-cover] counts every fill HLE call by how its range meets
    // the storage images alive (StorageTexture::ClassifyFill) with each class's bytes, the covers
    // that had other images over the range, the pending results discarded as dead, and the exact
    // and layer covers by guest format; [fill-clear] counts the fills done as clears with the bytes
    // they did not store, and the covers refused by reason.
    static void fillClearCount(const Graphics::StorageTexture::FillCoverage& coverage, std::size_t bytes, std::span<const std::uint32_t, 4> pattern, std::size_t discarded, bool cleared, const char* refusal) {
        static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        if (!profile) return;
        using FillCover = Graphics::StorageTexture::FillCover;
        static constexpr std::array<const char*, 8> names{"none", "exact", "inside", "around", "straddle", "several", "keys", "layer"};
        static std::mutex countsMutex;
        static std::array<std::uint64_t, 8> counts{}, coverBytes{};
        static std::map<std::uint32_t, std::uint64_t> coveredFormats;
        static std::map<std::string, std::uint64_t> refusals;
        static std::map<std::uint32_t, std::uint64_t> keyCodes;
        static std::uint64_t clears = 0, clearedBytes = 0, layerClears = 0, withOthers = 0, othersInside = 0, discards = 0;
        static auto lastReport = std::chrono::steady_clock::now();
        std::lock_guard lock(countsMutex);
        const auto index = static_cast<std::size_t>(coverage.cover);
        ++counts[index];
        coverBytes[index] += bytes;
        if (coverage.image != nullptr) {
            ++coveredFormats[coverage.image->Descriptor().format];
            if (coverage.others != 0) ++withOthers;
            othersInside += coverage.inside;
        }
        if (coverage.cover == FillCover::Keys) {
            // The key byte a metadata fill stores (0x100: not one byte repeated).
            const auto word = pattern[0];
            const bool uniform = pattern[1] == word && pattern[2] == word && pattern[3] == word && word == (word & 0xffu) * 0x01010101u;
            ++keyCodes[uniform ? word & 0xffu : 0x100u];
        }
        discards += discarded;
        if (cleared) {
            ++clears;
            clearedBytes += bytes;
            if (coverage.cover == FillCover::Layer) ++layerClears;
        } else if (refusal != nullptr) {
            ++refusals[refusal];
        }
        const auto now = std::chrono::steady_clock::now();
        if (now - lastReport < std::chrono::seconds(10)) return;
        lastReport = now;
        std::string line = "[fill-cover] fills by cover (count/MiB):";
        for (std::size_t i = 0; i < names.size(); ++i) line += " " + std::string(names[i]) + " " + std::to_string(counts[i]) + "/" + std::to_string(coverBytes[i] >> 20u);
        line += "; covers with other images over the range " + std::to_string(withOthers) + " (" + std::to_string(othersInside) + " images wholly inside), pending results discarded as dead " + std::to_string(discards) + "; covers by guest format:";
        for (const auto& [format, count] : coveredFormats) line += " " + std::to_string(format) + ":" + std::to_string(count);
        line += "; key fills by code:";
        for (const auto& [code, count] : keyCodes) {
            char text[32];
            std::snprintf(text, sizeof(text), " 0x%02x:%llu", code, static_cast<unsigned long long>(count));
            line += text;
        }
        std::fprintf(stderr, "%s\n", line.c_str());
        line = "[fill-clear] " + std::to_string(clears) + " fills cleared on resident images (" + std::to_string(clearedBytes >> 20u) + " MiB not stored, " + std::to_string(layerClears) + " one array layer); covers refused:";
        for (const auto& [reason, count] : refusals) line += " " + reason + " " + std::to_string(count);
        std::fprintf(stderr, "%s\n", line.c_str());
    }

    bool fillBuffer(QueueState& queue, std::uint32_t queueId, std::span<const std::uint32_t> packet, std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData, const ShaderRecompiler::ShaderComputeStageInfo& compute, const std::shared_ptr<VulkanDevice>& localDevice) {
        if (!matchesFillKernel(code, userData, compute)) return false;
        const auto numRecords = userData[2];
        std::array<std::uint32_t, 3> groups{packet[1], packet[2], packet[3]};
        if ((packet[4] & 0x20u) != 0) {
            for (std::uint32_t axis = 0; axis < 3; ++axis) {
                const auto threads = std::max(readRegister(queue.shader, 0x207 + axis) & 0xffffu, 1u);
                groups[axis] = (groups[axis] + threads - 1) / threads;
            }
        }
        if (groups[1] != 1 || groups[2] != 1) return false;
        const auto records = std::min<std::uint64_t>(static_cast<std::uint64_t>(groups[0]) * 64u, numRecords);
        const auto base = userData[0] | (static_cast<std::uint64_t>(userData[1] & 0xffffu) << 32u);
        const auto bytes = static_cast<std::size_t>(records * 16u);
        const std::array<std::uint32_t, 4> pattern{userData[4], userData[5], userData[6], userData[7]};
        static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        static std::atomic<std::uint64_t> fills{0}, filledBytes{0}, cpuFills{0};
        // Fills stored by pattern kind (a uniform dword goes to vkCmdFillBuffer, a 16-byte pattern
        // to a seed copy doubled in place: one chain copy per doubling) and their bytes; under the
        // GpuMutex like the phases below.
        static std::uint64_t uniformFills = 0, uniformBytes = 0, patternFills = 0, patternBytes = 0, chainCopies = 0;
        // APS5_PROFILE_DRAW, every 10 s on the [fill] line: the hold's sub-phases in ms (labels,
        // check, classify, discard, clear = the FillClear attempt with its flush of straddling
        // images, flush = the pre-store FlushPending with its count, device = VulkanDevice::
        // FillBuffer, cpu = the CPU fallback), and the hold time by cover class. Under GpuMutex.
        enum FillPhase : std::size_t { FillLabels, FillCheck, FillClassify, FillDiscard, FillClearPhase, FillFlush, FillDevice, FillCpu, FillPhaseCount };
        static constexpr const char* fillPhaseNames[FillPhaseCount] = {"labels", "check", "classify", "discard", "clear", "flush", "device", "cpu"};
        static std::array<double, FillPhaseCount> fillPhaseMs{};
        static std::array<double, 8> holdByCoverMs{};
        static std::array<std::uint64_t, 8> holdByCover{};
        static std::uint64_t flushes = 0, flushed = 0;
        static auto fillReport = std::chrono::steady_clock::now();
        ++fills;
        filledBytes += bytes;
        if (bytes != 0) {
            GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Fill);
            std::lock_guard gpuLock(GuestMemory::GpuMutex());
            const auto holdStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
            auto phaseStart = holdStart;
            const auto phase = [&](FillPhase which) {
                if (!profile) return;
                const auto now = std::chrono::steady_clock::now();
                fillPhaseMs[which] += std::chrono::duration<double, std::milli>(now - phaseStart).count();
                phaseStart = now;
            };
            // This queue's labels first (queue order), as at every dispatch's lock.
            recordLabelsForPacket(localDevice.get(), queueId);
            phase(FillLabels);
            GuestMemory::CheckRange(reinterpret_cast<const void*>(base), bytes, 16, true);
            phase(FillCheck);
            const auto coverage = Graphics::StorageTexture::ClassifyFill(base, bytes);
            phase(FillClassify);
            // Debug aid (APS5_TRACE_DCC_KEYS=1): the title's fills of a surface's DCC metadata.
            static const bool traceKeys = std::getenv("APS5_TRACE_DCC_KEYS") != nullptr;
            if (traceKeys && coverage.cover == Graphics::StorageTexture::FillCover::Keys) std::fprintf(stderr, "[dcc-keys] title fills keys 0x%llx+0x%zx with %08x %08x %08x %08x (queue 0x%x)\n", static_cast<unsigned long long>(base), bytes, pattern[0], pattern[1], pattern[2], pattern[3], queueId);
            // Results pending in images wholly inside the range are dead whichever way the fill is
            // done (it overwrites every byte of them): dropped rather than stored first.
            const auto discarded = fillClearEnabled() ? Graphics::StorageTexture::DiscardPendingInside(base, bytes) : 0u;
            phase(FillDiscard);
            const char* refusal = nullptr;
            bool cleared = false;
            if (coverage.image != nullptr) {
                if (!fillClearEnabled()) {
                    refusal = "disabled";
                } else if (fillClearExactOnly() && (coverage.cover != Graphics::StorageTexture::FillCover::Exact || coverage.others != 0)) {
                    refusal = "exact only";
                } else {
                    // Results of images straddling the range land first (their bytes outside it
                    // stay); the covered image's own are superseded by the clear.
                    Graphics::StorageTexture::FlushPending(base, bytes, coverage.image.get(), "buffer fill", Graphics::PublishScope::PartialUnits);
                    cleared = coverage.image->FillClear(pattern, coverage.layer, refusal);
                }
            }
            phase(FillClearPhase);
            fillClearCount(coverage, bytes, pattern, discarded, cleared, refusal);
            noteForeignWriter(base, base + bytes, queueId);
            // Results still on the GPU for the range would be stored over the fill later.
            if (!cleared) {
                ++flushes;
                // The fill's own units need no publish (it overwrites them); the boundary units
                // keep a neighbour's shadowed results, which land first.
                if (Graphics::StorageTexture::FlushPending(base, bytes, nullptr, "buffer fill", Graphics::PublishScope::PartialUnits)) ++flushed;
            }
            phase(FillFlush);
            const bool stored = cleared || localDevice->FillBuffer(base, bytes, pattern);
            phase(FillDevice);
            if (profile && stored && !cleared) {
                const bool uniform = pattern[0] == pattern[1] && pattern[1] == pattern[2] && pattern[2] == pattern[3];
                ++(uniform ? uniformFills : patternFills);
                (uniform ? uniformBytes : patternBytes) += bytes;
                if (!uniform) chainCopies += static_cast<std::uint64_t>(std::bit_width((bytes / 16) - 1));
            }
            if (!stored) {
                ++cpuFills;
                // Ordered after recorded work like any CPU store from the queue.
                localDevice->WaitIdle();
                static thread_local std::vector<std::byte> block;
                const auto chunk = std::min<std::size_t>(bytes, 1u << 20u);
                block.resize(chunk);
                for (std::size_t at = 0; at < chunk; at += 16) std::memcpy(block.data() + at, pattern.data(), 16);
                for (std::size_t done = 0; done < bytes; done += chunk) GuestMemory::Write(base + done, std::span<const std::byte>(block).first(std::min(chunk, bytes - done)), 16);
                phase(FillCpu);
            }
            if (profile) {
                const auto now = std::chrono::steady_clock::now();
                const auto cover = static_cast<std::size_t>(coverage.cover);
                ++holdByCover[cover];
                holdByCoverMs[cover] += std::chrono::duration<double, std::milli>(now - holdStart).count();
                if (now - fillReport > std::chrono::seconds(10)) {
                    fillReport = now;
                    static constexpr std::array<const char*, 8> coverNames{"none", "exact", "inside", "around", "straddle", "several", "keys", "layer"};
                    std::string line;
                    char text[64];
                    for (std::size_t i = 0; i < FillPhaseCount; ++i) {
                        std::snprintf(text, sizeof(text), " %s %.0f", fillPhaseNames[i], fillPhaseMs[i]);
                        line += text;
                    }
                    line += "; hold by cover (count/ms):";
                    for (std::size_t i = 0; i < coverNames.size(); ++i) {
                        std::snprintf(text, sizeof(text), " %s %llu/%.0f", coverNames[i], static_cast<unsigned long long>(holdByCover[i]), holdByCoverMs[i]);
                        line += text;
                    }
                    std::fprintf(stderr, "[fill] %llu buffer fills (%.0f MiB), %llu stored by the CPU; hold phases ms (cumulative):%s; pre-store flushes %llu (%llu stored an image); stored on the GPU: uniform %llu (%.0f MiB), pattern %llu (%.0f MiB), chain copies %llu\n", static_cast<unsigned long long>(fills.load()), filledBytes.load() / 1048576.0, static_cast<unsigned long long>(cpuFills.load()), line.c_str(), static_cast<unsigned long long>(flushes), static_cast<unsigned long long>(flushed), static_cast<unsigned long long>(uniformFills), uniformBytes / 1048576.0, static_cast<unsigned long long>(patternFills), patternBytes / 1048576.0, static_cast<unsigned long long>(chainCopies));
                }
            }
        }
        return true;
    }

    // The engine's memcpy kernel (38 code words: count and n from a one-record V#, then every thread
    // i < count stores src[i mod n] to dst[i]; src in user[0..3], dst in user[4..7], the record in
    // user[8..11], stride-4 32_UINT V#s and a stride-16 one). Demon's Souls runs it ~27 times per
    // frame at the movie stage, mostly on small records whose destinations later captures read, so
    // every such capture waited for the kernel's batch. Matched by the code hash and the exact V#
    // control words and done by VulkanDevice::CopyBuffer: a CPU copy at packet
    // time when the copy is small and nothing recorded may still read the destination in place
    // (the recorder's read tracking) or write either range (the bytes are settled, with no pending
    // write for readers to wait on), else a transfer between the host imports.
    // count and n are read on the CPU only when nothing recorded may still change their record;
    // otherwise, and whenever a guard fails, the kernel runs as before (never a sync). Debug aids:
    // APS5_NO_COPY_HLE=1 runs the kernel always; APS5_COPY_HLE_CPU_MAX=<bytes> bounds the CPU copies
    // (default 262144, 0 keeps every copy on the GPU, as APS5_NO_CPU_COPY=1 does);
    // APS5_COPY_HLE_GPU_MAX=<bytes> bounds the transfers (a larger copy that is neither a CPU copy
    // nor an image copy runs the kernel; unset: no bound, since the kernel and the transfer cost
    // the GPU the same and the transfer spares the dispatch's CPU work; 0: never a transfer);
    // APS5_COPY_READ_TRACKING=0 makes the CPU path require an idle recorder (the rule before read
    // tracking); APS5_COPY_SYNC, APS5_COPY_WAIT_SOURCE, APS5_COPY_VERIFY see VulkanDevice::CopyBuffer.
    static bool matchesCopyKernel(std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData, const ShaderRecompiler::ShaderComputeStageInfo& compute) {
        static const bool enabled = std::getenv("APS5_NO_COPY_HLE") == nullptr;
        if (!enabled || userData.size() != 12 || compute.numThreads[0] != 64 || compute.numThreads[1] != 1 || compute.numThreads[2] != 1 || !compute.groupIdEnable[0]) return false;
        static constexpr std::size_t copyKernelWords = 38;
        static constexpr std::uint64_t copyKernelHash = 0x6ec00fe8aa95f99aull;
        if (code.size() < copyKernelWords || code[0] != 0xbfa00002u || code[copyKernelWords - 1] != 0xbf810000u) return false;
        std::uint64_t hash = 0xcbf29ce484222325ull;
        for (const auto word : code.first(copyKernelWords)) {
            hash ^= word;
            hash *= 0x100000001b3ull;
        }
        if (hash != copyKernelHash) return false;
        const auto stride = [&](std::size_t word) { return (userData[word] >> 16u) & 0x3fffu; };
        const auto swizzled = [&](std::size_t word) { return ((userData[word] >> 31u) & 1u) != 0; };
        return userData[3] == 0x00014004u && userData[7] == 0x00014004u && userData[11] == 0x0004dfacu && stride(1) == 4 && stride(5) == 4 && stride(9) == 16 && !swizzled(1) && !swizzled(5) && !swizzled(9) && userData[10] >= 1;
    }

    // The copies' outcomes for the [copy] line (APS5_PROFILE_DRAW, every 10 s): 0 and 1 are
    // VulkanDevice::CopyBuffer's paths and 2 its refusal, the rest are decided here before it.
    enum CopyPath { CopyCpu = 0, CopyGpu = 1, CopyNotImported = 2, CopyAliased = 3, CopyRecordPending = 4, CopyShape = 5, CopyOverlapping = 6, CopyInaccessible = 7, CopyOverGpuMax = 8, CopyPaths = 9 };
    // `started` and `locked` are when the packet's handling began and when it got the GPU mutex
    // (equal when it never took it), so the line shows what the copies cost the worker. `outcome`
    // (paths 0..2) carries the CPU-path refusal and the wait/verify counters.
    static void countCopy(int path, std::size_t bytes, std::chrono::steady_clock::time_point started, std::chrono::steady_clock::time_point locked, const VulkanDevice::CopyOutcome* outcome = nullptr) {
        static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        if (!profile || path < 0 || path >= CopyPaths) return;
        using Outcome = VulkanDevice::CopyOutcome;
        static std::mutex countsMutex;
        static std::uint64_t counts[CopyPaths] = {};
        static std::uint64_t refused[Outcome::Refusals] = {};
        static std::uint64_t cpuBytes = 0, gpuBytes = 0, aliasedBytes = 0, syncs = 0, settledBySignal = 0, sourceWaits = 0;
        static double syncedMs = 0, totalMs = 0, lockWaitMs = 0, sourceWaitMs = 0;
        static auto lastReport = std::chrono::steady_clock::now();
        const auto now = std::chrono::steady_clock::now();
        std::lock_guard lock(countsMutex);
        ++counts[path];
        if (path == CopyCpu) cpuBytes += bytes;
        if (path == CopyGpu) gpuBytes += bytes;
        if (path == CopyAliased) aliasedBytes += bytes;
        if (outcome != nullptr) {
            if (outcome->synced) {
                ++syncs;
                syncedMs += outcome->syncMs;
            }
            if (outcome->reason > Outcome::None && outcome->reason < Outcome::Refusals) ++refused[outcome->reason];
            if (outcome->sourceSettledBySignal) ++settledBySignal;
            if (outcome->waitedForSource) {
                ++sourceWaits;
                sourceWaitMs += outcome->waitMs;
            }
        }
        totalMs += std::chrono::duration<double, std::milli>(now - started).count();
        lockWaitMs += std::chrono::duration<double, std::milli>(locked - started).count();
        if (now - lastReport < std::chrono::seconds(10)) return;
        lastReport = now;
        std::uint64_t matched = 0;
        for (const auto count : counts) matched += count;
        const auto verify = VulkanDevice::CopyVerifyCounts();
        const auto aliasing = VulkanDevice::CopyAliasCounts();
        // Known values (cumulative): ring entries carrying a value, reads served from one (capture
        // words and validation regions), and the verified ones (APS5_VERIFY_KNOWN_VALUES=1).
        const auto known = ShaderMemory::KnownValues();
        const auto load = [](const std::atomic<std::uint64_t>& counter) { return static_cast<unsigned long long>(counter.load(std::memory_order_relaxed)); };
        std::fprintf(stderr, "[copy] %llu copy kernels matched: cpu %llu (%.1f MiB; source settled by signal %llu), gpu %llu (%.0f MiB; %llu waited %.0f ms), aliased %llu (%.0f MiB; %llu destination images created; transfers instead: no source image %llu, shape %llu, refused %llu); refused cpu: size %llu, image %llu, shadow %llu, source pending %llu, source unsettled %llu, destination pending %llu, destination unsettled %llu, label %llu, reader %llu; kernel run instead: count record pending %llu, shape %llu, not imported %llu, overlapping %llu, inaccessible %llu, over gpu max %llu; waited for the source %llu / %.0f ms; verify: %llu verified, source changed %llu, destination changed %llu, reader missed %llu; known values: %llu entries, %llu reads served, %llu verified, %llu mismatches; took %.0f ms (%.0f ms waiting for the mutex)\n", static_cast<unsigned long long>(matched), static_cast<unsigned long long>(counts[CopyCpu]), cpuBytes / 1048576.0, static_cast<unsigned long long>(settledBySignal), static_cast<unsigned long long>(counts[CopyGpu]), gpuBytes / 1048576.0, static_cast<unsigned long long>(syncs), syncedMs, static_cast<unsigned long long>(counts[CopyAliased]), aliasedBytes / 1048576.0, static_cast<unsigned long long>(aliasing.created), static_cast<unsigned long long>(aliasing.noSource), static_cast<unsigned long long>(aliasing.shape), static_cast<unsigned long long>(aliasing.refused), static_cast<unsigned long long>(refused[Outcome::Size]), static_cast<unsigned long long>(refused[Outcome::Image]), static_cast<unsigned long long>(refused[Outcome::Shadow]), static_cast<unsigned long long>(refused[Outcome::SourcePending]), static_cast<unsigned long long>(refused[Outcome::SourceUnsettled]), static_cast<unsigned long long>(refused[Outcome::DestinationPending]), static_cast<unsigned long long>(refused[Outcome::DestinationUnsettled]), static_cast<unsigned long long>(refused[Outcome::Label]), static_cast<unsigned long long>(refused[Outcome::Reader]), static_cast<unsigned long long>(counts[CopyRecordPending]), static_cast<unsigned long long>(counts[CopyShape]), static_cast<unsigned long long>(counts[CopyNotImported]), static_cast<unsigned long long>(counts[CopyOverlapping]), static_cast<unsigned long long>(counts[CopyInaccessible]), static_cast<unsigned long long>(counts[CopyOverGpuMax]), static_cast<unsigned long long>(sourceWaits), sourceWaitMs, static_cast<unsigned long long>(verify.verified), static_cast<unsigned long long>(verify.sourceChanged), static_cast<unsigned long long>(verify.destinationChanged), static_cast<unsigned long long>(verify.readerMissed), load(knownValueEntries), load(knownValueReads), static_cast<unsigned long long>(known.verified) + load(knownValueVerified), static_cast<unsigned long long>(known.mismatches) + load(knownValueMismatches), totalMs, lockWaitMs);
    }

    // The evidence ring's entry for a copy recorded on the GPU: the kernel's destination element
    // under its program, as noteWrittenBuffers enters it for the kernel itself, so the
    // write-evidence policy sees the same writer as before. `value`, when VulkanDevice::CopyBuffer
    // knows the destination's post-copy bytes, makes it a known-value entry (classifyPendingWrite's
    // KnownValue), current while UnchangedSince(destination, `generation`) holds.
    void noteCopyWriter(std::uint64_t program, std::uint64_t begin, std::uint64_t end, std::uint32_t queue, std::span<const std::byte> value = {}, std::uint64_t generation = 0) {
        if (!(WriteEvidenceEnabled() || TraceCapSync())) return;
        std::shared_ptr<const std::vector<std::byte>> known;
        if (!value.empty() && value.size() == end - begin && generation != 0) {
            known = std::make_shared<const std::vector<std::byte>>(value.begin(), value.end());
            knownValueEntries.fetch_add(1, std::memory_order_relaxed);
        }
        std::lock_guard lock(writtenBuffersMutex);
        writtenBuffers.push_back({program, begin, end, ++writtenBufferSerial, queue, false, std::move(known), generation});
        while (writtenBuffers.size() > WrittenBufferRing) writtenBuffers.pop_front();
    }
    // Known-value copy destinations (K): APS5_NO_COPY_KNOWN_VALUES=1 carries no values (the copy
    // destinations take the Raw/Sync policies as before); APS5_VERIFY_KNOWN_VALUES=1 serves them
    // and reads through the hook as well, counting the mismatches on the [copy] line.
    static bool CopyKnownValues() {
        static const bool enabled = std::getenv("APS5_NO_COPY_KNOWN_VALUES") == nullptr;
        return enabled;
    }
    static bool KnownValueVerify() {
        static const bool verify = std::getenv("APS5_VERIFY_KNOWN_VALUES") != nullptr;
        return verify;
    }
    static constexpr std::size_t KnownValueMaxBytes = 4096;
    static inline std::atomic<std::uint64_t> knownValueEntries{0}, knownValueReads{0}, knownValueVerified{0}, knownValueMismatches{0};

    bool copyBuffer(QueueState& queue, std::uint32_t queueId, std::span<const std::uint32_t> packet, std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& userData, const ShaderRecompiler::ShaderComputeStageInfo& compute, const std::shared_ptr<VulkanDevice>& localDevice, std::uint64_t programAddress) {
        if (!matchesCopyKernel(code, userData, compute)) return false;
        const auto started = std::chrono::steady_clock::now();
        static const std::size_t cpuMax = [] {
            if (std::getenv("APS5_NO_CPU_COPY") != nullptr) return std::size_t{0};
            const char* text = std::getenv("APS5_COPY_HLE_CPU_MAX");
            return text ? static_cast<std::size_t>(std::strtoull(text, nullptr, 10)) : std::size_t{262144};
        }();
        static const std::size_t gpuMax = [] {
            const char* text = std::getenv("APS5_COPY_HLE_GPU_MAX");
            return text ? static_cast<std::size_t>(std::strtoull(text, nullptr, 10)) : std::numeric_limits<std::size_t>::max();
        }();
        const auto base = [&](std::size_t word) { return userData[word] | (static_cast<std::uint64_t>(userData[word + 1] & 0xffffu) << 32u); };
        const auto source = base(0);
        const auto destination = base(4);
        const auto record = base(8);
        std::array<std::uint32_t, 3> groups{packet[1], packet[2], packet[3]};
        if ((packet[4] & 0x20u) != 0) {
            for (std::uint32_t axis = 0; axis < 3; ++axis) {
                const auto threads = std::max(readRegister(queue.shader, 0x207 + axis) & 0xffffu, 1u);
                groups[axis] = (groups[axis] + threads - 1) / threads;
            }
        }
        if (groups[1] != 1 || groups[2] != 1) {
            countCopy(CopyShape, 0, started, started);
            return false;
        }
        GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Copy);
        std::lock_guard gpuLock(GuestMemory::GpuMutex());
        const auto locked = std::chrono::steady_clock::now();
        // This queue's labels first (queue order), as at every dispatch's lock.
        recordLabelsForPacket(localDevice.get(), queueId);
        // Batches the GPU already finished are retired first (as FillBuffer does), so a writer of
        // the record that is long done does not count as pending below, and CopyBuffer sees the
        // recorder idle when it is. Only on the graphics worker: a reap runs completions under the
        // mutex, which a compute worker must never do against queue 0 (see WriteLabelOnGpu).
        if (queueId == 0) localDevice->ReapRecorded();
        // count and n (the record's first two dwords; the title keeps a GPU-written label in its
        // second half) are read raw, so only when no recorded work, pending image result or label
        // may still change them (a read through the flush hook would wait for it).
        const auto labelPending = [&](std::uint64_t address, std::size_t bytes) {
            for (auto dword = address & ~std::uint64_t{3}; dword < address + bytes; dword += 4) {
                if (localDevice->PendingLabel(dword, 4, 0).has_value()) return true;
            }
            return false;
        };
        const bool recordAccessible = GuestMemory::Accessible(reinterpret_cast<const void*>(record), 8);
        // A publish of shadowed results over the record is a recorded store like a flush: the raw
        // read below would miss it.
        bool published = false;
        if (!recordAccessible || Graphics::Recorder::SnapshotWriteOverlaps(record, 8) || Graphics::StorageTexture::FlushPending(record, 8, nullptr, "buffer copy count", Graphics::PublishScope::Whole, &published) || published || labelPending(record, 8)) {
            if (traceCopy()) traceCopyPending(recordAccessible ? "count record" : "inaccessible count record", record, 8, source, destination, 0, localDevice);
            countCopy(CopyRecordPending, 0, started, locked);
            return false;
        }
        std::uint32_t count = 0, period = 0;
        std::memcpy(&count, reinterpret_cast<const void*>(record), 4);
        std::memcpy(&period, reinterpret_cast<const void*>(record + 4), 4);
        const auto bytes = static_cast<std::size_t>(count) * 4u;
        // A plain copy only: n >= count (no repetition), every record inside both V#s, exactly the
        // groups for the count, at most 16 MiB, dword-aligned ranges, and the count
        // record outside the destination (every wave of the kernel re-reads it after other waves
        // stored, while the HLE reads it once).
        if (count == 0 || period < count || groups[0] != (count + 63u) / 64u || count > userData[2] || count > userData[6] || bytes > (16u << 20u) || source % 4 != 0 || destination % 4 != 0 || (record < destination + bytes && destination < record + 8)) {
            countCopy(CopyShape, bytes, started, locked);
            return false;
        }
        if (source < destination + bytes && destination < source + bytes) {
            countCopy(CopyOverlapping, bytes, started, locked);
            return false;
        }
        if (!GuestMemory::Accessible(reinterpret_cast<const void*>(source), bytes) || !GuestMemory::Accessible(reinterpret_cast<const void*>(destination), bytes, true)) {
            countCopy(CopyInaccessible, bytes, started, locked);
            return false;
        }
        // The CPU decision and, failing it, the flushes of pending image results and the transfer;
        // a transfer's ring entry (with the known destination bytes when the source is current)
        // is noted ahead of its pending write, as noteWrittenBuffers precedes a dispatch's note.
        const auto outcome = localDevice->CopyBuffer(destination, source, bytes, cpuMax, gpuMax, CopyKnownValues() ? KnownValueMaxBytes : 0, programAddress, queueId, [&](std::span<const std::byte> value, std::uint64_t generation) { noteCopyWriter(programAddress, destination, destination + bytes, queueId, value, generation); });
        countCopy(outcome.path, bytes, started, locked, &outcome);
        if (traceCopy() && bytes <= cpuMax && outcome.path != CopyCpu) traceCopyRefused(outcome, source, destination, bytes, localDevice);
        if (outcome.path == CopyNotImported || outcome.path == CopyOverGpuMax) return false;
        // A CPU copy leaves no pending write, so the evidence ring's dword streaks under the
        // kernel's program must not carry over to a later GPU copy of the same record: a foreign
        // entry makes the next pending write over it sync until a dispatch element writes it again.
        if (outcome.path == CopyCpu) noteForeignWriter(destination, destination + bytes, queueId);
        return true;
    }

    // The [copyhle] trace of a small copy that missed the CPU path: the refusing rule and, for a
    // reader, the batch that reads the destination in place; for a pending range, its writer.
    void traceCopyRefused(const VulkanDevice::CopyOutcome& outcome, std::uint64_t source, std::uint64_t destination, std::size_t bytes, const std::shared_ptr<VulkanDevice>& localDevice) {
        using Outcome = VulkanDevice::CopyOutcome;
        static constexpr const char* rules[Outcome::Refusals] = {"none", "size", "pending image", "source pending", "source unsettled", "destination pending", "destination unsettled", "label", "reader", "shadowed"};
        static constexpr const char* readers[] = {"dispatch element", "gpu copy", "address-based", "indirect", "storage upload", "copy source"};
        if (outcome.reason == Outcome::SourcePending || outcome.reason == Outcome::SourceUnsettled) {
            traceCopyPending(rules[outcome.reason], source, bytes, source, destination, bytes, localDevice);
            return;
        }
        if (outcome.reason == Outcome::DestinationPending || outcome.reason == Outcome::DestinationUnsettled || outcome.reason == Outcome::Label) {
            traceCopyPending(rules[outcome.reason], destination, bytes, source, destination, bytes, localDevice);
            return;
        }
        static std::atomic<int> shown{0};
        if (shown.fetch_add(1) >= 160 || outcome.reason <= Outcome::None || outcome.reason >= Outcome::Refusals) return;
        if (outcome.reason == Outcome::Reader) {
            const auto kind = outcome.readerKind >= 0 && outcome.readerKind < 6 ? readers[outcome.readerKind] : "?";
            std::fprintf(stderr, "[copyhle] reader refused the CPU copy 0x%llx -> 0x%llx (0x%zx bytes): batch %llu%s of queue 0x%x reads the destination in place (%s)\n", static_cast<unsigned long long>(source), static_cast<unsigned long long>(destination), bytes, static_cast<unsigned long long>(outcome.readerSerial), outcome.readerOpen ? " (open)" : "", outcome.readerQueue, kind);
            return;
        }
        std::fprintf(stderr, "[copyhle] %s refused the CPU copy 0x%llx -> 0x%llx (0x%zx bytes)\n", rules[outcome.reason], static_cast<unsigned long long>(source), static_cast<unsigned long long>(destination), bytes);
    }

    // Debug aid: APS5_TRACE_COPY_HLE=1 prints the first copies the HLE could not make on the CPU
    // because of a pending write ([copyhle] lines): the range, the copy, the newest recorded batch
    // writing it (as the flush hook would see it), pending label dwords in it, and the evidence
    // ring's writers of it.
    static bool traceCopy() {
        static const bool enabled = std::getenv("APS5_TRACE_COPY_HLE") != nullptr;
        return enabled;
    }
    void traceCopyPending(const char* what, std::uint64_t address, std::size_t bytes, std::uint64_t source, std::uint64_t destination, std::size_t copyBytes, const std::shared_ptr<VulkanDevice>& localDevice) {
        static std::atomic<int> shown{0};
        if (shown.fetch_add(1) >= 160) return;
        char pending[160];
        std::snprintf(pending, sizeof(pending), " no recorded write");
        if (auto* recorder = Graphics::Recorder::Active()) {
            if (const auto info = recorder->DescribePendingWrite(address, bytes)) {
                std::snprintf(pending, sizeof(pending), " batch %llu%s%s range 0x%llx+0x%llx (%zu batches to finish)", static_cast<unsigned long long>(info->serial), info->open ? " open" : "", info->signaled ? " signaled" : " unsignaled", static_cast<unsigned long long>(info->rangeBegin), static_cast<unsigned long long>(info->rangeEnd - info->rangeBegin), info->batchesToFinish);
            }
        }
        std::uint32_t labels = 0;
        for (auto dword = address & ~std::uint64_t{3}; dword < address + bytes; dword += 4) {
            if (localDevice->PendingLabel(dword, 4, 0).has_value()) ++labels;
        }
        std::fprintf(stderr, "[copyhle] %s 0x%llx+0x%zx pending (copy 0x%llx -> 0x%llx, 0x%zx bytes):%s; label dwords %u; writers:%s\n", what, static_cast<unsigned long long>(address), bytes, static_cast<unsigned long long>(source), static_cast<unsigned long long>(destination), copyBytes, pending, labels, describeWriters(address, address + bytes).c_str());
    }

    // How DISPATCH_INDIRECT group counts were resolved, for the [indirect] line (APS5_PROFILE_DRAW,
    // every 10 s). 0..3 are VulkanDevice::DispatchIndirect's outcomes (0 recorded GPU-side), the rest
    // are decided here before the dispatch is prepared. `readMs` is the CPU read's time (its sync).
    enum IndirectPath { IndirectGpu = 0, IndirectPendingImage = 1, IndirectCopiedWrite = 2, IndirectNotImported = 3, IndirectThreadDimensions = 4, IndirectFillKernel = 5, IndirectMisaligned = 6, IndirectDisabled = 7, IndirectPaths = 8 };
    static void countIndirect(int path, double readMs) {
        static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        if (!profile || path < 0 || path >= IndirectPaths) return;
        static std::mutex countsMutex;
        static std::uint64_t counts[IndirectPaths] = {};
        static double readWaitedMs = 0;
        static auto lastReport = std::chrono::steady_clock::now();
        std::lock_guard lock(countsMutex);
        ++counts[path];
        readWaitedMs += readMs;
        const auto now = std::chrono::steady_clock::now();
        if (now - lastReport < std::chrono::seconds(10)) return;
        lastReport = now;
        std::uint64_t cpu = 0;
        for (int i = 1; i < IndirectPaths; ++i) cpu += counts[i];
        std::fprintf(stderr, "[indirect] gpu-side %llu, cpu-side %llu (thread dimensions %llu, fill kernel %llu, pending image results %llu, pending label or copied write %llu, not imported %llu, misaligned %llu, disabled %llu); CPU argument reads took %.1f s\n", static_cast<unsigned long long>(counts[IndirectGpu]), static_cast<unsigned long long>(cpu), static_cast<unsigned long long>(counts[IndirectThreadDimensions]), static_cast<unsigned long long>(counts[IndirectFillKernel]), static_cast<unsigned long long>(counts[IndirectPendingImage]), static_cast<unsigned long long>(counts[IndirectCopiedWrite]), static_cast<unsigned long long>(counts[IndirectNotImported]), static_cast<unsigned long long>(counts[IndirectMisaligned]), static_cast<unsigned long long>(counts[IndirectDisabled]), readWaitedMs / 1000);
    }

    // Recorded GPU writes over captured words. The recompiler marks a V# element written when the
    // program stores through it anywhere, so a small descriptor or SRT table a kernel binds that
    // way is a pending write for every capture and dispatch-cache validation that reads it, and
    // the flush hook waited for the kernel's batch before each such read although the kernel never
    // changed the dwords read (thousands of syncs per run, all unchanged). The elements a dispatch
    // marks written are remembered (writtenBuffers, newest first), and every read that does wait on
    // one compares its dwords before and after the wait (dwordEvidence). A dword that came out
    // unchanged from its last WriteEvidenceAfter waits is then read without the wait, raw, as long
    // as the newest recorded writer of it is a dispatch's element of at most WriteEvidenceMaxBytes,
    // the pending range is that element (a larger one, a fill, shows at the element's edges) and
    // no GPU label is pending in it: ShaderMemory reads the dword raw (classifyPendingWrite) and
    // the dispatch-cache validation compares the captured words raw (validateCaptured). Every
    // WriteEvidenceSampleEvery-th capture or validation that could read raw waits and observes
    // anyway, so a kernel that starts changing a dword is noticed and its reads wait again (the
    // raw reads before that saw the old bytes). The validation also declares a miss without
    // waiting when the CPU-visible words already differ from the capture (a miss recaptures
    // through the hook, so that is always safe).
    // A streak belongs to the writer it was observed under (program and element): memory reused
    // by another kernel's element starts over. A recorded write the ring knows no program for (a
    // fill, a draw's written elements: program 0, "foreign") always syncs. An observation counts
    // only when the hook waited for unfinished GPU work across the read (Recorder::ThreadHookWaits):
    // against finished work both reads hold the GPU's bytes and "unchanged" proves nothing.
    // Debug aids: APS5_NO_WRITE_EVIDENCE=1 neither observes nor skips (APS5_NO_VALIDATE_SKIP=1
    // keeps the validation's compares and misses synced, APS5_NO_WORDWISE_CAPTURE=1 the page reads
    // whole); APS5_VALIDATE_SYNC_SKIP=1 makes every skipped read wait anyway and counts the ones
    // whose raw bytes differed from the waited ones ([validate] line, 10 s; the [capture] line for
    // dword reads); APS5_TRACE_CAPSYNC=1 prints every compare and capture that met a pending write,
    // with the recorded writers ([capsync]); APS5_NO_WRITER_KEYED_EVIDENCE=1 keys the streaks by
    // address alone (and notes a dispatch's elements after its record, as before);
    // APS5_NO_FOREIGN_WRITERS=1 leaves fills and draws out of the ring; APS5_NO_OBSERVATION_GUARD=1
    // observes every hook read and every pending region of a validation; APS5_NO_SITE_SAMPLING=1
    // samples captures and validations from one shared count.
    // A copy HLE transfer whose source was CPU-current at its note enters the ring with the
    // destination's post-copy bytes (`value`, at most KnownValueMaxBytes) and the write-watch
    // generation MarkWritten(destination) stamped: while UnchangedSince(destination, generation)
    // holds (a memo'd collect first, so a later CPU store is seen) the newest such writer answers
    // a read with the bytes the GPU will leave (KnownValue), before the evidence and without the
    // sample, since the copy reads a source nothing recorded changes; a later ring entry over the
    // dword supersedes it by serial as any writer does.
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
        // The newest writer of the dword when the streak was observed.
        std::uint64_t program = 0;
        std::uint64_t begin = 0;
        std::uint64_t end = 0;
    };
    // Every WriteEvidenceSampleEvery-th capture, and apart from those every
    // WriteEvidenceSampleEvery-th validation, that could read raw waits and observes instead
    // (SampledReadScope brackets the capture; the validation sets it itself).
    std::atomic<std::uint64_t> evidenceReads{0};
    std::atomic<std::uint64_t> evidenceValidations{0};
    static bool& SampledRead() {
        static thread_local bool sampled = false;
        return sampled;
    }
    class SampledReadScope {
    public:
        explicit SampledReadScope(std::atomic<std::uint64_t>& reads) : previous(SampledRead()) {
            SampledRead() = WriteEvidenceEnabled() && reads.fetch_add(1, std::memory_order_relaxed) % WriteEvidenceSampleEvery() == 0;
        }
        ~SampledReadScope() { SampledRead() = previous; }
        SampledReadScope(const SampledReadScope&) = delete;
        SampledReadScope& operator=(const SampledReadScope&) = delete;

    private:
        bool previous;
    };
    static constexpr std::size_t WrittenBufferRing = 4096;
    static constexpr std::size_t DwordEvidenceEntries = 65536;
    std::mutex writtenBuffersMutex;
    std::deque<WrittenBuffer> writtenBuffers;
    std::uint64_t writtenBufferSerial = 0;
    std::unordered_map<std::uint64_t, DwordEvidence> dwordEvidence;
    std::atomic<std::uint64_t> observedUnchanged{0};
    std::atomic<std::uint64_t> observedChanged{0};
    // Pending regions of validations not observed: compared after no GPU wait, or not compared
    // at all (a region before them differed).
    std::atomic<std::uint64_t> observationsNoWait{0};
    std::atomic<std::uint64_t> observationsNotReached{0};
    struct ValidateCounters {
        std::uint64_t pending = 0, unsyncedMisses = 0, skipped = 0, syncedNoWriter = 0, syncedForeign = 0, syncedLargeRange = 0, syncedLabel = 0, syncedEvidence = 0, syncedWriterChanged = 0, syncedSample = 0, syncedImage = 0, syncedShadow = 0, syncedOff = 0, verified = 0, mismatches = 0, verifiedMissesHit = 0, knownValue = 0;
        double syncedWaitMs = 0, verifiedWaitMs = 0;
        std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
    };
    std::mutex validateMutex;
    ValidateCounters validateCounters;

    static bool WriteEvidenceEnabled() {
        static const bool enabled = std::getenv("APS5_NO_WRITE_EVIDENCE") == nullptr;
        return enabled;
    }
    static bool WriterKeyedEvidence() {
        static const bool keyed = std::getenv("APS5_NO_WRITER_KEYED_EVIDENCE") == nullptr;
        return keyed;
    }
    static bool ForeignWriters() {
        static const bool noted = std::getenv("APS5_NO_FOREIGN_WRITERS") == nullptr;
        return noted;
    }
    static bool ObservationGuard() {
        static const bool guard = std::getenv("APS5_NO_OBSERVATION_GUARD") == nullptr;
        return guard;
    }
    static bool SiteSampling() {
        static const bool perSite = std::getenv("APS5_NO_SITE_SAMPLING") == nullptr;
        return perSite;
    }
    static std::uint64_t HookWaits() {
        return Graphics::Recorder::ThreadHookWaits();
    }
    // ShaderMemory's hook-wait counter (null under APS5_NO_OBSERVATION_GUARD: every read observed).
    static ShaderMemory::HookWaitCounter HookWaitCounter() {
        return ObservationGuard() ? &HookWaits : nullptr;
    }
    static bool ValidateSkipEnabled() {
        static const bool enabled = std::getenv("APS5_NO_VALIDATE_SKIP") == nullptr;
        return enabled;
    }
    // Debug aid: APS5_VALIDATE_LEGACY=1 restores the value validation's old compare path: the
    // per-run Accessible walk and EqualsCommittedUnsynced (each with its profile counters) instead
    // of one CompareMapped per run, a pending-write snapshot load per run and per edge test instead
    // of one per validation, and no retry of the gate after a moved serial.
    static bool ValidateLegacy() {
        static const bool legacy = std::getenv("APS5_VALIDATE_LEGACY") != nullptr;
        return legacy;
    }
    static bool GateRetry() {
        static const bool retry = std::getenv("APS5_NO_GATE_RETRY") == nullptr && !ValidateLegacy();
        return retry;
    }
    // A validation's view of the recorder's pending-write snapshot: loaded once (Load) and consulted
    // for every run and edge test, so all of them see the same snapshot (design13 R1's p0: loaded
    // after the gate's generation load, a later publish fails the generation re-check). A reload
    // is skipped while the publish generation read before the load has not moved (a publish
    // stores its snapshot before bumping it, so the loaded one is the newest stored or the gate's
    // re-check catches the bump): the atomic shared_ptr load is a locked RMW on a line every queue
    // worker shares. Under the legacy switch every query loads the current snapshot as before.
    struct PendingView {
        std::shared_ptr<const Graphics::Recorder::WriteRanges> snapshot;
        std::uint64_t generation = 0;
        bool loaded = false;

        void Load() {
            if (ValidateLegacy()) return;
            const auto current = Graphics::Recorder::PublishGeneration();
            if (loaded && current == generation) return;
            snapshot = Graphics::Recorder::PendingWriteSnapshot();
            generation = current;
            loaded = true;
        }
        bool Overlaps(std::uint64_t address, std::size_t bytes) const {
            return ValidateLegacy() ? Graphics::Recorder::SnapshotWriteOverlaps(address, bytes) : Graphics::Recorder::SnapshotOverlaps(snapshot.get(), address, bytes);
        }
    };
    static bool ValidateSkipVerify() {
        static const bool verify = std::getenv("APS5_VALIDATE_SYNC_SKIP") != nullptr;
        return verify;
    }
    static std::uint32_t WriteEvidenceAfter() {
        static const std::uint32_t value = [] { const char* text = std::getenv("APS5_WRITE_EVIDENCE_AFTER"); return text ? static_cast<std::uint32_t>(std::atoi(text)) : 4u; }();
        return value;
    }
    static std::uint32_t WriteEvidenceSampleEvery() {
        static const std::uint32_t value = [] { const char* text = std::getenv("APS5_WRITE_EVIDENCE_SAMPLE"); const auto parsed = text ? std::atoi(text) : 16; return parsed > 0 ? static_cast<std::uint32_t>(parsed) : 16u; }();
        return value;
    }
    static std::uint64_t WriteEvidenceMaxBytes() {
        static const std::uint64_t value = [] { const char* text = std::getenv("APS5_WRITE_EVIDENCE_MAX_KIB"); return (text ? std::strtoull(text, nullptr, 10) : 16ull) << 10u; }();
        return value;
    }
    static bool TraceCapSync() {
        static const bool trace = std::getenv("APS5_TRACE_CAPSYNC") != nullptr;
        return trace;
    }
    // The written V# elements of a compiled program, as the resource build notes them (bufferWritten).
    template<typename Visit>
    static void forEachWrittenBuffer(const ShaderRecompiler::RecompileResult& compiled, Visit&& visit) {
        for (const auto& binding : compiled.bindings) {
            if (binding.role != ShaderRecompiler::DescriptorRole::GuestBuffers) continue;
            for (std::uint32_t element = 0; element < binding.count; ++element) {
                const bool written = element >= binding.bufferWritten.size() || binding.bufferWritten[element];
                if (!written || binding.guestDescriptor.size() < (static_cast<std::size_t>(element) + 1) * 4) continue;
                const auto words = std::span<const std::uint32_t>(binding.guestDescriptor).subspan(static_cast<std::size_t>(element) * 4, 4);
                const ShaderRecompiler::ShaderBufferResource descriptor{{words[0], words[1], words[2], words[3]}};
                const auto base = descriptor.Base48();
                const auto size = descriptor.GetSize();
                if (base == 0 || size == 0) continue;
                visit(element, base, base + size, element < binding.bufferAtomic.size() && binding.bufferAtomic[element]);
            }
        }
    }
    // As a dispatch is recorded (under GuestMemory::GpuMutex, before its record, so a
    // classification never finds the previous writer under a note that already includes it):
    // its written elements join the ring.
    void noteWrittenBuffers(std::uint64_t program, std::uint32_t queue, const ShaderRecompiler::RecompileResult& compiled) {
        std::lock_guard lock(writtenBuffersMutex);
        const auto serial = ++writtenBufferSerial;
        forEachWrittenBuffer(compiled, [&](std::uint32_t, std::uint64_t begin, std::uint64_t end, bool atomic) {
            writtenBuffers.push_back({program, begin, end, serial, queue, atomic});
        });
        while (writtenBuffers.size() > WrittenBufferRing) writtenBuffers.pop_front();
    }
    // A recorded write the evidence does not cover (a fill, a draw's written elements): a foreign
    // entry (program 0) of the ring, so a dword under it goes through the hook until a dispatch
    // writes it again.
    void noteForeignWriter(std::uint64_t begin, std::uint64_t end, std::uint32_t queue) {
        if (!ForeignWriters() || !(WriteEvidenceEnabled() || TraceCapSync()) || end <= begin) return;
        std::lock_guard lock(writtenBuffersMutex);
        writtenBuffers.push_back({0, begin, end, ++writtenBufferSerial, queue, false});
        while (writtenBuffers.size() > WrittenBufferRing) writtenBuffers.pop_front();
    }
    void noteDrawWriters(std::span<const Graphics::CompiledShader> stages, std::uint32_t queue) {
        for (const auto& stage : stages) {
            forEachWrittenBuffer(*stage.program, [&](std::uint32_t, std::uint64_t begin, std::uint64_t end, bool) { noteForeignWriter(begin, end, queue); });
        }
    }
    std::optional<WrittenBuffer> newestWriterLocked(std::uint64_t begin, std::uint64_t end) const {
        for (auto it = writtenBuffers.rbegin(); it != writtenBuffers.rend(); ++it) {
            if (begin < it->end && it->begin < end) return *it;
        }
        return std::nullopt;
    }
    std::optional<WrittenBuffer> newestWriter(std::uint64_t begin, std::uint64_t end) {
        std::lock_guard lock(writtenBuffersMutex);
        return newestWriterLocked(begin, end);
    }
    // A read that waited: whether its dword still holds the bytes read before the wait.
    void observeDword(std::uint64_t address, bool unchanged) {
        if (!WriteEvidenceEnabled()) return;
        (unchanged ? observedUnchanged : observedChanged).fetch_add(1, std::memory_order_relaxed);
        std::lock_guard lock(writtenBuffersMutex);
        if (dwordEvidence.size() >= DwordEvidenceEntries && !dwordEvidence.contains(address)) dwordEvidence.clear();
        auto& evidence = dwordEvidence[address];
        if (WriterKeyedEvidence()) {
            const auto writer = newestWriterLocked(address, address + 4);
            const std::uint64_t program = writer ? writer->program : 0, begin = writer ? writer->begin : 0, end = writer ? writer->end : 0;
            if (program != evidence.program || begin != evidence.begin || end != evidence.end) evidence = DwordEvidence{0, 0, program, begin, end};
        }
        if (!unchanged) {
            evidence.streak = 0;
            ++evidence.changed;
        } else if (evidence.streak < (1u << 30u)) {
            ++evidence.streak;
        }
    }
    void observeRange(std::uint64_t address, std::span<const std::byte> before) {
        if (before.empty() || !GuestMemory::Accessible(reinterpret_cast<const void*>(address), before.size())) return;
        const auto* now = reinterpret_cast<const std::byte*>(address);
        for (std::size_t offset = 0; offset + 4 <= before.size(); offset += 4) {
            observeDword(address + offset, std::memcmp(before.data() + offset, now + offset, 4) == 0);
        }
    }
    static void ObservePendingWrite(std::uint64_t address, bool unchanged) {
        Get().observeDword(address, unchanged);
    }
    // Whether a known-value entry's bytes are still what the GPU leaves: no store over the
    // destination since the copy's stamp (a game store not collected yet becomes a stamp first).
    static bool knownValueCurrent(const WrittenBuffer& writer) {
        const auto bytes = static_cast<std::size_t>(writer.end - writer.begin);
        GuestMemory::CollectWrites(writer.begin, bytes);
        return GuestMemory::UnchangedSince(writer.begin, bytes, writer.generation);
    }
    // The policy for a read of [address, address + bytes) (ShaderMemory's query, the validation):
    // None when nothing recorded writes it; KnownValue when its newest recorded writer is a copy
    // HLE entry carrying the destination's bytes that is still current (copied into `known` when
    // the caller passes a span of `bytes`; an empty span asks for the answer alone) and no GPU
    // label is pending in it; Raw when its newest recorded writer is a dispatch's small element,
    // the pending range is that element, no GPU label is pending in it and every dword has an
    // unchanged streak observed under that writer, unless this is one of the sampled reads;
    // VerifyRaw/VerifyKnownValue under the verify switches; Sync otherwise, with `reason` naming
    // why for the [validate] line.
    ShaderMemory::PendingWrite classifyPendingWrite(std::uint64_t address, std::size_t bytes, std::uint64_t ValidateCounters::*& reason, const PendingView& pending, std::span<std::byte> known = {}) {
        using Policy = ShaderMemory::PendingWrite;
        if (!pending.Overlaps(address, bytes)) return Policy::None;
        // Shadowed results over the range are not in the import's bytes: a raw read would miss
        // them, the hook's read publishes them first.
        if (Graphics::AnyShadowedOverlaps(address, bytes)) {
            reason = &ValidateCounters::syncedShadow;
            return Policy::Sync;
        }
        reason = &ValidateCounters::syncedOff;
        if (!WriteEvidenceEnabled()) return Policy::Sync;
        const auto writer = newestWriter(address, address + bytes);
        if (!writer) {
            reason = &ValidateCounters::syncedNoWriter;
            return Policy::Sync;
        }
        if (writer->program == 0) {
            reason = &ValidateCounters::syncedForeign;
            return Policy::Sync;
        }
        if (writer->end - writer->begin > WriteEvidenceMaxBytes() || writer->begin < 4 || pending.Overlaps(writer->begin - 4, 4) || pending.Overlaps(writer->end, 4)) {
            reason = &ValidateCounters::syncedLargeRange;
            return Policy::Sync;
        }
        const auto first = address & ~3ull;
        const auto limit = address + bytes;
        for (std::uint64_t dword = first; dword < limit; dword += 4) {
            if (Graphics::Recorder::LookupLabelValue(dword, 4, 0).has_value()) {
                reason = &ValidateCounters::syncedLabel;
                return Policy::Sync;
            }
        }
        if (writer->value && writer->begin <= address && limit <= writer->end && (known.empty() || known.size() == bytes) && knownValueCurrent(*writer)) {
            if (!known.empty()) {
                std::memcpy(known.data(), writer->value->data() + (address - writer->begin), bytes);
                knownValueReads.fetch_add(1, std::memory_order_relaxed);
            }
            return KnownValueVerify() ? Policy::VerifyKnownValue : Policy::KnownValue;
        }
        {
            std::lock_guard lock(writtenBuffersMutex);
            for (std::uint64_t dword = first; dword < limit; dword += 4) {
                const auto found = dwordEvidence.find(dword);
                if (found == dwordEvidence.end() || found->second.streak < WriteEvidenceAfter()) {
                    reason = &ValidateCounters::syncedEvidence;
                    return Policy::Sync;
                }
                if (WriterKeyedEvidence() && (found->second.program != writer->program || found->second.begin != writer->begin || found->second.end != writer->end)) {
                    reason = &ValidateCounters::syncedWriterChanged;
                    return Policy::Sync;
                }
            }
        }
        if (SampledRead()) {
            reason = &ValidateCounters::syncedSample;
            return Policy::Sync;
        }
        return ValidateSkipVerify() ? Policy::VerifyRaw : Policy::Raw;
    }
    static ShaderMemory::PendingWrite ClassifyPendingWrite(std::uint64_t address, std::size_t bytes, std::span<std::byte> known) {
        std::uint64_t ValidateCounters::*reason = nullptr;
        PendingView pending;
        pending.Load();
        return Get().classifyPendingWrite(address, bytes, reason, pending, known);
    }
    std::string describeWriters(std::uint64_t begin, std::uint64_t end) {
        std::string text;
        std::lock_guard lock(writtenBuffersMutex);
        int shown = 0;
        for (auto it = writtenBuffers.rbegin(); it != writtenBuffers.rend() && shown < 3; ++it) {
            if (begin >= it->end || it->begin >= end) continue;
            char item[128];
            if (it->program == 0) std::snprintf(item, sizeof(item), " [foreign q0x%x 0x%llx+0x%llx age %llu]", it->queue, static_cast<unsigned long long>(it->begin), static_cast<unsigned long long>(it->end - it->begin), static_cast<unsigned long long>(writtenBufferSerial - it->serial));
            else std::snprintf(item, sizeof(item), " [0x%llx q0x%x 0x%llx+0x%llx%s%s age %llu]", static_cast<unsigned long long>(it->program), it->queue, static_cast<unsigned long long>(it->begin), static_cast<unsigned long long>(it->end - it->begin), it->atomic ? " atomic" : "", it->value ? " known" : "", static_cast<unsigned long long>(writtenBufferSerial - it->serial));
            text += item;
            ++shown;
        }
        if (shown == 0) return " none";
        std::uint32_t streakMin = 0xffffffffu, streakMax = 0, changed = 0, unseen = 0;
        for (std::uint64_t dword = begin & ~3ull; dword < end; dword += 4) {
            const auto found = dwordEvidence.find(dword);
            if (found == dwordEvidence.end()) {
                ++unseen;
                continue;
            }
            streakMin = std::min(streakMin, found->second.streak);
            streakMax = std::max(streakMax, found->second.streak);
            changed += found->second.changed;
        }
        char item[96];
        std::snprintf(item, sizeof(item), " dwords: streak %u..%u changed %u unseen %u", streakMin == 0xffffffffu ? 0u : streakMin, streakMax, changed, unseen);
        return text + item;
    }
    static std::string describeSelf(const ShaderRecompiler::RecompileResult& compiled, std::uint64_t begin, std::uint64_t end) {
        std::string text;
        forEachWrittenBuffer(compiled, [&](std::uint32_t element, std::uint64_t base, std::uint64_t limit, bool atomic) {
            if (begin >= limit || base >= end) return;
            char item[96];
            std::snprintf(item, sizeof(item), " own element %u 0x%llx+0x%llx%s", element, static_cast<unsigned long long>(base), static_cast<unsigned long long>(limit - base), atomic ? " atomic" : "");
            text += item;
        });
        return text;
    }
    static bool traceBudget() {
        static std::atomic<int> lines{0};
        return lines.fetch_add(1) < 600;
    }
    // A capture that waited: the captured regions recorded dispatches write, by page (what a
    // whole-page fetch meets) and by the exact dwords.
    void traceCapture(const char* what, std::uint64_t program, std::uint32_t queue, std::span<const ShaderRecompiler::MemoryRegion> regions, double waitedMs) {
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
    void reportValidation(ValidateCounters& counters) {
        const auto count = [](std::uint64_t value) { return static_cast<unsigned long long>(value); };
        std::size_t tracked = 0, eligible = 0;
        {
            std::lock_guard lock(writtenBuffersMutex);
            tracked = dwordEvidence.size();
            for (const auto& [address, evidence] : dwordEvidence) {
                if (evidence.streak >= WriteEvidenceAfter()) ++eligible;
            }
        }
        std::fprintf(stderr, "[validate] dispatch-cache compares over pending GPU writes (10 s): %llu; missed without the sync %llu, skipped the sync %llu, synced %llu waiting %.0f ms (no dispatch writer %llu, foreign writer %llu, larger range %llu, label %llu, evidence short %llu, writer changed %llu, sampled %llu, image %llu, shadow %llu, off %llu); known-value reads %llu; verify: %llu would-skip compares synced anyway (%.0f ms), %llu mismatches, %llu misses a sync made hits; dwords observed over waits: %llu unchanged, %llu changed (regions not observed: %llu after no GPU wait, %llu not compared); %zu tracked, %zu with a streak\n", count(counters.pending), count(counters.unsyncedMisses), count(counters.skipped), count(counters.syncedNoWriter + counters.syncedForeign + counters.syncedLargeRange + counters.syncedLabel + counters.syncedEvidence + counters.syncedWriterChanged + counters.syncedSample + counters.syncedImage + counters.syncedShadow + counters.syncedOff), counters.syncedWaitMs, count(counters.syncedNoWriter), count(counters.syncedForeign), count(counters.syncedLargeRange), count(counters.syncedLabel), count(counters.syncedEvidence), count(counters.syncedWriterChanged), count(counters.syncedSample), count(counters.syncedImage), count(counters.syncedShadow), count(counters.syncedOff), count(counters.knownValue), count(counters.verified), counters.verifiedWaitMs, count(counters.mismatches), count(counters.verifiedMissesHit), count(observedUnchanged.exchange(0)), count(observedChanged.exchange(0)), count(observationsNoWait.exchange(0)), count(observationsNotReached.exchange(0)), tracked, eligible);
        counters = ValidateCounters{};
    }
    // The stability compare of a freshly captured entry (bytes that already differ show a write
    // racing the capture, and nothing is kept): the pending regions the capture just read raw are
    // compared raw as well, the others through the hook as before.
    bool captureStable(std::span<const ShaderRecompiler::MemoryRegion> captured) {
        using Policy = ShaderMemory::PendingWrite;
        static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        std::uint64_t ValidateCounters::*reason = &ValidateCounters::syncedOff;
        bool stable = true;
        std::uint64_t rawRegions = 0, mismatches = 0;
        double waited = 0;
        PendingView pending;
        pending.Load();
        thread_local std::vector<std::byte> known;
        for (const auto& region : captured) {
            if (!stable) break;
            known.resize(region.bytes.size());
            const auto policy = ValidateSkipEnabled() ? classifyPendingWrite(region.guestAddress, region.bytes.size(), reason, pending, known) : Policy::Sync;
            if (policy == Policy::KnownValue || policy == Policy::VerifyKnownValue) {
                stable = std::memcmp(known.data(), region.bytes.data(), known.size()) == 0;
                if (policy != Policy::VerifyKnownValue) continue;
                const bool synced = GuestMemory::EqualsCommitted(region.guestAddress, region.bytes);
                knownValueVerified.fetch_add(1, std::memory_order_relaxed);
                if (synced != stable) knownValueMismatches.fetch_add(1, std::memory_order_relaxed);
                stable = synced;
                continue;
            }
            if (policy != Policy::Raw && policy != Policy::VerifyRaw) {
                stable = GuestMemory::EqualsCommitted(region.guestAddress, region.bytes);
                continue;
            }
            ++rawRegions;
            stable = GuestMemory::EqualsCommittedUnsynced(region.guestAddress, region.bytes);
            if (policy != Policy::VerifyRaw) continue;
            const auto start = profile ? Graphics::Recorder::ThreadWaitedMs() : 0.0;
            const bool synced = GuestMemory::EqualsCommitted(region.guestAddress, region.bytes);
            if (profile) waited += Graphics::Recorder::ThreadWaitedMs() - start;
            if (synced != stable) ++mismatches;
            stable = synced;
        }
        if (!profile || rawRegions == 0) return stable;
        std::lock_guard lock(validateMutex);
        auto& counters = validateCounters;
        ++counters.pending;
        if (ValidateSkipVerify()) {
            ++counters.verified;
            counters.verifiedWaitMs += waited;
            counters.mismatches += mismatches;
        } else {
            ++counters.skipped;
        }
        return stable;
    }
    // Compares a hit entry's captured regions with guest memory (true: unchanged). `inPlace`: the
    // regions no recorded work writes are compared without the flush hook (validateEntry has
    // checked the queued labels and the pending images for all of them already); a region of
    // theirs, or of the raw-compared pending ones, over an unmapped page is a miss that also sets
    // `unmapped` (validateVariant's Inaccessible). `view` is the caller's pending-write snapshot.
    // `sampling`: the caller's evidence sample, drawn here at the first pending region if it has
    // not been (one draw per validation, however many variants it compares). `data` (a variant
    // with data positions): a differing dword of an in-place region at one of its positions
    // (word positions over `captured` in order) is accepted and reported in `live`; the pending
    // and known-value regions keep the exact compare.
    struct DataMask {
        std::span<const std::uint32_t> positions;
        std::vector<std::pair<std::uint32_t, std::uint32_t>>* live;
    };
    bool validateCaptured(std::uint64_t program, std::uint32_t queue, std::span<const ShaderRecompiler::MemoryRegion> captured, const ShaderRecompiler::RecompileResult& compiled, bool inPlace, const PendingView& view, bool* unmapped = nullptr, std::optional<SampledReadScope>* sampling = nullptr, const DataMask* data = nullptr) {
        using Policy = ShaderMemory::PendingWrite;
        // Reused per thread: empty on nearly every hit, so it never allocates then.
        thread_local std::vector<std::size_t> pending;
        pending.clear();
        for (std::size_t i = 0; i < captured.size(); ++i) {
            const auto& region = captured[i];
            if (view.Overlaps(region.guestAddress, region.bytes.size())) pending.push_back(i);
        }
        // The masked re-read of a differing in-place region: its live bytes are copied (the same
        // page-state walk as the compare, inside the caller's gate window) and compared dword by
        // dword; a difference at a data position is recorded, any other is the miss.
        const auto compareMasked = [&](const ShaderRecompiler::MemoryRegion& region, std::size_t first) {
            thread_local std::vector<std::byte> live;
            live.resize(region.bytes.size());
            const auto copied = GuestMemory::CopyMapped(region.guestAddress, live);
            if (copied != GuestMemory::Compare::Equal) return copied;
            const auto count = region.bytes.size() / sizeof(std::uint32_t);
            for (std::size_t i = 0; i < count; ++i) {
                std::uint32_t stored = 0, fresh = 0;
                std::memcpy(&stored, region.bytes.data() + i * sizeof(std::uint32_t), sizeof(stored));
                std::memcpy(&fresh, live.data() + i * sizeof(std::uint32_t), sizeof(fresh));
                if (stored == fresh) continue;
                const auto position = static_cast<std::uint32_t>(first + i);
                if (!std::binary_search(data->positions.begin(), data->positions.end(), position)) return GuestMemory::Compare::Differs;
                data->live->emplace_back(position, fresh);
            }
            return GuestMemory::Compare::Equal;
        };
        // Only the pending regions bypass the hook in a raw compare; the others keep the hook's
        // image flush (they overlap no noted write, so it never waits for them) unless the caller
        // compares in place. A pending region with a known value (`knownOffsets`, its bytes in
        // `knownBytes`) is compared against that value in both passes, never through the hook (the
        // value is what the GPU leaves; under the verify switch the hook's answer is taken too and
        // a disagreement counted). Of the pending ones a hook compare reached (a differing region
        // ends the compare), `observable` are those compared after a GPU wait on this thread since
        // the copies below were taken: the only ones whose bytes before and after say anything.
        static constexpr std::size_t NoKnownValue = std::numeric_limits<std::size_t>::max();
        thread_local std::vector<Policy> policies;
        thread_local std::vector<std::size_t> knownOffsets;
        thread_local std::vector<std::byte> knownBytes;
        policies.assign(pending.size(), Policy::None);
        knownOffsets.assign(pending.size(), NoKnownValue);
        knownBytes.clear();
        std::uint64_t knownServed = 0, knownMismatches = 0;
        std::vector<std::size_t> observable;
        std::size_t reachedPending = 0;
        std::uint64_t hookWaitsAtCopy = 0;
        const auto compare = [&](bool rawPending) {
            std::size_t next = 0;
            std::size_t first = 0;
            observable.clear();
            reachedPending = 0;
            if (data != nullptr) data->live->clear();
            for (std::size_t i = 0; i < captured.size(); ++i) {
                const auto& region = captured[i];
                const bool isPending = next < pending.size() && pending[next] == i;
                if (isPending) ++next;
                const bool unsynced = isPending ? rawPending : inPlace;
                const bool known = isPending && knownOffsets[next - 1] != NoKnownValue;
                bool same = false;
                bool masked = false;
                if (known) {
                    same = std::memcmp(knownBytes.data() + knownOffsets[next - 1], region.bytes.data(), region.bytes.size()) == 0;
                    if (!rawPending && policies[next - 1] == Policy::VerifyKnownValue) {
                        const bool synced = GuestMemory::EqualsCommitted(region.guestAddress, region.bytes);
                        if (synced != same) ++knownMismatches;
                        same = synced;
                    }
                } else if (!unsynced) same = GuestMemory::EqualsCommitted(region.guestAddress, region.bytes);
                else if (ValidateLegacy()) same = GuestMemory::EqualsCommittedUnsynced(region.guestAddress, region.bytes);
                else {
                    auto outcome = GuestMemory::CompareMapped(region.guestAddress, region.bytes);
                    masked = !isPending && data != nullptr;
                    if (outcome == GuestMemory::Compare::Differs && masked) outcome = compareMasked(region, first);
                    if (outcome == GuestMemory::Compare::Unmapped && unmapped != nullptr) *unmapped = true;
                    same = outcome == GuestMemory::Compare::Equal;
                }
                first += region.bytes.size() / sizeof(std::uint32_t);
                if (isPending && !rawPending && !known) {
                    ++reachedPending;
                    if (Graphics::Recorder::ThreadHookWaits() != hookWaitsAtCopy) observable.push_back(next - 1);
                }
                if (!same) {
                    if (data != nullptr && !masked) dataPendingMisses.fetch_add(1, std::memory_order_relaxed);
                    return false;
                }
            }
            return true;
        };
        if (pending.empty()) return compare(false);
        static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        const bool verify = ValidateSkipVerify();
        std::optional<SampledReadScope> ownSampling;
        auto& sample = sampling != nullptr ? *sampling : ownSampling;
        if (!sample) sample.emplace(SiteSampling() ? evidenceValidations : evidenceReads);
        std::uint64_t ValidateCounters::*reason = &ValidateCounters::syncedOff;
        bool skip = ValidateSkipEnabled();
        // The policies first (the known values among them decide what a region is compared
        // against); `reason` is the first sync's.
        for (std::size_t i = 0; ValidateSkipEnabled() && i < pending.size(); ++i) {
            const auto& region = captured[pending[i]];
            std::uint64_t ValidateCounters::*regionReason = &ValidateCounters::syncedOff;
            const auto offset = knownBytes.size();
            knownBytes.resize(offset + region.bytes.size());
            auto policy = classifyPendingWrite(region.guestAddress, region.bytes.size(), regionReason, view, std::span(knownBytes).subspan(offset));
            const bool known = policy == Policy::KnownValue || policy == Policy::VerifyKnownValue;
            if (!known) knownBytes.resize(offset);
            if (policy != Policy::Sync && policy != Policy::None) {
                // A publish of shadowed results is a recorded store over the region like a flush
                // (over a known value too: it lands after the copy).
                bool published = false;
                const bool stored = Graphics::StorageTexture::FlushPending(region.guestAddress, region.bytes.size(), nullptr, "memory access", Graphics::PublishScope::Whole, &published);
                if (stored || published) {
                    regionReason = &ValidateCounters::syncedImage;
                    policy = Policy::Sync;
                    if (known) knownBytes.resize(offset);
                } else if (known) {
                    knownOffsets[i] = offset;
                    ++knownServed;
                }
            }
            policies[i] = policy;
            if (policy == Policy::Sync && skip) {
                skip = false;
                reason = regionReason;
            }
        }
        // The raw compare first: words the CPU already changed are a miss whatever the GPU does.
        const bool rawSame = skip ? compare(true) : false;
        const bool unsyncedMiss = skip && !rawSame;
        if (unsyncedMiss) skip = false;
        const bool verifyKnown = KnownValueVerify() && knownServed != 0;
        double waited = 0;
        bool same = rawSame;
        if (!(skip || unsyncedMiss) || verify || verifyKnown) {
            // The pending regions' bytes before the wait, for the dword evidence.
            std::vector<std::vector<std::byte>> before;
            if (WriteEvidenceEnabled()) {
                before.reserve(pending.size());
                for (const auto index : pending) {
                    const auto& region = captured[index];
                    std::vector<std::byte> copy;
                    if (GuestMemory::Accessible(reinterpret_cast<const void*>(region.guestAddress), region.bytes.size())) {
                        copy.resize(region.bytes.size());
                        std::memcpy(copy.data(), reinterpret_cast<const void*>(region.guestAddress), copy.size());
                    }
                    before.push_back(std::move(copy));
                }
            }
            const auto start = profile ? Graphics::Recorder::ThreadWaitedMs() : 0.0;
            hookWaitsAtCopy = Graphics::Recorder::ThreadHookWaits();
            same = compare(false);
            if (profile) waited = Graphics::Recorder::ThreadWaitedMs() - start;
            if (!ObservationGuard()) {
                for (std::size_t i = 0; i < before.size(); ++i) observeRange(captured[pending[i]].guestAddress, before[i]);
            } else if (!before.empty()) {
                for (const auto index : observable) observeRange(captured[pending[index]].guestAddress, before[index]);
                observationsNoWait.fetch_add(reachedPending - observable.size(), std::memory_order_relaxed);
                observationsNotReached.fetch_add(pending.size() - knownServed - reachedPending, std::memory_order_relaxed);
            }
        }
        if (verifyKnown) {
            knownValueVerified.fetch_add(knownServed, std::memory_order_relaxed);
            knownValueMismatches.fetch_add(knownMismatches, std::memory_order_relaxed);
        }
        if (profile) {
            std::lock_guard lock(validateMutex);
            auto& counters = validateCounters;
            ++counters.pending;
            counters.knownValue += knownServed;
            if (verify && (skip || unsyncedMiss)) {
                ++counters.verified;
                counters.verifiedWaitMs += waited;
                if (skip && !same) ++counters.mismatches;
                if (unsyncedMiss && same) ++counters.verifiedMissesHit;
            } else if (unsyncedMiss) {
                ++counters.unsyncedMisses;
            } else if (skip) {
                ++counters.skipped;
            } else {
                ++(counters.*reason);
                counters.syncedWaitMs += waited;
            }
            if (std::chrono::steady_clock::now() - counters.lastReport > std::chrono::seconds(10)) reportValidation(counters);
        }
        if (TraceCapSync() && traceBudget()) {
            const char* outcome = verify && (skip || unsyncedMiss) ? "verify" : unsyncedMiss ? "missed raw" : skip ? "skipped" : reason == &ValidateCounters::syncedEvidence ? "synced (evidence short)" : reason == &ValidateCounters::syncedSample ? "synced (sampled)" : reason == &ValidateCounters::syncedNoWriter ? "synced (no dispatch writer)" : reason == &ValidateCounters::syncedForeign ? "synced (foreign writer)" : reason == &ValidateCounters::syncedWriterChanged ? "synced (writer changed)" : reason == &ValidateCounters::syncedLargeRange ? "synced (larger range)" : reason == &ValidateCounters::syncedLabel ? "synced (label)" : reason == &ValidateCounters::syncedImage ? "synced (image)" : reason == &ValidateCounters::syncedShadow ? "synced (shadow)" : "synced (off)";
            for (const auto index : pending) {
                const auto& region = captured[index];
                const auto begin = region.guestAddress;
                const auto end = begin + region.bytes.size();
                std::fprintf(stderr, "[capsync] dispatch-cache q0x%x program 0x%llx region 0x%llx+0x%zx (%zu of %zu pending) %s: waited %.1f ms, same %d raw %d;%s; writers:%s\n", queue, static_cast<unsigned long long>(program), static_cast<unsigned long long>(begin), region.bytes.size(), pending.size(), captured.size(), outcome, waited, same ? 1 : 0, rawSame ? 1 : 0, describeSelf(compiled, begin, end).c_str(), describeWriters(begin, end).c_str());
            }
        }
        if (verify || verifyKnown) return same;
        return skip || unsyncedMiss ? rawSame : same;
    }
    // EqualData: equal apart from data words (a hit with the live words in the caller's list).
    enum class EntryOutcome { Equal, EqualData, Differing, Inaccessible, QueuedLabel, FlushingImage, PublishMoved, PendingMoved, ForgetMoved };
    // The captured dwords of a variant as regions over its stored values (or over `words`, a
    // data hit's live words in the same layout), in address order.
    static void appendEntryRegions(const DispatchVariant& variant, std::vector<ShaderRecompiler::MemoryRegion>& regions, const std::vector<std::uint32_t>* words = nullptr) {
        const auto& source = words != nullptr ? *words : variant.words;
        regions.reserve(regions.size() + variant.runs.size());
        std::size_t offset = 0;
        for (const auto& [begin, end] : variant.runs) {
            const auto count = static_cast<std::size_t>((end - begin) / sizeof(std::uint32_t));
            regions.push_back({begin, std::as_bytes(std::span<const std::uint32_t>(source).subspan(offset, count))});
            offset += count;
        }
    }
    // The runs of a variant recorded GPU work still writes whose pending-write policy is a sync:
    // waited for through the flush hook BEFORE the gate below loads its serials, so the sync's
    // own publish precedes them and each such run is then compared in place with the others
    // (compared through validateCaptured after the loads, every such hit ended as PublishMoved).
    // The bytes before the wait feed the dword evidence as validateCaptured's compare does, and
    // under the skip policy a run the CPU already changed is a miss without the wait (false).
    // `pending` is reloaded after every sync (the sync's finish publishes a snapshot without the
    // range), so the later runs are not synced against a stale one. `sampling` is the
    // validation's one evidence sample (validateCaptured), drawn here if a pending run comes first.
    bool syncPendingRuns(std::uint64_t program, std::uint32_t queue, const ShaderRecompiler::RecompileResult& compiled, std::span<const ShaderRecompiler::MemoryRegion> regions, std::uint64_t& synced, PendingView& pending, std::optional<SampledReadScope>& sampling) {
        using Policy = ShaderMemory::PendingWrite;
        static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        for (const auto& region : regions) {
            if (!pending.Overlaps(region.guestAddress, region.bytes.size())) continue;
            if (!sampling) sampling.emplace(SiteSampling() ? evidenceValidations : evidenceReads);
            std::uint64_t ValidateCounters::*reason = &ValidateCounters::syncedOff;
            const auto policy = ValidateSkipEnabled() ? classifyPendingWrite(region.guestAddress, region.bytes.size(), reason, pending) : Policy::Sync;
            if (policy != Policy::Sync) continue;
            if (ValidateSkipEnabled() && (ValidateLegacy() ? !GuestMemory::EqualsCommittedUnsynced(region.guestAddress, region.bytes) : GuestMemory::CompareMapped(region.guestAddress, region.bytes) != GuestMemory::Compare::Equal)) {
                if (profile) {
                    std::lock_guard lock(validateMutex);
                    ++validateCounters.pending;
                    ++validateCounters.unsyncedMisses;
                }
                return false;
            }
            std::vector<std::byte> before;
            if (WriteEvidenceEnabled() && GuestMemory::Accessible(reinterpret_cast<const void*>(region.guestAddress), region.bytes.size())) {
                before.resize(region.bytes.size());
                std::memcpy(before.data(), reinterpret_cast<const void*>(region.guestAddress), before.size());
            }
            const auto waitsBefore = Graphics::Recorder::ThreadHookWaits();
            const bool timed = profile || TraceCapSync();
            const auto waitedBefore = timed ? Graphics::Recorder::ThreadWaitedMs() : 0.0;
            GuestMemory::FlushGpuWrites(region.guestAddress, region.bytes.size());
            const auto waited = timed ? Graphics::Recorder::ThreadWaitedMs() - waitedBefore : 0.0;
            pending.Load();
            if (!before.empty()) {
                if (!ObservationGuard() || Graphics::Recorder::ThreadHookWaits() != waitsBefore) observeRange(region.guestAddress, before);
                else observationsNoWait.fetch_add(1, std::memory_order_relaxed);
            }
            ++synced;
            if (profile) {
                std::lock_guard lock(validateMutex);
                auto& counters = validateCounters;
                ++counters.pending;
                ++(counters.*reason);
                counters.syncedWaitMs += waited;
                if (std::chrono::steady_clock::now() - counters.lastReport > std::chrono::seconds(10)) reportValidation(counters);
            }
            if (TraceCapSync() && traceBudget()) {
                const auto begin = region.guestAddress;
                const auto end = begin + region.bytes.size();
                std::fprintf(stderr, "[capsync] dispatch-cache q0x%x program 0x%llx region 0x%llx+0x%zx synced before the gate: waited %.1f ms;%s; writers:%s\n", queue, static_cast<unsigned long long>(program), static_cast<unsigned long long>(begin), region.bytes.size(), waited, describeSelf(compiled, begin, end).c_str(), describeWriters(begin, end).c_str());
            }
        }
        return true;
    }
    // Whether a variant still describes the memory its capture read: its words are compared with
    // guest memory in place, and the compare counts only if nothing that could have changed the
    // bytes under it moved meanwhile: the pending-write snapshot (Recorder::PublishGeneration,
    // bumped after every publish), the pending storage registry (StorageTexture::PendingSerial,
    // bumped after every store) and the page mappings (GuestMemory::ForgetSerial, odd while a
    // forget stores). The serials are loaded before the registry is scanned, so a mutation between
    // the two is caught by one test or the other. A run recorded GPU work still writes is synced
    // first (syncPendingRuns); one over an image whose results are pending is stored first
    // (FlushPending, the serials taken again behind the store); one over an image another thread
    // is storing right now, or under a label this worker still has queued (the miss's capture
    // reads it through the hook, which records the label first), is a miss, as is a page that is
    // not mapped. A miss never yields a wrong hit: a writer ordered before this dispatch had
    // published its note, marked its image or forgotten its pages before the gate's first load,
    // so the compare sees its bytes; one landing during the compare is unordered on hardware too
    // and still moves one of the serials. Each variant of an entry is gated over ITS runs (never
    // the union: a synced run only a non-matching variant reads must not wait); `sampling` is the
    // validation's one evidence sample, shared by every variant it compares. `live` (a dispatch
    // variant): the compare accepts differing data words (DataMask) and reports them there; a
    // hit with any is EqualData. The live words are read inside the same gate window, by the
    // same page-checked path, as the words the compare accepts.
    EntryOutcome validateVariant(std::uint64_t program, std::uint32_t queue, const DispatchVariant& variant, std::span<const ShaderRecompiler::MemoryRegion> regions, std::uint64_t& imagesFlushed, std::uint64_t& runsSynced, std::optional<SampledReadScope>& sampling, std::vector<std::pair<std::uint32_t, std::uint32_t>>* live = nullptr) {
        if (live != nullptr) live->clear();
        const bool masked = live != nullptr && !variant.dataPositions.empty();
        const DataMask mask{variant.dataPositions, live};
        PendingView pending;
        pending.Load();
        if (!syncPendingRuns(program, queue, *variant.compiled, regions, runsSynced, pending, sampling)) return EntryOutcome::Differing;
        auto publish = Graphics::Recorder::PublishGeneration();
        auto pendingSerial = Graphics::StorageTexture::PendingSerial();
        auto forget = GuestMemory::ForgetSerial();
        // Results a unit shadow holds over a run are not in the import's bytes either: the flush
        // publishes them (a recorded store the reload below then syncs on).
        if (Graphics::StorageTexture::AnyPendingOverlaps(variant.runs) || Graphics::AnyShadowedOverlaps(variant.runs)) {
            for (const auto& region : regions) {
                if (Graphics::StorageTexture::FlushPending(region.guestAddress, region.bytes.size(), nullptr, "dispatch-cache entry")) ++imagesFlushed;
            }
            pending.Load();
            if (!syncPendingRuns(program, queue, *variant.compiled, regions, runsSynced, pending, sampling)) return EntryOutcome::Differing;
            publish = Graphics::Recorder::PublishGeneration();
            pendingSerial = Graphics::StorageTexture::PendingSerial();
            forget = GuestMemory::ForgetSerial();
            if (Graphics::StorageTexture::AnyPendingOverlaps(variant.runs) || Graphics::AnyShadowedOverlaps(variant.runs)) return EntryOutcome::FlushingImage;
        }
        if ((forget & 1) != 0) return EntryOutcome::ForgetMoved;
        for (const auto& label : deferredLabels().labels) {
            for (const auto& [begin, end] : variant.runs) {
                if (label.address < end && begin < label.address + label.size) return EntryOutcome::QueuedLabel;
            }
        }
        if (ValidateLegacy()) {
            for (const auto& region : regions) {
                if (!GuestMemory::Accessible(reinterpret_cast<const void*>(region.guestAddress), region.bytes.size())) return EntryOutcome::Inaccessible;
            }
        }
        // The snapshot the compare consults: loaded after the final generation load above.
        pending.Load();
        bool unmapped = false;
        if (!validateCaptured(program, queue, regions, *variant.compiled, true, pending, &unmapped, &sampling, masked ? &mask : nullptr)) return unmapped ? EntryOutcome::Inaccessible : EntryOutcome::Differing;
        if (Graphics::Recorder::PublishGeneration() != publish) return EntryOutcome::PublishMoved;
        if (Graphics::StorageTexture::PendingSerial() != pendingSerial) return EntryOutcome::PendingMoved;
        if (GuestMemory::ForgetSerial() != forget) return EntryOutcome::ForgetMoved;
        return masked && !live->empty() ? EntryOutcome::EqualData : EntryOutcome::Equal;
    }
    // A 'differing' miss's fresh variant against the front variant it goes ahead of (under
    // dispatchCacheMutex; see EntryCounters): the differing dwords are classified by matching
    // their fresh values against the capture's V#/T# base words (address) and its flattened SRT
    // (data).
    void classifyDiffering(std::uint64_t program, std::uint64_t key, const DispatchVariant& old, const DispatchVariant& fresh, const ShaderRecompiler::ResourceCapture* capture, EntryCounters& counters) {
        ++counters.differingClassified;
        ++counters.differingByProgram[program];
        auto& ring = priorValueSets[key];
        if (std::any_of(ring.begin(), ring.end(), [&](const ValueSet& set) { return set.first == fresh.runs && set.second == fresh.words; })) ++counters.differingMatchedPrior;
        ring.emplace_front(old.runs, old.words);
        while (ring.size() > 3) ring.pop_back();
        if (priorValueSets.size() > DispatchCacheEntries()) priorValueSets.clear();
        if (old.runs != fresh.runs || old.words.size() != fresh.words.size()) {
            ++counters.differingRunsChanged;
            ++counters.differingWalk;
            return;
        }
        const auto baseWord = [&](const std::vector<ShaderRecompiler::DescriptorValue>& values, std::uint32_t value) {
            return std::any_of(values.begin(), values.end(), [&](const ShaderRecompiler::DescriptorValue& descriptor) { return descriptor.dwordCount >= 2 && (descriptor.dwords[0] == value || descriptor.dwords[1] == value); });
        };
        std::size_t words = 0, addressWords = 0, dataWords = 0;
        for (std::size_t i = 0; i < fresh.words.size(); ++i) {
            if (old.words[i] == fresh.words[i]) continue;
            ++words;
            counters.differingPositions.insert(i);
            counters.differingFirstPosition = std::min(counters.differingFirstPosition, i);
            counters.differingLastPosition = std::max(counters.differingLastPosition, i);
            const auto value = fresh.words[i];
            if (capture != nullptr && (baseWord(capture->snapshot.buffers, value) || baseWord(capture->snapshot.images, value))) ++addressWords;
            else if (capture != nullptr && std::find(capture->snapshot.flattenedSrt.begin(), capture->snapshot.flattenedSrt.end(), value) != capture->snapshot.flattenedSrt.end()) ++dataWords;
        }
        counters.differingWords += words;
        ++counters.differingWordBuckets[words <= 1 ? 0 : words <= 4 ? 1 : words <= 16 ? 2 : 3];
        if (words == 0) ++counters.differingWalk;
        else if (addressWords == words) ++counters.differingAddress;
        else if (dataWords == words) ++counters.differingData;
        else if (addressWords + dataWords == 0) ++counters.differingWalk;
        else ++counters.differingMixed;
    }
    void reportDispatchCache(EntryCounters& counters) {
        const auto count = [](std::uint64_t value) { return static_cast<unsigned long long>(value); };
        const auto validated = counters.lookups - counters.absent;
        std::fprintf(stderr, "[dispatch-cache] %llu lookups (10 s): %llu no entry, %llu validated by value in %.1f us each: %llu equal, %llu differing, %llu page not mapped, %llu queued label, %llu image being stored, %llu publish generation moved, %llu pending serial moved, %llu forget serial moved (%llu pending images stored first, %llu pending runs synced first, %llu equal entries inserted before a forget; gate retried %llu: %llu equal, %llu moved again); runs per entry %.1f validated / %.1f inserted; %llu misses found the entry replaced meanwhile; %llu inserts (%llu captures not kept: unstable), %llu evictions in total, %zu entries, %llu LRU moves\n", count(counters.lookups), count(counters.absent), count(validated), validated != 0 ? counters.validateUs / static_cast<double>(validated) : 0.0, count(counters.equal), count(counters.differing), count(counters.inaccessible), count(counters.queuedLabel), count(counters.flushingImage), count(counters.publishMoved), count(counters.pendingMoved), count(counters.forgetMoved), count(counters.imagesFlushed), count(counters.runsSynced), count(counters.forgetSinceInsert), count(counters.retriesEqual + counters.retriesMoved), count(counters.retriesEqual), count(counters.retriesMoved), validated != 0 ? static_cast<double>(counters.runsValidated) / static_cast<double>(validated) : 0.0, counters.inserts != 0 ? static_cast<double>(counters.runsInserted) / static_cast<double>(counters.inserts) : 0.0, count(counters.replaced), count(counters.inserts), count(counters.unstable), count(dispatchCacheEvictions), dispatchCache.size(), count(counters.touches));
        std::vector<std::pair<std::uint64_t, std::uint64_t>> programs(counters.differingByProgram.begin(), counters.differingByProgram.end());
        std::sort(programs.begin(), programs.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
        std::string top;
        for (std::size_t i = 0; i < programs.size() && i < 8; ++i) {
            char text[48];
            std::snprintf(text, sizeof(text), " 0x%llx x%llu", static_cast<unsigned long long>(programs[i].first), count(programs[i].second));
            top += text;
        }
        std::fprintf(stderr, "[dispatch-cache] differing by class (10 s, %llu classified): address-only %llu, data-only %llu, walk %llu, mixed %llu (runs changed %llu); matched one of the last 3 value sets %llu; differing words %llu in total (misses with 1 / 2-4 / 5-16 / >16 words: %llu / %llu / %llu / %llu), %zu distinct positions (first %zu, last %zu); top programs by differing:%s\n", count(counters.differingClassified), count(counters.differingAddress), count(counters.differingData), count(counters.differingWalk), count(counters.differingMixed), count(counters.differingRunsChanged), count(counters.differingMatchedPrior), count(counters.differingWords), count(counters.differingWordBuckets[0]), count(counters.differingWordBuckets[1]), count(counters.differingWordBuckets[2]), count(counters.differingWordBuckets[3]), counters.differingPositions.size(), counters.differingPositions.empty() ? std::size_t{0} : counters.differingFirstPosition, counters.differingLastPosition, top.c_str());
        std::string ranks;
        for (std::size_t rank = 0; rank < DispatchVariants(); ++rank) {
            char text[32];
            std::snprintf(text, sizeof(text), "%s%llu", rank == 0 ? "" : " / ", count(counters.variantHitsByRank[rank]));
            ranks += text;
        }
        std::fprintf(stderr, "[dispatch-cache] variants (10 s, k = %zu): hits by rank 1..k %s; %.2f variants compared per validation; %llu inserted into an entry, %llu evicted beyond k; %.2f variants per entry (%llu over %zu entries, ~%.1f MiB)\n", DispatchVariants(), ranks.c_str(), validated != 0 ? static_cast<double>(counters.variantsCompared) / static_cast<double>(validated) : 0.0, count(counters.variantsInserted), count(counters.variantsEvicted), dispatchCache.empty() ? 0.0 : static_cast<double>(dispatchCacheVariants) / static_cast<double>(dispatchCache.size()), count(dispatchCacheVariants), dispatchCache.size(), static_cast<double>(dispatchCacheVariantBytes) / (1024.0 * 1024.0));
        std::string dataRanks;
        for (std::size_t rank = 0; rank < DispatchVariants(); ++rank) {
            char text[32];
            std::snprintf(text, sizeof(text), "%s%llu", rank == 0 ? "" : " / ", count(counters.dataHitsByRank[rank]));
            dataRanks += text;
        }
        std::fprintf(stderr, "[dispatch-cache] data hits (10 s): %llu (%llu words refreshed; by rank 1..k %s; %llu data variants missed on pending runs), verified %llu; inserts with data positions %llu of %llu (%.1f positions each), leaves skipped: unmapped %llu, mismatched %llu, aliased %llu\n", count(counters.dataHits), count(counters.dataWordsRefreshed), dataRanks.c_str(), count(dataPendingMisses.exchange(0, std::memory_order_relaxed)), count(counters.dataVerified), count(counters.dataInserts), count(counters.inserts), counters.dataInserts != 0 ? static_cast<double>(counters.dataPositionsInserted) / static_cast<double>(counters.dataInserts) : 0.0, count(counters.dataLeavesUnmapped), count(counters.dataLeavesMismatched), count(counters.dataLeavesAliased));
        counters = EntryCounters{};
    }

    // APS5_VERIFY_DATA_HITS=1: the miss path's capture and recompile beside a data-only hit. The
    // read set (the captured regions outside the code and header), its bytes, the variant and the
    // bindings must agree with what the hit reused, else the process aborts: a disagreement would
    // be a stale read set served silently.
    void verifyDataHit(const ShaderSnapshot& snapshot, std::size_t codeOffset, std::uint64_t deviceSerial, ShaderRecompiler::RecompileRequest request, std::span<const ShaderRecompiler::MemoryRegion> memory, std::uint64_t address, const DispatchVariant& variant, std::span<const std::uint32_t> liveWords, const ShaderRecompiler::RecompileResult& patched) {
        const auto fail = [&](const char* what, std::size_t position, std::size_t slot) {
            std::fprintf(stderr, "[dispatch-cache] APS5_VERIFY_DATA_HITS: %s disagrees (program 0x%llx, position %zu, slot %zu)\n", what, static_cast<unsigned long long>(address), position, slot);
            std::fflush(stderr);
            std::abort();
        };
        auto verifyMemory = std::make_shared<ShaderMemory>(memory, &ClassifyPendingWrite, &ObservePendingWrite, HookWaitCounter());
        const auto handle = SourceHandleFor(snapshot, codeOffset, deviceSerial, request, false);
        const auto capture = verifyMemory->Capture(request, handle.get());
        const auto regions = verifyMemory->Regions();
        request.context.memory = regions;
        const auto result = ShaderRecompiler::Recompile(request, *capture);
        std::size_t position = 0, run = 0;
        for (const auto& region : regions) {
            const bool registered = std::any_of(memory.begin(), memory.end(), [&](const auto& known) { return region.guestAddress >= known.guestAddress && region.guestAddress < known.guestAddress + known.bytes.size(); });
            if (registered) continue;
            if (run >= variant.runs.size() || variant.runs[run].first != region.guestAddress || variant.runs[run].second != region.guestAddress + region.bytes.size()) fail("the read set", position, run);
            const auto count = region.bytes.size() / sizeof(std::uint32_t);
            if (position + count > liveWords.size() || std::memcmp(region.bytes.data(), liveWords.data() + position, region.bytes.size()) != 0) fail("the captured bytes", position, run);
            position += count;
            ++run;
        }
        if (run != variant.runs.size()) fail("the read set", position, run);
        if (result->variantId != patched.variantId) fail("the variant", 0, 0);
        if (result->bindings.size() != patched.bindings.size() || result->pushConstants != patched.pushConstants) fail("the bindings", 0, 0);
        for (std::size_t b = 0; b < result->bindings.size(); ++b) {
            const auto& fresh = result->bindings[b];
            const auto& reused = patched.bindings[b];
            if (fresh.kind != reused.kind || fresh.role != reused.role || fresh.binding != reused.binding || fresh.count != reused.count || fresh.guestDescriptor != reused.guestDescriptor) fail("the bindings", b, 0);
        }
        std::lock_guard cacheLock(dispatchCacheMutex);
        ++entryCounters.dataVerified;
    }

    // `indirectArguments` (non-zero): a DISPATCH_INDIRECT whose group counts, at that guest address,
    // are not read here; `packet` then carries only the initiator (see dispatchIndirect).
    void dispatch(QueueState& queue, std::span<const std::uint32_t> packet, const Submission& submission, std::uint64_t indirectArguments = 0) {
        const auto address = (static_cast<std::uint64_t>(readRegister(queue.shader, 0x20c)) << 8u) | (static_cast<std::uint64_t>(readRegister(queue.shader, 0x20d) & 0xffu) << 40u);
        auto it = submission.shaders->upper_bound(address);
        require(it != submission.shaders->begin(), "compute program does not belong to a registered shader");
        --it;
        const auto& snapshot = *it->second;
        require(address - snapshot.codeAddress < snapshot.code.size() * sizeof(std::uint32_t), "compute program is outside registered shader code");
        require(snapshot.type == 0, "compute program refers to a non-compute shader");
        const auto userCount = (readRegister(queue.shader, 0x213) >> 1u) & 0x1fu;
        std::vector<std::uint32_t> userData;
        for (std::uint32_t i = 0; i < userCount; ++i) {
            userData.push_back(readRegister(queue.shader, 0x240 + i));
        }
        const auto compute = Graphics::DecodeComputeStageInfo(queue.shader);
        const std::array<ShaderRecompiler::MemoryRegion, 2> memory{{{snapshot.codeAddress, std::as_bytes(std::span(snapshot.code))}, {snapshot.headerAddress, snapshot.header}}};
        // The GpuMutex is only taken to create the device: taking it just to copy the pointer made
        // every dispatch wait behind another queue's whole device phase before its lock-free prologue.
        // Debug aid: APS5_NO_UNLOCKED_DEVICE=1 takes the lock to read it as before.
        static const bool unlockedDevice = std::getenv("APS5_NO_UNLOCKED_DEVICE") == nullptr;
        std::shared_ptr<VulkanDevice> localDevice = unlockedDevice ? device.load() : nullptr;
        if (localDevice == nullptr) {
            GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Dispatch);
            std::lock_guard gpuLock(GuestMemory::GpuMutex());
            if (device == nullptr) device = std::make_shared<VulkanDevice>();
            localDevice = device;
        }
        const auto codeOffset = static_cast<std::size_t>((address - snapshot.codeAddress) / sizeof(std::uint32_t));
        std::array<std::uint32_t, 5> resolved{};
        if (indirectArguments != 0 && matchesFillKernel(std::span(snapshot.code).subspan(codeOffset), userData, compute)) {
            // The fill HLE consumes the group count on the CPU: read it as before (waiting for the
            // producer through the flush hook) and continue as a direct dispatch.
            recordQueuedLabelsBeforeRead(submission.queue);
            const auto readStart = std::chrono::steady_clock::now();
            resolved = Pm4::ReadDispatchArguments(indirectArguments, packet[4]);
            countIndirect(IndirectFillKernel, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - readStart).count());
            packet = resolved;
            indirectArguments = 0;
        }
        if (fillBuffer(queue, submission.queue, packet, std::span(snapshot.code).subspan(codeOffset), userData, compute, localDevice)) {
            pendingDispatchPhases().outcome = DispatchOutcome::FillHle;
            return;
        }
        if (indirectArguments == 0 && copyBuffer(queue, submission.queue, packet, std::span(snapshot.code).subspan(codeOffset), userData, compute, localDevice, address)) {
            pendingDispatchPhases().outcome = DispatchOutcome::CopyHle;
            return;
        }
        ShaderRecompiler::RecompileRequest request{
            {ShaderRecompiler::ShaderStage::Compute, address, std::span(snapshot.code).subspan(codeOffset), snapshot.headerAddress, snapshot.header},
            {(packet[4] & 0x8000u) != 0 ? 32u : 64u, 0, userData, compute, std::nullopt, std::nullopt, memory},
            localDevice->Target(),
            {0, 0, 0, 128}
        };
        static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        static double captureMs = 0, keyMs = 0, recompileMs = 0, deviceMs = 0;
        static std::uint64_t cacheHits = 0;
        static std::uint64_t dispatches = 0;
        auto lap = std::chrono::steady_clock::now();
        const auto elapsed = [&] {
            const auto now = std::chrono::steady_clock::now();
            const auto ms = std::chrono::duration<double, std::milli>(now - lap).count();
            lap = now;
            return ms;
        };
        std::array<double, DriverPhaseCount> phaseMs{};
        auto phaseLap = lap;
        if (profile && packetStartedAt() != std::chrono::steady_clock::time_point{}) phaseMs[PhasePrologue] = std::chrono::duration<double, std::milli>(lap - packetStartedAt()).count();
        const auto phase = [&](DriverPhase which) {
            if (!profile) return;
            const auto now = std::chrono::steady_clock::now();
            phaseMs[which] += std::chrono::duration<double, std::milli>(now - phaseLap).count();
            phaseLap = now;
        };
        // Debug aid: APS5_NO_DISPATCH_CACHE=1 captures and recompiles every dispatch.
        static const bool noDispatchCacheEnv = std::getenv("APS5_NO_DISPATCH_CACHE") != nullptr;
        // Debug aid: APS5_PROBE_DISPATCH=<hex code address>:<n> applies the recompiler's APS5_PROBE
        // register probe to the n-th (0-based) dispatch of that program only; that dispatch is
        // recompiled outside the dispatch cache.
        static const std::pair<std::uint64_t, std::uint64_t> probeDispatch = [] {
            const char* text = std::getenv("APS5_PROBE_DISPATCH");
            if (text == nullptr) return std::pair<std::uint64_t, std::uint64_t>{0, 0};
            char* end = nullptr;
            const auto probeAddress = std::strtoull(text, &end, 16);
            const auto index = end != nullptr && *end == ':' ? std::strtoull(end + 1, nullptr, 10) : 0ull;
            return std::pair<std::uint64_t, std::uint64_t>{probeAddress, index};
        }();
        bool probeThis = false;
        // The heap base differs between runs (0x10…, 0x20…, 0x30…), so only the low 36 bits are compared.
        if (probeDispatch.first != 0 && (address & 0xfffffffffull) == (probeDispatch.first & 0xfffffffffull)) {
            static std::atomic<std::uint64_t> dispatchesSeen{0};
            probeThis = dispatchesSeen.fetch_add(1) == probeDispatch.second;
            if (probeThis) std::fprintf(stderr, "[gpu] probing dispatch %llu of 0x%llx\n", static_cast<unsigned long long>(probeDispatch.second), static_cast<unsigned long long>(address));
        }
        // A program whose plan build failed before (the poisoned source-handle memo) is skipped
        // here, before the key, the capture, the ShaderMemory and the lock, without a throw; the
        // first failure took the full path and reported it. A probed dispatch bypasses the memo.
        if (FailureMemo() && snapshot.handles->poisoned.load(std::memory_order_relaxed) != 0) {
            const std::string* poisoned = nullptr;
            if (SourceHandleFor(snapshot, codeOffset, localDevice->Serial(), request, probeThis, &poisoned) == nullptr && poisoned != nullptr) {
                pendingDispatchPhases().outcome = DispatchOutcome::SkippedMemo;
                return;
            }
        }
        const bool noDispatchCache = noDispatchCacheEnv || probeThis;
        std::uint64_t key = 0xcbf29ce484222325ull;
        const auto mix = [&](std::uint64_t value) {
            key ^= value;
            key *= 0x100000001b3ull;
        };
        mix(address);
        mix(packet[4] & 0x8000u);
        for (const auto word : userData) mix(word);
        // Only the shader registers the request reads (thread counts and RSRC1/2; the program
        // address is `address`): on queue 0 the bank also holds the graphics stages' registers,
        // which every draw rewrites. Debug aid: APS5_NO_DISPATCH_KEY_HYGIENE=1 mixes the whole bank.
        static const bool keyHygiene = std::getenv("APS5_NO_DISPATCH_KEY_HYGIENE") == nullptr;
        if (keyHygiene) {
            for (const auto offset : {0x207u, 0x208u, 0x209u, 0x212u, 0x213u}) {
                const auto found = queue.shader.find(offset);
                // An absent register mixes a value no 32-bit register can hold.
                mix(found == queue.shader.end() ? (1ull << 32u) : found->second);
            }
        } else {
            for (const auto& [offset, value] : queue.shader) {
                mix(offset);
                mix(value);
            }
        }
        std::shared_ptr<const ShaderRecompiler::RecompileResult> compiledResult;
        // A hit's captured regions point into the variant (its stored words; under stamp
        // validation its capture's pages and its snapshot's code): the variant is kept past
        // eviction.
        std::shared_ptr<DispatchVariant> keepVariant;
        // A miss's inserted variant (or the equal one another worker inserted meanwhile): the
        // recipe the device call builds is attached to it.
        std::shared_ptr<DispatchVariant> attachVariant;
        // A miss's captured regions point into its capture's pages, and the snapshots made from
        // them are compared by an address-based build's Complete() under the device lock: the
        // capture lives for the whole dispatch, not only until the entry is made from it.
        std::shared_ptr<ShaderMemory> shaderMemory;
        std::vector<ShaderRecompiler::MemoryRegion> captured;
        // A data-only hit: the matched variant's words with the differing data words at their
        // live values; `captured` points into them, so they live for the whole dispatch (an
        // address-based build's Complete() compares the snapshots under the device lock).
        std::vector<std::uint32_t> liveWords;
        bool dataHit = false;
        bool cached = false;
        bool validated = false;
        // A miss keeps the entry it validated: the fresh variant goes in front of its variants.
        // A 'differing' one also keeps its own capture for classifyDiffering (profile only).
        std::shared_ptr<DispatchEntry> missedEntry;
        bool missedDiffering = false;
        std::shared_ptr<const ShaderRecompiler::ResourceCapture> capture;
        // Debug aid: APS5_TRACE_DISPATCH_CACHE reports, per program, what changed between dispatches.
        static const bool traceCache = std::getenv("APS5_TRACE_DISPATCH_CACHE") != nullptr;
        if (traceCache) {
            std::lock_guard traceLock(dispatchCacheMutex);
            struct Last { std::vector<std::uint32_t> userData; std::map<std::uint32_t, std::uint32_t> shader; std::uint64_t key; };
            static std::map<std::uint64_t, Last> last;
            static int reports = 0;
            auto& previous = last[address];
            if (previous.key != 0 && previous.key != key && reports < 200) {
                std::string what;
                for (std::size_t i = 0; i < userData.size(); ++i) {
                    if (i >= previous.userData.size() || previous.userData[i] != userData[i]) {
                        char text[48];
                        std::snprintf(text, sizeof(text), " user[%zu] %08x->%08x", i, i < previous.userData.size() ? previous.userData[i] : 0u, userData[i]);
                        what += text;
                    }
                }
                for (const auto& [offset, value] : queue.shader) {
                    const auto old = previous.shader.find(offset);
                    if (old == previous.shader.end() || old->second != value) {
                        char text[48];
                        std::snprintf(text, sizeof(text), " sh[%x] %08x->%08x", offset, old == previous.shader.end() ? 0u : old->second, value);
                        what += text;
                    }
                }
                ++reports;
                std::fprintf(stderr, "[dispatch-cache] 0x%llx key changed:%s\n", static_cast<unsigned long long>(address), what.c_str());
            }
            previous.userData = userData;
            previous.shader = std::map<std::uint32_t, std::uint32_t>(queue.shader.begin(), queue.shader.end());
            previous.key = key;
        }
        // The key names the registered shader object (RegisterShader makes a new one per
        // registration, identical bytes included), so an entry's code and header are never
        // compared: the recompile result was made from that object's copy of them. Under stamp
        // validation the entry compares them itself, as before.
        if (!StampValidate()) mix(reinterpret_cast<std::uintptr_t>(it->second.get()));
        phase(PhaseKey);
        if (!noDispatchCache) {
            // The entry is validated without the cache lock: the compares (which can wait for the
            // GpuMutex through the flush hook over a pending write) would otherwise stall every
            // other queue's lookup behind this one. An entry is immutable once inserted (a
            // re-stamp is an atomic store), so the re-lock only has to skip an entry another worker
            // replaced meanwhile. Debug aid: APS5_NO_UNLOCKED_VALIDATE=1 holds the lock.
            static const bool validateUnlocked = std::getenv("APS5_NO_UNLOCKED_VALIDATE") == nullptr;
            std::unique_lock cacheLock(dispatchCacheMutex);
            ++entryCounters.lookups;
            const auto found = dispatchCache.find(key);
            std::shared_ptr<DispatchEntry> entry = found != dispatchCache.end() ? found->second : nullptr;
            if (entry == nullptr) ++entryCounters.absent;
            if (validateUnlocked) cacheLock.unlock();
            phase(PhaseLookup);
            if (entry != nullptr) {
                validated = true;
                const auto& variants = entry->variants;
                // The matched variant and its rank; the front variant under stamp validation.
                std::shared_ptr<DispatchVariant> variant;
                std::size_t rank = 0;
                const auto generation = variants.front()->generation.load(std::memory_order_acquire);
                bool current = false;
                std::uint64_t restamped = 0;
                // The front variant's outcome, as the single entry's was; Equal for any hit.
                auto outcome = EntryOutcome::Differing;
                std::uint64_t imagesFlushed = 0, runsSynced = 0, retriesEqual = 0, retriesMoved = 0, compared = 0;
                std::vector<ShaderRecompiler::MemoryRegion> regions;
                // The differing data words of the matched variant (position, live word).
                std::vector<std::pair<std::uint32_t, std::uint32_t>> liveData;
                const auto waitedBeforeValidate = profile ? Graphics::Recorder::ThreadWaitedMs() : 0.0;
                if (!StampValidate()) {
                    const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::DispatchCache);
                    // One evidence sample for the whole validation, drawn at its first pending run.
                    std::optional<SampledReadScope> sampling;
                    // Most recently hit first; the first Equal is the hit. A Differing variant says
                    // nothing about the others (they are independent), so every one is compared
                    // before a miss.
                    for (std::size_t i = 0; i < variants.size(); ++i) {
                        regions.clear();
                        appendEntryRegions(*variants[i], regions);
                        ++compared;
                        auto result = validateVariant(address, submission.queue, *variants[i], regions, imagesFlushed, runsSynced, sampling, &liveData);
                        // A serial that moved during an otherwise equal compare means "compare
                        // again", not "changed": one fresh pass of the same gate, a second move is
                        // the miss.
                        if (GateRetry() && (result == EntryOutcome::PublishMoved || result == EntryOutcome::PendingMoved)) {
                            result = validateVariant(address, submission.queue, *variants[i], regions, imagesFlushed, runsSynced, sampling, &liveData);
                            ++(result == EntryOutcome::Equal || result == EntryOutcome::EqualData ? retriesEqual : retriesMoved);
                        }
                        if (i == 0) outcome = result;
                        if (result == EntryOutcome::Equal || result == EntryOutcome::EqualData) {
                            outcome = result;
                            variant = variants[i];
                            rank = i;
                            dataHit = result == EntryOutcome::EqualData;
                            break;
                        }
                    }
                    current = variant != nullptr;
                } else {
                    variant = variants.front();
                    compared = 1;
                    current = true;
                    for (const auto& [begin, bytes] : variant->spans) {
                        GuestMemory::CollectWrites(begin, bytes);
                        if (!GuestMemory::UnchangedSince(begin, bytes, generation)) {
                            current = false;
                            break;
                        }
                    }
                    if (!current) {
                        // A write landed in a watched span (spans merge regions up to 64 KiB apart,
                        // so it is often unrelated): the captured bytes themselves decide, and an
                        // unchanged capture is re-stamped at a fresh generation. The generation is
                        // collected BEFORE the compare, as on insert: a write landing after the
                        // collect dirties the pages again and fails the next validation, whereas
                        // one landing between a compare and a later collect would be stamped into
                        // the entry and never seen.
                        std::uint64_t collected = 0;
                        for (const auto& [begin, bytes] : variant->spans) collected = std::max(collected, GuestMemory::CollectWrites(begin, bytes));
                        bool same = collected != 0;
                        if (same) {
                            const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::DispatchCache);
                            PendingView pending;
                            pending.Load();
                            same = validateCaptured(address, submission.queue, variant->captured, *variant->compiled, false, pending);
                        }
                        if (same) {
                            restamped = collected;
                            current = true;
                        }
                    }
                    if (current) outcome = EntryOutcome::Equal;
                }
                phase(PhaseValidate);
                if (profile) {
                    const auto waited = std::min(Graphics::Recorder::ThreadWaitedMs() - waitedBeforeValidate, phaseMs[PhaseValidate]);
                    phaseMs[PhaseValidate] -= waited;
                    phaseMs[PhaseValidateWait] += waited;
                }
                if (!cacheLock.owns_lock()) cacheLock.lock();
                // Only the entry still in the map, at the copied generation, is re-stamped or
                // moved; the validated variant is used either way.
                const auto again = dispatchCache.find(key);
                const bool untouched = again != dispatchCache.end() && again->second == entry && variants.front()->generation.load(std::memory_order_acquire) == generation;
                auto& counters = entryCounters;
                counters.validateUs += phaseMs[PhaseValidate] * 1000;
                counters.imagesFlushed += imagesFlushed;
                counters.runsSynced += runsSynced;
                for (std::size_t i = 0; i < compared && i < variants.size(); ++i) counters.runsValidated += variants[i]->runs.size();
                counters.retriesEqual += retriesEqual;
                counters.retriesMoved += retriesMoved;
                counters.variantsCompared += compared;
                switch (outcome) {
                    case EntryOutcome::Equal: ++counters.equal; break;
                    case EntryOutcome::EqualData: ++counters.equal; break;
                    case EntryOutcome::Differing: ++counters.differing; break;
                    case EntryOutcome::Inaccessible: ++counters.inaccessible; break;
                    case EntryOutcome::QueuedLabel: ++counters.queuedLabel; break;
                    case EntryOutcome::FlushingImage: ++counters.flushingImage; break;
                    case EntryOutcome::PublishMoved: ++counters.publishMoved; break;
                    case EntryOutcome::PendingMoved: ++counters.pendingMoved; break;
                    case EntryOutcome::ForgetMoved: ++counters.forgetMoved; break;
                }
                if (current) {
                    ++counters.variantHitsByRank[rank];
                    compiledResult = variant->compiled;
                    if (dataHit) {
                        // The live words: the regions, the snapshots and the label check see them
                        // as a fresh capture would. The compiled result is a copy with the
                        // FlattenedSrt binding patched to them; the variant id, hence the
                        // pipeline, is unchanged (no pure word reaches the specialization). The
                        // recipe or the template hit then refreshes the data buffers by words.
                        liveWords = variant->words;
                        for (const auto& [position, value] : liveData) liveWords[position] = value;
                        regions.clear();
                        appendEntryRegions(*variant, regions, &liveWords);
                        auto patched = std::make_shared<ShaderRecompiler::RecompileResult>(*variant->compiled);
                        auto& descriptor = patched->bindings[variant->flatBinding].guestDescriptor;
                        for (std::size_t k = 0; k < variant->dataPositions.size(); ++k) {
                            if (variant->dataSlots[k] < descriptor.size()) descriptor[variant->dataSlots[k]] = liveWords[variant->dataPositions[k]];
                        }
                        compiledResult = std::move(patched);
                        ++counters.dataHits;
                        counters.dataWordsRefreshed += liveData.size();
                        ++counters.dataHitsByRank[rank];
                    }
                    if (StampValidate()) {
                        captured = variant->captured;
                    } else {
                        // `regions` holds the matched variant's (the loop stopped at it).
                        captured.reserve(memory.size() + regions.size());
                        captured.assign(memory.begin(), memory.end());
                        captured.insert(captured.end(), regions.begin(), regions.end());
                        if (variant->forgetSerial != GuestMemory::ForgetSerial()) ++counters.forgetSinceInsert;
                    }
                    keepVariant = variant;
                    cached = true;
                    ++dispatchCacheHits;
                    if (untouched) {
                        if (restamped != 0) variant->generation.store(restamped, std::memory_order_release);
                        // A hit behind the front reorders by replacing the entry with a rotated
                        // copy (the entry is immutable).
                        if (rank != 0) {
                            auto rotated = std::make_shared<DispatchEntry>();
                            rotated->variants.reserve(variants.size());
                            rotated->variants.push_back(variant);
                            for (std::size_t i = 0; i < variants.size(); ++i) {
                                if (i != rank) rotated->variants.push_back(variants[i]);
                            }
                            rotated->touched = entry->touched;
                            rotated->order = entry->order;
                            again->second = std::move(rotated);
                        }
                        // The move to the front is amortized: an entry hit within the newest eighth
                        // of the cache's worth of hits stays where it is.
                        if (dispatchCacheHits - again->second->touched > DispatchCacheEntries() / 8) {
                            dispatchOrder.splice(dispatchOrder.begin(), dispatchOrder, again->second->order);
                            again->second->touched = dispatchCacheHits;
                            ++counters.touches;
                        }
                    }
                } else {
                    if (traceCache) std::fprintf(stderr, "[dispatch-cache] 0x%llx captured memory changed\n", static_cast<unsigned long long>(address));
                    // The entry stays until the insert puts the fresh variant in front of its
                    // variants (a slot another worker replaced meanwhile is as good).
                    missedEntry = entry;
                    missedDiffering = outcome == EntryOutcome::Differing;
                    if (!untouched) ++counters.replaced;
                }
                if (profile && std::chrono::steady_clock::now() - counters.lastReport > std::chrono::seconds(10)) {
                    counters.lastReport = std::chrono::steady_clock::now();
                    reportDispatchCache(counters);
                }
                phase(PhaseRelock);
            }
        }
        if (cached) {
            captureMs += elapsed();
        } else {
            shaderMemory = std::make_shared<ShaderMemory>(memory, &ClassifyPendingWrite, &ObservePendingWrite, HookWaitCounter());
            std::uint64_t forgetAtCapture = 0;
            // Debug aid: APS5_DUMP_SHADERS=1 saves every request, failing ones included, for agc_shader_replay.
            static const bool dumpShaders = std::getenv("APS5_DUMP_SHADERS") != nullptr;
            try {
                // The probe is part of the recompiler's cache key, so it covers the capture's plan
                // lookup as well as the recompile that reuses it.
                struct ProbeScope {
                    bool active;
                    explicit ProbeScope(bool active) : active(active) { if (active) ShaderRecompiler::SetDebugProbeActive(true); }
                    ~ProbeScope() { if (active) ShaderRecompiler::SetDebugProbeActive(false); }
                } probeScope{probeThis};
                const auto waitedBefore = TraceCapSync() ? Graphics::Recorder::ThreadWaitedMs() : 0.0;
                forgetAtCapture = GuestMemory::ForgetSerial();
                const auto handle = SourceHandleFor(snapshot, codeOffset, localDevice->Serial(), request, probeThis);
                capture = [&] {
                    const SampledReadScope sampling(evidenceReads);
                    return shaderMemory->Capture(request, handle.get());
                }();
                captured = shaderMemory->Regions();
                request.context.memory = captured;
                if (TraceCapSync()) traceCapture("dispatch-capture", address, submission.queue, captured, Graphics::Recorder::ThreadWaitedMs() - waitedBefore);
                captureMs += elapsed();
                phase(PhaseCapture);
                if (dumpShaders) static_cast<void>(dumpRequest(address, request));
                const auto started = std::chrono::steady_clock::now();
                // The recompile reuses the capture's plan and materialization rather than walking
                // the captured regions again. Debug aid: APS5_NO_CAPTURE_REUSE=1 walks them again.
                static const bool reuseCapture = std::getenv("APS5_NO_CAPTURE_REUSE") == nullptr;
                bool memoHit = false;
                compiledResult = reuseCapture ? ShaderRecompiler::Recompile(request, *capture, &memoHit) : std::make_shared<const ShaderRecompiler::RecompileResult>(ShaderRecompiler::Recompile(request));
                if (compiledResult->cacheHit || memoHit) ++cacheHits;
                const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
                static double totalMs = 0;
                totalMs += elapsed;
                if (profile && elapsed > 200) std::fprintf(stderr, "[gpu] compute shader 0x%llx recompile took %.0f ms (%zu SPIR-V words, %zu captured regions, total %.1f s)\n", static_cast<unsigned long long>(address), elapsed, compiledResult->spirv.size(), captured.size(), totalMs / 1000);
            } catch (const std::exception& error) {
                const auto dump = dumpShaders ? dumpRequest(address, request) : std::string{};
                // Recompile errors append the serialized request; keep only the first line in the report.
                std::string reason = error.what();
                if (const auto newline = reason.find('\n'); newline != std::string::npos) reason.resize(newline);
                char where[96];
                if (dump.empty()) std::snprintf(where, sizeof(where), "compute shader 0x%llx: ", static_cast<unsigned long long>(address));
                else std::snprintf(where, sizeof(where), "compute shader 0x%llx (%s): ", static_cast<unsigned long long>(address), dump.c_str());
                throw std::runtime_error(where + reason);
            }
            recompileMs += elapsed();
            phase(PhaseRecompile);
            if (!noDispatchCache) {
                auto fresh = std::make_shared<DispatchVariant>();
                fresh->compiled = compiledResult;
                fresh->shader = it->second;
                fresh->forgetSerial = forgetAtCapture;
                // The captured dwords outside the registered code and header (the capture served
                // those from the registration, so no page region lies inside them), as the entry
                // stores them.
                std::vector<ShaderRecompiler::MemoryRegion> srtRegions;
                for (const auto& region : captured) {
                    const bool registered = std::any_of(memory.begin(), memory.end(), [&](const auto& known) { return region.guestAddress >= known.guestAddress && region.guestAddress < known.guestAddress + known.bytes.size(); });
                    if (registered) continue;
                    srtRegions.push_back(region);
                    fresh->runs.emplace_back(region.guestAddress, region.guestAddress + region.bytes.size());
                    const auto count = region.bytes.size() / sizeof(std::uint32_t);
                    const auto offset = fresh->words.size();
                    fresh->words.resize(offset + count);
                    std::memcpy(fresh->words.data() + offset, region.bytes.data(), count * sizeof(std::uint32_t));
                }
                // The positions of the pure flat-SRT leaves among the stored words (data-only
                // hits): none under stamp validation, without a FlattenedSrt binding, or when the
                // capture traced no leaf (no pure slot in the plan).
                DataWordPositionCounts dataCounts;
                if (DataHits() && !StampValidate() && capture != nullptr && !capture->readTrace.leaves.empty()) {
                    for (std::size_t b = 0; b < compiledResult->bindings.size(); ++b) {
                        if (compiledResult->bindings[b].role != ShaderRecompiler::DescriptorRole::FlattenedSrt) continue;
                        fresh->flatBinding = static_cast<std::uint32_t>(b);
                        dataCounts = DataWordPositions(fresh->runs, capture->readTrace.leaves, capture->readTrace.otherReads, fresh->words, compiledResult->bindings[b].guestDescriptor, fresh->dataPositions, fresh->dataSlots);
                        break;
                    }
                }
                bool stable = true;
                if (StampValidate()) {
                    // The captured regions, merged into spans when close, are watched for writes
                    // from now on; bytes that already differ show a write racing the capture, and
                    // nothing is kept.
                    fresh->memory = shaderMemory;
                    fresh->captured = captured;
                    for (const auto& region : captured) {
                        const auto begin = region.guestAddress;
                        const auto end = begin + region.bytes.size();
                        if (!fresh->spans.empty() && begin <= fresh->spans.back().first + fresh->spans.back().second + 65536) {
                            auto& last = fresh->spans.back();
                            last.second = static_cast<std::size_t>(std::max(last.first + last.second, end) - last.first);
                        } else {
                            fresh->spans.emplace_back(begin, static_cast<std::size_t>(end - begin));
                        }
                    }
                    std::uint64_t generation = 0;
                    for (const auto& [begin, bytes] : fresh->spans) generation = std::max(generation, GuestMemory::CollectWrites(begin, bytes));
                    stable = generation != 0;
                    if (stable) {
                        const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::DispatchCache);
                        stable = captureStable(captured);
                    }
                    fresh->generation.store(generation, std::memory_order_release);
                } else if (InsertCompare()) {
                    const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::DispatchCache);
                    stable = captureStable(srtRegions);
                }
                std::lock_guard cacheLock(dispatchCacheMutex);
                if (profile && missedEntry != nullptr && missedDiffering) classifyDiffering(address, key, *missedEntry->variants.front(), *fresh, capture.get(), entryCounters);
                if (stable) {
                    ++entryCounters.inserts;
                    entryCounters.runsInserted += fresh->runs.size();
                    if (!fresh->dataPositions.empty()) {
                        ++entryCounters.dataInserts;
                        entryCounters.dataPositionsInserted += fresh->dataPositions.size();
                    }
                    entryCounters.dataLeavesUnmapped += dataCounts.unmapped;
                    entryCounters.dataLeavesMismatched += dataCounts.mismatched;
                    entryCounters.dataLeavesAliased += dataCounts.aliased;
                    attachVariant = fresh;
                    const auto found = dispatchCache.find(key);
                    if (found == dispatchCache.end()) {
                        auto created = std::make_shared<DispatchEntry>();
                        created->touched = dispatchCacheHits;
                        accountVariant(*fresh, true);
                        created->variants.push_back(std::move(fresh));
                        dispatchOrder.push_front(key);
                        created->order = dispatchOrder.begin();
                        dispatchCache.emplace(key, std::move(created));
                    } else if (found->second != missedEntry && !found->second->variants.empty() && found->second->variants.front()->runs == fresh->runs && found->second->variants.front()->words == fresh->words) {
                        // Another worker inserted this value set meanwhile; its variant is as good.
                        attachVariant = found->second->variants.front();
                    } else {
                        // The fresh variant goes in front of the slot's variants (the ones this
                        // miss validated, or another worker's: as good); the least recently hit
                        // beyond DispatchVariants() go. The entry is replaced whole.
                        const auto& old = found->second;
                        auto replacement = std::make_shared<DispatchEntry>();
                        replacement->variants.reserve(DispatchVariants());
                        accountVariant(*fresh, true);
                        replacement->variants.push_back(std::move(fresh));
                        for (const auto& kept : old->variants) {
                            if (replacement->variants.size() < DispatchVariants()) {
                                replacement->variants.push_back(kept);
                            } else {
                                accountVariant(*kept, false);
                                ++entryCounters.variantsEvicted;
                            }
                        }
                        ++entryCounters.variantsInserted;
                        replacement->touched = dispatchCacheHits;
                        replacement->order = old->order;
                        dispatchOrder.splice(dispatchOrder.begin(), dispatchOrder, replacement->order);
                        found->second = std::move(replacement);
                    }
                    // Debug aid: APS5_NO_DISPATCH_LRU=1 clears the cache when it fills, as before.
                    static const bool lru = std::getenv("APS5_NO_DISPATCH_LRU") == nullptr;
                    if (dispatchCache.size() > DispatchCacheEntries()) {
                        if (lru) {
                            eraseDispatchEntry(dispatchCache.find(dispatchOrder.back()));
                            ++dispatchCacheEvictions;
                        } else {
                            dispatchCacheEvictions += dispatchCache.size();
                            dispatchCache.clear();
                            dispatchOrder.clear();
                            dispatchCacheVariants = 0;
                            dispatchCacheVariantBytes = 0;
                        }
                    }
                } else {
                    ++entryCounters.unstable;
                }
                phase(PhaseInsert);
            }
        }
        if (VerifyDataHits() && dataHit) verifyDataHit(snapshot, codeOffset, localDevice->Serial(), request, memory, address, *keepVariant, liveWords, *compiledResult);
        // A label this queue still has queued over a captured region: recorded now, and the dispatch
        // starts over (the cache entry just validated or inserted fails its next validation, the
        // label store marks the tracker). The queue is empty after, so this happens once.
        if (recordQueuedLabelsAfterCapture(submission.queue, captured)) {
            dispatch(queue, packet, submission, indirectArguments);
            return;
        }
        phase(PhaseQueuedLabels);
        const auto& compiled = *compiledResult;
        std::vector<Graphics::GuestMemorySnapshot> snapshots;
        for (const auto& region : captured) snapshots.push_back({region.guestAddress, region.bytes});
        std::array<std::uint32_t, 3> groups{packet[1], packet[2], packet[3]};
        if (indirectArguments == 0 && (packet[4] & 0x20u) != 0) {
            // USE_THREAD_DIMENSIONS: the packet counts threads; launch enough whole groups to cover them.
            for (std::uint32_t axis = 0; axis < 3; ++axis) {
                const auto threads = std::max(readRegister(queue.shader, 0x207 + axis) & 0xffffu, 1u);
                groups[axis] = (groups[axis] + threads - 1) / threads;
            }
        }
        static const bool traceIo = std::getenv("APS5_TRACE_DISPATCH_IO") != nullptr;
        if (traceIo) {
            // APS5_TRACE_DISPATCH_IO=2 also lists the user data.
            std::string words;
            if (std::getenv("APS5_TRACE_DISPATCH_IO")[0] == '2') {
                for (const auto word : userData) {
                    char text[12];
                    std::snprintf(text, sizeof(text), " %08x", word);
                    words += text;
                }
            }
            std::fprintf(stderr, "[dispatch-io] shader 0x%llx%s\n", static_cast<unsigned long long>(address), words.c_str());
        }
        // A failure of the build or the device work is reported against the shader.
        const auto rethrow = [&](const std::exception& error) {
            char where[64];
            std::snprintf(where, sizeof(where), "compute shader 0x%llx: ", static_cast<unsigned long long>(address));
            throw std::runtime_error(where + std::string(error.what()));
        };
        phase(PhaseSnapshots);
        // The matched variant's recipe (design_cpu_final M4): a value-equal hit whose variant
        // carries one skips PrepareDispatch (the content key, the cache find, stage A), the
        // pipeline lookup and the data-word compare; its pre-checks run here without the mutex,
        // its proof under it. Stamp validation keeps no recipes (one variant, the stamps' path).
        // APS5_NO_DISPATCH_RECIPE=1 takes the ordinary path for every hit.
        std::shared_ptr<RecipeHit> recipeHit;
        if (cached && keepVariant != nullptr && !StampValidate()) {
            recipeHit = localDevice->PrepareRecipe(keepVariant->recipe.load(std::memory_order_acquire), indirectArguments != 0);
            phase(PhaseRecipePrecheck);
        }
        // The recipe the device call builds for the variant (a hit without a usable recipe, or
        // the miss's fresh variant); attached after the call.
        std::shared_ptr<const Recipe> builtRecipe;
        auto* const attachTo = StampValidate() ? nullptr : keepVariant != nullptr ? keepVariant.get() : attachVariant.get();
        bool writersNoted = false;
        const bool noteWrites = WriteEvidenceEnabled() || TraceCapSync();
        // A Rebuild from the recipe path (a failed proof, a replaced device) restarts here at
        // PrepareDispatch with the mutex released, so stage A stays unlocked; once at most, since
        // the second pass has no recipe.
        for (;;) {
            // Stage A of the resource build (copies, buffers, the descriptor set) runs before the
            // lock, which then covers only stage B (texture lookups, imports, descriptor writes) and
            // the record: a compute queue's build no longer stalls the graphics queue for its whole
            // resources phase. Null when the resource cache may serve this dispatch, or with
            // APS5_LOCKED_BUILD=1 (the whole build under the lock as before). A recipe hit skips
            // it, except under APS5_VERIFY_RECIPE=1 (the prepared object is then compared).
            std::shared_ptr<PreparedDispatch> prepared;
            if (recipeHit == nullptr || VulkanDevice::VerifyRecipes()) {
                try {
                    prepared = localDevice->PrepareDispatch(compiled, snapshots);
                } catch (const std::exception& error) {
                    rethrow(error);
                }
                if (profile) {
                    const auto now = std::chrono::steady_clock::now();
                    const auto prepareMs = std::chrono::duration<double, std::milli>(now - phaseLap).count();
                    phaseLap = now;
                    double parts = 0;
                    if (prepared != nullptr) {
                        const auto phases = VulkanDevice::PreparePhaseMs(*prepared);
                        phaseMs[PhasePrepareKey] += phases[0];
                        phaseMs[PhasePrepareFind] += phases[1];
                        phaseMs[PhasePreparePrecollect] += phases[2];
                        phaseMs[PhasePreparePresync] += phases[3];
                        phaseMs[PhasePrepareStageA] += phases[4];
                        for (const auto part : phases) parts += part;
                    }
                    phaseMs[PhasePrepareOther] += std::max(0.0, prepareMs - parts);
                }
            }
            GuestMemory::TagGpuLockSite(indirectArguments != 0 ? GuestMemory::GpuLockSite::Indirect : GuestMemory::GpuLockSite::Dispatch);
            std::lock_guard gpuLock(GuestMemory::GpuMutex());
            phase(PhaseLockWait);
            // This queue's labels first (queue order), and their batch out before the dispatch. Not
            // under the shader's name: a label that fails to record is not a shader failure.
            recordLabelsForPacket(localDevice.get(), submission.queue);
            phase(PhaseLabels);
            if (noteWrites && WriterKeyedEvidence() && !writersNoted) {
                noteWrittenBuffers(address, submission.queue, compiled);
                writersNoted = true;
            }
            phase(PhaseNoteWriters);
            try {
                if (recipeHit != nullptr) {
                    VulkanDevice::IndirectOutcome outcome{0, 0};
                    const auto result = localDevice->DispatchRecipe(compiled, groups[0], groups[1], groups[2], indirectArguments, address, recipeHit, outcome, VulkanDevice::VerifyRecipes() ? prepared : nullptr, dataHit);
                    if (result == RecipeOutcome::Rebuild) {
                        // Nothing was recorded: the ordinary path, re-attaching its recipe.
                        recipeHit = nullptr;
                        VulkanDevice::NoteRecipe(VulkanDevice::RecipeEvent::Restart, indirectArguments != 0);
                        continue;
                    }
                    if (indirectArguments != 0) countIndirect(outcome.cpuReason, outcome.argumentReadMs);
                } else if (indirectArguments != 0) {
                    const auto outcome = localDevice->DispatchIndirect(compiled, indirectArguments, snapshots, address, std::move(prepared), attachTo != nullptr ? &builtRecipe : nullptr);
                    countIndirect(outcome.cpuReason, outcome.argumentReadMs);
                } else {
                    localDevice->Dispatch(compiled, groups[0], groups[1], groups[2], snapshots, address, std::move(prepared), attachTo != nullptr ? &builtRecipe : nullptr);
                }
            } catch (const std::exception& error) {
                rethrow(error);
            }
            if (builtRecipe != nullptr) {
                if (dataHit) {
                    // Built from the patched result: the hash must name the variant's own words,
                    // else a later exact hit of the variant would find the template's (this hit's)
                    // words "the same" and skip the refresh back.
                    auto own = std::make_shared<Recipe>(*builtRecipe);
                    own->dataWordsHash = Graphics::ShaderResources::DataWordsHash({ShaderRecompiler::ShaderStage::Compute, keepVariant->compiled.get(), 0});
                    builtRecipe = std::move(own);
                }
                attachTo->recipe.store(std::move(builtRecipe), std::memory_order_release);
                VulkanDevice::NoteRecipe(VulkanDevice::RecipeEvent::Attach, indirectArguments != 0);
            }
            break;
        }
        phase(PhaseDevice);
        if (noteWrites && !WriterKeyedEvidence()) noteWrittenBuffers(address, submission.queue, compiled);
        deviceMs += elapsed();
        phase(PhaseTail);
        if (profile) {
            auto& pending = pendingDispatchPhases();
            pending.outcome = DispatchOutcome::Real;
            pending.phases = true;
            pending.hit = cached;
            pending.validated = validated;
            pending.ms = phaseMs;
            pending.tailAt = phaseLap;
        }
        if (profile && ++dispatches % 100 == 0) std::fprintf(stderr, "[gpu] %llu dispatches (%llu dispatch cache hits, %llu evictions, %llu recompile cache hits): capture %.1f s, cache key %.1f s, recompile %.1f s, device %.1f s\n", static_cast<unsigned long long>(dispatches), static_cast<unsigned long long>(dispatchCacheHits), static_cast<unsigned long long>(dispatchCacheEvictions), static_cast<unsigned long long>(cacheHits), captureMs / 1000, keyMs / 1000, recompileMs / 1000, deviceMs / 1000);
    }

    // DISPATCH_INDIRECT: the group counts are three dwords a shader usually wrote, and reading them
    // on the CPU waited for that shader (the flush hook syncs on its batch) before this dispatch could
    // even be prepared. The GPU reads them in place instead (VulkanDevice::DispatchIndirect, which
    // still falls back to a CPU read when it cannot see the current bytes); the CPU path stays for
    // thread-dimension initiators (the counts are rounded to groups here) and the fill HLE (needs the
    // count, decided in dispatch once the code is known). Debug aid: APS5_NO_GPU_INDIRECT=1 reads
    // every count on the CPU as before.
    void dispatchIndirect(QueueState& queue, std::span<const std::uint32_t> packet, const Submission& submission) {
        static const bool gpuIndirect = std::getenv("APS5_NO_GPU_INDIRECT") == nullptr;
        const auto arguments = Pm4::DispatchArgumentAddress(packet, queue);
        const auto initiator = packet.back();
        int path = IndirectGpu;
        if (!gpuIndirect) path = IndirectDisabled;
        else if ((initiator & 0x20u) != 0) path = IndirectThreadDimensions;
        else if (arguments % 4 != 0) path = IndirectMisaligned;
        if (path != IndirectGpu) {
            recordQueuedLabelsBeforeRead(submission.queue);
            const auto readStart = std::chrono::steady_clock::now();
            const auto direct = Pm4::ReadDispatchArguments(arguments, initiator);
            countIndirect(path, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - readStart).count());
            dispatch(queue, direct, submission);
            return;
        }
        // A DISPATCH_DIRECT shape with the counts unknown: dispatch reads only the initiator from it.
        const std::array<std::uint32_t, 5> unresolved{0xc0031500u, 0, 0, 0, initiator};
        dispatch(queue, unresolved, submission, arguments);
    }

    // Drawn; Nothing: the packet draws nothing (an empty count, or no color writes and no pixel
    // shader); Rejected: the register precheck refused it before the decode, `rejected` holding the
    // message the decode would have thrown (APS5_NO_DRAW_PRECHECK=1 leaves every rejection to the
    // decode's throw, as before).
    enum class DrawVerdict { Drawn, Nothing, Rejected };
    static bool DrawPrecheck() {
        static const bool precheck = std::getenv("APS5_NO_DRAW_PRECHECK") == nullptr;
        return precheck;
    }
    DrawVerdict draw(QueueState& queue, std::span<const std::uint32_t> packet, const Submission& submission, std::string& rejected) {
        PerformanceTimer timing("Driver.Draw");
        static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        std::array<double, DrawDriverPhaseCount> phaseMs{};
        std::uint64_t captures = 0;
        auto phaseLap = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        if (profile && packetStartedAt() != std::chrono::steady_clock::time_point{}) phaseMs[DrawRowPrologue] = std::chrono::duration<double, std::milli>(phaseLap - packetStartedAt()).count();
        const auto phase = [&](DrawDriverPhase which) {
            if (!profile) return;
            const auto now = std::chrono::steady_clock::now();
            phaseMs[which] += std::chrono::duration<double, std::milli>(now - phaseLap).count();
            phaseLap = now;
        };
        // The packet loop reads the phases after the call (see pendingDrawPhases).
        const auto drawn = [&] {
            phase(DrawRowVectors);
            if (!profile) return DrawVerdict::Drawn;
            auto& pending = pendingDrawPhases();
            pending.phases = true;
            pending.captures = captures;
            pending.ms = phaseMs;
            pending.tailAt = phaseLap;
            return DrawVerdict::Drawn;
        };
        auto drawParameters = Pm4::ResolveDraw(packet, queue);
        if (!drawParameters.indirect && !drawParameters.indexed && (drawParameters.indexCount == 0 || drawParameters.instanceCount == 0)) return DrawVerdict::Nothing;
        static const bool traceIndirect = std::getenv("APS5_TRACE_INDIRECT_DRAWS") != nullptr;
        if (traceIndirect && drawParameters.indirect) std::fprintf(stderr, "[draw] indirect packet %s args 0x%llx count %u reached\n", Pm4::Name(packet[0]).c_str(), static_cast<unsigned long long>(drawParameters.indirect->arguments), drawParameters.indirect->count);
        // Depth/stencil-only passes (no color writes, no pixel shader) have no effect without depth
        // targets, which are not emulated.
        {
            const auto targetMask = queue.context.find(0x8e);
            const auto shaderMask = queue.context.find(0x8f);
            const bool colorWrites = targetMask != queue.context.end() && shaderMask != queue.context.end() && (targetMask->second & shaderMask->second) != 0;
            if (!colorWrites && !queue.shader.contains(0x8)) return DrawVerdict::Nothing;
        }
        if (drawParameters.indexed) {
            const auto restart = queue.userConfig.find(0x24b);
            if (restart != queue.userConfig.end() && restart->second != 0) {
                rejected = "AGC graphics: primitive restart (GE_MULTI_PRIM_IB_RESET_EN) is unsupported for indexed draws";
                return DrawVerdict::Rejected;
            }
        }
        if (DrawPrecheck()) {
            rejected = Graphics::DrawRejection(queue, drawParameters.indexed);
            if (!rejected.empty()) return DrawVerdict::Rejected;
        }
        phase(DrawRowPrecheck);
        using Stage = ShaderRecompiler::ShaderStage;
        using Role = ShaderRecompiler::ProgramRole;
        // Debug aid: APS5_DUMP_DRAW_SHADERS=<hex color target> saves the requests of draws into that
        // target as shader_<address>.req for agc_shader_replay.
        static const std::uint64_t dumpTarget = [] { const char* text = std::getenv("APS5_DUMP_DRAW_SHADERS"); return text ? std::strtoull(text, nullptr, 16) : 0ull; }();
        // Debug aid: APS5_DUMP_DRAW_SLOT1=<hex address> saves the requests and registers of draws whose
        // second color target (slot 1) is at that address, as shader_<address>.req and draw_slot1.regs.
        static const std::uint64_t dumpSlot1 = [] { const char* text = std::getenv("APS5_DUMP_DRAW_SLOT1"); return text ? std::strtoull(text, nullptr, 16) : 0ull; }();
        // As in dispatch, the capture, the recompiles and the rect-list shaders run without the
        // GpuMutex (guest memory is read through the self-locking flush hook, the recompiler has its
        // own locks); only the device creation and the device Draw take it, so the compute workers
        // do not pile up behind a draw's whole preparation. The device is held by this shared_ptr
        // meanwhile (DevicePointer's arrow form is for holders of the mutex only); it is taken
        // before the decode because the draw key names its serial. Debug aid:
        // APS5_LOCKED_DRAW_PREPARE=1 holds the mutex across the preparation as before.
        static const bool lockedPrepare = std::getenv("APS5_LOCKED_DRAW_PREPARE") != nullptr;
        std::unique_lock gpuLock(GuestMemory::GpuMutex(), std::defer_lock);
        std::shared_ptr<VulkanDevice> localDevice;
        if (lockedPrepare) {
            // The device is created under the lock already held: one "draw" acquisition per draw,
            // as in the baseline, so the [lock] site counts stay comparable.
            GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Draw);
            gpuLock.lock();
            timing.Mark("gpu_mutex_wait");
            phase(DrawRowLockWait);
            if (device == nullptr) device = std::make_shared<VulkanDevice>();
            localDevice = device;
            // This queue's labels first (queue order), as at the unlocked path's acquisition below.
            recordLabelsForPacket(localDevice.get(), submission.queue);
            phase(DrawRowLabels);
        } else if ((localDevice = device.load()) == nullptr) {
            GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Draw);
            std::lock_guard createLock(GuestMemory::GpuMutex());
            if (device == nullptr) device = std::make_shared<VulkanDevice>();
            localDevice = device;
        }
        timing.Mark("device_setup");
        phase(DrawRowVectors);
        // The draw cache lookup (see drawCache): under the register key the entry is found before
        // any decode and its decode products serve the draw; every stage is validated over its own
        // runs under the DrawCache read site, the matched variants kept for the whole draw (a hit's
        // regions, results and vertex infos point into them).
        const bool useDrawEntries = DrawEntries() && !ShaderRecompiler::DebugProbeActive() && dumpTarget == 0 && dumpSlot1 == 0;
        const bool registerKey = useDrawEntries && RegisterKey();
        std::uint64_t drawKey = 0;
        std::shared_ptr<DrawEntry> entry;
        std::shared_ptr<const DrawDecode> decode;
        if (registerKey) {
            const auto keyStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
            drawKey = drawRegisterKey(queue, *submission.shaders, localDevice->Serial());
            std::lock_guard cacheLock(drawCacheMutex);
            ++drawEntryCounters.lookups;
            ++drawEntryCounters.registerKeyLookups;
            if (profile) drawEntryCounters.keyUs += std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - keyStart).count();
            const auto found = drawCache.find(drawKey);
            if (found != drawCache.end()) {
                entry = found->second;
                decode = entry->decode;
            } else {
                ++drawEntryCounters.absent;
            }
        }
        phase(DrawRowKeyLookupValidate);
        // The decode (DecodeState with DecodeShaderStages, the program prepare, DecodePixelStageInfo),
        // skipped when the entry carries it. In verify mode (APS5_VERIFY_DRAW_RECIPE=1) it runs on
        // every draw: with the register facade's log on, checked against DrawKeyRegisters, and
        // compared with the entry's decode when there is one (the fresh decode is used then).
        const auto programAddress = [&](std::uint32_t base) {
            Graphics::NoteRegisterRead(Graphics::RegisterBank::Shader, base);
            Graphics::NoteRegisterRead(Graphics::RegisterBank::Shader, base + 1);
            const auto high = readRegister(queue.shader, base + 1);
            require((high & ~0xffu) == 0, "reserved graphics program address bits are set");
            return (static_cast<std::uint64_t>(readRegister(queue.shader, base)) << 8u) | (static_cast<std::uint64_t>(high) << 40u);
        };
        const auto prepare = [&](std::uint64_t address, std::uint8_t type, Stage stage, std::uint32_t rsrc2, std::uint32_t userDataBase) {
            auto it = submission.shaders->upper_bound(address);
            require(it != submission.shaders->begin(), "graphics program does not belong to a registered shader");
            --it;
            const auto& snapshot = *it->second;
            require(address - snapshot.codeAddress < snapshot.code.size() * sizeof(std::uint32_t), "graphics program is outside registered shader code");
            require(snapshot.type == type, "graphics program refers to an incompatible shader binary type");
            Graphics::NoteRegisterRead(Graphics::RegisterBank::Shader, rsrc2);
            const auto resources = readRegister(queue.shader, rsrc2);
            const auto userCount = ((resources >> 1u) & 0x1fu) | (((resources >> 27u) & 1u) << 5u);
            require(userCount <= 32, "graphics user SGPR count exceeds the register bank");
            const auto codeOffset = static_cast<std::size_t>((address - snapshot.codeAddress) / sizeof(std::uint32_t));
            DrawProgram result{
                {stage, address, std::span(snapshot.code).subspan(codeOffset), snapshot.headerAddress, snapshot.header},
                userDataBase,
                8,
                {},
                {{{snapshot.codeAddress, std::as_bytes(std::span(snapshot.code))}, {snapshot.headerAddress, snapshot.header}}},
                it->second,
                codeOffset
            };
            for (std::uint32_t i = 0; i < userCount; ++i) {
                Graphics::NoteRegisterRead(Graphics::RegisterBank::Shader, userDataBase + i);
                result.userData.push_back(readRegister(queue.shader, userDataBase + i));
            }
            return result;
        };
        const auto decodeAll = [&] {
            auto product = std::make_shared<DrawDecode>();
            product->state = Graphics::DecodeState(queue);
            auto& programs = product->programs;
            auto& roles = product->roles;
            programs.reserve(5);
            roles.reserve(5);
            const auto append = [&](std::uint32_t base, std::uint8_t type, Stage stage, std::uint32_t resources, std::uint32_t users, Role role) {
                programs.push_back(prepare(programAddress(base), type, stage, resources, users));
                roles.push_back(role);
            };
            const auto initializeMerged = [&](DrawProgram& program, std::uint32_t pointerBase, bool pointerRequired) {
                program.firstUserSgpr = 0;
                program.userData.insert(program.userData.begin(), 8, 0);
                if (pointerRequired) {
                    Graphics::NoteRegisterRead(Graphics::RegisterBank::Shader, pointerBase);
                    Graphics::NoteRegisterRead(Graphics::RegisterBank::Shader, pointerBase + 1);
                    const auto low = readRegister(queue.shader, pointerBase);
                    const auto high = readRegister(queue.shader, pointerBase + 1);
                    const auto address = static_cast<std::uint64_t>(low) | (static_cast<std::uint64_t>(high) << 32u);
                    require(address != 0, "merged shader user-data address is null");
                    GuestMemory::CheckRange(reinterpret_cast<const void*>(address), 8, 4);
                    program.userData[0] = low;
                    program.userData[1] = high;
                }
            };
            const auto& graphics = product->state;
            if (graphics.stages.path == Graphics::ShaderPath::Tessellation) {
                append(0x148, 5, Stage::Local, 0x10b, 0x10c, Role::Local);
                append(0x108, 7, Stage::TessellationControl, 0x10b, 0x10c, Role::Hull);
                initializeMerged(programs.back(), 0x102, true);
                append(0x0c8, 2, Stage::TessellationEvaluation, 0x08b, 0x08c, Role::Domain);
            } else if (graphics.stages.path == Graphics::ShaderPath::Geometry) {
                const auto frontAddress = programAddress(0xc8);
                auto snapshot = submission.shaders->upper_bound(frontAddress);
                require(snapshot != submission.shaders->begin(), "geometry front program is not registered");
                --snapshot;
                const auto type = snapshot->second->type;
                require(type == 2 || type == 4, "invalid geometry front binary type");
                append(0xc8, type, Stage::Mesh, 0x8b, 0x8c, Role::Main);
                initializeMerged(programs.back(), 0x82, type == 4);
                if (type == 4) append(0x88, 6, Stage::Mesh, 0x8b, 0x8c, Role::GeometryBack);
            } else {
                append(0xc8, 2, Stage::Vertex, 0x8b, 0x8c, Role::Main);
            }
            append(0x008, 1, Stage::Fragment, 0x00b, 0x00c, Role::Fragment);
            programs.back().firstUserSgpr = 0;
            product->pixel = Graphics::DecodePixelStageInfo(queue.context, graphics.hasColorTarget, graphics.color.componentMapping);
            return product;
        };
        if (decode == nullptr || VerifyDrawRecipe()) {
            std::vector<Graphics::RegisterRead> readLog;
            struct LogScope {
                explicit LogScope(std::vector<Graphics::RegisterRead>* log) { Graphics::RegisterReadLog() = log; }
                ~LogScope() { Graphics::RegisterReadLog() = nullptr; }
            } logScope(VerifyDrawRecipe() && registerKey ? &readLog : nullptr);
            auto fresh = decodeAll();
            if (VerifyDrawRecipe() && registerKey) {
                std::uint64_t facadeMismatches = 0;
                for (const auto read : readLog) {
                    if (Graphics::DrawKeyCovers(read)) continue;
                    ++facadeMismatches;
                    static std::atomic<std::uint64_t> reports{0};
                    if (reports.fetch_add(1) < 20) std::fprintf(stderr, "[draw-cache] verify: the decoders read %s register 0x%x, which DrawKeyRegisters lacks\n", Graphics::RegisterBankName(read.bank), read.offset);
                }
                std::uint64_t decodeMismatches = 0;
                if (decode != nullptr && !sameDecode(*decode, *fresh)) {
                    decodeMismatches = 1;
                    static std::atomic<std::uint64_t> reports{0};
                    if (reports.fetch_add(1) < 20) std::fprintf(stderr, "[draw-cache] verify: the entry's decode differs from a fresh decode (key 0x%llx, target 0x%llx)\n", static_cast<unsigned long long>(drawKey), static_cast<unsigned long long>(fresh->state.color.address));
                }
                std::lock_guard cacheLock(drawCacheMutex);
                drawEntryCounters.facadeMismatches += facadeMismatches;
                if (decode != nullptr) ++drawEntryCounters.verifyDecodes;
                drawEntryCounters.verifyDecodeMismatches += decodeMismatches;
            }
            decode = std::move(fresh);
        } else if (profile) {
            std::lock_guard cacheLock(drawCacheMutex);
            ++drawEntryCounters.decodeSkipped;
        }
        // The draw's copies of the products: the CPU path of an indirect draw patches user words.
        const auto& graphics = decode->state;
        const auto& pixel = decode->pixel;
        std::vector<DrawProgram> programs = decode->programs;
        const std::vector<Role>& roles = decode->roles;
        phase(DrawRowDecode);
        // The user word an SH register named by an indirect draw packet lands in: the program whose
        // bank holds it and the index into its user data (a merged program carries eight hidden
        // words in front of its bank), or nothing for the sentinel 0x280 and a register no active
        // stage loads (the CP writes it; no shader sees it).
        const auto locate = [&](std::uint32_t location) -> std::optional<std::pair<std::size_t, std::size_t>> {
            if (location == 0x280u) return std::nullopt;
            for (std::size_t i = 0; i < programs.size(); ++i) {
                if (roles[i] == Role::Fragment || roles[i] == Role::GeometryBack || location < programs[i].userDataBase) continue;
                const auto word = location - programs[i].userDataBase + (8u - programs[i].firstUserSgpr);
                if (word < programs[i].userData.size()) return std::make_pair(i, static_cast<std::size_t>(word));
            }
            return std::nullopt;
        };
        if (drawParameters.indirect) {
            auto& indirect = *drawParameters.indirect;
            const auto sgprOf = [&](std::uint32_t location) -> std::int32_t {
                const auto word = locate(location);
                if (!word || word->first != 0) return -1;
                return static_cast<std::int32_t>(programs.front().firstUserSgpr + word->second);
            };
            indirect.baseVertexSgpr = sgprOf(indirect.baseVertexLocation);
            indirect.startInstanceSgpr = sgprOf(indirect.startInstanceLocation);
            indirect.drawIndexSgpr = sgprOf(indirect.drawIndexLocation);
        }
        std::vector<ShaderRecompiler::MemoryRegion> memory;
        std::vector<ShaderRecompiler::LinkedProgram> linked;
        for (std::size_t i = 0; i < programs.size(); ++i) {
            const auto& program = programs[i];
            memory.insert(memory.end(), program.memory.begin(), program.memory.end());
            linked.push_back({roles[i], program.binary, program.userDataBase, program.firstUserSgpr, program.userData});
        }
        timing.Mark("prepare");
        phase(DrawRowProgramPrepare);
        // The vertex stage infos: the decode reads the attribute and V# tables from guest memory
        // outside the capture (its reads are kept per stage, DecodeRead, and become runs of the
        // stage's variant), so under the register key a stage is decoded only when no variant
        // matched it (a matched variant carries the info it was compiled with); under the
        // decoded-bytes key every stage is decoded before the key. The CPU path of an indirect
        // draw decodes a program again once its user words are patched.
        std::vector<std::optional<ShaderRecompiler::ShaderVertexStageInfo>> vertexInfos(programs.size());
        std::vector<std::vector<Graphics::DecodeRead>> decodeReads(programs.size());
        const auto decodeVertexInfo = [&](std::size_t i) {
            const auto& program = programs[i];
            if (program.binary.stage == Stage::Fragment || roles[i] == Role::GeometryBack) return;
            decodeReads[i].clear();
            vertexInfos[i] = Graphics::DecodeVertexStageInfo(program.binary.header, program.binary.headerAddress, program.userData, &decodeReads[i]);
        };
        if (!registerKey) {
            for (std::size_t i = 0; i < programs.size(); ++i) decodeVertexInfo(i);
            phase(DrawRowDecode);
        }
        ShaderMemory shaderMemory(memory, &ClassifyPendingWrite, &ObservePendingWrite, HookWaitCounter());
        std::vector<ShaderRecompiler::RecompileResult> results;
        std::vector<Graphics::CompiledShader> stages;
        results.reserve(programs.size() + (graphics.rectList ? 2u : 0u));
        stages.reserve(programs.size());
        std::uint32_t pushCursorBytes = 0;
        // The compiled result of each program (null for GeometryBack): a miss's copy in `results`,
        // a hit's shared object held by the matched variant.
        std::vector<const ShaderRecompiler::RecompileResult*> programResults(programs.size(), nullptr);
        // A miss's per-stage capture, what the draw entry's variant is made of.
        struct StageCapture {
            std::shared_ptr<const ShaderRecompiler::RecompileResult> compiled;
            std::vector<ShaderRecompiler::MemoryRegion> regions;
            std::uint64_t forgetSerial = 0;
            std::uint32_t pushOffset = 0;
        };
        std::vector<StageCapture> stageCaptures(programs.size());
        std::vector<std::shared_ptr<DispatchVariant>> matched(programs.size());
        std::vector<std::vector<ShaderRecompiler::MemoryRegion>> matchedRegions(programs.size());
        // A miss's fresh variants (per program index), the kept objects after the insert.
        std::vector<std::shared_ptr<DispatchVariant>> fresh(programs.size());
        // The stages `compile` below captured on the shared ShaderMemory: a hit's other stages
        // keep their matched regions in the draw's union.
        std::vector<bool> recompiled(programs.size(), false);
        bool drawHit = false;
        bool verifyHit = false;
        if (useDrawEntries) {
            if (!registerKey) {
                drawKey = 0xcbf29ce484222325ull;
                const auto mix = [&](std::uint64_t value) {
                    drawKey ^= value;
                    drawKey *= 0x100000001b3ull;
                };
                mix(localDevice->Serial());
                mix(static_cast<std::uint64_t>(graphics.stages.path));
                mix(graphics.stages.registerValue);
                mix(graphics.stages.vertexWaveSize);
                mix(graphics.stages.fragmentWaveSize);
                mix(graphics.stages.mesh.has_value());
                if (graphics.stages.mesh) {
                    const auto& mesh = *graphics.stages.mesh;
                    for (const auto value : {mesh.inputPrimitive, mesh.primitivesPerGroup, mesh.verticesPerGroup, mesh.maxVertices, mesh.maxPrimitives, mesh.threadsPerGroup, mesh.ldsSizeDwords, mesh.provokingVertex}) mix(value);
                }
                mix(graphics.stages.tessellation.has_value());
                if (graphics.stages.tessellation) {
                    const auto& tess = *graphics.stages.tessellation;
                    for (const auto value : {tess.inputControlPoints, tess.outputControlPoints, tess.domain, tess.partitioning, tess.outputTopology}) mix(value);
                }
                mix(graphics.rectList);
                mix(programs.size());
                for (std::size_t i = 0; i < programs.size(); ++i) {
                    const auto& program = programs[i];
                    mix(reinterpret_cast<std::uintptr_t>(program.snapshot.get()));
                    mix(program.codeOffset);
                    mix(static_cast<std::uint64_t>(roles[i]));
                    mix(static_cast<std::uint64_t>(program.binary.stage));
                    mix(program.userDataBase);
                    mix(program.firstUserSgpr);
                    mix(program.userData.size());
                    for (const auto word : program.userData) mix(word);
                    mix(vertexInfos[i].has_value());
                    if (!vertexInfos[i]) continue;
                    const auto& vertex = *vertexInfos[i];
                    require(vertex.resourcesNum <= vertex.resources.size(), "vertex stage info resource count exceeds its table");
                    mix(vertex.resourcesNum);
                    mix(vertex.fetchAttribReg);
                    mix(vertex.fetchBufferReg);
                    mix(vertex.fetchEmbedded);
                    for (std::uint32_t r = 0; r < vertex.resourcesNum; ++r) {
                        for (const auto field : vertex.resources[r].fields) mix(field);
                        const auto& destination = vertex.resourcesDst[r];
                        mix(static_cast<std::uint32_t>(destination.registerStart));
                        mix(static_cast<std::uint32_t>(destination.registersNum));
                        mix(static_cast<std::uint32_t>(destination.attrId));
                        mix(destination.fetchIndex);
                    }
                }
                require(pixel.interpolatorCount <= pixel.interpolatorSettings.size(), "pixel stage info interpolator count exceeds its table");
                mix(pixel.interpolatorCount);
                for (std::uint32_t i = 0; i < pixel.interpolatorCount; ++i) mix(pixel.interpolatorSettings[i]);
                mix(pixel.perspectiveCenterVgpr);
                for (const bool flag : {pixel.wave32, pixel.hasPerspectiveCenterVgpr, pixel.posX, pixel.posY, pixel.posZ, pixel.posW, pixel.frontFace, pixel.ancillary, pixel.sampleShading, pixel.noPerspective, pixel.pixelKillEnable, pixel.depthExportEnable, pixel.sampleMaskExportEnable, pixel.earlyZ, pixel.executeOnNoop}) mix(flag);
                for (const auto value : pixel.targetOutputMode) mix(value);
                for (const auto value : pixel.targetExportMapping) mix(value);
                std::lock_guard cacheLock(drawCacheMutex);
                ++drawEntryCounters.lookups;
                const auto found = drawCache.find(drawKey);
                if (found != drawCache.end()) entry = found->second;
                else ++drawEntryCounters.absent;
            }
            if (entry != nullptr) {
                const auto waitedBeforeValidate = profile ? Graphics::Recorder::ThreadWaitedMs() : 0.0;
                std::optional<DrawMiss> miss;
                std::vector<std::size_t> ranks(programs.size(), 0);
                std::uint64_t stageValidations = 0, stageEqual = 0, compared = 0, imagesFlushed = 0, runsSynced = 0;
                if (entry->stages.size() != programs.size()) miss = DrawMiss::Stages;
                // The push offset the stage must have been compiled at: the matched earlier
                // stages' push sizes.
                std::uint32_t cursor = 0;
                {
                    const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::DrawCache);
                    // One evidence sample for the whole validation, as a dispatch's.
                    std::optional<SampledReadScope> sampling;
                    for (std::size_t i = 0; !miss && i < programs.size(); ++i) {
                        if (roles[i] == Role::GeometryBack) continue;
                        ++stageValidations;
                        const auto& variants = entry->stages[i];
                        auto outcome = EntryOutcome::Differing;
                        bool anyLayout = false;
                        for (std::size_t rank = 0; rank < variants.size(); ++rank) {
                            const auto& variant = variants[rank];
                            if (variant->pushOffset != cursor) continue;
                            auto& regions = matchedRegions[i];
                            regions.clear();
                            appendEntryRegions(*variant, regions);
                            ++compared;
                            auto result = validateVariant(programs[i].binary.codeAddress, submission.queue, *variant, regions, imagesFlushed, runsSynced, sampling);
                            if (GateRetry() && (result == EntryOutcome::PublishMoved || result == EntryOutcome::PendingMoved)) result = validateVariant(programs[i].binary.codeAddress, submission.queue, *variant, regions, imagesFlushed, runsSynced, sampling);
                            if (!anyLayout) outcome = result;
                            anyLayout = true;
                            if (result != EntryOutcome::Equal) continue;
                            matched[i] = variant;
                            ranks[i] = rank;
                            ++stageEqual;
                            cursor += static_cast<std::uint32_t>(variant->compiled->pushConstants.size());
                            break;
                        }
                        if (matched[i] != nullptr) continue;
                        if (!anyLayout) miss = DrawMiss::Layout;
                        else if (outcome != EntryOutcome::Differing) miss = DrawMiss::Gate;
                        else miss = roles[i] == Role::Fragment ? DrawMiss::FragmentDiffering : i == 0 ? DrawMiss::FrontDiffering : DrawMiss::OtherDiffering;
                    }
                }
                drawHit = !miss;
                std::lock_guard cacheLock(drawCacheMutex);
                auto& counters = drawEntryCounters;
                counters.stageValidations += stageValidations;
                counters.stageEqual += stageEqual;
                counters.variantsCompared += compared;
                if (drawHit) {
                    ++counters.hits;
                    if (registerKey) ++counters.registerKeyHits;
                    ++drawCacheHits;
                    bool rotate = false;
                    for (std::size_t i = 0; i < programs.size(); ++i) {
                        if (matched[i] == nullptr) continue;
                        ++counters.variantHitsByRank[ranks[i]];
                        if (ranks[i] != 0) rotate = true;
                    }
                    // Only the entry still in the map is reordered: a stage hit behind its front
                    // moves to the front by replacing the entry with a rotated copy.
                    const auto again = drawCache.find(drawKey);
                    if (again != drawCache.end() && again->second == entry) {
                        if (rotate) {
                            auto rotated = std::make_shared<DrawEntry>();
                            rotated->decode = entry->decode;
                            rotated->stages = entry->stages;
                            rotated->recipes.store(entry->recipes.load());
                            for (std::size_t i = 0; i < programs.size(); ++i) {
                                if (ranks[i] == 0) continue;
                                auto& variants = rotated->stages[i];
                                variants.erase(variants.begin() + static_cast<std::ptrdiff_t>(ranks[i]));
                                variants.insert(variants.begin(), matched[i]);
                            }
                            rotated->touched = entry->touched;
                            rotated->order = entry->order;
                            again->second = std::move(rotated);
                        }
                        if (drawCacheHits - again->second->touched > DrawCacheEntries() / 8) {
                            drawOrder.splice(drawOrder.begin(), drawOrder, again->second->order);
                            again->second->touched = drawCacheHits;
                            ++counters.touches;
                        }
                    }
                    if (VerifyDrawEntries()) {
                        ++counters.verifyHits;
                        verifyHit = true;
                        drawHit = false;
                    }
                } else {
                    ++counters.misses[static_cast<std::size_t>(*miss)];
                    if (registerKey && entry->decode != nullptr) ++counters.decodePartial;
                }
                if (profile) {
                    // The hook waits inside the validations are their own row.
                    phase(DrawRowKeyLookupValidate);
                    const auto waited = std::min(Graphics::Recorder::ThreadWaitedMs() - waitedBeforeValidate, phaseMs[DrawRowKeyLookupValidate]);
                    phaseMs[DrawRowKeyLookupValidate] -= waited;
                    phaseMs[DrawRowValidateWait] += waited;
                    counters.validateUs += phaseMs[DrawRowKeyLookupValidate] * 1000;
                    if (std::chrono::steady_clock::now() - counters.lastReport > std::chrono::seconds(10)) {
                        counters.lastReport = std::chrono::steady_clock::now();
                        reportDrawCache(counters);
                    }
                }
            }
            phase(DrawRowKeyLookupValidate);
        }
        if (registerKey) {
            // On a hit every stage's info is its variant's; a miss (a partial one included: its
            // matched stages are compiled again, and the fresh variant's runs must carry the
            // decode's reads, rule RD) and a verified hit (for the compare) decode every stage now.
            for (std::size_t i = 0; i < programs.size(); ++i) {
                if (matched[i] != nullptr && drawHit && !VerifyDrawRecipe()) {
                    if (matched[i]->vertexInfo != nullptr) vertexInfos[i] = *matched[i]->vertexInfo;
                    continue;
                }
                decodeVertexInfo(i);
                if (matched[i] != nullptr && VerifyDrawRecipe() && (vertexInfos[i].has_value() != (matched[i]->vertexInfo != nullptr) || (vertexInfos[i] && !sameVertexInfo(*vertexInfos[i], *matched[i]->vertexInfo)))) {
                    static std::atomic<std::uint64_t> reports{0};
                    if (reports.fetch_add(1) < 20) std::fprintf(stderr, "[draw-cache] verify: stage %zu (program 0x%llx) of a hit has a vertex stage info unlike its variant's\n", i, static_cast<unsigned long long>(programs[i].binary.codeAddress));
                    std::lock_guard cacheLock(drawCacheMutex);
                    ++drawEntryCounters.verifyDecodeMismatches;
                }
            }
            phase(DrawRowDecode);
        }
        // One program's capture and recompile at a push constant offset; the CPU path of an
        // indirect draw repeats it for a program whose user words the CP would have patched.
        const auto compile = [&](std::size_t i, std::uint32_t pushOffset) {
            phase(DrawRowVectors);
            const auto& program = programs[i];
            const auto waveSize = program.binary.stage == Stage::Fragment ? graphics.stages.fragmentWaveSize : graphics.stages.vertexWaveSize;
            ShaderRecompiler::RecompileRequest request{
                program.binary,
                {waveSize, program.firstUserSgpr, program.userData, std::nullopt, program.binary.stage == Stage::Fragment ? std::optional(pixel) : std::nullopt, vertexInfos[i], memory},
                localDevice->Target(),
                {0, 0, pushOffset, Graphics::PipelinePushConstantBytes - pushOffset},
                ShaderRecompiler::GraphicsCompileContext{program.firstUserSgpr, linked, graphics.stages.mesh, graphics.stages.tessellation, {drawParameters.indexAddress, drawParameters.indexCount, drawParameters.indexSize, drawParameters.instanceCount}}
            };
            const auto waitedBefore = TraceCapSync() || profile ? Graphics::Recorder::ThreadWaitedMs() : 0.0;
            const auto handle = SourceHandleFor(*program.snapshot, program.codeOffset, localDevice->Serial(), request, false);
            auto& stageCapture = stageCaptures[i];
            stageCapture.forgetSerial = GuestMemory::ForgetSerial();
            stageCapture.pushOffset = pushOffset;
            const auto capture = [&] {
                const SampledReadScope sampling(evidenceReads);
                return shaderMemory.Capture(request, handle.get());
            }();
            // This stage's own reads (a page the earlier stage fetched included), the entry's runs.
            stageCapture.regions = shaderMemory.TakeRecentRegions();
            recompiled[i] = true;
            memory = shaderMemory.Regions();
            // Regions() holds only the stages captured here: on a hit (the CPU path of an
            // indirect draw recompiling a patched program) the other stages' validated regions
            // stay in the union (rule RD).
            if (drawHit) {
                for (std::size_t j = 0; j < programs.size(); ++j) {
                    if (matched[j] != nullptr && !recompiled[j]) memory.insert(memory.end(), matchedRegions[j].begin(), matchedRegions[j].end());
                }
            }
            request.context.memory = memory;
            if (TraceCapSync()) traceCapture("draw-capture", program.binary.codeAddress, submission.queue, memory, Graphics::Recorder::ThreadWaitedMs() - waitedBefore);
            if (profile) {
                ++captures;
                phase(DrawRowCapture);
                // The hook waits inside the capture are their own row, as the validate row's are.
                const auto waited = std::min(Graphics::Recorder::ThreadWaitedMs() - waitedBefore, phaseMs[DrawRowCapture]);
                phaseMs[DrawRowCapture] -= waited;
                phaseMs[DrawRowCaptureHookWaits] += waited;
            }
            if (dumpTarget != 0) {
                // Match the first enabled target or the raw slot-0 base, which skip reports name.
                const auto slot0 = (static_cast<std::uint64_t>(readRegister(queue.context, 0x390)) << 40u) | (static_cast<std::uint64_t>(readRegister(queue.context, 0x318)) << 8u);
                if ((graphics.hasColorTarget && graphics.color.address == dumpTarget) || slot0 == dumpTarget) static_cast<void>(dumpRequest(program.binary.codeAddress, request));
            }
            if (dumpSlot1 != 0) {
                const auto value = [&](std::uint32_t offset) -> std::uint64_t { const auto it = queue.context.find(offset); return it == queue.context.end() ? 0u : it->second; };
                const auto slot1 = (value(0x391) << 40u) | (value(0x327) << 8u);
                if (slot1 == dumpSlot1) {
                    static_cast<void>(dumpRequest(program.binary.codeAddress, request));
                    if (std::FILE* file = std::fopen("draw_slot1.regs", "w")) {
                        for (const auto& [offset, value] : queue.context) std::fprintf(file, "context %x %08x\n", offset, value);
                        for (const auto& [offset, value] : queue.userConfig) std::fprintf(file, "uconfig %x %08x\n", offset, value);
                        for (const auto& [offset, value] : queue.shader) std::fprintf(file, "shader %x %08x\n", offset, value);
                        std::fclose(file);
                    }
                }
            }
            // As in dispatch: the recompile reuses the capture (APS5_NO_CAPTURE_REUSE=1 restores).
            static const bool reuseCapture = std::getenv("APS5_NO_CAPTURE_REUSE") == nullptr;
            phase(DrawRowCapture);
            // The memo'd result is copied into this draw's results (stages point into them and
            // the CPU indirect path rewrites slots); the descriptor population is what the memo
            // saves. The shared object is what the draw entry's variant keeps.
            stageCapture.compiled = reuseCapture ? ShaderRecompiler::Recompile(request, *capture) : std::make_shared<const ShaderRecompiler::RecompileResult>(ShaderRecompiler::Recompile(request));
            ShaderRecompiler::RecompileResult result = *stageCapture.compiled;
            phase(DrawRowRecompile);
            return result;
        };
        // The front program's user word an SGPR holds (the recompiler numbers user SGPRs from
        // firstUserSgpr).
        const auto userWord = [&](std::int32_t sgpr) {
            const auto& program = programs.front();
            require(sgpr >= 0 && static_cast<std::uint32_t>(sgpr) >= program.firstUserSgpr, "invalid draw offset SGPR");
            const auto index = static_cast<std::uint32_t>(sgpr) - program.firstUserSgpr;
            require(index < program.userData.size(), "draw offset SGPR exceeds user data");
            return program.userData[index];
        };
        // The fixed-function fetch starts where the hardware's does: the shader adds its base
        // vertex / start instance SGPR to the id VGPR itself (the analyzer names it), so that user
        // word is folded into the draw's first vertex / instance, on top of GE_INDX_OFFSET, which
        // the hardware adds as well (APS5_INDX_OFFSET_SKIP_FOLD=1 restores the skip of the fold
        // when it is set), and for indexed draws as the vertex offset (APS5_NO_INDEXED_OFFSET_FOLD=1
        // restores the old no-fold).
        static const bool indxOffsetSkipFold = std::getenv("APS5_INDX_OFFSET_SKIP_FOLD") != nullptr;
        static const bool indexedOffsetFold = std::getenv("APS5_NO_INDEXED_OFFSET_FOLD") == nullptr;
        const auto fold = [&](const ShaderRecompiler::RecompileResult& main, Pm4::DrawParameters& parameters) {
            if (parameters.indexed && !indexedOffsetFold) return;
            if (main.vertexOffsetSgpr >= 0 && (parameters.firstVertex == 0 || !indxOffsetSkipFold)) {
                const auto offset = userWord(main.vertexOffsetSgpr);
                require(offset <= std::numeric_limits<std::uint32_t>::max() - parameters.firstVertex, "draw vertex offset overflow");
                parameters.firstVertex += offset;
            }
            if (main.instanceOffsetSgpr >= 0) parameters.firstInstance = userWord(main.instanceOffsetSgpr);
        };
        // Why an indirect draw's records must be read on the CPU (nothing: the GPU reads them).
        std::optional<Graphics::IndirectDrawPath> indirectCpu;
        std::vector<std::uint32_t> pushOffsets(programs.size(), 0);
        std::vector<std::size_t> resultIndex(programs.size(), 0);
        for (std::size_t i = 0; i < programs.size(); ++i) {
            if (roles[i] == Role::GeometryBack) continue;
            const auto& program = programs[i];
            pushOffsets[i] = pushCursorBytes;
            if (drawHit) {
                // The matched variant's shared result and its validated regions (the draw's
                // union, rule RD).
                programResults[i] = matched[i]->compiled.get();
                memory.insert(memory.end(), matchedRegions[i].begin(), matchedRegions[i].end());
            } else {
                resultIndex[i] = results.size();
                results.push_back(compile(i, pushCursorBytes));
                programResults[i] = &results.back();
            }
            const auto& result = *programResults[i];
            if (i == 0 && drawParameters.indirect) {
                // Per dimension (vertex, instance) how the GPU reproduces the CP's register write
                // for the fixed-function fetch, with k the SGPR the shader adds to the dimension's
                // id VGPR, p the SGPR the packet names for the CP's write and X GE_INDX_OFFSET (the
                // vertex dimension of non-indexed draws): a sentinel p leaves the shader's add
                // reading the game's word, so the record's dword is rewritten with X + user[k]
                // (Constant); k == p read nowhere else means the record's value is exactly the fold
                // (InPlace; X must be 0, the GPU cannot add it to a record); anything else needs the
                // CP's write in the user word, which only the CPU path below makes.
                auto& indirect = *drawParameters.indirect;
                using Rule = Pm4::DrawParameters::IndirectDraw::Rule;
                using Path = Graphics::IndirectDrawPath;
                const auto support = localDevice->DrawIndirectSupport();
                const auto fail = [&](Path reason) { if (!indirectCpu) indirectCpu = reason; };
                if (graphics.stages.path != Graphics::ShaderPath::Vertex || graphics.rectList) fail(Path::NonVertexPath);
                if (indirect.drawIndexEnabled && indirect.drawIndexSgpr >= 0) fail(Path::DrawIndex);
                if (indirect.countIndirect && !support.count) fail(Path::FeatureGap);
                if (indirect.countIndirect && indirect.count > 1 && !support.multi) fail(Path::FeatureGap);
                const auto dimension = [&](std::int32_t k, std::int32_t p, bool shared, bool conflict, std::uint32_t x, Rule& rule, std::uint32_t& constant) {
                    if (p < 0) {
                        rule = Rule::Constant;
                        const auto offset = k >= 0 ? userWord(k) : 0u;
                        require(offset <= std::numeric_limits<std::uint32_t>::max() - x, "draw vertex offset overflow");
                        constant = x + offset;
                    } else if (conflict) fail(Path::FetchUnknown);
                    else if (k != p || shared) fail(Path::NotFolded);
                    else if (x != 0) fail(Path::IndxOffset);
                    else rule = Rule::InPlace;
                };
                dimension(result.vertexOffsetSgpr, indirect.baseVertexSgpr, result.vertexOffsetShared, result.vertexOffsetConflict, drawParameters.indexed ? 0u : indirect.indxOffset, indirect.vertexRule, indirect.vertexConstant);
                dimension(result.instanceOffsetSgpr, indirect.startInstanceSgpr, result.instanceOffsetShared, result.instanceOffsetConflict, 0u, indirect.instanceRule, indirect.instanceConstant);
                if (!support.firstInstance && (indirect.instanceRule == Rule::InPlace || indirect.instanceConstant != 0)) fail(Path::FeatureGap);
                if ((indirect.vertexRule == Rule::Constant || indirect.instanceRule == Rule::Constant) && indirect.count > 256) fail(Path::FeatureGap);
                // The GPU path copies every vertex descriptor's whole range (the counts are in the
                // records): APS5_INDIRECT_VERTEX_MIB (default 64) caps it.
                static const std::uint64_t vertexCap = [] { const char* text = std::getenv("APS5_INDIRECT_VERTEX_MIB"); return (text ? std::strtoull(text, nullptr, 10) : 64ull) << 20u; }();
                for (const auto& attribute : result.vertexAttributes) {
                    const auto stride = (attribute.resource.fields[1] >> 16u) & 0x3fffu;
                    const auto extent = stride == 0 ? static_cast<std::uint64_t>(attribute.resource.fields[2]) : static_cast<std::uint64_t>(attribute.resource.fields[2]) * stride;
                    if (extent > vertexCap) fail(Path::VertexRange);
                }
                if (traceIndirect) {
                    const auto location = [](std::uint32_t value, std::int32_t sgpr) { char text[24]; if (value == 0x280u) std::snprintf(text, sizeof(text), "none"); else std::snprintf(text, sizeof(text), "0x%x(s%d)", value, sgpr); return std::string(text); };
                    const auto rule = [](Rule value, std::uint32_t constant) { char text[24]; if (value == Rule::InPlace) std::snprintf(text, sizeof(text), "in-place"); else std::snprintf(text, sizeof(text), "const %u", constant); return std::string(text); };
                    std::string decision = indirectCpu ? "cpu (" + std::string(Graphics::IndirectDrawPathName(*indirectCpu)) + ")" : "gpu vertex=" + rule(indirect.vertexRule, indirect.vertexConstant) + " instance=" + rule(indirect.instanceRule, indirect.instanceConstant);
                    std::fprintf(stderr, "[draw] indirect 0x%x args 0x%llx stride %u count %u%s locs base=%s inst=%s idx=%s%s analyzer v=%d i=%d shared=%d/%d conflict=%d/%d indx=%u -> %s\n", indirect.opcode, static_cast<unsigned long long>(indirect.arguments), indirect.stride, indirect.count, indirect.countIndirect ? " (indirect)" : "", location(indirect.baseVertexLocation, indirect.baseVertexSgpr).c_str(), location(indirect.startInstanceLocation, indirect.startInstanceSgpr).c_str(), location(indirect.drawIndexLocation, indirect.drawIndexSgpr).c_str(), indirect.drawIndexEnabled ? " (enabled)" : "", result.vertexOffsetSgpr, result.instanceOffsetSgpr, result.vertexOffsetShared ? 1 : 0, result.instanceOffsetShared ? 1 : 0, result.vertexOffsetConflict ? 1 : 0, result.instanceOffsetConflict ? 1 : 0, indirect.indxOffset, decision.c_str());
                }
            } else if (i == 0) {
                fold(result, drawParameters);
            }
            require(result.pushConstants.size() <= Graphics::PipelinePushConstantBytes - pushCursorBytes, "stage push constants exceed the pipeline push constant block");
            stages.push_back({program.binary.stage, &result, result.pushConstants.empty() ? 0u : pushCursorBytes});
            pushCursorBytes += static_cast<std::uint32_t>(result.pushConstants.size());
        }
        // A miss inserts its stages' fresh variants (the CPU path of an indirect draw recompiles
        // on patched words below: nothing of it is kept); a verified hit compares them with the
        // matched variants first.
        if (useDrawEntries && !drawHit && !(drawParameters.indirect && indirectCpu)) {
            phase(DrawRowVectors);
            std::uint64_t unstable = 0, mismatches = 0;
            for (std::size_t i = 0; i < programs.size(); ++i) {
                const auto& stageCapture = stageCaptures[i];
                if (stageCapture.compiled == nullptr) continue;
                auto variant = std::make_shared<DispatchVariant>();
                variant->compiled = stageCapture.compiled;
                variant->shader = programs[i].snapshot;
                variant->forgetSerial = stageCapture.forgetSerial;
                variant->pushOffset = stageCapture.pushOffset;
                if (vertexInfos[i]) variant->vertexInfo = std::make_shared<const ShaderRecompiler::ShaderVertexStageInfo>(*vertexInfos[i]);
                // The stage's runs: its capture's regions and the vertex decode's guest reads
                // (rule RD), in address order.
                std::vector<ShaderRecompiler::MemoryRegion> regions(stageCapture.regions.begin(), stageCapture.regions.end());
                for (const auto& read : decodeReads[i]) regions.push_back({read.address, std::as_bytes(std::span(read.bytes))});
                std::stable_sort(regions.begin(), regions.end(), [](const ShaderRecompiler::MemoryRegion& a, const ShaderRecompiler::MemoryRegion& b) { return a.guestAddress < b.guestAddress; });
                for (const auto& region : regions) {
                    variant->runs.emplace_back(region.guestAddress, region.guestAddress + region.bytes.size());
                    const auto count = region.bytes.size() / sizeof(std::uint32_t);
                    const auto offset = variant->words.size();
                    variant->words.resize(offset + count);
                    std::memcpy(variant->words.data() + offset, region.bytes.data(), count * sizeof(std::uint32_t));
                }
                if (verifyHit && matched[i] != nullptr && (matched[i]->runs != variant->runs || matched[i]->words != variant->words)) {
                    ++mismatches;
                    static std::atomic<std::uint64_t> reports{0};
                    if (reports.fetch_add(1) < 20) std::fprintf(stderr, "[draw-cache] verify: stage %zu (program 0x%llx) of a hit captured differently: %zu runs / %zu words matched, %zu / %zu fresh\n", i, static_cast<unsigned long long>(programs[i].binary.codeAddress), matched[i]->runs.size(), matched[i]->words.size(), variant->runs.size(), variant->words.size());
                }
                if (InsertCompare()) {
                    const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::DrawCache);
                    if (!captureStable(stageCapture.regions)) {
                        ++unstable;
                        continue;
                    }
                }
                fresh[i] = std::move(variant);
            }
            insertDrawEntry(drawKey, fresh, registerKey ? decode : nullptr);
            if (unstable != 0 || mismatches != 0) {
                std::lock_guard cacheLock(drawCacheMutex);
                drawEntryCounters.unstable += unstable;
                drawEntryCounters.verifyMismatches += mismatches;
            }
            phase(DrawRowKeyLookupValidate);
        }
        timing.Mark("shader_compile_and_link");
        // The vertex decode's guest reads join the union the label test and the snapshots cover
        // (a matched stage's are among its regions already).
        for (const auto& reads : decodeReads) {
            for (const auto& read : reads) memory.push_back({read.address, std::as_bytes(std::span(read.bytes))});
        }
        // As in dispatch: a label this queue still has queued over a captured region is recorded
        // now and the draw starts over (nothing queued on the locked-prepare path, recorded above).
        if (recordQueuedLabelsAfterCapture(submission.queue, memory)) return draw(queue, packet, submission, rejected);
        // The rect-list stages are generated from the front and fragment results (rebuilt when the
        // CPU path below recompiles the front program).
        bool rectListBuilt = false;
        // Where the rect-list results sit in `results` (behind a miss's program results; first
        // on a hit).
        std::size_t rectIndex = 0;
        const auto buildRectList = [&] {
            phase(DrawRowVectors);
            require(programs.size() == 2 && programResults[0] != nullptr && programResults[1] != nullptr, "rect-list requires vertex and fragment programs");
            auto rectangle = ShaderRecompiler::BuildRectListShaders(*programResults[0], *programResults[1], localDevice->Target());
            if (rectListBuilt) {
                results[rectIndex] = std::move(rectangle.control);
                results[rectIndex + 1] = std::move(rectangle.evaluation);
                phase(DrawRowRectList);
                return;
            }
            require(stages.size() == 2, "rect-list requires vertex and fragment programs");
            rectIndex = results.size();
            results.push_back(std::move(rectangle.control));
            results.push_back(std::move(rectangle.evaluation));
            stages.insert(stages.begin() + 1, {{Stage::TessellationControl, &results[rectIndex], 0}, {Stage::TessellationEvaluation, &results[rectIndex + 1], 0}});
            rectListBuilt = true;
            phase(DrawRowRectList);
        };
        if (graphics.rectList) buildRectList();
        std::vector<Graphics::GuestMemorySnapshot> snapshots;
        const auto snapshot = [&] {
            snapshots.clear();
            for (const auto& region : memory) snapshots.push_back({region.guestAddress, region.bytes});
        };
        snapshot();
        timing.Mark("post_compile_prepare");
        const auto lockForDraw = [&] {
            if (gpuLock.owns_lock()) return;
            phase(DrawRowVectors);
            GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Draw);
            gpuLock.lock();
            timing.Mark("gpu_mutex_wait");
            phase(DrawRowLockWait);
            // Present replaces a windowless device by a windowed one (after WaitIdle) while draws
            // may be preparing unlocked: the draw then records on the current device, whose
            // recorder Recorder::Active() already is, rather than mixing the old device's images
            // and pipelines into the new recorder's batch. The preparation is device-independent
            // (SPIR-V for the same physical device; pipelines are created inside Draw).
            if (auto current = device.load(); current != nullptr && current != localDevice) {
                static std::atomic<std::uint64_t> replaced{0};
                std::fprintf(stderr, "[draw] device replaced during unlocked preparation (%llu)\n", static_cast<unsigned long long>(++replaced));
                localDevice = std::move(current);
            }
            // This queue's labels first (queue order), and their batch out before the draw.
            recordLabelsForPacket(localDevice.get(), submission.queue);
            phase(DrawRowLabels);
        };
        if (drawParameters.indirect && indirectCpu) {
            // The CPU path of an indirect draw: the records are read through the flush hook (after
            // this queue's queued labels went in, as for a DISPATCH_INDIRECT resolved here), the
            // CP's register writes are made in the user words of the programs whose banks the
            // packet names, those programs are recompiled on the patched words (a cache hit: only
            // their push constants change) and every record becomes a direct draw with the fold
            // rule, so the fetch and every read of a patched SGPR match the hardware; the id VGPR
            // after a folded add keeps the direct path's double add.
            const auto indirect = *drawParameters.indirect;
            if (drawHit) {
                // A hit's results are the variants' shared objects; this path rewrites slots, so
                // it works on copies of them (the reserve above holds them).
                for (std::size_t i = 0; i < programs.size(); ++i) {
                    if (programResults[i] == nullptr) continue;
                    resultIndex[i] = results.size();
                    results.push_back(ShaderRecompiler::RecompileResult(*programResults[i]));
                    for (auto& stage : stages) {
                        if (stage.program == programResults[i]) stage.program = &results[resultIndex[i]];
                    }
                    programResults[i] = &results[resultIndex[i]];
                }
            }
            recordQueuedLabelsBeforeRead(submission.queue);
            const auto readStart = std::chrono::steady_clock::now();
            const auto count = std::min(indirect.countIndirect ? Pm4::ReadDrawCount(indirect) : indirect.count, indirect.count);
            std::vector<Pm4::DrawArguments> records;
            for (std::uint32_t record = 0; record < count; ++record) records.push_back(Pm4::ReadDrawArguments(indirect, record));
            Graphics::CountIndirectDraw(*indirectCpu, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - readStart).count());
            const auto baseVertexWord = locate(indirect.baseVertexLocation);
            const auto startInstanceWord = locate(indirect.startInstanceLocation);
            const auto drawIndexWord = indirect.drawIndexEnabled ? locate(indirect.drawIndexLocation) : std::nullopt;
            for (std::uint32_t record = 0; record < records.size(); ++record) {
                const auto& arguments = records[record];
                if (traceIndirect) std::fprintf(stderr, "[draw]   record %u: count %u instances %u first %u vertexOffset %u startInstance %u\n", record, arguments.count, arguments.instances, arguments.firstVertexOrIndex, arguments.vertexOffset, arguments.firstInstance);
                if (arguments.count == 0 || arguments.instances == 0) continue;
                std::set<std::size_t> patched;
                const auto patch = [&](const std::optional<std::pair<std::size_t, std::size_t>>& word, std::uint32_t value) {
                    if (!word) return;
                    programs[word->first].userData[word->second] = value;
                    patched.insert(word->first);
                };
                patch(baseVertexWord, indirect.recordBytes == 20 ? arguments.vertexOffset : arguments.firstVertexOrIndex);
                patch(startInstanceWord, arguments.firstInstance);
                patch(drawIndexWord, record);
                Pm4::DrawParameters direct{0, arguments.count, 0, arguments.instances, drawParameters.flags, drawParameters.indexed, 0, 0};
                if (drawParameters.indexed) {
                    // The CP clamps the index range to INDEX_BUFFER_SIZE (the declared range).
                    if (arguments.firstVertexOrIndex >= drawParameters.indexCount) continue;
                    direct.indexAddress = drawParameters.indexAddress + static_cast<std::uint64_t>(arguments.firstVertexOrIndex) * drawParameters.indexSize;
                    direct.indexCount = std::min(arguments.count, drawParameters.indexCount - arguments.firstVertexOrIndex);
                    direct.indexSize = drawParameters.indexSize;
                } else {
                    direct.firstVertex = indirect.indxOffset;
                }
                for (const auto programIndex : patched) {
                    auto& result = results[resultIndex[programIndex]];
                    const auto pushBytes = result.pushConstants.size();
                    decodeVertexInfo(programIndex);
                    result = compile(programIndex, pushOffsets[programIndex]);
                    require(result.pushConstants.size() == pushBytes, "patched program changed its push constant layout");
                }
                fold(*programResults[0], direct);
                if (graphics.rectList && patched.contains(0)) buildRectList();
                snapshot();
                lockForDraw();
                noteDrawWriters(stages, submission.queue);
                phase(DrawRowVectors);
                localDevice->Draw(graphics, direct, stages, snapshots);
                phase(DrawRowGraphics);
            }
            timing.Mark("draw_and_resource_release");
            return drawn();
        }
        // The draw recipe (design_cpu_final M8, step 8b): the stage variants this draw records
        // with, by identity (a hit's matched ones, a miss's kept fresh ones), name the recipe a
        // later hit of the same set records from. Recipes need the register key (the entry then
        // fixes the state the recipe's pipeline and targets were built for); the CPU path of an
        // indirect draw never reaches here, and a GPU-side indirect draw takes the ordinary path.
        std::vector<std::shared_ptr<DispatchVariant>> recipeStages;
        if (registerKey && !drawParameters.indirect && Graphics::DrawRecipes()) {
            recipeStages.reserve(programs.size());
            for (std::size_t i = 0; i < programs.size(); ++i) recipeStages.push_back(drawHit ? matched[i] : fresh[i]);
            if (std::all_of(recipeStages.begin(), recipeStages.end(), [](const std::shared_ptr<DispatchVariant>& variant) { return variant == nullptr; })) recipeStages.clear();
        }
        std::shared_ptr<const DrawRecipe> recipe;
        if (drawHit && !recipeStages.empty()) {
            recipe = findDrawRecipe(drawKey, recipeStages);
            if (recipe == nullptr) VulkanDevice::NoteDrawRecipeMiss(VulkanDevice::DrawRecipePrecheck::NoRecipe);
        }
        lockForDraw();
        noteDrawWriters(stages, submission.queue);
        phase(DrawRowVectors);
        if (recipe != nullptr) {
            if (localDevice->DrawFromRecipe(graphics, drawParameters, stages, snapshots, recipe) == RecipeOutcome::Recorded) {
                phase(DrawRowGraphics);
                timing.Mark("draw_and_resource_release");
                return drawn();
            }
            // Nothing recorded: the ordinary path with the variant's stages, then the re-attach.
            VulkanDevice::NoteRecipe(VulkanDevice::RecipeEvent::Restart, VulkanDevice::RecipeKind::Draw);
        }
        std::shared_ptr<const DrawRecipe> built;
        localDevice->Draw(graphics, drawParameters, stages, snapshots, recipeStages.empty() ? nullptr : &built);
        phase(DrawRowGraphics);
        if (built != nullptr) attachDrawRecipe(drawKey, recipeStages, std::move(built));
        timing.Mark("draw_and_resource_release");
        return drawn();
    }

    // Serials complete out of order when a wait runs other queues' work; `completed` stays the
    // highest serial below which everything has finished. Caller holds `mutex`.
    void markCompleted(std::uint64_t serial) {
        completedOutOfOrder.insert(serial);
        while (!completedOutOfOrder.empty() && *completedOutOfOrder.begin() == completed + 1) {
            completed = *completedOutOfOrder.begin();
            completedOutOfOrder.erase(completedOutOfOrder.begin());
        }
    }

    // Debug aid: APS5_TRACE_LABEL=<hex address> logs every GPU write to or wait on memory within 256
    // bytes of that address.
    static void traceLabel(std::span<const std::uint32_t> packet, std::uint32_t queue) {
        static const std::uint64_t watched = [] {
            const char* value = std::getenv("APS5_TRACE_LABEL");
            return value ? std::strtoull(value, nullptr, 16) : 0ull;
        }();
        const auto opcode = (packet[0] >> 8u) & 0xffu;
        std::uint64_t target = 0;
        const char* kind = nullptr;
        if (opcode == 0x49 && packet.size() >= 7) { target = packet[3] | (static_cast<std::uint64_t>(packet[4]) << 32u); kind = "RELEASE_MEM"; }
        else if (opcode == 0x37 && packet.size() >= 5) { target = packet[2] | (static_cast<std::uint64_t>(packet[3]) << 32u); kind = "WRITE_DATA"; }
        else if ((opcode == 0x3c || opcode == 0x93) && packet.size() >= 7) { target = packet[2] | (static_cast<std::uint64_t>(packet[3]) << 32u); kind = "WAIT_REG_MEM"; }
        else if (opcode == 0x40 && packet.size() >= 6) { target = packet[4] | (static_cast<std::uint64_t>(packet[5]) << 32u); kind = "COPY_DATA"; }
        std::uint64_t length = 4;
        if (opcode == 0x50 && packet.size() >= 7) {
            target = packet[4] | (static_cast<std::uint64_t>(packet[5]) << 32u);
            length = packet[6] & 0x3ffffffu;
            kind = "DMA_DATA";
        }
        if (kind == nullptr) return;
        if (std::string_view(kind) != "WAIT_REG_MEM") {
            static std::mutex historyMutex;
            std::lock_guard lock(historyMutex);
            auto& entry = writeHistory()[writeCursor()++ % writeHistory().size()];
            entry = {target, length, queue, opcode};
        }
        if (watched == 0 || target + length + 0x100 < watched || target > watched + 0x100) return;
        std::fprintf(stderr, "[label] %lld ms queue 0x%x %s 0x%llx:", static_cast<long long>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()), queue, kind, static_cast<unsigned long long>(target));
        for (std::size_t i = 1; i < packet.size() && i < 9; ++i) std::fprintf(stderr, " %08x", packet[i]);
        std::fprintf(stderr, "\n");
    }

    struct WriteRecord {
        std::uint64_t target;
        std::uint64_t length;
        std::uint32_t queue;
        std::uint32_t opcode;
    };
    static std::array<WriteRecord, 16384>& writeHistory() {
        static std::array<WriteRecord, 16384> history{};
        return history;
    }
    static std::size_t& writeCursor() {
        static std::size_t cursor = 0;
        return cursor;
    }

    static int WaitTimeoutMs() {
        static const int value = [] { const char* text = std::getenv("APS5_GPU_WAIT_TIMEOUT_MS"); return text ? std::atoi(text) : 1000; }();
        return value;
    }

    // The WAIT_REG_MEM poll loop's service calls, per queue worker (the [poll] line printed with its
    // [packets] line, APS5_PROFILE_DRAW): service calls, waits ended from the label table, mutex
    // tries and failed tries, submits and reaps made for the producer (with the reaps that retired
    // a batch), the table's refusals by reason (Recorder::LabelRefusal), and the try holds.
    struct PollStats {
        std::uint64_t ticks = 0, tableHits = 0, tries = 0, triesFailed = 0, submits = 0, reaps = 0, reapsWithWork = 0;
        // Reaps made for a label refused as BehindCompletion (the one case a poll cannot converge
        // without one) and the rest (queue 0's poller standing in for its boundary reap, a compute
        // poller while queue 0 is dormant, every poller with APS5_POLL_REAP=1).
        std::uint64_t reapsBehindCompletion = 0, reapsOther = 0;
        std::array<std::uint64_t, 6> refusals{};
        double longestTryHoldUs = 0;
        double tryHoldUs = 0;
    };
    static PollStats& pollStats() {
        static thread_local PollStats stats;
        return stats;
    }
    static constexpr const char* LabelRefusalNames[6] = {"none", "trust-off", "queued", "overwritten", "unclosed", "behind-completion"};
    // A compute poller reaps only for a label refused as BehindCompletion (design_cpu_final M5
    // P1) or while queue 0 is dormant; queue 0's poller reaps whenever completions are pending
    // (its boundary reap cannot run inside a wait). APS5_POLL_REAP=1 makes every poller reap
    // whenever completions are pending, as before.
    static bool PollReapAll() {
        static const bool all = std::getenv("APS5_POLL_REAP") != nullptr;
        return all;
    }
    // A poller tries the GPU mutex only when the recorder's write generation moved, or at most once
    // per PollTryInterval when only a label deadline or a reap is due (M5 P2); APS5_POLL_TRY_EACH=1
    // tries at every service call, as before.
    static bool PollTryEach() {
        static const bool each = std::getenv("APS5_POLL_TRY_EACH") != nullptr;
        return each;
    }
    static constexpr std::chrono::microseconds PollTryInterval{1000};
    // flushBetweenPackets skips the label record-try when the next packet records the group at its
    // own lock (M5 L1); APS5_LABEL_TRY_EACH_PACKET=1 tries before every packet, as before.
    static bool LabelTryEachPacket() {
        static const bool each = std::getenv("APS5_LABEL_TRY_EACH_PACKET") != nullptr;
        return each;
    }

    // How WAIT_REG_MEM packets were satisfied, per queue worker (reported in its [packets] line):
    // already true when reached; from the recorder's pending-label table (and how many of those by
    // a label of this same queue); by polling memory (the GPU, another queue or the CPU wrote it);
    // timed out. Plus the submits and reaps the poll loop made on behalf of the producer.
    // fromRecorderUnlocked: table hits (at entry or polling) that took no GPU mutex; entriesUnlocked:
    // waits that went to polling without the GPU mutex (nothing recorded writes the range);
    // entryTriesFailed: overlapping entries whose try of the mutex failed and went to polling
    // instead of waiting for it (see waitMemory). fromRecorderLate: table hits taken under the late
    // rule (Recorder::NoteLabel); lateRefusedCpuStore: late hits refused because a CPU store
    // touched the dwords since the label's group closed.
    struct WaitOutcomes {
        std::uint64_t atEntry = 0, fromRecorder = 0, fromRecorderSameQueue = 0, fromRecorderPolling = 0, polled = 0, timedOut = 0, pollSubmits = 0, pollReaps = 0;
        std::uint64_t fromRecorderUnlocked = 0, entriesUnlocked = 0, entryTriesFailed = 0, fromRecorderLate = 0, lateRefusedCpuStore = 0;
    };
    static WaitOutcomes& waitOutcomes() {
        static thread_local WaitOutcomes outcomes;
        return outcomes;
    }
    // The late counters of Recorder::LateCounts (cumulative, per thread) at this worker's last
    // [packets] line, for the deltas it prints.
    static Graphics::Recorder::LateStatistics& lateCountsSeen() {
        static thread_local Graphics::Recorder::LateStatistics seen{};
        return seen;
    }
    static bool TraceLateLabels() {
        static const bool trace = std::getenv("APS5_TRACE_LATE_LABELS") != nullptr;
        return trace;
    }

    // Collect-epoch bumps by ordering point, per queue worker (reported in its [packets] line, see
    // GuestMemory::BumpCollectEpoch): the start of a submission, a wait the CPU may have satisfied,
    // a drain, a reap outside a submission, and (APS5_PACKET_EPOCH=1 only) every packet.
    struct EpochBumps {
        std::uint64_t submissions = 0, waits = 0, drains = 0, reaps = 0, packets = 0;
    };
    static EpochBumps& epochBumps() {
        static thread_local EpochBumps bumps;
        return bumps;
    }
    static void bumpEpoch(std::uint64_t EpochBumps::*counter) {
        GuestMemory::BumpCollectEpoch();
        ++(epochBumps().*counter);
    }
    static bool PacketEpoch() {
        static const bool packet = std::getenv("APS5_PACKET_EPOCH") != nullptr;
        return packet;
    }

    // Labels recorded on the GPU are not submitted one by one (see VulkanDevice::WriteLabelOnGpu):
    // the open batch goes out at the latest APS5_LABEL_FLUSH_US (default 1000; 250 with
    // APS5_NO_LABEL_BATCH_SUBMIT=1, where it also goes out at the next non-label packet of the
    // recording worker) after its first label, checked by every worker between packets and by
    // waiting workers inside their poll loop, so a game thread polling the label is never left
    // waiting on an unsubmitted batch for long. The deadline is read lock-free.
    static std::chrono::microseconds LabelFlushDeadline() {
        static const std::chrono::microseconds value = [] {
            const char* text = std::getenv("APS5_LABEL_FLUSH_US");
            return std::chrono::microseconds(text ? std::atoi(text) : (LabelBatchSubmit() ? 1000 : 250));
        }();
        return value;
    }
    // Asked by the poll loop of a WAIT_REG_MEM (Driver::waitMemory): whether the open batch's first
    // label passed its deadline, so the poller submits it. With the queued-label table it records
    // this worker's queued labels first whenever the mutex is free: a wait of this queue on another
    // queue's label can last milliseconds, and a label this worker queued before it (which only
    // this queue's waits can take from the table) must reach its other consumers meanwhile, after
    // which the deadline submits it like any recorded label.
    static bool LabelFlushDue() {
        if (QueuedLabelTable() && !deferredLabels().labels.empty()) Get().recordQueuedLabelsByTry(GuestMemory::GpuLockThreadTag(), pollLabelRecords);
        const auto since = Graphics::Recorder::PendingLabelSince();
        return since.has_value() && std::chrono::steady_clock::now() - *since >= LabelFlushDeadline();
    }
    // Whether some unfinished batch holds a completion the CPU must reap for: a label store or a
    // write-back (the gate of every reap made outside a packet's own hold).
    static bool CompletionsPending() {
        return Graphics::Recorder::PendingCompletionLabels() != 0 || Graphics::Recorder::PendingWriteBackCompletions() != 0;
    }
    // Boundary reaps (flushBetweenPackets) are tried at most this often per worker; a reap only
    // bounds how late a completion lands for a CPU poller (reads sync through the hook anyway).
    static constexpr std::chrono::microseconds ReapInterval{250};
    static bool ReapEachPacket() {
        // Debug aid: APS5_REAP_EACH_PACKET=1 tries a boundary reap at every packet of every queue, as before.
        static const bool each = std::getenv("APS5_REAP_EACH_PACKET") != nullptr;
        return each;
    }
    // Submits the open batch after this many dispatches/draws even without a label, so the GPU
    // starts on them while the CPU records the rest (APS5_BATCH_CAP, default 64, the previous 16
    // measured no better on the GPU and worse on the presenter's tail; 0 disables).
    static std::uint64_t BatchCap() {
        static const std::uint64_t value = [] {
            const char* text = std::getenv("APS5_BATCH_CAP");
            return text ? std::strtoull(text, nullptr, 10) : 64ull;
        }();
        return value;
    }

    // Records this worker's deferred labels (see DeferredLabels) under GuestMemory::GpuMutex, in
    // queue order, each as VulkanDevice::WriteLabelOnGpu would have at its packet: on the GPU, as a
    // completion action, or by a CPU store when nothing is recorded (the store is ordered already).
    // The stamp is taken here, right before the label is noted, so it orders after every submission
    // the game made before this point (a wait received later never trusts the label). Only an
    // unusable recorder (reason 4) can still ask for a drain here (APS5_DRAIN_COMPLETION_LABELS=1
    // disables deferral, see DeferLabels); it waits idle under the mutex, which is cheap then.
    // A throw (lost device) leaves nothing queued: the worker fails with the packet, or, when the
    // record ran inside a tolerated dispatch or draw, that packet is skipped; the dropped group is
    // reported either way, so a WAIT_REG_MEM timeout later is not the first sign of it.
    void recordDeferredLabels(VulkanDevice* localDevice, std::uint32_t queue) {
        auto& deferred = deferredLabels();
        if (deferred.labels.empty()) return;
        // The group leaves the queue before it is recorded: a completion store inside the record
        // (queue 0's reap before the first label) can reach the flush hook, whose queued-label
        // callback (recordQueuedLabelsFromHook) must then find nothing to record a second time.
        static thread_local std::vector<DeferredLabel> labels;
        labels.clear();
        labels.swap(deferred.labels);
        struct Clear {
            std::vector<DeferredLabel>& labels;
            std::uint32_t queue;
            ~Clear() {
                const bool failed = std::uncaught_exceptions() != 0;
                if (failed) std::fprintf(stderr, "[gpu] queue 0x%x dropped %zu queued labels: their record failed\n", queue, labels.size());
                labels.clear();
                // The group's table entries close with the tracker generation after the group's
                // last MarkWritten (a failed record leaves them unclosed, never late-trusted).
                Graphics::Recorder::CloseLabelGroup(failed ? 0 : GuestMemory::TrackerGeneration());
                // The recorded labels replaced their queued table entries; the rest (stored by
                // the CPU, or dropped) leave the table with the per-thread ranges.
                Graphics::Recorder::ForgetQueuedLabels();
                queuedLabelsNoted() = 0;
            }
        } clear{labels, queue};
        bool first = true;
        for (const auto& label : labels) {
            const auto bytes = std::span<const std::byte>(label.bytes).first(label.size);
            const int reason = localDevice != nullptr ? localDevice->WriteLabelOnGpu(label.address, bytes, ++eventSerial, queue, first) : 4;
            first = false;
            countLabelOutcome(reason);
            if (reason == 0 || reason == 5 || reason == 6) continue;
            if (reason != 1 && localDevice != nullptr) localDevice->WaitIdle();
            GuestMemory::Write(label.address, bytes, 4);
        }
        ++labelGroups;
        static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        // The [sync] line every 10 s (checked every 64 groups; drains alone are too rare now).
        if (profile && (labelGroups.load(std::memory_order_relaxed) & 63u) == 0 && std::chrono::steady_clock::now() - lastSyncReport > std::chrono::seconds(10)) {
            lastSyncReport = std::chrono::steady_clock::now();
            reportSync();
        }
    }

    // The first thing a packet does under its own GpuMutex hold (dispatch, draw, flip: see
    // PacketLocksItself), before its work is recorded: this worker's deferred labels go in, in
    // queue order ahead of that work, and the open batch is submitted when it carries labels,
    // exactly what flushBetweenPackets did under an acquisition of its own before the packet
    // (APS5_LABEL_OWN_LOCK=1 restores that). One acquisition instead of two: the label one was
    // queue 0's first after a run of unlocked packets and absorbed the compute queues' whole
    // critical sections ([lock] site 'label'), after which the packet's own came uncontended.
    // Returns whether the batch was submitted (the flip then needs no submit of its own).
    bool recordLabelsForPacket(VulkanDevice* localDevice, std::uint32_t queue) {
        static const bool packetFlush = !LabelBatchSubmit() && std::getenv("APS5_NO_LABEL_PACKET_FLUSH") == nullptr;
        // APS5_PROFILE_DRAW: what this costs inside the packet's own hold ([labels] line): the
        // record (with queue 0's reap of finished batches before the first label) and the submit
        // (with another reap on queue 0), both of which run completions, so a dispatch's or
        // indirect dispatch's hold ([lock] line) is partly this.
        static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        const auto start = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        if (!deferredLabels().labels.empty()) {
            recordDeferredLabels(localDevice, queue);
            ++packetLockRecords;
            if (profile) packetLockRecordUs += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count());
        }
        if (localDevice == nullptr || !packetFlush || !Graphics::Recorder::PendingLabelSince().has_value()) return false;
        // As in flushBetweenPackets: a compute worker submits without reaping.
        const auto submitStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        localDevice->SubmitRecorded(queue == 0);
        ++packetLockSubmits;
        if (profile) packetLockSubmitUs += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - submitStart).count());
        return true;
    }

    // Before a packet's CPU read of guest memory outside any lock (the group counts of a
    // DISPATCH_INDIRECT resolved on the CPU): a label this queue still has queued may write those
    // bytes and the flush hook only knows recorded stores, so the queued labels are recorded first.
    // Rare (a label is normally followed by a wait or a dispatch that records it) and cheap.
    void recordQueuedLabelsBeforeRead(std::uint32_t queue) {
        if (deferredLabels().labels.empty()) return;
        GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Label);
        std::lock_guard gpuLock(GuestMemory::GpuMutex());
        const auto localDevice = device.load();
        recordDeferredLabels(localDevice.get(), queue);
    }

    // After a self-locking packet's unlocked capture (dispatch, draw), when this worker still has
    // labels queued (the try in flushBetweenPackets failed): a queued label whose bytes lie in a
    // captured region (a fence value in SRT-chased data, a patched constant) was read before its
    // write, and the hook could not sync on it, so the labels are recorded outright now and the
    // caller redoes the packet from the top (returns true; once, the queue is empty after). With no
    // overlap the labels are recorded if the mutex happens to be free, so a poller of the label
    // does not wait for the rest of this prologue (stage A takes milliseconds for a BDA build);
    // otherwise the packet's own acquisition records them as planned. The check itself is a few
    // dozen compares: a group holds a handful of labels, a capture a few dozen regions.
    bool recordQueuedLabelsAfterCapture(std::uint32_t queue, std::span<const ShaderRecompiler::MemoryRegion> regions) {
        const auto& labels = deferredLabels().labels;
        if (labels.empty()) return false;
        for (const auto& label : labels) {
            for (const auto& region : regions) {
                if (label.address < region.guestAddress + region.bytes.size() && region.guestAddress < label.address + label.size) {
                    recordQueuedLabelsBeforeRead(queue);
                    ++captureRetries;
                    return true;
                }
            }
        }
        GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Label);
        std::unique_lock gpuLock(GuestMemory::GpuMutex(), std::defer_lock);
        if (!gpuLock.try_lock()) {
            ++triesFailed[TryCapture];
            return false;
        }
        const auto localDevice = device.load();
        recordDeferredLabels(localDevice.get(), queue);
        ++captureTryRecords;
        return false;
    }

    // Enters this worker's queued labels that are not in the label table yet (the group's tail
    // since the last call) as queued entries, see QueuedLabelTable. The stamp taken now is older
    // than the one the record takes, so a wait the queued entry does not satisfy is satisfied by
    // the recorded one at the latest, never the reverse.
    void noteQueuedLabels(std::uint32_t queue) {
        static const bool installed = (Graphics::Recorder::SetQueuedLabelRecorder(&Driver::recordQueuedLabelsFromHook), true);
        (void)installed;
        const auto& labels = deferredLabels().labels;
        for (auto& noted = queuedLabelsNoted(); noted < labels.size(); ++noted) {
            const auto& label = labels[noted];
            Graphics::Recorder::NoteQueuedLabel(label.address, std::span<const std::byte>(label.bytes).first(label.size), ++eventSerial, queue);
        }
    }

    // Records this worker's queued labels if the mutex is free (a try): from the poll loop of a
    // wait (LabelFlushDue), so the group reaches its other consumers while this queue waits.
    void recordQueuedLabelsByTry(std::uint32_t queue, std::atomic<std::uint64_t>& counter) {
        GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Label);
        std::unique_lock gpuLock(GuestMemory::GpuMutex(), std::defer_lock);
        if (!gpuLock.try_lock()) {
            ++triesFailed[TryPollRecord];
            return;
        }
        const auto localDevice = device.load();
        recordDeferredLabels(localDevice.get(), queue);
        ++counter;
    }

    // The flush hook's callback (Recorder::SetQueuedLabelRecorder): a CPU access by this worker
    // overlaps a label it queued, which precedes the access in queue order, so the group is
    // recorded outright now; the hook then syncs on the recorded store like on any other.
    static void recordQueuedLabelsFromHook() {
        if (deferredLabels().labels.empty()) return;
        auto& driver = Get();
        GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Label);
        std::lock_guard gpuLock(GuestMemory::GpuMutex());
        const auto localDevice = driver.device.load();
        driver.recordDeferredLabels(localDevice.get(), GuestMemory::GpuLockThreadTag());
        ++hookLabelRecords;
    }

    // Between packets: records this worker's deferred labels before a packet that needs them (or
    // once their deadline passed), submits the open batch once a label's deadline passed (at once
    // on a non-label packet with APS5_NO_LABEL_BATCH_SUBMIT=1, see LabelBatchSubmit) or the batch
    // holds enough work, and reaps finished batches whose completion actions still hold labels.
    // The mutex is taken outright when the packet needs the labels (without the queued-label
    // table, see QueuedLabelTable) or a deadline is well past (four times the deadline, twice the
    // cap: a try that fails is retried at the next packet within microseconds, while a wait behind
    // a compute queue's hold costs queue 0 milliseconds); otherwise it is only tried (a
    // register-only packet must not queue behind a 2-4 ms dispatch for this) and the next packet
    // retries. A packet that takes the mutex itself (PacketLocksItself) gets the record (and the
    // submit) at its own acquisition (recordLabelsForPacket), so for it the mutex is only tried
    // here as well: a try that succeeds records at once (the packet's unlocked prologue then reads
    // through a hook that knows the labels, as before), one that fails leaves the group to the
    // packet. APS5_NO_LABEL_PACKET_FLUSH=1 leaves recorded labels pending until the deadline with
    // the old submit policy; APS5_LABEL_OWN_LOCK=1 takes the mutex outright before every packet
    // that needs the labels, as before.
    void flushBetweenPackets(std::uint32_t queue, std::uint32_t header, bool labelPacket) {
        static const bool packetFlush = !LabelBatchSubmit() && std::getenv("APS5_NO_LABEL_PACKET_FLUSH") == nullptr;
        static const bool ownLock = std::getenv("APS5_LABEL_OWN_LOCK") != nullptr;
        static std::atomic<std::uint64_t> packetFlushes{0}, deadlineFlushes{0}, capFlushes{0}, boundaryReaps{0}, overdueLocks{0};
        auto& deferred = deferredLabels();
        const bool queued = !deferred.labels.empty();
        const bool queuedTable = queued && QueuedLabelTable();
        if (queuedTable) noteQueuedLabels(queue);
        const bool selfLocking = !ownLock && PacketLocksItself(header);
        bool submit = false, record = false, needsRecord = false, deferredDue = false, pendingDue = false;
        const auto pending = Graphics::Recorder::PendingLabelSince();
        // Boundary reaps: queue 0 only (a compute worker must not run completions against queue
        // 0; it reaps in its poll loops and the idle wake), at most once per ReapInterval whether
        // the try succeeds or not (design13 step 6: the try at every packet was the mutex's main
        // contender). APS5_REAP_EACH_PACKET=1 restores the try at every packet on every queue.
        thread_local std::chrono::steady_clock::time_point lastReapTry{};
        const bool completions = CompletionsPending();
        const bool reapDue = completions && (ReapEachPacket() || queue == 0);
        // The clock is read only while some label or reap waits (every packet passes here).
        const auto now = queued || pending.has_value() || reapDue ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        // A self-locking packet records the group at its own acquisition within one packet, before
        // its deadline can matter, so a queued group alone is no reason to try the mutex before it;
        // a try made for another reason (submit, cap, reap) still records it.
        bool recordAtLock = false;
        if (queued) {
            if (!labelPacket) {
                record = true;
                // With the queued-label table the packet's reads and waits are covered without the
                // record (the hook and the table), so the mutex is only tried, as for a self-locking packet.
                needsRecord = !queuedTable && NeedsRecordedLabels(header) && !selfLocking;
            }
            if (now - deferred.since >= LabelFlushDeadline()) record = deferredDue = true;
            if (record && !deferredDue && selfLocking && !LabelTryEachPacket()) {
                record = false;
                recordAtLock = true;
            }
        }
        if (pending.has_value()) {
            if (!labelPacket && packetFlush) submit = true;
            if (now - *pending >= LabelFlushDeadline()) submit = pendingDue = true;
        }
        const auto work = Graphics::Recorder::RecordedWorkSinceSubmit();
        const bool capped = !submit && !record && BatchCap() != 0 && work >= BatchCap();
        const bool reap = !submit && !record && !capped && reapDue && (ReapEachPacket() || now - lastReapTry >= ReapInterval);
        if (!submit && !record && !capped && !reap) {
            if (recordAtLock) ++recordTriesSkipped;
            return;
        }
        if (reap) lastReapTry = now;
        std::unique_lock gpuLock(GuestMemory::GpuMutex(), std::defer_lock);
        // The site names the hold too (a try's hold is charged to it; a reap alone shows as 'try').
        if (record) GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Label);
        else if (submit || capped) GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Flush);
        // A reap alone is never worth waiting for the mutex (the next packet retries); neither is a
        // submit the self-locking packet makes itself, unless the label's deadline passed.
        bool outright = needsRecord;
        if (LabelBatchSubmit()) {
            const auto grace = 4 * LabelFlushDeadline();
            const bool overdue = (deferredDue && now - deferred.since >= grace) || (pendingDue && now - *pending >= grace) || (capped && work >= 2 * BatchCap());
            if (overdue) ++overdueLocks;
            outright = outright || overdue;
        } else if (deferredDue || (queue == 0 && (capped || (submit && (!selfLocking || pendingDue))))) {
            outright = true;
        }
        if (outright) {
            gpuLock.lock();
        } else if (!gpuLock.try_lock()) {
            ++triesFailed[record ? TryLabel : submit || capped ? TryFlush : TryReap];
            if (selfLocking && record) ++packetLockDeferred;
            else if (selfLocking && submit) ++packetSubmitDeferred;
            return;
        }
        const auto localDevice = device.load();
        if (record || recordAtLock) {
            recordDeferredLabels(localDevice.get(), queue);
            // The group goes out with the packet flush (or at once past the deadline), as a label
            // recorded at its own packet would have.
            if ((!labelPacket && packetFlush) || deferredDue) submit = true;
        }
        if (localDevice == nullptr) return;
        if (submit || capped) {
            // Rechecked under the lock: another worker may have submitted meanwhile. A compute
            // worker only submits (no reap: a write-back's sync would hold the mutex against queue 0).
            if (!Graphics::Recorder::PendingLabelSince().has_value() && (BatchCap() == 0 || Graphics::Recorder::RecordedWorkSinceSubmit() < BatchCap())) return;
            localDevice->SubmitRecorded(queue == 0);
            if (capped) ++capFlushes;
            else if (!labelPacket && packetFlush) ++packetFlushes;
            else ++deadlineFlushes;
        } else if (reap) {
            localDevice->ReapRecorded();
            ++boundaryReaps;
        }
        static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        // Every 10 s (under the mutex still, so the clock is a plain static), cumulative counts.
        static std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
        if (!profile) return;
        const auto reportNow = std::chrono::steady_clock::now();
        if (reportNow - lastReport < std::chrono::seconds(10)) return;
        lastReport = reportNow;
        const auto stores = Graphics::Recorder::StoreCounts();
        const auto failed = [](TrySite site) { return static_cast<unsigned long long>(triesFailed[site].load()); };
        std::fprintf(stderr, "[labels] batch flushes between packets: %llu at the next packet, %llu by deadline, %llu by size cap (%llu well overdue, waited for the mutex); %llu boundary reaps; record tries skipped (self-locking next) %llu; failed tries: label %llu, flush %llu, reap %llu, poll %llu, idle %llu, capture %llu, poll-record %llu; at the packet's own lock: %llu label groups (%.0f ms), %llu submits (%.0f ms incl. queue 0's reaps), left to it by a failed try: %llu groups, %llu submits; after a capture: %llu groups recorded by a try, %llu packets redone for a queued label over the capture; %llu suspend points; stores: %llu in %llu runs (%llu joined, %llu replaced, %llu WAW barriers, %llu joins refused; %llu runs recorded at submit, %llu in place before a writer or reader); DCC key stores: %llu queued in %llu runs (%llu before a writer, %llu joined a queued range); queued labels: %llu noted (%llu over a recorded entry), %llu same-queue wait hits, %llu groups recorded from a poll loop, %llu from the flush hook (%llu hook accesses, %llu inside a completion skipped)\n", static_cast<unsigned long long>(packetFlushes.load()), static_cast<unsigned long long>(deadlineFlushes.load()), static_cast<unsigned long long>(capFlushes.load()), static_cast<unsigned long long>(overdueLocks.load()), static_cast<unsigned long long>(boundaryReaps.load()), static_cast<unsigned long long>(recordTriesSkipped.load()), failed(TryLabel), failed(TryFlush), failed(TryReap), failed(TryPoll), failed(TryIdle), failed(TryCapture), failed(TryPollRecord), static_cast<unsigned long long>(packetLockRecords.load()), packetLockRecordUs.load() / 1000.0, static_cast<unsigned long long>(packetLockSubmits.load()), packetLockSubmitUs.load() / 1000.0, static_cast<unsigned long long>(packetLockDeferred.load()), static_cast<unsigned long long>(packetSubmitDeferred.load()), static_cast<unsigned long long>(captureTryRecords.load()), static_cast<unsigned long long>(captureRetries.load()), static_cast<unsigned long long>(suspendPoints.load()), static_cast<unsigned long long>(stores.stores), static_cast<unsigned long long>(stores.runs), static_cast<unsigned long long>(stores.joined), static_cast<unsigned long long>(stores.replaced), static_cast<unsigned long long>(stores.wawBarriers), static_cast<unsigned long long>(stores.joinsRefused), static_cast<unsigned long long>(stores.runsAtSubmit), static_cast<unsigned long long>(stores.runsForced), static_cast<unsigned long long>(stores.keyStores), static_cast<unsigned long long>(stores.keyStoreRuns), static_cast<unsigned long long>(stores.keyStoreRunsForWriter), static_cast<unsigned long long>(stores.keyStoresJoined), static_cast<unsigned long long>(stores.queuedNoted), static_cast<unsigned long long>(stores.queuedOverRecorded), static_cast<unsigned long long>(stores.queuedHits), static_cast<unsigned long long>(pollLabelRecords.load()), static_cast<unsigned long long>(hookLabelRecords.load()), static_cast<unsigned long long>(stores.queuedHookRecords), static_cast<unsigned long long>(stores.queuedHookInCompletion));
    }

    // WAIT_REG_MEM: another queue or the CPU produces the value; the other queues run on their own threads.
    void waitMemory(std::span<const std::uint32_t> packet, std::uint32_t queue, const PacketHistory& context, std::uint64_t received) {
        // The timeout counts from the last sign of GPU progress: while any queue is executing packets,
        // the producer may still be on its way (shader compiles alone take hundreds of milliseconds).
        auto start = std::chrono::steady_clock::now();
        auto lastDone = packetsDone.load();
        // Names this worker's thread in the [lock] GpuMutex wait report.
        GuestMemory::TagGpuLockThread(queue);
        auto& outcomes = waitOutcomes();
        const bool wide = ((packet[0] >> 8u) & 0xffu) == 0x93u;
        const std::uint64_t awaited = packet[2] | (static_cast<std::uint64_t>(packet[3]) << 32u);
        const std::size_t awaitedBytes = wide ? 8 : 4;
        static const bool traceGpu = std::getenv("APS5_TRACE_GPU") != nullptr;
        if (traceGpu) std::fprintf(stderr, "[gpu] %.1f queue 0x%x waits 0x%llx == 0x%x (now 0x%x)\n", TraceMs(), queue, static_cast<unsigned long long>(packet[2] | (static_cast<std::uint64_t>(packet[3]) << 32u)), packet[4],
                                   *reinterpret_cast<const volatile std::uint32_t*>(packet[2] | (static_cast<std::uint64_t>(packet[3]) << 32u)));
        struct WaitTrace {
            bool enabled; std::uint32_t queue; std::uint64_t address; std::chrono::steady_clock::time_point begin;
            ~WaitTrace() {
                if (!enabled) return;
                const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count();
                if (ms >= 0.5) std::fprintf(stderr, "[gpu] %.1f queue 0x%x wait on 0x%llx done after %.1f ms\n", TraceMs(), queue, static_cast<unsigned long long>(address), ms);
            }
        } waitTrace{traceGpu, queue, packet[2] | (static_cast<std::uint64_t>(packet[3]) << 32u), std::chrono::steady_clock::now()};
        bool warned = false;
        // The collect epoch advances once the wait is over (at entry, polled, timed out, or from a
        // label another queue recorded: the CPU's writes before that queue's submission are ordered
        // through it), unless a label this queue itself recorded satisfied it, which orders no CPU
        // write. After the wait, never before: completions run inside pollService make memo entries
        // during the wait that may predate the CPU's store of the flag.
        struct EpochPoint {
            bool bump = true;
            ~EpochPoint() {
                if (bump) bumpEpoch(&EpochBumps::waits);
            }
        } epochPoint;
        GuestMemory::CheckRange(reinterpret_cast<const void*>(awaited), awaitedBytes, awaitedBytes);
        if (Pm4::WaitSatisfiedUnchecked(packet)) {
            ++outcomes.atEntry;
            return;
        }
        // Debug aids: APS5_NO_LABEL_SHORTCUT=1 never satisfies a wait from the pending-label table;
        // APS5_NO_WAIT_OVERLAP_SUBMIT=1 submits the open batch at every wait, as before, instead of
        // only when it writes the awaited range.
        static const bool labelShortcut = std::getenv("APS5_NO_LABEL_SHORTCUT") == nullptr;
        static const bool overlapSubmit = std::getenv("APS5_NO_WAIT_OVERLAP_SUBMIT") == nullptr;
        // APS5_WAIT_LOCK=1 takes the GPU mutex at every wait's entry and poll check, as before.
        static const bool waitLock = std::getenv("APS5_WAIT_LOCK") != nullptr;
        // The wait side of the late rule (Recorder::NoteLabel), shared by the entry lookup and the
        // poll loop's: a hit must satisfy the packet's compare; a late hit is taken only when no
        // CPU store touched the awaited dwords since its group closed (an uncached resetting
        // collect of the page, then the block stamps: the tracker mutex alone, after the table
        // mutex was released), and a refusal is remembered per generation so the poll loop does
        // not repeat the syscall every 200 us. A late hit's submission orders the game's CPU
        // stores before it, so the epoch bump stays even for a label of this same queue.
        // APS5_TRACE_LATE_LABELS=1 prints the first 200 late decisions.
        std::uint64_t refusedGeneration = 0;
        // The last table lookup's verdict: the awaited label is stored by a completion action of
        // its batch, so this poller must reap for it (the one reap a compute poll loop makes
        // while queue 0 can reap: M5 P1).
        bool behindCompletion = false;
        const auto traceLate = [&](const char* outcome, const std::optional<Graphics::Recorder::LabelHit>& hit, Graphics::Recorder::LabelRefusal refusal) {
            static std::atomic<int> shown{0};
            if (shown.fetch_add(1) >= 200) return;
            std::fprintf(stderr, "[late] queue 0x%x wait on 0x%llx (function %u ref 0x%x mask 0x%x) received %llu: %s; entry value 0x%llx stamp %llu generation %llu, refusal %d\n", queue, static_cast<unsigned long long>(awaited), packet[1] & 7u, packet[4], packet[5], static_cast<unsigned long long>(received), outcome,
                         static_cast<unsigned long long>(hit ? hit->value : 0), static_cast<unsigned long long>(hit ? hit->stamp : 0), static_cast<unsigned long long>(hit ? hit->generation : 0), static_cast<int>(refusal));
        };
        const auto takeLabel = [&](const std::optional<Graphics::Recorder::LabelHit>& hit, Graphics::Recorder::LabelRefusal refusal, bool polling, bool unlockedHit) {
            behindCompletion = refusal == Graphics::Recorder::LabelRefusal::BehindCompletion;
            const bool trace = TraceLateLabels() && (refusal != Graphics::Recorder::LabelRefusal::None || (hit.has_value() && hit->late));
            if (!hit.has_value() || !Pm4::WaitComparesValue(packet, hit->value)) {
                if (trace) traceLate(hit.has_value() ? "value mismatch" : "refused by the table", hit, refusal);
                return false;
            }
            if (hit->late) {
                if (hit->generation == refusedGeneration) return false;
                if (!GuestMemory::UnchangedSinceCollected(awaited, awaitedBytes, hit->generation)) {
                    ++outcomes.lateRefusedCpuStore;
                    refusedGeneration = hit->generation;
                    if (trace) traceLate("refused, CPU store since the group closed", hit, refusal);
                    return false;
                }
                ++outcomes.fromRecorderLate;
                if (trace) traceLate("trusted", hit, refusal);
            }
            ++(polling ? outcomes.fromRecorderPolling : outcomes.fromRecorder);
            if (unlockedHit) ++outcomes.fromRecorderUnlocked;
            if (hit->queue == queue) {
                ++outcomes.fromRecorderSameQueue;
                if (!hit->late) epochPoint.bump = false;
            }
            return true;
        };
        // Read before the checks below: a write noted before the (locked or snapshot) check is seen
        // by that check, one noted after it changes the generation the poll loop compares against.
        std::uint64_t seenGeneration = Graphics::Recorder::WriteGeneration();
        // Without the GPU mutex first: the label table has its own small mutex, and the lock-free
        // pending-write snapshot says whether any recorded work writes the range at all. Only an
        // overlap needs the mutex (to submit the open batch); a miss goes straight to polling (its
        // producer is the CPU, an in-flight batch, or a queue that has not recorded the label yet:
        // the poll loop watches the write generation for that). The snapshot may lag the generation
        // (Recorder::noteWrite bumps it before publishing, and NoteLabel follows the note), so a
        // decision made from the snapshot never consumes a generation: an unlocked entry makes the
        // poll loop's first check a locked (try_lock) one, which sees the producer's consistent view.
        const bool unlocked = !waitLock && overlapSubmit;
        if (unlocked) {
            if (labelShortcut) {
                Graphics::Recorder::LabelRefusal refusal{};
                if (takeLabel(Graphics::Recorder::LookupLabel(awaited, awaitedBytes, received, &refusal), refusal, false, true)) return;
            }
        }
        const bool lockedEntry = !unlocked || Graphics::Recorder::SnapshotWriteOverlaps(awaited, awaitedBytes);
        if (!lockedEntry) ++outcomes.entriesUnlocked;
        // An overlapping entry has one thing to do under the mutex: submit the open batch when it
        // writes the range (the table lookup already ran without it). It only TRIES the mutex: the
        // wait's hold is microseconds, but waiting for the mutex behind a compute queue's build cost
        // queue 0 ~4 ms per entry ([lock] 'wait' site). A failed try goes to the poll loop, whose
        // first service call repeats the entry's checks under a try of its own (`recheck`) and every
        // later one re-tries as well, so the submit happens as soon as the mutex is free, while the
        // value may arrive meanwhile (an in-flight batch, another queue, the CPU) and end the wait
        // without any hold. Debug aid: APS5_WAIT_LOCK_ENTRY=1 waits for the mutex at entry as before.
        static const bool blockingEntry = std::getenv("APS5_WAIT_LOCK_ENTRY") != nullptr;
        bool recheck = !lockedEntry;
        bool entryTryFailed = false;
        if (lockedEntry) {
            GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Wait);
            std::unique_lock gpuLock(GuestMemory::GpuMutex(), std::defer_lock);
            if (blockingEntry || !unlocked) {
                gpuLock.lock();
            } else if (!gpuLock.try_lock()) {
                ++outcomes.entryTriesFailed;
                recheck = true;
                entryTryFailed = true;
            }
            if (const auto localDevice = gpuLock.owns_lock() ? device.load() : std::shared_ptr<VulkanDevice>{}) {
                // A label the recorder holds for this range, recorded after the game submitted this
                // packet, will store the value in order (see Recorder::NoteLabel): the wait is over
                // without the GPU round trip. The label's batch is submitted by the flush rules.
                if (labelShortcut) {
                    Graphics::Recorder::LabelRefusal refusal{};
                    if (takeLabel(localDevice->PendingLabel(awaited, awaitedBytes, received, &refusal), refusal, false, false)) return;
                }
                // Recorded work may write the awaited value: send it to the GPU; the poll below sees
                // the store once it lands in the imported memory. Work not writing the range stays
                // in the open batch (its producer is the CPU, an in-flight batch or a queue that has
                // not recorded the label yet; the poll loop submits it once it does). Submit only,
                // no reap: this thread must not wait for the GPU under the mutex on another queue's
                // behalf (a reaped write-back can sync a later batch through the flush hook).
                if (!overlapSubmit || localDevice->OpenWriteOverlaps(awaited, awaitedBytes)) localDevice->SubmitRecorded(false);
            }
        }
        // Consumer-driven submission: the producer's label may be recorded into the open batch after
        // this wait began, and the producing worker submits it only at its next packet, so the
        // poller (idle anyway) watches the recorder's write generation and submits the batch itself
        // when it writes the awaited range, or when any label passed its deadline. In the sleeping
        // phase it also reaps finished batches when labels wait in completion actions (a wait on
        // such a label converges without a reaper thread). The mutex is only tried: a failed try
        // keeps the generation unconsumed and the next check retries. Returns true when the label
        // table satisfied the wait meanwhile: a label recorded after this wait began (the common
        // queue 0 -> compute -> queue 0 chain when queue 0 reaches its wait first) is accepted on
        // the same stamp argument as at entry, sparing the GPU round trip. Every submit here is
        // reap-free: a poller never waits for the GPU under the mutex on the producer's behalf.
        // An unlocked entry decided from the snapshot alone (or one whose try failed): the first
        // check re-does the entry's overlap test under the mutex (a note whose snapshot was not yet
        // published when the entry looked must still get its batch submitted; see the entry comment).
        static const bool pollProfile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        auto& poll = pollStats();
        const auto pollRefused = [&](Graphics::Recorder::LabelRefusal refusal) {
            if (pollProfile && refusal != Graphics::Recorder::LabelRefusal::None && static_cast<std::size_t>(refusal) < poll.refusals.size()) ++poll.refusals[static_cast<std::size_t>(refusal)];
        };
        // A compute poller reaps only for a label the table refused as BehindCompletion (its
        // batch's completion action is the store the poll waits for); every other completion
        // belongs to queue 0's rate-limited reap, which queue 0's own poller continues inside a
        // wait (its boundary reap cannot run there, and the awaited value may itself land only
        // through a completion the table cannot describe: a copied buffer's write-back). While
        // queue 0 sleeps untimed nobody else reaps, so a compute poller does. Without the table
        // (APS5_NO_LABEL_SHORTCUT) no lookup can say BehindCompletion, so the old rule (reap
        // whenever completions are pending) stays, as with APS5_POLL_REAP=1.
        const bool reapAll = PollReapAll() || !labelShortcut || queue == 0;
        std::chrono::steady_clock::time_point lastTry{};
        const auto pollService = [&](bool sleeping) {
            if (pollProfile) ++poll.ticks;
            const auto generation = Graphics::Recorder::WriteGeneration();
            const bool changed = generation != seenGeneration || recheck;
            const bool labelDue = LabelFlushDue();
            const bool reap = sleeping && (behindCompletion || ((reapAll || queue0Dormant.load(std::memory_order_relaxed)) && CompletionsPending()));
            if (!changed && !labelDue && !reap) return false;
            // Only a deadline or a reap due: the try is rate-limited (the producer's record shows as
            // a generation change and is tried at once).
            if (!changed && !PollTryEach()) {
                const auto now = std::chrono::steady_clock::now();
                if (now - lastTry < PollTryInterval) return false;
                lastTry = now;
            }
            if (changed && unlocked && labelShortcut) {
                // The table first, without the mutex: a hit ends the wait. A miss is not final (the
                // producer may be between its generation bump and its table entry or snapshot), so
                // the generation is consumed only below, under a successful try_lock, where the
                // table, the open batch and the snapshot are consistent (a try_lock never waits,
                // so this costs no mutex wait; a failed one leaves the generation for the next check).
                Graphics::Recorder::LabelRefusal refusal{};
                if (takeLabel(Graphics::Recorder::LookupLabel(awaited, awaitedBytes, received, &refusal), refusal, true, true)) {
                    if (pollProfile) ++poll.tableHits;
                    return true;
                }
                pollRefused(refusal);
            }
            if (pollProfile) ++poll.tries;
            std::unique_lock gpuLock(GuestMemory::GpuMutex(), std::try_to_lock);
            if (!gpuLock.owns_lock()) {
                ++triesFailed[TryPoll];
                if (pollProfile) ++poll.triesFailed;
                return false;
            }
            // Destroyed before the unique_lock releases: the hold as the other threads saw it.
            struct TryHold {
                bool enabled;
                PollStats& poll;
                std::chrono::steady_clock::time_point start;
                ~TryHold() {
                    if (!enabled) return;
                    const auto us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
                    poll.tryHoldUs += us;
                    poll.longestTryHoldUs = std::max(poll.longestTryHoldUs, us);
                }
            } tryHold{pollProfile, poll, pollProfile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{}};
            const auto localDevice = device.load();
            if (localDevice == nullptr) {
                seenGeneration = generation;
                recheck = false;
                return false;
            }
            bool submitted = false;
            if (changed) {
                seenGeneration = generation;
                recheck = false;
                if (labelShortcut) {
                    Graphics::Recorder::LabelRefusal refusal{};
                    if (takeLabel(localDevice->PendingLabel(awaited, awaitedBytes, received, &refusal), refusal, true, false)) {
                        if (pollProfile) ++poll.tableHits;
                        return true;
                    }
                    pollRefused(refusal);
                }
                if (localDevice->OpenWriteOverlaps(awaited, awaitedBytes)) {
                    localDevice->SubmitRecorded(false);
                    submitted = true;
                }
            }
            if (labelDue && !submitted && Graphics::Recorder::PendingLabelSince().has_value()) {
                localDevice->SubmitRecorded(false);
                submitted = true;
            }
            if (submitted) {
                ++outcomes.pollSubmits;
                if (pollProfile) ++poll.submits;
            }
            if (reap) {
                const auto withWorkBefore = pollProfile ? Graphics::Recorder::ReapsWithWork() : 0;
                localDevice->ReapRecorded();
                ++outcomes.pollReaps;
                if (pollProfile) {
                    ++poll.reaps;
                    ++(behindCompletion ? poll.reapsBehindCompletion : poll.reapsOther);
                    poll.reapsWithWork += Graphics::Recorder::ReapsWithWork() - withWorkBefore;
                }
            }
            return false;
        };
        // A failed entry try is retried once at once, not only after the spin's first 64 pauses:
        // the mutex may have freed during the entry's own lookup, and the submit a blocking entry
        // made the instant it got the mutex should not wait for the loop's first service.
        if (entryTryFailed && pollService(false)) return;
        // The producer is usually another queue or the CPU: spin briefly with pause (the value is
        // often written within microseconds; the graphics queue's waits average under a millisecond
        // and it is the frame-critical thread, so it keeps its core longer than the compute queues,
        // which mostly wait tens of milliseconds for it), then poll every 200 us so waiting queues
        // do not burn cores. While spinning, the clock, the failure flag and the progress counters
        // are read every 64 pauses only; the timeout accounting below stays on the sleeping path.
        // Debug aid: APS5_NO_PAUSE_SPIN=1 yields 4000 times before sleeping as before.
        static const bool pauseSpin = std::getenv("APS5_NO_PAUSE_SPIN") == nullptr;
        const auto spinLimit = queue == 0 ? std::chrono::microseconds(1500) : std::chrono::microseconds(100);
        const auto spinStart = std::chrono::steady_clock::now();
        bool spinning = pauseSpin;
        std::uint32_t polls = 0;
        while (!Pm4::WaitSatisfiedUnchecked(packet)) {
            ++polls;
            if (spinning) {
                _mm_pause();
                if ((polls & 63u) != 0) continue;
                if (std::chrono::steady_clock::now() - spinStart > spinLimit) spinning = false;
            } else if (pauseSpin || polls >= 4000) {
                PollSleep();
            } else {
                std::this_thread::yield();
            }
            CheckFailure();
            if (pollService(!spinning)) return;
            if (const auto done = packetsDone.load(); done != lastDone || packetsInFlight.load() != 0) {
                lastDone = done;
                start = std::chrono::steady_clock::now();
            }
            // Some producers (skipped shaders, interrupt-driven CPU work) are not modeled; rather than deadlock
            // the queue, give up on a wait after a while and report it once.
            if (!warned && std::chrono::steady_clock::now() - start > std::chrono::milliseconds(WaitTimeoutMs())) {
                warned = true;
                ++outcomes.timedOut;
                static std::set<std::uint64_t> reported;
                static std::uint64_t timeouts = 0;
                if (++timeouts % 20 == 0) std::fprintf(stderr, "[gpu] %llu GPU waits have timed out\n", static_cast<unsigned long long>(timeouts));
                if (!reported.insert(packet[2] | (static_cast<std::uint64_t>(packet[3]) << 32u)).second) return;
                std::fprintf(stderr, "[gpu] queue 0x%x WAIT_REG_MEM at 0x%llx timed out after %dms (function %u ref 0x%x mask 0x%x value 0x%x)\n", queue,
                             static_cast<unsigned long long>(packet[2] | (static_cast<std::uint64_t>(packet[3]) << 32u)), WaitTimeoutMs(), packet[1] & 7u, packet[4], packet[5],
                             *reinterpret_cast<const volatile std::uint32_t*>(packet[2] | (static_cast<std::uint64_t>(packet[3]) << 32u)));
                const auto address = packet[2] | (static_cast<std::uint64_t>(packet[3]) << 32u);
                for (const auto& record : writeHistory()) {
                    if (record.length != 0 && record.target <= address && address < record.target + std::max<std::uint64_t>(record.length, 4))
                        std::fprintf(stderr, "[gpu]   earlier write by queue 0x%x opcode 0x%x at 0x%llx+0x%llx\n", record.queue, record.opcode, static_cast<unsigned long long>(record.target), static_cast<unsigned long long>(record.length));
                }
                // The recorder's view of the awaited range: what a poller could still do for it.
                if (const auto reportDevice = device.load(); reportDevice != nullptr) {
                    Graphics::Recorder::LabelRefusal refusal{};
                    const auto hit = reportDevice->PendingLabel(awaited, awaitedBytes, received, &refusal);
                    const auto since = Graphics::Recorder::PendingLabelSince();
                    const auto age = since.has_value() ? std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - *since).count() : -1.0;
                    std::fprintf(stderr, "[gpu]   recorder: table %s (refusal %u), open batch writes it %d, snapshot write overlaps %d, pending completion labels %llu, write-back completions %llu, pending label age %.1f ms, write generation %llu seen %llu, queue 0 dormant %d, completions pending %d, label due %d\n",
                                 hit.has_value() ? "hit" : "miss", static_cast<unsigned>(refusal), reportDevice->OpenWriteOverlaps(awaited, awaitedBytes) ? 1 : 0, Graphics::Recorder::SnapshotWriteOverlaps(awaited, awaitedBytes) ? 1 : 0,
                                 static_cast<unsigned long long>(Graphics::Recorder::PendingCompletionLabels()), static_cast<unsigned long long>(Graphics::Recorder::PendingWriteBackCompletions()), age,
                                 static_cast<unsigned long long>(Graphics::Recorder::WriteGeneration()), static_cast<unsigned long long>(seenGeneration), queue0Dormant.load(std::memory_order_relaxed) ? 1 : 0, CompletionsPending() ? 1 : 0, LabelFlushDue() ? 1 : 0);
                }
                context.Each([](const std::string& line) { std::fprintf(stderr, "[gpu]   preceding packet %s\n", line.c_str()); });
            }
            if (warned) return;
        }
        ++outcomes.polled;
    }

    void execute(const Submission& submission) {
        if (submission.suspend) {
            // A suspend point only marks where the system may suspend the title (the system, not the
            // title, waits for the GPU there); this drained the device under the mutex once per
            // frame, which was every 'idle' fence wait of queue 0 in the [recorder] line (~10 ms
            // each, the frame's whole batch list) although nothing reads results here. Now the
            // frame's batches are only submitted, this worker's queued labels first; the register
            // reset that follows needs no idle GPU (recorded work keeps what it uses).
            // Debug aid: APS5_SUSPEND_DRAIN=1 drains as before.
            static const bool suspendDrain = std::getenv("APS5_SUSPEND_DRAIN") != nullptr;
            static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
            ++suspendPoints;
            auto& costs = submissionCosts(submission.queue);
            // Nothing queued or recorded (the lock-free test of the submission end): no mutex.
            if (suspendDrain || !deferredLabels().labels.empty() || Graphics::Recorder::PendingLabelSince().has_value() || Graphics::Recorder::RecordedWorkSinceSubmit() != 0) {
                const auto start = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
                GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Flush);
                std::lock_guard gpuLock(GuestMemory::GpuMutex());
                if (const auto localDevice = device.load()) {
                    recordDeferredLabels(localDevice.get(), submission.queue);
                    if (suspendDrain) localDevice->WaitIdle();
                    else localDevice->SubmitRecorded(submission.queue == 0);
                }
                ++costs.suspends;
                if (profile) costs.suspendNs += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count());
            } else {
                ++costs.suspendsSkipped;
            }
            resetGraphics = true;
            return;
        }
        QueueState* state = nullptr;
        {
            std::lock_guard lock(mutex);
            if (submission.queue == 0 && resetGraphics) {
                queues.erase(0);
                resetGraphics = false;
            }
            state = &queues[submission.queue];
        }
        auto& queue = *state;
        static const bool traceGpu = std::getenv("APS5_TRACE_GPU") != nullptr;
        if (traceGpu) std::fprintf(stderr, "[gpu] %.1f execute serial=%llu queue=0x%x dwords=%zu\n", TraceMs(), static_cast<unsigned long long>(submission.serial), submission.queue, submission.commands.size());
        // Debug aid: APS5_DUMP_QUEUE=<hex queue> prints the packets of that queue's first 40 submissions.
        static const long dumpQueue = [] { const char* text = std::getenv("APS5_DUMP_QUEUE"); return text ? std::strtol(text, nullptr, 16) : -1L; }();
        if (static_cast<long>(submission.queue) == dumpQueue) {
            // Only this queue's thread gets here.
            static int dumped = 0;
            if (dumped++ < 40) {
                std::string text = "[queue] submission " + std::to_string(submission.serial) + ":\n";
                for (std::size_t cursor = 0; cursor < submission.commands.size();) {
                    const auto header = submission.commands[cursor];
                    const auto count = Pm4::PacketWords(header);
                    char line[200];
                    int length = std::snprintf(line, sizeof(line), "[queue]   %s", Pm4::Name(header).c_str());
                    for (std::size_t i = 1; i < count && i < 10 && length < 180; ++i) length += std::snprintf(line + length, sizeof(line) - length, " %08x", submission.commands[cursor + i]);
                    text += line;
                    text += "\n";
                    cursor += count;
                }
                std::fputs(text.c_str(), stderr);
            }
        }
        PacketHistory recent{submission.commands};
        // APS5_PROFILE_DRAW: per-queue time spent handling each packet type, reported every 10 s, so
        // the packets a frame's submission spends its time on are visible.
        static const bool profilePackets = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        struct PacketProfile {
            std::map<std::uint32_t, std::pair<std::uint64_t, double>> byOpcode;
            std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
            std::uint64_t submissions = 0;
            // Time in flushBetweenPackets (deferred label submits, reaps and, on queue 0, the mutex
            // wait for them), kept apart from the packets so their buckets show their own cost.
            double flushMs = 0;
            // DISPATCH_DIRECT packets by outcome (count, ms): real dispatches, the copy and fill HLEs,
            // and packets a tolerated failure skipped.
            std::array<std::pair<std::uint64_t, double>, static_cast<std::size_t>(DispatchOutcome::Count)> dispatchOutcomes{};
        };
        thread_local PacketProfile packetProfile;
        ++packetProfile.submissions;
        // A submission orders every CPU write the game made before submitting it (the collect memo
        // of this worker starts over here); inside it only the ordering points below do, unless
        // APS5_PACKET_EPOCH=1 restores an epoch per packet.
        bumpEpoch(&EpochBumps::submissions);
        for (std::size_t cursor = 0; cursor < submission.commands.size();) {
            if (PacketEpoch()) bumpEpoch(&EpochBumps::packets);
            CheckFailure();
            const auto header = submission.commands[cursor];
            if (Pm4::FillerPacket(header)) { ++cursor; continue; }
            const auto count = Pm4::PacketWords(header);
            const auto packet = std::span(submission.commands).subspan(cursor, count);
            const auto opcode = (header >> 8u) & 0xffu;
            // Names this packet for the flush hook's sync attribution ([hooksync]); the flip is 0xffff.
            GuestMemory::SetCurrentPacket(header == FlipPacketHeader ? 0xffffu : opcode, submission.queue);
            CaptureTrace::Log("packet submission=%llu queue=%x offset=%zu header=%08x words=%zu", static_cast<unsigned long long>(submission.serial), submission.queue, cursor, header, packet.size());
            // Pending labels and full batches go to the GPU before this packet's own work starts
            // (see flushBetweenPackets); labels themselves only check the deadline, so a label
            // group shares one submission. Timed on its own, before the packet's timer starts.
            const auto flushStart = profilePackets ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
            flushBetweenPackets(submission.queue, header, opcode == 0x49 || opcode == 0x37);
            struct PacketTimer {
                bool enabled; std::uint32_t key; std::uint32_t queue; PacketProfile& profile; std::chrono::steady_clock::time_point start;
                ~PacketTimer() {
                    if (!enabled) return;
                    const auto now = std::chrono::steady_clock::now();
                    auto& entry = profile.byOpcode[key];
                    ++entry.first;
                    entry.second += std::chrono::duration<double, std::milli>(now - start).count();
                    if (now - profile.lastReport < std::chrono::seconds(10)) return;
                    profile.lastReport = now;
                    std::vector<std::pair<std::uint32_t, std::pair<std::uint64_t, double>>> hot(profile.byOpcode.begin(), profile.byOpcode.end());
                    std::sort(hot.begin(), hot.end(), [](const auto& a, const auto& b) { return a.second.second > b.second.second; });
                    std::string report;
                    for (std::size_t i = 0; i < hot.size() && i < 10; ++i) {
                        char text[96];
                        std::snprintf(text, sizeof(text), " %s x%llu %.0fms", hot[i].first == 0xffffu ? "flip" : Pm4::Name(hot[i].first << 8u).c_str(), static_cast<unsigned long long>(hot[i].second.first), hot[i].second.second);
                        report += text;
                    }
                    auto& waits = waitOutcomes();
                    auto& epochs = epochBumps();
                    // The late counters are cumulative per thread: this window's deltas.
                    const auto late = Graphics::Recorder::LateCounts();
                    auto& lateSeen = lateCountsSeen();
                    // The per-submission costs of this queue over the window (Driver::Submit's on the
                    // game thread, the rest on this worker), reset here.
                    auto& costs = submissionCosts(queue);
                    const auto submitted = costs.submissions.exchange(0);
                    const auto perSubmission = [&](std::atomic<std::uint64_t>& ns) { const auto total = ns.exchange(0); return submitted != 0 ? static_cast<double>(total) / 1000.0 / static_cast<double>(submitted) : 0.0; };
                    const auto validateUs = perSubmission(costs.validateNs), copyUs = perSubmission(costs.copyNs), dequeueUs = perSubmission(costs.dequeueNs), completeUs = perSubmission(costs.completeNs);
                    const auto suspends = costs.suspends.exchange(0);
                    const auto suspendUs = suspends != 0 ? static_cast<double>(costs.suspendNs.exchange(0)) / 1000.0 / static_cast<double>(suspends) : 0.0;
                    std::string outcomes;
                    for (std::size_t i = 0; i < profile.dispatchOutcomes.size(); ++i) {
                        char text[64];
                        std::snprintf(text, sizeof(text), " %s %llu %.0fms", DispatchOutcomeNames[i], static_cast<unsigned long long>(profile.dispatchOutcomes[i].first), profile.dispatchOutcomes[i].second);
                        outcomes += text;
                    }
                    auto& poll = pollStats();
                    std::fprintf(stderr, "[poll] queue 0x%x (10 s): ticks %llu, table hits %llu, tries %llu / failed %llu, submits %llu, reaps %llu (%llu with work; by reason: behind completion %llu / other %llu), refusals: %s %llu, %s %llu, %s %llu, %s %llu, %s %llu; longest try hold %.1f us, try holds %.1f ms\n", queue, static_cast<unsigned long long>(poll.ticks), static_cast<unsigned long long>(poll.tableHits), static_cast<unsigned long long>(poll.tries), static_cast<unsigned long long>(poll.triesFailed), static_cast<unsigned long long>(poll.submits), static_cast<unsigned long long>(poll.reaps), static_cast<unsigned long long>(poll.reapsWithWork), static_cast<unsigned long long>(poll.reapsBehindCompletion), static_cast<unsigned long long>(poll.reapsOther), LabelRefusalNames[1], static_cast<unsigned long long>(poll.refusals[1]), LabelRefusalNames[2], static_cast<unsigned long long>(poll.refusals[2]), LabelRefusalNames[3], static_cast<unsigned long long>(poll.refusals[3]), LabelRefusalNames[4], static_cast<unsigned long long>(poll.refusals[4]), LabelRefusalNames[5], static_cast<unsigned long long>(poll.refusals[5]), poll.longestTryHoldUs, poll.tryHoldUs / 1000.0);
                    poll = PollStats{};
                    std::fprintf(stderr, "[packets] queue 0x%x %llu submissions, time by packet (10 s):%s, flush %.0fms; DISPATCH_DIRECT by outcome:%s; per submission (%llu submitted): validate %.1f us, copy %.1f us, dequeue %.1f us, complete %.1f us; suspend points %llu x %.1f us, %llu skipped (nothing open); end submits %llu made, %llu skipped, %llu notifies skipped; waits satisfied: at entry %llu, from recorder %llu (%llu while polling, same queue %llu, %llu without the GPU mutex, %llu late-trusted), polled %llu (%llu entered without the GPU mutex, %llu entry tries failed; late candidates %llu: refused cpu-store %llu, overwritten %llu, unclosed %llu, queued %llu), timed out %llu; poll submits %llu, poll reaps %llu; epoch bumps: submissions %llu, waits %llu, drains %llu, reaps %llu, packets %llu\n", queue, static_cast<unsigned long long>(profile.submissions), report.c_str(), profile.flushMs, outcomes.c_str(), static_cast<unsigned long long>(submitted), validateUs, copyUs, dequeueUs, completeUs, static_cast<unsigned long long>(suspends), suspendUs, static_cast<unsigned long long>(costs.suspendsSkipped.exchange(0)), static_cast<unsigned long long>(costs.endSubmits.exchange(0)), static_cast<unsigned long long>(costs.endSkipped.exchange(0)), static_cast<unsigned long long>(costs.notifiesSkipped.exchange(0)), static_cast<unsigned long long>(waits.atEntry), static_cast<unsigned long long>(waits.fromRecorder + waits.fromRecorderPolling), static_cast<unsigned long long>(waits.fromRecorderPolling), static_cast<unsigned long long>(waits.fromRecorderSameQueue), static_cast<unsigned long long>(waits.fromRecorderUnlocked), static_cast<unsigned long long>(waits.fromRecorderLate), static_cast<unsigned long long>(waits.polled), static_cast<unsigned long long>(waits.entriesUnlocked), static_cast<unsigned long long>(waits.entryTriesFailed), static_cast<unsigned long long>(late.candidates - lateSeen.candidates), static_cast<unsigned long long>(waits.lateRefusedCpuStore), static_cast<unsigned long long>(late.overwritten - lateSeen.overwritten), static_cast<unsigned long long>(late.unclosed - lateSeen.unclosed), static_cast<unsigned long long>(late.queued - lateSeen.queued), static_cast<unsigned long long>(waits.timedOut), static_cast<unsigned long long>(waits.pollSubmits), static_cast<unsigned long long>(waits.pollReaps), static_cast<unsigned long long>(epochs.submissions), static_cast<unsigned long long>(epochs.waits), static_cast<unsigned long long>(epochs.drains), static_cast<unsigned long long>(epochs.reaps), static_cast<unsigned long long>(epochs.packets));
                    lateSeen = late;
                    waits = WaitOutcomes{};
                    epochs = EpochBumps{};
                    profile.byOpcode.clear();
                    profile.submissions = 0;
                    profile.flushMs = 0;
                    profile.dispatchOutcomes = {};
                }
            } packetTimer{profilePackets, header == FlipPacketHeader ? 0xffffu : opcode, submission.queue, packetProfile, std::chrono::steady_clock::now()};
            // The packet bracket of the dispatch and draw phase lines (see pendingDispatchPhases).
            if (profilePackets) {
                packetStartedAt() = packetTimer.start;
                pendingDispatchPhases() = {};
                pendingDrawPhases() = {};
            }
            const auto finishDispatchPacket = [&](bool indirect) {
                if (!profilePackets) return;
                const auto now = std::chrono::steady_clock::now();
                auto& pending = pendingDispatchPhases();
                if (pending.phases) {
                    pending.ms[PhaseEpilogue] = std::chrono::duration<double, std::milli>(now - pending.tailAt).count();
                    addDriverPhases(submission.queue != 0 ? OtherQueues : indirect ? Queue0Indirect : Queue0Direct, pending.ms, pending.hit, pending.validated);
                }
                if (indirect) return;
                auto& row = packetProfile.dispatchOutcomes[static_cast<std::size_t>(pending.outcome)];
                ++row.first;
                row.second += std::chrono::duration<double, std::milli>(now - packetTimer.start).count();
            };
            const auto finishDrawPacket = [&](bool drawn) {
                if (!profilePackets) return;
                const auto now = std::chrono::steady_clock::now();
                auto& pending = pendingDrawPhases();
                std::array<double, DrawDriverPhaseCount> ms{};
                if (drawn && pending.phases) {
                    ms = pending.ms;
                    ms[DrawRowEpilogue] = std::chrono::duration<double, std::milli>(now - pending.tailAt).count();
                } else {
                    ms[DrawRowSkipped] = std::chrono::duration<double, std::milli>(now - packetTimer.start).count();
                }
                addDrawPhases(ms, drawn && pending.phases, pending.captures);
            };
            if (profilePackets) packetProfile.flushMs += std::chrono::duration<double, std::milli>(packetTimer.start - flushStart).count();
            // Label writes are recorded on the GPU behind the work they signal whenever possible, so
            // the CPU never waits for them. Otherwise packets that write guest memory in order with
            // GPU work (labels, copies, constant RAM dumps), draws and flips drain the recorded batches
            // first; reads (indirect arguments, register lists, waits) go through the flush hook.
            // Debug aid: APS5_DRAIN_ALL=1 drains before every memory packet as before.
            static const bool drainAll = std::getenv("APS5_DRAIN_ALL") != nullptr;
            static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
            bool wroteOnGpu = false;
            // A label the CPU writes while the recorder is idle needs no drain: nothing the GPU was
            // given is still running (draws complete synchronously), so the write is already ordered
            // after every earlier packet. That was 70% of all drains at the movie stage.
            bool orderedAlready = false;
            // Labels that store nothing (RELEASE_MEM without data select or destination:
            // interrupt-only) need no drain because Pm4::Execute is a no-op for them. The label
            // counters and the [sync] report live at namespace scope (see reportSync).
            if (!drainAll && (opcode == 0x49 || opcode == 0x37)) {
                if (const auto label = Pm4::DecodeLabelWrite(packet)) {
                    const auto bytes = label->Bytes();
                    if (DeferLabels() && bytes.size() <= DeferredLabel::Capacity && bytes.size() % 4 == 0 && label->address % 4 == 0) {
                        // Queued on this worker, no mutex: recorded with the group by
                        // flushBetweenPackets before the next packet that needs it (or by the
                        // deadline), still ahead of every later packet of this queue.
                        auto& deferred = deferredLabels();
                        if (deferred.labels.empty()) deferred.since = std::chrono::steady_clock::now();
                        auto& entry = deferred.labels.emplace_back();
                        entry.address = label->address;
                        entry.size = bytes.size();
                        std::memcpy(entry.bytes.data(), bytes.data(), bytes.size());
                        ++queuedLabels;
                        wroteOnGpu = true;
                    } else {
                        GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Label);
                        std::lock_guard gpuLock(GuestMemory::GpuMutex());
                        const auto localDevice = device.load();
                        // Earlier labels of this queue go first (queue order). The stamp is taken
                        // under the mutex, right before the label is noted, so it orders after every
                        // submission the game made before this point. Without a device (reason 4)
                        // the packet drains nothing and Pm4::Execute stores it, as before.
                        recordDeferredLabels(localDevice.get(), submission.queue);
                        const auto reason = localDevice != nullptr ? localDevice->WriteLabelOnGpu(label->address, bytes, ++eventSerial, submission.queue) : 4;
                        wroteOnGpu = reason == 0 || reason == 5 || reason == 6;
                        if (reason == 1) {
                            // Idle recorder: stored here, still under the mutex, as
                            // recordDeferredLabels does. Stored later by Pm4::Execute outside it,
                            // another worker could record work over the range first and the flush
                            // hook would then sync for the store.
                            GuestMemory::Write(label->address, bytes, 4);
                            wroteOnGpu = true;
                        }
                        Graphics::Recorder::CloseLabelGroup(GuestMemory::TrackerGeneration());
                        countLabelOutcome(reason);
                        ++immediateLabels;
                    }
                } else if (opcode == 0x49 ? ((packet[2] >> 29u) == 0 || (packet[3] | (static_cast<std::uint64_t>(packet[4]) << 32u)) == 0) : (packet[2] | (static_cast<std::uint64_t>(packet[3]) << 32u)) == 0) {
                    orderedAlready = true;
                    ++noOpLabels;
                } else {
                    ++labelFallbacks[4];
                }
            }
            // The other stores the driver makes for the title (COPY_DATA and DMA_DATA to memory,
            // DUMP_CONST_RAM) used to drain the device and store on the CPU. Their bytes are known
            // before the GPU runs them (immediate, constant RAM, or a source read through the flush
            // hook, which waits only for recorded work writing the source), so the store is recorded
            // on the GPU exactly like a label (VulkanDevice::WriteLabelOnGpu: host import,
            // barriers, label table, pending-write note): a destination that recorded work also
            // writes is then ordered by the queue and no CPU wait happens. An idle recorder keeps
            // the CPU store (nothing to order after; done under the mutex so no work can be recorded
            // over the range first). A store WriteLabelOnGpu cannot take (more than 64 KiB, not
            // 4-byte aligned, no device) or that does not decode drains and stores as before.
            // Kill switch: APS5_CPU_STORES=1 keeps the drain and CPU store for every such packet.
            static const bool cpuStores = std::getenv("APS5_CPU_STORES") != nullptr;
            if (!drainAll && !cpuStores && (opcode == 0x40 || opcode == 0x50 || opcode == 0x83)) {
                constexpr std::size_t gpuStoreLimit = 65536;
                bool drained = true;
                if (const auto store = Pm4::ResolveStore(packet, queue, gpuStoreLimit)) {
                    const auto bytes = store->Bytes();
                    if (bytes.empty()) {
                        // Nothing to store: Pm4::Execute is a no-op for it too.
                        orderedAlready = true;
                        drained = false;
                    } else {
                        // No lock site of its own: these stores count under [lock] 'label'.
                        GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Label);
                        std::lock_guard gpuLock(GuestMemory::GpuMutex());
                        const auto localDevice = device.load();
                        // Results still on the GPU for the range would be stored over (or skipped
                        // around, as a block MarkWritten stamps) the store later: written back
                        // first, like a buffer fill (a GPU-direct write-back is recorded before the
                        // store and ordered by its ALL_COMMANDS -> TRANSFER barrier). The CPU and
                        // drained paths get this through the flush hook of GuestMemory::Write.
                        Graphics::StorageTexture::FlushPending(store->address, bytes.size(), nullptr, "packet store", Graphics::PublishScope::PartialUnits);
                        // Earlier labels of this queue go first (queue order); the stamp is taken
                        // under the mutex like a label's.
                        recordDeferredLabels(localDevice.get(), submission.queue);
                        const auto reason = localDevice != nullptr ? localDevice->WriteLabelOnGpu(store->address, bytes, ++eventSerial, submission.queue) : 4;
                        if (reason == 0 || reason == 5 || reason == 6) {
                            if (reason == 0) ++storesOnGpu;
                            else ++storesBehindCompletions;
                            wroteOnGpu = true;
                            drained = false;
                        } else if (reason == 1) {
                            GuestMemory::Write(store->address, bytes, 1);
                            ++storesOnCpu;
                            wroteOnGpu = true;
                            drained = false;
                        }
                        Graphics::Recorder::CloseLabelGroup(GuestMemory::TrackerGeneration());
                    }
                }
                if (drained) ++storesDrained;
            }
            // Draws no longer drain: a recorded draw follows earlier recorded work in queue order, and
            // a synchronous draw's batch submits the recorder first (CommandBatch); the guest memory a
            // draw's preparation reads goes through the flush hook. Debug aid: APS5_DRAW_DRAIN=1 restores.
            static const bool drawDrain = std::getenv("APS5_DRAW_DRAIN") != nullptr;
            const bool drawPacket = Pm4::DrawOpcode(opcode);
            // Flips no longer drain either: the frame's batches are submitted and the presenter's blit
            // follows them on the same queue (see Driver::Present). Debug aid: APS5_SYNC_FLIP=1 restores.
            static const bool syncFlip = std::getenv("APS5_SYNC_FLIP") != nullptr;
            const bool drains = drainAll ? ((Pm4::AccessesMemory(header) && opcode != 0x16) || opcode == 0x42 || opcode == 0x46 || opcode == 0x58 || header == FlipPacketHeader)
                                         : (!wroteOnGpu && !orderedAlready && (opcode == 0x49 || opcode == 0x37 || opcode == 0x40 || opcode == 0x50 || opcode == 0x83 || (drawPacket && drawDrain) || (header == FlipPacketHeader && syncFlip)));
            if (drains) {
                // The GPU wait happens without the mutex: the batches are submitted under it, the
                // timeline value is waited for outside (the other workers keep recording), then the
                // completions of the batches up to that serial run under it again, in order, before
                // the packet's own CPU store. Work another thread submits meanwhile is not waited
                // for: a RELEASE_MEM only orders after earlier work, and a CPU store into a range
                // such work writes is caught by the flush hook. vkDeviceWaitIdle is not needed: the
                // presenter waits its own render fence and every other submission is synchronous.
                // The device is held by this shared_ptr across the wait and released under the
                // mutex (a replacement on another thread must not destroy it outside).
                // Debug aids: APS5_NO_UNLOCKED_DRAIN=1 and APS5_DRAIN_ALL=1 drain under the mutex.
                static const bool unlockedDrain = std::getenv("APS5_NO_UNLOCKED_DRAIN") == nullptr && !drainAll;
                // A drain is an ordering point for the collect memo (see GuestMemory::BumpCollectEpoch).
                bumpEpoch(&EpochBumps::drains);
                std::shared_ptr<VulkanDevice> draining;
                std::uint64_t epoch = 0;
                {
                    GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Flush);
                    std::lock_guard gpuLock(GuestMemory::GpuMutex());
                    if (profile) {
                        ++drainCounts[header == FlipPacketHeader ? 0xffffu : opcode];
                        ++drainTotal;
                        if (std::chrono::steady_clock::now() - lastSyncReport > std::chrono::seconds(10)) {
                            lastSyncReport = std::chrono::steady_clock::now();
                            reportSync();
                        }
                    }
                    draining = device.load();
                    // A drain is the ordering point: this worker's queued labels (an undecodable
                    // label packet skips the record in flushBetweenPackets) go in first, so the
                    // packet's own CPU store lands after them in queue order.
                    recordDeferredLabels(draining.get(), submission.queue);
                    if (draining != nullptr) {
                        if (unlockedDrain && draining->CanWaitUnlocked()) epoch = draining->SubmitAndEpoch();
                        else draining->WaitIdle();
                    }
                    if (epoch == 0) draining.reset();
                }
                if (epoch != 0) {
                    ++unlockedDrains;
                    try {
                        draining->WaitRecorded(epoch);
                    } catch (...) {
                        // A lost device: if Present replaced it meanwhile this copy is the last
                        // owner, and its recorder must still be torn down under the mutex.
                        GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Flush);
                        std::lock_guard gpuLock(GuestMemory::GpuMutex());
                        draining.reset();
                        throw;
                    }
                    GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Flush);
                    std::lock_guard gpuLock(GuestMemory::GpuMutex());
                    draining->ReapRecorded(epoch);
                    draining.reset();
                }
            }
            traceLabel(packet, submission.queue);
            // Waits do not count as progress; every other packet does, including while it runs.
            const bool waitPacket = opcode == 0x3c || opcode == 0x93 || header == RenderingWaitPacketHeader;
            struct Progress {
                Driver& driver;
                bool counted;
                ~Progress() {
                    if (!counted) return;
                    --driver.packetsInFlight;
                    ++driver.packetsDone;
                }
            } progress{*this, !waitPacket};
            if (!waitPacket) ++packetsInFlight;
            recent.Record(cursor);
            if (header == RenderingWaitPacketHeader) {
                timed(&WorkerProfile::waitMs, [&] { submission.renderingWaits.at(cursor)->Wait(); });
            } else if (header == FlipPacketHeader) {
                CheckFailure();
                std::uint64_t batchesAtFlip = 0, unsignaledAtFlip = 0;
                if (!drains) {
                    // Frame N's recorded batches go to the GPU now; the presenter's own submission
                    // follows them in queue order, so no wait is needed here and GpuReady returns at
                    // once (the title paces on flipPendingNum, which the presenter drops after queuing
                    // the frame's presentation).
                    GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::Flush);
                    std::lock_guard gpuLock(GuestMemory::GpuMutex());
                    const auto localDevice = device.load();
                    // This worker's queued labels belong to the frame: recorded first (queue order),
                    // then everything goes out (an empty open batch submits nothing) unless the
                    // record submitted already (a second call would only scan the fences again).
                    if (!recordLabelsForPacket(localDevice.get(), submission.queue) && localDevice != nullptr) localDevice->SubmitRecorded(submission.queue == 0);
                    if (localDevice != nullptr) localDevice->FlipBatches(batchesAtFlip, unsignaledAtFlip);
                }
                ++flipsCounted;
                if (batchesAtFlip != 0) flipSerial = batchesAtFlip;
                flipBatchesUnsignaled += unsignaledAtFlip;
                // Each flip carries a fresh frame timing record; the per-stage frame statistics upstream
                // collects are not gathered on this path.
                auto frame = std::make_shared<FrameTiming>(++frameSerial);
                const auto now = FrameTiming::Clock::now();
                frame->IncludeSubmission(submission.serial, now, now, now, true);
                frame->SetFlip(submission.serial, cursor, now, now);
                frame->NoteFlipBatches(batchesAtFlip, unsignaledAtFlip);
                CaptureTrace::Log("flip frame=%llu submission=%llu offset=%zu batch=%llu unsignaled=%llu", static_cast<unsigned long long>(frameSerial), static_cast<unsigned long long>(submission.serial), cursor, static_cast<unsigned long long>(batchesAtFlip), static_cast<unsigned long long>(unsignaledAtFlip));
                submission.flips.at(cursor)->GpuReady(frame);
            } else if (opcode == 0x15) {
                timed(&WorkerProfile::dispatchMs, [&] { tolerate("dispatch", [&] { dispatch(queue, packet, submission); }); });
                Graphics::Recorder::CountRecordedWork();
                finishDispatchPacket(false);
            } else if (opcode == 0x16) {
                timed(&WorkerProfile::dispatchMs, [&] { tolerate("indirect dispatch", [&] { dispatchIndirect(queue, packet, submission); }); });
                Graphics::Recorder::CountRecordedWork();
                finishDispatchPacket(true);
            } else if (opcode == 0x3c || opcode == 0x93) {
                static const bool traceGpu = std::getenv("APS5_TRACE_GPU") != nullptr;
                const auto waitStart = std::chrono::steady_clock::now();
                timed(&WorkerProfile::waitMs, [&] { waitMemory(packet, submission.queue, recent, submission.received); });
                if (traceGpu && std::chrono::steady_clock::now() - waitStart > std::chrono::milliseconds(200)) {
                    // List the rest of the submission to show what the stalled queue would have done next.
                    for (std::size_t next = cursor + count, shown = 0; next < submission.commands.size() && shown < 48; ++shown) {
                        const auto nextHeader = submission.commands[next];
                        const auto nextCount = Pm4::PacketWords(nextHeader);
                        const auto nextPacket = std::span(submission.commands).subspan(next, nextCount);
                        std::fprintf(stderr, "[gpu]   then %s", Pm4::Name(nextHeader).c_str());
                        for (std::size_t i = 1; i < nextPacket.size() && i < 7; ++i) std::fprintf(stderr, " %08x", nextPacket[i]);
                        std::fprintf(stderr, "\n");
                        next += nextCount;
                    }
                }
            } else if (drawPacket) {
                bool drawn = false;
                timed(&WorkerProfile::drawMs, [&] { tolerate("draw", [&] {
                    static const bool traceDraws = std::getenv("APS5_TRACE_DRAWS") != nullptr;
                    static const bool profileDraws = std::getenv("APS5_PROFILE_DRAW") != nullptr;
                    const auto color = (static_cast<std::uint64_t>(readRegister(queue.context, 0x390)) << 40u) | (static_cast<std::uint64_t>(readRegister(queue.context, 0x318)) << 8u);
                    const auto started = profileDraws ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
                    const auto countSkip = [&](Graphics::DrawSkip kind) {
                        if (profileDraws) Graphics::CountDrawSkip(kind, std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - started).count());
                    };
                    // A draw the decode refused (prechecked without a throw, or thrown): reported
                    // once per message, and the first per color target and reason dumps its banks.
                    const auto skipped = [&](const std::string& what) {
                        if (traceDraws) std::fprintf(stderr, "[draw] target 0x%llx mask 0x%x failed: %.160s\n",static_cast<unsigned long long>(color), readRegister(queue.context, 0x8e), what.c_str());
                        // Name the packet and the color target so skipped draws can be matched against the scanout buffer.
                        char suffix[80];
                        std::snprintf(suffix, sizeof(suffix), " [%s, color target 0x%llx]", Pm4::Name(header).c_str(), static_cast<unsigned long long>(color));
                        // Debug aid: the first failing draw per color target and reason saves its register banks to
                        // draw_<target>_<reason hash>.regs.
                        static std::set<std::pair<std::uint64_t, std::string>> dumpedTargets;
                        const std::string reason = what.substr(0, 48);
                        if (dumpedTargets.insert({color, reason}).second) {
                            char name[64];
                            std::snprintf(name, sizeof(name), "draw_%llx_%08x.regs", static_cast<unsigned long long>(color), static_cast<std::uint32_t>(std::hash<std::string>{}(reason)));
                            if (std::FILE* file = std::fopen(name, "w")) {
                                std::fprintf(file, "# %s\n", what.c_str());
                                recent.Each([&](const std::string& line) { std::fprintf(file, "# packet %s\n", line.c_str()); });
                                for (const auto& [offset, value] : queue.context) std::fprintf(file, "context %x %08x\n", offset, value);
                                for (const auto& [offset, value] : queue.userConfig) std::fprintf(file, "uconfig %x %08x\n", offset, value);
                                for (const auto& [offset, value] : queue.shader) std::fprintf(file, "shader %x %08x\n", offset, value);
                                std::fclose(file);
                            }
                        }
                        reportSkip("draw", what + suffix);
                    };
                    try {
                        std::string rejected;
                        const auto verdict = draw(queue, packet, submission, rejected);
                        drawn = verdict == DrawVerdict::Drawn;
                        CaptureTrace::Log("draw submission=%llu queue=%x offset=%zu target=%llx mask=%x verdict=%d reason=%.256s", static_cast<unsigned long long>(submission.serial), submission.queue, cursor, static_cast<unsigned long long>(color), readRegister(queue.context, 0x8e), static_cast<int>(verdict), rejected.c_str());
                        if (verdict == DrawVerdict::Rejected) {
                            skipped(rejected);
                            countSkip(Graphics::DrawSkip::Prechecked);
                        } else if (verdict == DrawVerdict::Nothing) {
                            countSkip(Graphics::DrawSkip::Nothing);
                        } else if (traceDraws) {
                            std::fprintf(stderr, "[draw] target 0x%llx mask 0x%x ok\n",static_cast<unsigned long long>(color), readRegister(queue.context, 0x8e));
                        }
                    } catch (const std::exception& error) {
                        CaptureTrace::Log("draw-error submission=%llu offset=%zu reason=%.256s", static_cast<unsigned long long>(submission.serial), cursor, error.what());
                        skipped(error.what());
                        countSkip(Graphics::DrawSkip::Thrown);
                    }
                }); });
                finishDrawPacket(drawn);
            } else if (opcode != 0x42 && opcode != 0x46 && opcode != 0x58) {
                if (!wroteOnGpu) Pm4::Execute(packet, queue);
            }
            if (drawPacket) Graphics::Recorder::CountRecordedWork();
            cursor += count;
        }
        // The submission's last labels and dispatches go to the GPU before it counts as completed
        // (Driver::WaitIdle and the game threads polling the labels must not depend on a later
        // packet of some queue). Checked lock-free first: an empty open batch needs no submit. A
        // compute worker submits without reaping (it must not wait under the mutex for queue 0).
        // A queue-0 submission whose successor is already queued leaves its labels queued and the
        // batch open to that successor's packets (recorded and submitted at their flush or own
        // lock, the mid-submission rule; the label deadline still bounds pollers): the last one
        // of a burst submits, so WaitIdle and polling after an idle queue keep their timing. A
        // compute queue always submits (its tail would otherwise wait for a poller).
        // Debug aid: APS5_SUBMIT_AT_END=1 submits at every submission's end, as before.
        static const bool submitAtEnd = std::getenv("APS5_SUBMIT_AT_END") != nullptr;
        if (!deferredLabels().labels.empty() || Graphics::Recorder::PendingLabelSince().has_value() || Graphics::Recorder::RecordedWorkSinceSubmit() != 0) {
            auto& costs = submissionCosts(submission.queue);
            if (!submitAtEnd && submission.queue == 0 && workerQueued() != nullptr && workerQueued()->load(std::memory_order_acquire) != 0) {
                ++costs.endSkipped;
                return;
            }
            ++costs.endSubmits;
            GuestMemory::TagGpuLockSite(GuestMemory::GpuLockSite::End);
            std::lock_guard gpuLock(GuestMemory::GpuMutex());
            const auto localDevice = device.load();
            // This worker's deferred labels are recorded first (they belong to this submission).
            recordDeferredLabels(localDevice.get(), submission.queue);
            if (localDevice != nullptr) localDevice->SubmitRecorded(submission.queue == 0);
        }
    }

    // The calling worker's pending count (QueueWorker::queued), set by run(); null off a worker.
    static const std::atomic<std::uint64_t>*& workerQueued() {
        static thread_local const std::atomic<std::uint64_t>* queued = nullptr;
        return queued;
    }

    // An idle worker retires labels waiting in completion actions (see Recorder::AfterCompletions):
    // without it a label whose only consumer is a game thread, submitted last before the game waits
    // for it, would never land once every worker sleeps. Non-blocking (a fence status check under
    // the mutex, tried only); the caller polls it every millisecond while such labels exist.
    void reapCompletionLabels() {
        std::unique_lock gpuLock(GuestMemory::GpuMutex(), std::try_to_lock);
        if (!gpuLock.owns_lock()) {
            ++triesFailed[TryIdle];
            return;
        }
        // Every idle worker polls this; once another worker reaped the labels there is nothing to
        // do, and the epoch bump below would only inflate the "reaps" and [guestmem] epoch counts.
        if (!CompletionsPending()) return;
        // The write-backs the reap runs collect outside any submission: a fresh epoch, so no walk
        // of this worker's last packet is reused for them.
        bumpEpoch(&EpochBumps::reaps);
        if (const auto localDevice = device.load()) localDevice->ReapRecorded();
    }

    void run(std::uint32_t id) noexcept {
        OnWorkerThread() = true;
        {
            char role[32];
            std::snprintf(role, sizeof role, "queue worker 0x%x", id);
            PinWorkerThread(role);
        }
        // Named from the start, so a worker's fence waits before its first WAIT_REG_MEM land in its
        // own row of the [recorder] "fence waits by thread" report and "untagged" is exactly the
        // presenter and the game threads.
        GuestMemory::TagGpuLockThread(id);
        if (id == 0) StartWorkerSampler();
        Submission submission;
        static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        auto& costs = submissionCosts(id);
        try {
            for (;;) {
                submission = Submission{};
                static const bool traceGpu = std::getenv("APS5_TRACE_GPU") != nullptr;
                {
                    std::unique_lock lock(mutex);
                    auto& worker = workers.at(id);
                    auto& pending = worker.pending;
                    workerQueued() = &worker.queued;
                    if (traceGpu && pending.empty()) std::fprintf(stderr, "[gpu] %.1f idle queue=0x%x\n", TraceMs(), id);
                    const auto ready = [&] { return failure || stopping || !pending.empty(); };
                    // While completions are pending, the idle wait wakes every millisecond to reap
                    // them (the driver mutex is dropped for that: it is never held with the GPU
                    // mutex); otherwise it sleeps until work arrives. A compute worker wakes for
                    // completion labels only (a game thread may poll one); write-backs are queue 0's.
                    // Queue 0's untimed sleep is published: a completion registered during it has
                    // no reaper until work arrives, so a sleeping poller reaps meanwhile.
                    while (!ready()) {
                        if (!CompletionsPending() || (id != 0 && Graphics::Recorder::PendingCompletionLabels() == 0)) {
                            if (id == 0) queue0Dormant.store(true, std::memory_order_relaxed);
                            changed.wait(lock, ready);
                            if (id == 0) queue0Dormant.store(false, std::memory_order_relaxed);
                            break;
                        }
                        if (changed.wait_for(lock, std::chrono::milliseconds(1), ready)) break;
                        lock.unlock();
                        reapCompletionLabels();
                        lock.lock();
                    }
                    rethrowFailure();
                    if (pending.empty()) {
                        break;
                    }
                    submission = std::move(pending.front());
                    pending.pop_front();
                    worker.queued.fetch_sub(1, std::memory_order_acq_rel);
                    if (profile && submission.enqueuedAt != std::chrono::steady_clock::time_point{}) costs.dequeueNs += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - submission.enqueuedAt).count());
                }
                execute(submission);
                if (traceGpu) std::fprintf(stderr, "[gpu] %.1f done serial=%llu queue=0x%x\n", TraceMs(), static_cast<unsigned long long>(submission.serial), id);
                const auto completeStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
                bool notify = true;
                {
                    std::lock_guard lock(mutex);
                    rethrowFailure();
                    markCompleted(submission.serial);
                    // Only WaitIdle waits for a completion (idle workers wait for work, notified at
                    // its enqueue): the notify is skipped while nobody is in it.
                    notify = idleWaiters != 0;
                }
                if (notify) changed.notify_all();
                else ++costs.notifiesSkipped;
                if (profile) costs.completeNs += static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - completeStart).count());
            }
        } catch (...) {
            const auto error = std::current_exception();
            try {
                std::rethrow_exception(error);
            } catch (const std::exception& reason) {
                std::fprintf(stderr, "[gpu] worker failed: %s\n", reason.what());
            } catch (...) {
                std::fprintf(stderr, "[gpu] worker failed with a non-standard exception\n");
            }
            std::fflush(stderr);
            std::terminate();
        }
    }
};

}

void Submit(const Packet* packet, std::uint32_t queue) {
    Driver::Get().Submit(packet, queue);
}

void WaitIdle() {
    Driver::Get().WaitIdle();
}

void RegisterShader(const Shader* shader) {
    Driver::Get().RegisterShader(shader);
}

void SuspendPoint() {
    Driver::Get().SuspendPoint();
}

void RegisterVideoOutput(std::uint32_t handle, const std::shared_ptr<IVideoOutput>& output) {
    Driver::Get().RegisterVideoOutput(handle, output);
}

void UnregisterVideoOutput(std::uint32_t handle, const std::shared_ptr<IVideoOutput>& output) {
    Driver::Get().UnregisterVideoOutput(handle, output);
}

void PresentClear(const PresentationWindow& window, bool opaque, void (*gpuReady)(void*), void* context) {
    Driver::Get().Present(window, nullptr, opaque, gpuReady, context);
}

void PresentBuffer(const PresentationWindow& window, const DisplayBuffer& buffer, void (*gpuReady)(void*), void* context) {
    Driver::Get().Present(window, &buffer, true, gpuReady, context);
}

void ReleaseWindow(void* window) {
    Driver::Get().ReleaseWindow(window);
}

void ReportFailure(std::exception_ptr error) {
    Driver::Get().ReportFailure(error);
}

}

extern "C" void AgcDriverWaitIdle_nid_postfix() {
    AgcDriver::WaitIdle();
}

extern "C" void AgcDriverRegisterShader_nid_postfix(const Shader* shader) {
    AgcDriver::RegisterShader(shader);
}

extern "C" void AgcDriverSuspendPoint_nid_postfix() {
    AgcDriver::SuspendPoint();
}

extern "C" void AgcDriverRegisterVideoOutput_nid_postfix(std::uint32_t handle, const std::shared_ptr<AgcDriver::IVideoOutput>& output) {
    AgcDriver::RegisterVideoOutput(handle, output);
}

extern "C" void AgcDriverUnregisterVideoOutput_nid_postfix(std::uint32_t handle, const std::shared_ptr<AgcDriver::IVideoOutput>& output) {
    AgcDriver::UnregisterVideoOutput(handle, output);
}

extern "C" void AgcDriverPresentClear_nid_postfix(const AgcDriver::PresentationWindow& window, bool opaque, void (*gpuReady)(void*), void* context) {
    AgcDriver::PresentClear(window, opaque, gpuReady, context);
}

extern "C" void AgcDriverPresentBuffer_nid_postfix(const AgcDriver::PresentationWindow& window, const AgcDriver::DisplayBuffer& buffer, void (*gpuReady)(void*), void* context) {
    AgcDriver::PresentBuffer(window, buffer, gpuReady, context);
}

extern "C" void AgcDriverReleaseWindow_nid_postfix(void* window) {
    AgcDriver::ReleaseWindow(window);
}

extern "C" void AgcDriverReportFailure_nid_postfix(std::exception_ptr error) {
    AgcDriver::ReportFailure(error);
}
