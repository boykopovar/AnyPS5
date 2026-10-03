#include "RdnaDecoder/RdnaScalarOpDecoder.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include <stdexcept>
#include <string>

namespace ShaderRecompiler {

namespace {

RdnaOpcode decodeSop1Opcode(std::uint32_t opcode) {
    switch (opcode) {
        case 0x03u: return RdnaOpcode::SMovB32;
        case 0x04u: return RdnaOpcode::SMovB64;
        case 0x05u: return RdnaOpcode::SCmovB32;
        case 0x06u: return RdnaOpcode::SCmovB64;
        case 0x07u: return RdnaOpcode::SNotB32;
        case 0x08u: return RdnaOpcode::SNotB64;
        case 0x09u: return RdnaOpcode::SWqmB32;
        case 0x0au: return RdnaOpcode::SWqmB64;
        case 0x0bu: return RdnaOpcode::SBrevB32;
        case 0x0cu: return RdnaOpcode::SBrevB64;
        case 0x0du: return RdnaOpcode::SBcnt0I32B32;
        case 0x0eu: return RdnaOpcode::SBcnt0I32B64;
        case 0x0fu: return RdnaOpcode::SBcnt1I32B32;
        case 0x10u: return RdnaOpcode::SBcnt1I32B64;
        case 0x11u: return RdnaOpcode::SFf0I32B32;
        case 0x12u: return RdnaOpcode::SFf0I32B64;
        case 0x13u: return RdnaOpcode::SFf1I32B32;
        case 0x14u: return RdnaOpcode::SFf1I32B64;
        case 0x15u: return RdnaOpcode::SFlbitI32B32;
        case 0x16u: return RdnaOpcode::SFlbitI32B64;
        case 0x17u: return RdnaOpcode::SFlbitI32;
        case 0x18u: return RdnaOpcode::SFlbitI32I64;
        case 0x19u: return RdnaOpcode::SSextI32I8;
        case 0x1au: return RdnaOpcode::SSextI32I16;
        case 0x1bu: return RdnaOpcode::SBitset0B32;
        case 0x1cu: return RdnaOpcode::SBitset0B64;
        case 0x1du: return RdnaOpcode::SBitset1B32;
        case 0x1eu: return RdnaOpcode::SBitset1B64;
        case 0x1fu: return RdnaOpcode::SGetpcB64;
        case 0x20u: return RdnaOpcode::SSetpcB64;
        case 0x24u: return RdnaOpcode::SAndSaveexecB64;
        case 0x25u: return RdnaOpcode::SOrSaveexecB64;
        case 0x26u: return RdnaOpcode::SXorSaveexecB64;
        case 0x27u: return RdnaOpcode::SAndn2SaveexecB64;
        case 0x28u: return RdnaOpcode::SOrn2SaveexecB64;
        case 0x29u: return RdnaOpcode::SNandSaveexecB64;
        case 0x2au: return RdnaOpcode::SNorSaveexecB64;
        case 0x2bu: return RdnaOpcode::SXnorSaveexecB64;
        case 0x2cu: return RdnaOpcode::SQuadmaskB32;
        case 0x2du: return RdnaOpcode::SQuadmaskB64;
        case 0x2eu: return RdnaOpcode::SMovrelsB32;
        case 0x2fu: return RdnaOpcode::SMovrelsB64;
        case 0x30u: return RdnaOpcode::SMovreldB32;
        case 0x31u: return RdnaOpcode::SMovreldB64;
        case 0x34u: return RdnaOpcode::SAbsI32;
        case 0x37u: return RdnaOpcode::SAndn1SaveexecB64;
        case 0x38u: return RdnaOpcode::SOrn1SaveexecB64;
        case 0x39u: return RdnaOpcode::SAndn1WrexecB64;
        case 0x3au: return RdnaOpcode::SAndn2WrexecB64;
        case 0x3bu: return RdnaOpcode::SBitreplicateB64B32;
        case 0x3cu: return RdnaOpcode::SAndSaveexecB32;
        case 0x3du: return RdnaOpcode::SOrSaveexecB32;
        case 0x3eu: return RdnaOpcode::SXorSaveexecB32;
        case 0x3fu: return RdnaOpcode::SAndn2SaveexecB32;
        case 0x40u: return RdnaOpcode::SOrn2SaveexecB32;
        case 0x41u: return RdnaOpcode::SNandSaveexecB32;
        case 0x42u: return RdnaOpcode::SNorSaveexecB32;
        case 0x43u: return RdnaOpcode::SXnorSaveexecB32;
        case 0x44u: return RdnaOpcode::SAndn1SaveexecB32;
        case 0x45u: return RdnaOpcode::SOrn1SaveexecB32;
        case 0x46u: return RdnaOpcode::SAndn1WrexecB32;
        case 0x47u: return RdnaOpcode::SAndn2WrexecB32;
        case 0x49u: return RdnaOpcode::SMovrelsd2B32;
        default: throw std::invalid_argument("unsupported SOP1 opcode " + std::to_string(opcode));
    }
}

RdnaOpcode decodeSop2Opcode(std::uint32_t opcode) {
    switch (opcode) {
        case 0x00u: return RdnaOpcode::SAddU32;
        case 0x01u: return RdnaOpcode::SSubU32;
        case 0x02u: return RdnaOpcode::SAddI32;
        case 0x03u: return RdnaOpcode::SSubI32;
        case 0x04u: return RdnaOpcode::SAddcU32;
        case 0x05u: return RdnaOpcode::SSubbU32;
        case 0x06u: return RdnaOpcode::SMinI32;
        case 0x07u: return RdnaOpcode::SMinU32;
        case 0x08u: return RdnaOpcode::SMaxI32;
        case 0x09u: return RdnaOpcode::SMaxU32;
        case 0x0au: return RdnaOpcode::SCselectB32;
        case 0x0bu: return RdnaOpcode::SCselectB64;
        case 0x0eu: return RdnaOpcode::SAndB32;
        case 0x0fu: return RdnaOpcode::SAndB64;
        case 0x10u: return RdnaOpcode::SOrB32;
        case 0x11u: return RdnaOpcode::SOrB64;
        case 0x12u: return RdnaOpcode::SXorB32;
        case 0x13u: return RdnaOpcode::SXorB64;
        case 0x14u: return RdnaOpcode::SAndn2B32;
        case 0x15u: return RdnaOpcode::SAndn2B64;
        case 0x16u: return RdnaOpcode::SOrn2B32;
        case 0x17u: return RdnaOpcode::SOrn2B64;
        case 0x18u: return RdnaOpcode::SNandB32;
        case 0x19u: return RdnaOpcode::SNandB64;
        case 0x1au: return RdnaOpcode::SNorB32;
        case 0x1bu: return RdnaOpcode::SNorB64;
        case 0x1cu: return RdnaOpcode::SXnorB32;
        case 0x1du: return RdnaOpcode::SXnorB64;
        case 0x1eu: return RdnaOpcode::SLshlB32;
        case 0x1fu: return RdnaOpcode::SLshlB64;
        case 0x20u: return RdnaOpcode::SLshrB32;
        case 0x21u: return RdnaOpcode::SLshrB64;
        case 0x22u: return RdnaOpcode::SAshrI32;
        case 0x23u: return RdnaOpcode::SAshrI64;
        case 0x24u: return RdnaOpcode::SBfmB32;
        case 0x25u: return RdnaOpcode::SBfmB64;
        case 0x26u: return RdnaOpcode::SMulI32;
        case 0x27u: return RdnaOpcode::SBfeU32;
        case 0x28u: return RdnaOpcode::SBfeI32;
        case 0x29u: return RdnaOpcode::SBfeU64;
        case 0x2cu: return RdnaOpcode::SAbsdiffI32;
        case 0x2eu: return RdnaOpcode::SLshl1AddU32;
        case 0x2fu: return RdnaOpcode::SLshl2AddU32;
        case 0x30u: return RdnaOpcode::SLshl3AddU32;
        case 0x31u: return RdnaOpcode::SLshl4AddU32;
        case 0x32u: return RdnaOpcode::SPackLlB32B16;
        case 0x33u: return RdnaOpcode::SPackLhB32B16;
        case 0x34u: return RdnaOpcode::SPackHhB32B16;
        case 0x35u: return RdnaOpcode::SMulHiU32;
        case 0x36u: return RdnaOpcode::SMulHiI32;
        default: throw std::invalid_argument("unsupported SOP2 opcode " + std::to_string(opcode));
    }
}

RdnaOpcode decodeSopcOpcode(std::uint32_t opcode) {
    switch (opcode) {
        case 0x00u: return RdnaOpcode::SCmpEqI32;
        case 0x01u: return RdnaOpcode::SCmpLgI32;
        case 0x02u: return RdnaOpcode::SCmpGtI32;
        case 0x03u: return RdnaOpcode::SCmpGeI32;
        case 0x04u: return RdnaOpcode::SCmpLtI32;
        case 0x05u: return RdnaOpcode::SCmpLeI32;
        case 0x06u: return RdnaOpcode::SCmpEqU32;
        case 0x07u: return RdnaOpcode::SCmpLgU32;
        case 0x08u: return RdnaOpcode::SCmpGtU32;
        case 0x09u: return RdnaOpcode::SCmpGeU32;
        case 0x0au: return RdnaOpcode::SCmpLtU32;
        case 0x0bu: return RdnaOpcode::SCmpLeU32;
        case 0x0cu: return RdnaOpcode::SBitcmp0B32;
        case 0x0du: return RdnaOpcode::SBitcmp1B32;
        case 0x0eu: return RdnaOpcode::SBitcmp0B64;
        case 0x0fu: return RdnaOpcode::SBitcmp1B64;
        case 0x12u: return RdnaOpcode::SCmpEqU64;
        case 0x13u: return RdnaOpcode::SCmpLgU64;
        default: throw std::invalid_argument("unsupported SOPC opcode " + std::to_string(opcode));
    }
}

RdnaOpcode decodeSopkOpcode(std::uint32_t opcode) {
    switch (opcode) {
        case 0x00u: return RdnaOpcode::SMovkI32;
        case 0x03u: return RdnaOpcode::SCmpEqI32;
        case 0x04u: return RdnaOpcode::SCmpLgI32;
        case 0x05u: return RdnaOpcode::SCmpGtI32;
        case 0x06u: return RdnaOpcode::SCmpGeI32;
        case 0x07u: return RdnaOpcode::SCmpLtI32;
        case 0x08u: return RdnaOpcode::SCmpLeI32;
        case 0x09u: return RdnaOpcode::SCmpEqU32;
        case 0x0au: return RdnaOpcode::SCmpLgU32;
        case 0x0bu: return RdnaOpcode::SCmpGtU32;
        case 0x0cu: return RdnaOpcode::SCmpGeU32;
        case 0x0du: return RdnaOpcode::SCmpLtU32;
        case 0x0eu: return RdnaOpcode::SCmpLeU32;
        case 0x0fu: return RdnaOpcode::SAddI32;
        case 0x10u: return RdnaOpcode::SMulkI32;
        case 0x13u: return RdnaOpcode::SSetregB32;
        case 0x17u: return RdnaOpcode::SWaitcnt;
        case 0x18u: return RdnaOpcode::SWaitcnt;
        case 0x19u: return RdnaOpcode::SWaitcnt;
        case 0x1au: return RdnaOpcode::SWaitcnt;
        case 0x1bu: return RdnaOpcode::SSubvectorLoopBegin;
        case 0x1cu: return RdnaOpcode::SSubvectorLoopEnd;
        default: throw std::invalid_argument("unsupported SOPK opcode " + std::to_string(opcode));
    }
}

RdnaOpcode decodeSoppOpcode(std::uint32_t opcode) {
    switch (opcode) {
        case 0x00u: return RdnaOpcode::SNop;
        case 0x01u: return RdnaOpcode::SEndpgm;
        case 0x02u: return RdnaOpcode::SBranch;
        case 0x04u: return RdnaOpcode::SCbranchScc0;
        case 0x05u: return RdnaOpcode::SCbranchScc1;
        case 0x06u: return RdnaOpcode::SCbranchVccz;
        case 0x07u: return RdnaOpcode::SCbranchVccnz;
        case 0x08u: return RdnaOpcode::SCbranchExecz;
        case 0x09u: return RdnaOpcode::SCbranchExecnz;
        case 0x0au: return RdnaOpcode::SBarrier;
        case 0x0cu: return RdnaOpcode::SWaitcnt;
        case 0x0eu: return RdnaOpcode::SSleep;
        case 0x0fu: return RdnaOpcode::SSetprio;
        case 0x10u: return RdnaOpcode::SSendmsg;
        case 0x12u: return RdnaOpcode::STrap;
        case 0x13u: return RdnaOpcode::SIcacheInv;
        case 0x14u: return RdnaOpcode::SIncperflevel;
        case 0x15u: return RdnaOpcode::SDecperflevel;
        case 0x16u: return RdnaOpcode::STtracedata;
        case 0x17u:
        case 0x18u:
        case 0x19u:
        case 0x1au: return RdnaOpcode::SCbranchCdbg;
        case 0x20u: return RdnaOpcode::SInstPrefetch;
        case 0x21u: return RdnaOpcode::SClause;
        case 0x22u: return RdnaOpcode::SWaitIdle;
        case 0x23u: return RdnaOpcode::SWaitcntDepctr;
        case 0x25u: return RdnaOpcode::SDenormMode;
        case 0x28u: return RdnaOpcode::STtracedata;
        default: throw std::invalid_argument("unsupported SOPP opcode " + std::to_string(opcode));
    }
}

void decodeScalarBinarySources(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex, RdnaInstruction& instruction, std::uint32_t scalarSource0, std::uint32_t scalarSource1) {
    instruction.source0 = DecodeRdnaScalarSource(scalarSource0, programCounter);
    instruction.source1 = DecodeRdnaScalarSource(scalarSource1, programCounter);
    instruction.sourceCount = 2;
    ReadRdnaLiteralOperands(code, wordIndex, instruction);
}

bool isSoppWaitOpcode(RdnaOpcode opcode) {
    return opcode == RdnaOpcode::SNop || opcode == RdnaOpcode::SWaitcnt || opcode == RdnaOpcode::SWaitcntDepctr ||
        opcode == RdnaOpcode::SSleep || opcode == RdnaOpcode::SSetprio || opcode == RdnaOpcode::SSendmsg ||
        opcode == RdnaOpcode::STrap || opcode == RdnaOpcode::STtracedata || opcode == RdnaOpcode::SInstPrefetch ||
        opcode == RdnaOpcode::SClause || opcode == RdnaOpcode::SCbranchCdbg || opcode == RdnaOpcode::SIcacheInv ||
        opcode == RdnaOpcode::SIncperflevel || opcode == RdnaOpcode::SDecperflevel || opcode == RdnaOpcode::SWaitIdle ||
        opcode == RdnaOpcode::SDenormMode;
}

std::uint32_t scalarDestinationDwordCount(RdnaOpcode opcode) {
    switch (opcode) {
        case RdnaOpcode::SMovB64:
        case RdnaOpcode::SCmovB64:
        case RdnaOpcode::SNotB64:
        case RdnaOpcode::SBrevB64:
        case RdnaOpcode::SAshrI64:
        case RdnaOpcode::SWqmB64:
        case RdnaOpcode::SBitset0B64:
        case RdnaOpcode::SBitset1B64:
        case RdnaOpcode::SGetpcB64:
        case RdnaOpcode::SAndSaveexecB64:
        case RdnaOpcode::SOrSaveexecB64:
        case RdnaOpcode::SXorSaveexecB64:
        case RdnaOpcode::SAndn2SaveexecB64:
        case RdnaOpcode::SOrn2SaveexecB64:
        case RdnaOpcode::SNandSaveexecB64:
        case RdnaOpcode::SNorSaveexecB64:
        case RdnaOpcode::SXnorSaveexecB64:
        case RdnaOpcode::SOrn1SaveexecB64:
        case RdnaOpcode::SAndn1WrexecB64:
        case RdnaOpcode::SAndn2WrexecB64:
        case RdnaOpcode::SQuadmaskB64:
        case RdnaOpcode::SMovrelsB64:
        case RdnaOpcode::SMovreldB64:
        case RdnaOpcode::SAndn1SaveexecB64:
        case RdnaOpcode::SBitreplicateB64B32:
        case RdnaOpcode::SCselectB64:
        case RdnaOpcode::SAndB64:
        case RdnaOpcode::SOrB64:
        case RdnaOpcode::SXorB64:
        case RdnaOpcode::SAndn2B64:
        case RdnaOpcode::SOrn2B64:
        case RdnaOpcode::SNandB64:
        case RdnaOpcode::SNorB64:
        case RdnaOpcode::SXnorB64:
        case RdnaOpcode::SLshlB64:
        case RdnaOpcode::SLshrB64:
        case RdnaOpcode::SBfmB64:
        case RdnaOpcode::SBfeU64: return 2u;
        default: return 1u;
    }
}

}

RdnaInstruction DecodeRdnaScalarOp(std::span<const std::uint32_t> code, std::uint32_t wordIndex) {
    if (wordIndex >= code.size()) {
        throw std::out_of_range("word index is out of the code span bounds");
    }

    const std::uint32_t word = code[wordIndex];
    if ((word & 0xc0000000u) != 0x80000000u) {
        throw std::invalid_argument("word at the given index is not a scalar instruction");
    }

    const std::uint32_t programCounter = wordIndex * static_cast<std::uint32_t>(sizeof(std::uint32_t));
    const std::uint32_t subOpcode = (word >> 23u) & 0x7fu;

    if (subOpcode == 0x7du) {
        return DecodeRdnaSop1(programCounter, code, wordIndex);
    }
    if (subOpcode == 0x7eu) {
        return DecodeRdnaSopc(programCounter, code, wordIndex);
    }
    if (subOpcode == 0x7fu) {
        return DecodeRdnaSopp(programCounter, code, wordIndex);
    }
    if (subOpcode >= 0x60u) {
        return DecodeRdnaSopk(programCounter, code, wordIndex);
    }

    return DecodeRdnaSop2(programCounter, code, wordIndex);
}

RdnaInstruction DecodeRdnaSop1(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex) {
    const std::uint32_t word = code[wordIndex];
    const std::uint32_t opcode = (word >> 8u) & 0xffu;
    const std::uint32_t scalarSource0 = word & 0xffu;
    const std::uint32_t scalarDestination = (word >> 16u) & 0x7fu;

    RdnaInstruction instruction;
    instruction.programCounter = programCounter;
    instruction.family = RdnaInstructionFamily::SOP1;
    instruction.opcodeId = opcode;
    instruction.op = decodeSop1Opcode(opcode);
    instruction.dataDwordCount = scalarDestinationDwordCount(instruction.op);
    SetRdnaRawWords(instruction, code, wordIndex, 1);

    if (instruction.op == RdnaOpcode::SGetpcB64) {
        instruction.sourceCount = 0;
        instruction.destination = DecodeRdnaScalarDestination(scalarDestination, programCounter);
        return instruction;
    }
    if (instruction.op == RdnaOpcode::SSetpcB64) {
        instruction.sourceCount = 1;
        instruction.destination.kind = RdnaOperandKind::Null;
        instruction.source0 = DecodeRdnaScalarSource(scalarSource0, programCounter);
        ReadRdnaLiteralOperands(code, wordIndex, instruction);
        return instruction;
    }

    instruction.source0 = DecodeRdnaScalarSource(scalarSource0, programCounter);
    instruction.destination = DecodeRdnaScalarDestination(scalarDestination, programCounter);
    instruction.sourceCount = 1;
    ReadRdnaLiteralOperands(code, wordIndex, instruction);
    return instruction;
}

RdnaInstruction DecodeRdnaSop2(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex) {
    const std::uint32_t word = code[wordIndex];
    const std::uint32_t opcode = (word >> 23u) & 0x7fu;
    const std::uint32_t scalarSource1 = (word >> 8u) & 0xffu;
    const std::uint32_t scalarSource0 = word & 0xffu;
    const std::uint32_t scalarDestination = (word >> 16u) & 0x7fu;

    RdnaInstruction instruction;
    instruction.programCounter = programCounter;
    instruction.family = RdnaInstructionFamily::SOP2;
    instruction.opcodeId = opcode;
    instruction.op = decodeSop2Opcode(opcode);
    instruction.dataDwordCount = scalarDestinationDwordCount(instruction.op);
    SetRdnaRawWords(instruction, code, wordIndex, 1);

    instruction.destination = DecodeRdnaScalarDestination(scalarDestination, programCounter);
    decodeScalarBinarySources(programCounter, code, wordIndex, instruction, scalarSource0, scalarSource1);
    return instruction;
}

RdnaInstruction DecodeRdnaSopk(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex) {
    const std::uint32_t word = code[wordIndex];
    const std::uint32_t opcode = (word >> 23u) & 0x1fu;
    const std::uint32_t scalarRegister = (word >> 16u) & 0x7fu;
    const std::int32_t immediate = (opcode >= 0x09u && opcode <= 0x0eu)
        ? static_cast<std::int32_t>(word & 0xffffu)
        : static_cast<std::int32_t>(static_cast<std::int16_t>(word & 0xffffu));

    RdnaInstruction instruction;
    instruction.programCounter = programCounter;
    instruction.family = RdnaInstructionFamily::SOPK;
    instruction.opcodeId = opcode;
    instruction.op = decodeSopkOpcode(opcode);
    instruction.source0.kind = RdnaOperandKind::IntegerInlineConstant;
    instruction.source0.signedVal = immediate;
    instruction.source0.value = static_cast<std::uint32_t>(immediate);
    instruction.sourceCount = 1;
    SetRdnaRawWords(instruction, code, wordIndex, 1);

    if (instruction.op == RdnaOpcode::SMovkI32) {
        instruction.destination = DecodeRdnaScalarDestination(scalarRegister, programCounter);
        return instruction;
    }
    if (instruction.op == RdnaOpcode::SSubvectorLoopBegin || instruction.op == RdnaOpcode::SSubvectorLoopEnd) {
        instruction.destination = DecodeRdnaScalarDestination(scalarRegister, programCounter);
        instruction.branchTarget = programCounter + 4u + static_cast<std::uint32_t>(immediate * 4);
        return instruction;
    }
    if (instruction.op == RdnaOpcode::SWaitcnt) {
        const std::uint32_t waitcnt = word & 0xffffu;
        instruction.destination.kind = RdnaOperandKind::Null;
        instruction.source0.signedVal = static_cast<std::int32_t>(waitcnt);
        instruction.source0.value = waitcnt;
        instruction.sourceCount = 1;
        return instruction;
    }
    if (instruction.op == RdnaOpcode::SSetregB32) {
        instruction.destination.kind = RdnaOperandKind::Null;
        instruction.source1.kind = RdnaOperandKind::LiteralConstant;
        instruction.source1.value = word & 0xffffu;
        instruction.source1.signedVal = immediate;
        instruction.sourceCount = 2;
        instruction.source0 = DecodeRdnaScalarSource(scalarRegister, programCounter);
        return instruction;
    }

    instruction.source1 = instruction.source0;
    instruction.source0 = DecodeRdnaScalarSource(scalarRegister, programCounter);
    if (instruction.op == RdnaOpcode::SAddI32 || instruction.op == RdnaOpcode::SMulkI32) {
        instruction.sourceCount = 2;
        instruction.destination = DecodeRdnaScalarDestination(scalarRegister, programCounter);
        return instruction;
    }

    instruction.destination.kind = RdnaOperandKind::Scc;
    instruction.sourceCount = 2;
    return instruction;
}

RdnaInstruction DecodeRdnaSopc(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex) {
    const std::uint32_t word = code[wordIndex];
    const std::uint32_t scalarSource1 = (word >> 8u) & 0xffu;
    const std::uint32_t scalarSource0 = word & 0xffu;
    const std::uint32_t opcode = (word >> 16u) & 0x7fu;

    RdnaInstruction instruction;
    instruction.programCounter = programCounter;
    instruction.family = RdnaInstructionFamily::SOPC;
    instruction.opcodeId = opcode;
    instruction.op = decodeSopcOpcode(opcode);
    instruction.destination.kind = RdnaOperandKind::Scc;
    SetRdnaRawWords(instruction, code, wordIndex, 1);

    decodeScalarBinarySources(programCounter, code, wordIndex, instruction, scalarSource0, scalarSource1);
    return instruction;
}

RdnaInstruction DecodeRdnaSopp(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex) {
    const std::uint32_t word = code[wordIndex];
    const std::uint32_t opcode = (word >> 16u) & 0x7fu;
    const std::uint32_t simm = word & 0xffffu;

    RdnaInstruction instruction;
    instruction.programCounter = programCounter;
    instruction.family = RdnaInstructionFamily::SOPP;
    instruction.opcodeId = opcode;
    instruction.op = decodeSoppOpcode(opcode);
    instruction.source0.kind = RdnaOperandKind::LiteralConstant;
    instruction.source0.value = instruction.op == RdnaOpcode::STrap ? simm & 0xffu : simm;
    instruction.source0.signedVal = instruction.op == RdnaOpcode::STrap
        ? static_cast<std::int32_t>(instruction.source0.value)
        : static_cast<std::int32_t>(static_cast<std::int16_t>(simm));
    instruction.sourceCount = isSoppWaitOpcode(instruction.op) ? 1u : 0u;

    const std::int32_t branchOffset = static_cast<std::int32_t>(static_cast<std::int16_t>(simm)) * 4;
    instruction.branchTarget = programCounter + 4u + static_cast<std::uint32_t>(branchOffset);
    SetRdnaRawWords(instruction, code, wordIndex, 1);
    return instruction;
}

}
