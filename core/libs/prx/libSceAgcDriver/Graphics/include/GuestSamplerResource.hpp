#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GUESTSAMPLERRESOURCE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_GUESTSAMPLERRESOURCE_HPP

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>
#include <cstdint>
#include <span>

namespace AgcDriver::Graphics {

struct GuestSamplerResource {
    VkFilter magFilter;
    VkFilter minFilter;
    VkSamplerMipmapMode mipmapMode;
    VkSamplerAddressMode addressModeU;
    VkSamplerAddressMode addressModeV;
    VkSamplerAddressMode addressModeW;
    bool anisotropyEnable;
    float maxAnisotropy;
    float minLod;
    float maxLod;
    float lodBias;
    VkBorderColor borderColor;
    VkSamplerReductionMode reductionMode = VK_SAMPLER_REDUCTION_MODE_WEIGHTED_AVERAGE_EXT;
    bool forceDegamma = false;
    bool compareEnable = false;
    VkCompareOp compareOp = VK_COMPARE_OP_NEVER;
    bool unnormalizedCoordinates = false;
};

GuestSamplerResource DecodeSamplerResource(std::span<const std::uint32_t> words, bool unnormalizedProven = false, bool forceDegammaPaired = false);

}

#endif
