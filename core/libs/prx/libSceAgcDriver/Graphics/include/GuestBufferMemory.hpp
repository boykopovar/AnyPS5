#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GUESTBUFFERMEMORY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GUESTBUFFERMEMORY_HPP

#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "BdaAbi.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics {

struct GuestMemorySnapshot {
    std::uint64_t address;
    std::span<const std::byte> bytes;
};

// A guest allocation imported with VK_EXT_external_memory_host, usable by the GPU in place.
struct HostImport {
    std::uint64_t base;
    std::uint64_t bytes;
    VkBuffer buffer;
    VkDeviceMemory memory;
    VkDeviceAddress address;
    // Identity for the life of this import (see HostImportSerial); 0 until first asked for.
    std::uint64_t serial = 0;
};

// The host import of the registered allocation containing [address, address + bytes), made on demand
// (alignment and budget permitting), or null. Bytes at `address` are at `address - import->base` in
// the import's buffer.
const HostImport* HostImportFor(const Context& context, std::uint64_t address, std::size_t bytes);
// Whether an existing import covers [address, address + bytes), without reconciling the imports
// with the registry or making one (HostImportFor may take a registry lease): a hint for choices
// made outside the device lock (a sampled texture's path, a dispatch's pre-sync); the path taken
// re-checks with HostImportFor when it binds the import.
bool HostImportCovers(const Context& context, std::uint64_t address, std::size_t bytes);

// A persistent device copy of one registered range of the main guest image (which cannot be host
// imported); see GuestBufferMemory.cpp.
struct ImageMirror;
class Recorder;

// Identity of the import serving [address, address + bytes): a serial unique for the life of one
// import, 0 when no import serves the range. An equal serial later means the same VkBuffer still
// backs the range (a re-import gets a new serial), which is what a descriptor set written against it
// needs to stay valid (see the ShaderResources cache). With `reconcile`, imports whose registered
// range changed are dropped first, as an upload does; without it (right after an upload, before the
// work using the import is recorded) the imports are left as they are.
std::uint64_t HostImportSerial(const Context& context, std::uint64_t address, std::size_t bytes, bool reconcile);

// Identity of the read-only image mirror serving [address, address + bytes) (the counterpart of
// HostImportSerial for main-image ranges, which no import serves): a serial unique for the life of
// one mirror with its top bit set, 0 when no read-only mirror whose registered range still exists
// covers the range. A read-only mirror's bytes and VkBuffer never change, so a descriptor set written
// against it stays valid while the serial is unchanged.
std::uint64_t ImageMirrorSerial(const Context& context, std::uint64_t address, std::size_t bytes);

// Deferred lease release (see GuestBufferMemory.cpp). An address-based build leases every readable
// registered allocation until its write-back, which runs when its batch completed; the guest's
// unmap/mprotect/free of a leased allocation waits for that through the registry's pin waiter, which
// submits the recorder and finishes its batches up to the newest one recorded with a lease. SyncLeaseWork
// says whether the build's work must instead be synced as soon as it is recorded
// (APS5_SYNC_LEASE_DISPATCH=1, the behaviour before deferral). The driver and the draw path count
// their choice with CountLeaseOutcome right after the lease-holding resources were kept by the open
// batch: `batchSerial` is that batch's serial (Recorder::Submissions() + 1 under the device lock), 0
// when synced, and the waiter targets the newest of them. Under APS5_PROFILE_DRAW CountLeaseOutcome
// prints the [address-sync] leases line every 10 s from LeaseCounters (cumulative).
bool SyncLeaseWork();
void CountLeaseOutcome(bool synced, std::uint64_t batchSerial);
struct LeaseStats {
    std::uint64_t deferred = 0;
    std::uint64_t synced = 0;
    // Pin-contention waits made by guest threads (the waiter calls), how many of them finished the
    // newest lease batch, how many had to drain the whole recorder instead (no lease batch noted),
    // and their total time.
    std::uint64_t contentionWaits = 0;
    std::uint64_t contentionSyncs = 0;
    std::uint64_t contentionDrains = 0;
    double contentionMs = 0;
};
LeaseStats LeaseCounters();

