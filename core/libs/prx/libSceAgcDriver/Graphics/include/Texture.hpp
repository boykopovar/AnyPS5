#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_TEXTURE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_TEXTURE_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include <cstddef>
#include <map>
#include <memory>
#include <vector>

namespace AgcDriver::Graphics {

class ResidentColor;
class CommandBatch;
class StorageTexture;

// The Vulkan format storage images of a guest format use (sRGB formats store as their UNORM form).
// Throws when the format has no storage form; the query is cached per format (a lookup cost a
// vkGetPhysicalDeviceFormatProperties call before).
VkFormat StorageFormatForGuest(const Context& context, std::uint32_t guestFormat);
// Whether StorageFormatForGuest would succeed, without throwing (unknown guest formats included).
bool StorageFormatAvailable(const Context& context, std::uint32_t guestFormat);

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

private:
    void release() noexcept;

    // Held by value: cached textures outlive the Context of the draw that created them.
    Context context;
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
    VkImageView AttachmentView(VkFormat format);
    void WriteBack();
    // Deferred write-back (APS5_EAGER_WRITEBACK=1 stores at once instead).
    void MarkDirty();
    void Flush();
    // Stores every pending image overlapping the range, except `except`; returns whether any was.
    // Stores into host-imported memory are recorded (not waited for): a CPU reader syncs afterwards.
    static bool FlushPending(std::uint64_t address, std::size_t bytes, const StorageTexture* except = nullptr, const char* reason = "memory access");
    static void FlushAllPending(const char* reason);
    // The pending image whose surface starts at `address` and covers at least `bytes` (a mip chain
    // contains a descriptor of its first mips), if any; the caller decides whether the geometry fits
    // (Texture::CanCopyFrom, ResidentPresentable).
    static std::shared_ptr<StorageTexture> FindPending(std::uint64_t address, std::uint64_t bytes);
    VkImage Image() const { return image; }
    const GuestTextureResource& Descriptor() const { return descriptor; }
    std::uint32_t ImageLayers() const { return geometry.imageLayers; }
    std::uint32_t ImageDepth() const { return geometry.imageDepth; }
    // Content version: advances when the image is re-uploaded or a shader wrote it. Together with
    // Generation (the write generation guest memory was last known to match the content at) it
    // validates textures copied from this image.
    std::uint64_t Version() const { return version; }
    std::uint64_t Generation() const { return generation; }
    // Brings the image up to date with guest memory before another use; returns whether its content
    // was still current (nothing uploaded).
    // Keeps the image current with guest memory (see GuestMemory::CollectWrites).
    bool Refresh();
    std::uint64_t GuestBytes() const;

private:
    std::vector<VkBufferImageCopy> CopyRegions() const;
    void upload();
    // Stores the image to guest memory; with `skipWrittenSince` set, 64 KiB blocks the CPU wrote since
    // that write generation keep the CPU's bytes.
    void writeBack(std::uint64_t skipWrittenSince);
    // Pending images whose surface shares a 64 KiB write-stamp block with this one without
    // overlapping it, and whose memory is unchanged since their generation (see writeBack), with
    // the generation seen at the check.
    struct Adjacent {
        std::shared_ptr<StorageTexture> texture;
        std::uint64_t generation;
    };
    std::vector<Adjacent> adjacentPendingUnchanged() const;
    // Advances the entries still at their seen generation to `now`, the generation this write-back's
    // stamps of blocks firstBlock..lastBlock (64 KiB) are below, unless the neighbour's other blocks
    // were stamped since the check.
    static void advanceAdjacent(const std::vector<Adjacent>& adjacent, std::uint64_t now, std::uint64_t firstBlock, std::uint64_t lastBlock);
    bool overlaps(std::uint64_t address, std::size_t bytes) const;
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
    // Write generation `original` is known current at.
    std::uint64_t generation = 0;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    std::uint32_t defaultMip = 0;
    std::map<std::uint32_t, VkImageView> extraViews;
    bool attachable = false;
    std::map<VkFormat, VkImageView> attachmentViews;
    VkFormat storageFormat = VK_FORMAT_UNDEFINED;
    // Results are on the GPU only (guarded by the pending-write registry lock).
    bool dirty = false;
    std::uint64_t version = 0;
    // `original` holds the guest bytes; false after a GPU-side clear, which never read them.
    bool originalValid = true;
};

}

#endif
