#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BdaResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#ifdef _WIN32
#include <windows.h>
#endif
#include <atomic>
#include <bit>
#include <condition_variable>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#include <cstdlib>
#include <cstdio>
#include <chrono>
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace AgcDriver::Graphics {

// Image mirrors. The main guest image (the PE at 0x7ff6b...) is MEM_IMAGE memory, which
// VK_EXT_external_memory_host refuses, and address-based shaders map every registered range, so each
// of their builds copied the whole image (~55 MiB: ~40 MiB of read-only sections snapshotted and copied
// again, ~15 MiB of writable ones copied twice). Instead one host-cached device buffer per registered
// image range persists here. Read-only ranges are filled once: their pages change only through an
// mprotect, which replaces the registered Range object, and a mirror whose Range is gone is rebuilt.
// Writable ranges keep a shadow of the guest bytes and copy the 64 KiB blocks that differ on each use.
// A mirror never holds the Range shared_ptr: a pinned range makes the guest's mprotect spin and fail.
struct ImageMirror {
    std::uint64_t base = 0;
    std::uint64_t bytes = 0;
    bool writable = false;
    std::weak_ptr<const GuestAllocations::Range> range;
    std::shared_ptr<Buffer> buffer;
    // Writable ranges only: the guest bytes the mirror holds, compared on refresh and at write-back.
    std::vector<std::byte> shadow;
    // Identity for the life of this mirror (see ImageMirrorSerial); the top bit keeps mirror serials
    // apart from host import serials.
    std::uint64_t serial = 0;
};

namespace {

// Imports persist across draws, keyed by allocation base, and are dropped when their allocation leaves
// the registered set; a dropped import is destroyed once the recorded work that may read it completed.
struct HostImports {
    std::mutex mutex;
    VkDevice device = VK_NULL_HANDLE;
    std::map<std::uint64_t, HostImport> imports;
    std::set<std::uint64_t> failed;
    // Registry generation the imports were last reconciled with.
    std::uint64_t refreshedGeneration = 0;
    // Bumped whenever an import is dropped, so HostImport pointers taken under the lock earlier can be
    // reused while it is unchanged.
    std::uint64_t epoch = 1;
};

HostImports& Imports() {
    static HostImports imports;
    return imports;
}

void destroyImport(const Context& context, const HostImport& entry) {
    context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(context.device, entry.buffer, nullptr);
    context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, entry.memory, nullptr);
}

// Frees a dropped import's Vulkan objects when it is released. Never copied: a copy would destroy the
// same handles twice (and a temporary would destroy them at once).
struct RetiredImport {
    RetiredImport(const Context& context, const HostImport& entry) : context(context), entry(entry) {}
    RetiredImport(const RetiredImport&) = delete;
    RetiredImport& operator=(const RetiredImport&) = delete;
    ~RetiredImport() { destroyImport(context, entry); }
    Context context;
    HostImport entry;
};

// Drops an import. Recorded batches (dispatches, recorded draws, GPU label writes) may still read it,
// so the objects live until the batch open now completed; with no batches in flight all GPU work that
// used it was synchronous and it is destroyed at once.
void retireImport(const Context& context, HostImports& state, std::map<std::uint64_t, HostImport>::iterator it) {
    auto holder = std::make_shared<RetiredImport>(context, it->second);
    if (auto* recorder = Recorder::Active(); recorder != nullptr && !recorder->Idle()) recorder->Keep(std::move(holder));
    ++state.epoch;
    state.imports.erase(it);
}

const HostImport* importAllocation(const Context& context, HostImports& state, std::uint64_t base, std::uint64_t bytes) {
    if (const auto found = state.imports.find(base); found != state.imports.end()) {
        if (found->second.bytes == bytes) return &found->second;
        retireImport(context, state, found);
    }
    const auto alignment = context.hostImportAlignment;
    if (alignment == 0 || base % alignment != 0 || bytes % alignment != 0 || state.failed.contains(base)) return nullptr;
    // Pinned imports count against the driver's system memory budget; past it ordinary host
    // allocations fail, so imports stop at APS5_HOST_IMPORT_MIB (default 6 GiB, which covers the
    // registered memory of address-based shaders; past it they copy gigabytes per dispatch).
    static const std::uint64_t budget = [] {
        const char* value = std::getenv("APS5_HOST_IMPORT_MIB");
        return (value ? std::strtoull(value, nullptr, 10) : 6144ull) << 20u;
    }();
    std::uint64_t live = 0;
    for (const auto& [address, existing] : state.imports) live += existing.bytes;
    if (live + bytes > budget) {
        // A refused import turns every later use of the range into CPU copies, so say so.
        static std::uint64_t refused = 0, refusedBytes = 0;
        static auto lastReport = std::chrono::steady_clock::now() - std::chrono::seconds(60);
        ++refused;
        refusedBytes += bytes;
        if (std::chrono::steady_clock::now() - lastReport > std::chrono::seconds(10)) {
            lastReport = std::chrono::steady_clock::now();
            std::fprintf(stderr, "[gpu] host import budget: %llu MiB live of %llu MiB (APS5_HOST_IMPORT_MIB); %llu imports (%llu MiB) refused so far, last 0x%llx+0x%llx\n", static_cast<unsigned long long>(live >> 20u), static_cast<unsigned long long>(budget >> 20u), static_cast<unsigned long long>(refused), static_cast<unsigned long long>(refusedBytes >> 20u), static_cast<unsigned long long>(base), static_cast<unsigned long long>(bytes));
        }
        return nullptr;
    }
#ifdef _WIN32
    // Drivers pin imported pages, so every page must be committed and accessible.
    for (std::uint64_t cursor = base; cursor < base + bytes;) {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &info, sizeof(info)) == 0 || info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
            state.failed.insert(base);
            return nullptr;
        }
        cursor = reinterpret_cast<std::uint64_t>(info.BaseAddress) + info.RegionSize;
    }
#endif
    HostImport entry{base, bytes, VK_NULL_HANDLE, VK_NULL_HANDLE, 0};
    const auto fail = [&](const char* step, VkResult result) -> const HostImport* {
        if (entry.buffer) context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(context.device, entry.buffer, nullptr);
        if (entry.memory) context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, entry.memory, nullptr);
        state.failed.insert(base);
        std::fprintf(stderr, "[gpu] host import of 0x%llx+0x%llx failed at %s (%d); falling back to copies\n", static_cast<unsigned long long>(base), static_cast<unsigned long long>(bytes), step, static_cast<int>(result));
#ifdef _WIN32
        static const bool trace = std::getenv("APS5_TRACE_HOST_IMPORT") != nullptr;
        for (std::uint64_t cursor = base; trace && cursor < base + bytes;) {
            MEMORY_BASIC_INFORMATION info{};
            if (VirtualQuery(reinterpret_cast<const void*>(cursor), &info, sizeof(info)) == 0) break;
            const auto regionEnd = reinterpret_cast<std::uint64_t>(info.BaseAddress) + info.RegionSize;
            std::fprintf(stderr, "[gpu]   0x%llx+0x%llx state 0x%lx protect 0x%lx type 0x%lx allocation 0x%llx\n", static_cast<unsigned long long>(cursor), static_cast<unsigned long long>(std::min(regionEnd, base + bytes) - cursor), info.State, info.Protect, info.Type, reinterpret_cast<unsigned long long>(info.AllocationBase));
            cursor = regionEnd;
        }
#endif
        return nullptr;
    };
    const VkExternalMemoryBufferCreateInfo external{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO, nullptr, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT};
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, &external};
    info.size = bytes;
    // INDIRECT_BUFFER: DISPATCH_INDIRECT group counts are read in place (VulkanDevice::DispatchIndirect).
    info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (const auto result = context.Function<PFN_vkCreateBuffer>("vkCreateBuffer")(context.device, &info, nullptr, &entry.buffer); result != VK_SUCCESS) return fail("vkCreateBuffer", result);
    VkMemoryHostPointerPropertiesEXT pointer{VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT};
    if (const auto result = context.Function<PFN_vkGetMemoryHostPointerPropertiesEXT>("vkGetMemoryHostPointerPropertiesEXT")(context.device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, reinterpret_cast<const void*>(base), &pointer); result != VK_SUCCESS) return fail("vkGetMemoryHostPointerPropertiesEXT", result);
    VkMemoryRequirements requirements{};
    context.Function<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(context.device, entry.buffer, &requirements);
    const auto types = requirements.memoryTypeBits & pointer.memoryTypeBits;
    if (types == 0) return fail("memory type selection", VK_ERROR_FORMAT_NOT_SUPPORTED);
    const VkImportMemoryHostPointerInfoEXT import{VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT, nullptr, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, reinterpret_cast<void*>(base)};
    const VkMemoryAllocateFlagsInfo flags{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, &import, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, 0};
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &flags};
    allocation.allocationSize = bytes;
    allocation.memoryTypeIndex = static_cast<std::uint32_t>(std::countr_zero(types));
    if (const auto result = context.Function<PFN_vkAllocateMemory>("vkAllocateMemory")(context.device, &allocation, nullptr, &entry.memory); result != VK_SUCCESS) return fail("vkAllocateMemory", result);
    if (const auto result = context.Function<PFN_vkBindBufferMemory>("vkBindBufferMemory")(context.device, entry.buffer, entry.memory, 0); result != VK_SUCCESS) return fail("vkBindBufferMemory", result);
    const VkBufferDeviceAddressInfo addressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, entry.buffer};
    entry.address = context.Function<PFN_vkGetBufferDeviceAddressKHR>("vkGetBufferDeviceAddressKHR")(context.device, &addressInfo);
    if (entry.address == 0) return fail("vkGetBufferDeviceAddressKHR", VK_ERROR_UNKNOWN);
    static std::uint64_t importedBytes = 0;
    importedBytes += bytes;
    static const bool trace = std::getenv("APS5_TRACE_HOST_IMPORT") != nullptr;
    std::uint64_t liveBytes = bytes;
    for (const auto& [address, existing] : state.imports) liveBytes += existing.bytes;
    if (trace) std::fprintf(stderr, "[gpu] host import of 0x%llx+0x%llx ok (%zu live, %.1f MiB live, %.1f MiB ever)\n", static_cast<unsigned long long>(base), static_cast<unsigned long long>(bytes), state.imports.size() + 1, liveBytes / 1048576.0, importedBytes / 1048576.0);
    return &state.imports.emplace(base, entry).first->second;
}

