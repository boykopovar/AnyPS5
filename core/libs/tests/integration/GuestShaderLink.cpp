#include "SceShaders.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"

#include <Testing/Test.hpp>

#include <array>
#include <cstring>
#include <stdexcept>
#include <span>
#include <string>

extern "C" int APS5_VABI sceAgcLinkShaders(ShaderRegister*, ShaderRegister*, const void*, const Shader*, const Shader*, std::uint32_t);
extern "C" void* APS5_VABI sceAgcGetRegisterDefaults();
extern "C" void* APS5_VABI sceAgcGetRegisterDefaults2(std::uint32_t);

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

unsigned preparations = 0;
unsigned mappings = 0;
unsigned links = 0;
const Shader* mappedPixel = nullptr;

struct HookCalls {
    unsigned preparations;
    unsigned mappings;
    unsigned links;
};

class LinkFixture {
public:
    LinkFixture() {
        using namespace ShaderRegs;
        preparations = mappings = links = 0;
        mappedPixel = nullptr;
        special.vgt_shader_stages_en = {VGT_SHADER_STAGES_EN, 0x2000u};
        special.vgt_gs_out_prim_type = {VGT_GS_OUT_PRIM_TYPE, 0};
        special.ge_cntl = {GE_CNTL, 0x123};
        special.ge_user_vgpr_en = {GE_USER_VGPR_EN, 7};
        vertex.type = static_cast<std::uint8_t>(ShaderBinaryType::Gs);
        vertex.specials = &special;
        pixel.type = static_cast<std::uint8_t>(ShaderBinaryType::Ps);
        context.back() = {0xdeadbeef, 0xcafebabe};
        primitive.back() = context.back();
    }

    ~LinkFixture() {
        mappedPixel = nullptr;
    }

    LinkFixture(const LinkFixture&) = delete;
    LinkFixture& operator=(const LinkFixture&) = delete;

    int Link(const Shader* pixelShader, std::uint32_t primitiveType) {
        return sceAgcLinkShaders(context.data(), primitive.data(), nullptr, &vertex, pixelShader, primitiveType);
    }

    void RequireHookCalls(HookCalls expected) const {
        RequireEqual(preparations, expected.preparations, "stage preparation calls");
        RequireEqual(mappings, expected.mappings, "interpolant mapping calls");
        RequireEqual(links, expected.links, "link calls");
    }

    ShaderSpecialRegs special{};
    Shader vertex{};
    Shader pixel{};
    std::array<ShaderRegister, 35> context{};
    std::array<ShaderRegister, 4> primitive{};
};

const Case registerDefaults{"GetRegisterDefaults_PublicTable_IsStableAndHoldsContextRegisters", [] {
    auto* defaults = static_cast<unsigned char*>(sceAgcGetRegisterDefaults());
    Require(defaults != nullptr && defaults == sceAgcGetRegisterDefaults(), "stable defaults pointer");
    Require(defaults == sceAgcGetRegisterDefaults2(0), "version 0 defaults are the public table");
    ShaderRegister** contextBlocks = nullptr;
    std::uint32_t contextCount = 0;
    std::memcpy(&contextBlocks, defaults, sizeof(contextBlocks));
    std::memcpy(&contextCount, defaults + 0x20, sizeof(contextCount));
    Require(contextBlocks != nullptr && contextBlocks[0] != nullptr, "context register block at native offset 0");
    RequireEqual(contextCount, 523u, "context register count at native offset 0x20");
    bool hasRenderTarget = false;
    bool hasRasterizer = false;
    for (std::uint32_t i = 0; i < contextCount; ++i) {
        hasRenderTarget |= contextBlocks[0][i].offset == 0x318;
        hasRasterizer |= contextBlocks[0][i].offset == 0x205;
    }
    Require(hasRenderTarget, "defaults hold a render target register");
    Require(hasRasterizer, "defaults hold a rasterizer register");
}};

const Case linkGeometryPixel{"LinkShaders_GeometryAndPixel_WritesStagesInterpolantsAndPrimitive", [] {
    using namespace ShaderRegs;
    LinkFixture fixture;
    RequireEqual(fixture.Link(&fixture.pixel, 4), 0, "link");
    Require(fixture.context[0].offset == VGT_SHADER_STAGES_EN && fixture.context[0].value == 0x2000u, "stages register");
    Require(fixture.context[1].offset == VGT_GS_OUT_PRIM_TYPE && fixture.context[1].value == 2, "geometry output type");
    for (unsigned i = 0; i < 32; ++i) {
        Require(fixture.context[i + 2].offset == SPI_PS_INPUT_CNTL_0 + i && fixture.context[i + 2].value == i,
                "identity interpolant " + std::to_string(i));
    }
    Require(fixture.primitive[0].offset == GE_CNTL && fixture.primitive[0].value == 0x123, "GE_CNTL");
    Require(fixture.primitive[1].offset == GE_USER_VGPR_EN && fixture.primitive[1].value == 7, "GE_USER_VGPR_EN");
    Require(fixture.primitive[2].offset == VGT_PRIMITIVE_TYPE && fixture.primitive[2].value == 4, "primitive type");
    Require(fixture.context.back().value == 0xcafebabe && fixture.primitive.back().value == 0xcafebabe, "no overrun");
    fixture.RequireHookCalls({1u, 1u, 2u});
}};

