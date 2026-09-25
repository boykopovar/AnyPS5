#include <cstdio>
#include "RdnaDecoder/RdnaMemoryOpDecoder.hpp"
#include <bit>
#include <limits>
#include <stdexcept>

namespace ShaderRecompiler {

namespace {

struct MemoryOpcodeInfo {
    std::uint32_t encoding;
    RdnaOpcode opcode;
    std::uint32_t dataDwords;
    std::uint32_t dataBits;
    bool dataSigned;
    bool typed;
    bool formatted;
};

constexpr MemoryOpcodeInfo smemOpcodes[] = {
    {0x00u, RdnaOpcode::SLoadDword, 1, 32, false, false, false},
    {0x01u, RdnaOpcode::SLoadDwordx2, 2, 32, false, false, false},
    {0x02u, RdnaOpcode::SLoadDwordx4, 4, 32, false, false, false},
    {0x03u, RdnaOpcode::SLoadDwordx8, 8, 32, false, false, false},
    {0x04u, RdnaOpcode::SLoadDwordx16, 16, 32, false, false, false},
    {0x08u, RdnaOpcode::SBufferLoadDword, 1, 32, false, false, false},
    {0x09u, RdnaOpcode::SBufferLoadDwordx2, 2, 32, false, false, false},
    {0x0au, RdnaOpcode::SBufferLoadDwordx4, 4, 32, false, false, false},
    {0x0bu, RdnaOpcode::SBufferLoadDwordx8, 8, 32, false, false, false},
    {0x0cu, RdnaOpcode::SBufferLoadDwordx16, 16, 32, false, false, false},
};

constexpr MemoryOpcodeInfo mubufOpcodes[] = {
    {0x00u, RdnaOpcode::BufferLoadFormatX, 1, 32, false, false, true},
    {0x01u, RdnaOpcode::BufferLoadFormatXy, 2, 32, false, false, true},
    {0x02u, RdnaOpcode::BufferLoadFormatXyz, 3, 32, false, false, true},
    {0x03u, RdnaOpcode::BufferLoadFormatXyzw, 4, 32, false, false, true},
    {0x04u, RdnaOpcode::BufferStoreFormatX, 1, 32, false, false, true},
    {0x05u, RdnaOpcode::BufferStoreFormatXy, 2, 32, false, false, true},
    {0x06u, RdnaOpcode::BufferStoreFormatXyz, 3, 32, false, false, true},
    {0x07u, RdnaOpcode::BufferStoreFormatXyzw, 4, 32, false, false, true},
    {0x08u, RdnaOpcode::BufferLoadUbyte, 1, 8, false, false, false},
    {0x09u, RdnaOpcode::BufferLoadSbyte, 1, 8, true, false, false},
    {0x0au, RdnaOpcode::BufferLoadUshort, 1, 16, false, false, false},
    {0x0bu, RdnaOpcode::BufferLoadSshort, 1, 16, true, false, false},
    {0x0cu, RdnaOpcode::BufferLoadDword, 1, 32, false, false, false},
    {0x0du, RdnaOpcode::BufferLoadDwordx2, 2, 32, false, false, false},
    {0x0eu, RdnaOpcode::BufferLoadDwordx4, 4, 32, false, false, false},
    {0x0fu, RdnaOpcode::BufferLoadDwordx3, 3, 32, false, false, false},
    {0x18u, RdnaOpcode::BufferStoreByte, 1, 8, false, false, false},
    {0x1au, RdnaOpcode::BufferStoreShort, 1, 16, false, false, false},
    {0x1cu, RdnaOpcode::BufferStoreDword, 1, 32, false, false, false},
    {0x1du, RdnaOpcode::BufferStoreDwordx2, 2, 32, false, false, false},
    {0x1eu, RdnaOpcode::BufferStoreDwordx4, 4, 32, false, false, false},
    {0x1fu, RdnaOpcode::BufferStoreDwordx3, 3, 32, false, false, false},
    {0x30u, RdnaOpcode::BufferAtomicSwap, 1, 32, false, false, false},
    {0x31u, RdnaOpcode::BufferAtomicCmpswap, 1, 32, false, false, false},
    {0x32u, RdnaOpcode::BufferAtomicAdd, 1, 32, false, false, false},
    {0x33u, RdnaOpcode::BufferAtomicSub, 1, 32, false, false, false},
    {0x35u, RdnaOpcode::BufferAtomicSmin, 1, 32, false, false, false},
    {0x36u, RdnaOpcode::BufferAtomicUmin, 1, 32, false, false, false},
    {0x37u, RdnaOpcode::BufferAtomicSmax, 1, 32, false, false, false},
    {0x38u, RdnaOpcode::BufferAtomicUmax, 1, 32, false, false, false},
    {0x39u, RdnaOpcode::BufferAtomicAnd, 1, 32, false, false, false},
    {0x3au, RdnaOpcode::BufferAtomicOr, 1, 32, false, false, false},
    {0x3bu, RdnaOpcode::BufferAtomicXor, 1, 32, false, false, false},
    {0x3fu, RdnaOpcode::BufferAtomicFmin, 1, 32, false, false, false},
    {0x40u, RdnaOpcode::BufferAtomicFmax, 1, 32, false, false, false},
    {0x50u, RdnaOpcode::BufferAtomicSwapX2, 2, 32, false, false, false},
    {0x5au, RdnaOpcode::BufferAtomicOrX2, 2, 32, false, false, false},
};

constexpr MemoryOpcodeInfo mtbufOpcodes[] = {
    {0x00u, RdnaOpcode::TbufferLoadFormatX, 1, 32, false, true, true},
    {0x01u, RdnaOpcode::TbufferLoadFormatXy, 2, 32, false, true, true},
    {0x02u, RdnaOpcode::TbufferLoadFormatXyz, 3, 32, false, true, true},
    {0x03u, RdnaOpcode::TbufferLoadFormatXyzw, 4, 32, false, true, true},
    {0x04u, RdnaOpcode::TbufferStoreFormatX, 1, 32, false, true, true},
    {0x05u, RdnaOpcode::TbufferStoreFormatXy, 2, 32, false, true, true},
    {0x06u, RdnaOpcode::TbufferStoreFormatXyz, 3, 32, false, true, true},
    {0x07u, RdnaOpcode::TbufferStoreFormatXyzw, 4, 32, false, true, true},
};

constexpr MemoryOpcodeInfo flatOpcodes[] = {
    {0x08u, RdnaOpcode::FlatLoadUbyte, 1, 8, false, false, false},
    {0x09u, RdnaOpcode::FlatLoadSbyte, 1, 8, true, false, false},
    {0x0au, RdnaOpcode::FlatLoadUshort, 1, 16, false, false, false},
    {0x0bu, RdnaOpcode::FlatLoadSshort, 1, 16, true, false, false},
    {0x0cu, RdnaOpcode::FlatLoadDword, 1, 32, false, false, false},
    {0x0du, RdnaOpcode::FlatLoadDwordx2, 2, 32, false, false, false},
    {0x0eu, RdnaOpcode::FlatLoadDwordx4, 4, 32, false, false, false},
    {0x0fu, RdnaOpcode::FlatLoadDwordx3, 3, 32, false, false, false},
    {0x18u, RdnaOpcode::FlatStoreByte, 1, 8, false, false, false},
    {0x1au, RdnaOpcode::FlatStoreShort, 1, 16, false, false, false},
    {0x1cu, RdnaOpcode::FlatStoreDword, 1, 32, false, false, false},
    {0x1du, RdnaOpcode::FlatStoreDwordx2, 2, 32, false, false, false},
    {0x1eu, RdnaOpcode::FlatStoreDwordx4, 4, 32, false, false, false},
    {0x1fu, RdnaOpcode::FlatStoreDwordx3, 3, 32, false, false, false},
};

constexpr MemoryOpcodeInfo dsOpcodes[] = {
    {0x00u, RdnaOpcode::DsAddU32, 1, 32, false, false, false},
    {0x01u, RdnaOpcode::DsSubU32, 1, 32, false, false, false},
    {0x05u, RdnaOpcode::DsMinI32, 1, 32, false, false, false},
    {0x06u, RdnaOpcode::DsMaxI32, 1, 32, false, false, false},
    {0x07u, RdnaOpcode::DsMinU32, 1, 32, false, false, false},
    {0x08u, RdnaOpcode::DsMaxU32, 1, 32, false, false, false},
    {0x09u, RdnaOpcode::DsAndB32, 1, 32, false, false, false},
    {0x0au, RdnaOpcode::DsOrB32, 1, 32, false, false, false},
    {0x0bu, RdnaOpcode::DsXorB32, 1, 32, false, false, false},
    {0x0du, RdnaOpcode::DsWriteB32, 1, 32, false, false, false},
    {0x0eu, RdnaOpcode::DsWrite2B32, 2, 32, false, false, false},
    {0x0fu, RdnaOpcode::DsWrite2st64B32, 2, 32, false, false, false},
    {0x12u, RdnaOpcode::DsMinF32, 1, 32, false, false, false},
    {0x13u, RdnaOpcode::DsMaxF32, 1, 32, false, false, false},
    {0x1eu, RdnaOpcode::DsWriteB8, 1, 8, false, false, false},
    {0x1fu, RdnaOpcode::DsWriteB16, 1, 16, false, false, false},
    {0x20u, RdnaOpcode::DsAddRtnU32, 1, 32, false, false, false},
    {0x21u, RdnaOpcode::DsSubRtnU32, 1, 32, false, false, false},
    {0x23u, RdnaOpcode::DsIncRtnU32, 1, 32, false, false, false},
    {0x24u, RdnaOpcode::DsDecRtnU32, 1, 32, false, false, false},
    {0x25u, RdnaOpcode::DsMinRtnI32, 1, 32, false, false, false},
    {0x26u, RdnaOpcode::DsMaxRtnI32, 1, 32, false, false, false},
    {0x27u, RdnaOpcode::DsMinRtnU32, 1, 32, false, false, false},
    {0x28u, RdnaOpcode::DsMaxRtnU32, 1, 32, false, false, false},
    {0x29u, RdnaOpcode::DsAndRtnB32, 1, 32, false, false, false},
    {0x2au, RdnaOpcode::DsOrRtnB32, 1, 32, false, false, false},
    {0x2bu, RdnaOpcode::DsXorRtnB32, 1, 32, false, false, false},
    {0x2du, RdnaOpcode::DsWrxchgRtnB32, 1, 32, false, false, false},
    {0x35u, RdnaOpcode::DsSwizzleB32, 1, 32, false, false, false},
    {0x36u, RdnaOpcode::DsReadB32, 1, 32, false, false, false},
    {0x37u, RdnaOpcode::DsRead2B32, 2, 32, false, false, false},
    {0x38u, RdnaOpcode::DsRead2st64B32, 2, 32, false, false, false},
    {0x39u, RdnaOpcode::DsReadI8, 1, 8, true, false, false},
    {0x3au, RdnaOpcode::DsReadU8, 1, 8, false, false, false},
    {0x3bu, RdnaOpcode::DsReadI16, 1, 16, true, false, false},
    {0x3cu, RdnaOpcode::DsReadU16, 1, 16, false, false, false},
    {0x3du, RdnaOpcode::DsConsume, 1, 32, false, false, false},
    {0x3eu, RdnaOpcode::DsAppend, 1, 32, false, false, false},
    {0x4du, RdnaOpcode::DsWriteB64, 2, 32, false, false, false},
    {0x4eu, RdnaOpcode::DsWrite2B64, 4, 32, false, false, false},
    {0x4fu, RdnaOpcode::DsWrite2st64B64, 4, 32, false, false, false},
    {0x76u, RdnaOpcode::DsReadB64, 2, 32, false, false, false},
    {0x77u, RdnaOpcode::DsRead2B64, 4, 32, false, false, false},
    {0x78u, RdnaOpcode::DsRead2st64B64, 4, 32, false, false, false},
    {0xa1u, RdnaOpcode::DsWriteB16D16Hi, 1, 16, false, false, false},
    {0xa6u, RdnaOpcode::DsReadU16D16, 1, 16, false, false, false},
    {0xa7u, RdnaOpcode::DsReadU16D16Hi, 1, 16, false, false, false},
    {0xb0u, RdnaOpcode::DsWriteAddtidB32, 1, 32, false, false, false},
    {0xb1u, RdnaOpcode::DsReadAddtidB32, 1, 32, false, false, false},
    {0xb3u, RdnaOpcode::DsBpermuteB32, 1, 32, false, false, false},
    {0xdeu, RdnaOpcode::DsWriteB96, 3, 32, false, false, false},
    {0xdfu, RdnaOpcode::DsWriteB128, 4, 32, false, false, false},
    {0xfeu, RdnaOpcode::DsReadB96, 3, 32, false, false, false},
    {0xffu, RdnaOpcode::DsReadB128, 4, 32, false, false, false},
};

template <std::size_t Size>
const MemoryOpcodeInfo& lookupOpcode(const MemoryOpcodeInfo (&table)[Size], std::uint32_t encoding, const char* notSupportedReason) {
    for (const auto& entry : table) {
        if (entry.encoding == encoding) {
            return entry;
        }
    }
    throw std::runtime_error(notSupportedReason);
}

std::uint32_t signExtend(std::uint32_t value, std::uint32_t bits) {
    if (bits == 0u || bits >= 32u) {
        return value;
    }
    const std::uint32_t sign = 1u << (bits - 1u);
    return (value ^ sign) - sign;
}

std::uint32_t floatBits(float value) {
    return std::bit_cast<std::uint32_t>(value);
}

void applyMemoryInfo(RdnaInstruction& instruction, const MemoryOpcodeInfo& info) {
    instruction.op = info.opcode;
    instruction.dataDwordCount = info.dataDwords;
    instruction.dataBits = info.dataBits;
    instruction.dataSigned = info.dataSigned;
    instruction.typed = info.typed;
    instruction.formatted = info.formatted;
}

RdnaOperand vectorRegister(std::uint32_t reg) {
    RdnaOperand operand{};
    operand.kind = RdnaOperandKind::VectorRegister;
    operand.reg = reg;
    return operand;
}

RdnaOperand scalarSource(std::uint32_t code) {
    RdnaOperand operand{};
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
        constexpr float values[] = {0.5f, -0.5f, 1.0f, -1.0f, 2.0f, -2.0f, 4.0f, -4.0f};
        operand.kind = RdnaOperandKind::FloatInlineConstant;
        operand.value = floatBits(values[code - 240u]);
        return operand;
    }
    switch (code) {
        case 106u: operand.kind = RdnaOperandKind::VccLo; return operand;
        case 107u: operand.kind = RdnaOperandKind::VccHi; return operand;
        case 124u: operand.kind = RdnaOperandKind::M0; return operand;
        case 125u: operand.kind = RdnaOperandKind::Null; return operand;
        case 126u: operand.kind = RdnaOperandKind::ExecLo; return operand;
        case 127u: operand.kind = RdnaOperandKind::ExecHi; return operand;
        case 239u: operand.kind = RdnaOperandKind::PopsExitingWaveId; return operand;
        case 248u: operand.kind = RdnaOperandKind::FloatInlineConstant; operand.value = floatBits(0.15915494309189535f); return operand;
        case 251u: operand.kind = RdnaOperandKind::VccZ; return operand;
        case 252u: operand.kind = RdnaOperandKind::ExecZ; return operand;
        case 253u: operand.kind = RdnaOperandKind::Scc; return operand;
        default: throw std::runtime_error("unsupported scalar source operand");
    }
}

RdnaOperand scalarDestination(std::uint32_t code) {
    RdnaOperand operand{};
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
        default: throw std::runtime_error("unsupported scalar destination operand");
    }
}