// The lease is in address order (it is built from the registry map), so a range is found by binary search.
const GuestAllocations::Range* leasedRangeAt(const GuestAllocations::Lease& lease, std::uint64_t base) {
    const auto found = std::lower_bound(lease.begin(), lease.end(), base, [](const auto& range, std::uint64_t value) { return range->address < value; });
    return found != lease.end() && (*found)->address == base ? found->get() : nullptr;
}

// The readable registered range containing [begin, end), or null.
const GuestAllocations::Range* containingRange(const GuestAllocations::Lease& lease, std::uint64_t begin, std::uint64_t end) {
    const auto found = std::upper_bound(lease.begin(), lease.end(), begin, [](std::uint64_t value, const auto& range) { return value < range->address; });
    if (found == lease.begin()) return nullptr;
    const auto& range = *std::prev(found);
    return range->readable && begin >= range->address && end <= range->address + range->bytes ? range.get() : nullptr;
}

// Drops imports whose registered range changed or disappeared: their pages may no longer back the
// guest addresses. Walks the imports only when the registry changed since the last walk.
void refreshImports(const Context& context, HostImports& state, const GuestAllocations::Lease& lease) {
    if (state.device != context.device) {
        state.imports.clear();
        state.failed.clear();
        state.device = context.device;
        state.refreshedGeneration = 0;
        ++state.epoch;
    }
    const auto generation = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    if (generation == state.refreshedGeneration) return;
    state.refreshedGeneration = generation;
    for (auto it = state.imports.begin(); it != state.imports.end();) {
        const auto* range = leasedRangeAt(lease, it->first);
        if (range != nullptr && range->bytes == it->second.bytes) {
            ++it;
            continue;
        }
        const auto next = std::next(it);
        retireImport(context, state, it);
        it = next;
    }
    for (auto it = state.failed.begin(); it != state.failed.end();) it = leasedRangeAt(lease, *it) != nullptr ? std::next(it) : state.failed.erase(it);
}

const HostImport* findImport(HostImports& state, std::uint64_t begin, std::uint64_t end) {
    auto found = state.imports.upper_bound(begin);
    if (found == state.imports.begin()) return nullptr;
    --found;
    const auto& entry = found->second;
    return begin >= entry.base && end <= entry.base + entry.bytes ? &entry : nullptr;
}

// Whether the imports need reconciling before a lookup: the registry changed since the last walk.
bool importsStale(const Context& context, const HostImports& state) {
    return state.device != context.device || GuestAllocations::GuestAllocationsGeneration_nid_postfix() != state.refreshedGeneration;
}

// The mirror registry: by range base. The map is protected by `mutex` (findMirror runs in a build's
// unlocked stage A, UploadPrepare); the mirrors' contents (fills, refreshes, sweeps) change under
// GuestMemory::GpuMutex only (every completion and every stage B holds it). APS5_NO_LEASE_MIRROR=1
// restores copies.
struct ImageMirrors {
    std::mutex mutex;
    VkDevice device = VK_NULL_HANDLE;
    std::map<std::uint64_t, std::shared_ptr<ImageMirror>> entries;
    std::set<std::uint64_t> failed;
    // APS5_PROFILE_DRAW counters, reported every 10 s as [buffers] image mirrors.
    std::uint64_t builds = 0;
    std::uint64_t subranges = 0;
    std::uint64_t rebuilds = 0;
    std::uint64_t refreshes = 0;
    std::uint64_t blocksCompared = 0;
    std::uint64_t blocksCopied = 0;
    // Refreshes that had to wait for recorded work writing the range.
    std::uint64_t syncs = 0;
    std::uint64_t serials = 0;
    std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
};

ImageMirrors& Mirrors() {
    static ImageMirrors mirrors;
    return mirrors;
}

bool mirrorsEnabled() {
    static const bool disabled = std::getenv("APS5_NO_LEASE_MIRROR") != nullptr;
    return !disabled;
}

bool sameRange(const std::weak_ptr<const GuestAllocations::Range>& mirrored, const std::shared_ptr<const GuestAllocations::Range>& range) {
    return !mirrored.owner_before(range) && !range.owner_before(mirrored);
}

// One 64 KiB (or shorter, at a range's ends) block of a writable mirror to compare with guest memory.
struct RefreshBlock {
    ImageMirror* mirror;
    std::uint64_t address;
    std::size_t length;
};

// Compares a block's guest bytes with the mirror's shadow and copies a changed block into the device
// buffer and the shadow. Blocks are distinct, so blocks of one refresh can be compared in parallel.
bool compareBlock(const RefreshBlock& block) {
    const auto offset = static_cast<std::size_t>(block.address - block.mirror->base);
    const auto* guest = reinterpret_cast<const std::byte*>(block.address);
    if (std::memcmp(block.mirror->shadow.data() + offset, guest, block.length) == 0) return false;
    std::memcpy(block.mirror->buffer->Bytes().data() + offset, guest, block.length);
    std::memcpy(block.mirror->shadow.data() + offset, guest, block.length);
    return true;
}

// Helper threads for the block compares. An address-based build refreshes every writable image range
// (~15 MiB in ~300 blocks) under the device lock, and that memcmp was most of its cost; the blocks
// are handed out by an atomic cursor to the helpers and the calling thread alike, and Run returns
// once every block was taken and finished. The threads are started on first use and never joined
// (they only wait on the condition variable). APS5_MIRROR_REFRESH_THREADS sets the helper count
// (default 3); APS5_NO_PARALLEL_MIRROR_REFRESH=1 compares on the calling thread only.
class RefreshPool {
public:
    static RefreshPool& Get() {
        // Never destroyed (as WorkerSampler's): the helpers block on its condition variable, which a
        // static destructor at std::exit would tear down under them.
        static RefreshPool* pool = new RefreshPool;
        return *pool;
    }

    // Compares `blocks` and returns how many were copied.
    std::uint64_t Run(std::span<const RefreshBlock> blocks) {
        // Small jobs are not worth waking the helpers (a sub-range refresh at Upload is a few blocks).
        if (helpers == 0 || blocks.size() < 8) {
            std::uint64_t copied = 0;
            for (const auto& block : blocks) copied += compareBlock(block) ? 1 : 0;
            return copied;
        }
        // Every caller holds the device lock today; the pool serializes its jobs regardless.
        std::lock_guard running(runMutex);
        {
            std::lock_guard lock(mutex);
            job = blocks;
            next.store(0, std::memory_order_relaxed);
            copiedBlocks.store(0, std::memory_order_relaxed);
            finished = 0;
            ++serial;
        }
        wake.notify_all();
        work(blocks);
        std::unique_lock lock(mutex);
        done.wait(lock, [&] { return finished == helpers; });
        job = {};
        return copiedBlocks.load(std::memory_order_relaxed);
    }

private:
    RefreshPool() {
        static const bool disabled = std::getenv("APS5_NO_PARALLEL_MIRROR_REFRESH") != nullptr;
        const char* text = std::getenv("APS5_MIRROR_REFRESH_THREADS");
        const auto requested = text != nullptr ? std::atoi(text) : 3;
        const auto wanted = disabled ? 0u : static_cast<unsigned>(std::clamp(requested, 0, 16));
        // Only started threads are counted: Run waits for `helpers` finishes, and a thread that could
        // not be started would leave it waiting forever.
        for (unsigned i = 0; i < wanted; ++i) {
            try {
                std::thread([this] { helper(); }).detach();
                ++helpers;
            } catch (const std::system_error& error) {
                std::fprintf(stderr, "[gpu] mirror refresh helper %u not started: %s\n", i, error.what());
                break;
            }
        }
    }

    // Takes blocks until none is left.
    void work(std::span<const RefreshBlock> blocks) {
        std::uint64_t copied = 0;
        for (auto index = next.fetch_add(1, std::memory_order_relaxed); index < blocks.size(); index = next.fetch_add(1, std::memory_order_relaxed)) copied += compareBlock(blocks[index]) ? 1 : 0;
        copiedBlocks.fetch_add(copied, std::memory_order_relaxed);
    }

    void helper() {
        std::uint64_t seen = 0;
        for (;;) {
            std::span<const RefreshBlock> blocks;
            {
                std::unique_lock lock(mutex);
                wake.wait(lock, [&] { return serial != seen; });
                seen = serial;
                blocks = job;
            }
            work(blocks);
            {
                std::lock_guard lock(mutex);
                ++finished;
            }
            done.notify_all();
        }
    }

