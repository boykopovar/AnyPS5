#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/UnitShadow.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics {
namespace {
namespace {
namespace {
DccKeys textureClearKeys(const GuestTextureResource& resource, std::uint64_t guestBytes, bool& memoized) {
    memoized = false;
    if (resource.dccAddress == 0) return DccKeys::Uncompressed;
    DccKeys keys;
    if (const auto known = WaitForKeyWriters(resource, guestBytes)) keys = *known;
    else keys = currentDccKeys(resource.dccAddress, DccKeyBytes(guestBytes), memoized);
    if (keys == DccKeys::Uncompressed || (keys == DccKeys::ClearSingle && SingleKeysReadable(resource))) return keys;
    if (DccClearTextureFormat(resource.format) == ResolveTextureFormat(resource.format) && IsConvertedTextureFormat(resource.format) && (keys == DccKeys::Clear0001 || keys == DccKeys::Clear1110)) throw std::runtime_error(std::string("AGC graphics: DCC clear code ") + DccKeysName(keys) + " of converted texture format " + std::to_string(resource.format) + " is not implemented");
    std::byte probe[16]{};
    if (!IsDccClear(keys) || !FillDccClear(DccClearTextureFormat(resource.format), keys, resource.dccAlphaOnMsb, std::span(probe, std::min<std::size_t>(sizeof(probe), BytesPerElement(resource.format))))) {
        static AgcDriver::Mutex reportedMutex;
        static std::set<std::pair<std::uint64_t, int>> reported;
        std::lock_guard lock(reportedMutex);
        if (reported.size() < 32 && reported.insert({resource.baseAddress, static_cast<int>(keys)}).second) std::fprintf(stderr, "[gpu] texture 0x%llx (format %u) has %s DCC keys at 0x%llx; its texels are read as stored\n", static_cast<unsigned long long>(resource.baseAddress), resource.format, DccKeysName(keys), static_cast<unsigned long long>(resource.dccAddress));
        return DccKeys::Uncompressed;
    }
    return keys;
}

DccKeys currentDccKeys(std::uint64_t metaAddress, std::size_t count, bool& memoized) {
    memoized = false;
    if (metaAddress != 0 && count != 0 && GuestMemory::GpuMutex().HeldByThisThread()) {
        if (auto* recorder = Recorder::Active(); recorder != nullptr && recorder->PendingWriteOverlaps(metaAddress, count)) {
            if (const auto keys = PendingStoreKeys(*recorder, metaAddress, count)) {
                memoized = true;
                return *keys;
            }
            Recorder::CountSync(2);
            recorder->SyncThrough(metaAddress, count);
        }
    }
    return readDccKeys(metaAddress, count, memoized);
}

std::size_t StoredKeyCount(std::uint64_t metaAddress, std::uint64_t surfaceBytes, std::size_t keyCount) {
    const auto keys = DccKeyBytes(surfaceBytes);
    Require(keyCount >= keys, "a DCC key range covers fewer keys than one per 256 surface bytes");
    if (metaAddress == 0 || keyCount == 0) return 0;
    if (GuestMemory::Accessible(reinterpret_cast<const void*>(metaAddress), keyCount, true)) return keyCount;
    if (keyCount == keys) return 0;
    char message[256];
    std::snprintf(message, sizeof(message), "AGC graphics: DCC metadata 0x%llx is not writable over the 0x%zx key bytes of its surface (0x%zx at one per 256 bytes): metadata smaller than the console's DCC extent is not modeled", static_cast<unsigned long long>(metaAddress), keyCount, keys);
    throw std::runtime_error(message);
}

}

DccKeys ReadDccKeys(std::uint64_t metaAddress, std::uint64_t surfaceBytes) {
    bool memoized = false;
    return readDccKeys(metaAddress, DccKeyBytes(surfaceBytes), memoized);
}

DccKeys CurrentDccKeys(std::uint64_t metaAddress, std::uint64_t surfaceBytes) {
    bool memoized = false;
    return currentDccKeys(metaAddress, DccKeyBytes(surfaceBytes), memoized);
}

DccKeys CurrentDccKeys(std::uint64_t metaAddress, std::uint64_t surfaceBytes, std::size_t keyCount) {
    Require(keyCount >= DccKeyBytes(surfaceBytes), "a DCC key range covers fewer keys than one per 256 surface bytes");
    bool memoized = false;
    return currentDccKeys(metaAddress, keyCount, memoized);
}

std::optional<std::uint8_t> CurrentUniformMetadataByte(std::uint64_t address, std::size_t bytes) {
    if (address == 0 || bytes == 0 || !GuestMemory::Accessible(reinterpret_cast<const void*>(address), bytes)) return std::nullopt;
    if (GuestMemory::GpuMutex().HeldByThisThread()) {
        if (AnyShadowedOverlaps(address, bytes)) PublishShadow(address, bytes, PublishScope::Whole, PublishReason::Keys);
        if (auto* recorder = Recorder::Active(); recorder != nullptr && (recorder->PendingWriteOverlaps(address, bytes) || AnyShadowedOverlaps(address, bytes))) {
            const auto landed = KeysAfterPendingWrites(*recorder, address, bytes);
            if (landed.mixed) return std::nullopt;
            if (!landed.unknown && landed.value >= 0 && !AnyShadowedOverlaps(address, bytes)) return static_cast<std::uint8_t>(landed.value);
            Recorder::CountSync(2);
            recorder->Sync();
        }
    }
    std::vector<std::byte> data(bytes);
    GuestMemory::Read(address, data);
    const auto first = data.front();
    if (!std::all_of(data.begin(), data.end(), [&](std::byte value) { return value == first; })) return std::nullopt;
    return std::to_integer<std::uint8_t>(first);
}

std::optional<DccKeys> KnownPendingDccKeys(std::uint64_t metaAddress, std::uint64_t surfaceBytes) {
    const auto count = DccKeyBytes(surfaceBytes);
    if (metaAddress == 0 || count == 0 || !GuestMemory::GpuMutex().HeldByThisThread()) return std::nullopt;
    auto* recorder = Recorder::Active();
    if (recorder == nullptr || !recorder->PendingWriteOverlaps(metaAddress, count)) return std::nullopt;
    return PendingStoreKeys(*recorder, metaAddress, count);
}

bool IsDccClear(DccKeys keys) {
    return keys == DccKeys::Clear0000 || keys == DccKeys::Clear0001 || keys == DccKeys::Clear1110 || keys == DccKeys::Clear1111 || keys == DccKeys::ClearRegister;
}

std::optional<std::uint8_t> LandedUniformMetadataByte(std::uint64_t address, std::size_t bytes) {
    if (address == 0 || bytes == 0 || !GuestMemory::Accessible(reinterpret_cast<const void*>(address), bytes)) return std::nullopt;
    if (AnyShadowedOverlaps(address, bytes)) PublishShadow(address, bytes, PublishScope::Whole, PublishReason::Keys);
    if (AnyShadowedOverlaps(address, bytes)) return std::nullopt;
    GuestMemory::FlushGpuWrites(address, bytes);
    auto* recorder = GuestMemory::GpuMutex().HeldByThisThread() ? Recorder::Active() : nullptr;
    if (recorder != nullptr && recorder->PendingWriteOverlaps(address, bytes)) {
        const auto landed = KeysAfterPendingWrites(*recorder, address, bytes);
        if (landed.mixed || landed.unknown || landed.value < 0) return std::nullopt;
        return static_cast<std::uint8_t>(landed.value);
    }
    const auto* data = reinterpret_cast<const std::uint8_t*>(address);
    if (!AllKeysEqual(data + 1, bytes - 1, data[0])) return std::nullopt;
    return data[0];
}

void MarkDccUncompressed(std::uint64_t metaAddress, std::uint64_t surfaceBytes) {
    MarkDccUncompressed(metaAddress, surfaceBytes, DccKeyBytes(surfaceBytes));
}

void MarkDccUncompressed(std::uint64_t metaAddress, std::uint64_t surfaceBytes, std::size_t keyCount) {
    const auto count = StoredKeyCount(metaAddress, surfaceBytes, keyCount);
    if (count == 0) return;
    // The bytes are trusted only when no recorded work writes them (lock-free, with or without
    // GuestMemory::GpuMutex); otherwise the store runs and waits through the flush hook. A pending
    // GPU store of the driver's own is no reason to skip: a title kernel recorded into the same
    // batch after it may write clear keys, and the memo cannot order inside a batch.
    if (!Recorder::SnapshotWriteOverlaps(metaAddress, count) && AlreadyUncompressed(metaAddress, count)) return;
    StoreUncompressedOnCpu(metaAddress, count);
}

void MarkDccUncompressed(const Context& context, std::uint64_t metaAddress, std::uint64_t surfaceBytes) {
    MarkDccUncompressed(context, metaAddress, surfaceBytes, DccKeyBytes(surfaceBytes));
}

void MarkDccUncompressed(const Context& context, std::uint64_t metaAddress, std::uint64_t surfaceBytes, std::size_t keyCount) {
    const auto count = StoredKeyCount(metaAddress, surfaceBytes, keyCount);
    if (count == 0) return;
    auto* recorder = CpuKeysOnly() || !GuestMemory::GpuMutex().HeldByThisThread() ? nullptr : Recorder::Active();
    // The bytes are scanned only when no recorded work writes them: a pending clear kernel's keys are
    // not in memory yet, so the bytes could read as uncompressed while the kernel will store a clear
    // code, and the range is stored without looking (the fill lands after the kernel). That includes
    // a pending GPU store of the driver's own (the memo): a title kernel recorded into the same batch
    // after it may write clear keys, and nothing orders inside a batch, so a fresh fill is recorded
    // (idempotent, no wait) rather than trusting the earlier one.
    const bool pending = recorder != nullptr && recorder->PendingWriteOverlaps(metaAddress, count);
    if (!pending && AlreadyUncompressed(metaAddress, count)) return;
    if (recorder != nullptr && StoreUncompressedOnGpu(context, *recorder, metaAddress, count)) return;
    StoreUncompressedOnCpu(metaAddress, count);
}

bool FillDccClear(VkFormat format, DccKeys keys, bool alphaOnMsb, std::span<std::byte> bytes) {
    if (keys == DccKeys::Clear0000) {
        std::fill(bytes.begin(), bytes.end(), std::byte{0});
        return true;
    }
    Layout layout{};
    if (!IsDccClear(keys) || keys == DccKeys::ClearRegister || !LayoutFor(format, layout)) {
        std::fill(bytes.begin(), bytes.end(), std::byte{0});
        return false;
    }
    const bool color = keys == DccKeys::Clear1110 || keys == DccKeys::Clear1111;
    const bool alpha = keys == DccKeys::Clear0001 || keys == DccKeys::Clear1111;
    const int alphaChannel = layout.channels == 3 ? -1 : alphaOnMsb ? static_cast<int>(layout.channels) - 1 : 0;
    std::uint32_t elementBits = 0;
    for (std::uint32_t channel = 0; channel < layout.channels; ++channel) elementBits += layout.bits[channel];
    std::vector<std::byte> element(elementBits / 8u);
    std::uint32_t bit = 0;
    for (std::uint32_t channel = 0; channel < layout.channels; ++channel) {
        const bool set = static_cast<int>(channel) == alphaChannel ? alpha : color;
        const auto value = set ? One(layout.kind, layout.bits[channel]) : 0u;
        for (std::uint32_t i = 0; i < layout.bits[channel]; ++i, ++bit) {
            if (((value >> i) & 1u) != 0) element[bit / 8u] |= static_cast<std::byte>(1u << (bit % 8u));
        }
    }
    const std::size_t whole = bytes.size() / element.size() * element.size();
    if (whole != 0) std::memcpy(bytes.data(), element.data(), element.size());
    for (std::size_t filled = element.size(); filled < whole;) {
        const auto chunk = std::min(filled, whole - filled);
        std::memcpy(bytes.data() + filled, bytes.data(), chunk);
        filled += chunk;
    }
    std::fill(bytes.begin() + static_cast<std::ptrdiff_t>(whole), bytes.end(), std::byte{0});
    return true;
}

bool DccAlphaOnMsb(VkFormat format, std::uint32_t componentSwap) {
    Layout layout{};
    const auto channels = LayoutFor(format, layout) ? layout.channels : 4u;
    constexpr std::uint32_t standardReversed = 2;
    constexpr std::uint32_t alternateReversed = 3;
    if (channels == 1) return componentSwap == alternateReversed;
    return componentSwap != standardReversed && componentSwap != alternateReversed;
}

DccKeys ScannedDccKeys(std::uint64_t metaAddress, std::size_t keyCount) {
    const std::span keys(reinterpret_cast<const std::uint8_t*>(metaAddress), keyCount);
    return std::ranges::all_of(keys, [&](std::uint8_t key) { return key == keys.front(); }) ? ByteKeys(keys.front()) : DccKeys::Mixed;
}

DccKeys TextureClearKeys(const GuestTextureResource& resource, std::uint64_t guestBytes) {
    bool memoized = false;
    return textureClearKeys(resource, guestBytes, memoized);
}

bool KeyFastPath() {
    static const bool enabled = std::getenv("APS5_NO_KEY_FAST_PATH") == nullptr;
    return enabled;
}

DccKeys ProvedClearKeys(const GuestTextureResource& resource, std::uint64_t guestBytes, DccKeyProof& proof) {
    if (resource.dccAddress == 0) return DccKeys::Uncompressed;
    auto& counters = Proofs();
    if (!KeyFastPath()) {
        counters.scanned.fetch_add(1, std::memory_order_relaxed);
        return TextureClearKeys(resource, guestBytes);
    }
    const auto count = static_cast<std::size_t>(guestBytes / KeyBytes);
    // The collect precedes the scan: a write landing between them is stamped above `collected`, so
    // the proof it would invalidate is never taken as current.
    const auto collected = count == 0 ? 0 : GuestMemory::CollectWrites(resource.dccAddress, count);
    if (proof.generation != 0 && collected != 0 && GuestMemory::UnchangedSince(resource.dccAddress, count, proof.generation) && PendingKeyProofHolds(resource, guestBytes, proof)) {
        counters.proved.fetch_add(1, std::memory_order_relaxed);
        return proof.keys;
    }
    bool memoized = false;
    const auto keys = textureClearKeys(resource, guestBytes, memoized);
    bool stable = collected != 0 && !memoized;
    if (stable) {
        // After the scan: a write noted since the collect is stamped above it anyway; one noted
        // before it may still land after the bytes were read.
        if (GuestMemory::GpuMutex().HeldByThisThread()) {
            if (auto* recorder = Recorder::Active(); recorder != nullptr) {
                const auto info = recorder->DescribePendingWrite(resource.dccAddress, count);
                stable = !info.has_value() || info->signaled;
            }
        } else {
            stable = !Recorder::SnapshotWriteOverlaps(resource.dccAddress, count);
        }
    }
    proof = stable ? DccKeyProof{keys, collected} : memoized && collected != 0 ? PendingKeyProof(resource, guestBytes, keys) : DccKeyProof{};
    counters.scanned.fetch_add(1, std::memory_order_relaxed);
    if (!stable) counters.unstable.fetch_add(1, std::memory_order_relaxed);
    return keys;
}

DccKeyProof PendingKeyProof(const GuestTextureResource& resource, std::uint64_t guestBytes, DccKeys keys) {
    const auto count = static_cast<std::size_t>(guestBytes / KeyBytes);
    if (!KeyFastPath() || resource.dccAddress == 0 || count == 0 || keys == DccKeys::Mixed || keys == DccKeys::Unreadable || !GuestMemory::GpuMutex().HeldByThisThread()) return {};
    auto* recorder = Recorder::Active();
    if (recorder == nullptr) return {};
    const auto info = recorder->DescribePendingWrite(resource.dccAddress, count);
    if (!info.has_value() || info->signaled) return {};
    const auto note = recorder->NewestWriteNote(resource.dccAddress, count);
    if (note == 0) return {};
    const auto collected = GuestMemory::CollectWrites(resource.dccAddress, count);
    if (collected == 0) return {};
    return {keys, collected, note, recorder->Id()};
}

bool PendingKeyProofHolds(const GuestTextureResource& resource, std::uint64_t guestBytes, const DccKeyProof& proof) {
    if (proof.pendingNote == 0) return true;
    const auto count = static_cast<std::size_t>(guestBytes / KeyBytes);
    if (!GuestMemory::GpuMutex().HeldByThisThread()) return false;
    auto* recorder = Recorder::Active();
    if (recorder == nullptr || recorder->Id() != proof.pendingRecorder || recorder->NewestWriteNote(resource.dccAddress, count) != proof.pendingNote) return false;
    const auto info = recorder->DescribePendingWrite(resource.dccAddress, count);
    return info.has_value() && !info->signaled;
}

DccKeyProofCounts KeyProofCounts() {
    const auto& counters = Proofs();
    return {counters.proved.load(std::memory_order_relaxed), counters.scanned.load(std::memory_order_relaxed), counters.unstable.load(std::memory_order_relaxed), counters.rangeProved.load(std::memory_order_relaxed), counters.rangeScanned.load(std::memory_order_relaxed)};
}

DccKeys ProvedCurrentDccKeys(std::uint64_t metaAddress, std::uint64_t surfaceBytes, DccRangeProof& proof) {
    const auto count = DccKeyBytes(surfaceBytes);
    if (metaAddress == 0 || count == 0 || !KeyFastPath() || !GuestMemory::GpuMutex().HeldByThisThread()) return CurrentDccKeys(metaAddress, surfaceBytes);
    auto& counters = Proofs();
    const auto collected = GuestMemory::CollectWrites(metaAddress, count);
    if (proof.generation != 0 && proof.address == metaAddress && proof.count == count && collected != 0 && GuestMemory::UnchangedSince(metaAddress, count, proof.generation)) {
        counters.rangeProved.fetch_add(1, std::memory_order_relaxed);
        return proof.keys;
    }
    bool memoized = false;
    const auto keys = currentDccKeys(metaAddress, count, memoized);
    bool stable = collected != 0 && !memoized;
    if (stable) {
        if (auto* recorder = Recorder::Active(); recorder != nullptr) {
            const auto info = recorder->DescribePendingWrite(metaAddress, count);
            stable = !info.has_value() || info->signaled;
        }
    }
    proof = stable ? DccRangeProof{metaAddress, count, keys, collected} : DccRangeProof{};
    counters.rangeScanned.fetch_add(1, std::memory_order_relaxed);
    return keys;
}

void ReadTextureSurface(const GuestTextureResource& resource, DccKeys keys, std::span<std::byte> bytes) {
    // Named for the [hooksync] attribution: the read goes through the flush hook.
    const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::TextureRead);
    if (keys == DccKeys::Uncompressed || keys == DccKeys::ClearSingle) GuestMemory::ReadCommitted(resource.baseAddress, bytes);
    else FillDccClear(DccClearTextureFormat(resource.format), keys, resource.dccAlphaOnMsb, bytes);
    if (keys == DccKeys::ClearSingle) ExpandSingleBlocks(bytes, BytesPerElement(resource.format));
}

bool SingleKeysReadable(const GuestTextureResource& resource) {
    return KeyBytes % BytesPerElement(resource.format) == 0;
}

bool ReadSingleElement(const GuestTextureResource& resource, std::span<std::byte> element) {
    if (!SingleKeysReadable(resource)) return false;
    const auto bytes = BytesPerElement(resource.format);
    if (element.size() < bytes) return false;
    const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::TextureRead);
    GuestMemory::ReadCommitted(resource.baseAddress, element.subspan(0, bytes));
    return true;
}

