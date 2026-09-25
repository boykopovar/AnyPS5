#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_TEXTURETILING_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_TEXTURETILING_HPP

#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include <array>
#include <cstdint>
#include <vector>

namespace AgcDriver::Graphics {

struct TileMipLayout {
    std::uint64_t tiledOffset;
    std::uint64_t tiledSize;
    std::uint64_t linearOffset;
    std::uint64_t linearSize;
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t blocksPerRow;
    std::uint32_t pitchBytes;
    bool tail;
    std::uint32_t tailX;
    std::uint32_t tailY;
};

std::vector<TileMipLayout> ComputeMipLayout(TextureTileMode tileMode, std::uint32_t format, std::uint32_t width, std::uint32_t height, std::uint32_t mipCount);
std::uint64_t ComputeSurfaceSize(const std::vector<TileMipLayout>& mips, std::uint32_t arrayLayers);
// Mip chain of an uncompressed surface given its element size (color targets have no texture format).
std::vector<TileMipLayout> ComputeElementMipLayout(TextureTileMode tileMode, std::uint32_t bytesPerElement, std::uint32_t width, std::uint32_t height, std::uint32_t mipCount);

// Thick (3D) SW_4KB_S / SW_64KB_S blocks span several depth slices: {width, height, depth} in elements.
std::array<std::uint32_t, 3> ThickBlockExtent(TextureTileMode tileMode, std::uint32_t bytesPerElement);

// Single-mip 3D surface. Each depth slice z is detiled from slab z / blockDepth (slabBytes apart) with the
// swizzle's slice input set to z, into linear slices sliceLinearBytes apart.
struct ThickLayout {
    TileMipLayout mip;
    std::uint32_t depth;
    std::uint32_t blockDepth;
    std::uint64_t slabBytes;
    std::uint64_t sliceLinearBytes;
    std::uint64_t guestBytes;
};
ThickLayout ComputeThickLayout(TextureTileMode tileMode, std::uint32_t format, std::uint32_t width, std::uint32_t height, std::uint32_t depth);

// Guest surface as the texture upload/write-back loops walk it: array layers of thin surfaces, or the
// depth slices of a volume (which Vulkan holds as one layer with extent depth).
struct SurfaceGeometry {
    std::vector<TileMipLayout> mips;
    std::uint32_t layers = 1;
    std::uint32_t imageLayers = 1;
    std::uint32_t imageDepth = 1;
    std::uint64_t guestBytes = 0;
    std::uint64_t sliceLinearBytes = 0;
    bool thick = false;
    std::uint32_t blockDepth = 1;
    std::uint64_t layerBytes = 0;

    std::uint64_t GuestLayerOffset(std::uint32_t layer) const { return thick ? static_cast<std::uint64_t>(layer / blockDepth) * layerBytes : static_cast<std::uint64_t>(layer) * layerBytes; }
    std::uint64_t LinearLayerOffset(std::uint32_t layer) const { return static_cast<std::uint64_t>(layer) * sliceLinearBytes; }
    std::uint32_t CopyLayer(std::uint32_t layer) const { return imageDepth > 1 ? 0u : layer; }
    std::int32_t CopyDepth(std::uint32_t layer) const { return imageDepth > 1 ? static_cast<std::int32_t>(layer) : 0; }
};
SurfaceGeometry DescribeSurface(const GuestTextureResource& descriptor);

}

#endif