    unsigned helpers = 0;
    std::mutex runMutex;
    std::mutex mutex;
    std::condition_variable wake;
    std::condition_variable done;
    std::span<const RefreshBlock> job;
    std::uint64_t serial = 0;
    unsigned finished = 0;
    std::atomic<std::size_t> next{0};
    std::atomic<std::uint64_t> copiedBlocks{0};
};

// The device-lock work before [address, address + bytes) of a writable mirror can be compared with
// guest memory: recorded GPU work that writes the range (a descriptor dispatch bound to a sub-range)
// completes first, so its results reach the guest and the shadow before the compare and no batch
// writes a block being replaced; GPU reads of the mirror by earlier recorded work (an address-based
// dispatch whose lease is released at completion) see the newer bytes, as they would on hardware.
// Returns whether there is anything to compare.
bool prepareRefresh(ImageMirror& mirror, std::uint64_t address, std::uint64_t bytes) {
    if (!mirror.writable || bytes == 0) return false;
    // The compare reads the pages directly; a mirror is only reused while its Range exists, but a
    // lease of a build in flight can keep a replaced Range alive past a protection change.
    Require(GuestMemory::Accessible(reinterpret_cast<const void*>(address), static_cast<std::size_t>(bytes)), "image mirror range became inaccessible");
    // Named for the [hooksync] attribution: the flush goes through the hook, which waits when
    // recorded work notes a write inside the range (a descriptor written inside a writable exe
    // range; the lease itself maps the range read-only). The access is the whole range.
    const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::MirrorRefresh);
    GuestMemory::FlushGpuWrites(address, static_cast<std::size_t>(bytes));
    auto& state = Mirrors();
    if (auto* recorder = Recorder::Active(); recorder != nullptr && recorder->PendingWriteOverlaps(address, static_cast<std::size_t>(bytes))) {
        Recorder::CountSync(4);
        ++state.syncs;
        recorder->Sync();
    }
    ++state.refreshes;
    return true;
}

void appendBlocks(std::vector<RefreshBlock>& blocks, ImageMirror& mirror, std::uint64_t address, std::uint64_t bytes) {
    constexpr std::uint64_t block = 65536;
    const auto end = address + bytes;
    for (auto at = address; at < end;) {
        const auto next = std::min(end, (at / block + 1) * block);
        blocks.push_back({&mirror, at, static_cast<std::size_t>(next - at)});
        at = next;
    }
}

// Compares the gathered blocks (of one or many mirrors) and counts them.
void compareBlocks(std::span<const RefreshBlock> blocks) {
    if (blocks.empty()) return;
    const auto copied = RefreshPool::Get().Run(blocks);
    auto& state = Mirrors();
    state.blocksCompared += blocks.size();
    state.blocksCopied += copied;
}

// Brings [address, address + bytes) of a writable mirror up to date with guest memory.
void refreshMirror(ImageMirror& mirror, std::uint64_t address, std::uint64_t bytes) {
    if (!prepareRefresh(mirror, address, bytes)) return;
    std::vector<RefreshBlock> blocks;
    appendBlocks(blocks, mirror, address, bytes);
    compareBlocks(blocks);
}

// The mirror for a leased image range: the existing one, or a new one when none exists or the
// registered Range object changed (a protection change). Null when the range cannot be mirrored. An
// existing writable mirror's blocks are appended to `blocks` for the caller's one compare of every
// mirror of the build (prepareRefresh ran); a new mirror is filled here.
std::shared_ptr<ImageMirror> acquireMirror(const Context& context, const std::shared_ptr<const GuestAllocations::Range>& range, std::vector<RefreshBlock>& blocks) {
    auto& state = Mirrors();
    std::shared_ptr<ImageMirror> mirror;
    {
        std::lock_guard lock(state.mutex);
        if (state.device != context.device) {
            state.entries.clear();
            state.failed.clear();
            state.device = context.device;
        }
        if (state.failed.contains(range->address)) return nullptr;
        const auto found = state.entries.find(range->address);
        if (found != state.entries.end() && sameRange(found->second->range, range) && found->second->bytes == range->bytes && found->second->writable == range->writable) mirror = found->second;
    }
    if (mirror != nullptr) {
        if (prepareRefresh(*mirror, range->address, range->bytes)) appendBlocks(blocks, *mirror, range->address, range->bytes);
        return mirror;
    }
    // The registered flags were taken from the pages; a range that disagrees is copied by this build.
    if (!GuestMemory::Accessible(reinterpret_cast<const void*>(range->address), range->bytes, range->writable)) return nullptr;
    mirror = std::make_shared<ImageMirror>();
    mirror->base = range->address;
    mirror->bytes = range->bytes;
    mirror->writable = range->writable;
    mirror->range = range;
    try {
        mirror->buffer = std::make_shared<Buffer>(context, range->bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    } catch (const std::runtime_error& error) {
        std::fprintf(stderr, "[gpu] image mirror of 0x%llx+0x%llx failed: %s; falling back to copies\n", static_cast<unsigned long long>(range->address), static_cast<unsigned long long>(range->bytes), error.what());
        std::lock_guard lock(state.mutex);
        state.failed.insert(range->address);
        return nullptr;
    }
    // Read stores GPU results pending in the range first (write-backs of earlier copies of it);
    // named for the [hooksync] attribution like the refresh's flush.
    {
        const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::MirrorRefresh);
        GuestMemory::Read(range->address, mirror->buffer->Bytes());
    }
    if (range->writable) mirror->shadow.assign(mirror->buffer->Bytes().begin(), mirror->buffer->Bytes().end());
    std::lock_guard lock(state.mutex);
    ++state.rebuilds;
    mirror->serial = (1ull << 63u) | ++state.serials;
    // A replaced mirror lives on in the regions of recorded work that bound it.
    state.entries[range->address] = mirror;
    return mirror;
}

// The mirror containing [begin, end) whose registered range still exists, or null. A mirror whose
// Range was replaced is only rebuilt by the next address-based build; until then the range is copied.
std::shared_ptr<ImageMirror> findMirror(const Context& context, std::uint64_t begin, std::uint64_t end) {
    auto& state = Mirrors();
    std::lock_guard lock(state.mutex);
    if (state.device != context.device || state.entries.empty()) return nullptr;
    auto found = state.entries.upper_bound(begin);
    if (found == state.entries.begin()) return nullptr;
    --found;
    const auto& mirror = found->second;
    if (begin < mirror->base || end > mirror->base + mirror->bytes || mirror->range.expired()) return nullptr;
    return mirror;
}

// Drops mirrors whose registered range is gone (a later build made a new one for the replacement).
void sweepMirrors() {
    auto& state = Mirrors();
    std::lock_guard lock(state.mutex);
    for (auto it = state.entries.begin(); it != state.entries.end();) it = it->second->range.expired() ? state.entries.erase(it) : std::next(it);
}

void reportMirrors() {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!profile) return;
    auto& state = Mirrors();
    const auto now = std::chrono::steady_clock::now();
    if (now - state.lastReport < std::chrono::seconds(10)) return;
    state.lastReport = now;
    std::size_t count = 0;
    std::size_t writable = 0;
    std::uint64_t bytes = 0;
    {
        std::lock_guard lock(state.mutex);
        for (const auto& [base, mirror] : state.entries) {
            ++count;
            bytes += mirror->bytes;
            if (mirror->writable) ++writable;
        }
    }
    std::fprintf(stderr, "[buffers] image mirrors: %zu ranges (%.1f MiB, %zu writable), %llu address-based builds served, %llu descriptor sub-ranges bound, %llu rebuilds, %llu refreshes: %llu blocks compared, %llu copied, %llu refresh syncs\n", count, bytes / 1048576.0, writable, static_cast<unsigned long long>(state.builds), static_cast<unsigned long long>(state.subranges), static_cast<unsigned long long>(state.rebuilds), static_cast<unsigned long long>(state.refreshes), static_cast<unsigned long long>(state.blocksCompared), static_cast<unsigned long long>(state.blocksCopied), static_cast<unsigned long long>(state.syncs));
}

// Deferred lease release. The lease an address-based build takes (AcquireRegistered) is dropped by its
// write-back, which runs as a completion action when the batch that recorded the work finished; the
// work is no longer synced as soon as it is recorded (that sync was ~30000 waits per 200 s). A guest
// thread that unmaps, mprotects or frees a leased allocation finds it pinned in the registry and calls
// this waiter (with the registry lock released, see GuestAllocationsRequireUnpinned): under the device
// lock it submits the recorder and finishes its batches up to the newest one that was recorded with a
// lease (CountLeaseOutcome notes that serial); the completions release the leases, later batches stay
// in flight, and the registry rescans. A lease held by a build in progress belongs to a worker holding
// the device lock, so the waiter waits for that worker first; once it holds the lock every lease of
// recorded work is in a batch. A lease whose batch was not noted (the record threw between the keep
// and the note) is covered by draining the recorder when the noted serial is already finished. The
// waiter yields instead when its own thread holds the device lock (a driver thread mutating the
// registry mid-packet, as the plain spin did) and reports whether it finished any work.
struct LeaseState {
    std::mutex mutex;
    LeaseStats stats;
    std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
    // Under GuestMemory::GpuMutex: the recorder whose batches hold leases (serials restart with a
    // replaced device's recorder), the serial of its newest batch recorded with a lease, and the
    // newest one a waiter finished.
    const Recorder* leaseRecorder = nullptr;
    std::uint64_t newestLeaseSerial = 0;
    std::uint64_t finishedLeaseSerial = 0;
};