bool ProvedSingleUniform(const GuestTextureResource& resource, std::uint64_t guestBytes, std::uint64_t collected, DccSingleProof& proof) {
    if (!SingleKeysReadable(resource) || guestBytes < KeyBytes) return false;
    const auto bytes = BytesPerElement(resource.format);
    if (bytes == 0 || bytes > 16) return false;
    const auto blocks = static_cast<std::size_t>(guestBytes / KeyBytes);
    if (proof.generation != 0 && proof.address == resource.baseAddress && proof.bytes == guestBytes && proof.format == resource.format && collected != 0 && GuestMemory::UnchangedSince(resource.baseAddress, static_cast<std::size_t>(guestBytes), proof.generation)) return proof.uniform;
    std::byte first[16]{};
    bool uniform = true;
    std::vector<std::byte> buffer(std::min<std::uint64_t>(guestBytes, std::uint64_t(1) << 20));
    {
        const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::TextureRead);
        GuestMemory::ReadCommitted(resource.baseAddress, std::span(first, bytes));
        for (std::size_t block = 1; block < blocks && uniform;) {
            const auto offset = block * KeyBytes;
            const auto size = static_cast<std::size_t>(std::min<std::uint64_t>(buffer.size(), guestBytes - offset));
            GuestMemory::ReadCommitted(resource.baseAddress + offset, std::span(buffer).first(size));
            const auto covered = size / KeyBytes;
            for (std::size_t index = 0; index < covered && uniform; ++index) uniform = std::equal(first, first + bytes, buffer.begin() + index * KeyBytes);
            block += covered;
        }
    }
    proof = collected != 0 ? DccSingleProof{resource.baseAddress, guestBytes, collected, resource.format, uniform} : DccSingleProof{};
    return uniform;
}

