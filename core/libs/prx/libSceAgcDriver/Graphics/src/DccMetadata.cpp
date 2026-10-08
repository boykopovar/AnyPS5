#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/UnitShadow.hpp"
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

constexpr std::uint64_t KeyBytes = 256;

// Whether every key in [keys, keys + count) equals `first`. Eight keys per compare: the scan runs
// on every texture and storage validation, and an all-0xff (uncompressed) surface's keys are read
// to the end each time (132 KiB for a 4K RGBA8 target). APS5_NO_DCC_WORD_SCAN=1 restores the byte loop.
bool AllKeysEqual(const std::uint8_t* keys, std::size_t count, std::uint8_t first) {
    static const bool byteScan = std::getenv("APS5_NO_DCC_WORD_SCAN") != nullptr;
    if (byteScan) return std::all_of(keys, keys + count, [&](std::uint8_t key) { return key == first; });
    const std::uint64_t pattern = 0x0101010101010101ull * first;
    std::size_t i = 0;
    for (; i + 8 <= count; i += 8) {
        std::uint64_t word;
        std::memcpy(&word, keys + i, sizeof(word));
        if (word != pattern) return false;
    }
    for (; i < count; ++i) {
        if (keys[i] != first) return false;
    }
    return true;
}

DccKeys ByteKeys(std::uint8_t key) {
    switch (key) {
        case 0x00: return DccKeys::Clear0000;
        case 0x40: return DccKeys::Clear0001;
        case 0x80: return DccKeys::Clear1110;
        case 0xc0: return DccKeys::Clear1111;
        case 0x20: return DccKeys::ClearRegister;
        case 0xff: return DccKeys::Uncompressed;
        default: return DccKeys::Mixed;
    }
}

// APS5_PROFILE_DRAW: key scans, their bytes and time, printed as [dcc] every 10 s, with the hits of
// the GPU-store memo below and the "uncompressed" key stores by where they were made. Flush syncs
// stay 0 (no key read syncs the recorder); the field is kept so the line's shape does not change.
struct ScanProfile {
    std::atomic<std::uint64_t> scans{0};
    std::atomic<std::uint64_t> bytes{0};
    std::atomic<std::uint64_t> nanoseconds{0};
    std::atomic<std::uint64_t> memoHits{0};
    std::atomic<std::uint64_t> flushSyncs{0};
    std::atomic<std::uint64_t> gpuStores{0};
    // GPU stores whose range had a 4-byte unaligned head or tail (copied instead of filled).
    std::atomic<std::uint64_t> gpuSplitStores{0};
    std::atomic<std::uint64_t> cpuStores{0};
    std::atomic<std::int64_t> lastReport{0};
};

ScanProfile& Scans() {
    static ScanProfile profile;
    return profile;
}

// ProvedClearKeys outcomes, counted with or without profiling (the tests read them).
struct ProofCounters {
    std::atomic<std::uint64_t> proved{0};
    std::atomic<std::uint64_t> scanned{0};
    std::atomic<std::uint64_t> unstable{0};
};

ProofCounters& Proofs() {
    static ProofCounters counters;
    return counters;
}

bool ScanProfileEnabled() {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    return profile;
}

void ReportScans(std::chrono::steady_clock::time_point now) {
    auto& profile = Scans();
    const auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    auto last = profile.lastReport.load();
    // The first event starts the period rather than reporting an empty one.
    if (last == 0) {
        profile.lastReport.compare_exchange_strong(last, nowMs);
        return;
    }
    if (nowMs - last < 10000 || !profile.lastReport.compare_exchange_strong(last, nowMs)) return;
    const auto& proofs = Proofs();
    std::fprintf(stderr, "[dcc] %llu scans, %.1f MiB scanned, %llu memo hits, %llu flush syncs, %.1f ms; uncompressed keys stored: %llu on the GPU (%llu with an unaligned head or tail), %llu on the CPU; key proofs: %llu proved, %llu scanned, %llu unstable\n", static_cast<unsigned long long>(profile.scans.load()), profile.bytes.load() / 1048576.0, static_cast<unsigned long long>(profile.memoHits.load()), static_cast<unsigned long long>(profile.flushSyncs.load()), profile.nanoseconds.load() / 1e6, static_cast<unsigned long long>(profile.gpuStores.load()), static_cast<unsigned long long>(profile.gpuSplitStores.load()), static_cast<unsigned long long>(profile.cpuStores.load()), static_cast<unsigned long long>(proofs.proved.load()), static_cast<unsigned long long>(proofs.scanned.load()), static_cast<unsigned long long>(proofs.unstable.load()));
}

