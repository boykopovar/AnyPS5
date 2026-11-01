#include "prx/libSceAgc/Shader/include/PrimState.hpp"

#include <cstdio>
#include <array>
#include <algorithm>
#include <stdexcept>
#include <prx/libc/include/General.hpp>

#include "SceShaders.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderPreparationScope.hpp"
#include "prx/libSceAgc/Shader/include/ShaderUtils.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"

namespace {

void RequireSpecialOffset(const ShaderRegister& reg, std::uint32_t expected, const char* name) {
    if (reg.offset != expected) {
        char message[160];
        std::snprintf(message, sizeof(message), "sceAgcCreatePrimState: %s special register at offset 0x%x, expected 0x%x", name, static_cast<unsigned>(reg.offset), static_cast<unsigned>(expected));
        throw std::runtime_error(message);
    }
}

}

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
    RequireSpecialOffset(gs->specials->vgt_shader_stages_en, ShaderRegs::VGT_SHADER_STAGES_EN, "VGT_SHADER_STAGES_EN");
    if (hs != nullptr) RequireSpecialOffset(hs->specials->vgt_shader_stages_en, ShaderRegs::VGT_SHADER_STAGES_EN, "VGT_SHADER_STAGES_EN");
    RequireSpecialOffset(gs->specials->ge_cntl, ShaderRegs::GE_CNTL, "GE_CNTL");
    std::array<ShaderRegister, 2> contextValues{gs->specials->vgt_shader_stages_en, gs->specials->vgt_gs_out_prim_type};
    if (hs != nullptr) {
        contextValues[0].value |= hs->specials->vgt_shader_stages_en.value;
    }
    if ((contextValues[0].value & ShaderRegs::VGT_SHADER_STAGES_GS_BIT) != 0) {
        RequireSpecialOffset(gs->specials->vgt_gs_out_prim_type, ShaderRegs::VGT_GS_OUT_PRIM_TYPE, "VGT_GS_OUT_PRIM_TYPE");
    } else if (hs != nullptr) {
        RequireSpecialOffset(hs->specials->vgt_gs_out_prim_type, ShaderRegs::VGT_GS_OUT_PRIM_TYPE, "VGT_GS_OUT_PRIM_TYPE");
        contextValues[1] = hs->specials->vgt_gs_out_prim_type;
    } else {
        contextValues[1] = {ShaderRegs::VGT_GS_OUT_PRIM_TYPE, GraphicsPrimTypeToGsOut(prim_type)};
    }
    std::array<ShaderRegister, 3> primitiveValues{gs->specials->ge_cntl, hs != nullptr ? hs->specials->ge_user_vgpr_en : gs->specials->ge_user_vgpr_en, ShaderRegister{ShaderRegs::VGT_PRIMITIVE_TYPE, prim_type}};
    const std::array<const Shader*, 2> stages{hs, gs};
    const bool patch = prim_type == static_cast<std::uint32_t>(ShaderRegs::PrimitiveType::Patch);
    if (patch == ((contextValues[0].value & ShaderRegs::VGT_SHADER_STAGES_HS_BIT) != 0)) {
        AgcDriver::ShaderPreparationScope transaction({hs, gs});
        AgcDriverResolveGraphicsStagesAbi_nid_postfix(std::span(stages).subspan(hs == nullptr ? 1u : 0u), contextValues, primitiveValues);
        transaction.Commit();
    }
    if (cx_regs != nullptr) std::copy(contextValues.begin(), contextValues.end(), cx_regs);
    if (uc_regs != nullptr) std::copy(primitiveValues.begin(), primitiveValues.end(), uc_regs);
    return 0;
}

int APS5_VABI sceAgcUpdatePrimState(ShaderRegister* cx_regs, ShaderRegister* uc_regs, std::uint32_t prim_type) {
    if (cx_regs != nullptr && (cx_regs[0].value & (ShaderRegs::VGT_SHADER_STAGES_GS_BIT | ShaderRegs::VGT_SHADER_STAGES_HS_BIT)) == 0) {
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
