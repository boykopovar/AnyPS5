#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_MULTISAMPLETARGET_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_MULTISAMPLETARGET_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace AgcDriver::Graphics {

class StorageTexture;
class Texture;
struct GuestTextureResource;

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

class MultisampledColorFill {
public:
    explicit MultisampledColorFill(const ColorTarget& target);
    void NoteFill(std::uint64_t fillAddress, std::size_t fillBytes, std::uint32_t pattern);
    std::optional<std::uint32_t> TakeFill();
    void Discard();

private:
    std::uint64_t address = 0;
    std::uint64_t bytes = 0;
    std::optional<std::uint32_t> filled;
    std::string refusal;
};

std::uint64_t MultisampledColorBytes(const ColorTarget& target);
bool MultisampledFillClearColor(const ColorTarget& target, std::uint32_t fill, VkClearColorValue& clear);
bool TakeMultisampledClear(MultisampledCmask& cmask, MultisampledColorFill& colorFill, const ColorTarget& use, VkClearColorValue& clear);
VkImageView MultisampleTargetView(const Context& context, const ColorTarget& target);
std::shared_ptr<Texture> MultisampleTargetTexture(const Context& context, const GuestTextureResource& resource, VkComponentMapping components);
void MaterializeMultisampleTargetClear(const Context& context, const ColorTarget& target);
VkImageView MultisampleTargetStorageView(const Context& context, const GuestTextureResource& resource);
bool MultisampleTargetStorable(const Context& context, const ColorTarget& target);
void ResolveMultisampleTarget(const Context& context, const ColorTarget& source, StorageTexture& destination, VkImageView destinationView);
bool MultisampledColorFillAbsorbs(std::uint64_t address, std::size_t bytes, std::uint32_t pattern);
void NoteColorMetadataFill(std::uint64_t address, std::size_t bytes, std::uint32_t pattern);
void ClearMultisampleTargets(VkDevice device);
void InstallResidentImageReadGuard_nid_no_patch();
void UpdateResidentImageUnmapObserver_nid_no_patch();
void RequireNoMultisampleTarget(std::uint64_t address, std::size_t bytes, const char* access);

}

#endif
