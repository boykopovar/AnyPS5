#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_SHADERS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_SHADERS_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "IntermediateRepresentation/IrMetadata/ResourcePlan.hpp"
#include "Recompiler.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

namespace AgcDriver::Graphics {

inline constexpr std::uint32_t PipelinePushConstantBytes = 256;
inline constexpr std::uint32_t PipelinePushSlotBytes = 128;
inline constexpr std::uint32_t ComputePushConstantBytes = 128;

inline std::uint32_t PushBlockBytes(bool libraries) {
    return libraries ? PipelinePushConstantBytes : PipelinePushSlotBytes;
}

inline VkShaderStageFlags PreRasterizationPushStages(bool meshShader, bool tessellation, bool geometry) {
    VkShaderStageFlags stages = VK_SHADER_STAGE_VERTEX_BIT;
    if (tessellation) stages |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    if (geometry) stages |= VK_SHADER_STAGE_GEOMETRY_BIT;
    if (meshShader) stages |= VK_SHADER_STAGE_MESH_BIT_EXT;
    return stages;
}

inline std::uint32_t FixedPushOffset(ShaderRecompiler::ShaderStage stage) {
    return stage == ShaderRecompiler::ShaderStage::Fragment ? PipelinePushSlotBytes : 0u;
}

inline std::uint32_t StagePushOffset(std::uint32_t cursor, ShaderRecompiler::ShaderStage stage, bool fixedSlots) {
    return fixedSlots && stage == ShaderRecompiler::ShaderStage::Fragment ? PipelinePushSlotBytes : cursor;
}

inline std::uint32_t StagePushBudget(std::uint32_t limit, std::uint32_t offset, ShaderRecompiler::ShaderStage stage, bool fixedSlots, bool hasFragment) {
    const bool fragment = stage == ShaderRecompiler::ShaderStage::Fragment;
    const auto base = fixedSlots && fragment ? PipelinePushSlotBytes : 0u;
    if (offset < base) return 0u;
    const auto capacity = fixedSlots && fragment ? PipelinePushSlotBytes : std::min(limit, PipelinePushSlotBytes);
    const auto used = offset - base;
    const auto reserved = !fixedSlots && hasFragment && !fragment ? 4u : 0u;
    return used <= capacity && reserved <= capacity - used ? capacity - used - reserved : 0u;
}

struct CompiledShader {
    ShaderRecompiler::ShaderStage stage;
    const ShaderRecompiler::RecompileResult* program;
    std::uint32_t pushConstantOffset;
    const std::vector<ShaderRecompiler::RegisteredImageView>* registeredImageViews = nullptr;
};

inline std::uint32_t KeyedPushOffset(const CompiledShader& shader) {
    return shader.program->pushConstants.empty() ? 0u : shader.pushConstantOffset;
}

inline VkShaderStageFlagBits VulkanStage(ShaderRecompiler::ShaderStage stage) {
    switch (stage) {
        case ShaderRecompiler::ShaderStage::Vertex:
        case ShaderRecompiler::ShaderStage::Local: return VK_SHADER_STAGE_VERTEX_BIT;
        case ShaderRecompiler::ShaderStage::TessellationControl: return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case ShaderRecompiler::ShaderStage::TessellationEvaluation: return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        case ShaderRecompiler::ShaderStage::Geometry: return VK_SHADER_STAGE_GEOMETRY_BIT;
        case ShaderRecompiler::ShaderStage::Mesh: return VK_SHADER_STAGE_MESH_BIT_EXT;
        case ShaderRecompiler::ShaderStage::Fragment: return VK_SHADER_STAGE_FRAGMENT_BIT;
        case ShaderRecompiler::ShaderStage::Compute: return VK_SHADER_STAGE_COMPUTE_BIT;
        default: throw std::runtime_error("AGC graphics: unsupported compiled shader stage");
    }
}

inline VkPipelineStageFlags PipelineStages(std::span<const CompiledShader> shaders) {
    VkPipelineStageFlags result = 0;
    for (const auto& shader : shaders) {
        switch (VulkanStage(shader.stage)) {
            case VK_SHADER_STAGE_VERTEX_BIT: result |= VK_PIPELINE_STAGE_VERTEX_SHADER_BIT; break;
            case VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT: result |= VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT; break;
            case VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT: result |= VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT; break;
            case VK_SHADER_STAGE_GEOMETRY_BIT: result |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT; break;
            case VK_SHADER_STAGE_MESH_BIT_EXT: result |= VK_PIPELINE_STAGE_MESH_SHADER_BIT_EXT; break;
            case VK_SHADER_STAGE_FRAGMENT_BIT: result |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; break;
            default: throw std::runtime_error("AGC graphics: invalid graphics pipeline stage");
        }
    }
    return result;
}

inline VkShaderStageFlags PushConstantStages(std::span<const CompiledShader> shaders) {
    VkShaderStageFlags result = 0;
    for (const auto& shader : shaders) {
        Require(shader.program != nullptr, "missing compiled shader");
        if (!shader.program->pushConstants.empty() || shader.program->readsDepthRemapPush || shader.stage == ShaderRecompiler::ShaderStage::Mesh || shader.stage == ShaderRecompiler::ShaderStage::TessellationControl) result |= VulkanStage(shader.stage);
    }
    return result;
}

inline std::array<std::byte, PipelinePushConstantBytes> AssemblePushConstants(std::span<const CompiledShader> shaders) {
    static_assert(PipelinePushConstantBytes % 4 == 0 && PipelinePushConstantBytes / 4 <= 64 && PipelinePushSlotBytes / 4 < 64, "the occupied mask holds one bit per DWORD of the block");
    std::array<std::byte, PipelinePushConstantBytes> result{};
    std::uint64_t occupied = 0;
    for (const auto& shader : shaders) {
        Require(shader.program != nullptr, "missing compiled shader");
        const auto& bytes = shader.program->pushConstants;
        if (bytes.empty()) continue;
        Require(bytes.size() % 4 == 0 && shader.pushConstantOffset % 4 == 0, "shader push constant range is not DWORD aligned");
        Require(shader.pushConstantOffset < PipelinePushConstantBytes && bytes.size() <= PipelinePushConstantBytes - shader.pushConstantOffset, "shader push constant range lies outside the pipeline push constant block");
        Require(bytes.size() <= PipelinePushSlotBytes - shader.pushConstantOffset % PipelinePushSlotBytes, "shader push constant range crosses a push constant slot");
        const auto range = ((std::uint64_t{1} << (bytes.size() / 4)) - 1) << (shader.pushConstantOffset / 4);
        Require((occupied & range) == 0, "shader push constant ranges of different stages overlap");
        occupied |= range;
        std::memcpy(result.data() + shader.pushConstantOffset, bytes.data(), bytes.size());
    }
    return result;
}

}

#endif
