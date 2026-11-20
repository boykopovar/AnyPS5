#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MapImportGate.hpp"
#include "prx/libSceAgcDriver/Graphics/include/AdaptiveStaging.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "ThreadOwned.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SnapshotCopies.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BdaResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ResidentMirror.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ReferenceStorage.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/UnitShadow.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libc/include/CpuTopology.hpp"
#include "prx/libc/include/HostThread.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libc/include/general/AtomicSharedPtr.hpp"
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#ifdef __APPLE__
struct udmabuf_create {
    std::uint32_t memfd;
    std::uint32_t flags;
    std::uint64_t offset;
    std::uint64_t size;
};
struct udmabuf_create_item {
    std::uint32_t memfd;
    std::uint32_t pad;
    std::uint64_t offset;
    std::uint64_t size;
};
struct udmabuf_create_list {
    std::uint32_t flags;
    std::uint32_t count;
    udmabuf_create_item list[];
};
constexpr std::uint32_t UDMABUF_FLAGS_CLOEXEC = 0x01;
constexpr unsigned long UDMABUF_CREATE = 0;
constexpr unsigned long UDMABUF_CREATE_LIST = 0;
#else
#include <linux/fs.h>
#include <linux/udmabuf.h>
#endif
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <array>
#include <vector>
#include <span>
#include <atomic>
#include <tuple>
#include <utility>
#include <functional>
#include <bit>
#include <condition_variable>
#include <deque>
#include <iterator>
#include <list>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <unordered_set>
#include <thread>
#include <stop_token>
#include <cerrno>
#include <cstdlib>
#include <cstdio>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>

