#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceOverlay.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Pipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineLibrary.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineJournal.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DepthSurface.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PatchVertexLayer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineSpecialization.hpp"
#include "prx/libSceAgcDriver/Graphics/include/SampleOrder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include "prx/libSceAgcDriver/Graphics/include/VertexInput.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <list>
#include <mutex>
#include <spirv/unified1/spirv.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace AgcDriver::Graphics {

namespace {

bool LogicOpFormat(VkFormat format) {
    switch (format) {
        case VK_FORMAT_R8_UNORM:
        case VK_FORMAT_R8G8_UNORM:
        case VK_FORMAT_R8G8B8A8_UNORM:
        case VK_FORMAT_B8G8R8A8_UNORM:
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
        case VK_FORMAT_A2R10G10B10_UNORM_PACK32:
        case VK_FORMAT_R16_UNORM:
        case VK_FORMAT_R16G16_UNORM:
        case VK_FORMAT_R16G16B16A16_UNORM:
        case VK_FORMAT_R8_UINT:
        case VK_FORMAT_R8G8_UINT:
        case VK_FORMAT_R8G8B8A8_UINT:
        case VK_FORMAT_R16_UINT:
        case VK_FORMAT_R16G16_UINT:
        case VK_FORMAT_R16G16B16A16_UINT:
        case VK_FORMAT_R32_UINT:
        case VK_FORMAT_R32G32_UINT:
        case VK_FORMAT_R32G32B32A32_UINT: return true;
        default: return false;
    }
}

}

void LogPipelineStatistics_nid_no_patch(const Context& context, VkPipeline pipeline, std::span<const CompiledShader> shaders) {
    if (!context.pipelineExecutableInfo) return;
    const auto getProperties = context.Function<PFN_vkGetPipelineExecutablePropertiesKHR>("vkGetPipelineExecutablePropertiesKHR");
    const auto getStatistics = context.Function<PFN_vkGetPipelineExecutableStatisticsKHR>("vkGetPipelineExecutableStatisticsKHR");
    VkPipelineInfoKHR pipelineInfo{VK_STRUCTURE_TYPE_PIPELINE_INFO_KHR};
    pipelineInfo.pipeline = pipeline;
    std::uint32_t count = 0;
    Check(getProperties(context.device, &pipelineInfo, &count, nullptr), "vkGetPipelineExecutablePropertiesKHR count");
    std::vector<VkPipelineExecutablePropertiesKHR> properties(count, {VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_PROPERTIES_KHR});
    Check(getProperties(context.device, &pipelineInfo, &count, properties.data()), "vkGetPipelineExecutablePropertiesKHR");
    properties.resize(count);
    std::string report;
    const auto append = [&report](const char* format, auto... values) {
        std::array<char, 1024> line{};
        std::snprintf(line.data(), line.size(), format, values...);
        report += line.data();
    };
    for (const auto& shader : shaders) {
        if (shader.program == nullptr) continue;
        const auto identity = shader.program->Identity();
        append("[pipeline-source] stage=0x%x variant=0x%llx pipelineVariant=0x%llx identity=%016llx%016llx\n", VulkanStage(shader.stage), static_cast<unsigned long long>(shader.program->variantId), static_cast<unsigned long long>(shader.program->PipelineVariantId()), static_cast<unsigned long long>(identity.high), static_cast<unsigned long long>(identity.low));
    }
    for (std::uint32_t index = 0; index < properties.size(); ++index) {
        VkPipelineExecutableInfoKHR executable{VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_INFO_KHR};
        executable.pipeline = pipeline;
        executable.executableIndex = index;
        count = 0;
        Check(getStatistics(context.device, &executable, &count, nullptr), "vkGetPipelineExecutableStatisticsKHR count");
        std::vector<VkPipelineExecutableStatisticKHR> statistics(count, {VK_STRUCTURE_TYPE_PIPELINE_EXECUTABLE_STATISTIC_KHR});
        Check(getStatistics(context.device, &executable, &count, statistics.data()), "vkGetPipelineExecutableStatisticsKHR");
        statistics.resize(count);
        const auto& property = properties[index];
        append("[pipeline-stats] executable=%u stages=0x%x subgroup=%u name=%s\n", index, property.stages, property.subgroupSize, property.name);
        for (const auto& statistic : statistics) {
            switch (statistic.format) {
            case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_BOOL32_KHR:
                append("[pipeline-stats] executable=%u %s=%s (%s)\n", index, statistic.name, statistic.value.b32 ? "true" : "false", statistic.description);
                break;
            case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_INT64_KHR:
                append("[pipeline-stats] executable=%u %s=%lld (%s)\n", index, statistic.name, static_cast<long long>(statistic.value.i64), statistic.description);
                break;
            case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_UINT64_KHR:
                append("[pipeline-stats] executable=%u %s=%llu (%s)\n", index, statistic.name, static_cast<unsigned long long>(statistic.value.u64), statistic.description);
                break;
            case VK_PIPELINE_EXECUTABLE_STATISTIC_FORMAT_FLOAT64_KHR:
                append("[pipeline-stats] executable=%u %s=%.17g (%s)\n", index, statistic.name, statistic.value.f64, statistic.description);
                break;
            default:
                throw std::runtime_error("unsupported pipeline statistic format");
            }
        }
    }
    std::fputs(report.c_str(), stderr);
}