LeaseState& Leases() {
    static LeaseState state;
    return state;
}

// APS5_PROFILE_DRAW: AddSnapshot's consistency compares of a captured region against the region
// serving it, made and skipped (see AddSnapshot); printed in the [address-sync] line. Atomic: the
// unlocked stage A of descriptor builds adds snapshots too.
struct SnapshotStats {
    std::atomic<std::uint64_t> checked{0};
    std::atomic<std::uint64_t> skipped{0};
};

SnapshotStats& Snapshots() {
    static SnapshotStats stats;
    return stats;
}

bool WaitForLeases() noexcept {
    const auto start = std::chrono::steady_clock::now();
    bool synced = false;
    bool drained = false;
    if (GuestMemory::GpuMutex().HeldByThisThread()) {
        std::this_thread::yield();
    } else {
        try {
            std::lock_guard lock(GuestMemory::GpuMutex());
            auto& state = Leases();
            auto* recorder = Recorder::Active();
            const auto target = state.newestLeaseSerial;
            if (recorder != nullptr && recorder == state.leaseRecorder && target > state.finishedLeaseSerial) {
                // The open batch may be the target; FinishUpTo waits front to back up to it only.
                Recorder::CountSync(4);
                recorder->Submit();
                recorder->FinishUpTo(target);
                state.finishedLeaseSerial = target;
                synced = true;
            } else if (recorder != nullptr && !recorder->Idle()) {
                Recorder::CountSync(4);
                recorder->Sync();
                synced = true;
                drained = true;
            } else {
                // Nothing recorded holds the lease: the holder is a lease-holding object that has
                // yet to record (a test's, or a build whose worker gave up the lock); give it time.
                std::this_thread::yield();
            }
        } catch (const std::exception& error) {
            std::fprintf(stderr, "[gpu] lease wait failed: %s\n", error.what());
        }
    }
    auto& state = Leases();
    std::lock_guard lock(state.mutex);
    ++state.stats.contentionWaits;
    if (synced) ++state.stats.contentionSyncs;
    if (drained) ++state.stats.contentionDrains;
    state.stats.contentionMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return synced;
}

void ensurePinWaiter() {
    static const bool registered = [] {
        GuestAllocations::GuestAllocationsSetPinWaiter_nid_postfix(&WaitForLeases);
        return true;
    }();
    static_cast<void>(registered);
}

}

bool SyncLeaseWork() {
    static const bool sync = std::getenv("APS5_SYNC_LEASE_DISPATCH") != nullptr;
    return sync;
}

void CountLeaseOutcome(bool synced, std::uint64_t batchSerial) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    auto& state = Leases();
    if (!synced) {
        // Serials grow with the submissions, so the newest noted one covers every earlier lease batch
        // of the same recorder.
        GuestMemory::AssertGpuLockHeld("CountLeaseOutcome");
        if (const auto* recorder = Recorder::Active(); recorder != state.leaseRecorder) {
            state.leaseRecorder = recorder;
            state.newestLeaseSerial = 0;
            state.finishedLeaseSerial = 0;
        }
        state.newestLeaseSerial = std::max(state.newestLeaseSerial, batchSerial);
    }
    std::lock_guard lock(state.mutex);
    ++(synced ? state.stats.synced : state.stats.deferred);
    if (!profile) return;
    // APS5_PROFILE_DRAW: the leases (dispatches and draws) deferred and synced, the guest's
    // pin-contention waits and the BDA table cache, every 10 s (cumulative counts, like the
    // [recorder] line's), printed by whichever path counts a lease.
    const auto now = std::chrono::steady_clock::now();
    if (now - state.lastReport < std::chrono::seconds(10)) return;
    state.lastReport = now;
    const auto& stats = state.stats;
    const auto table = BdaResources::TableCacheCounters();
    const auto& snapshots = Snapshots();
    std::fprintf(stderr, "[address-sync] leases: %llu released at completion, %llu synced at once; %llu pin-contention waits by guest threads (%llu finished the lease batch, %llu drained the recorder) %.1f s; BDA table cache %llu hits / %llu misses (%zu tables held); snapshot compares: %llu skipped (live-backed region), %llu made\n", static_cast<unsigned long long>(stats.deferred), static_cast<unsigned long long>(stats.synced), static_cast<unsigned long long>(stats.contentionWaits), static_cast<unsigned long long>(stats.contentionSyncs), static_cast<unsigned long long>(stats.contentionDrains), stats.contentionMs / 1000, static_cast<unsigned long long>(table.hits), static_cast<unsigned long long>(table.misses), table.held, static_cast<unsigned long long>(snapshots.skipped.load(std::memory_order_relaxed)), static_cast<unsigned long long>(snapshots.checked.load(std::memory_order_relaxed)));
}

LeaseStats LeaseCounters() {
    auto& state = Leases();
    std::lock_guard lock(state.mutex);
    return state.stats;
}

const HostImport* HostImportFor(const Context& context, std::uint64_t address, std::size_t bytes) {
    if (context.hostImportAlignment == 0 || bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address) return nullptr;
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    // A hit is only valid while the registry has not changed since the imports were reconciled.
    if (!importsStale(context, state)) {
        if (const auto* entry = findImport(state, address, address + bytes)) return entry;
    }
    const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
    refreshImports(context, state, lease);
    if (const auto* range = containingRange(lease, address, address + bytes)) return importAllocation(context, state, range->address, range->bytes);
    return nullptr;
}

bool HostImportCovers(const Context& context, std::uint64_t address, std::size_t bytes) {
    if (context.hostImportAlignment == 0 || bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address) return false;
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    // Imports of a replaced device are not this device's; a stale registry only means an import
    // may be dropped at the next reconcile, which the binding path handles.
    return state.device == context.device && findImport(state, address, address + bytes) != nullptr;
}

GuestBufferMemory::GuestBufferMemory(const Context& context) : context(context) {}

bool GuestBufferMemory::WritesOverlap(std::uint64_t address, std::size_t bytes) const {
    return std::any_of(writes.begin(), writes.end(), [&](const auto& range) { return address < range.second && range.first < address + bytes; });
}

void GuestBufferMemory::AcquireRegistered() {
    Require(!uploaded && regions.empty() && lease.empty(), "guest allocation lease must precede resource registration");
    ensurePinWaiter();
    // Pending GPU results in these ranges are stored when Upload prepares each region.
    lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
    const bool importable = context.hostImportAlignment != 0;
    if (importable) {
        auto& state = Imports();
        std::lock_guard lock(state.mutex);
        refreshImports(context, state, lease);
        // Taken before the pass: the imports it takes are the registry's ones (just reconciled, so
        // none is retired by a make below), and a mirror's preparation may sync the recorder, whose
        // completions can retire an import already taken; that moves the epoch, and Upload then
        // re-checks every pointer instead of trusting it.
        importsEpoch = state.epoch;
    }
    // One pass in the registry's (ascending) order, so the regions come out sorted for AddSnapshot.
    // The import lock is taken per range: a mirror's preparation may wait for recorded work, which
    // must not happen under it.
    regions.reserve(lease.size());
    std::vector<RefreshBlock> blocks;
    bool mirrored = false;
    for (const auto& range : lease) {
        if (!range->readable) continue;
        validate(range->address, range->bytes);
        Region region{range->address, range->address + range->bytes, range->writable, {}, nullptr};
        const HostImport* entry = nullptr;
        if (importable) {
            auto& state = Imports();
            std::lock_guard lock(state.mutex);
            entry = importAllocation(context, state, range->address, range->bytes);
        }
        if (entry != nullptr) {
            region.hostBacked = true;
            // Reused by Upload while no import was dropped since (see importsEpoch).
            region.direct = entry;
            regions.push_back(std::move(region));
            continue;
        }
        if (!range->releasable && mirrorsEnabled()) {
            // Main image: served by a persistent mirror (see ImageMirror); the writable ones are
            // compared with guest memory below, all at once.
            region.mirror = acquireMirror(context, range, blocks);
            if (region.mirror != nullptr) {
                mirrored = true;
                regions.push_back(std::move(region));
                continue;
            }
        }
        // Read-only ranges are read from live guest memory at Upload: the lease pins them and the guest
        // cannot write them, so that equals a snapshot taken now without the extra copy. Reserved heaps
        // are registered whole while the guest commits pages on demand: committed pages only.
        region.hostBacked = !range->writable;
        auto committed = GuestMemory::DescribeCommitted(range->address, range->bytes, range->writable);
        if (!committed.whole) {
            region.sparse = true;
            region.backed = std::move(committed.ranges);
        }
        regions.push_back(std::move(region));
    }
    // APS5_NO_SORTED_SNAPSHOT_LOOKUP=1: AddSnapshot scans the regions as before instead of searching.
    static const bool sortedLookup = std::getenv("APS5_NO_SORTED_SNAPSHOT_LOOKUP") == nullptr;
    regionsSorted = sortedLookup;
    compareBlocks(blocks);
    if (mirrorsEnabled()) {
        if (mirrored) ++Mirrors().builds;
        sweepMirrors();
        reportMirrors();
    }
}

void GuestBufferMemory::validate(std::uint64_t address, std::size_t bytes) const {
    Require(!uploaded, "guest memory ownership is frozen for GPU execution");
    Require(address != 0 && bytes != 0, "empty guest memory range");
    Require(bytes <= std::numeric_limits<std::uint64_t>::max() - address, "guest memory range overflow");
}

