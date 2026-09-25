#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DCCMETADATA_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DCCMETADATA_HPP

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include <cstddef>
#include <cstdint>
#include <span>

namespace AgcDriver::Graphics {

struct Context;

// Delta color compression keeps one key byte per 256 bytes of a color surface. Surfaces are written
// uncompressed here, so the keys that matter are the fast-clear codes a title writes into the metadata
// (the surface then reads as a constant whatever its texels hold) and "uncompressed", which the driver
// stores after it writes a surface so later reads see the texels.
enum class DccKeys { Uncompressed, Clear0000, Clear0001, Clear1110, Clear1111, ClearRegister, Mixed, Unreadable };

const char* DccKeysName(DccKeys keys);
// The keys covering a surface of `surfaceBytes`, when they all agree.
DccKeys ReadDccKeys(std::uint64_t metaAddress, std::uint64_t surfaceBytes);
bool IsDccClear(DccKeys keys);
// Stores "uncompressed" keys over the surface's metadata on the CPU (a guest memory write: it waits
// for recorded GPU work that writes the keys first).
void MarkDccUncompressed(std::uint64_t metaAddress, std::uint64_t surfaceBytes);
// The same after a write-back recorded under GuestMemory::GpuMutex: when the keys are in host-imported
// memory and a recorder is active, the store is a fill recorded into the open batch (ordered behind
// the title's key-writing kernels like the write-back itself, nothing waits on the CPU) and the range
// reads as uncompressed from ReadDccKeys while that batch is pending; otherwise the CPU store above.
// APS5_CPU_DCC_KEYS=1 always stores on the CPU.
void MarkDccUncompressed(const Context& context, std::uint64_t metaAddress, std::uint64_t surfaceBytes);
// Fills `bytes` with the texel a 0000/0001/1110/1111 clear code stands for ("1" is 1.0 or the integer
// maximum; the alpha channel is the last one in memory when alphaOnMsb, else the first, and 3-channel
// formats have none). False when the format has no encoding here.
bool FillDccClear(VkFormat format, DccKeys keys, bool alphaOnMsb, std::span<std::byte> bytes);

// Where a color target's metadata puts alpha: the last channel in memory unless the component swap is
// reversed (single-channel formats: only with the alternate reversed swap).
bool DccAlphaOnMsb(VkFormat format, std::uint32_t componentSwap);

// The keys of a texture's DCC surface when they fast-clear it to a value encodable here, else
// Uncompressed (the texels are read as stored; other keys are reported once).
DccKeys TextureClearKeys(const GuestTextureResource& resource, std::uint64_t guestBytes);
// A surface's texels as a read sees them: the guest bytes, or the clear value of fast-cleared keys.
void ReadTextureSurface(const GuestTextureResource& resource, DccKeys keys, std::span<std::byte> bytes);

}

#endif
