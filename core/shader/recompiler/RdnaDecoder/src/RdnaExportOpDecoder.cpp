#include "RdnaDecoder/RdnaExportOpDecoder.hpp"
#include <limits>
#include <stdexcept>

namespace ShaderRecompiler {

RdnaInstruction DecodeRdnaExportOp(std::span<const std::uint32_t> code, std::uint32_t wordIndex) {
    if (wordIndex > std::numeric_limits<std::uint32_t>::max() / 4u) {
        throw std::runtime_error("export program counter overflow");
    }
    return DecodeRdnaExportOp(wordIndex * 4u, code, wordIndex);
}

RdnaInstruction DecodeRdnaExportOp(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex) {
    const std::size_t index = wordIndex;
    if (index >= code.size() || code.size() - index < 2u) {
        throw std::out_of_range("truncated export instruction");
    }
    if (programCounter % 4u != 0u || programCounter > std::numeric_limits<std::uint32_t>::max() - 7u) {
        throw std::runtime_error("invalid export program counter");
    }

    const std::uint32_t word0 = code[index];
    const std::uint32_t word1 = code[index + 1u];
    if ((word0 >> 26u) != 0x3Eu || (word0 & 0x03FFE000u) != 0u) {
        throw std::runtime_error("invalid export encoding");
    }

    RdnaInstruction instruction{};
    instruction.op = RdnaOpcode::Exp;
    instruction.programCounter = programCounter;
    instruction.wordCount = 2;
    instruction.rawWords[0] = word0;
    instruction.rawWords[1] = word1;
    instruction.family = RdnaInstructionFamily::EXP;
    instruction.exportTarget = (word0 >> 4u) & 0x3Fu;
    instruction.opcodeId = instruction.exportTarget;
    instruction.exportEnableMask = word0 & 0xFu;
    instruction.exportIsCompressed = ((word0 >> 10u) & 1u) != 0u;
    instruction.exportIsLast = ((word0 >> 11u) & 1u) != 0u;
    instruction.exportValidMask = ((word0 >> 12u) & 1u) != 0u;

    instruction.source0.kind = RdnaOperandKind::VectorRegister;
    instruction.source0.reg = word1 & 0xFFu;
    instruction.source1.kind = RdnaOperandKind::VectorRegister;
    instruction.source1.reg = (word1 >> 8u) & 0xFFu;
    instruction.source2.kind = RdnaOperandKind::VectorRegister;
    instruction.source2.reg = (word1 >> 16u) & 0xFFu;
    instruction.source3.kind = RdnaOperandKind::VectorRegister;
    instruction.source3.reg = (word1 >> 24u) & 0xFFu;

    if (instruction.exportEnableMask == 0u) {
        instruction.sourceCount = 0;
    } else if (instruction.exportTarget == 0x14u && instruction.exportIsLast && instruction.exportEnableMask == 1u) {
        instruction.sourceCount = 1;
    } else {
        instruction.sourceCount = instruction.exportIsCompressed ? 2u : 4u;
    }
    return instruction;
}

}
