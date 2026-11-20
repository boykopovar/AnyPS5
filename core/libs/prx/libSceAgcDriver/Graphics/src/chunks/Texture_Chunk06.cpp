#include "prx/libc/include/HostThreadSlot.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Execution/include/AsyncLog.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SlimMutex.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/AtomicSharedPtr.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Execution/include/DeviceFault.hpp"
#include "prx/libc/include/HostThreadSlot.hpp"
#include "prx/libSceAgcDriver/Execution/include/ThreadScratch.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MapImportGate.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
// For the declaration of PendingStorageOverlaps, defined below.
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "ShaderCacheDirectory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/UnitShadow.hpp"
#include "prx/libSceAgcDriver/Graphics/include/KeptUnits.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "RdnaDecoder/include/RdnaDecoder/RdnaDescriptorFormat.hpp"
#include "IntermediateRepresentation/include/IntermediateRepresentation/IrMetadata/BufferFormat.hpp"
#include <algorithm>
#include <filesystem>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <stdexcept>
#include <vector>
#include <atomic>
#include <mutex>
#include <map>
#include <limits>
#include <exception>
#include <functional>
#include <optional>
#include <thread>
#include <unordered_map>
#include <utility>
#if defined(__APPLE__)
#include <unistd.h>
#endif

namespace AgcDriver::Graphics {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
DccKeys StorageTexture::ProvedKeys() const {
    std::optional<DccKeys> keys;
    if (descriptor.dccAddress != 0 && uploadedKeys == DccKeys::Uncompressed) keys = WaitForKeyWriters(descriptor, guestBytes);
    if (keys.has_value()) keyProof = PendingKeyProof(descriptor, guestBytes, *keys);
    else keys = ProvedClearKeys(descriptor, guestBytes, keyProof);
    if (IsDccClear(filledKeys) && *keys != filledKeys && !anyLayerPending()) filledKeys = DccKeys::Uncompressed;
    return *keys;
}

std::uint64_t StorageTexture::SinglePassMoves() {
    return singlePassMoves.load(std::memory_order_relaxed);
}

std::uint64_t StorageTexture::SlabRetiles() {
    return slabRetiles.load(std::memory_order_relaxed);
}

std::uint64_t StorageTexture::PaddingSeedCopies() {
    return paddingSeedCopies.load(std::memory_order_relaxed);
}

std::uint64_t StorageTexture::LinearImportRetiles() {
    return linearImportRetiles.load(std::memory_order_relaxed);
}

bool StorageTexture::otherPendingOverlaps() const {
    auto& pending = Pending();
    std::lock_guard lock(pending.mutex);
    const auto other = [&](const StorageTexture* texture) { return texture != this && texture->overlaps(descriptor.baseAddress, static_cast<std::size_t>(guestBytes)); };
    return std::any_of(pending.textures.begin(), pending.textures.end(), other) || std::any_of(pending.flushing.begin(), pending.flushing.end(), other);
}

const char* StorageTexture::RefreshObligation(std::uint32_t first, std::uint32_t count) const {
    const auto [firstLayer, lastLayer] = refreshUnits(first, count);
    const auto scoped = firstLayer != 0 || lastLayer != trackedLayers;
    {
        const auto base = scoped ? layerBegin(firstLayer) : descriptor.baseAddress;
        const auto bytes = scoped ? static_cast<std::size_t>(layerBegin(lastLayer - 1) + layerBytes(lastLayer - 1) - base) : static_cast<std::size_t>(guestBytes);
        auto& pending = Pending();
        std::lock_guard lock(pending.mutex);
        const auto other = [&](const StorageTexture* texture) { return texture != this && texture->overlaps(base, bytes); };
        const auto owed = [&](const StorageTexture* texture) { return other(texture) && texture->pendingUnitInside(base, bytes); };
        if (std::any_of(pending.textures.begin(), pending.textures.end(), owed) || std::any_of(pending.flushing.begin(), pending.flushing.end(), other)) return "another image's pending results";
    }
    auto* recorder = Recorder::Active();
    const auto keyCount = static_cast<std::size_t>(guestBytes / 256);
    if (descriptor.dccAddress != 0 && keyCount != 0 && (recorder == nullptr || !recorder->PendingWriteOverlaps(descriptor.dccAddress, keyCount)) && ScannedDccKeys(descriptor.dccAddress, keyCount) != uploadedKeys) return "a DCC key change";
    if (original.size() != guestBytes) return nullptr;
    for (auto unit = firstLayer; unit < lastLayer; ++unit) {
        if (layerPending[unit] || !layerOriginal[unit]) continue;
        const auto begin = layerBegin(unit);
        const auto bytes = static_cast<std::size_t>(layerBytes(unit));
        if (std::memcmp(reinterpret_cast<const void*>(begin), original.data() + (begin - descriptor.baseAddress), bytes) != 0) return "guest bytes the image does not hold";
    }
    return nullptr;
}

bool StorageTexture::refreshProved() {
    if (refreshProof.generation == 0) return false;
    if (generation != refreshProof.generation || uploadedKeys != refreshProof.keys || !GuestMemory::GpuMutex().HeldByThisThread()) return false;
    const auto bytes = static_cast<std::size_t>(guestBytes);
    std::array<GuestMemory::UnchangedQuery, 2> queries{};
    std::size_t used = 0;
    if (GuestMemory::CollectWrites(descriptor.baseAddress, bytes) == 0) return false;
    queries[used++] = {descriptor.baseAddress, bytes, generation};
    if (descriptor.dccAddress != 0) {
        const auto keyCount = static_cast<std::size_t>(guestBytes / 256);
        if (keyCount == 0 || keyProof.generation != refreshProof.keyGeneration || keyProof.keys != uploadedKeys || !PendingKeyProofHolds(descriptor, guestBytes, keyProof)) return false;
        if (GuestMemory::CollectWrites(descriptor.dccAddress, keyCount) == 0) return false;
        queries[used++] = {descriptor.dccAddress, keyCount, keyProof.generation};
    }
    if (!GuestMemory::UnchangedSinceAll(std::span(queries.data(), used))) return false;
    const auto serial = PendingSerial();
    if (serial != refreshProof.pendingSerial) {
        if (otherPendingOverlaps()) return false;
        refreshProof.pendingSerial = serial;
    }
    return true;
}

void StorageTexture::takeRefreshProof(bool aliased) {
    refreshProof = {};
    if (aliased || generation == 0 || !GuestMemory::GpuMutex().HeldByThisThread()) return;
    if (descriptor.dccAddress != 0 && (keyProof.generation == 0 || keyProof.keys != uploadedKeys)) return;
    const auto serial = PendingSerial();
    if (otherPendingOverlaps()) return;
    refreshProof = {generation, serial, keyProof.generation, uploadedKeys};
}

std::uint64_t StorageTexture::RefreshesProved() {
    return refreshesProved.load(std::memory_order_relaxed);
}

std::uint64_t StorageTexture::UntrackedCompares() {
    return untrackedCompares.load(std::memory_order_relaxed);
}

std::uint64_t StorageTexture::RewatchedConfirms() {
    return rewatchedConfirms.load(std::memory_order_relaxed);
}

std::uint64_t StorageTexture::GuestSnapshots() {
    return guestSnapshots.load(std::memory_order_relaxed);
}

bool StorageTexture::ServesKeysAt(std::uint64_t dccAddress) const {
    const bool locked = GuestMemory::GpuMutex().HeldByThisThread();
    const auto readFollowed = [&] {
        DccKeyProof unlocked;
        return ProvedClearKeys(descriptor, guestBytes, locked ? keyProof : unlocked);
    };
    const auto readNamed = [&] {
        auto named = descriptor;
        named.dccAddress = dccAddress;
        DccKeyProof unlocked;
        if (!locked) return ProvedClearKeys(named, guestBytes, unlocked);
        auto slot = std::find_if(foreignKeyProofs.begin(), foreignKeyProofs.end(), [&](const ForeignKeyProof& entry) { return entry.dccAddress == dccAddress; });
        if (slot == foreignKeyProofs.end()) {
            slot = foreignKeyProofs.begin() + nextForeignKeyProof++ % foreignKeyProofs.size();
            *slot = ForeignKeyProof{dccAddress, {}};
        }
        return ProvedClearKeys(named, guestBytes, slot->proof);
    };
    return KeysServeSurface(descriptor.dccAddress, uploadedKeys, filledKeys, dccAddress, readFollowed, readNamed);
}

namespace {

// Debug aid (APS5_TRACE_DCC_KEYS=1): who stores uncompressed keys over a surface (recommendation
// 2's tracer): the write-back path, the surface and its key range.
void traceKeyStore(const char* path, const GuestTextureResource& descriptor, std::uint64_t guestBytes) {
    static const bool trace = std::getenv("APS5_TRACE_DCC_KEYS") != nullptr;
    if (!trace || descriptor.dccAddress == 0) return;
    const auto packet = GuestMemory::CurrentPacket();
    std::fprintf(stderr, "[dcc-keys] uncompressed store by %s (%s) for surface 0x%llx+0x%llx: keys 0x%llx+0x%llx (packet 0x%x queue 0x%x)\n", path, flushReason != nullptr ? flushReason : "?", static_cast<unsigned long long>(descriptor.baseAddress), static_cast<unsigned long long>(guestBytes), static_cast<unsigned long long>(descriptor.dccAddress), static_cast<unsigned long long>(DccKeyCount(descriptor, guestBytes)), packet.opcode, packet.queue);
}

}

void StorageTexture::captureGuestBytes(const std::vector<bool>* layers, bool keepWritten) {
    guestSnapshots.fetch_add(1, std::memory_order_relaxed);
    original.resize(static_cast<std::size_t>(guestBytes));
    comparedRanges = {};
    std::vector<bool> selected(trackedLayers, true);
    if (layers != nullptr) selected = *layers;
    const auto runs = unitRuns(selected);
    for (const auto& [begin, end] : runs) GuestMemory::FlushGpuWrites(descriptor.baseAddress + begin, static_cast<std::size_t>(end - begin));
    for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
        if (!selected[layer]) continue;
        const auto offset = static_cast<std::size_t>(layerBegin(layer) - descriptor.baseAddress);
        GuestMemory::ReadCommittedUnsynced(layerBegin(layer), std::span(original).subspan(offset, static_cast<std::size_t>(layerBytes(layer))));
    }
    if (keepWritten) {
        for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
            if (!selected[layer] || layerGeneration[layer] == 0) continue;
            GuestMemory::CollectWritesUncached(layerBegin(layer), static_cast<std::size_t>(layerBytes(layer)));
            keepWrittenSince(layer);
        }
    }
    for (std::uint32_t layer = 0; layer < trackedLayers; ++layer) {
        if (!selected[layer]) continue;
        layerOriginal[layer] = true;
        cancelStoredSave(layer);
    }
}

