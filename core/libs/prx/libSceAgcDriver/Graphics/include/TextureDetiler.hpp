#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_TEXTUREDETILER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_TEXTUREDETILER_HPP

#include <array>
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include <cstdint>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics {

    // The part of a mip one Dispatch moves: only elements whose tiled offset (relative to the mip's
    // tiled base; the linear offset of a linear surface) lies in [rangeBegin, rangeEnd), with the
    // tiled buffer holding the mip from tiledBase and the linear buffer holding `linearBytes` of
    // it from linearBase (a window of whole rows: linearBase is a row's start, every element in
    // the range lies inside the window; 0 for the rest of the mip). The element rectangle
    // [columnBegin, columnEnd) x [rowBegin, rowEnd) (an end of 0: the mip's edge) bounds the
    // elements the range can hold: only its workgroups are dispatched, the range check stays the
    // guard. The default moves the whole mip out of whole buffers.
    struct DetileWindow {
        std::uint32_t rangeBegin = 0;
        std::uint32_t rangeEnd = 0xffffffffu;
        std::uint32_t tiledBase = 0;
        std::uint32_t linearBase = 0;
        std::uint64_t linearBytes = 0;
        std::uint32_t columnBegin = 0;
        std::uint32_t columnEnd = 0;
        std::uint32_t rowBegin = 0;
        std::uint32_t rowEnd = 0;
    };

    class TextureDetiler {
    public:
        explicit TextureDetiler(const Context& context);
        ~TextureDetiler();
        TextureDetiler(const TextureDetiler&) = delete;
        TextureDetiler& operator=(const TextureDetiler&) = delete;

        // Detiles `source` (tiled) into `destination` (linear), or with `retile` writes linear `source`
        // into tiled `destination`; offsets always refer to the respective buffers, and `window`
        // restricts the move to part of the mip (a partial upload or write-back, StorageTexture),
        // leaving the other elements of both buffers alone.
        void Dispatch(VkCommandBuffer commands, TextureTileMode tileMode, std::uint32_t elementBytes, VkBuffer source, std::uint64_t sourceOffset, VkBuffer destination, std::uint64_t destinationOffset, const TileMipLayout& layout, bool retile = false, std::uint32_t slice = 0, bool thick = false, const DetileWindow& window = {});
        // Recycles the descriptor sets of the previous batch; call before recording a new command batch.
        void BeginBatch();

    private:
        VkPipeline pipeline(TextureTileMode tileMode, std::uint32_t elementBytes, bool retile, bool thick);
        void release() noexcept;
        VkDescriptorSet allocateSet();

        const Context context;
        VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;
        VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkShaderModule module = VK_NULL_HANDLE;
        std::vector<std::pair<std::uint32_t, VkPipeline>> pipelines;
        std::vector<VkDescriptorPool> descriptorPools;
        std::size_t allocatedSets = 0;
    };

}

#endif
