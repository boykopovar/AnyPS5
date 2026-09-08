#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include "SceShaders.hpp"
#include "prx/libc/include/General.hpp"
#include "ShaderConstants.hpp"

namespace {

using namespace ShaderRegs;

template <typename T>
static void _resolveRelativePtr(T*& field) {
    if (field == nullptr) {
        return;
    }
    field = reinterpret_cast<T*>(
        reinterpret_cast<std::uintptr_t>(field) + reinterpret_cast<std::uintptr_t>(&field)
    );
}

static bool _getProgramAddressRegisterOffset(std::uint8_t type, std::uint32_t& loOffset) {
    switch (static_cast<ShaderBinaryType>(type)) {
        case ShaderBinaryType::Cs: loOffset = COMPUTE_PGM_LO; return true;
        case ShaderBinaryType::Ps: loOffset = SPI_SHADER_PGM_LO_PS; return true;
        case ShaderBinaryType::Gs: loOffset = SPI_SHADER_PGM_LO_ES; return true;
        case ShaderBinaryType::Hs: loOffset = SPI_SHADER_PGM_LO_LS; return true;
        case ShaderBinaryType::GsBack: loOffset = SPI_SHADER_PGM_LO_GS; return true;
        case ShaderBinaryType::HsBack: loOffset = SPI_SHADER_PGM_LO_HS; return true;
        default: return false;
    }
}

static int _patchProgramAddressRegister(ShaderRegister* regs, std::uint32_t numRegs, std::uint8_t type, std::uint64_t base) {
    std::uint32_t loOffset = 0;
    if (!_getProgramAddressRegisterOffset(type, loOffset)) {
        return 0;
    }

    if (regs == nullptr || numRegs == 0) {
        throw std::runtime_error("sceAgcCreateShader: sh_registers missing for shader type requiring program address patch");
    }

    for (std::uint32_t i = 0; i < numRegs; ++i) {
        if (regs[i].offset != loOffset) {
            continue;
        }
        const std::uint32_t hiIndex = i + 1u;
        if (hiIndex >= numRegs || regs[hiIndex].offset != loOffset + 1u) {
            throw std::runtime_error("sceAgcCreateShader: shader program address hi register missing");
        }
        const std::uint64_t shaderOffset =
            (static_cast<std::uint64_t>(regs[i].value) << 8u) |
            ((static_cast<std::uint64_t>(regs[hiIndex].value) & 0xFFu) << 40u);
        const std::uint64_t addr = base + shaderOffset;
        regs[i].value = static_cast<std::uint32_t>((addr >> 8u) & 0xFFFFFFFFu);
        regs[hiIndex].value &= 0xFFFFFF00u;
        regs[hiIndex].value |= static_cast<std::uint32_t>((addr >> 40u) & 0xFFu);
        return 0;
    }

    throw std::runtime_error("sceAgcCreateShader: shader program address lo register not found");
}

static std::uint32_t _graphicsPrimTypeToGsOut(std::uint32_t primType) {
    switch (static_cast<PrimitiveType>(primType)) {
        case PrimitiveType::PointList:
            return static_cast<std::uint32_t>(GsOutputPrimitiveType::Points);
        case PrimitiveType::LineList:
        case PrimitiveType::LineStrip:
        case PrimitiveType::LineListAdjacency:
        case PrimitiveType::LineStripAdjacency:
        case PrimitiveType::LineLoop:
            return static_cast<std::uint32_t>(GsOutputPrimitiveType::Lines);
        case PrimitiveType::RectList:
            return static_cast<std::uint32_t>(GsOutputPrimitiveType::Rectangle2D);
        case PrimitiveType::RectListLegacy:
            return static_cast<std::uint32_t>(GsOutputPrimitiveType::RectList);
        default:
            return static_cast<std::uint32_t>(GsOutputPrimitiveType::Triangles);
    }
}

static std::uint32_t _shaderSemanticWord(const ShaderSemantic& s) {
    return ((s.semantic & 0xFFu) << 0u)
         | ((s.hardware_mapping & 0xFFu) << 8u)
         | ((s.size_in_elements & 0xFu) << 16u)
         | ((s.is_f16 & 0x3u) << 20u)
         | ((s.is_flat_shaded & 0x1u) << 22u)
         | ((s.is_linear & 0x1u) << 23u)
         | ((s.is_custom & 0x1u) << 24u)
         | ((s.static_vb_index & 0x1u) << 25u)
         | ((s.static_attribute & 0x1u) << 26u)
         | ((s.reserved & 0x1u) << 27u)
         | ((s.default_value & 0x3u) << 28u)
         | ((s.default_value_hi & 0x3u) << 30u);
}

static std::uint32_t _applyInterpolantDefaultValue(std::uint32_t value, std::uint32_t psWord) {
    value &= ~0x00000300u;
    value |= ((psWord >> 28u) & 0x3u) << 8u;
    return value;
}

static std::uint32_t _applyInterpolantDefaultValueHi(std::uint32_t value, std::uint32_t psWord) {
    value &= ~0x00600000u;
    value |= ((psWord >> 30u) & 0x3u) << 21u;
    return value;
}

static std::uint32_t _createInterpolantF16Value(std::uint32_t psWord, const ShaderSemantic* gsSemantic) {
    std::uint32_t value = (psWord << 4u) & 0x03000000u;
    if (gsSemantic == nullptr) {
        value |= 0x00180020u;
    } else {
        const std::uint32_t commonWord = psWord & _shaderSemanticWord(*gsSemantic);
        value &= 0xFFF7FFDFu;
        value |= (commonWord >> 15u) & 0x20u;
        value ^= 0x00080020u;
        value &= ~0x00100000u;
        value |= (~commonWord >> 1u) & 0x00100000u;
    }
    return _applyInterpolantDefaultValueHi(value, psWord);
}

static std::uint32_t _createInterpolantNonF16Value(std::uint32_t psWord, const ShaderSemantic* gsSemantic) {
    std::uint32_t value = 0;
    if ((psWord & 0x01000000u) != 0 || gsSemantic == nullptr) {
        value |= 0x20u;
    }
    return value;
}

static std::uint32_t _createInterpolantMappingValue(std::uint32_t value, std::uint32_t psWord, std::uint32_t gsWord) {
    const std::uint32_t flatShade =
        ((psWord & 0x00400000u) != 0 || (psWord & 0x01000000u) != 0) ? 0x00000400u : 0u;
    value &= ~0x0000001Fu;
    value |= (gsWord >> 8u) & 0x1Fu;
    value &= ~0x00000400u;
    value |= flatShade;
    return _applyInterpolantDefaultValue(value, psWord);
}

static std::uint32_t _createInterpolantDefaultValue(std::uint32_t value, std::uint32_t psWord) {
    value &= ~0x0000001Fu;
    value &= ~0x00000400u;
    return _applyInterpolantDefaultValue(value, psWord);
}

static const ShaderSemantic* _findOutputSemantic(const Shader* gs, std::uint32_t semantic) {
    if (gs == nullptr || gs->output_semantics == nullptr) {
        return nullptr;
    }
    for (std::uint16_t i = 0; i < gs->num_output_semantics; ++i) {
        if (gs->output_semantics[i].semantic == semantic) {
            return &gs->output_semantics[i];
        }
    }
    return nullptr;
}

static void _setInterpolantRegister(ShaderRegister* regs, std::uint32_t index, std::uint32_t value) {
    regs[index].offset = SPI_PS_INPUT_CNTL_0 + index;
    regs[index].value = value;
}

static void _fillIdentityInterpolants(ShaderRegister* regs, std::uint32_t firstIndex) {
    for (std::uint32_t i = firstIndex; i < 32u; ++i) {
        _setInterpolantRegister(regs, i, i);
    }
}

}

