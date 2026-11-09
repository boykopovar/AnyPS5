#include "prx/libSceGnmDriver/libSceGnmDriver.hpp"
#include "prx/libSceGnmDriverForNeoMode/libSceGnmDriverForNeoMode.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#include <cstdio>
#include <source_location>

namespace {

#if defined(GNM_NORMAL_PROVIDER)
constexpr auto DispatchDirect = sceGnmDispatchDirect_nid_disambig1;
constexpr auto DispatchIndirect = sceGnmDispatchIndirect_nid_disambig1;
constexpr auto DispatchIndirectOnMec = sceGnmDispatchIndirectOnMec_nid_disambig1;
constexpr auto SetCsShaderWithModifier = sceGnmSetCsShaderWithModifier_nid_disambig1;
constexpr auto SetPsShader350 = sceGnmSetPsShader350_nid_disambig1;
constexpr auto UpdateGsShader = sceGnmUpdateGsShader_nid_disambig1;
constexpr auto UpdateHsShader = sceGnmUpdateHsShader_nid_disambig1;
constexpr auto UpdatePsShader = sceGnmUpdatePsShader_nid_disambig1;
constexpr auto UpdatePsShader350 = sceGnmUpdatePsShader350_nid_disambig1;
constexpr auto UpdateVsShader = sceGnmUpdateVsShader_nid_disambig1;
#else
constexpr auto DispatchDirect = sceGnmDispatchDirect;
constexpr auto DispatchIndirect = sceGnmDispatchIndirect;
constexpr auto DispatchIndirectOnMec = sceGnmDispatchIndirectOnMec;
constexpr auto SetCsShaderWithModifier = sceGnmSetCsShaderWithModifier;
constexpr auto SetPsShader350 = sceGnmSetPsShader350;
constexpr auto UpdateGsShader = sceGnmUpdateGsShader;
constexpr auto UpdateHsShader = sceGnmUpdateHsShader;
constexpr auto UpdatePsShader = sceGnmUpdatePsShader;
constexpr auto UpdatePsShader350 = sceGnmUpdatePsShader350;
constexpr auto UpdateVsShader = sceGnmUpdateVsShader;
#endif


constexpr std::uint32_t Untouched = 0xa5a5a5a5u;
constexpr std::uint32_t U = Untouched;
constexpr std::size_t BufferWords = 48;
constexpr std::uint32_t Regs[12] = {0x1000, 0, 0x2000, 0x3000, 0x4000, 0x5000, 0x6000, 0x7000, 0x8000, 0x9000, 0xa000, 0xb000};
constexpr std::uint32_t BadRegs[12] = {0x1000, 1};
constexpr std::uint32_t MaskedRegs[12] = {0x1000, 0, 0x030003c0u, 0x3000, 0x4000, 0x5000, 0x6000};

std::uint32_t g_words[BufferWords];

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "GNM shared writer check failed at line %u\n", location.line());
        std::abort();
    }
}

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
    Require(UpdateVsShader(Fresh(), 0x1d, Regs, 0) == 0);
    RequireWords({0xc0027600u, 0x48, 0x1000, 0, 0xc0027600u, 0x4a, 0x2000, 0x3000, 0xc0011000u, 0xc01e0207u, 0x6000,
        0xc0011000u, 0xc01e01b1u, 0x4000, 0xc0011000u, 0xc01e01c3u, 0x5000, 0xc00a1000u, 0});
    Require(UpdateVsShader(Fresh(), 0x1d, MaskedRegs, 0x40) == 0);
    Require(g_words[6] == 0x40);
    Require(UpdateVsShader(Fresh(), 0x1d, MaskedRegs, 0) == 0);
    Require(g_words[6] == 0x030003c0u);
    RequireRefused(UpdateVsShader(Fresh(), 0x1d, Regs, 0x80000000u));
    RequireRefused(UpdateVsShader(Fresh(), 0x1c, Regs, 0));
    RequireRefused(UpdateVsShader(Fresh(), 0x1d, BadRegs, 0));
    RequireRefused(UpdateVsShader(Fresh(), 0x1d, nullptr, 0));
}

