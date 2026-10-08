#include "prx/libSceAgc/Misc/include/ShaderFusion.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <string>
#include <cstdint>
#include <cstddef>
#include <vector>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"
#include "prx/libSceAgc/Shader/include/ShaderUtils.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"

namespace {

using namespace ShaderRegs;

constexpr int GRAPHICS5_ERROR_INVALID_SHADER_HALVES = static_cast<int>(0x8a6c0008u);
constexpr std::uint32_t SPI_SHADER_PGM_CHKSUM_GS = 0x080u;
constexpr std::uint32_t SPI_SHADER_PGM_RSRC1_GS = 0x08Au;
constexpr std::uint32_t SPI_SHADER_PGM_RSRC2_GS = 0x08Bu;
constexpr std::uint32_t SPI_SHADER_PGM_CHKSUM_HS = 0x100u;
constexpr std::uint32_t SPI_SHADER_PGM_RSRC1_HS = 0x10Au;
constexpr std::uint32_t SPI_SHADER_PGM_RSRC2_HS = 0x10Bu;
constexpr std::uint32_t SCodeEnd = 0xbf9f0000u;
constexpr std::uint32_t SNop = 0xbf800000u;
constexpr std::uint32_t SSetpcS6 = 0xbe802006u;
constexpr std::uint32_t SSwappcNullS6 = 0xbefd2106u;
constexpr std::uint8_t FusedCodeAlignmentLog2 = 8;
constexpr std::size_t FusedCodeAlignment = std::size_t{1} << FusedCodeAlignmentLog2;
constexpr char TrailerMagic[8] = {'b', 'a', 'r', 'e', 'f', 'o', 'o', 't'};
constexpr std::size_t TrailerCodeBytesOffset = sizeof(TrailerMagic) + 3 * sizeof(std::uint32_t);
constexpr std::uint32_t Rsrc1VgprMask = 0x3fu;
constexpr std::uint32_t Rsrc1SgprMask = 0xfu << 6u;
constexpr std::uint32_t Rsrc2UserSgprMask = (0x1fu << 1u) | (1u << 27u);

bool ValidHalves(const Shader* front, const Shader* back) {
    const auto frontType = static_cast<ShaderBinaryType>(front->type);
    const auto backType = static_cast<ShaderBinaryType>(back->type);
    return (frontType == ShaderBinaryType::GsFront && backType == ShaderBinaryType::GsBack) ||
           (frontType == ShaderBinaryType::HsFront && backType == ShaderBinaryType::HsBack);
}

bool GeometryHalves(const Shader* front) {
    return static_cast<ShaderBinaryType>(front->type) == ShaderBinaryType::GsFront;
}

ShaderRegister& FindRegister(ShaderRegister* regs, std::uint32_t count, std::uint32_t offset, std::uint32_t occurrence = 0) {
    for (std::uint32_t i = 0; regs != nullptr && i < count; ++i) {
        if (regs[i].offset == offset && occurrence-- == 0) return regs[i];
    }
    throw std::runtime_error("sceAgcFuseShaderHalves_0200: shader half lacks register " + std::to_string(offset));
}

void MergeMax(ShaderRegister& dst, const ShaderRegister& src, std::uint32_t shift, std::uint32_t mask) {
    const auto field = std::max((dst.value >> shift) & mask, (src.value >> shift) & mask);
    dst.value = (dst.value & ~(mask << shift)) | (field << shift);
}

struct GeometryLayout {
    std::uint32_t frontBytes;
    std::size_t backOffset;
    std::size_t codeBytes;
    std::vector<ShaderRegister> sh;
    std::vector<ShaderRegister> cx;
    std::size_t headerOffset;
    std::size_t shOffset;
    std::size_t cxOffset;
    std::size_t specialsOffset;
    std::size_t inputsOffset;
    std::size_t outputsOffset;
    std::size_t userDataOffset;
    std::size_t directOffset;
    std::array<std::size_t, 4> sharpOffsets;
    std::size_t totalBytes;
};

[[noreturn]] void Fail(const char* function, const std::string& message) {
    throw std::runtime_error(std::string(function) + ": " + message);
}

std::size_t AlignUp(std::size_t value, std::size_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

void ValidateGeometryHalf(const char* function, const Shader* shader) {
    if (shader->file_header != SHADER_FILE_HEADER_MAGIC || shader->version != SHADER_VERSION) Fail(function, "invalid shader header or version");
    if (shader->code == nullptr || shader->shader_size == 0 || (shader->shader_size & 3u) != 0) Fail(function, "invalid shader half code");
    if (shader->embedded_constant_buffer_size_dqw != 0) Fail(function, "fusing halves with an embedded constant buffer is not implemented");
    if ((shader->num_sh_registers != 0 && shader->sh_registers == nullptr) || (shader->num_cx_registers != 0 && shader->cx_registers == nullptr)) Fail(function, "shader half registers missing");
}

std::uint32_t FrontProgramBytes(const char* function, const Shader* front) {
    const auto* bytes = static_cast<const std::uint8_t*>(const_cast<const void*>(front->code));
    const std::size_t size = front->shader_size;
    if (size < TrailerCodeBytesOffset + sizeof(std::uint32_t)) Fail(function, "front half has no program trailer");
    std::size_t trailer = size;
    for (std::size_t offset = size - TrailerCodeBytesOffset - sizeof(std::uint32_t) + 1; offset-- > 0;) {
        if (std::memcmp(bytes + offset, TrailerMagic, sizeof(TrailerMagic)) == 0) {
            trailer = offset;
            break;
        }
    }
    if (trailer == size) Fail(function, "front half has no program trailer");
    std::uint32_t codeBytes = 0;
    std::memcpy(&codeBytes, bytes + trailer + TrailerCodeBytesOffset, sizeof(codeBytes));
    if (codeBytes == 0 || (codeBytes & 3u) != 0 || codeBytes > trailer) Fail(function, "front half trailer has an invalid code size");
    std::size_t end = codeBytes / 4;
    const auto word = [&](std::size_t index) {
        std::uint32_t value = 0;
        std::memcpy(&value, bytes + index * 4, sizeof(value));
        return value;
    };
    while (end > 0 && word(end - 1) == SCodeEnd) --end;
    if (end == 0 || (word(end - 1) != SSetpcS6 && word(end - 1) != SSwappcNullS6)) Fail(function, "front half does not end with s_setpc_b64 s[6:7] or s_swappc_b64 null, s[6:7]");
    return static_cast<std::uint32_t>((end - 1) * 4);
}

std::uint32_t MergeRsrc1(std::uint32_t back, std::uint32_t front) {
    const auto vgprs = std::max(back & Rsrc1VgprMask, front & Rsrc1VgprMask);
    const auto sgprs = std::max(back & Rsrc1SgprMask, front & Rsrc1SgprMask);
    return (back & ~(Rsrc1VgprMask | Rsrc1SgprMask)) | vgprs | sgprs;
}

std::uint32_t MergeRsrc2(const char* function, std::uint32_t back, std::uint32_t front) {
    if ((front & ~Rsrc2UserSgprMask & ~back) != 0) Fail(function, "front half requires hardware state the back half does not enable");
    return (back & ~Rsrc2UserSgprMask) | (front & Rsrc2UserSgprMask);
}

const ShaderRegister* FindHalfRegister(const Shader* shader, std::uint32_t offset) {
    for (std::uint32_t i = 0; i < shader->num_sh_registers; ++i) {
        if (shader->sh_registers[i].offset == offset) return &shader->sh_registers[i];
    }
    return nullptr;
}

GeometryLayout ComputeGeometryLayout(const char* function, const Shader* front, const Shader* back) {
    ValidateGeometryHalf(function, back);
    ValidateGeometryHalf(function, front);
    GeometryLayout layout{};
    layout.frontBytes = FrontProgramBytes(function, front);
    layout.backOffset = AlignUp(layout.frontBytes, FusedCodeAlignment);
    layout.codeBytes = layout.backOffset + back->shader_size;
    const auto* frontRsrc1 = FindHalfRegister(front, SPI_SHADER_PGM_RSRC1_GS);
    const auto* frontRsrc2 = FindHalfRegister(front, SPI_SHADER_PGM_RSRC2_GS);
    if (frontRsrc1 == nullptr || frontRsrc2 == nullptr) Fail(function, "front half has no GS resource registers");
    layout.sh.assign(back->sh_registers, back->sh_registers + back->num_sh_registers);
    for (auto& reg : layout.sh) {
        if (reg.offset == SPI_SHADER_PGM_RSRC1_GS) reg.value = MergeRsrc1(reg.value, frontRsrc1->value);
        else if (reg.offset == SPI_SHADER_PGM_RSRC2_GS) reg.value = MergeRsrc2(function, reg.value, frontRsrc2->value);
    }
    for (std::uint32_t i = 0; i < front->num_sh_registers; ++i) {
        const auto& reg = front->sh_registers[i];
        if (reg.offset == SPI_SHADER_PGM_CHKSUM_GS || FindHalfRegister(back, reg.offset) != nullptr) continue;
        layout.sh.push_back(reg);
    }
    layout.cx.assign(back->cx_registers, back->cx_registers + back->num_cx_registers);
    for (std::uint32_t i = 0; i < front->num_cx_registers; ++i) {
        const auto& reg = front->cx_registers[i];
        if (std::none_of(layout.cx.begin(), layout.cx.end(), [&](const ShaderRegister& other) { return other.offset == reg.offset; })) layout.cx.push_back(reg);
    }
    if (layout.sh.size() > 0xff || layout.cx.size() > 0xff) Fail(function, "fused register count exceeds the shader header");
    layout.headerOffset = AlignUp(layout.codeBytes, alignof(Shader));
    layout.shOffset = AlignUp(layout.headerOffset + sizeof(Shader), alignof(ShaderRegister));
    layout.cxOffset = layout.shOffset + layout.sh.size() * sizeof(ShaderRegister);
    layout.specialsOffset = AlignUp(layout.cxOffset + layout.cx.size() * sizeof(ShaderRegister), alignof(ShaderSpecialRegs));
    layout.inputsOffset = AlignUp(layout.specialsOffset + back->special_sizes_bytes, alignof(ShaderSemantic));
    layout.outputsOffset = layout.inputsOffset + front->num_input_semantics * sizeof(ShaderSemantic);
    layout.userDataOffset = AlignUp(layout.outputsOffset + back->num_output_semantics * sizeof(ShaderSemantic), alignof(ShaderUserData));
    auto end = layout.userDataOffset + (back->user_data != nullptr ? sizeof(ShaderUserData) : 0);
    layout.directOffset = AlignUp(end, alignof(std::uint16_t));
    if (back->user_data != nullptr) end = layout.directOffset + back->user_data->direct_resource_count * sizeof(std::uint16_t);
    for (std::size_t i = 0; i < layout.sharpOffsets.size(); ++i) {
        layout.sharpOffsets[i] = AlignUp(end, alignof(ShaderSharp));
        if (back->user_data != nullptr) end = layout.sharpOffsets[i] + back->user_data->sharp_resource_count[i] * sizeof(ShaderSharp);
    }
    layout.totalBytes = end;
    return layout;
}

void SetProgramAddress(std::vector<ShaderRegister>& regs, std::uint32_t loOffset, std::uint64_t address) {
    for (std::size_t i = 0; i + 1 < regs.size(); ++i) {
        if (regs[i].offset != loOffset || regs[i + 1].offset != loOffset + 1u) continue;
        regs[i].value = static_cast<std::uint32_t>(address >> 8u);
        regs[i + 1].value = (regs[i + 1].value & 0xFFFFFF00u) | static_cast<std::uint32_t>((address >> 40u) & 0xFFu);
        return;
    }
    throw std::runtime_error("sceAgcFuseShaderHalves_0200: back half has no program address register pair");
}

int FuseGeometryHalves(Shader* fused_result, const Shader* front, const Shader* back, void* scratch_mem) {
    constexpr auto fn = "sceAgcFuseShaderHalves_0200";
    if (scratch_mem == nullptr) Fail(fn, "geometry halves need fused shader memory");
    auto layout = ComputeGeometryLayout(fn, front, back);
    const auto scratch = reinterpret_cast<std::uintptr_t>(scratch_mem);
    const auto base = AlignUp(scratch, FusedCodeAlignment);
    if ((base & SHADER_BASE_ALIGN_MASK) != 0) Fail(fn, "fused shader memory is not a valid program address");
    auto* memory = static_cast<std::uint8_t*>(scratch_mem) + (base - scratch);
    std::memcpy(memory, const_cast<const void*>(front->code), layout.frontBytes);
    for (std::size_t offset = layout.frontBytes; offset < layout.backOffset; offset += sizeof(SNop)) std::memcpy(memory + offset, &SNop, sizeof(SNop));
    std::memcpy(memory + layout.backOffset, const_cast<const void*>(back->code), back->shader_size);
    SetProgramAddress(layout.sh, SPI_SHADER_PGM_LO_GS, base + layout.backOffset);
    if (PatchProgramAddressRegister(layout.sh.data(), static_cast<std::uint32_t>(layout.sh.size()), static_cast<std::uint8_t>(ShaderBinaryType::Gs), base) != 0) Fail(fn, "fused program address patch failed");
    auto* sh = reinterpret_cast<ShaderRegister*>(memory + layout.shOffset);
    auto* cx = reinterpret_cast<ShaderRegister*>(memory + layout.cxOffset);
    auto* specials = reinterpret_cast<ShaderSpecialRegs*>(memory + layout.specialsOffset);
    auto* inputs = reinterpret_cast<ShaderSemantic*>(memory + layout.inputsOffset);
    auto* outputs = reinterpret_cast<ShaderSemantic*>(memory + layout.outputsOffset);
    std::memcpy(sh, layout.sh.data(), layout.sh.size() * sizeof(ShaderRegister));
    std::memcpy(cx, layout.cx.data(), layout.cx.size() * sizeof(ShaderRegister));
    if (back->special_sizes_bytes != 0) std::memcpy(specials, back->specials, back->special_sizes_bytes);
    if (front->num_input_semantics != 0) std::memcpy(inputs, front->input_semantics, front->num_input_semantics * sizeof(ShaderSemantic));
    if (back->num_output_semantics != 0) std::memcpy(outputs, back->output_semantics, back->num_output_semantics * sizeof(ShaderSemantic));
    Shader fused = *back;
    if (back->user_data != nullptr) {
        auto* userData = reinterpret_cast<ShaderUserData*>(memory + layout.userDataOffset);
        *userData = *back->user_data;
        auto* direct = reinterpret_cast<std::uint16_t*>(memory + layout.directOffset);
        if (userData->direct_resource_count != 0) std::memcpy(direct, back->user_data->direct_resource_offset, userData->direct_resource_count * sizeof(std::uint16_t));
        userData->direct_resource_offset = userData->direct_resource_count != 0 ? direct : nullptr;
        for (std::size_t i = 0; i < layout.sharpOffsets.size(); ++i) {
            auto* sharps = reinterpret_cast<ShaderSharp*>(memory + layout.sharpOffsets[i]);
            if (userData->sharp_resource_count[i] != 0) std::memcpy(sharps, back->user_data->sharp_resource_offset[i], userData->sharp_resource_count[i] * sizeof(ShaderSharp));
            userData->sharp_resource_offset[i] = userData->sharp_resource_count[i] != 0 ? sharps : nullptr;
        }
        fused.user_data = userData;
    }
    fused.header_size = static_cast<std::uint32_t>(layout.totalBytes - layout.headerOffset);
    fused.code = memory;
    fused.sh_registers = sh;
    fused.cx_registers = layout.cx.empty() ? nullptr : cx;
    fused.specials = back->special_sizes_bytes != 0 ? specials : nullptr;
    fused.input_semantics = front->num_input_semantics != 0 ? inputs : nullptr;
    fused.output_semantics = back->num_output_semantics != 0 ? outputs : nullptr;
    fused.shader_size = static_cast<std::uint32_t>(layout.codeBytes);
    fused.num_input_semantics = front->num_input_semantics;
    fused.scratch_size_dw_per_thread = std::max(front->scratch_size_dw_per_thread, back->scratch_size_dw_per_thread);
    fused.type = static_cast<std::uint8_t>(ShaderBinaryType::Gs);
    fused.num_sh_registers = static_cast<std::uint8_t>(layout.sh.size());
    fused.num_cx_registers = static_cast<std::uint8_t>(layout.cx.size());
    auto* header = reinterpret_cast<Shader*>(memory + layout.headerOffset);
    *header = fused;
    *fused_result = fused;
    AgcDriverRegisterShader_nid_postfix(header);
    return 0;
}

}

extern "C" {

int APS5_VABI sceAgcFuseShaderHalves_0200(Shader* fused_result, const Shader* front, const Shader* back, void* scratch_mem) {
    if (fused_result == nullptr || front == nullptr || back == nullptr) APS5_INVALID_ARG_EX;
    if (!ValidHalves(front, back)) return GRAPHICS5_ERROR_INVALID_SHADER_HALVES;
    const bool isGs = GeometryHalves(front);
    const std::uint32_t stageBit = isGs ? (1u << 22u) : (1u << 21u);
    if (((front->specials->vgt_shader_stages_en.value ^ back->specials->vgt_shader_stages_en.value) & stageBit) != 0) {
        return GRAPHICS5_ERROR_INVALID_SHADER_HALVES;
    }
    if (isGs) return FuseGeometryHalves(fused_result, front, back, scratch_mem);
    *fused_result = *back;
    fused_result->type = static_cast<std::uint8_t>(ShaderBinaryType::Hs);
    if (scratch_mem != nullptr) {
        auto* regs = static_cast<ShaderRegister*>(scratch_mem);
        std::memcpy(regs, back->sh_registers, std::size_t{back->num_sh_registers} * sizeof(ShaderRegister));
        fused_result->sh_registers = regs;
    }
    ShaderRegister* fused = fused_result->sh_registers;
    const std::uint32_t fusedCount = fused_result->num_sh_registers;
    const std::uint32_t frontCount = front->num_sh_registers;
    for (std::uint32_t occurrence = 0; occurrence < 2; ++occurrence) {
        FindRegister(fused, fusedCount, SPI_SHADER_PGM_CHKSUM_HS, occurrence).value = FindRegister(front->sh_registers, frontCount, SPI_SHADER_PGM_CHKSUM_HS, occurrence).value;
    }
    const auto& frontRsrc1 = FindRegister(front->sh_registers, frontCount, SPI_SHADER_PGM_RSRC1_HS);
    const auto& frontRsrc2 = FindRegister(front->sh_registers, frontCount, SPI_SHADER_PGM_RSRC2_HS);
    auto& fusedRsrc1 = FindRegister(fused, fusedCount, SPI_SHADER_PGM_RSRC1_HS);
    auto& fusedRsrc2 = FindRegister(fused, fusedCount, SPI_SHADER_PGM_RSRC2_HS);
    const std::uint32_t frontVgprs = ((frontRsrc1.value & 0x3fu) + 1u) * 4u;
    const std::uint32_t backVgprs = ((fusedRsrc1.value & 0x3fu) + 1u) * 4u;
    const std::uint32_t frontTotal = frontVgprs + (frontRsrc2.value >> 28u) * 8u;
    const std::uint32_t backTotal = backVgprs + (fusedRsrc2.value >> 28u) * 8u;
    const std::uint32_t maxTotal = std::max(frontTotal, backTotal);
    const std::uint32_t shared = std::max(frontVgprs, backVgprs) >= maxTotal ? 0u : (maxTotal - std::min(frontTotal, backTotal) + 7u) / 64u;
    fusedRsrc2.value = (fusedRsrc2.value & 0x0fffffffu) | ((shared & 0xfu) << 28u);
    MergeMax(fusedRsrc1, frontRsrc1, 0, 0x3fu);
    MergeMax(fusedRsrc1, frontRsrc1, 28, 0x3u);
    fusedRsrc2.value = (fusedRsrc2.value & 0xf7ffffc1u) | (frontRsrc2.value & 0x0800003eu);
    auto& loRegister = FindRegister(fused, fusedCount, SPI_SHADER_PGM_LO_LS);
    auto& hiRegister = FindRegister(fused, fusedCount, SPI_SHADER_PGM_LO_LS + 1u);
    const auto address = reinterpret_cast<std::uint64_t>(front->code);
    loRegister.value = static_cast<std::uint32_t>(address >> 8u);
    hiRegister.value = (hiRegister.value & 0xffffff00u) | static_cast<std::uint32_t>((address >> 40u) & 0xffu);
    fused_result->user_data = nullptr;
    return 0;
}

int APS5_VABI sceAgcFuseShaderHalves_nid_postfix(Shader* fused_result, const Shader* front, const Shader* back, void* scratch_mem) {
    return sceAgcFuseShaderHalves_0200(fused_result, front, back, scratch_mem);
}

int APS5_VABI sceAgcGetFusedShaderSize_0080(SizeAlign* dst, const Shader* front, const Shader* back) {
    if (dst == nullptr || front == nullptr || back == nullptr) APS5_INVALID_ARG_EX;
    if (!ValidHalves(front, back)) return GRAPHICS5_ERROR_INVALID_SHADER_HALVES;
    if (GeometryHalves(front)) {
        const auto layout = ComputeGeometryLayout("sceAgcGetFusedShaderSize_0080", front, back);
        dst->m_size = layout.totalBytes + FusedCodeAlignment - 1;
        dst->m_align = FusedCodeAlignmentLog2;
        return 0;
    }
    dst->m_size = std::uint64_t{back->num_sh_registers} * sizeof(ShaderRegister);
    dst->m_align = 4;
    return 0;
}

int APS5_VABI sceAgcUpdateInterpolantMapping_0100(ShaderRegister* regs, const Shader* vs, const Shader* ps) {
    (void)regs;
    (void)vs;
    (void)ps;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
