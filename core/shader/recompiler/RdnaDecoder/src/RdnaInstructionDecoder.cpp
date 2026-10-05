#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "RdnaDecoder/RdnaExportOpDecoder.hpp"
#include "RdnaDecoder/RdnaImageOpDecoder.hpp"
#include "RdnaDecoder/RdnaMemoryOpDecoder.hpp"
#include "RdnaDecoder/RdnaOpcode.hpp"
#include "RdnaDecoder/RdnaScalarOpDecoder.hpp"
#include "RdnaDecoder/RdnaVectorOpDecoder.hpp"
#include <algorithm>
#include <bit>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace ShaderRecompiler {

namespace {

bool instructionHasLiteral(const RdnaInstruction& instruction) {
    return instruction.source0.kind == RdnaOperandKind::LiteralConstant ||
        instruction.source1.kind == RdnaOperandKind::LiteralConstant ||
        instruction.source2.kind == RdnaOperandKind::LiteralConstant ||
        instruction.source3.kind == RdnaOperandKind::LiteralConstant;
}

void applyLiteral(RdnaOperand& operand, std::uint32_t literal) {
    if (operand.kind != RdnaOperandKind::LiteralConstant) {
        return;
    }

    operand.value = literal;
    operand.signedVal = static_cast<std::int32_t>(literal);
}

std::string toHexString(std::uint32_t value) {
    char buffer[11];
    std::snprintf(buffer, sizeof(buffer), "0x%08x", value);
    return std::string(buffer);
}

}

RdnaProgram RdnaInstructionDecoder::Decode(std::span<const std::uint32_t> code) const {
    RdnaProgram program;
    DecodeRdnaProgram(code, program);
    return program;
}

RdnaInstruction RdnaInstructionDecoder::decodeAt(std::span<const std::uint32_t> code, std::uint32_t wordIndex) const {
    if (wordIndex >= code.size()) {
        throw std::out_of_range("word index is out of the code span bounds");
    }

    const std::uint32_t programCounter = wordIndex * static_cast<std::uint32_t>(sizeof(std::uint32_t));
    return DecodeRdnaInstruction(programCounter, code, wordIndex);
}

RdnaInstructionFamily GetRdnaInstructionFamily(std::uint32_t word) {
    if ((word & 0x80000000u) == 0u) {
        switch ((word >> 25u) & 0x3fu) {
            case 0x3eu: return RdnaInstructionFamily::VOPC;
            case 0x3fu: return RdnaInstructionFamily::VOP1;
            default: return RdnaInstructionFamily::VOP2;
        }
    }

    if ((word & 0xc0000000u) == 0x80000000u) {
        const std::uint32_t opcode = (word >> 23u) & 0x7fu;
        switch (opcode) {
            case 0x7du: return RdnaInstructionFamily::SOP1;
            case 0x7eu: return RdnaInstructionFamily::SOPC;
            case 0x7fu: return RdnaInstructionFamily::SOPP;
            default: return opcode >= 0x60u ? RdnaInstructionFamily::SOPK : RdnaInstructionFamily::SOP2;
        }
    }

    switch (word >> 26u) {
        case 0x32u: return RdnaInstructionFamily::VINTRP;
        case 0x33u: return RdnaInstructionFamily::VOP3P;
        case 0x35u: return RdnaInstructionFamily::VOP3;
        case 0x36u: return RdnaInstructionFamily::DS;
        case 0x37u: return RdnaInstructionFamily::FLAT;
        case 0x38u: return RdnaInstructionFamily::MUBUF;
        case 0x3au: return RdnaInstructionFamily::MTBUF;
        case 0x3cu: return RdnaInstructionFamily::MIMG;
        case 0x3du: return RdnaInstructionFamily::SMEM;
        case 0x3eu: return RdnaInstructionFamily::EXP;
        default: return RdnaInstructionFamily::Unknown;
    }
}

