#include <codegen/x86/X64InstructionRewriter.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <codegen/CodegenException.hpp>
#include <codegen/IInstructionScanner.hpp>
#include <cstdint>
#include <limits>

namespace Codegen {

X64InstructionRewriter::ReferenceSite X64InstructionRewriter::ClassifyInstruction(
    const std::uint8_t* data,
    std::size_t length
) {
    std::size_t pos = 0;

    while (pos < length) {
        const std::uint8_t b = data[pos];
        if (b == OpcodePrefixLock || b == OpcodePrefixRepne || b == OpcodePrefixRep ||
            b == OpcodePrefixSegCs || b == OpcodePrefixSegSs || b == OpcodePrefixSegDs ||
            b == OpcodePrefixSegEs || b == OpcodePrefixSegFs || b == OpcodePrefixSegGs ||
            b == OpcodePrefixOperandSize || b == OpcodePrefixAddressSize) {
            pos += 1;
            continue;
        }
        break;
    }

    if (pos < length && data[pos] >= RexMin && data[pos] <= RexMax) {
        pos += 1;
    }

    if (pos >= length) {
        return {ReferenceKind::None, 0};
    }

    std::uint8_t opcode = data[pos];
    const std::size_t opcodeOffset = pos;
    pos += 1;
    bool twoByteOpcode = false;

    if (opcode == TwoByteOpcodeEscape) {
        if (pos >= length) {
            return {ReferenceKind::None, 0};
        }
        twoByteOpcode = true;
        opcode = data[pos];
        pos += 1;
    }

    if (!twoByteOpcode) {
        if (opcode == OpcodeCallRel32 || opcode == OpcodeJmpRel32) {
            if (length >= opcodeOffset + Rel32InstructionLength) {
                return {ReferenceKind::Rel32Branch, pos};
            }
            return {ReferenceKind::None, 0};
        }

        if (opcode == OpcodeJmpRel8 || (opcode >= OpcodeJccRel8Min && opcode <= OpcodeJccRel8Max)) {
            if (length >= pos + Rel8FieldSize) {
                return {ReferenceKind::Rel8Branch, pos};
            }
            return {ReferenceKind::None, 0};
        }
    } else {
        if (opcode >= OpcodeJccRel32Min && opcode <= OpcodeJccRel32Max) {
            if (length >= pos + Rel32FieldSize) {
                return {ReferenceKind::Rel32Branch, pos};
            }
            return {ReferenceKind::None, 0};
        }
    }

    if (pos >= length) {
        return {ReferenceKind::None, 0};
    }

    const std::uint8_t modrm = data[pos];
    const std::uint8_t mod = static_cast<std::uint8_t>((modrm >> ModRmModShift) & ModRmModMask);
    const std::uint8_t rm = static_cast<std::uint8_t>(modrm & ModRmRmMask);

    if (mod == ModRmModIndirect && rm == ModRmRmRipRelative) {
        const std::size_t dispOffset = pos + 1;
        if (length >= dispOffset + Disp32FieldSize) {
            return {ReferenceKind::RipRelativeDisp32, dispOffset};
        }
    }

    return {ReferenceKind::None, 0};
}

std::int32_t X64InstructionRewriter::ReadInt32(const std::uint8_t* data) {
    return static_cast<std::int32_t>(
        static_cast<std::uint32_t>(data[0]) |
        (static_cast<std::uint32_t>(data[1]) << 8) |
        (static_cast<std::uint32_t>(data[2]) << 16) |
        (static_cast<std::uint32_t>(data[3]) << 24));
}

void X64InstructionRewriter::WriteInt32(std::uint8_t* data, std::int32_t value) {
    const auto unsignedValue = static_cast<std::uint32_t>(value);
    data[0] = static_cast<std::uint8_t>(unsignedValue & 0xFF);
    data[1] = static_cast<std::uint8_t>((unsignedValue >> 8) & 0xFF);
    data[2] = static_cast<std::uint8_t>((unsignedValue >> 16) & 0xFF);
    data[3] = static_cast<std::uint8_t>((unsignedValue >> 24) & 0xFF);
}

std::int64_t X64InstructionRewriter::AdjustedDelta(
    std::int64_t instrPosition,
    std::int64_t targetPosition,
    std::int64_t cutStart,
    std::int64_t delta
) {
    const bool instrBeforeCut = instrPosition < cutStart;
    const bool targetBeforeCut = targetPosition < cutStart;

    if (instrBeforeCut == targetBeforeCut) {
        return 0;
    }

    return instrBeforeCut ? delta : -delta;
}

RewriteResult X64InstructionRewriter::Rewrite(
    const std::vector<std::uint8_t>& codeSection,
    const RewriteRequest& request
) const {
    const auto offset = static_cast<std::size_t>(request.Offset);

    if (offset > codeSection.size()) {
        throw CodegenException("Rewrite offset is out of bounds", request.Offset);
    }

    const X64InstructionDecoder decoder;
    const auto decoded = decoder.Decode(codeSection.data() + offset, codeSection.size() - offset);

    if (!decoded.Valid || decoded.Length == 0) {
        throw CodegenException("Cannot decode instruction at rewrite offset", request.Offset);
    }

    const auto oldLength = static_cast<std::int64_t>(decoded.Length);
    const auto newLength = static_cast<std::int64_t>(request.NewBytes.size());
    const std::int64_t delta = newLength - oldLength;

    const auto cutStart = static_cast<std::int64_t>(offset);
    const std::int64_t cutEnd = cutStart + oldLength;

    std::vector<std::uint8_t> rewritten;
    rewritten.reserve(codeSection.size() + static_cast<std::size_t>(delta > 0 ? delta : 0));
    rewritten.insert(rewritten.end(), codeSection.begin(), codeSection.begin() + offset);
    rewritten.insert(rewritten.end(), request.NewBytes.begin(), request.NewBytes.end());
    rewritten.insert(rewritten.end(), codeSection.begin() + offset + decoded.Length, codeSection.end());

    const auto scanner = MakeInstructionScanner();
    const auto matches = scanner->ScanCodeSection(codeSection, 0, codeSection.size());

    std::vector<AddressAdjustment> adjustments;

    for (const auto& match : matches) {
        const auto matchOffset = static_cast<std::int64_t>(match.Offset);

        if (matchOffset >= cutStart && matchOffset < cutEnd) {
            continue;
        }

        const auto site = ClassifyInstruction(codeSection.data() + match.Offset, match.Length);

        if (site.Kind != ReferenceKind::None) {
            const std::int64_t instrEnd = matchOffset + static_cast<std::int64_t>(match.Length);
            const std::size_t fieldPosition = static_cast<std::size_t>(match.Offset) + site.FieldOffset;

            if (site.Kind == ReferenceKind::Rel8Branch) {
                const auto disp = static_cast<std::int8_t>(codeSection[fieldPosition]);
                const std::int64_t targetPosition = instrEnd + disp;
                const std::int64_t adjust = AdjustedDelta(matchOffset, targetPosition, cutStart, delta);

                if (adjust != 0) {
                    const std::int64_t newDisp = static_cast<std::int64_t>(disp) + adjust;

                    if (newDisp < std::numeric_limits<std::int8_t>::min() ||
                        newDisp > std::numeric_limits<std::int8_t>::max()) {
                        throw CodegenException("Rel8 branch displacement overflow after rewrite", match.Offset);
                    }

                    const std::int64_t newMatchOffset = matchOffset < cutStart ? matchOffset : matchOffset + delta;
                    const auto newFieldPosition = static_cast<std::size_t>(
                        newMatchOffset + static_cast<std::int64_t>(site.FieldOffset));
                    rewritten[newFieldPosition] = static_cast<std::uint8_t>(static_cast<std::int8_t>(newDisp));
                }
            } else {
                const auto disp = ReadInt32(codeSection.data() + fieldPosition);
                const std::int64_t targetPosition = instrEnd + disp;
                const std::int64_t adjust = AdjustedDelta(matchOffset, targetPosition, cutStart, delta);

                if (adjust != 0) {
                    const std::int64_t newDisp = static_cast<std::int64_t>(disp) + adjust;

                    if (newDisp < std::numeric_limits<std::int32_t>::min() ||
                        newDisp > std::numeric_limits<std::int32_t>::max()) {
                        throw CodegenException("Rel32/disp32 displacement overflow after rewrite", match.Offset);
                    }

                    const std::int64_t newMatchOffset = matchOffset < cutStart ? matchOffset : matchOffset + delta;
                    const auto newFieldPosition = static_cast<std::size_t>(
                        newMatchOffset + static_cast<std::int64_t>(site.FieldOffset));
                    WriteInt32(rewritten.data() + newFieldPosition, static_cast<std::int32_t>(newDisp));
                }
            }
        }

        if (matchOffset >= cutEnd && delta != 0) {
            const auto newOffset = static_cast<Domain::FileByteOffset>(matchOffset + delta);
            adjustments.push_back({match.Offset, newOffset, delta});
        }
    }

    return {rewritten, adjustments};
}

}