bool StorageTexture::guestBytesStillHeld() const {
    const auto bytes = static_cast<std::size_t>(guestBytes);
    if (original.size() != bytes || !originalHolds(descriptor.baseAddress, bytes)) return false;
    if (std::any_of(savedTicket.begin(), savedTicket.end(), [](std::uint64_t ticket) { return ticket != 0; })) return false;
    for (const auto& [from, unitGeneration] : keptAtUnwatch) {
        if (keptAtUnwatchHolds(from, static_cast<std::uint32_t>((from - descriptor.baseAddress) / trackedLayerBytes))) return false;
    }
    constexpr std::uint64_t blockBytes = 65536;
    std::vector<std::uint8_t> blocks(static_cast<std::size_t>((descriptor.baseAddress + guestBytes - 1) / blockBytes - descriptor.baseAddress / blockBytes + 1));
    if (!compareUntracked(descriptor.baseAddress, bytes, blocks, true)) return false;
    return std::all_of(blocks.begin(), blocks.end(), [](std::uint8_t block) { return block == GuestMemory::BlockUnchanged; });
}

bool StorageTexture::keepWrittenSince(std::uint32_t unit) {
    constexpr std::uint64_t block = 65536;
    const auto begin = layerBegin(unit);
    const auto end = begin + layerBytes(unit);
    const auto spanBegin = begin & ~(block - 1);
    const auto spanBlocks = static_cast<std::size_t>((end - spanBegin + block - 1) / block);
    std::vector<std::uint64_t> generations(spanBlocks);
    for (std::size_t k = 0; k < spanBlocks; ++k) generations[k] = unitBlockGeneration(unit, spanBegin + k * block);
    std::vector<std::uint8_t> changedBlocks(spanBlocks);
    if (!GuestMemory::ChangedBlocks(begin, static_cast<std::size_t>(end - begin), generations, changedBlocks)) return false;
    forgetKeptAtUnwatch(unit);
    for (auto at = spanBegin; at < end; at += block) {
        const auto from = std::max(at, begin);
        const auto to = std::min(at + block, end);
        const bool edge = from != at || to != at + block;
        if (changedBlocks[static_cast<std::size_t>((at - spanBegin) / block)] == GuestMemory::BlockWritten && (!edge || GuestMemory::StoredOver(from, static_cast<std::size_t>(to - from), unitBlockGeneration(unit, at))) && !cpuLeftUnchanged(unit, from, to, edge)) keptAtUnwatch.emplace(from, layerGeneration[unit]);
    }
    return true;
}