void CountScan(std::size_t bytes, std::chrono::steady_clock::time_point start) {
    auto& profile = Scans();
    const auto now = std::chrono::steady_clock::now();
    profile.scans.fetch_add(1, std::memory_order_relaxed);
    profile.bytes.fetch_add(bytes, std::memory_order_relaxed);
    profile.nanoseconds.fetch_add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now - start).count()), std::memory_order_relaxed);
    ReportScans(now);
}

void CountStore(std::atomic<std::uint64_t>& counter) {
    if (!ScanProfileEnabled()) return;
    counter.fetch_add(1, std::memory_order_relaxed);
    ReportScans(std::chrono::steady_clock::now());
}

// "Uncompressed" keys the driver stored on the GPU (MarkDccUncompressed's recorded fill) whose batch
// may not have run yet: the bytes ReadDccKeys scans would still show the keys from before the store
// (a fast-clear code), and a storage image whose write-back made the store would be re-uploaded as
// cleared. While recorded work that writes the range is still running, the range reads as
// uncompressed from here; the bytes are trusted again only once the newest recorded writer of the
// range has signaled its fence (batches of the recorder's queue finish in order, so the fill and
// every writer up to that one have landed and the bytes are final, whichever batch wrote last).
// Dropping an entry on a later batch's serial alone would not do: until the fill's batch runs the
// bytes PREDATE the fill (the previous clear code), and a later writer that is not a same-code clear
// (a decompress kernel, a partial key write, a false overlap from an unrelated V#) would make the
// next Refresh compare that stale code against the uploaded keys. The lookup runs with or without
// GuestMemory::GpuMutex (a resource build's stage A reads keys unlocked, its stage B re-reads them
// under the lock): unlocked it can only ask the lock-free snapshot whether the range is written at
// all. Entries belong to one recorder: when the active recorder changes (device replacement; a new
// recorder can reuse the old one's address) they are all dropped. Its mutex is a leaf (nothing is
// taken under it).
struct GpuKeyStore {
    std::uint64_t begin;
    std::uint64_t end;
    DccKeys keys = DccKeys::Uncompressed;
    std::uint64_t note = 0;
};

struct KeyStoreMemo {
    std::mutex mutex;
    std::vector<GpuKeyStore> entries;
    const Recorder* recorder = nullptr;
};

KeyStoreMemo& Memo() {
    static KeyStoreMemo memo;
    return memo;
}

// Under memo.mutex: the active recorder, with the entries of any other recorder dropped.
Recorder* MemoRecorder(KeyStoreMemo& memo) {
    auto* active = Recorder::Active();
    if (active != memo.recorder) {
        memo.entries.clear();
        memo.recorder = active;
    }
    return active;
}

void ForgetStores(std::uint64_t begin, std::uint64_t end) {
    auto& memo = Memo();
    std::lock_guard lock(memo.mutex);
    std::erase_if(memo.entries, [&](const GpuKeyStore& store) { return begin < store.end && store.begin < end; });
}