RdnaInstruction DecodeRdnaInstruction(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex) {
    if (wordIndex >= code.size()) {
        throw std::out_of_range("word index is out of the code span bounds");
    }

    switch (GetRdnaInstructionFamily(code[wordIndex])) {
        case RdnaInstructionFamily::SOP1: return DecodeRdnaSop1(programCounter, code, wordIndex);
        case RdnaInstructionFamily::SOP2: return DecodeRdnaSop2(programCounter, code, wordIndex);
        case RdnaInstructionFamily::SOPK: return DecodeRdnaSopk(programCounter, code, wordIndex);
        case RdnaInstructionFamily::SOPC: return DecodeRdnaSopc(programCounter, code, wordIndex);
        case RdnaInstructionFamily::SOPP: return DecodeRdnaSopp(programCounter, code, wordIndex);
        case RdnaInstructionFamily::VOP1: return DecodeRdnaVop1(programCounter, code, wordIndex);
        case RdnaInstructionFamily::VOP2: return DecodeRdnaVop2(programCounter, code, wordIndex);
        case RdnaInstructionFamily::VOP3: return DecodeRdnaVop3(programCounter, code, wordIndex);
        case RdnaInstructionFamily::VOP3P: return DecodeRdnaVop3p(programCounter, code, wordIndex);
        case RdnaInstructionFamily::VOPC: return DecodeRdnaVopc(programCounter, code, wordIndex);
        case RdnaInstructionFamily::VINTRP: return DecodeRdnaVintrp(programCounter, code, wordIndex);
        case RdnaInstructionFamily::SMEM: return DecodeRdnaSmem(programCounter, code, wordIndex);
        case RdnaInstructionFamily::MUBUF: return DecodeRdnaMubuf(programCounter, code, wordIndex);
        case RdnaInstructionFamily::MTBUF: return DecodeRdnaMtbuf(programCounter, code, wordIndex);
        case RdnaInstructionFamily::FLAT: return DecodeRdnaFlat(programCounter, code, wordIndex);
        case RdnaInstructionFamily::DS: return DecodeRdnaDs(programCounter, code, wordIndex);
        case RdnaInstructionFamily::MIMG: return DecodeRdnaMimg(programCounter, code, wordIndex);
        case RdnaInstructionFamily::EXP: return DecodeRdnaExportOp(programCounter, code, wordIndex);
        default: break;
    }

    throw std::invalid_argument("unknown RDNA instruction family at program counter " + toHexString(programCounter) + " raw word " + toHexString(code[wordIndex]));
}

RdnaProgram DecodeRdnaFrontProgram(std::span<const std::uint32_t> front) {
    RdnaProgram result;
    std::uint32_t frontWords = 0;

    while (frontWords < front.size()) {
        const std::uint32_t programCounter = frontWords * static_cast<std::uint32_t>(sizeof(std::uint32_t));
        result.instructions.push_back(DecodeRdnaInstruction(programCounter, front, frontWords));

        const RdnaInstruction& instruction = result.instructions.back();
        frontWords += instruction.wordCount;

        if (instruction.op == RdnaOpcode::SSetpcB64) {
            if (instruction.source0.kind != RdnaOperandKind::ScalarRegister || instruction.source0.reg != 6u) {
                throw std::invalid_argument("s_setpc_b64 in the front program does not target scalar register s6");
            }

            result.code = front.first(frontWords);
            return result;
        }

        if (instruction.op == RdnaOpcode::SEndpgm) {
            throw std::invalid_argument("front program reached s_endpgm before s_setpc_b64");
        }
    }

    throw std::out_of_range("front program decode reached the code boundary before s_setpc_b64");
}

void DecodeRdnaProgram(std::span<const std::uint32_t> code, RdnaProgram& program) {
    program.instructions.clear();
    program.instructions.reserve(code.size());
    program.code = code;

    std::uint32_t furthestBranchTarget = 0;
    for (std::uint32_t wordIndex = 0; wordIndex < code.size();) {
        const std::uint32_t programCounter = wordIndex * static_cast<std::uint32_t>(sizeof(std::uint32_t));
        program.instructions.push_back(DecodeRdnaInstruction(programCounter, code, wordIndex));

        const RdnaInstruction& instruction = program.instructions.back();
        wordIndex += instruction.wordCount;

        if (IsDirectBranchOpcode(instruction.op)) {
            const std::uint32_t targetIndex = instruction.branchTarget / static_cast<std::uint32_t>(sizeof(std::uint32_t));
            if (targetIndex >= code.size()) {
                throw std::out_of_range("branch target is out of the code span bounds");
            }
            furthestBranchTarget = std::max(furthestBranchTarget, targetIndex);
        }

        if (instruction.op == RdnaOpcode::SEndpgm && furthestBranchTarget < wordIndex) {
            return;
        }
    }

    throw std::out_of_range("RDNA program decode reached the code boundary before s_endpgm");
}

