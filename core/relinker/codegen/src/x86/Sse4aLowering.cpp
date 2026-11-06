#include <codegen/x86/Sse4aLowering.hpp>
#include <codegen/x86/StubBodyBuilder.hpp>
#include <codegen/x86/Amd64OnlySubstitutionTable.hpp>
#include <algorithm>
#include <array>
#include <span>
#include <initializer_list>

namespace Codegen {

namespace {

using namespace Amd64OnlySubstitutionTable;

using Constant = StubConstant;
using BodyBuilder = StubBodyBuilder;

constexpr std::uint8_t kPrefixPacked = kStubPrefixPacked;
constexpr std::uint8_t kPrefixScalar = 0xF3;
constexpr std::uint8_t kPrefixInsert = 0xF2;
constexpr std::uint8_t kShiftRight = 2;
constexpr std::uint8_t kShiftBytesRight = 3;
constexpr std::uint8_t kShiftLeft = 6;
constexpr std::uint8_t kFieldBits = 64;
constexpr std::uint8_t kInsertqLengthByte = 8;
constexpr std::uint8_t kInsertqIndexByte = 9;

void _sse(std::vector<std::uint8_t>& out, const std::uint8_t prefix, const std::initializer_list<std::uint8_t> opcode, const std::uint8_t dst, const std::uint8_t src) {
    EmitSse(out, prefix, opcode, dst, src);
}

void _shiftImm(std::vector<std::uint8_t>& out, const std::uint8_t extension, const std::uint8_t reg, const std::uint8_t imm) {
    EmitShiftImm(out, kStubShiftQword, extension, reg, imm);
}

void _nopFill(std::vector<std::uint8_t>& out, const std::size_t count) {
    EmitNopFill(out, count);
}

void _zeroUpper(StubBodyBuilder& body, const std::uint8_t dst) {
    body.Sse(kPrefixScalar, {0x0F, 0x7E}, dst, dst);
}

std::uint64_t _fieldMask(const std::uint8_t length) {
    return length >= kFieldBits ? ~std::uint64_t{0} : ((std::uint64_t{1} << length) - 1);
}

void _emitInsertqRegisterForm(BodyBuilder& body, const Sse4aOperands& operands) {
    const auto dst = operands.Destination;
    const auto src = operands.Source;
    std::array<std::uint8_t, 3> scratch{};
    for (std::uint8_t reg = 0, found = 0; found < scratch.size(); ++reg)
        if (reg != dst && reg != src) scratch[found++] = reg;
    Constant fieldMask{};
    fieldMask[0] = kFieldBits - 1;
    Constant one{};
    one[0] = 1;
    body.Spill(scratch[0]);
    body.Spill(scratch[1]);
    body.Spill(scratch[2]);
    body.Sse(kPrefixPacked, {0x0F, 0x6F}, scratch[0], src);
    body.ShiftImm(kStubShiftQword, kShiftBytesRight, scratch[0], kInsertqIndexByte);
    body.RipOperand({0x0F, 0xDB}, scratch[0], fieldMask);
    body.Sse(kPrefixPacked, {0x0F, 0x6F}, scratch[1], src);
    body.ShiftImm(kStubShiftQword, kShiftBytesRight, scratch[1], kInsertqLengthByte);
    body.RipOperand({0x0F, 0xDB}, scratch[1], fieldMask);
    body.RipOperand({0x0F, 0xEF}, scratch[1], fieldMask);
    body.RipOperand({0x0F, 0xD4}, scratch[1], one);
    body.RipOperand({0x0F, 0xDB}, scratch[1], fieldMask);
    body.Sse(kPrefixPacked, {0x0F, 0x76}, scratch[2], scratch[2]);
    body.Sse(kPrefixPacked, {0x0F, 0xD3}, scratch[2], scratch[1]);
    body.Sse(kPrefixPacked, {0x0F, 0xF3}, scratch[2], scratch[0]);
    body.Sse(kPrefixPacked, {0x0F, 0x6F}, scratch[1], src);
    body.Sse(kPrefixPacked, {0x0F, 0xF3}, scratch[1], scratch[0]);
    body.Sse(kPrefixPacked, {0x0F, 0xDB}, scratch[1], scratch[2]);
    body.Sse(kPrefixPacked, {0x0F, 0xDF}, scratch[2], dst);
    body.Sse(kPrefixPacked, {0x0F, 0xEB}, scratch[2], scratch[1]);
    body.Sse(kPrefixInsert, {0x0F, 0x10}, dst, scratch[2]);
    body.Restore(scratch[2]);
    body.Restore(scratch[1]);
    body.Restore(scratch[0]);
}

void _emitOutOfLine(BodyBuilder& body, const Sse4aOperands& operands) {
    if (operands.RegisterForm) {
        if (operands.Insertq) {
            _emitInsertqRegisterForm(body, operands);
            return;
        }
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
        body.ShiftImm(kStubShiftQword, kShiftRight, scratch[0], 8);
        body.RipOperand({0x0F, 0xDB}, scratch[0], fieldMask);
        body.Sse(kPrefixPacked, {0x0F, 0x6F}, scratch[1], src);
        body.RipOperand({0x0F, 0xDB}, scratch[1], fieldMask);
        body.RipOperand({0x0F, 0xEF}, scratch[1], fieldMask);
        body.RipOperand({0x0F, 0xD4}, scratch[1], one);
        body.RipOperand({0x0F, 0xDB}, scratch[1], fieldMask);
        body.Sse(kPrefixPacked, {0x0F, 0xD3}, dst, scratch[0]);
        body.Sse(kPrefixPacked, {0x0F, 0xF3}, dst, scratch[1]);
        body.Sse(kPrefixPacked, {0x0F, 0xD3}, dst, scratch[1]);
        _zeroUpper(body, dst);
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
                body.ShiftImm(kStubShiftQword, kShiftRight, dst, index);
            if (length != kFieldBits) {
                body.ShiftImm(kStubShiftQword, kShiftLeft, dst, static_cast<std::uint8_t>(kFieldBits - length));
                body.ShiftImm(kStubShiftQword, kShiftRight, dst, static_cast<std::uint8_t>(kFieldBits - length));
            }
            _zeroUpper(body, dst);
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
            body.ShiftImm(kStubShiftQword, kShiftLeft, scratch, index);
        body.Sse(kPrefixPacked, {0x0F, 0xEF}, scratch, dst);
        Constant hole{};
        const auto holeMask = _fieldMask(length) << index;
        for (std::size_t byte = 0; byte < 8; ++byte)
            hole[byte] = static_cast<std::uint8_t>(holeMask >> (byte * 8));
        body.RipOperand({0x0F, 0xDB}, scratch, hole);
        body.Sse(kPrefixPacked, {0x0F, 0xEF}, dst, scratch);
        _zeroUpper(body, dst);
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
            EmitSse(sequence, kPrefixScalar, {0x0F, 0x7E}, dst, dst);
        } else if (length == kFieldBits && index == 0) {
            EmitSse(sequence, kPrefixScalar, {0x0F, 0x7E}, dst, src);
        } else {
            return std::nullopt;
        }
    } else {
        if (index == 0 && length == kFieldBits) {
            EmitSse(sequence, kPrefixScalar, {0x0F, 0x7E}, dst, dst);
        } else {
            return std::nullopt;
        }
    }
    if (sequence.size() > originalLength)
        return std::nullopt;
    _nopFill(sequence, originalLength - sequence.size());
    return sequence;
}

void Sse4aLowering::EmitOutOfLine(StubBodyBuilder& body, const Sse4aOperands& operands) const {
    _emitOutOfLine(body, operands);
}

LoweredBody Sse4aLowering::LowerOutOfLine(const Sse4aOperands& operands) const {
    return LowerOutOfLine(std::span<const Sse4aOperands>(&operands, 1));
}

LoweredBody Sse4aLowering::LowerOutOfLine(std::span<const Sse4aOperands> sequence) const {
    StubBodyBuilder body;
    for (const auto& operands : sequence)
        _emitOutOfLine(body, operands);
    return body.Finish();
}

}
