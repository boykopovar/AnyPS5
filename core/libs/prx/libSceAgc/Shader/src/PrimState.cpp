#include "prx/libSceAgc/Shader/include/PrimState.hpp"

#include <cstdio>
#include <stdexcept>
#include <prx/libc/include/General.hpp>

#include "SceShaders.hpp"
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

    if (cx_regs != nullptr) {
        cx_regs[0] = gs->specials->vgt_shader_stages_en;
        if ((cx_regs[0].value & ShaderRegs::VGT_SHADER_STAGES_GS_BIT) != 0) {
            cx_regs[1] = gs->specials->vgt_gs_out_prim_type;
        } else {
            cx_regs[1].offset = ShaderRegs::VGT_GS_OUT_PRIM_TYPE;
            cx_regs[1].value = GraphicsPrimTypeToGsOut(prim_type);
        }

        if (hs != nullptr) {
            cx_regs[0].value |= hs->specials->vgt_shader_stages_en.value;
            if ((cx_regs[0].value & ShaderRegs::VGT_SHADER_STAGES_GS_BIT) == 0) {
                cx_regs[1] = hs->specials->vgt_gs_out_prim_type;
            }
        }
    }

    if (uc_regs != nullptr) {
        uc_regs[0] = gs->specials->ge_cntl;
        uc_regs[1] = gs->specials->ge_user_vgpr_en;
        uc_regs[2].offset = ShaderRegs::VGT_PRIMITIVE_TYPE;
        uc_regs[2].value = prim_type;

        if (hs != nullptr) {
            uc_regs[1] = hs->specials->ge_user_vgpr_en;
        }
    }

    APS5_LOG_OUT("OK prim_type=%u", prim_type);
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

    APS5_LOG_OUT("OK prim_type=%u", prim_type);
    return 0;
}

}
