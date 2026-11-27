#include <codegen/x86/LengthChangingFixups.hpp>
#include <codegen/CodegenException.hpp>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace Codegen {

namespace {

constexpr std::uint8_t kEncodingOmit = 0xFF;
constexpr std::uint8_t kEncodingFormatMask = 0x0F;
constexpr std::uint8_t kEncodingApplicationMask = 0x70;
constexpr std::uint8_t kEncodingIndirect = 0x80;
constexpr std::uint8_t kFormatAbsPtr = 0x00;
constexpr std::uint8_t kFormatUleb128 = 0x01;
constexpr std::uint8_t kFormatUdata2 = 0x02;
constexpr std::uint8_t kFormatUdata4 = 0x03;
constexpr std::uint8_t kFormatUdata8 = 0x04;
constexpr std::uint8_t kFormatSleb128 = 0x09;
constexpr std::uint8_t kFormatSdata2 = 0x0A;
constexpr std::uint8_t kFormatSdata4 = 0x0B;
constexpr std::uint8_t kFormatSdata8 = 0x0C;
constexpr std::uint8_t kApplicationAbsPtr = 0x00;
constexpr std::uint8_t kApplicationPcRel = 0x10;
constexpr std::uint8_t kApplicationFunctionRel = 0x40;
constexpr std::uint32_t kExtendedRecordLength = 0xFFFFFFFF;

bool InCode(const Domain::VirtualAddress address, const CodeExtent& code) {
    return address >= code.Address && address - code.Address < code.Size;
}

std::optional<std::int64_t> OffsetInCode(const Domain::VirtualAddress address, const CodeExtent& code) {
    if (!InCode(address, code))
        return std::nullopt;
    return static_cast<std::int64_t>(address - code.Address);
}

Domain::VirtualAddress AddressOf(const std::int64_t offset, const CodeExtent& code) {
    return code.Address + static_cast<Domain::VirtualAddress>(offset);
}

Domain::VirtualAddress MapCodeAddress(const Domain::VirtualAddress address, const CodeExtent& code, const AddressMap& map) {
    const auto offset = OffsetInCode(address, code);
    if (!offset)
        return address;
    return AddressOf(map.MapAddress(*offset), code);
}

Domain::VirtualAddress MapCodeEnd(const Domain::VirtualAddress address, const CodeExtent& code, const AddressMap& map) {
    if (address == code.Address + code.Size)
        return AddressOf(map.MapAddress(static_cast<std::int64_t>(code.Size)), code);
    return MapCodeAddress(address, code, map);
}

std::int64_t SignedDifference(const std::uint64_t left, const std::uint64_t right) {
    return static_cast<std::int64_t>(left - right);
}

struct UnwindCursor {
    std::span<std::uint8_t> Bytes;
    std::size_t Position;
    std::size_t End;
};

struct UnwindPointer {
    std::uint64_t Address;
    bool Indirect;
};

struct CieEncoding {
    std::uint8_t PointerEncoding = kFormatAbsPtr;
    std::uint8_t LsdaEncoding = kEncodingOmit;
    bool Augmented = false;
};

struct CieRecord {
    std::size_t Start;
    CieEncoding Encoding;
};

std::uint8_t ReadUnwindByte(UnwindCursor& cursor) {
    if (cursor.Position >= cursor.End)
        throw CodegenException(".eh_frame record exceeds its length", cursor.Position);
    return cursor.Bytes[cursor.Position++];
}

std::uint16_t ReadUnwindU16(UnwindCursor& cursor) {
    if (cursor.Position > cursor.End || sizeof(std::uint16_t) > cursor.End - cursor.Position)
        throw CodegenException(".eh_frame record exceeds its length", cursor.Position);
    std::uint16_t value = 0;
    std::memcpy(&value, cursor.Bytes.data() + cursor.Position, sizeof(value));
    cursor.Position += sizeof(value);
    return value;
}

std::uint32_t ReadUnwindU32(UnwindCursor& cursor) {
    if (cursor.Position > cursor.End || sizeof(std::uint32_t) > cursor.End - cursor.Position)
        throw CodegenException(".eh_frame record exceeds its length", cursor.Position);
    std::uint32_t value = 0;
    std::memcpy(&value, cursor.Bytes.data() + cursor.Position, sizeof(value));
    cursor.Position += sizeof(value);
    return value;
}

std::uint64_t ReadUnwindU64(UnwindCursor& cursor) {
    if (cursor.Position > cursor.End || sizeof(std::uint64_t) > cursor.End - cursor.Position)
        throw CodegenException(".eh_frame record exceeds its length", cursor.Position);
    std::uint64_t value = 0;
    std::memcpy(&value, cursor.Bytes.data() + cursor.Position, sizeof(value));
    cursor.Position += sizeof(value);
    return value;
}

std::uint64_t ReadUnwindLeb(UnwindCursor& cursor, const bool signedValue) {
    std::uint64_t value = 0;
    unsigned shift = 0;
    for (;;) {
        const auto byte = ReadUnwindByte(cursor);
        if (shift == 63 && (byte & 0x7F) != 0 && (byte & 0x7F) != (signedValue ? 0x7F : 1))
            throw CodegenException("Overflowing .eh_frame LEB128 value", cursor.Position);
        value |= static_cast<std::uint64_t>(byte & 0x7F) << shift;
        shift += 7;
        if ((byte & 0x80) == 0) {
            if (signedValue && (byte & 0x40) != 0 && shift < 64)
                value |= (~std::uint64_t{0}) << shift;
            return value;
        }
        if (shift >= 64)
            throw CodegenException("Unterminated .eh_frame LEB128 value", cursor.Position);
    }
}

std::size_t UnwindFormatWidth(const std::uint8_t format) {
    switch (format & kEncodingFormatMask) {
    case kFormatAbsPtr:
    case kFormatUdata8:
    case kFormatSdata8:
        return 8;
    case kFormatUdata2:
    case kFormatSdata2:
        return 2;
    case kFormatUdata4:
    case kFormatSdata4:
        return 4;
    default:
        return 0;
    }
}

std::uint64_t ReadUnwindFormat(UnwindCursor& cursor, const std::uint8_t format) {
    switch (format & kEncodingFormatMask) {
    case kFormatAbsPtr:
    case kFormatUdata8:
        return ReadUnwindU64(cursor);
    case kFormatUleb128:
        return ReadUnwindLeb(cursor, false);
    case kFormatUdata2:
        return ReadUnwindU16(cursor);
    case kFormatUdata4:
        return ReadUnwindU32(cursor);
    case kFormatSleb128:
        return ReadUnwindLeb(cursor, true);
    case kFormatSdata2:
        return static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int16_t>(ReadUnwindU16(cursor))));
    case kFormatSdata4:
        return static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(ReadUnwindU32(cursor))));
    case kFormatSdata8:
        return static_cast<std::uint64_t>(static_cast<std::int64_t>(ReadUnwindU64(cursor)));
    default:
        throw CodegenException("Unsupported .eh_frame pointer format", cursor.Position);
    }
}

