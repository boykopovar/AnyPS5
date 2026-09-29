#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DEPTHSURFACE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DEPTHSURFACE_HPP

#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include <cstdint>

namespace AgcDriver::Graphics {

// The attachment view of the device's image for a depth/stencil target, made on first use. The
// image lives on the GPU only, always in the general layout: its guest surface (tiled, HTILE
// compressed) is never read or written, so a new image starts at the target's clear values, and
// texture reads of the surface are refused (DepthSurfaceAt). Images stay until the device goes
// (ClearDepthSurfaces), so their views are stable for cached framebuffers.
// ponytail: never evicted, one image per distinct surface; evict (and keep recorded ones alive) if
// titles churn through many.
VkImageView DepthSurfaceView(const Context& context, const DepthTarget& target);
void ClearDepthSurfaces(VkDevice device);
// Whether the depth or stencil plane of a surface drawn with starts at `address`.
bool DepthSurfaceAt(std::uint64_t address);

}

#endif
