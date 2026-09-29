#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_TEXTURE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_TEXTURE_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/UnitShadow.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <map>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics {

class ResidentColor;
class CommandBatch;
class StorageTexture;
struct HostImport;

// The Vulkan format storage images of a guest format use (sRGB formats store as their UNORM form).
// Throws when the format has no storage form; the query is cached per format (a lookup cost a
// vkGetPhysicalDeviceFormatProperties call before).
VkFormat StorageFormatForGuest(const Context& context, std::uint32_t guestFormat);
// Whether StorageFormatForGuest would succeed, without throwing (unknown guest formats included).
bool StorageFormatAvailable(const Context& context, std::uint32_t guestFormat);
// Whether a storage image of the guest format takes DCC clear `keys` as a GPU clear (see
// StorageTexture::upload); false for integer formats and non-clear keys.
bool StorageClearAvailable(const Context& context, std::uint32_t guestFormat, DccKeys keys);

// A sampled texture's own VkImage with its memory, shared with the recorder while a recorded upload
// still writes it (see the snapshot constructor), so the texture may go before the batch completes.
struct OwnedImage {
    OwnedImage(const Context& context, VkImage image, VkDeviceMemory memory) : context(context), image(image), memory(memory) {}
    OwnedImage(const OwnedImage&) = delete;
    OwnedImage& operator=(const OwnedImage&) = delete;
    ~OwnedImage() {
        if (image) context.Function<PFN_vkDestroyImage>("vkDestroyImage")(context.device, image, nullptr);
        if (memory) context.Function<PFN_vkFreeMemory>("vkFreeMemory")(context.device, memory, nullptr);
    }
    Context context;
    VkImage image;
    VkDeviceMemory memory;
};

class Texture {
public:
    Texture(const Context& context, TextureDetiler& detiler, const GuestTextureResource& descriptor, VkComponentMapping components, std::span<const std::byte> snapshot);
    Texture(const Context& context, const std::shared_ptr<ResidentColor>& source, const GuestTextureResource& descriptor, VkComponentMapping components);
    // A view of a storage image's own VkImage: the sampled texture follows the image's content, so a
    // compute pass writing it and the next pass sampling it share one image and copy nothing.
    // CanCopyFrom says whether the two descriptors address the same surface compatibly.
    Texture(const Context& context, const std::shared_ptr<StorageTexture>& source, const GuestTextureResource& descriptor, VkComponentMapping components);
    static bool CanCopyFrom(const StorageTexture& source, const GuestTextureResource& descriptor);
    ~Texture();
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    VkImageView View() const;
    // The layout the image is kept in while sampled.
    VkImageLayout Layout() const { return layout; }
    VkDeviceSize AllocationBytes() const { return allocationBytes; }
    // Whether this texture is a view of a storage image (no snapshot of its own).
    bool ViewsStorageImage() const { return storageSource != nullptr; }
    const StorageTexture* StorageSource() const { return storageSource.get(); }
    const std::shared_ptr<StorageTexture>& SharedStorageSource() const { return storageSource; }
    // The last key scan of the sampled surface (ProvedClearKeys), for surfaces whose keys are not
    // the source image's own (snapshots, views over other metadata); one per cache entry, so every
    // object binding the texture shares it. Under GuestMemory::GpuMutex only.
    DccKeyProof& KeyProof() const { return keyProof; }

private:
    void release() noexcept;

    // Held by value: cached textures outlive the Context of the draw that created them.
    Context context;
    mutable DccKeyProof keyProof;
    // The snapshot constructor's image (null for a view of a storage image); `owned` frees it.
    VkImage image = VK_NULL_HANDLE;
    std::shared_ptr<OwnedImage> owned;
    VkImageView view = VK_NULL_HANDLE;
    VkImageLayout layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkDeviceSize allocationBytes = 0;
    std::shared_ptr<ResidentColor> source;
    std::shared_ptr<StorageTexture> storageSource;
    std::unique_ptr<CommandBatch> upload;
};

