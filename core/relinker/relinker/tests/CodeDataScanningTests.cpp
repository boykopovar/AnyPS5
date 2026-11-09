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

Bytes ReachableFixture(const std::vector<std::uint8_t>& stub) {
    constexpr std::array<std::uint8_t, DataOffset> entry{0xe8, 0x09, 0x00, 0x00, 0x00, 0xc3};
    Bytes code(RoutineOffset, 0x90);
    std::copy(entry.begin(), entry.end(), code.begin());
    code.insert(code.end(), stub.begin(), stub.end());
    code.push_back(0xc3);
    return code;
}

void CheckForbiddenInstruction(const Bytes& code, Domain::FileByteOffset origin, std::size_t offset, const std::string& number, const std::vector<Domain::CodeRange>& unwind = {}) {
    for (const bool trap : {false, true}) {
        TestProgress progress;
        try {
            Relinker::MakeSyscallScanner(progress, trap)->ScanCodeSectionForSyscalls(code, origin, code.size(), {}, unwind);
        } catch (const Domain::RelinkerException& error) {
            Require(error.FailureOffset == origin + offset, "Forbidden instruction reported at the wrong offset");
            Require(std::string(error.what()).find("Forbidden syscall instruction at code offset 0x") != std::string::npos, "Unexpected syscall rejection");
            Require(number.empty() || std::string(error.what()).find(number) != std::string::npos, "Lost the reachable routine's syscall number");
            continue;
        }
        throw std::runtime_error("Forbidden instruction was not reported");
    }
}

void CheckUndecodableData(Domain::FileByteOffset origin) {
    TestProgress progress;
    const auto scanner = Relinker::MakeSyscallScanner(progress);
    auto code = Fixture(false);
    Require(scanner->ScanCodeSectionForSyscalls(code, origin, code.size(), {}, {{0, DataOffset}}).empty(), "Valid code across a padding gap was rejected");
    std::copy(UndecodableData.begin(), UndecodableData.end(), code.begin() + DataOffset);
    Require(scanner->ScanCodeSectionForSyscalls(code, origin, code.size(), {}, {{0, DataOffset}}).empty(), "Embedded data in gap caused decode rejection");
    auto codeWithSyscall = Fixture(true);
    std::copy(UndecodableData.begin(), UndecodableData.end(), codeWithSyscall.begin() + DataOffset);
    CheckForbiddenInstruction(codeWithSyscall, origin, RoutineOffset + 5, "syscall number 9", {{0, DataOffset}});
}

void CheckGapAndReachableRoutine(Domain::FileByteOffset origin) {
    TestProgress progress;
    const auto scanner = Relinker::MakeSyscallScanner(progress);
    auto code = Fixture(false);
    code[DataOffset] = 0x0f;
    code[DataOffset + 1] = 0x05;
    Require(scanner->ScanCodeSectionForSyscalls(code, origin, code.size(), {}, {{0, DataOffset}}).empty(), "Syscall byte pattern in data gap was falsely rejected");
    auto decodableData = Fixture(false);
    decodableData[DataOffset] = 0x00;
    decodableData[DataOffset + 1] = 0x00;
    Require(scanner->ScanCodeSectionForSyscalls(decodableData, origin, decodableData.size(), {}, {{0, DataOffset}}).empty(), "Decodable data in gap caused unexpected rejection");
    constexpr std::array<std::uint8_t, 7> constantStub{0xb8, 0x09, 0x00, 0x00, 0x00, 0x0f, 0x05};
    std::copy(constantStub.begin(), constantStub.end(), code.begin() + DataOffset);
    CheckForbiddenInstruction(code, origin, DataOffset + 5, "syscall number 9", {{0, DataOffset}});
    CheckForbiddenInstruction(Fixture(true), origin, RoutineOffset + 5, "syscall number 9", {{0, DataOffset}});
    CheckForbiddenInstruction(ReachableFixture({0x8b, 0x07, 0x0f, 0x05}), origin, RoutineOffset + 2, {}, {{0, DataOffset}});
    CheckForbiddenInstruction(ReachableFixture({0x89, 0xc8, 0x0f, 0x05}), origin, RoutineOffset + 2, {}, {{0, DataOffset}});
}

