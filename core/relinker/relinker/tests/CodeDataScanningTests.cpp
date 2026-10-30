#include "TestProgress.hpp"
#include <relinker/analysis/SyscallScanner.hpp>
#include <codegen/CodegenException.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Bytes = std::vector<std::uint8_t>;

constexpr std::size_t DataOffset = 6;
constexpr std::size_t RoutineOffset = 14;
constexpr std::array<std::uint8_t, 8> UndecodableData{0x0f, 0x78, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55};

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

Bytes Fixture(bool syscall) {
    constexpr std::array<std::uint8_t, DataOffset> entry{0xe8, 0x09, 0x00, 0x00, 0x00, 0xc3};
    Bytes code(RoutineOffset, 0x90);
    std::copy(entry.begin(), entry.end(), code.begin());
    if (syscall) code.insert(code.end(), {0xb8, 0x09, 0x00, 0x00, 0x00, 0x0f, 0x05});
    code.push_back(0xc3);
    return code;
}

void CheckUndecodableData(Domain::FileByteOffset origin) {
    TestProgress progress;
    const auto scanner = Relinker::MakeSyscallScanner(progress);
    auto code = Fixture(false);
    Require(scanner->ScanCodeSectionForSyscalls(code, origin, code.size()).empty(), "Valid code across a padding gap was rejected");
    std::copy(UndecodableData.begin(), UndecodableData.end(), code.begin() + DataOffset);
    try {
        scanner->ScanCodeSectionForSyscalls(code, origin, code.size());
    } catch (const Codegen::CodegenException& error) {
        Require(error.FailureOffset == origin + DataOffset, "Decode failure did not identify the embedded data");
        Require(std::string(error.what()).find("Cannot decode instruction: Unsupported 0F 78 opcode without SSE4A prefix") != std::string::npos, "Unexpected embedded-data decode failure");
        std::cout << "Embedded-data decode rejection reproduced at " << error.FailureOffset << '\n';
        return;
    }
    throw std::runtime_error("Embedded-data scanning changed; update the known-gap fixture and debt entry");
}

void CheckForbiddenInstruction(const Bytes& code, Domain::FileByteOffset origin, std::size_t offset, const std::string& number) {
    for (const bool trap : {false, true}) {
        TestProgress progress;
        try {
            Relinker::MakeSyscallScanner(progress, trap)->ScanCodeSectionForSyscalls(code, origin, code.size());
        } catch (const Domain::RelinkerException& error) {
            Require(error.FailureOffset == origin + offset, "Forbidden instruction reported at the wrong offset");
            Require(std::string(error.what()).find("Forbidden syscall instruction at code offset 0x") != std::string::npos, "Unexpected syscall rejection");
            Require(number.empty() || std::string(error.what()).find(number) != std::string::npos, "Lost the reachable routine's syscall number");
            continue;
        }
        throw std::runtime_error("Forbidden instruction was not reported");
    }
}

void CheckGapAndReachableRoutine(Domain::FileByteOffset origin) {
    auto code = Fixture(false);
    code[DataOffset] = 0x0f;
    code[DataOffset + 1] = 0x05;
    CheckForbiddenInstruction(code, origin, DataOffset, {});
    CheckForbiddenInstruction(Fixture(true), origin, RoutineOffset + 5, "syscall number 9");
}

}

int main() {
    try {
        for (const auto origin : {std::uint64_t{0}, std::uint64_t{0x123400}, std::uint64_t{0x100000000}}) {
            CheckUndecodableData(origin);
            CheckGapAndReachableRoutine(origin);
        }
        std::cout << "Embedded-code-data known-gap and reachable-code controls passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
