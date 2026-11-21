#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureSwizzleEquations.hpp"
#include <string>
#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace AgcDriver::Graphics {
namespace {
namespace {
namespace {
std::vector<TileMipLayout> ComputeElementMipLayout(TextureTileMode tileMode, std::uint32_t bytesPerElement, std::uint32_t width, std::uint32_t height, std::uint32_t mipCount) {
    Require(width != 0 && height != 0 && mipCount != 0 && mipCount <= 16u, "invalid surface mip chain");
    Require(!IsGcnTiled(tileMode), "PS4 surfaces are laid out by ComputeGcnMipLayout from their pitch");
    if (tileMode == TextureTileMode::kLinear) return ComputeLinearMipLayout(bytesPerElement, 1u, 1u, width, height, mipCount, 0u);
    return ComputeTiledMipLayout(tileMode, bytesPerElement, 1u, 1u, width, height, mipCount);
}

std::uint64_t ComputeSurfaceSize(std::span<const TileMipLayout> mips, std::uint32_t arrayLayers) {
    Require(!mips.empty(), "cannot compute surface size for an empty mip chain");
    Require(arrayLayers != 0, "cannot compute surface size for zero array layers");

    std::uint64_t sliceSize = 0;
    for (const auto& mip : mips) {
        Require(mip.tiledSize != 0, "encountered a zero-sized mip level while computing surface size");
        sliceSize = std::max(sliceSize, mip.tiledOffset + mip.tiledSize);
    }

    Require(sliceSize <= UINT64_MAX / arrayLayers, "tiled texture surface size overflows");
    return sliceSize * arrayLayers;
}

}
}
}
}
