#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PLACEHOLDERIMAGES_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PLACEHOLDERIMAGES_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "RuntimeAbi.hpp"
#include <array>
#include <cstdint>

namespace AgcDriver::Graphics {

inline constexpr std::uint32_t PlaceholderDimensionsPerClass = (ShaderRecompiler::RuntimeAbi::FirstComparisonImageBinding - ShaderRecompiler::RuntimeAbi::FirstImageBinding) / 3u;
static_assert(ShaderRecompiler::RuntimeAbi::FirstStorageImageBinding == ShaderRecompiler::RuntimeAbi::FirstComparisonImageBinding + PlaceholderDimensionsPerClass);
static_assert(ShaderRecompiler::RuntimeAbi::ImageBindingCount == 8u * PlaceholderDimensionsPerClass && PlaceholderDimensionsPerClass == 7u);

struct PlaceholderImageShape {
    VkFormat format;
    VkFormatFeatureFlags features;
    VkImageUsageFlags usage;
    VkImageAspectFlags aspect;
    VkImageType type;
    VkImageViewType viewType;
    VkSampleCountFlagBits samples;
};

constexpr PlaceholderImageShape PlaceholderImageShapeOf(std::uint32_t heap) {
    constexpr std::array<VkFormat, 8> formats{VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R32_UINT, VK_FORMAT_R32_SINT, VK_FORMAT_D16_UNORM,
                                              VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R32_UINT, VK_FORMAT_R32_UINT, VK_FORMAT_R64_UINT};
    constexpr std::array<VkImageType, PlaceholderDimensionsPerClass> types{VK_IMAGE_TYPE_1D, VK_IMAGE_TYPE_1D, VK_IMAGE_TYPE_2D, VK_IMAGE_TYPE_2D,
                                                                           VK_IMAGE_TYPE_2D, VK_IMAGE_TYPE_2D, VK_IMAGE_TYPE_3D};
    constexpr std::array<VkImageViewType, PlaceholderDimensionsPerClass> views{VK_IMAGE_VIEW_TYPE_1D, VK_IMAGE_VIEW_TYPE_1D_ARRAY, VK_IMAGE_VIEW_TYPE_2D,
                                                                               VK_IMAGE_VIEW_TYPE_2D_ARRAY, VK_IMAGE_VIEW_TYPE_2D, VK_IMAGE_VIEW_TYPE_2D_ARRAY,
                                                                               VK_IMAGE_VIEW_TYPE_3D};
    constexpr std::array<VkSampleCountFlagBits, PlaceholderDimensionsPerClass> samples{VK_SAMPLE_COUNT_1_BIT, VK_SAMPLE_COUNT_1_BIT, VK_SAMPLE_COUNT_1_BIT,
                                                                                       VK_SAMPLE_COUNT_1_BIT, VK_SAMPLE_COUNT_4_BIT, VK_SAMPLE_COUNT_4_BIT,
                                                                                       VK_SAMPLE_COUNT_1_BIT};
    const auto index = heap - ShaderRecompiler::RuntimeAbi::FirstImageBinding;
    const auto kind = index / PlaceholderDimensionsPerClass;
    const auto format = formats[kind];
    const auto dimension = index % PlaceholderDimensionsPerClass;
    const bool storage = heap >= ShaderRecompiler::RuntimeAbi::FirstStorageImageBinding;
    const bool atomic = kind >= 6u;
    const VkFormatFeatureFlags access = atomic ? VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT
                                       : storage ? VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT
                                       : VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    return {format,
            static_cast<VkFormatFeatureFlags>(access | VK_FORMAT_FEATURE_TRANSFER_DST_BIT),
            static_cast<VkImageUsageFlags>((storage ? VK_IMAGE_USAGE_STORAGE_BIT : VK_IMAGE_USAGE_SAMPLED_BIT) | VK_IMAGE_USAGE_TRANSFER_DST_BIT),
            static_cast<VkImageAspectFlags>(format == VK_FORMAT_D16_UNORM ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT),
            types[dimension],
            views[dimension],
            samples[dimension]};
}

class PlaceholderImages {
public:
    explicit PlaceholderImages(const Context& context);
    ~PlaceholderImages();
    PlaceholderImages(const PlaceholderImages&) = delete;
    PlaceholderImages& operator=(const PlaceholderImages&) = delete;
    VkImageView View(std::uint32_t heap) const;

private:
    struct Image {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
    };
    void release() noexcept;
    Context context;
    std::array<Image, ShaderRecompiler::RuntimeAbi::ImageBindingCount> images{};
};

}

#endif