bool HasStencil(VkFormat format) {
    return format == VK_FORMAT_D16_UNORM_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT || format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_S8_UINT;
}

Framebuffer::Framebuffer(const Context& context, VkRenderPass renderPass, std::span<const VkImageView> targets, VkExtent2D extent, std::uint32_t layers) : context(context) {
    // Cached objects outlive their device's teardown; they must not keep its buffer pool alive past it.
    this->context.bufferPool.reset();
    Require(extent.width != 0 && extent.height != 0 && extent.width <= context.limits.maxFramebufferWidth && extent.height <= context.limits.maxFramebufferHeight, "framebuffer extent exceeds device limits");
    Require(layers != 0 && layers <= context.limits.maxFramebufferLayers, "framebuffer layers exceed device limits");
    views.assign(targets.begin(), targets.end());
    if (renderPass == VK_NULL_HANDLE) return;
    VkFramebufferCreateInfo framebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    framebufferInfo.renderPass = renderPass;
    framebufferInfo.attachmentCount = static_cast<std::uint32_t>(targets.size());
    framebufferInfo.pAttachments = targets.empty() ? nullptr : targets.data();
    framebufferInfo.width = extent.width;
    framebufferInfo.height = extent.height;
    framebufferInfo.layers = layers;
    Check(context.Function<PFN_vkCreateFramebuffer>("vkCreateFramebuffer")(context.device, &framebufferInfo, nullptr, &framebuffer), "vkCreateFramebuffer");
}

Framebuffer::~Framebuffer() {
    if (framebuffer) context.Function<PFN_vkDestroyFramebuffer>("vkDestroyFramebuffer")(context.device, framebuffer, nullptr);
}

void ValidateDepthBounds(const Context& context, const State& state) {
    Require(!state.depthBoundsTest || context.depthRangeUnrestricted || (state.minDepthBounds >= 0.0f && state.minDepthBounds <= 1.0f && state.maxDepthBounds >= 0.0f && state.maxDepthBounds <= 1.0f), "depth bounds outside [0, 1] require VK_EXT_depth_range_unrestricted");
}

bool BarycentricPatches(const State& state, std::span<const CompiledShader> shaders) {
    return !state.rectList && state.stages.path == ShaderPath::Vertex && std::any_of(shaders.begin(), shaders.end(), [](const CompiledShader& shader) { return shader.stage == ShaderRecompiler::ShaderStage::TessellationControl; });
}

bool FragmentInterlock(std::span<const CompiledShader> shaders) {
    return std::any_of(shaders.begin(), shaders.end(), [](const CompiledShader& shader) {
        if (shader.stage != ShaderRecompiler::ShaderStage::Fragment || shader.program == nullptr) return false;
        const auto words = shader.program->spirv.Words();
        for (std::size_t cursor = 5; cursor + 1 < words.size() && (words[cursor] & 0xffffu) == spv::OpCapability && (words[cursor] >> 16u) == 2u; cursor += 2) {
            const auto capability = words[cursor + 1];
            if (capability == spv::CapabilityFragmentShaderPixelInterlockEXT || capability == spv::CapabilityFragmentShaderSampleInterlockEXT || capability == spv::CapabilityFragmentShaderShadingRateInterlockEXT) return true;
        }
        return false;
    });
}

