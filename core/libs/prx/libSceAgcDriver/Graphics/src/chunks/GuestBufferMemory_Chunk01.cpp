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
    std::vector<std::byte> shadow;
    std::uint64_t serial = 0;
    std::atomic<std::uint64_t> watchedSerial{0};
    std::uint64_t lastDispatch = 0;
};

struct GuestBufferMemory::AddressSpace {
    std::uint64_t generation = 0;
    std::uint64_t importsEpoch = 0;
    VkDevice device = VK_NULL_HANDLE;
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
    mutable std::atomic<std::uint64_t> refreshedEpoch{0};
    mutable std::atomic<std::uint64_t> refreshedProof{0};
    std::vector<CopiedRange> copied;
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
void clearCarriedShadows(VkDevice device);
struct RefreshBlock;
static thread_local GuestBufferMemory* buildingBufferMemory = nullptr;
static thread_local std::vector<RefreshBlock>* buildingBlocks = nullptr;

std::atomic<std::uint64_t> importSetGeneration{1};

struct RetiredImport;

std::atomic<std::uint64_t> retiredPinnedBytes{0};
std::atomic<bool> retiredPinnedReleased{false};

struct AddressSpaceCache {
    AtomicSharedPtr<const GuestBufferMemory::AddressSpace> current;
    std::atomic<std::uint64_t> serials{0};
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
    std::atomic<std::uint64_t> patchedEpoch{0};
    std::atomic<std::uint64_t> rebuiltGrowth{0};
};

AddressSpaceCache& Spaces() {
    static AddressSpaceCache cache;
    return cache;
}

std::atomic<RetainedAddressSpace*> retainedSpaces{nullptr};

void pushRetained(RetainedAddressSpace* node) noexcept {
    node->next = retainedSpaces.load(std::memory_order_acquire);
    while (!retainedSpaces.compare_exchange_weak(node->next, node, std::memory_order_acq_rel, std::memory_order_acquire)) {}
}

template <typename Done>
void pruneRetained(Done done) {
    auto* node = retainedSpaces.exchange(nullptr, std::memory_order_acq_rel);
    while (node != nullptr) {
        auto* const next = node->next;
        if (done(*node)) delete node;
        else pushRetained(node);
        node = next;
    }
}

bool addressSpaceCacheEnabled() {
    static const bool disabled = std::getenv("APS5_NO_ADDRESS_SPACE_CACHE") != nullptr;
    return !disabled;
}

struct DmaBufImportFailure : std::runtime_error {
    using std::runtime_error::runtime_error;
};

struct ImportFailure {
    std::uint64_t base;
    std::uint64_t bytes;
    GuestAllocations::Mapped ranges;
};

struct ImportRefusal {
    std::uint64_t base;
    std::uint64_t bytes;
    GuestAllocations::Mapped ranges;
    std::uint64_t pinnedBytes;
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
    std::uint64_t pinnedBytes;
    std::uint64_t budget;
    std::uint64_t room;
    std::uint64_t window;
    std::uint64_t floor;
    std::uint64_t ready;
    std::uint64_t readyHeld;
    bool joined;
    bool immediate;
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

const bool importsLockOrderChecked = MapImportGate::LockOrderChecked();

thread_local std::uint32_t importsHeld = 0;

class ImportsMutex {
public:
    void lock() noexcept {
        mutex.lock();
        if (importsLockOrderChecked) ++importsHeld;
    }

    bool try_lock() noexcept {
        if (!mutex.try_lock()) return false;
        if (importsLockOrderChecked) ++importsHeld;
        return true;
    }

