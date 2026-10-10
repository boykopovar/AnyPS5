#include "SceShaders.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <stdexcept>

extern "C" int APS5_VABI sceAgcGetGsOversubscription(ShaderRegister* regs, const Shader* gs, std::uint32_t budget, float factor);

namespace {

using Registers = std::array<ShaderRegister, 2>;

struct GsSetup {
    std::array<ShaderRegister, 5> cx;
    ShaderSpecialRegs specials;
    Shader shader;
};

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename TAction>
void expectFailure(TAction action) {
    try {
        action();
    } catch (const std::exception& error) {
        check(error.what()[0] != '\0', "empty exception message");
        return;
    }
    throw std::runtime_error("expected an exception");
}

Registers filled() {
    Registers regs{};
    std::memset(regs.data(), 0xcc, sizeof(regs));
    return regs;
}

void makeGs(GsSetup& setup, std::uint32_t onchip, std::uint32_t subgroup, std::uint32_t vsOut, std::uint32_t clOut, std::uint32_t maxOutput, bool wave32) {
    setup.cx = {{
        {ShaderRegs::VGT_GS_ONCHIP_CNTL, onchip},
        {ShaderRegs::GE_NGG_SUBGRP_CNTL, subgroup},
        {ShaderRegs::SPI_VS_OUT_CONFIG, vsOut},
        {ShaderRegs::PA_CL_VS_OUT_CNTL, clOut},
        {ShaderRegs::GE_MAX_OUTPUT_PER_SUBGROUP, maxOutput},
    }};
    setup.specials = {};
    setup.specials.vgt_shader_stages_en = {ShaderRegs::VGT_SHADER_STAGES_EN, wave32 ? 0x00400000u : 0u};
    setup.shader = {};
    setup.shader.cx_registers = setup.cx.data();
    setup.shader.num_cx_registers = static_cast<std::uint8_t>(setup.cx.size());
    setup.shader.specials = &setup.specials;
}

void expectRegs(const Shader* gs, std::uint32_t budget, float factor, std::uint32_t pcAlloc, std::uint32_t rsrc4, const char* message) {
    auto regs = filled();
    check(sceAgcGetGsOversubscription(regs.data(), gs, budget, factor) == 0, message);
    check(regs[0].offset == ShaderRegs::GE_PC_ALLOC && regs[1].offset == ShaderRegs::SPI_SHADER_PGM_RSRC4_GS, message);
    check(regs[0].value == pcAlloc && regs[1].value == rsrc4, message);
}

void testBudgetLimits() {
    expectRegs(nullptr, 0, 0.5f, 0, 0, "zero budget does not disable oversubscription");
    expectRegs(nullptr, std::numeric_limits<std::uint32_t>::max(), 0.5f, 0x7ffu, 0x7f0000u, "unlimited budget does not allow full oversubscription");
}

void testVertexBound() {
    GsSetup setup;
    makeGs(setup, 2u << 11u, 4, 3u << 2u, 0, 64, false);
    expectRegs(&setup.shader, 1u << 20u, 0.5f, 0x3ffu, 0x7f0000u, "vertex-bound oversubscription changed");
    expectRegs(&setup.shader, 64, 0.5f, 0, 0, "budget below the base occupancy changed the registers");
    expectRegs(&setup.shader, 1u << 20u, 0.0f, 0, 0, "zero factor changed the registers");
    expectRegs(&setup.shader, 1u << 20u, -1.0f, 0, 0, "factor reaching a zero target changed the registers");
}

void testExportBound() {
    GsSetup setup;
    makeGs(setup, 8u << 11u, 8, 0, 7u << 21u, 64, true);
    expectRegs(&setup.shader, 1u << 20u, 0.75f, 0x7ffu, 0x5f0000u, "export-bound oversubscription changed");
    expectRegs(&setup.shader, 1u << 20u, 4.0f, 0x7ffu, 0x7f0000u, "factor above one is not clamped");
    makeGs(setup, 1u << 11u, 1, 0x80u, 1u << 21u, 32, false);
    expectRegs(&setup.shader, 1u << 20u, 0.25f, 0x7ffu, 0x1f0000u, "oversubscription without parameter cache exports changed");
}

void testRegisterFields() {
    GsSetup setup;
    makeGs(setup, 5u << 11u, 2, 2u << 2u, 0, 128, false);
    expectRegs(&setup.shader, 16384, 0.75f, 0x301u, 0x7f0000u, "wave64 subgroup waves are not halved");
    makeGs(setup, 13u << 11u, 4, 0, 3u << 22u, 32, false);
    expectRegs(&setup.shader, 16384, 0.5f, 0x7ffu, 0x100000u, "position export count changed");
    makeGs(setup, 10u << 11u, 1, 3u << 2u, 7u << 21u, 32, true);
    expectRegs(&setup.shader, 8192, 0.25f, 0x7ffu, 0x100000u, "wave32 budget shift changed");
    makeGs(setup, 6u << 11u, 8, 0x80u, 0, 96, false);
    expectRegs(&setup.shader, 32768, 0.5f, 0x7ffu, 0x3f0000u, "vertex limit without parameter cache exports changed");
    makeGs(setup, 12u << 11u, 7, 0, 1u << 22u, 32, true);
    expectRegs(&setup.shader, 32768, 0.25f, 0x7ffu, 0x50000u, "primitive amplification factor is ignored");
}

void testRejections() {
    GsSetup setup;
    makeGs(setup, 2u << 11u, 4, 3u << 2u, 0, 64, false);
    auto regs = filled();
    const auto saved = regs;
    expectFailure([&] { sceAgcGetGsOversubscription(nullptr, &setup.shader, 1u << 20u, 0.5f); });
    expectFailure([&] { sceAgcGetGsOversubscription(regs.data(), nullptr, 1u << 20u, 0.5f); });
    expectFailure([&] { sceAgcGetGsOversubscription(regs.data(), &setup.shader, 1u << 20u, -2.0f); });
    expectFailure([&] { sceAgcGetGsOversubscription(regs.data(), &setup.shader, 1u << 20u, std::nanf("")); });
    setup.shader.specials = nullptr;
    expectFailure([&] { sceAgcGetGsOversubscription(regs.data(), &setup.shader, 1u << 20u, 0.5f); });
    setup.shader.specials = &setup.specials;
    setup.cx[4].value = 0;
    expectFailure([&] { sceAgcGetGsOversubscription(regs.data(), &setup.shader, 1u << 20u, 0.5f); });
    setup.shader.num_cx_registers = 4;
    expectFailure([&] { sceAgcGetGsOversubscription(regs.data(), &setup.shader, 1u << 20u, 0.5f); });
    check(std::memcmp(regs.data(), saved.data(), sizeof(regs)) == 0, "rejected call wrote registers");
}

}

int main() {
    try {
        testBudgetLimits();
        testVertexBound();
        testExportBound();
        testRegisterFields();
        testRejections();
        LibcRunShutdown_nid_postfix();
        std::puts("AGC GS oversubscription tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try { LibcRunShutdown_nid_postfix(); }
        catch (const std::exception& shutdown) { std::fprintf(stderr, "shutdown: %s\n", shutdown.what()); }
        return 1;
    }
}