void StoreUnwindU16(std::span<std::uint8_t> bytes, const std::size_t at, const std::uint16_t value) {
    if (at > bytes.size() || sizeof(value) > bytes.size() - at)
        throw CodegenException(".eh_frame field exceeds the section", at);
    std::memcpy(bytes.data() + at, &value, sizeof(value));
}

void StoreUnwindU32(std::span<std::uint8_t> bytes, const std::size_t at, const std::uint32_t value) {
    if (at > bytes.size() || sizeof(value) > bytes.size() - at)
        throw CodegenException(".eh_frame field exceeds the section", at);
    std::memcpy(bytes.data() + at, &value, sizeof(value));
}

void StoreUnwindU64(std::span<std::uint8_t> bytes, const std::size_t at, const std::uint64_t value) {
    if (at > bytes.size() || sizeof(value) > bytes.size() - at)
        throw CodegenException(".eh_frame field exceeds the section", at);
    std::memcpy(bytes.data() + at, &value, sizeof(value));
}

void StoreUnwindFormat(std::span<std::uint8_t> bytes, const std::size_t at, const std::uint8_t format, const std::int64_t value) {
    switch (format & kEncodingFormatMask) {
    case kFormatAbsPtr:
    case kFormatUdata8:
        if (value < 0)
            throw CodegenException("Negative value in an unsigned .eh_frame pointer", at);
        StoreUnwindU64(bytes, at, static_cast<std::uint64_t>(value));
        return;
    case kFormatUdata2:
        if (value < 0 || value > std::numeric_limits<std::uint16_t>::max())
            throw CodegenException(".eh_frame pointer does not fit its encoding", at);
        StoreUnwindU16(bytes, at, static_cast<std::uint16_t>(value));
        return;
    case kFormatUdata4:
        if (value < 0 || static_cast<std::uint64_t>(value) > std::numeric_limits<std::uint32_t>::max())
            throw CodegenException(".eh_frame pointer does not fit its encoding", at);
        StoreUnwindU32(bytes, at, static_cast<std::uint32_t>(value));
        return;
    case kFormatSdata2:
        if (value < std::numeric_limits<std::int16_t>::min() || value > std::numeric_limits<std::int16_t>::max())
            throw CodegenException(".eh_frame pointer does not fit its encoding", at);
        StoreUnwindU16(bytes, at, static_cast<std::uint16_t>(static_cast<std::int16_t>(value)));
        return;
    case kFormatSdata4:
        if (value < std::numeric_limits<std::int32_t>::min() || value > std::numeric_limits<std::int32_t>::max())
            throw CodegenException(".eh_frame pointer does not fit its encoding", at);
        StoreUnwindU32(bytes, at, static_cast<std::uint32_t>(static_cast<std::int32_t>(value)));
        return;
    case kFormatSdata8:
        StoreUnwindU64(bytes, at, static_cast<std::uint64_t>(value));
        return;
    default:
        throw CodegenException("Cannot re-encode a variable-length .eh_frame pointer", at);
    }
}

