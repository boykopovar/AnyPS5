#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SHADERMEMORY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_SHADERMEMORY_HPP

#include "Recompiler.hpp"
#include <array>
#include <bitset>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace ShaderRecompiler {
struct SourceHandle;
}

namespace AgcDriver {

// Records the guest words a shader's resource analysis reads, as the memory regions its recompile
// request carries. Guest memory is fetched a 4 KiB page at a time; the regions report exactly the
// dwords read, so cache keys built from them do not change with unrelated bytes nearby. A page that
// `pendingWrite` reports as written by recorded GPU work is read word by word instead, so only a
// dword the capture needs waits for that work through the flush hook, not the whole page; a dword
// the query knows to be left unchanged by that work (Driver.cpp's written-element evidence) is read
// raw, without the hook. Debug aid: APS5_NO_WORDWISE_CAPTURE=1 fetches every page whole.
class ShaderMemory {
public:
    // The driver's answer for a range: nothing recorded writes it; recorded work writes it and the
    // read must go through the flush hook; recorded work writes it but leaves it unchanged, so the
    // bytes are read raw; VerifyRaw reads raw and through the hook, counting a difference;
    // KnownValue: recorded work writes it with bytes the driver already knows (a copy HLE's
    // destination, Driver.cpp's known-value ring entries), copied into `known` when the caller
    // passes a span of exactly `bytes` (empty: the answer alone); VerifyKnownValue serves them and
    // reads through the hook too, counting a difference.
    enum class PendingWrite : std::uint8_t { None, Sync, Raw, VerifyRaw, KnownValue, VerifyKnownValue };
    using PendingWriteQuery = PendingWrite (*)(std::uint64_t address, std::size_t bytes, std::span<std::byte> known);
    // Words served from known values by every capture, and the verified ones and their mismatches
    // (APS5_VERIFY_KNOWN_VALUES=1), for Driver.cpp's [copy] line.
    struct KnownValueCounts {
        std::uint64_t served, verified, mismatches;
    };
    static KnownValueCounts KnownValues();
    // Told, for a dword read that went through the hook, whether the bytes read before the hook's
    // wait were still there after it (the driver's evidence for later raw reads).
    using PendingWriteObserver = void (*)(std::uint64_t address, bool unchanged);
    // The calling thread's count of hook syncs that waited for unfinished GPU work: a read is only
    // observed when the count moved across it (against finished work both reads see the GPU's
    // bytes, and "unchanged" would be no evidence). Null observes every read that went through
    // the hook.
    using HookWaitCounter = std::uint64_t (*)();
    // The calling thread's GPU waits so far in milliseconds (Recorder::ThreadWaitedMs), set once
    // by the driver: the [capture] line charges the waits made inside a capture to it.
    using WaitedMsProvider = double (*)();
    static void SetWaitedMsProvider(WaitedMsProvider provider);
    explicit ShaderMemory(std::span<const ShaderRecompiler::MemoryRegion> initial, PendingWriteQuery pendingWrite = nullptr, PendingWriteObserver observe = nullptr, HookWaitCounter hookWaits = nullptr);
    ~ShaderMemory();
    ShaderMemory(const ShaderMemory&) = delete;
    ShaderMemory& operator=(const ShaderMemory&) = delete;
    ShaderMemory(ShaderMemory&& other) noexcept;
    ShaderMemory& operator=(ShaderMemory&&) = delete;
    // Returns what the capture resolved (plan, snapshot, specialization) for
    // ShaderRecompiler::Recompile(request, capture), which then skips its own materialization. One
    // result per call: the draw path captures several stages on one ShaderMemory. With `handle`
    // (the driver's memoized ShaderRecompiler::ResolveSource result) the capture skips the source
    // resolution.
    std::shared_ptr<const ShaderRecompiler::ResourceCapture> Capture(const ShaderRecompiler::RecompileRequest& request, const ShaderRecompiler::SourceHandle* handle = nullptr);
    [[nodiscard]] std::vector<ShaderRecompiler::MemoryRegion> Regions() const;
    // The page regions read since the previous call (or construction), a word read again
    // included, the initial regions excluded: one stage's own reads on the draw path's shared
    // ShaderMemory (Regions() stays the union). The spans point into the pages, as Regions()'s do.
    [[nodiscard]] std::vector<ShaderRecompiler::MemoryRegion> TakeRecentRegions();
    // The driver's source handle memo outcomes, for the [capture] line (APS5_PROFILE_DRAW).
    static void CountHandleMemo(bool hit);

private:
    static constexpr std::size_t PageBytes = 4096;
    static constexpr std::size_t PageWords = PageBytes / sizeof(std::uint32_t);

    class WordMask {
    public:
        [[nodiscard]] bool Test(std::size_t index) const { return (bits[index / 64u] >> (index % 64u) & 1u) != 0u; }
        void Set(std::size_t index) { bits[index / 64u] |= std::uint64_t{1} << (index % 64u); }
        void SetAll() { bits.fill(~std::uint64_t{0}); }
        void SetBlock(std::size_t index) { bits[index / 64u] |= ((std::uint64_t{1} << BlockWords) - 1u) << (index % 64u / BlockWords * BlockWords); }
        void Reset() { bits.fill(0u); }
        [[nodiscard]] bool None() const;
        template<typename TRun>
        void ForEachRun(TRun&& run) const;

    private:
        std::array<std::uint64_t, PageWords / 64u> bits{};
    };

    static constexpr std::size_t BlockWords = 16;
    struct Page {
        std::array<std::uint32_t, PageWords> words;
        WordMask valid;
        WordMask read;
        WordMask recent;
        bool wordwise = false;
        bool lazy = false;
    };

    static bool read(void* context, std::uint64_t address, std::uint32_t* value);
    Page& page(std::uint64_t base);

    // Regions given at construction (the registered shader's code and header), referenced as given:
    // the caller keeps them alive for as long as the capture is used.
    std::map<std::uint64_t, std::span<const std::byte>> initial;
    std::vector<std::pair<std::uint64_t, Page*>> pages;
    std::uint64_t lastBase = ~std::uint64_t{0};
    Page* lastPage = nullptr;
    PendingWriteQuery pendingWrite = nullptr;
    PendingWriteObserver observe = nullptr;
    HookWaitCounter hookWaits = nullptr;
};

// The positions, among a dispatch-cache variant's stored words, of the pure flat-SRT leaves a
// capture read (ShaderRecompiler::SrtReadTrace), for a hit that differs only there. `runs` are the
// variant's [begin, end) byte ranges in address order and `words` their dwords in that order;
// `leaves` are (flat offset, address) and `otherReads` the sorted addresses of every other read.
// A leaf is skipped when its address is among the other reads (a walk read aliases the dword,
// counted `aliased`), is not dword-aligned or lies in no run (counted `unmapped`), or its stored
// word differs from the flattened SRT's at that offset (counted `mismatched`). `positions`
// (sorted, may repeat) and `slots` (the flat offset of each) are parallel.
struct DataWordPositionCounts {
    std::uint64_t unmapped = 0;
    std::uint64_t mismatched = 0;
    std::uint64_t aliased = 0;
};
DataWordPositionCounts DataWordPositions(std::span<const std::pair<std::uint64_t, std::uint64_t>> runs, std::span<const std::pair<std::uint32_t, std::uint64_t>> leaves, std::span<const std::uint64_t> otherReads, std::span<const std::uint32_t> words, std::span<const std::uint32_t> flattenedSrt, std::vector<std::uint32_t>& positions, std::vector<std::uint32_t>& slots);

}

#endif
