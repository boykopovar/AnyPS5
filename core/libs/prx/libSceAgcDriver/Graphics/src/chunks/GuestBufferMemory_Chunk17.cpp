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
namespace {
namespace {
namespace {
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

bool GuestBufferMemory::addressWritable(const Region& region) {
    return region.direct != nullptr && region.writable;
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

bool GuestBufferMemory::InPlaceReadsOverlap(std::uint64_t address, std::size_t bytes) const {
    if (!uploaded || committed) return false;
    const auto end = address + bytes;
    const auto overlaps = [&](const auto& region, std::uint64_t regionEnd) { return region.direct != nullptr && address < regionEnd && region.begin < end; };
    if (space != nullptr && std::any_of(space->base.begin(), space->base.end(), [&](const auto& region) { return overlaps(region, region.end); })) return true;
    return std::any_of(regions.begin(), regions.end(), [&](const auto& region) { return overlaps(region, inPlaceEnd(region)); });
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

void GuestBufferMemory::AddressStores(std::vector<std::pair<std::uint64_t, std::uint64_t>>& result) const {
    result.clear();
    if (!uploaded || committed) return;
    if (space != nullptr) {
        for (const auto& region : space->base) {
            if (addressWritable(region)) result.emplace_back(region.begin, inPlaceEnd(region));
        }
    }
    for (const auto& region : regions) {
        if (addressWritable(region)) result.emplace_back(region.begin, inPlaceEnd(region));
    }
}

void GuestBufferMemory::RecordStagingCopies(Recorder& recorder) {
    ResidentBuffer::FlushChanged(context);
    if (!uploaded || committed) return;
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    std::vector<Region*> copies;
    if (std::any_of(regions.begin(), regions.end(), [](const Region& region) { return region.gpuCopy && region.deviceLocal && !region.copiedBack; })) recorder.CloseRunOwing(this);
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

bool TouchHostImportsIfUnchanged(const Context& context, const HostImportsProof& proof, std::span<const HostImport* const> imports) {
    if (proof.device == VK_NULL_HANDLE || proof.device != context.device) return false;
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    if (state.device != proof.device || state.epoch != proof.epoch || state.refreshedGeneration != proof.refreshedGeneration || importsStale(context, state)) return false;
    for (const auto* import : imports) import->lastUse = ++state.useCounter;
    return true;
}

bool HostImportsReconciledUnchanged(const Context& context, const HostImportsProof& proof) {
    if (proof.device == VK_NULL_HANDLE || proof.device != context.device) return false;
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    if (state.device != proof.device) return false;
    if (GuestAllocations::GuestAllocationsGeneration_nid_postfix() != state.refreshedGeneration) refreshImports(context, state, GuestAllocations::GuestAllocationsAcquire_nid_postfix());
    return state.device == proof.device && state.epoch == proof.epoch && state.inserted == proof.inserted;
}

std::uint64_t RetiredImportSerial() {
    return retiredImports.load(std::memory_order_acquire);
}

void SetDeviceLocalFullForTests(bool full) {
    deviceLocalFullForTests.store(full, std::memory_order_relaxed);
}

std::uint64_t StagingShadowsRefused() {
    return Copies().stagingRefused.load(std::memory_order_relaxed);
}

std::uint64_t ChangedCopySnapshotBytes() {
    return changedCopySnapshotBytes.load(std::memory_order_relaxed);
}

std::uint64_t ChangedCopySnapshotsMade() {
    return changedCopySnapshotsMade.load(std::memory_order_relaxed);
}

HostImportsProof HostImportsIdentity(const Context& context) {
    auto& state = Imports();
    std::lock_guard lock(state.mutex);
    if (importsStale(context, state)) return {};
    return {state.device, state.epoch, state.refreshedGeneration, state.inserted};
}

}
}
}
}
}
}
}
}
}
}
}