namespace {

DccKeys EncodedKeys(const GuestTextureResource& resource, DccKeys keys) {
    if (keys == DccKeys::ClearSingle && SingleKeysReadable(resource)) return keys;
    std::byte probe[16]{};
    if (!IsDccClear(keys) || !FillDccClear(DccClearTextureFormat(resource.format), keys, resource.dccAlphaOnMsb, std::span(probe, std::min<std::size_t>(sizeof(probe), BytesPerElement(resource.format))))) return DccKeys::Uncompressed;
    return keys;
}

bool PendingFillsLeaveUncompressed(const Recorder& recorder, const GuestTextureResource& resource, std::size_t count) {
    const auto begin = resource.dccAddress;
    if (!GuestMemory::Accessible(reinterpret_cast<const void*>(begin), count) || AnyShadowedOverlaps(begin, count)) return false;
    const auto landed = KeysAfterPendingWrites(recorder, begin, count);
    return landed.mixed || (landed.value >= 0 && EncodedKeys(resource, ByteKeys(static_cast<std::uint8_t>(landed.value))) == DccKeys::Uncompressed);
}

}

namespace {

struct KeyWaitTotals {
    std::atomic<std::uint64_t> calls{0};
    std::atomic<std::uint64_t> noPending{0};
    std::atomic<std::uint64_t> known{0};
    std::atomic<std::uint64_t> fillsUncompressed{0};
    std::atomic<std::uint64_t> synced{0};
};

KeyWaitTotals& KeyWaits() {
    static KeyWaitTotals totals;
    return totals;
}

}

