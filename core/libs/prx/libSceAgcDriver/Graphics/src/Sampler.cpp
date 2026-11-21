#include "prx/libSceAgcDriver/Graphics/include/Sampler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include "PipelineSpecialization.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <bit>
#include <cstdlib>
#include <string>

namespace AgcDriver::Graphics {

    namespace {

        std::array<VkComponentSwizzle, 4> Swizzles(const VkComponentMapping& mapping) {
            std::array<VkComponentSwizzle, 4> swizzles{mapping.r, mapping.g, mapping.b, mapping.a};
            for (std::size_t component = 0; component < swizzles.size(); ++component) {
                if (swizzles[component] == VK_COMPONENT_SWIZZLE_IDENTITY) swizzles[component] = static_cast<VkComponentSwizzle>(VK_COMPONENT_SWIZZLE_R + component);
            }
            return swizzles;
        }

        bool IsIdentityMapping(const VkComponentMapping& mapping) {
            return Swizzles(mapping) == std::array{VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A};
        }

        bool SameMapping(const BorderComponentMapping& left, const BorderComponentMapping& right) {
            return Swizzles(left.components) == Swizzles(right.components) && left.srgb == right.srgb;
        }

        bool HostOpaqueBlackOne(VkComponentSwizzle swizzle) {
            return swizzle == VK_COMPONENT_SWIZZLE_ONE || swizzle == VK_COMPONENT_SWIZZLE_A;
        }

        bool GuestOpaqueBlackOne(std::uint8_t dstSel) {
            return dstSel == 1u || dstSel == 7u;
        }

        bool KeepsOpaqueBlack(const OpaqueBlackPairing& pairing) {
            for (std::size_t component = 0; component < pairing.dstSel.size(); ++component) {
                if (GuestOpaqueBlackOne(pairing.dstSel[component]) != (component == 3u)) return false;
            }
            return true;
        }

        bool SwizzlesLikeGuest(const OpaqueBlackPairing& pairing) {
            const auto swizzles = Swizzles(pairing.view.components);
            for (std::size_t component = 0; component < swizzles.size(); ++component) {
                if (HostOpaqueBlackOne(swizzles[component]) != GuestOpaqueBlackOne(pairing.dstSel[component])) return false;
            }
            return true;
        }

        bool Compatible(const std::optional<BorderComponentMapping>& mapping, const OpaqueBlackPairing& pairing) {
            if (mapping.has_value()) return SameMapping(*mapping, pairing.view) && SwizzlesLikeGuest(pairing);
            return IsIdentityMapping(pairing.view.components) || KeepsOpaqueBlack(pairing);
        }

    }

    bool Sampler::ShaderAppliesLodBias(const GuestSamplerResource& descriptor) {
        return descriptor.lodBias < -ShaderRecompiler::PipelineSpecialization::SamplerLodBiasShaderThreshold || descriptor.lodBias > ShaderRecompiler::PipelineSpecialization::SamplerLodBiasShaderThreshold;
    }

    void Sampler::Validate(const Context& context, const GuestSamplerResource& descriptor) {
        Require(!descriptor.anisotropyEnable || context.samplerAnisotropy, "guest sampler descriptor requests anisotropic filtering which the device does not support");
        Require(descriptor.maxAnisotropy <= context.limits.maxSamplerAnisotropy, "guest sampler descriptor requests an anisotropy ratio beyond the device limit");
        Require(ShaderAppliesLodBias(descriptor) || (descriptor.lodBias >= -context.limits.maxSamplerLodBias && descriptor.lodBias <= context.limits.maxSamplerLodBias), "guest sampler descriptor requests a LOD bias beyond the device limit");
        const auto mirrorsOnce = [](VkSamplerAddressMode mode) { return mode == VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE; };
        Require(context.samplerMirrorClampToEdge || !(mirrorsOnce(descriptor.addressModeU) || mirrorsOnce(descriptor.addressModeV) || mirrorsOnce(descriptor.addressModeW)), "guest sampler descriptor uses a mirror-once clamp mode which the device does not support");
        Require(!(descriptor.unnormalizedCoordinates && descriptor.compareEnable), "guest sampler descriptor with unnormalized coordinates enables depth comparison, which is not implemented");
        if (descriptor.reductionMode != VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE_EXT) {
            Require(context.samplerFilterMinmax, "guest sampler descriptor uses a min or max reduction which the device does not support");
            Require(!descriptor.compareEnable, "guest sampler descriptor combines a min or max reduction with depth comparison, which is not implemented");
        }
    }

