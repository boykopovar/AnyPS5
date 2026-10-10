#include "SceShaders.hpp"
#include "prx/libSceAgc/Shader/include/PrimState.hpp"
#include "prx/libSceAgc/Shader/include/InterpolantMapping.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <algorithm>
#include <array>
#include <span>
#include <stdexcept>
#include <string>

extern "C" int APS5_VABI sceAgcLinkShaders(ShaderRegister*, ShaderRegister*, const void*, const Shader*, const Shader*, std::uint32_t);
extern "C" int APS5_VABI sceAgcCreatePrimState(ShaderRegister*, ShaderRegister*, const Shader*, const Shader*, std::uint32_t);
extern "C" int APS5_VABI sceAgcCreateInterpolantMapping_0100(ShaderRegister*, const Shader*, const Shader*);

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;
using Testing::RequireThrows;

std::array<ShaderRegister, 2> preparedContext;
std::array<ShaderRegister, 3> preparedPrimitive;
std::size_t stageCount = 0;
unsigned preparations = 0;
unsigned mappings = 0;
unsigned links = 0;
bool rejectLink = false;
const Shader* mappedPixel = nullptr;

void ResetHooks() {
    preparedContext = {};
    preparedPrimitive = {};
    stageCount = 0;
    preparations = 0;
    mappings = 0;
    links = 0;
    rejectLink = false;
    mappedPixel = nullptr;
}

class ShaderSet {
public:
    ShaderSet() {
        using namespace ShaderRegs;
        ResetHooks();
        special.vgt_shader_stages_en = {VGT_SHADER_STAGES_EN, 0};
        special.vgt_gs_out_prim_type = {VGT_GS_OUT_PRIM_TYPE, 2};
        special.ge_cntl = {GE_CNTL, 0x123};
        special.ge_user_vgpr_en = {GE_USER_VGPR_EN, 7};
        vertex.type = static_cast<std::uint8_t>(ShaderBinaryType::Gs);
        vertex.specials = &special;
        hull.type = static_cast<std::uint8_t>(ShaderBinaryType::Hs);
        hull.specials = &special;
        pixel.type = static_cast<std::uint8_t>(ShaderBinaryType::Ps);
        input.semantic = 9;
        pixel.input_semantics = &input;
        pixel.num_input_semantics = 1;
    }

    ~ShaderSet() {
        ResetHooks();
    }

    ShaderSet(const ShaderSet&) = delete;
    ShaderSet& operator=(const ShaderSet&) = delete;

    ShaderSpecialRegs special{};
    Shader vertex{};
    Shader hull{};
    Shader pixel{};
    ShaderSemantic input{};
};

template<std::size_t TCount>
void Poison(std::array<ShaderRegister, TCount>& registers) {
    for (auto& value : registers) value.value = 0xdeadbeef;
}

template<std::size_t TCount>
void RequirePoisoned(const std::array<ShaderRegister, TCount>& registers, const char* message) {
    for (std::size_t i = 0; i < TCount; ++i) RequireEqual(registers[i].value, 0xdeadbeefu, std::string(message) + " register " + std::to_string(i));
}

