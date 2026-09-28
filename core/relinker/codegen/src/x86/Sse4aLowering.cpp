#include <codegen/x86/Sse4aLowering.hpp>
#include <codegen/x86/Amd64OnlySubstitutionTable.hpp>
#include <codegen/CodegenException.hpp>
#include <algorithm>
#include <array>
#include <span>
#include <initializer_list>

namespace Codegen {

namespace {

using namespace Amd64OnlySubstitutionTable;

using Constant = std::array<std::uint8_t, 16>;

constexpr std::uint8_t kPrefixPacked = 0x66;
constexpr std::uint8_t kPrefixScalar = 0xF3;
constexpr std::uint8_t kRexBase = 0x40;
constexpr std::uint8_t kRexR = 0x04;
constexpr std::uint8_t kRexB = 0x01;
constexpr std::uint8_t kModRmRegister = 0xC0;
constexpr std::uint8_t kModRmRip = 0x05;
constexpr std::uint8_t kModRmRspBase = 0x04;
constexpr std::uint8_t kSibRsp = 0x24;
constexpr std::uint8_t kShiftRight = 2;
constexpr std::uint8_t kShiftLeft = 6;
constexpr std::uint8_t kFieldBits = 64;

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

void _sse(std::vector<std::uint8_t>& out, const std::uint8_t prefix, const std::initializer_list<std::uint8_t> opcode, const std::uint8_t dst, const std::uint8_t src) {
    _emit(out, prefix, dst, src, opcode, static_cast<std::uint8_t>(kModRmRegister | ((dst & 7) << 3) | (src & 7)));
}

void _shiftImm(std::vector<std::uint8_t>& out, const std::uint8_t extension, const std::uint8_t reg, const std::uint8_t imm) {
    _emit(out, kPrefixPacked, 0, reg, {0x0F, 0x73}, static_cast<std::uint8_t>(kModRmRegister | (extension << 3) | (reg & 7)));
    out.push_back(imm);
}

void _nopFill(std::vector<std::uint8_t>& out, std::size_t count) {
    while (count > 0) {
        const auto& nop = kNops[std::min<std::size_t>(count, std::size(kNops)) - 1];
        out.insert(out.end(), nop.Bytes, nop.Bytes + nop.Size);
        count -= nop.Size;
    }
}

std::uint64_t _fieldMask(const std::uint8_t length) {
    return length >= kFieldBits ? ~std::uint64_t{0} : ((std::uint64_t{1} << length) - 1);
}

class BodyBuilder {
public:
    void Sse(const std::uint8_t prefix, const std::initializer_list<std::uint8_t> opcode, const std::uint8_t dst, const std::uint8_t src) {
        _sse(_bytes, prefix, opcode, dst, src);
    }

    void ShiftImm(const std::uint8_t extension, const std::uint8_t reg, const std::uint8_t imm) {
        _shiftImm(_bytes, extension, reg, imm);
    }

    void RipOperand(const std::initializer_list<std::uint8_t> opcode, const std::uint8_t reg, const Constant& constant) {
        _emit(_bytes, kPrefixPacked, reg, 0, opcode, static_cast<std::uint8_t>(((reg & 7) << 3) | kModRmRip));
        _fixups.push_back({_bytes.size(), _bytes.size() + 4, _constants.size()});
        _bytes.insert(_bytes.end(), 4, 0);
        _constants.push_back(constant);
    }

    void Spill(const std::uint8_t reg) {
        _bytes.insert(_bytes.end(), kLeaRspBelowRedZone.Bytes, kLeaRspBelowRedZone.Bytes + kLeaRspBelowRedZone.Size);
        _emit(_bytes, kPrefixScalar, reg, 0, {0x0F, 0x7F}, static_cast<std::uint8_t>(((reg & 7) << 3) | kModRmRspBase));
        _bytes.push_back(kSibRsp);
    }

    void Restore(const std::uint8_t reg) {
        _emit(_bytes, kPrefixScalar, reg, 0, {0x0F, 0x6F}, static_cast<std::uint8_t>(((reg & 7) << 3) | kModRmRspBase));
        _bytes.push_back(kSibRsp);
        _bytes.insert(_bytes.end(), kLeaRspRestore.Bytes, kLeaRspRestore.Bytes + kLeaRspRestore.Size);
    }

    void Raw(std::span<const std::uint8_t> bytes) {
        _bytes.insert(_bytes.end(), bytes.begin(), bytes.end());
    }

