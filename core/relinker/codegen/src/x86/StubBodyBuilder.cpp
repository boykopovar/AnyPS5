#include <codegen/x86/StubBodyBuilder.hpp>
#include <codegen/x86/Amd64OnlySubstitutionTable.hpp>
#include <codegen/x86/X64OpcodeConstants.hpp>
#include <codegen/CodegenException.hpp>
#include <limits>

namespace Codegen {

namespace {

using namespace Amd64OnlySubstitutionTable;

constexpr std::uint8_t kPrefixPacked = 0x66;
constexpr std::uint8_t kPrefixScalar = 0xF3;
constexpr std::uint8_t kRexBase = 0x40;
constexpr std::uint8_t kRexR = 0x04;
constexpr std::uint8_t kRexX = 0x02;
constexpr std::uint8_t kRexB = 0x01;
constexpr std::uint8_t kModRmRegister = 0xC0;
constexpr std::uint8_t kModRmRip = 0x05;
constexpr std::uint8_t kModRmRspBase = 0x04;
constexpr std::uint8_t kSibRsp = 0x24;
constexpr std::uint8_t kShiftDwords = 0x72;
constexpr std::uint8_t kShiftQwords = 0x73;
constexpr std::uint8_t kMovdqu = 0x6F;

std::uint8_t _rex(const std::uint8_t reg, const std::uint8_t rm) {
    return static_cast<std::uint8_t>(kRexBase | ((reg & 8) != 0 ? kRexR : 0) | ((rm & 8) != 0 ? kRexB : 0));
}

void _emit(std::vector<std::uint8_t>& out, const std::uint8_t prefix, const std::uint8_t reg, const std::uint8_t rm, const std::initializer_list<std::uint8_t> opcode, const std::uint8_t modrm) {
    out.push_back(prefix);
    const auto rex = _rex(reg, rm);
    if (rex != kRexBase)
        out.push_back(rex);
    out.insert(out.end(), opcode.begin(), opcode.end());
    out.push_back(modrm);
}

std::size_t _displacementSize(const std::uint8_t mod, const std::uint8_t rm, const std::uint8_t sib) {
    using namespace X64OpcodeConstants;
    if (mod == ModRmModDisp8)
        return Disp8Size;
    if (mod == ModRmModDisp32 || (mod == ModRmModIndirect && rm == ModRmRmSibPresent && (sib & SibBaseMask) == SibBaseDisp32))
        return Disp32Size;
    return 0;
}

void _shiftImm(std::vector<std::uint8_t>& out, const std::uint8_t opcode, const std::uint8_t extension, const std::uint8_t reg, const std::uint8_t imm) {
    _emit(out, kPrefixPacked, 0, reg, {0x0F, opcode}, static_cast<std::uint8_t>(kModRmRegister | (extension << 3) | (reg & 7)));
    out.push_back(imm);
}

}

MemoryOperand DecodeMemoryOperand(const std::uint8_t* data, const std::size_t length, const std::size_t modRmOffset, const std::uint8_t rex, std::vector<std::uint8_t> prefixes) {
    using namespace X64OpcodeConstants;
    if (modRmOffset >= length)
        throw CodegenException("Memory operand truncated before its ModRM byte");
    const auto modrm = data[modRmOffset];
    MemoryOperand operand{std::move(prefixes), static_cast<std::uint8_t>(rex & (kRexX | kRexB)), static_cast<std::uint8_t>((modrm >> ModRmModShift) & ModRmModMask), static_cast<std::uint8_t>(modrm & ModRmRmMask), 0, 0, false, 0};
    if (operand.Mod == ModRmModRegister)
        throw CodegenException("Register operand where a memory operand was expected");
    if (operand.Mod == ModRmModIndirect && operand.Rm == ModRmRmRipRelative)
        throw CodegenException("RIP-relative memory operand cannot move into a stub");
    auto pos = modRmOffset + 1;
    if (operand.Rm == ModRmRmSibPresent) {
        if (pos >= length)
            throw CodegenException("Memory operand truncated before its SIB byte");
        operand.Sib = data[pos++];
        operand.StackBase = (operand.Sib & SibBaseMask) == ModRmRmSibPresent && (rex & kRexB) == 0;
    }
    const auto size = _displacementSize(operand.Mod, operand.Rm, operand.Sib);
    if (pos + size > length)
        throw CodegenException("Memory operand truncated in its displacement");
    operand.EncodedSize = pos + size - modRmOffset;
    if (size == Disp8Size)
        operand.Displacement = static_cast<std::int8_t>(data[pos]);
    for (std::size_t index = 0; size == Disp32Size && index < size; ++index)
        operand.Displacement = static_cast<std::int32_t>(static_cast<std::uint32_t>(operand.Displacement) | (static_cast<std::uint32_t>(data[pos + index]) << (index * 8)));
    return operand;
}

void EmitSse(std::vector<std::uint8_t>& out, const std::uint8_t prefix, const std::initializer_list<std::uint8_t> opcode, const std::uint8_t dst, const std::uint8_t src) {
    _emit(out, prefix, dst, src, opcode, static_cast<std::uint8_t>(kModRmRegister | ((dst & 7) << 3) | (src & 7)));
}

void EmitShiftImm(std::vector<std::uint8_t>& out, const std::uint8_t extension, const std::uint8_t reg, const std::uint8_t imm) {
    _shiftImm(out, kShiftQwords, extension, reg, imm);
}

void StubBodyBuilder::Sse(const std::uint8_t prefix, const std::initializer_list<std::uint8_t> opcode, const std::uint8_t dst, const std::uint8_t src) {
    EmitSse(_bytes, prefix, opcode, dst, src);
}

void StubBodyBuilder::SsePlain(const std::initializer_list<std::uint8_t> opcode, const std::uint8_t dst, const std::uint8_t src) {
    const auto rex = _rex(dst, src);
    if (rex != kRexBase)
        _bytes.push_back(rex);
    _bytes.insert(_bytes.end(), opcode.begin(), opcode.end());
    _bytes.push_back(static_cast<std::uint8_t>(kModRmRegister | ((dst & 7) << 3) | (src & 7)));
}

void StubBodyBuilder::SseImm(const std::uint8_t prefix, const std::initializer_list<std::uint8_t> opcode, const std::uint8_t dst, const std::uint8_t src, const std::uint8_t imm) {
    EmitSse(_bytes, prefix, opcode, dst, src);
    _bytes.push_back(imm);
}

void StubBodyBuilder::ShiftImm(const std::uint8_t extension, const std::uint8_t reg, const std::uint8_t imm) {
    _shiftImm(_bytes, kShiftQwords, extension, reg, imm);
}

void StubBodyBuilder::ShiftDwordImm(const std::uint8_t extension, const std::uint8_t reg, const std::uint8_t imm) {
    _shiftImm(_bytes, kShiftDwords, extension, reg, imm);
}

void StubBodyBuilder::RipOperand(const std::initializer_list<std::uint8_t> opcode, const std::uint8_t reg, const StubConstant& constant) {
    _emit(_bytes, kPrefixPacked, reg, 0, opcode, static_cast<std::uint8_t>(((reg & 7) << 3) | kModRmRip));
    _fixups.push_back({_bytes.size(), _bytes.size() + 4, _constants.size()});
    _bytes.insert(_bytes.end(), 4, 0);
    _constants.push_back(constant);
}

void StubBodyBuilder::Load(const std::uint8_t reg, const MemoryOperand& operand) {
    using namespace X64OpcodeConstants;
    const auto displacement = static_cast<std::int64_t>(operand.Displacement) + static_cast<std::int64_t>(operand.StackBase ? _stackDepth : 0);
    if (displacement > std::numeric_limits<std::int32_t>::max())
        throw CodegenException("Stack displacement does not fit after spilling");
    const auto mod = operand.StackBase && _stackDepth != 0 ? ModRmModDisp32 : operand.Mod;
    _bytes.insert(_bytes.end(), operand.Prefixes.begin(), operand.Prefixes.end());
    _bytes.push_back(kPrefixScalar);
    const auto rex = static_cast<std::uint8_t>(kRexBase | ((reg & 8) != 0 ? kRexR : 0) | operand.RexIndexBase);
    if (rex != kRexBase)
        _bytes.push_back(rex);
    _bytes.insert(_bytes.end(), {TwoByteOpcodeEscape, kMovdqu, static_cast<std::uint8_t>((mod << ModRmModShift) | ((reg & 7) << ModRmRegShift) | operand.Rm)});
    if (operand.Rm == ModRmRmSibPresent)
        _bytes.push_back(operand.Sib);
    const auto value = static_cast<std::uint32_t>(static_cast<std::int32_t>(displacement));
    for (std::size_t index = 0; index < _displacementSize(mod, operand.Rm, operand.Sib); ++index)
        _bytes.push_back(static_cast<std::uint8_t>(value >> (index * 8)));
}

void StubBodyBuilder::Spill(const std::uint8_t reg) {
    _stackDepth += kRedZoneSpillFrame;
    _bytes.insert(_bytes.end(), kLeaRspBelowRedZone.Bytes, kLeaRspBelowRedZone.Bytes + kLeaRspBelowRedZone.Size);
    _emit(_bytes, kPrefixScalar, reg, 0, {0x0F, 0x7F}, static_cast<std::uint8_t>(((reg & 7) << 3) | kModRmRspBase));
    _bytes.push_back(kSibRsp);
}

void StubBodyBuilder::Restore(const std::uint8_t reg) {
    _emit(_bytes, kPrefixScalar, reg, 0, {0x0F, 0x6F}, static_cast<std::uint8_t>(((reg & 7) << 3) | kModRmRspBase));
    _bytes.push_back(kSibRsp);
    _bytes.insert(_bytes.end(), kLeaRspRestore.Bytes, kLeaRspRestore.Bytes + kLeaRspRestore.Size);
    _stackDepth -= kRedZoneSpillFrame;
}

void StubBodyBuilder::Raw(std::span<const std::uint8_t> bytes) {
    _bytes.insert(_bytes.end(), bytes.begin(), bytes.end());
}

LoweredBody StubBodyBuilder::Finish() {
    const auto returnBranchOffset = _bytes.size();
    _bytes.insert(_bytes.end(), kJmpRel32.Bytes, kJmpRel32.Bytes + kJmpRel32.Size);
    std::vector<std::size_t> constantOffsets;
    for (const auto& constant : _constants) {
        while (_bytes.size() % kStubAlignment != 0)
            _bytes.push_back(kTrapFill);
        constantOffsets.push_back(_bytes.size());
        _bytes.insert(_bytes.end(), constant.begin(), constant.end());
    }
    for (const auto& fixup : _fixups) {
        const auto displacement = static_cast<std::int64_t>(constantOffsets[fixup.ConstantIndex]) - static_cast<std::int64_t>(fixup.InstructionEnd);
        const auto value = static_cast<std::uint32_t>(static_cast<std::int32_t>(displacement));
        for (std::size_t index = 0; index < 4; ++index)
            _bytes[fixup.DisplacementOffset + index] = static_cast<std::uint8_t>(value >> (index * 8));
    }
    return {std::move(_bytes), returnBranchOffset};
}

}
