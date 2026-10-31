#include "prx/libScePigletv2VSH/include/Context.hpp"

#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>

#include <mutex>
#include <string>
#include <vector>

namespace Piglet {

namespace {

constexpr EShMessages Messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);

void Configure(glslang::TShader& shader, EShLanguage stage, const char*& text) {
    shader.setStrings(&text, 1);
    shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 100);
    shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_1);
    shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_3);
    shader.setEnvInputVulkanRulesRelaxed();
    shader.setAutoMapBindings(true);
    shader.setAutoMapLocations(true);
    shader.setShiftBinding(glslang::EResTexture, 1);
    shader.setShiftBinding(glslang::EResSampler, 1);
}

VkShaderModule Module(const std::vector<unsigned int>& code) {
    auto& device = Device::Get();
    VkShaderModuleCreateInfo info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    info.codeSize = code.size() * sizeof(unsigned int);
    info.pCode = code.data();
    VkShaderModule module = VK_NULL_HANDLE;
    Check(device.vkCreateShaderModule(device.device, &info, nullptr, &module), "vkCreateShaderModule");
    return module;
}

}

void LinkProgram(Program& program, const Shader& vertexShader, const Shader& fragmentShader) {
    static std::once_flag initialized;
    std::call_once(initialized, [] { glslang::InitializeProcess(); });
    auto& device = Device::Get();
    program.linked = false;
    program.log.clear();
    glslang::TShader vertex(EShLangVertex);
    glslang::TShader fragment(EShLangFragment);
    const char* vertexText = vertexShader.source.c_str();
    const char* fragmentText = fragmentShader.source.c_str();
    Configure(vertex, EShLangVertex, vertexText);
    Configure(fragment, EShLangFragment, fragmentText);
    if (!vertex.parse(GetDefaultResources(), 310, true, Messages)) {
        program.log = vertex.getInfoLog();
        return;
    }
    if (!fragment.parse(GetDefaultResources(), 310, true, Messages)) {
        program.log = fragment.getInfoLog();
        return;
    }
    glslang::TProgram linker;
    linker.addShader(&vertex);
    linker.addShader(&fragment);
    if (!linker.link(Messages) || !linker.mapIO() || !linker.buildReflection()) {
        program.log = linker.getInfoLog();
        return;
    }

    program.uniforms.clear();
    program.attributes.clear();
    program.uniformBinding = -1;
    std::size_t blockSize = 0;
    for (int index = 0; index < linker.getNumUniformBlocks(); ++index) {
        const auto& block = linker.getUniformBlock(index);
        if (block.name != "gl_DefaultUniformBlock") throw std::runtime_error("glLinkProgram: uniform blocks are not part of OpenGL ES 2.0");
        program.uniformBinding = block.getBinding();
        blockSize = static_cast<std::size_t>(block.size);
    }
    for (int index = 0; index < linker.getNumUniformVariables(); ++index) {
        const auto& variable = linker.getUniform(index);
        Uniform uniform;
        uniform.name = variable.name;
        uniform.type = static_cast<GLenum>(variable.glDefineType);
        uniform.size = variable.size;
        if (variable.offset < 0) uniform.binding = variable.getBinding();
        else uniform.offset = variable.offset;
        program.uniforms.push_back(uniform);
    }
    for (int index = 0; index < linker.getNumPipeInputs(); ++index) {
        const auto& input = linker.getPipeInput(index);
        if (input.name.rfind("gl_", 0) == 0) continue;
        program.attributes.push_back({input.name, static_cast<GLenum>(input.glDefineType), static_cast<GLint>(input.layoutLocation())});
    }

    std::vector<unsigned int> vertexCode;
    std::vector<unsigned int> fragmentCode;
    glslang::GlslangToSpv(*linker.getIntermediate(EShLangVertex), vertexCode);
    glslang::GlslangToSpv(*linker.getIntermediate(EShLangFragment), fragmentCode);
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    if (program.uniformBinding >= 0)
        bindings.push_back({static_cast<std::uint32_t>(program.uniformBinding), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
    for (const auto& uniform : program.uniforms)
        if (uniform.binding >= 0)
            bindings.push_back({static_cast<std::uint32_t>(uniform.binding), VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr});
    VkDescriptorSetLayoutCreateInfo setInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    setInfo.bindingCount = static_cast<std::uint32_t>(bindings.size());
    setInfo.pBindings = bindings.data();
    if (program.vertex) device.vkDestroyShaderModule(device.device, program.vertex, nullptr);
    if (program.fragment) device.vkDestroyShaderModule(device.device, program.fragment, nullptr);
    if (program.layout) device.vkDestroyPipelineLayout(device.device, program.layout, nullptr);
    if (program.setLayout) device.vkDestroyDescriptorSetLayout(device.device, program.setLayout, nullptr);
    Check(device.vkCreateDescriptorSetLayout(device.device, &setInfo, nullptr, &program.setLayout), "vkCreateDescriptorSetLayout");
    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &program.setLayout;
    Check(device.vkCreatePipelineLayout(device.device, &layoutInfo, nullptr, &program.layout), "vkCreatePipelineLayout");
    program.vertex = Module(vertexCode);
    program.fragment = Module(fragmentCode);
    program.uniformData.assign((blockSize + 15) / 16 * 16, 0);
    program.samplerUnits.assign(program.uniforms.size(), 0);
    program.linked = true;
}

}
