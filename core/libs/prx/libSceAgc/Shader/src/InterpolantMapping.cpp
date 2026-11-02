#include "prx/libSceAgc/Shader/include/InterpolantMapping.hpp"

#include <cstdio>
#include <array>
#include <algorithm>
#include <stdexcept>
#include <prx/libc/include/General.hpp>

#include "SceShaders.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderPreparationScope.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgc/Shader/include/ShaderUtils.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"

namespace {

constexpr std::uint32_t InterpolantRegisterCount = 32;
constexpr std::uint32_t InterpolantF16HiOnly = 2;

using CreateInterpolantValueFn = std::uint32_t (*)(std::uint32_t psWord, const ShaderSemantic* gsSemantic);

std::uint32_t CreateInterpolantValue(std::uint32_t psWord, const ShaderSemantic* gsSemantic) {
    const std::uint32_t value = ((psWord & 0x00300000u) != 0)
        ? CreateInterpolantF16Value(psWord, gsSemantic)
        : CreateInterpolantNonF16Value(psWord, gsSemantic);

    return (gsSemantic == nullptr)
        ? CreateInterpolantDefaultValue(value, psWord)
        : CreateInterpolantMappingValue(value, psWord, ShaderSemanticWord(*gsSemantic));
}

std::uint32_t CreateInterpolantF16HiValue(std::uint32_t psWord, const ShaderSemantic* gsSemantic) {
    std::uint32_t value = ((psWord << 4u) & 0x03000000u) | 0x00080000u;
    if (gsSemantic == nullptr || (psWord & ShaderSemanticWord(*gsSemantic) & 0x00200000u) == 0) {
        value |= 0x00000020u;
    }
    value = ApplyInterpolantDefaultValueHi(value, psWord);
    value |= ((psWord >> 30u) & 0x3u) << 8u;
    if (gsSemantic == nullptr) {
        return value;
    }

    value |= (ShaderSemanticWord(*gsSemantic) >> 8u) & 0x1Fu;
    if ((psWord & 0x01400000u) != 0) {
        value |= 0x00000400u;
    }
    return value;
}

std::uint32_t CreateInterpolantValueSplitF16(std::uint32_t psWord, const ShaderSemantic* gsSemantic) {
    return (((psWord >> 20u) & 0x3u) == InterpolantF16HiOnly)
        ? CreateInterpolantF16HiValue(psWord, gsSemantic)
        : CreateInterpolantValue(psWord, gsSemantic);
}

std::uint32_t CreateInterpolantValueFlatDefault(std::uint32_t psWord, const ShaderSemantic* gsSemantic) {
    const std::uint32_t value = CreateInterpolantValue(psWord, gsSemantic);
    return ((psWord & 0x00700000u) == 0x00400000u) ? (value | 0x00000020u) : value;
}

int CreateInterpolantMapping(const char* fn, ShaderRegister* regs, const Shader* gs, const Shader* ps, CreateInterpolantValueFn createValue, bool fillIdentityTail) {
    if (regs == nullptr) {
        throw std::runtime_error(std::string(fn) + ": regs is null");
    }
    if (ps == nullptr && !fillIdentityTail) {
        throw std::runtime_error(std::string(fn) + ": ps is null");
    }
    if (ps != nullptr && ps->num_input_semantics > 32) {
        throw std::runtime_error(std::string(fn) + ": input semantic count exceeds 32");
    }

    AgcDriver::ShaderPreparationScope transaction({ps != nullptr ? gs : nullptr, ps});
    std::array<ShaderRegister, 32> values{};
    auto* output = regs;
    regs = values.data();
    if (ps == nullptr || ps->num_input_semantics == 0) {
        const std::uint32_t count = fillIdentityTail ? InterpolantRegisterCount : 0;
        if (fillIdentityTail) FillIdentityInterpolants(regs, 0);
        if (ps != nullptr) {
            AgcDriverResolveShaderAbi_nid_postfix(ps, {regs, count}, {});
            if (gs != nullptr) AgcDriverResolveGraphicsAbi_nid_postfix(gs, ps, 0);
        }
        transaction.Commit();
        std::copy_n(values.begin(), count, output);
        return 0;
    }

    if (ps->num_input_semantics != 0 && ps->input_semantics == nullptr) {
        throw std::runtime_error(std::string(fn) + ": ps->input_semantics is null but num_input_semantics != 0");
    }

    if (ps->num_input_semantics > InterpolantRegisterCount) {
        throw std::runtime_error(std::string(fn) + ": ps->num_input_semantics exceeds the interpolant register count");
    }

    if (gs == nullptr) {
        throw std::runtime_error(std::string(fn) + ": gs is null");
    }

    if (gs->num_output_semantics != 0 && gs->output_semantics == nullptr) {
        throw std::runtime_error(std::string(fn) + ": gs->output_semantics is null but num_output_semantics != 0");
    }

    for (std::uint32_t i = 0; i < ps->num_input_semantics; ++i) {
        const ShaderSemantic& psSemantic = ps->input_semantics[i];
        const ShaderSemantic* gsSemantic = FindOutputSemantic(gs, psSemantic.semantic);
        SetInterpolantRegister(regs, i, createValue(ShaderSemanticWord(psSemantic), gsSemantic));
    }

    const std::uint32_t count = fillIdentityTail ? InterpolantRegisterCount : ps->num_input_semantics;
    if (fillIdentityTail) FillIdentityInterpolants(regs, ps->num_input_semantics);
    AgcDriverResolveShaderAbi_nid_postfix(ps, {regs, count}, {});
    AgcDriverResolveGraphicsAbi_nid_postfix(gs, ps, 0);
    transaction.Commit();
    std::copy_n(values.begin(), count, output);
    return 0;
}

}

extern "C" {

int APS5_VABI sceAgcCreateInterpolantMapping_0100(ShaderRegister* regs, const Shader* gs, const Shader* ps) {
    return CreateInterpolantMapping(__func__, regs, gs, ps, CreateInterpolantValue, true);
}

int APS5_VABI sceAgcCreateInterpolantMapping_10000(ShaderRegister* regs, const Shader* gs, const Shader* ps) {
    return CreateInterpolantMapping(__func__, regs, gs, ps, CreateInterpolantValueSplitF16, true);
}

int APS5_VABI sceAgcUpdateInterpolantMapping_10000(ShaderRegister* regs, const Shader* gs, const Shader* ps) {
    return CreateInterpolantMapping(__func__, regs, gs, ps, CreateInterpolantValueSplitF16, false);
}

int APS5_VABI sceAgcCreateInterpolantMapping_nid_postfix(ShaderRegister* regs, const Shader* gs, const Shader* ps) {
    return CreateInterpolantMapping(__func__, regs, gs, ps, CreateInterpolantValueFlatDefault, true);
}

int APS5_VABI sceAgcUpdateInterpolantMapping_nid_postfix(ShaderRegister* regs, const Shader* gs, const Shader* ps) {
    return CreateInterpolantMapping(__func__, regs, gs, ps, CreateInterpolantValueFlatDefault, false);
}

}