std::optional<DccKeys> MemoizedKeys(std::uint64_t begin, std::uint64_t end) {
    auto& memo = Memo();
    std::lock_guard lock(memo.mutex);
    auto* recorder = MemoRecorder(memo);
    for (auto it = memo.entries.begin(); it != memo.entries.end(); ++it) {
        if (it->begin > begin || it->end < end) continue;
        const auto bytes = static_cast<std::size_t>(it->end - it->begin);
        bool pending = false;
        if (recorder != nullptr) {
            if (GuestMemory::GpuMutex().HeldByThisThread()) {
                // The open batch is never signaled (also with APS5_NO_SYNC_THROUGH, where the info
                // carries the open flag with the newest in-flight batch's fence state).
                const auto info = recorder->DescribePendingWrite(it->begin, bytes);
                pending = info.has_value() && !info->signaled;
            } else {
                pending = Recorder::SnapshotWriteOverlaps(it->begin, bytes);
            }
        }
        if (!pending) {
            memo.entries.erase(it);
            return std::nullopt;
        }
        if (ScanProfileEnabled()) Scans().memoHits.fetch_add(1, std::memory_order_relaxed);
        return it->keys;
    }
    return std::nullopt;
}

// Debug aid (APS5_TRACE_DCC_KEYS=1): every uncompressed key store, with the packet that made it.
void TraceKeyStore(const char* path, std::uint64_t begin, std::size_t count) {
    static const bool trace = std::getenv("APS5_TRACE_DCC_KEYS") != nullptr;
    if (!trace) return;
    const auto packet = GuestMemory::CurrentPacket();
    std::fprintf(stderr, "[dcc-keys] uncompressed keys stored on the %s: 0x%llx+0x%zx (packet 0x%x queue 0x%x)\n", path, static_cast<unsigned long long>(begin), count, packet.opcode, packet.queue);
}

// The 0xff keys as a fill recorded into the active recorder's open batch, into the range's host
// import (the WriteLabelOnGpu / FillBuffer pattern): ordered behind every earlier recorded read or
// write of the range (the title's DCC clear or decompress kernel storing keys through a V#, which the
// CPU store had to wait for) and visible to the work recorded after it and to the host. vkCmdFillBuffer
// needs a 4-byte aligned offset and size; an unaligned head or tail is copied from a kept 4-byte
// buffer of 0xff instead. Under GuestMemory::GpuMutex (it records). False when the keys cannot be
// stored this way (memory the GPU has no view of): the caller stores them on the CPU.
bool StoreUncompressedOnGpu(const Context& context, Recorder& recorder, std::uint64_t begin, std::size_t count) {
    const auto import = HostImportFor(context, begin, count);
    if (import == nullptr) return false;
    const VkDeviceSize first = begin - import->base;
    const VkDeviceSize last = first + count;
    const VkDeviceSize fillBegin = std::min((first + 3) & ~VkDeviceSize{3}, last);
    const VkDeviceSize fillEnd = std::max(last & ~VkDeviceSize{3}, fillBegin);
    // Everything that can throw (the seed's allocation, the batch's command buffer) comes before the
    // first recorded command: a fill left in the batch without its note and memo entry would be an
    // unordered write of the keys.
    std::shared_ptr<Buffer> seed;
    if (fillBegin > first || last > fillEnd) {
        seed = std::make_shared<Buffer>(context, 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        std::memset(seed->Bytes().data(), 0xff, seed->Bytes().size());
        CountStore(Scans().gpuSplitStores);
    }
    // A unit shadow's results in the key range's edge units reach the import before the fill
    // lands over part of them (the stamp below then makes those units stale).
    if (AnyShadowedOverlaps(begin, count)) PublishShadow(begin, count, PublishScope::PartialUnits, PublishReason::Keys);
    // Queued on the open batch and recorded with the batch's other key stores as one run (or before
    // a later command writing the keys; see Recorder::QueueKeyStore).
    recorder.QueueKeyStore(import->buffer, first, last, std::move(seed), begin, begin + count);
    if (ShadowVerify()) recorder.Keep(import);
    recorder.NotePendingFill(begin, count, 0xff);
    GuestMemory::MarkWritten(begin, count);
    // After the note: an unlocked memo lookup drops an entry the snapshot does not cover yet. The
    // entry is kept only for the active recorder (the one the flush hook and the lookups consult).
    {
        auto& memo = Memo();
        std::lock_guard lock(memo.mutex);
        const auto end = begin + count;
        std::erase_if(memo.entries, [&](const GpuKeyStore& store) { return begin < store.end && store.begin < end; });
        if (MemoRecorder(memo) == &recorder) memo.entries.push_back({begin, end, DccKeys::Uncompressed, recorder.LastWriteNote(begin, count)});
    }
    CountStore(Scans().gpuStores);
    TraceKeyStore("gpu", begin, count);
    return true;
}

void StoreUncompressedOnCpu(std::uint64_t begin, std::size_t count) {
    TraceKeyStore("cpu", begin, count);
    // The write waits for recorded work over the range (the flush hook), so the bytes are current after.
    ForgetStores(begin, begin + count);
    const std::vector<std::byte> uncompressed(count, std::byte{0xff});
    GuestMemory::Write(begin, uncompressed);
    CountStore(Scans().cpuStores);
}

}

