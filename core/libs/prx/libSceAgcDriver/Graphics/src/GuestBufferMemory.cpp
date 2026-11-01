#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/AdaptiveStaging.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "ThreadOwned.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SnapshotCopies.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BdaResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ResidentMirror.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/UnitShadow.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libc/include/CpuTopology.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libc/include/general/AtomicSharedPtr.hpp"
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <linux/udmabuf.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <array>
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
    bool heap = false;
    bool joinedHeap = false;
    bool sparse = false;
    std::uint64_t reserved = 0;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> committed;
    std::vector<std::uint64_t> generations;
    std::weak_ptr<const GuestAllocations::Range> range;
    std::shared_ptr<Buffer> buffer;
    std::shared_ptr<ResidentBuffer> resident;
    std::uint64_t accounted = 0;
    bool listed = false;
    // Writable ranges only: the guest bytes the mirror holds, compared on refresh and at write-back.
    std::vector<std::byte> shadow;
    // Identity for the life of this mirror (see ImageMirrorSerial); the top bit keeps mirror serials
    // apart from host import serials.
    std::uint64_t serial = 0;
    std::atomic<std::uint64_t> watchedSerial{0};
    std::uint64_t lastDispatch = 0;
};

// The address space. An address-based build maps every readable registered range; building that
// list per build (the lease's ~1200 shared_ptrs, an import lookup per range, the sort and merge, the
// upload loop, the BDA table) repeated the same result while the registry stood still. One space is
// cached per device and taken by every build made while it is current: a pure function of the
// registry generation read BEFORE its lease was acquired (a mutation bumps the generation before it
// releases the registry, so an unchanged generation means the lease is the one a build would
// acquire now), the import registry's epoch (bumped by every retire, so the `direct` pointers of
// its regions stand) and the device. `base` holds the ranges served in place (imports, mirrors),
// sorted and free of per-build state; the ranges copied per build are `copied`, re-added to each
// build's own regions, as are the V#s and snapshots outside the space. The space's lease pins its
// ranges while any build, batch or the cache holds it; the registry's pin waiter (WaitForLeases)
// drops the cache's reference first, lock-free, so a guest free of a range held only by the cache
// costs no GPU wait. APS5_NO_ADDRESS_SPACE_CACHE=1 restores the per-build path.
struct GuestBufferMemory::AddressSpace {
    std::uint64_t generation = 0;
    std::uint64_t importsEpoch = 0;
    VkDevice device = VK_NULL_HANDLE;
    // Identity for the life of this space (see CachedAddressTable).
    std::uint64_t serial = 0;
    GuestAllocations::Lease lease;
    std::vector<Region> base;
    struct HeapRun {
        std::uint64_t begin;
        std::uint64_t bytes;
        std::size_t first;
        std::size_t last;
    };
    bool mirrored = false;
    std::vector<std::size_t> writableMirrors;
    std::vector<ImageMirror*> heapMirrors;
    std::vector<HeapRun> heapRuns;
    std::vector<ImageMirror*> sparseMirrors;
    std::unique_ptr<std::atomic<std::uint64_t>[]> verified;
    std::vector<CopiedRange> copied;
    // The BDA table entries of `base`, in its order.
    std::vector<ShaderRecompiler::BdaAbi::Range> ranges;
    std::shared_ptr<const std::vector<std::pair<std::uint64_t, std::uint64_t>>> directReads;
    mutable AgcDriver::Mutex tableMutex;
    struct Table {
        std::uint64_t serial;
        std::shared_ptr<const std::vector<ShaderRecompiler::BdaAbi::Range>> ranges;
        std::list<const std::vector<std::uint64_t>*>::iterator recent;
    };
    mutable std::map<std::vector<std::uint64_t>, Table> tables;
    mutable std::list<const std::vector<std::uint64_t>*> tableRecency;
};

std::atomic<std::uint64_t> testHeapMirrorBudget{0};

void SetHeapMirrorBudget(std::uint64_t bytes) {
    testHeapMirrorBudget.store(bytes, std::memory_order_release);
}

namespace {
struct RefreshBlock;
static thread_local GuestBufferMemory* buildingBufferMemory = nullptr;
static thread_local std::vector<RefreshBlock>* buildingBlocks = nullptr;

struct RetiredImport;

struct AddressSpaceCache {
    AtomicSharedPtr<const GuestBufferMemory::AddressSpace> current;
    std::atomic<std::uint64_t> serials{0};
    // Set by the waiter's drop, cleared by the next publish: the rebuild's reason.
    std::atomic<bool> droppedByWaiter{false};
    std::atomic<std::uint64_t> hits{0};
    std::atomic<std::uint64_t> rebuiltFirst{0};
    std::atomic<std::uint64_t> rebuiltGeneration{0};
    std::atomic<std::uint64_t> rebuiltEpoch{0};
    std::atomic<std::uint64_t> rebuiltDevice{0};
    std::atomic<std::uint64_t> rebuiltWaiterDrop{0};
    std::atomic<std::uint64_t> unpublished{0};
    std::atomic<std::uint64_t> dissolvedOverlap{0};
    std::atomic<std::uint64_t> dissolvedImports{0};
    std::atomic<std::uint64_t> waiterDrops{0};
};

AddressSpaceCache& Spaces() {
    static AddressSpaceCache cache;
    return cache;
}

bool addressSpaceCacheEnabled() {
    static const bool disabled = std::getenv("APS5_NO_ADDRESS_SPACE_CACHE") != nullptr;
    return !disabled;
}

struct ImportFailure {
    std::uint64_t base;
    std::uint64_t bytes;
    GuestAllocations::Mapped ranges;
};

struct ImportRefusal {
    std::uint64_t base;
    std::uint64_t bytes;
    GuestAllocations::Mapped ranges;
    std::uint64_t liveBytes;
    std::uint64_t budget;
};

struct GateRefusal {
    std::uint64_t base;
    std::uint64_t bytes;
    GuestAllocations::Mapped ranges;
    std::uint64_t commits;
    std::uint64_t watches;
};

struct EvictingRefusal {
    std::uint64_t base;
    std::uint64_t bytes;
    GuestAllocations::Mapped ranges;
    std::uint64_t epoch;
    std::uint64_t frames;
    std::uint64_t liveBytes;
    std::uint64_t budget;
    std::uint64_t room;
    std::uint64_t window;
    std::uint64_t floor;
    std::uint64_t ready;
    std::uint64_t readyHeld;
    bool joined;
};

#ifdef _WIN32
constexpr std::size_t settleBatchCount = 4;
constexpr std::uint32_t settleBatchCopies = 256;

struct SettleBatch {
    VkCommandBuffer commands = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
    std::vector<std::uint64_t> bases;
    std::vector<VkBuffer> aliases;
    std::vector<std::shared_ptr<RetiredImport>> retired;
    bool busy = false;
};

enum class SettlePhase : std::uint8_t { Wanted, Submitted, Settled };

struct PendingSettle {
    SettlePhase phase = SettlePhase::Wanted;
    std::size_t batch = 0;
    VkDevice device = VK_NULL_HANDLE;
};
#endif

struct ImportSettle {
    VkBuffer settleTarget = VK_NULL_HANDLE;
    VkDeviceMemory settleMemory = VK_NULL_HANDLE;
    VkCommandBuffer settleCommands = VK_NULL_HANDLE;
    VkFence settleFence = VK_NULL_HANDLE;
    PFN_vkDestroyFence destroyFence = nullptr;
    PFN_vkDestroyBuffer settleDestroyBuffer = nullptr;
    PFN_vkFreeMemory settleFreeMemory = nullptr;
#ifdef _WIN32
    PFN_vkWaitForFences waitForFences = nullptr;
    VkBuffer batchTarget = VK_NULL_HANDLE;
    VkDeviceMemory batchMemory = VK_NULL_HANDLE;
    std::array<SettleBatch, settleBatchCount> batches{};
#endif
};

// Imports persist across draws, keyed by allocation base, and are dropped when their allocation leaves
// the registered set; a dropped import is destroyed once the recorded work that may read it completed.
struct RefreshedRange {
    std::uint64_t address;
    std::uint64_t bytes;
    std::weak_ptr<const GuestAllocations::Range> range;
};

struct HostImports {
    AgcDriver::Mutex mutex;
    VkDevice device = VK_NULL_HANDLE;
    PFN_vkDestroyBuffer destroyBuffer = nullptr;
    PFN_vkFreeMemory freeMemory = nullptr;
    std::map<std::uint64_t, HostImport> imports;
    std::map<VkDevice, std::vector<std::shared_ptr<RetiredImport>>> parked;
    std::uint64_t liveBytes = 0;
    std::map<std::uint64_t, ImportFailure> failed;
    std::map<std::uint64_t, ImportFailure> failedJoins;
    std::map<std::uint64_t, std::uint64_t> sparseRunEnds;
    std::map<std::uint64_t, std::uint32_t> sparseRunJoins;
    std::set<std::pair<std::uint64_t, std::size_t>> importedAtMap;
    std::map<std::pair<std::uint64_t, std::uint64_t>, ImportFailure> failedBlocks;
    std::map<std::pair<std::uint64_t, std::uint64_t>, ImportRefusal> refusals;
    std::map<std::pair<std::uint64_t, std::uint64_t>, EvictingRefusal> evictingRefusals;
    std::map<std::pair<std::uint64_t, std::uint64_t>, GateRefusal> gateRefusals;
    // Registry generation the imports were last reconciled with.
    std::uint64_t refreshedGeneration = 0;
    std::vector<RefreshedRange> refreshedRanges;
    std::uint64_t refreshedWindow = 0;
    std::uint64_t refreshedBudget = 0;
    std::set<std::uint64_t> pendingImports;
    // Bumped whenever an import is dropped, so HostImport pointers taken under the lock earlier can be
    // reused while it is unchanged.
    std::uint64_t epoch = 1;
    VkDevice watchDevice = VK_NULL_HANDLE;
    bool unwatchImports = false;
    bool unwatchDmaBufImports = false;
    bool watchPrivateImports = false;
    std::map<VkDevice, ImportSettle> settles;
#ifdef _WIN32
    std::map<std::uint64_t, PendingSettle> pendingSettles;
    std::vector<std::uint64_t> settleWanted;
    std::vector<std::uint64_t> settleReady;
    std::chrono::steady_clock::time_point settlePolled{};
    std::uint64_t settlesSubmitted = 0;
    std::uint64_t settlesWaited = 0;
    std::uint64_t settlesRewatched = 0;
    std::uint64_t settlesKeptCompared = 0;
    std::uint64_t settlesParked = 0;
    std::uint64_t settleUnmapWaits = 0;
#endif
    std::uint64_t useCounter = 0;
    std::uint64_t buildFloor = 0;
    std::uint64_t frameFloor = 0;
    std::uint64_t previousFrameFloor = 0;
    std::uint64_t framesEnded = 0;
    bool mirrorsYield = false;
    std::array<HostImportRequesterCounts, ImportRequesterCount> counts{};
    VkPhysicalDevice reportedDevice = VK_NULL_HANDLE;
    std::chrono::steady_clock::time_point reportedAt{};
    std::uint64_t reportedBudget = 0;
    std::uint32_t reportedHeap = 0;
    std::uint64_t reportedHeapBudget = 0;
    std::uint64_t reportedHeapUsage = 0;
    std::uint64_t ceiling = 0;
    std::uint32_t allocationFailures = 0;
    bool allocationFailed = false;
    std::uint8_t reportNoted = 0;
    std::map<std::uint64_t, std::uint64_t> spaceUses;
    std::uint64_t budgetRefusals = 0;
    std::array<const HostImport*, 4> recentImports{};
    std::uint64_t recentEpoch = std::numeric_limits<std::uint64_t>::max();
    std::size_t recentSize = 0;
    std::size_t recentNext = 0;
};

HostImports& Imports() {
    static HostImports imports;
    return imports;
}

bool recentImportsEnabled() {
    static const bool enabled = std::getenv("APS5_NO_RECENT_IMPORTS") == nullptr;
    return enabled;
}

const HostImport* recentImport(HostImports& state, std::uint64_t begin, std::uint64_t end) {
    if (state.recentEpoch != state.epoch || state.recentSize != state.imports.size()) return nullptr;
    for (const auto* entry : state.recentImports) {
        if (entry != nullptr && begin >= entry->base && end <= entry->base + entry->bytes) {
            entry->lastUse = ++state.useCounter;
            return entry;
        }
    }
    return nullptr;
}

void rememberImport(HostImports& state, const HostImport* entry) {
    if (state.recentEpoch != state.epoch || state.recentSize != state.imports.size()) {
        state.recentImports = {};
        state.recentEpoch = state.epoch;
        state.recentSize = state.imports.size();
    }
    state.recentImports[state.recentNext++ % state.recentImports.size()] = entry;
}

#ifdef _WIN32
void waitSettleBatch(const ImportSettle& settle, VkDevice device, const SettleBatch& batch) {
    if (batch.busy && settle.waitForFences != nullptr) Check(settle.waitForFences(device, 1, &batch.fence, VK_TRUE, 60'000'000'000ull), "vkWaitForFences (settle batch)");
}

bool settleBatchRunning(const ImportSettle& settle, VkDevice device, const SettleBatch& batch) {
    return batch.busy && settle.waitForFences != nullptr && settle.waitForFences(device, 1, &batch.fence, VK_TRUE, 0) == VK_TIMEOUT;
}

SettleBatch* forgetSettle(HostImports& state, std::uint64_t base) {
    const auto found = state.pendingSettles.find(base);
    if (found == state.pendingSettles.end()) return nullptr;
    SettleBatch* running = nullptr;
    if (const auto settle = state.settles.find(found->second.device); found->second.phase == SettlePhase::Submitted && settle != state.settles.end()) {
        auto& batch = settle->second.batches[found->second.batch];
        if (batch.busy) running = &batch;
    }
    state.pendingSettles.erase(found);
    return running;
}

void drainSettles(HostImports& state) {
    for (auto& [device, settle] : state.settles) {
        for (auto& batch : settle.batches) {
            waitSettleBatch(settle, device, batch);
            for (const auto alias : batch.aliases) settle.settleDestroyBuffer(device, alias, nullptr);
            batch.aliases.clear();
            batch.retired.clear();
            batch.bases.clear();
            batch.busy = false;
        }
    }
    state.pendingSettles.clear();
    state.settleWanted.clear();
    state.settleReady.clear();
}

void releaseSettleBatches(VkDevice device, ImportSettle& settle) {
    for (auto& batch : settle.batches) {
        waitSettleBatch(settle, device, batch);
        for (const auto alias : batch.aliases) settle.settleDestroyBuffer(device, alias, nullptr);
        if (batch.fence != VK_NULL_HANDLE) settle.destroyFence(device, batch.fence, nullptr);
        batch = SettleBatch{};
    }
    if (settle.batchTarget != VK_NULL_HANDLE) settle.settleDestroyBuffer(device, settle.batchTarget, nullptr);
    if (settle.batchMemory != VK_NULL_HANDLE) MemoryAccounting::Free(settle.settleFreeMemory, device, settle.batchMemory);
    settle.batchTarget = VK_NULL_HANDLE;
    settle.batchMemory = VK_NULL_HANDLE;
}
#endif

void destroyImport(const Context& context, const HostImport& entry) {
    context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(context.device, entry.buffer, nullptr);
    MemoryAccounting::Free(context, entry.memory);
#ifdef _WIN32
    GuestArena::GuestArenaUnmapAlias_nid_postfix(entry.alias);
#endif
}

using ImportNode = std::map<std::uint64_t, HostImport>::node_type;

// Frees a dropped import's Vulkan objects when it is released. Never copied: a copy would destroy the
// same handles twice (and a temporary would destroy them at once).
struct RetiredImport {
    RetiredImport(VkDevice device, PFN_vkDestroyBuffer destroyBuffer, PFN_vkFreeMemory freeMemory, ImportNode node)
        : device(device), destroyBuffer(destroyBuffer), freeMemory(freeMemory), node(std::move(node)) {}
    RetiredImport(const RetiredImport&) = delete;
    RetiredImport& operator=(const RetiredImport&) = delete;
    ~RetiredImport() {
        const auto& entry = node.mapped();
        destroyBuffer(device, entry.buffer, nullptr);
        MemoryAccounting::Free(freeMemory, device, entry.memory);
#ifdef _WIN32
        GuestArena::GuestArenaUnmapAlias_nid_postfix(entry.alias);
#endif
    }
    VkDevice device;
    PFN_vkDestroyBuffer destroyBuffer;
    PFN_vkFreeMemory freeMemory;
    ImportNode node;
};

const GuestAllocations::Range* containingRange(const GuestAllocations::Lease& lease, std::uint64_t begin, std::uint64_t end);
bool runCovers(const GuestAllocations::Lease& lease, std::uint64_t begin, std::uint64_t end);
bool joinedCovers(const GuestAllocations::Lease& lease, std::uint64_t begin, std::uint64_t end);

void retireImport(VkDevice device, HostImports& state, std::map<std::uint64_t, HostImport>::iterator it, const GuestAllocations::Lease& lease, bool hold = true) {
#ifdef _WIN32
    auto* reading = forgetSettle(state, it->first);
#endif
    state.pendingImports.erase(it->first);
    // Results shadowed for the import reach its (old) buffer first where the memory is still a
    // readable registered range (the import retires because its registration vanished or changed
    // size); the holder below outlives the batch that copies them.
    RetireShadow(device, it->second, [&lease, joined = it->second.joined](std::uint64_t begin, std::uint64_t end) { return runCovers(lease, begin, end) || (joined && joinedCovers(lease, begin, end)); });
    ++state.epoch;
    state.liveBytes -= it->second.pinnedBytes;
    auto holder = std::make_shared<RetiredImport>(state.device, state.destroyBuffer, state.freeMemory, state.imports.extract(it));
#ifdef _WIN32
    if (reading != nullptr) {
        reading->retired.push_back(holder);
        ++state.settlesParked;
    }
#endif
    auto* recorder = Recorder::Active();
    if (recorder == nullptr) return;
    if (hold) recorder->KeepThroughHold(std::move(holder));
    else if (!recorder->Idle()) recorder->Keep(std::move(holder));
}

bool releaseParkedImports(HostImports& state, std::uint64_t begin, std::uint64_t end) {
    std::vector<std::shared_ptr<RetiredImport>> released;
    {
        std::lock_guard lock(state.mutex);
        for (auto device = state.parked.begin(); device != state.parked.end();) {
            auto& list = device->second;
            for (auto it = list.begin(); it != list.end();) {
                if ((*it)->node.mapped().base < end && begin < (*it)->node.mapped().base + (*it)->node.mapped().bytes) {
                    released.push_back(std::move(*it));
                    it = list.erase(it);
                } else {
                    ++it;
                }
            }
            device = list.empty() ? state.parked.erase(device) : std::next(device);
        }
    }
    return !released.empty();
}

#ifdef _WIN32
bool releaseSettleRetired(HostImports& state, std::uint64_t begin, std::uint64_t end) {
    std::vector<std::shared_ptr<RetiredImport>> released;
    {
        std::lock_guard lock(state.mutex);
        for (auto& [device, settle] : state.settles) {
            for (auto& batch : settle.batches) {
                const bool overlaps = std::any_of(batch.retired.begin(), batch.retired.end(), [&](const std::shared_ptr<RetiredImport>& holder) { return holder->node.mapped().base < end && begin < holder->node.mapped().base + holder->node.mapped().bytes; });
                if (!overlaps) continue;
                if (settleBatchRunning(settle, device, batch)) {
                    ++state.settleUnmapWaits;
                    waitSettleBatch(settle, device, batch);
                }
                for (const auto alias : batch.aliases) settle.settleDestroyBuffer(device, alias, nullptr);
                batch.aliases.clear();
                for (auto& holder : batch.retired) released.push_back(std::move(holder));
                batch.retired.clear();
            }
        }
    }
    return !released.empty();
}
#endif

#ifndef _WIN32
struct ImportFamilies {
    std::array<std::uint32_t, 3> indices{};
    std::uint32_t count = 0;
};

// A dma-buf import's buffer is used on the main queue, on the settle queue (settleImport) and, when
// sparse, on the sparse queue.
void shareDmaBufImport(const Context& context, VkBufferCreateInfo& info, ImportFamilies& families, bool sparse) {
    const auto add = [&](std::uint32_t family) {
        if (std::find(families.indices.begin(), families.indices.begin() + families.count, family) == families.indices.begin() + families.count) families.indices[families.count++] = family;
    };
    add(context.queueFamily);
    if (context.settleQueue != VK_NULL_HANDLE) add(context.settleFamily);
    if (sparse && context.sparseQueue != VK_NULL_HANDLE) add(context.sparseFamily);
    const bool concurrent = families.count > 1;
    info.sharingMode = concurrent ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
    info.queueFamilyIndexCount = concurrent ? families.count : 0u;
    info.pQueueFamilyIndices = concurrent ? families.indices.data() : nullptr;
}
#endif

VkResult createImportBuffer(const Context& context, VkExternalMemoryHandleTypeFlagBits handleType, VkDeviceSize size, VkBuffer* buffer) {
    const VkExternalMemoryBufferCreateInfo external{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO, nullptr, static_cast<VkExternalMemoryHandleTypeFlags>(handleType)};
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, &external};
    info.size = size;
    info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    return context.Function<PFN_vkCreateBuffer>("vkCreateBuffer")(context.device, &info, nullptr, buffer);
}

const char* bindImport(const Context& context, HostImport& entry, VkExternalMemoryHandleTypeFlagBits handleType, const void* import, std::uint32_t importTypes, bool& allocated, VkResult& failure) {
    const auto failed = [&](const char* step, VkResult result) -> const char* {
        if (entry.buffer) context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(context.device, entry.buffer, nullptr);
        if (allocated && entry.memory) MemoryAccounting::Free(context, entry.memory);
        entry.buffer = VK_NULL_HANDLE;
        entry.memory = VK_NULL_HANDLE;
        failure = result;
        return step;
    };
    allocated = false;
    const VkExternalMemoryBufferCreateInfo external{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO, nullptr, static_cast<VkExternalMemoryHandleTypeFlags>(handleType)};
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, &external};
    info.size = entry.bytes;
    // INDIRECT_BUFFER: DISPATCH_INDIRECT group counts are read in place (VulkanDevice::DispatchIndirect).
    info.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
#ifndef _WIN32
    ImportFamilies families;
    if (handleType == VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT) shareDmaBufImport(context, info, families, false);
#endif
    VkBuffer buffer = VK_NULL_HANDLE;
    if (const auto result = context.Function<PFN_vkCreateBuffer>("vkCreateBuffer")(context.device, &info, nullptr, &buffer); result != VK_SUCCESS) return failed("vkCreateBuffer", result);
    entry.buffer = buffer;
    VkMemoryRequirements requirements{};
    context.Function<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(context.device, entry.buffer, &requirements);
    const auto types = requirements.memoryTypeBits & importTypes;
    if (types == 0) return failed("memory type selection", VK_ERROR_FORMAT_NOT_SUPPORTED);
    const VkMemoryAllocateFlagsInfo flags{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, import, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, 0};
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &flags};
    allocation.allocationSize = entry.bytes;
    allocation.memoryTypeIndex = static_cast<std::uint32_t>(std::countr_zero(types));
    if (const auto result = MemoryAccounting::Allocate(context, allocation, "guest memory import", &entry.memory); result != VK_SUCCESS) return failed("vkAllocateMemory", result);
    allocated = true;
    entry.memoryType = allocation.memoryTypeIndex;
    if (const auto result = context.Function<PFN_vkBindBufferMemory>("vkBindBufferMemory")(context.device, entry.buffer, entry.memory, 0); result != VK_SUCCESS) return failed("vkBindBufferMemory", result);
    const VkBufferDeviceAddressInfo addressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, entry.buffer};
    entry.address = context.Function<PFN_vkGetBufferDeviceAddressKHR>("vkGetBufferDeviceAddressKHR")(context.device, &addressInfo);
    if (entry.address == 0) return failed("vkGetBufferDeviceAddressKHR", VK_ERROR_UNKNOWN);
    return nullptr;
}

const char* createHostPointerImport(const Context& context, HostImport& entry, VkResult& failure) {
    void* const host = entry.alias != nullptr ? entry.alias : reinterpret_cast<void*>(entry.base);
    VkMemoryHostPointerPropertiesEXT pointer{VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT};
    if (const auto result = context.Function<PFN_vkGetMemoryHostPointerPropertiesEXT>("vkGetMemoryHostPointerPropertiesEXT")(context.device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, host, &pointer); result != VK_SUCCESS) {
        failure = result;
        return "vkGetMemoryHostPointerPropertiesEXT";
    }
    const VkImportMemoryHostPointerInfoEXT import{VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT, nullptr, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, host};
    bool allocated = false;
    return bindImport(context, entry, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, &import, pointer.memoryTypeBits, allocated, failure);
}

#ifndef _WIN32
int udmabufDevice() {
    static const int device = [] {
        const int result = open("/dev/udmabuf", O_RDWR | O_CLOEXEC);
        if (result < 0) {
            const int error = errno;
            std::fprintf(stderr, "[gpu] open /dev/udmabuf: %s; shared direct memory is copied instead of imported, and GPU stores through FLAT/GLOBAL addresses do not reach it%s\n",
                std::strerror(error), error == EACCES ? " (give the user read-write access to /dev/udmabuf, for example through the kvm group)" : "");
        }
        return result;
    }();
    return device;
}

const char* createDmaBufImport(const Context& context, HostImport& entry, int file, std::uint64_t offset, VkResult& failure) {
    udmabuf_create request{};
    request.memfd = static_cast<std::uint32_t>(file);
    request.flags = UDMABUF_FLAGS_CLOEXEC;
    request.offset = offset;
    request.size = entry.bytes;
    const int device = udmabufDevice();
    const int buffer = device < 0 ? -1 : ioctl(device, UDMABUF_CREATE, &request);
    close(file);
    if (buffer < 0) {
        failure = VK_ERROR_INVALID_EXTERNAL_HANDLE;
        return device < 0 ? "open /dev/udmabuf" : "UDMABUF_CREATE";
    }
    VkMemoryFdPropertiesKHR properties{VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR};
    if (const auto result = context.Function<PFN_vkGetMemoryFdPropertiesKHR>("vkGetMemoryFdPropertiesKHR")(context.device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, buffer, &properties); result != VK_SUCCESS) {
        close(buffer);
        failure = result;
        return "vkGetMemoryFdPropertiesKHR";
    }
    const VkImportMemoryFdInfoKHR import{VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR, nullptr, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, buffer};
    bool allocated = false;
    const char* step = bindImport(context, entry, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, &import, properties.memoryTypeBits, allocated, failure);
    if (!allocated) close(buffer);
    entry.dmaBuf = step == nullptr;
    return step;
}
#endif

#ifndef _WIN32
constexpr std::uint64_t ImportChunkBytes = std::uint64_t{64} << 20u;
constexpr VkBufferUsageFlags ChunkImportUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
constexpr std::uint64_t SparseBlockBytes = 65536;

struct ImportChunk {
    VkDevice device = VK_NULL_HANDLE;
    PFN_vkFreeMemory freeMemory = nullptr;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    std::uint32_t memoryType = 0;
    ImportChunk() = default;
    ImportChunk(const ImportChunk&) = delete;
    ImportChunk& operator=(const ImportChunk&) = delete;
    ~ImportChunk() {
        if (memory != VK_NULL_HANDLE) MemoryAccounting::Free(freeMemory, device, memory);
    }
};

struct ImportChunks {
    AgcDriver::Mutex mutex;
    VkDevice device = VK_NULL_HANDLE;
    std::map<std::tuple<dev_t, ino_t, std::uint64_t>, std::weak_ptr<ImportChunk>> chunks;
};

ImportChunks& Chunks() {
    static ImportChunks chunks;
    return chunks;
}

std::shared_ptr<ImportChunk> importChunk(const Context& context, int file, std::uint64_t index, std::uint32_t bufferTypes, const char*& step, VkResult& failure) {
    struct stat info {};
    if (fstat(file, &info) != 0) {
        failure = VK_ERROR_INVALID_EXTERNAL_HANDLE;
        step = "fstat of the shared backing";
        return nullptr;
    }
    auto& chunks = Chunks();
    std::lock_guard lock(chunks.mutex);
    if (chunks.device != context.device) {
        chunks.chunks.clear();
        chunks.device = context.device;
    }
    const auto key = std::make_tuple(info.st_dev, info.st_ino, index);
    if (const auto found = chunks.chunks.find(key); found != chunks.chunks.end()) {
        if (auto chunk = found->second.lock()) return chunk;
        chunks.chunks.erase(found);
    }
    if (static_cast<std::uint64_t>(info.st_size) < (index + 1) * ImportChunkBytes) {
        failure = VK_ERROR_INVALID_EXTERNAL_HANDLE;
        step = "a chunk past the end of the shared backing";
        return nullptr;
    }
    udmabuf_create request{};
    request.memfd = static_cast<std::uint32_t>(file);
    request.flags = UDMABUF_FLAGS_CLOEXEC;
    request.offset = index * ImportChunkBytes;
    request.size = ImportChunkBytes;
    const int device = udmabufDevice();
    const int buffer = device < 0 ? -1 : ioctl(device, UDMABUF_CREATE, &request);
    if (buffer < 0) {
        failure = VK_ERROR_INVALID_EXTERNAL_HANDLE;
        step = device < 0 ? "open /dev/udmabuf" : "UDMABUF_CREATE";
        return nullptr;
    }
    VkMemoryFdPropertiesKHR properties{VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR};
    if (const auto result = context.Function<PFN_vkGetMemoryFdPropertiesKHR>("vkGetMemoryFdPropertiesKHR")(context.device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, buffer, &properties); result != VK_SUCCESS) {
        close(buffer);
        failure = result;
        step = "vkGetMemoryFdPropertiesKHR";
        return nullptr;
    }
    const auto types = properties.memoryTypeBits & bufferTypes;
    if (types == 0) {
        close(buffer);
        failure = VK_ERROR_FORMAT_NOT_SUPPORTED;
        step = "memory type selection";
        return nullptr;
    }
    const VkImportMemoryFdInfoKHR import{VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR, nullptr, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, buffer};
    const VkMemoryAllocateFlagsInfo flags{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, &import, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, 0};
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &flags};
    allocation.allocationSize = ImportChunkBytes;
    allocation.memoryTypeIndex = static_cast<std::uint32_t>(std::countr_zero(types));
    auto chunk = std::make_shared<ImportChunk>();
    if (const auto result = MemoryAccounting::Allocate(context, allocation, "dma-buf import", &chunk->memory); result != VK_SUCCESS) {
        close(buffer);
        failure = result;
        step = "vkAllocateMemory";
        return nullptr;
    }
    chunk->device = context.device;
    chunk->freeMemory = context.Function<PFN_vkFreeMemory>("vkFreeMemory");
    chunk->memoryType = allocation.memoryTypeIndex;
    for (auto it = chunks.chunks.begin(); it != chunks.chunks.end();) it = it->second.expired() ? chunks.chunks.erase(it) : std::next(it);
    chunks.chunks[key] = chunk;
    return chunk;
}

const char* createChunkedDmaBufImport(const Context& context, HostImport& entry, int file, std::uint64_t offset, bool& fits, VkResult& failure) {
    const auto index = offset / ImportChunkBytes;
    const auto within = offset - index * ImportChunkBytes;
    fits = entry.bytes != 0 && within + entry.bytes <= ImportChunkBytes;
    if (!fits) return nullptr;
    const VkExternalMemoryBufferCreateInfo external{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO, nullptr, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT};
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, &external};
    info.size = entry.bytes;
    info.usage = ChunkImportUsage;
    ImportFamilies families;
    shareDmaBufImport(context, info, families, false);
    VkBuffer buffer = VK_NULL_HANDLE;
    if (const auto result = context.Function<PFN_vkCreateBuffer>("vkCreateBuffer")(context.device, &info, nullptr, &buffer); result != VK_SUCCESS) {
        failure = result;
        return "vkCreateBuffer";
    }
    const auto destroy = [&] { context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(context.device, buffer, nullptr); };
    VkMemoryRequirements requirements{};
    context.Function<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(context.device, buffer, &requirements);
    if (requirements.alignment == 0 || within % requirements.alignment != 0 || within + requirements.size > ImportChunkBytes) {
        destroy();
        fits = false;
        return nullptr;
    }
    const char* step = nullptr;
    auto chunk = importChunk(context, file, index, requirements.memoryTypeBits, step, failure);
    if (chunk == nullptr) {
        destroy();
        return step;
    }
    if (((requirements.memoryTypeBits >> chunk->memoryType) & 1u) == 0) {
        destroy();
        fits = false;
        return nullptr;
    }
    if (const auto result = context.Function<PFN_vkBindBufferMemory>("vkBindBufferMemory")(context.device, buffer, chunk->memory, within); result != VK_SUCCESS) {
        destroy();
        failure = result;
        return "vkBindBufferMemory";
    }
    const VkBufferDeviceAddressInfo addressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, buffer};
    entry.buffer = buffer;
    entry.memory = VK_NULL_HANDLE;
    entry.address = context.Function<PFN_vkGetBufferDeviceAddressKHR>("vkGetBufferDeviceAddressKHR")(context.device, &addressInfo);
    entry.chunk = std::move(chunk);
    entry.dmaBuf = true;
    return nullptr;
}

constexpr VkBufferUsageFlags SparseImportUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;

[[noreturn]] void throwSparseImport(const HostImport& entry, const char* step, VkResult result) {
    char text[256];
    std::snprintf(text, sizeof(text), "AGC graphics: sparse dma-buf import of 0x%llx+0x%llx failed at %s (%d)", static_cast<unsigned long long>(entry.base), static_cast<unsigned long long>(entry.bytes), step, static_cast<int>(result));
    throw std::runtime_error(text);
}

VkMemoryRequirements dmaBufBufferRequirements(const Context& context, const HostImport& entry, VkBufferCreateFlags flags, VkBufferUsageFlags usage, VkDeviceSize size, VkBuffer* kept) {
    const VkExternalMemoryBufferCreateInfo external{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO, nullptr, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT};
    VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, &external};
    info.flags = flags;
    info.size = size;
    info.usage = usage;
    ImportFamilies families;
    shareDmaBufImport(context, info, families, (flags & VK_BUFFER_CREATE_SPARSE_BINDING_BIT) != 0);
    VkBuffer buffer = VK_NULL_HANDLE;
    if (const auto result = context.Function<PFN_vkCreateBuffer>("vkCreateBuffer")(context.device, &info, nullptr, &buffer); result != VK_SUCCESS) throwSparseImport(entry, "vkCreateBuffer", result);
    VkMemoryRequirements requirements{};
    context.Function<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(context.device, buffer, &requirements);
    if (kept != nullptr) {
        *kept = buffer;
    } else {
        context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(context.device, buffer, nullptr);
    }
    return requirements;
}

bool createSparseDmaBufImport(const Context& context, HostImport& entry, int file, std::uint64_t offset) {
    const auto lead = offset % SparseBlockBytes;
    offset -= lead;
    const bool ownQueue = context.sparseQueue != VK_NULL_HANDLE && context.sparseQueueMutex != nullptr;
    if (!context.sparseBinding || entry.bytes <= ImportChunkBytes || (!ownQueue && !GuestMemory::GpuMutex().HeldByThisThread()) || udmabufDevice() < 0) return false;
    struct stat backing {};
    if (fstat(file, &backing) != 0) throwSparseImport(entry, "fstat of the shared backing", VK_ERROR_INVALID_EXTERNAL_HANDLE);
    const auto chunkTypes = dmaBufBufferRequirements(context, entry, 0, ChunkImportUsage, ImportChunkBytes, nullptr).memoryTypeBits;
    VkBuffer buffer = VK_NULL_HANDLE;
    const auto requirements = dmaBufBufferRequirements(context, entry, VK_BUFFER_CREATE_SPARSE_BINDING_BIT, SparseImportUsage, lead + entry.bytes, &buffer);
    const auto alignment = requirements.alignment;
    const bool chosen = alignment != 0 && offset % alignment == 0 && ImportChunkBytes % alignment == 0 && (chunkTypes & ~requirements.memoryTypeBits) == 0 && (requirements.memoryTypeBits & chunkTypes) != 0 &&
        (offset + requirements.size + ImportChunkBytes - 1) / ImportChunkBytes * ImportChunkBytes <= static_cast<std::uint64_t>(backing.st_size);
    const auto fail = [&](const char* step, VkResult result) {
        context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(context.device, buffer, nullptr);
        throwSparseImport(entry, step, result);
    };
    if (!chosen) {
        context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(context.device, buffer, nullptr);
        return false;
    }
    auto chunks = std::make_shared<std::vector<std::shared_ptr<ImportChunk>>>();
    std::vector<VkSparseMemoryBind> binds;
    for (VkDeviceSize resource = 0; resource < requirements.size;) {
        const auto position = offset + resource;
        const auto index = position / ImportChunkBytes;
        const auto within = position - index * ImportChunkBytes;
        const auto size = std::min<VkDeviceSize>(ImportChunkBytes - within, requirements.size - resource);
        const char* step = nullptr;
        VkResult failure = VK_SUCCESS;
        auto chunk = importChunk(context, file, index, requirements.memoryTypeBits, step, failure);
        if (chunk == nullptr) fail(step, failure);
        if (((requirements.memoryTypeBits >> chunk->memoryType) & 1u) == 0) fail("the memory type of a chunk import", VK_ERROR_FORMAT_NOT_SUPPORTED);
        binds.push_back({resource, size, chunk->memory, within, 0});
        chunks->push_back(std::move(chunk));
        resource += size;
    }
    const VkSparseBufferMemoryBindInfo bufferBinds{buffer, static_cast<std::uint32_t>(binds.size()), binds.data()};
    VkBindSparseInfo bind{VK_STRUCTURE_TYPE_BIND_SPARSE_INFO};
    bind.bufferBindCount = 1;
    bind.pBufferBinds = &bufferBinds;
    const VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence = VK_NULL_HANDLE;
    if (const auto result = context.Function<PFN_vkCreateFence>("vkCreateFence")(context.device, &fenceInfo, nullptr, &fence); result != VK_SUCCESS) fail("vkCreateFence", result);
    VkResult bound = VK_SUCCESS;
    VkResult waited = VK_SUCCESS;
    {
        std::unique_lock<std::mutex> queueLock;
        if (ownQueue) queueLock = std::unique_lock(*context.sparseQueueMutex);
        bound = context.Function<PFN_vkQueueBindSparse>("vkQueueBindSparse")(ownQueue ? context.sparseQueue : context.queue, 1, &bind, fence);
        if (bound == VK_SUCCESS) waited = context.Function<PFN_vkWaitForFences>("vkWaitForFences")(context.device, 1, &fence, VK_TRUE, UINT64_MAX);
    }
    context.Function<PFN_vkDestroyFence>("vkDestroyFence")(context.device, fence, nullptr);
    if (bound != VK_SUCCESS) fail("vkQueueBindSparse", bound);
    if (waited != VK_SUCCESS) fail("vkWaitForFences", waited);
    const VkBufferDeviceAddressInfo addressInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, buffer};
    const auto address = context.Function<PFN_vkGetBufferDeviceAddressKHR>("vkGetBufferDeviceAddressKHR")(context.device, &addressInfo);
    if (address == 0) fail("vkGetBufferDeviceAddressKHR", VK_ERROR_UNKNOWN);
    entry.buffer = buffer;
    entry.memory = VK_NULL_HANDLE;
    entry.address = address + lead;
    entry.chunk = std::move(chunks);
    entry.dmaBuf = true;
    entry.lead = lead;
    return true;
}
#endif