RdnaOperand scalarDescriptorBase(std::uint32_t reg, std::uint32_t registerCount, const char* reason) {
    const auto operand = scalarSource(reg);
    if (operand.kind != RdnaOperandKind::ScalarRegister || reg + (registerCount - 1u) > 105u) {
        throw std::runtime_error(reason);
    }
    return operand;
}

bool isDsWriteOpcode(RdnaOpcode opcode) {
    switch (opcode) {
        case RdnaOpcode::DsWriteB8:
        case RdnaOpcode::DsWriteB16:
        case RdnaOpcode::DsWriteB16D16Hi:
        case RdnaOpcode::DsWrite2B32:
        case RdnaOpcode::DsWrite2st64B32:
        case RdnaOpcode::DsWrite2B64:
        case RdnaOpcode::DsWrite2st64B64:
        case RdnaOpcode::DsWriteB32:
        case RdnaOpcode::DsWriteB64:
        case RdnaOpcode::DsWriteB96:
        case RdnaOpcode::DsWriteB128: return true;
        default: return false;
    }
}

bool isDsAtomicOpcode(RdnaOpcode opcode) {
    switch (opcode) {
        case RdnaOpcode::DsAddU32:
        case RdnaOpcode::DsAddRtnU32:
        case RdnaOpcode::DsSubU32:
        case RdnaOpcode::DsSubRtnU32:
        case RdnaOpcode::DsIncRtnU32:
        case RdnaOpcode::DsDecRtnU32:
        case RdnaOpcode::DsMinI32:
        case RdnaOpcode::DsMinRtnI32:
        case RdnaOpcode::DsMaxI32:
        case RdnaOpcode::DsMaxRtnI32:
        case RdnaOpcode::DsMinU32:
        case RdnaOpcode::DsMinRtnU32:
        case RdnaOpcode::DsMaxU32:
        case RdnaOpcode::DsMaxRtnU32:
        case RdnaOpcode::DsAndB32:
        case RdnaOpcode::DsAndRtnB32:
        case RdnaOpcode::DsOrB32:
        case RdnaOpcode::DsOrRtnB32:
        case RdnaOpcode::DsXorB32:
        case RdnaOpcode::DsXorRtnB32:
        case RdnaOpcode::DsWrxchgRtnB32: return true;
        default: return false;
    }
}

