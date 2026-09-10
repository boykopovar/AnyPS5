#include <cstdio>
#include <stdexcept>
#include <prx/libc/include/General.hpp>

#include "SceShaders.hpp"
#include "ShaderUtils.hpp"
#include "ShaderConstants.hpp"

extern "C" {

int APS5_VABI sceAgcCreateShader(Shader** dst, void* header, const volatile void* code) {
    constexpr auto fn = __func__;
    if (dst == nullptr) {
        throw std::runtime_error(std::string(fn) + ": dst is null");
    }
    if (header == nullptr) {
        throw std::runtime_error(std::string(fn) + ": header is null");
    }
    if (code == nullptr) {
        throw std::runtime_error(std::string(fn) + ": code is null");
    }

    auto* h = static_cast<Shader*>(header);

    ResolveRelativePtr(h->cx_registers);
    ResolveRelativePtr(h->sh_registers);
    ResolveRelativePtr(h->user_data);
    ResolveRelativePtr(h->specials);
    ResolveRelativePtr(h->input_semantics);
    ResolveRelativePtr(h->output_semantics);

    if (h->user_data != nullptr) {
        ResolveRelativePtr(h->user_data->direct_resource_offset);
        ResolveRelativePtr(h->user_data->sharp_resource_offset[0]);
        ResolveRelativePtr(h->user_data->sharp_resource_offset[1]);
        ResolveRelativePtr(h->user_data->sharp_resource_offset[2]);
        ResolveRelativePtr(h->user_data->sharp_resource_offset[3]);
    }

    h->code = code;

    if (h->file_header != ShaderRegs::SHADER_FILE_HEADER_MAGIC) {
        throw std::runtime_error(std::string(fn) + ": invalid file_header magic");
    }
    if (h->version != ShaderRegs::SHADER_VERSION) {
        throw std::runtime_error(std::string(fn) + ": unsupported shader version");
    }

    const auto base = reinterpret_cast<std::uint64_t>(code);

    if ((base & ShaderRegs::SHADER_BASE_ALIGN_MASK) != 0) {
        throw std::runtime_error(std::string(fn) + ": code address violates alignment mask");
    }

    int result = PatchProgramAddressRegister(h->sh_registers, h->num_sh_registers, h->type, base);
    if (result != 0) {
        return result;
    }

    *dst = h;
    APS5_LOG_OUT("OK type=%u sh_regs=%u shader_size=%u", h->type, h->num_sh_registers, h->shader_size);
    return 0;
}

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
