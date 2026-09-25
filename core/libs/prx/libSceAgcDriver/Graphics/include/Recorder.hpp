#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_RECORDER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_RECORDER_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <array>
#include <deque>
#include <mutex>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <unordered_map>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics {

// Accumulates GPU work across guest commands so the CPU does not wait for each one. Dispatches and the
// copies that feed them record into one open batch; Submit sends it to the queue without waiting and
// Sync waits for every batch, then runs its completion actions (write-backs) in order. Objects handed
// to Keep live until the batch that recorded them completed. Guest memory ranges the recorded work
// will write are noted, so a CPU access to such a range (the GuestMemory flush hook) syncs first.
// Every call happens under GuestMemory::GpuMutex, except WaitSerial and the static lock-free readers
// below. The noted ranges are also published as an immutable snapshot (rebuilt under the mutex at
// every note and after a batch's completions ran) that the flush hook reads without the mutex, so
// accesses overlapping nothing never wait behind device work.
//
// Submissions are numbered (serials) and, when the device has timeline semaphores, every Submit
// signals a timeline semaphore with its serial: a thread can then wait for "everything submitted
// up to serial S" WITHOUT the mutex (WaitSerial), because the monotonic value cannot be reset or
// recycled the way the pooled batch fences are, and afterwards run the completions under the mutex
// (FinishUpTo). Without timeline semaphores callers use the locked Sync as before.
class Recorder {
public:
    // `timelineSemaphores`: the device enabled VK_KHR_timeline_semaphore (WaitSerial is usable).
    explicit Recorder(const Context& context, bool timelineSemaphores = false);
    ~Recorder();
    Recorder(const Recorder&) = delete;
    Recorder& operator=(const Recorder&) = delete;

    // The open batch's command buffer, starting a batch when none is open.
    VkCommandBuffer Commands();
    bool Recording() const { return open != nullptr; }
    bool Idle() const { return open == nullptr && inFlight.empty(); }
    // Whether recorded work still has completion actions (write-backs the CPU must see) to run.
    bool HasCompletions() const;
    // The object lives at least until the batch open now completed; it is then destroyed AFTER the
    // finishing thread released GuestMemory::GpuMutex, on a release thread of its own (never under
    // a hold, see Recorder.cpp ReleaseDeferredKeeps; the finishing thread destroys it itself when
    // the thread's queue is full, APS5_RELEASE_QUEUE_MAX batches, or with APS5_RELEASE_ON_UNLOCK=1;
    // APS5_RELEASE_UNDER_LOCK=1 destroys it in the reap as before), so its destructor must need
    // neither the mutex nor the recorder nor a particular thread. ~Recorder joins the release
    // thread and waits for every release in progress before the device goes.
    void Keep(std::shared_ptr<void> object);
    void OnComplete(std::function<void()> action);
    void NotePendingWrite(std::uint64_t address, std::size_t bytes);
    // Notes several [begin, end) ranges and publishes the snapshot once (a dispatch writes many buffers).
    void NotePendingWrites(std::span<const std::pair<std::uint64_t, std::uint64_t>> ranges);
    bool PendingWriteOverlaps(std::uint64_t address, std::size_t bytes) const;
    // Whether the OPEN (unsubmitted) batch writes the range: a wait on such a range must submit it.
    bool OpenWriteOverlaps(std::uint64_t address, std::size_t bytes) const;
    // What SyncThrough(address, bytes) would wait for, for the [hooksync] attribution: the newest
    // batch whose noted range overlaps the access (the open batch counts first, with the serial it
    // will get), whether that batch's fence has already signaled (its GPU work is done, so the
    // access's bytes are already final; never for the open batch), that noted range, and how many
    // batches the wait finishes. Nullopt: no overlap.
    struct PendingWriteInfo {
        std::uint64_t serial;
        bool open;
        bool signaled;
        std::uint64_t rangeBegin;
        std::uint64_t rangeEnd;
        std::size_t batchesToFinish;
    };
    std::optional<PendingWriteInfo> DescribePendingWrite(std::uint64_t address, std::size_t bytes) const;
    // Ends and submits the open batch without waiting.
    void Submit();
    // Submits the open batch and returns the serial of the newest submitted batch (0 when nothing was
    // ever submitted); WaitSerial(serial) then covers all recorded work.
    std::uint64_t SubmitAndEpoch();
    bool HasTimeline() const { return timeline != VK_NULL_HANDLE; }
    // Waits until every batch up to `serial` completed on the GPU. Called WITHOUT GuestMemory::GpuMutex:
    // it touches only the immutable timeline semaphore (the caller keeps the device alive). Requires
    // HasTimeline(); the debug switch APS5_NO_TIMELINE makes the device create the recorder without one.
    void WaitSerial(std::uint64_t serial);
    // Under the mutex: finishes (completions, release) the in-flight batches up to `serial`, from the
    // front only; tolerates batches another thread finished meanwhile. Later batches stay in flight.
    void FinishUpTo(std::uint64_t serial);
    // Submits and waits for every batch, running completions in order.
    void Sync();
    // Waits only for the batches up to the newest one that writes the range (submitting the open
    // batch when it is that one); later batches stay in flight. Fences of one queue signal in
    // submission order, so completions still run in order. Debug aid: APS5_NO_SYNC_THROUGH=1 syncs all.
    // `waitUnlocked` (the flush hook): when the caller's GpuMutex acquisition is the outermost one
    // on this thread and the device has a timeline, the GPU wait runs WITHOUT the mutex (released
    // and retaken here; the caller must hold nothing else that orders after it) and only the
    // completions run under it, so other queues do not queue behind a CPU read's wait. Inside a
    // batch's completion action no wait is made at all: every batch still in flight was recorded
    // after the completing one, and its store is that batch's in-order write (see Recorder.cpp).
    void SyncThrough(std::uint64_t address, std::size_t bytes, bool waitUnlocked = false);
    // Completes batches that already finished; returns whether nothing is in flight.
    bool Reap();
    std::uint64_t Submissions() const { return submissions; }