    LoweredBody Finish() {
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

private:
    struct Fixup {
        std::size_t DisplacementOffset;
        std::size_t InstructionEnd;
        std::size_t ConstantIndex;
    };

    std::vector<std::uint8_t> _bytes;
    std::vector<Fixup> _fixups;
    std::vector<Constant> _constants;
};

void _emitOutOfLine(BodyBuilder& body, const Sse4aOperands& operands) {
    if (operands.RegisterForm) {
        if (operands.Insertq)
            throw CodegenException("INSERTQ register form has no Intel lowering");
        const auto dst = operands.Destination;
        const auto src = operands.Source;
        std::array<std::uint8_t, 2> scratch{};
        for (std::uint8_t reg = 0, found = 0; found < scratch.size(); ++reg)
            if (reg != dst && reg != src) scratch[found++] = reg;
        Constant fieldMask{};
        fieldMask[0] = kFieldBits - 1;
        Constant one{};
        one[0] = 1;
        body.Spill(scratch[0]);
        body.Spill(scratch[1]);
        body.Sse(kPrefixPacked, {0x0F, 0x6F}, scratch[0], src);
        body.ShiftImm(kShiftRight, scratch[0], 8);
        body.RipOperand({0x0F, 0xDB}, scratch[0], fieldMask);
        body.Sse(kPrefixPacked, {0x0F, 0x6F}, scratch[1], src);
        body.RipOperand({0x0F, 0xDB}, scratch[1], fieldMask);
        body.RipOperand({0x0F, 0xEF}, scratch[1], fieldMask);
        body.RipOperand({0x0F, 0xD4}, scratch[1], one);
        body.RipOperand({0x0F, 0xDB}, scratch[1], fieldMask);
        body.Sse(kPrefixPacked, {0x0F, 0xD3}, dst, scratch[0]);
        body.Sse(kPrefixPacked, {0x0F, 0xF3}, dst, scratch[1]);
        body.Sse(kPrefixPacked, {0x0F, 0xD3}, dst, scratch[1]);
        body.Restore(scratch[1]);
        body.Restore(scratch[0]);
        return;
    }
    const auto length = operands.Length;
    const auto index = operands.Index;
    const auto dst = operands.Destination;
    const auto src = operands.Source;
    const bool byteAligned = length % 8 == 0 && index % 8 == 0;
    if (!operands.Insertq) {
        if (byteAligned) {
            Constant mask;
            mask.fill(kPshufbZero);
            for (std::size_t byte = 0; byte < length / 8; ++byte)
                mask[byte] = static_cast<std::uint8_t>(index / 8 + byte);
            body.RipOperand({0x0F, 0x38, 0x00}, dst, mask);
        } else {
            if (index != 0)
                body.ShiftImm(kShiftRight, dst, index);
            if (length != kFieldBits) {
                body.ShiftImm(kShiftLeft, dst, static_cast<std::uint8_t>(kFieldBits - length));
                body.ShiftImm(kShiftRight, dst, static_cast<std::uint8_t>(kFieldBits - length));
            }
        }
    } else if (byteAligned) {
        if (src != dst)
            body.Sse(kPrefixPacked, {0x0F, 0x6C}, dst, src);
        Constant mask;
        mask.fill(kPshufbZero);
        for (std::size_t byte = 0; byte < 8; ++byte)
            mask[byte] = static_cast<std::uint8_t>(byte);
        for (std::size_t byte = index / 8; byte < index / 8 + length / 8; ++byte)
            mask[byte] = static_cast<std::uint8_t>((src != dst ? 8 : 0) + (byte - index / 8));
        body.RipOperand({0x0F, 0x38, 0x00}, dst, mask);
    } else {
        std::uint8_t scratch = 0;
        while (scratch == dst || scratch == src)
            ++scratch;
        body.Spill(scratch);
        body.Sse(kPrefixPacked, {0x0F, 0x6F}, scratch, src);
        if (index != 0)
            body.ShiftImm(kShiftLeft, scratch, index);
        body.Sse(kPrefixPacked, {0x0F, 0xEF}, scratch, dst);
        Constant hole{};
        const auto holeMask = _fieldMask(length) << index;
        for (std::size_t byte = 0; byte < 8; ++byte)
            hole[byte] = static_cast<std::uint8_t>(holeMask >> (byte * 8));
        body.RipOperand({0x0F, 0xDB}, scratch, hole);
        body.Sse(kPrefixPacked, {0x0F, 0xEF}, dst, scratch);
        body.Restore(scratch);
    }
}

}

std::optional<std::vector<std::uint8_t>> Sse4aLowering::LowerInPlace(const Sse4aOperands& operands, const std::size_t originalLength) const {
    if (operands.RegisterForm)
        return std::nullopt;
    const auto length = operands.Length;
    const auto index = operands.Index;
    const auto dst = operands.Destination;
    const auto src = operands.Source;
    std::vector<std::uint8_t> sequence;
    if (operands.Insertq) {
        if (dst == src && index == 0) {
        } else if (length == kFieldBits && index == 0) {
            _sse(sequence, kPrefixScalar, {0x0F, 0x7E}, dst, src);
        } else if (index == 0 && length % 16 == 0) {
            _sse(sequence, kPrefixPacked, {0x0F, 0x3A, 0x0E}, dst, src);
            sequence.push_back(static_cast<std::uint8_t>((1u << (length / 16)) - 1));
        } else {
            return std::nullopt;
        }
    } else {
        if (index == 0 && length == kFieldBits) {
        } else if (index + length == kFieldBits) {
            _shiftImm(sequence, kShiftRight, dst, index);
        } else if (index == 0 && (length == 8 || length == 16 || length == 32)) {
            const std::uint8_t opcode = length == 8 ? 0x32 : (length == 16 ? 0x34 : 0x35);
            _sse(sequence, kPrefixPacked, {0x0F, 0x38, opcode}, dst, dst);
        } else {
            return std::nullopt;
        }
    }
    if (sequence.size() > originalLength)
        return std::nullopt;
    _nopFill(sequence, originalLength - sequence.size());
    return sequence;
}

LoweredBody Sse4aLowering::LowerOutOfLine(const Sse4aOperands& operands, std::span<const std::uint8_t> trailing) const {
    return LowerOutOfLine(std::span<const Sse4aOperands>(&operands, 1), trailing);
}

LoweredBody Sse4aLowering::LowerOutOfLine(std::span<const Sse4aOperands> sequence, std::span<const std::uint8_t> trailing) const {
    BodyBuilder body;
    for (const auto& operands : sequence)
        _emitOutOfLine(body, operands);
    body.Raw(trailing);
    return body.Finish();
}

}
