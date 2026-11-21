#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DLSSUPSCALER_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DLSSUPSCALER_HPP

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <cstdint>
#include <memory>

namespace AgcDriver {

struct DlssParameters {
    VkImageView color = VK_NULL_HANDLE;
    VkImageView depth = VK_NULL_HANDLE;
    VkImageView motionVectors = VK_NULL_HANDLE;
    VkImageView destination = VK_NULL_HANDLE;
    VkImage colorImage = VK_NULL_HANDLE;
    VkImage depthImage = VK_NULL_HANDLE;
    VkImage motionVectorsImage = VK_NULL_HANDLE;
    VkImage destinationImage = VK_NULL_HANDLE;
    VkFormat colorFormat = VK_FORMAT_UNDEFINED;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    VkFormat motionVectorsFormat = VK_FORMAT_UNDEFINED;
    VkFormat destinationFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D inputExtent{};
    VkExtent2D outputExtent{};
    float jitterX = 0.0f;
    float jitterY = 0.0f;
    float sharpness = 0.0f;
    bool reset = false;
};

struct DlssSupportDetails {
    bool libraryLoaded = false;
    std::uint32_t initResult = 0;
    int needsUpdatedDriver = 0;
    int isSupported = 0;
    int featureInitResult = 0;
};

class DlssUpscaler {
public:
    static bool IsAvailable() noexcept;
    static bool SupportsDevice(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);
    static bool QueryDeviceSupport(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, DlssSupportDetails& details);

    DlssUpscaler(const Graphics::Context& context, VkInstance instance, VkExtent2D inputExtent, VkExtent2D outputExtent, int quality = 2, float sharpness = 0.0f);
    ~DlssUpscaler();

    DlssUpscaler(const DlssUpscaler&) = delete;
    DlssUpscaler& operator=(const DlssUpscaler&) = delete;

    void RecordUpscale(VkCommandBuffer commands, const DlssParameters& params);
    void ResetExtents(VkExtent2D inputExtent, VkExtent2D outputExtent);
    void Release() noexcept;

    VkExtent2D InputExtent() const noexcept;
    VkExtent2D OutputExtent() const noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

}

#endif
