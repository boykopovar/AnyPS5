#include "DummyShaders.hpp"

#include <SPIRV/GlslangToSpv.h>
#include <glslang/Public/ShaderLang.h>

#include <cstdio>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace ShaderRecompiler {

namespace {

constexpr const char* VertexSource = R"(
#version 450

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    vec2 p[3] = vec2[](
        vec2(-1.0, -1.0),
        vec2(3.0, -1.0),
        vec2(-1.0, 3.0)
    );

    gl_Position = vec4(p[gl_VertexIndex % 3], 0.0, 1.0);
}
)";

constexpr const char* FragmentSource = R"(
#version 450

layout(location = 0) out vec4 color;

void main() {
    color = vec4(1.0, 0.0, 1.0, 1.0);
}
)";

TBuiltInResource MakeResources() {
    TBuiltInResource r{};

    r.maxLights = 32;
    r.maxClipPlanes = 6;
    r.maxTextureUnits = 32;
    r.maxTextureCoords = 32;
    r.maxVertexAttribs = 64;
    r.maxVertexUniformComponents = 4096;
    r.maxVaryingFloats = 64;
    r.maxVertexTextureImageUnits = 32;
    r.maxCombinedTextureImageUnits = 80;
    r.maxTextureImageUnits = 32;
    r.maxFragmentUniformComponents = 4096;
    r.maxDrawBuffers = 32;
    r.maxVertexUniformVectors = 128;
    r.maxVaryingVectors = 8;
    r.maxFragmentUniformVectors = 16;
    r.maxVertexOutputVectors = 16;
    r.maxFragmentInputVectors = 15;
    r.minProgramTexelOffset = -8;
    r.maxProgramTexelOffset = 7;
    r.maxClipDistances = 8;
    r.maxComputeWorkGroupCountX = 65535;
    r.maxComputeWorkGroupCountY = 65535;
    r.maxComputeWorkGroupCountZ = 65535;
    r.maxComputeWorkGroupSizeX = 1024;
    r.maxComputeWorkGroupSizeY = 1024;
    r.maxComputeWorkGroupSizeZ = 64;
    r.maxComputeUniformComponents = 1024;
    r.maxComputeTextureImageUnits = 16;
    r.maxComputeImageUniforms = 8;
    r.maxComputeAtomicCounters = 8;
    r.maxComputeAtomicCounterBuffers = 1;
    r.maxVaryingComponents = 60;
    r.maxVertexOutputComponents = 64;
    r.maxGeometryInputComponents = 64;
    r.maxGeometryOutputComponents = 128;
    r.maxFragmentInputComponents = 128;
    r.maxImageUnits = 8;
    r.maxCombinedImageUnitsAndFragmentOutputs = 8;
    r.maxCombinedShaderOutputResources = 8;
    r.maxImageSamples = 0;
    r.maxVertexImageUniforms = 0;
    r.maxTessControlImageUniforms = 0;
    r.maxTessEvaluationImageUniforms = 0;
    r.maxGeometryImageUniforms = 0;
    r.maxFragmentImageUniforms = 8;
    r.maxCombinedImageUniforms = 8;
    r.maxGeometryTextureImageUnits = 16;
    r.maxGeometryOutputVertices = 256;
    r.maxGeometryTotalOutputComponents = 1024;
    r.maxGeometryUniformComponents = 1024;
    r.maxGeometryVaryingComponents = 64;
    r.maxTessControlInputComponents = 128;
    r.maxTessControlOutputComponents = 128;
    r.maxTessControlTextureImageUnits = 16;
    r.maxTessControlUniformComponents = 1024;
    r.maxTessControlTotalOutputComponents = 4096;
    r.maxTessEvaluationInputComponents = 128;
    r.maxTessEvaluationOutputComponents = 128;
    r.maxTessEvaluationTextureImageUnits = 16;
    r.maxTessEvaluationUniformComponents = 1024;
    r.maxTessPatchComponents = 120;
    r.maxPatchVertices = 32;
    r.maxTessGenLevel = 64;
    r.maxViewports = 16;
    r.maxVertexAtomicCounters = 0;
    r.maxTessControlAtomicCounters = 0;
    r.maxTessEvaluationAtomicCounters = 0;
    r.maxGeometryAtomicCounters = 0;
    r.maxFragmentAtomicCounters = 8;
    r.maxCombinedAtomicCounters = 8;
    r.maxAtomicCounterBindings = 1;
    r.maxVertexAtomicCounterBuffers = 0;
    r.maxTessControlAtomicCounterBuffers = 0;
    r.maxTessEvaluationAtomicCounterBuffers = 0;
    r.maxGeometryAtomicCounterBuffers = 0;
    r.maxFragmentAtomicCounterBuffers = 1;
    r.maxCombinedAtomicCounterBuffers = 1;
    r.maxAtomicCounterBufferSize = 16384;
    r.maxTransformFeedbackBuffers = 4;
    r.maxTransformFeedbackInterleavedComponents = 64;
    r.maxCullDistances = 8;
    r.maxCombinedClipAndCullDistances = 8;
    r.maxSamples = 4;

    r.limits.nonInductiveForLoops = 1;
    r.limits.whileLoops = 1;
    r.limits.doWhileLoops = 1;
    r.limits.generalUniformIndexing = 1;
    r.limits.generalAttributeMatrixVectorIndexing = 1;
    r.limits.generalVaryingIndexing = 1;
    r.limits.generalSamplerIndexing = 1;
    r.limits.generalVariableIndexing = 1;
    r.limits.generalConstantMatrixVectorIndexing = 1;

    return r;
}

