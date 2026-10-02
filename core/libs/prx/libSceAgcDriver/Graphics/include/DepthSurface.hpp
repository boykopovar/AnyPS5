#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DEPTHSURFACE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DEPTHSURFACE_HPP

#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include <cstdint>
#include <memory>
#include <span>

namespace AgcDriver::Graphics {

class Texture;

VkImageView DepthSurfaceView(const Context& context, const DepthTarget& target);
std::uint64_t DepthSliceBytes(VkExtent2D extent, std::uint32_t bytesPerTexel);
void ClearDepthSurfaces(VkDevice device);
bool DepthSurfaceAt(std::uint64_t address);
void NoteDepthMetadataFill(std::uint64_t address, std::size_t bytes, std::uint32_t pattern);
VkImageAspectFlags HtileFillClears(std::uint32_t pattern, bool stencilInHtile);
bool HtileFillCovers(std::uint64_t htile, VkExtent2D extent, std::uint64_t address, std::size_t bytes);
std::shared_ptr<Texture> DepthSurfaceTexture(const Context& context, std::span<const std::uint32_t> words, const GuestTextureResource& resource, VkComponentMapping components);
class StorageTexture;
void SeedStorageFromDepth(const Context& context, const std::shared_ptr<StorageTexture>& storage);

}

#endif
