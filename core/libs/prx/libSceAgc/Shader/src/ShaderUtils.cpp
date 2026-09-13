#include <stdexcept>
#include "prx/libSceAgc/Shader/include/ShaderUtils.hpp"

using namespace ShaderRegs;

bool GetProgramAddressRegisterOffset(std::uint8_t type, std::uint32_t& loOffset) {
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

int PatchProgramAddressRegister(ShaderRegister* regs, std::uint32_t numRegs, std::uint8_t type, std::uint64_t base) {
    std::uint32_t loOffset = 0;
    if (!GetProgramAddressRegisterOffset(type, loOffset)) {
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

std::uint32_t GraphicsPrimTypeToGsOut(std::uint32_t primType) {
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

std::uint32_t ShaderSemanticWord(const ShaderSemantic& s) {
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

std::uint32_t ApplyInterpolantDefaultValue(std::uint32_t value, std::uint32_t psWord) {
    value &= ~0x00000300u;
    value |= ((psWord >> 28u) & 0x3u) << 8u;
    return value;
}

std::uint32_t ApplyInterpolantDefaultValueHi(std::uint32_t value, std::uint32_t psWord) {
    value &= ~0x00600000u;
    value |= ((psWord >> 30u) & 0x3u) << 21u;
    return value;
}

std::uint32_t CreateInterpolantF16Value(std::uint32_t psWord, const ShaderSemantic* gsSemantic) {
    std::uint32_t value = (psWord << 4u) & 0x03000000u;
    if (gsSemantic == nullptr) {
        value |= 0x00180020u;
    } else {
        const std::uint32_t commonWord = psWord & ShaderSemanticWord(*gsSemantic);
        value &= 0xFFF7FFDFu;
        value |= (commonWord >> 15u) & 0x20u;
        value ^= 0x00080020u;
        value &= ~0x00100000u;
        value |= (~commonWord >> 1u) & 0x00100000u;
    }
    return ApplyInterpolantDefaultValueHi(value, psWord);
}

std::uint32_t CreateInterpolantNonF16Value(std::uint32_t psWord, const ShaderSemantic* gsSemantic) {
    std::uint32_t value = 0;
    if ((psWord & 0x01000000u) != 0 || gsSemantic == nullptr) {
        value |= 0x20u;
    }
    return value;
}

std::uint32_t CreateInterpolantMappingValue(std::uint32_t value, std::uint32_t psWord, std::uint32_t gsWord) {
    const std::uint32_t flatShade =
        ((psWord & 0x00400000u) != 0 || (psWord & 0x01000000u) != 0) ? 0x00000400u : 0u;
    value &= ~0x0000001Fu;
    value |= (gsWord >> 8u) & 0x1Fu;
    value &= ~0x00000400u;
    value |= flatShade;
    return ApplyInterpolantDefaultValue(value, psWord);
}

std::uint32_t CreateInterpolantDefaultValue(std::uint32_t value, std::uint32_t psWord) {
    value &= ~0x0000001Fu;
    value &= ~0x00000400u;
    return ApplyInterpolantDefaultValue(value, psWord);
}

const ShaderSemantic* FindOutputSemantic(const Shader* gs, std::uint32_t semantic) {
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

void SetInterpolantRegister(ShaderRegister* regs, std::uint32_t index, std::uint32_t value) {
    regs[index].offset = SPI_PS_INPUT_CNTL_0 + index;
    regs[index].value = value;
}

void FillIdentityInterpolants(ShaderRegister* regs, std::uint32_t firstIndex) {
    for (std::uint32_t i = firstIndex; i < 32u; ++i) {
        SetInterpolantRegister(regs, i, i);
    }
}
