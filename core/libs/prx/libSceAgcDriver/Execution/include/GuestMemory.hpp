#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_GUESTMEMORY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_GUESTMEMORY_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <span>
#include <utility>
#include <vector>

namespace AgcDriver::GuestMemory {

void CheckRange(const void* pointer, std::size_t bytes, std::size_t alignment, bool writable = false);
// CheckRange without throwing: whether the whole range is mapped with the requested access.
bool Accessible(const void* pointer, std::size_t bytes, bool writable = false);
// The accessible parts [begin, end) of a range, in address order. GPU heaps are often bound whole while
// the guest commits their pages on demand, so a binding can cover reserved but uncommitted pages.
std::vector<std::pair<std::uint64_t, std::uint64_t>> CommittedRanges(std::uint64_t address, std::size_t bytes, bool writable = false);
// Both facts from one walk of the page tables: the accessible parts of a range and whether they cover
// all of it (`whole`, what Accessible answers). Reserved pages are queried each time, so asking twice
// costs two VirtualQuery per run.
struct Commitment {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
    bool whole = false;
};
Commitment DescribeCommitted(std::uint64_t address, std::size_t bytes, bool writable = false);
void Read(std::uint64_t address, std::span<std::byte> destination, std::size_t alignment = 1);
// Variants for resources in heaps the guest commits on demand (partially resident textures, GPU
// scratch heaps): uncommitted pages read as zeros, are not compared, and are never stored.
void ReadCommitted(std::uint64_t address, std::span<std::byte> destination);
bool EqualsCommitted(std::uint64_t address, std::span<const std::byte> bytes);
// EqualsCommitted without the flush hook: the caller has decided that the recorded GPU work
// writing the range need not be waited for (the dispatch-cache validation in Driver.cpp).
bool EqualsCommittedUnsynced(std::uint64_t address, std::span<const std::byte> bytes);
// EqualsCommittedUnsynced for a caller that treats an unmapped page as a miss (the dispatch-cache
// validation's per-run compare): one walk of the page states, one memcmp per mapped run, no profile
// counter and no reason string. Unmapped: some page of the range is not readable (or the address
// space cannot be queried).
enum class Compare : std::uint8_t { Equal, Differs, Unmapped };
Compare CompareMapped(std::uint64_t address, std::span<const std::byte> bytes);
// CompareMapped's walk with a copy in place of the compare: the bytes at `address` are copied into
// `out` (Equal), or Unmapped as CompareMapped answers it (the dispatch-cache validation's masked
// re-read of a differing run).
Compare CopyMapped(std::uint64_t address, std::span<std::byte> out);
void WriteChangedCommitted(std::uint64_t address, std::span<const std::byte> current, std::span<const std::byte> original);
void Write(std::uint64_t address, std::span<const std::byte> source, std::size_t alignment = 1);
// Stores the parts of `current` that differ from `original` (the guest bytes the GPU started from), so
// guest writes made meanwhile to untouched bytes survive. Compares in 256-byte blocks.
void WriteChanged(std::uint64_t address, std::span<const std::byte> current, std::span<const std::byte> original);

// Write tracking. The guest arena is reserved with page write watching, so CollectWrites learns which
// pages of a range the CPU wrote since they were last collected, stamps their 64 KiB blocks with a new
// generation and returns it; a snapshot of the range taken after that call is current while
// UnchangedSince(range, that generation) holds. Every validation collects its own range first, so
// resources sharing pages see each other's writes. GPU writes into host-imported memory bypass the
// page tables and are reported with MarkWritten. Without write watching CollectWrites returns 0 and
// UnchangedSince is always false, so callers fall back to comparing bytes.
bool WriteWatched();
void Unwatch(std::uint64_t address, std::size_t bytes);
bool ImportWatched(std::uint64_t address, std::size_t bytes, const std::function<bool()>& import);
bool Watched(std::uint64_t address, std::size_t bytes);
std::uint64_t CollectWrites(std::uint64_t address, std::size_t bytes);
bool UnchangedSince(std::uint64_t address, std::size_t bytes, std::uint64_t generation);
// UnchangedSince for several ranges under one tracker lock: true only when every one holds.
struct UnchangedQuery {
    std::uint64_t address;
    std::size_t bytes;
    std::uint64_t generation;
};
bool WrittenSince(std::uint64_t address, std::size_t bytes, std::uint64_t generation);
bool UnchangedSinceAll(std::span<const UnchangedQuery> queries);
// Returns the generation the blocks were stamped with (0 when the arena is not write-watched):
// UnchangedSince(range, it) holds until the next store over the range.
std::uint64_t MarkWritten(std::uint64_t address, std::size_t bytes);
std::uint64_t StoreOwnBytes(std::uint64_t address, std::size_t bytes, const std::function<void()>& store);
std::uint64_t StoreOwnBytes(std::uint64_t address, std::span<const std::byte> source);
bool StoredOver(std::uint64_t address, std::size_t bytes, std::uint64_t generation);
// Collect epoch: within one epoch a range already collected is not walked again, CollectWrites
// returns the current generation instead. A guest write landing between two collects of the same
// epoch is seen by the next epoch, which is the ordering real hardware gives a CPU write made while a
// submission runs. The driver's own writes (Write, WriteChanged, MarkWritten) stamp their blocks, so
// they are never hidden. APS5_NO_COLLECT_MEMO=1 disables the memo. CollectWritesUncached always walks:
// for decisions about which blocks the CPU wrote while a GPU result was pending (a stale answer there
// stores old texels over reused memory).
// The epoch is per queue worker: BumpCollectEpoch advances the calling thread's epoch to a value no
// other thread ever holds (one worker's walk never satisfies another's check) at the hardware
// ordering points of its queue: the start of a submission, a WAIT_REG_MEM the CPU may have satisfied
// (at entry, polled, timed out, or by another queue's recorded label; a label this queue recorded
// itself orders no CPU write), a device drain, and before reaping completions outside a submission.
// A thread that never bumps (the presenter, game threads inside the flush hook) gets a fresh epoch
// for every collect: nothing is reused without an ordering point. APS5_PACKET_EPOCH=1 makes the
// workers bump before every packet as before. CollectEpochBumps counts the bumps ([guestmem] line).
void BumpCollectEpoch();
std::uint64_t CollectEpoch();
std::uint64_t CollectEpochBumps();
std::uint64_t ThreadCollectEpoch();
std::uint64_t UnwatchSerial();
std::uint64_t CollectWritesUncached(std::uint64_t address, std::size_t bytes);
// The tracker's current generation (every collect and MarkWritten bumps it): a stamp taken after
// a set of driver stores, for UnchangedSinceCollected to compare against later. Read under the
// tracker mutex, so every collect that bumped before it has finished its walk and every later
// collect stamps newer.
std::uint64_t TrackerGeneration();
// Whether no CPU write touched the range since `generation`: an uncached resetting collect of the
// range's pages first (a game store not collected yet becomes a block stamp), then the compare over
// the stamps collects make (dirty pages), not the MarkWritten stamps of the driver's own GPU label
// records, which share the title's label blocks. False when the arena is not write-watched.
bool UnchangedSinceCollected(std::uint64_t address, std::size_t bytes, std::uint64_t generation);
// Per 64 KiB tracker block of [address, address + bytes) (block 0 holds `address`): `changed[k]`
// receives whether the block was stamped after generations[k] (UnchangedSince over that block
// alone), `cpu[k]`, when given, whether a collect stamped it after (a CPU store, as
// UnchangedSinceCollected tells them apart from the driver's MarkWritten stamps, but without its
// walk: the caller collected the range first). One tracker lock for the whole range. Outside the
// watched arena, or for a generation of 0, a block reads as changed.
constexpr std::uint8_t BlockUnchanged = 0;
constexpr std::uint8_t BlockWritten = 1;
constexpr std::uint8_t BlockMaybeWritten = 2;
bool ChangedBlocks(std::uint64_t address, std::size_t bytes, std::span<const std::uint64_t> generations, std::span<std::uint8_t> changed, std::span<std::uint8_t> cpu = {});

// Serializes device work: draws, dispatches, presentation and the deferred write-backs below. The
// mutex is recursive; it is wrapped so every acquisition (std::lock_guard at any site) measures how
// long it waited: the graphics worker's frame is mostly spent waiting for this lock behind the
// compute workers, and that wait has to stay visible. Under APS5_PROFILE_DRAW each thread reports its
// waits in a [lock] line every 10 s (APS5_NO_LOCK_PROFILE=1 turns the timing off); a queue worker tags
// its thread with TagGpuLockThread so the line names the queue.
class GpuMutexType {
public:
    void lock();
    bool try_lock();
    void unlock();
    // Whether the calling thread holds the mutex (at any depth): the APS5_ASSERT_GPU_LOCK checks
    // (AssertGpuLockHeld) and code that must know whether it runs inside the device lock.
    bool HeldByThisThread() const;
    // The calling thread's recursion depth (0 when it does not hold the mutex): 1 means its
    // current acquisition is the outermost, so code that releases the mutex around a wait
    // (Recorder::SyncThrough for the flush hook) gives up no caller's hold.
    std::uint32_t DepthOnThisThread() const;

private:
    void acquired();
    std::recursive_mutex mutex;
    // The holder's thread token and recursion depth, written by the holder only; another thread
    // reads the owner just to see that it is not itself.
    std::atomic<const void*> owner{nullptr};
    std::uint32_t depth = 0;
};
GpuMutexType& GpuMutex();
// Called on the thread that just gave up its outermost hold, right after the mutex was released:
// work the hold deferred to run without it (the recorder's release of the objects completed batches
// kept). One hook, set once; it must not take GpuMutex itself.
void SetGpuUnlockHook(void (*hook)());
// Debug aid (APS5_ASSERT_GPU_LOCK=1): aborts with a [lock] line naming `where` when the calling
// thread does not hold GpuMutex. Recording, submits, command batches and the detiler's pools must
// only be reached under it (a resource build's stage A runs without it, see ShaderResources).
void AssertGpuLockHeld(const char* where);
void TagGpuLockThread(std::uint32_t queue);
// The presenter stays untagged (its tag is a queue id to the recorder) but takes its own column
// of the [lock] holder matrix, apart from the game threads.
void MarkPresenterThread();
// The queue tag of the calling thread (0xffffffff when untagged), for reports that name the thread.
std::uint32_t GpuLockThreadTag();
// Lock sites (APS5_PROFILE_DRAW): a caller names the site of its next lock() so the [lock] line
// splits acquisitions and waits per site (whose holds queue 0 waits behind is then measurable). The
// tag is consumed by that acquisition, so a nested lock inside the flush hook is attributed to the
// hook, and an acquisition nobody tagged counts as "other". Try-only acquisitions are not counted
// as acquisitions; the hold they start is timed under "try". Holds are timed per site as well (the
// outermost acquisition of a thread until its unlock; APS5_NO_HOLD_PROFILE=1 turns that off), so
// the line shows who holds the mutex, not only who waits for it.
enum class GpuLockSite : std::uint8_t { Other = 0, Dispatch, Indirect, Draw, Hook, Wait, Label, Flush, Present, Fill, Copy, End, Try, Count };
void TagGpuLockSite(GpuLockSite site);
// A GPU wait (a fence or timeline wait that found its work unfinished) made while the calling
// thread holds GpuMutex: counted on the thread's [lock] line as 'locked GPU waits' (APS5_PROFILE_DRAW).
void NoteLockedGpuWait(double ms);
// Bytes the calling thread's write-watch collects walked so far (0 without the memory profile):
// a caller reads the difference across a span of its own work.
std::uint64_t ThreadCollectedBytes();
// A code address (a return address) as an offset into its module, in the form of the [guestmem]
// callers (symbolized with nm against the driver's image). Takes the loader lock: for reports only.
unsigned long long CodeOffset(const void* address);

// Deferred GPU writes. The graphics layer keeps the results of storage images on the GPU until guest
// memory is read; it registers a hook that stores every pending result overlapping a range, which
// every read and store through this interface calls first. Code that reads guest memory directly
// calls FlushGpuWrites itself.
void SetFlushHook(void (*hook)(std::uint64_t address, std::size_t bytes));
void FlushGpuWrites(std::uint64_t address, std::size_t bytes);
// Moves with every ForgetPages call (the libc invalidator: memory unmapped or re-registered): odd
// while the call stores its page states, moved past it after, so a reader that loads an even
// value before consulting the page states and reading directly, and the same value after, knows
// the states it consulted were current and no page it read changed its mapping meanwhile.
std::uint64_t ForgetSerial();

// Attribution of guest memory accesses (the [hooksync] line in Recorder.cpp and the read-site
// counts of the [guestmem] line, APS5_PROFILE_DRAW): a sync the flush hook makes for an access has
// to be charged to the PM4 packet and the driver code that made the access. A queue worker names
// the packet it executes at the top of its packet loop (SetCurrentPacket; opcode 0xffff is the
// flip); a thread that runs no packets (the presenter, game threads) reads back NoPacket. The main
// read paths name themselves with a ReadSiteScope; an access made by none of them is attributed by
// its return addresses instead (CaptureCallerOffsets).
struct PacketTag {
    std::uint32_t opcode;
    std::uint32_t queue;
};
constexpr std::uint32_t NoPacket = 0xfffffffeu;
void SetCurrentPacket(std::uint32_t opcode, std::uint32_t queue);
PacketTag CurrentPacket();

// Store: a CPU write (Write/WriteChanged) rather than a read; a hook sync made for one orders a
// write after a pending GPU write, so whether the bytes change says nothing about its necessity.
// MirrorRefresh: an address-based build bringing an image mirror up to date with guest memory
// (GuestBufferMemory.cpp: the flush before a writable mirror's compare, a new mirror's fill).
// DrawCache: a draw entry's per-stage value validation (Driver.cpp drawCache), where a draw
// capture's hook waits reappear on a hit.
// Every enumerator needs its name in ReadSiteName's table (GuestMemory.cpp).
enum class ReadSite : std::uint8_t { Unknown = 0, Capture, DispatchCache, TextureCompare, TextureRead, BufferUpload, IndexBuffer, VertexBuffer, Registers, IndirectArguments, Wait, Label, Scanout, Store, MirrorRefresh, DrawCache, Count };
const char* ReadSiteName(ReadSite site);
// Sets the calling thread's read site and returns the previous one (nested scopes restore it).
ReadSite SetReadSite(ReadSite site);
ReadSite CurrentReadSite();
class ReadSiteScope {
public:
    explicit ReadSiteScope(ReadSite site) : previous(SetReadSite(site)) {}
    ~ReadSiteScope() { SetReadSite(previous); }
    ReadSiteScope(const ReadSiteScope&) = delete;
    ReadSiteScope& operator=(const ReadSiteScope&) = delete;

private:
    ReadSite previous;
};
// Return addresses above the caller as offsets into the driver's module (the same form as the
// [guestmem] callers, symbolized with nm against the driver's image; an address outside the module
// is written as 0), newest first, skipping `skip` frames above the caller; returns how many were
// written. Stack unwinding, so only for rare, profiled events.
std::size_t CaptureCallerOffsets(std::span<unsigned long long> frames, unsigned skip);

}

extern "C" void AgcDriverCheckGuestMemory_nid_postfix(const void* pointer, std::size_t bytes, std::size_t alignment, bool writable = false);

#endif