std::uint64_t UnwindPointerBase(const std::uint8_t application, const std::uint64_t fieldAddress, const std::uint64_t functionAddress) {
    switch (application) {
    case kApplicationAbsPtr:
        return 0;
    case kApplicationPcRel:
        return fieldAddress;
    case kApplicationFunctionRel:
        return functionAddress;
    default:
        throw CodegenException("Unsupported .eh_frame pointer base");
    }
}

UnwindPointer ReadUnwindPointer(UnwindCursor& cursor, const std::uint8_t encoding, const std::uint64_t fieldAddress, const std::uint64_t functionAddress) {
    const auto format = static_cast<std::uint8_t>(encoding & kEncodingFormatMask);
    const auto value = ReadUnwindFormat(cursor, format);
    if (value == 0)
        return {0, false};
    const auto base = UnwindPointerBase(static_cast<std::uint8_t>(encoding & kEncodingApplicationMask), fieldAddress, functionAddress);
    const auto indirect = (encoding & kEncodingIndirect) != 0;
    if (format == kFormatSleb128 || format == kFormatSdata2 || format == kFormatSdata4 || format == kFormatSdata8) {
        const auto signedValue = static_cast<std::int64_t>(value);
        if (signedValue < 0) {
            const auto magnitude = static_cast<std::uint64_t>(-(signedValue + 1)) + 1;
            if (magnitude > base)
                throw CodegenException(".eh_frame pointer underflows the address space", cursor.Position);
            return {base - magnitude, indirect};
        }
        if (static_cast<std::uint64_t>(signedValue) > std::numeric_limits<std::uint64_t>::max() - base)
            throw CodegenException(".eh_frame pointer overflows the address space", cursor.Position);
        return {base + static_cast<std::uint64_t>(signedValue), indirect};
    }
    if (value > std::numeric_limits<std::uint64_t>::max() - base)
        throw CodegenException(".eh_frame pointer overflows the address space", cursor.Position);
    return {base + value, indirect};
}

