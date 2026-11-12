#include "prx/libSceGnmDriver/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace {

constexpr std::uint32_t Untouched = 0xdeadbeefu;
constexpr std::uint32_t Regs[12] = {0x1000, 0, 0x2000, 0x3000, 0x4000, 0x5000, 0x6000, 0x7000, 0x8000, 0x9000, 0xa000, 0xb000};
constexpr std::uint32_t BadRegs[12] = {0x1000, 1};

void Require(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("Gnm shader writers: " + what);
}

constexpr std::uint32_t Header(std::uint32_t opcode, std::uint32_t bodyWords) {
    return 0xc0000000u | ((bodyWords - 1) << 16) | (opcode << 8);
}

bool WholePackets(const std::uint32_t* words, std::size_t count) {
    std::size_t at = 0;
    while (at < count) {
        if ((words[at] >> 30) != 3) return false;
        at += ((words[at] >> 16) & 0x3fffu) + 2;
    }
    return at == count;
}

std::array<std::uint32_t, 0x100> Fresh() {
    std::array<std::uint32_t, 0x100> words{};
    words.fill(Untouched);
    return words;
}

void TestPs() {
    auto words = Fresh();
    Require(sceGnmSetPsShader(words.data(), 0x100, Regs) == 0, "the pixel shader failed");
    Require(words[0] == Header(0x76, 3) && words[1] == 8 && words[2] == Regs[0] && words[3] == 0, "the pixel shader does not start with SET_SH_REG 8");
    Require(words[4] == Header(0x76, 3) && words[5] == 10 && words[6] == Regs[2] && words[7] == Regs[3], "the pixel shader does not write SET_SH_REG 10");
    Require(words[8] == Header(0x69, 3) && words[9] == 0x1c4 && words[10] == Regs[4] && words[11] == Regs[5], "the pixel shader does not write SET_CONTEXT_REG 0x1c4");
    Require(words[12] == Header(0x69, 3) && words[13] == 0x1b3 && words[14] == Regs[6] && words[15] == Regs[7], "the pixel shader does not write SET_CONTEXT_REG 0x1b3");
    Require(words[16] == Header(0x69, 2) && words[17] == 0x1b6 && words[18] == Regs[8], "the pixel shader does not write SET_CONTEXT_REG 0x1b6");
    Require(words[19] == Header(0x69, 2) && words[20] == 0x1b8 && words[21] == Regs[9], "the pixel shader does not write SET_CONTEXT_REG 0x1b8");
    Require(words[22] == Header(0x69, 2) && words[23] == 0x203 && words[24] == Regs[10], "the pixel shader does not write SET_CONTEXT_REG 0x203");
    Require(words[25] == Header(0x69, 2) && words[26] == 0x8f && words[27] == Regs[11], "the pixel shader does not write SET_CONTEXT_REG 0x8f");
    Require(words[28] == Header(0x10, 11), "the pixel shader does not end with the 11-word NOP");
    Require(WholePackets(words.data(), 40), "the pixel shader is not whole PM4 packets");

    words = Fresh();
    Require(sceGnmSetPsShader(words.data(), 0x100, nullptr) == 0, "the null-register pixel shader failed");
    Require(words[0] == Header(0x76, 3) && words[1] == 8 && words[2] == 0 && words[3] == 0, "the null-register pixel shader does not clear SET_SH_REG 8");
    Require(WholePackets(words.data(), 40), "the null-register pixel shader is not whole PM4 packets");

    words = Fresh();
    Require(sceGnmSetPsShader(words.data(), 0x27, Regs) == -1 && words[0] == Untouched, "a short pixel shader buffer was accepted");
    Require(sceGnmSetPsShader(words.data(), 0x100, BadRegs) == -1 && words[0] == Untouched, "a pixel shader with a non-zero second register was accepted");
    Require(sceGnmSetPsShader(nullptr, 0x100, Regs) == -1, "a null pixel shader buffer was accepted");
}

void TestVs() {
    auto words = Fresh();
    Require(sceGnmSetVsShader(words.data(), 0x100, Regs, 0) == 0, "the vertex shader failed");
    Require(words[0] == Header(0x76, 3) && words[1] == 0x48 && words[2] == Regs[0] && words[3] == 0, "the vertex shader does not start with SET_SH_REG 0x48");
    Require(words[4] == Header(0x76, 3) && words[5] == 0x4a && words[6] == Regs[2] && words[7] == Regs[3], "the vertex shader does not write SET_SH_REG 0x4a");
    Require(words[8] == Header(0x69, 2) && words[9] == 0x207 && words[10] == Regs[6], "the vertex shader does not write SET_CONTEXT_REG 0x207");
    Require(words[11] == Header(0x69, 2) && words[12] == 0x1b1 && words[13] == Regs[4], "the vertex shader does not write SET_CONTEXT_REG 0x1b1");
    Require(words[14] == Header(0x69, 2) && words[15] == 0x1c3 && words[16] == Regs[5], "the vertex shader does not write SET_CONTEXT_REG 0x1c3");
    Require(WholePackets(words.data(), 29), "the vertex shader is not whole PM4 packets");

    words = Fresh();
    Require(sceGnmSetVsShader(words.data(), 0x100, Regs, 0x40) == 0 && words[6] == (Regs[2] | 0x40), "the vertex shader modifier was not merged into the resource word");
    Require(sceGnmSetVsShader(words.data(), 0x1c, Regs, 0) == -1, "a short vertex shader buffer was accepted");
    Require(sceGnmSetVsShader(words.data(), 0x100, Regs, 0x80000000u) == -1, "an out-of-range vertex shader modifier was accepted");
}