void GuestBufferMemory::addDescriptorRegion(std::uint64_t address, std::size_t bytes) {
    validate(address, bytes);
    // `writable` here means the bytes come from live guest memory (not a snapshot), whether or not
    // the shader stores to them; what is written back is decided by Writes() alone.
    Region region{address, address + bytes, true, {}, nullptr};
    auto committed = GuestMemory::DescribeCommitted(address, bytes, true);
    if (!committed.whole) {
        // A GPU heap bound whole while the guest commits its pages on demand, or a descriptor left
        // pointing at unmapped memory: only committed pages are copied in and stored back. Shaders
        // must not touch the rest, which reads as zeros.
        region.sparse = true;
        region.backed = std::move(committed.ranges);
    }
    regions.push_back(std::move(region));
    regionsSorted = false;
}

void GuestBufferMemory::AddWritable(std::uint64_t address, std::size_t bytes) {
    addDescriptorRegion(address, bytes);
    writes.emplace_back(address, address + bytes);
}

void GuestBufferMemory::AddReadable(std::uint64_t address, std::size_t bytes) {
    // Not in `writes`: no reference copy (copyRegion), no write-back, no pending-write note, no
    // direct-write mark, and UploadPrepare copies it without the device lock. A written descriptor
    // overlapping the range still covers it through its own Writes() entry.
    addDescriptorRegion(address, bytes);
}