constexpr std::uint64_t GenerationBaselineBytes = 1ull << 20;

std::array<std::uint64_t, 2> StorageTexture::unitContentHash(std::uint32_t unit) const {
    constexpr std::size_t chunkBytes = 65536;
    std::array<std::uint64_t, 2> hash{0x5eed7e0d00000001ull, 0x5eed7e0d00000002ull};
    const auto begin = layerBegin(unit);
    const auto bytes = static_cast<std::size_t>(layerBytes(unit));
    std::vector<std::byte> chunk(std::min(chunkBytes, bytes));
    for (std::size_t at = 0; at < bytes; at += chunkBytes) {
        const auto part = std::span<std::byte>(chunk).first(std::min(chunkBytes, bytes - at));
        GuestMemory::ReadCommittedUnsynced(begin + at, part);
        hash[0] = ShaderRecompiler::HashBytes(part, hash[0]);
        hash[1] = ShaderRecompiler::HashBytes(part, hash[1]);
    }
    if (hash[0] == 0 && hash[1] == 0) hash[0] = 1;
    return hash;
}

void StorageTexture::hashBeforeUnwatch(std::uint64_t address, std::size_t bytes, bool proven) {
    if (released || unwatchHash.size() != trackedLayers || generationBaseline.size() == guestBytes) return;
    for (std::uint32_t unit = 0; unit < trackedLayers; ++unit) {
        const auto begin = layerBegin(unit);
        const auto unitBytes = static_cast<std::size_t>(layerBytes(unit));
        if (begin >= address + bytes || address >= begin + unitBytes || layerGeneration[unit] == 0) continue;
        const auto skip = [&](BaselineMiss miss) {
            unwatchHash[unit].skipped = miss;
            unwatchHash[unit].skippedGeneration = layerGeneration[unit];
        };
        if (layerPending[unit] || layerOriginal[unit] || savedTicket[unit] != 0) {
            skip(BaselineMiss::UnwatchWhileSaved);
            continue;
        }
        if (!proven || GuestMemory::CollectWritesUncached(begin, unitBytes) == 0) {
            skip(BaselineMiss::UnwatchUnproven);
            continue;
        }
        if (!GuestMemory::UnchangedSince(begin, unitBytes, layerGeneration[unit])) {
            skip(BaselineMiss::UnwatchSawCpuWrites);
            continue;
        }
        unwatchHash[unit] = {unitContentHash(unit), layerGeneration[unit]};
    }
}

