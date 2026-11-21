#include "prx/libSceAgcDriver/Execution/include/AsyncLog.hpp"
#include "prx/libc/include/HostThread.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileCounter.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libc/include/HostThreadSlot.hpp"
#include "prx/libSceAgcDriver/Execution/include/ThreadScratch.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BdaResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ReferenceStorage.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libkernel/Time/include/SleepWake.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ParallelCompare.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DeferredDestruction.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/SubmissionSnapshots.hpp"
#include "prx/libSceAgcDriver/Graphics/shaders/MeshArguments_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/StagingCopyBack_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/StagingCopyBackBound_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/SampleCounter_spv.h"
#include "prx/libSceAgcDriver/Graphics/shaders/SampleDumps_spv.h"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceOverlay.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4Opcodes.hpp"
#include "prx/libc/include/CpuTopology.hpp"
#include "prx/libc/include/RunReport.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libSceAgcDriver/Graphics/shaders/DispatchArguments_spv.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>

#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <mutex>
#include <new>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <unordered_map>
#include "prx/libc/include/general/AtomicSharedPtr.hpp"

namespace AgcDriver::Graphics {
namespace {
namespace {
namespace {
namespace {
namespace {
std::size_t Recorder::UnsignaledBatches() const {
    return static_cast<std::size_t>(std::count_if(inFlight.begin(), inFlight.end(), [&](const auto& batch) { return !signaled(*batch); }));
}

std::shared_ptr<DeviceBuffer> Recorder::UploadScratchBuffer(UploadScratch kind, std::size_t bytes) {
    Require(kind < UploadScratch::Count && bytes != 0, "invalid texture upload scratch request");
    ensureOpen();
    auto& scratch = open->uploadScratch[static_cast<std::size_t>(kind)];
    if (scratch == nullptr || scratch->Size() < bytes) {
        const VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | (kind == UploadScratch::Tiled ? VK_BUFFER_USAGE_TRANSFER_DST_BIT : VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        scratch = std::make_shared<DeviceBuffer>(context, bytes, usage);
        Keep(scratch, bytes);
    }
    return scratch;
}

void Recorder::Keep(std::shared_ptr<void> object, std::size_t bytes) {
    ensureOpen();
    open->kept.push_back(std::move(object));
    open->keptBytes += bytes;
}

void Recorder::KeepThroughHold(std::shared_ptr<void> object) {
    GuestMemory::AssertGpuLockHeld("Recorder::KeepThroughHold");
    if (const auto hold = GuestMemory::GpuHoldSerial(); hold != holdKeptSerial) {
        holdKept.clear();
        holdKeptSerial = hold;
    }
    Keep(object);
    holdKept.push_back(std::move(object));
}

void Recorder::dropStaleHoldKept() {
    if (!holdKept.empty() && holdKeptSerial != GuestMemory::GpuHoldSerial() && GuestMemory::GpuMutex().HeldByThisThread()) holdKept.clear();
}

void Recorder::KeepBytes(const void* owner, std::size_t bytes) {
    if (owner == nullptr || bytes == 0) return;
    ensureOpen();
    if (open->keptOwners.insert(owner).second) open->keptBytes += bytes;
}

void Recorder::BoundKeptBytes() {
    if (!GuestMemory::GpuMutex().HeldByThisThread()) return;
    if (open != nullptr && open->keptBytes >= KeptBytesBudget) Submit();
    if (DeferredBytes() != 0 && inFlightKeptBytes + DeferredBytes() > 2 * KeptBytesBudget) DestroyDeferred(TakeDeferred(), false);
    while (!inFlight.empty() && inFlightKeptBytes > 2 * KeptBytesBudget) {
        auto batch = std::move(inFlight.front());
        inFlight.pop_front();
        finish(std::move(batch), true, 4, true);
    }
}

namespace {
std::size_t SnapshotPool(Recorder::SnapshotUse use) {
    return use == Recorder::SnapshotUse::Storage ? 0 : 1;
}
}

void Recorder::dropDrawInputMirror(std::map<DrawSnapshotKey, DrawSnapshot>::iterator entry) {
    if (entry->second.mirror == nullptr) return;
    drawSnapshotPools[SnapshotPool(std::get<1>(entry->first))].bytes -= std::get<2>(entry->first);
    drawInputMirrorBytes -= std::get<2>(entry->first);
    entry->second.mirror.reset();
}

void Recorder::eraseDrawSnapshot(std::map<DrawSnapshotKey, DrawSnapshot>::iterator entry) {
    dropDrawInputMirror(entry);
    auto& pool = drawSnapshotPools[SnapshotPool(std::get<1>(entry->first))];
    pool.bytes -= std::get<2>(entry->first);
    pool.recency.erase(entry->second.recent);
    if (!entry->second.owned.words.empty()) --ownedDrawInputs;
    drawSnapshots.erase(entry);
}

bool Recorder::DrawInputMirrors() {
    static const bool enabled = std::getenv("APS5_NO_INPUT_MIRRORS") == nullptr;
    return enabled;
}

std::pair<std::size_t, std::size_t> Recorder::DrawInputBytes() const {
    return {drawSnapshotPools[SnapshotPool(SnapshotUse::Vertex)].bytes, drawInputMirrorBytes};
}

std::shared_ptr<Buffer> Recorder::mirrorDrawInput(std::map<DrawSnapshotKey, DrawSnapshot>::iterator entry) {
    const auto bytes = std::get<2>(entry->first);
    auto& pool = drawSnapshotPools[SnapshotPool(std::get<1>(entry->first))];
    if (bytes > DrawInputMirrorBudget || 2 * bytes > DrawInputBudget || !context.HostMemoryType(~0u)) return {};
    const auto host = entry->second.buffer;
    const auto copied = host->Bytes().size();
    auto mirror = Buffer::WithinDeviceLocalHeadroom(context, copied, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    if (mirror == nullptr) return {};
    for (auto oldest = pool.recency.begin(); drawInputMirrorBytes + bytes > DrawInputMirrorBudget && oldest != pool.recency.end(); ++oldest) {
        if (*oldest != entry->first) dropDrawInputMirror(drawSnapshots.find(*oldest));
    }
    while (pool.bytes + bytes > DrawInputBudget && pool.recency.front() != entry->first) eraseDrawSnapshot(drawSnapshots.find(pool.recency.front()));
    const auto commands = Commands();
    const auto timing = BeginGpuTiming(CommandClass::InputMirror);
    CopyBuffer(context, commands, host->Handle(), 0, mirror->Handle(), 0, copied);
    RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT);
    CountBarriers(CommandClass::InputMirror);
    EndGpuTiming(timing, copied);
    Keep(host);
    Keep(mirror);
    pool.bytes += bytes;
    drawInputMirrorBytes += bytes;
    return mirror;
}

Recorder::UploadSlice Recorder::DrawUpload(std::size_t bytes) {
    const VkDeviceSize alignment = std::max<VkDeviceSize>(context.limits.minStorageBufferOffsetAlignment, 16);
    auto offset = (drawUploadUsed + alignment - 1) & ~(alignment - 1);
    if (drawUpload == nullptr || offset + bytes > drawUpload->Bytes().size()) {
        drawUpload = std::make_shared<Buffer>(context, std::max(DrawUploadChunk, bytes), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        offset = 0;
    }
    drawUploadUsed = offset + bytes;
    return {drawUpload, offset, drawUpload->Bytes().data() + offset};
}

void Recorder::retireUnwrittenSnapshots(const std::array<std::uint64_t, 3>& stamp) {
    if (stamp == unwrittenStamp) return;
    unwrittenSnapshots.clear();
    unwrittenBytes = 0;
    unwrittenStamp = stamp;
}

std::shared_ptr<Buffer> Recorder::UnwrittenDrawSnapshot(std::uint64_t address, std::size_t bytes, const std::array<std::uint64_t, 3>& stamp, VkDeviceSize* offset) {
    retireUnwrittenSnapshots(stamp);
    const auto found = unwrittenSnapshots.find(address);
    if (found == unwrittenSnapshots.end() || found->second.bytes != bytes) return {};
    if (offset != nullptr) *offset = found->second.offset;
    return found->second.buffer;
}

void Recorder::NoteUnwrittenDrawSnapshot(std::uint64_t address, std::size_t bytes, const std::array<std::uint64_t, 3>& stamp, std::shared_ptr<Buffer> buffer, VkDeviceSize offset) {
    retireUnwrittenSnapshots(stamp);
    if (const auto found = unwrittenSnapshots.find(address); found != unwrittenSnapshots.end()) {
        unwrittenBytes -= found->second.bytes;
        unwrittenSnapshots.erase(found);
    }
    if (unwrittenSnapshots.size() >= UnwrittenSnapshotEntries || bytes > DrawSnapshotBudget - unwrittenBytes) return;
    unwrittenSnapshots.emplace(address, UnwrittenSnapshot{bytes, std::move(buffer), offset});
    unwrittenBytes += bytes;
}

std::shared_ptr<Buffer> Recorder::ReusableDrawSnapshot(std::uint64_t address, std::size_t bytes, SnapshotUse use, std::uint32_t* derived, std::uint64_t generation, VkDeviceSize* offset, std::size_t from, std::uint32_t* lowest, bool mirrored) {
    Require(from == 0 || (use == SnapshotUse::Vertex && from < bytes), "only a vertex snapshot starts past its address");
    auto found = use == SnapshotUse::Vertex ? drawSnapshots.lower_bound({address, use, bytes}) : drawSnapshots.find({address, use, bytes});
    if (found == drawSnapshots.end() || std::get<0>(found->first) != address || std::get<1>(found->first) != use) {
        if (use != SnapshotUse::Storage || offset == nullptr) return {};
        return containingDrawSnapshot(address, bytes, offset);
    }
    if (offset != nullptr) *offset = 0;
    if (!found->second.owned.words.empty()) {
        eraseDrawSnapshot(found);
        return {};
    }
    const bool sameRegistry = found->second.registryGeneration == GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    const auto guestBytes = use == SnapshotUse::PaddedVertex ? static_cast<std::size_t>(found->second.derived) : bytes;
    const auto held = std::max(from, found->second.from);
    if (!sameRegistry || (held < guestBytes && !GuestMemory::UnchangedSince(address + held, guestBytes - held, found->second.generation))) {
        if (!sameRegistry || generation == 0 || use != SnapshotUse::Storage || !refreshDrawSnapshot(found->second, address, bytes)) {
            eraseDrawSnapshot(found);
            return {};
        }
        found->second.generation = generation;
    }
    if (found->second.from > from) {
        const auto start = from - std::min(from, std::get<2>(found->first) - found->second.from);
        GuestMemory::Read(address + start, found->second.buffer->Bytes().subspan(start, found->second.from - start), 1);
        found->second.from = start;
        dropDrawInputMirror(found);
    }
    auto& recency = drawSnapshotPools[SnapshotPool(use)].recency;
    recency.splice(recency.end(), recency, found->second.recent);
    if (derived != nullptr) *derived = found->second.derived;
    if (lowest != nullptr) *lowest = found->second.lowest;
    if (!mirrored || use == SnapshotUse::Storage || !DrawInputMirrors()) return found->second.buffer;
    if (found->second.mirror == nullptr) found->second.mirror = mirrorDrawInput(found);
    return found->second.mirror != nullptr ? found->second.mirror : found->second.buffer;
}

std::shared_ptr<Buffer> Recorder::containingDrawSnapshot(std::uint64_t address, std::size_t bytes, VkDeviceSize* offset) {
    const VkDeviceSize alignment = std::max<VkDeviceSize>(context.limits.minStorageBufferOffsetAlignment, 4);
    const auto registry = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    auto entry = drawSnapshots.upper_bound({address, SnapshotUse::Storage, std::numeric_limits<std::size_t>::max()});
    for (std::size_t probe = 0; probe < ContainingSnapshotProbes && entry != drawSnapshots.begin(); ++probe) {
        --entry;
        const auto& [start, use, held] = entry->first;
        if (use != SnapshotUse::Storage || start + held < address + bytes || (address - start) % alignment != 0) continue;
        if (entry->second.registryGeneration != registry || !GuestMemory::UnchangedSince(address, bytes, entry->second.generation)) continue;
        auto& recency = drawSnapshotPools[SnapshotPool(use)].recency;
        recency.splice(recency.end(), recency, entry->second.recent);
        return entry->second.buffer;
    }
    return {};
}

bool Recorder::refreshDrawSnapshot(DrawSnapshot& entry, std::uint64_t address, std::size_t bytes) {
    const auto held = entry.buffer->Bytes();
    if (held.size() != bytes || bytes == 0) return false;
    constexpr std::uint64_t block = 65536;
    const auto aligned = address / block * block;
    const auto end = address + bytes;
    const auto count = static_cast<std::size_t>((end - 1) / block - address / block + 1);
    struct Scratch {
        std::vector<std::uint64_t> generations;
        std::vector<std::uint8_t> changed;
        std::vector<std::pair<std::size_t, std::size_t>> differing;
        std::vector<CompareSpan> spans;
        std::vector<std::uint8_t> equal;
    };
    auto& scratch = HostThreadLocal<Scratch, Scratch>();
    auto& generations = scratch.generations;
    auto& changed = scratch.changed;
    auto& differing = scratch.differing;
    auto& spans = scratch.spans;
    auto& equal = scratch.equal;
    generations.assign(count, entry.generation);
    changed.assign(count, GuestMemory::BlockWritten);
    differing.clear();
    spans.clear();
    const bool sole = entry.buffer.use_count() == 1;
    GuestMemory::ChangedBlocks(address, bytes, generations, changed);
    for (std::size_t k = 0; k < count; ++k) {
        if (changed[k] == GuestMemory::BlockUnchanged) continue;
        const auto from = std::max(address, aligned + k * block);
        const auto to = std::min(end, aligned + (k + 1) * block);
        const auto offset = static_cast<std::size_t>(from - address);
        spans.push_back({held.data() + offset, reinterpret_cast<const void*>(from), static_cast<std::size_t>(to - from)});
    }
    equal.assign(spans.size(), 0);
    CompareSpans(spans, equal);
    for (std::size_t k = 0; k < spans.size(); ++k) {
        if (equal[k] != 0) continue;
        const auto offset = static_cast<std::size_t>(static_cast<const std::byte*>(spans[k].first) - held.data());
        const auto length = spans[k].bytes;
        if (!sole) return false;
        if (!differing.empty() && differing.back().first + differing.back().second == offset) differing.back().second += length;
        else differing.emplace_back(offset, length);
    }
    for (const auto& [offset, length] : differing) std::memcpy(held.data() + offset, reinterpret_cast<const void*>(address + offset), length);
    return true;
}

static void OwnedDrawTraceCount(const char* name, std::uint64_t amount = 1) {
    PerformanceTrace::CountOwnedDraw(name, amount);
}

void Recorder::KeepDrawSnapshot(std::uint64_t address, std::size_t bytes, std::uint64_t generation, std::uint64_t registryGeneration, std::shared_ptr<Buffer> buffer, SnapshotUse use, std::uint32_t derived, std::size_t from, std::uint32_t lowest, bool ownedInsertion) {
    Require(from == 0 || (use == SnapshotUse::Vertex && from < bytes), "only a vertex snapshot starts past its address");
    const bool storage = use == SnapshotUse::Storage;
    const auto budget = storage ? DrawSnapshotBudget : DrawInputBudget;
    const auto maxEntries = storage ? DrawSnapshotEntries : DrawInputEntries;
    auto& pool = drawSnapshotPools[SnapshotPool(use)];
    if (generation == 0 || bytes > budget) return;
    if (use == SnapshotUse::Vertex) {
        for (auto it = drawSnapshots.lower_bound({address, use, 0}); it != drawSnapshots.end() && std::get<0>(it->first) == address && std::get<1>(it->first) == use && std::get<2>(it->first) <= bytes;) eraseDrawSnapshot(it++);
    } else if (const auto found = drawSnapshots.find({address, use, bytes}); found != drawSnapshots.end()) {
        eraseDrawSnapshot(found);
    }
    while (!pool.recency.empty() && (pool.bytes + bytes > budget || pool.recency.size() >= maxEntries)) {
        const auto victim = drawSnapshots.find(pool.recency.front());
        if (!storage) {
            const bool ownedVictim = !victim->second.owned.words.empty();
            OwnedDrawTraceCount(ownedVictim ? "OwnedDraw.evicted_owned" : "OwnedDraw.evicted_other");
            if (ownedInsertion) OwnedDrawTraceCount(ownedVictim ? "OwnedDraw.evicted_owned_by_owned" : "OwnedDraw.evicted_other_by_owned");
        }
        eraseDrawSnapshot(victim);
    }
    const DrawSnapshotKey key{address, use, bytes};
    pool.recency.push_back(key);
    try {
        drawSnapshots.emplace(key, DrawSnapshot{generation, registryGeneration, std::prev(pool.recency.end()), std::move(buffer), derived, from, lowest, nullptr});
    } catch (...) {
        pool.recency.pop_back();
        throw;
    }
    pool.bytes += bytes;
}

Recorder::OwnedDrawInputProof Recorder::ArmOwnedDrawInput(std::uint64_t address, std::size_t bytes) const {
    GuestMemory::AssertGpuLockHeld("arm owned draw input");
    OwnedDrawTraceCount("OwnedDraw.arm_attempt");
    if (bytes == 0 || bytes > DrawInputBudget || bytes > std::numeric_limits<std::uint64_t>::max() - address || !GuestMemory::WriteWatched() || PendingWriteOverlaps(address, bytes)) {
        OwnedDrawTraceCount("OwnedDraw.arm_refused");
        return {};
    }
    OwnedDrawInputProof proof;
    proof.words.resize((address + bytes - 1) / 65536 - address / 65536 + 1);
    OwnedDrawTraceCount("OwnedDraw.arm_blocks_requested", proof.words.size());
    if (!GuestArena::GuestArenaArmOwned_nid_no_patch(static_cast<std::uintptr_t>(address), bytes, proof.words.data(), proof.words.size(), 1)) {
        OwnedDrawTraceCount("OwnedDraw.arm_refused");
        return {};
    }
    proof.registry = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    proof.forget = GuestMemory::ForgetSerial();
    proof.since = GuestMemory::TrackerGeneration();
    proof.note = NewestWriteNote(address, bytes);
    if (proof.since == 0 || (proof.forget & 1u) != 0) {
        OwnedDrawTraceCount("OwnedDraw.arm_refused");
        return {};
    }
    OwnedDrawTraceCount("OwnedDraw.arm_success");
    return proof;
}

bool Recorder::ownedDrawInputHolds(std::uint64_t address, std::size_t bytes, const OwnedDrawInputProof& proof) const {
    GuestMemory::AssertGpuLockHeld("check owned draw input");
    OwnedDrawTraceCount("OwnedDraw.proof_check");
    if (proof.words.empty() || proof.since == 0 || (proof.forget & 1u) != 0 || !GuestMemory::WriteWatched()) return false;
    if (proof.registry != GuestAllocations::GuestAllocationsGeneration_nid_postfix() || proof.forget != GuestMemory::ForgetSerial()) return false;
    if (PendingWriteOverlaps(address, bytes) || proof.note != NewestWriteNote(address, bytes) || GuestMemory::NewestStamp(address, bytes) > proof.since) return false;
    OwnedDrawTraceCount("OwnedDraw.arena_check");
    OwnedDrawTraceCount("OwnedDraw.check_blocks_requested", proof.words.size());
    return GuestArena::GuestArenaOwnedUnchanged_nid_no_patch(static_cast<std::uintptr_t>(address), bytes, proof.words.data(), proof.words.size());
}

std::shared_ptr<Buffer> Recorder::ReusableOwnedDrawInput(std::uint64_t address, std::size_t bytes, SnapshotUse use, std::uint32_t* derived, std::size_t from, std::uint32_t* lowest) {
    GuestMemory::AssertGpuLockHeld("reuse owned draw input");
    Require(use != SnapshotUse::Storage, "an owned draw input is a vertex or index buffer");
    const auto found = drawSnapshots.find({address, use, bytes});
    if (found == drawSnapshots.end() || found->second.owned.words.empty() || found->second.from > from) return {};
    if (!ownedDrawInputHolds(address, bytes, found->second.owned)) {
        OwnedDrawTraceCount("OwnedDraw.invalidated");
        eraseDrawSnapshot(found);
        return {};
    }
    static const bool verify = std::getenv("APS5_VERIFY_OWNED_DRAW_INPUTS") != nullptr;
    if (verify) {
        const auto held = found->second.from;
        const bool equal = GuestMemory::EqualsCommittedUnsynced(address + held, found->second.buffer->Bytes().subspan(held));
        if (!ownedDrawInputHolds(address, bytes, found->second.owned)) {
            eraseDrawSnapshot(found);
            return {};
        }
        Require(equal, "an owned draw input differs while its proof holds");
    }
    auto& recency = drawSnapshotPools[SnapshotPool(use)].recency;
    recency.splice(recency.end(), recency, found->second.recent);
    if (derived != nullptr) *derived = found->second.derived;
    if (lowest != nullptr) *lowest = found->second.lowest;
    OwnedDrawTraceCount("OwnedDraw.reused");
    return found->second.buffer;
}

void Recorder::KeepOwnedDrawInput(std::uint64_t address, std::size_t bytes, const OwnedDrawInputProof& proof, std::shared_ptr<Buffer> buffer, SnapshotUse use, std::uint32_t derived, std::size_t from, std::uint32_t lowest) {
    Require(use != SnapshotUse::Storage, "an owned draw input is a vertex or index buffer");
    if (buffer == nullptr || buffer->Bytes().size() != bytes || !ownedDrawInputHolds(address, bytes, proof)) return;
    auto kept = proof;
    KeepDrawSnapshot(address, bytes, proof.since, proof.registry, std::move(buffer), use, derived, from, lowest, true);
    if (const auto found = drawSnapshots.find({address, use, bytes}); found != drawSnapshots.end()) {
        found->second.owned = std::move(kept);
        ++ownedDrawInputs;
    }
}

bool Recorder::KnownHighestIndex(std::uint64_t address, std::size_t bytes, SnapshotUse use, std::optional<std::uint32_t>& highest, std::uint32_t* lowest) {
    const auto found = indexRanges.find({address, use, bytes});
    if (found == indexRanges.end()) return false;
    if (found->second.registryGeneration != GuestAllocations::GuestAllocationsGeneration_nid_postfix() || !GuestMemory::UnchangedSince(address, bytes, found->second.generation)) {
        indexRangeRecency.erase(found->second.recent);
        indexRanges.erase(found);
        return false;
    }
    indexRangeRecency.splice(indexRangeRecency.end(), indexRangeRecency, found->second.recent);
    highest = found->second.highest;
    if (lowest != nullptr) *lowest = found->second.lowest;
    return true;
}

void Recorder::KeepHighestIndex(std::uint64_t address, std::size_t bytes, SnapshotUse use, std::uint64_t generation, std::uint64_t registryGeneration, std::optional<std::uint32_t> highest, std::uint32_t lowest) {
    Require(use != SnapshotUse::Storage && use != SnapshotUse::Vertex, "a highest index belongs to an index buffer");
    if (generation == 0) return;
    const DrawSnapshotKey key{address, use, bytes};
    if (const auto found = indexRanges.find(key); found != indexRanges.end()) {
        indexRangeRecency.erase(found->second.recent);
        indexRanges.erase(found);
    }
    while (indexRanges.size() >= IndexRangeEntries) {
        indexRanges.erase(indexRangeRecency.front());
        indexRangeRecency.pop_front();
    }
    indexRangeRecency.push_back(key);
    try {
        indexRanges.emplace(key, IndexRange{generation, registryGeneration, std::prev(indexRangeRecency.end()), highest, lowest});
    } catch (...) {
        indexRangeRecency.pop_back();
        throw;
    }
}

}
}
}
}
}
}
