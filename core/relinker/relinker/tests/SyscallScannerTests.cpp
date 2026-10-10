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

}

int main(const int argc, char** argv) {
    try {
        require(argc == 2, "usage: syscall_scanner_tests <relinker>");
        const std::string binary = argv[1];

        RequireNumber(binary, kConstantStub, "syscall number 9", "64-bit constant stub");
        RequireNumber(binary, kUnsignedStub, "syscall number 57", "32-bit constant stub");
        RequireNumber(binary, kInt80Stub, "syscall number 11", "int 0x80 stub");

        RequireNoNumber(binary, kRegisterStub, "stub that loads the number from a register");
        RequireNoNumber(binary, kSysenterStub, "sysenter stub");
        RequireNoNumber(binary, kSysretStub, "sysret stub with a constant load in front of it");
        RequireNoNumber(binary, kClobberedStub, "stub whose intervening instruction writes rax");

        RequireNumber(binary, kPrefixedConstantStub, "syscall number 9", "prefixed constant stub");
        RequireNoNumber(binary, kPrefixedClobberedStub, "prefixed stub whose intervening instruction writes rax");

        std::cout << "syscall scanner tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "syscall_scanner_tests: " << error.what() << '\n';
        return 1;
    }
}
