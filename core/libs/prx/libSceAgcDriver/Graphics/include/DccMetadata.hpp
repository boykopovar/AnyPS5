#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DCCMETADATA_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_DCCMETADATA_HPP

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>
#include "prx/libSceAgcDriver/Graphics/include/GuestTextureResource.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace AgcDriver::Graphics {

struct Context;

// Delta color compression keeps one key byte per 256 bytes of a color surface. Surfaces are written
// uncompressed here, so the keys that matter are the fast-clear codes a title writes into the metadata
// (the surface then reads as a constant whatever its texels hold) and "uncompressed", which the driver
// stores after it writes a surface so later reads see the texels.
enum class DccKeys { Uncompressed, Clear0000, Clear0001, Clear1110, Clear1111, ClearRegister, Mixed, Unreadable };

const char* DccKeysName(DccKeys keys);
std::size_t DccKeyBytes(std::uint64_t surfaceBytes);
std::size_t DccKeyCount(TextureTileMode tileMode, std::uint32_t elementBytes, std::uint32_t width, std::uint32_t height, std::uint64_t surfaceBytes);
std::size_t DccKeyCount(const GuestTextureResource& surface, std::uint64_t surfaceBytes);

template<typename ReadFollowed, typename ReadNamed>
bool KeysServeSurface(std::uint64_t followedDcc, DccKeys uploaded, DccKeys filled, std::uint64_t namedDcc, ReadFollowed&& readFollowed, ReadNamed&& readNamed) {
    if (namedDcc == 0 || namedDcc == followedDcc) return true;
    if (uploaded != DccKeys::Uncompressed || filled != DccKeys::Uncompressed) return false;
    if (followedDcc != 0 && readFollowed() != DccKeys::Uncompressed) return false;
    return readNamed() == DccKeys::Uncompressed;
}
// The keys covering a surface of `surfaceBytes`, when they all agree.
DccKeys ReadDccKeys(std::uint64_t metaAddress, std::uint64_t surfaceBytes);
bool IsDccClear(DccKeys keys);
DccKeys CurrentDccKeys(std::uint64_t metaAddress, std::uint64_t surfaceBytes);
DccKeys CurrentDccKeys(std::uint64_t metaAddress, std::uint64_t surfaceBytes, std::size_t keyCount);
// Stores "uncompressed" keys over the surface's metadata on the CPU (a guest memory write: it waits
// for recorded GPU work that writes the keys first).
void MarkDccUncompressed(std::uint64_t metaAddress, std::uint64_t surfaceBytes);
void MarkDccUncompressed(std::uint64_t metaAddress, std::uint64_t surfaceBytes, std::size_t keyCount);
// The same after a write-back recorded under GuestMemory::GpuMutex: when the keys are in host-imported
// memory and a recorder is active, the store is a fill recorded into the open batch (ordered behind
// the title's key-writing kernels like the write-back itself, nothing waits on the CPU) and the range
// reads as uncompressed from ReadDccKeys while that batch is pending; otherwise the CPU store above.
// APS5_CPU_DCC_KEYS=1 always stores on the CPU.
void MarkDccUncompressed(const Context& context, std::uint64_t metaAddress, std::uint64_t surfaceBytes);
void MarkDccUncompressed(const Context& context, std::uint64_t metaAddress, std::uint64_t surfaceBytes, std::size_t keyCount);
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
// The last stable key scan of one surface's metadata (ProvedClearKeys): the keys read and the write
// generation the key range was collected at before the scan. Held by the image or texture of the
// surface, read and written under GuestMemory::GpuMutex only.
struct DccKeyProof {
    DccKeys keys = DccKeys::Uncompressed;
    std::uint64_t generation = 0;
};
// TextureClearKeys with the scan skipped while the key range is unchanged since `proof` was taken
// (CollectWrites over the keys, then UnchangedSince its generation), so a surface's keys are scanned
// once per change instead of on every use. A scan becomes the proof only when it read the bytes
// (not the pending-store memo) and no recorded work still writes them: a recorded key store stamps
// the range at its note, before it lands, so a proof taken meanwhile would outlive the change. Every
// other key writer stamps the range too (CPU writes through the page write watch, GPU writes at
// their record), which makes the proof exactly as sound as UnchangedSince over the texels.
// APS5_NO_KEY_FAST_PATH=1 scans on every call and stores nothing.
DccKeys ProvedClearKeys(const GuestTextureResource& resource, std::uint64_t guestBytes, DccKeyProof& proof);
bool KeyFastPath();
// Cumulative outcomes of ProvedClearKeys: calls answered by a proof, scans made, and scans not kept.
struct DccKeyProofCounts {
    std::uint64_t proved;
    std::uint64_t scanned;
    std::uint64_t unstable;
};
DccKeyProofCounts KeyProofCounts();
// A surface's texels as a read sees them: the guest bytes, or the clear value of fast-cleared keys.
void ReadTextureSurface(const GuestTextureResource& resource, DccKeys keys, std::span<std::byte> bytes);
void NoteKeysFillOnGpu(std::uint64_t begin, std::size_t count, DccKeys keys);
std::optional<DccKeys> WaitForKeyWriters(const GuestTextureResource& resource, std::uint64_t guestBytes);

}

#endif