std::uint32_t dsSourceCount(RdnaOpcode opcode) {
    switch (opcode) {
        case RdnaOpcode::DsWrite2B32:
        case RdnaOpcode::DsWrite2st64B32:
        case RdnaOpcode::DsWrite2B64:
        case RdnaOpcode::DsWrite2st64B64:
        case RdnaOpcode::DsMinF32:
        case RdnaOpcode::DsMaxF32: return 3u;
        case RdnaOpcode::DsBpermuteB32: return 2u;
        case RdnaOpcode::DsReadAddtidB32:
        case RdnaOpcode::DsConsume:
        case RdnaOpcode::DsAppend: return 0u;
        default: return isDsWriteOpcode(opcode) || isDsAtomicOpcode(opcode) ? 2u : 1u;
    }
}

bool isFlatStoreOpcode(RdnaOpcode opcode) {
    switch (opcode) {
        case RdnaOpcode::FlatStoreByte:
        case RdnaOpcode::FlatStoreShort:
        case RdnaOpcode::FlatStoreDword:
        case RdnaOpcode::FlatStoreDwordx2:
        case RdnaOpcode::FlatStoreDwordx3:
        case RdnaOpcode::FlatStoreDwordx4: return true;
        default: return false;
    }
}

void setRawWords(RdnaInstruction& instruction, std::span<const std::uint32_t> code, std::uint32_t wordIndex, std::uint32_t wordCount) {
    instruction.wordCount = wordCount;
    for (std::uint32_t i = 0; i < wordCount; ++i) {
        instruction.rawWords[i] = code[wordIndex + i];
    }
}

