#include "prx/libSceAgc/Shader/include/LinkShaders.hpp"
#include "prx/libSceAgc/Shader/include/InterpolantMapping.hpp"
#include "prx/libSceAgc/Misc/include/RegisterDefaults.hpp"
#include "SceShaders.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"
#include <array>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <span>

static void Require(bool value) { if (!value) std::abort(); }
static unsigned preparations = 0;
static unsigned mappings = 0;
static unsigned links = 0;
static const Shader* mappedPixel = nullptr;

extern "C" void AgcDriverResolveGraphicsStagesAbi_nid_postfix(std::span<const Shader* const> stages, std::span<const ShaderRegister> context, std::span<const ShaderRegister> primitive) {
    Require(!stages.empty() && stages[0] != nullptr && primitive.size() == 3u);
    Require((stages.size() == 1u && context.size() == 2u) ||
            (stages.size() == 2u && stages[1] == mappedPixel && context.size() == 34u));
    ++preparations;
}

extern "C" void AgcDriverResolveShaderAbi_nid_postfix(const Shader* shader, std::span<const ShaderRegister> context, std::span<const ShaderRegister> primitive) {
    // The SDK mapping passes only the shader inputs; the other variants fill all 32.
    Require(shader != nullptr && context.size() <= 32u && primitive.empty());
    mappedPixel = shader;
    ++mappings;
}