class GuestBufferMemory {
public:
    explicit GuestBufferMemory(const Context& context);
    void AcquireRegistered();
    // A guest range bound through a descriptor. Both read live guest memory at upload and bind the
    // same way (a storage buffer); AddWritable also notes the range in Writes(), so it gets the
    // write-back's reference copy, a write-back, the recorder's pending-write note and the
    // direct-write marks. AddReadable is for an element the shader is proved never to store to
    // (DescriptorBinding::bufferWritten): the CPU never has to wait for it.
    void AddWritable(std::uint64_t address, std::size_t bytes);
    void AddReadable(std::uint64_t address, std::size_t bytes);
    void AddSnapshot(const GuestMemorySnapshot& snapshot);
    // Upload is the two stages below back to back. UploadPrepare needs no device lock: it merges the
    // regions, binds the image mirrors and host imports that already serve them (an import pointer is
    // re-checked against the registry epoch later) and copies read-only regions. UploadFinish runs
    // under GuestMemory::GpuMutex: it reconciles and makes imports (retiring one hands its buffer to
    // the recorder), refreshes writable mirrors and flushes pending results (both may wait for
    // recorded work) and copies the regions a descriptor may write, so the window between that copy
    // and the recorder's pending-write note stays closed.
    void Upload(bool addressable);
    void UploadPrepare(bool addressable);
    void UploadFinish(bool addressable);
    VkDescriptorBufferInfo Descriptor(std::uint64_t address, std::size_t bytes) const;
    std::vector<ShaderRecompiler::BdaAbi::Range> AddressRanges() const;
    void WriteBack();
    bool WritesOverlap(std::uint64_t address, std::size_t bytes) const;
    // Guest ranges the shader may write through descriptors, as [begin, end).
    const std::vector<std::pair<std::uint64_t, std::uint64_t>>& Writes() const { return writes; }
    // Whether any written range lives in a copied buffer, so a write-back must run once the GPU is done.
    bool HasCopiedWrites() const;
    // Reports writes into host-imported memory (made by the GPU in place, or copied back into it by
    // RecordCopyBacks) to the write tracking now.
    void MarkDirectWrites() const;
    // Records, into the recorder's open batch, the copy of every written sub-range of a region the
    // GPU copied out of a host import (see Region::gpuCopy) back into the import: what the shader
    // wrote lands in guest memory by the GPU, ordered after the recorded work, so the region needs
    // no CPU write-back (HasCopiedWrites no longer counts it) and the caller notes the ranges as
    // pending writes and marks them as MarkDirectWrites does. Called right after the work using the
    // regions was recorded (ShaderResources::MarkGpuWrites), under GuestMemory::GpuMutex. A use that
    // never calls it (a synchronous draw) stores the staging bytes from the CPU in WriteBack.
    void RecordCopyBacks(Recorder& recorder);
    // Whether registered allocations are pinned until write-back (address-based shaders).
    bool HoldsLease() const { return !lease.empty(); }
    // Every uploaded region as [begin, end) when all of them are served by host imports (nothing was
    // copied, so the upload can serve a later identical build), else nothing.
    std::optional<std::vector<std::pair<std::uint64_t, std::uint64_t>>> DirectRegions() const;

private:
    struct Region {
        std::uint64_t begin;
        std::uint64_t end;
        bool writable;
        std::vector<std::byte> snapshot;
        // Shared so a recorded GPU copy into it (see gpuCopy) can keep it in the batch itself, before
        // the caller keeps the whole resources: a throw between the two would otherwise return it to
        // the pool under an unsubmitted copy command.
        std::shared_ptr<Buffer> buffer;
        // Guest bytes as uploaded; write-back only stores bytes the GPU changed.
        std::vector<std::byte> uploaded {};
        // Registered allocation that is imported: its bytes are read from live guest memory, not a snapshot.
        bool hostBacked = false;
        // Set when the region is served by an imported allocation; nothing is copied or written back.
        const HostImport* direct = nullptr;
        // Set when the region covers uncommitted pages: only the `backed` parts (possibly none) are guest
        // memory; the rest reads as zeros and is never stored.
        bool sparse = false;
        std::vector<std::pair<std::uint64_t, std::uint64_t>> backed {};
        // Set when the region is served by an image mirror: nothing is copied; writable mirrors are
        // written back by comparing with the mirror's shadow. Kept alive here for recorded work.
        std::shared_ptr<ImageMirror> mirror {};
        // The mirror is a descriptor sub-range one found by UploadPrepare (not a lease mirror), which
        // UploadFinish confirms still has its Range before the region binds it.
        bool subrangeMirror = false;
        // Set by UploadPrepare when UploadFinish still has device-lock work for the region: a copy a
        // descriptor writes, a writable mirror's refresh, an import to reconcile or make, or the
        // pending-results flush of an import it took.
        bool pending = false;
        // The region lies inside a host import but is misaligned for binding in place: `buffer` is
        // filled by a vkCmdCopyBuffer from the import recorded into the batch (no CPU read, so no
        // flush-hook wait), from `copySource` (the import's VkBuffer, alive until the batch
        // completed, with the guest address of its first byte). `copiedBack` once RecordCopyBacks
        // recorded the written sub-ranges' copies back into the import.
        bool gpuCopy = false;
        VkBuffer copySource = VK_NULL_HANDLE;
        std::uint64_t copySourceBase = 0;
        bool copiedBack = false;
    };

    void validate(std::uint64_t address, std::size_t bytes) const;
    // The region of a descriptor-bound range (AddWritable/AddReadable), committed pages only.
    void addDescriptorRegion(std::uint64_t address, std::size_t bytes);
    // Gives a region a buffer of its own with its bytes (guest memory for host-backed and writable
    // ranges, plus the write-back's reference copy for a range a descriptor writes; else its snapshot).
    void copyRegion(Region& region, bool addressable);
    // Whether a region inside a host import that cannot be bound in place is copied by the GPU
    // instead of the CPU (see Region::gpuCopy): live guest bytes, not sparse, at most the size
    // APS5_GPU_COPY_MAX_KIB allows, and APS5_CPU_COPIES unset.
    bool gpuCopyEligible(const Region& region) const;
    // Records the import-to-buffer copies of the given gpuCopy regions into the open batch, with
    // the barriers that order them after earlier recorded writes and before the shaders reading them.
    void recordGpuCopies(std::span<Region* const> copies, bool addressable);
    Context context;
    GuestAllocations::Lease lease;
    // Import registry epoch when `direct` pointers were taken at acquire time; they are reused while
    // no import was destroyed since.
    std::uint64_t importsEpoch = 0;
    std::vector<Region> regions;
    // Whether `regions` is in ascending address order (true right after AcquireRegistered, whose
    // regions follow the registry's order), so AddSnapshot can search instead of scanning.
    bool regionsSorted = false;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> writes;
    // UploadPrepare ran (regions are frozen); `uploaded` once UploadFinish ran.
    bool prepared = false;
    bool uploaded = false;
    bool committed = false;
};

}

#endif
