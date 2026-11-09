#include "prx/libkernel/System/include/HostCpu.hpp"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

static void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

static constexpr std::uint32_t Bit(unsigned bit) {
    return std::uint32_t{1} << bit;
}

static HostCpuRegisters Zen2() {
    HostCpuRegisters registers;
    registers.Leaf1Ecx = Bit(0) | Bit(1) | Bit(9) | Bit(12) | Bit(13) | Bit(19) | Bit(20) | Bit(22) | Bit(23) | Bit(25) | Bit(26) | Bit(27) | Bit(28) | Bit(29) | Bit(30);
    registers.Leaf1Edx = Bit(25) | Bit(26);
    registers.Leaf7Ebx = Bit(3) | Bit(5) | Bit(8) | Bit(18) | Bit(19) | Bit(23) | Bit(24) | Bit(29);
    registers.Leaf7Ecx = Bit(22);
    registers.LeafDSub1Eax = Bit(0) | Bit(1);
    registers.Leaf80000001Ecx = Bit(0) | Bit(5) | Bit(6) | Bit(8);
    registers.Leaf80000008Ebx = Bit(0);
    registers.Xcr0 = 0x7;
    return registers;
}

static std::string Text(const std::vector<std::string>& names) {
    std::string text;
    for (const auto& name : names) text += (text.empty() ? "" : ",") + name;
    return text;
}

static void CheckMissing(const HostCpuRegisters& registers, const std::vector<std::string>& expected, const std::string& label) {
    const auto missing = MissingHostCpuFeatures(registers);
    Require(missing == expected, label + ": expected [" + Text(expected) + "], got [" + Text(missing) + "]");
}

static void CheckZen2() {
    CheckMissing(Zen2(), {}, "Zen 2");
    for (const auto& feature : HostCpuFeatures(Zen2()))
        Require(feature.Present, std::string("Zen 2 lacks ") + feature.Name);
}

static void CheckIntelWithoutAmdOnlyExtensions() {
    auto registers = Zen2();
    registers.Leaf7Ebx &= ~(Bit(18) | Bit(19) | Bit(23) | Bit(24) | Bit(29));
    registers.Leaf7Ecx = 0;
    registers.LeafDSub1Eax = Bit(0);
    registers.Leaf80000001Ecx &= ~(Bit(6) | Bit(8));
    registers.Leaf80000008Ebx = 0;
    CheckMissing(registers, {}, "AVX2 host without SSE4A, CLZERO, SHA, ADX, RDSEED, CLFLUSHOPT, CLWB, RDPID");
}

static void CheckSingleMissingExtension() {
    struct Case {
        const char* Name;
        std::uint32_t HostCpuRegisters::* Register;
        unsigned Bit;
    };
    const Case cases[] = {
        {"BMI2", &HostCpuRegisters::Leaf7Ebx, 8},
        {"BMI1", &HostCpuRegisters::Leaf7Ebx, 3},
        {"AVX2", &HostCpuRegisters::Leaf7Ebx, 5},
        {"MOVBE", &HostCpuRegisters::Leaf1Ecx, 22},
        {"FMA", &HostCpuRegisters::Leaf1Ecx, 12},
        {"F16C", &HostCpuRegisters::Leaf1Ecx, 29},
        {"POPCNT", &HostCpuRegisters::Leaf1Ecx, 23},
        {"SSE4.2", &HostCpuRegisters::Leaf1Ecx, 20},
        {"CMPXCHG16B", &HostCpuRegisters::Leaf1Ecx, 13},
        {"LZCNT", &HostCpuRegisters::Leaf80000001Ecx, 5},
        {"LAHF/SAHF", &HostCpuRegisters::Leaf80000001Ecx, 0},
    };
    for (const auto& entry : cases) {
        auto registers = Zen2();
        registers.*entry.Register &= ~Bit(entry.Bit);
        CheckMissing(registers, {entry.Name}, std::string("without ") + entry.Name);
    }
}

static void CheckAvxWithoutOsState() {
    auto registers = Zen2();
    registers.Xcr0 = 0x3;
    CheckMissing(registers, {"OS AVX state (XCR0)"}, "XCR0 without YMM state");
    registers = Zen2();
    registers.Leaf1Ecx &= ~Bit(27);
    CheckMissing(registers, {"OS AVX state (XCR0)"}, "OSXSAVE clear");
}

static void CheckPreHaswell() {
    auto registers = Zen2();
    registers.Leaf1Ecx &= ~(Bit(12) | Bit(22));
    registers.Leaf7Ebx = 0;
    registers.Leaf80000001Ecx &= ~Bit(5);
    CheckMissing(registers, {"LZCNT", "BMI1", "BMI2", "MOVBE", "AVX2", "FMA"}, "AVX host without AVX2");
}

static void CheckError() {
    auto registers = Zen2();
    registers.Leaf7Ebx &= ~Bit(8);
    CheckHostCpu(Zen2(), "AMD Ryzen 7 3700X 8-Core Processor");
    try {
        CheckHostCpu(registers, "Example CPU");
        throw std::logic_error("missing BMI2 did not throw");
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        Require(message.find("(Example CPU) lacks BMI2.") != std::string::npos, "unexpected message: " + message);
    }
    try {
        CheckHostCpu(registers, "AMD Example   ");
        throw std::logic_error("missing BMI2 did not throw");
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        Require(message.find("(AMD Example) lacks BMI2.") != std::string::npos, "unexpected message: " + message);
    }
    try {
        CheckHostCpu(registers, "");
        throw std::logic_error("missing BMI2 did not throw");
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        Require(message.find("(unknown model) lacks BMI2.") != std::string::npos, "unexpected message: " + message);
    }
}

static void CheckExportedHelpers() {
    const auto registers = ReadHostCpuRegisters_nid_no_patch();
    const auto direct = ReadHostCpuRegisters();
    Require(registers.Leaf1Ecx == direct.Leaf1Ecx && registers.Leaf7Ebx == direct.Leaf7Ebx && registers.Xcr0 == direct.Xcr0, "exported registers differ");
    Require(std::string(HostCpuModel_nid_no_patch().value) == HostCpuModel(), "exported model differs");
    auto none = Zen2();
    Require(std::string(MissingHostCpuFeatures_nid_no_patch(&none).value).empty(), "exported missing list is not empty for a Zen 2 CPU");
    auto lacking = Zen2();
    lacking.Leaf7Ebx &= ~Bit(8);
    Require(std::string(MissingHostCpuFeatures_nid_no_patch(&lacking).value) == "BMI2", "exported missing list is not BMI2");
    lacking.Leaf7Ebx &= ~Bit(3);
    Require(std::string(MissingHostCpuFeatures_nid_no_patch(&lacking).value) == "BMI1, BMI2", "exported missing list is not joined with a comma and a space");
}

int main() {
    try {
        CheckExportedHelpers();
        CheckZen2();
        CheckIntelWithoutAmdOnlyExtensions();
        CheckSingleMissingExtension();
        CheckAvxWithoutOsState();
        CheckPreHaswell();
        CheckError();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
