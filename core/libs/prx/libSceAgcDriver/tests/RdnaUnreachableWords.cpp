#include "ControlFlow/GraphBuilder.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "RdnaDecoder/RdnaOpcode.hpp"
#include "RdnaDecoder/RdnaProgram.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ShaderRecompiler;

namespace {

constexpr std::uint32_t SNop = 0xbf800000u;
constexpr std::uint32_t SEndpgm = 0xbf810000u;
constexpr std::uint32_t SBranchSkipOne = 0xbf820001u;
constexpr std::uint32_t SBranchSkipTwo = 0xbf820002u;
constexpr std::uint32_t SCodeEnd = 0xbf9f0000u;
constexpr std::uint32_t SCbranchScc1SkipOne = 0xbf850001u;
constexpr std::uint32_t SCbranchScc1SkipTwo = 0xbf850002u;
constexpr std::uint32_t SCbranchScc1SkipThree = 0xbf850003u;
constexpr std::uint32_t UnsupportedSop2 = 0x86060005u;
constexpr std::uint32_t UnknownFamily = 0xffffffffu;

int failures = 0;

void Fail(const char* name, const std::string& message) {
    std::fprintf(stderr, "%s: %s\n", name, message.c_str());
    ++failures;
}

void ExpectBranchTargets(const char* name, std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& targets) {
    RdnaProgram program;
    try {
        program = RdnaInstructionDecoder{}.Decode(code);
    } catch (const std::exception& error) {
        Fail(name, error.what());
        return;
    }
    std::vector<std::uint32_t> decodedTargets;
    for (const auto& instruction : program.instructions) {
        if (!IsDirectBranchOpcode(instruction.op)) {
            continue;
        }
        decodedTargets.push_back(instruction.branchTarget);
        if (FindInstructionAtProgramCounter(program, instruction.branchTarget) == nullptr) {
            Fail(name, "branch target " + std::to_string(instruction.branchTarget) + " is not an instruction boundary");
        }
    }
    if (decodedTargets != targets) {
        Fail(name, "decoded " + std::to_string(decodedTargets.size()) + " branch targets");
    }
}

void ExpectProgram(const char* name, std::span<const std::uint32_t> code, const std::vector<std::uint32_t>& programCounters) {
    RdnaProgram program;
    try {
        program = RdnaInstructionDecoder{}.Decode(code);
    } catch (const std::exception& error) {
        Fail(name, error.what());
        return;
    }
    if (program.instructions.size() != programCounters.size()) {
        Fail(name, "decoded " + std::to_string(program.instructions.size()) + " instructions");
        return;
    }
    for (std::size_t index = 0; index < programCounters.size(); ++index) {
        if (program.instructions[index].programCounter != programCounters[index]) {
            Fail(name, "instruction " + std::to_string(index) + " at " + std::to_string(program.instructions[index].programCounter));
        }
    }
    ControlFlowGraph graph;
    try {
        graph = GraphBuilder{}.Build(program);
    } catch (const std::exception& error) {
        Fail(name, std::string("control flow graph: ") + error.what());
        return;
    }
    for (const auto& block : graph.blocks) {
        if (block.instructionBegin == block.instructionEnd) {
            continue;
        }
        const auto& last = program.instructions[block.instructionEnd - 1u];
        std::vector<std::uint32_t> expected;
        if (last.op == RdnaOpcode::SBranch) {
            for (const auto& target : graph.blocks) {
                if (target.startProgramCounter == last.branchTarget) {
                    expected.push_back(target.id);
                }
            }
        } else if (last.op != RdnaOpcode::SEndpgm && last.op != RdnaOpcode::SCodeEnd) {
            continue;
        }
        if (block.successors != expected) {
            Fail(name, "block at " + std::to_string(block.startProgramCounter) + " has " + std::to_string(block.successors.size()) + " successors");
        }
    }
}

template <typename Error>
void ExpectError(const char* name, std::span<const std::uint32_t> code, const std::string& expected) {
    try {
        static_cast<void>(RdnaInstructionDecoder{}.Decode(code));
        Fail(name, "decodes");
    } catch (const Error& error) {
        if (expected != error.what()) {
            Fail(name, error.what());
        }
    } catch (const std::exception& error) {
        Fail(name, std::string("unexpected error: ") + error.what());
    }
}

}

int main() {
    const std::array<std::uint32_t, 3> afterBranch{SBranchSkipOne, UnsupportedSop2, SEndpgm};
    ExpectProgram("unsupported word after s_branch", afterBranch, {0u, 8u});

    const std::array<std::uint32_t, 5> afterEndpgm{SCbranchScc1SkipThree, SEndpgm, UnknownFamily, UnsupportedSop2, SEndpgm};
    ExpectProgram("undecodable words after s_endpgm", afterEndpgm, {0u, 4u, 16u});

    const std::array<std::uint32_t, 4> afterCodeEnd{SCbranchScc1SkipTwo, SCodeEnd, UnknownFamily, SEndpgm};
    ExpectProgram("undecodable word after s_code_end", afterCodeEnd, {0u, 4u, 12u});

    const std::array<std::uint32_t, 6> codeAfterGap{SCbranchScc1SkipThree, SNop, SBranchSkipTwo, UnsupportedSop2, SNop, SEndpgm};
    ExpectProgram("code and targets after an unreachable word", codeAfterGap, {0u, 4u, 8u, 16u, 20u});
    ExpectBranchTargets("code and targets after an unreachable word", codeAfterGap, {16u, 20u});

    const std::array<std::uint32_t, 3> afterNop{SNop, UnsupportedSop2, SEndpgm};
    ExpectError<UnsupportedInstructionError>("unsupported word after s_nop", afterNop, "unsupported SOP2 opcode 12");

    const std::array<std::uint32_t, 4> branchTarget{SCbranchScc1SkipOne, SEndpgm, UnsupportedSop2, SEndpgm};
    ExpectError<UnsupportedInstructionError>("branch to an unsupported word", branchTarget, "unsupported SOP2 opcode 12");

    const std::array<std::uint32_t, 3> branchTargetAtSpanEnd{SCbranchScc1SkipOne, SEndpgm, UnsupportedSop2};
    ExpectError<UnsupportedInstructionError>("branch to an unsupported word at the end of the span", branchTargetAtSpanEnd, "unsupported SOP2 opcode 12");

    const std::array<std::uint32_t, 2> unknownFirst{UnknownFamily, SEndpgm};
    ExpectError<std::invalid_argument>("unknown first word", unknownFirst,
        "unknown RDNA instruction family at program counter 0x00000000 raw word 0xffffffff");

    return failures == 0 ? 0 : 1;
}
