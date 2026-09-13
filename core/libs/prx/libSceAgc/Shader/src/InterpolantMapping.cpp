#include "prx/libSceAgc/Shader/include/InterpolantMapping.hpp"

#include <cstdio>
#include <stdexcept>
#include <prx/libc/include/General.hpp>

#include "SceShaders.hpp"
#include "prx/libSceAgc/Shader/include/ShaderUtils.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"

extern "C" {

APS5_EXPORT("HV4j+E0MBHE", sceAgcCreateInterpolantMapping);
int APS5_VABI sceAgcCreateInterpolantMapping(ShaderRegister* regs, const Shader* gs, const Shader* ps) {
    constexpr auto fn = __func__;
    if (regs == nullptr) {
        throw std::runtime_error(std::string(fn) + ": regs is null");
    }

    if (ps == nullptr || ps->num_input_semantics == 0) {
        FillIdentityInterpolants(regs, 0);
        APS5_LOG_CHARS_OUT("OK identity ps_inputs=0");
        return 0;
    }

    if (ps->num_input_semantics != 0 && ps->input_semantics == nullptr) {
        throw std::runtime_error(std::string(fn) + ": ps->input_semantics is null but num_input_semantics != 0");
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
        const std::uint32_t psWord = ShaderSemanticWord(psSemantic);

        std::uint32_t value = ((psWord & 0x00300000u) != 0)
            ? CreateInterpolantF16Value(psWord, gsSemantic)
            : CreateInterpolantNonF16Value(psWord, gsSemantic);

        value = (gsSemantic == nullptr)
            ? CreateInterpolantDefaultValue(value, psWord)
            : CreateInterpolantMappingValue(value, psWord, ShaderSemanticWord(*gsSemantic));

        SetInterpolantRegister(regs, i, value);
    }

    FillIdentityInterpolants(regs, ps->num_input_semantics);
    APS5_LOG_OUT("OK ps_inputs=%u gs_outputs=%u", ps->num_input_semantics, gs->num_output_semantics);
    return 0;
}

}
