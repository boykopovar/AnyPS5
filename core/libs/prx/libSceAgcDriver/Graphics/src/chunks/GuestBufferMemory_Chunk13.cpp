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
void GuestBufferMemory::AddSnapshot(const GuestMemorySnapshot& snapshot) {
    validate(snapshot.address, snapshot.bytes.size());
    const auto end = snapshot.address + snapshot.bytes.size();
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
        static const bool checkLive = std::getenv("APS5_NO_SNAPSHOT_CHECK") == nullptr;
        static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        const bool live = owner->writable || owner->hostBacked || owner->mirror != nullptr;
        if (live && (!checkLive || snapshot.compared || AnyShadowedOverlaps(snapshot.address, snapshot.bytes.size()))) {
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

std::atomic<std::uint64_t> importUs{0};
std::atomic<std::uint64_t> allocateUs{0};
std::atomic<std::uint64_t> readUs{0};
std::atomic<std::uint64_t> uploadsProfiled{0};

std::uint64_t microsecondsSince(std::chrono::steady_clock::time_point start) {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count());
}

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

bool changedCopyBacks(const Context& context) {
    static const bool disabled = std::getenv("APS5_NO_CHANGED_COPY_BACK") != nullptr;
    return !disabled && (context.pushDescriptors || context.bufferDeviceAddress);
}

bool addressedChangedCopyBacks(const Context& context) {
    return changedCopyBacks(context) && !context.pushDescriptors;
}

VkBufferUsageFlags gpuCopyUsage(const Context& context, bool addressable) {
    return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | (addressable || addressedChangedCopyBacks(context) ? VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT : 0u);
}

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

constexpr auto StagingCopyInKey = Recorder::CommandClass::StagingIn;
constexpr auto StagingCopyBackKey = Recorder::CommandClass::StagingOut;

struct CopyStats {
    std::atomic<std::uint64_t> gpuCopies{0};
    std::atomic<std::uint64_t> gpuCopyBytes{0};
    std::atomic<std::uint64_t> gpuCopyBacks{0};
    std::atomic<std::uint64_t> gpuCopyBackBytes{0};
    std::atomic<std::uint64_t> stagingStores{0};
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

std::atomic<bool> deviceLocalFullForTests{false};

std::atomic<std::uint64_t> liveShadowBytes{0};
std::atomic<std::uint64_t> liveShadowLimit{std::numeric_limits<std::uint64_t>::max()};

std::uint64_t shadowBytesLimit(std::uint64_t heapBytes) {
    static const char* const setting = std::getenv("APS5_STAGING_SHADOW_MAX_MIB");
    if (setting != nullptr) return std::strtoull(setting, nullptr, 10) << 20u;
    return std::max<std::uint64_t>(std::uint64_t{256} << 20u, heapBytes / 16u);
}

bool deviceLocalHasRoom(const Context& context, std::size_t bytes) {
    if (deviceLocalFullForTests.load(std::memory_order_relaxed)) return false;
    static std::atomic<std::int64_t> checkedMs{std::numeric_limits<std::int64_t>::min()};
    static std::atomic<std::uint64_t> room{std::numeric_limits<std::uint64_t>::max()};
    static std::atomic<VkDevice> checkedDevice{VK_NULL_HANDLE};
    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (const auto last = checkedMs.load(std::memory_order_relaxed); last == std::numeric_limits<std::int64_t>::min() || nowMs - last >= 250 || checkedDevice.load(std::memory_order_relaxed) != context.device) {
        checkedDevice.store(context.device, std::memory_order_relaxed);
        VkPhysicalDeviceMemoryBudgetPropertiesEXT reported{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_BUDGET_PROPERTIES_EXT};
        VkPhysicalDeviceMemoryProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2, &reported};
        if (context.memoryProperties2 != nullptr && context.physical != VK_NULL_HANDLE) context.memoryProperties2(context.physical, &properties);
        const auto heap = context.memory.memoryTypes[context.MemoryType(~0u, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)].heapIndex;
        const auto budget = reported.heapBudget[heap];
        const auto usage = reported.heapUsage[heap];
        room.store(budget == 0 ? std::numeric_limits<std::uint64_t>::max() : usage + budget / 8u < budget ? budget - budget / 8u - usage : 0u, std::memory_order_relaxed);
        liveShadowLimit.store(shadowBytesLimit(budget != 0 ? budget : context.memory.memoryHeaps[heap].size), std::memory_order_relaxed);
        checkedMs.store(nowMs, std::memory_order_relaxed);
    }
    return bytes <= room.load(std::memory_order_relaxed);
}

std::shared_ptr<Buffer> stagingBuffer(const Context& context, std::size_t bytes, VkBufferUsageFlags usage) {
    if (!deviceLocalHasRoom(context, bytes) || liveShadowBytes.load(std::memory_order_relaxed) + bytes > liveShadowLimit.load(std::memory_order_relaxed)) {
        Copies().stagingRefused.fetch_add(1, std::memory_order_relaxed);
        return nullptr;
    }
    try {
        if (auto buffer = Buffer::WithinDeviceLocalHeadroom(context, bytes, usage)) {
            liveShadowBytes.fetch_add(bytes, std::memory_order_relaxed);
            auto* shadow = buffer.get();
            return std::shared_ptr<Buffer>(shadow, [owner = std::move(buffer), bytes](Buffer*) mutable {
                owner.reset();
                liveShadowBytes.fetch_sub(bytes, std::memory_order_relaxed);
            });
        }
        static std::atomic<int> reported{0};
        if (reported.fetch_add(1, std::memory_order_relaxed) < 4) std::fprintf(stderr, "[buffers] staging shadow of %zu bytes refused: its device-local heap is within an eighth of its budget\n", bytes);
        Copies().stagingRefused.fetch_add(1, std::memory_order_relaxed);
        return nullptr;
    } catch (const std::exception& error) {
        static std::atomic<int> reported{0};
        if (reported.fetch_add(1, std::memory_order_relaxed) < 4) std::fprintf(stderr, "[buffers] staging shadow of %zu bytes refused: %s\n", bytes, error.what());
        Copies().stagingRefused.fetch_add(1, std::memory_order_relaxed);
        return nullptr;
    }
}

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
    static std::uint64_t lastStaged = 0, lastIn = 0, lastOut = 0;
    const auto staged = stats.staged.load();
    const auto in = stats.stagedInBytes.load();
    const auto out = stats.stagedOutBytes.load();
    AgcDriver::ProfilePrint_nid_no_patch("[buffers] staging: %llu regions staged device-local (%llu with atomics, %llu of reused builds), %.0f KiB copied in, %.0f KiB copied back, %llu without copy-back, %llu shadows refused, %llu MiB in live shadows (limit %llu MiB); last 10 s: %llu regions, %.0f KiB in, %.0f KiB back\n", static_cast<unsigned long long>(staged), static_cast<unsigned long long>(stats.stagedAtomic.load()), static_cast<unsigned long long>(stats.stagedReused.load()), in / 1024.0, out / 1024.0, static_cast<unsigned long long>(stats.stagedLost.load()), static_cast<unsigned long long>(stats.stagingRefused.load()), static_cast<unsigned long long>(liveShadowBytes.load() >> 20u), static_cast<unsigned long long>(liveShadowLimit.load() >> 20u), static_cast<unsigned long long>(staged - lastStaged), (in - lastIn) / 1024.0, (out - lastOut) / 1024.0);
    lastStaged = staged;
    lastIn = in;
    lastOut = out;
}

}