void NoteKeysFillOnGpu(std::uint64_t begin, std::size_t count, DccKeys keys) {
    static const bool disabled = std::getenv("APS5_NO_KEYS_FILL_MEMO") != nullptr;
    if (disabled || count == 0) return;
    auto& memo = Memo();
    std::lock_guard lock(memo.mutex);
    const auto end = begin + count;
    std::erase_if(memo.entries, [&](const GpuKeyStore& store) { return begin < store.end && store.begin < end; });
    auto* recorder = MemoRecorder(memo);
    if (recorder != nullptr) memo.entries.push_back({begin, end, keys, GuestMemory::GpuMutex().HeldByThisThread() ? recorder->LastWriteNote(begin, count) : 0});
}

namespace {

std::optional<DccKeys> PendingStoreKeys(Recorder& recorder, std::uint64_t begin, std::size_t count) {
    const auto end = begin + count;
    std::uint64_t note = 0;
    DccKeys keys = DccKeys::Mixed;
    {
        auto& memo = Memo();
        std::lock_guard lock(memo.mutex);
        if (MemoRecorder(memo) != &recorder) return std::nullopt;
        for (const auto& store : memo.entries) {
            if (store.note == 0 || store.begin > begin || store.end < end) continue;
            note = store.note;
            keys = store.keys;
            break;
        }
    }
    if (note == 0 || AnyShadowedOverlaps(begin, count) || recorder.NewestWriteNote(begin, count) != note) return std::nullopt;
    return keys;
}

// Whether the keys already read as uncompressed: scans the same bytes as ReadDccKeys, so it counts
// as a scan too.
bool AlreadyUncompressed(std::uint64_t metaAddress, std::size_t count) {
    const auto* keys = reinterpret_cast<const std::uint8_t*>(metaAddress);
    const auto start = ScanProfileEnabled() ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const bool uncompressed = AllKeysEqual(keys, count, 0xff);
    if (ScanProfileEnabled()) CountScan(count, start);
    return uncompressed;
}

bool CpuKeysOnly() {
    static const bool cpuKeys = std::getenv("APS5_CPU_DCC_KEYS") != nullptr;
    return cpuKeys;
}

enum class Kind { Unorm, Snorm, Uint, Sint, Float };

struct Layout {
    std::uint32_t channels;
    std::uint32_t bits[4];
    Kind kind;
};

bool LayoutFor(VkFormat format, Layout& layout) {
    switch (format) {
        case VK_FORMAT_R8_UNORM: case VK_FORMAT_R8_SRGB: layout = {1, {8}, Kind::Unorm}; return true;
        case VK_FORMAT_R8_UINT: layout = {1, {8}, Kind::Uint}; return true;
        case VK_FORMAT_R16_UNORM: layout = {1, {16}, Kind::Unorm}; return true;
        case VK_FORMAT_R16_SNORM: layout = {1, {16}, Kind::Snorm}; return true;
        case VK_FORMAT_R16_UINT: layout = {1, {16}, Kind::Uint}; return true;
        case VK_FORMAT_R16_SINT: layout = {1, {16}, Kind::Sint}; return true;
        case VK_FORMAT_R16_SFLOAT: layout = {1, {16}, Kind::Float}; return true;
        case VK_FORMAT_R8G8_UNORM: case VK_FORMAT_R8G8_SRGB: layout = {2, {8, 8}, Kind::Unorm}; return true;
        case VK_FORMAT_R8G8_SNORM: layout = {2, {8, 8}, Kind::Snorm}; return true;
        case VK_FORMAT_R8G8_UINT: layout = {2, {8, 8}, Kind::Uint}; return true;
        case VK_FORMAT_R8G8_SINT: layout = {2, {8, 8}, Kind::Sint}; return true;
        case VK_FORMAT_R32_UINT: layout = {1, {32}, Kind::Uint}; return true;
        case VK_FORMAT_R32_SINT: layout = {1, {32}, Kind::Sint}; return true;
        case VK_FORMAT_R32_SFLOAT: layout = {1, {32}, Kind::Float}; return true;
        case VK_FORMAT_R16G16_UNORM: layout = {2, {16, 16}, Kind::Unorm}; return true;
        case VK_FORMAT_R16G16_SNORM: layout = {2, {16, 16}, Kind::Snorm}; return true;
        case VK_FORMAT_R16G16_UINT: layout = {2, {16, 16}, Kind::Uint}; return true;
        case VK_FORMAT_R16G16_SINT: layout = {2, {16, 16}, Kind::Sint}; return true;
        case VK_FORMAT_R16G16_SFLOAT: layout = {2, {16, 16}, Kind::Float}; return true;
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32: case VK_FORMAT_A2R10G10B10_UNORM_PACK32: layout = {4, {10, 10, 10, 2}, Kind::Unorm}; return true;
        case VK_FORMAT_A2B10G10R10_UINT_PACK32: layout = {4, {10, 10, 10, 2}, Kind::Uint}; return true;
        case VK_FORMAT_R8G8B8A8_UNORM: case VK_FORMAT_R8G8B8A8_SRGB: case VK_FORMAT_B8G8R8A8_UNORM: case VK_FORMAT_B8G8R8A8_SRGB: layout = {4, {8, 8, 8, 8}, Kind::Unorm}; return true;
        case VK_FORMAT_R8G8B8A8_SNORM: layout = {4, {8, 8, 8, 8}, Kind::Snorm}; return true;
        case VK_FORMAT_R8G8B8A8_UINT: layout = {4, {8, 8, 8, 8}, Kind::Uint}; return true;
        case VK_FORMAT_R8G8B8A8_SINT: layout = {4, {8, 8, 8, 8}, Kind::Sint}; return true;
        case VK_FORMAT_R32G32_UINT: layout = {2, {32, 32}, Kind::Uint}; return true;
        case VK_FORMAT_R32G32_SINT: layout = {2, {32, 32}, Kind::Sint}; return true;
        case VK_FORMAT_R32G32_SFLOAT: layout = {2, {32, 32}, Kind::Float}; return true;
        case VK_FORMAT_R16G16B16A16_UNORM: layout = {4, {16, 16, 16, 16}, Kind::Unorm}; return true;
        case VK_FORMAT_R16G16B16A16_SNORM: layout = {4, {16, 16, 16, 16}, Kind::Snorm}; return true;
        case VK_FORMAT_R16G16B16A16_UINT: layout = {4, {16, 16, 16, 16}, Kind::Uint}; return true;
        case VK_FORMAT_R16G16B16A16_SINT: layout = {4, {16, 16, 16, 16}, Kind::Sint}; return true;
        case VK_FORMAT_R16G16B16A16_SFLOAT: layout = {4, {16, 16, 16, 16}, Kind::Float}; return true;
        case VK_FORMAT_R32G32B32A32_UINT: layout = {4, {32, 32, 32, 32}, Kind::Uint}; return true;
        case VK_FORMAT_R32G32B32A32_SINT: layout = {4, {32, 32, 32, 32}, Kind::Sint}; return true;
        case VK_FORMAT_R32G32B32A32_SFLOAT: layout = {4, {32, 32, 32, 32}, Kind::Float}; return true;
        default: return false;
    }
}

// "1" in a channel of this kind and width.
std::uint64_t One(Kind kind, std::uint32_t bits) {
    switch (kind) {
        case Kind::Unorm: case Kind::Uint: return bits >= 64 ? ~0ull : (1ull << bits) - 1u;
        case Kind::Snorm: case Kind::Sint: return (1ull << (bits - 1u)) - 1u;
        case Kind::Float: return bits == 16 ? 0x3c00u : 0x3f800000u;
    }
    return 0;
}

}