void requireTwoWords(std::span<const std::uint32_t> code, std::uint32_t wordIndex, std::uint32_t programCounter, const char* reason) {
    const std::size_t index = wordIndex;
    if (index >= code.size() || code.size() - index < 2u) {
        throw std::runtime_error(reason);
    }
    if (programCounter % 4u != 0u || programCounter > std::numeric_limits<std::uint32_t>::max() - 7u) {
        throw std::runtime_error("invalid memory instruction program counter");
    }
}

std::uint32_t toProgramCounter(std::uint32_t wordIndex) {
    if (wordIndex > std::numeric_limits<std::uint32_t>::max() / 4u) {
        throw std::runtime_error("memory instruction program counter overflow");
    }
    return wordIndex * 4u;
}

}

RdnaInstruction DecodeRdnaSmem(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex) {
    requireTwoWords(code, wordIndex, programCounter, "truncated SMEM instruction");
    const std::size_t index = wordIndex;
    const auto word0 = code[index];
    const auto word1 = code[index + 1u];
    if ((word0 >> 26u) != 0x3Du) {
        throw std::runtime_error("instruction is not SMEM");
    }
    const auto opcode = (word0 >> 18u) & 0xFFu;
    const auto sdst = (word0 >> 6u) & 0x7Fu;
    const auto sbase = word0 & 0x3Fu;
    const auto soffsetCode = (word1 >> 25u) & 0x7Fu;
    const auto& info = lookupOpcode(smemOpcodes, opcode, "SMEM opcode is not supported");

    RdnaInstruction instruction{};
    instruction.programCounter = programCounter;
    instruction.family = RdnaInstructionFamily::SMEM;
    instruction.opcodeId = opcode;
    instruction.glc = ((word0 >> 16u) & 1u) != 0u;
    instruction.memoryOffset = signExtend(word1 & 0x1FFFFFu, 21u);
    applyMemoryInfo(instruction, info);
    setRawWords(instruction, code, wordIndex, 2u);

    instruction.destination = scalarDestination(sdst);
    // The base pair may also be VCC (s106:107), which compilers use as a scratch address register.
    if (sbase * 2u == 106u) {
        RdnaOperand vcc{};
        vcc.kind = RdnaOperandKind::VccLo;
        instruction.source0 = vcc;
    } else if (sbase * 2u > 104u) {
        char reason[96];
        std::snprintf(reason, sizeof(reason), "SMEM base register range overflow (sbase s%u at pc 0x%x, words %08x %08x)", sbase * 2u, programCounter, word0, word1);
        throw std::runtime_error(reason);
    } else {
        instruction.source0 = scalarDescriptorBase(sbase * 2u, 2u, "SMEM base register range overflow");
    }
    instruction.source1 = scalarSource(soffsetCode);
    instruction.sourceCount = 2;
    return instruction;
}

