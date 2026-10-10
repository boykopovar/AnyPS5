#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PIPELINELIBRARY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PIPELINELIBRARY_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <array>
#include <atomic>
#include <memory>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace AgcDriver::Graphics {

struct PipelineLibraryKeys {
    std::vector<std::byte> vertexInput;
    std::vector<std::byte> preRasterization;
    std::vector<std::byte> fragmentShader;
    std::vector<std::byte> fragmentOutput;
    std::vector<std::byte> renderPass;
    std::vector<std::byte> layout;
};

std::span<const VkDynamicState> PipelineLibraryDynamicStates();

struct OptimizedPipeline {
    VkDevice device = VK_NULL_HANDLE;
    PFN_vkDestroyPipeline destroy = nullptr;
    std::atomic<VkPipeline> handle{VK_NULL_HANDLE};
    std::atomic<bool> released{false};
    void Release() noexcept;
};

VkPipeline LinkPipelineFromLibraries(const Context& context, const VkGraphicsPipelineCreateInfo& info, const VkPipelineRenderingCreateInfoKHR& rendering, const VkPipelineLayoutCreateInfo& layout, const PipelineLibraryKeys& keys, std::shared_ptr<OptimizedPipeline>* optimized = nullptr);

void WaitForOptimizedPipelines(VkDevice device);

void ClearPipelineLibraries(VkDevice device);

struct PipelineLibraryCounters {
    std::array<std::uint64_t, 4> built{};
    std::uint64_t linked = 0;
    std::uint64_t optimized = 0;
};

PipelineLibraryCounters PipelineLibraryCountersOf(VkDevice device);

}

#endif
