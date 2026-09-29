#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DEPTHSURFACE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DEPTHSURFACE_HPP

#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include <cstdint>
#include <memory>
#include <span>

namespace AgcDriver::Graphics {

class Texture;

// The attachment view of the device's image for a depth/stencil target, made on first use. The
// image lives on the GPU only, always in the general layout: its guest surface (tiled, HTILE
// compressed) is never read or written, so a new image starts at the target's clear values, and
// shaders sample the image itself (DepthSurfaceTexture). Images stay until the device goes
// (ClearDepthSurfaces), so their views are stable for cached framebuffers.
// ponytail: never evicted, one image per distinct surface; evict (and keep recorded ones alive) if
// titles churn through many.
VkImageView DepthSurfaceView(const Context& context, const DepthTarget& target);
void ClearDepthSurfaces(VkDevice device);
// Whether the depth or stencil plane of a surface drawn with starts at `address`.
bool DepthSurfaceAt(std::uint64_t address);
// The texture sampling the depth or stencil plane that starts at the descriptor's base address,
// or null when no surface drawn with on this device has a plane there. The plane is read in place,
// so the descriptor must describe it (its format, extent, one level and slice); anything else throws.
std::shared_ptr<Texture> DepthSurfaceTexture(const Context& context, std::span<const std::uint32_t> words, const GuestTextureResource& resource, VkComponentMapping components);

}

#endif