#ifndef _WIN32
struct BackingFile {
    BackingFile() = default;
    BackingFile(const BackingFile&) = delete;
    BackingFile& operator=(const BackingFile&) = delete;
    ~BackingFile() {
        if (file >= 0) close(file);
    }
    int file = -1;
    std::uint64_t offset = 0;
};

bool sharedBacking(const Context& context, std::uint64_t base, std::uint64_t bytes, BackingFile& backing) {
    return context.dmaBufImport && GuestArena::GuestArenaSharedBacking_nid_postfix(static_cast<std::uintptr_t>(base), static_cast<std::size_t>(bytes), &backing.file, &backing.offset);
}

bool sharedBacking(const Context& context, const HostImport& entry, BackingFile& backing) {
    return sharedBacking(context, entry.base, entry.bytes, backing);
}

const char* createBackingImport(const Context& context, HostImport& entry, BackingFile& backing, VkResult& failure) {
    bool fits = false;
    VkResult chunkFailure = VK_SUCCESS;
    if (createChunkedDmaBufImport(context, entry, backing.file, backing.offset, fits, chunkFailure) == nullptr && fits) return nullptr;
    if (!fits && createSparseDmaBufImport(context, entry, backing.file, backing.offset)) return nullptr;
    return createDmaBufImport(context, entry, std::exchange(backing.file, -1), backing.offset, failure);
}
#endif

const char* createImport(const Context& context, HostImport& entry, VkResult& failure) {
    const char* step = createHostPointerImport(context, entry, failure);
#ifndef _WIN32
    BackingFile backing;
    if (step != nullptr && sharedBacking(context, entry, backing)) step = createBackingImport(context, entry, backing, failure);
#endif
    return step;
}

#ifndef _WIN32
enum class ImportWatchRequest : std::uint8_t { Probe, Watch, Unwatch };

ImportWatchRequest importWatchRequest() {
    static const auto request = [] {
        const char* value = std::getenv("APS5_WRITE_WATCH_IMPORTS");
        if (value == nullptr || std::strcmp(value, "probe") == 0) return ImportWatchRequest::Probe;
        if (std::strcmp(value, "watch") == 0) return ImportWatchRequest::Watch;
        if (std::strcmp(value, "unwatch") == 0) return ImportWatchRequest::Unwatch;
        throw std::runtime_error(std::string("APS5_WRITE_WATCH_IMPORTS=") + value + ": expected probe, watch or unwatch");
    }();
    return request;
}
#endif

bool importsStale(const Context& context, const HostImports& state);

constexpr std::uint64_t settleFloorBytes = 0x100000;
#ifdef _WIN32
constexpr std::uint64_t settleBlockBytes = 0x10000;
constexpr std::uint64_t settlePageBytes = 0x1000;
constexpr auto settlePollPeriod = std::chrono::microseconds(100);
std::atomic<bool> deferredSettles{std::getenv("APS5_NO_DEFERRED_SETTLES") == nullptr};
#endif

ImportSettle& settleFor(const Context& context, HostImports& state) {
    auto& settle = state.settles[context.device];
    settle.destroyFence = context.Function<PFN_vkDestroyFence>("vkDestroyFence");
    settle.settleDestroyBuffer = context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer");
    settle.settleFreeMemory = context.Function<PFN_vkFreeMemory>("vkFreeMemory");
#ifdef _WIN32
    settle.waitForFences = context.Function<PFN_vkWaitForFences>("vkWaitForFences");
#endif
    return settle;
}

std::atomic<std::uint64_t> settledImports{0};

