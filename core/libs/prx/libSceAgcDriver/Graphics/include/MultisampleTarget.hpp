#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_MULTISAMPLETARGET_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_MULTISAMPLETARGET_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include <cstddef>
#include <cstdint>
#include <string>

namespace AgcDriver::Graphics {

class StorageTexture;

class MultisampledCmask {
public:
    MultisampledCmask() = default;
    MultisampledCmask(std::uint64_t cmaskAddress, std::uint64_t cmaskBytes) : address(cmaskAddress), bytes(cmaskBytes) {}
    void NoteFill(std::uint64_t fillAddress, std::size_t fillBytes, std::uint32_t pattern);
    void SeedKeys(DccKeys seeded);
    void CheckAddress(std::uint64_t cmaskAddress) const;
    bool TakeClear();
    DccKeys Keys() const { return keys; }
    const std::string& Refusal() const { return refusal; }

private:
    std::uint64_t address = 0;
    std::uint64_t bytes = 0;
    DccKeys keys = DccKeys::Uncompressed;
    std::string refusal;
};

VkImageView MultisampleTargetView(const Context& context, const ColorTarget& target);
void ResolveMultisampleTarget(const Context& context, const ColorTarget& source, StorageTexture& destination, VkImageView destinationView);
void NoteColorMetadataFill(std::uint64_t address, std::size_t bytes, std::uint32_t pattern);
void ClearMultisampleTargets(VkDevice device);

}

#endif