void StoreUnwindPointer(std::span<std::uint8_t> bytes, const std::size_t at, const std::uint8_t encoding, const std::int64_t value) {
    if ((encoding & kEncodingIndirect) != 0)
        throw CodegenException("Indirect .eh_frame pointers cannot be adjusted", at);
    StoreUnwindFormat(bytes, at, static_cast<std::uint8_t>(encoding & kEncodingFormatMask), value);
}

CieEncoding ReadCieEncoding(std::span<std::uint8_t> bytes, const std::size_t cieStart) {
    UnwindCursor cursor{bytes, cieStart, bytes.size()};
    const auto length = ReadUnwindU32(cursor);
    if (length == kExtendedRecordLength)
        throw CodegenException("64-bit .eh_frame records are not supported", cieStart);
    if (length < 8 || length > cursor.End - cursor.Position)
        throw CodegenException(".eh_frame CIE exceeds the section", cieStart);
    cursor.End = cursor.Position + length;
    if (ReadUnwindU32(cursor) != 0)
        throw CodegenException(".eh_frame CIE identifier is not zero", cieStart);
    const auto version = ReadUnwindByte(cursor);
    if (version != 1 && version != 3 && version != 4)
        throw CodegenException("Unsupported .eh_frame CIE version", cieStart);
    std::string augmentation;
    for (;;) {
        const auto character = ReadUnwindByte(cursor);
        if (character == 0)
            break;
        augmentation.push_back(static_cast<char>(character));
        if (augmentation.size() > 32)
            throw CodegenException(".eh_frame CIE augmentation is not terminated", cieStart);
    }
    if (version == 4 && (ReadUnwindByte(cursor) != 8 || ReadUnwindByte(cursor) != 0))
        throw CodegenException("Unsupported .eh_frame CIE address size", cieStart);
    (void)ReadUnwindLeb(cursor, false);
    (void)ReadUnwindLeb(cursor, true);
    if (version == 1)
        (void)ReadUnwindByte(cursor);
    else
        (void)ReadUnwindLeb(cursor, false);
    CieEncoding encoding;
    if (augmentation.empty())
        return encoding;
    if (augmentation.front() != 'z')
        throw CodegenException("Unsupported .eh_frame CIE augmentation", cieStart);
    encoding.Augmented = true;
    const auto augmentationLength = ReadUnwindLeb(cursor, false);
    if (augmentationLength > cursor.End - cursor.Position)
        throw CodegenException(".eh_frame CIE augmentation exceeds its record", cieStart);
    const auto augmentationEnd = cursor.Position + static_cast<std::size_t>(augmentationLength);
    for (std::size_t index = 1; index < augmentation.size(); ++index) {
        switch (augmentation[index]) {
        case 'L':
            encoding.LsdaEncoding = ReadUnwindByte(cursor);
            break;
        case 'R':
            encoding.PointerEncoding = ReadUnwindByte(cursor);
            break;
        case 'P': {
            const auto personalityEncoding = ReadUnwindByte(cursor);
            (void)ReadUnwindFormat(cursor, personalityEncoding);
            break;
        }
        case 'S':
            break;
        default:
            throw CodegenException("Unsupported .eh_frame CIE augmentation", cieStart);
        }
    }
    if (cursor.Position != augmentationEnd)
        throw CodegenException(".eh_frame CIE augmentation size mismatch", cieStart);
    return encoding;
}

