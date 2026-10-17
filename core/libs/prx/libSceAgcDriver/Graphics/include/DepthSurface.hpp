#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DEPTHSURFACE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DEPTHSURFACE_HPP

#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SampleOrder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>

namespace AgcDriver::Graphics {

class Texture;
class StorageTexture;

VkImageView DepthSurfaceView(const Context& context, const DepthTarget& target);
SampleOrderTarget DepthSurfaceOrderTarget(const Context& context, const DepthTarget& target);
DepthSampleLocations DepthSurfaceSampleLocations(const Context& context, const DepthTarget& target);
bool SampleLocationsCompatibleDepth(const Context& context, VkFormat format, std::uint32_t samples);
std::uint64_t DepthSurfacesSerial();
std::uint64_t DepthSliceBytes(VkExtent2D extent, std::uint32_t bytesPerTexel);
std::uint64_t MultisampledDepthBytes_nid_no_patch(VkExtent2D extent, std::uint32_t bytesPerTexel, std::uint32_t samples);
void RequireNoResidentDepthRead_nid_no_patch(std::uint64_t address, std::size_t bytes);
bool HasResidentDepthGuards_nid_no_patch();
void RetireUnmappedDepth_nid_no_patch(std::uintptr_t address, std::size_t bytes);
void RequireNoNativeDepthFeedback_nid_no_patch(const Context& context, const DepthTarget& target, std::span<const std::shared_ptr<Texture>> sampled);
void ClearDepthSurfaces(VkDevice device);
void RetireDepthSurfaces(VkDevice device, std::uint64_t address, std::uint64_t bytes);
bool DepthSurfaceAt(std::uint64_t address);
bool DepthSurfaceAt(const Context& context, std::span<const std::uint32_t> words, const GuestTextureResource& resource);
std::uint64_t HtileDepthClearAddress(std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, const std::array<std::uint32_t, 3>& numThreads);
std::size_t HtileDepthClearBytes_nid_no_patch(std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, const ShaderRecompiler::ShaderComputeStageInfo& compute, std::span<const std::uint32_t> packet, std::uint64_t indirectArguments);
void NoteHtileDepthClear(std::uint64_t htileAddress, std::size_t bytes, std::size_t nativeBytes = 0u);
struct MemoryValueFill {
    std::uint64_t address;
    std::size_t bytes;
    std::uint64_t valueAddress;
};
std::optional<MemoryValueFill> MatchMemoryValueFill(std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, const std::array<std::uint32_t, 3>& numThreads, const std::array<std::uint32_t, 3>& groups);
void NoteMemoryValueFill(const MemoryValueFill& fill);
bool DepthSurfaceHolds(const Context& context, const GuestTextureResource& resource, const Texture* texture);
void NoteDepthMetadataFill(std::uint64_t address, std::size_t bytes, std::uint32_t pattern);
VkImageAspectFlags HtileFillClears(std::uint32_t pattern, bool stencilInHtile);
std::uint64_t HtileMetadataBytes(VkExtent2D extent);
bool HtileFillCovers(std::uint64_t htile, VkExtent2D extent, std::uint64_t address, std::size_t bytes);
std::shared_ptr<Texture> DepthSurfaceTexture(const Context& context, std::span<const std::uint32_t> words, const GuestTextureResource& resource, VkComponentMapping components);
VkFormat HostDepthFormat(const Context& context, VkFormat format);
float HostDepthBiasConstant(const Context& context, VkFormat format, float constant);
std::shared_ptr<StorageTexture> DepthSurfaceStorage(const Context& context, std::span<const std::uint32_t> words, const GuestTextureResource& resource, std::uint32_t mip);
bool DepthSurfaceStoreBack(const Context& context, const StorageTexture& storage);

}

#endif