RdnaInstruction DecodeRdnaMubuf(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex) {
    requireTwoWords(code, wordIndex, programCounter, "truncated MUBUF instruction");
    const std::size_t index = wordIndex;
    const auto word0 = code[index];
    const auto word1 = code[index + 1u];
    if ((word0 >> 26u) != 0x38u) {
        throw std::runtime_error("instruction is not MUBUF");
    }
    const auto opcode = ((word0 >> 18u) & 0x7Fu) | (((word0 >> 25u) & 1u) << 7u);
    const auto vdata = (word1 >> 8u) & 0xFFu;
    const auto vaddr = word1 & 0xFFu;
    const auto srsrc = (word1 >> 16u) & 0x1Fu;
    const auto soffsetCode = (word1 >> 24u) & 0xFFu;
    const auto& info = lookupOpcode(mubufOpcodes, opcode, "MUBUF opcode is not supported");

    RdnaInstruction instruction{};
    instruction.programCounter = programCounter;
    instruction.family = RdnaInstructionFamily::MUBUF;
    instruction.opcodeId = opcode;
    instruction.memoryOffset = word0 & 0xFFFu;
    instruction.offen = ((word0 >> 12u) & 1u) != 0u;
    instruction.idxen = ((word0 >> 13u) & 1u) != 0u;
    instruction.glc = ((word0 >> 14u) & 1u) != 0u;
    instruction.slc = ((word1 >> 22u) & 1u) != 0u;
    applyMemoryInfo(instruction, info);
    setRawWords(instruction, code, wordIndex, 2u);

    instruction.destination = vectorRegister(vdata);
    instruction.source0 = vectorRegister(vaddr);
    instruction.source1 = scalarDescriptorBase(srsrc * 4u, 4u, "MUBUF resource descriptor register range overflow");
    instruction.source2 = scalarSource(soffsetCode);
    instruction.sourceCount = 3;
    return instruction;
}