extern "C" void AgcDriverResolveGraphicsAbi_nid_postfix(const Shader* vertex, const Shader* pixel, std::uint32_t primitiveType) {
    Require(vertex != nullptr && (pixel == nullptr || pixel == mappedPixel) && (primitiveType == 0u || primitiveType == 2u || primitiveType == 4u));
    ++links;
}
int main() {
    using namespace ShaderRegs;
    auto* defaults = static_cast<unsigned char*>(sceAgcGetRegisterDefaults());
    Require(defaults && defaults == sceAgcGetRegisterDefaults());
    auto* versionedDefaults = static_cast<unsigned char*>(sceAgcGetRegisterDefaults2(13));
    Require(versionedDefaults != nullptr);
    Require(std::memcmp(defaults + 0x20, versionedDefaults + 0x20, 4 * sizeof(std::uint32_t)) == 0);
    // These are the byte offsets used by native callers, independent of the
    // private C++ structure used to construct the defaults object.
    ShaderRegister** contextBlocks = nullptr;
    std::uint32_t contextCount = 0;
    std::memcpy(&contextBlocks, defaults, sizeof(contextBlocks));
    std::memcpy(&contextCount, defaults + 0x20, sizeof(contextCount));
    Require(contextBlocks && contextBlocks[0] && contextCount == 488);
    bool hasRenderTarget = false;
    bool hasRasterizer = false;
    for (std::uint32_t i = 0; i < contextCount; ++i) {
        hasRenderTarget |= contextBlocks[0][i].offset == 0x318;
        hasRasterizer |= contextBlocks[0][i].offset == 0x205;
    }
    Require(hasRenderTarget && hasRasterizer);
    ShaderSpecialRegs special{};
    special.vgt_shader_stages_en = {VGT_SHADER_STAGES_EN, 0x2000u};
    special.vgt_gs_out_prim_type = {VGT_GS_OUT_PRIM_TYPE, 0};
    special.ge_cntl = {GE_CNTL, 0x123};
    special.ge_user_vgpr_en = {GE_USER_VGPR_EN, 7};
    Shader vertex{};
    vertex.type = static_cast<std::uint8_t>(ShaderBinaryType::Gs);
    vertex.specials = &special;
    Shader pixel{};
    pixel.type = static_cast<std::uint8_t>(ShaderBinaryType::Ps);
    std::array<ShaderRegister, 35> context{};
    std::array<ShaderRegister, 4> primitive{};
    context.back() = {0xdeadbeef, 0xcafebabe};
    primitive.back() = context.back();
    Require(sceAgcLinkShaders(context.data(), primitive.data(), nullptr, &vertex, &pixel, 4) == 0);
    Require(context[0].offset == VGT_SHADER_STAGES_EN && context[0].value == 0x2000u);
    Require(context[1].offset == VGT_GS_OUT_PRIM_TYPE && context[1].value == 2);
    for (unsigned i = 0; i < 32; ++i)
        Require(context[i + 2].offset == SPI_PS_INPUT_CNTL_0 + i && context[i + 2].value == i);
    Require(primitive[0].offset == GE_CNTL && primitive[0].value == 0x123);
    Require(primitive[1].offset == GE_USER_VGPR_EN && primitive[1].value == 7);
    Require(primitive[2].offset == VGT_PRIMITIVE_TYPE && primitive[2].value == 4);
    Require(context.back().value == 0xcafebabe && primitive.back().value == 0xcafebabe);
    const auto savedContext = context;
    const auto savedPrimitive = primitive;
    pixel.num_input_semantics = 33;
    Require(sceAgcLinkShaders(context.data(), primitive.data(), nullptr, &vertex, &pixel, 4) == GRAPHICS5_ERROR_INVALID_SHADER_PROGRAM);
    Require(std::memcmp(context.data(), savedContext.data(), sizeof(context)) == 0);
    Require(std::memcmp(primitive.data(), savedPrimitive.data(), sizeof(primitive)) == 0);
    pixel.num_input_semantics = 0;
    special.ge_cntl.offset = 0;
    bool rejected = false;
    try { sceAgcLinkShaders(context.data(), primitive.data(), nullptr, &vertex, &pixel, 4); }
    catch (const std::runtime_error&) { rejected = true; }
    Require(rejected);
    Require(std::memcmp(context.data(), savedContext.data(), sizeof(context)) == 0);
    special.ge_cntl.offset = GE_CNTL;
    Require(sceAgcLinkShaders(context.data(), primitive.data(), nullptr, &vertex, nullptr, 2) == 0);
    Require(context[1].value == 1 && primitive[2].value == 2);
    ShaderSemantic output{};
    output.semantic = 9;
    output.hardware_mapping = 5;
    vertex.output_semantics = &output;
    vertex.num_output_semantics = 1;
    ShaderSemantic input{};
    input.semantic = 9;
    input.is_flat_shaded = 1;
    pixel.input_semantics = &input;
    pixel.num_input_semantics = 1;
    Require(sceAgcLinkShaders(context.data(), primitive.data(), nullptr, &vertex, &pixel, 4) == 0);
    Require(context[2].offset == SPI_PS_INPUT_CNTL_0 && context[2].value == (5 | 0x400));
    input.semantic = 10;
    input.default_value = 2;
    Require(sceAgcLinkShaders(context.data(), primitive.data(), nullptr, &vertex, &pixel, 4) == 0);
    Require(context[2].value == 0x220);
    Require(preparations == 7u && mappings == 3u && links == 7u);

    // vieBRwlh1Lw writes one register per pixel shader input and leaves the rest untouched.
    std::array<ShaderRegister, 33> sdkMapping{};
    sdkMapping.fill({0xdeadbeef, 0xcafebabe});
    output.semantic = 9;
    output.hardware_mapping = 5;
    input.semantic = 9;
    input.default_value = 0;
    input.is_flat_shaded = 1;
    vertex.output_semantics = &output;
    vertex.num_output_semantics = 1;
    pixel.input_semantics = &input;
    pixel.num_input_semantics = 1;
    Require(sceAgcUpdateInterpolantMapping_10000(sdkMapping.data(), &vertex, &pixel) == 0);
    Require(sdkMapping[0].offset == SPI_PS_INPUT_CNTL_0 && sdkMapping[0].value == (5 | 0x400));
    for (unsigned i = 1; i < sdkMapping.size(); ++i)
        Require(sdkMapping[i].offset == 0xdeadbeef && sdkMapping[i].value == 0xcafebabe);

    input.semantic = 10;
    input.default_value = 2;
    Require(sceAgcUpdateInterpolantMapping_10000(sdkMapping.data(), &vertex, &pixel) == 0);
    Require(sdkMapping[0].value == 0x220);

    // Without a PS (ANIMAL WELL's first pipeline links a GS alone) every interpolant is written, disabled.
    for (const Shader* gs : {static_cast<const Shader*>(nullptr), static_cast<const Shader*>(&vertex)}) {
        sdkMapping.fill({0xdeadbeef, 0xcafebabe});
        Require(sceAgcUpdateInterpolantMapping_10000(sdkMapping.data(), gs, nullptr) == 0);
        for (unsigned i = 0; i < 32; ++i)
            Require(sdkMapping[i].offset == SPI_PS_INPUT_CNTL_0 + i && sdkMapping[i].value == 0);
        Require(sdkMapping.back().offset == 0xdeadbeef && sdkMapping.back().value == 0xcafebabe);
    }

    const auto rejects = [&](const Shader* gs) {
        sdkMapping.fill({0xdeadbeef, 0xcafebabe});
        const auto saved = sdkMapping;
        bool thrown = false;
        try { sceAgcUpdateInterpolantMapping_10000(sdkMapping.data(), gs, &pixel); }
        catch (const std::runtime_error&) { thrown = true; }
        Require(thrown && std::memcmp(sdkMapping.data(), saved.data(), sizeof(sdkMapping)) == 0);
    };
    rejects(nullptr);
    pixel.num_input_semantics = 33;
    rejects(&vertex);
}