    Sampler::Sampler(const Context& context, const GuestSamplerResource& descriptor, const std::optional<BorderComponentMapping>& borderMapping) : context(context), borderMapping(borderMapping) {
        Validate(context, descriptor);
        forcesDegamma = descriptor.forceDegamma;
        disablesDegamma = descriptor.disableDegamma;

        VkSamplerCreateInfo info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        VkSamplerReductionModeCreateInfoEXT reduction{VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT, nullptr, descriptor.reductionMode};
        if (descriptor.reductionMode != VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE_EXT) {
            info.pNext = &reduction;
            requiresFilterMinmax = descriptor.magFilter == VK_FILTER_LINEAR || descriptor.minFilter == VK_FILTER_LINEAR;
        }
        VkSamplerBorderColorComponentMappingCreateInfoEXT borderComponents{VK_STRUCTURE_TYPE_SAMPLER_BORDER_COLOR_COMPONENT_MAPPING_CREATE_INFO_EXT};
        if (borderMapping.has_value()) {
            Require(context.borderColorSwizzle, "a sampler with a border color component mapping needs VK_EXT_border_color_swizzle");
            borderComponents.pNext = info.pNext;
            borderComponents.components = borderMapping->components;
            borderComponents.srgb = borderMapping->srgb ? VK_TRUE : VK_FALSE;
            info.pNext = &borderComponents;
        }
        info.magFilter = descriptor.magFilter;
        info.minFilter = descriptor.minFilter;
        info.mipmapMode = descriptor.mipmapMode;
        info.addressModeU = descriptor.addressModeU;
        info.addressModeV = descriptor.addressModeV;
        info.addressModeW = descriptor.addressModeW;
        info.mipLodBias = ShaderAppliesLodBias(descriptor) ? 0.0f : descriptor.lodBias;
        info.anisotropyEnable = descriptor.anisotropyEnable ? VK_TRUE : VK_FALSE;
        info.maxAnisotropy = descriptor.maxAnisotropy;
        info.compareEnable = descriptor.compareEnable ? VK_TRUE : VK_FALSE;
        info.compareOp = descriptor.compareOp;
        info.minLod = descriptor.minLod;
        info.maxLod = descriptor.maxLod;
        info.borderColor = descriptor.borderColor;
        opaqueBlackBorder = ReadsOpaqueBlackBorder(descriptor);
        info.unnormalizedCoordinates = descriptor.unnormalizedCoordinates ? VK_TRUE : VK_FALSE;
        if (descriptor.nonSeamlessCube) {
            if (context.nonSeamlessCubeMap) info.flags |= VK_SAMPLER_CREATE_NON_SEAMLESS_CUBE_MAP_BIT_EXT;
            else requiresNonSeamlessCube = true;
        }
        Check(context.Function<PFN_vkCreateSampler>("vkCreateSampler")(context.device, &info, nullptr, &sampler), "vkCreateSampler");
    }

    Sampler::~Sampler() {
        release();
    }

    void Sampler::release() noexcept {
        if (sampler) context.Function<PFN_vkDestroySampler>("vkDestroySampler")(context.device, sampler, nullptr);
    }

    VkSampler Sampler::Handle() const {
        return sampler;
    }

    bool Sampler::RequiresFilterMinmax() const {
        return requiresFilterMinmax;
    }

    bool Sampler::ReadsOpaqueBlackBorder() const {
        return opaqueBlackBorder;
    }

    bool Sampler::ReadsOpaqueBlackBorder(const GuestSamplerResource& descriptor) {
        const bool clampsToBorder = descriptor.addressModeU == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER || descriptor.addressModeV == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER || descriptor.addressModeW == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        return clampsToBorder && (descriptor.borderColor == VK_BORDER_COLOR_INT_OPAQUE_BLACK || descriptor.borderColor == VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK);
    }

    SamplerCache::SamplerCache(std::size_t capacity) : capacity(std::max<std::size_t>(capacity, 1)) {}