DccKeyWaitCounts KeyWaitCounts() {
    const auto& totals = KeyWaits();
    return {totals.calls.load(std::memory_order_relaxed), totals.noPending.load(std::memory_order_relaxed), totals.known.load(std::memory_order_relaxed), totals.fillsUncompressed.load(std::memory_order_relaxed), totals.synced.load(std::memory_order_relaxed)};
}

bool DccKeyWritersPending(std::uint64_t metaAddress, std::size_t count) {
    if (metaAddress == 0 || count == 0 || !Recorder::SnapshotWriteOverlaps(metaAddress, count)) return false;
    auto& memo = Memo();
    std::lock_guard lock(memo.mutex);
    MemoRecorder(memo);
    return std::none_of(memo.entries.begin(), memo.entries.end(), [&](const GpuKeyStore& store) { return store.note != 0 && store.begin <= metaAddress && metaAddress + count <= store.end; });
}

bool DccKeyWriterNeedsSync(std::uint64_t metaAddress, std::size_t count) {
    if (metaAddress == 0 || count == 0 || !GuestMemory::GpuMutex().HeldByThisThread()) return false;
    auto* recorder = Recorder::Active();
    return recorder != nullptr && recorder->PendingWriteOverlaps(metaAddress, count) && !PendingStoreKeys(*recorder, metaAddress, count).has_value();
}

