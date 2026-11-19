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

void CheckRanges(const char* name, const std::vector<std::uint8_t>& bytes, const std::vector<Domain::CodeRange>& ranges, const std::uint32_t expected) {
    const auto actual = Codegen::RequiredExtensions(bytes.data(), bytes.size(), ranges);
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
    Check("rorx", {0xC4, 0xE3, 0xFB, 0xF0, 0xC0, 0x01}, bit(CpuExtension::Bmi2));
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
    Check("vpxor-xmm", {0xC5, 0xF9, 0xEF, 0xC0}, 0);
    Check("vmovdqu-xmm", {0xC5, 0xFA, 0x6F, 0x07}, 0);
    Check("vpshufb-xmm", {0xC4, 0xE2, 0x79, 0x00, 0xC1}, 0);
    Check("vpaddd-xmm", {0xC5, 0xF9, 0xFE, 0xC1}, 0);
    Check("vmovdqu-ymm", {0xC5, 0xFE, 0x6F, 0x07}, 0);
    Check("vpermilps-ymm", {0xC4, 0xE2, 0x7D, 0x0C, 0xC1}, 0);
    Check("vpermilps-ymm-imm", {0xC4, 0xE3, 0x7D, 0x04, 0xC1, 0x1B}, 0);
    Check("vpermilpd-ymm-imm", {0xC4, 0xE3, 0x7D, 0x05, 0xC1, 0x05}, 0);
    Check("vmaskmovpd-load-ymm", {0xC4, 0xE2, 0x75, 0x2D, 0x07}, 0);
    Check("vmaskmovpd-store-ymm", {0xC4, 0xE2, 0x75, 0x2F, 0x07}, 0);
    Check("vhaddps-ymm", {0xC5, 0xFF, 0x7C, 0xC1}, 0);
    Check("vaddsubpd-ymm", {0xC5, 0xFD, 0xD0, 0xC1}, 0);
    Check("vlddqu-ymm", {0xC5, 0xFF, 0xF0, 0x07}, 0);
    Check("vdpps-ymm", {0xC4, 0xE3, 0x7D, 0x40, 0xC1, 0x01}, 0);
}

void DistinguishesBroadcastSource() {
    Check("vbroadcastss-reg", {0xC4, 0xE2, 0x7D, 0x18, 0xC1}, bit(CpuExtension::Avx2));
    Check("vbroadcastss-mem", {0xC4, 0xE2, 0x7D, 0x18, 0x00}, 0);
    Check("vbroadcastsd-reg", {0xC4, 0xE2, 0x7D, 0x19, 0xC1}, bit(CpuExtension::Avx2));
    Check("vbroadcastsd-mem", {0xC4, 0xE2, 0x7D, 0x19, 0x00}, 0);
}

void RequiresAvx2() {
    Check("vpaddd", {0xC5, 0xFD, 0xFE, 0xC1}, bit(CpuExtension::Avx2));
    Check("vpermd-ymm", {0xC4, 0xE2, 0x7D, 0x36, 0xC1}, bit(CpuExtension::Avx2));
    Check("vpermps-ymm", {0xC4, 0xE2, 0x75, 0x16, 0xC2}, bit(CpuExtension::Avx2));
    Check("vpsllvd", {0xC4, 0xE2, 0x71, 0x47, 0xC2}, bit(CpuExtension::Avx2));
    Check("vpinserti128", {0xC4, 0xE3, 0x7D, 0x38, 0xC1, 0x00}, bit(CpuExtension::Avx2));
    Check("vpbroadcastd-xmm", {0xC4, 0xE2, 0x79, 0x58, 0xC1}, bit(CpuExtension::Avx2));
    Check("vpblendd-xmm", {0xC4, 0xE3, 0x79, 0x02, 0xC1, 0x01}, bit(CpuExtension::Avx2));
    Check("vpsllvd-xmm", {0xC4, 0xE2, 0x79, 0x47, 0xC1}, bit(CpuExtension::Avx2));
    Check("vpsrlvd-xmm", {0xC4, 0xE2, 0x71, 0x45, 0xC2}, bit(CpuExtension::Avx2));
    Check("vpsravd-xmm", {0xC4, 0xE2, 0x71, 0x46, 0xC2}, bit(CpuExtension::Avx2));
    Check("vpmaskmovd-xmm", {0xC4, 0xE2, 0x71, 0x8C, 0x07}, bit(CpuExtension::Avx2));
    Check("vpgatherdd-xmm", {0xC4, 0xE2, 0x69, 0x90, 0x04, 0x8F}, bit(CpuExtension::Avx2));
    Check("vpbroadcastd-ymm", {0xC4, 0xE2, 0x7D, 0x58, 0xC1}, bit(CpuExtension::Avx2));
    Check("vpblendd-ymm", {0xC4, 0xE3, 0x7D, 0x02, 0xC1, 0x01}, bit(CpuExtension::Avx2));
    Check("vpmaskmovd-ymm", {0xC4, 0xE2, 0x7D, 0x8C, 0x07}, bit(CpuExtension::Avx2));
    Check("vpgatherdd-ymm", {0xC4, 0xE2, 0x6D, 0x90, 0x04, 0x8F}, bit(CpuExtension::Avx2));
}

void UnionsAcrossBytes() {
    Check("union", {0x90, 0xF3, 0x0F, 0xBD, 0xC1, 0xC5, 0xFD, 0xFE, 0xC1},
          bit(CpuExtension::Lzcnt) | bit(CpuExtension::Avx2));
}

void ScansOutsideFunctionRanges() {
    CheckRanges("covered-avx2", {0xC5, 0xFD, 0xFE, 0xC1}, {{0, 4}}, bit(CpuExtension::Avx2));
    CheckRanges("outside-range-avx2", {0x90, 0x90, 0xC5, 0xFD, 0xFE, 0xC1}, {{0, 2}}, bit(CpuExtension::Avx2));
    CheckRanges("uncovered-undecodable", {0x90, 0xC4}, {{0, 1}}, Codegen::AllCpuExtensions);
}

void RequiresEverythingOnUndecodableBytes() {
    Check("undecodable", {0xC4}, Codegen::AllCpuExtensions);
}

}

int main() {
    ScansEveryInstruction();
    DistinguishesAvx1();
    DistinguishesBroadcastSource();
    RequiresAvx2();
    UnionsAcrossBytes();
    ScansOutsideFunctionRanges();
    RequiresEverythingOnUndecodableBytes();
    std::puts("Required extension tests passed");
    return 0;
}