void RequireCfiLocationsStable(UnwindCursor& cursor, const std::int64_t beginOffset, const AddressMap& map) {
    std::uint64_t location = 0;
    while (cursor.Position < cursor.End) {
        const auto opcode = ReadUnwindByte(cursor);
        std::uint64_t delta = 0;
        bool advances = false;
        if ((opcode & 0xC0) == 0x40) {
            delta = opcode & 0x3F;
            advances = true;
        } else if ((opcode & 0xC0) == 0x80) {
            (void)ReadUnwindLeb(cursor, false);
        } else if ((opcode & 0xC0) != 0xC0) {
            switch (opcode) {
            case 0x00:
            case 0x2D:
                break;
            case 0x01:
                location = ReadUnwindLeb(cursor, false);
                break;
            case 0x02:
                delta = ReadUnwindByte(cursor);
                advances = true;
                break;
            case 0x03:
                delta = ReadUnwindU16(cursor);
                advances = true;
                break;
            case 0x04:
                delta = ReadUnwindU32(cursor);
                advances = true;
                break;
            case 0x05:
            case 0x09:
            case 0x0C:
            case 0x14:
            case 0x2F:
                (void)ReadUnwindLeb(cursor, false);
                (void)ReadUnwindLeb(cursor, false);
                break;
            case 0x06:
            case 0x07:
            case 0x08:
            case 0x0D:
            case 0x0E:
            case 0x2E:
                (void)ReadUnwindLeb(cursor, false);
                break;
            case 0x0F:
            case 0x10:
            case 0x16: {
                if (opcode != 0x0F)
                    (void)ReadUnwindLeb(cursor, false);
                const auto length = ReadUnwindLeb(cursor, false);
                if (length > cursor.End - cursor.Position)
                    throw CodegenException(".eh_frame CFI expression exceeds its record", cursor.Position);
                cursor.Position += static_cast<std::size_t>(length);
                break;
            }
            case 0x11:
            case 0x12:
            case 0x15:
                (void)ReadUnwindLeb(cursor, false);
                (void)ReadUnwindLeb(cursor, true);
                break;
            case 0x13:
                (void)ReadUnwindLeb(cursor, true);
                break;
            default:
                throw CodegenException("Unsupported .eh_frame CFI instruction", cursor.Position);
            }
        }
        if (advances) {
            const auto rowStart = beginOffset + static_cast<std::int64_t>(location);
            const auto rowEnd = rowStart + static_cast<std::int64_t>(delta);
            if (map.MapAddress(rowEnd) - map.MapAddress(rowStart) != static_cast<std::int64_t>(delta))
                throw CodegenException(".eh_frame CFI location crosses a rewritten instruction", cursor.Position);
            location += delta;
        }
    }
}

