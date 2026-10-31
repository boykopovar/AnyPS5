#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PIPELINEBINARYSTORE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PIPELINEBINARYSTORE_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <filesystem>
#include <span>

namespace AgcDriver::Graphics {

class PipelineBinaryStore {
public:
    PipelineBinaryStore(const Context& context, const VkPhysicalDeviceProperties& properties);
    std::vector<std::byte> Key(const VkComputePipelineCreateInfo& info, std::span<const std::uint32_t> spirv, std::span<const std::uint32_t> layout) const;
    void CreateComputePipeline(const VkComputePipelineCreateInfo& info, std::span<const std::uint32_t> spirv, std::span<const std::uint32_t> layout, VkPipeline* pipeline) const;

private:
    enum class RestoreResult { Missing, Restored, BelowThreshold };
    RestoreResult restore(const VkComputePipelineCreateInfo& info, std::span<const std::byte> key, VkPipeline* pipeline) const;
    RestoreResult restoreFile(const VkComputePipelineCreateInfo& info, std::span<const std::byte> key, VkPipeline* pipeline, const std::filesystem::path& path) const;
    void capture(VkPipeline pipeline, std::span<const std::byte> key) const;
    void rememberThreshold(std::span<const std::byte> key) const;
    std::filesystem::path filePath(std::span<const std::byte> key) const;

    Context context;
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
