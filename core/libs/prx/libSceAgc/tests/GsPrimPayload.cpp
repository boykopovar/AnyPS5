#include "prx/libSceAgc/Shader/include/GsPrimPayload.hpp"
#include "SceShaders.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {

constexpr std::uint32_t Sentinel = 0xccccccccu;

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename TAction>
void expectFailure(TAction action, const char* message) {
    try {
        action();
    } catch (const std::exception& error) {
        check(error.what()[0] != '\0', "empty exception message");
        return;
    }
    throw std::runtime_error(message);
}

Shader makeGs(std::vector<ShaderRegister>& cx) {
    Shader gs{};
    gs.cx_registers = cx.empty() ? nullptr : cx.data();
    gs.num_cx_registers = static_cast<std::uint8_t>(cx.size());
    return gs;
}

void expectPayload(const Shader& gs, std::uint32_t expected, const char* message) {
    std::array<std::uint32_t, 2> out{Sentinel, Sentinel};
    check(sceAgcGetGsPrimPayload(out.data(), &gs) == 0, message);
    check(out[0] == expected, message);
    check(out[1] == Sentinel, "the payload write went past four bytes");
}

void testNoRegisters() {
    std::vector<ShaderRegister> cx;
    expectPayload(makeGs(cx), 0, "a shader without context registers reported a payload");
}

void testTwoComponentIndexFormat() {
    std::vector<ShaderRegister> cx{{ShaderRegs::SPI_SHADER_IDX_FORMAT, 2u}};
    expectPayload(makeGs(cx), 8, "a two-component index export did not report 8 bytes");
}

void testOnlyTheLowNibbleCounts() {
    std::vector<ShaderRegister> high{{ShaderRegs::SPI_SHADER_IDX_FORMAT, 0xfffffff2u}};
    expectPayload(makeGs(high), 8, "bits above IDX0_EXPORT_FORMAT changed the payload");
    for (const std::uint32_t format : {0u, 1u, 3u, 4u, 0x20u}) {
        std::vector<ShaderRegister> cx{{ShaderRegs::SPI_SHADER_IDX_FORMAT, format}};
        expectPayload(makeGs(cx), 0, "an index export format other than two components reported a payload");
    }
}

void testOtherRegistersAreIgnored() {
    std::vector<ShaderRegister> cx{{ShaderRegs::SPI_VS_OUT_CONFIG, 2u}, {ShaderRegs::GE_MAX_OUTPUT_PER_SUBGROUP, 2u}};
    expectPayload(makeGs(cx), 0, "a register other than SPI_SHADER_IDX_FORMAT reported a payload");
}

void testSearchContinuesPastAMismatch() {
    std::vector<ShaderRegister> cx{
        {ShaderRegs::SPI_VS_OUT_CONFIG, 0u},
        {ShaderRegs::SPI_SHADER_IDX_FORMAT, 4u},
        {ShaderRegs::SPI_SHADER_IDX_FORMAT, 2u},
    };
    expectPayload(makeGs(cx), 8, "a non-matching SPI_SHADER_IDX_FORMAT entry stopped the search");
}

void testCountBoundsTheSearch() {
    std::vector<ShaderRegister> cx{{ShaderRegs::SPI_VS_OUT_CONFIG, 0u}, {ShaderRegs::SPI_SHADER_IDX_FORMAT, 2u}};
    Shader gs = makeGs(cx);
    gs.num_cx_registers = 1;
    expectPayload(gs, 0, "an entry past num_cx_registers was read");
}

void testRejections() {
    std::vector<ShaderRegister> cx{{ShaderRegs::SPI_SHADER_IDX_FORMAT, 2u}};
    const Shader gs = makeGs(cx);
    std::uint32_t out = Sentinel;
    expectFailure([&] { sceAgcGetGsPrimPayload(nullptr, &gs); }, "a null payload pointer was accepted");
    expectFailure([&] { sceAgcGetGsPrimPayload(&out, nullptr); }, "a null shader was accepted");
    Shader missing{};
    missing.num_cx_registers = 1;
    expectFailure([&] { sceAgcGetGsPrimPayload(&out, &missing); }, "a register count without a register array was accepted");
    check(out == Sentinel, "a rejected call wrote the payload");
}

}

int main() {
    try {
        testNoRegisters();
        testTwoComponentIndexFormat();
        testOnlyTheLowNibbleCounts();
        testOtherRegistersAreIgnored();
        testSearchContinuesPastAMismatch();
        testCountBoundsTheSearch();
        testRejections();
        LibcRunShutdown_nid_postfix();
        std::puts("AGC GS primitive payload tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try { LibcRunShutdown_nid_postfix(); }
        catch (const std::exception& shutdown) { std::fprintf(stderr, "shutdown: %s\n", shutdown.what()); }
        return 1;
    }
}