void PatchFde(
    std::span<std::uint8_t> ehFrame,
    const std::size_t fieldsStart,
    const std::size_t recordEnd,
    const CieEncoding& cie,
    const Domain::VirtualAddress ehFrameAddress,
    const CodeExtent& code,
    const AddressMap& map
) {
    const auto application = static_cast<std::uint8_t>(cie.PointerEncoding & kEncodingApplicationMask);
    if (application == kApplicationFunctionRel)
        throw CodegenException(".eh_frame FDE start is relative to its own function", fieldsStart);
    UnwindCursor cursor{ehFrame, fieldsStart, recordEnd};
    const auto beginField = cursor.Position;
    const auto beginFieldAddress = ehFrameAddress + beginField;
    if (InCode(beginFieldAddress, code))
        throw CodegenException("Unwind record lies inside the rewritten code", beginField);
    const auto begin = ReadUnwindPointer(cursor, cie.PointerEncoding, beginFieldAddress, 0);
    const auto rangeField = cursor.Position;
    const auto rangeFormat = static_cast<std::uint8_t>(cie.PointerEncoding & kEncodingFormatMask);
    const auto range = ReadUnwindFormat(cursor, rangeFormat);
    if (begin.Address == 0 || range == 0)
        return;
    if (begin.Indirect)
        throw CodegenException("Indirect .eh_frame pointers cannot be adjusted", beginField);
    const auto beginOffset = OffsetInCode(begin.Address, code);
    if (!beginOffset)
        return;
    if (range > std::numeric_limits<std::uint64_t>::max() - begin.Address)
        throw CodegenException(".eh_frame FDE range overflows the address space", beginField);
    const auto oldEnd = begin.Address + range;
    if (oldEnd != code.Address + code.Size && !OffsetInCode(oldEnd, code))
        throw CodegenException(".eh_frame FDE extends past the rewritten code", beginField);
    const auto newBegin = AddressOf(map.MapAddress(*beginOffset), code);
    const auto newEnd = MapCodeEnd(oldEnd, code, map);
    if (newEnd < newBegin)
        throw CodegenException(".eh_frame FDE range shrinks below zero", beginField);
    if (newBegin != begin.Address) {
        const auto value = application == kApplicationPcRel
            ? SignedDifference(newBegin, beginFieldAddress)
            : static_cast<std::int64_t>(newBegin);
        StoreUnwindPointer(ehFrame, beginField, cie.PointerEncoding, value);
    }
    const auto newRange = newEnd - newBegin;
    if (newRange != range)
        StoreUnwindFormat(ehFrame, rangeField, rangeFormat, static_cast<std::int64_t>(newRange));
    if (cie.Augmented) {
        const auto augmentationLength = ReadUnwindLeb(cursor, false);
        if (augmentationLength > cursor.End - cursor.Position)
            throw CodegenException(".eh_frame FDE augmentation exceeds its record", cursor.Position);
        if (cie.LsdaEncoding == kEncodingOmit) {
            if (augmentationLength != 0)
                throw CodegenException("Unsupported .eh_frame FDE augmentation", cursor.Position);
        } else {
            const auto lsdaField = cursor.Position;
            const auto lsdaFieldAddress = ehFrameAddress + lsdaField;
            const auto lsda = ReadUnwindPointer(cursor, cie.LsdaEncoding, lsdaFieldAddress, begin.Address);
            if (lsda.Address != 0) {
                if (lsda.Indirect)
                    throw CodegenException("Indirect .eh_frame pointers cannot be adjusted", lsdaField);
                const auto lsdaApplication = static_cast<std::uint8_t>(cie.LsdaEncoding & kEncodingApplicationMask);
                auto mapped = lsda.Address;
                if (const auto lsdaOffset = OffsetInCode(lsda.Address, code))
                    mapped = AddressOf(map.MapAddress(*lsdaOffset), code);
                const auto oldValue = lsdaApplication == kApplicationPcRel
                    ? SignedDifference(lsda.Address, lsdaFieldAddress)
                    : lsdaApplication == kApplicationFunctionRel
                        ? SignedDifference(lsda.Address, begin.Address)
                        : static_cast<std::int64_t>(lsda.Address);
                const auto newValue = lsdaApplication == kApplicationPcRel
                    ? SignedDifference(mapped, lsdaFieldAddress)
                    : lsdaApplication == kApplicationFunctionRel
                        ? SignedDifference(mapped, newBegin)
                        : static_cast<std::int64_t>(mapped);
                if (newValue != oldValue)
                    StoreUnwindPointer(ehFrame, lsdaField, cie.LsdaEncoding, newValue);
            }
        }
    }
    RequireCfiLocationsStable(cursor, *beginOffset, map);
}

}