    // Pending-label table. A GPU label (RELEASE_MEM/WRITE_DATA recorded as a store into the batch)
    // is noted per 4-byte dword with the value it will store and a record-order stamp (the driver's
    // event serial, taken when the label is recorded). A WAIT_REG_MEM whose submission was received
    // (stamped) BEFORE the label was recorded may take the value from the table instead of waiting
    // for the GPU: any CPU store the game ordered before that submit call precedes the label in
    // program order exactly as on hardware, and later recorded work follows the label in queue
    // order. Entries recorded before the submission (the previous frame's label at the same address)
    // are never trusted. Entries leave the table with their batch (a memory bound only).
    void NoteLabel(std::uint64_t address, std::span<const std::byte> bytes, std::uint64_t stamp, std::uint32_t queue);
    // The 4- or 8-byte value the table holds for `address` when every dword is present with a stamp
    // newer than `afterStamp`; `queue` receives the recording queue of the first dword.
    std::optional<std::uint64_t> PendingLabel(std::uint64_t address, std::size_t bytes, std::uint64_t afterStamp, std::uint32_t& queue) const;
    std::size_t PendingLabels() const { return labels.size(); }
    // Whether any tracked label dword lies inside [address, address + bytes): a range query for
    // large ranges (a fill of megabytes), where a per-dword lookup would not do.
    bool PendingLabelIn(std::uint64_t address, std::size_t bytes) const;
    // PendingLabel of the active recorder WITHOUT GuestMemory::GpuMutex: the table has a small mutex
    // of its own (every mutation holds both), so a WAIT_REG_MEM consults it without queueing behind
    // device work. Nothing is done under the table mutex but the lookup (it never takes the GPU mutex).
    static std::optional<std::uint64_t> LookupLabel(std::uint64_t address, std::size_t bytes, std::uint64_t afterStamp, std::uint32_t& queue);
    // Whether the lock-free pending-write snapshot (open, in-flight and finishing batches) overlaps
    // the range: false means no recorded work writes it, so a wait on it has nothing to submit.
    static bool SnapshotWriteOverlaps(std::uint64_t address, std::size_t bytes);
    // A label behind pending completions (CPU write-backs the label must not precede): stored to
    // guest memory by a completion action appended to the open batch, or to the newest in-flight
    // batch when none is open (no empty submission), so it runs after every earlier write-back in
    // submission order. The range is noted as a pending write of that batch so CPU reads through the
    // flush hook still sync, and the label enters the table like a GPU one.
    // `storedOnGpu`: the same bytes were also recorded into the open batch (a host import), so the
    // completion re-stores them only when a CPU write-back noted by NoteWrittenBack overlapped the
    // range since; the plain store of a label the GPU has no view of always runs. An unconditional
    // completion store would land on memory the game may have reused by then (a stale label value
    // over a fresh command buffer). Debug aid: APS5_LABEL_STORE_ALWAYS=1 stores unconditionally.
    void AfterCompletions(std::uint64_t address, std::span<const std::byte> bytes, std::uint64_t stamp, std::uint32_t queue, bool storedOnGpu);
    // A CPU store of GPU results into guest memory (GuestBufferMemory::WriteBack): recorded so a
    // completion label store can tell whether its bytes were overwritten.
    static void NoteWrittenBack(std::uint64_t address, std::size_t bytes);