namespace {

struct ProvokingDecorations {
    std::unordered_map<std::uint64_t, std::uint32_t> builtIns;
    std::vector<std::uint64_t> perVertexValues;
    std::vector<std::uint64_t> perPrimitiveValues;
};

bool ReadProvokingDecorations(const ShaderRecompiler::RecompileResult& program, ProvokingDecorations& decorations) {
    const auto words = program.spirv.Words();
    if (words.size() < 5) return false;
    constexpr std::uint32_t NoMember = 0xffffffffu;
    const auto key = [](std::uint32_t target, std::uint32_t member) { return (static_cast<std::uint64_t>(target) << 32u) | member; };
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> 16u;
        if (count == 0 || count > words.size() - cursor) return false;
        const auto op = static_cast<spv::Op>(words[cursor] & 0xffffu);
        const std::size_t decoration = op == spv::OpDecorate ? 2 : op == spv::OpMemberDecorate ? 3 : 0;
        if (decoration != 0 && count > decoration) {
            const auto target = key(words[cursor + 1], op == spv::OpMemberDecorate ? words[cursor + 2] : NoMember);
            if (words[cursor + decoration] == spv::DecorationFlat || words[cursor + decoration] == spv::DecorationPerVertexKHR) decorations.perVertexValues.push_back(target);
            if (words[cursor + decoration] == spv::DecorationPerPrimitiveEXT) decorations.perPrimitiveValues.push_back(target);
            if (words[cursor + decoration] == spv::DecorationBuiltIn && count > decoration + 1) decorations.builtIns[target] = words[cursor + decoration + 1];
        }
        cursor += count;
    }
    return true;
}

bool ReadsProvokingVertex(const ShaderRecompiler::RecompileResult& program) {
    if (std::any_of(program.fragmentParameters.begin(), program.fragmentParameters.end(), [](const ShaderRecompiler::FragmentParameter& parameter) {
        return parameter.flat || parameter.perVertex || parameter.custom || parameter.flatP10;
    })) return true;
    ProvokingDecorations decorations;
    if (!ReadProvokingDecorations(program, decorations)) return true;
    const auto& builtIns = decorations.builtIns;
    return std::any_of(decorations.perVertexValues.begin(), decorations.perVertexValues.end(), [&](std::uint64_t target) {
        const auto builtIn = builtIns.find(target);
        if (builtIn == builtIns.end()) return true;
        switch (builtIn->second) {
        case spv::BuiltInFragCoord:
        case spv::BuiltInPointCoord:
        case spv::BuiltInFrontFacing:
        case spv::BuiltInSampleId:
        case spv::BuiltInSamplePosition:
        case spv::BuiltInSampleMask:
        case spv::BuiltInHelperInvocation:
        case spv::BuiltInSubgroupSize:
        case spv::BuiltInSubgroupLocalInvocationId:
            return false;
        default:
            return true;
        }
    });
}

bool WritesProvokingVertexBuiltIn(const ShaderRecompiler::RecompileResult& program) {
    ProvokingDecorations decorations;
    if (!ReadProvokingDecorations(program, decorations)) return true;
    return std::any_of(decorations.builtIns.begin(), decorations.builtIns.end(), [&](const auto& builtIn) {
        return (builtIn.second == spv::BuiltInLayer || builtIn.second == spv::BuiltInViewportIndex) &&
               std::find(decorations.perPrimitiveValues.begin(), decorations.perPrimitiveValues.end(), builtIn.first) == decorations.perPrimitiveValues.end();
    });
}

}

bool LastVertexPatches(const State& state, std::span<const CompiledShader> shaders, bool provokingVertexLast, bool tessellation, bool primitiveListRestart, bool indexed) {
    if (provokingVertexLast || !tessellation || state.provokingVertexMode != VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT) return false;
    if (state.rectList || state.stages.path != ShaderPath::Vertex || state.stages.mesh || state.stages.tessellation) return false;
    if (shaders.size() != 2u || shaders[0].stage != ShaderRecompiler::ShaderStage::Vertex || shaders[1].stage != ShaderRecompiler::ShaderStage::Fragment || shaders[0].program == nullptr || shaders[1].program == nullptr) return false;
    if (!ReadsProvokingVertex(*shaders[1].program) || WritesProvokingVertexBuiltIn(*shaders[0].program)) return false;
    return (state.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST && (!state.primitiveRestart || !primitiveListRestart)) || (state.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP && (!state.primitiveRestart || !indexed));
}

