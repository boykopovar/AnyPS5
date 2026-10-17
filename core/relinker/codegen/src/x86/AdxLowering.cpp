#include <codegen/x86/AdxLowering.hpp>
#include <codegen/x86/X64OpcodeConstants.hpp>
#include <codegen/CodegenException.hpp>
#include <algorithm>
#include <array>
#include <limits>

namespace Codegen {

namespace {

constexpr std::uint8_t kRexBase = 0x40;
constexpr std::uint8_t kRexW = 0x08;
constexpr std::uint8_t kRexR = 0x04;
constexpr std::uint8_t kRexX = 0x02;
constexpr std::uint8_t kRexB = 0x01;
constexpr std::int32_t kFrameSize = 144;
constexpr std::uint8_t kStackRegister = 4;

void _append32(std::vector<std::uint8_t>& bytes, const std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        bytes.push_back(static_cast<std::uint8_t>(value >> shift));
}

std::uint8_t _scratch(const AdxOperands& operands) {
    std::array<bool, 16> used{};
    used[kStackRegister] = true;
    used[operands.Destination] = true;
    if (!operands.Memory) {
        used[operands.Source] = true;
    } else {
        const auto& memory = *operands.Memory;
        if (memory.Rm == 4) {
            const auto base = static_cast<std::uint8_t>((memory.Sib & 7) | ((memory.RexIndexBase & kRexB) << 3));
            const auto index = static_cast<std::uint8_t>(((memory.Sib >> 3) & 7) | ((memory.RexIndexBase & kRexX) << 2));
            if (memory.Mod != 0 || (memory.Sib & 7) != 5 || (memory.RexIndexBase & kRexB) != 0)
                used[base] = true;
            if (((memory.Sib >> 3) & 7) != 4 || (memory.RexIndexBase & kRexX) != 0)
                used[index] = true;
        } else if (!memory.RipRelative) {
            used[memory.Rm | ((memory.RexIndexBase & kRexB) << 3)] = true;
        }
    }
    for (const auto reg : {11, 10, 9, 8, 0, 1, 2, 3, 5, 6, 7, 12, 13, 14, 15})
        if (!used[reg])
            return static_cast<std::uint8_t>(reg);
    throw CodegenException("ADCX/ADOX has no scratch register");
}

void _appendRex(std::vector<std::uint8_t>& bytes, const std::uint8_t rex) {
    if (rex != kRexBase)
        bytes.push_back(rex);
}

void _emitRegMemory(std::vector<std::uint8_t>& bytes, const std::uint8_t opcode, const std::uint8_t reg, const std::uint8_t base, const std::int8_t displacement) {
    _appendRex(bytes, static_cast<std::uint8_t>(kRexBase | kRexW | ((reg & 8) != 0 ? kRexR : 0) | ((base & 8) != 0 ? kRexB : 0)));
    bytes.insert(bytes.end(), {opcode, static_cast<std::uint8_t>(0x44 | ((reg & 7) << 3)), 0x24, static_cast<std::uint8_t>(displacement)});
}

void _emitRegStack(std::vector<std::uint8_t>& bytes, const std::uint8_t opcode, const std::uint8_t reg, const std::int8_t displacement) {
    _emitRegMemory(bytes, opcode, reg, kStackRegister, displacement);
}

std::vector<std::uint8_t> _translatedInstruction(const AdxOperands& operands, const std::uint8_t scratch) {
    using namespace X64OpcodeConstants;
    std::vector<std::uint8_t> bytes;
    for (std::size_t index = 0; index < operands.OpcodeOffset; ++index) {
        const auto prefix = operands.Instruction[index];
        if (prefix != (operands.Overflow ? PrefixRep : PrefixOperandSize) && (prefix < RexMin || prefix > RexMax))
            bytes.push_back(operands.Instruction[index]);
    }
    auto rex = operands.Rex;
    if (!operands.Memory && operands.Source == kStackRegister)
        rex = static_cast<std::uint8_t>((rex & (kRexW | kRexR)) | ((scratch & 8) != 0 ? kRexB : 0));
    const auto encodedRex = static_cast<std::uint8_t>(kRexBase | rex);
    _appendRex(bytes, encodedRex);
    bytes.push_back(0x13);
    const auto originalModrm = operands.Instruction[operands.ModRmOffset];
    if (operands.Memory && operands.Memory->StackBase) {
        const auto& memory = *operands.Memory;
        const auto displacement = static_cast<std::int64_t>(memory.Displacement) + kFrameSize;
        const auto address32 = std::find_if(operands.Instruction.begin(), operands.Instruction.begin() + static_cast<std::ptrdiff_t>(operands.OpcodeOffset), [](const auto prefix) { return prefix == PrefixAddressSize; }) != operands.Instruction.begin() + static_cast<std::ptrdiff_t>(operands.OpcodeOffset);
        if (!address32 && (displacement < std::numeric_limits<std::int32_t>::min() || displacement > std::numeric_limits<std::int32_t>::max()))
            throw CodegenException("ADCX/ADOX stack displacement does not fit after spilling");
        bytes.push_back(static_cast<std::uint8_t>((originalModrm & (ModRmRegMask << ModRmRegShift)) | (ModRmModDisp32 << ModRmModShift) | ModRmRmSibPresent));
        bytes.push_back(memory.Sib);
        _append32(bytes, static_cast<std::uint32_t>(displacement));
    } else if (!operands.Memory) {
        const auto source = operands.Source == kStackRegister ? scratch : operands.Source;
        bytes.push_back(static_cast<std::uint8_t>((originalModrm & (ModRmModMask << ModRmModShift | ModRmRegMask << ModRmRegShift)) | (source & ModRmRmMask)));
    } else {
        bytes.insert(bytes.end(), operands.Instruction.begin() + static_cast<std::ptrdiff_t>(operands.ModRmOffset), operands.Instruction.end());
    }
    return bytes;
}

void _emitImmediate(std::vector<std::uint8_t>& bytes, const std::uint8_t opcode, const std::uint8_t extension, const std::uint8_t reg, const std::uint32_t immediate) {
    _appendRex(bytes, static_cast<std::uint8_t>(kRexBase | kRexW | ((reg & 8) != 0 ? kRexB : 0)));
    bytes.insert(bytes.end(), {opcode, static_cast<std::uint8_t>(0xC0 | (extension << 3) | (reg & 7))});
    _append32(bytes, immediate);
}

void _emitStackAnd(std::vector<std::uint8_t>& bytes, const std::uint32_t immediate) {
    bytes.insert(bytes.end(), {0x48, 0x81, 0x64, 0x24, 0x08});
    _append32(bytes, immediate);
}

void _emitStackPushPopFlags(std::vector<std::uint8_t>& bytes) {
    bytes.insert(bytes.end(), {0xFF, 0x74, 0x24, 0x08, 0x9D});
}

}

std::optional<AdxOperands> DecodeAdx(const std::uint8_t* data, const std::size_t length) {
    using namespace X64OpcodeConstants;
    std::size_t opcode = 0;
    std::size_t operandPrefixes = 0;
    std::size_t repPrefixes = 0;
    std::size_t repnePrefixes = 0;
    std::size_t lockPrefixes = 0;
    std::uint8_t rex = 0;
    while (opcode < length) {
        const auto value = data[opcode];
        if (value == PrefixOperandSize) {
            ++operandPrefixes;
            rex = 0;
        } else if (value == PrefixRep) {
            ++repPrefixes;
            rex = 0;
        } else if (value == PrefixRepne) {
            ++repnePrefixes;
            rex = 0;
        } else if (value == PrefixLock) {
            ++lockPrefixes;
            rex = 0;
        } else if (value == PrefixAddressSize || value == PrefixSegCs || value == PrefixSegSs || value == PrefixSegDs || value == PrefixSegEs || value == PrefixSegFs || value == PrefixSegGs) {
            rex = 0;
        } else if (value >= RexMin && value <= RexMax) {
            rex = value;
        } else {
            break;
        }
        ++opcode;
    }
    if (opcode + 2 >= length || data[opcode] != TwoByteOpcodeEscape || data[opcode + 1] != ThreeByteEscape38 || data[opcode + 2] != 0xF6)
        return std::nullopt;
    const bool adcx = operandPrefixes == 1 && repPrefixes == 0 && repnePrefixes == 0;
    const bool adox = repPrefixes == 1 && operandPrefixes == 0 && repnePrefixes == 0;
    if (!adcx && !adox)
        return std::nullopt;
    if (lockPrefixes != 0)
        throw CodegenException("ADCX/ADOX does not accept the LOCK prefix");
    const auto modrmOffset = opcode + 3;
    if (modrmOffset >= length)
        throw CodegenException("ADCX/ADOX truncated before its ModRM byte");
    const auto modrm = data[modrmOffset];
    const auto destination = static_cast<std::uint8_t>(((modrm >> ModRmRegShift) & ModRmRegMask) | ((rex & kRexR) != 0 ? 8 : 0));
    const auto mod = static_cast<std::uint8_t>((modrm >> ModRmModShift) & ModRmModMask);
    AdxOperands operands{adox, (rex & kRexW) != 0, destination,
        static_cast<std::uint8_t>((modrm & ModRmRmMask) | ((rex & kRexB) != 0 ? 8 : 0)), rex, opcode, modrmOffset,
        {data, data + length}, std::nullopt};
    if (mod == ModRmModRegister) {
        if (modrmOffset + 1 != length)
            throw CodegenException("ADCX/ADOX register operand has trailing bytes");
    } else {
        std::vector<std::uint8_t> prefixes(data, data + opcode);
        operands.Memory = DecodeMemoryOperand(data, length, modrmOffset, static_cast<std::uint8_t>(rex & (kRexX | kRexB)), std::move(prefixes));
        const auto address32 = std::find(data, data + opcode, PrefixAddressSize) != data + opcode;
        if (address32 && operands.Memory->RipRelative)
            operands.Memory->RipRelative = false;
        if (modrmOffset + operands.Memory->EncodedSize != length)
            throw CodegenException("ADCX/ADOX memory operand has trailing bytes");
    }
    return operands;
}

void EmitAdx(StubBodyBuilder& body, const AdxOperands& operands) {
    if (operands.Destination == kStackRegister)
        throw CodegenException("ADCX/ADOX with RSP destination cannot be lowered through a trampoline");
    const auto scratch = _scratch(operands);
    const auto flagMask = operands.Overflow ? std::uint32_t{0x800} : std::uint32_t{1};
    const auto clearedMask = ~flagMask;
    std::vector<std::uint8_t> prologue{0x48, 0x8D, 0xA4, 0x24, 0x70, 0xFF, 0xFF, 0xFF};
    _emitRegStack(prologue, 0x89, scratch, 0);
    prologue.push_back(0x9C);
    _emitRegMemory(prologue, 0x8B, scratch, kStackRegister, 0);
    prologue.push_back(0x9D);
    _emitRegStack(prologue, 0x89, scratch, 8);
    body.Raw(prologue);
    if (operands.Source == kStackRegister && !operands.Memory) {
        std::vector<std::uint8_t> originalRsp;
        _appendRex(originalRsp, static_cast<std::uint8_t>(kRexBase | kRexW | ((scratch & 8) != 0 ? kRexR : 0)));
        originalRsp.push_back(0x8D);
        originalRsp.push_back(static_cast<std::uint8_t>(0x84 | ((scratch & 7) << 3)));
        originalRsp.push_back(0x24);
        _append32(originalRsp, kFrameSize);
        body.Raw(originalRsp);
    }
    auto instruction = _translatedInstruction(operands, scratch);
    if (operands.Overflow)
        body.Raw(std::array<std::uint8_t, 6>{0x48, 0x0F, 0xBA, 0x64, 0x24, 0x08});
    if (operands.Overflow)
        body.Raw(std::array<std::uint8_t, 1>{0x0B});
    if (operands.Memory && operands.Memory->RipRelative) {
        const auto displacementOffset = instruction.size() - 4;
        body.RipRelative(instruction, displacementOffset, static_cast<std::int64_t>(operands.Instruction.size()) + operands.Memory->Displacement);
    } else {
        body.Raw(instruction);
    }
    std::vector<std::uint8_t> epilogue{0x9C};
    _emitRegMemory(epilogue, 0x8B, scratch, kStackRegister, 0);
    epilogue.push_back(0x9D);
    _emitStackAnd(epilogue, clearedMask);
    _emitImmediate(epilogue, 0x81, 4, scratch, 1);
    if (operands.Overflow) {
        _appendRex(epilogue, static_cast<std::uint8_t>(kRexBase | kRexW | ((scratch & 8) != 0 ? kRexB : 0)));
        epilogue.insert(epilogue.end(), {0xC1, static_cast<std::uint8_t>(0xE0 | (scratch & 7)), 11});
    }
    _emitRegMemory(epilogue, 0x09, scratch, kStackRegister, 8);
    _emitStackPushPopFlags(epilogue);
    _emitRegMemory(epilogue, 0x8B, scratch, kStackRegister, 0);
    epilogue.insert(epilogue.end(), {0x48, 0x8D, 0xA4, 0x24, 0x90, 0x00, 0x00, 0x00});
    body.Raw(epilogue);
}

}
