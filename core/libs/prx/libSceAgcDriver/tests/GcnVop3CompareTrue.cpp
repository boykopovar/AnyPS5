#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <exception>
#include <stdexcept>

using namespace ShaderRecompiler;

namespace {

int failures = 0;

void Check(bool condition, const char* what) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", what);
    ++failures;
}

struct Comparison {
    std::uint32_t encoding;
    RdnaOpcode opcode;
    bool writesExec;
};

void CheckComparison(const Comparison& comparison, std::uint32_t destination, bool e64) {
    const std::array<std::uint32_t, 3> words{
        e64 ? 0xd0000000u | (comparison.encoding << 17u) | destination : 0x7c000300u | (comparison.encoding << 17u),
        e64 ? 0x00020300u : 0xbf810000u,
        0xbf810000u,
    };
    try {
        const auto instruction = DecodeGcnInstruction(0u, words, 0u);
        Check(instruction.op == comparison.opcode, "comparison has the wrong opcode");
        Check(instruction.wordCount == (e64 ? 2u : 1u), "comparison has the wrong word count");
        Check(instruction.sourceCount == 2u, "comparison has the wrong source count");
        Check(instruction.source0.kind == RdnaOperandKind::VectorRegister && instruction.source0.reg == 0u, "comparison lost source v0");
        Check(instruction.source1.kind == RdnaOperandKind::VectorRegister && instruction.source1.reg == 1u, "comparison lost source v1");
        const auto scalarKind = destination == 106u ? RdnaOperandKind::VccLo : RdnaOperandKind::ScalarRegister;
        if (comparison.writesExec) {
            Check(instruction.destination.kind == RdnaOperandKind::ExecLo, "CMPX lost its EXEC destination");
            Check(instruction.destination2.kind == (e64 ? scalarKind : RdnaOperandKind::VccLo), "CMPX lost its scalar destination kind");
            if (e64 && destination != 106u) Check(instruction.destination2.reg == destination, "CMPX lost its scalar destination register");
        } else {
            Check(instruction.destination.kind == (e64 ? scalarKind : RdnaOperandKind::VccLo), "CMP lost its scalar destination kind");
            if (e64 && destination != 106u) Check(instruction.destination.reg == destination, "CMP lost its scalar destination register");
            Check(instruction.destination2.kind == RdnaOperandKind::Unknown, "CMP gained a second destination");
        }
        const auto following = DecodeGcnInstruction(instruction.wordCount * 4u, words, instruction.wordCount);
        Check(following.op == RdnaOpcode::SEndpgm && following.wordCount == 1u, "comparison consumed the following s_endpgm");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "comparison 0x%x %s destination %u failed: %s\n", comparison.encoding, e64 ? "e64" : "e32", destination, error.what());
        ++failures;
    }
}

void CheckTruncated(std::uint32_t opcode) {
    const std::array<std::uint32_t, 1> words{0xd0000004u | (opcode << 17u)};
    try {
        static_cast<void>(DecodeGcnInstruction(0u, words, 0u));
        Check(false, "truncated VOP3 was accepted");
    } catch (const std::out_of_range&) {
    } catch (const std::exception& error) {
        std::fprintf(stderr, "truncated VOP3 0x%x threw the wrong exception: %s\n", opcode, error.what());
        ++failures;
    }
}

}

int main() {
    const std::array<Comparison, 6> comparisons{{
        {0x1fu, RdnaOpcode::VCmpxTruF32, true},
        {0x3fu, RdnaOpcode::VCmpxTruF64, true},
        {0x1eu, RdnaOpcode::VCmpxNltF32, true},
        {0x3eu, RdnaOpcode::VCmpxNltF64, true},
        {0x0fu, RdnaOpcode::VCmpTruF32, false},
        {0x2fu, RdnaOpcode::VCmpTruF64, false},
    }};
    for (const auto& comparison : comparisons) {
        for (const auto destination : {4u, 8u, 106u}) CheckComparison(comparison, destination, true);
        CheckComparison(comparison, 106u, false);
    }
    for (const auto opcode : {0x1fu, 0x3fu}) CheckTruncated(opcode);
    if (failures == 0) std::puts("GCN VOP3 compare true tests passed");
    return failures == 0 ? 0 : 1;
}