void ValidateProvokingVertex(const Context& context, const State& state, std::span<const CompiledShader> shaders) {
    Require(state.provokingVertexMode == VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT || state.provokingVertexMode == VK_PROVOKING_VERTEX_MODE_LAST_VERTEX_EXT, "invalid provoking vertex mode");
    if (state.provokingVertexMode == VK_PROVOKING_VERTEX_MODE_FIRST_VERTEX_EXT) {
        for (const auto& shader : shaders) {
            if (shader.stage != ShaderRecompiler::ShaderStage::Fragment || shader.program == nullptr) continue;
            Require(std::none_of(shader.program->fragmentParameters.begin(), shader.program->fragmentParameters.end(), [](const ShaderRecompiler::FragmentParameter& parameter) { return parameter.flatP10; }), "v_interp_mov p10 of a flat input with the first provoking vertex is not measured");
        }
        return;
    }
    for (const auto& shader : shaders) {
        if (shader.stage != ShaderRecompiler::ShaderStage::Fragment || shader.program == nullptr) continue;
        Require(std::none_of(shader.program->fragmentParameters.begin(), shader.program->fragmentParameters.end(), [](const ShaderRecompiler::FragmentParameter& parameter) { return parameter.flatP20; }), "v_interp_mov p20 of a flat input with the last provoking vertex is not measured");
    }
    if (BarycentricPatches(state, shaders)) {
        for (const auto& shader : shaders) {
            if (shader.stage != ShaderRecompiler::ShaderStage::Fragment || shader.program == nullptr) continue;
            Require(std::none_of(shader.program->fragmentParameters.begin(), shader.program->fragmentParameters.end(), [](const ShaderRecompiler::FragmentParameter& parameter) { return parameter.flat && parameter.perVertex; }), "last provoking vertex with explicit per-vertex flat interpolation is unsupported");
        }
        return;
    }
    const bool observed = std::any_of(shaders.begin(), shaders.end(), [&](const CompiledShader& shader) {
        if (shader.stage == ShaderRecompiler::ShaderStage::Fragment) return shader.program == nullptr || ReadsProvokingVertex(*shader.program);
        return shader.program != nullptr && WritesProvokingVertexBuiltIn(*shader.program);
    });
    if (!context.provokingVertexLast && !observed) return;
    Require(context.provokingVertexLast, "last provoking vertex requires VK_EXT_provoking_vertex with provokingVertexLast enabled");
    for (const auto& shader : shaders) {
        if (shader.stage != ShaderRecompiler::ShaderStage::Fragment || shader.program == nullptr) continue;
        Require(std::none_of(shader.program->fragmentParameters.begin(), shader.program->fragmentParameters.end(), [](const ShaderRecompiler::FragmentParameter& parameter) { return parameter.flat && parameter.custom; }), "last provoking vertex is unsupported for a pass-through input read only at P0, which takes the first vertex's value as a flat input");
    }
    const bool meshPrimitives = state.stages.path == ShaderPath::Geometry && state.stages.mesh;
    Require(!state.stages.tessellation && (meshPrimitives || (state.stages.path == ShaderPath::Vertex && !state.stages.mesh)), "last provoking vertex is unsupported for generated primitive pipelines");
    Require(state.rectList || state.topology == VK_PRIMITIVE_TOPOLOGY_POINT_LIST || state.topology == VK_PRIMITIVE_TOPOLOGY_LINE_LIST || state.topology == VK_PRIMITIVE_TOPOLOGY_LINE_STRIP || state.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST || state.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP || state.topology == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN, "last provoking vertex is unsupported for this primitive topology");
    for (const auto& shader : shaders) {
        if (shader.stage != ShaderRecompiler::ShaderStage::Fragment) continue;
        Require(shader.program != nullptr, "missing compiled fragment shader");
        Require(std::none_of(shader.program->fragmentParameters.begin(), shader.program->fragmentParameters.end(), [](const ShaderRecompiler::FragmentParameter& parameter) { return parameter.flat && parameter.perVertex; }), "last provoking vertex with explicit per-vertex flat interpolation is unsupported");
        Require(state.topology != VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP || std::none_of(shader.program->fragmentParameters.begin(), shader.program->fragmentParameters.end(), [](const ShaderRecompiler::FragmentParameter& parameter) { return parameter.vertexOrder; }), "last provoking vertex with explicit per-vertex triangle-strip interpolation is unsupported");
    }
}