void GuestBufferMemory::AddSnapshot(const GuestMemorySnapshot& snapshot) {
    validate(snapshot.address, snapshot.bytes.size());
    const auto end = snapshot.address + snapshot.bytes.size();
    // A snapshot inside a region needs no copy: the region's bytes serve it (they are checked to
    // agree). Right after AcquireRegistered the regions are sorted and non-overlapping (registered
    // ranges), so the one that can contain the snapshot is found by search rather than by scanning
    // the ~1200 of an address-based build for each captured region.
    const Region* owner = nullptr;
    if (regionsSorted) {
        const auto found = std::upper_bound(regions.begin(), regions.end(), snapshot.address, [](std::uint64_t value, const Region& region) { return value < region.begin; });
        if (found != regions.begin() && end <= std::prev(found)->end) owner = &*std::prev(found);
    } else {
        for (const auto& region : regions) {
            if (region.begin <= snapshot.address && end <= region.end) {
                owner = &region;
                break;
            }
        }
    }
    if (owner != nullptr) {
        // A region read live (an import or mirror bound in place, a copy of live bytes) serves the
        // GPU whatever the snapshot holds: the compare only checked that the capture (made before
        // the device lock) still agreed with memory the guest, or a dispatch writing a descriptor
        // range around the captured words, may since have changed, and a difference failed the
        // whole dispatch where hardware would have run with the bytes in memory. A snapshot-backed
        // region uploads its own snapshot, so two snapshots of one range must agree. The live
        // compare reads the pages directly (no flush hook), so it costs no sync, only the memcmp;
        // it stays on because it is the one check that the words the recompiler baked into
        // descriptors and specialization still equal what the GPU reads. APS5_NO_SNAPSHOT_CHECK=1
        // skips the compare of live-backed regions (the region serves the GPU whatever the snapshot
        // held; only the diagnostic is lost).
        static const bool checkLive = std::getenv("APS5_NO_SNAPSHOT_CHECK") == nullptr;
        static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        const bool live = owner->writable || owner->hostBacked || owner->mirror != nullptr;
        if (live && !checkLive) {
            if (profile) Snapshots().skipped.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        const auto offset = static_cast<std::size_t>(snapshot.address - owner->begin);
        const auto* source = live ? reinterpret_cast<const std::byte*>(snapshot.address) : owner->snapshot.data() + offset;
        Require(std::memcmp(source, snapshot.bytes.data(), snapshot.bytes.size()) == 0, "guest snapshot differs from registered memory");
        if (profile) Snapshots().checked.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    regions.push_back({snapshot.address, end, false, {snapshot.bytes.begin(), snapshot.bytes.end()}, nullptr});
    regionsSorted = false;
}

void GuestBufferMemory::Upload(bool addressable) {
    UploadPrepare(addressable);
    UploadFinish(addressable);
}

namespace {

// APS5_PROFILE_DRAW: sub-phase totals of the uploads in microseconds, reported every 2000 uploads
// ([buffers] line); atomic because the prepare stage of several builds runs at once.
std::atomic<std::uint64_t> importUs{0};
std::atomic<std::uint64_t> allocateUs{0};
std::atomic<std::uint64_t> readUs{0};
std::atomic<std::uint64_t> uploadsProfiled{0};

std::uint64_t microsecondsSince(std::chrono::steady_clock::time_point start) {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count());
}

// GPU-side copies. A descriptor sub-range inside a host import whose offset is not a multiple of
// minStorageBufferOffsetAlignment cannot be bound in place; ~31000 of them per run (about 34 bytes
// each) were copied through the CPU, and every such read went through the flush hook and waited
// for the producer batch ([hooksync] 'buffer-upload'). Instead the copy is a vkCmdCopyBuffer from
// the import into the region's buffer, recorded into the open batch after the producer, and the
// written sub-ranges are copied back into the import by the GPU (RecordCopyBacks), so neither
// direction touches guest memory from the CPU. APS5_CPU_COPIES=1 copies on the CPU as before;
// APS5_GPU_COPY_MAX_KIB (default 1024) bounds the region size the GPU copies (larger ones and
// regions outside imports keep the CPU path).
bool gpuCopiesEnabled() {
    static const bool disabled = std::getenv("APS5_CPU_COPIES") != nullptr;
    return !disabled;
}

std::uint64_t gpuCopyLimit() {
    static const std::uint64_t limit = [] {
        const char* value = std::getenv("APS5_GPU_COPY_MAX_KIB");
        return (value != nullptr ? std::strtoull(value, nullptr, 10) : 1024ull) << 10u;
    }();
    return limit;
}

VkBufferUsageFlags gpuCopyUsage(bool addressable) {
    return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | (addressable ? VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT : 0u);
}

// APS5_PROFILE_DRAW: the GPU copies, reported in the [buffers] uploads line (cumulative).
struct CopyStats {
    std::atomic<std::uint64_t> gpuCopies{0};
    std::atomic<std::uint64_t> gpuCopyBytes{0};
    std::atomic<std::uint64_t> gpuCopyBacks{0};
    std::atomic<std::uint64_t> gpuCopyBackBytes{0};
    // Written gpuCopy regions of a use that recorded no copy-back (a synchronous draw): stored
    // from the staging buffer by the CPU in WriteBack.
    std::atomic<std::uint64_t> stagingStores{0};
};

CopyStats& Copies() {
    static CopyStats stats;
    return stats;
}

}

bool GuestBufferMemory::gpuCopyEligible(const Region& region) const {
    if (!gpuCopiesEnabled() || region.sparse || !(region.hostBacked || region.writable)) return false;
    return region.end - region.begin <= gpuCopyLimit();
}

void GuestBufferMemory::UploadPrepare(bool addressable) {
    Require(!prepared && !uploaded, "guest memory was already uploaded");
    prepared = true;
    std::sort(regions.begin(), regions.end(), [](const Region& left, const Region& right) { return left.begin < right.begin; });
    // Merged regions stay sparse when either part covered uncommitted pages.
    const auto mergeBacked = [](Region& into, const Region& from) {
        if (!into.sparse && !from.sparse) return;
        auto ranges = into.sparse ? into.backed : decltype(into.backed){{into.begin, into.end}};
        if (from.sparse) ranges.insert(ranges.end(), from.backed.begin(), from.backed.end());
        else ranges.emplace_back(from.begin, from.end);
        std::sort(ranges.begin(), ranges.end());
        into.backed.clear();
        for (const auto& range : ranges) {
            if (!into.backed.empty() && range.first <= into.backed.back().second) into.backed.back().second = std::max(into.backed.back().second, range.second);
            else into.backed.push_back(range);
        }
        const auto end = std::max(into.end, from.end);
        into.sparse = !(into.backed.size() == 1 && into.backed.front().first <= into.begin && into.backed.front().second >= end);
        if (!into.sparse) into.backed.clear();
    };
    std::vector<Region> merged;
    for (auto& region : regions) {
        if (!merged.empty() && region.begin < merged.back().end) {
            auto& previous = merged.back();
            if (previous.mirror != nullptr || region.mirror != nullptr) {
                // A descriptor range inside a mirrored image range binds the mirror's bytes (registered
                // ranges never overlap, so the other part is such a descriptor; it sorts first when it
                // starts at the range's base). Writes into a read-only mirror are never stored. One
                // crossing the mirror's end cannot be served by it: this build copies the range.
                if (previous.mirror == nullptr) std::swap(previous, region);
                Require(region.mirror == nullptr, "image mirrors overlap");
                if (region.begin >= previous.begin && region.end <= previous.end) continue;
                previous.mirror = nullptr;
                previous.hostBacked = true;
            }
            mergeBacked(previous, region);
            // A range starting before the mirror (only possible after the swap) keeps its prefix; the
            // earlier merged region ends at or before it, so the merged list stays sorted.
            previous.begin = std::min(previous.begin, region.begin);
            if (previous.hostBacked || region.hostBacked) {
                // Live guest bytes back the merged range, so snapshots inside it add nothing.
                previous.hostBacked = true;
                previous.writable = previous.writable || region.writable;
                previous.snapshot.clear();
                previous.end = std::max(previous.end, region.end);
                continue;
            }
            Require(previous.writable == region.writable, "writable guest memory overlaps an immutable snapshot");
            if (!region.writable) {
                const auto offset = static_cast<std::size_t>(region.begin - previous.begin);
                const auto overlap = static_cast<std::size_t>(std::min(previous.end, region.end) - region.begin);
                Require(std::memcmp(previous.snapshot.data() + offset, region.snapshot.data(), overlap) == 0, "inconsistent overlapping guest snapshots");
                if (region.end > previous.end) previous.snapshot.insert(previous.snapshot.end(), region.snapshot.begin() + overlap, region.snapshot.end());
            }
            previous.end = std::max(previous.end, region.end);
        } else {
            merged.push_back(std::move(region));
        }
    }
    regions = std::move(merged);
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    const auto started = std::chrono::steady_clock::now();
    for (auto& region : regions) {
        Require(region.end - region.begin <= std::numeric_limits<std::size_t>::max(), "guest GPU allocation size overflow");
        // Served by a lease mirror, brought up to date when it was acquired.
        if (region.mirror != nullptr || region.direct != nullptr || !mirrorsEnabled()) continue;
        // A descriptor range inside a mirrored image range binds the mirror's sub-range. Looked up
        // before the imports: the image can never be imported, and the import fallback would acquire
        // a registry lease for it on nearly every descriptor upload.
        auto mirror = findMirror(context, region.begin, region.end);
        if (mirror == nullptr || (region.begin - mirror->base) % context.limits.minStorageBufferOffsetAlignment != 0) continue;
        // A read-only mirror's bytes never change while its Range exists; a writable one is
        // refreshed by UploadFinish (the refresh may wait for recorded work). Either is confirmed
        // there, under the lock, before it serves the region: the Range can go between the stages,
        // and until then the region keeps its snapshot for the copy it would need instead.
        region.pending = mirror->writable;
        region.mirror = std::move(mirror);
        region.subrangeMirror = true;
    }
    // Imports already serving regions, taken under one hold of the import lock so every pointer
    // belongs to the same registry epoch (UploadFinish trusts them while the epoch is unchanged). No
    // reconcile and no new import here: retiring one hands its buffer to the recorder, and a region
    // without an import waits for UploadFinish, which may make one.
    if (context.hostImportAlignment != 0) {
        auto& state = Imports();
        std::lock_guard lock(state.mutex);
        const bool stale = importsStale(context, state);
        for (auto& region : regions) {
            if (region.mirror != nullptr) continue;
            // An import found when the lease was acquired is reused while none was dropped since.
            const HostImport* entry = region.direct != nullptr && state.epoch == importsEpoch ? region.direct : nullptr;
            region.direct = nullptr;
            if (stale) {
                region.pending = true;
                continue;
            }
            if (entry == nullptr) entry = findImport(state, region.begin, region.end);
            if (entry == nullptr) {
                region.pending = true;
                continue;
            }
            // Misaligned inside an import: copied, whatever the registry does meanwhile. By the GPU
            // when eligible (see Region::gpuCopy): UploadFinish records the copy from the import it
            // confirms then (the pointer is provisional, as for an aligned region); the buffer the
            // copy fills is made here, without the device lock, and serves the CPU fallback too.
            if ((region.begin - entry->base) % context.limits.minStorageBufferOffsetAlignment != 0) {
                if (!gpuCopyEligible(region)) continue;
                const auto allocateStart = std::chrono::steady_clock::now();
                region.buffer = std::make_shared<Buffer>(context, static_cast<std::size_t>(region.end - region.begin), gpuCopyUsage(addressable));
                if (profile) allocateUs.fetch_add(microsecondsSince(allocateStart), std::memory_order_relaxed);
                region.direct = entry;
                region.pending = true;
                continue;
            }
            // Provisional until UploadFinish confirms it against the epoch; the snapshot stays for
            // the copy the region falls back to when the import is gone and none can be made.
            region.direct = entry;
            // Storage results pending in the range are flushed by UploadFinish.
            region.pending = true;
        }
        importsEpoch = state.epoch;
    }
    if (profile) importUs.fetch_add(microsecondsSince(started), std::memory_order_relaxed);
    // Copies that need no device work: a range no descriptor writes needs no reference copy, and
    // GuestMemory::Read stores pending GPU results first (the flush hook locks for itself).
    for (auto& region : regions) {
        if (region.pending || region.mirror != nullptr || region.direct != nullptr) continue;
        if (region.writable && WritesOverlap(region.begin, static_cast<std::size_t>(region.end - region.begin))) {
            region.pending = true;
            continue;
        }
        copyRegion(region, addressable);
    }
}

void GuestBufferMemory::UploadFinish(bool addressable) {
    Require(prepared && !uploaded, "guest memory upload was not prepared");
    uploaded = true;
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    const auto started = std::chrono::steady_clock::now();
    // Registered ranges to import from. Address-based shaders hold their lease until write-back;
    // descriptor-only uploads acquire one only when the registry changed or an import must be made
    // (a lease copies every registered range's shared_ptr under the registry lock), so the guest can
    // keep managing memory.
    GuestAllocations::Lease acquired;
    const auto importRanges = [&]() -> const GuestAllocations::Lease& {
        if (lease.empty() && acquired.empty()) acquired = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
        return lease.empty() ? acquired : lease;
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
    // Regions the GPU copies out of their import (see Region::gpuCopy), recorded together below.
    // No recorder (tests) keeps them on the CPU path.
    auto* recorder = Recorder::Active();
    std::vector<Region*> gpuCopies;
    for (auto& region : regions) {
        if (!region.pending) continue;
        region.pending = false;
        const auto bytes = region.end - region.begin;
        if (region.mirror != nullptr) {
            // A writable sub-range mirror: refreshed here, where waiting for recorded work is allowed.
            // Read-only mirrors are never written back; a writable one is diffed for this sub-range
            // at write-back.
            const auto refreshStart = std::chrono::steady_clock::now();
            refreshMirror(*region.mirror, region.begin, bytes);
            if (profile) readUs.fetch_add(microsecondsSince(refreshStart), std::memory_order_relaxed);
            continue;
        }
        bool copyOnGpu = false;
        if (context.hostImportAlignment != 0) {
            const auto lookupStart = std::chrono::steady_clock::now();
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
            if (entry == nullptr) {
                if (const auto* range = containingRange(importRanges(), region.begin, region.end)) entry = importAllocation(context, state, range->address, range->bytes);
            }
            if (entry != nullptr && (region.begin - entry->base) % context.limits.minStorageBufferOffsetAlignment == 0) {
                region.direct = entry;
                region.snapshot.clear();
                // A buffer UploadPrepare made for a GPU copy is not needed: the import serves the
                // region in place (Descriptor prefers `direct`), and a kept buffer would count as copied.
                region.buffer.reset();
            } else if (entry != nullptr && recorder != nullptr && gpuCopyEligible(region)) {
                // Misaligned in the import: the GPU copies the sub-range out of it. The handle and
                // base are taken now, under the import lock: a reconcile by the flush below could
                // retire the entry (the batch opened below keeps a retired import's buffer alive).
                copyOnGpu = true;
                region.copySource = entry->buffer;
                region.copySourceBase = entry->base;
            }
            if (profile) importUs.fetch_add(microsecondsSince(lookupStart), std::memory_order_relaxed);
        }
        if (region.direct != nullptr) {
            // The GPU reads this range in place, so storage results pending in it must be stored first
            // (outside the import lock: the store may import memory itself). The whole registered heaps
            // an address-based shader maps are exempt (they hold every pending image, and such shaders
            // do not read texture memory through pointers); APS5_BDA_FLUSH=1 flushes for them too.
            static const bool bdaFlush = std::getenv("APS5_BDA_FLUSH") != nullptr;
            if (!(addressable && region.hostBacked) || bdaFlush) StorageTexture::FlushPending(region.begin, static_cast<std::size_t>(bytes), nullptr, "imported buffer region");
            continue;
        }
        if (copyOnGpu) {
            // As for a direct region, storage results pending in the range reach the import before
            // the copy reads it: the flush is recorded (GPU-direct into the import) ahead of the copy
            // in the same batch. The batch is opened first so that an import the flush's own
            // reconcile retires is handed to it (retireImport keeps nothing while the recorder is
            // idle) and outlives the copy.
            static_cast<void>(recorder->Commands());
            StorageTexture::FlushPending(region.begin, static_cast<std::size_t>(bytes), nullptr, "copied buffer region");
            region.gpuCopy = true;
            gpuCopies.push_back(&region);
            continue;
        }
        copyRegion(region, addressable);
    }
    if (!gpuCopies.empty()) recordGpuCopies(gpuCopies, addressable);
    if (!profile) return;
    const auto uploads = uploadsProfiled.fetch_add(1, std::memory_order_relaxed) + 1;
    if (uploads % 2000 == 0) {
        const auto& copies = Copies();
        std::fprintf(stderr, "[buffers] %llu uploads: import lookup %.0f ms, buffer allocation %.0f ms, guest read %.0f ms; copies on the GPU: %llu regions (%.0f KiB) copied out of imports, %llu written sub-ranges (%.0f KiB) copied back, %llu staging stores by the CPU at write-back\n", static_cast<unsigned long long>(uploads), importUs.load(std::memory_order_relaxed) / 1000.0, allocateUs.load(std::memory_order_relaxed) / 1000.0, readUs.load(std::memory_order_relaxed) / 1000.0, static_cast<unsigned long long>(copies.gpuCopies.load(std::memory_order_relaxed)), copies.gpuCopyBytes.load(std::memory_order_relaxed) / 1024.0, static_cast<unsigned long long>(copies.gpuCopyBacks.load(std::memory_order_relaxed)), copies.gpuCopyBackBytes.load(std::memory_order_relaxed) / 1024.0, static_cast<unsigned long long>(copies.stagingStores.load(std::memory_order_relaxed)));
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
        std::fprintf(stderr, "[resources] guest upload of %zu regions, %.1f MiB (largest %.1f MiB, copied %.1f MiB, addressable %d) took %.0f ms to finish\n", regions.size(), total / 1048576.0, largest / 1048576.0, copied / 1048576.0, addressable ? 1 : 0, ms);
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
    const auto usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | (addressable ? VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT : 0u);
    if (profile) {
        // Why the region is copied rather than bound in place, totalled every 1000 uploads (under a
        // mutex: prepare stages of several builds copy at once).
        static std::mutex reasonsMutex;
        static std::uint64_t uploads = 0, noImport = 0, noImportBytes = 0, misaligned = 0, misalignedBytes = 0, snapshotOnly = 0, snapshotBytes = 0;
        static std::map<std::uint64_t, std::pair<std::uint64_t, std::uint64_t>> outside;
        bool inImport = false;
        if (context.hostImportAlignment != 0) {
            auto& state = Imports();
            std::lock_guard lock(state.mutex);
            inImport = findImport(state, region.begin, region.end) != nullptr;
        }
        std::lock_guard lock(reasonsMutex);
        if (!region.hostBacked && !region.writable) { ++snapshotOnly; snapshotBytes += bytes; }
        else if (inImport) { ++misaligned; misalignedBytes += bytes; }
        else {
            ++noImport;
            noImportBytes += bytes;
            auto& bucket = outside[region.begin >> 28u];
            ++bucket.first;
            bucket.second += bytes;
        }
        if (++uploads % 1000 == 0) {
            // 'misaligned in imports' are the ones the GPU did not copy: over APS5_GPU_COPY_MAX_KIB,
            // sparse, APS5_CPU_COPIES, no recorder, or an import gone by UploadFinish.
            std::fprintf(stderr, "[buffers] %llu copied regions on the CPU: %llu snapshots (%.0f MiB), %llu misaligned in imports (%.0f MiB), %llu outside imports (%.0f MiB):", static_cast<unsigned long long>(uploads), static_cast<unsigned long long>(snapshotOnly), snapshotBytes / 1048576.0, static_cast<unsigned long long>(misaligned), misalignedBytes / 1048576.0, static_cast<unsigned long long>(noImport), noImportBytes / 1048576.0);
            for (const auto& [granule, counts] : outside) std::fprintf(stderr, " 0x%llx0000000:%llu/%.0fMiB", static_cast<unsigned long long>(granule), static_cast<unsigned long long>(counts.first), counts.second / 1048576.0);
            std::fprintf(stderr, "\n");
        }
    }
    const auto allocateStart = std::chrono::steady_clock::now();
    // A buffer made by UploadPrepare for a GPU copy that fell back here (its import went) is kept.
    if (region.buffer == nullptr) region.buffer = std::make_shared<Buffer>(context, static_cast<std::size_t>(bytes), usage);
    const auto readStart = std::chrono::steady_clock::now();
    if (profile) allocateUs.fetch_add(microsecondsSince(allocateStart), std::memory_order_relaxed);
    // Named for the [hooksync] attribution: the reads below go through the flush hook.
    const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::BufferUpload);
    if (region.hostBacked || region.writable) {
        // Only ranges a descriptor may write need the copy the write-back diffs against; the
        // registered ranges of an address-based shader are read-only to it (AddressRanges).
        const bool written = region.writable && WritesOverlap(region.begin, static_cast<std::size_t>(bytes));
        if (!region.sparse) {
            GuestMemory::Read(region.begin, region.buffer->Bytes());
            if (written) region.uploaded.assign(region.buffer->Bytes().begin(), region.buffer->Bytes().end());
        } else {
            // `uploaded` holds the backed pieces back to back.
            for (const auto& [first, last] : region.backed) {
                const auto piece = region.buffer->Bytes().subspan(static_cast<std::size_t>(first - region.begin), static_cast<std::size_t>(last - first));
                GuestMemory::Read(first, piece);
                if (written) region.uploaded.insert(region.uploaded.end(), piece.begin(), piece.end());
            }
        }
    }
    else std::memcpy(region.buffer->Bytes().data(), region.snapshot.data(), region.snapshot.size());
    region.snapshot.clear();
    if (profile) readUs.fetch_add(microsecondsSince(readStart), std::memory_order_relaxed);
}

void GuestBufferMemory::recordGpuCopies(std::span<Region* const> copies, bool addressable) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    auto* recorder = Recorder::Active();
    Require(recorder != nullptr, "GPU buffer copies need an active recorder");
    const auto commands = recorder->Commands();
    // Every earlier recorded store into the imports (dispatches in place, fills, GPU labels,
    // GPU-direct write-backs and the flushes just recorded) and the host's own writes precede the
    // copies. A CPU write made after the batch is submitted is the game's race, as on hardware.
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
    for (auto* region : copies) {
        const auto bytes = region->end - region->begin;
        if (region->buffer == nullptr) {
            // Not made by UploadPrepare (the registry was stale then): made here, under the lock.
            const auto allocateStart = std::chrono::steady_clock::now();
            region->buffer = std::make_shared<Buffer>(context, static_cast<std::size_t>(bytes), gpuCopyUsage(addressable));
            if (profile) allocateUs.fetch_add(microsecondsSince(allocateStart), std::memory_order_relaxed);
        }
        CopyBuffer(context, commands, region->copySource, region->begin - region->copySourceBase, region->buffer->Handle(), 0, bytes);
        // Kept by the batch at record time, as every other recorded target is: the caller keeps
        // the resources only after descriptor and pipeline work that may throw, and a released
        // buffer would go back to the pool (reused or destroyed) under this copy command. The
        // source is a live import or one retireImport already handed to this batch.
        recorder->Keep(region->buffer);
        region->snapshot.clear();
        if (profile) {
            Copies().gpuCopies.fetch_add(1, std::memory_order_relaxed);
            Copies().gpuCopyBytes.fetch_add(bytes, std::memory_order_relaxed);
        }
    }
    // The copied bytes are visible to the shaders bound to the buffers, which may also store into
    // them. Every stage: dispatches and draws share this path, and draws run mesh, task and
    // tessellation shaders too (the batch's own barriers around dispatches are as wide).
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
}

void GuestBufferMemory::RecordCopyBacks(Recorder& recorder) {
    if (!uploaded || committed) return;
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    // The written ranges, merged: a V# bound twice would otherwise copy the same bytes twice.
    std::vector<std::pair<std::uint64_t, std::uint64_t>> merged;
    bool recording = false;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    for (auto& region : regions) {
        if (!region.gpuCopy || region.copiedBack) continue;
        if (merged.empty() && !writes.empty()) {
            auto sorted = writes;
            std::sort(sorted.begin(), sorted.end());
            for (const auto& range : sorted) {
                if (!merged.empty() && range.first < merged.back().second) merged.back().second = std::max(merged.back().second, range.second);
                else merged.push_back(range);
            }
        }
        for (const auto& [begin, end] : merged) {
            const auto from = std::max(begin, region.begin);
            const auto to = std::min(end, region.end);
            if (from >= to) continue;
            if (!recording) {
                commands = recorder.Commands();
                // The shader's stores into the buffer (whichever stage made them) precede the copy.
                RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
                recording = true;
            }
            // Exactly the sub-range the shader may write, in place in the import: what the shader
            // would have stored there itself had the range been bindable.
            CopyBuffer(context, commands, region.buffer->Handle(), from - region.begin, region.copySource, from - region.copySourceBase, to - from);
            recorder.Keep(region.buffer);
            if (profile) {
                Copies().gpuCopyBacks.fetch_add(1, std::memory_order_relaxed);
                Copies().gpuCopyBackBytes.fetch_add(to - from, std::memory_order_relaxed);
            }
        }
        // Only once its copies are recorded: a throw above leaves the region counted by
        // HasCopiedWrites, so the caller still registers the CPU write-back that stores the
        // staging bytes, instead of losing the shader's results.
        region.copiedBack = true;
    }
    if (!recording) return;
    // As a GPU label store or fill: visible to everything recorded after (shaders, transfers, an
    // indirect dispatch's arguments) and to the host once the batch completed; the caller's
    // pending-write note makes CPU readers wait for that.
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_HOST_READ_BIT);
}

