#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_STATE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_STATE_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ColorTargetLayout.hpp"
#include "prx/libSceAgcDriver/Execution/include/QueueState.hpp"
#include <vector>
#include <array>
#include <cstddef>
#include <cstdint>
#include "Recompiler.hpp"

namespace AgcDriver::Graphics {

enum class ShaderPath {
    Vertex,
    Geometry,
    Tessellation,
    TessellationGeometry
};

struct ShaderStages {
    ShaderPath path;
    std::uint32_t registerValue;
    std::uint32_t vertexWaveSize;
    std::uint32_t fragmentWaveSize;
    std::optional<ShaderRecompiler::MeshConfiguration> mesh;
    std::optional<ShaderRecompiler::TessellationConfiguration> tessellation;
};

struct ColorTarget {
    std::uint64_t address;
    VkExtent2D extent;
    VkFormat format;
    std::size_t bytes;
    std::uint8_t componentMapping;
    ColorTileMode tileMode = ColorTileMode::Linear;
    std::uint32_t elementBytes = 4;
    // DCC metadata of a compressed target (CB_COLOR_INFO DCC_ENABLE), or 0 (see DccMetadata.hpp).
    std::uint64_t dccAddress = 0;
    bool dccAlphaOnMsb = false;
};

struct State {
    ShaderStages stages;
    // MRT slot 0; `colors`/`blends` hold every written slot, attachment i being slot i.
    ColorTarget color;
    std::vector<ColorTarget> colors;
    std::vector<VkPipelineColorBlendAttachmentState> blends;
    bool hasColorTarget;
    bool rectList = false;
    VkExtent2D renderExtent;
    VkPrimitiveTopology topology;
    VkViewport viewport;
    bool negativeOneToOne;
    bool depthClamp = false;
    VkRect2D scissor;
    VkCullModeFlags cullMode;
    VkFrontFace frontFace;
    VkPipelineColorBlendAttachmentState blend;
    std::array<float, 4> blendConstants;
};

ShaderStages DecodeShaderStages(const QueueState& queue);
State DecodeState(const QueueState& queue);

}

#endif