bool ValidateSampleLocations(const Context& context, const State& state) {
    const bool extensionSupportsSamples = (context.sampleLocationSampleCounts & state.samples) != 0;
    Require(!state.customSampleLocations || extensionSupportsSamples, "custom sample locations require VK_EXT_sample_locations support for the draw's sample count");
    const bool enabled = extensionSupportsSamples && (state.samples > 1 || state.customSampleLocations) && (!state.depth || SampleLocationsCompatibleDepth(context, HostDepthFormat(context, state.depth->format), state.depth->samples));
    Require(!state.customSampleLocations || enabled, "custom sample locations require a compatible depth format and sample count");
    return enabled;
}

void ValidateMeshLimits(const Context& context, const ShaderRecompiler::MeshConfiguration& mesh, std::uint32_t invocations) {
    const auto& limits = context.meshLimits;
    Require(invocations <= limits.maxMeshWorkGroupInvocations && invocations <= limits.maxMeshWorkGroupSize[0], "mesh workgroup of " + std::to_string(invocations) + " invocations exceeds device limits (" + std::to_string(limits.maxMeshWorkGroupInvocations) + " invocations, " + std::to_string(limits.maxMeshWorkGroupSize[0]) + " wide)");
    Require(mesh.maxVertices <= limits.maxMeshOutputVertices, "mesh output vertices " + std::to_string(mesh.maxVertices) + " exceed the device limit " + std::to_string(limits.maxMeshOutputVertices));
    Require(mesh.maxPrimitives <= limits.maxMeshOutputPrimitives, "mesh output primitives " + std::to_string(mesh.maxPrimitives) + " exceed the device limit " + std::to_string(limits.maxMeshOutputPrimitives));
    const auto sharedBytes = static_cast<std::uint64_t>(mesh.ldsSizeDwords) * 4;
    Require(sharedBytes <= limits.maxMeshSharedMemorySize, "mesh LDS " + std::to_string(sharedBytes) + " bytes exceeds the device shared memory limit " + std::to_string(limits.maxMeshSharedMemorySize));
}

void ValidateLineWidth(const Context& context, float width) {
    if (width == 1.0f) return;
    Require(context.wideLines, "a line polygon mode wider or narrower than one pixel needs the wideLines feature");
    const auto low = context.limits.lineWidthRange[0];
    const auto high = context.limits.lineWidthRange[1];
    const auto granularity = context.limits.lineWidthGranularity;
    const auto describe = [&] { return "line width " + std::to_string(width) + " against lineWidthRange [" + std::to_string(low) + ", " + std::to_string(high) + "] and lineWidthGranularity " + std::to_string(granularity); };
    Require(width >= low && width <= high, "a line polygon mode needs a width the device supports: " + describe() + " is outside the range");
    const auto steps = granularity > 0.0f ? (width - low) / granularity : 0.0f;
    Require(granularity > 0.0f ? std::abs(steps - std::round(steps)) <= 1.0e-3f : width == low, "a line polygon mode needs a width the device supports: " + describe() + " is not a whole number of granularity steps from the range's low end");
}

void ValidateViewport(const Context& context, const VkViewport& viewport) {
    Require(std::isfinite(viewport.minDepth) && std::isfinite(viewport.maxDepth), "non-finite viewport depth range");
    Require(context.depthRangeUnrestricted || (viewport.minDepth >= 0 && viewport.minDepth <= 1 && viewport.maxDepth >= 0 && viewport.maxDepth <= 1), "viewport depth outside [0, 1] requires VK_EXT_depth_range_unrestricted");
    Require(std::isfinite(viewport.x) && std::isfinite(viewport.y) && std::isfinite(viewport.width) && std::isfinite(viewport.height), "viewport arithmetic overflow");
    Require(viewport.width <= context.limits.maxViewportDimensions[0] && std::abs(viewport.height) <= context.limits.maxViewportDimensions[1], "viewport dimensions exceed device limits");
    Require(viewport.x >= context.limits.viewportBoundsRange[0] && viewport.x + viewport.width <= context.limits.viewportBoundsRange[1], "viewport X exceeds device bounds");
    Require(std::min(viewport.y, viewport.y + viewport.height) >= context.limits.viewportBoundsRange[0] && std::max(viewport.y, viewport.y + viewport.height) <= context.limits.viewportBoundsRange[1], "viewport Y exceeds device bounds");
}