void CheckNoUnwindIndirectJumpSyscall(Domain::FileByteOffset origin) {
    const Bytes code = {
        0x48, 0x8D, 0x05, 0x02, 0x00, 0x00, 0x00,
        0xFF, 0xE0,
        0x8B, 0x07,
        0x0F, 0x05,
        0xC3,
    };
    CheckForbiddenInstruction(code, origin, 11, {}, {});
}

void CheckIndirectOnlyDynamicSyscall(Domain::FileByteOffset origin) {
    const Bytes code = {
        0xEB, 0x03,
        0x0F, 0x78, 0x00,
        0x48, 0x8D, 0x05, 0x02, 0x00, 0x00, 0x00,
        0xFF, 0xE0,
        0x8B, 0x07,
        0x0F, 0x05,
        0xC3,
    };
    CheckForbiddenInstruction(code, origin, 16, {}, {{0, 2}});
}

void CheckGapBranchToSyscall(Domain::FileByteOffset origin) {
    const Bytes code = {
        0xC3,
        0xEB, 0x03,
        0x00, 0x00, 0x00,
        0x8B, 0x07,
        0x0F, 0x05,
        0xC3,
    };
    TestProgress progress;
    const auto scanner = Relinker::MakeSyscallScanner(progress);
    Require(scanner->ScanCodeSectionForSyscalls(code, origin, code.size(), {}, {{0, 1}}).empty(), "Data gap branch falsely created a routine and rejected 0F 05");
}

void CheckDirectCallAfterIndirectJump(Domain::FileByteOffset origin) {
    const Bytes code = {
        0xFF, 0xE0,
        0xE8, 0x08, 0x00, 0x00, 0x00,
        0xC3,
        0x0F, 0x78, 0x00, 0x11, 0x22, 0x33, 0x44,
        0x8B, 0x07, 0x0F, 0x05, 0xC3,
    };
    CheckForbiddenInstruction(code, origin, 17, {}, {{0, 8}});
}

void CheckConditionalBranchAcrossGap(Domain::FileByteOffset origin) {
    const Bytes code = {
        0x74, 0x05,
        0x0F, 0x78, 0x00, 0x11, 0x22,
        0x8B, 0x07, 0x0F, 0x05, 0xC3,
    };
    CheckForbiddenInstruction(code, origin, 9, {}, {{0, 2}});
}

void CheckUnwindGapAndBoundedCallee(Domain::FileByteOffset origin) {
    const Bytes gap = {
        0xC3,
        0x0F, 0x78, 0x11,
        0xC3,
    };
    const Bytes body = {
        0xC3,
        0x0F, 0x78, 0x00,
        0x8B, 0x07, 0x0F, 0x05, 0xC3,
    };
    const Bytes callee = {
        0x48, 0x8D, 0x05, 0x02, 0x00, 0x00, 0x00,
        0xFF, 0xE0,
        0x8B, 0x07,
        0x0F, 0x05,
        0xC3,
        0x0F, 0x78, 0x00,
    };
    const Bytes tail = {
        0xE8, 0x01, 0x00, 0x00, 0x00,
        0xC3,
        0xFF, 0xE0,
        0x0F, 0x78, 0x00,
    };
    TestProgress progress;
    const auto scanner = Relinker::MakeSyscallScanner(progress);
    Require(scanner->ScanCodeSectionForSyscalls(gap, origin, gap.size(), {}, {{0, 1}, {4, 5}}).empty(), "Embedded data in an unwind gap was rejected");
    CheckForbiddenInstruction(body, origin, 6, {}, {{0, 1}, {4, 9}});
    CheckForbiddenInstruction(callee, origin, 11, {}, {{0, 7}});
    Require(scanner->ScanCodeSectionForSyscalls(tail, origin, tail.size(), {}, {{0, 6}}).empty(), "Embedded data after an outside indirect jump was rejected");
}

}

int main() {
    try {
        for (const auto origin : {std::uint64_t{0}, std::uint64_t{0x123400}, std::uint64_t{0x100000000}}) {
            CheckUndecodableData(origin);
            CheckGapAndReachableRoutine(origin);
            CheckNoUnwindIndirectJumpSyscall(origin);
            CheckIndirectOnlyDynamicSyscall(origin);
            CheckDirectCallAfterIndirectJump(origin);
            CheckConditionalBranchAcrossGap(origin);
            CheckUnwindGapAndBoundedCallee(origin);
            CheckGapBranchToSyscall(origin);
        }
        std::cout << "Embedded-code-data known-gap and reachable-code controls passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
