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

    Shader gsShader{};
    Shader hsShader{};
    if (!GuestCopyIn_nid_no_patch(&gsShader, gs, sizeof(gsShader)) || (hs != nullptr && !GuestCopyIn_nid_no_patch(&hsShader, hs, sizeof(hsShader)))) {
        throw std::runtime_error(std::string(__func__) + ": gs or hs is not readable");
    }
    if (gsShader.type != static_cast<std::uint8_t>(ShaderRegs::ShaderBinaryType::Gs) || gsShader.specials == nullptr || (hs != nullptr && (hsShader.type != static_cast<std::uint8_t>(ShaderRegs::ShaderBinaryType::Hs) || hsShader.specials == nullptr))) {
        throw std::runtime_error(std::string(__func__) + ": invalid shader type or missing special registers");
    }
    ShaderSpecialRegs gsSpecials{};
    ShaderSpecialRegs hsSpecials{};
    if (!GuestCopyIn_nid_no_patch(&gsSpecials, gsShader.specials, sizeof(gsSpecials)) || (hs != nullptr && !GuestCopyIn_nid_no_patch(&hsSpecials, hsShader.specials, sizeof(hsSpecials)))) {
        throw std::runtime_error(std::string(__func__) + ": special registers are not readable");
    }
    (void)GraphicsPrimTypeToGsOut(prim_type);
    RequireSpecialOffset(gsSpecials.vgt_shader_stages_en, ShaderRegs::VGT_SHADER_STAGES_EN, "VGT_SHADER_STAGES_EN");
    if (hs != nullptr) RequireSpecialOffset(hsSpecials.vgt_shader_stages_en, ShaderRegs::VGT_SHADER_STAGES_EN, "VGT_SHADER_STAGES_EN");
    RequireSpecialOffset(gsSpecials.ge_cntl, ShaderRegs::GE_CNTL, "GE_CNTL");
    std::array<ShaderRegister, 2> contextValues{gsSpecials.vgt_shader_stages_en, gsSpecials.vgt_gs_out_prim_type};
    if (hs != nullptr) {
        contextValues[0].value |= hsSpecials.vgt_shader_stages_en.value;
    }
    if ((contextValues[0].value & ShaderRegs::VGT_SHADER_STAGES_GS_BIT) != 0) {
        RequireSpecialOffset(gsSpecials.vgt_gs_out_prim_type, ShaderRegs::VGT_GS_OUT_PRIM_TYPE, "VGT_GS_OUT_PRIM_TYPE");
    } else if (hs != nullptr) {
        RequireSpecialOffset(hsSpecials.vgt_gs_out_prim_type, ShaderRegs::VGT_GS_OUT_PRIM_TYPE, "VGT_GS_OUT_PRIM_TYPE");
        contextValues[1] = hsSpecials.vgt_gs_out_prim_type;
    } else {
        contextValues[1] = {ShaderRegs::VGT_GS_OUT_PRIM_TYPE, GraphicsPrimTypeToGsOut(prim_type)};
    }
    std::array<ShaderRegister, 3> primitiveValues{gsSpecials.ge_cntl, hs != nullptr ? hsSpecials.ge_user_vgpr_en : gsSpecials.ge_user_vgpr_en, ShaderRegister{ShaderRegs::VGT_PRIMITIVE_TYPE, prim_type}};
    const std::array<const Shader*, 2> stages{hs, gs};
    const bool patch = prim_type == static_cast<std::uint32_t>(ShaderRegs::PrimitiveType::Patch);
    if (patch == ((contextValues[0].value & ShaderRegs::VGT_SHADER_STAGES_HS_BIT) != 0)) {
        AgcDriver::ShaderPreparationScope transaction({hs, gs});
        AgcDriverResolveGraphicsStagesAbi_nid_postfix(std::span(stages).subspan(hs == nullptr ? 1u : 0u), contextValues, primitiveValues);
        transaction.Commit();
    }
    if (cx_regs != nullptr && !GuestCopyOut_nid_no_patch(cx_regs, contextValues.data(), sizeof(contextValues))) {
        throw std::runtime_error(std::string(__func__) + ": cx_regs is not writable");
    }
    if (uc_regs != nullptr && !GuestCopyOut_nid_no_patch(uc_regs, primitiveValues.data(), sizeof(primitiveValues))) {
        throw std::runtime_error(std::string(__func__) + ": uc_regs is not writable");
    }
    return 0;
}

int APS5_VABI sceAgcUpdatePrimState(ShaderRegister* cx_regs, ShaderRegister* uc_regs, std::uint32_t prim_type) {
    if (cx_regs != nullptr) {
        std::array<ShaderRegister, 2> context{};
        if (!GuestCopyIn_nid_no_patch(context.data(), cx_regs, sizeof(context))) {
            throw std::runtime_error(std::string(__func__) + ": cx_regs is not readable");
        }
        if ((context[0].value & (ShaderRegs::VGT_SHADER_STAGES_GS_BIT | ShaderRegs::VGT_SHADER_STAGES_HS_BIT)) == 0) {
            context[1].value &= ~0x7u;
            context[1].value |= GraphicsPrimTypeToGsOut(prim_type);
            if (!GuestCopyOut_nid_no_patch(&cx_regs[1].value, &context[1].value, sizeof(context[1].value))) {
                throw std::runtime_error(std::string(__func__) + ": cx_regs is not writable");
            }
        }
    }

    if (uc_regs != nullptr) {
        std::uint32_t value = 0;
        if (!GuestCopyIn_nid_no_patch(&value, &uc_regs[2].value, sizeof(value))) {
            throw std::runtime_error(std::string(__func__) + ": uc_regs is not readable");
        }
        value &= ~0x1Fu;
        value |= prim_type;
        if (!GuestCopyOut_nid_no_patch(&uc_regs[2].value, &value, sizeof(value))) {
            throw std::runtime_error(std::string(__func__) + ": uc_regs is not writable");
        }
    }

    return 0;
}

}
