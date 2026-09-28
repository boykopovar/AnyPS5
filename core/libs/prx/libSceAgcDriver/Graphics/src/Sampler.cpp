#include "prx/libSceAgcDriver/Graphics/include/Sampler.hpp"
#include <algorithm>
#include <cstdlib>

namespace AgcDriver::Graphics {

    Sampler::Sampler(const Context& context, const GuestSamplerResource& descriptor) : context(context) {
        Require(!descriptor.anisotropyEnable || context.samplerAnisotropy, "guest sampler descriptor requests anisotropic filtering which the device does not support");
        Require(descriptor.maxAnisotropy <= context.limits.maxSamplerAnisotropy, "guest sampler descriptor requests an anisotropy ratio beyond the device limit");
        Require(descriptor.lodBias >= -context.limits.maxSamplerLodBias && descriptor.lodBias <= context.limits.maxSamplerLodBias, "guest sampler descriptor requests a LOD bias beyond the device limit");

        VkSamplerCreateInfo info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
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
        info.unnormalizedCoordinates = VK_FALSE;
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

    SamplerCache::SamplerCache(std::size_t capacity) : capacity(std::max<std::size_t>(capacity, 1)) {}

    std::shared_ptr<Sampler> SamplerCache::Get(const Context& context, std::span<const std::uint32_t> words, bool compareEnable) {
        Require(words.size() == 4, "guest sampler descriptor must contain 4 dwords");
        const std::array<std::uint32_t, 5> key{words[0], words[1], words[2], words[3], compareEnable ? 1u : 0u};
        std::lock_guard lock(mutex);
        ++clock;
        if (const auto found = entries.find(key); found != entries.end()) {
            ++hits;
            found->second.lastUse = clock;
            return found->second.sampler;
        }
        ++misses;
        auto resource = DecodeSamplerResource(words);
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

}
