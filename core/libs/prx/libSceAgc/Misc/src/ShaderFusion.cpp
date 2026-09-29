#include "prx/libSceAgc/Misc/include/ShaderFusion.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"

struct SizeAlign {
 uint64_t m_size;
 size_t m_align;
};

namespace {

using namespace ShaderRegs;

constexpr int GRAPHICS5_ERROR_INVALID_SHADER_HALVES = static_cast<int>(0x8a6c0008u);
constexpr std::uint32_t SPI_SHADER_PGM_CHKSUM_GS = 0x080u;
constexpr std::uint32_t SPI_SHADER_PGM_RSRC1_GS = 0x08Au;
constexpr std::uint32_t SPI_SHADER_PGM_RSRC2_GS = 0x08Bu;
constexpr std::uint32_t SPI_SHADER_PGM_CHKSUM_HS = 0x100u;
constexpr std::uint32_t SPI_SHADER_PGM_RSRC1_HS = 0x10Au;
constexpr std::uint32_t SPI_SHADER_PGM_RSRC2_HS = 0x10Bu;

bool ValidHalves(const Shader* front, const Shader* back) {
    const auto frontType = static_cast<ShaderBinaryType>(front->type);
    const auto backType = static_cast<ShaderBinaryType>(back->type);
    return (frontType == ShaderBinaryType::GsFront && backType == ShaderBinaryType::GsBack) ||
           (frontType == ShaderBinaryType::HsFront && backType == ShaderBinaryType::HsBack);
}

ShaderRegister& FindRegister(ShaderRegister* regs, std::uint32_t count, std::uint32_t offset, std::uint32_t occurrence = 0) {
    for (std::uint32_t i = 0; regs != nullptr && i < count; ++i) {
        if (regs[i].offset == offset && occurrence-- == 0) return regs[i];
    }
    throw std::runtime_error("sceAgcUnknownFuseShaderHalves: shader half lacks register " + std::to_string(offset));
}

void MergeMax(ShaderRegister& dst, const ShaderRegister& src, std::uint32_t shift, std::uint32_t mask) {
    const auto field = std::max((dst.value >> shift) & mask, (src.value >> shift) & mask);
    dst.value = (dst.value & ~(mask << shift)) | (field << shift);
}

}

