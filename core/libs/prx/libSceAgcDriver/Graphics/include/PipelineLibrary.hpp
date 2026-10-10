#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PIPELINELIBRARY_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_PIPELINELIBRARY_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include <array>
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

VkPipeline LinkPipelineFromLibraries(const Context& context, const VkGraphicsPipelineCreateInfo& info, const VkRenderPassCreateInfo& pass, const VkPipelineLayoutCreateInfo& layout, const PipelineLibraryKeys& keys);

void ClearPipelineLibraries(VkDevice device);

struct PipelineLibraryCounters {
    std::array<std::uint64_t, 4> built{};
    std::uint64_t linked = 0;
};

PipelineLibraryCounters PipelineLibraryCountersOf(VkDevice device);

}

#endif
