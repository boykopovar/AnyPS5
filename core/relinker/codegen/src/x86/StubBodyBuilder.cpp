#include <codegen/x86/StubBodyBuilder.hpp>
#include <codegen/x86/Amd64OnlySubstitutionTable.hpp>
#include <codegen/x86/DecodedInstruction.hpp>
#include <codegen/x86/X64OpcodeConstants.hpp>
#include <codegen/CodegenException.hpp>
#include <algorithm>
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

bool _addressSize32(const std::span<const std::uint8_t> instruction) {
    const DecodedInstruction decoded{instruction.data(), instruction.size()};
    const auto prefixes = instruction.first(decoded.OpcodeOffset());
    return std::find(prefixes.begin(), prefixes.end(), X64OpcodeConstants::PrefixAddressSize) != prefixes.end();
}

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
    if (mod == ModRmModDisp32 || (mod == ModRmModIndirect && rm == ModRmRmRipRelative) || (mod == ModRmModIndirect && rm == ModRmRmSibPresent && (sib & SibBaseMask) == SibBaseDisp32))
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
    MemoryOperand operand{std::move(prefixes), static_cast<std::uint8_t>(rex & (kRexX | kRexB)), static_cast<std::uint8_t>((modrm >> ModRmModShift) & ModRmModMask), static_cast<std::uint8_t>(modrm & ModRmRmMask), 0, 0, false, 0, false, length};
    if (operand.Mod == ModRmModRegister)
        throw CodegenException("Register operand where a memory operand was expected");
    operand.RipRelative = operand.Mod == ModRmModIndirect && operand.Rm == ModRmRmRipRelative;
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

void ApplyStubRelocations(const std::span<std::uint8_t> body, const std::span<const StubRelocation> relocations, const std::uint64_t siteAddress, const std::uint64_t stubAddress, const Domain::FileByteOffset failureOffset) {
    using namespace X64OpcodeConstants;
    for (const auto& relocation : relocations) {
        if (relocation.InstructionEnd > body.size() || relocation.DisplacementOffset > relocation.InstructionEnd || relocation.InstructionEnd - relocation.DisplacementOffset < Disp32Size)
            throw CodegenException("Stub relocation lies outside the stub body", failureOffset);
        const auto target = siteAddress + static_cast<std::uint64_t>(relocation.SiteTarget);
        const auto displacement = static_cast<std::int64_t>(target - (stubAddress + relocation.InstructionEnd));
        if (!relocation.AddressSize32 && (displacement < std::numeric_limits<std::int32_t>::min() || displacement > std::numeric_limits<std::int32_t>::max()))
            throw CodegenException("RIP-relative operand is out of rel32 range from its stub", failureOffset);
        const auto value = static_cast<std::uint32_t>(displacement);
        for (std::size_t index = 0; index < Disp32Size; ++index)
            body[relocation.DisplacementOffset + index] = static_cast<std::uint8_t>(value >> (index * 8));
    }
}