RdnaInstruction DecodeRdnaMtbuf(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex) {
    requireTwoWords(code, wordIndex, programCounter, "truncated MTBUF instruction");
    const std::size_t index = wordIndex;
    const auto word0 = code[index];
    const auto word1 = code[index + 1u];
    if ((word0 >> 26u) != 0x3Au) {
        throw std::runtime_error("instruction is not MTBUF");
    }
    const auto opcode = ((word0 >> 16u) & 0x7u) | (((word1 >> 21u) & 1u) << 3u);
    const auto dfmt = (word0 >> 19u) & 0xFu;
    const auto nfmt = (word0 >> 23u) & 0x7u;
    const auto vdata = (word1 >> 8u) & 0xFFu;
    const auto vaddr = word1 & 0xFFu;
    const auto srsrc = (word1 >> 16u) & 0x1Fu;
    const auto soffsetCode = (word1 >> 24u) & 0xFFu;
    const auto& info = lookupOpcode(mtbufOpcodes, opcode, "MTBUF opcode is not supported");

    RdnaInstruction instruction{};
    instruction.programCounter = programCounter;
    instruction.family = RdnaInstructionFamily::MTBUF;
    instruction.opcodeId = opcode;
    instruction.dataFormat = dfmt;
    instruction.numberFormat = nfmt;
    instruction.memoryOffset = word0 & 0xFFFu;
    instruction.offen = ((word0 >> 12u) & 1u) != 0u;
    instruction.idxen = ((word0 >> 13u) & 1u) != 0u;
    instruction.glc = ((word0 >> 14u) & 1u) != 0u;
    instruction.slc = ((word1 >> 22u) & 1u) != 0u;
    applyMemoryInfo(instruction, info);
    setRawWords(instruction, code, wordIndex, 2u);

    instruction.destination = vectorRegister(vdata);
    instruction.source0 = vectorRegister(vaddr);
    instruction.source1 = scalarDescriptorBase(srsrc * 4u, 4u, "MTBUF resource descriptor register range overflow");
    instruction.source2 = scalarSource(soffsetCode);
    instruction.sourceCount = 3;
    return instruction;
}