    // Lock-free readers for the queue workers' packet loops and WAIT_REG_MEM polls (values of the
    // active recorder; a torn-down recorder never publishes):
    // when the open batch received its first label (nullopt: none pending), so the label's
    // submission can be bounded (APS5_LABEL_FLUSH_US) without taking the mutex to look;
    static std::optional<std::chrono::steady_clock::time_point> PendingLabelSince();
    // bumped by every note of a pending write into the open batch, so a poller learns that the
    // producer may have recorded the label it waits for;
    static std::uint64_t WriteGeneration();
    // labels waiting in completion actions (the CPU must reap their batch for them to land);
    static std::uint64_t PendingCompletionLabels();
    // dispatches and draws recorded since the last submit (the driver counts them: CountRecordedWork),
    // so a batch is submitted after a bounded amount of work (APS5_BATCH_CAP).
    static std::uint64_t RecordedWorkSinceSubmit();
    static void CountRecordedWork();

    // The recorder of the current device, for code that only has guest addresses (the flush hook)
    // and for helpers that must not recycle resources the recorded work still uses.
    static Recorder* Active();
    void Activate();
    // APS5_PROFILE_DRAW: syncs by source (0 device idle, 1 CPU access to pending writes, 2 CPU read
    // after a recorded store, 3 address-based dispatch, 4 other). Contract: the call announces the
    // source for the Sync/SyncThrough this thread makes NEXT, which consumes it (a sync without a
    // preceding CountSync counts as "other"); call it right before the sync and never without one,
    // or the announcement attributes an unrelated later wait. `site` names the call site for the
    // [recorder] "top sync sites" table (a return address, printed as a module offset like the
    // [guestmem] callers): a caller that syncs on behalf of others (VulkanDevice::WaitIdle) passes
    // its own return address, so the table names the driver site, not the wrapper; without it the
    // return address of the Sync/SyncThrough call itself is taken, so an unannounced sync still
    // names its real caller.
    static void CountSync(int source, const void* site = nullptr);
    // Names only the site for the sync or drain this thread makes next (its source stays as
    // announced, or as the drain counts it): for a wrapper (VulkanDevice::ReapRecorded) whose
    // FinishUpTo counts the drain itself, so a CountSync there would count it twice.
    static void AnnounceSyncSite(const void* site);
    // APS5_PROFILE_DRAW: the calling thread's fence and timeline waits so far, in milliseconds (0
    // when not profiling): a caller reads it around a span of its own work to learn how much of
    // that span waited for the GPU (a resource build's nested flush-hook waits, a draw's).
    static double ThreadWaitedMs();
    // APS5_PROFILE_GPU=1: GPU time of recorded work by key (a guest program address), from timestamp
    // queries around each timed range; the totals per key are reported every 10 s. Begin returns the
    // range index to pass to End, or NoTiming when timing is off or the batch's queries are used up.
    static constexpr std::uint32_t NoTiming = 0xffffffffu;
    std::uint32_t BeginGpuTiming(std::uint64_t key);
    void EndGpuTiming(std::uint32_t index);

private:
    struct Batch {
        VkCommandBuffer commands = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        std::vector<std::shared_ptr<void>> kept;
        std::vector<std::function<void()>> completions;
        std::vector<std::pair<std::uint64_t, std::uint64_t>> writes;
        // Submission number (1-based): identifies a batch after its allocation may have been reused.
        std::uint64_t serial = 0;
        bool submitted = false;
        VkQueryPool queries = VK_NULL_HANDLE;
        std::vector<std::uint64_t> timedKeys;
        // Dword addresses this batch noted in the label table (removed when it finishes).
        std::vector<std::uint64_t> labelDwords;
        // Labels stored by this batch's completion actions (see AfterCompletions); subtracted from
        // the lock-free pending count when the batch finishes, whether or not its completions ran.
        std::uint32_t completionLabelCount = 0;
    };
    struct LabelEntry {
        std::uint32_t value;
        std::uint32_t queue;
        std::uint64_t stamp;
        const Batch* batch;
    };
    void readGpuTiming(Batch& batch);