std::optional<DccKeys> WaitForKeyWriters(const GuestTextureResource& resource, std::uint64_t guestBytes) {
    const auto count = static_cast<std::size_t>(guestBytes / KeyBytes);
    const auto metaAddress = resource.dccAddress;
    if (metaAddress == 0 || count == 0 || !GuestMemory::GpuMutex().HeldByThisThread()) return std::nullopt;
    auto& waits = KeyWaits();
    waits.calls.fetch_add(1, std::memory_order_relaxed);
    auto* recorder = Recorder::Active();
    if (recorder == nullptr || !recorder->PendingWriteOverlaps(metaAddress, count)) {
        waits.noPending.fetch_add(1, std::memory_order_relaxed);
        return std::nullopt;
    }
    if (PendingStoreKeys(*recorder, metaAddress, count).has_value()) {
        waits.known.fetch_add(1, std::memory_order_relaxed);
        return std::nullopt;
    }
    if (PendingFillsLeaveUncompressed(*recorder, resource, count)) {
        waits.fillsUncompressed.fetch_add(1, std::memory_order_relaxed);
        return DccKeys::Uncompressed;
    }
    waits.synced.fetch_add(1, std::memory_order_relaxed);
    Recorder::CountSync(2);
    recorder->SyncThrough(metaAddress, count);
    return std::nullopt;
}

}
}
}
