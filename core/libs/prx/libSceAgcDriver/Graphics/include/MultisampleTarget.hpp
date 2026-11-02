#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_MULTISAMPLETARGET_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_MULTISAMPLETARGET_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SampleOrder.hpp"
#include "BdaAbi.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace AgcDriver::Graphics {

class StorageTexture;
class Texture;
struct GuestTextureResource;

std::optional<DccKeys> MultisampledCmaskKeys(std::uint8_t code);

class MultisampledCmask {
public:
    MultisampledCmask() = default;
    MultisampledCmask(std::uint64_t cmaskAddress, std::uint64_t cmaskBytes) : address(cmaskAddress), bytes(cmaskBytes) {}
    void NoteFill(std::uint64_t fillAddress, std::size_t fillBytes, std::uint32_t pattern);
    void SeedCode(std::optional<std::uint8_t> code);
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
std::size_t MultisampledDccBytes(const ColorTarget& target);
VkClearColorValue MultisampledDccClear(const ColorTarget& target, std::span<const std::byte> dcc, std::span<const std::byte> cmask);
bool MultisampledFillClearColor(const ColorTarget& target, std::uint32_t fill, VkClearColorValue& clear);
struct MultisampledSingleClear {
    std::uint64_t descriptor = 0;
    std::array<std::uint32_t, 4> color{};
};
std::optional<MultisampledSingleClear> MatchMultisampledSingleClear(std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, const std::array<std::uint32_t, 3>& numThreads);
void NoteMultisampledSingleClear(const GuestTextureResource& resource, const std::array<std::uint32_t, 4>& color);
bool TakeMultisampledSingleClear(std::span<const std::uint32_t> code, std::span<const std::uint32_t> userData, const std::array<std::uint32_t, 3>& numThreads);
bool TakeMultisampledClear(MultisampledCmask& cmask, MultisampledColorFill& colorFill, const ColorTarget& use, VkClearColorValue& clear);
SampleOrderTarget MultisampleTargetOrderTarget(const Context& context, const ColorTarget& target);
std::shared_ptr<Texture> MultisampleTargetTexture(const Context& context, const GuestTextureResource& resource, VkComponentMapping components, VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D);
std::shared_ptr<Texture> MultisampleTargetFirstSample(const Context& context, const GuestTextureResource& resource, VkComponentMapping components);
VkImageView MultisampleTargetView(const Context& context, const ColorTarget& target, VkImage* image = nullptr);
VkImageView MultisampleTargetView(const Context& context, const ColorTarget& target, std::shared_ptr<void>& owner, VkImage* image = nullptr);
void KeepMultisampleTarget_nid_no_patch(const std::shared_ptr<void>& owner);
void MaterializeMultisampleTargetClear(const Context& context, const ColorTarget& target);
VkImageView MultisampleTargetStorageView(const Context& context, const GuestTextureResource& resource, bool uintStorage = false);
bool MultisampleTargetStorable(const Context& context, const ColorTarget& target);
void ResolveMultisampleTarget(const Context& context, const ColorTarget& source, StorageTexture& destination, VkImageView destinationView);
bool MultisampledColorFillAbsorbs(std::uint64_t address, std::size_t bytes, std::uint32_t pattern);
void NoteColorMetadataFill(std::uint64_t address, std::size_t bytes, std::uint32_t pattern);
std::shared_ptr<Texture> MultisampleTargetFmaskTexture_nid_no_patch(const Context& context, const GuestTextureResource& resource, VkComponentMapping components);
void ClearMultisampleTargets(VkDevice device);
std::uint64_t MultisampleTargetRangeGeneration();
std::optional<std::vector<ShaderRecompiler::BdaAbi::Range>> ExcludeMultisampleTargetRanges(
    VkDevice device, std::span<const ShaderRecompiler::BdaAbi::Range> ranges, std::uint64_t& generation);
void InstallResidentImageReadGuard_nid_no_patch();
void UpdateResidentImageUnmapObserver_nid_no_patch();
void ClaimMultisampleTargetMemory(std::uint64_t address, std::uint64_t bytes);
void GuardOneFragmentFmask(const Context& context, const ColorTarget& target);
void RequireNoMultisampleTarget(std::uint64_t address, std::size_t bytes, const char* access);
void RequireNoMultisampleColorTarget(std::uint64_t address, std::size_t bytes, const char* access);
std::vector<std::pair<std::uint64_t, std::uint64_t>> HostOnlySampleRanges(std::uint64_t address, std::size_t bytes);

}

#endif