void FixupCodeRelocations(std::span<CodeRelocation> relocations, const CodeExtent& code, const AddressMap& map) {
    if (map.IsIdentity())
        return;
    for (auto& relocation : relocations) {
        relocation.Offset = MapCodeAddress(relocation.Offset, code, map);
        if (relocation.AddendKind != RelocationAddendKind::CodeAddress || relocation.Addend < 0)
            continue;
        const auto addend = MapCodeAddress(static_cast<Domain::VirtualAddress>(relocation.Addend), code, map);
        relocation.Addend = static_cast<std::int64_t>(addend);
    }
}

void FixupCodeSymbols(std::span<CodeSymbol> symbols, const CodeExtent& code, const AddressMap& map) {
    if (map.IsIdentity())
        return;
    for (auto& symbol : symbols)
        symbol.Value = MapCodeAddress(symbol.Value, code, map);
}

void FixupRelativeJumpTable(const RelativeJumpTableRef& table, const CodeExtent& code, const AddressMap& map) {
    if (map.IsIdentity())
        return;
    const auto base = MapCodeAddress(table.Address, code, map);
    for (auto& entry : table.Entries) {
        const auto target = static_cast<std::int64_t>(table.Address) + static_cast<std::int64_t>(entry);
        if (target < 0)
            throw CodegenException("Relative jump table entry points before the address space", table.Address);
        const auto mapped = MapCodeAddress(static_cast<Domain::VirtualAddress>(target), code, map);
        const auto displacement = static_cast<std::int64_t>(mapped) - static_cast<std::int64_t>(base);
        if (displacement < std::numeric_limits<std::int32_t>::min() || displacement > std::numeric_limits<std::int32_t>::max())
            throw CodegenException("Relative jump table displacement overflows after rewrite", table.Address);
        entry = static_cast<std::int32_t>(displacement);
    }
}

void FixupEhFrame(std::span<std::uint8_t> ehFrame, const Domain::VirtualAddress ehFrameAddress, const CodeExtent& code, const AddressMap& map) {
    if (map.IsIdentity() || ehFrame.empty())
        return;
    std::vector<CieRecord> cies;
    std::size_t position = 0;
    while (position + 4 <= ehFrame.size()) {
        const auto lengthField = position;
        std::uint32_t length = 0;
        std::memcpy(&length, ehFrame.data() + position, sizeof(length));
        position += 4;
        if (length == 0)
            continue;
        if (length == kExtendedRecordLength)
            throw CodegenException("64-bit .eh_frame records are not supported", lengthField);
        if (length < 4 || length > ehFrame.size() - position)
            throw CodegenException(".eh_frame record exceeds the section", lengthField);
        const auto recordEnd = position + length;
        const auto cieField = position;
        std::uint32_t cieOffset = 0;
        std::memcpy(&cieOffset, ehFrame.data() + position, sizeof(cieOffset));
        position += 4;
        if (cieOffset == 0) {
            position = recordEnd;
            continue;
        }
        if (cieOffset > cieField)
            throw CodegenException(".eh_frame FDE CIE offset is out of range", cieField);
        const auto cieStart = cieField - cieOffset;
        std::optional<CieEncoding> encoding;
        for (const auto& record : cies) {
            if (record.Start == cieStart)
                encoding = record.Encoding;
        }
        if (!encoding) {
            encoding = ReadCieEncoding(ehFrame, cieStart);
            cies.push_back({cieStart, *encoding});
        }
        PatchFde(ehFrame, position, recordEnd, *encoding, ehFrameAddress, code, map);
        position = recordEnd;
    }
}

void ApplyLengthChangingFixups(const LengthChangingFixups& fixups, const AddressMap& map) {
    if (map.IsIdentity())
        return;
    FixupCodeRelocations(fixups.Relocations, fixups.Code, map);
    FixupCodeSymbols(fixups.Symbols, fixups.Code, map);
    for (const auto& table : fixups.JumpTables)
        FixupRelativeJumpTable(table, fixups.Code, map);
    if (!fixups.EhFrame.empty())
        FixupEhFrame(fixups.EhFrame, fixups.EhFrameAddress, fixups.Code, map);
}

}