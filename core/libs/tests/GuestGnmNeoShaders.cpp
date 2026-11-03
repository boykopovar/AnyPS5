#include "prx/libSceGnmDriverForNeoMode/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <initializer_list>

namespace {

constexpr std::uint32_t Untouched = 0xa5a5a5a5u;
constexpr std::size_t BufferWords = 48;
constexpr std::uint32_t Regs[12] = {0x1000, 0, 0x2000, 0x3000, 0x4000, 0x5000, 0x6000, 0x7000, 0x8000, 0x9000, 0xa000, 0xb000};
constexpr std::uint32_t BadRegs[12] = {0x1000, 1};
constexpr std::uint32_t MaskedRegs[12] = {0x1000, 0, 0x030003c0u, 0x3000, 0x4000, 0x5000, 0x6000};

std::uint32_t g_words[BufferWords];

void Require(bool value) { if (!value) std::abort(); }

std::uint32_t* Fresh() {
    for (auto& word : g_words) word = Untouched;
    return g_words;
}

void RequireWords(std::initializer_list<std::uint32_t> expected) {
    std::size_t at = 0;
    for (const auto word : expected) Require(g_words[at++] == word);
    for (; at < BufferWords; ++at) Require(g_words[at] == Untouched);
}

void RequireRefused(int result) {
    Require(result == -1);
    RequireWords({});
}

void CheckVs() {
    Require(sceGnmUpdateVsShader(Fresh(), 0x1d, Regs, 0) == 0);
    RequireWords({0xc0027600u, 0x48, 0x1000, 0, 0xc0027600u, 0x4a, 0x2000, 0x3000, 0xc0011000u, 0xc01e0207u, 0x6000,
        0xc0011000u, 0xc01e01b1u, 0x4000, 0xc0011000u, 0xc01e01c3u, 0x5000, 0xc00a1000u, 0});
    Require(sceGnmUpdateVsShader(Fresh(), 0x1d, MaskedRegs, 0x40) == 0);
    Require(g_words[6] == 0x40);
    Require(sceGnmUpdateVsShader(Fresh(), 0x1d, MaskedRegs, 0) == 0);
    Require(g_words[6] == 0x030003c0u);
    RequireRefused(sceGnmUpdateVsShader(Fresh(), 0x1d, Regs, 0x80000000u));
    RequireRefused(sceGnmUpdateVsShader(Fresh(), 0x1c, Regs, 0));
    RequireRefused(sceGnmUpdateVsShader(Fresh(), 0x1d, BadRegs, 0));
    RequireRefused(sceGnmUpdateVsShader(Fresh(), 0x1d, nullptr, 0));
}

void CheckPs() {
    Require(sceGnmUpdatePsShader(Fresh(), 0x28, Regs) == 0);
    RequireWords({0xc0027600u, 8, 0x1000, 0, 0xc0027600u, 10, 0x2000, 0x3000, 0xc0021000u, 0xc01e01c4u, 0x4000, 0x5000,
        0xc0021000u, 0xc01e01b3u, 0x6000, 0x7000, 0xc0011000u, 0xc01e01b6u, 0x8000, 0xc0011000u, 0xc01e01b8u, 0x9000,
        0xc0011000u, 0xc01e0203u, 0xa000, 0xc0011000u, 0xc01e008fu, 0xb000, 0xc00a1000u, 0});
    Require(sceGnmUpdatePsShader(Fresh(), 0x28, nullptr) == 0);
    RequireWords({0xc0027600u, 8, 0, 0, 0xc0011000u, 0xc01e0203u, 0, 0xc01f1000u, 0});
    RequireRefused(sceGnmUpdatePsShader(Fresh(), 0x27, Regs));
    RequireRefused(sceGnmUpdatePsShader(Fresh(), 0x28, BadRegs));

    Require(sceGnmUpdatePsShader350(Fresh(), 0x28, Regs) == 0);
    Require(g_words[26] == 0xc01e008fu && g_words[28] == 0xc00a1000u);
    Require(sceGnmUpdatePsShader350(Fresh(), 0x28, nullptr) == 0);
    RequireWords({0xc0027600u, 8, 0, 0, 0xc0011000u, 0xc01e0203u, 0, 0xc0016900u, 0x8f, 0xf, 0xc01c1000u, 0});
    RequireRefused(sceGnmUpdatePsShader350(Fresh(), 0x27, nullptr));
    RequireRefused(sceGnmUpdatePsShader350(Fresh(), 0x28, BadRegs));

    Require(sceGnmSetPsShader350(Fresh(), 0x28, Regs) == 0);
    RequireWords({0xc0027600u, 8, 0x1000, 0, 0xc0027600u, 10, 0x2000, 0x3000, 0xc0026900u, 0x1c4, 0x4000, 0x5000,
        0xc0026900u, 0x1b3, 0x6000, 0x7000, 0xc0016900u, 0x1b6, 0x8000, 0xc0016900u, 0x1b8, 0x9000,
        0xc0016900u, 0x203, 0xa000, 0xc0016900u, 0x8f, 0xb000, 0xc00a1000u, 0});
    Require(sceGnmSetPsShader350(Fresh(), 0x28, nullptr) == 0);
    RequireWords({0xc0027600u, 8, 0, 0, 0xc0016900u, 0x203, 0, 0xc0016900u, 0x8f, 0xf, 0xc01c1000u, 0});
    RequireRefused(sceGnmSetPsShader350(Fresh(), 0x27, Regs));
    RequireRefused(sceGnmSetPsShader350(Fresh(), 0x28, BadRegs));
}

void CheckGsHsCs() {
    Require(sceGnmUpdateGsShader(Fresh(), 0x1d, Regs) == 0);
    RequireWords({0xc0027600u, 0x88, 0x1000, 0, 0xc0027600u, 0x8a, 0x2000, 0x3000, 0xc0011000u, 0xc01e02e5u, 0x4000,
        0xc0011000u, 0xc01e029bu, 0x5000, 0xc0011000u, 0xc01e02e4u, 0x6000, 0xc00a1000u, 0});
    RequireRefused(sceGnmUpdateGsShader(Fresh(), 0x1c, Regs));
    RequireRefused(sceGnmUpdateGsShader(Fresh(), 0x1d, nullptr));
    RequireRefused(sceGnmUpdateGsShader(Fresh(), 0x1d, BadRegs));

    Require(sceGnmUpdateHsShader(Fresh(), 0x1d, Regs, 0x77) == 0);
    RequireWords({0xc0027600u, 0x108, 0x1000, 0, 0xc0027600u, 0x10a, 0x2000, 0x3000, 0xc0021000u, 0xc01e0286u, 0x5000, 0x6000,
        0xc0011000u, 0xc01e02dbu, 0x4000, 0xc0011000u, 0xc01e02d6u, 0x77, 0xc00a1000u, 0});
    RequireRefused(sceGnmUpdateHsShader(Fresh(), 0x1c, Regs, 0));
    RequireRefused(sceGnmUpdateHsShader(Fresh(), 0x1d, BadRegs, 0));

    Require(sceGnmSetCsShaderWithModifier(Fresh(), 0x19, Regs, 0x3c0) == 0);
    RequireWords({0xc0027600u, 0x20c, 0x1000, 0, 0xc0027600u, 0x212, 0x23c0, 0x3000, 0xc0037600u, 0x207, 0x4000, 0x5000, 0x6000,
        0xc00a1000u, 0});
    Require(sceGnmSetCsShaderWithModifier(Fresh(), 0x19, MaskedRegs, 0x40) == 0);
    Require(g_words[6] == 0x03000040u);
    Require(sceGnmSetCsShaderWithModifier(Fresh(), 0x19, MaskedRegs, 0) == 0);
    Require(g_words[6] == 0x030003c0u);
    RequireRefused(sceGnmSetCsShaderWithModifier(Fresh(), 0x19, Regs, 0x400));
    RequireRefused(sceGnmSetCsShaderWithModifier(Fresh(), 0x18, Regs, 0));
    RequireRefused(sceGnmSetCsShaderWithModifier(Fresh(), 0x19, BadRegs, 0));
}

void CheckWorkloads() {
    std::uint32_t stream = Untouched;
    Require(sceGnmCreateWorkloadStream(0, &stream) == 3 && stream == Untouched);
    Require(sceGnmCreateWorkloadStream(5, nullptr) == 3);
    Require(sceGnmCreateWorkloadStream(5, &stream) == 0 && stream == 1);

    std::uint64_t workload = Untouched;
    Require(sceGnmBeginWorkload(3, &workload) == 0 && workload == 1);
    Require(sceGnmBeginWorkload(0x10, &workload) == 1 && workload == 0);
    Require(sceGnmBeginWorkload(0, nullptr) == 3);

    Require(sceGnmEndWorkload(0) == 2);
    Require(sceGnmEndWorkload(1) == 0);
    Require(sceGnmEndWorkload(0x0full << 56) == 0);
    Require(sceGnmEndWorkload(0x10ull << 56) == 2);
}

} // namespace

int main() {
    CheckVs();
    CheckPs();
    CheckGsHsCs();
    CheckWorkloads();
    Require(sceGnmGetGpuCoreClockFrequency() == 911000000);
    Require(sceGnmGetOffChipTessellationBufferSize() == 0x800000);
}