void CheckPs() {
    Require(UpdatePsShader(Fresh(), 0x28, Regs) == 0);
    RequireWords({0xc0027600u, 8, 0x1000, 0, 0xc0027600u, 10, 0x2000, 0x3000, 0xc0021000u, 0xc01e01c4u, 0x4000, 0x5000,
        0xc0021000u, 0xc01e01b3u, 0x6000, 0x7000, 0xc0011000u, 0xc01e01b6u, 0x8000, 0xc0011000u, 0xc01e01b8u, 0x9000,
        0xc0011000u, 0xc01e0203u, 0xa000, 0xc0011000u, 0xc01e008fu, 0xb000, 0xc00a1000u, 0});
    Require(UpdatePsShader(Fresh(), 0x28, nullptr) == 0);
    RequireWords({0xc0027600u, 8, 0, 0, 0xc0011000u, 0xc01e0203u, 0, 0xc01f1000u, 0});
    RequireRefused(UpdatePsShader(Fresh(), 0x27, Regs));
    RequireRefused(UpdatePsShader(Fresh(), 0x28, BadRegs));

    Require(UpdatePsShader350(Fresh(), 0x28, Regs) == 0);
    Require(g_words[26] == 0xc01e008fu && g_words[28] == 0xc00a1000u);
    Require(UpdatePsShader350(Fresh(), 0x28, nullptr) == 0);
    RequireWords({0xc0027600u, 8, 0, 0, 0xc0011000u, 0xc01e0203u, 0, 0xc0016900u, 0x8f, 0xf, 0xc01c1000u, 0});
    RequireRefused(UpdatePsShader350(Fresh(), 0x27, nullptr));
    RequireRefused(UpdatePsShader350(Fresh(), 0x28, BadRegs));

    Require(SetPsShader350(Fresh(), 0x28, Regs) == 0);
    RequireWords({0xc0027600u, 8, 0x1000, 0, 0xc0027600u, 10, 0x2000, 0x3000, 0xc0026900u, 0x1c4, 0x4000, 0x5000,
        0xc0026900u, 0x1b3, 0x6000, 0x7000, 0xc0016900u, 0x1b6, 0x8000, 0xc0016900u, 0x1b8, 0x9000,
        0xc0016900u, 0x203, 0xa000, 0xc0016900u, 0x8f, 0xb000, 0xc00a1000u, 0});
    Require(SetPsShader350(Fresh(), 0x28, nullptr) == 0);
    RequireWords({0xc0027600u, 8, 0, 0, 0xc0016900u, 0x203, 0, 0xc0016900u, 0x8f, 0xf, 0xc01c1000u, 0});
    RequireRefused(SetPsShader350(Fresh(), 0x27, Regs));
    RequireRefused(SetPsShader350(Fresh(), 0x28, BadRegs));
}

void CheckGsHsCs() {
    Require(UpdateGsShader(Fresh(), 0x1d, Regs) == 0);
    RequireWords({0xc0027600u, 0x88, 0x1000, 0, 0xc0027600u, 0x8a, 0x2000, 0x3000, 0xc0011000u, 0xc01e02e5u, 0x4000,
        0xc0011000u, 0xc01e029bu, 0x5000, 0xc0011000u, 0xc01e02e4u, 0x6000, 0xc00a1000u, 0});
    RequireRefused(UpdateGsShader(Fresh(), 0x1c, Regs));
    RequireRefused(UpdateGsShader(Fresh(), 0x1d, nullptr));
    RequireRefused(UpdateGsShader(Fresh(), 0x1d, BadRegs));

    Require(UpdateHsShader(Fresh(), 0x1d, Regs, 0x77) == 0);
    RequireWords({0xc0027600u, 0x108, 0x1000, 0, 0xc0027600u, 0x10a, 0x2000, 0x3000, 0xc0021000u, 0xc01e0286u, 0x5000, 0x6000,
        0xc0011000u, 0xc01e02dbu, 0x4000, 0xc0011000u, 0xc01e02d6u, 0x77, 0xc00a1000u, 0});
    RequireRefused(UpdateHsShader(Fresh(), 0x1c, Regs, 0));
    RequireRefused(UpdateHsShader(Fresh(), 0x1d, BadRegs, 0));

    Require(SetCsShaderWithModifier(Fresh(), 0x19, Regs, 0x3c0) == 0);
    RequireWords({0xc0027600u, 0x20c, 0x1000, 0, 0xc0027600u, 0x212, 0x23c0, 0x3000, 0xc0037600u, 0x207, 0x4000, 0x5000, 0x6000,
        0xc00a1000u, 0});
    Require(SetCsShaderWithModifier(Fresh(), 0x19, MaskedRegs, 0x40) == 0);
    Require(g_words[6] == 0x03000040u);
    Require(SetCsShaderWithModifier(Fresh(), 0x19, MaskedRegs, 0) == 0);
    Require(g_words[6] == 0x030003c0u);
    RequireRefused(SetCsShaderWithModifier(Fresh(), 0x19, Regs, 0x400));
    RequireRefused(SetCsShaderWithModifier(Fresh(), 0x18, Regs, 0));
    RequireRefused(SetCsShaderWithModifier(Fresh(), 0x19, BadRegs, 0));
}

