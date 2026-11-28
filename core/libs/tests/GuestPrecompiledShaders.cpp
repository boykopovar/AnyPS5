#include "prx/libScePrecompiledShaders/include/PrecompiledShaders.hpp"

#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <SPIRV/GlslangToSpv.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

extern "C" const PrecompiledShaders::Entry scePrecompiledShaderEntries_nid_postfix[];

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Precompiled shader check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static const PrecompiledShaders::Entry* Find(const char* name) {
    for (const auto* entry = scePrecompiledShaderEntries_nid_postfix; entry->name; ++entry)
        if (std::strcmp(entry->name, name) == 0) return entry;
    return nullptr;
}

static std::string Source(const PrecompiledShaders::Entry* entry) {
    const std::string_view bytes(static_cast<const char*>(entry->data), entry->size);
    Require(bytes.starts_with(PrecompiledShaders::SourceMagic));
    Require(std::strlen(static_cast<const char*>(entry->data)) == entry->size);
    return std::string(bytes.substr(PrecompiledShaders::SourceMagic.size()));
}

static std::unique_ptr<glslang::TShader> Parse(EShLanguage stage, const std::string& source) {
    auto shader = std::make_unique<glslang::TShader>(stage);
    const char* text = source.c_str();
    shader->setStrings(&text, 1);
    shader->setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 100);
    shader->setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
    shader->setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);
    shader->setEnvInputVulkanRulesRelaxed();
    shader->setAutoMapBindings(true);
    shader->setAutoMapLocations(true);
    const bool parsed = shader->parse(GetDefaultResources(), 310, true, static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules));
    if (!parsed) std::fprintf(stderr, "%s\n", shader->getInfoLog());
    Require(parsed);
    return shader;
}

int main() {
    int count = 0;
    for (const auto* entry = scePrecompiledShaderEntries_nid_postfix; entry->name; ++entry) {
        Require(entry->reserved0 == 0 && entry->reserved1 == 0 && entry->data != nullptr);
        ++count;
    }
    Require(count == 2);
    const auto* vertex = Find("texmap/v_2.vert");
    const auto* fragment = Find("texmap/f_2.frag");
    Require(vertex && fragment && !Find("texmap/v_2.frag"));

    Require(glslang::InitializeProcess());
    {
        const auto vertexShader = Parse(EShLangVertex, Source(vertex));
        const auto fragmentShader = Parse(EShLangFragment, Source(fragment));
        glslang::TProgram program;
        program.addShader(vertexShader.get());
        program.addShader(fragmentShader.get());
        const bool linked = program.link(static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules));
        if (!linked) std::fprintf(stderr, "%s\n", program.getInfoLog());
        Require(linked && program.buildReflection());

        std::map<std::string, int> uniforms;
        for (int index = 0; index < program.getNumUniformVariables(); ++index)
            uniforms[program.getUniformName(index)] = program.getUniformType(index);
        Require(uniforms.size() == 5);
        Require(uniforms["u_projectionMatrix"] == 0x8B5C && uniforms["u_modelViewMatrix"] == 0x8B5C && uniforms["u_textureSpaceMatrix"] == 0x8B5C);
        Require(uniforms["u_opacity"] == 0x1406 && uniforms["s_sampler"] == 0x8B5E);
        Require(program.getNumPipeInputs() == 1 && std::strcmp(program.getPipeInput(0).name.c_str(), "a_vertex") == 0 && program.getPipeInput(0).glDefineType == 0x8B52);
        std::vector<unsigned int> spirv;
        glslang::GlslangToSpv(*program.getIntermediate(EShLangFragment), spirv);
        Require(!spirv.empty());
    }
    glslang::FinalizeProcess();
}