const Case tooManyInputs{"LinkShaders_TooManyPixelInputs_FailsInvalidProgramWithoutWriting", [] {
    LinkFixture fixture;
    const auto savedContext = fixture.context;
    const auto savedPrimitive = fixture.primitive;
    fixture.pixel.num_input_semantics = 33;
    RequireEqual(fixture.Link(&fixture.pixel, 4), ShaderRegs::GRAPHICS5_ERROR_INVALID_SHADER_PROGRAM, "33 pixel inputs");
    Require(std::memcmp(fixture.context.data(), savedContext.data(), sizeof(fixture.context)) == 0, "context untouched");
    Require(std::memcmp(fixture.primitive.data(), savedPrimitive.data(), sizeof(fixture.primitive)) == 0, "primitive untouched");
    fixture.RequireHookCalls({0u, 0u, 0u});
}};

const Case missingGeCntl{"LinkShaders_MissingGeCntlRegister_ThrowsWithoutWriting", [] {
    LinkFixture fixture;
    const auto savedContext = fixture.context;
    fixture.special.ge_cntl.offset = 0;
    Testing::RequireThrows<std::runtime_error>([&] { fixture.Link(&fixture.pixel, 4); }, "link without GE_CNTL");
    Require(std::memcmp(fixture.context.data(), savedContext.data(), sizeof(fixture.context)) == 0, "context untouched");
    fixture.RequireHookCalls({0u, 0u, 0u});
}};

const Case noPixel{"LinkShaders_NoPixelShader_WritesLineOutputForPrimitiveType2", [] {
    LinkFixture fixture;
    RequireEqual(fixture.Link(nullptr, 2), 0, "link without a pixel shader");
    RequireEqual(fixture.context[1].value, 1u, "geometry output type");
    RequireEqual(fixture.primitive[2].value, 2u, "primitive type");
    fixture.RequireHookCalls({1u, 0u, 1u});
}};

const Case matchedSemantic{"LinkShaders_MatchingFlatSemantic_MapsToGeometryOutput", [] {
    using namespace ShaderRegs;
    LinkFixture fixture;
    ShaderSemantic output{};
    output.semantic = 9;
    output.hardware_mapping = 5;
    fixture.vertex.output_semantics = &output;
    fixture.vertex.num_output_semantics = 1;
    ShaderSemantic input{};
    input.semantic = 9;
    input.is_flat_shaded = 1;
    fixture.pixel.input_semantics = &input;
    fixture.pixel.num_input_semantics = 1;
    RequireEqual(fixture.Link(&fixture.pixel, 4), 0, "link");
    RequireEqual(fixture.context[2].offset, SPI_PS_INPUT_CNTL_0, "interpolant register");
    RequireEqual(fixture.context[2].value, 5u | 0x400u, "flat interpolant mapped to output 5");
    fixture.RequireHookCalls({1u, 1u, 2u});
}};

const Case unmatchedSemantic{"LinkShaders_UnmatchedSemantic_UsesDefaultValue", [] {
    LinkFixture fixture;
    ShaderSemantic output{};
    output.semantic = 9;
    output.hardware_mapping = 5;
    fixture.vertex.output_semantics = &output;
    fixture.vertex.num_output_semantics = 1;
    ShaderSemantic input{};
    input.semantic = 10;
    input.is_flat_shaded = 1;
    input.default_value = 2;
    fixture.pixel.input_semantics = &input;
    fixture.pixel.num_input_semantics = 1;
    RequireEqual(fixture.Link(&fixture.pixel, 4), 0, "link");
    RequireEqual(fixture.context[2].value, 0x220u, "default value interpolant");
    fixture.RequireHookCalls({1u, 1u, 2u});
}};

} // namespace

extern "C" void AgcDriverResolveGraphicsStagesAbi_nid_postfix(std::span<const Shader* const> stages, std::span<const ShaderRegister> context, std::span<const ShaderRegister> primitive) {
    Require(!stages.empty() && stages[0] != nullptr && context.size() == 2u && primitive.size() == 3u, "stage preparation arguments");
    ++preparations;
}

extern "C" void AgcDriverResolveShaderAbi_nid_postfix(const Shader* shader, std::span<const ShaderRegister> context, std::span<const ShaderRegister> primitive) {
    Require(shader != nullptr && context.size() == 32u && primitive.empty(), "mapping preparation arguments");
    mappedPixel = shader;
    ++mappings;
}

extern "C" void AgcDriverResolveGraphicsAbi_nid_postfix(const Shader* vertex, const Shader* pixel, std::uint32_t primitiveType) {
    Require(vertex != nullptr && (pixel == nullptr || pixel == mappedPixel) && (primitiveType == 0u || primitiveType == 2u || primitiveType == 4u),
            "link arguments");
    ++links;
}
