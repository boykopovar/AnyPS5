#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PIPELINEBINARYSTORE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PIPELINEBINARYSTORE_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <array>
#include <filesystem>
#include <span>

namespace AgcDriver::Graphics {

struct alignas(64) PipelineBinaryKeyOutput {
    VkPipelineBinaryKeyKHR key{VK_STRUCTURE_TYPE_PIPELINE_BINARY_KEY_KHR};
    std::array<std::uint8_t, 64> guard;
};

enum class PipelineBinaryPolicy { Enabled, Disabled, DisabledByDriver };

class PipelineBinaryStore {
public:
    static VkDriverId DriverIdentity(VkPhysicalDevice physical, std::uint32_t apiVersion, bool driverPropertiesExtension, PFN_vkGetPhysicalDeviceProperties2 query);
    static PipelineBinaryPolicy PolicyForDriver(VkDriverId driverId);
    static void ReportPolicy(PipelineBinaryPolicy policy);
    static bool LayersCompatible(VkPhysicalDevice physical, PFN_vkEnumerateDeviceLayerProperties enumerate);
    PipelineBinaryStore(const Context& context, const VkPhysicalDeviceProperties& properties, VkDriverId driverId);
    PipelineBinaryPolicy PolicyStatus() const { return policy; }
    bool IsReady() const { return !directory.empty(); }
    std::vector<std::byte> Key(const VkComputePipelineCreateInfo& info, std::span<const std::uint32_t> spirv, std::span<const std::uint32_t> layout) const;
    void CreateComputePipeline(const VkComputePipelineCreateInfo& info, std::span<const std::uint32_t> spirv, std::span<const std::uint32_t> layout, VkPipeline* pipeline) const;

private:
    enum class RestoreResult { Missing, Restored, BelowThreshold, CaptureFailed };
    RestoreResult restore(const VkComputePipelineCreateInfo& info, std::span<const std::byte> key, VkPipeline* pipeline) const;
    RestoreResult restoreFile(const VkComputePipelineCreateInfo& info, std::span<const std::byte> key, VkPipeline* pipeline, const std::filesystem::path& path) const;
    bool capture(VkPipeline pipeline, std::span<const std::byte> key) const;
    void rememberThreshold(std::span<const std::byte> key, bool captureFailed = false) const;
    std::filesystem::path filePath(std::span<const std::byte> key) const;

    Context context;
    PipelineBinaryPolicy policy;
    VkPhysicalDeviceProperties properties{};
    VkPipelineBinaryKeyKHR globalKey{VK_STRUCTURE_TYPE_PIPELINE_BINARY_KEY_KHR};
    std::filesystem::path directory;
    std::size_t minimumWords = 100000;
    std::uint64_t minimumMilliseconds = 1000;
    PFN_vkGetPipelineKeyKHR getKey = nullptr;
    PFN_vkCreatePipelineBinariesKHR createBinaries = nullptr;
    PFN_vkDestroyPipelineBinaryKHR destroyBinary = nullptr;
    PFN_vkGetPipelineBinaryDataKHR getData = nullptr;
    PFN_vkReleaseCapturedPipelineDataKHR releaseData = nullptr;
};

}

#endif
