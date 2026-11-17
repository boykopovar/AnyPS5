#include "prx/libSceAgc/Shader/include/GsOversubscription.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <prx/libc/include/General.hpp>

#include "SceShaders.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"

namespace {

constexpr std::uint32_t GE_PC_ALLOC_FULL = 0x7FFu;
constexpr std::uint32_t LATE_ALLOC_GS_FULL = 0x007F0000u;
constexpr std::uint32_t VGT_SHADER_STAGES_GS_W32_BIT = 0x00400000u;

struct GsOccupancyLimits {
    std::uint32_t vertex;
    std::uint32_t exportCount;
};

std::uint32_t findContextRegister(std::span<const ShaderRegister> registers, std::uint32_t offset) {
    for (const ShaderRegister& reg : registers) {
        if (reg.offset == offset) {
            return reg.value;
        }
    }
    throw std::runtime_error("sceAgcGetGsOversubscription: gs has no context register " + std::to_string(offset));
}

GsOccupancyLimits getGsOccupancyLimits(std::span<const ShaderRegister> gs, std::uint32_t stagesEnable, std::uint32_t vertexCapacity, std::uint32_t exportCapacity) {
    const std::uint32_t onchip = findContextRegister(gs, ShaderRegs::VGT_GS_ONCHIP_CNTL);
    const std::uint32_t subgroup = findContextRegister(gs, ShaderRegs::GE_NGG_SUBGRP_CNTL);
    const std::uint32_t vsOut = findContextRegister(gs, ShaderRegs::SPI_VS_OUT_CONFIG);
    const std::uint32_t clOut = findContextRegister(gs, ShaderRegs::PA_CL_VS_OUT_CNTL);
    const std::uint32_t maxOutput = findContextRegister(gs, ShaderRegs::GE_MAX_OUTPUT_PER_SUBGROUP);

    const std::uint32_t outputWords = ((maxOutput & 0x3FFu) + 31u) >> 5u;
    if (outputWords == 0) {
        throw std::runtime_error("sceAgcGetGsOversubscription: GE_MAX_OUTPUT_PER_SUBGROUP is zero");
    }
    const std::uint32_t subgroupWork = ((((onchip >> 11u) & 0x7FFu) * (subgroup & 0x1FFu)) + 31u) >> 5u;
    std::uint32_t waves = std::max(subgroupWork, outputWords);
    if ((stagesEnable & VGT_SHADER_STAGES_GS_W32_BIT) == 0) {
        waves >>= 1u;
    }
    waves = std::max(waves, 1u);

    const std::uint32_t exports = 1u + ((clOut >> 21u) & 1u) + ((clOut >> 22u) & 1u) + ((clOut >> 23u) & 1u);
    const std::uint32_t exportLimit = (exportCapacity / exports) * 4u;
    const std::uint32_t vertexLimit = (vsOut & 0x80u) != 0 ? 2048u : vertexCapacity / (((vsOut >> 2u) & 0xFu) + 1u);

    return {waves * (vertexLimit / outputWords), waves * (exportLimit / outputWords)};
}

}

extern "C" {

int APS5_VABI sceAgcGetGsOversubscription(ShaderRegister* regs, const Shader* gs, std::uint32_t budget, float factor) {
    if (regs == nullptr) {
        throw std::runtime_error(std::string(__func__) + ": regs is null");
    }

    ShaderRegister pcAlloc{ShaderRegs::GE_PC_ALLOC, 0u};
    ShaderRegister rsrc4{ShaderRegs::SPI_SHADER_PGM_RSRC4_GS, 0u};

    if (budget == std::numeric_limits<std::uint32_t>::max()) {
        pcAlloc.value = GE_PC_ALLOC_FULL;
        rsrc4.value = LATE_ALLOC_GS_FULL;
    } else if (budget != 0) {
        Shader shader{};
        if (gs == nullptr || !GuestCopyIn_nid_no_patch(&shader, gs, sizeof(shader)) || shader.specials == nullptr) {
            throw std::runtime_error(std::string(__func__) + ": gs or its special registers are null or not readable");
        }
        ShaderSpecialRegs specials{};
        std::array<ShaderRegister, std::numeric_limits<std::uint8_t>::max()> registers{};
        const std::size_t count = shader.cx_registers != nullptr ? shader.num_cx_registers : 0u;
        if (!GuestCopyIn_nid_no_patch(&specials, shader.specials, sizeof(specials)) || !GuestCopyIn_nid_no_patch(registers.data(), shader.cx_registers, count * sizeof(ShaderRegister))) {
            throw std::runtime_error(std::string(__func__) + ": gs registers are not readable");
        }
        const std::span<const ShaderRegister> cx(registers.data(), count);
        const GsOccupancyLimits base = getGsOccupancyLimits(cx, specials.vgt_shader_stages_en.value, 1024u, 128u);
        const GsOccupancyLimits expanded = getGsOccupancyLimits(cx, specials.vgt_shader_stages_en.value, 2048u, 382u);
        const std::uint32_t baseMin = std::min(base.vertex, base.exportCount);
        const std::uint32_t limitShift = (specials.vgt_shader_stages_en.value & VGT_SHADER_STAGES_GS_W32_BIT) != 0 ? 5u : 6u;
        const std::uint32_t expandedLimit = std::min({expanded.vertex, expanded.exportCount, budget >> limitShift, 1024u});
        const std::uint32_t headroom = expandedLimit > baseMin ? expandedLimit - baseMin : 0u;
        const float scaled = std::fma(factor, static_cast<float>(headroom), static_cast<float>(baseMin));
        if (!(scaled >= 0.0f && scaled < 4294967296.0f)) {
            throw std::runtime_error(std::string(__func__) + ": factor gives an unrepresentable target");
        }
        const auto target = static_cast<std::uint32_t>(scaled);

        if (target > baseMin) {
            if (target < base.exportCount) {
                const std::uint32_t range = std::max(expanded.vertex - base.vertex, 1u);
                std::uint32_t value = static_cast<std::uint32_t>(std::min<std::uint64_t>((static_cast<std::uint64_t>(target - base.vertex) << 10u) / range, 1024u));
                value = std::max(value, 1u);
                pcAlloc.value = ((value << 1u) - 1u) & GE_PC_ALLOC_FULL;
                rsrc4.value = LATE_ALLOC_GS_FULL;
            } else {
                const std::uint32_t range = std::max(expanded.exportCount - base.exportCount, 1u);
                const auto value = static_cast<std::uint32_t>(std::min<std::uint64_t>((static_cast<std::uint64_t>(target - base.exportCount) * 127u) / range, 127u));
                pcAlloc.value = GE_PC_ALLOC_FULL;
                rsrc4.value = value << 16u;
            }
        }
    }

    const std::array<ShaderRegister, 2> result{pcAlloc, rsrc4};
    if (!GuestCopyOut_nid_no_patch(regs, result.data(), sizeof(result))) {
        throw std::runtime_error(std::string(__func__) + ": regs is not writable");
    }
    return 0;
}

}
