#include "prx/libSceAgc/Shader/include/PrimState.hpp"

#include <cstdio>
#include <array>
#include <algorithm>
#include <stdexcept>
#include <prx/libc/include/General.hpp>

#include "SceShaders.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgc/Shader/include/ShaderUtils.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"

extern "C" {

int APS5_VABI sceAgcCreatePrimState(ShaderRegister* cx_regs, ShaderRegister* uc_regs, const Shader* hs, const Shader* gs, std::uint32_t prim_type) {
    if (cx_regs == nullptr && uc_regs == nullptr) {
        return 0;
    }
    if (gs == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": gs is null");
    }

    if (gs->type != static_cast<std::uint8_t>(ShaderRegs::ShaderBinaryType::Gs) || gs->specials == nullptr || (hs != nullptr && (hs->type != static_cast<std::uint8_t>(ShaderRegs::ShaderBinaryType::Hs) || hs->specials == nullptr))) {
        throw std::runtime_error(std::string(__func__) + ": invalid shader type or missing special registers");
    }
    (void)GraphicsPrimTypeToGsOut(prim_type);
    const auto validStages = [](const Shader* shader) { return shader->specials->vgt_shader_stages_en.offset == ShaderRegs::VGT_SHADER_STAGES_EN; };
    if (!validStages(gs) || (hs != nullptr && !validStages(hs))) {
        throw std::runtime_error(std::string(__func__) + ": invalid context register offsets");
    }
    if (gs->specials->ge_cntl.offset != ShaderRegs::GE_CNTL || gs->specials->ge_user_vgpr_en.offset != ShaderRegs::GE_USER_VGPR_EN || (hs != nullptr && hs->specials->ge_user_vgpr_en.offset != ShaderRegs::GE_USER_VGPR_EN)) {
        throw std::runtime_error(std::string(__func__) + ": invalid user configuration register offsets");
    }
    std::array<ShaderRegister, 2> contextValues{gs->specials->vgt_shader_stages_en, gs->specials->vgt_gs_out_prim_type};
    if (hs != nullptr) {
        contextValues[0].value |= hs->specials->vgt_shader_stages_en.value;
    }
    if ((contextValues[0].value & ShaderRegs::VGT_SHADER_STAGES_GS_BIT) != 0) {
        if (gs->specials->vgt_gs_out_prim_type.offset != ShaderRegs::VGT_GS_OUT_PRIM_TYPE) throw std::runtime_error(std::string(__func__) + ": invalid context register offsets");
    } else if (hs != nullptr) {
        if (hs->specials->vgt_gs_out_prim_type.offset != ShaderRegs::VGT_GS_OUT_PRIM_TYPE) throw std::runtime_error(std::string(__func__) + ": invalid context register offsets");
        contextValues[1] = hs->specials->vgt_gs_out_prim_type;
    } else {
        contextValues[1] = {ShaderRegs::VGT_GS_OUT_PRIM_TYPE, GraphicsPrimTypeToGsOut(prim_type)};
    }
    std::array<ShaderRegister, 3> primitiveValues{gs->specials->ge_cntl, hs != nullptr ? hs->specials->ge_user_vgpr_en : gs->specials->ge_user_vgpr_en, ShaderRegister{ShaderRegs::VGT_PRIMITIVE_TYPE, prim_type}};
    const std::array<const Shader*, 2> stages{hs, gs};
    AgcDriverResolveGraphicsStagesAbi_nid_postfix(std::span(stages).subspan(hs == nullptr ? 1u : 0u), contextValues, primitiveValues);
    if (cx_regs != nullptr) std::copy(contextValues.begin(), contextValues.end(), cx_regs);
    if (uc_regs != nullptr) std::copy(primitiveValues.begin(), primitiveValues.end(), uc_regs);
    return 0;
}

int APS5_VABI sceAgcUpdatePrimState(ShaderRegister* cx_regs, ShaderRegister* uc_regs, std::uint32_t prim_type) {
    if (cx_regs != nullptr && (cx_regs[0].value & (ShaderRegs::VGT_SHADER_STAGES_GS_BIT | ShaderRegs::VGT_SHADER_STAGES_NGG_BIT)) == 0) {
        cx_regs[1].value &= ~0x7u;
        cx_regs[1].value |= GraphicsPrimTypeToGsOut(prim_type);
    }

    if (uc_regs != nullptr) {
        uc_regs[2].value &= ~0x1Fu;
        uc_regs[2].value |= prim_type;
    }

    return 0;
}

}