const char* settleImport(const Context& context, HostImports& state, const HostImport& entry, VkResult& result) {
    std::unique_lock queueLock(*context.settleQueueMutex);
    auto& settle = settleFor(context, state);
    if (settle.settleTarget == VK_NULL_HANDLE) {
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size = 256;
        info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        if ((result = context.Function<PFN_vkCreateBuffer>("vkCreateBuffer")(context.device, &info, nullptr, &settle.settleTarget)) != VK_SUCCESS) {
            settle.settleTarget = VK_NULL_HANDLE;
            return "vkCreateBuffer (settle)";
        }
        VkMemoryRequirements requirements{};
        context.Function<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(context.device, settle.settleTarget, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = static_cast<std::uint32_t>(std::countr_zero(requirements.memoryTypeBits));
        if ((result = MemoryAccounting::Allocate(context, allocation, "host import settle", &settle.settleMemory)) != VK_SUCCESS || (result = context.Function<PFN_vkBindBufferMemory>("vkBindBufferMemory")(context.device, settle.settleTarget, settle.settleMemory, 0)) != VK_SUCCESS) {
            context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(context.device, settle.settleTarget, nullptr);
            if (settle.settleMemory != VK_NULL_HANDLE) MemoryAccounting::Free(context, settle.settleMemory);
            settle.settleTarget = VK_NULL_HANDLE;
            settle.settleMemory = VK_NULL_HANDLE;
            return "the settle target";
        }
        VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocate.commandPool = context.settlePool;
        allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate.commandBufferCount = 1;
        if ((result = context.Function<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")(context.device, &allocate, &settle.settleCommands)) != VK_SUCCESS) {
            settle.settleCommands = VK_NULL_HANDLE;
            return "vkAllocateCommandBuffers (settle)";
        }
        const VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        if ((result = context.Function<PFN_vkCreateFence>("vkCreateFence")(context.device, &fenceInfo, nullptr, &settle.settleFence)) != VK_SUCCESS) {
            settle.settleFence = VK_NULL_HANDLE;
            return "vkCreateFence (settle)";
        }
    }
    if (settle.settleCommands == VK_NULL_HANDLE || settle.settleFence == VK_NULL_HANDLE) return "the settle commands";
    VkBuffer source = entry.buffer;
    VkBuffer alias = VK_NULL_HANDLE;
    const auto run = [&]() -> const char* {
#ifdef _WIN32
        if (context.settleFamily != context.queueFamily) {
            VkExternalMemoryBufferCreateInfo external{VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_BUFFER_CREATE_INFO};
            external.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT;
            VkBufferCreateInfo aliasInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, &external};
            aliasInfo.size = std::min<VkDeviceSize>(entry.bytes, 65536);
            aliasInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            if ((result = context.Function<PFN_vkCreateBuffer>("vkCreateBuffer")(context.device, &aliasInfo, nullptr, &alias)) != VK_SUCCESS) return "vkCreateBuffer (settle alias)";
            if ((result = context.Function<PFN_vkBindBufferMemory>("vkBindBufferMemory")(context.device, alias, entry.memory, 0)) != VK_SUCCESS) return "vkBindBufferMemory (settle alias)";
            source = alias;
        }
#endif
        if ((result = context.Function<PFN_vkResetFences>("vkResetFences")(context.device, 1, &settle.settleFence)) != VK_SUCCESS) return "vkResetFences (settle)";
        if ((result = context.Function<PFN_vkResetCommandBuffer>("vkResetCommandBuffer")(settle.settleCommands, 0)) != VK_SUCCESS) return "vkResetCommandBuffer (settle)";
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if ((result = context.Function<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(settle.settleCommands, &begin)) != VK_SUCCESS) return "vkBeginCommandBuffer (settle)";
        const VkBufferCopy region{0, 0, 4};
        context.Function<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer")(settle.settleCommands, source, settle.settleTarget, 1, &region);
        if ((result = context.Function<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(settle.settleCommands)) != VK_SUCCESS) return "vkEndCommandBuffer (settle)";
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &settle.settleCommands;
        if ((result = context.Function<PFN_vkQueueSubmit>("vkQueueSubmit")(context.settleQueue, 1, &submit, settle.settleFence)) != VK_SUCCESS) return "vkQueueSubmit (settle)";
#ifdef _WIN32
        ++state.settlesWaited;
#endif
        if ((result = context.Function<PFN_vkWaitForFences>("vkWaitForFences")(context.device, 1, &settle.settleFence, VK_TRUE, 10'000'000'000ull)) != VK_SUCCESS) return "vkWaitForFences (settle)";
        return nullptr;
    };
    const char* step = run();
    if (step != nullptr) context.Function<PFN_vkQueueWaitIdle>("vkQueueWaitIdle")(context.settleQueue);
    if (alias != VK_NULL_HANDLE) context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(context.device, alias, nullptr);
    if (step == nullptr) settledImports.fetch_add(1, std::memory_order_relaxed);
    return step;
}

#ifdef _WIN32
const char* prepareSettleBatch(const Context& context, ImportSettle& settle, SettleBatch& batch, VkResult& result) {
    if (settle.batchTarget == VK_NULL_HANDLE) {
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size = VkDeviceSize{settleBatchCount} * settleBatchCopies * 4u;
        info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        if ((result = context.Function<PFN_vkCreateBuffer>("vkCreateBuffer")(context.device, &info, nullptr, &settle.batchTarget)) != VK_SUCCESS) {
            settle.batchTarget = VK_NULL_HANDLE;
            return "vkCreateBuffer (settle batch)";
        }
        VkMemoryRequirements requirements{};
        context.Function<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(context.device, settle.batchTarget, &requirements);
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = static_cast<std::uint32_t>(std::countr_zero(requirements.memoryTypeBits));
        if ((result = MemoryAccounting::Allocate(context, allocation, "host import batch", &settle.batchMemory)) != VK_SUCCESS || (result = context.Function<PFN_vkBindBufferMemory>("vkBindBufferMemory")(context.device, settle.batchTarget, settle.batchMemory, 0)) != VK_SUCCESS) {
            context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(context.device, settle.batchTarget, nullptr);
            if (settle.batchMemory != VK_NULL_HANDLE) MemoryAccounting::Free(context, settle.batchMemory);
            settle.batchTarget = VK_NULL_HANDLE;
            settle.batchMemory = VK_NULL_HANDLE;
            return "the settle batch target";
        }
    }
    if (batch.commands == VK_NULL_HANDLE) {
        VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocate.commandPool = context.pool;
        allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate.commandBufferCount = 1;
        if ((result = context.Function<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")(context.device, &allocate, &batch.commands)) != VK_SUCCESS) {
            batch.commands = VK_NULL_HANDLE;
            return "vkAllocateCommandBuffers (settle batch)";
        }
    }
    if (batch.fence == VK_NULL_HANDLE) {
        const VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        if ((result = context.Function<PFN_vkCreateFence>("vkCreateFence")(context.device, &fenceInfo, nullptr, &batch.fence)) != VK_SUCCESS) {
            batch.fence = VK_NULL_HANDLE;
            return "vkCreateFence (settle batch)";
        }
    }
    return nullptr;
}

void dropWantedSettles(HostImports& state) {
    for (const auto base : state.settleWanted) {
        if (const auto settle = state.pendingSettles.find(base); settle != state.pendingSettles.end() && settle->second.phase == SettlePhase::Wanted) state.pendingSettles.erase(settle);
    }
    state.settleWanted.clear();
}

void submitSettleBatch(const Context& context, HostImports& state) {
    if (state.settleWanted.empty()) return;
    auto& deviceSettle = settleFor(context, state);
    const auto free = std::find_if(deviceSettle.batches.begin(), deviceSettle.batches.end(), [](const SettleBatch& batch) { return !batch.busy; });
    if (free == deviceSettle.batches.end()) return;
    const auto index = static_cast<std::size_t>(free - deviceSettle.batches.begin());
    auto& batch = *free;
    std::unique_lock queueLock(*context.settleQueueMutex);
    VkResult result = VK_SUCCESS;
    const auto destroyBuffer = context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer");
    const auto fail = [&](const char* step) {
        for (const auto alias : batch.aliases) destroyBuffer(context.device, alias, nullptr);
        batch.aliases.clear();
        for (const auto base : batch.bases) state.pendingSettles.erase(base);
        batch.bases.clear();
        dropWantedSettles(state);
        std::fprintf(stderr, "[gpu] settling host imports under %llu KiB without waiting failed at %s (%d); they stay compared\n", static_cast<unsigned long long>(settleFloorBytes >> 10u), step, static_cast<int>(result));
    };
    if (const char* step = prepareSettleBatch(context, deviceSettle, batch, result)) return fail(step);
    if ((result = context.Function<PFN_vkResetFences>("vkResetFences")(context.device, 1, &batch.fence)) != VK_SUCCESS) return fail("vkResetFences (settle batch)");
    if ((result = context.Function<PFN_vkResetCommandBuffer>("vkResetCommandBuffer")(batch.commands, 0)) != VK_SUCCESS) return fail("vkResetCommandBuffer (settle batch)");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if ((result = context.Function<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(batch.commands, &begin)) != VK_SUCCESS) return fail("vkBeginCommandBuffer (settle batch)");
    const auto copy = context.Resolved(&DeviceFunctions::cmdCopyBuffer, "vkCmdCopyBuffer");
    RecordMemoryBarrier(context, batch.commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
    std::size_t consumed = 0;
    for (; consumed < state.settleWanted.size() && batch.bases.size() < settleBatchCopies; ++consumed) {
        const auto base = state.settleWanted[consumed];
        const auto settle = state.pendingSettles.find(base);
        if (settle == state.pendingSettles.end() || settle->second.phase != SettlePhase::Wanted) continue;
        const auto import = state.imports.find(base);
        if (import == state.imports.end() || !import->second.unwatched) {
            state.pendingSettles.erase(settle);
            continue;
        }
        const VkBufferCopy region{0, (VkDeviceSize{index} * settleBatchCopies + batch.bases.size()) * 4u, 4};
        copy(batch.commands, import->second.buffer, deviceSettle.batchTarget, 1, &region);
        settle->second = PendingSettle{SettlePhase::Submitted, index, context.device};
        batch.bases.push_back(base);
    }
    state.settleWanted.erase(state.settleWanted.begin(), state.settleWanted.begin() + static_cast<std::ptrdiff_t>(consumed));
    RecordMemoryBarrier(context, batch.commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_WRITE_BIT);
    if ((result = context.Function<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(batch.commands)) != VK_SUCCESS) return fail("vkEndCommandBuffer (settle batch)");
    if (batch.bases.empty()) return;
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &batch.commands;
    if ((result = context.Function<PFN_vkQueueSubmit>("vkQueueSubmit")(context.queue, 1, &submit, batch.fence)) != VK_SUCCESS) return fail("vkQueueSubmit (settle batch)");
    batch.busy = true;
    state.settlesSubmitted += batch.bases.size();
}

void reapSettleBatches(const Context& context, HostImports& state) {
    const auto found = state.settles.find(context.device);
    if (found == state.settles.end()) return;
    auto& deviceSettle = found->second;
    const auto status = context.Resolved(&DeviceFunctions::getFenceStatus, "vkGetFenceStatus");
    for (std::size_t index = 0; index < deviceSettle.batches.size(); ++index) {
        auto& batch = deviceSettle.batches[index];
        if (!batch.busy) continue;
        const auto result = status(context.device, batch.fence);
        if (result == VK_NOT_READY) continue;
        for (const auto base : batch.bases) {
            const auto settle = state.pendingSettles.find(base);
            if (settle == state.pendingSettles.end() || settle->second.phase != SettlePhase::Submitted || settle->second.batch != index || settle->second.device != context.device) continue;
            if (result != VK_SUCCESS) {
                state.pendingSettles.erase(settle);
                continue;
            }
            settle->second.phase = SettlePhase::Settled;
            state.settleReady.push_back(base);
        }
        for (const auto alias : batch.aliases) deviceSettle.settleDestroyBuffer(context.device, alias, nullptr);
        batch.aliases.clear();
        batch.retired.clear();
        batch.bases.clear();
        batch.busy = false;
    }
}

bool comparedImportShares(const HostImports& state, std::uint64_t own, std::uint64_t begin, std::uint64_t end) {
    const auto shares = [&](const HostImport& entry) { return entry.unwatched && entry.memoryBase < end && begin < entry.memoryBase + entry.pinnedBytes; };
    for (const auto& [base, entry] : state.imports) {
        if (base != own && shares(entry)) return true;
    }
    for (const auto& [device, list] : state.parked) {
        for (const auto& retired : list) {
            if (shares(retired->node.mapped())) return true;
        }
    }
    return false;
}

void rewatchSettledImports(HostImports& state) {
    if (state.settleReady.empty()) return;
    std::vector<std::uint64_t> bases;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
    for (const auto base : state.settleReady) {
        const auto settle = state.pendingSettles.find(base);
        if (settle == state.pendingSettles.end() || settle->second.phase != SettlePhase::Settled) continue;
        state.pendingSettles.erase(settle);
        const auto import = state.imports.find(base);
        if (import == state.imports.end() || !import->second.unwatched) continue;
        const auto first = base & ~(settleBlockBytes - 1);
        const auto stop = (base + import->second.bytes + settleBlockBytes - 1) & ~(settleBlockBytes - 1);
        if ((first != base || stop != base + import->second.bytes) && comparedImportShares(state, base, first, stop)) continue;
        const std::array<std::pair<std::uint64_t, std::uint64_t>, 1> span{{{first, stop}}};
        if (StorageTexture::AnyLiveOverlaps(span)) {
            ++state.settlesKeptCompared;
            continue;
        }
        bases.push_back(base);
        ranges.emplace_back(base & ~(settlePageBytes - 1), (base + import->second.bytes + settlePageBytes - 1) & ~(settlePageBytes - 1));
    }
    state.settleReady.clear();
    if (ranges.empty()) return;
    GuestMemory::Rewatch(ranges);
    for (const auto base : bases) {
        const auto import = state.imports.find(base);
        if (import == state.imports.end() || !GuestMemory::Watched(base, static_cast<std::size_t>(import->second.bytes))) continue;
        import->second.unwatched = false;
        ++state.settlesRewatched;
    }
}

void settleUnwatchedImports(const Context& context, HostImports& state) {
    if (state.pendingSettles.empty() || importsStale(context, state) || context.settleQueue == VK_NULL_HANDLE || context.settleQueueMutex == nullptr || !GuestMemory::GpuMutex().HeldByThisThread()) return;
    const auto now = std::chrono::steady_clock::now();
    if (now - state.settlePolled < settlePollPeriod) return;
    state.settlePolled = now;
    reapSettleBatches(context, state);
    rewatchSettledImports(state);
    submitSettleBatch(context, state);
}

void settleImportsNow(const Context& context, HostImports& state, std::uint64_t begin, std::uint64_t end) {
    if (context.settleQueue == VK_NULL_HANDLE || context.settleQueueMutex == nullptr || !GuestMemory::GpuMutex().HeldByThisThread()) return;
    std::vector<std::uint64_t> needed;
    auto it = state.imports.lower_bound(begin);
    if (it != state.imports.begin()) --it;
    for (; it != state.imports.end() && it->first < end; ++it) {
        auto& entry = it->second;
        if (entry.base + entry.bytes <= begin || !entry.unwatched) continue;
        auto settle = state.pendingSettles.find(it->first);
        if (settle == state.pendingSettles.end()) {
            if (!entry.settleOnDemand || !deferredSettles.load(std::memory_order_relaxed)) continue;
            entry.settleOnDemand = false;
            settle = state.pendingSettles.insert_or_assign(it->first, PendingSettle{}).first;
        }
        if (settle->second.phase == SettlePhase::Wanted) {
            std::erase(state.settleWanted, it->first);
            state.settleWanted.insert(state.settleWanted.begin(), it->first);
        }
        needed.push_back(it->first);
    }
    if (needed.empty()) return;
    auto& deviceSettle = settleFor(context, state);
    const auto wanted = [&] {
        return std::any_of(needed.begin(), needed.end(), [&](std::uint64_t base) {
            const auto settle = state.pendingSettles.find(base);
            return settle != state.pendingSettles.end() && settle->second.phase == SettlePhase::Wanted;
        });
    };
    while (wanted()) {
        if (std::all_of(deviceSettle.batches.begin(), deviceSettle.batches.end(), [](const SettleBatch& batch) { return batch.busy; })) {
            for (const auto& batch : deviceSettle.batches) waitSettleBatch(deviceSettle, context.device, batch);
            reapSettleBatches(context, state);
        }
        submitSettleBatch(context, state);
    }
    for (const auto base : needed) {
        const auto settle = state.pendingSettles.find(base);
        if (settle != state.pendingSettles.end() && settle->second.phase == SettlePhase::Submitted && settle->second.device == context.device) waitSettleBatch(deviceSettle, context.device, deviceSettle.batches[settle->second.batch]);
    }
    reapSettleBatches(context, state);
    rewatchSettledImports(state);
}
#endif

void decideImportWatch(const Context& context, HostImports& state) {
    if (state.watchDevice == context.device) return;
#ifdef _WIN32
    state.watchDevice = context.device;
    state.unwatchImports = true;
    state.unwatchDmaBufImports = false;
    state.watchPrivateImports = false;
    if (context.hostImportAlignment == 0 || !GuestMemory::WriteWatched()) return;
    if (context.settleQueue == VK_NULL_HANDLE || context.settleQueueMutex == nullptr) {
        std::fprintf(stderr, "[write-watch] Windows direct host imports use comparisons (no second queue to settle imports on); separate shared aliases retain guest write tracking\n");
        return;
    }
    const auto probe = ProbeImportWriteProtection(context);
    if (probe.failure != nullptr) {
        std::fprintf(stderr, "[write-watch] host imports of write-watched memory keep the write watch: unknown (probe failed at %s, %d); direct host imports use comparisons, separate shared aliases retain guest write tracking\n", probe.failure, static_cast<int>(probe.result));
        return;
    }
    state.watchPrivateImports = probe.writtenAfterSettle == 0 && probe.writtenAfterSubmit == 0 && probe.writtenAfterSecondSubmit == 0 && probe.writtenAfterFill == 0 && probe.writtenByCpu != 0;
    std::fprintf(stderr, "[write-watch] host imports of write-watched memory keep the write watch: %s (%u of %u scratch pages reported written at the import, %u by its 4-byte settle read, %u after it, %u after a full GPU read, %u after a second, %u after a GPU write, %u after a CPU store); direct host imports of private memory %s, separate shared aliases retain guest write tracking\n", state.watchPrivateImports ? "yes" : "no", probe.writtenAtImport, probe.pages, probe.writtenDuringSettle, probe.writtenAfterSettle, probe.writtenAfterSubmit, probe.writtenAfterSecondSubmit, probe.writtenAfterFill, probe.writtenByCpu, state.watchPrivateImports ? "are settled and stay watched" : "use comparisons");
#else
    const auto request = importWatchRequest();
    state.watchDevice = context.device;
    state.unwatchImports = false;
    state.unwatchDmaBufImports = false;
    if (context.hostImportAlignment == 0 || !GuestMemory::WriteWatched()) return;
    if (request == ImportWatchRequest::Watch) {
        std::fprintf(stderr, "[write-watch] host imports stay watched (APS5_WRITE_WATCH_IMPORTS=watch)\n");
        return;
    }
    if (request == ImportWatchRequest::Unwatch) {
        state.unwatchImports = true;
        state.unwatchDmaBufImports = true;
        std::fprintf(stderr, "[write-watch] host imports are compared, not watched (APS5_WRITE_WATCH_IMPORTS=unwatch)\n");
        return;
    }
    const auto probe = ProbeImportWriteProtection(context);
    if (probe.failure != nullptr) {
        state.unwatchImports = true;
        std::fprintf(stderr, "[write-watch] host imports resolve write protection: unknown (probe failed at %s, %d); imported ranges are compared\n", probe.failure, static_cast<int>(probe.result));
    } else {
        state.unwatchImports = probe.writtenAfterSubmit != 0 || probe.writtenAfterFill != 0;
        std::fprintf(stderr, "[write-watch] host imports resolve write protection: %s (%u of %u scratch pages written after a GPU read, %u after a GPU write, %u after the import); imported ranges %s\n", state.unwatchImports ? "yes" : "no", probe.writtenAfterSubmit, probe.pages, probe.writtenAfterFill, probe.writtenAtImport, state.unwatchImports ? "are compared" : "stay watched");
    }
    state.unwatchDmaBufImports = state.unwatchImports;
    if (!context.dmaBufImport) return;
    const auto dmaBuf = ProbeDmaBufImportWriteProtection(context);
    if (dmaBuf.failure != nullptr) {
        state.unwatchDmaBufImports = true;
        std::fprintf(stderr, "[write-watch] dma-buf imports keep write protection: unknown (probe failed at %s, %d); imported ranges are compared\n", dmaBuf.failure, static_cast<int>(dmaBuf.result));
        return;
    }
    state.unwatchDmaBufImports = dmaBuf.writtenAfterSubmit != 0 || dmaBuf.writtenAfterFill != 0 || dmaBuf.writtenByCpu == 0;
    std::fprintf(stderr, "[write-watch] dma-buf imports keep write protection: %s (%u of %u scratch pages written after a GPU read, %u after a GPU write, %u after the import, %u seen after a CPU store); imported ranges %s\n", state.unwatchDmaBufImports ? "no" : "yes", dmaBuf.writtenAfterSubmit, dmaBuf.pages, dmaBuf.writtenAfterFill, dmaBuf.writtenAtImport, dmaBuf.writtenByCpu, state.unwatchDmaBufImports ? "are compared" : "stay watched");
#endif
}

struct ImportWatching {
    bool settled = false;
    bool hostPointer = false;
    bool dmaBuf = false;
};

ImportWatching importWatching(const Context& context, HostImports& state, bool alias, bool writable, std::uint64_t bytes) {
    decideImportWatch(context, state);
#ifdef _WIN32
    const bool settled = !alias && state.watchPrivateImports && writable && bytes >= settleFloorBytes;
    const bool watched = alias || !state.unwatchImports || settled;
    return {settled, watched, watched};
#else
    static_cast<void>(alias);
    static_cast<void>(writable);
    static_cast<void>(bytes);
    return {false, !state.unwatchImports, !state.unwatchDmaBufImports};
#endif
}

#ifdef _WIN32
struct ImportPages {
    bool committed = true;
    bool writable = true;
    bool readOnly = true;
    bool shared = true;
    MEMORY_BASIC_INFORMATION refused{};
};

bool writableProtection(std::uint32_t protection) {
    protection &= 0xffu;
    return protection == PAGE_READWRITE || protection == PAGE_EXECUTE_READWRITE || protection == PAGE_WRITECOPY || protection == PAGE_EXECUTE_WRITECOPY;
}

ImportPages importPages(std::uint64_t base, std::uint64_t bytes, bool opening = false) {
    ImportPages pages;
    for (std::uint64_t cursor = base; cursor < base + bytes;) {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(reinterpret_cast<const void*>(cursor), &info, sizeof(info)) == 0 || info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
            pages.committed = false;
            return pages;
        }
        const auto protection = info.Protect & 0xffu;
        const bool pageWritable = (info.Type == MEM_PRIVATE || info.Type == MEM_IMAGE) && writableProtection(protection);
        const bool pageReadOnly = info.Type != MEM_MAPPED && (protection == PAGE_READONLY || protection == PAGE_EXECUTE_READ);
        if (!pageReadOnly) pages.readOnly = false;
        if (info.Type != MEM_MAPPED) pages.shared = false;
        if (info.Type != MEM_MAPPED && !pageWritable && !pageReadOnly && pages.refused.BaseAddress == nullptr) pages.refused = info;
        if (!pageWritable) pages.writable = false;
        if (opening && !pages.writable && !pages.shared) return pages;
        cursor = reinterpret_cast<std::uint64_t>(info.BaseAddress) + info.RegionSize;
    }
    return pages;
}
#endif

std::shared_ptr<const GuestAllocations::Range> leasedRangeOwner(const GuestAllocations::Lease& lease, std::uint64_t base) {
    const auto found = std::lower_bound(lease.begin(), lease.end(), base, [](const auto& range, std::uint64_t value) { return range->address < value; });
    return found != lease.end() && (*found)->address == base ? *found : nullptr;
}

std::shared_ptr<const GuestAllocations::Range> leasedRangeOwner(const GuestAllocations::Lease& lease, std::uint64_t begin, std::uint64_t end) {
    const auto found = std::upper_bound(lease.begin(), lease.end(), begin, [](std::uint64_t value, const auto& range) { return value < range->address; });
    if (found == lease.begin()) return nullptr;
    const auto& range = *std::prev(found);
    return begin >= range->address && end <= range->address + range->bytes ? range : nullptr;
}

struct ImportRun {
    std::uint64_t base = 0;
    std::uint64_t bytes = 0;
    std::vector<std::shared_ptr<const GuestAllocations::Range>> ranges;
};

bool importStaysWatched(const Context& context, HostImports& state, const ImportRun& run) {
    if (std::any_of(run.ranges.begin(), run.ranges.end(), [](const auto& range) { return !range->writable; })) return false;
    if (!GuestMemory::Watched(run.base, static_cast<std::size_t>(run.bytes))) return false;
#ifdef _WIN32
    MEMORY_BASIC_INFORMATION first{};
    if (!importWatching(context, state, false, true, run.bytes).hostPointer && (VirtualQuery(reinterpret_cast<const void*>(run.base), &first, sizeof(first)) == 0 || first.Type != MEM_MAPPED)) return false;
    const auto pages = importPages(run.base, run.bytes, true);
    if (!pages.committed || (!pages.writable && !pages.shared)) return false;
    return importWatching(context, state, !pages.writable, pages.writable, run.bytes).hostPointer;
#else
    const auto watching = importWatching(context, state, false, true, run.bytes);
    BackingFile backing;
    return watching.hostPointer && (watching.dmaBuf || !sharedBacking(context, run.base, run.bytes, backing));
#endif
}

bool continuesRun(const GuestAllocations::Range& before, const GuestAllocations::Range& after) {
    return before.address + before.bytes == after.address && before.readable && after.readable && before.writable == after.writable && before.gpu == after.gpu;
}

std::size_t leasedIndexContaining(const GuestAllocations::Lease& lease, std::uint64_t address) {
    const auto found = std::upper_bound(lease.begin(), lease.end(), address, [](std::uint64_t value, const auto& range) { return value < range->address; });
    if (found == lease.begin()) return lease.size();
    const auto index = static_cast<std::size_t>(std::prev(found) - lease.begin());
    const auto& range = *lease[index];
    return range.readable && address < range.address + range.bytes ? index : lease.size();
}

std::atomic<std::uint64_t> importWindow{0};

ImportRun leasedRun(const GuestAllocations::Lease& lease, std::size_t first, std::size_t last) {
    ImportRun run{lease[first]->address, lease[last]->address + lease[last]->bytes - lease[first]->address, {}};
    run.ranges.assign(lease.begin() + static_cast<std::ptrdiff_t>(first), lease.begin() + static_cast<std::ptrdiff_t>(last) + 1);
    return run;
}

template<typename Remembered>
const Remembered* rememberedRun(const std::map<std::pair<std::uint64_t, std::uint64_t>, Remembered>& runs, const GuestAllocations::Lease& lease, std::size_t first, std::size_t last) {
    const auto base = lease[first]->address;
    const auto found = runs.find({base, lease[last]->address + lease[last]->bytes - base});
    if (found == runs.end() || found->second.ranges.size() != last - first + 1) return nullptr;
    for (std::size_t index = first; index <= last; ++index) {
        const auto& remembered = found->second.ranges[index - first];
        if (remembered.owner_before(lease[index]) || lease[index].owner_before(remembered)) return nullptr;
    }
    return &found->second;
}

bool stillRefused(const HostImports& state, const ImportRefusal* refusal, std::uint64_t budget) {
    return refusal != nullptr && budget != 0 && budget <= refusal->budget && state.liveBytes >= refusal->liveBytes;
}

struct RunBounds {
    std::size_t first = 0;
    std::size_t last = 0;
    bool found = false;
};

std::pair<std::size_t, std::size_t> walkRun(const GuestAllocations::Lease& lease, std::size_t index, std::uint64_t low, std::uint64_t high) {
    auto first = index;
    while (first > 0 && lease[first - 1]->address >= low && continuesRun(*lease[first - 1], *lease[first])) --first;
    auto last = index;
    while (last + 1 < lease.size() && lease[last + 1]->address + lease[last + 1]->bytes <= high && continuesRun(*lease[last], *lease[last + 1])) ++last;
    return {first, last};
}

std::pair<std::size_t, std::size_t> continuedRun(const GuestAllocations::Lease& lease, std::size_t index, std::uint64_t window, std::uint64_t low, std::uint64_t high) {
    static const bool memo = std::getenv("APS5_NO_RUN_MEMO") == nullptr;
    if (!memo) return walkRun(lease, index, low, high);
    struct Memo {
        std::weak_ptr<const GuestAllocations::Lease::Ranges> snapshot;
        std::size_t index = 0;
        std::uint64_t window = 0;
        std::uint64_t low = 0;
        std::pair<std::size_t, std::size_t> run;
    };
    struct MemoStorage {};
    auto& memos = HostThreadLocal<std::array<Memo, 1024>, MemoStorage>();
    const auto& snapshot = lease.Snapshot();
    const auto block = window != 0 ? low / window : 0;
    const auto slot = (reinterpret_cast<std::uintptr_t>(snapshot.get()) / 16u ^ index * 0x9e3779b1u ^ block * 0x85ebca6bu) % memos.size();
    auto& entry = memos[slot];
    if (!entry.snapshot.owner_before(snapshot) && !snapshot.owner_before(entry.snapshot) && entry.index == index && entry.window == window && entry.low == low) return entry.run;
    entry = Memo{snapshot, index, window, low, walkRun(lease, index, low, high)};
    return entry.run;
}

RunBounds importRunBounds(const GuestAllocations::Lease& lease, std::uint64_t address, std::uint64_t largest = std::numeric_limits<std::uint64_t>::max(), std::uint64_t budget = 0) {
    const auto index = leasedIndexContaining(lease, address);
    if (index == lease.size()) return {};
    const RunBounds single{index, index, true};
    const auto& containing = *lease[index];
    const auto& state = Imports();
    const auto window = importWindow.load(std::memory_order_relaxed);
    std::uint64_t low = 0;
    std::uint64_t high = std::numeric_limits<std::uint64_t>::max();
    if (window != 0) {
        low = address / window * window;
        high = low + window;
        if (containing.address < low || containing.address + containing.bytes > high) return single;
    }
    const auto [first, last] = continuedRun(lease, index, window, low, high);
    const auto skipped = [&](std::size_t from, std::size_t to) { return rememberedRun(state.failedBlocks, lease, from, to) != nullptr || stillRefused(state, rememberedRun(state.refusals, lease, from, to), budget); };
    if (window == 0) return first == last || lease[last]->address + lease[last]->bytes - lease[first]->address > largest || skipped(first, last) ? single : RunBounds{first, last, true};
    const auto bounding = [&](std::size_t from, std::size_t to, std::uint64_t value, bool start) {
        const auto found = std::lower_bound(lease.begin() + static_cast<std::ptrdiff_t>(from), lease.begin() + static_cast<std::ptrdiff_t>(to) + 1, value, [start](const auto& range, std::uint64_t wanted) { return (start ? range->address : range->address + range->bytes) < wanted; });
        const auto position = static_cast<std::size_t>(found - lease.begin());
        return position <= to && (start ? (*found)->address : (*found)->address + (*found)->bytes) == value ? position : lease.size();
    };
    for (auto size = window; size >= containing.bytes; size /= 2) {
        if (size > largest) continue;
        const auto blockLow = address / size * size;
        const auto blockHigh = blockLow + size;
        if (containing.address < blockLow || containing.address + containing.bytes > blockHigh) break;
        if (blockLow < lease[first]->address || blockHigh > lease[last]->address + lease[last]->bytes) continue;
        const auto blockFirst = bounding(first, index, blockLow, true);
        const auto blockLast = bounding(index, last, blockHigh, false);
        if (blockFirst == lease.size() || blockLast == lease.size()) continue;
        if (blockFirst == blockLast) break;
        if (skipped(blockFirst, blockLast)) continue;
        return {blockFirst, blockLast, true};
    }
    return single;
}

ImportRun importRun(const GuestAllocations::Lease& lease, std::uint64_t address, std::uint64_t largest = std::numeric_limits<std::uint64_t>::max(), std::uint64_t budget = 0) {
    const auto bounds = importRunBounds(lease, address, largest, budget);
    return bounds.found ? leasedRun(lease, bounds.first, bounds.last) : ImportRun{};
}

template<typename Entry>
bool sameRun(const Entry& entry, const GuestAllocations::Lease& lease, const RunBounds& bounds) {
    if (!bounds.found || lease[bounds.first]->address != entry.base || lease[bounds.last]->address + lease[bounds.last]->bytes - entry.base != entry.bytes || bounds.last - bounds.first + 1 != entry.ranges.size()) return false;
    for (std::size_t index = 0; index < entry.ranges.size(); ++index) {
        if (entry.ranges[index].owner_before(lease[bounds.first + index]) || lease[bounds.first + index].owner_before(entry.ranges[index])) return false;
    }
    return true;
}

ImportRun exactRun(const GuestAllocations::Lease& lease, std::uint64_t base, std::uint64_t bytes) {
    const auto first = leasedIndexContaining(lease, base);
    if (first == lease.size() || lease[first]->address != base) return {};
    auto last = first;
    while (lease[last]->address + lease[last]->bytes < base + bytes) {
        if (last + 1 == lease.size() || !continuesRun(*lease[last], *lease[last + 1])) return {};
        ++last;
    }
    return lease[last]->address + lease[last]->bytes == base + bytes ? leasedRun(lease, first, last) : ImportRun{};
}

bool runCovers(const GuestAllocations::Lease& lease, std::uint64_t begin, std::uint64_t end) {
    const auto bounds = importRunBounds(lease, begin);
    return bounds.found && end <= lease[bounds.last]->address + lease[bounds.last]->bytes;
}

ImportRun crossedRun(const GuestAllocations::Lease& lease, std::uint64_t begin, std::uint64_t end) {
    auto last = leasedIndexContaining(lease, begin);
    if (last == lease.size()) return {};
    const auto first = last;
    while (lease[last]->address + lease[last]->bytes < end) {
        if (last + 1 == lease.size() || !continuesRun(*lease[last], *lease[last + 1])) return {};
        ++last;
    }
    if (first == last) return {};
    ImportRun run{lease[first]->address, lease[last]->address + lease[last]->bytes - lease[first]->address, {}};
    run.ranges.assign(lease.begin() + static_cast<std::ptrdiff_t>(first), lease.begin() + static_cast<std::ptrdiff_t>(last) + 1);
    return run;
}

ImportRun joinedRun(const GuestAllocations::Lease& lease, std::uint64_t begin, std::uint64_t end) {
    const auto window = importWindow.load(std::memory_order_relaxed);
    if (window == 0) return {};
    auto run = crossedRun(lease, begin, end);
    if (run.ranges.empty()) return run;
    if (std::all_of(run.ranges.begin(), run.ranges.end(), [window](const auto& range) { return range->bytes > window; })) return run;
    if (begin == run.base) return run;
    return {};
}

bool joinedCovers(const GuestAllocations::Lease& lease, std::uint64_t begin, std::uint64_t end) {
    return !crossedRun(lease, begin, end).ranges.empty();
}

template<typename Entry>
bool sameRun(const Entry& entry, const ImportRun& run) {
    if (run.base != entry.base || run.bytes != entry.bytes || run.ranges.size() != entry.ranges.size()) return false;
    for (std::size_t index = 0; index < run.ranges.size(); ++index) {
        if (entry.ranges[index].owner_before(run.ranges[index]) || run.ranges[index].owner_before(entry.ranges[index])) return false;
    }
    return true;
}

bool wouldRetireImport(const HostImports& state, const ImportRun& run) {
    if (run.ranges.empty()) return false;
    const auto next = state.imports.lower_bound(run.base);
    if (next != state.imports.end()) {
        if (next->first == run.base && sameRun(next->second, run)) return false;
        if (next->first < run.base + run.bytes) return true;
    }
    if (next == state.imports.begin()) return false;
    const auto& previous = std::prev(next)->second;
    return previous.base + previous.bytes > run.base;
}

bool sameRange(const ImportFailure& entry, const GuestAllocations::Lease& lease, std::uint64_t budget = 0) {
    const auto current = entry.ranges.size() == 1 ? leasedRangeOwner(lease, entry.base, entry.base + entry.bytes) : nullptr;
    if (current != nullptr && entry.bytes < current->bytes) return !entry.ranges.front().owner_before(current) && !current.owner_before(entry.ranges.front());
    return sameRun(entry, lease, importRunBounds(lease, entry.base, std::numeric_limits<std::uint64_t>::max(), budget));
}

bool sameRange(const HostImport& entry, const GuestAllocations::Lease& lease, std::uint64_t budget = 0) {
    if (entry.ranges.size() == 1 && !entry.joined && entry.bytes < entry.ranges.front()->bytes) {
        const auto current = leasedRangeOwner(lease, entry.base, entry.base + entry.bytes);
        return current != nullptr && !entry.ranges.front().owner_before(current) && !current.owner_before(entry.ranges.front());
    }
    if (entry.joined) return sameRun(entry, joinedRun(lease, entry.base, entry.base + entry.bytes));
    return sameRun(entry, lease, importRunBounds(lease, entry.base, std::numeric_limits<std::uint64_t>::max(), budget));
}

std::uint64_t effectiveUse(const HostImports& state, const HostImport& entry) {
    if (entry.spaceSerial == 0) return entry.lastUse;
    const auto found = state.spaceUses.find(entry.spaceSerial);
    return found == state.spaceUses.end() ? entry.lastUse : std::max(entry.lastUse, found->second);
}

bool pointerHeld(const HostImports& state, const HostImport& entry) {
    return effectiveUse(state, entry) >= state.buildFloor;
}

constexpr std::uint64_t ProtectedUses = 256;

bool mirrorsEnabled();

bool mirrorStandsIn(const HostImport& entry) {
    return mirrorsEnabled() && !entry.joined && entry.ranges.size() == 1 && entry.range != nullptr && entry.range->releasable && GuestMemory::Watched(entry.range->address, entry.range->bytes);
}

bool hasFrameHistory(const HostImports& state) {
    return state.previousFrameFloor != 0;
}

bool evictable(const HostImports& state, const HostImport& entry) {
    if (pointerHeld(state, entry)) return false;
    if (state.mirrorsYield && mirrorStandsIn(entry)) return true;
    if (!hasFrameHistory(state)) return effectiveUse(state, entry) + ProtectedUses < state.useCounter;
    return effectiveUse(state, entry) < state.previousFrameFloor;
}

struct MirrorsYield {
    HostImports& state;
    std::uint64_t floor;
    MirrorsYield(HostImports& state, std::uint64_t buildFloor) : state(state), floor(state.buildFloor) {
        state.mirrorsYield = true;
        state.buildFloor = buildFloor;
    }
    ~MirrorsYield() {
        state.mirrorsYield = false;
        state.buildFloor = floor;
    }
    MirrorsYield(const MirrorsYield&) = delete;
    MirrorsYield& operator=(const MirrorsYield&) = delete;
};

bool evictingRefusalCurrent(const HostImports& state, const EvictingRefusal& refusal, std::uint64_t budget) {
    return refusal.epoch == state.epoch && refusal.frames == state.framesEnded && refusal.liveBytes == state.liveBytes && budget <= refusal.budget && refusal.window == importWindow.load(std::memory_order_relaxed) && state.useCounter < refusal.ready && (state.buildFloor <= refusal.floor || state.useCounter < refusal.readyHeld);
}

void noteRetiredUnwatched(std::vector<std::pair<std::uint64_t, std::uint64_t>>& spans, const HostImport& entry, const GuestAllocations::Lease& lease) {
    if (!entry.unwatched) return;
    for (const auto& range : entry.ranges) {
        if (leasedRangeOwner(lease, range->address) != range) continue;
        const auto begin = std::max(range->address, entry.base);
        const auto end = std::min(range->address + range->bytes, entry.base + entry.bytes);
        if (begin < end) spans.emplace_back(begin, end);
    }
}

void rewatchRetired(const HostImports& state, std::vector<std::pair<std::uint64_t, std::uint64_t>> spans) {
    std::erase_if(spans, [&](const auto& span) {
        auto it = state.imports.upper_bound(span.first);
        if (it != state.imports.begin()) --it;
        for (; it != state.imports.end() && it->first < span.second; ++it) {
            if (it->second.unwatched && span.first < it->first + it->second.bytes) return true;
        }
        const std::array<std::pair<std::uint64_t, std::uint64_t>, 1> one{{span}};
        return StorageTexture::AnyLiveOverlaps(one);
    });
    if (!spans.empty()) GuestMemory::Rewatch(spans);
}

std::uint64_t retireLeastRecent(const Context& context, HostImports& state, const GuestAllocations::Lease& lease, std::uint64_t wanted) {
    std::uint64_t freed = 0;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> unwatched;
    while (freed < wanted) {
        auto oldest = state.imports.end();
        for (auto it = state.imports.begin(); it != state.imports.end(); ++it) {
            if (evictable(state, it->second) && (oldest == state.imports.end() || effectiveUse(state, it->second) < effectiveUse(state, oldest->second))) oldest = it;
        }
        if (oldest == state.imports.end()) break;
        freed += oldest->second.pinnedBytes;
        noteRetiredUnwatched(unwatched, oldest->second, lease);
        retireImport(context.device, state, oldest, lease);
    }
    rewatchRetired(state, std::move(unwatched));
    return freed;
}

std::uint64_t physicalMemoryBytes() {
#ifdef _WIN32
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    return GlobalMemoryStatusEx(&status) ? status.ullTotalPhys : 0;
#else
    const auto pages = sysconf(_SC_PHYS_PAGES);
    const auto pageSize = sysconf(_SC_PAGESIZE);
    return pages > 0 && pageSize > 0 ? static_cast<std::uint64_t>(pages) * static_cast<std::uint64_t>(pageSize) : 0;
#endif
}

std::atomic<std::uint64_t> budgetOverride{0};
std::atomic<std::uint64_t> addressCopyLimitOverride{0};

std::uint64_t addressCopyLimit() {
    static const std::uint64_t configured = [] {
        const char* value = std::getenv("APS5_ADDRESS_COPY_MAX_MIB");
        return (value ? std::strtoull(value, nullptr, 10) : 64ull) << 20u;
    }();
    const auto limit = addressCopyLimitOverride.load(std::memory_order_relaxed);
    return limit != 0 ? limit : configured;
}

constexpr unsigned AddressCopyEpochShift = 48;
constexpr std::uint64_t AddressCopyEpochMask = 0xffff;
constexpr std::uint64_t AddressCopyLargestMask = (std::uint64_t{1} << AddressCopyEpochShift) - 1;
std::atomic<std::uint64_t> addressCopyEpoch{0};

struct AddressCopyRegistry {
    std::mutex mutex;
    std::deque<AddressCopyCounters> counters;
    std::vector<AddressCopyCounters*> unused;
};

AddressCopyRegistry& addressCopyRegistry() {
    static auto* registry = new AddressCopyRegistry;
    return *registry;
}

struct AddressCopyLease {
    AddressCopyCounters* counters = nullptr;
    ~AddressCopyLease() {
        if (counters == nullptr) return;
        auto& registry = addressCopyRegistry();
        std::lock_guard lock(registry.mutex);
        registry.unused.push_back(counters);
    }
};

// Pinned imports count against the driver's system memory heap, which imports, type-3 mirrors, buffers and textures
// placed in host memory share; past it ordinary host allocations fail. The budget is APS5_HOST_IMPORT_MIB, else
// HostImportBudget.
std::uint64_t configuredBudget() {
    if (const auto set = budgetOverride.load(std::memory_order_relaxed); set != 0) return set;
    static const std::uint64_t configured = []() -> std::uint64_t {
        const char* value = std::getenv("APS5_HOST_IMPORT_MIB");
        return value != nullptr ? std::strtoull(value, nullptr, 10) << 20u : 0u;
    }();
    return configured;
}

std::uint64_t importBudget(const Context& context) {
    if (const auto configured = configuredBudget(); configured != 0) return configured;
    return HostImportBudget(context.memory, physicalMemoryBytes());
}

std::optional<std::uint32_t> hostHeapIndex(const VkPhysicalDeviceMemoryProperties& memory) {
    std::optional<std::uint32_t> heap;
    for (std::uint32_t index = 0; index < memory.memoryHeapCount; ++index) {
        if ((memory.memoryHeaps[index].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) == 0 && (!heap || memory.memoryHeaps[index].size > memory.memoryHeaps[*heap].size)) heap = index;
    }
    return heap;
}

void noteReportedBudget(HostImports& state, std::uint64_t configured, bool set, std::uint64_t reported) {
    if (reported >= configured || state.reportedHeapBudget == 0) return;
    const std::uint8_t stage = !set && state.allocationFailed ? 2 : 1;
    if (state.reportNoted >= stage) return;
    state.reportNoted = stage;
    const char* outcome = set ? "the setting stays, as the report never lowers it" : stage == 2 ? "imports are held to it since an import's vkAllocateMemory failed" : "the default stays until an import's vkAllocateMemory fails";
    std::fprintf(stderr, "[gpu] host import budget: host heap %u reports a VK_EXT_memory_budget budget of %llu MiB and a usage of %llu MiB with %llu MiB of imports live, which leaves imports %llu MiB of the %llu MiB %s; %s\n", state.reportedHeap, static_cast<unsigned long long>(state.reportedHeapBudget >> 20u), static_cast<unsigned long long>(state.reportedHeapUsage >> 20u), static_cast<unsigned long long>(state.liveBytes >> 20u), static_cast<unsigned long long>(reported >> 20u), static_cast<unsigned long long>(configured >> 20u), set ? "APS5_HOST_IMPORT_MIB sets" : "of the default", outcome);
}

std::uint64_t effectiveBudget(const Context& context, HostImports& state) {
    const auto configured = importBudget(context);
    const bool set = configuredBudget() != 0;
    auto reported = std::numeric_limits<std::uint64_t>::max();
    if (context.memoryProperties2 != nullptr && context.physical != VK_NULL_HANDLE) {
        const auto now = std::chrono::steady_clock::now();
        if (state.reportedDevice != context.physical || now - state.reportedAt >= std::chrono::seconds(1)) {
            VkPhysicalDeviceMemoryBudgetPropertiesEXT report{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT};
            VkPhysicalDeviceMemoryProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2, &report};
            context.memoryProperties2(context.physical, &properties);
            state.reportedBudget = HostImportBudgetFor(context.memory, &report, std::numeric_limits<std::uint64_t>::max(), state.liveBytes);
            const auto heap = hostHeapIndex(context.memory);
            state.reportedHeap = heap.value_or(0);
            state.reportedHeapBudget = heap ? report.heapBudget[*heap] : 0;
            state.reportedHeapUsage = heap ? report.heapUsage[*heap] : 0;
            state.reportedDevice = context.physical;
            state.reportedAt = now;
        }
        reported = state.reportedBudget;
    }
    noteReportedBudget(state, configured, set, reported);
    return HostImportBudgetWith(configured, set, reported, state.allocationFailed, state.ceiling);
}

std::string budgetSource(const Context& context, const HostImports& state, std::uint64_t budget) {
    const auto configured = importBudget(context);
    const char* source = state.ceiling != 0 && budget == state.ceiling ? "lowered after vkAllocateMemory failed" : budget < configured ? "the host heap's VK_EXT_memory_budget report" : configuredBudget() != 0 ? "APS5_HOST_IMPORT_MIB" : "the default";
    if (state.reportedHeapBudget == 0) return source;
    char text[192];
    std::snprintf(text, sizeof(text), "%s; host heap %u reports a budget of %llu MiB and a usage of %llu MiB", source, state.reportedHeap, static_cast<unsigned long long>(state.reportedHeapBudget >> 20u), static_cast<unsigned long long>(state.reportedHeapUsage >> 20u));
    return text;
}

std::atomic<std::uint64_t> rangeImportWindow{64ull << 20u};
std::atomic<std::uint64_t> importMaxBytes{0};

// The import of an access to a registered range larger than the range window: the cells the access touches, grown to
// the union of the range's imports it overlaps, so accesses across neighbouring cell edges share one import instead of
// replacing each other's.
std::optional<std::pair<std::uint64_t, std::uint64_t>> importWindowBounds(const GuestAllocations::Range& range, std::uint64_t begin, std::uint64_t end, std::uint64_t alignment) {
    const auto window = rangeImportWindow.load(std::memory_order_relaxed);
    if (window == 0 || alignment == 0 || window % alignment != 0 || range.bytes <= window || begin < range.address || end > range.address + range.bytes || end <= begin) return std::nullopt;
    const auto lastCell = range.bytes / window - 1;
    const auto cell = [&](std::uint64_t address) { return std::min((address - range.address) / window, lastCell); };
    const auto windowBegin = range.address + cell(begin) * window;
    const auto endCell = cell(end - 1);
    const auto windowEnd = endCell == lastCell ? range.address + range.bytes : range.address + (endCell + 1) * window;
    if (windowBegin % alignment != 0 || (windowEnd - windowBegin) % alignment != 0) return std::nullopt;
    return std::pair{windowBegin, windowEnd};
}

ImportRun windowRun(const HostImports& state, ImportRun run, std::uint64_t begin, std::uint64_t end, std::uint64_t alignment) {
    if (run.ranges.size() != 1) return run;
    const auto& range = *run.ranges.front();
    const auto bounds = importWindowBounds(range, begin, end, alignment);
    if (!bounds) return run;
    auto [windowBegin, windowEnd] = *bounds;
    for (bool grown = true; grown;) {
        grown = false;
        auto it = state.imports.upper_bound(windowBegin);
        if (it != state.imports.begin()) --it;
        for (; it != state.imports.end() && it->first < windowEnd; ++it) {
            const auto importEnd = it->second.base + it->second.bytes;
            if (importEnd <= windowBegin || it->second.base < range.address || importEnd > range.address + range.bytes) continue;
            if (it->second.base < windowBegin || importEnd > windowEnd) grown = true;
            windowBegin = std::min(windowBegin, it->second.base);
            windowEnd = std::max(windowEnd, importEnd);
        }
    }
    if (windowBegin % alignment != 0 || (windowEnd - windowBegin) % alignment != 0) return run;
    run.base = windowBegin;
    run.bytes = windowEnd - windowBegin;
    return run;
}

const HostImport* peekImport(const HostImports& state, std::uint64_t begin, std::uint64_t end);
const HostImport* findImport(HostImports& state, std::uint64_t begin, std::uint64_t end);
const HostImport* importRunAt(const Context& context, HostImports& state, ImportRun run, bool joined, std::uint64_t address, const GuestAllocations::Lease& lease, std::uint64_t end, bool evict, ImportRequester requester);

// The bytes retireLeastRecent could free outside [begin, end). Before two frames have ended the imports age by lookups as they
// did before frames existed. After that readyHeld is 0 when an import a build still holds is already two frames old: it becomes
// evictable as soon as that build's floor rises, which ends a remembered refusal.
std::uint64_t evictableBytes(const HostImports& state, std::uint64_t begin, std::uint64_t end, std::uint64_t& ready, std::uint64_t& readyHeld) {
    std::uint64_t bytes = 0;
    for (const auto& [base, entry] : state.imports) {
        if (entry.base + entry.bytes > begin && entry.base < end) continue;
        if (evictable(state, entry)) {
            bytes += entry.pinnedBytes;
            continue;
        }
        if (!hasFrameHistory(state)) {
            auto& horizon = pointerHeld(state, entry) ? readyHeld : ready;
            horizon = std::min(horizon, effectiveUse(state, entry) + ProtectedUses + 1);
        } else if (pointerHeld(state, entry) && effectiveUse(state, entry) < state.previousFrameFloor) readyHeld = 0;
    }
    return bytes;
}

struct ImportConstruction {
    ImportConstruction(const Context& context, std::uint64_t base, std::uint64_t bytes)
        : device(context.device), destroyBuffer(context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")), freeMemory(context.Function<PFN_vkFreeMemory>("vkFreeMemory")), entry{base, bytes, VK_NULL_HANDLE, VK_NULL_HANDLE, 0} {}
    ImportConstruction(const ImportConstruction&) = delete;
    ImportConstruction& operator=(const ImportConstruction&) = delete;
    ~ImportConstruction() {
        if (!armed) return;
        if (entry.buffer != VK_NULL_HANDLE) destroyBuffer(device, entry.buffer, nullptr);
        if (entry.memory != VK_NULL_HANDLE) MemoryAccounting::Free(freeMemory, device, entry.memory);
#ifdef _WIN32
        GuestArena::GuestArenaUnmapAlias_nid_postfix(entry.alias);
#endif
    }
    VkDevice device;
    PFN_vkDestroyBuffer destroyBuffer;
    PFN_vkFreeMemory freeMemory;
    HostImport entry;
    bool armed = true;
};

void ensurePinWaiter();

const HostImport* importAllocation(const Context& context, HostImports& state, std::uint64_t address, const GuestAllocations::Lease& lease, ImportRequester requester, std::uint64_t end = 0, bool evict = true, std::uint64_t largest = std::numeric_limits<std::uint64_t>::max()) {
    ensurePinWaiter();
#ifdef _WIN32
    settleUnwatchedImports(context, state);
#endif
    const bool fresh = !importsStale(context, state);
    if (fresh && end != 0) {
        if (const auto* current = findImport(state, address, end)) return current;
    } else if (!fresh) {
        if (const auto* current = peekImport(state, address, address + 1); current != nullptr && current->joined && sameRange(*current, lease)) {
            current->lastUse = ++state.useCounter;
            return current;
        }
    }
    const auto budget = evict ? 0 : effectiveBudget(context, state);
    const auto index = leasedIndexContaining(lease, address);
    const auto windowBounds = index != lease.size() && end != 0 ? importWindowBounds(*lease[index], address, end, context.hostImportAlignment) : std::nullopt;
    const bool complete = !windowBounds || (windowBounds->first == lease[index]->address && windowBounds->second == lease[index]->address + lease[index]->bytes);
    if (budget != 0 && index != lease.size() && complete && stillRefused(state, rememberedRun(state.refusals, lease, index, index), budget) && findImport(state, address, address + 1) == nullptr) {
        auto& counts = state.counts[static_cast<std::size_t>(requester)];
        ++counts.refused;
        counts.refusedBytes += lease[index]->bytes;
        ++counts.remembered;
        ++state.budgetRefusals;
        return nullptr;
    }
    const auto bounds = importRunBounds(lease, address, largest, budget);
    // Without an end the caller views the whole run through the import (AcquireRegistered): a window that holds the
    // first byte is shorter than that, so only an import of the whole run is a hit; a window is replaced below.
    if (fresh && end == 0) {
        if (const auto* current = findImport(state, address, bounds.found ? lease[bounds.last]->address + lease[bounds.last]->bytes : address + 1)) return current;
    }
    auto run = bounds.found ? leasedRun(lease, bounds.first, bounds.last) : ImportRun{};
    if (end != 0) run = windowRun(state, std::move(run), address, end, context.hostImportAlignment);
    if (run.ranges.empty()) return nullptr;
    return importRunAt(context, state, std::move(run), false, address, lease, end, evict, requester);
}

struct ImportPiece {
    std::uint64_t begin;
    std::uint64_t end;
    const HostImport* entry;
};

std::vector<ImportPiece> importPieces(const Context& context, HostImports& state, const GuestAllocations::Range& range, const GuestAllocations::Lease& lease) {
    const auto limit = importMaxBytes.load(std::memory_order_relaxed);
    const auto window = rangeImportWindow.load(std::memory_order_relaxed);
    const auto alignment = context.hostImportAlignment;
    if (limit < 2 * window || window == 0 || alignment == 0 || window % alignment != 0 || range.address % alignment != 0) return {};
    const auto step = (limit / window - 1) * window;
    const auto end = range.address + range.bytes;
    std::vector<ImportPiece> pieces;
    for (auto cursor = range.address; cursor < end;) {
        const auto wanted = end - cursor < step + window ? end : cursor + step;
        const auto* entry = importAllocation(context, state, cursor, lease, ImportRequester::Build, wanted, false);
        if (entry == nullptr || entry->base > cursor || entry->bytes > limit) return {};
        const auto to = std::min(end, entry->base + entry->bytes);
        if (to <= cursor) return {};
        pieces.push_back({cursor, to, entry});
        cursor = to;
    }
    return pieces;
}

std::vector<ImportPiece> budgetedImportPieces(const Context& context, HostImports& state, const GuestAllocations::Range& range, const GuestAllocations::Lease& lease) {
    const auto window = rangeImportWindow.load(std::memory_order_relaxed);
    const auto alignment = context.hostImportAlignment;
    if (!range.releasable || !GuestMemory::Watched(range.address, range.bytes) || window == 0 || alignment == 0 || window % alignment != 0 || range.address % alignment != 0 || range.bytes <= window) return {};
    const auto end = range.address + range.bytes;
    std::vector<ImportPiece> pieces;
    for (auto cursor = range.address; cursor < end;) {
        const HostImport* entry = nullptr;
        if (const auto* previous = peekImport(state, cursor, cursor + 1); previous != nullptr && previous->base >= range.address && previous->base + previous->bytes <= end) {
            entry = importAllocation(context, state, cursor, lease, ImportRequester::Build, std::min(end, previous->base + previous->bytes), false);
        }
        if (entry == nullptr) {
            const auto budget = effectiveBudget(context, state);
            auto room = budget - std::min(budget, state.liveBytes);
            if (const auto limit = importMaxBytes.load(std::memory_order_relaxed); limit != 0) room = std::min(room, limit);
            if (room < window) break;
            auto wanted = cursor + std::min(end - cursor, window);
            if (const auto bounds = importWindowBounds(range, cursor, wanted, alignment); bounds && bounds->second - bounds->first > room) {
                wanted = range.address + (range.bytes / window - 1) * window;
                if (wanted <= cursor) break;
            }
            entry = importAllocation(context, state, cursor, lease, ImportRequester::Build, wanted, false);
        }
        if (entry == nullptr || entry->base > cursor) break;
        const auto to = std::min(end, entry->base + entry->bytes);
        if (to <= cursor) break;
        pieces.push_back({cursor, to, entry});
        cursor = to;
    }
    if (pieces.empty()) return {};
    if (pieces.back().end != end) pieces.push_back({pieces.back().end, end, nullptr});
    return pieces;
}

const HostImport* importJoined(const Context& context, HostImports& state, std::uint64_t begin, std::uint64_t end, const GuestAllocations::Lease& lease) {
    if (const auto* low = peekImport(state, begin, begin + 1); low != nullptr && low->joined) begin = low->base;
    if (const auto* high = peekImport(state, end - 1, end); high != nullptr && high->joined) end = high->base + high->bytes;
    auto run = joinedRun(lease, begin, end);
    if (run.ranges.empty()) return nullptr;
    if (const auto failed = state.failedJoins.find(run.base); failed != state.failedJoins.end() && sameRun(failed->second, run)) return nullptr;
    const auto existing = state.imports.find(run.base);
    const bool existed = existing != state.imports.end() && sameRun(existing->second, run);
    const auto* entry = importRunAt(context, state, std::move(run), true, begin, lease, end, true, ImportRequester::Join);
    if (entry != nullptr && !existed) ++state.epoch;
    return entry;
}

const HostImport* importRunAt(const Context& context, HostImports& state, ImportRun run, bool joined, std::uint64_t address, const GuestAllocations::Lease& lease, std::uint64_t end, bool evict, ImportRequester requester) {
    auto& counts = state.counts[static_cast<std::size_t>(requester)];
    const auto base = run.base;
    const auto bytes = run.bytes;
    auto& failures = joined ? state.failedJoins : state.failed;
    const bool stepDown = !joined && run.ranges.size() > 1 && importWindow.load(std::memory_order_relaxed) != 0;
    const auto alignment = context.hostImportAlignment;
    if (joined && (alignment == 0 || base % alignment != 0 || bytes % alignment != 0)) {
        failures.insert_or_assign(base, ImportFailure{base, bytes, GuestAllocations::Mapped(run.ranges.begin(), run.ranges.end())});
        return nullptr;
    }
    if (const auto found = state.imports.find(base); found != state.imports.end() && sameRun(found->second, run)) {
        found->second.lastUse = ++state.useCounter;
        return &found->second;
    }
    if (alignment == 0 || base % alignment != 0 || bytes % alignment != 0) return nullptr;
    if (const auto limit = importMaxBytes.load(std::memory_order_relaxed); limit != 0 && bytes > limit) return nullptr;
    if (const auto failed = failures.find(base); !stepDown && failed != failures.end()) {
        if (sameRun(failed->second, run)) return nullptr;
        failures.erase(failed);
    }
    const auto budget = effectiveBudget(context, state);
    const auto containing = std::find_if(run.ranges.begin(), run.ranges.end(), [address](const auto& range) { return address >= range->address && address < range->address + range->bytes; });
    if (const auto remembered = state.evictingRefusals.find({base, bytes}); evict && !state.mirrorsYield && remembered != state.evictingRefusals.end()) {
        if (remembered->second.joined == joined && evictingRefusalCurrent(state, remembered->second, budget) && sameRun(remembered->second, run)) {
            ++counts.refused;
            counts.refusedBytes += bytes;
            ++counts.remembered;
            const auto room = remembered->second.room - std::min(remembered->second.room, remembered->second.budget - budget);
            if (stepDown && containing != run.ranges.end() && room >= (*containing)->bytes) return importAllocation(context, state, address, lease, requester, end, evict, std::min(bytes / 2, room));
            return nullptr;
        }
        state.evictingRefusals.erase(remembered);
    }
    if (requester == ImportRequester::ColorTarget) {
#ifdef _WIN32
        const auto commits = GuestArena::GuestArenaCommitGeneration_nid_postfix();
#else
        const std::uint64_t commits = 0;
#endif
        const auto watches = GuestMemory::UnwatchSerial();
        const std::pair key{base, bytes};
        if (const auto refused = state.gateRefusals.find(key); refused != state.gateRefusals.end() && refused->second.commits == commits && refused->second.watches == watches && sameRun(refused->second, run)) return nullptr;
        if (!importStaysWatched(context, state, run)) {
            state.gateRefusals.insert_or_assign(key, GateRefusal{base, bytes, GuestAllocations::Mapped(run.ranges.begin(), run.ranges.end()), commits, watches});
            return nullptr;
        }
        state.gateRefusals.erase(key);
    }
    auto first = state.imports.lower_bound(base);
    if (first != state.imports.begin() && std::prev(first)->second.base + std::prev(first)->second.bytes > base) --first;
    std::uint64_t replaced = 0;
    // A join may replace imports this build already looked up: it runs before any region confirms an import
    // (UploadFinish's pre-pass re-resolves every pointer after it), and a retired import stays alive through the
    // hold (KeepThroughHold). Refusing it there would keep the run copied for good.
    const auto held = [&](const HostImport& entry) {
        if (joined || !pointerHeld(state, entry)) return false;
        const bool expandedWindow = run.ranges.size() == 1 && entry.ranges.size() == 1 && entry.range == run.ranges.front() && base <= entry.base && base + bytes >= entry.base + entry.bytes && bytes > entry.bytes;
        return !expandedWindow || Recorder::Active() == nullptr;
    };
    for (auto it = first; it != state.imports.end() && it->first < base + bytes; ++it) {
        if (held(it->second)) return stepDown ? importAllocation(context, state, address, lease, requester, end, evict, bytes / 2) : nullptr;
        replaced += it->second.pinnedBytes;
    }
    const auto live = state.liveBytes - replaced;
    auto ready = std::numeric_limits<std::uint64_t>::max();
    auto readyHeld = std::numeric_limits<std::uint64_t>::max();
    const auto evictable = evict && live + bytes > budget ? evictableBytes(state, base, base + bytes, ready, readyHeld) : 0;
    if (live + bytes > budget + evictable) {
        // A refused import turns every later use of the range into CPU copies, so say so.
        static auto lastReport = std::chrono::steady_clock::now() - std::chrono::seconds(60);
        ++counts.refused;
        counts.refusedBytes += bytes;
        const auto remember = [&](std::uint64_t at, std::uint64_t size, GuestAllocations::Mapped ranges) {
            if (!evict && !joined) state.refusals.insert_or_assign(std::pair{at, size}, ImportRefusal{at, size, std::move(ranges), state.liveBytes, budget});
        };
        remember(base, bytes, GuestAllocations::Mapped(run.ranges.begin(), run.ranges.end()));
        if (std::chrono::steady_clock::now() - lastReport > std::chrono::seconds(10)) {
            lastReport = std::chrono::steady_clock::now();
            std::uint64_t refused = 0;
            std::uint64_t refusedBytes = 0;
            std::uint64_t remembered = 0;
            for (const auto& requested : state.counts) {
                refused += requested.refused;
                refusedBytes += requested.refusedBytes;
                remembered += requested.remembered;
            }
            std::fprintf(stderr, "[gpu] host import budget: %llu MiB live of %llu MiB (%s); %llu imports (%llu MiB) refused so far, %llu of them remembered, last 0x%llx+0x%llx by %s\n", static_cast<unsigned long long>(state.liveBytes >> 20u), static_cast<unsigned long long>(budget >> 20u), budgetSource(context, state, budget).c_str(), static_cast<unsigned long long>(refused), static_cast<unsigned long long>(refusedBytes >> 20u), static_cast<unsigned long long>(remembered), static_cast<unsigned long long>(base), static_cast<unsigned long long>(bytes), ImportRequesterName(requester));
        }
        const auto room = budget + evictable > live ? budget + evictable - live : 0;
        if (evict) state.evictingRefusals.insert_or_assign(std::pair{base, bytes}, EvictingRefusal{base, bytes, GuestAllocations::Mapped(run.ranges.begin(), run.ranges.end()), state.epoch, state.framesEnded, state.liveBytes, budget, room, importWindow.load(std::memory_order_relaxed), state.buildFloor, ready, readyHeld, joined});
        if (stepDown && containing != run.ranges.end() && room >= (*containing)->bytes) return importAllocation(context, state, address, lease, requester, end, evict, std::min(bytes / 2, room));
        if (containing != run.ranges.end() && run.ranges.size() > 1) remember((*containing)->address, (*containing)->bytes, GuestAllocations::Mapped{*containing});
        ++state.budgetRefusals;
        return nullptr;
    }
    for (auto it = first; it != state.imports.end() && it->first < base + bytes;) {
        const auto next = std::next(it);
        retireImport(context.device, state, it, lease);
        it = next;
    }
    if (state.liveBytes + bytes > budget) retireLeastRecent(context, state, lease, state.liveBytes + bytes - budget);
    ImportConstruction owner(context, base, bytes);
    auto& entry = owner.entry;
    entry.range = run.ranges.front();
    entry.ranges = std::move(run.ranges);
    entry.memoryBase = base;
    entry.pinnedBytes = bytes;
    entry.backingSerial = entry.ranges.size() == 1 ? entry.range->backingSerial : 0;
    const auto failedBlockImport = [&]() -> const HostImport* {
        ++counts.failed;
        state.failedBlocks.insert_or_assign(std::pair{base, bytes}, ImportFailure{base, bytes, GuestAllocations::Mapped(entry.ranges.begin(), entry.ranges.end())});
        return importAllocation(context, state, address, lease, requester, end, evict);
    };
    const auto failedImport = [&]() -> const HostImport* {
        if (stepDown) return failedBlockImport();
        ++counts.failed;
        failures.insert_or_assign(base, ImportFailure{base, bytes, GuestAllocations::Mapped(entry.ranges.begin(), entry.ranges.end())});
        return nullptr;
    };
#ifdef _WIN32
    // Drivers pin imported pages, so every page must be committed and accessible.
    const GuestArena::HostImportAccess importAccess(reinterpret_cast<void*>(base), static_cast<std::size_t>(bytes));
    const auto pages = importPages(base, bytes);
    if (!pages.committed) return failedImport();
    const bool writable = pages.writable;
    if (!writable && pages.readOnly) return failedImport();
    if (!writable) {
        if (pages.refused.BaseAddress != nullptr) {
            char text[256];
            std::snprintf(text, sizeof(text), "AGC graphics: host import of 0x%llx+0x%llx: memory at 0x%llx (type 0x%lx, protection 0x%lx) is neither read-write, read-only nor a shared mapping", static_cast<unsigned long long>(base), static_cast<unsigned long long>(bytes), reinterpret_cast<unsigned long long>(pages.refused.BaseAddress), pages.refused.Type, pages.refused.Protect);
            throw std::runtime_error(text);
        }
        std::erase_if(state.gateRefusals, [&](const auto& refusal) { return refusal.first.first < base + bytes && base < refusal.first.first + refusal.first.second; });
        try {
            entry.alias = GuestArena::GuestArenaMapAlias_nid_postfix(static_cast<std::uintptr_t>(base), static_cast<std::size_t>(bytes));
        } catch (const std::runtime_error&) {
            if (entry.ranges.size() == 1) throw;
            return joined ? failedImport() : failedBlockImport();
        }
        if (entry.alias == nullptr) return nullptr;
    }
#else
    if (!GuestMemory::Accessible(reinterpret_cast<const void*>(base), static_cast<std::size_t>(bytes), true)) return failedImport();
#endif
    VkResult result = VK_SUCCESS;
    const char* step = nullptr;
#ifdef _WIN32
    const auto watching = importWatching(context, state, entry.alias != nullptr, writable, bytes);
    const bool settled = watching.settled;
#else
    const auto watching = importWatching(context, state, false, true, bytes);
#endif
    const bool watchedHostPointer = watching.hostPointer;
    const bool watchedDmaBuf = watching.dmaBuf;
    GuestMemory::ImportWatched(base, bytes, [&] {
        step = watchedDmaBuf == watchedHostPointer ? createImport(context, entry, result) : createHostPointerImport(context, entry, result);
#ifdef _WIN32
        if (step == nullptr && settled && (step = settleImport(context, state, entry, result)) != nullptr) {
            destroyImport(context, entry);
            entry.buffer = VK_NULL_HANDLE;
            entry.memory = VK_NULL_HANDLE;
        }
#endif
        return step == nullptr;
    }, watchedHostPointer);
#ifndef _WIN32
    if (BackingFile backing; step != nullptr && watchedDmaBuf != watchedHostPointer && sharedBacking(context, entry, backing)) {
        GuestMemory::ImportWatched(base, bytes, [&] {
            step = createBackingImport(context, entry, backing, result);
            return step == nullptr;
        }, watchedDmaBuf, false);
    }
    if (step == nullptr && entry.dmaBuf && bytes >= settleFloorBytes && context.settleQueue != VK_NULL_HANDLE && context.settleQueueMutex != nullptr) {
        static const bool traceSettle = std::getenv("APS5_TRACE_HOST_IMPORT") != nullptr;
        const auto start = std::chrono::steady_clock::now();
        if ((step = settleImport(context, state, entry, result)) != nullptr) {
            destroyImport(context, entry);
            entry.buffer = VK_NULL_HANDLE;
            entry.memory = VK_NULL_HANDLE;
            entry.chunk.reset();
        } else if (traceSettle) {
            std::fprintf(stderr, "[gpu] settled the dma-buf import of 0x%llx+0x%llx on the settle queue in %.2f ms\n", static_cast<unsigned long long>(base), static_cast<unsigned long long>(bytes), std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
        }
    }
#endif
    entry.unwatched = step == nullptr && !(entry.dmaBuf ? watchedDmaBuf : watchedHostPointer);
    if (step != nullptr) {
#ifdef _WIN32
        GuestArena::GuestArenaUnmapAlias_nid_postfix(std::exchange(entry.alias, nullptr));
#endif
        std::fprintf(stderr, "[gpu] host import of 0x%llx+0x%llx failed at %s (%d); falling back to copies, requested by %s\n", static_cast<unsigned long long>(base), static_cast<unsigned long long>(bytes), step, static_cast<int>(result), ImportRequesterName(requester));
        const bool exhausted = result == VK_ERROR_OUT_OF_HOST_MEMORY || result == VK_ERROR_OUT_OF_DEVICE_MEMORY;
        if (std::string_view(step) == "vkAllocateMemory") {
            state.allocationFailed = true;
            if (exhausted && (++state.allocationFailures >= 3 || state.liveBytes + bytes + budget / 8 >= budget)) {
                state.ceiling = std::max<std::uint64_t>(state.liveBytes, 1);
                std::fprintf(stderr, "[gpu] host import budget lowered to the %.1f MiB live after vkAllocateMemory failed %s\n", state.liveBytes / 1048576.0, state.allocationFailures >= 3 ? "3 times in a row" : "within an eighth of the budget");
            }
        }
#ifdef _WIN32
        static const bool trace = std::getenv("APS5_TRACE_HOST_IMPORT") != nullptr;
        for (std::uint64_t cursor = base; trace && cursor < base + bytes;) {
            MEMORY_BASIC_INFORMATION info{};
            if (VirtualQuery(reinterpret_cast<const void*>(cursor), &info, sizeof(info)) == 0) break;
            const auto regionEnd = reinterpret_cast<std::uint64_t>(info.BaseAddress) + info.RegionSize;
            std::fprintf(stderr, "[gpu]   0x%llx+0x%llx state 0x%lx protect 0x%lx type 0x%lx allocation 0x%llx\n", static_cast<unsigned long long>(cursor), static_cast<unsigned long long>(std::min(regionEnd, base + bytes) - cursor), info.State, info.Protect, info.Type, reinterpret_cast<unsigned long long>(info.AllocationBase));
            cursor = regionEnd;
        }
#else
        static const bool trace = std::getenv("APS5_TRACE_HOST_IMPORT") != nullptr;
        if (std::FILE* maps = trace ? std::fopen("/proc/self/maps", "r") : nullptr) {
            char line[512];
            while (std::fgets(line, sizeof(line), maps) != nullptr) {
                unsigned long long first = 0;
                unsigned long long last = 0;
                if (std::sscanf(line, "%llx-%llx", &first, &last) != 2 || last <= base || first >= base + bytes) continue;
                std::fprintf(stderr, "[gpu]   %s", line);
            }
            std::fclose(maps);
        }
#endif
        return failedImport();
    }
    static std::uint64_t importedBytes = 0;
    importedBytes += bytes;
    static const bool trace = std::getenv("APS5_TRACE_HOST_IMPORT") != nullptr;
    state.liveBytes += bytes;
    state.allocationFailures = 0;
    ++counts.made;
    counts.madeBytes += bytes;
    for (auto it = state.refusals.lower_bound({base, 0}); it != state.refusals.end() && it->first.first < base + bytes;) it = it->first.first + it->first.second <= base + bytes ? state.refusals.erase(it) : std::next(it);
    if (trace) std::fprintf(stderr, "[gpu] host import of 0x%llx+0x%llx ok (%zu live, %.1f MiB live, %.1f MiB ever)%s by %s\n", static_cast<unsigned long long>(base), static_cast<unsigned long long>(bytes), state.imports.size() + 1, state.liveBytes / 1048576.0, importedBytes / 1048576.0, joined ? ", joined at bind" : "", ImportRequesterName(requester));
    entry.joined = joined;
    entry.lastUse = ++state.useCounter;
    state.pendingImports.insert(base);
#ifdef _WIN32
    if (deferredSettles.load(std::memory_order_relaxed) && entry.alias == nullptr && state.watchPrivateImports && writable) {
        if (entry.unwatched) {
            state.pendingSettles.insert_or_assign(base, PendingSettle{});
            state.settleWanted.push_back(base);
        } else if (base % settleBlockBytes == 0 && bytes % settleBlockBytes == 0 && settled && !GuestMemory::Watched(base, static_cast<std::size_t>(bytes))) {
            entry.unwatched = true;
            state.pendingSettles.insert_or_assign(base, PendingSettle{SettlePhase::Settled, 0});
            state.settleReady.push_back(base);
        }
    }
#endif
    const auto inserted = state.imports.emplace(base, entry);
    owner.armed = false;
    return &inserted.first->second;
}

// The readable registered range containing [begin, end), or null.
const GuestAllocations::Range* containingRange(const GuestAllocations::Lease& lease, std::uint64_t begin, std::uint64_t end) {
    const auto found = std::upper_bound(lease.begin(), lease.end(), begin, [](std::uint64_t value, const auto& range) { return value < range->address; });
    if (found == lease.begin()) return nullptr;
    const auto& range = *std::prev(found);
    return range->readable && begin >= range->address && end <= range->address + range->bytes ? range.get() : nullptr;
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> changedSpans(std::vector<RefreshedRange>& refreshed, const GuestAllocations::Lease& lease) {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> spans;
    std::vector<RefreshedRange> next;
    next.reserve(lease.size());
    const auto add = [&](std::uint64_t address, std::uint64_t bytes) {
        if (!spans.empty() && address <= spans.back().second) spans.back().second = std::max(spans.back().second, address + bytes);
        else spans.emplace_back(address, address + bytes);
    };
    const auto take = [&](const std::shared_ptr<const GuestAllocations::Range>& range) {
        add(range->address, range->bytes);
        next.push_back({range->address, range->bytes, range});
    };
    std::size_t first = 0;
    std::size_t second = 0;
    while (first < refreshed.size() || second < lease.size()) {
        if (second == lease.size() || (first < refreshed.size() && refreshed[first].address < lease[second]->address)) {
            add(refreshed[first].address, refreshed[first].bytes);
            ++first;
        } else if (first == refreshed.size() || lease[second]->address < refreshed[first].address) {
            take(lease[second++]);
        } else {
            if (refreshed[first].range.owner_before(lease[second]) || lease[second].owner_before(refreshed[first].range)) {
                add(refreshed[first].address, refreshed[first].bytes);
                take(lease[second]);
            } else {
                next.push_back(std::move(refreshed[first]));
            }
            ++first;
            ++second;
        }
    }
    refreshed = std::move(next);
    return spans;
}

// Drops imports whose registered range changed or disappeared: their pages may no longer back the
// guest addresses. Walks the imports only when the registry changed since the last walk.
void refreshImports(const Context& context, HostImports& state, const GuestAllocations::Lease& lease) {
    if (state.device != context.device) {
#ifdef _WIN32
        drainSettles(state);
#endif
        while (!state.imports.empty()) {
            auto node = state.imports.extract(state.imports.begin());
            if (state.device != VK_NULL_HANDLE && state.destroyBuffer != nullptr && state.freeMemory != nullptr) {
                state.parked[state.device].push_back(std::make_shared<RetiredImport>(state.device, state.destroyBuffer, state.freeMemory, std::move(node)));
                continue;
            }
#ifdef _WIN32
            GuestArena::GuestArenaUnmapAlias_nid_postfix(node.mapped().alias);
#endif
        }
        state.liveBytes = 0;
        state.failed.clear();
        state.failedJoins.clear();
        state.failedBlocks.clear();
        state.refusals.clear();
        state.evictingRefusals.clear();
        state.gateRefusals.clear();
        state.ceiling = 0;
        state.allocationFailures = 0;
        state.allocationFailed = false;
        state.reportNoted = 0;
        state.sparseRunEnds.clear();
        state.sparseRunJoins.clear();
        state.importedAtMap.clear();
        state.device = context.device;
        state.destroyBuffer = context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer");
        state.freeMemory = context.Function<PFN_vkFreeMemory>("vkFreeMemory");
        state.refreshedRanges.clear();
        state.refreshedWindow = 0;
        state.pendingImports.clear();
        state.refreshedGeneration = 0;
        ++state.epoch;
    }
    if (state.destroyBuffer == nullptr) state.destroyBuffer = context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer");
    if (state.freeMemory == nullptr) state.freeMemory = context.Function<PFN_vkFreeMemory>("vkFreeMemory");
    const auto generation = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    if (generation == state.refreshedGeneration) return;
    state.refreshedGeneration = generation;
    state.gateRefusals.clear();
    const auto budget = effectiveBudget(context, state);
    const auto window = importWindow.load(std::memory_order_relaxed);
    const bool whole = window == 0 || window != state.refreshedWindow || budget != state.refreshedBudget;
    const auto changed = changedSpans(state.refreshedRanges, lease);
    state.refreshedWindow = window;
    state.refreshedBudget = budget;
    auto pending = std::exchange(state.pendingImports, {});
    const auto affected = [&](std::uint64_t base, std::uint64_t bytes) {
        if (whole) return true;
        const auto low = base / window * window;
        const auto high = std::max(low + window, base + bytes);
        const auto found = std::upper_bound(changed.begin(), changed.end(), low, [](std::uint64_t value, const auto& span) { return value < span.second; });
        return found != changed.end() && found->first < high;
    };
    const auto starts = [&](std::uint64_t address) {
        const auto index = leasedIndexContaining(lease, address);
        return index != lease.size() && lease[index]->address == address;
    };
    std::erase_if(state.sparseRunEnds, [&](const auto& entry) { return affected(entry.first, 1) && !starts(entry.first); });
    std::erase_if(state.sparseRunJoins, [&](const auto& entry) { return affected(entry.first, 1) && !starts(entry.first); });
    std::vector<std::pair<std::uint64_t, std::uint64_t>> unwatched;
    const auto check = [&](std::map<std::uint64_t, HostImport>::iterator it) {
        if (sameRange(it->second, lease, budget)) {
            if (it->second.unwatched) GuestMemory::Unwatch(it->first, it->second.bytes);
            return std::next(it);
        }
        static const bool trace = std::getenv("APS5_TRACE_HOST_IMPORT") != nullptr;
        if (trace && leasedRangeOwner(lease, it->first, it->first + it->second.bytes) != nullptr) std::fprintf(stderr, "[gpu] host import of 0x%llx+0x%llx retired: the range was mapped again\n", static_cast<unsigned long long>(it->first), static_cast<unsigned long long>(it->second.bytes));
        const auto next = std::next(it);
        noteRetiredUnwatched(unwatched, it->second, lease);
        retireImport(context.device, state, it, lease);
        return next;
    };
    if (whole) {
        for (auto it = state.imports.begin(); it != state.imports.end();) it = check(it);
    } else {
        std::uint64_t unchecked = 0;
        for (const auto& [begin, end] : changed) {
            auto it = state.imports.lower_bound(std::max(begin / window * window, unchecked));
            if (it != state.imports.begin() && std::prev(it)->first >= unchecked && std::prev(it)->first + std::prev(it)->second.bytes > begin) --it;
            while (it != state.imports.end() && it->first / window * window < end) {
                pending.erase(it->first);
                it = check(it);
            }
            unchecked = it == state.imports.end() ? std::numeric_limits<std::uint64_t>::max() : it->first;
        }
        for (const auto base : pending) {
            if (const auto it = state.imports.find(base); it != state.imports.end()) check(it);
        }
    }
    rewatchRetired(state, std::move(unwatched));
    for (auto it = state.failed.begin(); it != state.failed.end();) it = !affected(it->second.base, it->second.bytes) || sameRange(it->second, lease, budget) ? std::next(it) : state.failed.erase(it);
    for (auto it = state.failedJoins.begin(); it != state.failedJoins.end();) it = !affected(it->second.base, it->second.bytes) || sameRun(it->second, joinedRun(lease, it->second.base, it->second.base + it->second.bytes)) ? std::next(it) : state.failedJoins.erase(it);
    for (auto it = state.failedBlocks.begin(); it != state.failedBlocks.end();) it = !affected(it->first.first, it->first.second) || sameRun(it->second, exactRun(lease, it->first.first, it->first.second)) ? std::next(it) : state.failedBlocks.erase(it);
    for (auto it = state.refusals.begin(); it != state.refusals.end();) it = !affected(it->first.first, it->first.second) || (stillRefused(state, &it->second, budget) && sameRun(it->second, exactRun(lease, it->first.first, it->first.second))) ? std::next(it) : state.refusals.erase(it);
    for (auto it = state.evictingRefusals.begin(); it != state.evictingRefusals.end();) it = !affected(it->first.first, it->first.second) || (evictingRefusalCurrent(state, it->second, budget) && sameRun(it->second, exactRun(lease, it->first.first, it->first.second))) ? std::next(it) : state.evictingRefusals.erase(it);
}

const HostImport* peekImport(const HostImports& state, std::uint64_t begin, std::uint64_t end) {
    auto found = state.imports.upper_bound(begin);
    if (found == state.imports.begin()) return nullptr;
    --found;
    const auto& entry = found->second;
    return begin < entry.base || end > entry.base + entry.bytes ? nullptr : &entry;
}

const HostImport* findImport(HostImports& state, std::uint64_t begin, std::uint64_t end) {
    const auto* entry = peekImport(state, begin, end);
    if (entry != nullptr) entry->lastUse = ++state.useCounter;
    return entry;
}

const HostImport* importAccess(const Context& context, HostImports& state, std::uint64_t begin, std::uint64_t end, const GuestAllocations::Lease& lease, ImportRequester requester) {
    if (!runCovers(lease, begin, end)) return findImport(state, begin, end);
    const auto* entry = importAllocation(context, state, begin, lease, requester, end);
    return entry != nullptr && findImport(state, begin, end) == entry ? entry : nullptr;
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
    AgcDriver::Mutex mutex;
    VkDevice device = VK_NULL_HANDLE;
    std::map<std::uint64_t, std::shared_ptr<ImageMirror>> entries;
    std::set<std::uint64_t> failed;
    std::uint64_t heapBytes = 0;
    // APS5_PROFILE_DRAW counters, reported every 10 s as [buffers] image mirrors.
    std::uint64_t builds = 0;
    std::uint64_t heapRefills = 0;
    std::uint64_t heapUnwatched = 0;
    std::uint64_t heapImported = 0;
    std::uint64_t heapImportedBytes = 0;
    std::uint64_t subranges = 0;
    std::uint64_t sparseBinds = 0;
    std::uint64_t rebuilds = 0;
    std::uint64_t refreshes = 0;
    std::uint64_t blocksCompared = 0;
    std::uint64_t blocksCopied = 0;
    // Refreshes that had to wait for recorded work writing the range.
    std::uint64_t syncs = 0;
    std::uint64_t serials = 0;
    std::uint64_t version = 0;
    std::uint64_t pinnedSpace = 0;
    std::uint64_t pinnedVersion = 0;
    std::uint64_t sweeps = 0;
    std::uint64_t heapChecks = 0;
    std::uint64_t evictions = 0;
    std::uint64_t dispatchCounter = 0;
    std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
};

// APS5_PROFILE_DRAW: the parts of one address-based build (see GuestBufferMemory::CountAddressBuild),
// timed on the building thread by AcquireRegistered, summed into the totals with the caller's
// snapshot compares and printed as the [address] line every 10 s.
struct AddressBuildTiming {
    double leaseUs = 0;
    double importsUs = 0;
    double mirrorsUs = 0;
    double compareUs = 0;
    std::uint64_t blocksCompared = 0;
    std::uint64_t blocksCopied = 0;
};

AddressBuildTiming& ThreadAddressTiming() {
    thread_local AddressBuildTiming timing;
    return timing;
}

struct AddressBuildTotals {
    AgcDriver::Mutex mutex;
    std::uint64_t builds = 0;
    AddressBuildTiming sums;
    double snapshotsUs = 0;
    BdaResources::TableCacheStats tableSeen;
    AddressSpaceStats spaceSeen;
    std::chrono::steady_clock::time_point lastReport = std::chrono::steady_clock::now();
};

AddressBuildTotals& AddressBuilds() {
    static AddressBuildTotals totals;
    return totals;
}

ImageMirrors& Mirrors() {
    static ImageMirrors mirrors;
    return mirrors;
}

using SnapshotCopies = Detail::SnapshotCopies;

SnapshotCopies& SnapshotCopyCache() {
    static SnapshotCopies copies;
    return copies;
}

std::shared_ptr<Buffer> snapshotCopy(const Context& context, std::uint64_t address, std::span<const std::byte> bytes, VkBufferUsageFlags usage) {
    auto& copies = SnapshotCopyCache();
    std::lock_guard lock(copies.mutex);
    if (copies.device != context.device) {
        copies.Clear();
        copies.device = context.device;
    }
    const SnapshotCopies::Key key{address, bytes.size(), usage};
    if (const auto found = copies.entries.find(key); found != copies.entries.end()) {
        if (std::memcmp(found->second.bytes.data(), bytes.data(), bytes.size()) == 0) {
            copies.recency.splice(copies.recency.end(), copies.recency, found->second.recent);
            return found->second.buffer;
        }
        copies.recency.erase(found->second.recent);
        copies.bytes -= bytes.size();
        copies.entries.erase(found);
    }
    auto buffer = std::make_shared<Buffer>(context, bytes.size(), usage);
    std::memcpy(buffer->Bytes().data(), bytes.data(), bytes.size());
    if (bytes.size() > SnapshotCopies::Budget) return buffer;
    while (!copies.recency.empty() && (copies.bytes + bytes.size() > SnapshotCopies::Budget || copies.entries.size() >= SnapshotCopies::MaxEntries)) {
        const auto oldest = copies.entries.find(copies.recency.front());
        copies.bytes -= std::get<1>(oldest->first);
        copies.entries.erase(oldest);
        copies.recency.pop_front();
    }
    copies.Insert(key, buffer, bytes);
    return buffer;
}

bool mirrorsEnabled() {
    static const bool disabled = std::getenv("APS5_NO_LEASE_MIRROR") != nullptr;
    return !disabled;
}

std::uint64_t heapMirrorBudget() {
    if (const auto test = testHeapMirrorBudget.load(std::memory_order_acquire); test != 0) return test;
    static const std::uint64_t bytes = [] {
        const char* value = std::getenv("APS5_HEAP_MIRROR_MIB");
        return (value ? std::strtoull(value, nullptr, 10) : 24576ull) << 20u;
    }();
    return bytes;
}

std::string heapMirrorError(std::uint64_t address, std::uint64_t bytes, std::uint64_t held, std::string_view reason) {
    constexpr std::string_view prefix = "AGC graphics: ";
    if (reason.starts_with(prefix)) reason.remove_prefix(prefix.size());
    char where[64];
    std::snprintf(where, sizeof(where), "heap mirror of 0x%llx+0x%llx: ", static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes));
    char holding[96];
    std::snprintf(holding, sizeof(holding), "; heap mirrors hold %llu MiB of %llu MiB (APS5_HEAP_MIRROR_MIB)", static_cast<unsigned long long>(held >> 20u), static_cast<unsigned long long>(heapMirrorBudget() >> 20u));
    return std::string(prefix) + where + std::string(reason) + holding;
}

void sweepMirrors(std::uint64_t neededBytes = 0, std::uint64_t protectAddress = 0);

void unlistMirror(ImageMirrors& state, ImageMirror& mirror) {
    if (!mirror.listed) return;
    state.heapBytes -= mirror.accounted;
    mirror.listed = false;
}

struct JoinedHeapRange {
    std::vector<std::weak_ptr<const GuestAllocations::Range>> members;
    std::shared_ptr<const GuestAllocations::Range> range;
};

struct JoinedHeapRanges {
    std::mutex mutex;
    std::map<std::uint64_t, JoinedHeapRange> entries;
};

JoinedHeapRanges& JoinedHeaps() {
    static JoinedHeapRanges ranges;
    return ranges;
}

bool releaseExpiredJoinedHeaps() {
    std::vector<std::shared_ptr<const GuestAllocations::Range>> released;
    {
        auto& joined = JoinedHeaps();
        std::lock_guard lock(joined.mutex);
        for (auto it = joined.entries.begin(); it != joined.entries.end();) {
            const auto& members = it->second.members;
            if (std::none_of(members.begin(), members.end(), [](const auto& member) { return member.expired(); })) {
                ++it;
                continue;
            }
            released.push_back(std::move(it->second.range));
            it = joined.entries.erase(it);
        }
    }
    return !released.empty();
}

std::uint64_t blockBytes(const GuestRuns& runs) {
    constexpr std::uint64_t block = ResidentBuffer::BlockBytes;
    std::uint64_t blocks = 0;
    std::uint64_t last = 0;
    bool any = false;
    for (const auto& [first, end] : runs) {
        if (first >= end) continue;
        auto index = first / block;
        if (any && index == last) ++index;
        if (index <= (end - 1) / block) blocks += (end - 1) / block - index + 1;
        last = (end - 1) / block;
        any = true;
    }
    return blocks * block;
}

void reserveHeapBytes(std::uint64_t address, std::uint64_t bytes, std::uint64_t need) {
    auto& state = Mirrors();
    for (int attempt = 0;; ++attempt) {
        {
            std::lock_guard lock(state.mutex);
            std::uint64_t held = state.heapBytes;
            if (const auto found = state.entries.find(address); found != state.entries.end() && found->second->heap && found->second->listed) held -= found->second->accounted;
            if (held + need <= heapMirrorBudget()) {
                state.heapBytes += need;
                return;
            }
            if (attempt != 0) throw std::runtime_error(heapMirrorError(address, bytes, held, "past the budget"));
        }
        sweepMirrors(need, address);
    }
}

void growResident(ImageMirror& mirror, const GuestRuns& runs) {
    if (mirror.resident == nullptr) return;
    const auto need = mirror.resident->UnboundBytes(runs);
    if (need == 0) return;
    auto& state = Mirrors();
    bool counted = false;
    for (int attempt = 0; !counted; ++attempt) {
        {
            std::lock_guard lock(state.mutex);
            if (!mirror.listed) break;
            if (state.heapBytes + need <= heapMirrorBudget()) {
                state.heapBytes += need;
                mirror.accounted += need;
                counted = true;
                break;
            }
            if (attempt != 0) throw std::runtime_error(heapMirrorError(mirror.base, mirror.reserved, state.heapBytes, "past the budget"));
        }
        sweepMirrors(need, mirror.base);
    }
    try {
        mirror.resident->Bind(runs);
    } catch (const std::runtime_error& error) {
        std::uint64_t held = 0;
        {
            std::lock_guard lock(state.mutex);
            if (counted && mirror.listed) {
                state.heapBytes -= need;
                mirror.accounted -= need;
            }
            held = state.heapBytes;
        }
        throw std::runtime_error(heapMirrorError(mirror.base, mirror.reserved, held, error.what()));
    }
}

std::uint64_t mirrorStart(const ImageMirror& mirror) {
    return mirror.resident != nullptr ? mirror.resident->Origin() : mirror.base;
}

VkBuffer mirrorHandle(const ImageMirror& mirror) {
    return mirror.resident != nullptr ? mirror.resident->Handle() : mirror.buffer->Handle();
}

VkDeviceAddress mirrorAddress(const ImageMirror& mirror) {
    return mirror.resident != nullptr ? mirror.resident->DeviceAddress() : mirror.buffer->DeviceAddress();
}

bool sameRange(const std::weak_ptr<const GuestAllocations::Range>& mirrored, const std::shared_ptr<const GuestAllocations::Range>& range) {
    return !mirrored.owner_before(range) && !range.owner_before(mirrored);
}

// One 64 KiB (or shorter, at a range's ends) block of a writable mirror to compare with guest memory.
struct RefreshBlock {
    ImageMirror* mirror;
    std::uint64_t address;
    std::size_t length;
    std::uint64_t generation = 0;
    std::uint8_t fill = 0;
};

constexpr std::uint8_t SparseCopy = 1;
constexpr std::uint8_t SparseZero = 2;

// Compares a block's guest bytes with the mirror's shadow and copies a changed block into the device
bool compareBlock(const RefreshBlock& block) {
    const auto offset = static_cast<std::size_t>(block.address - block.mirror->base);
    const auto* guest = reinterpret_cast<const std::byte*>(block.address);
    if (block.mirror->resident != nullptr) {
        auto* target = block.mirror->resident->Block(block.address);
        if (block.fill == SparseZero) {
            if (target != nullptr) std::memset(target, 0, block.length);
            return true;
        }
        if (target != nullptr) std::memcpy(target, guest, block.length);
        if (block.fill == 0) block.mirror->generations[static_cast<std::size_t>(block.address / 65536 - block.mirror->base / 65536)] = block.generation;
        return target != nullptr;
    }
    if (block.fill != 0) {
        auto* target = block.mirror->buffer->Bytes().data() + offset;
        if (block.fill == SparseZero) std::memset(target, 0, block.length);
        else std::memcpy(target, guest, block.length);
        return true;
    }
    if (block.mirror->heap) {
        std::memcpy(block.mirror->buffer->Bytes().data() + offset, guest, block.length);
        block.mirror->generations[static_cast<std::size_t>(block.address / 65536 - block.mirror->base / 65536)] = block.generation;
        return true;
    }
    if (std::memcmp(block.mirror->shadow.data() + offset, guest, block.length) == 0) return false;
    std::memcpy(block.mirror->buffer->Bytes().data() + offset, guest, block.length);
    std::memcpy(block.mirror->shadow.data() + offset, guest, block.length);
    return true;
}

class RefreshPool {
public:
    static RefreshPool& Get() {
        if (instance == nullptr) instance = new RefreshPool;
        return *instance;
    }

    static void Shutdown() {
        delete std::exchange(instance, nullptr);
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
        for (unsigned i = 0; i < wanted; ++i) {
            threads.emplace_back([this](std::stop_token token) {
                CpuTopology::PinHelperThread("mirror refresh");
                helper(token);
            });
            ++helpers;
        }
    }

    // Takes blocks until none is left.
    void work(std::span<const RefreshBlock> blocks) {
        std::uint64_t copied = 0;
        for (auto index = next.fetch_add(1, std::memory_order_relaxed); index < blocks.size(); index = next.fetch_add(1, std::memory_order_relaxed)) copied += compareBlock(blocks[index]) ? 1 : 0;
        copiedBlocks.fetch_add(copied, std::memory_order_relaxed);
    }

    void helper(std::stop_token token) {
        std::uint64_t seen = 0;
        for (;;) {
            std::span<const RefreshBlock> blocks;
            {
                std::unique_lock lock(mutex);
                if (!wake.wait(lock, token, [&] { return serial != seen; })) return;
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

    inline static RefreshPool* instance = nullptr;
    unsigned helpers = 0;
    AgcDriver::Mutex runMutex;
    std::mutex mutex;
    std::condition_variable_any wake;
    std::condition_variable done;
    std::span<const RefreshBlock> job;
    std::uint64_t serial = 0;
    unsigned finished = 0;
    std::atomic<std::size_t> next{0};
    std::atomic<std::uint64_t> copiedBlocks{0};
    std::vector<std::jthread> threads;
};

// The device-lock work before [address, address + bytes) of a writable mirror can be compared with
// guest memory: recorded GPU work that writes the range (a descriptor dispatch bound to a sub-range)
// completes first, so its results reach the guest and the shadow before the compare and no batch
// writes a block being replaced; GPU reads of the mirror by earlier recorded work (an address-based
// dispatch whose lease is released at completion) see the newer bytes, as they would on hardware.
void syncRange(std::uint64_t address, std::uint64_t bytes) {
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
}

void prepareRange(std::uint64_t address, std::uint64_t bytes) {
    Require(GuestMemory::Accessible(reinterpret_cast<const void*>(address), static_cast<std::size_t>(bytes)), "image mirror range became inaccessible");
    syncRange(address, bytes);
}

bool prepareRefresh(ImageMirror& mirror, std::uint64_t address, std::uint64_t bytes) {
    if ((!mirror.writable && !mirror.heap) || bytes == 0) return false;
    prepareRange(address, bytes);
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
void bindHeapBlocks(std::span<const RefreshBlock> blocks) {
    std::vector<std::pair<ImageMirror*, GuestRuns>> wanted;
    for (const auto& block : blocks) {
        if (block.mirror->resident == nullptr || block.fill == SparseZero) continue;
        if (block.mirror->resident->Block(block.address) == nullptr && GuestBytesZero(block.address, block.address + block.length)) continue;
        if (wanted.empty() || wanted.back().first != block.mirror) {
            const auto found = std::find_if(wanted.begin(), wanted.end(), [&](const auto& entry) { return entry.first == block.mirror; });
            if (found == wanted.end()) wanted.emplace_back(block.mirror, GuestRuns{});
            else std::rotate(found, std::next(found), wanted.end());
        }
        auto& runs = wanted.back().second;
        if (!runs.empty() && runs.back().second == block.address) runs.back().second = block.address + block.length;
        else runs.emplace_back(block.address, block.address + block.length);
    }
    for (auto& [mirror, runs] : wanted) {
        std::sort(runs.begin(), runs.end());
        growResident(*mirror, runs);
    }
}

void compareBlocks(std::span<const RefreshBlock> blocks) {
    if (blocks.empty()) return;
    bindHeapBlocks(blocks);
    const auto copied = RefreshPool::Get().Run(blocks);
    for (const auto& block : blocks) {
        if (block.mirror->resident != nullptr && block.mirror->resident->Block(block.address) != nullptr) block.mirror->resident->MarkChanged(block.address, block.address + block.length);
    }
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

void appendChangedBlocks(std::span<ImageMirror* const> mirrors, std::uint64_t generation, std::vector<RefreshBlock>& blocks) {
    constexpr std::uint64_t block = 65536;
    thread_local std::vector<std::uint8_t>* changedSlot = nullptr;
    auto& changed = ShaderRecompiler::ThreadOwned(changedSlot);
    Mirrors().heapChecks += mirrors.size();
    for (auto* const pointer : mirrors) {
        auto& mirror = *pointer;
        changed.assign(mirror.generations.size(), 0);
        GuestMemory::ChangedBlocks(mirror.base, static_cast<std::size_t>(mirror.bytes), mirror.generations, changed);
        const auto aligned = mirror.base / block * block;
        const auto end = mirror.base + mirror.bytes;
        bool refilled = false;
        for (std::size_t at = 0; at < changed.size(); ++at) {
            if (changed[at] == 0) continue;
            const auto from = std::max(mirror.base, aligned + at * block);
            blocks.push_back({&mirror, from, static_cast<std::size_t>(std::min(end, aligned + (at + 1) * block) - from), generation});
            refilled = true;
        }
        if (refilled) ++Mirrors().heapRefills;
    }
}

void markUnwatched(ImageMirror& mirror) {
    ++Mirrors().heapUnwatched;
    std::fill(mirror.generations.begin(), mirror.generations.end(), 0);
}

void appendHeapRun(std::span<ImageMirror* const> mirrors, std::uint64_t generation, std::vector<RefreshBlock>& blocks) {
    if (generation != 0) {
        appendChangedBlocks(mirrors, generation, blocks);
        return;
    }
    for (std::size_t index = 0; index < mirrors.size(); ++index) {
        auto& mirror = *mirrors[index];
        const auto own = GuestMemory::CollectWrites(mirror.base, static_cast<std::size_t>(mirror.bytes));
        if (own == 0) markUnwatched(mirror);
        appendChangedBlocks(mirrors.subspan(index, 1), own, blocks);
    }
}

void refreshHeapMirrors(std::vector<ImageMirror*>& mirrors, std::vector<RefreshBlock>& blocks) {
    std::sort(mirrors.begin(), mirrors.end(), [](const ImageMirror* left, const ImageMirror* right) { return left->base < right->base; });
    for (std::size_t first = 0; first < mirrors.size();) {
        auto last = first + 1;
        while (last < mirrors.size() && mirrors[last]->base == mirrors[last - 1]->base + mirrors[last - 1]->bytes) ++last;
        const auto begin = mirrors[first]->base;
        const auto bytes = mirrors[last - 1]->base + mirrors[last - 1]->bytes - begin;
        prepareRange(begin, bytes);
        const auto generation = GuestMemory::CollectWrites(begin, static_cast<std::size_t>(bytes));
        appendHeapRun(std::span<ImageMirror* const>(mirrors).subspan(first, last - first), generation, blocks);
        first = last;
    }
}

bool sparseMirrorsEnabled() {
    static const bool disabled = std::getenv("APS5_NO_SPARSE_HEAP_MIRROR") != nullptr;
    return !disabled;
}

constexpr std::uint32_t MaxSparseRunJoins = 64;

using Pieces = std::vector<std::pair<std::uint64_t, std::uint64_t>>;

std::uint64_t sparseExtent(std::uint64_t base, std::uint64_t reserved, const Pieces& pieces) {
    constexpr std::uint64_t granule = std::uint64_t{1} << 20u;
    const auto used = pieces.empty() ? 0 : pieces.back().second - base;
    const auto wanted = used + used / 8;
    return std::min(reserved, std::max<std::uint64_t>(granule, (wanted + granule - 1) / granule * granule));
}

std::uint64_t collectPieces(const Pieces& pieces) {
    std::uint64_t generation = 0;
    for (const auto& [first, last] : pieces) {
        generation = GuestMemory::CollectWrites(first, static_cast<std::size_t>(last - first));
        if (generation == 0) return 0;
    }
    return generation != 0 ? generation : GuestMemory::TrackerGeneration();
}

bool covers(const Pieces& pieces, std::uint64_t address) {
    return std::any_of(pieces.begin(), pieces.end(), [&](const auto& piece) { return piece.first <= address && address < piece.second; });
}

void forgetCommitChanges(ImageMirror& mirror, const Pieces& pieces) {
    if (pieces == mirror.committed) return;
    std::vector<std::uint64_t> edges;
    for (const auto* list : std::array<const Pieces*, 2>{&pieces, &mirror.committed}) {
        for (const auto& [first, last] : *list) {
            edges.push_back(first);
            edges.push_back(last);
        }
    }
    std::sort(edges.begin(), edges.end());
    edges.erase(std::unique(edges.begin(), edges.end()), edges.end());
    constexpr std::uint64_t block = 65536;
    const auto end = mirror.base + mirror.bytes;
    for (std::size_t at = 0; at + 1 < edges.size(); ++at) {
        const auto from = std::max(edges[at], mirror.base);
        const auto to = std::min(edges[at + 1], end);
        if (from >= to || covers(pieces, from) == covers(mirror.committed, from)) continue;
        for (auto index = from / block - mirror.base / block; index <= (to - 1) / block - mirror.base / block; ++index) mirror.generations[static_cast<std::size_t>(index)] = 0;
    }
}

void appendSparseBlocks(ImageMirror& mirror, Pieces pieces, std::uint64_t generation, std::vector<RefreshBlock>& blocks) {
    constexpr std::uint64_t block = 65536;
    thread_local std::vector<std::uint8_t>* changedSlot = nullptr;
    auto& changed = ShaderRecompiler::ThreadOwned(changedSlot);
    forgetCommitChanges(mirror, pieces);
    ++Mirrors().heapChecks;
    changed.assign(mirror.generations.size(), 0);
    GuestMemory::ChangedBlocks(mirror.base, static_cast<std::size_t>(mirror.bytes), mirror.generations, changed);
    const auto aligned = mirror.base / block * block;
    const auto end = mirror.base + mirror.bytes;
    bool refilled = false;
    std::size_t piece = 0;
    for (std::size_t at = 0; at < changed.size(); ++at) {
        if (changed[at] == 0) continue;
        refilled = true;
        mirror.generations[at] = generation;
        const auto from = std::max(mirror.base, aligned + at * block);
        const auto to = std::min(end, aligned + (at + 1) * block);
        auto cursor = from;
        while (piece < pieces.size() && pieces[piece].second <= from) ++piece;
        for (auto next = piece; next < pieces.size() && pieces[next].first < to; ++next) {
            const auto first = std::max(pieces[next].first, from);
            const auto last = std::min(pieces[next].second, to);
            if (cursor < first) blocks.push_back({&mirror, cursor, static_cast<std::size_t>(first - cursor), 0, SparseZero});
            blocks.push_back({&mirror, first, static_cast<std::size_t>(last - first), 0, SparseCopy});
            cursor = last;
        }
        if (cursor < to) blocks.push_back({&mirror, cursor, static_cast<std::size_t>(to - cursor), 0, SparseZero});
    }
    if (refilled) ++Mirrors().heapRefills;
    mirror.committed = std::move(pieces);
}

Pieces clipPieces(const Pieces& pieces, std::uint64_t begin, std::uint64_t end) {
    Pieces result;
    for (const auto& [first, last] : pieces) {
        if (std::max(first, begin) < std::min(last, end)) result.emplace_back(std::max(first, begin), std::min(last, end));
    }
    return result;
}

bool refreshSparseMirror(ImageMirror& mirror, std::vector<RefreshBlock>& blocks) {
    auto pieces = GuestMemory::DescribeCommitted(mirror.base, static_cast<std::size_t>(mirror.reserved)).ranges;
    if (!pieces.empty() && pieces.back().second > mirror.base + mirror.bytes) return false;
    syncRange(mirror.base, mirror.bytes);
    const auto generation = collectPieces(pieces);
    if (generation == 0) markUnwatched(mirror);
    appendSparseBlocks(mirror, std::move(pieces), generation, blocks);
    return true;
}

void planSpaceMirrors(GuestBufferMemory::AddressSpace& space) {
    for (std::size_t index = 0; index < space.base.size(); ++index) {
        const auto& region = space.base[index];
        if (region.mirror == nullptr) continue;
        space.mirrored = true;
        if (!region.mirror->heap) {
            if (region.mirror->writable) space.writableMirrors.push_back(index);
            continue;
        }
        auto* mirror = region.mirror.get();
        if (mirror->sparse) {
            space.sparseMirrors.push_back(mirror);
            continue;
        }
        if (space.heapRuns.empty() || mirror->base != space.heapMirrors.back()->base + space.heapMirrors.back()->bytes) space.heapRuns.push_back({mirror->base, 0, space.heapMirrors.size(), 0});
        space.heapMirrors.push_back(mirror);
        auto& run = space.heapRuns.back();
        run.bytes = mirror->base + mirror->bytes - run.begin;
        run.last = space.heapMirrors.size();
    }
    space.verified = std::make_unique<std::atomic<std::uint64_t>[]>(space.heapRuns.size());
}

std::vector<std::pair<std::size_t, std::uint64_t>> refreshHeapRuns(const GuestBufferMemory::AddressSpace& space, std::vector<RefreshBlock>& blocks) {
    std::vector<std::pair<std::size_t, std::uint64_t>> checked;
    checked.reserve(space.heapRuns.size());
    for (std::size_t index = 0; index < space.heapRuns.size(); ++index) {
        const auto& run = space.heapRuns[index];
        prepareRange(run.begin, run.bytes);
        const auto generation = GuestMemory::CollectWrites(run.begin, static_cast<std::size_t>(run.bytes));
        const auto since = space.verified[index].load(std::memory_order_relaxed);
        if (since == 0 || !GuestMemory::UnchangedSince(run.begin, static_cast<std::size_t>(run.bytes), since)) appendHeapRun(std::span<ImageMirror* const>(space.heapMirrors).subspan(run.first, run.last - run.first), generation, blocks);
        checked.emplace_back(index, generation);
    }
    return checked;
}

bool heapMirrorWatched(ImageMirror& mirror) {
    const auto serial = GuestMemory::UnwatchSerial();
    if (mirror.watchedSerial.load(std::memory_order_relaxed) == serial) return true;
    if (!GuestMemory::Watched(mirror.base, static_cast<std::size_t>(mirror.bytes))) return false;
    mirror.watchedSerial.store(serial, std::memory_order_relaxed);
    return true;
}

bool importsNow(const Context& context, HostImports& state, const GuestAllocations::Lease& lease, std::uint64_t address) {
    if (context.hostImportAlignment == 0) return false;
    if (importsStale(context, state)) return true;
    const MirrorsYield yield(state, state.useCounter + 1);
    return importAllocation(context, state, address, lease, ImportRequester::Build, 0, true) != nullptr;
}

const char* heapCopyReason(bool overBudget, bool watched) {
    if (overBudget) return watched ? "import over the host import budget, heap mirror refused" : "import over the host import budget, heap not fully write-watched";
    return watched ? "heap mirror refused" : "heap not fully write-watched";
}

// The mirror for a leased image range: the existing one, or a new one when none exists or the
// registered Range object changed (a protection change). Null when the range cannot be mirrored. An
// existing writable mirror's blocks are appended to `blocks` for the caller's one compare of every
std::shared_ptr<ImageMirror> acquireMirror(const Context& context, const std::shared_ptr<const GuestAllocations::Range>& range, std::vector<RefreshBlock>& blocks, bool heap, std::uint64_t begin = 0, std::uint64_t bytes = 0, bool joinedHeap = false) {
    auto selected = *range;
    if (bytes != 0) {
        Require(begin >= range->address && bytes <= range->bytes && begin - range->address <= range->bytes - bytes, "heap mirror selection exceeds its registered range");
        selected.address = begin;
        selected.bytes = static_cast<std::size_t>(bytes);
    }
    auto& state = Mirrors();
    std::shared_ptr<ImageMirror> mirror;
    {
        std::lock_guard lock(state.mutex);
        if (state.device != context.device) {
            for (auto& [base, entry] : state.entries) entry->listed = false;
            state.entries.clear();
            state.failed.clear();
            state.heapBytes = 0;
            state.device = context.device;
            ++state.version;
        }
        if (state.failed.contains(selected.address)) return nullptr;
        const auto found = state.entries.find(selected.address);
        if (found != state.entries.end() && sameRange(found->second->range, range) && found->second->reserved == selected.bytes && found->second->writable == range->writable && found->second->heap == heap && found->second->joinedHeap == joinedHeap) mirror = found->second;
    }
    if (mirror != nullptr) {
        if (!mirror->sparse) {
            if (!heap && prepareRefresh(*mirror, selected.address, selected.bytes)) appendBlocks(blocks, *mirror, selected.address, selected.bytes);
            return mirror;
        }
        if (refreshSparseMirror(*mirror, blocks)) return mirror;
    }
    Pieces pieces;
    if (!GuestMemory::Accessible(reinterpret_cast<const void*>(selected.address), selected.bytes, range->writable && !heap)) {
        if (!heap || !sparseMirrorsEnabled()) return nullptr;
        pieces = GuestMemory::DescribeCommitted(selected.address, selected.bytes).ranges;
        if (pieces.empty()) return nullptr;
    }
    const bool sparse = !pieces.empty();
    mirror = std::make_shared<ImageMirror>();
    mirror->base = selected.address;
    mirror->bytes = sparse ? sparseExtent(selected.address, selected.bytes, pieces) : selected.bytes;
    mirror->reserved = selected.bytes;
    mirror->sparse = sparse;
    mirror->writable = range->writable;
    mirror->heap = heap;
    mirror->joinedHeap = joinedHeap;
    mirror->range = range;
    GuestRuns backed;
    if (heap) {
        std::uint64_t generation = 0;
        const auto serial = GuestMemory::UnwatchSerial();
        if (sparse) {
            syncRange(mirror->base, mirror->bytes);
            if (GuestMemory::Watched(mirror->base, static_cast<std::size_t>(mirror->bytes))) generation = collectPieces(pieces);
        } else {
            prepareRefresh(*mirror, selected.address, selected.bytes);
            generation = GuestMemory::CollectWrites(selected.address, selected.bytes);
        }
        constexpr std::uint64_t block = 65536;
        mirror->generations.assign(static_cast<std::size_t>(((mirror->base + mirror->bytes + block - 1) / block) - mirror->base / block), generation);
        if (generation == 0) {
            std::shared_ptr<ImageMirror> stale;
            std::lock_guard lock(state.mutex);
            ++state.heapUnwatched;
            if (const auto found = state.entries.find(selected.address); found != state.entries.end() && found->second->heap) {
                unlistMirror(state, *found->second);
                stale = std::move(found->second);
                state.entries.erase(found);
                ++state.version;
            }
            return nullptr;
        }
        mirror->watchedSerial.store(serial, std::memory_order_relaxed);
        if (context.sparseBinding && context.sparseResidencyBuffer && context.sparseResidencyAliased) backed = GuestNonzeroBlocks(sparse ? clipPieces(pieces, mirror->base, mirror->base + mirror->bytes) : GuestRuns{{mirror->base, mirror->base + mirror->bytes}});
        auto need = blockBytes(backed);
        reserveHeapBytes(selected.address, selected.bytes, need);
        try {
            mirror->resident = ResidentBuffer::Create(context, mirror->base, mirror->bytes, backed);
        } catch (const std::runtime_error& error) {
            std::uint64_t held = 0;
            {
                std::lock_guard lock(state.mutex);
                state.heapBytes -= need;
                held = state.heapBytes;
            }
            throw std::runtime_error(heapMirrorError(selected.address, selected.bytes, held, error.what()));
        }
        if (mirror->resident == nullptr) {
            {
                std::lock_guard lock(state.mutex);
                state.heapBytes -= need;
            }
            need = mirror->bytes;
            reserveHeapBytes(selected.address, selected.bytes, need);
        }
        mirror->accounted = need;
    }
    if (mirror->resident != nullptr) {
        {
            const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::MirrorRefresh);
            for (const auto& [first, last] : backed) {
                for (auto at = first; at < last;) {
                    const auto next = std::min(last, (at / ResidentBuffer::BlockBytes + 1) * ResidentBuffer::BlockBytes);
                    GuestMemory::Read(at, std::span<std::byte>(mirror->resident->Block(at), static_cast<std::size_t>(next - at)));
                    at = next;
                }
                mirror->resident->MarkChanged(first / ResidentBuffer::BlockBytes * ResidentBuffer::BlockBytes, (last + ResidentBuffer::BlockBytes - 1) / ResidentBuffer::BlockBytes * ResidentBuffer::BlockBytes);
            }
        }
        mirror->committed = std::move(pieces);
        std::lock_guard lock(state.mutex);
        ++state.rebuilds;
        mirror->serial = (1ull << 63u) | ++state.serials;
        if (const auto found = state.entries.find(selected.address); found != state.entries.end() && found->second->heap) unlistMirror(state, *found->second);
        mirror->listed = true;
        state.entries[selected.address] = mirror;
        ++state.version;
        return mirror;
    }
    try {
        mirror->buffer = std::make_shared<Buffer>(context, static_cast<std::size_t>(mirror->bytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT);
    } catch (const std::runtime_error& error) {
        if (heap) {
            std::uint64_t held = 0;
            {
                std::lock_guard lock(state.mutex);
                state.heapBytes -= mirror->accounted;
                held = state.heapBytes;
            }
            throw std::runtime_error(heapMirrorError(selected.address, selected.bytes, held, error.what()));
        }
        std::fprintf(stderr, "[gpu] image mirror of 0x%llx+0x%llx failed: %s; falling back to copies\n", static_cast<unsigned long long>(selected.address), static_cast<unsigned long long>(selected.bytes), error.what());
        std::lock_guard lock(state.mutex);
        state.failed.insert(selected.address);
        return nullptr;
    }
    // Read stores GPU results pending in the range first (write-backs of earlier copies of it);
    // named for the [hooksync] attribution like the refresh's flush.
    {
        const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::MirrorRefresh);
        const auto target = mirror->buffer->Bytes().first(static_cast<std::size_t>(mirror->bytes));
        if (!sparse) GuestMemory::Read(selected.address, target);
        std::size_t cursor = 0;
        for (const auto& [first, last] : pieces) {
            const auto at = static_cast<std::size_t>(first - mirror->base);
            std::memset(target.data() + cursor, 0, at - cursor);
            GuestMemory::Read(first, target.subspan(at, static_cast<std::size_t>(last - first)));
            cursor = static_cast<std::size_t>(last - mirror->base);
        }
        if (sparse) std::memset(target.data() + cursor, 0, target.size() - cursor);
        mirror->committed = std::move(pieces);
    }
    if (range->writable && !heap) mirror->shadow.assign(mirror->buffer->Bytes().begin(), mirror->buffer->Bytes().end());
    std::lock_guard lock(state.mutex);
    ++state.rebuilds;
    mirror->serial = (1ull << 63u) | ++state.serials;
    // A replaced mirror lives on in the regions of recorded work that bound it.
    if (const auto found = state.entries.find(selected.address); found != state.entries.end() && found->second->heap) unlistMirror(state, *found->second);
    mirror->listed = heap;
    state.entries[selected.address] = mirror;
    ++state.version;
    return mirror;
}

std::shared_ptr<ImageMirror> findMirror(const Context& context, std::uint64_t begin, std::uint64_t end) {
    auto& state = Mirrors();
    std::lock_guard lock(state.mutex);
    if (state.device != context.device || state.entries.empty()) return nullptr;
    auto found = state.entries.upper_bound(begin);
    if (found == state.entries.begin()) return nullptr;
    --found;
    const auto& mirror = found->second;
    if (mirror->heap || begin < mirror->base || end > mirror->base + mirror->bytes || mirror->range.expired()) return nullptr;
    return mirror;
}

void sweepMirrors(std::uint64_t neededBytes, std::uint64_t protectAddress) {
    const bool released = releaseExpiredJoinedHeaps();
    auto space = Spaces().current.load();
    auto& state = Mirrors();
    std::vector<std::shared_ptr<ImageMirror>> dropped;
    std::lock_guard lock(state.mutex);
    if (released) ++state.version;
    if (neededBytes == 0 && space != nullptr && state.pinnedSpace == space->serial && state.pinnedVersion == state.version) return;
    if (neededBytes != 0 && state.heapBytes + neededBytes > heapMirrorBudget()) {
        if (auto cached = Spaces().current.exchange(nullptr); cached != nullptr) {
            space.reset();
            cached.reset();
        }
    }
    ++state.sweeps;
    bool pinned = space != nullptr;
    std::size_t leased = 0;
    for (auto it = state.entries.begin(); it != state.entries.end();) {
        if (!it->second->range.expired()) {
            if (pinned) {
                while (leased < space->lease.size() && space->lease[leased]->address < it->first) ++leased;
                pinned = leased < space->lease.size() && sameRange(it->second->range, space->lease[leased]);
            }
            ++it;
            continue;
        }
        if (it->second->heap) unlistMirror(state, *it->second);
        dropped.push_back(std::move(it->second));
        it = state.entries.erase(it);
        ++state.version;
    }
    if (neededBytes != 0 && state.heapBytes + neededBytes > heapMirrorBudget()) {
        std::unordered_set<const ImageMirror*> queued;
        if (buildingBlocks != nullptr) {
            for (const auto& block : *buildingBlocks) queued.insert(block.mirror);
        }
        while (state.heapBytes + neededBytes > heapMirrorBudget()) {
            auto victim = state.entries.end();
            for (auto it = state.entries.begin(); it != state.entries.end(); ++it) {
                if (!it->second->heap || it->first == protectAddress || queued.count(it->second.get()) != 0) continue;
                const bool buildingHolds = buildingBufferMemory != nullptr && buildingBufferMemory->HoldsMirror(it->second);
                const auto allowedCount = (buildingHolds && !it->second->writable) ? 2 : 1;
                if (it->second.use_count() == allowedCount) {
                    if (victim == state.entries.end()) {
                        victim = it;
                    } else if (it->second->lastDispatch != victim->second->lastDispatch) {
                        if (it->second->lastDispatch < victim->second->lastDispatch) victim = it;
                    } else if (it->second->writable != victim->second->writable) {
                        if (!it->second->writable && victim->second->writable) victim = it;
                    } else if (it->first < victim->first) {
                        victim = it;
                    }
                }
            }
            if (victim == state.entries.end()) break;
            if (buildingBufferMemory != nullptr) {
                buildingBufferMemory->RecordEvicted(victim->first);
                if (buildingBufferMemory->HoldsMirror(victim->second)) {
                    buildingBufferMemory->EvictMirror(victim->second);
                }
            }
            unlistMirror(state, *victim->second);
            dropped.push_back(std::move(victim->second));
            state.entries.erase(victim);
            ++state.version;
            ++state.evictions;
        }
    }
    state.pinnedSpace = pinned ? space->serial : 0;
    state.pinnedVersion = state.version;
}

void releaseImportedMirrors(const Context& context, std::span<const std::pair<std::uint64_t, std::uint64_t>> imported) {
    if (imported.empty()) return;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> coverage(imported.begin(), imported.end());
    std::sort(coverage.begin(), coverage.end());
    std::size_t count = 0;
    for (const auto& interval : coverage) {
        if (count != 0 && interval.first <= coverage[count - 1].second) coverage[count - 1].second = std::max(coverage[count - 1].second, interval.second);
        else coverage[count++] = interval;
    }
    coverage.resize(count);
    std::vector<std::shared_ptr<ImageMirror>> released;
    auto& state = Mirrors();
    std::lock_guard lock(state.mutex);
    if (state.device != context.device || state.entries.empty()) return;
    for (const auto& [begin, end] : imported) {
        auto it = state.entries.upper_bound(begin);
        if (it != state.entries.begin()) --it;
        while (it != state.entries.end() && it->first < end) {
            const auto& mirror = it->second;
            if (!mirror->heap || mirror->base + mirror->bytes <= begin) {
                ++it;
                continue;
            }
            if (mirror->joinedHeap) {
                const auto covered = std::upper_bound(coverage.begin(), coverage.end(), mirror->base, [](std::uint64_t address, const auto& interval) { return address < interval.first; });
                if (covered == coverage.begin() || std::prev(covered)->second < mirror->base + mirror->reserved) {
                    ++it;
                    continue;
                }
            }
            ++state.heapImported;
            state.heapImportedBytes += mirror->accounted;
            unlistMirror(state, *mirror);
            released.push_back(std::move(it->second));
            it = state.entries.erase(it);
            ++state.version;
        }
    }
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
    std::size_t heaps = 0;
    std::uint64_t bytes = 0;
    std::uint64_t heapBytes = 0;
    {
        std::lock_guard lock(state.mutex);
        for (const auto& [base, mirror] : state.entries) {
            ++count;
            bytes += mirror->bytes;
            if (mirror->writable) ++writable;
            if (mirror->heap) ++heaps;
        }
        heapBytes = state.heapBytes;
    }
    AgcDriver::ProfilePrint_nid_no_patch("[buffers] image mirrors: %zu ranges (%.1f MiB, %zu writable, %zu heap %.1f MiB), %llu address-based builds served, %llu descriptor sub-ranges bound, %llu sparse heaps bound, %llu rebuilds, %llu refreshes: %llu blocks compared, %llu copied, %llu refresh syncs; heap refills %llu, unwatched heaps %llu, %llu heap mirrors evicted for budget, %llu heap mirrors released for imports (%.1f MiB)\n", count, bytes / 1048576.0, writable, heaps, heapBytes / 1048576.0, static_cast<unsigned long long>(state.builds), static_cast<unsigned long long>(state.subranges), static_cast<unsigned long long>(state.sparseBinds), static_cast<unsigned long long>(state.rebuilds), static_cast<unsigned long long>(state.refreshes), static_cast<unsigned long long>(state.blocksCompared), static_cast<unsigned long long>(state.blocksCopied), static_cast<unsigned long long>(state.syncs), static_cast<unsigned long long>(state.heapRefills), static_cast<unsigned long long>(state.heapUnwatched), static_cast<unsigned long long>(state.evictions), static_cast<unsigned long long>(state.heapImported), state.heapImportedBytes / 1048576.0);
#ifdef _WIN32
    auto& imports = Imports();
    std::lock_guard importsLock(imports.mutex);
    if (imports.settlesSubmitted != 0 || imports.settlesWaited != 0 || !imports.settles.empty()) AgcDriver::ProfilePrint_nid_no_patch("[buffers] host imports under %llu KiB: %llu settle reads submitted, %llu back in the write watch, %llu kept compared under a storage image, %llu retired during their read (%llu unmaps waited for it), %zu pending; %llu larger imports settled inside their window\n", static_cast<unsigned long long>(settleFloorBytes >> 10u), static_cast<unsigned long long>(imports.settlesSubmitted), static_cast<unsigned long long>(imports.settlesRewatched), static_cast<unsigned long long>(imports.settlesKeptCompared), static_cast<unsigned long long>(imports.settlesParked), static_cast<unsigned long long>(imports.settleUnmapWaits), imports.settles.size(), static_cast<unsigned long long>(imports.settlesWaited));
#endif
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
    AgcDriver::Mutex mutex;
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
    std::atomic<std::uint64_t> mismatched{0};
};

SnapshotStats& Snapshots() {
    static SnapshotStats stats;
    return stats;
}

bool WaitForLeasesAndImports(std::uintptr_t address, std::size_t bytes) {
    const auto start = std::chrono::steady_clock::now();
    bool synced = false;
    bool drained = false;
    bool retiredImport = false;
    bool droppedAddressSpace = false;
    // A range pinned only by the cached address space is released by dropping the cache's
    // reference: no GPU wait, no device lock (the mutating thread may be a driver thread holding
    // it). The registry rescans; a range still pinned by a build or a batch holding the space comes
    // back here and finishes lease batches below.
    if (auto dropped = Spaces().current.exchange(nullptr); dropped != nullptr) {
        Spaces().droppedByWaiter.store(true, std::memory_order_relaxed);
        Spaces().waiterDrops.fetch_add(1, std::memory_order_relaxed);
        dropped.reset();
        droppedAddressSpace = true;
        auto& imports = Imports();
        const auto end = static_cast<std::uint64_t>(address) + bytes;
        bool overlapsImport = false;
        {
            std::lock_guard importsLock(imports.mutex);
            const auto next = imports.imports.lower_bound(address);
            const auto previous = next == imports.imports.begin() ? imports.imports.end() : std::prev(next);
            overlapsImport = (next != imports.imports.end() && next->first < end) || (previous != imports.imports.end() && previous->first + previous->second.bytes > address);
        }
        if (!overlapsImport) {
            auto& state = Leases();
            std::lock_guard lock(state.mutex);
            ++state.stats.contentionWaits;
            ++state.stats.cacheDrops;
            state.stats.contentionMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            return true;
        }
    }
    if (Recorder::InCompletion()) throw std::runtime_error("guest allocation mutation cannot wait for an active GPU completion");
    if (GuestMemory::GpuMutex().HeldByThisThread()) {
        auto* recorder = Recorder::Active();
        if (recorder != nullptr && !recorder->Idle()) {
            Recorder::CountSync(4);
            recorder->Sync();
            synced = true;
            drained = true;
        }
        if (recorder != nullptr) recorder->FlushDeferredReleases();
        const auto end = static_cast<std::uint64_t>(address) + bytes;
        auto& imports = Imports();
        if (releaseParkedImports(imports, address, end)) retiredImport = true;
        const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
        {
            std::lock_guard importsLock(imports.mutex);
            for (auto it = imports.imports.begin(); it != imports.imports.end();) {
                const auto base = it->first;
                if (base >= end || address >= base + it->second.bytes) {
                    ++it;
                    continue;
                }
                const auto next = std::next(it);
                retireImport(imports.device, imports, it, lease, false);
                it = next;
                retiredImport = true;
            }
        }
#ifdef _WIN32
        if (releaseSettleRetired(imports, address, end)) retiredImport = true;
#endif
        if (retiredImport && recorder != nullptr && !recorder->Idle()) {
            Recorder::CountSync(4);
            recorder->Sync();
            synced = true;
            drained = true;
        }
        if (retiredImport && recorder != nullptr) recorder->FlushDeferredReleases();
    } else {
        std::lock_guard lock(GuestMemory::GpuMutex());
        auto& state = Leases();
        auto* recorder = Recorder::Active();
        const auto target = state.newestLeaseSerial;
        if (recorder != nullptr && recorder == state.leaseRecorder && target > state.finishedLeaseSerial) {
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
            std::this_thread::yield();
        }
        if (recorder != nullptr) recorder->FlushDeferredReleases();

        const auto end = static_cast<std::uint64_t>(address) + bytes;
        auto& imports = Imports();
        if (releaseParkedImports(imports, address, end)) retiredImport = true;
        bool overlapsImport = false;
        {
            std::lock_guard importsLock(imports.mutex);
            for (const auto& [base, entry] : imports.imports) {
                if (base < end && address < base + entry.bytes) {
                    overlapsImport = true;
                    break;
                }
            }
        }
        if (overlapsImport) {
            if (recorder != nullptr && !recorder->Idle()) {
                Recorder::CountSync(4);
                recorder->Sync();
                synced = true;
                drained = true;
            }
            const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
            {
                std::lock_guard importsLock(imports.mutex);
                for (auto it = imports.imports.begin(); it != imports.imports.end();) {
                    const auto base = it->first;
                    if (base >= end || address >= base + it->second.bytes) {
                        ++it;
                        continue;
                    }
                    const auto next = std::next(it);
                    retireImport(imports.device, imports, it, lease, false);
                    it = next;
                    retiredImport = true;
                }
            }
            if (retiredImport && recorder != nullptr && !recorder->Idle()) {
                Recorder::CountSync(4);
                recorder->Sync();
                synced = true;
                drained = true;
            }
        }
#ifdef _WIN32
        if (releaseSettleRetired(imports, address, end)) retiredImport = true;
#endif
    }
    auto& state = Leases();
    std::lock_guard lock(state.mutex);
    ++state.stats.contentionWaits;
    if (droppedAddressSpace) ++state.stats.cacheDrops;
    if (synced) ++state.stats.contentionSyncs;
    if (drained) ++state.stats.contentionDrains;
    state.stats.contentionMs += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    return synced || retiredImport;
}

void ensurePinWaiter() {
    static const bool registered = [] {
        GuestAllocations::GuestAllocationsSetPinWaiter_nid_postfix(&WaitForLeasesAndImports);
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
    AgcDriver::ProfilePrint_nid_no_patch("[address-sync] leases: %llu released at completion, %llu synced at once; %llu pin-contention waits by guest threads (%llu finished the lease batch, %llu drained the recorder, %llu cache-only drops) %.1f s; BDA table cache %llu hits / %llu misses (%zu tables held); snapshot compares: %llu skipped (live-backed region), %llu made\n", static_cast<unsigned long long>(stats.deferred), static_cast<unsigned long long>(stats.synced), static_cast<unsigned long long>(stats.contentionWaits), static_cast<unsigned long long>(stats.contentionSyncs), static_cast<unsigned long long>(stats.contentionDrains), static_cast<unsigned long long>(stats.cacheDrops), stats.contentionMs / 1000, static_cast<unsigned long long>(table.hits), static_cast<unsigned long long>(table.misses), table.held, static_cast<unsigned long long>(snapshots.skipped.load(std::memory_order_relaxed)), static_cast<unsigned long long>(snapshots.checked.load(std::memory_order_relaxed)));
}

void FinishLeaseWork() {
    GuestMemory::AssertGpuLockHeld("FinishLeaseWork");
    const auto& state = Leases();
    auto* recorder = Recorder::Active();
    const auto target = state.newestLeaseSerial;
    if (recorder == nullptr || recorder != state.leaseRecorder || target <= recorder->CompletedThrough()) return;
    if (target >= recorder->OpenSerial()) recorder->Submit();
    Recorder::AnnounceSyncSite(__builtin_return_address(0));
    recorder->FinishUpTo(target);
}

LeaseStats LeaseCounters() {
    auto& state = Leases();
    std::lock_guard lock(state.mutex);
    return state.stats;
}

AddressSpaceStats AddressSpaceCounters() {
    const auto& cache = Spaces();
    const auto load = [](const std::atomic<std::uint64_t>& counter) { return counter.load(std::memory_order_relaxed); };
    return {addressSpaceCacheEnabled(), load(cache.hits), load(cache.rebuiltFirst), load(cache.rebuiltGeneration), load(cache.rebuiltEpoch), load(cache.rebuiltDevice), load(cache.rebuiltWaiterDrop), load(cache.unpublished), load(cache.dissolvedOverlap), load(cache.dissolvedImports), load(cache.waiterDrops)};
}

std::string AddressCopyOverflow(std::vector<AddressCopy> copies, std::uint64_t limit) {
    std::uint64_t total = 0;
    for (const auto& copy : copies) total += copy.committed;
    if (total <= limit) return {};
    std::sort(copies.begin(), copies.end(), [](const AddressCopy& left, const AddressCopy& right) { return left.committed > right.committed; });
    char text[256];
    std::snprintf(text, sizeof(text), "an address-based build copies %llu MiB of %zu registered ranges, past %llu MiB (APS5_ADDRESS_COPY_MAX_MIB); the largest:", static_cast<unsigned long long>(total >> 20u), copies.size(), static_cast<unsigned long long>(limit >> 20u));
    std::string message = text;
    for (std::size_t index = 0; index < copies.size() && index < 8; ++index) {
        const auto& copy = copies[index];
        const char* reason = copy.heap ? heapCopyReason(copy.overBudget, GuestMemory::Watched(copy.begin, copy.end - copy.begin)) : copy.reason;
        std::snprintf(text, sizeof(text), " 0x%llx+0x%llx (%.1f MiB committed, %s)", static_cast<unsigned long long>(copy.begin), static_cast<unsigned long long>(copy.end - copy.begin), copy.committed / 1048576.0, reason);
        message += text;
    }
    return message;
}

MirrorStats MirrorCounters() {
    auto& state = Mirrors();
    std::lock_guard lock(state.mutex);
    MirrorStats stats{0, state.heapBytes, state.rebuilds, state.blocksCopied, state.heapRefills, state.sweeps, state.heapChecks, state.sparseBinds, state.evictions};
    for (const auto& [base, mirror] : state.entries) stats.heapMirrors += mirror->heap ? 1 : 0;
    return stats;
}

std::uint64_t SnapshotMismatches() {
    return Snapshots().mismatched.load(std::memory_order_relaxed);
}

std::shared_ptr<void> HoldHeapMirror(std::uint64_t address) {
    auto& state = Mirrors();
    std::lock_guard lock(state.mutex);
    auto it = state.entries.find(address);
    if (it != state.entries.end()) return it->second;
    return nullptr;
}

HeapMirrorHold HeapMirrorsWithin(std::uint64_t begin, std::uint64_t end) {
    auto& state = Mirrors();
    std::lock_guard lock(state.mutex);
    HeapMirrorHold hold;
    for (auto it = state.entries.lower_bound(begin); it != state.entries.end() && it->first < end; ++it) {
        if (!it->second->heap || it->second->range.expired()) continue;
        ++hold.mirrors;
        if (it->second->listed) hold.bytes += it->second->accounted;
    }
    return hold;
}

std::vector<std::uint64_t> AddressTableKey(std::span<const std::pair<std::uint64_t, std::uint64_t>> writes, std::span<const ShaderRecompiler::BdaAbi::Range> own) {
    std::vector<std::uint64_t> key;
    key.reserve(writes.size() * 2u + 1u + own.size() * 4u);
    for (const auto& [begin, end] : writes) key.insert(key.end(), {begin, end});
    key.push_back(std::numeric_limits<std::uint64_t>::max());
    for (const auto& range : own) key.insert(key.end(), {range.begin, range.end, range.deviceAddress, (static_cast<std::uint64_t>(range.reserved) << 32u) | range.permissions});
    return key;
}

void ClearSnapshotCopies(VkDevice device) {
    auto& copies = SnapshotCopyCache();
    std::lock_guard lock(copies.mutex);
    if (copies.device != device) return;
    copies.Clear();
    copies.device = VK_NULL_HANDLE;
}

void ClearImageMirrors(VkDevice device) {
    auto space = Spaces().current.load();
    if (space != nullptr && space->device == device) {
        Spaces().current.compare_exchange_strong(space, std::shared_ptr<const GuestBufferMemory::AddressSpace>{});
    }
    auto& state = Mirrors();
    std::map<std::uint64_t, std::shared_ptr<ImageMirror>> entries;
    {
        std::lock_guard lock(state.mutex);
        if (state.device != device) return;
        entries.swap(state.entries);
        for (auto& [base, entry] : entries) entry->listed = false;
        state.failed.clear();
        state.heapBytes = 0;
        state.device = VK_NULL_HANDLE;
    }
}

void ClearHostImports(VkDevice device) {
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    state.parked.erase(device);
    state.importedAtMap.clear();
#ifdef _WIN32
    if (device == state.device) drainSettles(state);
#endif
    if (const auto found = state.settles.find(device); found != state.settles.end()) {
#ifdef _WIN32
        releaseSettleBatches(device, found->second);
        std::erase_if(state.pendingSettles, [device](const auto& settle) { return settle.second.phase == SettlePhase::Submitted && settle.second.device == device; });
#endif
        const auto& settle = found->second;
        if (settle.settleFence != VK_NULL_HANDLE) settle.destroyFence(device, settle.settleFence, nullptr);
        if (settle.settleTarget != VK_NULL_HANDLE) settle.settleDestroyBuffer(device, settle.settleTarget, nullptr);
        if (settle.settleMemory != VK_NULL_HANDLE) MemoryAccounting::Free(settle.settleFreeMemory, device, settle.settleMemory);
        state.settles.erase(found);
    }
    if (state.device != device) return;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> unwatched;
    GuestAllocations::Lease lease;
    if (std::any_of(state.imports.begin(), state.imports.end(), [](const auto& import) { return import.second.unwatched; })) {
        lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
        for (const auto& [address, entry] : state.imports) noteRetiredUnwatched(unwatched, entry, lease);
    }
    for (const auto& [address, entry] : state.imports) {
        state.destroyBuffer(state.device, entry.buffer, nullptr);
        MemoryAccounting::Free(state.freeMemory, state.device, entry.memory);
#ifdef _WIN32
        GuestArena::GuestArenaUnmapAlias_nid_postfix(entry.alias);
#endif
    }
    state.imports.clear();
    rewatchRetired(state, std::move(unwatched));
    state.pendingImports.clear();
    state.liveBytes = 0;
    state.failed.clear();
    state.failedJoins.clear();
    state.failedBlocks.clear();
    state.refusals.clear();
    state.evictingRefusals.clear();
    state.gateRefusals.clear();
    state.ceiling = 0;
    state.allocationFailures = 0;
    state.allocationFailed = false;
    state.reportNoted = 0;
    state.device = VK_NULL_HANDLE;
    state.refreshedGeneration = 0;
    ++state.epoch;
}

namespace {

void runImportProbe(const Context& context, std::uint64_t base, std::uint64_t bytes, const std::function<const char*(HostImport&, VkResult&)>& importStep, const std::function<bool(std::uint32_t&)>& collect, ImportProbe& probe, const std::function<const char*(HostImport&, VkResult&)>& settleStep = {}) {
    HostImport import{base, bytes, VK_NULL_HANDLE, VK_NULL_HANDLE, 0};
    VkBuffer destination = VK_NULL_HANDLE;
    VkDeviceMemory destinationMemory = VK_NULL_HANDLE;
    VkCommandBuffer commands[2]{};
    VkFence fence = VK_NULL_HANDLE;
    bool submitted = false;
    const auto run = [&]() -> const char* {
        std::uint32_t quiet = 0;
        if (!collect(quiet)) return "the first collect";
        if (!collect(quiet)) return "the second collect";
        if (quiet != 0) return "an unwritten scratch range";
        if (const char* step = importStep(import, probe.result)) return step;
        if (!collect(probe.writtenAtImport)) return "the collect after the import";
        if (settleStep) {
            if (const char* step = settleStep(import, probe.result)) return step;
            if (!collect(probe.writtenDuringSettle)) return "the collect after the settle";
            if (!collect(probe.writtenAfterSettle)) return "the collect after a settled import";
        }
        VkBufferCreateInfo info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        info.size = bytes;
        info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if ((probe.result = context.Function<PFN_vkCreateBuffer>("vkCreateBuffer")(context.device, &info, nullptr, &destination)) != VK_SUCCESS) return "vkCreateBuffer";
        VkMemoryRequirements requirements{};
        context.Function<PFN_vkGetBufferMemoryRequirements>("vkGetBufferMemoryRequirements")(context.device, destination, &requirements);
        if (requirements.memoryTypeBits == 0) return "the destination memory type";
        VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        allocation.allocationSize = requirements.size;
        allocation.memoryTypeIndex = static_cast<std::uint32_t>(std::countr_zero(requirements.memoryTypeBits));
        if ((probe.result = MemoryAccounting::Allocate(context, allocation, "host import probe", &destinationMemory)) != VK_SUCCESS) return "vkAllocateMemory";
        if ((probe.result = context.Function<PFN_vkBindBufferMemory>("vkBindBufferMemory")(context.device, destination, destinationMemory, 0)) != VK_SUCCESS) return "vkBindBufferMemory";
        VkCommandBufferAllocateInfo allocate{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        allocate.commandPool = context.pool;
        allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocate.commandBufferCount = 2;
        if ((probe.result = context.Function<PFN_vkAllocateCommandBuffers>("vkAllocateCommandBuffers")(context.device, &allocate, commands)) != VK_SUCCESS) return "vkAllocateCommandBuffers";
        VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        if ((probe.result = context.Function<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(commands[0], &begin)) != VK_SUCCESS) return "vkBeginCommandBuffer";
        const VkBufferCopy region{0, 0, bytes};
        context.Function<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer")(commands[0], import.buffer, destination, 1, &region);
        if ((probe.result = context.Function<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(commands[0])) != VK_SUCCESS) return "vkEndCommandBuffer";
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        if ((probe.result = context.Function<PFN_vkBeginCommandBuffer>("vkBeginCommandBuffer")(commands[1], &begin)) != VK_SUCCESS) return "vkBeginCommandBuffer";
        context.Function<PFN_vkCmdFillBuffer>("vkCmdFillBuffer")(commands[1], import.buffer, 0, bytes, 0x02020202u);
        if ((probe.result = context.Function<PFN_vkEndCommandBuffer>("vkEndCommandBuffer")(commands[1])) != VK_SUCCESS) return "vkEndCommandBuffer";
        const VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        if ((probe.result = context.Function<PFN_vkCreateFence>("vkCreateFence")(context.device, &fenceInfo, nullptr, &fence)) != VK_SUCCESS) return "vkCreateFence";
        const auto execute = [&](VkCommandBuffer buffer) -> const char* {
            if ((probe.result = context.Function<PFN_vkResetFences>("vkResetFences")(context.device, 1, &fence)) != VK_SUCCESS) return "vkResetFences";
            VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &buffer;
            if ((probe.result = context.Function<PFN_vkQueueSubmit>("vkQueueSubmit")(context.queue, 1, &submit, fence)) != VK_SUCCESS) return "vkQueueSubmit";
            submitted = true;
            if ((probe.result = context.Function<PFN_vkWaitForFences>("vkWaitForFences")(context.device, 1, &fence, VK_TRUE, 10'000'000'000ull)) != VK_SUCCESS) return "vkWaitForFences";
            submitted = false;
            return nullptr;
        };
        if (const char* step = execute(commands[0])) return step;
        if (!collect(probe.writtenAfterSubmit)) return "the collect after the submission";
        if (const char* step = execute(commands[0])) return step;
        if (!collect(probe.writtenAfterSecondSubmit)) return "the collect after the second submission";
        if (const char* step = execute(commands[1])) return step;
        if (!collect(probe.writtenAfterFill)) return "the collect after the fill";
        *reinterpret_cast<volatile std::uint8_t*>(base) = 2;
        if (!collect(probe.writtenByCpu)) return "the collect after a CPU store";
        return nullptr;
    };
    probe.failure = run();
    if (submitted) context.Function<PFN_vkQueueWaitIdle>("vkQueueWaitIdle")(context.queue);
    if (fence != VK_NULL_HANDLE) context.Function<PFN_vkDestroyFence>("vkDestroyFence")(context.device, fence, nullptr);
    if (commands[0] != VK_NULL_HANDLE) context.Function<PFN_vkFreeCommandBuffers>("vkFreeCommandBuffers")(context.device, context.pool, 2, commands);
    if (destination != VK_NULL_HANDLE) context.Function<PFN_vkDestroyBuffer>("vkDestroyBuffer")(context.device, destination, nullptr);
    if (destinationMemory != VK_NULL_HANDLE) MemoryAccounting::Free(context, destinationMemory);
    if (import.buffer != VK_NULL_HANDLE) destroyImport(context, import);
}

#ifndef _WIN32
bool collectWriteWatch(std::uint64_t base, std::uint64_t bytes, std::uint32_t& pages) {
    pages = 0;
    return GuestWriteWatch::GuestWriteWatchCollect_nid_postfix(static_cast<std::uintptr_t>(base), static_cast<std::size_t>(bytes), [](void* count, std::uintptr_t begin, std::uintptr_t end) { *static_cast<std::uint32_t*>(count) += static_cast<std::uint32_t>((end - begin) / 4096); }, &pages);
}
#endif

}

ImportProbe ProbeImportWriteProtection(const Context& context) {
    ImportProbe probe;
#ifdef _WIN32
    if (context.hostImportAlignment == 0) {
        probe.failure = "host import support";
        return probe;
    }
    constexpr std::uint64_t page = 4096;
    const std::uint64_t alignment = std::max<std::uint64_t>(context.hostImportAlignment, page);
    const std::uint64_t bytes = (4096 * page + alignment - 1) / alignment * alignment;
    probe.pages = static_cast<std::uint32_t>(bytes / page);
    void* raw = VirtualAlloc(nullptr, static_cast<SIZE_T>(bytes + alignment), MEM_RESERVE | MEM_COMMIT | MEM_WRITE_WATCH, PAGE_READWRITE);
    if (raw == nullptr) {
        probe.failure = "VirtualAlloc";
        return probe;
    }
    const auto base = (reinterpret_cast<std::uint64_t>(raw) + alignment - 1) / alignment * alignment;
    auto* scratch = reinterpret_cast<volatile std::uint8_t*>(base);
    for (std::uint64_t offset = 0; offset < bytes; offset += page) scratch[offset] = 1;
    std::vector<void*> written(static_cast<std::size_t>(bytes / page));
    const auto collect = [&](std::uint32_t& pages) {
        ULONG_PTR count = written.size();
        DWORD granularity = 0;
        if (GetWriteWatch(WRITE_WATCH_FLAG_RESET, reinterpret_cast<void*>(base), static_cast<SIZE_T>(bytes), written.data(), &count, &granularity) != 0) return false;
        pages = static_cast<std::uint32_t>(count);
        return true;
    };
    std::uint32_t touched = 0;
    if (!collect(touched)) probe.failure = "GetWriteWatch";
    else if (context.settleQueue == VK_NULL_HANDLE || context.settleQueueMutex == nullptr) probe.failure = "the settle queue";
    else runImportProbe(context, base, bytes, [&](HostImport& import, VkResult& result) { return createImport(context, import, result); }, collect, probe, [&](HostImport& import, VkResult& result) { return settleImport(context, Imports(), import, result); });
    VirtualFree(raw, 0, MEM_RELEASE);
    return probe;
#else
    if (context.hostImportAlignment == 0) {
        probe.failure = "host import support";
        return probe;
    }
    if (!GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) {
        probe.failure = "the write watch";
        return probe;
    }
    constexpr std::uint64_t page = 4096;
    const std::uint64_t alignment = std::max<std::uint64_t>(context.hostImportAlignment, page);
    const std::uint64_t bytes = (4 * page + alignment - 1) / alignment * alignment;
    probe.pages = static_cast<std::uint32_t>(bytes / page);
    void* raw = mmap(nullptr, bytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (raw == MAP_FAILED) {
        probe.failure = "mmap";
        return probe;
    }
    const auto base = (reinterpret_cast<std::uint64_t>(raw) + alignment - 1) / alignment * alignment;
    auto* scratch = reinterpret_cast<volatile std::uint8_t*>(base);
    for (std::uint64_t offset = 0; offset < bytes; offset += page) scratch[offset] = 1;
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(reinterpret_cast<const void*>(base), bytes);
    runImportProbe(context, base, bytes, [&](HostImport& import, VkResult& result) { return createImport(context, import, result); }, [&](std::uint32_t& pages) { return collectWriteWatch(base, bytes, pages); }, probe);
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(reinterpret_cast<const void*>(base), bytes);
    munmap(raw, bytes + alignment);
    return probe;
#endif
}

ImportProbe ProbeDmaBufImportWriteProtection(const Context& context) {
    ImportProbe probe;
#ifdef _WIN32
    static_cast<void>(context);
    probe.failure = "the Linux write watch";
    return probe;
#else
    if (!context.dmaBufImport) {
        probe.failure = "dma-buf import support";
        return probe;
    }
    if (!GuestWriteWatch::GuestWriteWatchAvailable_nid_postfix()) {
        probe.failure = "the write watch";
        return probe;
    }
    constexpr std::uint64_t page = 4096;
    const std::uint64_t alignment = std::max<std::uint64_t>(context.hostImportAlignment, page);
    const std::uint64_t bytes = (4 * page + alignment - 1) / alignment * alignment;
    probe.pages = static_cast<std::uint32_t>(bytes / page);
    const int file = memfd_create("aps5-dma-buf-import-probe", MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (file < 0) {
        probe.failure = "memfd_create";
        return probe;
    }
    void* mapped = MAP_FAILED;
    if (ftruncate(file, static_cast<off_t>(bytes)) == 0 && fcntl(file, F_ADD_SEALS, F_SEAL_SHRINK) == 0) mapped = mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, file, 0);
    if (mapped == MAP_FAILED) {
        close(file);
        probe.failure = "the shared scratch mapping";
        return probe;
    }
    const auto base = reinterpret_cast<std::uint64_t>(mapped);
    auto* scratch = reinterpret_cast<volatile std::uint8_t*>(base);
    for (std::uint64_t offset = 0; offset < bytes; offset += page) scratch[offset] = 1;
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(reinterpret_cast<const void*>(base), bytes);
    runImportProbe(context, base, bytes, [&](HostImport& import, VkResult& result) -> const char* {
        const int copy = dup(file);
        if (copy < 0) {
            result = VK_ERROR_INITIALIZATION_FAILED;
            return "dup";
        }
        return createDmaBufImport(context, import, copy, 0, result);
    }, [&](std::uint32_t& pages) { return collectWriteWatch(base, bytes, pages); }, probe);
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(reinterpret_cast<const void*>(base), bytes);
    munmap(mapped, bytes);
    close(file);
    return probe;
#endif
}

ImportWatch PrepareImportWatch(const Context& context) {
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    decideImportWatch(context, state);
    return state.unwatchImports ? ImportWatch::Unwatch : ImportWatch::Watch;
}

void SetImportWatch(const Context& context, ImportWatch watch) {
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    state.watchDevice = context.device;
    state.unwatchImports = watch == ImportWatch::Unwatch;
    state.unwatchDmaBufImports = state.unwatchImports;
    state.gateRefusals.clear();
}

void SetHostImportWindow(std::uint64_t bytes) {
    importWindow.store(bytes, std::memory_order_relaxed);
}

void SetHostImportRangeWindow(std::uint64_t bytes) {
    rangeImportWindow.store(bytes, std::memory_order_relaxed);
}

void SetHostImportMaxBytes(std::uint64_t bytes) {
    importMaxBytes.store(bytes, std::memory_order_relaxed);
}

void NoteHostImportFrame() {
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    state.previousFrameFloor = state.frameFloor;
    state.frameFloor = state.useCounter + 1;
    ++state.framesEnded;
}

std::uint64_t HostImportEpoch() {
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    return state.epoch;
}

std::uint64_t HostImportFramesEnded() {
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    return state.framesEnded;
}

void SetHostImportBudget(std::uint64_t bytes) {
    budgetOverride.store(bytes, std::memory_order_relaxed);
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    state.ceiling = 0;
    state.allocationFailures = 0;
    state.allocationFailed = false;
    state.reportNoted = 0;
}

std::uint64_t HostImportBudgetFor(const VkPhysicalDeviceMemoryProperties& memory, const VkPhysicalDeviceMemoryBudgetPropertiesEXT* reported, std::uint64_t configured, std::uint64_t liveBytes) {
    const auto heap = hostHeapIndex(memory);
    if (reported == nullptr || !heap || reported->heapBudget[*heap] == 0) return configured;
    const auto granted = reported->heapBudget[*heap];
    const auto others = reported->heapUsage[*heap] - std::min<std::uint64_t>(reported->heapUsage[*heap], liveBytes);
    const auto held = others + granted / 8u;
    constexpr std::uint64_t step = 64ull << 20u;
    return std::min(configured, granted > held ? (granted - held) / step * step : 0u);
}

std::uint64_t HostImportBudgetWith(std::uint64_t configured, bool set, std::uint64_t reported, bool allocationFailed, std::uint64_t ceiling) {
    const auto budget = set || !allocationFailed ? configured : std::max<std::uint64_t>(std::min(configured, reported), 1);
    return ceiling != 0 ? std::min(budget, ceiling) : budget;
}

void SetAddressCopyLimit(std::uint64_t bytes) {
    addressCopyLimitOverride.store(bytes, std::memory_order_relaxed);
}

AddressCopyCounters& AddressCopyCounters::ThisThread() {
    auto& lease = HostThreadLocal<AddressCopyLease, AddressCopyLease>();
    if (lease.counters == nullptr) {
        auto& registry = addressCopyRegistry();
        std::lock_guard lock(registry.mutex);
        if (registry.unused.empty()) {
            lease.counters = &registry.counters.emplace_back();
        } else {
            lease.counters = registry.unused.back();
            registry.unused.pop_back();
        }
    }
    return *lease.counters;
}

void AddressCopyCounters::Count(std::uint64_t copied) {
    bytes.store(bytes.load(std::memory_order_relaxed) + copied, std::memory_order_relaxed);
    const auto epoch = addressCopyEpoch.load(std::memory_order_relaxed) & AddressCopyEpochMask;
    const auto kept = largest.load(std::memory_order_relaxed);
    const auto clamped = std::min(copied, AddressCopyLargestMask);
    if ((kept >> AddressCopyEpochShift) != epoch || (kept & AddressCopyLargestMask) < clamped) largest.store(epoch << AddressCopyEpochShift | clamped, std::memory_order_relaxed);
}

AddressCopyCounters::Totals AddressCopyCounters::Fold() {
    auto& registry = addressCopyRegistry();
    std::lock_guard lock(registry.mutex);
    const auto epoch = addressCopyEpoch.load(std::memory_order_relaxed);
    Totals totals;
    for (const auto& counters : registry.counters) {
        totals.bytes += counters.bytes.load(std::memory_order_relaxed);
        const auto kept = counters.largest.load(std::memory_order_relaxed);
        if ((kept >> AddressCopyEpochShift) == (epoch & AddressCopyEpochMask)) totals.largest = std::max(totals.largest, kept & AddressCopyLargestMask);
    }
    addressCopyEpoch.store(epoch + 1, std::memory_order_relaxed);
    return totals;
}

std::uint64_t AddressCopyCounters::Limit() {
    return addressCopyLimit();
}

void SetHostImportDeferredSettles(bool enabled) {
#ifdef _WIN32
    deferredSettles.store(enabled, std::memory_order_relaxed);
#else
    static_cast<void>(enabled);
#endif
}

std::uint64_t HostImportBudget(const VkPhysicalDeviceMemoryProperties& memory, const std::uint64_t physicalBytes) {
    std::uint64_t hostHeap = 0;
    for (std::uint32_t index = 0; index < std::min<std::uint32_t>(memory.memoryHeapCount, VK_MAX_MEMORY_HEAPS); ++index) {
        if ((memory.memoryHeaps[index].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) == 0) hostHeap = std::max<std::uint64_t>(hostHeap, memory.memoryHeaps[index].size);
    }
    const auto fromMemory = std::max<std::uint64_t>(6144ull << 20u, physicalBytes / 2);
    const auto reserve = std::max<std::uint64_t>(2048ull << 20u, hostHeap / 2);
    return hostHeap > reserve ? std::min(fromMemory, hostHeap - reserve) : fromMemory;
}

std::uint64_t HostImportLiveBytes() {
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    return state.liveBytes;
}

std::uint64_t HostImportBudgetRemaining(const Context& context) {
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    const auto budget = effectiveBudget(context, state);
    return budget > state.liveBytes ? budget - state.liveBytes : 0;
}

const char* ImportRequesterName(ImportRequester requester) {
    switch (requester) {
        case ImportRequester::Map: return "map";
        case ImportRequester::Build: return "build";
        case ImportRequester::Access: return "access";
        case ImportRequester::Join: return "join";
        case ImportRequester::Segment: return "segment";
        case ImportRequester::ColorTarget: return "color target";
    }
    return "unknown";
}

HostImportCounts HostImportCounters() {
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    return {state.imports.size(), state.liveBytes, state.counts};
}

std::string FormatHostImportCounts(const HostImportCounts& counts) {
    HostImportRequesterCounts total;
    std::string requesters;
    for (std::size_t index = 0; index < counts.requesters.size(); ++index) {
        const auto& requested = counts.requesters[index];
        total.made += requested.made;
        total.madeBytes += requested.madeBytes;
        total.refused += requested.refused;
        total.refusedBytes += requested.refusedBytes;
        total.failed += requested.failed;
        total.remembered += requested.remembered;
        char part[96];
        std::snprintf(part, sizeof(part), "%s%s %llu/%llu/%llu", index == 0 ? "" : ", ", ImportRequesterName(static_cast<ImportRequester>(index)), static_cast<unsigned long long>(requested.made), static_cast<unsigned long long>(requested.refused), static_cast<unsigned long long>(requested.failed));
        requesters += part;
    }
    char text[256];
    std::snprintf(text, sizeof(text), "%llu live (%.1f MiB); made %llu (%.1f MiB), refused %llu (%.1f MiB, %llu remembered), failed %llu; by requester (made/refused/failed): ", static_cast<unsigned long long>(counts.live), counts.liveBytes / 1048576.0, static_cast<unsigned long long>(total.made), total.madeBytes / 1048576.0, static_cast<unsigned long long>(total.refused), total.refusedBytes / 1048576.0, static_cast<unsigned long long>(total.remembered), static_cast<unsigned long long>(total.failed));
    return text + requesters;
}

void ReportHostImports() {
    const auto counts = HostImportCounters();
    if (std::all_of(counts.requesters.begin(), counts.requesters.end(), [](const HostImportRequesterCounts& requested) { return requested.made == 0 && requested.refused == 0 && requested.failed == 0; })) return;
    std::fprintf(stderr, "[gpu] host imports at shutdown: %s\n", FormatHostImportCounts(counts).c_str());
}

HostImportSettleCounts HostImportSettles() {
#ifdef _WIN32
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    return {state.settlesSubmitted, state.settlesWaited, state.settlesRewatched, state.settlesParked, state.settleUnmapWaits, state.settlesKeptCompared};
#else
    return {};
#endif
}

std::uint64_t HostImportsSettled() {
    return settledImports.load(std::memory_order_relaxed);
}

const HostImport* HostImportFor(const Context& context, std::uint64_t address, std::size_t bytes, ImportRequester requester) {
    if (context.hostImportAlignment == 0 || bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address) return nullptr;
    ensurePinWaiter();
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
#ifdef _WIN32
    settleUnwatchedImports(context, state);
#endif
    // A hit is only valid while the registry has not changed since the imports were reconciled.
    if (!importsStale(context, state)) {
        if (recentImportsEnabled()) {
            if (const auto* entry = recentImport(state, address, address + bytes)) {
                entry->lastUse = ++state.useCounter;
                return entry;
            }
        }
        if (const auto* entry = findImport(state, address, address + bytes)) {
            entry->lastUse = ++state.useCounter;
            if (recentImportsEnabled()) rememberImport(state, entry);
            return entry;
        }
    }
    const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
    refreshImports(context, state, lease);
    return importAccess(context, state, address, address + bytes, lease, requester);
}

const HostImport* ExistingHostImportFor(const Context& context, std::uint64_t address, std::size_t bytes) {
    if (context.hostImportAlignment == 0 || bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address) return nullptr;
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    if (importsStale(context, state)) return nullptr;
    return findImport(state, address, address + bytes);
}

bool WatchedAfterSettle(const Context& context, std::uint64_t address, std::size_t bytes) {
#ifdef _WIN32
    if (context.hostImportAlignment != 0 && bytes != 0 && bytes <= std::numeric_limits<std::uint64_t>::max() - address && GuestMemory::GpuMutex().HeldByThisThread()) {
        ensurePinWaiter();
        auto& state = Imports();
        std::lock_guard lock(state.mutex);
        if (importsStale(context, state)) {
            const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
            refreshImports(context, state, lease);
        }
        settleImportsNow(context, state, address, address + bytes);
    }
#endif
    return GuestMemory::Watched(address, bytes);
}

bool ImportMappedRanges(const Context& context, const GuestAllocations::Mapped& ranges, std::uint64_t generation, bool adoptDevice) {
    if (context.hostImportAlignment == 0) return true;
    ensurePinWaiter();
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    if (!adoptDevice && (state.device != context.device || state.watchDevice != context.device)) return false;
    const bool adopting = state.device != context.device;
    GuestAllocations::Lease current;
    if (adopting) {
        current = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
        refreshImports(context, state, current);
    } else if (state.refreshedGeneration > generation) {
        return true;
    }
    for (const auto& mapped : ranges) {
        const auto range = mapped.lock();
        if (range == nullptr || range->bytes <= 0x10000) continue;
        if (current.empty()) current = GuestAllocations::GuestAllocationsAcquireMapped_nid_no_patch(ranges);
        if (leasedRangeOwner(current, range->address) != range) continue;
        try {
            const auto run = importRun(current, range->address, std::numeric_limits<std::uint64_t>::max(), effectiveBudget(context, state));
            const bool replacing = wouldRetireImport(state, run);
            if (!GuestMemory::GpuMutex().HeldByThisThread()) {
                const std::array<std::pair<std::uint64_t, std::uint64_t>, 1> span{{{run.base, run.base + run.bytes}}};
                if (replacing || StorageTexture::AnyPendingOverlaps(span)) return false;
            } else if (replacing) {
                current = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
                if (leasedRangeOwner(current, range->address) != range) continue;
            }
            if (!state.importedAtMap.emplace(range->address, range->bytes).second && findImport(state, range->address, range->address + range->bytes) == nullptr) continue;
            importAllocation(context, state, range->address, current, ImportRequester::Map, 0, false);
        } catch (const std::exception& error) {
            std::fprintf(stderr, "[gpu] import of GPU memory mapped at 0x%llx+0x%zx failed: %s\n", static_cast<unsigned long long>(range->address), range->bytes, error.what());
        }
    }
    return true;
}

bool RegisteredReadableCovers(std::uint64_t address, std::size_t bytes, std::vector<RegisteredPiece>* pieces) {
    if (pieces != nullptr) pieces->clear();
    if (bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address) return false;
    const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
    const auto end = address + bytes;
    for (auto at = address; at < end;) {
        const auto* range = containingRange(lease, at, at + 1);
        if (range == nullptr) return false;
        if (pieces != nullptr) pieces->push_back({range->address, range->bytes, range->writable, range->gpu});
        at = range->address + range->bytes;
    }
    return true;
}

std::uint64_t RegisteredReadableEnd(std::uint64_t address) {
    if (address == std::numeric_limits<std::uint64_t>::max()) return 0;
    const auto lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
    const auto* range = containingRange(lease, address, address + 1);
    return range != nullptr ? range->address + range->bytes : 0;
}

bool HostImportCovers(const Context& context, std::uint64_t address, std::size_t bytes) {
    if (context.hostImportAlignment == 0 || bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address) return false;
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    // Imports of a replaced device are not this device's; a stale registry only means an import
    // may be dropped at the next reconcile, which the binding path handles.
    return state.device == context.device && peekImport(state, address, address + bytes) != nullptr;
}

bool ComparedHostImportCovers(const Context& context, std::uint64_t address, std::size_t bytes) {
#ifdef _WIN32
    static_cast<void>(context);
    static_cast<void>(address);
    static_cast<void>(bytes);
    return false;
#else
    if (context.hostImportAlignment == 0 || bytes == 0 || bytes > std::numeric_limits<std::uint64_t>::max() - address) return false;
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    const auto* import = state.device == context.device ? peekImport(state, address, address + bytes) : nullptr;
    return import != nullptr && import->unwatched;
#endif
}

GuestBufferMemory::GuestBufferMemory(const Context& context) : context(context) {}

bool GuestBufferMemory::WritesOverlap(std::uint64_t address, std::size_t bytes) const {
    return std::any_of(writes.begin(), writes.end(), [&](const auto& range) { return address < range.second && range.first < address + bytes; });
}

void GuestBufferMemory::CountAddressBuild(double snapshotsUs) {
    auto& totals = AddressBuilds();
    auto& timing = ThreadAddressTiming();
    std::lock_guard lock(totals.mutex);
    ++totals.builds;
    totals.sums.leaseUs += timing.leaseUs;
    totals.sums.importsUs += timing.importsUs;
    totals.sums.mirrorsUs += timing.mirrorsUs;
    totals.sums.compareUs += timing.compareUs;
    totals.sums.blocksCompared += timing.blocksCompared;
    totals.sums.blocksCopied += timing.blocksCopied;
    totals.snapshotsUs += snapshotsUs;
    timing = {};
    const auto now = std::chrono::steady_clock::now();
    if (now - totals.lastReport < std::chrono::seconds(10)) return;
    totals.lastReport = now;
    const auto per = [&](double us) { return us / static_cast<double>(totals.builds); };
    const auto table = BdaResources::TableCacheCounters();
    const auto& seen = totals.tableSeen;
    const auto delta = [](std::uint64_t now, std::uint64_t before) { return static_cast<unsigned long long>(now - before); };
    const auto space = AddressSpaceCounters();
    const auto& spaceSeen = totals.spaceSeen;
    AgcDriver::ProfilePrint_nid_no_patch("[address] %llu address-based builds (10 s), us per build: lease %.0f, imports pass %.0f, mirror prepare %.0f, compare %.0f (%.0f blocks, %.1f copied), snapshots %.0f; space hits %llu / rebuilds: generation %llu, epoch %llu, device %llu, waiter drop %llu, first %llu; unpublished %llu, dissolved: overlap %llu, imports %llu%s; [bda-table] hits %llu / misses %llu (space tables %llu), first entry: expired %llu, hash differs low %llu / heap %llu, same hash %llu, none held %llu\n", static_cast<unsigned long long>(totals.builds), per(totals.sums.leaseUs), per(totals.sums.importsUs), per(totals.sums.mirrorsUs), per(totals.sums.compareUs), per(static_cast<double>(totals.sums.blocksCompared)), per(static_cast<double>(totals.sums.blocksCopied)), per(totals.snapshotsUs), delta(space.hits, spaceSeen.hits), delta(space.rebuiltGeneration, spaceSeen.rebuiltGeneration), delta(space.rebuiltEpoch, spaceSeen.rebuiltEpoch), delta(space.rebuiltDevice, spaceSeen.rebuiltDevice), delta(space.rebuiltWaiterDrop, spaceSeen.rebuiltWaiterDrop), delta(space.rebuiltFirst, spaceSeen.rebuiltFirst), delta(space.unpublished, spaceSeen.unpublished), delta(space.dissolvedOverlap, spaceSeen.dissolvedOverlap), delta(space.dissolvedImports, spaceSeen.dissolvedImports), space.enabled ? "" : " (cache off)", delta(table.hits, seen.hits), delta(table.misses, seen.misses), delta(table.spaceTables, seen.spaceTables), delta(table.firstExpired, seen.firstExpired), delta(table.firstDiffersLow, seen.firstDiffersLow), delta(table.firstDiffersHeap, seen.firstDiffersHeap), delta(table.firstSameHash, seen.firstSameHash), delta(table.firstEmpty, seen.firstEmpty));
    totals.tableSeen = table;
    totals.spaceSeen = space;
    totals.builds = 0;
    totals.sums = {};
    totals.snapshotsUs = 0;
}

bool GuestBufferMemory::HoldsMirror(const std::shared_ptr<ImageMirror>& mirror) const {
    for (const auto& region : regions) {
        if (region.mirror == mirror) return true;
    }
    return false;
}

bool GuestBufferMemory::EvictMirror(const std::shared_ptr<ImageMirror>& mirror) {
    bool found = false;
    for (auto& region : regions) {
        if (region.mirror == mirror) {
            region.mirror.reset();
            region.hostBacked = !region.writable;
            auto committed = GuestMemory::DescribeCommitted(region.begin, region.end - region.begin);
            if (!committed.whole) {
                region.sparse = true;
                region.backed = std::move(committed.ranges);
            } else {
                region.sparse = false;
                region.backed.clear();
            }
            found = true;
        }
    }
    return found;
}

bool GuestBufferMemory::WasEvicted(std::uint64_t address) const {
    return std::find(evictedRanges.begin(), evictedRanges.end(), address) != evictedRanges.end();
}

void GuestBufferMemory::RecordEvicted(std::uint64_t address) {
    if (!WasEvicted(address)) evictedRanges.push_back(address);
}

void GuestBufferMemory::AcquireRegistered() {
    struct BuildingScope {
        explicit BuildingScope(GuestBufferMemory* self) { buildingBufferMemory = self; }
        ~BuildingScope() {
            buildingBufferMemory = nullptr;
            buildingBlocks = nullptr;
        }
    };
    const BuildingScope buildingScope(this);
    Require(!uploaded && regions.empty() && lease.empty() && space == nullptr, "guest allocation lease must precede resource registration");
    ensurePinWaiter();
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    auto& timing = ThreadAddressTiming();
    const auto lap = [&](double& into, std::chrono::steady_clock::time_point& from) {
        if (!profile) return;
        const auto now = std::chrono::steady_clock::now();
        into += std::chrono::duration<double, std::micro>(now - from).count();
        from = now;
    };
    auto at = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const bool importable = context.hostImportAlignment != 0;
    // APS5_NO_SORTED_SNAPSHOT_LOOKUP=1: AddSnapshot scans the regions as before instead of searching.
    static const bool sortedLookup = std::getenv("APS5_NO_SORTED_SNAPSHOT_LOOKUP") == nullptr;
    std::vector<RefreshBlock> blocks;
    buildingBlocks = &blocks;
    std::vector<ImageMirror*> heaps;
    bool mirrored = false;
    bool served = false;
    auto& spaces = Spaces();
    // Read before the lease below is acquired: a mutation ending in between leaves the space
    // stale by its generation, never newer than it (see AddressSpace).
    const auto generation = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    if (addressSpaceCacheEnabled()) {
        auto current = spaces.current.load();
        bool hit = false;
        {
            auto& state = Imports();
            std::lock_guard lock(state.mutex);
            const auto keeps = [&](ImageMirror* mirror) { return heapMirrorWatched(*mirror) || !importsNow(context, state, current->lease, mirror->base); };
            if (current == nullptr) (spaces.droppedByWaiter.exchange(false, std::memory_order_relaxed) ? spaces.rebuiltWaiterDrop : spaces.rebuiltFirst).fetch_add(1, std::memory_order_relaxed);
            else if (current->device != context.device) spaces.rebuiltDevice.fetch_add(1, std::memory_order_relaxed);
            else if (current->generation != generation) spaces.rebuiltGeneration.fetch_add(1, std::memory_order_relaxed);
            else if (current->importsEpoch != state.epoch) spaces.rebuiltEpoch.fetch_add(1, std::memory_order_relaxed);
            else hit = std::all_of(current->heapMirrors.begin(), current->heapMirrors.end(), keeps) && std::all_of(current->sparseMirrors.begin(), current->sparseMirrors.end(), keeps);
            importFloor = state.useCounter + 1;
            if (hit) {
                const auto stamp = ++state.useCounter;
                static const bool stampOnce = std::getenv("APS5_NO_SPACE_STAMP_ONCE") == nullptr;
                auto& spaceUse = state.spaceUses[current->serial];
                if (!stampOnce || spaceUse == 0) {
                    for (const auto& region : current->base) {
                        if (region.direct == nullptr) continue;
                        region.direct->lastUse = stamp;
                        if (stampOnce) region.direct->spaceSerial = current->serial;
                    }
                }
                spaceUse = stamp;
                if (state.spaceUses.size() > 64) {
                    const auto oldest = std::min_element(state.spaceUses.begin(), state.spaceUses.end(), [](const auto& left, const auto& right) { return left.second < right.second; });
                    for (auto& [base, entry] : state.imports) {
                        if (entry.spaceSerial != oldest->first) continue;
                        entry.lastUse = std::max(entry.lastUse, oldest->second);
                        entry.spaceSerial = 0;
                    }
                    state.spaceUses.erase(oldest);
                }
            }
        }
        if (hit) {
            for (auto* mirror : current->sparseMirrors) {
                if (refreshSparseMirror(*mirror, blocks)) continue;
                hit = false;
                blocks.clear();
                spaces.current.compare_exchange_strong(current, std::shared_ptr<const AddressSpace>{});
                break;
            }
        }
        if (hit) {
            spaces.hits.fetch_add(1, std::memory_order_relaxed);
            space = std::move(current);
            lap(timing.leaseUs, at);
            for (const auto& range : space->copied) addCopiedRange(range);
            regionsSorted = sortedLookup;
            lap(timing.importsUs, at);
            // Writable mirrors are compared with guest memory per build, as acquireMirror does for
            // an existing mirror. The preparation may wait for recorded work, whose completions can
            // retire an import the space's regions point at: UploadFinish re-checks the epoch
            // before any of them is dereferenced.
            served = true;
            mirrored = space->mirrored;
            for (const auto index : space->writableMirrors) {
                const auto& region = space->base[index];
                if (prepareRefresh(*region.mirror, region.begin, region.end - region.begin)) appendBlocks(blocks, *region.mirror, region.begin, region.end - region.begin);
            }
            lap(timing.mirrorsUs, at);
        }
    }
    if (space == nullptr) {
        // Pending GPU results in these ranges are stored when Upload prepares each region.
        lease = GuestAllocations::GuestAllocationsAcquire_nid_postfix();
        lap(timing.leaseUs, at);
        {
            auto& state = Imports();
            std::lock_guard lock(state.mutex);
            if (importable) refreshImports(context, state, lease);
            if (importFloor == 0) importFloor = state.useCounter + 1;
            state.buildFloor = importFloor;
            // Taken before the pass: the imports it takes are the registry's ones (just reconciled, so
            // none is retired by a make below), and a mirror's preparation may sync the recorder, whose
            // completions can retire an import already taken; that moves the epoch, and Upload then
            // re-checks every pointer instead of trusting it.
            importsEpoch = state.epoch;
        }
        lap(timing.importsUs, at);
        // One pass in the registry's (ascending) order, so the regions come out sorted for AddSnapshot.
        // The import lock is taken per range: a mirror's preparation may wait for recorded work, which
        // must not happen under it.
        regions.reserve(lease.size());
        static const std::uint64_t copyLimit = [] {
            const char* value = std::getenv("APS5_ADDRESS_COPY_MAX_MIB");
            return (value ? std::strtoull(value, nullptr, 10) : 64ull) << 20u;
        }();
        const auto copyOverride = addressCopyLimitOverride.load(std::memory_order_relaxed);
        const auto cap = copyOverride != 0 ? copyOverride : copyLimit;
        std::vector<AddressCopy> copies;
        std::vector<std::pair<std::uint64_t, std::uint64_t>> imported;
        const HostImport* covering = nullptr;
        std::uint64_t coveredBegin = 0;
        std::uint64_t coveredEnd = 0;
        std::uint64_t copiedBytes = 0;
        for (const auto& range : lease) {
            if (!range->readable || !range->gpu) continue;
            validate(range->address, range->bytes);
            Region region{range->address, range->address + range->bytes, range->writable, {}, nullptr};
            const HostImport* entry = nullptr;
            bool overBudget = false;
            std::vector<ImportPiece> pieces;
            const auto limit = importMaxBytes.load(std::memory_order_relaxed);
            if (importable && covering != nullptr && range->address >= coveredBegin && range->address + range->bytes <= coveredEnd) {
                entry = covering;
            } else if (importable && limit != 0 && range->bytes > limit) {
                auto& state = Imports();
                std::lock_guard lock(state.mutex);
                pieces = importPieces(context, state, *range, lease);
            } else if (importable) {
                auto& state = Imports();
                std::lock_guard lock(state.mutex);
                const auto refusals = state.budgetRefusals;
                entry = importAllocation(context, state, range->address, lease, ImportRequester::Build, 0, false);
                overBudget = state.budgetRefusals != refusals;
                covering = entry;
                if (entry != nullptr) {
                    coveredBegin = entry->base;
                    coveredEnd = entry->base + entry->bytes;
                }
            }
            if (importable && mirrorsEnabled() && entry == nullptr && pieces.empty()) {
                auto& state = Imports();
                std::lock_guard lock(state.mutex);
                pieces = budgetedImportPieces(context, state, *range, lease);
            }
            lap(timing.importsUs, at);
            if (!pieces.empty()) {
                const auto firstImported = imported.size();
                for (const auto& piece : pieces) if (piece.entry != nullptr) imported.emplace_back(piece.begin, piece.end);
                releaseImportedMirrors(context, std::span(imported).subspan(firstImported));
                for (const auto& piece : pieces) {
                    Region part{piece.begin, piece.end, range->writable, {}, nullptr};
                    if (piece.entry != nullptr) {
                        part.hostBacked = true;
                        part.direct = piece.entry;
                    } else {
                        part.mirror = acquireMirror(context, range, blocks, true, piece.begin, piece.end - piece.begin);
                        lap(timing.mirrorsUs, at);
                        Require(part.mirror != nullptr, "a partial heap import has no mirror for its remaining bytes");
                        mirrored = true;
                        if (part.mirror->sparse) {
                            part.sparse = true;
                            part.backed = part.mirror->committed;
                        }
                    }
                    regions.push_back(std::move(part));
                }
                continue;
            }
            if (entry != nullptr) {
                region.hostBacked = true;
                // Reused by Upload while no import was dropped since (see importsEpoch).
                region.direct = entry;
                regions.push_back(std::move(region));
                imported.emplace_back(range->address, range->address + range->bytes);
                continue;
            }
            if (mirrorsEnabled() && !WasEvicted(range->address)) {
                region.mirror = acquireMirror(context, range, blocks, range->releasable);
                lap(timing.mirrorsUs, at);
                if (region.mirror != nullptr) {
                    mirrored = true;
                    if (region.mirror->sparse) {
                        region.sparse = true;
                        region.backed = region.mirror->committed;
                    }
                    regions.push_back(std::move(region));
                    continue;
                }
            }
            addCopiedRange({range->address, range->address + range->bytes, range->writable});
            const auto& copied = regions.back();
            std::uint64_t committed = copied.end - copied.begin;
            if (copied.sparse) {
                committed = 0;
                for (const auto& [first, last] : copied.backed) committed += last - first;
            }
            if (overBudget && copiedBytes + committed > cap) {
                auto& state = Imports();
                std::lock_guard lock(state.mutex);
                {
                    const MirrorsYield yield(state, state.buildFloor);
                    entry = importAllocation(context, state, range->address, lease, ImportRequester::Build, 0, true);
                }
                if (entry != nullptr) {
                    covering = entry;
                    coveredBegin = entry->base;
                    coveredEnd = entry->base + entry->bytes;
                }
                static auto lastReport = std::chrono::steady_clock::now() - std::chrono::seconds(60);
                if (entry != nullptr && std::chrono::steady_clock::now() - lastReport > std::chrono::seconds(10)) {
                    lastReport = std::chrono::steady_clock::now();
                    std::fprintf(stderr, "[gpu] host import budget: 0x%llx+0x%llx imported by evicting idle imports, as copying it would pass APS5_ADDRESS_COPY_MAX_MIB\n", static_cast<unsigned long long>(range->address), static_cast<unsigned long long>(range->bytes));
                }
            }
            if (entry != nullptr) {
                regions.pop_back();
                region.hostBacked = true;
                region.direct = entry;
                regions.push_back(std::move(region));
                imported.emplace_back(range->address, range->address + range->bytes);
                continue;
            }
            copiedBytes += committed;
            const char* reason = !mirrorsEnabled() ? "mirrors disabled by APS5_NO_LEASE_MIRROR" : copied.sparse ? "unreadable pages" : !range->releasable ? "image mirror refused" : nullptr;
            copies.push_back({copied.begin, copied.end, committed, reason, reason == nullptr, overBudget});
        }
        releaseImportedMirrors(context, imported);
        for (const auto& region : regions) {
            if (region.mirror != nullptr && region.mirror->heap && !region.mirror->sparse) heaps.push_back(region.mirror.get());
        }
        regionsSorted = sortedLookup;
        if (!copies.empty()) {
            std::uint64_t copied = 0;
            for (const auto& copy : copies) copied += copy.committed;
            AddressCopyCounters::ThisThread().Count(copied);
        }
        if (const auto overflow = AddressCopyOverflow(std::move(copies), cap); !overflow.empty()) {
            std::fprintf(stderr, "FATAL: %s\n", overflow.c_str());
            std::fflush(stderr);
            std::abort();
        }
        lap(timing.importsUs, at);
        if (addressSpaceCacheEnabled()) {
            // Published only while the imports the regions point at stand: the table entries below
            // read them, and a retire during the pass (a mirror preparation's sync) has already
            // moved the epoch, in which case this build keeps the per-build form and Upload
            // re-resolves every pointer as before.
            std::vector<ShaderRecompiler::BdaAbi::Range> ranges;
            bool publish = true;
            {
                auto& state = Imports();
                std::lock_guard lock(state.mutex);
                if (state.epoch != importsEpoch) {
                    publish = false;
                    spaces.unpublished.fetch_add(1, std::memory_order_relaxed);
                } else {
                    std::size_t kept = 0;
                    for (std::size_t index = 0; index < regions.size(); ++index) {
                        auto& region = regions[index];
                        if (kept != 0) {
                            auto& previous = regions[kept - 1];
                            if (region.direct != nullptr && previous.direct == region.direct && previous.end == region.begin && previous.writable == region.writable && region.mirror == nullptr && previous.mirror == nullptr && !region.sparse && !previous.sparse && region.end <= region.direct->base + region.direct->bytes) {
                                previous.end = region.end;
                                continue;
                            }
                        }
                        if (kept != index) regions[kept] = std::move(region);
                        ++kept;
                    }
                    regions.erase(regions.begin() + static_cast<std::ptrdiff_t>(kept), regions.end());
                    for (const auto& region : regions) {
                        if (region.direct != nullptr || region.mirror != nullptr) ranges.push_back(addressRange(region));
                    }
                }
            }
            if (publish) {
                auto built = std::make_shared<AddressSpace>();
                built->generation = generation;
                built->importsEpoch = importsEpoch;
                built->device = context.device;
                built->serial = spaces.serials.fetch_add(1, std::memory_order_relaxed) + 1;
                built->lease = std::move(lease);
                built->ranges = std::move(ranges);
                std::vector<Region> extras;
                for (auto& region : regions) {
                    if (region.direct != nullptr || region.mirror != nullptr) {
                        built->base.push_back(std::move(region));
                    } else {
                        built->copied.push_back({region.begin, region.end, region.writable});
                        extras.push_back(std::move(region));
                    }
                }
                regions = std::move(extras);
                lease.clear();
                planSpaceMirrors(*built);
                {
                    auto reads = std::make_shared<Recorder::ReadList>();
                    for (const auto& region : built->base) {
                        if (region.direct != nullptr) reads->emplace_back(region.begin, region.end);
                    }
                    if (Recorder::ValidReadList(*reads)) built->directReads = std::move(reads);
                }
                space = std::move(built);
                spaces.droppedByWaiter.store(false, std::memory_order_relaxed);
                if (evictedRanges.empty()) spaces.current.store(space);
            }
        }
    }
    std::vector<std::pair<std::size_t, std::uint64_t>> checked;
    if (served) checked = refreshHeapRuns(*space, blocks);
    else refreshHeapMirrors(heaps, blocks);
    lap(timing.mirrorsUs, at);
    const auto copiedBefore = Mirrors().blocksCopied;
    compareBlocks(blocks);
    for (const auto& [run, collected] : checked) space->verified[run].store(collected, std::memory_order_relaxed);
    lap(timing.compareUs, at);
    if (profile) {
        timing.blocksCompared += blocks.size();
        timing.blocksCopied += Mirrors().blocksCopied - copiedBefore;
    }
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

void GuestBufferMemory::addCopiedRange(const CopiedRange& range) {
    validate(range.begin, range.end - range.begin);
    Region region{range.begin, range.end, range.writable, {}, nullptr};
    // Read-only ranges are read from live guest memory at Upload: the lease pins them and the guest
    // cannot write them, so that equals a snapshot taken now without the extra copy. Reserved heaps
    // are registered whole while the guest commits pages on demand: committed pages only.
    region.hostBacked = !range.writable;
    auto committed = GuestMemory::DescribeCommitted(range.begin, range.end - range.begin);
    if (!committed.whole) {
        region.sparse = true;
        region.backed = std::move(committed.ranges);
    }
    regions.push_back(std::move(region));
}

GuestBufferMemory::BaseOverlap GuestBufferMemory::baseOverlap(std::uint64_t begin, std::uint64_t end, const Region** owner) const {
    if (space == nullptr || space->base.empty()) return BaseOverlap::None;
    const auto& base = space->base;
    const auto found = std::upper_bound(base.begin(), base.end(), begin, [](std::uint64_t value, const Region& region) { return value < region.begin; });
    if (found != base.begin()) {
        const auto& previous = *std::prev(found);
        if (begin < previous.end) {
            if (end > previous.end) return BaseOverlap::Partial;
            if (owner != nullptr) *owner = &previous;
            return BaseOverlap::Inside;
        }
    }
    return found != base.end() && found->begin < end ? BaseOverlap::Partial : BaseOverlap::None;
}

void GuestBufferMemory::dissolveSpace(bool resolve) {
    Require(space != nullptr, "no address space to dissolve");
    std::vector<Region> merged;
    merged.reserve(space->base.size() + regions.size());
    if (prepared) {
        std::merge(space->base.begin(), space->base.end(), std::make_move_iterator(regions.begin()), std::make_move_iterator(regions.end()), std::back_inserter(merged), [](const Region& left, const Region& right) { return left.begin < right.begin; });
        std::uint64_t reached = 0;
        for (const auto& region : merged) {
            regionsOverlap = regionsOverlap || region.begin < reached;
            reached = std::max(reached, region.end);
        }
    } else {
        merged.assign(space->base.begin(), space->base.end());
        merged.insert(merged.end(), std::make_move_iterator(regions.begin()), std::make_move_iterator(regions.end()));
        regionsSorted = false;
    }
    if (resolve) {
        for (auto& region : merged) {
            if (region.direct == nullptr) continue;
            region.pending = true;
            region.writable = region.writable || WritesOverlap(region.begin, static_cast<std::size_t>(region.end - region.begin));
        }
    }
    lease = space->lease;
    importsEpoch = space->importsEpoch;
    regions = std::move(merged);
    space.reset();
}

const GuestBufferMemory::Region* GuestBufferMemory::owner(std::uint64_t address, std::uint64_t end) const {
    const auto candidate = [address, end](const std::vector<Region>& list, bool overlapping) -> const Region* {
        const auto found = std::upper_bound(list.begin(), list.end(), address, [](std::uint64_t value, const Region& region) { return value < region.begin; });
        if (found == list.begin()) return nullptr;
        if (overlapping) {
            for (auto it = found; it != list.begin();) {
                --it;
                if (end <= it->end) return &*it;
            }
        }
        return &*std::prev(found);
    };
    const auto* own = candidate(regions, regionsOverlap);
    const auto* shared = space != nullptr ? candidate(space->base, false) : nullptr;
    const auto holds = [end](const Region* region) { return region != nullptr && end <= region->end; };
    if (holds(own) != holds(shared)) return holds(own) ? own : shared;
    if (own == nullptr || (shared != nullptr && shared->begin > own->begin)) return shared;
    return own;
}

namespace {

bool bdaStagingEnabled() {
    static const bool disabled = std::getenv("APS5_NO_BDA_STAGING") != nullptr;
    return !disabled;
}

template<typename Regions>
bool mirrorOverlaps(const Regions& base, std::uint64_t begin, std::uint64_t end) {
    auto found = std::upper_bound(base.begin(), base.end(), begin, [](std::uint64_t value, const auto& region) { return value < region.begin; });
    if (found != base.begin()) --found;
    for (; found != base.end() && found->begin < end; ++found) {
        if (found->end > begin && found->mirror != nullptr) return true;
    }
    return false;
}

}

void GuestBufferMemory::addDescriptorRegion(std::uint64_t address, std::size_t bytes, bool atomic, bool swept, bool readOnly) {
    validate(address, bytes);
    const auto begin = address & ~std::uint64_t{3};
    const auto end = begin == address ? address + bytes : (address + bytes + 3) & ~std::uint64_t{3};
    const Region* inside = nullptr;
    bool overlay = false;
    switch (baseOverlap(begin, end, &inside)) {
        case BaseOverlap::Inside:
            if (!servesView(*inside, begin)) {
                dissolveSpace(false);
                break;
            }
            if (overlayCandidate(*inside, readOnly) && GuestMemory::DescribeCommitted(begin, static_cast<std::size_t>(end - begin)).whole) {
                overlay = true;
                break;
            }
            if (bdaStagingEnabled() && stagingAllowed) servedInsideBase.emplace_back(begin, end);
            return;
        case BaseOverlap::Partial:
            if (readOnly && !mirrorOverlaps(space->base, begin, end) && !WritesOverlap(begin, static_cast<std::size_t>(end - begin))) {
                readsOverBase.emplace_back(begin, end);
                break;
            }
            Spaces().dissolvedOverlap.fetch_add(1, std::memory_order_relaxed);
            dissolveSpace(false);
            break;
        case BaseOverlap::None:
            break;
    }
    // `writable` here means the bytes come from live guest memory (not a snapshot), whether or not
    // the shader stores to them; what is written back is decided by Writes() alone.
    Region region{begin, end, true, {}, nullptr};
    region.atomic = atomic;
    region.swept = swept;
    region.lastView = begin;
    region.overlay = overlay;
    auto committed = GuestMemory::DescribeCommitted(begin, static_cast<std::size_t>(end - begin));
    if (!committed.whole) {
        // A GPU heap bound whole while the guest commits its pages on demand, or a descriptor left
        region.sparse = true;
        region.sparseHeap = true;
        region.backed = std::move(committed.ranges);
    }
    regions.push_back(std::move(region));
    regionsSorted = false;
}

bool GuestBufferMemory::servesView(const Region& region, std::uint64_t view) {
    return region.mirror == nullptr || view + 4 <= region.mirror->base + region.mirror->bytes;
}

bool GuestBufferMemory::sparseHeapCandidate(const Region& region) {
    return region.sparseHeap && region.sparse && region.mirror == nullptr && region.direct == nullptr && (region.hostBacked || region.writable) && mirrorsEnabled() && sparseMirrorsEnabled();
}

std::shared_ptr<const GuestAllocations::Range> joinedHeapRange(const GuestAllocations::Lease& lease, std::uint64_t begin, std::uint64_t end) {
    const auto run = crossedRun(lease, begin, end);
    if (run.ranges.size() < 2 || std::any_of(run.ranges.begin(), run.ranges.end(), [](const auto& range) { return !range->releasable; })) return nullptr;
    releaseExpiredJoinedHeaps();
    auto& joined = JoinedHeaps();
    std::lock_guard lock(joined.mutex);
    auto& entry = joined.entries[run.base];
    bool same = entry.range != nullptr && entry.members.size() == run.ranges.size();
    for (std::size_t index = 0; same && index < run.ranges.size(); ++index) same = sameRange(entry.members[index], run.ranges[index]);
    if (!same) {
        const auto& first = *run.ranges.front();
        entry.members.assign(run.ranges.begin(), run.ranges.end());
        entry.range = std::make_shared<const GuestAllocations::Range>(GuestAllocations::Range{run.base, static_cast<std::size_t>(run.bytes), true, first.writable, first.allocationAddress, first.allocationBytes, true, first.gpu, first.backingSerial});
    }
    return entry.range;
}

bool GuestBufferMemory::bindSparseMirror(Region& region, const GuestAllocations::Lease& lease) {
    const auto backedEnd = region.backed.empty() ? region.begin : region.backed.back().second;
    const auto neededEnd = std::min(region.end, std::max(backedEnd, std::max(region.lastView, region.begin) + 4u));
    std::shared_ptr<const GuestAllocations::Range> owner;
    bool joined = false;
    if (const auto* range = containingRange(lease, region.begin, neededEnd); range != nullptr) {
        if (!range->releasable) return false;
        owner = leasedRangeOwner(lease, range->address);
    } else {
        joined = true;
        owner = joinedHeapRange(lease, region.begin, neededEnd);
        if (owner == nullptr) return false;
    }
    std::vector<RefreshBlock> blocks;
    std::shared_ptr<ImageMirror> mirror;
    try {
        mirror = acquireMirror(context, owner, blocks, true, 0, 0, joined);
    } catch (const std::runtime_error& error) {
        static std::atomic<int> reported{0};
        if (reported.fetch_add(1, std::memory_order_relaxed) < 4) std::fprintf(stderr, "[gpu] %s; copying the sparse heap instead\n", error.what());
        return false;
    }
    if (mirror != nullptr && mirror->heap && !mirror->sparse) {
        std::vector<ImageMirror*> whole{mirror.get()};
        refreshHeapMirrors(whole, blocks);
    }
    compareBlocks(blocks);
    if (mirror == nullptr || !mirror->heap || region.begin < mirror->base || (region.begin - mirrorStart(*mirror)) % context.limits.minStorageBufferOffsetAlignment != 0) return false;
    region.mirror = std::move(mirror);
    if (!servesView(region, std::max(region.lastView, region.begin))) {
        region.mirror = nullptr;
        return false;
    }
    if (region.mirror->sparse) region.backed = clipPieces(region.mirror->committed, region.begin, region.end);
    else region.backed = clipPieces({{region.mirror->base, region.mirror->base + region.mirror->bytes}}, region.begin, region.end);
    region.snapshot.clear();
    std::lock_guard lock(Mirrors().mutex);
    ++Mirrors().sparseBinds;
    return true;
}

void GuestBufferMemory::AddWritable(std::uint64_t address, std::size_t bytes, bool atomic, bool swept) {
    if (space != nullptr && std::any_of(readsOverBase.begin(), readsOverBase.end(), [&](const auto& range) { return address < range.second && range.first < address + bytes; })) {
        Spaces().dissolvedOverlap.fetch_add(1, std::memory_order_relaxed);
        dissolveSpace(false);
    }
    addDescriptorRegion(address, bytes, atomic, swept, false);
    writes.emplace_back(address, address + bytes);
}

void GuestBufferMemory::AddReadable(std::uint64_t address, std::size_t bytes) {
    // Not in `writes`: no reference copy (copyRegion), no write-back, no pending-write note, no
    // direct-write mark, and UploadPrepare copies it without the device lock. A written descriptor
    // overlapping the range still covers it through its own Writes() entry.
    addDescriptorRegion(address, bytes, false, false, true);
}

void GuestBufferMemory::AddSnapshot(const GuestMemorySnapshot& snapshot) {
    validate(snapshot.address, snapshot.bytes.size());
    const auto end = snapshot.address + snapshot.bytes.size();
    // A snapshot inside a region needs no copy: the region's bytes serve it (they are checked to
    // agree). Right after AcquireRegistered the regions are sorted and non-overlapping (registered
    // ranges), so the one that can contain the snapshot is found by search rather than by scanning
    // the ~1200 of an address-based build for each captured region.
    const Region* owner = nullptr;
    if (baseOverlap(snapshot.address, end, &owner) == BaseOverlap::Partial) {
        Spaces().dissolvedOverlap.fetch_add(1, std::memory_order_relaxed);
        dissolveSpace(false);
    }
    if (owner == nullptr && regionsSorted) {
        const auto found = std::upper_bound(regions.begin(), regions.end(), snapshot.address, [](std::uint64_t value, const Region& region) { return value < region.begin; });
        if (found != regions.begin() && end <= std::prev(found)->end) owner = &*std::prev(found);
    } else if (owner == nullptr) {
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
        // A live region over a unit shadow's fresh results reads the import's stale bytes here
        // (the publish happens when the region is bound, under the device lock): not compared.
        if (live && (!checkLive || AnyShadowedOverlaps(snapshot.address, snapshot.bytes.size()))) {
            if (profile) Snapshots().skipped.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        const auto offset = static_cast<std::size_t>(snapshot.address - owner->begin);
        const auto* source = live ? reinterpret_cast<const std::byte*>(snapshot.address) : owner->snapshot.data() + offset;
        if (std::memcmp(source, snapshot.bytes.data(), snapshot.bytes.size()) != 0) {
            std::size_t first = 0;
            while (first < snapshot.bytes.size() && source[first] == snapshot.bytes[first]) ++first;
            char text[256];
            std::snprintf(text, sizeof(text), "guest snapshot differs from registered memory: snapshot 0x%llx+0x%zx (%s region 0x%llx+0x%llx%s%s%s) differs first at +0x%zx", static_cast<unsigned long long>(snapshot.address), snapshot.bytes.size(), live ? "live" : "snapshot-backed", static_cast<unsigned long long>(owner->begin), static_cast<unsigned long long>(owner->end - owner->begin), owner->writable ? ", writable" : "", owner->hostBacked ? ", host-backed" : "", owner->mirror != nullptr ? ", mirrored" : "", first);
            static const bool fatal = std::getenv("APS5_SNAPSHOT_CHECK_FATAL") != nullptr;
            if (!live || fatal) throw std::runtime_error(text);
            static AgcDriver::Mutex reportMutex;
            static auto lastReport = std::chrono::steady_clock::now() - std::chrono::seconds(60);
            std::lock_guard lock(reportMutex);
            const auto mismatches = Snapshots().mismatched.fetch_add(1, std::memory_order_relaxed) + 1;
            const auto now = std::chrono::steady_clock::now();
            if (now - lastReport >= std::chrono::seconds(10)) {
                lastReport = now;
                std::fprintf(stderr, "[gpu] %s; the live region serves the GPU (%llu mismatches so far; APS5_SNAPSHOT_CHECK_FATAL=1 fails the dispatch instead)\n", text, static_cast<unsigned long long>(mismatches));
            }
            return;
        }
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

// Device-local staging. A written V# element bound in place makes the shader store into host
// memory over PCIe: the froxel culling kernel's ~248K scattered 4-byte stores took 2.8 ms per
// dispatch and the histogram kernels' ~60K atomics on a 1 KB bin table 1.4 ms, on a GPU that is
// otherwise idle. Instead an element the recompiler marks written (DescriptorBinding::bufferWritten,
// within a size window) or atomic (bufferAtomic, up to its own cap) that lies inside a host import
// takes the gpuCopy path above with a DEVICE_LOCAL buffer: copied in from the import before the
// work (never skipped for a write-only element: the copy-back is the whole written range, so every
// byte the shader leaves alone must hold the import's bytes), bound in the import's place, and
// copied back by RecordCopyBacks after the work, with the pending-write note and the direct-write
// marks of an in-place write. Only builds whose every use records the work and calls
// RecordCopyBacks take it (dispatches, see AllowDeviceStaging; a synchronous draw would store the
// staging bytes from the CPU, which a device-local buffer cannot serve). In an address-based build
// a written element inside one direct import region of the space becomes an overlay region
// (resolveOverlays), and AddressRanges cuts the import's range around it so that its pointers reach
// the shadow as its descriptor does. The
// shadow is per use, not resident: a reused build records its copy-in anew (RecordStagingCopies).
// Accepted trade-off, as for the misaligned copies above: a CPU write into a staged range between
// the copy-in and the copy-back is rolled back by the copy-back (the whole written element range,
// since store extents are not known), for the batch's latency; the staging window keeps the
// exposure to the elements named on the [buffers] staging line and APS5_TRACE_STAGING.
// APS5_NO_WRITTEN_SHADOW=1 and APS5_NO_ATOMIC_STAGING=1 bind those elements in place as before;
// APS5_WRITTEN_SHADOW_MIN_KIB / APS5_WRITTEN_SHADOW_MAX_KIB (16 / 2048, 8192 on a discrete GPU) bound
// the written window: on an integrated GPU a kernel streaming once through a large buffer (the
// engine's memcpy kernel over 4-8 MiB video frames) would only gain the two copies, while on a
// discrete GPU its stores into the host import cross the bus and cost more than the copies, so the
// window takes those elements there. APS5_ATOMIC_STAGE_MAX_KIB (1024) bounds the atomic one.
constexpr std::uint64_t SweptShadowMax = std::uint64_t{16} << 20u;
constexpr std::uint64_t DiscreteWrittenShadowMax = std::uint64_t{8} << 20u;
constexpr std::uint64_t DiscreteSweptShadowMax = std::uint64_t{256} << 20u;

bool writtenShadowEnabled() {
    static const bool disabled = std::getenv("APS5_NO_WRITTEN_SHADOW") != nullptr;
    return !disabled;
}

bool atomicStagingEnabled() {
    static const bool disabled = std::getenv("APS5_NO_ATOMIC_STAGING") != nullptr;
    return !disabled;
}

std::uint64_t kibSetting(const char* name, std::uint64_t defaultKib) {
    const char* value = std::getenv(name);
    return (value != nullptr ? std::strtoull(value, nullptr, 10) : defaultKib) << 10u;
}

std::uint64_t writtenShadowMin() {
    static const std::uint64_t bytes = kibSetting("APS5_WRITTEN_SHADOW_MIN_KIB", 16);
    return bytes;
}

std::uint64_t writtenShadowMax(bool discreteGpu) {
    static const std::uint64_t setting = kibSetting("APS5_WRITTEN_SHADOW_MAX_KIB", 0);
    if (setting != 0) return setting;
    return discreteGpu ? DiscreteWrittenShadowMax : std::uint64_t{2048} << 10u;
}

std::uint64_t atomicStageMax() {
    static const std::uint64_t bytes = kibSetting("APS5_ATOMIC_STAGE_MAX_KIB", 1024);
    return bytes;
}

VkMemoryPropertyFlags copyBufferProperties(bool deviceLocal) {
    return deviceLocal ? VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT : (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

// [gputime] classes (APS5_PROFILE_GPU) of a build's GPU copies, barriers included: the copy-ins
// of its regions and their copy-backs (staged or host shadows alike).
constexpr auto StagingCopyInKey = Recorder::CommandClass::StagingIn;
constexpr auto StagingCopyBackKey = Recorder::CommandClass::StagingOut;

// APS5_PROFILE_DRAW: the GPU copies, reported in the [buffers] uploads line (cumulative), and the
// device-local staging counts, reported as [buffers] staging every 10 s (see reportStaging).
struct CopyStats {
    std::atomic<std::uint64_t> gpuCopies{0};
    std::atomic<std::uint64_t> gpuCopyBytes{0};
    std::atomic<std::uint64_t> gpuCopyBacks{0};
    std::atomic<std::uint64_t> gpuCopyBackBytes{0};
    // Written gpuCopy regions of a use that recorded no copy-back (a synchronous draw): stored
    // from the staging buffer by the CPU in WriteBack.
    std::atomic<std::uint64_t> stagingStores{0};
    // Staged regions copied in (every use), those with an atomic element, those of a reused build,
    // the bytes copied in and back, staged regions whose use recorded a copy-in but no copy-back
    // (counted when the build is reused or destroyed), and shadows the device refused.
    std::atomic<std::uint64_t> staged{0};
    std::atomic<std::uint64_t> stagedAtomic{0};
    std::atomic<std::uint64_t> stagedReused{0};
    std::atomic<std::uint64_t> stagedInBytes{0};
    std::atomic<std::uint64_t> stagedOutBytes{0};
    std::atomic<std::uint64_t> stagedLost{0};
    std::atomic<std::uint64_t> stagingRefused{0};
    std::atomic<std::int64_t> lastStagingReport{0};
};

CopyStats& Copies() {
    static CopyStats stats;
    return stats;
}

// A shadow the device cannot provide (device memory or the allocation count exhausted: every
// shadow is its own VkDeviceMemory, retained by cached builds) is no error: nullptr, and the
// region takes the path it would take without staging (Region::unstaged).
std::shared_ptr<Buffer> stagingBuffer(const Context& context, std::size_t bytes, VkBufferUsageFlags usage) {
    try {
        return std::make_shared<Buffer>(context, bytes, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    } catch (const std::exception& error) {
        static std::atomic<int> reported{0};
        if (reported.fetch_add(1, std::memory_order_relaxed) < 4) std::fprintf(stderr, "[buffers] staging shadow of %zu bytes refused: %s\n", bytes, error.what());
        Copies().stagingRefused.fetch_add(1, std::memory_order_relaxed);
        return nullptr;
    }
}

// APS5_TRACE_STAGING=1: every distinct staged range once, with its size and whether an atomic
// element lies in it, to see which elements the staging window admits at a stage.
void traceStaged(std::uint64_t begin, std::uint64_t end, bool atomic) {
    static const bool trace = std::getenv("APS5_TRACE_STAGING") != nullptr;
    if (!trace) return;
    static AgcDriver::Mutex mutex;
    static std::set<std::pair<std::uint64_t, std::uint64_t>> seen;
    std::lock_guard lock(mutex);
    if (!seen.insert({begin, end}).second) return;
    std::fprintf(stderr, "[staging] 0x%llx+0x%llx (%.1f KiB)%s\n", static_cast<unsigned long long>(begin), static_cast<unsigned long long>(end - begin), (end - begin) / 1024.0, atomic ? " atomic" : "");
}

class AdaptiveStagingGate {
public:
    static AdaptiveStagingGate& Get() {
        static AdaptiveStagingGate instance;
        return instance;
    }

    static bool Enabled() {
        static const bool disabled = std::getenv("APS5_NO_ADAPTIVE_STAGING") != nullptr;
        return !disabled;
    }

    bool InPlace(std::uint64_t begin, std::uint64_t end, bool use) {
        if (!Enabled()) return false;
        std::lock_guard lock(mutex);
        if (!stampsRequested) {
            Recorder::RequestBatchStamps();
            policy.report = [](const std::string& line) { std::fprintf(stderr, "%s\n", line.c_str()); };
            stampsRequested = true;
        }
        const auto [frames, busyMs] = Recorder::FrameGpuBusy();
        const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        return policy.InPlace(begin, end, use, frames, busyMs, nowMs);
    }

private:
    std::mutex mutex;
    AdaptiveStaging policy;
    bool stampsRequested = false;
};

void reportStaging() {
    auto& stats = Copies();
    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    auto last = stats.lastStagingReport.load();
    if (nowMs - last < 10000 || !stats.lastStagingReport.compare_exchange_strong(last, nowMs)) return;
    // One reporter per period (the exchange above), so the previous totals need no lock.
    static std::uint64_t lastStaged = 0, lastIn = 0, lastOut = 0;
    const auto staged = stats.staged.load();
    const auto in = stats.stagedInBytes.load();
    const auto out = stats.stagedOutBytes.load();
    AgcDriver::ProfilePrint_nid_no_patch("[buffers] staging: %llu regions staged device-local (%llu with atomics, %llu of reused builds), %.0f KiB copied in, %.0f KiB copied back, %llu without copy-back, %llu shadows refused; last 10 s: %llu regions, %.0f KiB in, %.0f KiB back\n", static_cast<unsigned long long>(staged), static_cast<unsigned long long>(stats.stagedAtomic.load()), static_cast<unsigned long long>(stats.stagedReused.load()), in / 1024.0, out / 1024.0, static_cast<unsigned long long>(stats.stagedLost.load()), static_cast<unsigned long long>(stats.stagingRefused.load()), static_cast<unsigned long long>(staged - lastStaged), (in - lastIn) / 1024.0, (out - lastOut) / 1024.0);
    lastStaged = staged;
    lastIn = in;
    lastOut = out;
}

}

void ShutdownGuestBufferWorkers() {
    RefreshPool::Shutdown();
}

GuestBufferMemory::~GuestBufferMemory() {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!profile) return;
    // A staged region whose last use recorded its copy-in but no copy-back (the use threw before
    // RecordCopyBacks, or never dispatched): whatever the shader stored stayed in the shadow.
    for (const auto& region : regions) {
        if (region.gpuCopy && region.deviceLocal && !region.copiedBack) Copies().stagedLost.fetch_add(1, std::memory_order_relaxed);
    }
}

bool GuestBufferMemory::gpuCopyEligible(const Region& region) const {
    if (!gpuCopiesEnabled() || region.sparse || !(region.hostBacked || region.writable)) return false;
    return region.end - region.begin <= gpuCopyLimit();
}

bool GuestBufferMemory::bindableInPlace(std::uint64_t offset, bool addressable) const {
    if (adjustedRegions && !addressable) return offset % 4 == 0;
    return offset % context.limits.minStorageBufferOffsetAlignment == 0;
}

bool GuestBufferMemory::stagingEligible(const Region& region, bool addressable) const {
    return !region.adaptiveInPlace && stagingWanted(region, addressable);
}

bool GuestBufferMemory::adaptiveInPlace(const Region& region, bool use) const {
    if (!region.swept || region.atomic || region.overlay) return false;
    return AdaptiveStagingGate::Get().InPlace(region.begin, region.end, use);
}

bool GuestBufferMemory::StagingDecisionsCurrent() const {
    if (!AdaptiveStagingGate::Enabled()) return true;
    for (const auto& region : regions) {
        if (!(region.gpuCopy && region.deviceLocal) && !region.adaptiveInPlace) continue;
        if (adaptiveInPlace(region, true) != region.adaptiveInPlace) return false;
    }
    return true;
}

bool GuestBufferMemory::stagingWanted(const Region& region, bool addressable) const {
    if (!stagingAllowed || (addressable && !region.overlay) || region.unstaged || !gpuCopiesEnabled() || region.sparse || region.mirror != nullptr) return false;
    const auto bytes = region.end - region.begin;
    if (!WritesOverlap(region.begin, static_cast<std::size_t>(bytes))) return false;
    if (region.atomic && atomicStagingEnabled() && bytes <= atomicStageMax()) return true;
    return writtenShadowEnabled() && bytes >= writtenShadowMin() && bytes <= (region.swept ? std::max(writtenShadowMax(context.discreteGpu), context.discreteGpu ? DiscreteSweptShadowMax : SweptShadowMax) : writtenShadowMax(context.discreteGpu));
}

bool GuestBufferMemory::overlayCandidate(const Region& inside, bool readOnly) const {
    return !readOnly && bdaStagingEnabled() && stagingAllowed && addressStaging && gpuCopiesEnabled() && writtenShadowEnabled() && inside.direct != nullptr && inside.mirror == nullptr && !inside.sparse && Recorder::Active() != nullptr;
}

void GuestBufferMemory::resolveOverlays() {
    std::vector<std::size_t> members;
    for (std::size_t index = 0; index < regions.size(); ++index) {
        if (regions[index].overlay) members.push_back(index);
    }
    if (members.empty()) return;
    std::vector<bool> drop(regions.size(), space == nullptr);
    std::sort(members.begin(), members.end(), [&](std::size_t left, std::size_t right) { return regions[left].begin < regions[right].begin; });
    for (std::size_t first = 0; space != nullptr && first < members.size();) {
        Region cluster{regions[members[first]].begin, regions[members[first]].end, true, {}, nullptr};
        cluster.overlay = true;
        auto last = first;
        for (; last < members.size() && (last == first || regions[members[last]].begin < cluster.end); ++last) {
            cluster.end = std::max(cluster.end, regions[members[last]].end);
            cluster.atomic = cluster.atomic || regions[members[last]].atomic;
            cluster.swept = cluster.swept || regions[members[last]].swept;
        }
        const auto crosses = [&](std::uint64_t begin, std::uint64_t end) { return begin < cluster.end && cluster.begin < end && (begin < cluster.begin || end > cluster.end); };
        const Region* inside = nullptr;
        bool keep = baseOverlap(cluster.begin, cluster.end, &inside) == BaseOverlap::Inside && inside != nullptr && inside->direct != nullptr && inside->mirror == nullptr && stagingEligible(cluster, true);
        keep = keep && std::none_of(servedInsideBase.begin(), servedInsideBase.end(), [&](const auto& range) { return crosses(range.first, range.second); });
        keep = keep && std::none_of(regions.begin(), regions.end(), [&](const Region& region) { return !region.overlay && region.begin < cluster.end && cluster.begin < region.end; });
        for (auto member = first; member < last; ++member) drop[members[member]] = !keep;
        first = last;
    }
    std::size_t kept = 0;
    for (std::size_t index = 0; index < regions.size(); ++index) {
        if (drop[index]) continue;
        if (kept != index) regions[kept] = std::move(regions[index]);
        ++kept;
    }
    regions.erase(regions.begin() + static_cast<std::ptrdiff_t>(kept), regions.end());
}

void GuestBufferMemory::UploadPrepare(bool addressable) {
    Require(!prepared && !uploaded, "guest memory was already uploaded");
    prepared = true;
    resolveOverlays();
    servedInsideBase.clear();
    std::vector<std::uint32_t> order(regions.size());
    std::iota(order.begin(), order.end(), 0u);
    std::sort(order.begin(), order.end(), [&](std::uint32_t left, std::uint32_t right) { return regions[left].begin < regions[right].begin; });
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
    merged.reserve(regions.size());
    for (const auto index : order) {
        auto& region = regions[index];
        if (!merged.empty() && region.begin < merged.back().end) {
            auto& previous = merged.back();
            if (previous.mirror != nullptr || region.mirror != nullptr) {
                // A descriptor range inside a mirrored image range binds the mirror's bytes (registered
                // ranges never overlap, so the other part is such a descriptor; it sorts first when it
                // starts at the range's base). Writes into a read-only mirror are never stored. One
                // crossing the mirror's end cannot be served by it: this build copies the range.
                if (previous.mirror == nullptr) std::swap(previous, region);
                Require(region.mirror == nullptr, "image mirrors overlap");
                if (region.begin >= previous.begin && region.end <= previous.end && servesView(previous, std::max(region.lastView, region.begin))) continue;
                if (previous.mirror->sparse) previous.backed = clipPieces(previous.mirror->committed, previous.begin, previous.end);
                previous.mirror = nullptr;
                previous.hostBacked = true;
            }
            mergeBacked(previous, region);
            previous.lastView = std::max({previous.lastView, region.lastView, region.begin});
            if (region.end > previous.end) previous.direct = nullptr;
            previous.atomic = previous.atomic || region.atomic;
            previous.swept = previous.swept || region.swept;
            previous.sparseHeap = previous.sparseHeap || region.sparseHeap;
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
        if (importFloor == 0) importFloor = state.useCounter + 1;
        const bool stale = importsStale(context, state);
        for (auto& region : regions) {
            if (region.mirror != nullptr) continue;
            if (sparseHeapCandidate(region)) {
                region.direct = nullptr;
                region.pending = true;
                continue;
            }
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
            // copy fills is made here, without the device lock, and serves the CPU fallback too. A
            // staged region (see stagingEligible) takes the same path, aligned or not, with a
            // device-local buffer that the CPU fallback replaces (none without a recorder to
            // record the copies: UploadFinish then binds the region in place).
            region.adaptiveInPlace = Recorder::Active() != nullptr && stagingWanted(region, addressable) && adaptiveInPlace(region, true);
            bool staged = Recorder::Active() != nullptr && stagingEligible(region, addressable);
            const bool misaligned = !bindableInPlace(HostImportOffset(*entry, region.begin), addressable);
            if (staged) {
                const auto allocateStart = std::chrono::steady_clock::now();
                region.buffer = stagingBuffer(context, static_cast<std::size_t>(region.end - region.begin), gpuCopyUsage(addressable));
                if (profile) allocateUs.fetch_add(microsecondsSince(allocateStart), std::memory_order_relaxed);
                if (region.buffer == nullptr) {
                    // No shadow to be had: in place when aligned, else the host copy below.
                    region.unstaged = true;
                    staged = false;
                }
            }
            if (misaligned || staged) {
                if (!staged) {
                    if (!gpuCopyEligible(region)) continue;
                    const auto allocateStart = std::chrono::steady_clock::now();
                    region.buffer = std::make_shared<Buffer>(context, static_cast<std::size_t>(region.end - region.begin), gpuCopyUsage(addressable), copyBufferProperties(false));
                    if (profile) allocateUs.fetch_add(microsecondsSince(allocateStart), std::memory_order_relaxed);
                }
                region.deviceLocal = staged;
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
        if (sparseHeapCandidate(region) || (region.writable && WritesOverlap(region.begin, static_cast<std::size_t>(region.end - region.begin)))) {
            region.pending = true;
            continue;
        }
        copyRegion(region, addressable);
    }
}

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
                region.copySourceBase = entry->base - entry->lead;
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
            static_cast<void>(recorder->Commands());
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
        const bool written = region.writable && WritesOverlap(region.begin, static_cast<std::size_t>(bytes));
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

void GuestBufferMemory::recordGpuCopies(std::span<Region* const> copies, bool addressable) {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    static const bool capture = std::getenv("APS5_CAPTURE_GPU_COPIES") != nullptr;
    Require(!capture || CaptureTrace::Enabled(), "GPU copy capture requires APS5_CAPTURE_TRACE");
    auto* recorder = Recorder::Active();
    Require(recorder != nullptr, "GPU buffer copies need an active recorder");
    // A queued DCC key store or label store over a copied range lands before the copy reads it.
    for (const auto* region : copies) {
        recorder->FlushKeyStoresOverlapping(region->begin, static_cast<std::size_t>(region->end - region->begin));
        recorder->FlushStoresOverlapping(region->begin, static_cast<std::size_t>(region->end - region->begin));
    }
    VkAccessFlags covered = 0;
    const auto commands = recorder->Commands(&covered);
    const auto timing = recorder->BeginGpuTiming(StagingCopyInKey);
    if (Recorder::BarrierValidate()) {
        std::vector<std::pair<std::uint64_t, std::uint64_t>> reads;
        for (const auto* region : copies) reads.emplace_back(region->begin, region->end);
        recorder->NoteAccess(Recorder::CommandClass::StagingIn, Recorder::Access{reads, {}, {}, VK_PIPELINE_STAGE_TRANSFER_BIT});
    }
    constexpr BarrierMask leading{VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT};
    if (!Recorder::ElideCovered(Recorder::CommandClass::StagingIn, leading, covered)) {
        RecordMemoryBarrier(context, commands, leading.srcStage, leading.dstStage, leading.srcAccess, leading.dstAccess);
        Recorder::CountBarriers(Recorder::CommandClass::StagingIn);
    }
    bool stagedAny = false;
    std::uint64_t copiedBytes = 0;
    for (auto* region : copies) {
        const auto bytes = region->end - region->begin;
        copiedBytes += bytes;
        if (region->buffer == nullptr) {
            // Not made by UploadPrepare (the registry was stale then): made here, under the lock.
            const auto allocateStart = std::chrono::steady_clock::now();
            if (region->deviceLocal) region->buffer = stagingBuffer(context, static_cast<std::size_t>(bytes), gpuCopyUsage(addressable));
            if (region->buffer == nullptr) {
                // Or no shadow to be had: a host copy serves the region instead, aligned or not.
                region->deviceLocal = false;
                region->unstaged = true;
                region->buffer = std::make_shared<Buffer>(context, static_cast<std::size_t>(bytes), gpuCopyUsage(addressable), copyBufferProperties(false));
            }
            if (profile) allocateUs.fetch_add(microsecondsSince(allocateStart), std::memory_order_relaxed);
        }
        std::vector<std::byte> expected;
        const auto address = region->begin;
        const auto batch = static_cast<unsigned long long>(recorder->Submissions() + 1);
        const bool pendingWriter = recorder->PendingWriteOverlaps(address, static_cast<std::size_t>(bytes));
        const bool writable = WritesOverlap(address, static_cast<std::size_t>(bytes));
        std::shared_ptr<Buffer> inputSnapshot;
        auto copySource = region->copySource;
        auto copyOffset = region->begin - region->copySourceBase;
        if (!pendingWriter && !writable) {
            inputSnapshot = std::make_shared<Buffer>(context, static_cast<std::size_t>(bytes), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
            std::memcpy(inputSnapshot->Bytes().data(), reinterpret_cast<const void*>(address), static_cast<std::size_t>(bytes));
            copySource = inputSnapshot->Handle();
            copyOffset = 0;
            recorder->Keep(inputSnapshot, inputSnapshot->Bytes().size());
            CaptureTrace::Log("copy-input-snapshot batch=%llu address=%llx bytes=%llu", batch, static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes));
        }
        if (capture && bytes == 32) {
            CaptureTrace::Log("copy-input-candidate batch=%llu address=%llx bytes=%llu pendingWriter=%d writable=%d", batch, static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes), pendingWriter, writable);
            if (inputSnapshot != nullptr) {
                const auto snapshotBytes = inputSnapshot->Bytes();
                expected.assign(snapshotBytes.begin(), snapshotBytes.end());
            }
        }
        CopyBuffer(context, commands, copySource, copyOffset, region->buffer->Handle(), 0, bytes);
        if (!expected.empty()) {
            auto readback = std::make_shared<Buffer>(context, expected.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            CopyBuffer(context, commands, region->buffer->Handle(), 0, readback->Handle(), 0, bytes);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_HOST_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
            recorder->OnComplete([readback, expected = std::move(expected), address, batch] {
                const auto actual = readback->Bytes();
                std::size_t changed = 0;
                for (std::size_t index = 0; index < expected.size(); ++index) {
                    if (expected[index] == actual[index]) continue;
                    ++changed;
                    CaptureTrace::Log("copy-input-byte batch=%llu address=%llx offset=%zu cpu=%02x gpu=%02x", batch, static_cast<unsigned long long>(address), index, std::to_integer<unsigned>(expected[index]), std::to_integer<unsigned>(actual[index]));
                }
                CaptureTrace::Log("copy-input-result batch=%llu address=%llx bytes=%zu changed=%zu", batch, static_cast<unsigned long long>(address), expected.size(), changed);
            });
        }
        if (inputSnapshot == nullptr) recorder->NotePendingRead(region->begin, static_cast<std::size_t>(bytes), Recorder::ReadKind::GpuCopy);
        // Kept by the batch at record time, as every other recorded target is: the caller keeps
        // the resources only after descriptor and pipeline work that may throw, and a released
        // buffer would go back to the pool (reused or destroyed) under this copy command. The
        // source is a live import or one retireImport already handed to this batch.
        recorder->Keep(region->buffer);
        region->snapshot.clear();
        if (region->deviceLocal) traceStaged(region->begin, region->end, region->atomic);
        if (profile) {
            Copies().gpuCopies.fetch_add(1, std::memory_order_relaxed);
            Copies().gpuCopyBytes.fetch_add(bytes, std::memory_order_relaxed);
            if (region->deviceLocal) {
                stagedAny = true;
                Copies().staged.fetch_add(1, std::memory_order_relaxed);
                Copies().stagedInBytes.fetch_add(bytes, std::memory_order_relaxed);
                if (region->atomic) Copies().stagedAtomic.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }
    if (stagedAny) reportStaging();
    // The copied bytes are visible to the shaders bound to the buffers, which may also store into
    // them. Every stage: dispatches and draws share this path, and draws run mesh, task and
    // tessellation shaders too (the batch's own barriers around dispatches are as wide).
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    Recorder::CountBarriers(Recorder::CommandClass::StagingIn);
    recorder->MarkCovered(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
    recorder->EndGpuTiming(timing, copiedBytes);
}

void GuestBufferMemory::RecordCopyBacks(Recorder& recorder) {
    if (!uploaded || committed) return;
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    // The written ranges, merged: a V# bound twice would otherwise copy the same bytes twice.
    std::vector<std::pair<std::uint64_t, std::uint64_t>> merged;
    bool recording = false;
    VkCommandBuffer commands = VK_NULL_HANDLE;
    auto timing = Recorder::NoTiming;
    std::uint64_t copiedBytes = 0;
    if (!writes.empty()) {
        auto sorted = writes;
        std::sort(sorted.begin(), sorted.end());
        for (const auto& range : sorted) {
            if (!merged.empty() && range.first < merged.back().second) merged.back().second = std::max(merged.back().second, range.second);
            else merged.push_back(range);
        }
    }
    for (const auto& [begin, end] : merged) {
        const auto* found = owner(begin, end);
        if (found == nullptr || found->mirror == nullptr || !found->mirror->heap || !found->mirror->writable || found->mirror->resident == nullptr) continue;
        const auto& region = *found;
        {
            const auto from = begin;
            const auto to = std::min(end, servedEnd(region));
            if (from >= to || std::any_of(heapReadbacks.begin(), heapReadbacks.end(), [&](const auto& entry) { return entry.first == from; })) continue;
            if (!recording) {
                commands = recorder.Commands();
                timing = recorder.BeginGpuTiming(StagingCopyBackKey);
                RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
                Recorder::CountBarriers(Recorder::CommandClass::StagingOut);
                recording = true;
            }
            heapReadbacks.emplace_back(from, region.mirror->resident->RecordReadback(commands, from, to - from));
            recorder.Keep(heapReadbacks.back().second);
            copiedBytes += to - from;
        }
    }
    for (auto& region : regions) {
        if (!region.gpuCopy || region.copiedBack) continue;
        for (const auto& [begin, end] : merged) {
            const auto from = std::max(begin, region.begin);
            const auto to = std::min(end, region.end);
            if (from >= to) continue;
            if (!recording) {
                VkAccessFlags covered = 0;
                commands = recorder.Commands(&covered);
                timing = recorder.BeginGpuTiming(StagingCopyBackKey);
                // The shader's stores into the buffer (whichever stage made them) precede the copy.
                constexpr BarrierMask leading{VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT};
                if (!Recorder::ElideCovered(Recorder::CommandClass::StagingOut, leading, covered)) {
                    RecordMemoryBarrier(context, commands, leading.srcStage, leading.dstStage, leading.srcAccess, leading.dstAccess);
                    Recorder::CountBarriers(Recorder::CommandClass::StagingOut);
                }
                recording = true;
            }
            // Exactly the sub-range the shader may write, in place in the import: what the shader
            // would have stored there itself had the range been bindable.
            CopyBuffer(context, commands, region.buffer->Handle(), from - region.begin, region.copySource, from - region.copySourceBase, to - from);
            copiedBytes += to - from;
            recorder.Keep(region.buffer);
            if (profile) {
                Copies().gpuCopyBacks.fetch_add(1, std::memory_order_relaxed);
                Copies().gpuCopyBackBytes.fetch_add(to - from, std::memory_order_relaxed);
                if (region.deviceLocal) Copies().stagedOutBytes.fetch_add(to - from, std::memory_order_relaxed);
            }
        }
        // Only once its copies are recorded: a throw above leaves the region counted by
        // HasCopiedWrites, so the caller still registers the CPU write-back that stores the
        // staging bytes, instead of losing the shader's results.
        region.copiedBack = true;
    }
    if (!recording) return;
    if (Recorder::BarrierValidate()) recorder.NoteAccess(Recorder::CommandClass::StagingOut, Recorder::Access{{}, merged, {}, VK_PIPELINE_STAGE_TRANSFER_BIT});
    // As a GPU label store or fill: visible to everything recorded after (shaders, transfers, an
    // indirect dispatch's arguments) and to the host once the batch completed; the caller's
    // pending-write note makes CPU readers wait for that.
    constexpr VkAccessFlags copiedAccess = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_INDIRECT_COMMAND_READ_BIT | VK_ACCESS_HOST_READ_BIT;
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, copiedAccess);
    Recorder::CountBarriers(Recorder::CommandClass::StagingOut);
    recorder.MarkCovered(copiedAccess);
    recorder.EndGpuTiming(timing, copiedBytes);
}

VkDescriptorBufferInfo GuestBufferMemory::Descriptor(std::uint64_t address, std::size_t bytes, std::uint32_t& adjustment) const {
    Require(uploaded && !committed, "guest GPU memory is not available");
    Require(bytes != 0 && bytes <= std::numeric_limits<std::uint64_t>::max() - address, "invalid guest buffer view");
    const auto* found = owner(address, address + bytes);
    Require(found != nullptr, "guest buffer has no GPU owner");
    const auto& region = *found;
    const auto describe = [&](const char* what) {
        char text[256];
        std::snprintf(text, sizeof(text), "guest buffer view 0x%llx+0x%llx %s its GPU owner 0x%llx-0x%llx (buffer %d, direct %d, mirror %d)", static_cast<unsigned long long>(address), static_cast<unsigned long long>(bytes), what, static_cast<unsigned long long>(region.begin), static_cast<unsigned long long>(region.end), region.buffer != nullptr, region.direct != nullptr, region.mirror != nullptr);
        return std::string(text);
    };
    if (!(address >= region.begin && address + bytes <= region.end && (region.buffer != nullptr || region.direct != nullptr || region.mirror != nullptr))) Require(false, describe("exceeds"));
    const auto base = region.direct != nullptr ? region.direct->base - region.direct->lead : region.mirror != nullptr ? mirrorStart(*region.mirror) : region.begin;
    const auto offset = address - base;
    Require(context.limits.minStorageBufferOffsetAlignment != 0, "no storage buffer offset alignment");
    Require(base % 4 == 0, "a guest buffer view in a GPU owner that does not start at a DWORD boundary is not implemented");
    adjustment = static_cast<std::uint32_t>(offset % std::max<VkDeviceSize>(context.limits.minStorageBufferOffsetAlignment, 4));
    auto range = ViewBytes(bytes, adjustment);
    const auto end = region.direct != nullptr ? region.direct->base + region.direct->bytes : servedEnd(region);
    const bool cut = (region.direct == nullptr || region.runCut) && end < region.end;
    if (cut) {
        Require(address - adjustment < end, "guest buffer view starts past its copy");
        range = std::min<std::uint64_t>(range, end - (address - adjustment));
    }
    if (address - adjustment + range > end) Require(false, describe("rounded up to its view range exceeds"));
    Require(range <= context.limits.maxStorageBufferRange, "guest buffer view exceeds descriptor range limit");
    const auto handle = region.direct != nullptr ? region.direct->buffer : region.mirror != nullptr ? mirrorHandle(*region.mirror) : region.buffer->Handle();
    return {handle, offset - adjustment, range};
}

std::uint64_t GuestBufferMemory::ViewBytes(std::uint64_t bytes, std::uint32_t adjustment) {
    if (adjustment % 4 == 0) return bytes + adjustment;
    const auto dwords = adjustment / 4 + bytes / 4 + (adjustment % 4 + bytes % 4 + 3) / 4;
    Require(dwords <= std::numeric_limits<std::uint64_t>::max() / 4, "guest buffer view range overflow");
    return 4 * dwords;
}

ShaderRecompiler::BdaAbi::Range GuestBufferMemory::addressRange(const Region& region) {
    Require(region.buffer != nullptr || region.direct != nullptr || region.mirror != nullptr, "incomplete guest GPU upload");
    const auto address = region.direct != nullptr ? region.direct->address + (region.begin - region.direct->base) : region.mirror != nullptr ? mirrorAddress(*region.mirror) + (region.begin - region.mirror->base) : region.buffer->DeviceAddress();
    Require(region.end - region.begin <= std::numeric_limits<std::uint64_t>::max() - address, "GPU address range overflow");
    const auto permissions = ShaderRecompiler::BdaAbi::Read | (region.direct != nullptr && region.writable ? ShaderRecompiler::BdaAbi::Write : 0u);
    return {region.begin, region.direct != nullptr ? inPlaceEnd(region) : servedEnd(region), address, permissions, 0};
}

std::uint64_t GuestBufferMemory::servedEnd(const Region& region) {
    if (region.mirror != nullptr) return std::min(region.end, region.mirror->base + region.mirror->bytes);
    return region.bufferEnd != 0 ? region.bufferEnd : region.end;
}

namespace {

using AddressRange = ShaderRecompiler::BdaAbi::Range;

void AppendFrom(const AddressRange& range, std::uint64_t begin, std::uint64_t end, std::vector<AddressRange>& result) {
    if (begin >= end) return;
    auto part = range;
    part.begin = begin;
    part.end = end;
    part.deviceAddress += begin - range.begin;
    result.push_back(part);
}

void AppendDisjoint(const std::vector<AddressRange>& ranges, std::vector<AddressRange>& result) {
    std::uint64_t reached = 0;
    for (const auto& range : ranges) {
        AppendFrom(range, std::max(range.begin, reached), range.end, result);
        reached = std::max(reached, range.end);
    }
}

void AppendUncovered(const std::vector<AddressRange>& ranges, const std::vector<AddressRange>& covering, std::vector<AddressRange>& result) {
    for (const auto& range : ranges) {
        auto cursor = range.begin;
        auto cover = std::partition_point(covering.begin(), covering.end(), [&](const AddressRange& candidate) { return candidate.end <= range.begin; });
        for (; cover != covering.end() && cover->begin < range.end && cursor < range.end; ++cover) {
            AppendFrom(range, cursor, std::min(cover->begin, range.end), result);
            cursor = std::max(cursor, cover->end);
        }
        AppendFrom(range, cursor, range.end, result);
    }
}

std::vector<AddressRange> MergeByBegin(const std::vector<AddressRange>& left, const std::vector<AddressRange>& right) {
    std::vector<AddressRange> merged;
    merged.reserve(left.size() + right.size());
    std::merge(left.begin(), left.end(), right.begin(), right.end(), std::back_inserter(merged), [](const AddressRange& first, const AddressRange& second) { return first.begin < second.begin; });
    return merged;
}

}

std::vector<ShaderRecompiler::BdaAbi::Range> GuestBufferMemory::AddressRanges() const {
    Require(uploaded && !committed, "guest GPU address ranges are not available");
    std::vector<ShaderRecompiler::BdaAbi::Range> result;
    if (space != nullptr) {
        std::vector<ShaderRecompiler::BdaAbi::Range> own;
        std::vector<ShaderRecompiler::BdaAbi::Range> overlays;
        own.reserve(regions.size());
        for (const auto& region : regions) (region.overlay ? overlays : own).push_back(addressRange(region));
        std::vector<ShaderRecompiler::BdaAbi::Range> uncovered;
        uncovered.reserve(own.size());
        AppendUncovered(own, space->ranges, uncovered);
        result = MergeByBegin(space->ranges, uncovered);
        if (!overlays.empty()) {
            std::sort(overlays.begin(), overlays.end(), [](const auto& left, const auto& right) { return left.begin < right.begin; });
            std::vector<ShaderRecompiler::BdaAbi::Range> cut;
            cut.reserve(result.size() + overlays.size());
            AppendUncovered(result, overlays, cut);
            result = MergeByBegin(cut, overlays);
        }
    } else if (regionsOverlap) {
        std::vector<ShaderRecompiler::BdaAbi::Range> live;
        std::vector<ShaderRecompiler::BdaAbi::Range> copies;
        for (const auto& region : regions) (region.direct != nullptr || region.mirror != nullptr ? live : copies).push_back(addressRange(region));
        std::vector<ShaderRecompiler::BdaAbi::Range> liveParts;
        AppendDisjoint(live, liveParts);
        std::vector<ShaderRecompiler::BdaAbi::Range> copyParts;
        AppendDisjoint(copies, copyParts);
        std::vector<ShaderRecompiler::BdaAbi::Range> uncovered;
        AppendUncovered(copyParts, liveParts, uncovered);
        result = MergeByBegin(liveParts, uncovered);
    } else {
        result.reserve(regions.size());
        for (const auto& region : regions) result.push_back(addressRange(region));
    }
    if (writes.empty()) return result;
    auto sortedWrites = writes;
    std::sort(sortedWrites.begin(), sortedWrites.end());
    std::vector<std::pair<std::uint64_t, std::uint64_t>> intervals;
    for (const auto& interval : sortedWrites) {
        if (!intervals.empty() && interval.first <= intervals.back().second) intervals.back().second = std::max(intervals.back().second, interval.second);
        else intervals.push_back(interval);
    }
    std::vector<ShaderRecompiler::BdaAbi::Range> merged;
    merged.reserve(result.size() + intervals.size() * 2u);
    std::size_t firstWrite = 0;
    for (const auto& range : result) {
        if ((range.permissions & ShaderRecompiler::BdaAbi::Write) != 0u) {
            merged.push_back(range);
            continue;
        }
        while (firstWrite < intervals.size() && intervals[firstWrite].second <= range.begin) ++firstWrite;
        auto cursor = range.begin;
        const auto append = [&](std::uint64_t end, bool written) {
            if (cursor == end) return;
            auto part = range;
            part.begin = cursor;
            part.end = end;
            part.deviceAddress += cursor - range.begin;
            if (written) {
                const auto* region = owner(cursor, end);
                Require(region != nullptr && region->writable && (region->mirror == nullptr || region->mirror->writable), "runtime buffer write has no writable GPU owner");
                part.permissions |= ShaderRecompiler::BdaAbi::Write;
            }
            merged.push_back(part);
            cursor = end;
        };
        for (auto index = firstWrite; index < intervals.size() && intervals[index].first < range.end; ++index) {
            append(std::max(cursor, intervals[index].first), false);
            append(std::min(range.end, intervals[index].second), true);
        }
        append(range.end, false);
    }
    return merged;
}

std::optional<GuestBufferMemory::CachedTable> GuestBufferMemory::CachedAddressTable() const {
    if (space == nullptr) return std::nullopt;
    Require(uploaded && !committed, "guest GPU address ranges are not available");
    if (writes.empty() && regions.empty()) return CachedTable{space->serial, &space->ranges};
    if (tableRanges != nullptr) return CachedTable{tableSerial, tableRanges.get()};
    const auto& spaceRanges = space->ranges;
    const bool inWritable = regions.empty() && std::all_of(writes.begin(), writes.end(), [&](const auto& written) {
        auto at = std::partition_point(spaceRanges.begin(), spaceRanges.end(), [&](const auto& range) { return range.end <= written.first; });
        for (; at != spaceRanges.end() && at->begin < written.second; ++at) {
            if ((at->permissions & ShaderRecompiler::BdaAbi::Write) == 0u) return false;
        }
        return true;
    });
    if (inWritable) return CachedTable{space->serial, &space->ranges};
    std::vector<ShaderRecompiler::BdaAbi::Range> own;
    own.reserve(regions.size());
    for (const auto& region : regions) own.push_back(addressRange(region));
    auto key = AddressTableKey(writes, own);
    static const bool verify = std::getenv("APS5_VERIFY_BDA_TABLES") != nullptr;
    std::lock_guard lock(space->tableMutex);
    const auto found = space->tables.find(key);
    if (found != space->tables.end()) {
        space->tableRecency.splice(space->tableRecency.end(), space->tableRecency, found->second.recent);
        tableSerial = found->second.serial;
        tableRanges = found->second.ranges;
        if (verify) {
            const auto built = AddressRanges();
            Require(built.size() == tableRanges->size() && (built.empty() || std::memcmp(built.data(), tableRanges->data(), built.size() * sizeof(built.front())) == 0), "a cached BDA table differs from the ranges of the build that found it");
        }
    } else {
        tableRanges = std::make_shared<const std::vector<ShaderRecompiler::BdaAbi::Range>>(AddressRanges());
        tableSerial = Spaces().serials.fetch_add(1, std::memory_order_relaxed) + 1;
        if (space->tables.size() >= 64u) {
            space->tables.erase(*space->tableRecency.front());
            space->tableRecency.pop_front();
        }
        const auto inserted = space->tables.emplace(std::move(key), AddressSpace::Table{tableSerial, tableRanges, {}}).first;
        inserted->second.recent = space->tableRecency.insert(space->tableRecency.end(), &inserted->first);
    }
    return CachedTable{tableSerial, tableRanges.get()};
}

bool GuestBufferMemory::HasCopiedWrites() const {
    for (const auto& [begin, end] : writes) {
        const auto* found = owner(begin, end);
        if (found == nullptr) continue;
        const auto& region = *found;
        if (region.direct != nullptr) continue;
        // A GPU copy whose written sub-ranges were copied back by the GPU lands like a direct
        // write; until RecordCopyBacks ran it still counts (a use without it stores in WriteBack).
        // A staged copy never counts: its use always records the copy-back (AllowDeviceStaging),
        // and nothing could be stored from it by the CPU.
        if (region.gpuCopy && (region.copiedBack || region.deviceLocal)) continue;
        // Writes into a read-only image mirror are never stored (its pages could not take them).
        if (region.mirror != nullptr && !region.mirror->writable) continue;
        return true;
    }
    return false;
}

bool GuestBufferMemory::CopiedWritesOverlap(std::uint64_t address, std::size_t bytes) const {
    if (bytes == 0) return false;
    for (const auto& [begin, end] : writes) {
        if (!(address < end && begin < address + bytes)) continue;
        const auto* found = owner(begin, end);
        if (found == nullptr) continue;
        const auto& region = *found;
        if (region.direct != nullptr) continue;
        if (region.gpuCopy && (region.copiedBack || region.deviceLocal)) continue;
        if (region.mirror != nullptr && !region.mirror->writable) continue;
        return true;
    }
    return false;
}

void GuestBufferMemory::MarkDirectWrites() const {
    for (const auto& [begin, end] : writes) {
        const auto* found = owner(begin, end);
        if (found == nullptr) continue;
        const auto& region = *found;
        if (region.direct != nullptr || (region.gpuCopy && region.copiedBack)) {
            const auto generation = GuestMemory::MarkPossiblyWritten(begin, static_cast<std::size_t>(end - begin));
            if (generation != 0) directWritesGeneration = std::max(directWritesGeneration, generation);
        }
    }
}

namespace {

void storeChanged(std::uint64_t address, std::span<const std::byte> current, std::span<const std::byte> original) {
    const auto writable = GuestMemory::DescribeCommitted(address, current.size(), true);
    if (writable.whole) {
        GuestMemory::WriteChanged(address, current, original);
        return;
    }
    const auto unchanged = [&](std::uint64_t from, std::uint64_t to) {
        const auto at = static_cast<std::size_t>(from - address);
        if (std::memcmp(current.data() + at, original.data() + at, static_cast<std::size_t>(to - from)) == 0) return;
        char message[192];
        std::snprintf(message, sizeof(message), "the GPU changed guest memory 0x%llx+0x%llx that is not writable (a direct memory page shared by several views; aliased writes are not implemented)", static_cast<unsigned long long>(from), static_cast<unsigned long long>(to - from));
        throw std::runtime_error(message);
    };
    auto cursor = address;
    for (const auto& [first, last] : writable.ranges) {
        if (cursor < first) unchanged(cursor, first);
        const auto at = static_cast<std::size_t>(first - address);
        const auto length = static_cast<std::size_t>(last - first);
        GuestMemory::WriteChanged(first, current.subspan(at, length), original.subspan(at, length));
        cursor = last;
    }
    if (cursor < address + current.size()) unchanged(cursor, address + current.size());
}

void storeSparse(ImageMirror& mirror, std::uint64_t address, std::span<const std::byte> current, std::span<const std::byte> original) {
    constexpr std::uint64_t block = 65536;
    const auto end = address + current.size();
    const auto forget = [&](std::uint64_t from, std::uint64_t to) {
        const auto at = static_cast<std::size_t>(from - address);
        if (from >= to || std::memcmp(current.data() + at, original.data() + at, static_cast<std::size_t>(to - from)) == 0) return;
        for (auto index = from / block - mirror.base / block; index <= (to - 1) / block - mirror.base / block; ++index) mirror.generations[static_cast<std::size_t>(index)] = 0;
    };
    auto cursor = address;
    for (const auto& [first, last] : GuestMemory::DescribeCommitted(address, current.size(), true).ranges) {
        forget(cursor, first);
        const auto at = static_cast<std::size_t>(first - address);
        const auto length = static_cast<std::size_t>(last - first);
        GuestMemory::WriteChanged(first, current.subspan(at, length), original.subspan(at, length));
        cursor = last;
    }
    forget(cursor, end);
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> mergeWrites(std::vector<std::pair<std::uint64_t, std::uint64_t>> writes) {
    std::sort(writes.begin(), writes.end());
    std::vector<std::pair<std::uint64_t, std::uint64_t>> merged;
    for (const auto& range : writes) {
        if (!merged.empty() && range.first < merged.back().second) merged.back().second = std::max(merged.back().second, range.second);
        else merged.push_back(range);
    }
    return merged;
}

}

void GuestBufferMemory::takeHeapReferences() {
    heapReferences.clear();
    heapReadbacks.clear();
    for (const auto& [begin, end] : mergeWrites(writes)) {
        const auto* found = owner(begin, end);
        if (found == nullptr || found->mirror == nullptr || !found->mirror->heap || !found->mirror->writable) continue;
        Require(end <= found->end, "written range exceeds its heap mirror");
        auto& mirror = *found->mirror;
        const auto served = std::min(end, servedEnd(*found));
        if (begin >= served) continue;
        growResident(mirror, {{begin, served}});
        std::vector<std::byte> bytes(static_cast<std::size_t>(served - begin));
        if (mirror.resident != nullptr) mirror.resident->Read(begin, bytes);
        else std::memcpy(bytes.data(), mirror.buffer->Bytes().data() + (begin - mirror.base), bytes.size());
        heapReferences.emplace_back(begin, std::move(bytes));
    }
}

void GuestBufferMemory::WriteBack() {
    Require(uploaded && !committed, "guest memory cannot be committed twice or before upload");
    const auto merged = mergeWrites(writes);
    // The CPU keeps running while the GPU works, so storing whole ranges would roll back its writes to
    // bytes the shader never touched. Only runs that differ from the uploaded copy are stored.
    for (const auto& [begin, end] : merged) {
        const auto* found = owner(begin, end);
        Require(found != nullptr, "write-back range has no GPU owner");
        const auto& region = *found;
        // Every branch below but the direct, copied-back and staged ones stores from the CPU.
        if (region.direct == nullptr && !(region.gpuCopy && (region.copiedBack || region.deviceLocal)) && !(region.mirror != nullptr && !region.mirror->writable)) Recorder::NoteWrittenBack(begin, static_cast<std::size_t>(end - begin));
        if (region.direct != nullptr) {
            // The GPU wrote imported guest memory directly; page write watching did not see it.
            StorageTexture::StampKeepingNewerResults(begin, static_cast<std::size_t>(end - begin), directWritesGeneration, [&] { return GuestMemory::MarkPossiblyWritten(begin, static_cast<std::size_t>(end - begin)); });
            continue;
        }
        if (region.mirror != nullptr) {
            if (!region.mirror->writable) continue;
            Require(end <= region.end, "write-back range exceeds its GPU owner");
            // The shadow is what the GPU started from; it then takes the mirror's bytes so the next
            // refresh does not mistake the GPU's results for a guest change.
            auto& mirror = *region.mirror;
            const auto offset = static_cast<std::size_t>(begin - mirror.base);
            const auto served = std::min(end, servedEnd(region));
            if (mirror.heap && begin >= served) continue;
            const auto length = static_cast<std::size_t>((mirror.heap ? served : end) - begin);
            if (mirror.heap) {
                const auto reference = std::find_if(heapReferences.begin(), heapReferences.end(), [&](const auto& entry) { return entry.first == begin; });
                Require(reference != heapReferences.end() && reference->second.size() == length, "heap mirror write-back has no reference");
                if (mirror.resident == nullptr) {
                    const auto current = mirror.buffer->Bytes().subspan(offset, length);
                    if (mirror.sparse) storeSparse(mirror, begin, current, reference->second);
                    else storeChanged(begin, current, reference->second);
                    continue;
                }
                std::vector<std::byte> current(length);
                if (const auto readback = std::find_if(heapReadbacks.begin(), heapReadbacks.end(), [&](const auto& entry) { return entry.first == begin; }); readback != heapReadbacks.end()) {
                    Require(readback->second->Bytes().size() == length, "heap mirror readback does not match its write-back");
                    std::memcpy(current.data(), readback->second->Bytes().data(), length);
                } else {
                    mirror.resident->ReadDevice(begin, current);
                }
                mirror.resident->Store(begin, current);
                if (mirror.sparse) storeSparse(mirror, begin, current, reference->second);
                else storeChanged(begin, current, reference->second);
                continue;
            }
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
                StorageTexture::StampKeepingNewerResults(begin, static_cast<std::size_t>(end - begin), directWritesGeneration, [&] { return GuestMemory::MarkPossiblyWritten(begin, static_cast<std::size_t>(end - begin)); });
                continue;
            }
            static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
            if (region.deviceLocal) {
                // A staged region's use recorded no copy-back (it threw before RecordCopyBacks):
                // the results stay in the shadow, which has no mapping to store from; counted
                // once, when the build is destroyed or reused.
                continue;
            }
            // No copy-back was recorded (a synchronous draw): the staging bytes of the written
            // sub-range are stored whole, which is what the copy-back would have done. There is no
            // reference copy to diff against (the copy in never passed through the CPU).
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
            storeChanged(begin, region.buffer->Bytes().subspan(first, length), std::span<const std::byte>(region.uploaded).subspan(first, length));
            continue;
        }
        std::size_t packed = 0;
        for (const auto& [first, last] : region.backed) {
            const auto from = std::max(first, begin);
            const auto to = std::min(last, end);
            if (from < to) {
                const auto length = static_cast<std::size_t>(to - from);
                storeChanged(from, region.buffer->Bytes().subspan(static_cast<std::size_t>(from - region.begin), length), std::span<const std::byte>(region.uploaded).subspan(packed + static_cast<std::size_t>(from - first), length));
            }
            packed += static_cast<std::size_t>(last - first);
        }
    }
    committed = true;
    lease.clear();
    space.reset();
    heapReferences.clear();
    heapReadbacks.clear();
}

std::optional<std::vector<std::pair<std::uint64_t, std::uint64_t>>> GuestBufferMemory::DirectRegions() const {
    if (!uploaded || committed) return std::nullopt;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> result;
    if (space != nullptr) {
        for (const auto& region : space->base) {
            if (region.direct == nullptr && (region.mirror == nullptr || region.mirror->writable || region.mirror->heap)) return std::nullopt;
            result.emplace_back(region.begin, region.end);
        }
    }
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    const bool sameDevice = state.device == context.device;
    for (const auto& region : regions) {
        // A read-only mirror is as fixed as an import: its bytes and VkBuffer never change while its
        // A staged region is keyed by its import like one bound in place: the import is the source
        // of its copy-in and the destination of its copy-back, both recorded per use.
        const bool fixedMirror = region.mirror != nullptr && !region.mirror->writable && !region.mirror->heap;
        const bool staged = region.gpuCopy && region.deviceLocal;
        if (region.direct == nullptr && !fixedMirror && !staged) return std::nullopt;
        if (staged) {
            // A later use copies from `copySource` again, so it must still be the import serving
            // the range (the flush before the copy-in can retire and remake imports): the serial
            // the caller records names the current import, and its buffer must be that one.
            const auto* entry = sameDevice ? findImport(state, region.begin, region.end) : nullptr;
            if (entry == nullptr || entry->buffer != region.copySource) return std::nullopt;
        }
        if (region.direct != nullptr && (!sameDevice || findImport(state, region.begin, region.begin + 1) != region.direct)) return std::nullopt;
        result.emplace_back(region.begin, inPlaceEnd(region));
    }
    return result;
}

std::vector<GuestBufferMemory::CutRegion> GuestBufferMemory::CutRegions() const {
    std::vector<CutRegion> result;
    if (!uploaded || committed) return result;
    for (const auto& region : regions) {
        if (region.direct == nullptr || !region.runCut) continue;
        result.push_back({region.begin, region.end, inPlaceEnd(region)});
    }
    return result;
}

std::uint64_t GuestBufferMemory::inPlaceEnd(const Region& region) {
    return region.direct != nullptr && region.runCut ? std::min(region.end, region.direct->base + region.direct->bytes) : region.end;
}

void GuestBufferMemory::InPlaceReads(std::vector<std::pair<std::uint64_t, std::uint64_t>>& result) const {
    result.clear();
    if (!uploaded || committed) return;
    if (space != nullptr) {
        for (const auto& region : space->base) {
            if (region.direct != nullptr) result.emplace_back(region.begin, region.end);
        }
    }
    for (const auto& region : regions) {
        if (region.direct != nullptr) result.emplace_back(region.begin, inPlaceEnd(region));
    }
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> GuestBufferMemory::InPlaceReads() const {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> result;
    InPlaceReads(result);
    return result;
}

std::optional<GuestBufferMemory::SplitReads> GuestBufferMemory::SplitInPlaceReads() const {
    if (!uploaded || committed || space == nullptr || space->directReads == nullptr) return std::nullopt;
    SplitReads split{space->directReads, {}};
    for (const auto& region : regions) {
        if (region.direct != nullptr) split.own.emplace_back(region.begin, region.end);
    }
    return split;
}

void GuestBufferMemory::DeviceReads(std::vector<std::pair<std::uint64_t, std::uint64_t>>& result) const {
    result.clear();
    if (!uploaded || committed) return;
    if (space != nullptr) {
        for (const auto& region : space->base) {
            if (region.direct != nullptr || region.mirror != nullptr) result.emplace_back(region.begin, region.end);
        }
    }
    for (const auto& region : regions) {
        if (region.direct != nullptr || region.mirror != nullptr) result.emplace_back(region.begin, region.end);
    }
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> GuestBufferMemory::DeviceReads() const {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> result;
    DeviceReads(result);
    return result;
}

void GuestBufferMemory::RecordStagingCopies(Recorder& recorder) {
    ResidentBuffer::FlushChanged(context);
    if (!uploaded || committed) return;
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    std::vector<Region*> copies;
    for (auto& region : regions) {
        if (!region.gpuCopy || !region.deviceLocal) continue;
        // The previous use's copy-back was recorded by its MarkGpuWrites (or never, when that use
        // threw first: counted); this use records its own after its work, from the shadow this
        // copy-in refills. The import is the one the region was taken from (the caller checked its
        // serial), so the copy source still stands.
        if (!region.copiedBack && profile) Copies().stagedLost.fetch_add(1, std::memory_order_relaxed);
        region.copiedBack = false;
        copies.push_back(&region);
    }
    if (copies.empty()) return;
    Require(Recorder::Active() == &recorder, "staging copies recorded into a recorder that is not the device's");
    if (profile) Copies().stagedReused.fetch_add(copies.size(), std::memory_order_relaxed);
    recordGpuCopies(copies, false);
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

bool HostImportsUnchanged(const Context& context, const HostImportsProof& proof) {
    if (proof.device == VK_NULL_HANDLE || proof.device != context.device) return false;
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    return state.device == proof.device && state.epoch == proof.epoch && state.refreshedGeneration == proof.refreshedGeneration && !importsStale(context, state);
}

HostImportsProof HostImportsIdentity(const Context& context) {
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    if (importsStale(context, state)) return {};
    return {state.device, state.epoch, state.refreshedGeneration};
}

}