VkDescriptorBufferInfo GuestBufferMemory::Descriptor(std::uint64_t address, std::size_t bytes) const {
    Require(uploaded && !committed, "guest GPU memory is not available");
    Require(bytes != 0 && bytes <= std::numeric_limits<std::uint64_t>::max() - address, "invalid guest buffer view");
    const auto found = std::upper_bound(regions.begin(), regions.end(), address, [](std::uint64_t value, const Region& region) { return value < region.begin; });
    Require(found != regions.begin(), "guest buffer has no GPU owner");
    const auto& region = *std::prev(found);
    Require(address >= region.begin && address + bytes <= region.end && (region.buffer != nullptr || region.direct != nullptr || region.mirror != nullptr), "guest buffer view exceeds its GPU owner");
    const auto base = region.direct != nullptr ? region.direct->base : region.mirror != nullptr ? region.mirror->base : region.begin;
    const auto offset = address - base;
    Require(context.limits.minStorageBufferOffsetAlignment != 0 && offset % context.limits.minStorageBufferOffsetAlignment == 0, "guest buffer view violates storage buffer offset alignment");
    Require(bytes <= context.limits.maxStorageBufferRange, "guest buffer view exceeds descriptor range limit");
    const auto handle = region.direct != nullptr ? region.direct->buffer : region.mirror != nullptr ? region.mirror->buffer->Handle() : region.buffer->Handle();
    return {handle, offset, bytes};
}