extern "C" {

APS5_EXPORT("fd5Bp5tGTgo", sceAgcUnknownFuseShaderHalves);
int APS5_VABI sceAgcUnknownFuseShaderHalves(Shader* fused_result, const Shader* front, const Shader* back, void* scratch_mem) {
    if (fused_result == nullptr || front == nullptr || back == nullptr) APS5_INVALID_ARG_EX;
    if (!ValidHalves(front, back)) return GRAPHICS5_ERROR_INVALID_SHADER_HALVES;
    const bool isGs = static_cast<ShaderBinaryType>(front->type) == ShaderBinaryType::GsFront;
    const std::uint32_t stageBit = isGs ? (1u << 22u) : (1u << 21u);
    if (((front->specials->vgt_shader_stages_en.value ^ back->specials->vgt_shader_stages_en.value) & stageBit) != 0) {
        return GRAPHICS5_ERROR_INVALID_SHADER_HALVES;
    }
    *fused_result = *back;
    fused_result->type = static_cast<std::uint8_t>(isGs ? ShaderBinaryType::Gs : ShaderBinaryType::Hs);
    if (scratch_mem != nullptr) {
        auto* regs = static_cast<ShaderRegister*>(scratch_mem);
        std::memcpy(regs, back->sh_registers, std::size_t{back->num_sh_registers} * sizeof(ShaderRegister));
        fused_result->sh_registers = regs;
    }
    ShaderRegister* fused = fused_result->sh_registers;
    const std::uint32_t fusedCount = fused_result->num_sh_registers;
    const std::uint32_t frontCount = front->num_sh_registers;
    const std::uint32_t checksum = isGs ? SPI_SHADER_PGM_CHKSUM_GS : SPI_SHADER_PGM_CHKSUM_HS;
    for (std::uint32_t occurrence = 0; occurrence < 2; ++occurrence) {
        FindRegister(fused, fusedCount, checksum, occurrence).value = FindRegister(front->sh_registers, frontCount, checksum, occurrence).value;
    }
    const auto& frontRsrc1 = FindRegister(front->sh_registers, frontCount, isGs ? SPI_SHADER_PGM_RSRC1_GS : SPI_SHADER_PGM_RSRC1_HS);
    const auto& frontRsrc2 = FindRegister(front->sh_registers, frontCount, isGs ? SPI_SHADER_PGM_RSRC2_GS : SPI_SHADER_PGM_RSRC2_HS);
    auto& fusedRsrc1 = FindRegister(fused, fusedCount, isGs ? SPI_SHADER_PGM_RSRC1_GS : SPI_SHADER_PGM_RSRC1_HS);
    auto& fusedRsrc2 = FindRegister(fused, fusedCount, isGs ? SPI_SHADER_PGM_RSRC2_GS : SPI_SHADER_PGM_RSRC2_HS);
    // Both halves run in one wave, so VGPRs are the larger of the two and shared VGPRs are reallocated (after Kyty).
    const std::uint32_t frontVgprs = ((frontRsrc1.value & 0x3fu) + 1u) * 4u;
    const std::uint32_t backVgprs = ((fusedRsrc1.value & 0x3fu) + 1u) * 4u;
    const std::uint32_t frontTotal = frontVgprs + (frontRsrc2.value >> 28u) * 8u;
    const std::uint32_t backTotal = backVgprs + (fusedRsrc2.value >> 28u) * 8u;
    const std::uint32_t maxTotal = std::max(frontTotal, backTotal);
    const std::uint32_t shared = std::max(frontVgprs, backVgprs) >= maxTotal ? 0u : (maxTotal - std::min(frontTotal, backTotal) + 7u) / 64u;
    fusedRsrc2.value = (fusedRsrc2.value & 0x0fffffffu) | ((shared & 0xfu) << 28u);
    MergeMax(fusedRsrc1, frontRsrc1, 0, 0x3fu);
    if (isGs) {
        MergeMax(fusedRsrc1, frontRsrc1, 29, 0x3u);
        MergeMax(fusedRsrc2, frontRsrc2, 16, 0x3u);
        fusedRsrc2.value = (fusedRsrc2.value & 0xfffbffffu) | (frontRsrc2.value & 0x00040000u);
    } else {
        MergeMax(fusedRsrc1, frontRsrc1, 28, 0x3u);
    }
    fusedRsrc2.value = (fusedRsrc2.value & 0xf7ffffc1u) | (frontRsrc2.value & 0x0800003eu);
    // The fused program starts at the front half; its address goes in the ES/LS program registers.
    const std::uint32_t lo = isGs ? SPI_SHADER_PGM_LO_ES : SPI_SHADER_PGM_LO_LS;
    auto& loRegister = FindRegister(fused, fusedCount, lo);
    auto& hiRegister = FindRegister(fused, fusedCount, lo + 1u);
    const auto address = reinterpret_cast<std::uint64_t>(front->code);
    loRegister.value = static_cast<std::uint32_t>(address >> 8u);
    hiRegister.value = (hiRegister.value & 0xffffff00u) | static_cast<std::uint32_t>((address >> 40u) & 0xffu);
    fused_result->user_data = nullptr;
    return 0;
}

APS5_EXPORT("dolOmWH+huQ", sceAgcUnknownGetFusedShaderSize);
int APS5_VABI sceAgcUnknownGetFusedShaderSize(SizeAlign* dst, const Shader* front, const Shader* back) {
    if (dst == nullptr || front == nullptr || back == nullptr) APS5_INVALID_ARG_EX;
    if (!ValidHalves(front, back)) return GRAPHICS5_ERROR_INVALID_SHADER_HALVES;
    dst->m_size = std::uint64_t{back->num_sh_registers} * sizeof(ShaderRegister);
    dst->m_align = 4;
    return 0;
}

APS5_EXPORT("k0E7vkgqAuE", sceAgcCreateInterpolantMappingVsPs);
int APS5_VABI sceAgcCreateInterpolantMappingVsPs(ShaderRegister* regs, const Shader* vs, const Shader* ps) {
    (void)regs;
    (void)vs;
    (void)ps;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