void StorageTexture::readBaseline(std::uint32_t unit) {
    const auto offset = static_cast<std::size_t>(layerBegin(unit) - descriptor.baseAddress);
    GuestMemory::ReadCommittedUnsynced(layerBegin(unit), std::span<std::byte>(generationBaseline).subspan(offset, static_cast<std::size_t>(layerBytes(unit))));
    baselineDue[unit] = false;
}

void StorageTexture::rebaseBeforeUnwatch(std::uint64_t address, std::size_t bytes, bool proven) {
    if (released || !proven || !anyBaselineDue.load(std::memory_order_relaxed) || generationBaseline.size() != guestBytes) return;
    auto* recorder = recorderFor(context);
    std::vector<std::byte> copy;
    bool due = false;
    for (std::uint32_t unit = 0; unit < trackedLayers; ++unit) {
        if (!baselineDue[unit]) continue;
        const auto begin = layerBegin(unit);
        const auto unitBytes = static_cast<std::size_t>(layerBytes(unit));
        if (begin >= address + bytes || address >= begin + unitBytes) {
            due = true;
            continue;
        }
        if (layerPending[unit] || layerOriginal[unit] || savedTicket[unit] != 0 || layerGeneration[unit] == 0 || rebaseTicket[unit] != 0 || AnyShadowedOverlaps(begin, unitBytes)) continue;
        const bool storing = recorder != nullptr && recorder->PendingWriteOverlaps(begin, unitBytes);
        if (!storing) {
            copy.resize(unitBytes);
            GuestMemory::ReadCommittedUnsynced(begin, copy);
        }
        if (GuestMemory::CollectWritesUncached(begin, unitBytes) == 0 || !GuestMemory::UnchangedSince(begin, unitBytes, layerGeneration[unit])) continue;
        if (storing) {
            const auto ticket = ++lastRebaseTicket;
            rebaseTicket[unit] = ticket;
            recorder->OnWriteComplete(begin, unitBytes, [texture = weak_from_this(), ticket, unit] {
                if (auto alive = texture.lock()) alive->applyRebase(ticket, unit);
            });
            due = true;
            continue;
        }
        std::memcpy(generationBaseline.data() + static_cast<std::size_t>(begin - descriptor.baseAddress), copy.data(), unitBytes);
        baselineDue[unit] = false;
    }
    anyBaselineDue.store(due, std::memory_order_relaxed);
}