void TestCs() {
    auto words = Fresh();
    Require(sceGnmSetCsShader(words.data(), 0x100, Regs) == 0, "the compute shader failed");
    Require(words[0] == Header(0x76, 3) && words[1] == 0x20c && words[2] == Regs[0] && words[3] == 0, "the compute shader does not start with SET_SH_REG 0x20c");
    Require(words[4] == Header(0x76, 3) && words[5] == 0x212 && words[6] == Regs[2] && words[7] == Regs[3], "the compute shader does not write SET_SH_REG 0x212");
    Require(words[8] == Header(0x76, 4) && words[9] == 0x207 && words[10] == Regs[4] && words[11] == Regs[5] && words[12] == Regs[6], "the compute shader does not write SET_SH_REG 0x207");
    Require(WholePackets(words.data(), 25), "the compute shader is not whole PM4 packets");
    Require(sceGnmSetCsShader(words.data(), 0x18, Regs) == -1, "a short compute shader buffer was accepted");
}

void TestEs() {
    auto words = Fresh();
    Require(sceGnmSetEsShader(words.data(), 0x100, Regs, 0) == 0, "the export shader failed");
    Require(words[0] == Header(0x76, 3) && words[1] == 0xc8 && words[2] == Regs[0] && words[3] == 0, "the export shader does not start with SET_SH_REG 0xc8");
    Require(words[4] == Header(0x76, 3) && words[5] == 0xca && words[6] == Regs[2] && words[7] == Regs[3], "the export shader does not write SET_SH_REG 0xca");
    Require(WholePackets(words.data(), 20), "the export shader is not whole PM4 packets");
    Require(sceGnmSetEsShader(words.data(), 0x100, Regs, 0x40) == 0 && words[6] == (Regs[2] | 0x40), "the export shader modifier was not merged");
}

void TestGs() {
    auto words = Fresh();
    Require(sceGnmSetGsShader(words.data(), 0x100, Regs) == 0, "the geometry shader failed");
    Require(words[0] == Header(0x76, 3) && words[1] == 0x88 && words[2] == Regs[0] && words[3] == 0, "the geometry shader does not start with SET_SH_REG 0x88");
    Require(words[4] == Header(0x76, 3) && words[5] == 0x8a && words[6] == Regs[2] && words[7] == Regs[3], "the geometry shader does not write SET_SH_REG 0x8a");
    Require(WholePackets(words.data(), 29), "the geometry shader is not whole PM4 packets");
}

void TestHs() {
    auto words = Fresh();
    Require(sceGnmSetHsShader(words.data(), 0x100, Regs, 0x77) == 0, "the hull shader failed");
    Require(words[0] == Header(0x76, 3) && words[1] == 0x108 && words[2] == Regs[0] && words[3] == 0, "the hull shader does not start with SET_SH_REG 0x108");
    Require(words[4] == Header(0x76, 3) && words[5] == 0x10a && words[6] == Regs[2] && words[7] == Regs[3], "the hull shader does not write SET_SH_REG 0x10a");
    Require(words[15] == Header(0x69, 2) && words[16] == 0x2d6 && words[17] == 0x77, "the hull shader does not write the LS/HS config");
    Require(WholePackets(words.data(), 30), "the hull shader is not whole PM4 packets");
}

void TestLs() {
    auto words = Fresh();
    Require(sceGnmSetLsShader(words.data(), 0x100, Regs, 0) == 0, "the local shader failed");
    Require(words[0] == Header(0x76, 3) && words[1] == 0x148 && words[2] == Regs[0] && words[3] == 0, "the local shader does not start with SET_SH_REG 0x148");
    Require(words[4] == Header(0x76, 2) && words[5] == 0x14b && words[6] == Regs[3], "the local shader does not write SET_SH_REG 0x14b");
    Require(words[7] == Header(0x76, 3) && words[8] == 0x14a && words[9] == Regs[2] && words[10] == Regs[3], "the local shader does not write SET_SH_REG 0x14a");
    Require(WholePackets(words.data(), 23), "the local shader is not whole PM4 packets");
    Require(sceGnmSetLsShader(words.data(), 0x100, Regs, 0x40) == 0 && words[9] == (Regs[2] | 0x40), "the local shader modifier was not merged");
}

}  // namespace

int main() {
    try {
        TestPs();
        TestVs();
        TestCs();
        TestEs();
        TestGs();
        TestHs();
        TestLs();
        std::puts("gnm shader writer tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
