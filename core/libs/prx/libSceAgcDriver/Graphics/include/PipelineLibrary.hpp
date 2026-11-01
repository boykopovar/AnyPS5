#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PIPELINELIBRARY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PIPELINELIBRARY_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <array>
#include <atomic>
#include <functional>
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
    std::vector<std::byte> preRasterizationLayout;
    std::vector<std::byte> fragmentLayout;
    std::uint32_t preRasterizationSets = 0;
    std::uint32_t fragmentSets = 0;
};

std::span<const VkDynamicState> PipelineLibraryDynamicStates();

struct OptimizedPipeline {
    VkDevice device = VK_NULL_HANDLE;
    PFN_vkDestroyPipeline destroy = nullptr;
    std::atomic<VkPipeline> handle{VK_NULL_HANDLE};
    std::atomic<bool> released{false};
    // The relink of a pipeline the journal replay built waits for the first draw that binds it
    // (Schedule), so the relink thread serves the title's own pipelines first and a replayed
    // pipeline the title never draws is never relinked.
    std::atomic<bool> deferred{false};
    std::function<void()> relink;
    void Release() noexcept;
    void Schedule();
};

VkPipeline LinkPipelineFromLibraries(const Context& context, const VkGraphicsPipelineCreateInfo& info, const VkPipelineRenderingCreateInfoKHR& rendering, const VkPipelineLayoutCreateInfo& layout, const PipelineLibraryKeys& keys, std::shared_ptr<OptimizedPipeline>* optimized = nullptr, bool partsOnly = false, std::span<VkShaderModule> modules = {});

void WaitForOptimizedPipelines(VkDevice device);

void ClearPipelineLibraries(VkDevice device);

void StopPipelineLibraryWorker();
unsigned PipelineLibraryWorkers();

struct PipelineLibraryCounters {
    std::array<std::uint64_t, 4> built{};
    std::uint64_t linked = 0;
    std::uint64_t optimized = 0;
};

PipelineLibraryCounters PipelineLibraryCountersOf(VkDevice device);

}

#endif
