#ifndef AGC_GRAPHICS_DESCRIPTOR_HEAP_OBJECTS_HPP
#define AGC_GRAPHICS_DESCRIPTOR_HEAP_OBJECTS_HPP

#include <vulkan/vulkan.h>

#include <optional>

namespace AgcDriver::Graphics {

struct ImageViewRecord {
    VkImageViewCreateInfo info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    VkImageViewUsageCreateInfo usage{VK_STRUCTURE_TYPE_IMAGE_VIEW_USAGE_CREATE_INFO};
    VkImageViewMinLodCreateInfoEXT minLod{VK_STRUCTURE_TYPE_IMAGE_VIEW_MIN_LOD_CREATE_INFO_EXT};
    bool hasUsage = false;
    bool hasMinLod = false;
    const VkImageViewCreateInfo* Chain();
};

struct SamplerRecord {
    VkSamplerCreateInfo info{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    VkSamplerReductionModeCreateInfo reduction{VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO};
    bool hasReduction = false;
    const VkSamplerCreateInfo* Chain();
};

PFN_vkVoidFunction DescriptorHeapObjectFunction(VkDevice device, PFN_vkGetDeviceProcAddr deviceProc, const char* name);
std::optional<ImageViewRecord> FindImageView(VkImageView view);
std::optional<SamplerRecord> FindSampler(VkSampler sampler);

}

#endif