void StorageTexture::applyRebase(std::uint64_t ticket, std::uint32_t unit) {
    if (rebaseTicket[unit] != ticket) return;
    rebaseTicket[unit] = 0;
    if (generationBaseline.size() == guestBytes) readBaseline(unit);
}

void StorageTexture::awaitRebase(std::uint32_t unit) {
    if (rebaseTicket[unit] == 0) return;
    auto* recorder = recorderFor(context);
    if (recorder == nullptr) recorder = context.recorder;
    Require(recorder != nullptr, "a storage image's baseline is due after a store in flight without a recorder");
    recorder->SyncThrough(layerBegin(unit), static_cast<std::size_t>(layerBytes(unit)));
    if (rebaseTicket[unit] != 0) recorder->Sync();
}

void StorageTexture::forgetUnwatchHash(std::uint32_t first, std::uint32_t count) {
    for (auto unit = first; unit < first + count && unit < unwatchHash.size(); ++unit) unwatchHash[unit] = {};
}

const char* StorageTexture::BaselineMissText(BaselineMiss miss) {
    switch (miss) {
    case BaselineMiss::None:
        return "no cause was recorded";
    case BaselineMiss::LeftWatchWhilePending:
        return "its results became pending while its range was watched, and its bytes were not saved when the range left the watch";
    case BaselineMiss::SaveInFlight:
        return "a save of its bytes was in flight when its results became pending, and it did not complete";
    case BaselineMiss::SavedBytesDropped:
        return "its saved bytes were dropped while its results were pending";
    case BaselineMiss::NoUploadCopy:
        return "no copy of its bytes was taken at its upload";
    case BaselineMiss::UploadCopyDiffers:
        return "its bytes differ from the copy taken at its upload";
    case BaselineMiss::NoUnwatchHash:
        return "no hash of its bytes was taken for its current generation: its range left the watch without the unwatch hook, or its generation changed after the hash";
    case BaselineMiss::UnwatchUnproven:
        return "its range left the watch through the unwatch hook without its CPU writes proven (a failed collect or import page walk)";
    case BaselineMiss::UnwatchSawCpuWrites:
        return "the write watch saw CPU writes to it after its generation, before its range left the watch";
    case BaselineMiss::UnwatchWhileSaved:
        return "it had pending results or saved bytes when its range left the watch, so the unwatch hook did not hash it";
    case BaselineMiss::HashDiffers:
        return "its bytes no longer match the hash taken when its range left the watch";
    }
    throw std::runtime_error("AGC graphics: an unknown cause for a storage image unit without saved guest bytes");
}

StorageTexture::BaselineMiss StorageTexture::baselineMiss(std::uint32_t unit) const {
    if (generationBaseline.size() == guestBytes) {
        const auto offset = static_cast<std::size_t>(layerBegin(unit) - descriptor.baseAddress);
        return GuestMemory::EqualsCommittedUnsynced(layerBegin(unit), std::span<const std::byte>(generationBaseline).subspan(offset, static_cast<std::size_t>(layerBytes(unit)))) ? BaselineMiss::None : BaselineMiss::UploadCopyDiffers;
    }
    if (guestBytes <= GenerationBaselineBytes) return BaselineMiss::NoUploadCopy;
    if (unwatchHash.size() != trackedLayers) return BaselineMiss::NoUnwatchHash;
    const auto& entry = unwatchHash[unit];
    if (entry.generation != 0 && entry.generation == layerGeneration[unit]) return unitContentHash(unit) == entry.hash ? BaselineMiss::None : BaselineMiss::HashDiffers;
    if (entry.skipped != BaselineMiss::None && entry.skippedGeneration == layerGeneration[unit]) return entry.skipped;
    return BaselineMiss::NoUnwatchHash;
}

bool StorageTexture::originalHolds(std::uint64_t address, std::size_t bytes) const {
    for (auto layer = (address - descriptor.baseAddress) / trackedLayerBytes; layer < trackedLayers && layer * trackedLayerBytes < address - descriptor.baseAddress + bytes; ++layer) {
        if (!layerOriginal[static_cast<std::size_t>(layer)]) return false;
    }
    return true;
}

