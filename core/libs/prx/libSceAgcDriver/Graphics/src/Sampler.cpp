#include "prx/libSceAgcDriver/Graphics/include/Sampler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureFormat.hpp"
#include <algorithm>
#include <cstdlib>
#include <string>

namespace AgcDriver::Graphics {

    Sampler::Sampler(const Context& context, const GuestSamplerResource& descriptor) : context(context) {
        Require(!descriptor.anisotropyEnable || context.samplerAnisotropy, "guest sampler descriptor requests anisotropic filtering which the device does not support");
        Require(descriptor.maxAnisotropy <= context.limits.maxSamplerAnisotropy, "guest sampler descriptor requests an anisotropy ratio beyond the device limit");
        Require(descriptor.lodBias >= -context.limits.maxSamplerLodBias && descriptor.lodBias <= context.limits.maxSamplerLodBias, "guest sampler descriptor requests a LOD bias beyond the device limit");
        Require(!(descriptor.unnormalizedCoordinates && descriptor.compareEnable), "guest sampler descriptor with unnormalized coordinates enables depth comparison, which is not implemented");
        forcesDegamma = descriptor.forceDegamma;

        VkSamplerCreateInfo info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        VkSamplerReductionModeCreateInfoEXT reduction{VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO_EXT, nullptr, descriptor.reductionMode};
        if (descriptor.reductionMode != VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE_EXT) {
            Require(context.samplerFilterMinmax, "guest sampler descriptor uses a min or max reduction which the device does not support");
            Require(!descriptor.compareEnable, "guest sampler descriptor combines a min or max reduction with depth comparison, which is not implemented");
            info.pNext = &reduction;
            requiresFilterMinmax = descriptor.magFilter == VK_FILTER_LINEAR || descriptor.minFilter == VK_FILTER_LINEAR;
        }
        info.magFilter = descriptor.magFilter;
        info.minFilter = descriptor.minFilter;
        info.mipmapMode = descriptor.mipmapMode;
        info.addressModeU = descriptor.addressModeU;
        info.addressModeV = descriptor.addressModeV;
        info.addressModeW = descriptor.addressModeW;
        info.mipLodBias = descriptor.lodBias;
        info.anisotropyEnable = descriptor.anisotropyEnable ? VK_TRUE : VK_FALSE;
        info.maxAnisotropy = descriptor.maxAnisotropy;
        info.compareEnable = descriptor.compareEnable ? VK_TRUE : VK_FALSE;
        info.compareOp = descriptor.compareOp;
        info.minLod = descriptor.minLod;
        info.maxLod = descriptor.maxLod;
        info.borderColor = descriptor.borderColor;
        const bool clampsToBorder = descriptor.addressModeU == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER || descriptor.addressModeV == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER || descriptor.addressModeW == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        opaqueBlackBorder = clampsToBorder && (descriptor.borderColor == VK_BORDER_COLOR_INT_OPAQUE_BLACK || descriptor.borderColor == VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK);
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

    SamplerCache::SamplerCache(std::size_t capacity) : capacity(std::max<std::size_t>(capacity, 1)) {}

    std::shared_ptr<Sampler> SamplerCache::Get(const Context& context, std::span<const std::uint32_t> words, bool compareEnable, bool unnormalizedProven, bool forceDegammaPaired) {
        Require(words.size() == 4, "guest sampler descriptor must contain 4 dwords");
        const std::array<std::uint32_t, 5> key{words[0], words[1], words[2], words[3], (compareEnable ? 1u : 0u) | (unnormalizedProven ? 2u : 0u)};
        std::lock_guard lock(mutex);
        ++clock;
        if (const auto found = entries.find(key); found != entries.end()) {
            Require(forceDegammaPaired || !found->second.sampler->ForcesDegamma(), "guest sampler descriptor forces sRGB decoding which is not implemented");
            ++hits;
            found->second.lastUse = clock;
            return found->second.sampler;
        }
        ++misses;
        auto resource = DecodeSamplerResource(words, unnormalizedProven, forceDegammaPaired);
        resource.compareEnable = compareEnable;
        auto sampler = std::make_shared<Sampler>(context, resource);
        // The cap keeps live samplers well below the device's limit (NVIDIA: ~4000); a set in flight
        // still holds the evicted sampler through its own shared_ptr.
        while (entries.size() >= capacity) {
            const auto oldest = std::min_element(entries.begin(), entries.end(), [](const auto& left, const auto& right) { return left.second.lastUse < right.second.lastUse; });
            entries.erase(oldest);
        }
        entries.emplace(key, Entry{sampler, clock});
        return sampler;
    }

    void RequireFilterMinmax(const Context& context, VkFormat format, std::uint32_t samplerMask, std::span<const std::shared_ptr<Sampler>> samplers) {
        bool filtered = false;
        for (std::uint32_t element = 0; element < 32u; ++element) {
            if (((samplerMask >> element) & 1u) == 0u) continue;
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

    bool Sampler::RequiresNonSeamlessCube() const {
        return requiresNonSeamlessCube;
    }

    void RequireDegammaFormat(std::uint32_t guestFormat, std::uint32_t samplerMask, std::span<const std::shared_ptr<Sampler>> samplers) {
        for (std::uint32_t element = 0; element < 32u && element < samplers.size(); ++element) {
            if (((samplerMask >> element) & 1u) != 0u && samplers[element]->ForcesDegamma() && !IsSrgbTextureFormat(guestFormat)) Require(false, "a texture of guest format " + std::to_string(guestFormat) + ", which is not sRGB, is sampled through a sampler that forces sRGB decoding, which is not implemented");
        }
    }

    void RequireBorderSwizzle(std::uint32_t bcSwizzle, std::uint32_t samplerMask, std::span<const std::shared_ptr<Sampler>> samplers) {
        constexpr std::uint32_t BcSwizzleXyzw = 0u;
        constexpr std::uint32_t BcSwizzleZyxw = 4u;
        if (bcSwizzle == BcSwizzleXyzw || bcSwizzle == BcSwizzleZyxw) return;
        for (std::uint32_t element = 0; element < 32u; ++element) {
            if (((samplerMask >> element) & 1u) == 0u) continue;
            if (element >= samplers.size()) Require(false, "a sampled texture is paired with sampler element " + std::to_string(element) + ", which its shader does not bind");
            if (samplers[element]->ReadsOpaqueBlackBorder()) Require(false, "a sampled texture whose BC swizzle " + std::to_string(bcSwizzle) + " moves the alpha channel is sampled through an opaque black border, which is not implemented");
        }
    }

    void RequireNonSeamlessCube(bool cube, std::uint32_t samplerMask, std::span<const std::shared_ptr<Sampler>> samplers) {
        if (!cube) return;
        for (std::uint32_t element = 0; element < 32u && element < samplers.size(); ++element) {
            if (((samplerMask >> element) & 1u) != 0u && samplers[element]->RequiresNonSeamlessCube()) Require(false, "a cube texture is sampled through a sampler that disables seamless cube filtering, which needs VK_EXT_non_seamless_cube_map");
        }
    }

}