RdnaInstruction DecodeRdnaFlat(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex) {
    requireTwoWords(code, wordIndex, programCounter, "truncated FLAT instruction");
    const std::size_t index = wordIndex;
    const auto word0 = code[index];
    const auto word1 = code[index + 1u];
    if ((word0 >> 26u) != 0x37u) {
        throw std::runtime_error("instruction is not FLAT");
    }
    const auto rawOffset = word0 & 0xFFFu;
    const auto dlc = (word0 >> 12u) & 1u;
    const auto lds = (word0 >> 13u) & 1u;
    const auto seg = (word0 >> 14u) & 0x3u;
    const auto glc = ((word0 >> 16u) & 1u) != 0u;
    const auto slc = ((word0 >> 17u) & 1u) != 0u;
    const auto opcode = (word0 >> 18u) & 0x7Fu;
    const auto vdst = (word1 >> 24u) & 0xFFu;
    const auto saddr = (word1 >> 16u) & 0x7Fu;
    const auto data = (word1 >> 8u) & 0xFFu;
    const auto addr = word1 & 0xFFu;
    if (dlc != 0u || lds != 0u || glc || slc || seg == 3u) {
        throw std::runtime_error("unsupported FLAT modifiers or segment");
    }
    const auto& info = lookupOpcode(flatOpcodes, opcode, "FLAT opcode is not supported");

    RdnaInstruction instruction{};
    instruction.programCounter = programCounter;
    instruction.family = RdnaInstructionFamily::FLAT;
    instruction.opcodeId = opcode;
    instruction.memorySegment = seg;
    instruction.memoryOffset = seg == 0u ? (rawOffset & 0x7FFu) : signExtend(rawOffset, 12u);
    instruction.glc = glc;
    instruction.slc = slc;
    applyMemoryInfo(instruction, info);
    setRawWords(instruction, code, wordIndex, 2u);

    instruction.destination = vectorRegister(isFlatStoreOpcode(instruction.op) ? data : vdst);
    instruction.source0 = vectorRegister(addr);
    if (seg == 0u || saddr == 0x7Du || saddr == 0x7Fu) {
        if (addr == 255u) {
            throw std::runtime_error("FLAT address register range overflow");
        }
        instruction.source1 = vectorRegister(addr + 1u);
    } else {
        instruction.source1 = scalarDescriptorBase(saddr, 2u, "FLAT scalar address register range overflow");
    }
    instruction.sourceCount = 2;
    return instruction;
}