    // The unlocked wait of SyncThrough(waitUnlocked): the target batch (submitting the open one when
    // it is the target), the timeline wait with the mutex released, then the completions up to it.
    // Returns false when the locked path must run instead (no timeline, nested acquisition, off).
    bool syncThroughUnlocked(std::uint64_t address, std::uint64_t end, int source, const void* site);
    // `source` is the CountSync source the wait is attributed to.
    void finish(std::unique_ptr<Batch> batch, bool wait, int source);
    void release(Batch& batch) noexcept;
    static bool overlaps(const Batch& batch, std::uint64_t address, std::uint64_t end);
    // Rebuilds the lock-free snapshot of pending writes from open, inFlight and finishing.
    void publishPendingWrites() const;
    // Appends one range to the open batch; returns whether the snapshot must be rebuilt for it.
    bool noteWrite(std::uint64_t address, std::size_t bytes);
    // Appends one range to `batch` (open or in flight) and publishes the snapshot if needed.
    void noteWriteOn(Batch& batch, std::uint64_t address, std::size_t bytes);
    void noteLabelOn(Batch& batch, std::uint64_t address, std::span<const std::byte> bytes, std::uint64_t stamp, std::uint32_t queue);
    // Whether a write-back noted after `sequence` overlapped [begin, end); true when the ring no
    // longer reaches back to `sequence` (conservative: the store runs as before).
    bool writtenBackSince(std::uint64_t sequence, std::uint64_t begin, std::uint64_t end);
    std::mutex writtenBackMutex;
    std::deque<std::array<std::uint64_t, 3>> writtenBack;
    std::uint64_t writtenBackSequence = 0;
    // PendingLabel without the table mutex (the caller holds it, or the GPU mutex).
    std::optional<std::uint64_t> lookupLabel(std::uint64_t address, std::size_t bytes, std::uint64_t afterStamp, std::uint32_t& queue) const;

    Context context;
    VkSemaphore timeline = VK_NULL_HANDLE;
    // Identity for a thread that released the mutex around a wait (syncThroughUnlocked): the
    // recorder may have been torn down meanwhile, so it is looked up by this id, not by pointer.
    std::uint64_t id = 0;
    // Mutated only under both GuestMemory::GpuMutex and the table mutex (Recorder.cpp), so a holder
    // of either reads it consistently.
    std::unordered_map<std::uint64_t, LabelEntry> labels;
    std::unique_ptr<Batch> open;
    std::deque<std::unique_ptr<Batch>> inFlight;
    // Batches popped from inFlight whose completions have not run yet: their writes stay in the
    // snapshot (a rebuild from inside a completion must not drop them) until finish returns.
    std::vector<const Batch*> finishing;
    std::uint64_t submissions = 0;
    // Command buffers and fences of completed batches, reused by later ones (hundreds of batches per
    // frame would otherwise allocate and free their objects each time).
    std::vector<std::pair<VkCommandBuffer, VkFence>> spare;
};

}

#endif