void InitGlslang() {
    static std::once_flag flag;

    std::call_once(flag, [] {
        if (!glslang::InitializeProcess())
            throw std::runtime_error("glslang initialization failed");
    });
}

std::vector<std::uint32_t> Compile(const char* source, EShLanguage stage) {
    InitGlslang();

    const auto resources = MakeResources();

    glslang::TShader shader(stage);
    shader.setStrings(&source, 1);
    shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 450);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_3);

    constexpr auto messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);

    if (!shader.parse(&resources, 450, false, messages))
        throw std::runtime_error(std::string("glslang parse failed: ") + shader.getInfoLog());

    glslang::TProgram program;
    program.addShader(&shader);

    if (!program.link(messages))
        throw std::runtime_error(std::string("glslang link failed: ") + program.getInfoLog());

    const auto* intermediate = program.getIntermediate(stage);

    if (!intermediate)
        throw std::runtime_error("glslang returned no intermediate");

    std::vector<std::uint32_t> spirv;
    glslang::SpvOptions options{};
    options.disableOptimizer = true;
    options.validate = true;

    glslang::GlslangToSpv(*intermediate, spirv, &options);

    if (spirv.empty())
        throw std::runtime_error("glslang generated empty SPIR-V");

    return spirv;
}

const std::vector<std::uint32_t>& VertexSpv() {
    static const auto spirv = Compile(VertexSource, EShLangVertex);
    return spirv;
}

const std::vector<std::uint32_t>& FragmentSpv() {
    static const auto spirv = Compile(FragmentSource, EShLangFragment);
    return spirv;
}

RecompileResult MakeResult(const std::vector<std::uint32_t>& spirv) {
    RecompileResult result;
    result.spirv = spirv;
    return result;
}

}

RecompileResult RecompileDummy(const RecompileRequest& request) {
    switch (request.shader.stage) {
    case ShaderStage::Vertex:
        std::fprintf(stderr, "[DummyShader] Vertex\n");
        std::fflush(stderr);
        return MakeResult(VertexSpv());

    case ShaderStage::Fragment:
        std::fprintf(stderr, "[DummyShader] Fragment\n");
        std::fflush(stderr);
        return MakeResult(FragmentSpv());

    default:
        std::fprintf(stderr, "[DummyShader] Unsupported stage: %u\n", static_cast<unsigned>(request.shader.stage));
        std::fflush(stderr);
        throw std::runtime_error("Dummy shader: unsupported stage");
    }
}

}