// A guest texture a shader writes through a storage image. It is uploaded like a sampled texture;
// after the GPU work completes its results are stored to guest memory (retiled, changed bytes only),
// either at once (WriteBack) or deferred: MarkDirty keeps them on the GPU until something reads that
// memory (FlushPending, through the GuestMemory flush hook), the image is refreshed after a CPU write,
// or it leaves the cache. Dispatches reusing the image meanwhile skip the round trip entirely.
class StorageTexture : public std::enable_shared_from_this<StorageTexture> {
public:
    StorageTexture(const Context& context, TextureDetiler& detiler, const GuestTextureResource& descriptor, std::uint32_t mipLevel);
    ~StorageTexture();
    StorageTexture(const StorageTexture&) = delete;
    StorageTexture& operator=(const StorageTexture&) = delete;

    VkImageView View() const;
    // The image holds every mip of the surface; one storage view per written mip is made on demand,
    // so successive mip writes of a chain share one image and one write-back.
    VkImageView View(std::uint32_t mip);
    // Render targets live in the same images: draws attach mip 0 through a view of the color
    // buffer's format and mark the image dirty like a storage write.
    bool Attachable() const { return attachable; }
    VkImageView AttachmentView(VkFormat format, std::uint32_t mip = 0);
    void WriteBack();
    // Deferred write-back (APS5_EAGER_WRITEBACK=1 stores at once instead).
    void MarkDirty();
    void Flush();
    // Stores every pending image overlapping the range, except `except`; returns whether any was.
    // Stores into host-imported memory are recorded (not waited for): a CPU reader syncs afterwards.
    // Then the unit shadows over the range are published into their imports as `scope` says (see
    // UnitShadow.hpp; the reason string picks the publish reason), `published` receiving whether
    // any unit was: a reader of the import must sync for a publish as for a store. Only images
    // with a pending unit inside the range are stored (APS5_NO_FLUSH_PRETEST=1 lists every
    // overlapping pending image, as before).
    static bool FlushPending(std::uint64_t address, std::size_t bytes, const StorageTexture* except = nullptr, const char* reason = "memory access", PublishScope scope = PublishScope::Whole, bool* published = nullptr);
    static void FlushAllPending(const char* reason);
    // See PendingSerial: a change of a surface's source outside the registry (a unit shadow
    // retile) moves it too.
    static void BumpPendingSerial();
    // Whether an image with results pending, or one a FlushPending is storing right now, overlaps
    // any of the [begin, end) ranges: one acquisition of the registry's mutex.
    static bool AnyPendingOverlaps(std::span<const std::pair<std::uint64_t, std::uint64_t>> ranges);
    // Moves with every change of the pending registry (an image marked dirty, stored, flushed or
    // dropped; after the store for a flush), bumped under the registry's mutex with the change, so
    // a reader that loads it before and after its own reads knows the registry it consulted stayed
    // as it was, and one that finds it unchanged since an earlier read knows the registry is what
    // it was then.
    static std::uint64_t PendingSerial();
    // One registry scan for several ranges (the fast Revalidate): each query's `overlaps` receives
    // whether an image other than `except` has results pending, or is being stored right now, over
    // [begin, end), and `found` the image FindPending would return for the range (null: none);
    // returns whether every query's `pending` image, when given, is still that image.
    struct PendingQuery {
        std::uint64_t begin;
        std::uint64_t end;
        const StorageTexture* except;
        const StorageTexture* pending;
        bool overlaps;
        const StorageTexture* found = nullptr;
    };
    static bool ScanPending(std::span<PendingQuery> queries);
    // APS5_PROFILE_DRAW, for the flush hook's recorded-store attribution ([hooksync]): the pending
    // images overlapping a CPU access, whether a store of them would leave every 64 KiB block of the
    // access to the CPU (the write-back keep rule: a block of a pending layer stamped since the
    // layer's generation, after an uncached collect; a generation-0 layer is stored whole), and how
    // many of them no consumer proved current for DeadImagePresents presents. No state changes.
    struct AccessClassification {
        std::size_t images = 0;
        std::size_t dead = 0;
        std::size_t live = 0;
        bool allCpuWritten = false;
        // Accesses over a megabyte are not scanned.
        bool checked = false;
    };
    static constexpr std::uint64_t DeadImagePresents = 2;
    static void ClassifyAccess(std::uint64_t address, std::size_t bytes, AccessClassification& out);
    // The flush hook's skip (C4, Recorder.cpp FlushForAccess): whether every image FlushPending
    // would store for the access keeps every 64 KiB block of it (the keep rule of writeBackLayers:
    // stamped since the layer's generation, decided after an uncached collect over the access), so
    // the access reads the same bytes with or without the store and the images stay pending, dirty
    // and at their generation, except the layers a store for the access would leave empty-handed
    // (evictStale; `evicted` receives the images that left the registry so). False when no image
    // overlaps (`images` receives how many do), for an access over ClassifyLimit, or with a
    // generation-0 layer (stored whole). The decision is taken under GuestMemory::GpuMutex (a
    // short hold, no GPU wait), the collect before it.
    static constexpr std::size_t ClassifyLimit = 1u << 20u;
    static bool AccessKeptByCpu(std::uint64_t address, std::size_t bytes, std::size_t* images = nullptr, std::size_t* evicted = nullptr);
    // FlushPending's publish of the unit shadows over the range without the image stores (what it
    // does when it lists no image, and all an access AccessKeptByCpu approved needs): whether any
    // unit was published.
    static bool PublishShadowsOnly(std::uint64_t address, std::size_t bytes, PublishScope scope, const char* reason = "memory access");
    // For the [hooksync] recorded-store line: images a FlushPending stored after a hook skip of
    // theirs (AccessKeptByCpu), of which by the hook for the same read site as the last skip; 10 s deltas.
    struct HookSkipCounts {
        std::uint64_t flushedAfterSkip;
        std::uint64_t flushedAfterSkipSameSite;
    };
    static HookSkipCounts TakeHookSkipCounts();
    // A consumer found the image current and used it (Refresh, the fast revalidation).
    void NoteProved() const;
    // Whether the storage cache holds this image (set and cleared by the cache under its mutex, the
    // clear before the eviction's flush): what a lookup of its surface would return.
    bool Cached() const { return cached.load(std::memory_order_acquire); }
    void SetCached(bool value) { cached.store(value, std::memory_order_release); }
    // The pending image whose surface starts at `address` and covers at least `bytes` (a mip chain
    // contains a descriptor of its first mips), if any; the caller decides whether the geometry fits
    // (Texture::CanCopyFrom, ResidentPresentable).
    static std::shared_ptr<StorageTexture> FindPending(std::uint64_t address, std::uint64_t bytes);
    // How a buffer fill of [address, address + bytes) (Driver.cpp's fill HLE) meets the storage
    // images alive: none of them; a whole surface (Exact) or one array layer of a surface (Layer),
    // `image` and `layer` naming it, whatever other images lie over the range (`others`, of which
    // `inside` wholly inside it: stale images of earlier uses of the memory, which the title's
    // transient allocator hands out again); otherwise part of one image (the fill inside the
    // surface, the surface inside the fill, or straddling), several, or none but exactly one
    // surface's DCC metadata. Images the cache let go (see Flush) do not count.
    enum class FillCover { None, Exact, Inside, Around, Straddle, Several, Keys, Layer };
    struct FillCoverage {
        FillCover cover = FillCover::None;
        std::shared_ptr<StorageTexture> image;
        std::uint32_t layer = 0;
        std::size_t others = 0;
        std::size_t inside = 0;
    };
    static FillCoverage ClassifyFill(std::uint64_t address, std::size_t bytes);
    // Results pending in images lying wholly inside the range are dead (a fill overwrites every
    // byte of them): they are dropped instead of stored. Returns how many images were.
    static std::size_t DiscardPendingInside(std::uint64_t address, std::size_t bytes);
    // A fill of the whole surface (`layer` WholeImage) or of one array layer with the 16-byte
    // `pattern`, done as a GPU clear of the image instead of a store into guest memory: the
    // pattern must be one texel of the storage format repeated, exactly representable as a clear
    // value, the surface's DCC keys (if any) must read as uncompressed and not be written by
    // recorded work, the memory must be host-imported so the write-back that eventually publishes
    // the texels is GPU-direct, and for one layer the rest of the surface must be unchanged since
    // the image matched it. Records into the active recorder under GuestMemory::GpuMutex and leaves
    // the image dirty like a shader write: the guest bytes follow through the deferred write-back,
    // the range is stamped written so every other reader of it (other images over the memory,
    // captures, snapshots) takes the bytes through the flush hook, and this image alone is current.
    // False, naming why in `refusal`, when the fill must be stored as before.
    static constexpr std::uint32_t WholeImage = ~0u;
    bool FillClear(std::span<const std::uint32_t, 4> pattern, std::uint32_t layer, const char*& refusal);
    // The cached, live image whose surface is exactly [address, address + bytes), if any (the newest
    // of several: see ClassifyFill).
    static std::shared_ptr<StorageTexture> FindLive(std::uint64_t address, std::uint64_t bytes);
    // Whether the two surfaces differ in nothing but their address (and DCC metadata).
    bool SameSurfaceShape(const StorageTexture& other) const;
    // The copy HLE's buffer copy of one whole surface into another (VulkanDevice::CopyBuffer,
    // APS5_NO_COPY_ALIAS=1 disables): the source image's content is copied on the device into this
    // image, which is then dirty like a shader-written one (the guest bytes follow through the
    // deferred write-back; the range is stamped so every other reader takes them through the flush
    // hook), and the source's pending results stay pending. The shapes must match, neither surface
    // may carry DCC metadata, and this surface must be host-imported (the write-back is GPU-direct).
    // False, naming why, when the copy must be a transfer.
    bool CopyFrom(StorageTexture& source, const char*& refusal);
    VkImage Image() const { return image; }
    const GuestTextureResource& Descriptor() const { return descriptor; }
    std::uint32_t ImageLayers() const { return geometry.imageLayers; }
    std::uint32_t ImageDepth() const { return geometry.imageDepth; }
    // Content version: advances when the image is re-uploaded or a shader wrote it. Together with
    // Generation (the write generation guest memory was last known to match the content at) it
    // validates textures copied from this image.
    std::uint64_t Version() const { return version; }
    std::uint64_t Generation() const { return generation; }
    // The DCC keys the content was uploaded under and the last scan of the surface's keys
    // (ProvedClearKeys): Refresh's own key rule for the fast revalidation. Under
    // GuestMemory::GpuMutex only, as Refresh is; never from a build's stage A.
    DccKeys UploadedKeys() const { return uploadedKeys; }
    DccKeyProof& KeyProof() const { return keyProof; }
    // Brings the image up to date with guest memory before another use; returns whether its content
    // was still current (nothing uploaded).
    // Keeps the image current with guest memory (see GuestMemory::CollectWrites).
    bool Refresh();
    std::uint64_t GuestBytes() const;

private:
    // The regions of every array layer, or of the tracked layers `layers` selects.
    std::vector<VkBufferImageCopy> CopyRegions(const std::vector<bool>* layers = nullptr) const;
    // Uploads the surface, or only the tracked layers `layers` selects (the direct path; the others
    // upload everything).
    void upload(const std::vector<bool>* layers = nullptr);
    // Stores the pending tracked layers overlapping [address, address + bytes) to guest memory; in
    // each, 64 KiB blocks the CPU wrote since the layer's generation keep the CPU's bytes. Block
    // units asked for in pieces too often are all stored at once for a while (the hysteresis:
    // APS5_BLOCK_WRITEBACK_PIECES within APS5_BLOCK_WRITEBACK_FRAMES presents, defaults 4 and 8;
    // APS5_BLOCK_WRITEBACK_EACH=1 stores the touched units only).
    void writeBack(std::uint64_t address, std::size_t bytes);
    void writeBackLayers(const std::vector<bool>& layers);
    // Tracked units as 64 KiB write-stamp blocks (`blockUnits`: a thin tiled surface at a 64 KiB
    // aligned base; APS5_NO_BLOCK_TRACKING=1 tracks array layers as above instead): a fill of one
    // layer, a CPU write or another image's store then costs the blocks it touched, moved through
    // windows of their (layer, mip) slices (whole tile blocks in whole rows, see
    // TextureDetiler::DetileWindow), not the surface. The last unit may be short.
    std::uint64_t layerBytes(std::uint32_t layer) const { return std::min<std::uint64_t>(trackedLayerBytes, guestBytes - static_cast<std::uint64_t>(layer) * trackedLayerBytes); }
    // Contiguous runs of the selected units, surface-relative [begin, end).
    std::vector<std::pair<std::uint64_t, std::uint64_t>> unitRuns(const std::vector<bool>& units) const;
    struct SliceWindow {
        std::uint32_t layer;
        std::uint32_t level;
        // The window's tiled bytes, surface-relative, and the detiler window (mip-relative) that
        // moves them out of a linear window of `linearBytes`.
        std::uint64_t tiledBegin;
        std::uint64_t tiledEnd;
        DetileWindow window;
        std::uint64_t linearBytes;
        // Its tile blocks as buffer-image regions, bufferOffset relative to the linear window.
        std::vector<VkBufferImageCopy> regions;
    };
    // The windows moving the surface-relative runs: whole tile blocks of thin tiled mips, a tail
    // mip whole (its block holds every tail level), in slice order.
    std::vector<SliceWindow> sliceWindows(std::span<const std::pair<std::uint64_t, std::uint64_t>> runs) const;
    // The direct upload and GPU-direct write-back of the runs through their windows (block units);
    // both return the bytes moved. The upload reads each unit from its unit shadow while fresh,
    // else from the import (`discard`: the image has no layout yet, a whole-surface first upload).
    // The write-back retiles into the import's unit shadow where a slab takes the piece
    // (`shadowed` receives those guest ranges, `imported` the pieces written to the import).
    std::uint64_t uploadWindows(const HostImport& import, std::span<const std::pair<std::uint64_t, std::uint64_t>> runs, bool discard = false);
    std::uint64_t writeBackWindows(const HostImport& import, std::span<const std::pair<std::uint64_t, std::uint64_t>> keep, std::uint64_t firstStored, std::uint64_t lastStored, std::vector<ShadowedRange>& shadowed, std::vector<std::pair<std::uint64_t, std::uint64_t>>& imported);
    // Per-layer validity (thin array surfaces whose layers are 64 KiB-aligned guest slices; every
    // other surface is one tracked layer): a layer's generation is the write generation its guest
    // bytes were last known to match the image at, and a pending layer holds results (a clear, a
    // shader write, a device copy) guest memory has not received. `generation` stays the oldest
    // layer generation (the whole-surface fast check), `dirty` whether any layer is pending.
    std::uint64_t layerBegin(std::uint32_t layer) const { return descriptor.baseAddress + static_cast<std::uint64_t>(layer) * trackedLayerBytes; }
    bool anyLayerPending() const;
    void refreshGeneration();
    // Marks `count` tracked layers from `first` pending and registers the image (MarkDirty's
    // registration; APS5_EAGER_WRITEBACK=1 stores at once instead).
    void markLayersPending(std::uint32_t first, std::uint32_t count);
    // Registers or unregisters the image as its pending layers say (after a write-back).
    void reconcilePending();
    // Pending images whose surface shares one of the 64 KiB write-stamp blocks firstBlock..lastBlock
    // (the span a write-back stamps) without overlapping this one: the tracked layer of theirs
    // holding the shared block, when its memory is unchanged since its generation (see writeBack),
    // with the generation seen at the check.
    struct Adjacent {
        std::shared_ptr<StorageTexture> texture;
        std::uint32_t layer;
        std::uint64_t generation;
    };
    std::vector<Adjacent> adjacentPendingUnchanged(std::uint64_t firstBlock, std::uint64_t lastBlock) const;
    // Advances the entries' layers still at their seen generation to `now`, the generation this
    // write-back's stamps of blocks firstBlock..lastBlock are below, unless the layer's other
    // blocks were stamped since the check.
    static void advanceAdjacent(const std::vector<Adjacent>& adjacent, std::uint64_t now, std::uint64_t firstBlock, std::uint64_t lastBlock);
    // Per 64 KiB tracker block of the surface (block 0 holds the base address): the generation of
    // the tracked layer it belongs to, for GuestMemory::ChangedBlocks.
    void blockGenerations(std::vector<std::uint64_t>& generations) const;
    // Another image of exactly this surface under another storage format with results pending
    // (block units, uncompressed, still cached), if any: Refresh takes its pending units from its
    // image on the device (borrowUnits) instead of having them stored and re-read, and the alias
    // stays responsible for storing them until this image's own results supersede them
    // (markLayersPending). APS5_NO_ALIAS_BORROW=1 stores and re-reads as before.
    std::shared_ptr<StorageTexture> pendingAlias() const;
    // The pending images FlushPending(address, bytes) would store (not the one a Refresh validates,
    // a pending unit inside the range), kept alive across the caller's use.
    static std::vector<std::shared_ptr<StorageTexture>> overlappingPending(std::uint64_t address, std::size_t bytes);
    // Whether every 64 KiB block of [address, address + bytes) inside a pending layer of the images
    // is stamped since that layer's generation (the caller collected the range first): the keep
    // rule of writeBackLayers over the access. A generation-0 layer is stored whole: false.
    static bool blocksKept(std::span<const std::shared_ptr<StorageTexture>> images, std::uint64_t address, std::size_t bytes);
    // Under GuestMemory::GpuMutex, after blocksKept approved the access: the layers a write-back for
    // it would select (see writeBack; every pending one of an image no consumer proved current for
    // DeadImagePresents presents with APS5_STALE_EVICT=1, H3) are left as that write-back would
    // leave them when every one of their blocks is stamped since its generation (it would store
    // nothing: no results pending, the generation unchanged, `original` no longer vouching).
    // Returns whether they were; `dropped` counts the image once it left the registry.
    bool evictStale(std::uint64_t address, std::size_t bytes, std::size_t& dropped);
    // Whether the results the last hook skip left pending over its access (AccessKeptByCpu) are
    // still the pending ones and a write-back for the range would store them.
    bool skippedResultsInside(std::uint64_t address, std::size_t bytes) const;
    std::uint64_t borrowUnits(StorageTexture& source, const std::vector<bool>& units);
    void forgetBorrowed(std::uint32_t first, std::uint32_t count);
    bool overlaps(std::uint64_t address, std::size_t bytes) const;
    bool pendingUnitInside(std::uint64_t address, std::size_t bytes) const;
    VkImageView createView(std::uint32_t mip) const;
    void release() noexcept;