std::vector<ShaderRecompiler::BdaAbi::Range> GuestBufferMemory::AddressRanges() const {
    Require(uploaded && !committed, "guest GPU address ranges are not available");
    std::vector<ShaderRecompiler::BdaAbi::Range> result;
    for (const auto& region : regions) {
        Require(region.buffer != nullptr || region.direct != nullptr || region.mirror != nullptr, "incomplete guest GPU upload");
        const auto address = region.direct != nullptr ? region.direct->address + (region.begin - region.direct->base) : region.mirror != nullptr ? region.mirror->buffer->DeviceAddress() + (region.begin - region.mirror->base) : region.buffer->DeviceAddress();
        Require(region.end - region.begin <= std::numeric_limits<std::uint64_t>::max() - address, "GPU address range overflow");
        result.push_back({region.begin, region.end, address, ShaderRecompiler::BdaAbi::Read, 0});
    }
    return result;
}

bool GuestBufferMemory::HasCopiedWrites() const {
    for (const auto& [begin, end] : writes) {
        const auto found = std::upper_bound(regions.begin(), regions.end(), begin, [](auto address, const auto& region) { return address < region.begin; });
        if (found == regions.begin()) continue;
        const auto& region = *std::prev(found);
        if (region.direct != nullptr) continue;
        // A GPU copy whose written sub-ranges were copied back by the GPU lands like a direct
        // write; until RecordCopyBacks ran it still counts (a use without it stores in WriteBack).
        if (region.gpuCopy && region.copiedBack) continue;
        // Writes into a read-only image mirror are never stored (its pages could not take them).
        if (region.mirror != nullptr && !region.mirror->writable) continue;
        return true;
    }
    return false;
}

void GuestBufferMemory::MarkDirectWrites() const {
    for (const auto& [begin, end] : writes) {
        const auto found = std::upper_bound(regions.begin(), regions.end(), begin, [](auto address, const auto& region) { return address < region.begin; });
        if (found == regions.begin()) continue;
        const auto& region = *std::prev(found);
        if (region.direct != nullptr || (region.gpuCopy && region.copiedBack)) GuestMemory::MarkWritten(begin, static_cast<std::size_t>(end - begin));
    }
}

void GuestBufferMemory::WriteBack() {
    Require(uploaded && !committed, "guest memory cannot be committed twice or before upload");
    std::sort(writes.begin(), writes.end());
    std::vector<std::pair<std::uint64_t, std::uint64_t>> merged;
    for (const auto& range : writes) {
        if (!merged.empty() && range.first < merged.back().second) merged.back().second = std::max(merged.back().second, range.second);
        else merged.push_back(range);
    }
    // The CPU keeps running while the GPU works, so storing whole ranges would roll back its writes to
    // bytes the shader never touched. Only runs that differ from the uploaded copy are stored.
    for (const auto& [begin, end] : merged) {
        const auto found = std::upper_bound(regions.begin(), regions.end(), begin, [](auto address, const auto& region) { return address < region.begin; });
        Require(found != regions.begin(), "write-back range has no GPU owner");
        const auto& region = *std::prev(found);
        // Every branch below but the direct and copied-back ones stores from the CPU.
        if (region.direct == nullptr && !(region.gpuCopy && region.copiedBack) && !(region.mirror != nullptr && !region.mirror->writable)) Recorder::NoteWrittenBack(begin, static_cast<std::size_t>(end - begin));
        if (region.direct != nullptr) {
            // The GPU wrote imported guest memory directly; page write watching did not see it.
            GuestMemory::MarkWritten(begin, static_cast<std::size_t>(end - begin));
            continue;
        }
        if (region.mirror != nullptr) {
            if (!region.mirror->writable) continue;
            Require(end <= region.end, "write-back range exceeds its GPU owner");
            // The shadow is what the GPU started from; it then takes the mirror's bytes so the next
            // refresh does not mistake the GPU's results for a guest change.
            auto& mirror = *region.mirror;
            const auto offset = static_cast<std::size_t>(begin - mirror.base);
            const auto length = static_cast<std::size_t>(end - begin);
            const auto current = mirror.buffer->Bytes().subspan(offset, length);
            GuestMemory::WriteChanged(begin, current, std::span<const std::byte>(mirror.shadow).subspan(offset, length));
            std::memcpy(mirror.shadow.data() + offset, current.data(), length);
            continue;
        }
        if (region.gpuCopy) {
            Require(region.buffer != nullptr && end <= region.end, "write-back range exceeds its GPU owner");
            if (region.copiedBack) {
                // The GPU copied the written sub-range back into the import; page write watching
                // did not see it.
                GuestMemory::MarkWritten(begin, static_cast<std::size_t>(end - begin));
                continue;
            }
            // No copy-back was recorded (a synchronous draw): the staging bytes of the written
            // sub-range are stored whole, which is what the copy-back would have done. There is no
            // reference copy to diff against (the copy in never passed through the CPU).
            static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
            GuestMemory::Write(begin, region.buffer->Bytes().subspan(static_cast<std::size_t>(begin - region.begin), static_cast<std::size_t>(end - begin)));
            if (profile) Copies().stagingStores.fetch_add(1, std::memory_order_relaxed);
            continue;
        }
        Require(region.buffer != nullptr && region.writable && end <= region.end, "write-back range exceeds its GPU owner");
        static const bool traceBig = std::getenv("APS5_TRACE_BIGBUF") != nullptr;
        if (traceBig && end - begin >= (1u << 20u)) std::fprintf(stderr, "[bigbuf] writeback 0x%llx+0x%llx sparse=%d\n", static_cast<unsigned long long>(begin), static_cast<unsigned long long>(end - begin), region.sparse ? 1 : 0);
        if (!region.sparse) {
            const auto first = static_cast<std::size_t>(begin - region.begin);
            const auto length = static_cast<std::size_t>(end - begin);
            GuestMemory::WriteChanged(begin, region.buffer->Bytes().subspan(first, length), std::span<const std::byte>(region.uploaded).subspan(first, length));
            continue;
        }
        std::size_t packed = 0;
        for (const auto& [first, last] : region.backed) {
            const auto from = std::max(first, begin);
            const auto to = std::min(last, end);
            if (from < to) {
                const auto length = static_cast<std::size_t>(to - from);
                GuestMemory::WriteChanged(from, region.buffer->Bytes().subspan(static_cast<std::size_t>(from - region.begin), length), std::span<const std::byte>(region.uploaded).subspan(packed + static_cast<std::size_t>(from - first), length));
            }
            packed += static_cast<std::size_t>(last - first);
        }
    }
    committed = true;
    lease.clear();
}

std::optional<std::vector<std::pair<std::uint64_t, std::uint64_t>>> GuestBufferMemory::DirectRegions() const {
    if (!uploaded || committed) return std::nullopt;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> result;
    for (const auto& region : regions) {
        // A read-only mirror is as fixed as an import: its bytes and VkBuffer never change while its
        // Range exists (see ImageMirrorSerial). A writable one is refreshed and written back per build.
        const bool fixedMirror = region.mirror != nullptr && !region.mirror->writable;
        if (region.direct == nullptr && !fixedMirror) return std::nullopt;
        result.emplace_back(region.begin, region.end);
    }
    return result;
}

std::uint64_t ImageMirrorSerial(const Context& context, std::uint64_t address, std::size_t bytes) {
    if (bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address || !mirrorsEnabled()) return 0;
    // A replaced Range expires the mirror (findMirror), and a replacement mirror has a new serial.
    const auto mirror = findMirror(context, address, address + bytes);
    return mirror != nullptr && !mirror->writable ? mirror->serial : 0;
}

std::uint64_t HostImportSerial(const Context& context, std::uint64_t address, std::size_t bytes, bool reconcile) {
    if (context.hostImportAlignment == 0 || bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address) return 0;
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    // An import whose registered range changed must not be reused: reconcile first, as an upload
    // does (the walk only runs when the registry changed since the last one).
    if (reconcile && GuestAllocations::GuestAllocationsGeneration_nid_postfix() != state.refreshedGeneration) static_cast<void>(refreshImports(context, state, GuestAllocations::GuestAllocationsAcquire_nid_postfix()));
    auto found = state.imports.upper_bound(address);
    if (found == state.imports.begin()) return 0;
    --found;
    auto& entry = found->second;
    if (address < entry.base || address + bytes > entry.base + entry.bytes) return 0;
    // Serials are handed out on first use: a fresh import starts at 0, so one made after an earlier
    // import of the same range was dropped can never repeat that import's serial.
    static std::uint64_t serials = 0;
    if (entry.serial == 0) entry.serial = ++serials;
    return entry.serial;
}

}