    void unlock() noexcept {
        if (importsLockOrderChecked) --importsHeld;
        mutex.unlock();
    }

private:
    AgcDriver::Mutex mutex;
};

struct RefreshedRange {
    std::uint64_t address;
    std::uint64_t bytes;
    std::weak_ptr<const GuestAllocations::Range> range;
};

enum class GrowthBlock : std::uint8_t { Untried, Held, Budget, Failed };

struct GrowthRun {
    std::uint64_t end = 0;
    GrowthBlock blocked = GrowthBlock::Untried;
    std::uint64_t budget = 0;
    std::uint64_t pinnedBytes = 0;
};

struct HostImports {
    ImportsMutex mutex;
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
    std::uint64_t refreshedGeneration = 0;
    std::vector<RefreshedRange> refreshedRanges;
    std::uint64_t refreshedWindow = 0;
    std::set<std::uint64_t> pendingImports;
    std::map<std::uint64_t, GrowthRun> growth;
    std::uint64_t growthFloor = std::numeric_limits<std::uint64_t>::max();
    bool growthDue = false;
    std::uint64_t budgetChecks = 0;
    std::uint64_t epoch = 1;
    std::uint64_t inserted = 0;
    VkDevice watchDevice = VK_NULL_HANDLE;
    bool unwatchImports = false;
    bool unwatchDmaBufImports = false;
    bool dmaBufImportsCoherent = true;
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
    bool dmaBufSelected = false;
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

void importSetChanged() {
    importSetGeneration.fetch_add(1, std::memory_order_release);
}

void bumpEpoch(HostImports& state) {
    ++state.epoch;
    importSetChanged();
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
        if (!pinned) return;
        retiredPinnedBytes.fetch_sub(entry.pinnedBytes, std::memory_order_acq_rel);
        retiredPinnedReleased.store(true, std::memory_order_release);
    }
    void Pin() {
        if (pinned) return;
        pinned = true;
        retiredPinnedBytes.fetch_add(node.mapped().pinnedBytes, std::memory_order_acq_rel);
    }
    VkDevice device;
    PFN_vkDestroyBuffer destroyBuffer;
    PFN_vkFreeMemory freeMemory;
    ImportNode node;
    bool pinned = false;
};

std::uint64_t pinnedTotal(const HostImports& state) {
    return state.liveBytes + retiredPinnedBytes.load(std::memory_order_acquire);
}

bool holdsRangeOver(const HostImport& entry, std::uint64_t begin, std::uint64_t end) {
    const auto over = [&](const std::shared_ptr<const GuestAllocations::Range>& range) { return range != nullptr && range->address < end && begin < range->address + range->bytes; };
    return (entry.base < end && begin < entry.base + entry.bytes) || over(entry.range) || std::any_of(entry.ranges.begin(), entry.ranges.end(), over);
}

const GuestAllocations::Range* containingRange(const GuestAllocations::Lease& lease, std::uint64_t begin, std::uint64_t end);
bool runCovers(const GuestAllocations::Lease& lease, std::uint64_t begin, std::uint64_t end);
bool joinedCovers(const GuestAllocations::Lease& lease, std::uint64_t begin, std::uint64_t end);

bool retireFreesAtOnce(bool hold) {
    const auto* recorder = Recorder::Active();
    return recorder == nullptr || (!hold && recorder->Idle());
}

std::atomic<std::uint64_t> retiredImports{0};

void retireImport(VkDevice device, HostImports& state, std::map<std::uint64_t, HostImport>::iterator it, const GuestAllocations::Lease& lease, bool hold = true) {
#ifdef _WIN32
    auto* reading = forgetSettle(state, it->first);
#endif
    state.pendingImports.erase(it->first);
    RetireShadow(device, it->second, [&lease, joined = it->second.joined](std::uint64_t begin, std::uint64_t end) { return runCovers(lease, begin, end) || (joined && joinedCovers(lease, begin, end)); });
    bumpEpoch(state);
    retiredImports.fetch_add(1, std::memory_order_release);
    state.liveBytes -= it->second.pinnedBytes;
    auto holder = std::make_shared<RetiredImport>(state.device, state.destroyBuffer, state.freeMemory, state.imports.extract(it));
#ifdef _WIN32
    if (reading != nullptr) {
        holder->Pin();
        reading->retired.push_back(holder);
        ++state.settlesParked;
    }
#endif
    if (retireFreesAtOnce(hold)) return;
    auto* recorder = Recorder::Active();
    if (hold) {
        holder->Pin();
        recorder->KeepThroughHold(std::move(holder));
    } else {
        holder->Pin();
        recorder->Keep(std::move(holder));
    }
}

bool releaseParkedImports(HostImports& state, std::uint64_t begin, std::uint64_t end) {
    std::vector<std::shared_ptr<RetiredImport>> released;
    {
        std::lock_guard lock(state.mutex);
        for (auto device = state.parked.begin(); device != state.parked.end();) {
            auto& list = device->second;
            for (auto it = list.begin(); it != list.end();) {
                if (holdsRangeOver((*it)->node.mapped(), begin, end)) {
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
                const bool overlaps = std::any_of(batch.retired.begin(), batch.retired.end(), [&](const std::shared_ptr<RetiredImport>& holder) { return holdsRangeOver(holder->node.mapped(), begin, end); });
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

std::optional<std::uint32_t> coherentImportType(const Context& context, std::uint32_t types) {
    constexpr VkMemoryPropertyFlags coherent = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    std::optional<std::uint32_t> plain;
    for (std::uint32_t i = 0; i < context.memory.memoryTypeCount; ++i) {
        if ((types & (1u << i)) == 0) continue;
        const auto flags = context.memory.memoryTypes[i].propertyFlags;
        if ((flags & coherent) != coherent) continue;
        if ((flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0) return i;
        if (!plain.has_value()) plain = i;
    }
    return plain;
}

const char* bindImport(const Context& context, HostImport& entry, VkExternalMemoryHandleTypeFlagBits handleType, const void* import, std::uint32_t importTypes, bool& allocated, VkResult& failure, bool hostCoherent = false) {
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
    std::uint32_t type = static_cast<std::uint32_t>(std::countr_zero(types));
    if (hostCoherent && context.memory.memoryTypeCount != 0) {
        const auto coherent = coherentImportType(context, types);
        if (!coherent.has_value()) return failed("memory type selection without host coherency", VK_ERROR_FORMAT_NOT_SUPPORTED);
        type = *coherent;
    }
    const VkMemoryAllocateFlagsInfo flags{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, import, VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, 0};
    VkMemoryAllocateInfo allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO, &flags};
    allocation.allocationSize = entry.bytes;
    allocation.memoryTypeIndex = type;
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
    if (!context.hostPointerImport) {
        failure = VK_ERROR_EXTENSION_NOT_PRESENT;
        return "VK_EXT_external_memory_host";
    }
    void* const host = entry.alias != nullptr ? entry.alias : reinterpret_cast<void*>(entry.base);
    VkMemoryHostPointerPropertiesEXT pointer{VK_STRUCTURE_TYPE_MEMORY_HOST_POINTER_PROPERTIES_EXT};
    if (const auto result = context.Function<PFN_vkGetMemoryHostPointerPropertiesEXT>("vkGetMemoryHostPointerPropertiesEXT")(context.device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, host, &pointer); result != VK_SUCCESS) {
        failure = result;
        return "vkGetMemoryHostPointerPropertiesEXT";
    }
    const VkImportMemoryHostPointerInfoEXT import{VK_STRUCTURE_TYPE_IMPORT_MEMORY_HOST_POINTER_INFO_EXT, nullptr, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, host};
    bool allocated = false;
    return bindImport(context, entry, VK_EXTERNAL_MEMORY_HANDLE_TYPE_HOST_ALLOCATION_BIT_EXT, &import, pointer.memoryTypeBits, allocated, failure, true);
}

#ifndef _WIN32
int udmabufDevice() {
#ifdef __APPLE__
    return -1;
#endif
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

struct OwnedDescriptor {
    explicit OwnedDescriptor(int descriptor) : file(descriptor) {}
    OwnedDescriptor(const OwnedDescriptor&) = delete;
    OwnedDescriptor& operator=(const OwnedDescriptor&) = delete;
    ~OwnedDescriptor() {
        if (file >= 0) close(file);
    }
    int Release() { return std::exchange(file, -1); }
    int file;
};

constexpr std::size_t DmaBufRunLimit = 1024;

const char* createDmaBufImport(const Context& context, HostImport& entry, int file, std::span<const GuestArena::SharedBackingRun> runs, VkResult& failure) {
    OwnedDescriptor owned(file);
    const int device = udmabufDevice();
    int buffer = -1;
    if (device >= 0 && runs.size() == 1) {
        udmabuf_create request{};
        request.memfd = static_cast<std::uint32_t>(file);
        request.flags = UDMABUF_FLAGS_CLOEXEC;
        request.offset = runs[0].offset;
        request.size = runs[0].bytes;
        buffer = ioctl(device, UDMABUF_CREATE, &request);
    } else if (device >= 0) {
        std::vector<std::byte> storage(sizeof(udmabuf_create_list) + runs.size() * sizeof(udmabuf_create_item));
        auto* request = reinterpret_cast<udmabuf_create_list*>(storage.data());
        request->flags = UDMABUF_FLAGS_CLOEXEC;
        request->count = static_cast<std::uint32_t>(runs.size());
        for (std::size_t index = 0; index < runs.size(); ++index) request->list[index] = udmabuf_create_item{static_cast<std::uint32_t>(file), 0, runs[index].offset, runs[index].bytes};
        buffer = ioctl(device, UDMABUF_CREATE_LIST, request);
    }
    close(owned.Release());
    if (buffer < 0) {
        failure = VK_ERROR_INVALID_EXTERNAL_HANDLE;
        return device < 0 ? "open /dev/udmabuf" : runs.size() == 1 ? "UDMABUF_CREATE" : "UDMABUF_CREATE_LIST";
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
constexpr VkBufferUsageFlags ChunkImportUsage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
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

}
}