const char* DccKeysName(DccKeys keys) {
    switch (keys) {
        case DccKeys::Uncompressed: return "uncompressed";
        case DccKeys::Clear0000: return "0000";
        case DccKeys::Clear0001: return "0001";
        case DccKeys::Clear1110: return "1110";
        case DccKeys::Clear1111: return "1111";
        case DccKeys::ClearRegister: return "register";
        case DccKeys::Mixed: return "mixed";
        case DccKeys::Unreadable: return "unreadable";
    }
    return "?";
}

std::size_t DccKeyBytes(std::uint64_t surfaceBytes) {
    return static_cast<std::size_t>(surfaceBytes / KeyBytes);
}

namespace {

// ReadDccKeys, saying in `memoized` whether the answer came from the pending-store memo rather
// than the bytes.
DccKeys readDccKeys(std::uint64_t metaAddress, std::uint64_t surfaceBytes, bool& memoized) {
    memoized = false;
    const auto count = static_cast<std::size_t>(surfaceBytes / KeyBytes);
    if (metaAddress == 0 || count == 0 || !GuestMemory::Accessible(reinterpret_cast<const void*>(metaAddress), count)) return DccKeys::Unreadable;
    // A driver store still pending on the GPU is not in the bytes yet.
    if (const auto memo = MemoizedKeys(metaAddress, metaAddress + count)) {
        memoized = true;
        return *memo;
    }
    // Keys under a unit shadow's fresh results (metadata aliased with a surface): under the
    // device lock they are published and waited for; a build's unlocked stage answers "unstable"
    // so its stage B reads them under the lock.
    if (AnyShadowedOverlaps(metaAddress, count)) {
        if (!GuestMemory::GpuMutex().HeldByThisThread()) {
            memoized = true;
            return DccKeys::Mixed;
        }
        PublishShadow(metaAddress, count, PublishScope::Whole, PublishReason::Keys);
        if (auto* recorder = Recorder::Active()) {
            Recorder::CountSync(2);
            recorder->Sync();
        }
    }
    const auto* keys = reinterpret_cast<const std::uint8_t*>(metaAddress);
    const auto first = keys[0];
    const auto start = ScanProfileEnabled() ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
    const bool uniform = AllKeysEqual(keys + 1, count - 1, first);
    if (ScanProfileEnabled()) CountScan(count, start);
    return uniform ? ByteKeys(first) : DccKeys::Mixed;
}

DccKeys textureClearKeys(const GuestTextureResource& resource, std::uint64_t guestBytes, bool& memoized) {
    memoized = false;
    if (resource.dccAddress == 0) return DccKeys::Uncompressed;
    const auto keys = readDccKeys(resource.dccAddress, guestBytes, memoized);
    if (keys == DccKeys::Uncompressed) return keys;
    if (IsConvertedTextureFormat(resource.format) && (keys == DccKeys::Clear0001 || keys == DccKeys::Clear1110)) throw std::runtime_error(std::string("AGC graphics: DCC clear code ") + DccKeysName(keys) + " of converted texture format " + std::to_string(resource.format) + " is not implemented");
    std::byte probe[16]{};
    if (!IsDccClear(keys) || !FillDccClear(ResolveTextureFormat(resource.format), keys, resource.dccAlphaOnMsb, std::span(probe, std::min<std::size_t>(sizeof(probe), BytesPerElement(resource.format))))) {
        static std::mutex reportedMutex;
        static std::set<std::pair<std::uint64_t, int>> reported;
        std::lock_guard lock(reportedMutex);
        if (reported.size() < 32 && reported.insert({resource.baseAddress, static_cast<int>(keys)}).second) std::fprintf(stderr, "[gpu] texture 0x%llx (format %u) has %s DCC keys at 0x%llx; its texels are read as stored\n", static_cast<unsigned long long>(resource.baseAddress), resource.format, DccKeysName(keys), static_cast<unsigned long long>(resource.dccAddress));
        return DccKeys::Uncompressed;
    }
    return keys;
}

}