extern "C" {

int sceAgcCreateShader(Shader** dst, void* header, const volatile void* code) {
    if (dst == nullptr) {
        throw std::runtime_error("sceAgcCreateShader: dst is null");
    }
    if (header == nullptr) {
        throw std::runtime_error("sceAgcCreateShader: header is null");
    }
    if (code == nullptr) {
        throw std::runtime_error("sceAgcCreateShader: code is null");
    }

    auto* h = static_cast<Shader*>(header);

    _resolveRelativePtr(h->cx_registers);
    _resolveRelativePtr(h->sh_registers);
    _resolveRelativePtr(h->user_data);
    _resolveRelativePtr(h->specials);
    _resolveRelativePtr(h->input_semantics);
    _resolveRelativePtr(h->output_semantics);

    if (h->user_data != nullptr) {
        _resolveRelativePtr(h->user_data->direct_resource_offset);
        _resolveRelativePtr(h->user_data->sharp_resource_offset[0]);
        _resolveRelativePtr(h->user_data->sharp_resource_offset[1]);
        _resolveRelativePtr(h->user_data->sharp_resource_offset[2]);
        _resolveRelativePtr(h->user_data->sharp_resource_offset[3]);
    }

    h->code = code;

    if (h->file_header != ShaderRegs::SHADER_FILE_HEADER_MAGIC) {
        throw std::runtime_error("sceAgcCreateShader: invalid file_header magic");
    }
    if (h->version != ShaderRegs::SHADER_VERSION) {
        throw std::runtime_error("sceAgcCreateShader: unsupported shader version");
    }

    const auto base = reinterpret_cast<std::uint64_t>(code);

    if ((base & ShaderRegs::SHADER_BASE_ALIGN_MASK) != 0) {
        throw std::runtime_error("sceAgcCreateShader: code address violates alignment mask");
    }

    int result = _patchProgramAddressRegister(h->sh_registers, h->num_sh_registers, h->type, base);
    if (result != 0) {
        return result;
    }

    *dst = h;
    return 0;
}

int sceAgcCreatePrimState(ShaderRegister* cx_regs, ShaderRegister* uc_regs, const Shader* hs, const Shader* gs, std::uint32_t prim_type) {
    if (cx_regs == nullptr && uc_regs == nullptr) {
        return 0;
    }
    if (gs == nullptr) {
        throw std::runtime_error("sceAgcCreatePrimState: gs is null");
    }

    if (cx_regs != nullptr) {
        cx_regs[0] = gs->specials->vgt_shader_stages_en;
        if ((cx_regs[0].value & ShaderRegs::VGT_SHADER_STAGES_GS_BIT) != 0) {
            cx_regs[1] = gs->specials->vgt_gs_out_prim_type;
        } else {
            cx_regs[1].offset = ShaderRegs::VGT_GS_OUT_PRIM_TYPE;
            cx_regs[1].value = _graphicsPrimTypeToGsOut(prim_type);
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

    return 0;
}

int sceAgcUpdatePrimState(ShaderRegister* cx_regs, ShaderRegister* uc_regs, std::uint32_t prim_type) {
    if (cx_regs != nullptr && (cx_regs[0].value & (ShaderRegs::VGT_SHADER_STAGES_GS_BIT | ShaderRegs::VGT_SHADER_STAGES_NGG_BIT)) == 0) {
        cx_regs[1].value &= ~0x7u;
        cx_regs[1].value |= _graphicsPrimTypeToGsOut(prim_type);
    }

    if (uc_regs != nullptr) {
        uc_regs[2].value &= ~0x1Fu;
        uc_regs[2].value |= prim_type;
    }

    return 0;
}

int sceAgcCreateInterpolantMapping(ShaderRegister* regs, const Shader* gs, const Shader* ps) {
    if (regs == nullptr) {
        throw std::runtime_error("sceAgcCreateInterpolantMapping: regs is null");
    }

    if (ps == nullptr || ps->num_input_semantics == 0) {
        _fillIdentityInterpolants(regs, 0);
        return 0;
    }

    if (ps->num_input_semantics != 0 && ps->input_semantics == nullptr) {
        throw std::runtime_error("sceAgcCreateInterpolantMapping: ps->input_semantics is null but num_input_semantics != 0");
    }

    if (gs == nullptr) {
        throw std::runtime_error("sceAgcCreateInterpolantMapping: gs is null");
    }

    if (gs->num_output_semantics != 0 && gs->output_semantics == nullptr) {
        throw std::runtime_error("sceAgcCreateInterpolantMapping: gs->output_semantics is null but num_output_semantics != 0");
    }

    for (std::uint32_t i = 0; i < ps->num_input_semantics; ++i) {
        const ShaderSemantic& psSemantic = ps->input_semantics[i];
        const ShaderSemantic* gsSemantic = _findOutputSemantic(gs, psSemantic.semantic);
        const std::uint32_t psWord = _shaderSemanticWord(psSemantic);

        std::uint32_t value = ((psWord & 0x00300000u) != 0)
            ? _createInterpolantF16Value(psWord, gsSemantic)
            : _createInterpolantNonF16Value(psWord, gsSemantic);

        value = (gsSemantic == nullptr)
            ? _createInterpolantDefaultValue(value, psWord)
            : _createInterpolantMappingValue(value, psWord, _shaderSemanticWord(*gsSemantic));

        _setInterpolantRegister(regs, i, value);
    }

    _fillIdentityInterpolants(regs, ps->num_input_semantics);
    return 0;
}

} // extern "C"