    std::shared_ptr<Sampler> SamplerCache::Get(const Context& context, std::span<const std::uint32_t> words, bool compareEnable, bool unnormalizedProven, bool forceDegammaPaired, bool integerBorder, const std::optional<BorderComponentMapping>& borderMapping) {
        Require(words.size() == 4, "guest sampler descriptor must contain 4 dwords");
        const std::array<std::uint32_t, 5> key{words[0], words[1], words[2], words[3], (compareEnable ? 1u : 0u) | (unnormalizedProven ? 2u : 0u) | (integerBorder ? 4u : 0u) | (BorderMappingKey(borderMapping) << 4u)};
        std::lock_guard lock(mutex);
        ++clock;
        if (const auto found = entries.find(key); found != entries.end()) {
            Require(forceDegammaPaired || !found->second.sampler->ForcesDegamma(), "guest sampler descriptor forces sRGB decoding which is not implemented");
            ++hits;
            found->second.lastUse = clock;
            return found->second.sampler;
        }
        ++misses;
        auto resource = DecodeSamplerResource(words, unnormalizedProven, forceDegammaPaired, integerBorder);
        resource.compareEnable = compareEnable;
        auto sampler = std::make_shared<Sampler>(context, resource, borderMapping);
        // The cap keeps live samplers well below the device's limit (NVIDIA: ~4000); a set in flight
        // still holds the evicted sampler through its own shared_ptr.
        while (entries.size() >= capacity) {
            const auto oldest = std::min_element(entries.begin(), entries.end(), [](const auto& left, const auto& right) { return left.second.lastUse < right.second.lastUse; });
            entries.erase(oldest);
        }
        entries.emplace(key, Entry{sampler, clock});
        return sampler;
    }

    void RequireFilterMinmax(const Context& context, VkFormat format, std::uint64_t samplerMask, std::span<const std::shared_ptr<Sampler>> samplers) {
        bool filtered = false;
        for (auto mask = samplerMask; mask != 0u; mask &= mask - 1u) {
            const auto element = static_cast<std::uint32_t>(std::countr_zero(mask));
            if (element >= samplers.size()) Require(false, "a sampled texture is paired with sampler element " + std::to_string(element) + ", which its shader does not bind");
            filtered = filtered || samplers[element]->RequiresFilterMinmax();
        }
        if (!filtered) return;
        VkFormatProperties properties{};
        context.formatProperties(context.physical, format, &properties);
        if ((properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_MINMAX_BIT_EXT) == 0) Require(false, "a sampled texture whose format " + std::to_string(format) + " does not support min/max filtering is sampled through a min or max reduction sampler with linear filtering");
    }

    bool Sampler::ForcesDegamma() const {
        return forcesDegamma;
    }
    bool Sampler::DisablesDegamma() const {
        return disablesDegamma;
    }


    bool Sampler::RequiresNonSeamlessCube() const {
        return requiresNonSeamlessCube;
    }

    bool ForcesDegammaOn(std::uint32_t guestFormat, std::uint64_t samplerMask, std::span<const std::shared_ptr<Sampler>> samplers) {
        if (IsSrgbTextureFormat(guestFormat) || IgnoresForcedDegamma(guestFormat)) return false;
        for (auto mask = samplerMask; mask != 0u; mask &= mask - 1u) {
            const auto element = static_cast<std::uint32_t>(std::countr_zero(mask));
            if (element < samplers.size() && samplers[element]->ForcesDegamma()) return true;
        }
        return false;
    }

    void RequireBorderSwizzle(std::uint32_t bcSwizzle, std::uint64_t samplerMask, std::span<const std::shared_ptr<Sampler>> samplers) {
        constexpr std::uint32_t BcSwizzleXyzw = 0u;
        constexpr std::uint32_t BcSwizzleZyxw = 4u;
        if (bcSwizzle == BcSwizzleXyzw || bcSwizzle == BcSwizzleZyxw) return;
        for (auto mask = samplerMask; mask != 0u; mask &= mask - 1u) {
            const auto element = static_cast<std::uint32_t>(std::countr_zero(mask));
            if (element >= samplers.size()) Require(false, "a sampled texture is paired with sampler element " + std::to_string(element) + ", which its shader does not bind");
            if (samplers[element]->ReadsOpaqueBlackBorder()) Require(false, "a sampled texture whose BC swizzle " + std::to_string(bcSwizzle) + " moves the alpha channel is sampled through an opaque black border, which is not implemented");
        }
    }