void CheckDispatches() {
    Require(DispatchDirect(Fresh(), 9, 8, 4, 2, 0x19) == 0);
    RequireWords({0xc0031503u, 8, 4, 2, 0x19, 0xc0021000u, 0, U, U});
    RequireRefused(DispatchDirect(Fresh(), 9, 0x80000000u, 1, 1, 0));
    RequireRefused(DispatchDirect(Fresh(), 8, 1, 1, 1, 0));

    Require(DispatchIndirect(Fresh(), 7, 0x40, 0x8) == 0);
    RequireWords({0xc0011602u, 0x40, 9, 0xc0021000u, 0, U, U});
    RequireRefused(DispatchIndirect(Fresh(), 9, 0x40, 0));

    Require(DispatchIndirectOnMec(Fresh(), 8, 0x700001000ull, 0x19) == 0);
    RequireWords({0xc0021603u, 0x1000, 7, 0x19, 0xc0021000u, 0});
    RequireRefused(DispatchIndirectOnMec(Fresh(), 8, 0x700001002ull, 0));
    RequireRefused(DispatchIndirectOnMec(Fresh(), 8, 0, 0));
}


void CheckNullBuffers() {
    Require(DispatchDirect(nullptr, 9, 1, 1, 1, 0) == -1);
    Require(DispatchIndirect(nullptr, 7, 0, 0) == -1);
    Require(DispatchIndirectOnMec(nullptr, 8, 4, 0) == -1);
    Require(UpdateVsShader(nullptr, 0x1d, Regs, 0) == -1);
    Require(UpdatePsShader(nullptr, 0x28, Regs) == -1);
    Require(UpdatePsShader350(nullptr, 0x28, Regs) == -1);
    Require(SetPsShader350(nullptr, 0x28, Regs) == -1);
    Require(UpdateGsShader(nullptr, 0x1d, Regs) == -1);
    Require(UpdateHsShader(nullptr, 0x1d, Regs, 0) == -1);
    Require(SetCsShaderWithModifier(nullptr, 0x19, Regs, 0) == -1);
}

void CheckDispatchFlags() {
    for (std::uint32_t flags = 0; flags < 32; ++flags) {
        Require(DispatchDirect(Fresh(), 9, 0, 0x7fffffff, 1, flags) == 0);
        RequireWords({0xc0031502u | (flags & 1), 0, 0x7fffffff, 1, (flags & 0x18) | 1, 0xc0021000u, 0});
        Require(DispatchIndirect(Fresh(), 7, 0xffffffffu, flags) == 0);
        RequireWords({0xc0011602u | (flags & 1), 0xffffffffu, (flags & 0x18) | 1, 0xc0021000u, 0});
        Require(DispatchIndirectOnMec(Fresh(), 8, 0x123456789abcull, flags) == 0);
        RequireWords({0xc0021602u | (flags & 1), 0x56789abcu, 0x1234, (flags & 0x18) | 1, 0xc0021000u, 0});
    }
    RequireRefused(DispatchDirect(Fresh(), 9, 1, 0x80000000u, 1, 0));
    RequireRefused(DispatchDirect(Fresh(), 9, 1, 1, 0xffffffffu, 0));
    RequireRefused(DispatchDirect(Fresh(), 10, 1, 1, 1, 0));
    RequireRefused(DispatchIndirect(Fresh(), 6, 0, 0));
    RequireRefused(DispatchIndirectOnMec(Fresh(), 7, 4, 0));
    RequireRefused(DispatchIndirectOnMec(Fresh(), 9, 4, 0));
    for (const auto address : {1ull, 2ull, 3ull, 0x123456789abdull})
        RequireRefused(DispatchIndirectOnMec(Fresh(), 8, address, 0));
}

} // namespace

int main() {
    CheckVs();
    CheckPs();
    CheckGsHsCs();
    CheckDispatches();
    CheckNullBuffers();
    CheckDispatchFlags();
}