namespace AgcDriver::Graphics {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
void GuestBufferMemory::completeSpaceWrites() const {
    auto* recorder = Recorder::Active();
    if (recorder == nullptr || Recorder::InCompletion() || !AnyPendingCpuWrites(context)) return;
    const auto& base = space->base;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> writes;
    std::vector<std::size_t> candidates;
    std::size_t next = 0;
    while (next < base.size()) {
        recorder->PendingWriteRanges(writes);
        OverlappedRanges(std::span<const Region>(base), next, writes, candidates);
        next = base.size();
        for (const auto index : candidates) {
            if (!CompletePendingCpuWrites(context, base[index].begin, static_cast<std::size_t>(base[index].end - base[index].begin))) continue;
            next = index + 1;
            break;
        }
    }
}

void GuestBufferMemory::UploadFinish(bool addressable, std::optional<std::pair<std::uint64_t, std::size_t>> htileClear) {
    Require(prepared && !uploaded, "guest memory upload was not prepared");
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    const auto started = std::chrono::steady_clock::now();
    if (addressable) StoreDepthExpandWrites(context);
    for (const auto& region : regions) CompletePendingCpuWrites(context, region.begin, static_cast<std::size_t>(region.end - region.begin));
    if (space != nullptr) completeSpaceWrites();
    uploaded = true;
    {
        auto& state = Mirrors();
        std::lock_guard lock(state.mutex);
        const auto stamp = ++state.dispatchCounter;
        for (const auto& region : regions) {
            if (region.mirror != nullptr) region.mirror->lastDispatch = stamp;
        }
        if (space != nullptr) {
            for (const auto& region : space->base) {
                if (region.mirror != nullptr) region.mirror->lastDispatch = stamp;
            }
        }
    }
    for (const auto& region : regions) static_cast<void>(FlushDepthPlanes(region.begin, static_cast<std::size_t>(region.end - region.begin)));
    for (const auto& [begin, end] : writes) RequireNoResidentDepthRead_nid_no_patch(begin, static_cast<std::size_t>(end - begin), htileClear);
    for (const auto& [begin, end] : writes) StorageTexture::FlushPending(begin, static_cast<std::size_t>(end - begin), nullptr, "written buffer range");
    if (addressable) {
        for (const auto& [begin, end] : reads) StorageTexture::FlushPending(begin, static_cast<std::size_t>(end - begin), nullptr, "imported buffer region");
    }
    if (context.hostImportAlignment != 0) {
        auto& state = Imports();
        std::lock_guard lock(state.mutex);
        state.buildFloor = importFloor != 0 ? importFloor : state.useCounter + 1;
    }
    // Registered ranges to import from. Address-based shaders hold their lease until write-back;
    // descriptor-only uploads acquire one only when the registry changed or an import must be made
    // (a lease copies every registered range's shared_ptr under the registry lock), so the guest can
    // keep managing memory.
    GuestAllocations::Lease acquired;
    const auto importRanges = [&]() -> const GuestAllocations::Lease& {
        if (!lease.empty()) return lease;
        if (space != nullptr) return space->lease;
        if (acquired.empty()) acquired = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
        return acquired;
    };
    // Sub-range mirrors were found by UploadPrepare, possibly long before this lock was taken: one
    // whose Range went meanwhile (the guest reprotected its image) is dropped as findMirror would
    // have dropped it, and the region takes the import or copy path below instead; a confirmed one
    // serves the region, so its snapshot goes. (Lease mirrors need no check: the lease pins their
    // Range.)
    for (auto& region : regions) {
        if (region.mirror == nullptr || !region.subrangeMirror) continue;
        if (region.mirror->range.expired()) {
            region.mirror = nullptr;
            region.subrangeMirror = false;
            region.pending = true;
            continue;
        }
        region.snapshot.clear();
        std::lock_guard lock(Mirrors().mutex);
        ++Mirrors().subranges;
    }
    bool importsRefreshed = false;
    if (context.hostImportAlignment != 0) {
        auto& state = Imports();
        std::lock_guard lock(state.mutex);
        if (importsStale(context, state)) refreshImports(context, state, importRanges());
        importsRefreshed = true;
        for (const auto& region : regions) {
            if (!region.pending || region.mirror != nullptr || region.direct != nullptr || sparseHeapCandidate(region)) continue;
            if (findImport(state, region.begin, region.end) != nullptr || runCovers(importRanges(), region.begin, region.end)) continue;
            static_cast<void>(importJoined(context, state, region.begin, region.end, importRanges()));
        }
    }
    if (space != nullptr && context.hostImportAlignment != 0) {
        // The space's regions were confirmed against the imports when it was built; one check that
        // none was retired since (a mirror refresh or a flush between the stages can reconcile
        // them). If one was, every region takes the per-build path below, which re-resolves each
        // pointer instead of trusting it.
        auto& state = Imports();
        std::lock_guard lock(state.mutex);
        if (importsStale(context, state)) refreshImports(context, state, importRanges());
        importsRefreshed = true;
        if (state.epoch != space->importsEpoch) {
            Spaces().dissolvedImports.fetch_add(1, std::memory_order_relaxed);
            dissolveSpace(true);
        }
    }
    // Regions the GPU copies out of their import (see Region::gpuCopy), recorded together below.
    // No recorder (tests) keeps them on the CPU path.
    auto* recorder = Recorder::Active();
    std::vector<Region*> gpuCopies;
    // One clock pair around the loop (an address-based build has ~1200 regions): the [buffers]
    // 'import lookup' is the loop less the mirror refreshes and the CPU copies, timed apart.
    const auto loopStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    std::uint64_t refreshUs = 0;
    std::uint64_t copyUs = 0;
    std::uint64_t heapsBegin = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t heapsEnd = 0;
    if (addressable && space != nullptr && !space->base.empty()) {
        heapsBegin = space->base.front().begin;
        heapsEnd = space->base.back().end;
    }
    for (auto& region : regions) {
        if (!region.pending) continue;
        region.pending = false;
        const auto bytes = region.end - region.begin;
        if (region.mirror != nullptr) {
            // A writable sub-range mirror: refreshed here, where waiting for recorded work is allowed.
            // Read-only mirrors are never written back; a writable one is diffed for this sub-range
            // at write-back.
            const auto refreshStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
            refreshMirror(*region.mirror, region.begin, bytes);
            if (profile) refreshUs += microsecondsSince(refreshStart);
            continue;
        }
        bool copyOnGpu = false;
        if (context.hostImportAlignment != 0) {
            auto& state = Imports();
            std::lock_guard lock(state.mutex);
            // Imports are made on demand for the registered range around each region.
            if (!importsRefreshed) {
                if (importsStale(context, state)) refreshImports(context, state, importRanges());
                importsRefreshed = true;
            }
            // An import taken by UploadPrepare (or at the lease) is still the registry's unless one
            // was dropped since.
            const HostImport* entry = region.direct != nullptr && state.epoch == importsEpoch ? region.direct : nullptr;
            region.direct = nullptr;
            if (entry == nullptr) entry = findImport(state, region.begin, region.end);
            const bool sparseRun = region.sparseHeap && region.sparse;
            if (entry == nullptr && !sparseRun) {
                entry = importAccess(context, state, region.begin, region.end, importRanges(), ImportRequester::Build);
            }
            bool runImport = false;
            if (entry == nullptr && sparseRun && !region.backed.empty() && region.backed.front().first == region.begin && !WritesOverlap(region.begin, static_cast<std::size_t>(bytes))) {
                const auto committedEnd = region.backed.back().second;
                const auto neededEnd = std::min(region.end, std::max(committedEnd, std::max(region.lastView, region.begin) + 4u));
                const auto covers = [&](const HostImport* candidate) { return candidate != nullptr && candidate->base <= region.begin && candidate->base + candidate->bytes >= neededEnd; };
                entry = findImport(state, region.begin, neededEnd);
                if (entry == nullptr && runCovers(importRanges(), region.begin, neededEnd)) entry = importAllocation(context, state, region.begin, importRanges(), ImportRequester::Build);
                if (!covers(entry)) entry = nullptr;
                const auto& lease = importRanges();
                const auto index = leasedIndexContaining(lease, region.begin);
                if (entry == nullptr && index != lease.size() && lease[index]->address == region.begin) {
                    const auto* low = peekImport(state, region.begin, region.begin + 1);
                    const auto joinBase = low != nullptr && low->joined ? low->base : region.begin;
                    const auto joins = state.sparseRunJoins.find(joinBase);
                    const auto made = joins == state.sparseRunJoins.end() ? 0u : joins->second;
                    const auto seen = state.sparseRunEnds.find(region.begin);
                    if (seen != state.sparseRunEnds.end() && seen->second == committedEnd && made < MaxSparseRunJoins) {
                        entry = importJoined(context, state, region.begin, neededEnd, lease);
                        if (covers(entry) && ++state.sparseRunJoins[entry->base] == MaxSparseRunJoins) std::fprintf(stderr, "[gpu] the sparse heap at 0x%llx was joined into one import %u times; it is not joined again and is copied once its run outgrows the import\n", static_cast<unsigned long long>(entry->base), MaxSparseRunJoins);
                    }
                    state.sparseRunEnds[region.begin] = committedEnd;
                }
                if (!covers(entry)) entry = nullptr;
                runImport = entry != nullptr;
            }
            // A staged region (see stagingEligible) is copied out of the import even when aligned;
            // without a recorder to record the copies it binds in place like any other.
            const bool staged = recorder != nullptr && stagingEligible(region, addressable);
            if (entry != nullptr && !staged && bindableInPlace(HostImportOffset(*entry, region.begin), addressable)) {
                region.direct = entry;
                region.runCut = runImport;
                region.snapshot.clear();
                // A buffer UploadPrepare made for a GPU copy is not needed: the import serves the
                // region in place (Descriptor prefers `direct`), and a kept buffer would count as copied.
                region.buffer.reset();
                region.deviceLocal = false;
            } else if (entry != nullptr && recorder != nullptr && (staged || gpuCopyEligible(region))) {
                // Misaligned in the import (or staged): the GPU copies the sub-range out of it. The
                // handle and base are taken now, under the import lock: a reconcile by the flush
                // below could retire the entry (the batch opened below keeps a retired import's
                // buffer alive).
                copyOnGpu = true;
                region.deviceLocal = staged;
                region.copySource = entry->buffer;
                const auto start = HostImportBufferStart(*entry);
                region.copySourceBase = start.guest;
                region.copySourceAddress = start.device;
            }
        }
        if (region.direct != nullptr) {
            // The GPU reads this range in place, so storage results pending in it must be stored first
            // (outside the import lock: the store may import memory itself). The whole registered heaps
            // an address-based shader maps are exempt (they hold every pending image, and such shaders
            // do not read texture memory through pointers); APS5_BDA_FLUSH=1 flushes for them too.
            static const bool bdaFlush = std::getenv("APS5_BDA_FLUSH") != nullptr;
            if (!(addressable && region.hostBacked) || bdaFlush) {
                StorageTexture::FlushPending(region.begin, static_cast<std::size_t>(bytes), nullptr, "imported buffer region");
            } else {
                heapsBegin = std::min(heapsBegin, region.begin);
                heapsEnd = std::max(heapsEnd, region.end);
            }
            continue;
        }
        if (copyOnGpu) {
            // As for a direct region, storage results pending in the range reach the import before
            // the copy reads it: the flush is recorded (GPU-direct into the import) ahead of the copy
            // in the same batch. The batch is opened first so that an import the flush's own
            // reconcile retires is handed to it (retireImport keeps nothing while the recorder is
            // idle) and outlives the copy.
            recorder->OpenBatch();
            StorageTexture::FlushPending(region.begin, static_cast<std::size_t>(bytes), nullptr, "copied buffer region");
            region.gpuCopy = true;
            gpuCopies.push_back(&region);
            continue;
        }
        if (sparseHeapCandidate(region)) {
            const auto refreshStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
            const bool bound = bindSparseMirror(region, importRanges());
            if (profile) refreshUs += microsecondsSince(refreshStart);
            if (bound) continue;
        }
        if (region.deviceLocal) {
            // The staging shadow UploadPrepare made cannot take the CPU copy (no host mapping): the
            // region falls back to a host buffer of its own.
            region.buffer.reset();
            region.deviceLocal = false;
        }
        const auto copyStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        copyRegion(region, addressable);
        if (profile) copyUs += microsecondsSince(copyStart);
    }
    if (addressable) StoreDepthExpandWrites(context);
    if (heapsEnd > heapsBegin) StorageTexture::PublishShadowsOnly(heapsBegin, static_cast<std::size_t>(heapsEnd - heapsBegin), PublishScope::Whole, "address-based heaps");
    if (!gpuCopies.empty()) recordGpuCopies(gpuCopies, addressable);
    takeHeapReferences();
    ResidentBuffer::FlushChanged(context);
    if (!profile) return;
    const auto loopUs = microsecondsSince(loopStart);
    readUs.fetch_add(refreshUs, std::memory_order_relaxed);
    importUs.fetch_add(loopUs > refreshUs + copyUs ? loopUs - refreshUs - copyUs : 0, std::memory_order_relaxed);
    const auto uploads = uploadsProfiled.fetch_add(1, std::memory_order_relaxed) + 1;
    if (uploads % 2000 == 0) {
        const auto& copies = Copies();
        AgcDriver::ProfilePrint_nid_no_patch("[buffers] %llu uploads: import lookup %.0f ms, buffer allocation %.0f ms, guest read %.0f ms; copies on the GPU: %llu regions (%.0f KiB) copied out of imports, %llu written sub-ranges (%.0f KiB) copied back, %llu staging stores by the CPU at write-back\n", static_cast<unsigned long long>(uploads), importUs.load(std::memory_order_relaxed) / 1000.0, allocateUs.load(std::memory_order_relaxed) / 1000.0, readUs.load(std::memory_order_relaxed) / 1000.0, static_cast<unsigned long long>(copies.gpuCopies.load(std::memory_order_relaxed)), copies.gpuCopyBytes.load(std::memory_order_relaxed) / 1024.0, static_cast<unsigned long long>(copies.gpuCopyBacks.load(std::memory_order_relaxed)), copies.gpuCopyBackBytes.load(std::memory_order_relaxed) / 1024.0, static_cast<unsigned long long>(copies.stagingStores.load(std::memory_order_relaxed)));
    }
    const auto ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    if (ms > 50) {
        std::uint64_t total = 0;
        std::uint64_t largest = 0;
        std::uint64_t copied = 0;
        for (const auto& region : regions) {
            total += region.end - region.begin;
            largest = std::max<std::uint64_t>(largest, region.end - region.begin);
            if (region.buffer != nullptr) copied += region.end - region.begin;
        }
        AgcDriver::ProfilePrint_nid_no_patch("[resources] guest upload of %zu regions, %.1f MiB (largest %.1f MiB, copied %.1f MiB, addressable %d) took %.0f ms to finish\n", regions.size(), total / 1048576.0, largest / 1048576.0, copied / 1048576.0, addressable ? 1 : 0, ms);
    }
    // Debug aid: APS5_TRACE_BIGBUF lists every copied region of at least 1 MiB.
    static const bool traceBig = std::getenv("APS5_TRACE_BIGBUF") != nullptr;
    if (traceBig) {
        for (const auto& region : regions) {
            if (region.buffer != nullptr && region.end - region.begin >= (1u << 20u)) std::fprintf(stderr, "[bigbuf] upload 0x%llx+0x%llx writable=%d hostBacked=%d sparse=%d addressable=%d\n", static_cast<unsigned long long>(region.begin), static_cast<unsigned long long>(region.end - region.begin), region.writable ? 1 : 0, region.hostBacked ? 1 : 0, region.sparse ? 1 : 0, addressable ? 1 : 0);
        }
    }
}

void GuestBufferMemory::copyRegion(Region& region, bool addressable) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    const auto bytes = region.end - region.begin;
    const auto usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | (addressable ? VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT : 0u);
    std::uint64_t readBytes = 0;
    bool inImport = false;
    if (profile && context.hostImportAlignment != 0) {
        auto& state = Imports();
        std::lock_guard lock(state.mutex);
        inImport = peekImport(state, region.begin, region.end) != nullptr;
    }
    const auto allocateStart = std::chrono::steady_clock::now();
    if (region.sparse && region.buffer == nullptr && (region.hostBacked || region.writable)) {
        const auto backedEnd = region.backed.empty() ? region.begin : region.backed.back().second;
        const auto viewEnd = std::max(region.lastView, region.begin) + 4u;
        constexpr std::uint64_t granule = 256;
        const auto extent = std::max(backedEnd, viewEnd) - region.begin;
        const auto end = region.begin + std::min<std::uint64_t>((extent + granule - 1) / granule * granule, bytes);
        region.bufferEnd = end < region.end ? end : 0;
    }
    const auto allocated = region.bufferEnd != 0 ? region.bufferEnd - region.begin : bytes;
    const bool written = region.writable && WritesOverlap(region.begin, static_cast<std::size_t>(bytes));
    const bool cacheable = region.buffer == nullptr && (region.hostBacked || region.writable) && !written;
    std::uint64_t uploadGeneration = 0;
    if (cacheable) {
        if (auto cached = findUploaded(context, region.begin, region.sparse, region.backed, static_cast<std::size_t>(allocated), usage, uploadGeneration)) {
            bool unchanged = true;
            {
                const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::BufferUpload);
                if (const auto hook = GuestMemory::CurrentFlushHook()) {
                    if (!region.sparse) {
                        hook(region.begin, static_cast<std::size_t>(allocated));
                    } else {
                        for (const auto& [first, last] : region.backed) hook(first, static_cast<std::size_t>(last - first));
                    }
                    unchanged = uploadUnchanged(region.begin, region.sparse, region.backed, static_cast<std::size_t>(allocated), uploadGeneration);
                }
            }
            if (unchanged) {
                region.buffer = std::move(cached);
                region.snapshot.clear();
                static std::atomic<std::uint64_t> hits{0}, hitBytes{0};
                const auto hit = hits.fetch_add(1, std::memory_order_relaxed) + 1;
                hitBytes.fetch_add(allocated, std::memory_order_relaxed);
                if (profile && hit % 1000 == 0) {
                    AgcDriver::ProfilePrint_nid_no_patch("[buffers] %llu upload cache hits, %.0f MiB served\n", static_cast<unsigned long long>(hit), static_cast<double>(hitBytes.load(std::memory_order_relaxed)) / 1048576.0);
                }
                return;
            }
            forgetUploaded(context, region.begin, static_cast<std::size_t>(allocated), usage);
        }
        uploadGeneration = collectUploadWrites(region.begin, region.sparse, region.backed, static_cast<std::size_t>(allocated));
    } else if (written) {
        forgetUploaded(context, region.begin, static_cast<std::size_t>(allocated), usage);
    }
    // A buffer made by UploadPrepare for a GPU copy that fell back here (its import went) is kept.
    const bool shared = region.buffer == nullptr && !region.hostBacked && !region.writable && !region.sparse && region.bufferEnd == 0 && region.snapshot.size() == bytes;
    if (shared) region.buffer = snapshotCopy(context, region.begin, region.snapshot, usage);
    else if (region.buffer == nullptr) region.buffer = std::make_shared<Buffer>(context, static_cast<std::size_t>(allocated), usage);
    const auto readStart = std::chrono::steady_clock::now();
    if (profile) allocateUs.fetch_add(microsecondsSince(allocateStart), std::memory_order_relaxed);
    // Named for the [hooksync] attribution: the reads below go through the flush hook.
    const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::BufferUpload);
    if (region.hostBacked || region.writable) {
        // Only ranges a descriptor may write need the copy the write-back diffs against; the
        // registered ranges of an address-based shader are read-only to it (AddressRanges).
        if (!region.sparse) {
            GuestMemory::Read(region.begin, region.buffer->Bytes());
            readBytes += region.buffer->Bytes().size();
            if (written) region.uploaded.assign(region.buffer->Bytes().begin(), region.buffer->Bytes().end());
        } else {
            // `uploaded` holds the backed pieces back to back.
            const auto target = region.buffer->Bytes().first(static_cast<std::size_t>(allocated));
            std::size_t cursor = 0;
            for (const auto& [first, last] : region.backed) {
                const auto at = static_cast<std::size_t>(first - region.begin);
                if (at > cursor) std::memset(target.data() + cursor, 0, at - cursor);
                const auto piece = target.subspan(at, static_cast<std::size_t>(last - first));
                GuestMemory::Read(first, piece);
                readBytes += piece.size();
                if (written) region.uploaded.insert(region.uploaded.end(), piece.begin(), piece.end());
                cursor = at + piece.size();
            }
            if (cursor < target.size()) std::memset(target.data() + cursor, 0, target.size() - cursor);
        }
    }
    else {
        if (!shared) std::memcpy(region.buffer->Bytes().data(), region.snapshot.data(), region.snapshot.size());
        readBytes += region.snapshot.size();
    }
    region.snapshot.clear();
    if (uploadGeneration != 0 && uploadUnchanged(region.begin, region.sparse, region.backed, static_cast<std::size_t>(allocated), uploadGeneration)) insertUploaded(context, region.begin, region.backed, static_cast<std::size_t>(allocated), usage, region.buffer, uploadGeneration);
    if (profile) readUs.fetch_add(microsecondsSince(readStart), std::memory_order_relaxed);
    if (profile) {
        static AgcDriver::Mutex reasonsMutex;
        static std::uint64_t uploads = 0, noImport = 0, noImportBytes = 0, misaligned = 0, misalignedBytes = 0, snapshotOnly = 0, snapshotBytes = 0;
        static std::uint64_t logicalBytes = 0, readTotalBytes = 0;
        static std::map<std::uint64_t, std::pair<std::uint64_t, std::uint64_t>> outside;
        std::lock_guard lock(reasonsMutex);
        const auto bufferBytes = region.buffer->Bytes().size();
        logicalBytes += bytes;
        readTotalBytes += readBytes;
        if (!region.hostBacked && !region.writable) { ++snapshotOnly; snapshotBytes += bufferBytes; }
        else if (inImport) { ++misaligned; misalignedBytes += bufferBytes; }
        else {
            ++noImport;
            noImportBytes += bufferBytes;
            auto& bucket = outside[region.begin >> 28u];
            ++bucket.first;
            bucket.second += bufferBytes;
        }
        if (++uploads % 1000 == 0) {
            AgcDriver::ProfilePrint_nid_no_patch("[buffers] %llu completed CPU-copy operations: logical spans %.0f MiB, buffer spans %.0f MiB, bytes read %.0f MiB; %llu snapshots (%.0f MiB), %llu misaligned in imports (%.0f MiB), %llu outside imports (%.0f MiB):", static_cast<unsigned long long>(uploads), logicalBytes / 1048576.0, (snapshotBytes + misalignedBytes + noImportBytes) / 1048576.0, readTotalBytes / 1048576.0, static_cast<unsigned long long>(snapshotOnly), snapshotBytes / 1048576.0, static_cast<unsigned long long>(misaligned), misalignedBytes / 1048576.0, static_cast<unsigned long long>(noImport), noImportBytes / 1048576.0);
            for (const auto& [granule, counts] : outside) AgcDriver::ProfilePrint_nid_no_patch(" 0x%llx0000000:%llu/%.0fMiB", static_cast<unsigned long long>(granule), static_cast<unsigned long long>(counts.first), counts.second / 1048576.0);
            AgcDriver::ProfilePrint_nid_no_patch("\n");
        }
    }
}

namespace {

std::atomic<std::uint64_t> changedCopySnapshotBytes{0};
std::atomic<std::uint64_t> changedCopySnapshotsMade{0};

}
}
}
}
}
}
}
}
}