    std::uint32_t BorderMappingKey(const std::optional<BorderComponentMapping>& mapping) {
        if (!mapping.has_value()) return 0u;
        std::uint32_t key = 1u | (mapping->srgb ? 2u : 0u);
        const auto swizzles = Swizzles(mapping->components);
        for (std::size_t component = 0; component < swizzles.size(); ++component) key |= (static_cast<std::uint32_t>(swizzles[component]) & 7u) << (2u + 3u * component);
        return key;
    }

    std::optional<BorderComponentMapping> OpaqueBlackBorderMapping(const Context& context, std::span<const OpaqueBlackPairing> pairings) {
        if (context.borderColorSwizzleFromImage || pairings.empty()) return std::nullopt;
        if (std::all_of(pairings.begin(), pairings.end(), [](const OpaqueBlackPairing& pairing) { return Compatible(std::nullopt, pairing); })) return std::nullopt;
        if (!context.borderColorSwizzle) {
            static std::once_flag once;
            std::call_once(once, [] { APS5_LOG_CHARS_OUT("Sampler: the device lacks VK_EXT_border_color_swizzle, so an opaque black border that a texture's swizzle moves keeps the host's unswizzled border"); });
            return std::nullopt;
        }
        const BorderComponentMapping mapping = std::find_if(pairings.begin(), pairings.end(), [](const OpaqueBlackPairing& pairing) { return !Compatible(std::nullopt, pairing); })->view;
        for (const auto& pairing : pairings) {
            if (!SameMapping(mapping, pairing.view)) {
                static std::once_flag once;
                std::call_once(once, [] { APS5_LOG_CHARS_OUT("Sampler: a sampler with an opaque black border is paired with textures of different swizzles, one of which moves the border, so it keeps the host's unswizzled border"); });
                return std::nullopt;
            }
            if (!SwizzlesLikeGuest(pairing)) {
                static std::once_flag once;
                std::call_once(once, [] { APS5_LOG_CHARS_OUT("Sampler: a texture whose host channel order changes the swizzled opaque black border of its sampler keeps the host's unswizzled border"); });
                return std::nullopt;
            }
        }
        return mapping;
    }

    bool CheckOpaqueBlackPairing(const Context& context, const OpaqueBlackPairing& pairing, std::uint64_t samplerMask, std::span<const std::shared_ptr<Sampler>> samplers) {
        if (context.borderColorSwizzleFromImage) return true;
        for (auto mask = samplerMask; mask != 0u; mask &= mask - 1u) {
            const auto element = static_cast<std::uint32_t>(std::countr_zero(mask));
            if (element >= samplers.size()) Require(false, "a sampled texture is paired with sampler element " + std::to_string(element) + ", which its shader does not bind");
            if (samplers[element]->ReadsOpaqueBlackBorder() && !Compatible(samplers[element]->BorderMapping(), pairing)) {
                static std::once_flag once;
                std::call_once(once, [] { APS5_LOG_CHARS_OUT("Sampler: an image table texture whose swizzle disagrees with the opaque black border mapping of its sampler gets its sampler's border"); });
                return false;
            }
        }
        return true;
    }

    void RequireNonSeamlessCube(bool cube, std::uint64_t samplerMask, std::span<const std::shared_ptr<Sampler>> samplers) {
        if (!cube) return;
        for (auto mask = samplerMask; mask != 0u; mask &= mask - 1u) {
            const auto element = static_cast<std::uint32_t>(std::countr_zero(mask));
            if (element < samplers.size() && samplers[element]->RequiresNonSeamlessCube()) Require(false, "a cube texture is sampled through a sampler that disables seamless cube filtering, which needs VK_EXT_non_seamless_cube_map");
        }
    }
    void RequireDisablesDegammaOn(std::uint32_t guestFormat, std::uint64_t samplerMask, std::span<const std::shared_ptr<Sampler>> samplers) {
        if (!IsSrgbTextureFormat(guestFormat)) return;
        for (auto mask = samplerMask; mask != 0u; mask &= mask - 1u) {
            const auto element = static_cast<std::uint32_t>(std::countr_zero(mask));
            if (element < samplers.size() && samplers[element]->DisablesDegamma()) {
                Require(false, "guest sampler descriptor disables degamma with an sRGB texture, which is not implemented");
            }
        }
    }

}
