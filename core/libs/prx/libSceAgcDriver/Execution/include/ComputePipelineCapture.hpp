#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_COMPUTEPIPELINECAPTURE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_COMPUTEPIPELINECAPTURE_HPP

#include <vulkan/vulkan.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace AgcDriver {

struct ComputeDescriptorBinding {
    std::uint32_t binding = 0;
    VkDescriptorType type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    std::uint32_t count = 1;
    VkShaderStageFlags stages = VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorBindingFlags flags = 0;
};

struct ComputeDescriptorSetLayout {
    VkDescriptorSetLayoutCreateFlags flags = 0;
    std::vector<ComputeDescriptorBinding> bindings;
};

struct ComputeSpecialization {
    std::uint32_t id = 0;
    std::uint32_t value = 0;
};

struct ComputePipelineCapture {
    static constexpr std::uint32_t Version = 1;
    static constexpr std::uint32_t MaxSpirvWords = 8u * 1024u * 1024u;
    static constexpr std::size_t MaxFileBytes = 36u * 1024u * 1024u;
    std::vector<std::uint32_t> spirv;
    std::vector<ComputeDescriptorSetLayout> sets;
    std::vector<VkPushConstantRange> pushConstants;
    std::vector<ComputeSpecialization> specialization;
    std::uint32_t requiredSubgroupSize = 0;
    VkPipelineCreateFlags flags = 0;
    VkPipelineShaderStageCreateFlags stageFlags = 0;
    std::string deviceName;
    std::uint32_t vendorId = 0;
    std::uint32_t deviceId = 0;
    std::uint32_t driverVersion = 0;
    std::uint32_t apiVersion = 0;
    std::array<std::uint8_t, VK_UUID_SIZE> pipelineCacheUuid{};

    void Validate() const;
    std::vector<std::byte> Encode() const;
    static ComputePipelineCapture Decode(std::span<const std::byte> bytes);
    static ComputePipelineCapture Read(const std::filesystem::path& path);
    std::filesystem::path Write(const std::filesystem::path& directory) const;
    std::filesystem::path WriteOptional(const std::filesystem::path& directory) const;
    static bool OptionalEnabled();
    std::string Identity() const;
};

struct ComputePipelineReplayResult {
    double moduleMilliseconds = 0;
    double layoutMilliseconds = 0;
    double pipelineMilliseconds = 0;
    double cacheMilliseconds = 0;
    double totalMilliseconds = 0;
    std::string deviceName;
    std::uint32_t vendorId = 0;
    std::uint32_t deviceId = 0;
    std::uint32_t driverVersion = 0;
    std::uint32_t apiVersion = 0;
    std::array<std::uint8_t, VK_UUID_SIZE> pipelineCacheUuid{};
    std::size_t initialCacheBytes = 0;
    std::vector<std::byte> cache;
};

}

#endif