bool ShadersWriteMemory(std::span<const CompiledShader> shaders) {
    const auto any = [](const ShaderRecompiler::ElementFlags& written, std::uint32_t count) { return written.size() < count || std::ranges::any_of(written, [](bool element) { return element; }); };
    for (const auto& shader : shaders) {
        if (shader.program == nullptr) continue;
        for (const auto& binding : shader.program->bindings) {
            switch (binding.role) {
            case ShaderRecompiler::DescriptorRole::GuestBuffers:
                if (binding.kind != ShaderRecompiler::DescriptorKind::UniformBuffer && binding.kind != ShaderRecompiler::DescriptorKind::UniformTexelBuffer && any(binding.bufferWritten, binding.count)) return true;
                break;
            case ShaderRecompiler::DescriptorRole::GuestImages:
                if (binding.kind == ShaderRecompiler::DescriptorKind::StorageImage && any(binding.imageWritten, binding.count)) return true;
                break;
            case ShaderRecompiler::DescriptorRole::Gds:
                return true;
            case ShaderRecompiler::DescriptorRole::BdaPagetable:
                if (shader.program->bdaWrites) return true;
                break;
            case ShaderRecompiler::DescriptorRole::GuestSamplers:
            case ShaderRecompiler::DescriptorRole::FaultBuffer:
            case ShaderRecompiler::DescriptorRole::FlattenedSrt:
            case ShaderRecompiler::DescriptorRole::ShaderData:
                break;
            }
        }
    }
    return false;
}

void ValidateSampleMask(const Context& context, const State& state, std::span<const CompiledShader> shaders) {
    if (!PartialSampleMask(state)) return;
    const bool uniform = UniformSampleMask(state) && !SampleMaskFollowsOrder(state, HostSampleLocationsFor(context, state).order);
    for (const auto& shader : shaders) {
        if (shader.stage != ShaderRecompiler::ShaderStage::Fragment) continue;
        Require(shader.program != nullptr, "missing compiled fragment shader");
        if (uniform) {
            Require(context.earlyFragmentSampleMaskTestBeforeSampleCounting || !DeclaresEarlyFragmentTests(*shader.program), "a sample mask with early fragment tests needs VkPhysicalDeviceMaintenance5Properties::earlyFragmentSampleMaskTestBeforeSampleCounting, which the device lacks");
            continue;
        }
        const bool tested = state.depth.has_value() && (state.depthTest || state.stencilTest || state.depthBoundsTest);
        Require(!state.pixelEarlyZ || !tested || !ShadersWriteMemory(std::span(&shader, 1)), "a sample mask that differs between the pixels of a quad, from a pixel program with early depth or stencil tests that writes memory, is unsupported: the masked pixels are removed in the shader, after the tests");
    }
}

