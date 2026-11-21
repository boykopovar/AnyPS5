#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/CaptureTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "prx/libSceAgcDriver/Execution/include/ThreadScratch.hpp"
#include "ThreadOwned.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MultisampleTarget.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PlaceholderImages.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/BufferPool.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/MemoryAccounting.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include <algorithm>
#include <atomic>
#include <bit>
#include <memory>
#include <mutex>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <functional>
#include <stdexcept>
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureTiling.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "Optimization/include/Optimization/ShaderStageInputInfo.hpp"
#include "RdnaDecoder/include/RdnaDecoder/RdnaDescriptorFormat.hpp"
#include "IntermediateRepresentation/IrMetadata/ShaderStage.hpp"
#include "ImageTableAbi.hpp"
#include <cstring>
#include <limits>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "prx/libc/include/GuestAllocations.hpp"

namespace AgcDriver::Graphics {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
namespace {
bool ImageTableUpdateAfterBind(const Context& context) {
    return ImageTableCapacityFor(context, false) == 0u && ImageTableCapacityFor(context, true) != 0u;
}

bool SamplerTableUpdateAfterBind(const Context& context) {
    return SamplerTableCapacityFor(context, false) == 0u && SamplerTableCapacityFor(context, true) != 0u;
}

ShaderResources::TableStage& ShaderResources::tableStage(const CompiledShader& shader) {
    for (auto& table : tableStages) {
        if (table.program == shader.program) return table;
    }
    auto& table = tableStages.emplace_back();
    table.stage = shader.stage;
    table.program = shader.program;
    table.poison = shader.program->imageTablePoison;
    table.messages.assign(table.poison.size(), nullptr);
    return table;
}

std::uint32_t ShaderResources::rejectTableElement(TableStage& table, std::span<const std::uint32_t> words, const std::string& message) {
    ShaderRecompiler::ImageTableEntryPoison poison{};
    std::copy(words.begin(), words.end(), poison.words.begin());
    poison.dwordCount = static_cast<std::uint32_t>(words.size());
    poison.resource = ShaderRecompiler::NoTable;
    poison.reason = static_cast<std::uint32_t>(ShaderRecompiler::ImageTableAbi::PoisonReason::DriverRejected);
    table.poison.push_back(poison);
    const bool repeated = !table.messages.empty() && table.messages.back() != nullptr && *table.messages.back() == message;
    table.messages.push_back(repeated ? table.messages.back() : std::make_shared<const std::string>(message));
    faultChecks = true;
    return ShaderRecompiler::ImageTableAbi::PoisonCode(static_cast<std::uint32_t>(table.poison.size() - 1u));
}

void ShaderResources::addSamplerTable(const ShaderRecompiler::DescriptorBinding& binding, VkShaderStageFlags flags, TableStage& table) {
    namespace Abi = ShaderRecompiler::ImageTableAbi;
    Require(binding.kind == ShaderRecompiler::DescriptorKind::Sampler && binding.guestDescriptor.size() == static_cast<std::size_t>(binding.count) * 4u && binding.samplerDepthCompare.size() == binding.count && binding.count % Abi::SamplerEntryElements == 0u, "sampler table binding is malformed");
    Require(context.samplerTableCapacity != 0, "sampler tables need runtimeDescriptorArray and descriptorBindingPartiallyBound, which the device lacks");
    const VkDescriptorBindingFlags samplerTableFlags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | (context.samplerTableUpdateAfterBind ? VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT : 0);
    Binding item{{binding.binding, VK_DESCRIPTOR_TYPE_SAMPLER, context.samplerTableCapacity, flags, nullptr}, {}, {}, samplerTableFlags};
    static const bool noSamplerCache = std::getenv("APS5_NO_SAMPLER_CACHE") != nullptr;
    for (std::uint32_t entry = 0; entry < binding.count / Abi::SamplerEntryElements; ++entry) {
        const auto first = entry * Abi::SamplerEntryElements;
        const auto words = std::span<const std::uint32_t>(binding.guestDescriptor).subspan(static_cast<std::size_t>(first) * 4u, 4u);
        std::string rejected;
        if (first + Abi::SamplerEntryElements > context.samplerTableCapacity) rejected = "the sampler table exceeds the device's capacity of " + std::to_string(context.samplerTableCapacity) + " samplers";
        for (std::uint32_t element = first; rejected.empty() && element < first + Abi::SamplerEntryElements; ++element) {
            try {
                const auto elementWords = std::span<const std::uint32_t, 4>(binding.guestDescriptor.data() + static_cast<std::size_t>(element) * 4u, 4u);
                auto resource = DecodeSamplerResource(elementWords);
                resource.compareEnable = binding.samplerDepthCompare.at(element);
                Sampler::Validate(context, resource);
                Require(!Sampler::ShaderAppliesLodBias(resource), "a sampler table entry with a LOD bias beyond the sampler limit needs a per-entry shader bias, which is not implemented");
                Require(!Sampler::ReadsOpaqueBlackBorder(resource), "a sampler table entry with an opaque black border needs dynamic texture BC-swizzle pairing, which is not implemented");
                Require(resource.addressModeU != VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER && resource.addressModeV != VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER && resource.addressModeW != VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, "a sampler table entry with a border needs dynamic texture numeric-class pairing, which is not implemented");
                Require(!resource.nonSeamlessCube || context.nonSeamlessCubeMap, "a sampler table entry that disables seamless cube filtering needs VK_EXT_non_seamless_cube_map because dynamic texture/sampler pairing is not implemented");
                Require(!SingleLevelSamplerWords(elementWords, true, false).has_value(), "a sampler table entry with anisotropic ANISO_OVERRIDE needs dynamic texture mip-level pairing, which is not implemented");
            } catch (const std::runtime_error& error) {
                rejected = error.what();
            }
        }
        if (!rejected.empty()) {
            table.samplerRejections.emplace_back(entry, rejectTableElement(table, words, rejected));
            item.imageAllocations.insert(item.imageAllocations.end(), Abi::SamplerEntryElements, std::numeric_limits<std::size_t>::max());
            continue;
        }
        for (std::uint32_t element = first; element < first + Abi::SamplerEntryElements; ++element) {
            const auto elementWords = std::span<const std::uint32_t>(binding.guestDescriptor).subspan(static_cast<std::size_t>(element) * 4u, 4u);
            const bool compareEnable = binding.samplerDepthCompare.at(element);
            if (context.samplerCache != nullptr && !noSamplerCache) {
                samplers.push_back(context.samplerCache->Get(context, elementWords, compareEnable, false));
            } else {
                auto resource = DecodeSamplerResource(elementWords, false);
                resource.compareEnable = compareEnable;
                samplers.push_back(std::make_shared<Sampler>(context, resource));
            }
            samplerSources.push_back({{elementWords[0], elementWords[1], elementWords[2], elementWords[3]}, compareEnable, false});
            item.imageAllocations.push_back(samplers.size() - 1u);
        }
    }
    const auto setSamplers = context.samplerTableUpdateAfterBind ? context.descriptorIndexingLimits.maxDescriptorSetUpdateAfterBindSamplers : context.limits.maxDescriptorSetSamplers;
    Require(samplers.size() <= setSamplers, "pipeline sampler descriptors exceed device limits");
    bindings.push_back(std::move(item));
}

namespace {

void noteVariantSurface(std::uint64_t address, std::size_t members, const GuestTextureResource& chosen, const char* rule) {
    static std::mutex mutex;
    static std::unordered_set<std::uint64_t> seen;
    {
        std::lock_guard lock(mutex);
        if (seen.size() >= 4096u || !seen.insert(address).second) return;
    }
    TextureCounts().variantSurfaces.fetch_add(1, std::memory_order_relaxed);
    std::fprintf(stderr, "[textures] surface 0x%llx has %zu dynamic-resolution variants in one image table; they share the %ux%u texture (chosen by %s)\n", static_cast<unsigned long long>(address), members, chosen.width, chosen.height, rule);
}

std::vector<std::uint32_t> variantRepresentatives(VkDevice device, const ShaderRecompiler::DescriptorBinding& binding, std::uint64_t elementBase, std::uint64_t capacity) {
    struct Member {
        std::uint32_t element;
        GuestTextureResource resource;
    };
    using FamilyKey = std::tuple<std::uint64_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t>;
    std::map<FamilyKey, std::vector<Member>> families;
    for (std::uint32_t element = 0; element < binding.count && elementBase + element < capacity; ++element) {
        if (element < binding.imageWritten.size() && binding.imageWritten[element]) continue;
        const auto words = std::span<const std::uint32_t>(binding.guestDescriptor).subspan(static_cast<std::size_t>(element) * 8u, 8u);
        GuestTextureResource resource{};
        try {
            if (IsNullTextureResource(words)) continue;
            resource = DecodeTextureResource(words);
        } catch (const std::runtime_error&) {
            continue;
        }
        const FamilyKey key{resource.baseAddress, resource.pipeBankXor, resource.format, static_cast<std::uint32_t>(resource.tileMode), static_cast<std::uint32_t>(resource.dimension), resource.mipCount, resource.baseLevel, resource.lastLevel, resource.depthOrLastArray, resource.baseArray};
        families[key].push_back({element, resource});
    }
    std::vector<std::uint32_t> representatives;
    for (const auto& [key, members] : families) {
        const auto& first = members.front().resource;
        const auto otherExtent = [&](const Member& member) { return member.resource.width != first.width || member.resource.height != first.height; };
        if (std::none_of(members.begin(), members.end(), otherExtent)) continue;
        const auto withExtent = [&](std::uint32_t width, std::uint32_t height) {
            return std::find_if(members.begin(), members.end(), [&](const Member& member) { return member.resource.width == width && member.resource.height == height; });
        };
        auto chosen = members.end();
        const char* rule = "newest write";
        if (const auto record = StorageTexture::LastWrite(first.baseAddress); record && record->tileMode == first.tileMode && record->dimension == first.dimension && BytesPerElement(record->format) == BytesPerElement(first.format)) chosen = withExtent(record->width, record->height);
        if (chosen == members.end()) {
            rule = "active depth surface";
            if (const auto depth = ActiveDepthExtent(device, first.baseAddress)) chosen = withExtent(depth->width, depth->height);
        }
        if (chosen == members.end()) {
            rule = "largest accessible";
            std::uint64_t chosenBytes = 0;
            std::uint64_t chosenExtent = 0;
            for (auto member = members.begin(); member != members.end(); ++member) {
                try {
                    const auto bytes = DescribeSurface(member->resource).guestBytes;
                    const auto extent = static_cast<std::uint64_t>(member->resource.width) * member->resource.height;
                    if (bytes < chosenBytes || (bytes == chosenBytes && extent <= chosenExtent) || !GuestMemory::Accessible(reinterpret_cast<const void*>(member->resource.baseAddress), static_cast<std::size_t>(bytes))) continue;
                    chosen = member;
                    chosenBytes = bytes;
                    chosenExtent = extent;
                } catch (const std::runtime_error&) {
                }
            }
            if (chosen == members.end()) continue;
        }
        for (const auto& member : members) {
            if (member.resource.width == chosen->resource.width && member.resource.height == chosen->resource.height) continue;
            if (binding.imageShapes.at(member.element) != binding.imageShapes.at(chosen->element) || binding.imageDepthCompare.at(member.element) != binding.imageDepthCompare.at(chosen->element)) continue;
            if (representatives.empty()) representatives.assign(binding.count, NoRepresentative);
            representatives[member.element] = chosen->element;
        }
        noteVariantSurface(first.baseAddress, members.size(), chosen->resource, rule);
    }
    return representatives;
}

std::vector<std::uint32_t> arrayPartners(const ShaderRecompiler::DescriptorBinding& binding, std::uint64_t elementBase, std::uint64_t capacity) {
    struct Candidate {
        std::uint32_t element;
        std::array<std::uint32_t, 8> key;
    };
    std::map<std::array<std::uint32_t, 8>, std::uint32_t> flat;
    std::vector<Candidate> arrays;
    for (std::uint32_t element = 0; element < binding.count && elementBase + element < capacity; ++element) {
        if (element < binding.imageWritten.size() && binding.imageWritten[element]) continue;
        const auto words = std::span<const std::uint32_t>(binding.guestDescriptor).subspan(static_cast<std::size_t>(element) * 8u, 8u);
        GuestTextureResource resource{};
        try {
            if (IsNullSampledTextureResource(words)) continue;
            resource = DecodeTextureResource(words);
        } catch (const std::runtime_error&) {
            continue;
        }
        std::array<std::uint32_t, 8> key{};
        std::copy(words.begin(), words.end(), key.begin());
        key[3] &= ~(0xfu << 28u);
        const auto shape = binding.imageShapes.at(element);
        if (shape == ShaderRecompiler::DescriptorImageShape::Image2D && resource.dimension == TextureDimension::k2D) flat.emplace(key, element);
        else if (shape == ShaderRecompiler::DescriptorImageShape::Image2DArray && resource.dimension == TextureDimension::k2DArray && resource.baseArray == 0u && resource.depthOrLastArray == 0u) arrays.push_back({element, key});
    }
    std::vector<std::uint32_t> partners;
    for (const auto& candidate : arrays) {
        const auto found = flat.find(candidate.key);
        if (found == flat.end() || binding.imageDepthCompare.at(candidate.element) != binding.imageDepthCompare.at(found->second)) continue;
        if (partners.empty()) partners.assign(binding.count, NoRepresentative);
        partners[candidate.element] = found->second;
    }
    return partners;
}

}

void ShaderResources::resolveImageTables() {
    const auto none = [](const std::array<std::size_t, 2>& set) { return set[0] == std::numeric_limits<std::size_t>::max() && set[1] == std::numeric_limits<std::size_t>::max(); };
    if (none(tableBindings) && none(storageTableBindings)) return;
    auto& counters = TextureCounts();
    for (std::size_t set = 0; set < tableBindings.size(); ++set) {
        if (storageTableBindings[set] != std::numeric_limits<std::size_t>::max()) bindings[storageTableBindings[set]].imageAllocations.assign(std::min(tableElements[set], StorageTableCapacity(context)), std::numeric_limits<std::size_t>::max());
        if (tableBindings[set] != std::numeric_limits<std::size_t>::max()) bindings[tableBindings[set]].imageAllocations.assign(std::min(tableElements[set], context.imageTableCapacity), std::numeric_limits<std::size_t>::max());
    }
    for (auto& table : tableStages) {
        if (table.images == nullptr) continue;
        auto* item = tableBindings[table.set] != std::numeric_limits<std::size_t>::max() ? &bindings[tableBindings[table.set]] : nullptr;
        const auto& binding = *table.images;
        const auto faults = table.map[ShaderRecompiler::ImageTableAbi::HeaderElementFaults];
        const auto stageSamplers = std::span<const std::shared_ptr<Sampler>>(samplers).subspan(table.firstSampler, table.samplerCount);
        table.elementTextures.assign(binding.count, std::numeric_limits<std::size_t>::max());
        const auto capacityMessage = [&] { return "the image table exceeds the device's capacity of " + std::to_string(context.imageTableCapacity) + " images"; };
        std::vector<std::uint32_t> samplerKey;
        for (std::uint32_t sampler = 0; sampler < table.samplerCount; ++sampler) {
            const auto& source = samplerSources.at(table.firstSampler + sampler);
            samplerKey.insert(samplerKey.end(), source.words.begin(), source.words.end());
            samplerKey.push_back((source.compareEnable ? 1u : 0u) | (source.unnormalized ? 2u : 0u) | (source.integerBorder ? 4u : 0u));
        }
        const auto place = [&](std::uint32_t element, std::shared_ptr<Texture> texture, const GuestTextureResource& resource, std::uint64_t guestBytes, bool firstLayer) {
            textures.push_back(std::move(texture));
            textureFirstLayer.push_back(firstLayer);
            describedRanges.push_back({"texture", resource.baseAddress, guestBytes, resource.width, resource.height, resource.format, static_cast<int>(resource.tileMode), resource.dccAddress, true});
            table.elementTextures[element] = textures.size() - 1u;
            item->imageAllocations[static_cast<std::size_t>(table.elementBase) + element] = textures.size() - 1u;
        };
        const auto refusal = [&](std::uint32_t element, std::span<const std::uint32_t> words, const std::shared_ptr<Texture>& texture, const GuestTextureResource& resource) -> std::string {
            try {
                RequireFilterMinmax(context, texture->ViewFormat(), binding.imageSamplers.at(element), stageSamplers);
                RequireBorderSwizzle(resource.bcSwizzle, binding.imageSamplers.at(element), stageSamplers);
                RequireNonSeamlessCube(resource.dimension == TextureDimension::kCube, binding.imageSamplers.at(element), stageSamplers);
                RequireDisablesDegammaOn(resource.format, binding.imageSamplers.at(element), stageSamplers);
                const auto numeric = ShaderRecompiler::DepthBitsTextureWidth(words[1], words[3]) != 0u ? ShaderRecompiler::IrTextureNumericClass::Float : ShaderRecompiler::SampledTextureNumericClass(static_cast<ShaderRecompiler::IrBufferFormat>(resource.format));
                const bool integerImage = numeric == ShaderRecompiler::IrTextureNumericClass::Uint || numeric == ShaderRecompiler::IrTextureNumericClass::Sint;
                for (auto mask = binding.imageSamplers.at(element); mask != 0u; mask &= mask - 1u) {
                    const auto sampler = static_cast<std::uint32_t>(std::countr_zero(mask));
                    if (sampler >= stageSamplers.size()) break;
                    const auto& source = samplerSources.at(table.firstSampler + sampler);
                    const auto descriptor = DecodeSamplerResource(source.words, source.unnormalized, true, source.integerBorder);
                    const bool border = descriptor.addressModeU == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER || descriptor.addressModeV == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER || descriptor.addressModeW == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
                    Require(!border || source.integerBorder == integerImage, "an image table entry's sampler border color numeric class disagrees with its sampled image class, which needs dynamic pairing and is not implemented");
                    Require(!SingleLevelSamplerWords(source.words, true, false).has_value(), "an image table entry paired with anisotropic ANISO_OVERRIDE needs dynamic texture mip-level pairing, which is not implemented");
                }
            } catch (const std::runtime_error& error) {
                return error.what();
            }
            return {};
        };
        const auto bind = [&](std::uint32_t element, std::span<const std::uint32_t> words, std::shared_ptr<Texture> texture, const GuestTextureResource& resource, std::uint64_t guestBytes, bool firstLayer) -> std::string {
            auto message = refusal(element, words, texture, resource);
            if (!message.empty()) {
                table.map[faults + element] = rejectTableElement(table, words, message);
                return message;
            }
            place(element, std::move(texture), resource, guestBytes, firstLayer);
            return {};
        };
        struct DecodedEntry {
            GuestTextureResource resource{};
            std::uint64_t guestBytes = 0;
            bool firstLayer = false;
            std::string rejected;
        };
        const auto decodeEntry = [&](std::uint32_t element, std::span<const std::uint32_t> words) {
            DecodedEntry decoded;
            decoded.resource = DecodeTextureResource(words);
            const auto& resource = decoded.resource;
            const auto shape = binding.imageShapes.at(element);
            const bool depthCompare = element < binding.imageDepthCompare.size() && binding.imageDepthCompare[element];
            decoded.firstLayer = shape == ShaderRecompiler::DescriptorImageShape::Image2D && (resource.dimension == TextureDimension::k2DArray || (depthCompare && resource.dimension == TextureDimension::k1DArray));
            const bool depth1D = depthCompare && ((shape == ShaderRecompiler::DescriptorImageShape::Image2D && resource.dimension == TextureDimension::k1D) || (shape == ShaderRecompiler::DescriptorImageShape::Image2DArray && resource.dimension == TextureDimension::k1DArray));
            if (!decoded.firstLayer && !depth1D && !MatchesGuestDimension(shape, resource.dimension)) {
                decoded.rejected = "AGC graphics: guest texture dimension disagrees with the table entry's image shape (shape " + std::to_string(static_cast<int>(shape)) + ", dimension " + std::to_string(static_cast<int>(resource.dimension)) + ")";
                return decoded;
            }
            decoded.guestBytes = SurfaceGuestBytes(resource);
            if (!GuestMemory::Accessible(reinterpret_cast<const void*>(resource.baseAddress), static_cast<std::size_t>(decoded.guestBytes)) && GuestMemory::CommittedRanges(resource.baseAddress, static_cast<std::size_t>(decoded.guestBytes), false).empty()) {
                decoded.rejected = "AGC graphics: the table entry's surface at 0x" + HexText(resource.baseAddress) + " (0x" + HexText(decoded.guestBytes) + " bytes) is not mapped";
                return decoded;
            }
            static_cast<void>(ResolveTextureFormat(resource.format));
            return decoded;
        };
        struct SampledEntry {
            std::array<std::uint32_t, 8> words{};
            GuestTextureResource resource{};
            std::uint64_t guestBytes = 0;
            VkComponentMapping components{};
            bool depthCompare = false;
        };
        const auto prepareSampled = [&](std::uint32_t element, std::span<const std::uint32_t> words, const DecodedEntry& decoded) {
            SampledEntry sampled;
            sampled.resource = decoded.resource;
            sampled.guestBytes = decoded.guestBytes;
            std::array<std::uint32_t, 8> narrowed{};
            const auto sampledWords = firstLayerWords(words, binding.imageShapes.at(element), narrowed, true);
            std::copy(sampledWords.begin(), sampledWords.end(), sampled.words.begin());
            if (sampledWords.data() != words.data()) {
                sampled.resource = DecodeTextureResource(sampledWords);
                sampled.guestBytes = SurfaceGuestBytes(sampled.resource);
            }
            CompletePendingCpuWrites(context, sampled.resource.baseAddress, static_cast<std::size_t>(sampled.guestBytes));
            if (sampled.resource.dccAddress != 0) CompletePendingCpuWrites(context, sampled.resource.dccAddress, DccKeyBytes(sampled.guestBytes));
            sampled.components = ViewComponents(sampled.resource);
            sampled.depthCompare = binding.imageDepthCompare.at(element);
            return sampled;
        };
        const auto lookupSampled = [&](std::uint32_t element, const SampledEntry& sampled) {
            const VkImageViewType multisampleViewType = binding.imageShapes.at(element) == ShaderRecompiler::DescriptorImageShape::Image2DMultisampleArray ? VK_IMAGE_VIEW_TYPE_2D_ARRAY : VK_IMAGE_VIEW_TYPE_2D;
            Require(context.imageViewMinLod || EffectiveMinLod(sampled.resource) == 0.0f, "a sampled image table entry has a minimum LOD clamp, which needs VK_EXT_image_view_min_lod");
            return cachedTexture(context, sampled.words, sampled.resource, sampled.components, sampled.guestBytes, sampled.depthCompare, multisampleViewType);
        };
        const auto resolveLive = [&](std::uint32_t element, std::span<const std::uint32_t> words) -> std::string {
            DecodedEntry decoded;
            try {
                decoded = decodeEntry(element, words);
            } catch (const std::runtime_error& error) {
                decoded.rejected = error.what();
            }
            if (!decoded.rejected.empty()) {
                table.map[faults + element] = rejectTableElement(table, words, decoded.rejected);
                return decoded.rejected;
            }
            const auto sampled = prepareSampled(element, words, decoded);
            std::shared_ptr<Texture> texture;
            try {
                texture = lookupSampled(element, sampled);
            } catch (const std::runtime_error& error) {
                table.map[faults + element] = rejectTableElement(table, words, error.what());
                return error.what();
            }
            return bind(element, sampled.words, std::move(texture), sampled.resource, sampled.guestBytes, decoded.firstLayer);
        };
        const auto state = CurrentTableState();
        std::vector<std::shared_ptr<Texture>> cached;
        const bool storageTable = std::find(binding.imageWritten.begin(), binding.imageWritten.end(), true) != binding.imageWritten.end();
        auto* reused = storageTable || item == nullptr ? nullptr : FindReusedTable(context.device, binding, state);
        if (reused != nullptr && AnyPendingCpuWrites(context)) {
            bool landed = false;
            for (const auto& entry : reused->elements) {
                if (!entry.rejected && CompletePendingCpuWrites(context, entry.resource.baseAddress, static_cast<std::size_t>(entry.guestBytes))) landed = true;
            }
            if (landed && CurrentTableState() != state) reused = nullptr;
        }
        if (reused != nullptr && !reused->representatives.empty() && reused->representatives != variantRepresentatives(context.device, binding, table.elementBase, context.imageTableCapacity)) reused = nullptr;
        if (reused != nullptr && TableCurrent(*reused) && CachedTableTextures(*reused, cached)) {
            logTableLookups(*reused, cached);
            table.representatives = reused->representatives;
            const bool replay = TableReplay() && reused->replayable && reused->imageSamplers == binding.imageSamplers && reused->samplers == samplerKey;
            if (!replay) reused->bound.assign(binding.count, {});
            auto next = cached.begin();
            for (std::uint32_t element = 0; element < binding.count; ++element) {
                const auto words = std::span<const std::uint32_t>(binding.guestDescriptor).subspan(static_cast<std::size_t>(element) * 8u, 8u);
                const auto& entry = reused->elements[element];
                if (static_cast<std::uint64_t>(table.elementBase) + element >= context.imageTableCapacity) {
                    if (!entry.rejected && !entry.live && !entry.substitute) ++next;
                    table.map[faults + element] = rejectTableElement(table, words, capacityMessage());
                } else if (entry.rejected) {
                    table.map[faults + element] = rejectTableElement(table, words, entry.message);
                } else if (entry.substitute) {
                    continue;
                } else if (entry.live) {
                    auto message = resolveLive(element, words);
                    if (!replay) reused->bound[element] = std::move(message);
                } else {
                    if (!replay) {
                        reused->bound[element] = bind(element, words, std::move(*next++), entry.resource, entry.guestBytes, entry.firstLayer);
                    } else if (!reused->bound[element].empty()) {
                        ++next;
                        table.map[faults + element] = rejectTableElement(table, words, reused->bound[element]);
                    } else {
                        place(element, std::move(*next++), entry.resource, entry.guestBytes, entry.firstLayer);
                    }
                }
            }
            for (std::uint32_t element = 0; element < binding.count; ++element) {
                const auto& entry = reused->elements[element];
                if (!entry.substitute || static_cast<std::uint64_t>(table.elementBase) + element >= context.imageTableCapacity) continue;
                const auto words = std::span<const std::uint32_t>(binding.guestDescriptor).subspan(static_cast<std::size_t>(element) * 8u, 8u);
                const auto representativeWords = std::span<const std::uint32_t>(binding.guestDescriptor).subspan(static_cast<std::size_t>(entry.representative) * 8u, 8u);
                const auto index = table.elementTextures[entry.representative];
                const auto message = index == std::numeric_limits<std::size_t>::max() ? std::string("the dynamic-resolution variant this entry borrows from was rejected") : refusal(element, representativeWords, textures[index], entry.representativeResource);
                if (!message.empty()) {
                    table.map[faults + element] = rejectTableElement(table, words, message);
                    counters.variantRejected.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }
                table.elementTextures[element] = index;
                item->imageAllocations[static_cast<std::size_t>(table.elementBase) + element] = index;
                counters.variantSubstituted.fetch_add(1, std::memory_order_relaxed);
            }
            if (!replay) {
                reused->imageSamplers = binding.imageSamplers;
                reused->samplers = samplerKey;
                reused->replayable = static_cast<std::uint64_t>(table.elementBase) + binding.count <= context.imageTableCapacity;
            }
            (replay ? counters.tablesReplayed : counters.tablesReused).fetch_add(1, std::memory_order_relaxed);
            continue;
        }
        ReusedTable fresh;
        fresh.bound.assign(binding.count, {});
        bool reusable = state.epoch != 0;
        if (binding.count > 1) table.representatives = variantRepresentatives(context.device, binding, table.elementBase, context.imageTableCapacity);
        const auto& representatives = table.representatives;
        std::vector<bool> representativeNeeded(representatives.size(), false);
        for (const auto representative : representatives) {
            if (representative != NoRepresentative) representativeNeeded[representative] = true;
        }
        std::unordered_map<std::uint32_t, GuestTextureResource> representativeSources;
        std::vector<std::uint32_t> substitutes;
        const auto partners = binding.count > 1 ? arrayPartners(binding, table.elementBase, context.imageTableCapacity) : std::vector<std::uint32_t>{};
        std::vector<std::uint32_t> order;
        if (!partners.empty()) {
            reusable = false;
            order.reserve(binding.count);
            for (std::uint32_t element = 0; element < binding.count; ++element) {
                if (partners[element] == NoRepresentative) order.push_back(element);
            }
            for (std::uint32_t element = 0; element < binding.count; ++element) {
                if (partners[element] != NoRepresentative) order.push_back(element);
            }
        }
        for (std::uint32_t position = 0; position < binding.count; ++position) {
            const auto element = order.empty() ? position : order[position];
            const auto words = std::span<const std::uint32_t>(binding.guestDescriptor).subspan(static_cast<std::size_t>(element) * 8u, 8u);
            const auto absolute = static_cast<std::uint64_t>(table.elementBase) + element;
            if (absolute >= context.imageTableCapacity) {
                table.map[faults + element] = rejectTableElement(table, words, capacityMessage());
                reusable = false;
                continue;
            }
            if (!representatives.empty() && representatives[element] != NoRepresentative) {
                if (reusable) {
                    ReusedTableElement substitute;
                    substitute.substitute = true;
                    substitute.representative = representatives[element];
                    fresh.elements.push_back(std::move(substitute));
                }
                substitutes.push_back(element);
                continue;
            }
            if (!partners.empty() && partners[element] != NoRepresentative) {
                const auto partner = partners[element];
                const auto index = table.elementTextures[partner];
                if (index != std::numeric_limits<std::size_t>::max() && textures[index] != nullptr && textures[index]->HasArrayView()) {
                    const auto partnerWords = std::span<const std::uint32_t>(binding.guestDescriptor).subspan(static_cast<std::size_t>(partner) * 8u, 8u);
                    const auto partnerResource = DecodeTextureResource(partnerWords);
                    reusable = false;
                    fresh.bound[element] = bind(element, words, textures[index], partnerResource, DescribeSurface(partnerResource).guestBytes, false);
                    if (fresh.bound[element].empty()) {
                        arrayViewTextures.insert(textures.size() - 1u);
                        counters.arrayViewsShared.fetch_add(1, std::memory_order_relaxed);
                    }
                    continue;
                }
            }
            DecodedEntry decoded;
            try {
                decoded = decodeEntry(element, words);
            } catch (const std::runtime_error& error) {
                decoded.rejected = error.what();
            }
            if (!decoded.rejected.empty()) {
                table.map[faults + element] = rejectTableElement(table, words, decoded.rejected);
                if (reusable) fresh.elements.push_back({{}, {}, {}, 0, false, true, decoded.rejected});
                continue;
            }
            const auto& resource = decoded.resource;
            const auto guestBytes = decoded.guestBytes;
            const bool firstLayer = decoded.firstLayer;
            if (element < binding.imageWritten.size() && binding.imageWritten[element]) {
                reusable = false;
                const auto storageBinding = storageTableBindings[table.set];
                if (storageBinding == std::numeric_limits<std::size_t>::max()) {
                    table.map[faults + element] = rejectTableElement(table, words, "set " + std::to_string(table.set) + " has no image storage table, but the shader stores to its table element " + std::to_string(absolute));
                    continue;
                }
                if (absolute >= StorageTableCapacity(context)) {
                    table.map[faults + element] = rejectTableElement(table, words, "the stored table element " + std::to_string(absolute) + " of set " + std::to_string(table.set) + " is past the image storage table's capacity of " + std::to_string(StorageTableCapacity(context)) + " images");
                    continue;
                }
                CompletePendingCpuWrites(context, resource.baseAddress, static_cast<std::size_t>(guestBytes));
                if (resource.dccAddress != 0) CompletePendingCpuWrites(context, resource.dccAddress, DccKeyBytes(guestBytes));
                const auto mip = std::min(resource.baseLevel, resource.mipCount - 1u);
                if (resource.minLod > mip * 256u) {
                    table.map[faults + element] = rejectTableElement(table, words, "the table entry's storage T# clamps its minimum LOD above the level it addresses, which is not implemented");
                    continue;
                }
                const bool uintStorage = element < binding.imageUintStorage.size() && binding.imageUintStorage[element];
                try {
                    auto texture = cachedStorageTexture(context, words, resource, mip, guestBytes, true);
                    if (uintStorage && texture != nullptr) texture->StorageView(mip, firstLayer, true);
                    storageTextures.push_back(std::move(texture));
                } catch (const std::runtime_error& error) {
                    table.map[faults + element] = rejectTableElement(table, words, error.what());
                    continue;
                }
                storageMips.push_back(mip);
                storageKeys.push_back(resource.dccAddress);
                storageFirstLayer.push_back(firstLayer);
                storageWritten.push_back(true);
                storageAtomic.push_back(element < binding.imageAtomic.size() && binding.imageAtomic[element]);
                storageAtomic64.push_back(element < binding.imageAtomic64.size() && binding.imageAtomic64[element]);
                storageUint.push_back(uintStorage);
                describedRanges.push_back({"storage", resource.baseAddress, guestBytes, resource.width, resource.height, resource.format, static_cast<int>(resource.tileMode), resource.dccAddress, true});
                bindings[storageBinding].imageAllocations[absolute] = storageTextures.size() - 1u;
                continue;
            }
            if (item == nullptr) {
                table.map[faults + element] = rejectTableElement(table, words, "a sampled image table entry has no sampled image table binding");
                reusable = false;
                continue;
            }
            const auto sampled = prepareSampled(element, words, decoded);
            std::shared_ptr<Texture> texture;
            try {
                texture = lookupSampled(element, sampled);
            } catch (const std::runtime_error& error) {
                table.map[faults + element] = rejectTableElement(table, words, error.what());
                if (reusable) fresh.elements.push_back({{}, {}, {}, 0, false, true, error.what()});
                continue;
            }
            if (reusable) {
                const auto& log = lookupLog();
                const auto* looked = log.empty() ? nullptr : &log.back();
                if (looked != nullptr && looked->object == texture.get() && looked->source == nullptr && looked->generation != 0 && looked->keys == DccKeys::Uncompressed && sampled.resource.dccAddress == 0) {
                    fresh.elements.push_back({MakeTextureKey(context.device, sampled.words, sampled.components, sampled.depthCompare), texture, sampled.resource, sampled.guestBytes, firstLayer, false, {}});
                    fresh.queries.push_back({sampled.resource.baseAddress, static_cast<std::size_t>(sampled.guestBytes), looked->generation});
                    fresh.ranges.emplace_back(sampled.resource.baseAddress, sampled.resource.baseAddress + sampled.guestBytes);
                } else {
                    fresh.elements.push_back({{}, {}, {}, 0, firstLayer, false, {}, true});
                }
            }
            if (!representativeNeeded.empty() && representativeNeeded[element]) representativeSources.emplace(element, sampled.resource);
            fresh.bound[element] = bind(element, sampled.words, std::move(texture), sampled.resource, sampled.guestBytes, firstLayer);
        }
        for (const auto element : substitutes) {
            const auto representative = representatives[element];
            const auto words = std::span<const std::uint32_t>(binding.guestDescriptor).subspan(static_cast<std::size_t>(representative) * 8u, 8u);
            const auto index = table.elementTextures[representative];
            const auto source = representativeSources.find(representative);
            const auto message = index == std::numeric_limits<std::size_t>::max() || source == representativeSources.end() ? std::string("the dynamic-resolution variant this entry borrows from was rejected") : refusal(element, words, textures[index], source->second);
            if (reusable) {
                if (fresh.elements[representative].rejected || source == representativeSources.end()) reusable = false;
                else fresh.elements[element].representativeResource = source->second;
            }
            if (!message.empty()) {
                table.map[faults + element] = rejectTableElement(table, words, message);
                counters.variantRejected.fetch_add(1, std::memory_order_relaxed);
                continue;
            }
            table.elementTextures[element] = index;
            item->imageAllocations[static_cast<std::size_t>(table.elementBase) + element] = index;
            counters.variantSubstituted.fetch_add(1, std::memory_order_relaxed);
        }
        counters.tablesResolved.fetch_add(1, std::memory_order_relaxed);
        const bool anyKept = std::any_of(fresh.elements.begin(), fresh.elements.end(), [](const ReusedTableElement& element) { return !element.live; });
        if (reusable && anyKept && CurrentTableState() == state) {
            const auto serial = StorageTexture::PendingSerial();
            if (!StorageTexture::AnyPendingOverlaps(fresh.ranges) && StorageTexture::PendingSerial() == serial) {
                fresh.device = context.device;
                fresh.state = state;
                fresh.pendingSerial = serial;
                fresh.imageSamplers = binding.imageSamplers;
                fresh.samplers = std::move(samplerKey);
                fresh.replayable = static_cast<std::uint64_t>(table.elementBase) + binding.count <= context.imageTableCapacity;
                fresh.representatives = table.representatives;
                RememberTable(std::move(fresh), binding);
            }
        }
    }
    reportTextureCounters();
}

}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
}
