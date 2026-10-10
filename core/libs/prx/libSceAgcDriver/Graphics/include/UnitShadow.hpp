#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_UNITSHADOW_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_UNITSHADOW_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics {

struct HostImport;

// Device-local tiled shadows of host imports, indexed by 64 KiB unit (one unit = one write-tracker
// block; unit index of guest address a inside a shadow = (a - base) / 65536 with base = the import's
// base rounded down to 64 KiB). A storage image's GPU-direct write-back retiles its pending units into
// the shadow instead of the import, and a later upload of any image over those units detiles from the
// shadow: the title's aliased images then exchange their results in device memory, and only a host-
// facing consumer of the bytes (the flush hook, a V# bound in place, indirect arguments, a copy, a
// label or key store over the range, scanout, the import's retirement) publishes them into the
// import with one vkCmdCopyBuffer per slab.
//
// Freshness is derived from the write tracker, never stored: a unit is shadowed ("fresh") while its
// slab bytes were written at a tracker generation no block stamp exceeds (MarkShadowed takes the
// generation the write-back settled at, after its own MarkWritten); every import writer already
// stamps its blocks (labels, fills, copies, key stores, in-place V# writes, CPU stores), so a stale
// unit reads from the import as today. A CPU store stamps only when a collect walks its page, so a
// publish and a seed decision collect the units they judge first (as the write-back's keep decision
// does): the CPU's bytes win the unit. A publish records the copy and marks the unit published (the
// slab and the import agree until the next retile or stamp); a stale unit met by a publish is dropped
// ("stale-dropped": its generation is cleared). A publish never stamps.
//
// Slabs are 8 MiB pieces of a shadow (APS5_UNIT_SHADOW_SLAB_MIB) with their own VkDeviceMemory,
// made on the first retile into them under a budget (APS5_UNIT_SHADOW_MIB, default 1024, LRU
// eviction with a publish of the evicted slab's fresh units). APS5_NO_UNIT_SHADOW=1 makes every
// primitive inert. Every call but AnyShadowedOverlaps runs under GuestMemory::GpuMutex (they record
// or mutate freshness); the registry's own mutex is a leaf (lock order GpuMutex -> Imports/Pending ->
// Shadows -> tracker).
bool UnitShadowEnabled();

// None: a device-facing caller (a storage upload reads the slab itself); Whole: a reader of every
// byte of the range; PartialUnits: a writer of the range (only units it covers partly are published,
// so the neighbours' results in the rest of the unit reach the import before the write stamps it).
enum class PublishScope : std::uint8_t { None, Whole, PartialUnits };
enum class PublishReason : std::uint8_t { Hook, Region, Indirect, CopySource, CopyDestination, Fill, FillClear, Label, Keys, Scanout, Validate, Upload, TailMip, Evict, Retire, Teardown, Count };
// The reason for a FlushPending reason string ("memory access" -> Hook, ...).
PublishReason PublishReasonFor(const char* flushReason);

struct ShadowSlab {
    ShadowSlab(const Context& context, VkBuffer buffer, VkDeviceMemory memory, std::uint64_t firstUnit, std::uint32_t units);
    ShadowSlab(const ShadowSlab&) = delete;
    ShadowSlab& operator=(const ShadowSlab&) = delete;
    // Destroys the handles: safe on the recorder's release thread (Keep's contract).
    ~ShadowSlab();
    Context context;
    VkBuffer buffer;
    VkDeviceMemory memory;
    std::uint64_t firstUnit;
    std::uint32_t units;
    std::uint64_t lastUse = 0;
    // Destinations handed out for a write-back in progress: such a slab is never evicted.
    std::atomic<std::uint32_t> pins{0};
};

// Holds one pin of a slab (ShadowDestination::pin).
struct ShadowSlabPin {
    explicit ShadowSlabPin(std::shared_ptr<ShadowSlab> slab);
    ShadowSlabPin(const ShadowSlabPin&) = delete;
    ShadowSlabPin& operator=(const ShadowSlabPin&) = delete;
    ~ShadowSlabPin();
    std::shared_ptr<ShadowSlab> slab;
};

// A retile piece (guest range) and the slab its copies were recorded into.
struct ShadowedRange {
    std::uint64_t begin;
    std::uint64_t end;
    std::shared_ptr<ShadowSlab> slab;
};