RdnaOperand DecodeRdnaScalarSource(std::uint32_t code, std::uint32_t programCounter) {
    RdnaOperand operand;

    if (code <= 105u) {
        operand.kind = RdnaOperandKind::ScalarRegister;
        operand.reg = code;
        return operand;
    }

    if (code >= 128u && code <= 192u) {
        operand.kind = RdnaOperandKind::IntegerInlineConstant;
        operand.signedVal = static_cast<std::int32_t>(code - 128u);
        operand.value = static_cast<std::uint32_t>(operand.signedVal);
        return operand;
    }

    if (code >= 193u && code <= 208u) {
        operand.kind = RdnaOperandKind::IntegerInlineConstant;
        operand.signedVal = 192 - static_cast<std::int32_t>(code);
        operand.value = static_cast<std::uint32_t>(operand.signedVal);
        return operand;
    }

    if (code >= 240u && code <= 247u) {
        static constexpr float inlineFloatConstants[] = {0.5f, -0.5f, 1.0f, -1.0f, 2.0f, -2.0f, 4.0f, -4.0f};
        operand.kind = RdnaOperandKind::FloatInlineConstant;
        operand.value = std::bit_cast<std::uint32_t>(inlineFloatConstants[code - 240u]);
        return operand;
    }

    if (code >= 256u && code <= 511u) {
        return DecodeRdnaVectorGpr(code - 256u);
    }

    switch (code) {
        case 106u: operand.kind = RdnaOperandKind::VccLo; return operand;
        case 107u: operand.kind = RdnaOperandKind::VccHi; return operand;
        case 124u: operand.kind = RdnaOperandKind::M0; return operand;
        case 125u: operand.kind = RdnaOperandKind::Null; return operand;
        case 126u: operand.kind = RdnaOperandKind::ExecLo; return operand;
        case 127u: operand.kind = RdnaOperandKind::ExecHi; return operand;
        case 239u: operand.kind = RdnaOperandKind::PopsExitingWaveId; return operand;
        case 248u:
            operand.kind = RdnaOperandKind::FloatInlineConstant;
            operand.value = std::bit_cast<std::uint32_t>(0.15915494309189535f);
            return operand;
        case 251u: operand.kind = RdnaOperandKind::VccZ; return operand;
        case 252u: operand.kind = RdnaOperandKind::ExecZ; return operand;
        case 253u: operand.kind = RdnaOperandKind::Scc; return operand;
        case 255u: operand.kind = RdnaOperandKind::LiteralConstant; return operand;
        default: break;
    }

    throw std::invalid_argument("unsupported scalar source operand code " + std::to_string(code) + " at program counter " + std::to_string(programCounter));
}

RdnaOperand DecodeRdnaScalarDestination(std::uint32_t code, std::uint32_t programCounter) {
    RdnaOperand operand;

    if (code <= 105u) {
        operand.kind = RdnaOperandKind::ScalarRegister;
        operand.reg = code;
        return operand;
    }

    switch (code) {
        case 106u: operand.kind = RdnaOperandKind::VccLo; return operand;
        case 107u: operand.kind = RdnaOperandKind::VccHi; return operand;
        case 124u: operand.kind = RdnaOperandKind::M0; return operand;
        case 125u: operand.kind = RdnaOperandKind::Null; return operand;
        case 126u: operand.kind = RdnaOperandKind::ExecLo; return operand;
        case 127u: operand.kind = RdnaOperandKind::ExecHi; return operand;
        default: break;
    }

    throw std::invalid_argument("unsupported scalar destination operand code " + std::to_string(code) + " at program counter " + std::to_string(programCounter));
}

RdnaOperand DecodeRdnaVectorGpr(std::uint32_t reg) {
    RdnaOperand operand;
    operand.kind = RdnaOperandKind::VectorRegister;
    operand.reg = reg;
    return operand;
}

void SetRdnaRawWords(RdnaInstruction& instruction, std::span<const std::uint32_t> code, std::uint32_t wordIndex, std::uint32_t wordCount) {
    if (wordCount > MaxRdnaInstructionRawWords) {
        throw std::invalid_argument("raw word count exceeds the maximum instruction word capacity");
    }
    if (static_cast<std::size_t>(wordIndex) + wordCount > code.size()) {
        throw std::out_of_range("instruction words exceed the bounds of the code span");
    }

    instruction.wordCount = wordCount;
    for (std::uint32_t i = 0; i < wordCount; ++i) {
        instruction.rawWords[i] = code[wordIndex + i];
    }
}