    Context context;
    TextureDetiler& detiler;
    GuestTextureResource descriptor;
    std::vector<TileMipLayout> mips;
    std::uint32_t arrayLayers = 1;
    std::uint64_t guestBytes = 0;
    std::uint64_t sliceLinearBytes = 0;
    SurfaceGeometry geometry;
    std::vector<std::byte> original;
    // DCC keys the image content was uploaded under: a fast-cleared surface starts as its clear value.
    DccKeys uploadedKeys = DccKeys::Uncompressed;
    mutable DccKeyProof keyProof;
    // Write generation `original` is known current at (the oldest of layerGeneration).
    std::uint64_t generation = 0;
    std::uint32_t trackedLayers = 1;
    std::uint64_t trackedLayerBytes = 0;
    std::vector<std::uint64_t> layerGeneration;
    std::vector<bool> layerPending;
    bool blockUnits = false;
    // Write-back hysteresis (block units, see writeBack): partial stores in the current window of
    // presents, its first present, and the present until which every pending unit is stored.
    std::uint32_t partialStores = 0;
    std::uint64_t partialWindowStart = 0;
    std::uint64_t storeWholeUntil = 0;
    // Units holding the alias's results (see pendingAlias), at the alias's version then.
    std::weak_ptr<StorageTexture> borrowedFrom;
    std::uint64_t borrowedVersion = 0;
    std::vector<bool> borrowedUnits;
    // Units of this image were borrowed since it was last written: the borrower's proofs
    // (the resource cache's serial memo) hold only while this image's version does, so the next
    // write here bumps the pending serial although the image was pending already.
    bool lent = false;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    std::uint32_t defaultMip = 0;
    std::map<std::uint32_t, VkImageView> extraViews;
    bool attachable = false;
    std::map<std::pair<VkFormat, std::uint32_t>, VkImageView> attachmentViews;
    VkFormat storageFormat = VK_FORMAT_UNDEFINED;
    // Results are on the GPU only (guarded by the pending-write registry lock).
    bool dirty = false;
    std::uint64_t version = 0;
    // `original` holds the guest bytes; false after a GPU-side clear, which never read them.
    bool originalValid = true;
    // The storage cache let the image go (Flush): a lookup makes a new image of the surface, so
    // no fill may put results into this one (guarded by the live-image registry lock).
    bool released = false;
    std::atomic<bool> cached{false};
    // The present a consumer last proved the image current at (NoteProved).
    mutable std::atomic<std::uint64_t> provedPresent{0};
    // Hook skips of this image since FlushPending last listed it (AccessKeptByCpu), the read site
    // of the last one (a GuestMemory::ReadSite), and the first pending layer under its access with
    // the version and layer generation seen: a listing counts a flush after a skip only while that
    // layer is pending with those results (skippedResultsInside), for TakeHookSkipCounts.
    std::atomic<std::uint32_t> hookSkips{0};
    std::atomic<std::uint8_t> hookSkipSite{0};
    std::uint32_t hookSkipLayer = 0;
    std::uint64_t hookSkipVersion = 0;
    std::uint64_t hookSkipGeneration = 0;
    // The present evictStale last walked this image's layers beyond the access (once per present).
    std::uint64_t staleCheckPresent = ~0ull;
    // The generation at which a one-layer FillClear was refused as 'surface changed': the check
    // fails again until `generation` moves, so ClassifyFill skips the layer match meanwhile.
    std::uint64_t layerRefusedGeneration = ~0ull;
};

// APS5_PROFILE_DRAW: what the image lookups on this thread did since the last reset, by outcome
// (count and time), so a caller running them under the GPU mutex can name what its hold spent on
// (VulkanDevice's [indirect] line resets and reads it around stage B). The sampled rows are per
// element (a fast miss is followed by a full lookup); the refresh, upload, DCC scan and pending
// flush rows lie inside the storage and sampled rows.
struct LookupOutcomes {
    enum Kind : std::size_t { SampledFast, SampledFastMiss, SampledHitView, SampledHitClearedView, SampledHitSnapshot, SampledMadeView, SampledMadeSnapshot, StorageHit, StorageMade, RefreshUnchanged, RefreshCompared, UploadDirect, UploadCpu, UploadClear, DccScan, PendingFlush, Count };
    static const char* Name(Kind kind);
    static bool Profiled();
    // Charges the time since `start` to `kind` on this thread and returns now.
    static std::chrono::steady_clock::time_point Add(Kind kind, std::chrono::steady_clock::time_point start);
    std::array<std::uint64_t, Count> counts{};
    std::array<double, Count> ms{};
};
LookupOutcomes& ThreadLookupOutcomes();

}

#endif
