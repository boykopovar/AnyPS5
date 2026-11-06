#include <relinker/analysis/ExceptionTableParser.hpp>
#include <cstring>
#include <limits>
#include <set>

namespace Relinker {

namespace {

template <typename TValue>
TValue ReadValue(std::span<const std::uint8_t> bytes, VirtualAddress& pos) {
    if (pos > bytes.size() || sizeof(TValue) > bytes.size() - pos)
        throw RelinkerException("Strict filter: unwind data exceeds file", pos);
    TValue value{};
    std::memcpy(&value, bytes.data() + pos, sizeof(TValue));
    pos += sizeof(TValue);
    return value;
}

std::uint64_t ReadUleb(std::span<const std::uint8_t> bytes, VirtualAddress& pos) {
    std::uint64_t value = 0;
    unsigned shift = 0;
    while (pos < bytes.size()) {
        const auto byte = bytes[pos++];
        if (shift == 63 && (byte & 0x7F) > 1)
            throw RelinkerException("Strict filter: overflowing LEB128 value", pos);
        value |= static_cast<std::uint64_t>(byte & 0x7F) << shift;
        shift += 7;
        if ((byte & 0x80) == 0) return value;
        if (shift >= 64) throw RelinkerException("Strict filter: unterminated LEB128 value", pos);
    }
    throw RelinkerException("Strict filter: unwind data exceeds file", pos);
}

std::int64_t ReadSleb(std::span<const std::uint8_t> bytes, VirtualAddress& pos) {
    std::uint64_t value = 0;
    unsigned shift = 0;
    while (pos < bytes.size()) {
        const auto byte = bytes[pos++];
        if (shift == 63 && (byte & 0x7F) != 0 && (byte & 0x7F) != 0x7F)
            throw RelinkerException("Strict filter: overflowing LEB128 value", pos);
        value |= static_cast<std::uint64_t>(byte & 0x7F) << shift;
        shift += 7;
        if ((byte & 0x80) == 0) {
            if ((byte & 0x40) != 0 && shift < 64) value |= (~std::uint64_t{0}) << shift;
            return static_cast<std::int64_t>(value);
        }
        if (shift >= 64) throw RelinkerException("Strict filter: unterminated LEB128 value", pos);
    }
    throw RelinkerException("Strict filter: unwind data exceeds file", pos);
}

std::uint64_t ReadEncoded(std::span<const std::uint8_t> bytes, VirtualAddress& pos, std::uint8_t encoding, VirtualAddress functionBegin) {
    const auto field = pos;
    std::uint64_t value = 0;
    switch (encoding & 0x0F) {
        case 0: value = ReadValue<std::uint64_t>(bytes, pos); break;
        case 1: value = ReadUleb(bytes, pos); break;
        case 2: value = ReadValue<std::uint16_t>(bytes, pos); break;
        case 3: value = ReadValue<std::uint32_t>(bytes, pos); break;
        case 4: value = ReadValue<std::uint64_t>(bytes, pos); break;
        case 9: value = static_cast<std::uint64_t>(ReadSleb(bytes, pos)); break;
        case 10: value = static_cast<std::uint64_t>(static_cast<std::int64_t>(ReadValue<std::int16_t>(bytes, pos))); break;
        case 11: value = static_cast<std::uint64_t>(static_cast<std::int64_t>(ReadValue<std::int32_t>(bytes, pos))); break;
        case 12: value = static_cast<std::uint64_t>(ReadValue<std::int64_t>(bytes, pos)); break;
        default: throw RelinkerException("Strict filter: unsupported unwind pointer format", field);
    }
    if (value == 0) return 0;
    VirtualAddress base = 0;
    switch (encoding & 0x70) {
        case 0: break;
        case 0x10: base = field; break;
        case 0x40: base = functionBegin; break;
        default: throw RelinkerException("Strict filter: unsupported unwind pointer base", field);
    }
    if ((encoding & 0x70) != 0) {
        if ((encoding & 0x08) != 0) {
            const bool negative = (value >> 63) != 0;
            if (negative ? base < std::uint64_t{0} - value : value > std::numeric_limits<VirtualAddress>::max() - base)
                throw RelinkerException("Strict filter: unwind pointer address overflowed", field);
        } else if ((encoding & 0x0F) == 2 || (encoding & 0x0F) == 3 || ((encoding & 0x0F) == 4 && (encoding & 0x70) == 0x40)) {
            if (value > std::numeric_limits<VirtualAddress>::max() - base)
                throw RelinkerException("Strict filter: unwind pointer address overflowed", field);
        }
        value = base + value;
    }
    return value;
}

}

ParsedExceptionTable ExceptionTableParser::Parse(std::span<const std::uint8_t> bytes, VirtualAddress lsdaOffset, VirtualAddress functionBegin) {
    if (lsdaOffset >= bytes.size())
        throw RelinkerException("Strict filter: unwind data exceeds file", lsdaOffset);
    auto position = lsdaOffset;
    ParsedExceptionTable table{};
    table.LandingEncoding = ReadValue<std::uint8_t>(bytes, position);
    table.LandingBase = table.LandingEncoding == 0xFF ? functionBegin : ReadEncoded(bytes, position, table.LandingEncoding, functionBegin);
    table.TypeEncoding = ReadValue<std::uint8_t>(bytes, position);
    if (table.TypeEncoding != 0xFF) {
        const auto typeTableOffset = ReadUleb(bytes, position);
        if (typeTableOffset > std::numeric_limits<VirtualAddress>::max() - position)
            throw RelinkerException("Strict filter: unwind pointer address overflowed", position);
        table.TypeTable = position + typeTableOffset;
        if (table.TypeTable > bytes.size())
            throw RelinkerException("Strict filter: unwind data exceeds file", position);
    }
    table.CallEncoding = ReadValue<std::uint8_t>(bytes, position);
    if ((table.CallEncoding & 0xF0) != 0)
        throw RelinkerException("Strict filter: unsupported LSDA call-site encoding", lsdaOffset);
    const auto tableSize = ReadUleb(bytes, position);
    if (tableSize > bytes.size() || position > bytes.size() - tableSize)
        throw RelinkerException("Strict filter: invalid LSDA table size", lsdaOffset);
    table.CallSiteTable = position;
    const auto tableEnd = position + tableSize;
    table.ActionTable = tableEnd;

    while (position < tableEnd) {
        const auto start = ReadEncoded(bytes, position, table.CallEncoding, 0);
        const auto length = ReadEncoded(bytes, position, table.CallEncoding, 0);
        auto landing = ReadEncoded(bytes, position, table.CallEncoding, 0);
        const auto action = ReadUleb(bytes, position);
        if (position > tableEnd)
            throw RelinkerException("Strict filter: LSDA call-site table size mismatch", lsdaOffset);
        if (landing != 0) {
            const bool negative = (table.CallEncoding & 0x08) != 0 && (landing >> 63) != 0;
            if (negative ? table.LandingBase < std::uint64_t{0} - landing : landing > std::numeric_limits<VirtualAddress>::max() - table.LandingBase)
                throw RelinkerException("Strict filter: LSDA landing pad address overflowed", lsdaOffset);
            landing = table.LandingBase + landing;
        }
        CallSiteDescriptor desc{};
        desc.Start = start;
        desc.Length = length;
        desc.LandingPad = landing;
        desc.ActionOffset = action;
        table.CallSites.push_back(desc);
    }
    if (position != tableEnd)
        throw RelinkerException("Strict filter: LSDA call-site table size mismatch", lsdaOffset);

    for (auto& callSite : table.CallSites) {
        if (callSite.ActionOffset == 0) continue;
        const auto actionOffset = callSite.ActionOffset - 1;
        if (actionOffset > bytes.size() || table.ActionTable > bytes.size() - actionOffset || (table.TypeTable > table.ActionTable && table.ActionTable + actionOffset >= table.TypeTable))
            throw RelinkerException("Strict filter: LSDA action record exceeds table", table.ActionTable);
        auto actionPos = table.ActionTable + actionOffset;
        std::set<VirtualAddress> visited;
        while (true) {
            if (actionPos >= bytes.size() || (table.TypeTable > table.ActionTable && actionPos >= table.TypeTable))
                throw RelinkerException("Strict filter: LSDA action record exceeds table", actionPos);
            if (!visited.insert(actionPos).second)
                throw RelinkerException("Strict filter: cyclic LSDA action record chain", actionPos);
            const auto recordAddress = actionPos;
            const auto filter = ReadSleb(bytes, actionPos);
            const auto nextFieldPos = actionPos;
            const auto displacement = ReadSleb(bytes, actionPos);
            ActionRecord record{};
            record.Filter = filter;
            record.NextDisplacement = displacement;
            record.RecordAddress = recordAddress;
            if (filter == 0) {
                callSite.LinksToCatchAll = true;
            } else if (filter > 0 && table.TypeEncoding != 0xFF && table.TypeTable != 0) {
                std::size_t entrySize = 0;
                switch (table.TypeEncoding & 0x0F) {
                    case 0:
                    case 4:
                    case 12: entrySize = 8; break;
                    case 2:
                    case 10: entrySize = 2; break;
                    case 3:
                    case 11: entrySize = 4; break;
                    default: break;
                }
                if (entrySize > 0) {
                    const auto typeOffset = static_cast<VirtualAddress>(filter) * entrySize;
                    if (table.TypeTable >= typeOffset && table.TypeTable - typeOffset + entrySize <= bytes.size()) {
                        auto typePos = table.TypeTable - typeOffset;
                        const auto typeVal = ReadEncoded(bytes, typePos, table.TypeEncoding, 0);
                        if (typeVal == 0) callSite.LinksToCatchAll = true;
                    }
                }
            }
            if (displacement == 0) {
                record.NextRecordAddress = 0;
                callSite.ActionChain.push_back(record);
                break;
            }
            if (displacement < 0 && (nextFieldPos < static_cast<VirtualAddress>(-displacement) || nextFieldPos + static_cast<VirtualAddress>(displacement) < table.ActionTable))
                throw RelinkerException("Strict filter: LSDA action record address underflow", nextFieldPos);
            if (displacement > 0 && static_cast<VirtualAddress>(displacement) > std::numeric_limits<VirtualAddress>::max() - nextFieldPos)
                throw RelinkerException("Strict filter: LSDA action record address overflow", nextFieldPos);
            const auto nextRecordAddress = nextFieldPos + static_cast<VirtualAddress>(displacement);
            record.NextRecordAddress = nextRecordAddress;
            callSite.ActionChain.push_back(record);
            actionPos = nextRecordAddress;
        }
    }

    return table;
}

ParsedExceptionTable ExceptionTableParser::Parse(const std::vector<std::uint8_t>& bytes, const std::vector<ProgramHeader>& headers, VirtualAddress lsdaAddress, VirtualAddress functionBegin) {
    for (const auto& header : headers) {
        if (header.Type != 1 || lsdaAddress < header.MappedAddress || lsdaAddress - header.MappedAddress >= header.FileSize)
            continue;
        const auto relative = lsdaAddress - header.MappedAddress;
        if (header.Offset > bytes.size() || relative > bytes.size() - header.Offset)
            throw RelinkerException("Strict filter: unwind data exceeds file", lsdaAddress);
        const auto fileOffset = header.Offset + relative;
        return Parse(std::span<const std::uint8_t>(bytes.data(), bytes.size()), fileOffset, functionBegin);
    }
    throw RelinkerException("Strict filter: unwind address is not file-backed", lsdaAddress);
}

}
