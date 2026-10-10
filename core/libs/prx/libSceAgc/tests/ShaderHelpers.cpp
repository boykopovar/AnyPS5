#include "SceShaders.hpp"
#include "prx/libSceAgc/Shader/include/PrimState.hpp"
#include "prx/libSceAgc/Shader/include/InterpolantMapping.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <span>
#include <stdexcept>
#include <iostream>

extern "C" int APS5_VABI sceAgcLinkShaders(ShaderRegister*, ShaderRegister*, const void*, const Shader*, const Shader*, std::uint32_t);
extern "C" int APS5_VABI sceAgcCreatePrimState(ShaderRegister*, ShaderRegister*, const Shader*, const Shader*, std::uint32_t);
extern "C" int APS5_VABI sceAgcCreateInterpolantMapping_0100(ShaderRegister*, const Shader*, const Shader*);

namespace {
std::array<ShaderRegister, 2> preparedContext;
std::array<ShaderRegister, 3> preparedPrimitive;
std::size_t stageCount = 0;
unsigned preparations = 0;
unsigned mappings = 0;
unsigned links = 0;
bool rejectLink = false;
const Shader* mappedPixel = nullptr;

void Require(bool condition) {
    if (!condition) throw std::runtime_error("shader helper preparation mismatch");
}

void OptionalGeometryOutputRegister(ShaderSpecialRegs& special, const Shader& vertex, const Shader& hull) {
    using namespace ShaderRegs;
    special.vgt_gs_out_prim_type = {};
    std::array<ShaderRegister, 2> noGeometryContext{};
    Require(sceAgcCreatePrimState(noGeometryContext.data(), nullptr, nullptr, &vertex, 7) == 0);
    Require(noGeometryContext[0].offset == VGT_SHADER_STAGES_EN && noGeometryContext[1].offset == VGT_GS_OUT_PRIM_TYPE && noGeometryContext[1].value == static_cast<std::uint32_t>(GsOutputPrimitiveType::Rectangle2D));
    bool rejectedMissingHullOutput = false;
    try { static_cast<void>(sceAgcCreatePrimState(noGeometryContext.data(), nullptr, &hull, &vertex, 7)); }
    catch (const std::runtime_error&) { rejectedMissingHullOutput = true; }
    Require(rejectedMissingHullOutput);
    special.vgt_shader_stages_en.value = VGT_SHADER_STAGES_GS_BIT;
    bool rejectedMissingGeometryOutput = false;
    try { static_cast<void>(sceAgcCreatePrimState(noGeometryContext.data(), nullptr, nullptr, &vertex, 7)); }
    catch (const std::runtime_error&) { rejectedMissingGeometryOutput = true; }
    Require(rejectedMissingGeometryOutput);
    special.vgt_shader_stages_en.value = 0;
    special.vgt_gs_out_prim_type = {VGT_GS_OUT_PRIM_TYPE, 2};
}
}

extern "C" void AgcDriverResolveGraphicsStagesAbi_nid_postfix(std::span<const Shader* const> stages, std::span<const ShaderRegister> context, std::span<const ShaderRegister> primitive) {
    Require(context.size() == 2 && primitive.size() == 3);
    std::copy(context.begin(), context.end(), preparedContext.begin());
    std::copy(primitive.begin(), primitive.end(), preparedPrimitive.begin());
    stageCount = stages.size();
    ++preparations;
}

extern "C" void AgcDriverResolveShaderAbi_nid_postfix(const Shader* shader, std::span<const ShaderRegister> context, std::span<const ShaderRegister> primitive) {
    Require(context.size() == 32 && primitive.empty());
    mappedPixel = shader;
    ++mappings;
}

extern "C" void AgcDriverResolveGraphicsAbi_nid_postfix(const Shader* vertex, const Shader* pixel, std::uint32_t primitiveType) {
    if (rejectLink) throw std::runtime_error("injected link failure");
    Require(vertex != nullptr && pixel == mappedPixel && (primitiveType == 0 || primitiveType == 7));
    ++links;
}