// Records vkCmdCopyBuffer(slab -> import) for every fresh unpublished unit the scope selects over
// [address, address + bytes) into the active recorder's open batch (a waited CommandBatch without
// one), with the barriers, the pending-write note and the timing class of a fill; marks them
// published, clears stale ones. Returns the units copied. Inside a completion action a store's
// publish records nothing (the store's own stamp makes the unit stale; a recorded publish would land
// over the store later).
std::size_t PublishShadow(std::uint64_t address, std::size_t bytes, PublishScope scope, PublishReason reason);
// No GpuMutex, no tracker: conservative (a unit with a generation and no publish counts), for
// hook-free readers and a build's stage A.
bool AnyShadowedOverlaps(std::uint64_t address, std::size_t bytes);
bool AnyShadowedOverlaps(std::span<const std::pair<std::uint64_t, std::uint64_t>> ranges);

// The sources of a storage upload's surface-relative runs: [begin, end) pieces of them, each read
// from one buffer (a slab while the unit is fresh, else the import) at `offset` (byte x of the run
// is at offset + (x - begin)), split at unit freshness and slab boundaries with one tracker query
// over the span. A tail block (surface-relative, every unit of it) must have one source: a mixed
// one is published (TailMip) and read from the import. `countReads` false: not counted as detiled
// bytes (a write-back reading the current bytes of its padding).
struct ShadowRun {
    std::uint64_t begin;
    std::uint64_t end;
    VkBuffer buffer;
    VkDeviceSize offset;
    bool shadow;
    std::shared_ptr<ShadowSlab> slab;
};
std::vector<ShadowRun> ShadowSources(const Context& context, const HostImport& import, std::uint64_t surfaceBase, std::span<const std::pair<std::uint64_t, std::uint64_t>> runs, std::span<const std::pair<std::uint64_t, std::uint64_t>> tailBlocks, bool countReads = true);

// The slab destination of a retile copy [begin, end) (guest addresses inside one slab: the caller
// splits at SlabBoundary), making the slab under the budget; nullopt when refused (the caller writes
// the import as before). `seedUnits` names the units the copy covers partly that are not fresh (as
// [begin, end) clipped to the import, collected first): the caller records import -> slab for each
// before its copy. The slab stays pinned against eviction while a copy of the destination lives:
// the caller keeps one until MarkShadowed (an evicted slab would take the retile's copies with it).
struct ShadowDestination {
    VkBuffer buffer;
    VkDeviceSize offset;
    std::shared_ptr<ShadowSlab> slab;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> seedUnits;
    std::shared_ptr<ShadowSlabPin> pin;
};
std::optional<ShadowDestination> ShadowDestinationFor(const Context& context, const HostImport& import, std::uint64_t begin, std::uint64_t end);
// A seed copy (import -> slab of a partly covered unit) the caller recorded: counted and traced.
void NoteShadowSeed(std::uint64_t begin, std::uint64_t end);
// The first slab boundary above `address` (a guest address inside the import).
std::uint64_t SlabBoundary(const HostImport& import, std::uint64_t address);
// The slab offset of a guest address for a slab returned by ShadowDestinationFor or ShadowSources.
VkDeviceSize SlabOffset(const HostImport& import, const ShadowSlab& slab, std::uint64_t address);
// After a retile's copies were recorded: the units of `ranges` (guest) hold the newest bytes at
// `generation`, unpublished. A unit whose slab is no longer the one the copies went into is not
// marked (counted `slab lost`). Bumps StorageTexture's pending serial.
void MarkShadowed(const HostImport& import, std::span<const ShadowedRange> ranges, std::uint64_t generation);
// retireImport: publishes the fresh units whose memory `registered` still names as a readable
// registered range into the (old) import buffer, hands the slabs to the batch and drops the shadow.
// The import retires because its registration vanished or changed size, so the rest of its memory
// belongs to the title again and is dropped (counted `dropped on retire`); `lost on retire` counts
// the units nothing could record.
void RetireShadow(VkDevice device, const HostImport& import, const std::function<bool(std::uint64_t, std::uint64_t)>& registered);
// Teardown: every shadow of the context's device is published; DestroyShadows then drops them.
void PublishAllShadows(const Context& context, PublishReason reason);
void DestroyShadows(VkDevice device);
// The [storage] line's shadow segment (10 s deltas, live slab totals).
std::string ShadowReport();
// APS5_SHADOW_VERIFY=1: after a shadow-sourced upload, the units read must carry no stamp newer
// than their generation (counted as mismatches otherwise).
bool ShadowVerify();

}

#endif
