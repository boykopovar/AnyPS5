#include "ElfFixture.hpp"
#include "RelinkerProcess.hpp"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

using namespace RelinkerTests;

void require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

const Bytes kConstantStub = {
    0x48, 0xC7, 0xC0, 0x09, 0x00, 0x00, 0x00,
    0x49, 0x89, 0xCA,
    0x0F, 0x05,
    0xC3,
};

const Bytes kUnsignedStub = {
    0xB8, 0x39, 0x00, 0x00, 0x00,
    0x0F, 0x05,
    0xC3,
};

const Bytes kRegisterStub = {
    0x48, 0x89, 0xF8,
    0x0F, 0x05,
    0xC3,
};

const Bytes kInt80Stub = {
    0xB8, 0x0B, 0x00, 0x00, 0x00,
    0xCD, 0x80,
    0xC3,
};

const Bytes kSysenterStub = {
    0x0F, 0x34,
    0xC3,
};

const Bytes kSysretStub = {
    0x48, 0xC7, 0xC0, 0x09, 0x00, 0x00, 0x00,
    0x0F, 0x07,
    0xC3,
};

const Bytes kClobberedStub = {
    0x48, 0xC7, 0xC0, 0x09, 0x00, 0x00, 0x00,
    0x48, 0x83, 0xC0, 0x01,
    0x0F, 0x05,
    0xC3,
};

const Bytes kPrefixedConstantStub = {
    0x66, 0x48, 0xC7, 0xC0, 0x09, 0x00, 0x00, 0x00,
    0xF2, 0x49, 0x89, 0xCA,
    0xF3, 0x0F, 0x05,
    0xC3,
};

const Bytes kPrefixedClobberedStub = {
    0x66, 0x48, 0xC7, 0xC0, 0x09, 0x00, 0x00, 0x00,
    0x66, 0x48, 0x83, 0xC0, 0x01,
    0xF3, 0x0F, 0x05,
    0xC3,
};

const Bytes kSdkGenericStub = {
    0x48, 0x89, 0xF8, 0x48, 0x89, 0xF7, 0x48, 0x89, 0xD6, 0x48, 0x89, 0xCA,
    0x4D, 0x89, 0xC2, 0x4D, 0x89, 0xC8, 0x4C, 0x8B, 0x4C, 0x24, 0x08,
    0x0F, 0x05, 0xC3,
};

const Bytes kSignExtendedStub = {
    0x48, 0xC7, 0xC0, 0xFF, 0xFF, 0xFF, 0xFF,
    0x49, 0x89, 0xCA,
    0x0F, 0x05,
    0xC3,
};

const Bytes kImm64Stub = {
    0x48, 0xB8, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x49, 0x89, 0xCA,
    0x0F, 0x05,
    0xC3,
};

const Bytes kImm64NegativeStub = {
    0x48, 0xB8, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x49, 0x89, 0xCA,
    0x0F, 0x05,
    0xC3,
};

const Bytes kZeroedStub = {
    0x31, 0xC0,
    0x0F, 0x05,
    0xC3,
};

const Bytes kWideZeroedStub = {
    0x48, 0x31, 0xC0,
    0x0F, 0x05,
    0xC3,
};

const Bytes kWordZeroedStub = {
    0x66, 0x31, 0xC0,
    0x0F, 0x05,
    0xC3,
};

const Bytes kExtendedRegisterZeroedStub = {
    0x45, 0x31, 0xC0,
    0x0F, 0x05,
    0xC3,
};

const Bytes kExtendedSourceXorStub = {
    0x44, 0x31, 0xC0,
    0x0F, 0x05,
    0xC3,
};

const Bytes kExtendedIndexXorStub = {
    0x42, 0x31, 0xC0,
    0x0F, 0x05,
    0xC3,
};

const Bytes kLoadThenExtendedZeroedStub = {
    0xB8, 0x3B, 0x00, 0x00, 0x00,
    0x47, 0x31, 0xC0,
    0x0F, 0x05,
    0xC3,
};

const Bytes kPrefixAwareLoadStub = {
    0x44, 0xB8, 0x3B, 0x00, 0x00, 0x00,
    0x0F, 0x05,
    0xC3,
};

const Bytes kWordWideZeroedStub = {
    0x66, 0x48, 0x31, 0xC0,
    0x0F, 0x05,
    0xC3,
};

const Bytes kExtendedLoadStub = {
    0x41, 0xB8, 0x3B, 0x00, 0x00, 0x00,
    0x0F, 0x05,
    0xC3,
};

const Bytes kLoadThenMemoryLoadStub = {
    0xB8, 0x3B, 0x00, 0x00, 0x00,
    0x8B, 0x44, 0x8B, 0xF8,
    0x0F, 0x05,
    0xC3,
};

const Bytes kLoadThenImmediateAddStub = {
    0xB8, 0x3B, 0x00, 0x00, 0x00,
    0x05, 0x00, 0x00, 0x31, 0xC9,
    0x0F, 0x05,
    0xC3,
};

std::string RunScanner(const std::string& binary, const Bytes& code) {
    const TempDirectory directory;
    const auto input = directory.Path() / "input.elf";
    const auto output = directory.Path() / "output.elf";
    WriteFile(input, MakeExecutable(code));
    const auto run = RunRelinker(binary, {"--skip-sce-module", input.string(), output.string()}, directory.Path() / "relinker.log");
    require(run.ExitCode != 0, "Relinker accepted a forbidden syscall instruction:\n" + run.Output);
    return run.Output;
}