int main() {
    try {
        using namespace ShaderRegs;
        ShaderSpecialRegs special{};
        special.vgt_shader_stages_en = {VGT_SHADER_STAGES_EN, 0};
        special.vgt_gs_out_prim_type = {VGT_GS_OUT_PRIM_TYPE, 2};
        special.ge_cntl = {GE_CNTL, 0x123};
        special.ge_user_vgpr_en = {GE_USER_VGPR_EN, 7};
        Shader vertex{};
        vertex.type = static_cast<std::uint8_t>(ShaderBinaryType::Gs);
        vertex.specials = &special;
        Shader hull{};
        hull.type = static_cast<std::uint8_t>(ShaderBinaryType::Hs);
        hull.specials = &special;
        Shader pixel{};
        pixel.type = static_cast<std::uint8_t>(ShaderBinaryType::Ps);
        ShaderSemantic input{};
        input.semantic = 9;
        pixel.input_semantics = &input;
        std::array<ShaderRegister, 32> interpolants{};
        for (const auto count : {0u, 1u}) {
            pixel.num_input_semantics = count;
            for (const bool primitiveFirst : {false, true}) {
                for (unsigned outputs = 1; outputs <= 3; ++outputs) {
                    for (const auto* hs : {static_cast<const Shader*>(nullptr), static_cast<const Shader*>(&hull)}) {
                        preparations = mappings = links = 0;
                        std::array<ShaderRegister, 3> context{};
                        std::array<ShaderRegister, 4> primitive{};
                        context.back().value = primitive.back().value = 0xdeadbeef;
                        const auto prepare = [&] { Require(sceAgcCreatePrimState(outputs & 1 ? context.data() : nullptr, outputs & 2 ? primitive.data() : nullptr, hs, &vertex, 7) == 0); };
                        if (primitiveFirst) prepare();
                        Require(sceAgcCreateInterpolantMapping_0100(interpolants.data(), &vertex, &pixel) == 0);
                        if (!primitiveFirst) prepare();
                        Require(preparations == 1 && mappings == 1 && links == 1 && stageCount == (hs ? 2u : 1u));
                        Require(preparedContext[0].offset == VGT_SHADER_STAGES_EN && preparedContext[1].offset == VGT_GS_OUT_PRIM_TYPE);
                        Require(preparedPrimitive[0].value == 0x123 && preparedPrimitive[1].value == 7 && preparedPrimitive[2].value == 7);
                        Require(context.back().value == 0xdeadbeef && primitive.back().value == 0xdeadbeef);
                        for (unsigned i = 0; i < 2; ++i) Require(context[i].value == (outputs & 1 ? preparedContext[i].value : 0));
                        for (unsigned i = 0; i < 3; ++i) Require(primitive[i].value == (outputs & 2 ? preparedPrimitive[i].value : 0));
                    }
                }
            }
        }
        std::array<ShaderRegister, 34> linkedContext{};
        std::array<ShaderRegister, 3> linkedPrimitive{};
        for (auto& value : linkedContext) value.value = 0xdeadbeef;
        for (auto& value : linkedPrimitive) value.value = 0xdeadbeef;
        for (auto& value : interpolants) value.value = 0xdeadbeef;
        rejectLink = true;
        bool rejected = false;
        try { sceAgcLinkShaders(linkedContext.data(), linkedPrimitive.data(), nullptr, &vertex, &pixel, 7); }
        catch (const std::runtime_error&) { rejected = true; }
        Require(rejected);
        for (const auto value : linkedContext) Require(value.value == 0xdeadbeef);
        for (const auto value : linkedPrimitive) Require(value.value == 0xdeadbeef);
        rejected = false;
        try { sceAgcCreateInterpolantMapping_0100(interpolants.data(), &vertex, &pixel); }
        catch (const std::runtime_error&) { rejected = true; }
        Require(rejected);
        for (const auto value : interpolants) Require(value.value == 0xdeadbeef);
        rejectLink = false;
        pixel.num_input_semantics = 33;
        rejected = false;
        try { sceAgcCreateInterpolantMapping_0100(interpolants.data(), &vertex, &pixel); }
        catch (const std::runtime_error&) { rejected = true; }
        Require(rejected);
        for (const auto value : interpolants) Require(value.value == 0xdeadbeef);
        pixel.num_input_semantics = 1;
        Require(sceAgcLinkShaders(linkedContext.data(), linkedPrimitive.data(), nullptr, &vertex, &pixel, 7) == 0);
        preparations = mappings = links = 0;
        Require(sceAgcCreatePrimState(nullptr, nullptr, nullptr, nullptr, 7) == 0);
        Require(sceAgcCreateInterpolantMapping_0100(interpolants.data(), &vertex, nullptr) == 0);
        Require(preparations == 0 && mappings == 0 && links == 0);
        OptionalGeometryOutputRegister(special, vertex, hull);
        std::cout << "shader helper preparation tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