void ShutdownGuestBufferWorkers() {
    RefreshPool::Shutdown();
}

GuestBufferMemory::~GuestBufferMemory() {
    if (space != nullptr && spaceHeldThrough != 0 && retention != nullptr) {
        retention->device = space->device;
        retention->recorder = spaceHeldBy;
        retention->serial = spaceHeldThrough;
        retention->space = std::move(space);
        pushRetained(retention.release());
    }
    releaseHeapReferences();
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!profile) return;
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
    std::vector<bool> drop(regions.size(), false);
    if (space == nullptr) {
        for (const auto member : members) drop[member] = true;
    }
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
            previous.begin = std::min(previous.begin, region.begin);
            if (previous.hostBacked || region.hostBacked) {
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
        if (region.mirror != nullptr || region.direct != nullptr || !mirrorsEnabled()) continue;
        auto mirror = findMirror(context, region.begin, region.end);
        if (mirror == nullptr) {
            const auto range = fixedImageRange(region.begin, region.end);
            if (range != nullptr && (region.begin - range->address) % context.limits.minStorageBufferOffsetAlignment == 0) {
                std::vector<RefreshBlock> unused;
                mirror = acquireMirror(context, range, unused, false);
                if (mirror != nullptr) sweepMirrors();
            }
        }
        if (mirror == nullptr || (region.begin - mirror->base) % context.limits.minStorageBufferOffsetAlignment != 0) continue;
        region.pending = mirror->writable;
        region.mirror = std::move(mirror);
        region.subrangeMirror = true;
    }
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
            region.adaptiveInPlace = Recorder::Active() != nullptr && stagingWanted(region, addressable) && adaptiveInPlace(region, true);
            bool staged = Recorder::Active() != nullptr && stagingEligible(region, addressable);
            const bool misaligned = !bindableInPlace(HostImportOffset(*entry, region.begin), addressable);
            if (staged) {
                const auto allocateStart = std::chrono::steady_clock::now();
                region.buffer = stagingBuffer(context, static_cast<std::size_t>(region.end - region.begin), gpuCopyUsage(context, addressable));
                if (profile) allocateUs.fetch_add(microsecondsSince(allocateStart), std::memory_order_relaxed);
                if (region.buffer == nullptr) {
                    region.unstaged = true;
                    staged = false;
                }
            }
            if (misaligned || staged) {
                if (!staged) {
                    if (!gpuCopyEligible(region)) continue;
                    const auto allocateStart = std::chrono::steady_clock::now();
                    region.buffer = std::make_shared<Buffer>(context, static_cast<std::size_t>(region.end - region.begin), gpuCopyUsage(context, addressable), copyBufferProperties(false));
                    if (profile) allocateUs.fetch_add(microsecondsSince(allocateStart), std::memory_order_relaxed);
                }
                region.deviceLocal = staged;
                region.direct = entry;
                region.pending = true;
                continue;
            }
            region.direct = entry;
            region.pending = true;
        }
        importsEpoch = state.epoch;
    }
    if (profile) importUs.fetch_add(microsecondsSince(started), std::memory_order_relaxed);
    for (auto& region : regions) {
        if (region.pending || region.mirror != nullptr || region.direct != nullptr) continue;
        if (sparseHeapCandidate(region) || (region.writable && WritesOverlap(region.begin, static_cast<std::size_t>(region.end - region.begin)))) {
            region.pending = true;
            continue;
        }
        copyRegion(region, addressable);
    }
}

}
}
}
}
}
}
}