void RequireNumber(const std::string& binary, const Bytes& code, const std::string& expected, const std::string& description) {
    const auto output = RunScanner(binary, code);
    require(output.find("Forbidden syscall instruction at code offset 0x") != std::string::npos,
            "Relinker did not report the " + description + " as forbidden:\n" + output);
    require(output.find(expected) != std::string::npos,
            "Relinker did not report " + expected + " for the " + description + ":\n" + output);
}

void RequireNoNumber(const std::string& binary, const Bytes& code, const std::string& description) {
    const auto output = RunScanner(binary, code);
    require(output.find("Forbidden syscall instruction at code offset 0x") != std::string::npos,
            "Relinker did not report the " + description + " as forbidden:\n" + output);
    require(output.find("syscall number") == std::string::npos,
            "Relinker reported a syscall number for the " + description + ":\n" + output);
}

void RequireSdkStubsAreTrapped(const std::string& binary) {
    Bytes code = kConstantStub;
    const auto numberedOffset = code.size() - 3u;
    code.insert(code.end(), kSdkGenericStub.begin(), kSdkGenericStub.end());
    const auto genericOffset = kConstantStub.size() + kSdkGenericStub.size() - 3u;
    const TempDirectory directory;
    const auto input = directory.Path() / "input.elf";
    const auto output = directory.Path() / "output.elf";
    WriteFile(input, MakeExecutable(code));
    const auto run = RunRelinker(binary, {"--skip-sce-module", "--trap-sdk-syscall-stubs", input.string(), output.string()}, directory.Path() / "relinker.log");
    require(run.ExitCode == 0, "Relinker did not trap known SDK syscall stubs:\n" + run.Output);
    const auto relinked = ReadFile(output);
    require(Read<std::uint16_t>(relinked, kCodeOffset + numberedOffset) == 0x0B0Fu,
            "Relinker did not replace the numbered SDK syscall with UD2");
    require(Read<std::uint16_t>(relinked, kCodeOffset + genericOffset) == 0x0B0Fu,
            "Relinker did not replace the generic SDK syscall with UD2");
}

void RequireUnknownSyscallsStillFail(const std::string& binary) {
    const TempDirectory directory;
    const auto input = directory.Path() / "input.elf";
    const auto output = directory.Path() / "output.elf";
    WriteFile(input, MakeExecutable(kRegisterStub));
    const auto run = RunRelinker(binary, {"--skip-sce-module", "--trap-sdk-syscall-stubs", input.string(), output.string()}, directory.Path() / "relinker.log");
    require(run.ExitCode != 0, "Relinker accepted an unknown syscall in SDK-stub trap mode:\n" + run.Output);
    require(run.Output.find("Forbidden syscall instruction at code offset 0x") != std::string::npos,
            "Relinker did not report an unknown syscall in SDK-stub trap mode:\n" + run.Output);
}

}

int main(const int argc, char** argv) {
    try {
        require(argc == 2, "usage: syscall_scanner_tests <relinker>");
        const std::string binary = argv[1];

        RequireNumber(binary, kConstantStub, "syscall number 9", "64-bit constant stub");
        RequireNumber(binary, kUnsignedStub, "syscall number 57", "32-bit constant stub");
        RequireNumber(binary, kInt80Stub, "syscall number 11", "int 0x80 stub");
        RequireNumber(binary, kSignExtendedStub, "syscall number 18446744073709551615", "negative 32-bit constant stub");
        RequireNumber(binary, kImm64Stub, "syscall number 9", "64-bit immediate stub");
        RequireNumber(binary, kImm64NegativeStub, "syscall number 18446744073709551615", "negative 64-bit immediate stub");
        RequireNumber(binary, kZeroedStub, "syscall number 0", "zeroed stub");
        RequireNumber(binary, kWideZeroedStub, "syscall number 0", "wide zeroed stub");
        RequireNumber(binary, kExtendedIndexXorStub, "syscall number 0", "stub that zeroes eax with an index-only REX");
        RequireNumber(binary, kLoadThenExtendedZeroedStub, "syscall number 59", "stub whose intervening xor zeroes r8d");
        RequireNumber(binary, kPrefixAwareLoadStub, "syscall number 59", "32-bit constant stub with an index-only REX");

        RequireNoNumber(binary, kRegisterStub, "stub that loads the number from a register");
        RequireNoNumber(binary, kSysenterStub, "sysenter stub");
        RequireNoNumber(binary, kSysretStub, "sysret stub with a constant load in front of it");
        RequireNoNumber(binary, kClobberedStub, "stub whose intervening instruction writes rax");
        RequireNoNumber(binary, kWordZeroedStub, "stub that zeroes only ax");
        RequireNoNumber(binary, kWordWideZeroedStub, "stub whose wide zeroing xor carries an operand-size prefix");
        RequireNoNumber(binary, kExtendedRegisterZeroedStub, "stub that zeroes r8d");
        RequireNoNumber(binary, kExtendedSourceXorStub, "stub that xors eax with r8d");
        RequireNoNumber(binary, kExtendedLoadStub, "stub that loads r8d");
        RequireNoNumber(binary, kLoadThenMemoryLoadStub, "stub whose intervening instruction loads eax from memory");
        RequireNoNumber(binary, kLoadThenImmediateAddStub, "stub whose intervening instruction adds to eax");

        RequireNumber(binary, kPrefixedConstantStub, "syscall number 9", "prefixed constant stub");
        RequireNoNumber(binary, kPrefixedClobberedStub, "prefixed stub whose intervening instruction writes rax");

        RequireSdkStubsAreTrapped(binary);
        RequireUnknownSyscallsStillFail(binary);

        std::cout << "syscall scanner tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "syscall_scanner_tests: " << error.what() << '\n';
        return 1;
    }
}
