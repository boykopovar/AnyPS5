#include <codegen/x86/RequiredExtensions.hpp>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace {

using Codegen::CpuExtension;
using Codegen::RequiredExtensions;

std::uint32_t bit(const CpuExtension extension) {
    return static_cast<std::uint32_t>(extension);
}

void Check(const char* name, const std::vector<std::uint8_t>& bytes, const std::uint32_t expected) {
    const auto actual = RequiredExtensions(bytes.data(), bytes.size());
    if (actual == expected) return;
    std::fprintf(stderr, "%s: expected 0x%x, got 0x%x\n", name, expected, actual);
    throw 1;
}

void ScansEveryInstruction() {
    Check("lzcnt", {0xF3, 0x0F, 0xBD, 0xC1}, bit(CpuExtension::Lzcnt));
    Check("tzcnt", {0xF3, 0x0F, 0xBC, 0xC1}, bit(CpuExtension::Bmi1));
    Check("movbe", {0x0F, 0x38, 0xF0, 0x01}, bit(CpuExtension::Movbe));
    Check("movbe16", {0x66, 0x0F, 0x38, 0xF0, 0x01}, bit(CpuExtension::Movbe));
    Check("andn", {0xC4, 0xE2, 0x70, 0xF2, 0xC2}, bit(CpuExtension::Bmi1) | bit(CpuExtension::Bmi2));
    Check("mulx", {0xC4, 0xE2, 0x73, 0xF6, 0xC2}, bit(CpuExtension::Bmi1) | bit(CpuExtension::Bmi2));
    Check("vcvtph2ps", {0xC4, 0xE2, 0x79, 0x13, 0xC1}, bit(CpuExtension::F16c));
    Check("vcvtps2ph", {0xC4, 0xE3, 0x79, 0x1D, 0xC1, 0x00}, bit(CpuExtension::F16c));
    Check("vfmadd132ps", {0xC4, 0xE2, 0x71, 0x98, 0xC2}, bit(CpuExtension::Fma));
}

void DistinguishesAvx1() {
    Check("vaddps", {0xC5, 0xFC, 0x58, 0xC1}, 0);
    Check("vmovdqa", {0xC5, 0xFD, 0x6F, 0xC1}, 0);
    Check("vzeroupper", {0xC5, 0xF8, 0x77}, 0);
    Check("vblendps", {0xC4, 0xE3, 0x7D, 0x0C, 0xC1, 0x00}, 0);
    Check("vperm2f128", {0xC4, 0xE3, 0x7D, 0x06, 0xC1, 0x00}, 0);
}

void RequiresAvx2() {
    Check("vpaddd", {0xC5, 0xFD, 0xFE, 0xC1}, bit(CpuExtension::Avx2));
    Check("vpermd", {0xC4, 0xE2, 0x7D, 0x36, 0xC1}, bit(CpuExtension::Avx2));
    Check("vpsllvd", {0xC4, 0xE2, 0x71, 0x47, 0xC2}, bit(CpuExtension::Avx2));
    Check("vpinserti128", {0xC4, 0xE3, 0x7D, 0x38, 0xC1, 0x00}, bit(CpuExtension::Avx2));
}

void UnionsAcrossBytes() {
    Check("union", {0x90, 0xF3, 0x0F, 0xBD, 0xC1, 0xC5, 0xFD, 0xFE, 0xC1},
          bit(CpuExtension::Lzcnt) | bit(CpuExtension::Avx2));
}

void RequiresEverythingOnUndecodableBytes() {
    Check("undecodable", {0xC4}, Codegen::AllCpuExtensions);
}

}

int main() {
    ScansEveryInstruction();
    DistinguishesAvx1();
    RequiresAvx2();
    UnionsAcrossBytes();
    RequiresEverythingOnUndecodableBytes();
    std::puts("Required extension tests passed");
    return 0;
}