void EmitSse(std::vector<std::uint8_t>& out, const std::uint8_t prefix, const std::initializer_list<std::uint8_t> opcode, const std::uint8_t dst, const std::uint8_t src) {
    _emit(out, prefix, dst, src, opcode, static_cast<std::uint8_t>(kModRmRegister | ((dst & 7) << 3) | (src & 7)));
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
    if (operand.RipRelative) {
        const auto addressSize32 = std::find(operand.Prefixes.begin(), operand.Prefixes.end(), PrefixAddressSize) != operand.Prefixes.end();
        _relocations.push_back({_bytes.size(), _bytes.size() + Disp32Size, _siteOffset + static_cast<std::int64_t>(operand.InstructionLength) + operand.Displacement, addressSize32});
        _bytes.insert(_bytes.end(), Disp32Size, 0);
        return;
    }
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

void StubBodyBuilder::Move(const std::span<const std::uint8_t> instruction, const std::optional<std::size_t> ripDisplacementOffset) {
    using namespace X64OpcodeConstants;
    const auto start = _bytes.size();
    Raw(instruction);
    if (ripDisplacementOffset) {
        if (*ripDisplacementOffset > instruction.size() || instruction.size() - *ripDisplacementOffset < Disp32Size)
            throw CodegenException("RIP-relative displacement lies outside its instruction");
        std::uint32_t value = 0;
        for (std::size_t index = 0; index < Disp32Size; ++index) {
            value |= static_cast<std::uint32_t>(instruction[*ripDisplacementOffset + index]) << (index * 8);
            _bytes[start + *ripDisplacementOffset + index] = 0;
        }
        _relocations.push_back({start + *ripDisplacementOffset, start + instruction.size(), _siteOffset + static_cast<std::int64_t>(instruction.size()) + static_cast<std::int32_t>(value), _addressSize32(instruction)});
    }
    Advance(instruction.size());
}

void StubBodyBuilder::RipRelative(const std::span<const std::uint8_t> instruction, const std::size_t displacementOffset, const std::int64_t target) {
    using namespace X64OpcodeConstants;
    if (displacementOffset > instruction.size() || instruction.size() - displacementOffset < Disp32Size)
        throw CodegenException("RIP-relative displacement lies outside its instruction");
    const auto start = _bytes.size();
    Raw(instruction);
    std::fill_n(_bytes.begin() + static_cast<std::ptrdiff_t>(start + displacementOffset), Disp32Size, std::uint8_t{0});
    _relocations.push_back({start + displacementOffset, start + instruction.size(), _siteOffset + target, _addressSize32(instruction)});
}

void StubBodyBuilder::MoveControlFlow(const std::span<const std::uint8_t> instruction, const DecodedInstructionInfo& info) {
    using namespace X64OpcodeConstants;
    const auto end = _siteOffset + static_cast<std::int64_t>(instruction.size());
    if (info.FlowKind == ControlFlowKind::IndirectCall)
        throw CodegenException("A register or memory call cannot move into a stub");
    if (info.TruncatesBranchTarget)
        throw CodegenException("A relative branch with this prefix cannot move into a stub");
    if (info.FlowKind == ControlFlowKind::Call && info.HasRipRelativeDisp) {
        _pushReturnAddress(end);
        std::vector<std::uint8_t> jump(instruction.begin(), instruction.end());
        auto& modrm = jump[info.RipRelativeDispOffset - 1];
        modrm = static_cast<std::uint8_t>((modrm & ~(ModRmRegMask << ModRmRegShift)) | (4u << ModRmRegShift));
        Move(jump, info.RipRelativeDispOffset);
        return;
    }
    if (!info.HasBranchTarget || info.HasRipRelativeDisp) {
        Move(instruction, info.HasRipRelativeDisp ? std::optional<std::size_t>(info.RipRelativeDispOffset) : std::nullopt);
        return;
    }
    for (std::size_t index = 0; index < info.OpcodeOffset; ++index) {
        const auto prefix = instruction[index];
        if (prefix != PrefixSegCs && prefix != PrefixSegDs && prefix != PrefixRepne && prefix != PrefixAddressSize && (prefix & 0xF0) != kRexBase)
            throw CodegenException("A relative branch with this prefix cannot move into a stub");
    }
    const auto target = end + info.BranchDisp;
    if (info.IsTwoByteOpcode) {
        _rel32({TwoByteOpcodeEscape, info.Opcode}, target);
    } else if (info.Opcode == OneByteCallRel32) {
        _pushReturnAddress(end);
        _rel32({OneByteJmpRel32}, target);
    } else if (info.Opcode == OneByteJmpRel32 || info.Opcode == OneByteJmpRel8) {
        _rel32({OneByteJmpRel32}, target);
    } else if (info.Opcode >= OneByteJccRel8Min && info.Opcode <= OneByteJccRel8Max) {
        _rel32({TwoByteOpcodeEscape, static_cast<std::uint8_t>(TwoByteJccRel32Min | (info.Opcode & 0x0F))}, target);
    } else {
        throw CodegenException("A LOOP or JRCXZ branch cannot move into a stub");
    }
    Advance(instruction.size());
}

void StubBodyBuilder::JumpTo(const std::int64_t siteTarget) {
    _rel32({X64OpcodeConstants::OneByteJmpRel32}, siteTarget);
}

void StubBodyBuilder::Seek(const std::int64_t siteOffset) {
    _siteOffset = siteOffset;
}

void StubBodyBuilder::Advance(const std::size_t length) {
    _siteOffset += static_cast<std::int64_t>(length);
}

void StubBodyBuilder::_rel32(const std::initializer_list<std::uint8_t> opcode, const std::int64_t siteTarget) {
    _bytes.insert(_bytes.end(), opcode);
    const auto displacement = _bytes.size();
    _bytes.insert(_bytes.end(), X64OpcodeConstants::Disp32Size, std::uint8_t{0});
    _relocations.push_back({displacement, _bytes.size(), siteTarget});
}

void StubBodyBuilder::_pushReturnAddress(const std::int64_t siteTarget) {
    _bytes.insert(_bytes.end(), {0x48, 0x8D, 0x64, 0x24, 0xF8, 0x50});
    _rel32({0x48, 0x8D, 0x05}, siteTarget);
    _bytes.insert(_bytes.end(), {0x48, 0x89, 0x44, 0x24, 0x08, 0x58});
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
    return {std::move(_bytes), returnBranchOffset, std::move(_relocations)};
}

}