RdnaInstruction DecodeRdnaDs(std::uint32_t programCounter, std::span<const std::uint32_t> code, std::uint32_t wordIndex) {
    requireTwoWords(code, wordIndex, programCounter, "truncated DS instruction");
    const std::size_t index = wordIndex;
    const auto word0 = code[index];
    const auto word1 = code[index + 1u];
    if ((word0 >> 26u) != 0x36u) {
        throw std::runtime_error("instruction is not DS");
    }
    const auto opcode = (word0 >> 18u) & 0xFFu;
    const auto offset0 = word0 & 0xFFu;
    const auto offset1 = (word0 >> 8u) & 0xFFu;
    const auto gds = ((word0 >> 17u) & 1u) != 0u;
    const auto vdst = (word1 >> 24u) & 0xFFu;
    const auto data1 = (word1 >> 16u) & 0xFFu;
    const auto data0 = (word1 >> 8u) & 0xFFu;
    const auto addr = word1 & 0xFFu;
    const auto& info = lookupOpcode(dsOpcodes, opcode, "DS opcode is not supported");
    const auto combinedOffset = offset0 | (offset1 << 8u);
    if (info.opcode == RdnaOpcode::DsSwizzleB32 && combinedOffset >= 0xE000u) {
        throw std::runtime_error("DS swizzle FFT mode is not supported");
    }
    if (gds && (info.opcode == RdnaOpcode::DsSwizzleB32 || info.opcode == RdnaOpcode::DsBpermuteB32 || info.opcode == RdnaOpcode::DsWriteAddtidB32 || info.opcode == RdnaOpcode::DsReadAddtidB32)) {
        throw std::runtime_error("DS lane operation is available only for LDS");
    }
    if (info.opcode == RdnaOpcode::DsWriteAddtidB32 && data1 != 0u) {
        throw std::runtime_error("DS write addtid data1 operand is not supported");
    }
    if (info.opcode == RdnaOpcode::DsReadAddtidB32 && (data0 != 0u || data1 != 0u)) {
        throw std::runtime_error("DS read addtid data operands are not supported");
    }

    RdnaInstruction instruction{};
    instruction.programCounter = programCounter;
    instruction.family = RdnaInstructionFamily::DS;
    instruction.opcodeId = opcode;
    instruction.gds = gds;
    instruction.memoryOffset = combinedOffset;
    applyMemoryInfo(instruction, info);
    setRawWords(instruction, code, wordIndex, 2u);

    if (instruction.op == RdnaOpcode::DsWrite2B32 || instruction.op == RdnaOpcode::DsRead2B32) {
        instruction.memoryOffset = offset0 * 4u;
        instruction.secondaryOffset = offset1 * 4u;
    } else if (instruction.op == RdnaOpcode::DsWrite2st64B32 || instruction.op == RdnaOpcode::DsRead2st64B32) {
        instruction.memoryOffset = offset0 * 256u;
        instruction.secondaryOffset = offset1 * 256u;
    } else if (instruction.op == RdnaOpcode::DsWrite2B64 || instruction.op == RdnaOpcode::DsRead2B64) {
        instruction.memoryOffset = offset0 * 8u;
        instruction.secondaryOffset = offset1 * 8u;
    } else if (instruction.op == RdnaOpcode::DsWrite2st64B64 || instruction.op == RdnaOpcode::DsRead2st64B64) {
        instruction.memoryOffset = offset0 * 512u;
        instruction.secondaryOffset = offset1 * 512u;
    }

    instruction.destination = vectorRegister(vdst);
    if (instruction.op == RdnaOpcode::DsReadU16D16) {
        instruction.destination.sdwaSel = 4u;
    } else if (instruction.op == RdnaOpcode::DsReadU16D16Hi) {
        instruction.destination.sdwaSel = 5u;
    }
    instruction.source0 = vectorRegister(addr);
    instruction.source1 = vectorRegister(data0);
    if (instruction.op == RdnaOpcode::DsWriteB16D16Hi) {
        instruction.source1.sdwaSel = 5u;
    }
    instruction.source2 = vectorRegister(data1);
    instruction.sourceCount = dsSourceCount(instruction.op);
    return instruction;
}

RdnaInstruction DecodeRdnaMemoryOp(std::span<const std::uint32_t> code, std::uint32_t wordIndex) {
    const auto programCounter = toProgramCounter(wordIndex);
    if (static_cast<std::size_t>(wordIndex) >= code.size()) {
        throw std::runtime_error("truncated memory instruction");
    }
    switch (code[wordIndex] >> 26u) {
        case 0x36u: return DecodeRdnaDs(programCounter, code, wordIndex);
        case 0x37u: return DecodeRdnaFlat(programCounter, code, wordIndex);
        case 0x38u: return DecodeRdnaMubuf(programCounter, code, wordIndex);
        case 0x3Au: return DecodeRdnaMtbuf(programCounter, code, wordIndex);
        case 0x3Du: return DecodeRdnaSmem(programCounter, code, wordIndex);
        default: throw std::runtime_error("instruction is not a memory operation");
    }
}

}