DccKeys ReadDccKeys(std::uint64_t metaAddress, std::uint64_t surfaceBytes) {
    bool memoized = false;
    return readDccKeys(metaAddress, surfaceBytes, memoized);
}

DccKeys CurrentDccKeys(std::uint64_t metaAddress, std::uint64_t surfaceBytes) {
    const auto count = static_cast<std::size_t>(surfaceBytes / KeyBytes);
    if (metaAddress != 0 && count != 0 && GuestMemory::GpuMutex().HeldByThisThread()) {
        if (auto* recorder = Recorder::Active(); recorder != nullptr && recorder->PendingWriteOverlaps(metaAddress, count)) {
            if (const auto keys = PendingStoreKeys(*recorder, metaAddress, count)) return *keys;
            Recorder::CountSync(2);
            recorder->SyncThrough(metaAddress, count);
        }
    }
    return ReadDccKeys(metaAddress, surfaceBytes);
}

bool IsDccClear(DccKeys keys) {
    return keys == DccKeys::Clear0000 || keys == DccKeys::Clear0001 || keys == DccKeys::Clear1110 || keys == DccKeys::Clear1111 || keys == DccKeys::ClearRegister;
}

void MarkDccUncompressed(std::uint64_t metaAddress, std::uint64_t surfaceBytes) {
    const auto count = static_cast<std::size_t>(surfaceBytes / KeyBytes);
    if (metaAddress == 0 || count == 0 || !GuestMemory::Accessible(reinterpret_cast<const void*>(metaAddress), count, true)) return;
    // The bytes are trusted only when no recorded work writes them (lock-free, with or without
    // GuestMemory::GpuMutex); otherwise the store runs and waits through the flush hook. A pending
    // GPU store of the driver's own is no reason to skip: a title kernel recorded into the same
    // batch after it may write clear keys, and the memo cannot order inside a batch.
    if (!Recorder::SnapshotWriteOverlaps(metaAddress, count) && AlreadyUncompressed(metaAddress, count)) return;
    StoreUncompressedOnCpu(metaAddress, count);
}

