#include "prx/libSceAgc/Shader/include/InterpolantMapping.hpp"

#include <cstdio>
#include <stdexcept>
#include <prx/libc/include/General.hpp>

#include "SceShaders.hpp"
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

int CreateInterpolantMapping(const char* fn, ShaderRegister* regs, const Shader* gs, const Shader* ps, CreateInterpolantValueFn createValue) {
    if (regs == nullptr) {
        throw std::runtime_error(std::string(fn) + ": regs is null");
    }

    if (ps == nullptr || ps->num_input_semantics == 0) {
        FillIdentityInterpolants(regs, 0);
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

    FillIdentityInterpolants(regs, ps->num_input_semantics);
    return 0;
}

}

extern "C" {

APS5_EXPORT("HV4j+E0MBHE", sceAgcCreateInterpolantMapping);
int APS5_VABI sceAgcCreateInterpolantMapping(ShaderRegister* regs, const Shader* gs, const Shader* ps) {
    return CreateInterpolantMapping(__func__, regs, gs, ps, CreateInterpolantValue);
}

APS5_EXPORT("dbOlWdppb4o", sceAgcUnknownCreateInterpolantMapping);
int APS5_VABI sceAgcUnknownCreateInterpolantMapping(ShaderRegister* regs, const Shader* gs, const Shader* ps) {
    return CreateInterpolantMapping(__func__, regs, gs, ps, CreateInterpolantValueSplitF16);
}

}