const Case primStateAndMapping{"CreatePrimStateAndInterpolantMapping_AnyOrderAndOutputs_PrepareOnceAndPublish", [] {
    using namespace ShaderRegs;
    ShaderSet shaders;
    std::array<ShaderRegister, 32> interpolants{};
    for (const auto count : {0u, 1u}) {
        shaders.pixel.num_input_semantics = count;
        for (const bool primitiveFirst : {false, true}) {
            for (unsigned outputs = 1; outputs <= 3; ++outputs) {
                for (const auto* hs : {static_cast<const Shader*>(nullptr), static_cast<const Shader*>(&shaders.hull)}) {
                    const auto name = "inputs " + std::to_string(count) + (primitiveFirst ? " primitive first" : " mapping first") +
                                      " outputs " + std::to_string(outputs) + (hs ? " with hull" : " without hull");
                    preparations = mappings = links = 0;
                    std::array<ShaderRegister, 3> context{};
                    std::array<ShaderRegister, 4> primitive{};
                    context.back().value = primitive.back().value = 0xdeadbeef;
                    const auto prepare = [&] {
                        RequireEqual(sceAgcCreatePrimState(outputs & 1 ? context.data() : nullptr, outputs & 2 ? primitive.data() : nullptr, hs,
                                                           &shaders.vertex, 7), 0, name + ": prim state");
                    };
                    if (primitiveFirst) prepare();
                    RequireEqual(sceAgcCreateInterpolantMapping_0100(interpolants.data(), &shaders.vertex, &shaders.pixel), 0, name + ": mapping");
                    if (!primitiveFirst) prepare();
                    RequireEqual(preparations, 1u, name + ": preparations");
                    RequireEqual(mappings, 1u, name + ": mappings");
                    RequireEqual(links, 1u, name + ": links");
                    RequireEqual(stageCount, hs ? std::size_t{2} : std::size_t{1}, name + ": stages");
                    Require(preparedContext[0].offset == VGT_SHADER_STAGES_EN && preparedContext[1].offset == VGT_GS_OUT_PRIM_TYPE,
                            name + ": prepared context offsets");
                    Require(preparedPrimitive[0].value == 0x123 && preparedPrimitive[1].value == 7 && preparedPrimitive[2].value == 7,
                            name + ": prepared primitive values");
                    Require(context.back().value == 0xdeadbeef && primitive.back().value == 0xdeadbeef, name + ": no overrun");
                    for (unsigned i = 0; i < 2; ++i) {
                        RequireEqual(context[i].value, outputs & 1 ? preparedContext[i].value : 0u, name + ": context " + std::to_string(i));
                    }
                    for (unsigned i = 0; i < 3; ++i) {
                        RequireEqual(primitive[i].value, outputs & 2 ? preparedPrimitive[i].value : 0u, name + ": primitive " + std::to_string(i));
                    }
                }
            }
        }
    }
}};

const Case patchPrimState{"CreatePrimState_PatchPrimitives_PreparesOnlyCompleteHullPatchState", [] {
    using namespace ShaderRegs;
    ShaderSet shaders;
    std::array<ShaderRegister, 2> patchContext{};
    std::array<ShaderRegister, 3> patchPrimitive{};
    RequireEqual(sceAgcCreatePrimState(patchContext.data(), patchPrimitive.data(), nullptr, &shaders.vertex, 9), 0, "patch without hull");
    RequireEqual(preparations, 0u, "patch without hull prepares nothing");
    Require(patchContext[0].offset == VGT_SHADER_STAGES_EN && patchContext[0].value == 0, "patch without hull stages");
    Require(patchPrimitive[2].offset == VGT_PRIMITIVE_TYPE && patchPrimitive[2].value == 9, "patch without hull primitive type");
    ShaderSpecialRegs hullSpecial = shaders.special;
    hullSpecial.vgt_shader_stages_en.value = VGT_SHADER_STAGES_HS_BIT;
    shaders.hull.specials = &hullSpecial;
    RequireEqual(sceAgcCreatePrimState(patchContext.data(), patchPrimitive.data(), &shaders.hull, &shaders.vertex, 4), 0, "hull with type 4");
    RequireEqual(preparations, 0u, "hull with type 4 prepares nothing");
    Require(patchContext[0].value == VGT_SHADER_STAGES_HS_BIT && patchPrimitive[2].value == 4, "hull with type 4 registers");
    RequireEqual(sceAgcCreatePrimState(patchContext.data(), patchPrimitive.data(), &shaders.hull, &shaders.vertex, 9), 0, "hull with patch type");
    RequireEqual(preparations, 1u, "hull with patch type prepares once");
    RequireEqual(stageCount, std::size_t{2}, "hull with patch type stages");
    Require(preparedContext[0].value == VGT_SHADER_STAGES_HS_BIT && preparedPrimitive[2].value == 9 && patchPrimitive[2].value == 9,
            "hull with patch type registers");
}};

const Case linkRejected{"LinkShaders_LinkHookFails_ThrowsWithoutWriting", [] {
    ShaderSet shaders;
    std::array<ShaderRegister, 34> linkedContext{};
    std::array<ShaderRegister, 3> linkedPrimitive{};
    std::array<ShaderRegister, 32> interpolants{};
    Poison(linkedContext);
    Poison(linkedPrimitive);
    Poison(interpolants);
    rejectLink = true;
    RequireThrows<std::runtime_error>(
        [&] { sceAgcLinkShaders(linkedContext.data(), linkedPrimitive.data(), nullptr, &shaders.vertex, &shaders.pixel, 7); }, "link");
    RequirePoisoned(linkedContext, "linked context");
    RequirePoisoned(linkedPrimitive, "linked primitive");
    RequireThrows<std::runtime_error>(
        [&] { sceAgcCreateInterpolantMapping_0100(interpolants.data(), &shaders.vertex, &shaders.pixel); }, "mapping");
    RequirePoisoned(interpolants, "interpolants");
}};