namespace {

thread_local bool prebuilding = false;
thread_local bool prebuilt = false;

VkShaderStageFlags pushStagesOf(const Context& context) {
    return PreRasterizationPushStages(context.meshShader, context.tessellationShader, context.geometryShader);
}

constexpr std::size_t MaxColorWriteMasks = 8;

using ColorWriteMasks = std::array<VkColorComponentFlags, MaxColorWriteMasks>;

ColorWriteMasks channelWriteMasks(const State& state, std::span<const VkPipelineColorBlendAttachmentState> blends, bool secondChannel) {
    Require(blends.size() == state.blends.size() && blends.size() <= MaxColorWriteMasks, "color write masks disagree with the color attachments");
    ColorWriteMasks masks{};
    const bool alternateAlpha = SplitsAlternateAlpha(state);
    for (std::size_t index = 0; index < blends.size(); ++index) {
        auto mask = blends[index].colorWriteMask;
        if (alternateAlpha) {
            const bool split = index < state.colors.size() && state.colors[index].componentMapping == 0xecu && state.blends[index].blendEnable;
            const VkColorComponentFlags green = VK_COLOR_COMPONENT_G_BIT;
            if (!split) mask = secondChannel ? 0u : mask;
            else mask &= secondChannel ? green : ~green;
        }
        masks[index] = mask;
    }
    for (const auto& merged : state.colors) {
        if (merged.partialWriteMask != UnclampedBlendMask) continue;
        Require(merged.exportIndex < state.blends.size(), "color export without a blend attachment");
        masks[merged.exportIndex] = 0;
    }
    return masks;
}

VkPipelineColorBlendAttachmentState keyedBlend(bool dynamicColorWriteMask, VkPipelineColorBlendAttachmentState blend) {
    if (dynamicColorWriteMask) blend.colorWriteMask = 0;
    return blend;
}

template<typename TValue>
void appendKey(std::vector<std::byte>& key, const TValue& value) {
    static_assert(std::is_trivially_copyable_v<TValue>);
    const auto bytes = std::as_bytes(std::span(&value, 1));
    key.insert(key.end(), bytes.begin(), bytes.end());
}

bool narrowLibraryKeysNow() {
    return !RuntimeSwitches::Set(RuntimeSwitch::WideLibraryKeys);
}

bool shadesPerSample(const ShaderRecompiler::RecompileResult& program) {
    const auto words = program.spirv.Words();
    if (words.size() < 5) return true;
    for (std::size_t cursor = 5; cursor < words.size();) {
        const auto count = words[cursor] >> 16u;
        if (count == 0 || count > words.size() - cursor) return true;
        if (static_cast<spv::Op>(words[cursor] & 0xffffu) == spv::OpDecorate && count >= 3) {
            const auto decoration = words[cursor + 2];
            if (decoration == spv::DecorationSample) return true;
            if (decoration == spv::DecorationBuiltIn && count >= 4 && (words[cursor + 3] == spv::BuiltInSampleId || words[cursor + 3] == spv::BuiltInSamplePosition)) return true;
        }
        cursor += count;
    }
    return false;
}

bool libraryKeys(PipelineLibraryKeys& keys, const Context& context, const State& state, const VertexInputLayout& input, const PipelineSetLayouts& resources, std::span<const CompiledShader> shaders, bool narrow) {
    using Stage = ShaderRecompiler::ShaderStage;
    if (!context.graphicsPipelineLibrary || context.pipelineExecutableInfo || (state.stages.tessellation && !context.tessellationPipelineCache) || state.depthBiasPerFace || state.rectList || BarycentricPatches(state, shaders) || SplitsAlternateAlpha(state) || std::ranges::any_of(shaders, [](const CompiledShader& shader) { return shader.program != nullptr && shader.program->functionLdsBuffer; }) || UsesMeshWindow(shaders) || FragmentInterlock(shaders)) return false;
    keys = {};
    for (const auto& shader : shaders) {
        const auto identity = shader.program->Identity();
        if (shader.stage == Stage::Geometry || identity.Empty()) return false;
        auto& key = shader.stage == Stage::Fragment ? keys.fragmentShader : keys.preRasterization;
        appendKey(key, shader.stage);
        appendKey(key, identity.low);
        appendKey(key, identity.high);
        appendKey(key, KeyedPushOffset(shader));
        appendKey(key, state.meshLayerBase);
        appendKey(key, state.meshLayerCount);
        if (resources.split) {
            auto& sets = shader.stage == Stage::Fragment ? keys.fragmentSets : keys.preRasterizationSets;
            for (const auto& binding : shader.program->bindings) {
                Require(binding.descriptorSet < 2u, "split pipeline library descriptor set exceeds two slots");
                sets |= 1u << binding.descriptorSet;
            }
        }
    }
    if (resources.split) {
        const auto unused = 3u & ~(keys.preRasterizationSets | keys.fragmentSets);
        keys.preRasterizationSets |= unused & 1u;
        keys.fragmentSets |= unused & 2u;
    }
    for (const auto& binding : input.bindings) {
        appendKey(keys.vertexInput, binding.binding);
        appendKey(keys.vertexInput, binding.stride);
        appendKey(keys.vertexInput, binding.inputRate);
    }
    appendKey(keys.vertexInput, input.attributes.size());
    for (const auto& attribute : input.attributes) {
        appendKey(keys.vertexInput, attribute.location);
        appendKey(keys.vertexInput, attribute.binding);
        appendKey(keys.vertexInput, attribute.format);
        appendKey(keys.vertexInput, attribute.offset);
    }
    appendKey(keys.vertexInput, state.topology);
    appendKey(keys.vertexInput, state.primitiveRestart);
    appendKey(keys.preRasterization, state.provokingVertexMode);
    appendKey(keys.preRasterization, ViewportCount(state));
    appendKey(keys.preRasterization, state.polygonMode);
    appendKey(keys.preRasterization, state.lineWidth);
    appendKey(keys.preRasterization, state.negativeOneToOne);
    appendKey(keys.preRasterization, state.depthClamp && context.depthClamp);
    appendKey(keys.preRasterization, state.conservativeRasterization);
    if (!narrow || !context.dynamicDepthBiasEnable) appendKey(keys.preRasterization, state.depth.has_value() && state.depthBias);
    appendKey(keys.preRasterization, state.stages.tessellation.has_value());
    if (state.stages.tessellation) appendKey(keys.preRasterization, state.stages.tessellation->inputControlPoints);
    keys.narrow = narrow;
    keys.fragmentPerSample = state.perSampleShading || std::ranges::any_of(shaders, [](const CompiledShader& shader) { return shader.stage == Stage::Fragment && shadesPerSample(*shader.program); });
    appendKey(keys.preRasterization, narrow);
    appendKey(keys.fragmentShader, narrow);
    const bool fragmentSamples = !narrow || keys.fragmentPerSample;
    appendKey(keys.fragmentShader, fragmentSamples);
    const auto sampleKey = [&](const auto& value) {
        if (fragmentSamples) appendKey(keys.fragmentShader, value);
        appendKey(keys.fragmentOutput, value);
    };
    sampleKey(state.samples);
    sampleKey(state.perSampleShading);
    sampleKey(state.mergedCoverage);
    sampleKey(state.truncatedCoverage);
    const auto sampleLocationsEnabled = ValidateSampleLocations(context, state);
    sampleKey(sampleLocationsEnabled);
    sampleKey(state.customSampleLocations);
    if (state.customSampleLocations) {
        sampleKey(state.sampleLocationGrid);
        const auto grid = SampleLocationGrid(state);
        for (std::uint32_t pixel = 0; pixel < grid.width * grid.height; ++pixel) {
            for (std::uint32_t sample = 0; sample < state.samples; ++sample) sampleKey(state.sampleLocations[pixel * 8u + sample]);
        }
    }
    for (const auto& blend : state.blends) appendKey(keys.fragmentOutput, keyedBlend(context.dynamicColorWriteMask, blend));
    appendKey(keys.fragmentOutput, state.colorRop);
    for (const auto value : state.blendConstants) appendKey(keys.fragmentOutput, value);
    appendKey(keys.renderPass, state.blends.size());
    for (const auto& color : state.colors) {
        appendKey(keys.renderPass, color.format);
        appendKey(keys.renderPass, color.exportIndex);
        appendKey(keys.renderPass, color.partialWriteMask != 0u);
    }
    appendKey(keys.renderPass, state.depth.has_value());
    if (state.depth) appendKey(keys.renderPass, state.depth->format);
    sampleKey(HostSampleMask(state));
    for (auto* key : {&keys.fragmentShader, &keys.fragmentOutput}) {
        appendKey(*key, context.sampleLocationSampleCounts);
        appendKey(*key, context.sampleLocationGridSampleCounts);
        appendKey(*key, context.storageImageMultisample);
        appendKey(*key, context.shaderStencilExport);
    }
    if (!resources.split) {
        appendKey(keys.layout, pushStagesOf(context));
        for (const auto word : resources.mainKey) appendKey(keys.layout, word);
        keys.preRasterizationLayout = keys.fragmentLayout = keys.layout;
        return true;
    }
    const auto splitKey = [&](std::vector<std::byte>& key, std::uint32_t sets) {
        appendKey(key, ~0u);
        appendKey(key, sets);
        appendKey(key, pushStagesOf(context));
        if ((sets & 1u) != 0u) {
            appendKey(key, resources.mainKey.size());
            for (const auto word : resources.mainKey) appendKey(key, word);
        }
        if ((sets & 2u) != 0u) {
            appendKey(key, resources.fragmentKey.size());
            for (const auto word : resources.fragmentKey) appendKey(key, word);
        }
    };
    splitKey(keys.layout, 3u);
    splitKey(keys.preRasterizationLayout, keys.preRasterizationSets);
    splitKey(keys.fragmentLayout, keys.fragmentSets);
    return true;
}

}

Pipeline::Pipeline(const Context& context, const State& state, const VertexInputLayout& vertexInput, const ShaderResources& resources, std::span<const CompiledShader> shaders, VkImageLayout attachmentLayout) : Pipeline(context, state, vertexInput, resources.SetLayouts(), shaders, attachmentLayout) {}

}