void ReadRdnaLiteralOperands(std::span<const std::uint32_t> code, std::uint32_t wordIndex, RdnaInstruction& instruction) {
    if (!instructionHasLiteral(instruction)) {
        return;
    }
    if (static_cast<std::size_t>(wordIndex) + instruction.wordCount >= code.size()) {
        throw std::out_of_range("literal operand word is out of the code span bounds");
    }

    const std::uint32_t literal = code[wordIndex + instruction.wordCount];
    applyLiteral(instruction.source0, literal);
    applyLiteral(instruction.source1, literal);
    applyLiteral(instruction.source2, literal);
    applyLiteral(instruction.source3, literal);
    instruction.wordCount++;
    SetRdnaRawWords(instruction, code, wordIndex, instruction.wordCount);
}

void SetRdnaUnsupported(RdnaInstruction& instruction, RdnaInstructionFamily family, std::uint32_t opcodeId, const char* reason) {
    throw std::invalid_argument("unsupported RDNA instruction, family=" + std::to_string(static_cast<std::uint32_t>(family)) + " opcode=" + toHexString(opcodeId) + " reason=" + reason + " pc=" + toHexString(instruction.programCounter));
}

std::string RdnaOperandToString(const RdnaOperand& operand) {
    switch (operand.kind) {
        case RdnaOperandKind::None: return "none";
        case RdnaOperandKind::ScalarRegister: return "s" + std::to_string(operand.reg);
        case RdnaOperandKind::VectorRegister: return "v" + std::to_string(operand.reg);
        case RdnaOperandKind::VccLo: return "vcc_lo";
        case RdnaOperandKind::VccHi: return "vcc_hi";
        case RdnaOperandKind::ExecLo: return "exec_lo";
        case RdnaOperandKind::ExecHi: return "exec_hi";
        case RdnaOperandKind::Scc: return "scc";
        case RdnaOperandKind::Null: return "null";
        case RdnaOperandKind::LiteralConstant: return toHexString(operand.value);
        case RdnaOperandKind::IntegerInlineConstant: return std::to_string(operand.signedVal);
        case RdnaOperandKind::FloatInlineConstant: return toHexString(operand.value);
        case RdnaOperandKind::Unknown: return "unknown";
        case RdnaOperandKind::VccZ: return "vccz";
        case RdnaOperandKind::ExecZ: return "execz";
        case RdnaOperandKind::M0: return "m0";
        case RdnaOperandKind::PopsExitingWaveId: return "pops_exiting_wave_id";
    }

    throw std::invalid_argument("unsupported operand kind for string conversion");
}

const char* RdnaImageDimensionToString(RdnaImageDimension dimension) {
    switch (dimension) {
        case RdnaImageDimension::Unknown: return "unknown";
        case RdnaImageDimension::Dim1D: return "1d";
        case RdnaImageDimension::Dim1DArray: return "1d_array";
        case RdnaImageDimension::Dim2D: return "2d";
        case RdnaImageDimension::Dim3D: return "3d";
        case RdnaImageDimension::Dim2DArray: return "2d_array";
        case RdnaImageDimension::Dim2DMsaa: return "2d_msaa";
        case RdnaImageDimension::Dim2DMsaaArray: return "2d_msaa_array";
    }

    throw std::invalid_argument("unsupported image dimension for string conversion");
}

std::string RdnaInstructionToString(const RdnaInstruction& instruction) {
    std::string text = toHexString(instruction.programCounter) + ": family=" + std::to_string(static_cast<std::uint32_t>(instruction.family)) + " opcode=" + toHexString(instruction.opcodeId);

    if (instruction.destination.kind != RdnaOperandKind::Unknown) {
        text += " dst=" + RdnaOperandToString(instruction.destination);
    }
    if (instruction.destination2.kind != RdnaOperandKind::Unknown) {
        text += " dst2=" + RdnaOperandToString(instruction.destination2);
    }

    const RdnaOperand* sources[] = {&instruction.source0, &instruction.source1, &instruction.source2, &instruction.source3};
    for (std::uint32_t i = 0; i < instruction.sourceCount && i < 4u; ++i) {
        text += " src" + std::to_string(i) + "=" + RdnaOperandToString(*sources[i]);
    }

    return text;
}

std::string RdnaProgramToString(const RdnaProgram& program) {
    std::string text;
    for (const RdnaInstruction& instruction : program.instructions) {
        text += RdnaInstructionToString(instruction);
        text += "\n";
    }
    return text;
}

}