StorageTexture::RewatchProof StorageTexture::rewatchProof(std::uint64_t begin, std::uint64_t end, bool queried) const {
    if (end <= begin) return RewatchProof::None;
    const auto bytes = static_cast<std::size_t>(end - begin);
    const auto firstUnit = static_cast<std::uint32_t>((begin - descriptor.baseAddress) / trackedLayerBytes);
    bool ungenerated = false;
    auto since = std::numeric_limits<std::uint64_t>::max();
    for (auto unit = firstUnit; unit < trackedLayers && layerBegin(unit) < end; ++unit) {
        if (unit < savedTicket.size() && savedTicket[unit] != 0) return RewatchProof::None;
        if (layerGeneration[unit] == 0) ungenerated = true;
        else since = std::min(since, layerGeneration[unit]);
    }
    const bool copy = original.size() == guestBytes && originalHolds(begin, bytes);
    if (ungenerated ? !copy : since == std::numeric_limits<std::uint64_t>::max()) return RewatchProof::None;
    if (!queried && !(ungenerated ? GuestMemory::Rewatched(begin, bytes) : GuestMemory::OnlyRewatchedSince(begin, bytes, since))) return RewatchProof::None;
    struct Exempt {
        const StorageTexture* previous;
        ~Exempt() { Refreshing::Set(previous); }
    } exempt{Refreshing::Get()};
    Refreshing::Set(this);
    GuestMemory::FlushGpuWrites(begin, bytes);
    if (copy && originalHolds(begin, bytes)) {
        if (GuestMemory::EqualsCommittedUnsynced(begin, std::span<const std::byte>(original).subspan(static_cast<std::size_t>(begin - descriptor.baseAddress), bytes))) return RewatchProof::Unchanged;
        return ungenerated ? RewatchProof::Changed : RewatchProof::None;
    }
    if (ungenerated) return RewatchProof::None;
    for (auto unit = firstUnit; unit < trackedLayers && layerBegin(unit) < end; ++unit) {
        if (baselineMiss(unit) != BaselineMiss::None) return RewatchProof::None;
    }
    return RewatchProof::Unchanged;
}

void StorageTexture::confirmRewatched(std::uint64_t spanBegin, std::span<std::uint8_t> stamped, std::span<std::uint8_t> cpu, std::vector<bool>* comparedUnits) const {
    constexpr std::uint64_t block = 65536;
    if (stamped.empty() || std::all_of(stamped.begin(), stamped.end(), [](std::uint8_t entry) { return entry == GuestMemory::BlockUnchanged; })) return;
    std::vector<std::uint64_t> since(stamped.size(), std::numeric_limits<std::uint64_t>::max());
    for (std::uint32_t unit = 0; unit < trackedLayers; ++unit) {
        const auto unitEnd = layerBegin(unit) + layerBytes(unit);
        if (unitEnd <= spanBegin) continue;
        for (auto at = std::max(spanBegin, layerBegin(unit) & ~(block - 1)); at < unitEnd; at += block) {
            const auto k = static_cast<std::size_t>((at - spanBegin) / block);
            if (k >= since.size()) break;
            since[k] = std::min(since[k], layerGeneration[unit]);
        }
    }
    std::vector<std::uint8_t> rewatched(stamped.size(), 0);
    GuestMemory::RewatchedBlocks(spanBegin, stamped.size() * block, since, rewatched);
    for (std::size_t k = 0; k < stamped.size(); ++k) {
        if (stamped[k] == GuestMemory::BlockUnchanged || rewatched[k] == 0) continue;
        const auto begin = std::max(descriptor.baseAddress, spanBegin + k * block);
        const auto end = std::min(descriptor.baseAddress + guestBytes, spanBegin + (k + 1) * block);
        if (end <= begin) continue;
        const auto proof = rewatchProof(begin, end, true);
        if (proof == RewatchProof::Changed && comparedUnits != nullptr) {
            for (auto unit = static_cast<std::uint32_t>((begin - descriptor.baseAddress) / trackedLayerBytes); unit < trackedLayers && unit < comparedUnits->size() && layerBegin(unit) < end; ++unit) (*comparedUnits)[unit] = true;
        }
        if (proof != RewatchProof::Unchanged) continue;
        stamped[k] = GuestMemory::BlockUnchanged;
        if (k < cpu.size()) cpu[k] = 0;
        rewatchedConfirms.fetch_add(1, std::memory_order_relaxed);
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
