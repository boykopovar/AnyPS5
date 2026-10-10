#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_SHADERS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_GRAPHICS_INCLUDE_SHADERS_HPP

#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "Recompiler.hpp"
#include <array>
#include <cstddef>
#include <span>

namespace AgcDriver::Graphics {

inline constexpr std::uint32_t PipelinePushConstantBytes = 128;

struct CompiledShader {
    ShaderRecompiler::ShaderStage stage;
    const ShaderRecompiler::RecompileResult* program;
    std::uint32_t pushConstantOffset;
};

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
        if (!shader.program->pushConstants.empty() || shader.stage == ShaderRecompiler::ShaderStage::Mesh) result |= VulkanStage(shader.stage);
    }
    return result;
}

inline std::array<std::byte, PipelinePushConstantBytes> AssemblePushConstants(std::span<const CompiledShader> shaders) {
    std::array<std::byte, PipelinePushConstantBytes> result{};
    std::array<bool, PipelinePushConstantBytes> occupied{};
    for (const auto& shader : shaders) {
        Require(shader.program != nullptr, "missing compiled shader");
        const auto& bytes = shader.program->pushConstants;
        if (bytes.empty()) continue;
        Require(bytes.size() % 4 == 0 && shader.pushConstantOffset % 4 == 0, "shader push constant range is not DWORD aligned");
        Require(shader.pushConstantOffset < PipelinePushConstantBytes && bytes.size() <= PipelinePushConstantBytes - shader.pushConstantOffset, "shader push constant range lies outside the pipeline push constant block");
        for (std::size_t i = 0; i < bytes.size(); ++i) {
            const auto position = shader.pushConstantOffset + i;
            Require(!occupied[position], "shader push constant ranges of different stages overlap");
            occupied[position] = true;
            result[position] = bytes[i];
        }
    }
    return result;
}

}

#endif