void MarkDccUncompressed(const Context& context, std::uint64_t metaAddress, std::uint64_t surfaceBytes) {
    const auto count = static_cast<std::size_t>(surfaceBytes / KeyBytes);
    if (metaAddress == 0 || count == 0 || !GuestMemory::Accessible(reinterpret_cast<const void*>(metaAddress), count, true)) return;
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
    if (!IsDccClear(keys) || keys == DccKeys::ClearRegister || !LayoutFor(format, layout)) return false;
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
    for (std::size_t offset = 0; offset + element.size() <= bytes.size(); offset += element.size()) std::memcpy(bytes.data() + offset, element.data(), element.size());
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
    if (proof.generation != 0 && collected != 0 && GuestMemory::UnchangedSince(resource.dccAddress, count, proof.generation)) {
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
    proof = stable ? DccKeyProof{keys, collected} : DccKeyProof{};
    counters.scanned.fetch_add(1, std::memory_order_relaxed);
    if (!stable) counters.unstable.fetch_add(1, std::memory_order_relaxed);
    return keys;
}

DccKeyProofCounts KeyProofCounts() {
    const auto& counters = Proofs();
    return {counters.proved.load(std::memory_order_relaxed), counters.scanned.load(std::memory_order_relaxed), counters.unstable.load(std::memory_order_relaxed)};
}

void ReadTextureSurface(const GuestTextureResource& resource, DccKeys keys, std::span<std::byte> bytes) {
    // Named for the [hooksync] attribution: the read goes through the flush hook.
    const GuestMemory::ReadSiteScope site(GuestMemory::ReadSite::TextureRead);
    if (keys == DccKeys::Uncompressed) GuestMemory::ReadCommitted(resource.baseAddress, bytes);
    else FillDccClear(ResolveTextureFormat(resource.format), keys, resource.dccAlphaOnMsb, bytes);
}

namespace {

DccKeys EncodedKeys(const GuestTextureResource& resource, DccKeys keys) {
    std::byte probe[16]{};
    if (!IsDccClear(keys) || !FillDccClear(ResolveTextureFormat(resource.format), keys, resource.dccAlphaOnMsb, std::span(probe, std::min<std::size_t>(sizeof(probe), BytesPerElement(resource.format))))) return DccKeys::Uncompressed;
    return keys;
}

bool PendingFillsLeaveUncompressed(const Recorder& recorder, const GuestTextureResource& resource, std::size_t count) {
    const auto begin = resource.dccAddress;
    if (!GuestMemory::Accessible(reinterpret_cast<const void*>(begin), count) || AnyShadowedOverlaps(begin, count)) return false;
    auto writes = recorder.PendingWritesOver(begin, count);
    std::sort(writes.begin(), writes.end(), [](const Recorder::PendingWrite& a, const Recorder::PendingWrite& b) { return a.note > b.note; });
    std::vector<std::pair<std::uint64_t, std::uint64_t>> unwritten{{begin, begin + count}};
    int value = -1;
    bool mixed = false;
    const auto see = [&](int key) {
        if (value < 0) value = key;
        else if (value != key) mixed = true;
    };
    for (const auto& write : writes) {
        if (mixed || unwritten.empty()) break;
        std::vector<std::pair<std::uint64_t, std::uint64_t>> rest;
        bool covered = false;
        for (const auto& [from, to] : unwritten) {
            if (write.end <= from || to <= write.begin) {
                rest.emplace_back(from, to);
                continue;
            }
            covered = true;
            if (from < write.begin) rest.emplace_back(from, write.begin);
            if (write.end < to) rest.emplace_back(write.end, to);
        }
        unwritten = std::move(rest);
        if (covered && write.value >= 0) see(write.value);
    }
    for (const auto& [from, to] : unwritten) {
        if (mixed) break;
        const auto* keys = reinterpret_cast<const std::uint8_t*>(from);
        if (AllKeysEqual(keys + 1, static_cast<std::size_t>(to - from) - 1, keys[0])) see(keys[0]);
        else mixed = true;
    }
    return mixed || (value >= 0 && EncodedKeys(resource, ByteKeys(static_cast<std::uint8_t>(value))) == DccKeys::Uncompressed);
}

}

std::optional<DccKeys> WaitForKeyWriters(const GuestTextureResource& resource, std::uint64_t guestBytes) {
    const auto count = static_cast<std::size_t>(guestBytes / KeyBytes);
    const auto metaAddress = resource.dccAddress;
    if (metaAddress == 0 || count == 0 || !GuestMemory::GpuMutex().HeldByThisThread()) return std::nullopt;
    auto* recorder = Recorder::Active();
    if (recorder == nullptr || !recorder->PendingWriteOverlaps(metaAddress, count) || PendingStoreKeys(*recorder, metaAddress, count).has_value()) return std::nullopt;
    if (PendingFillsLeaveUncompressed(*recorder, resource, count)) return DccKeys::Uncompressed;
    Recorder::CountSync(2);
    recorder->SyncThrough(metaAddress, count);
    return std::nullopt;
}

}