const Case tooManyInputs{"CreateInterpolantMapping_TooManyInputs_ThrowsWithoutWriting", [] {
    ShaderSet shaders;
    std::array<ShaderRegister, 32> interpolants{};
    Poison(interpolants);
    shaders.pixel.num_input_semantics = 33;
    RequireThrows<std::runtime_error>(
        [&] { sceAgcCreateInterpolantMapping_0100(interpolants.data(), &shaders.vertex, &shaders.pixel); }, "33 inputs");
    RequirePoisoned(interpolants, "interpolants");
}};

const Case linkSucceeds{"LinkShaders_ValidShaders_Succeeds", [] {
    ShaderSet shaders;
    std::array<ShaderRegister, 34> linkedContext{};
    std::array<ShaderRegister, 3> linkedPrimitive{};
    RequireEqual(sceAgcLinkShaders(linkedContext.data(), linkedPrimitive.data(), nullptr, &shaders.vertex, &shaders.pixel, 7), 0, "link");
}};

const Case missingShaders{"Helpers_MissingShaders_DoNotCallDriverHooks", [] {
    ShaderSet shaders;
    std::array<ShaderRegister, 32> interpolants{};
    RequireEqual(sceAgcCreatePrimState(nullptr, nullptr, nullptr, nullptr, 7), 0, "prim state without shaders");
    RequireEqual(sceAgcCreateInterpolantMapping_0100(interpolants.data(), &shaders.vertex, nullptr), 0, "mapping without pixel shader");
    RequireEqual(preparations, 0u, "preparations");
    RequireEqual(mappings, 0u, "mappings");
    RequireEqual(links, 0u, "links");
}};

const Case optionalGeometryOutput{"CreatePrimState_MissingGeometryOutputRegister_DefaultsOrThrows", [] {
    using namespace ShaderRegs;
    ShaderSet shaders;
    shaders.special.vgt_gs_out_prim_type = {};
    std::array<ShaderRegister, 2> noGeometryContext{};
    RequireEqual(sceAgcCreatePrimState(noGeometryContext.data(), nullptr, nullptr, &shaders.vertex, 7), 0, "without geometry output");
    Require(noGeometryContext[0].offset == VGT_SHADER_STAGES_EN && noGeometryContext[1].offset == VGT_GS_OUT_PRIM_TYPE &&
            noGeometryContext[1].value == static_cast<std::uint32_t>(GsOutputPrimitiveType::Rectangle2D), "defaults to rectangle output");
    RequireThrows<std::runtime_error>(
        [&] { sceAgcCreatePrimState(noGeometryContext.data(), nullptr, &shaders.hull, &shaders.vertex, 7); }, "hull without its output");
    shaders.special.vgt_shader_stages_en.value = VGT_SHADER_STAGES_GS_BIT;
    RequireThrows<std::runtime_error>(
        [&] { sceAgcCreatePrimState(noGeometryContext.data(), nullptr, nullptr, &shaders.vertex, 7); }, "geometry stage without its output");
}};

} // namespace

extern "C" void AgcDriverResolveGraphicsStagesAbi_nid_postfix(std::span<const Shader* const> stages, std::span<const ShaderRegister> context, std::span<const ShaderRegister> primitive) {
    Require(context.size() == 2 && primitive.size() == 3, "stage preparation register counts");
    std::copy(context.begin(), context.end(), preparedContext.begin());
    std::copy(primitive.begin(), primitive.end(), preparedPrimitive.begin());
    stageCount = stages.size();
    ++preparations;
}

extern "C" void AgcDriverResolveShaderAbi_nid_postfix(const Shader* shader, std::span<const ShaderRegister> context, std::span<const ShaderRegister> primitive) {
    Require(context.size() == 32 && primitive.empty(), "mapping preparation register counts");
    mappedPixel = shader;
    ++mappings;
}

extern "C" void AgcDriverResolveGraphicsAbi_nid_postfix(const Shader* vertex, const Shader* pixel, std::uint32_t primitiveType) {
    if (rejectLink) throw std::runtime_error("injected link failure");
    Require(vertex != nullptr && pixel == mappedPixel && (primitiveType == 0 || primitiveType == 7), "link call arguments");
    ++links;
}
