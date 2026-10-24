#include <relinker/analysis/CodeInstructionCollector.hpp>
#include <relinker/analysis/UnusedNidFilter/IControlFlowGraph.hpp>
#include <relinker/analysis/UnusedNidFilter/EhFrameReader.hpp>
#include <relinker/analysis/SceCodeExtent.hpp>
#include <relinker/analysis/RelativeJumpTables.hpp>
#include <codegen/IInstructionScanner.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <codegen/CodegenException.hpp>
#include <elfpatcher/general/ElfConstants.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <bit>
#include <iterator>
#include <limits>
#include <map>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace Relinker {

namespace {

class CodePointers : public UnusedNidFilter::IRelativeRelocationIndex {
public:
    std::map<Domain::VirtualAddress, Domain::VirtualAddress> Values;
    std::optional<Domain::VirtualAddress> TargetOfSlot(Domain::VirtualAddress address) const override {
        const auto found = Values.find(address);
        if (found == Values.end()) return std::nullopt;
        return found->second;
    }
    std::vector<Domain::VirtualAddress> Targets() const override {
        std::vector<Domain::VirtualAddress> result;
        for (const auto& [slot, target] : Values) result.push_back(target);
        return result;
    }
};

}

std::set<Domain::VirtualAddress> CodeInstructionCollector::Collect(const std::vector<std::uint8_t>& bytes, const std::vector<Domain::ProgramHeader>& headers, const std::function<void(std::uint64_t, std::uint64_t)>& progress, std::set<Domain::VirtualAddress>* entryRoots, const std::function<void(std::string_view)>& message) const {
    std::uint64_t lastProgress = 0;
    const auto report = [&](std::uint64_t completed) {
        if (completed <= lastProgress) return;
        lastProgress = completed;
        if (progress) progress(completed, 10000);
    };
    const auto range = [&](std::uint64_t offset, std::uint64_t size) {
        if (offset > bytes.size() || size > bytes.size() - offset) throw Domain::RelinkerException("Code analysis: file range exceeds image", offset);
    };
    const auto fileOffset = [&](std::uint64_t address, std::uint64_t size) {
        for (const auto& header : headers) {
            if (header.Type != 1 || address < header.MappedAddress || address - header.MappedAddress > header.FileSize || size > header.FileSize - (address - header.MappedAddress)) continue;
            const auto offset = header.Offset + address - header.MappedAddress;
            range(offset, size);
            return offset;
        }
        throw Domain::RelinkerException("Code analysis: unmapped address", address);
    };
    auto codeHeaders = headers;
    const auto isCode = [&](std::uint64_t address) {
        for (const auto& header : codeHeaders) if (header.Type == 1 && (header.Flags & 1) != 0 && address >= header.MappedAddress && address - header.MappedAddress < header.FileSize) return true;
        return false;
    };
    std::map<std::uint64_t, std::uint64_t> tags;
    std::optional<std::uint64_t> sceBase;
    bool hasDynamic = false;
    for (const auto& header : headers) {
        if (header.Type == 0x61000000) sceBase = header.Offset;
        if (header.Type != 2) continue;
        if (hasDynamic || header.FileSize % 16 != 0) throw Domain::RelinkerException("Code analysis: invalid dynamic segment");
        hasDynamic = true;
        range(header.Offset, header.FileSize);
        bool terminated = false;
        for (std::uint64_t offset = header.Offset; offset < header.Offset + header.FileSize; offset += 16) {
            const auto tag = Io::ReadU64(bytes, offset);
            if (tag == 0) { terminated = true; break; }
            const auto value = Io::ReadU64(bytes, offset + 8);
            if (tag == 1 || (tag >= 0x61000000 && tag != 0x6ffffef5 && tag != 0x6100003f && (tag < 0x61000027 || tag > 0x6100003b))) continue;
            if (!tags.emplace(tag, value).second) throw Domain::RelinkerException("Code analysis: duplicate dynamic tag", offset);
        }
        if (!terminated) throw Domain::RelinkerException("Code analysis: unterminated dynamic segment");
    }
    const auto value = [&](std::uint64_t standard, std::uint64_t sce) -> std::optional<std::uint64_t> {
        if (tags.contains(standard) && tags.contains(sce)) throw Domain::RelinkerException("Code analysis: ambiguous dynamic tag");
        if (tags.contains(standard)) return tags.at(standard);
        if (tags.contains(sce)) return tags.at(sce);
        return std::nullopt;
    };
    const auto table = [&](std::uint64_t standard, std::uint64_t sce, std::uint64_t size) {
        const auto address = value(standard, sce);
        if (!address) throw Domain::RelinkerException("Code analysis: missing table");
        if (tags.contains(standard)) return fileOffset(*address, size);
        if (!sceBase || *address > std::numeric_limits<std::uint64_t>::max() - *sceBase) throw Domain::RelinkerException("Code analysis: invalid SCE table");
        const auto offset = *sceBase + *address;
        range(offset, size);
        return offset;
    };
    std::vector<Domain::CodeRange> sceFunctions;
    const auto trimToCode = [&](Domain::ProgramHeader& header, std::uint64_t end, bool interpreterAnywhere) {
        const std::span<const std::uint8_t> segment(bytes.data() + header.Offset, header.FileSize);
        auto begin = header.MappedAddress;
        for (const auto& interpreter : headers)
            if (interpreter.Type == 3 && interpreter.MappedAddress < end &&
                (interpreterAnywhere ? interpreter.MappedAddress >= begin : interpreter.MappedAddress == begin))
                begin = interpreter.MappedAddress + std::min(interpreter.FileSize, end - interpreter.MappedAddress);
        const std::string_view loaderPath("/libexec/ld-elf.so.1", 21);
        if (begin == header.MappedAddress && end - begin > loaderPath.size() && std::equal(loaderPath.begin(), loaderPath.end(), segment.begin())) begin += loaderPath.size();
        header.Offset += begin - header.MappedAddress;
        header.MappedAddress = begin;
        header.FileSize = end - begin;
    };
    if (sceBase) {
        const auto unwindFunctions = UnusedNidFilter::ReadExceptionFunctions(bytes, headers, {}, {});
        for (const auto& function : unwindFunctions) sceFunctions.push_back({function.Begin, function.End});
        sceFunctions = Codegen::MergeCodeRanges(std::move(sceFunctions));
        for (auto& header : codeHeaders) {
            if (header.Type != 1 || (header.Flags & 1) == 0) continue;
            range(header.Offset, header.FileSize);
            const std::span<const std::uint8_t> segment(bytes.data() + header.Offset, header.FileSize);
            trimToCode(header, FindSceCodeEnd(segment, header.MappedAddress, unwindFunctions, value(3, 0x61000027).value_or(0), value(2, 0x6100002d).value_or(0)).value_or(header.MappedAddress + header.FileSize), true);
        }
    } else {
        for (auto& header : codeHeaders) {
            if (header.Type != 1 || (header.Flags & 1) == 0) continue;
            trimToCode(header, header.MappedAddress + header.FileSize, false);
        }
    }
    std::set<std::uint64_t> roots;
    std::map<std::uint64_t, std::uint64_t> functions;
    std::map<std::uint64_t, std::uint64_t> tables;
    const auto inTable = [&](std::uint64_t address) { const auto after = tables.upper_bound(address); return after != tables.begin() && address < std::prev(after)->second; };
    const auto addRoot = [&](std::uint64_t address) { if (isCode(address) && !inTable(address)) roots.insert(address); };
    const auto sceFunctionAfter = [&](std::uint64_t address) {
        return std::upper_bound(sceFunctions.begin(), sceFunctions.end(), address, [](std::uint64_t value, const Domain::CodeRange& function) { return value < function.Begin; });
    };
    const auto insideSceFunction = [&](std::uint64_t address) {
        const auto after = sceFunctionAfter(address);
        return after != sceFunctions.begin() && address < std::prev(after)->End;
    };
    const auto nearSceFunctionEnd = [&](std::uint64_t address) {
        const auto after = sceFunctionAfter(address);
        if (after == sceFunctions.begin()) return false;
        const auto end = std::prev(after)->End;
        return address >= end && address - end < 256;
    };
    const auto codeAt = [&](std::uint64_t address) {
        for (const auto& header : codeHeaders)
            if (header.Type == 1 && (header.Flags & 1) != 0 && address >= header.MappedAddress && address - header.MappedAddress < header.FileSize)
                return std::span<const std::uint8_t>(bytes.data() + header.Offset + (address - header.MappedAddress), header.FileSize - (address - header.MappedAddress));
        return std::span<const std::uint8_t>();
    };
    const auto absoluteFsAccess = [](const std::uint8_t* code, const Codegen::DecodedInstructionInfo& info) {
        return info.SegmentPrefix == 0x64 && info.OpcodeOffset + 2 < info.Length && (code[info.OpcodeOffset + 1] & 0xc7) == 0x04 && code[info.OpcodeOffset + 2] == 0x25;
    };
    struct GapRun {
        bool Transfer = false;
        bool AbsoluteFsAccess = false;
    };
    const auto decodeGapRun = [&](std::uint64_t address) {
        const auto after = sceFunctionAfter(address);
        const auto limit = after == sceFunctions.end() ? std::numeric_limits<std::uint64_t>::max() : after->Begin;
        const Codegen::X64InstructionDecoder probe;
        GapRun run;
        for (std::size_t count = 0; count < 256 && address < limit; ++count) {
            const auto code = codeAt(address);
            if (code.empty()) return run;
            const auto available = std::min<std::uint64_t>(code.size(), limit - address);
            Codegen::DecodedInstructionInfo info;
            try {
                info = probe.DecodeInstruction(code.data(), available);
            } catch (const Codegen::CodegenException&) {
                return run;
            }
            if (info.Length == 0 || info.Length > available) return run;
            if (absoluteFsAccess(code.data(), info)) run.AbsoluteFsAccess = true;
            switch (info.FlowKind) {
                case Codegen::ControlFlowKind::UnconditionalJump:
                case Codegen::ControlFlowKind::Return:
                case Codegen::ControlFlowKind::IndirectJump:
                case Codegen::ControlFlowKind::Trap:
                    run.Transfer = true;
                    return run;
                default:
                    break;
            }
            address += info.Length;
        }
        return run;
    };
    std::set<std::uint64_t> dataTargets;
    const auto addPointerRoot = [&](std::uint64_t address) {
        if (sceFunctions.empty() || !isCode(address) || insideSceFunction(address)) {
            addRoot(address);
            return;
        }
        const auto run = decodeGapRun(address);
        if (run.AbsoluteFsAccess || (address % 16 == 0 && nearSceFunctionEnd(address) && run.Transfer)) addRoot(address);
        else dataTargets.insert(address);
    };
    const auto addAddressTakenRoot = [&](std::uint64_t address, const std::uint8_t* code, const Codegen::DecodedInstructionInfo& info) {
        if (!info.HasRipRelativeDisp || code[info.OpcodeOffset] != 0x8d || !(info.RexPrefix & 8)) return;
        const auto displacement = std::bit_cast<std::int32_t>(Io::ReadU32(bytes, fileOffset(address, info.Length) + info.RipRelativeDispOffset));
        addPointerRoot(address + info.Length + static_cast<std::int64_t>(displacement));
    };
    const auto addFunction = [&](std::uint64_t begin, std::uint64_t size, bool symbolAlias) {
        if (size == 0) return;
        if (!isCode(begin) || size > std::numeric_limits<std::uint64_t>::max() - begin)
            throw Domain::RelinkerException("Code analysis: invalid function range", begin);
        bool mapped = false;
        for (const auto& header : headers) {
            if (header.Type != 1 || (header.Flags & 1) == 0 || begin < header.MappedAddress || begin - header.MappedAddress >= header.FileSize) continue;
            if (size > header.FileSize - (begin - header.MappedAddress))
                throw Domain::RelinkerException("Code analysis: function exceeds executable segment", begin);
            range(header.Offset + begin - header.MappedAddress, size);
            mapped = true;
            break;
        }
        if (!mapped) throw Domain::RelinkerException("Code analysis: function is not file-backed", begin);
        const auto [position, inserted] = functions.emplace(begin, begin + size);
        if (!inserted && !symbolAlias && position->second != begin + size)
            throw Domain::RelinkerException("Code analysis: conflicting function ranges", begin);
        position->second = std::max(position->second, begin + size);
        roots.insert(begin);
    };
    range(0, 64);
    if (const auto entry = Io::ReadU64(bytes, 24); entry != 0) addRoot(entry);
    if (const auto init = value(12, 0x6000000c)) addRoot(*init);
    if (const auto fini = value(13, 0x6000000d)) addRoot(*fini);
    const auto symbolBytes = value(0x6100003f, 0xffffffffffffffffull);
    std::uint64_t symbolCount = 0;
    if (symbolBytes) {
        if (*symbolBytes % 24 != 0) throw Domain::RelinkerException("Code analysis: invalid symbol table size");
        symbolCount = *symbolBytes / 24;
    } else if (tags.contains(4)) symbolCount = Io::ReadU32(bytes, fileOffset(tags.at(4), 8) + 4);
    else if (tags.contains(0x6ffffef5)) {
        const auto address = tags.at(0x6ffffef5);
        const auto header = fileOffset(address, 16);
        const auto bucketCount = Io::ReadU32(bytes, header);
        const auto firstSymbol = Io::ReadU32(bytes, header + 4);
        const auto bloomCount = Io::ReadU32(bytes, header + 8);
        if (bucketCount == 0 || bloomCount == 0 || firstSymbol == 0)
            throw Domain::RelinkerException("Code analysis: invalid GNU hash header", address);
        const std::uint64_t bucketOffset = 16 + static_cast<std::uint64_t>(bloomCount) * 8;
        const auto chainOffset = bucketOffset + static_cast<std::uint64_t>(bucketCount) * 4;
        const auto start = fileOffset(address, chainOffset);
        std::uint64_t lastBucket = 0;
        for (std::uint64_t index = 0; index < bucketCount; ++index) {
            const auto bucket = Io::ReadU32(bytes, start + bucketOffset + index * 4);
            if (bucket != 0 && bucket < firstSymbol)
                throw Domain::RelinkerException("Code analysis: invalid GNU hash bucket", address);
            lastBucket = std::max(lastBucket, static_cast<std::uint64_t>(bucket));
        }
        symbolCount = firstSymbol;
        if (lastBucket != 0) {
            symbolCount = lastBucket;
            for (;;) {
                const auto offset = chainOffset + (symbolCount - firstSymbol) * 4;
                if (offset > std::numeric_limits<std::uint64_t>::max() - address)
                    throw Domain::RelinkerException("Code analysis: invalid GNU hash chain", address);
                const auto chain = Io::ReadU32(bytes, fileOffset(address + offset, 4));
                ++symbolCount;
                if (chain & 1) break;
            }
        }
    }
    std::uint64_t symbols = 0;
    if (symbolCount != 0) {
        if (value(11, 0x6100003b) != 24) throw Domain::RelinkerException("Code analysis: invalid symbol entry size");
        symbols = table(6, 0x61000039, symbolCount * 24);
        for (std::uint64_t index = 0; index < symbolCount; ++index) {
            const auto offset = symbols + index * 24;
            const auto section = Io::ReadU16(bytes, offset + 6);
            if ((bytes[offset + 4] & 15) == 2 && section != 0 && section != Elfpatcher::SHN_ABS) {
                const auto address = Io::ReadU64(bytes, offset + 8);
                addRoot(address);
                addFunction(address, Io::ReadU64(bytes, offset + 16), true);
            }
        }
    }
    CodePointers pointers;
    std::set<std::uint64_t> importSlots;
    const auto relocationRange = [&](std::uint64_t addressTag, std::uint64_t sceAddressTag, std::uint64_t sizeTag, std::uint64_t sceSizeTag) -> std::pair<std::uint64_t, std::uint64_t> {
        const auto size = value(sizeTag, sceSizeTag);
        if (!size) return {0, 0};
        if (*size % 24 != 0) throw Domain::RelinkerException("Code analysis: invalid relocation table size");
        if (*size == 0) return {0, 0};
        return {table(addressTag, sceAddressTag, *size), *size};
    };
    const auto [relaOffset, relaSize] = relocationRange(7, 0x6100002f, 8, 0x61000031);
    const auto [pltOffset, pltSize] = relocationRange(23, 0x61000029, 2, 0x6100002d);
    if (relaSize != 0 && pltSize != 0 && relaOffset < pltOffset + pltSize && pltOffset < relaOffset + relaSize
        && relaOffset % 24 != pltOffset % 24)
        throw Domain::RelinkerException("Code analysis: unaligned overlapping relocation tables", pltOffset);
    const auto readRelocations = [&](std::uint64_t start, std::uint64_t size, bool plt) {
        for (std::uint64_t offset = start; offset < start + size; offset += 24) {
            if (!plt && offset >= pltOffset && offset - pltOffset < pltSize) continue;
            const auto slot = Io::ReadU64(bytes, offset);
            const auto info = Io::ReadU64(bytes, offset + 8);
            const auto type = static_cast<std::uint32_t>(info);
            const auto index = info >> 32;
            const auto addend = Io::ReadU64(bytes, offset + 16);
            if (type == 8) {
                const auto hasSymbol = index != 0 && index < symbolCount;
                const auto symbol = hasSymbol ? symbols + index * 24 : 0;
                const auto zeroSectionSymbol = hasSymbol && (bytes[symbol + 4] & 15) == 3 && Io::ReadU64(bytes, symbol + 8) == 0;
                const auto matchingDefinedSymbol = hasSymbol && Io::ReadU16(bytes, symbol + 6) != 0
                    && addend <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())
                    && Io::ReadU64(bytes, symbol + 8) == addend;
                if ((index != 0 && !zeroSectionSymbol && !matchingDefinedSymbol) || !pointers.Values.emplace(slot, addend).second) throw Domain::RelinkerException("Code analysis: invalid relative relocation", slot);
                addPointerRoot(addend);
            } else if (type == 1 || type == 6 || type == 7) {
                if (index != 0 && index >= symbolCount) throw Domain::RelinkerException("Code analysis: invalid relocation symbol", slot);
                const auto symbol = symbols + index * 24;
                if (index == 0 || Io::ReadU16(bytes, symbol + 6) == 0) importSlots.insert(slot);
                else {
                    const auto target = Io::ReadU64(bytes, symbol + 8) + addend;
                    if (!pointers.Values.emplace(slot, target).second) throw Domain::RelinkerException("Code analysis: duplicate pointer relocation", slot);
                    addPointerRoot(target);
                }
            }
        }
    };
    readRelocations(relaOffset, relaSize, false);
    readRelocations(pltOffset, pltSize, true);
    for (const auto& [addressTag, sizeTag] : {std::pair{25u, 27u}, std::pair{26u, 28u}, std::pair{32u, 33u}}) {
        const auto address = value(addressTag, 0x60000000 + addressTag);
        const auto size = value(sizeTag, 0x60000000 + sizeTag);
        if (size && !address) throw Domain::RelinkerException("Code analysis: incomplete initializer array");
        if (!size || *size == 0) continue;
        if (*size % 8 != 0) throw Domain::RelinkerException("Code analysis: invalid initializer array size");
        const auto offset = fileOffset(*address, *size);
        for (std::uint64_t index = 0; index < *size; index += 8) {
            const auto relocated = pointers.TargetOfSlot(*address + index);
            const auto target = relocated ? *relocated : Io::ReadU64(bytes, offset + index);
            if (target == 0 || target == std::numeric_limits<std::uint64_t>::max()) continue;
            if (!isCode(target)) throw Domain::RelinkerException("Code analysis: initializer lies outside code", target);
            roots.insert(target);
        }
    }
    for (const auto& function : UnusedNidFilter::ReadExceptionFunctions(bytes, headers, pointers.Values, importSlots)) {
        if (function.End <= function.Begin) throw Domain::RelinkerException("Code analysis: invalid unwind function range", function.Begin);
        addFunction(function.Begin, function.End - function.Begin, false);
        for (const auto target : function.ExtraTargets) addRoot(target);
    }
    if (sceBase) {
        const std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges(functions.begin(), functions.end());
        for (const auto& header : codeHeaders) {
            if (header.Type != 1 || (header.Flags & 1) == 0) continue;
            const std::span<const std::uint8_t> segment(bytes.data() + header.Offset, header.FileSize);
            for (const auto& table : FindRelativeJumpTables(segment, header.MappedAddress, ranges)) {
                tables.emplace(table.Begin, table.End);
                auto& end = functions.at(table.Function);
                end = std::min(end, table.Begin);
            }
        }
        std::erase_if(roots, inTable);
    }
    if (roots.empty()) throw Domain::RelinkerException("Code analysis: no code entry points");
    std::set<std::uint64_t> instructions;
    const Codegen::X64InstructionDecoder decoder;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> skipped;
    std::set<std::uint64_t> staticTargets;
    std::uint64_t functionBytes = 0;
    for (const auto& [begin, end] : functions) functionBytes += end - begin;
    std::uint64_t decodedBytes = 0;
    report(1000);
    for (const auto& [begin, end] : functions) {
        const auto offset = fileOffset(begin, end - begin);
        for (auto address = begin; address < end;) {
            Codegen::DecodedInstructionInfo info;
            try {
                info = decoder.DecodeInstruction(bytes.data() + offset + address - begin, end - address);
            } catch (const Codegen::CodegenException& error) {
                const auto tailBytes = end - address;
                bool tailHasFsPrefix = false;
                for (auto probe = address; probe < end; ++probe) {
                    if (bytes[offset + probe - begin] == 0x64) {
                        tailHasFsPrefix = true;
                        break;
                    }
                }
                if (tailBytes > 15 || tailHasFsPrefix)
                    throw Domain::RelinkerException(std::string("Code analysis: ") + error.what(), address);
                skipped.emplace_back(address, end);
                break;
            }
            if (info.Length == 0 || info.Length > end - address)
                throw Domain::RelinkerException("Code analysis: instruction crosses function boundary", address);
            instructions.emplace_hint(instructions.end(), address);
            addAddressTakenRoot(address, bytes.data() + offset + address - begin, info);
            if (info.HasBranchTarget && !info.HasRipRelativeDisp) {
                const auto target = address + info.Length + static_cast<std::uint64_t>(info.BranchDisp);
                staticTargets.insert(target);
                addRoot(target);
            }
            address += info.Length;
            decodedBytes += info.Length;
            if (progress && (decodedBytes & 65535) < info.Length) report(1000 + 2500 * decodedBytes / functionBytes);
        }
    }
    report(3500);
    std::uint64_t scanBytes = 0;
    for (const auto& header : headers) if (header.Type == 1 && (header.Flags & 1) != 0) scanBytes += header.FileSize;
    std::uint64_t scannedBytes = 0;
    for (const auto& header : headers) {
        if (header.Type != 1 || (header.Flags & 1) == 0 || header.FileSize < 16) continue;
        range(header.Offset, header.FileSize);
        const auto* text = bytes.data() + header.Offset;
        const auto size = static_cast<std::size_t>(header.FileSize);
        const auto paddedStart = [&](std::size_t address) { return address % 16 == 0 && address >= 2 && text[address - 1] == 0xcc && text[address - 2] == 0xcc; };
        const auto nextPaddedStart = [&](std::size_t address) {
            address = (address + 16) & ~std::size_t{15};
            while (address < size && !paddedStart(address)) {
                if (progress && address % 65536 == 0) report(3500 + 2500 * (scannedBytes + address) / scanBytes);
                address += 16;
            }
            return address;
        };
        bool afterTransfer = false;
        for (std::size_t position = nextPaddedStart(0); position < size;) {
            if (progress && position % 65536 < 16) report(3500 + 2500 * (scannedBytes + position) / scanBytes);
            const auto aligned = (position + 15) & ~std::size_t{15};
            const bool zeroPadding = afterTransfer && aligned != position && aligned <= size && std::all_of(text + position, text + aligned, [](const std::uint8_t value) { return value == 0; });
            afterTransfer = false;
            if (zeroPadding) {
                position = aligned;
                continue;
            }
            if (position + 1 < size && text[position] == 0 && text[position + 1] == 0) {
                position = nextPaddedStart(position);
                continue;
            }
            Codegen::DecodedInstructionInfo info;
            try {
                info = decoder.DecodeInstruction(text + position, size - position);
            } catch (const Codegen::CodegenException&) {
                position = nextPaddedStart(position);
                continue;
            }
            if (info.Length == 0 || info.Length > size - position) {
                position = nextPaddedStart(position);
                continue;
            }
            const auto next = position + info.Length;
            const auto crossed = (position & ~std::size_t{15}) + 16;
            if (crossed < next && paddedStart(crossed)) {
                position = crossed;
                continue;
            }
            if (absoluteFsAccess(text + position, info)) addRoot(header.MappedAddress + position);
            afterTransfer = info.FlowKind != Codegen::ControlFlowKind::Sequential && info.FlowKind != Codegen::ControlFlowKind::ConditionalBranch;
            position = next;
        }
        scannedBytes += header.FileSize;
    }
    report(6000);
    const auto decodesInSegment = [&](std::uint64_t address) {
        for (const auto& header : headers) {
            if (header.Type != 1 || (header.Flags & 1) == 0 || address < header.MappedAddress || address - header.MappedAddress >= header.FileSize) continue;
            const auto offset = address - header.MappedAddress;
            try {
                return decoder.DecodeInstruction(bytes.data() + header.Offset + offset, header.FileSize - offset).Length != 0;
            } catch (const Codegen::CodegenException&) {
                return false;
            }
        }
        return false;
    };
    for (const auto target : staticTargets) {
        for (const auto& [skipBegin, skipEnd] : skipped) {
            if (skipBegin <= target && target < skipEnd && !decodesInSegment(target))
                throw Domain::RelinkerException("Code analysis: branch into skipped range tail", target);
        }
    }
    std::size_t previousRoots = 0;
    std::unordered_set<Domain::VirtualAddress> decodedInstructions;
    std::vector<std::unordered_set<Domain::VirtualAddress>> sectionReachable(codeHeaders.size());
    do {
        previousRoots = roots.size();
        for (std::size_t i = 0; i < codeHeaders.size(); ++i) {
            const auto& header = codeHeaders[i];
            if (header.Type != 1 || (header.Flags & 1) == 0 || header.FileSize == 0) continue;
            range(header.Offset, header.FileSize);
            std::vector<std::uint64_t> entries;
            for (const auto root : roots) if (root >= header.MappedAddress && root - header.MappedAddress < header.FileSize) entries.push_back(root);
            if (entries.empty()) continue;
            const std::span<const std::uint8_t> text(bytes.data() + header.Offset, header.FileSize);
            const auto graph = UnusedNidFilter::BuildControlFlowGraph(text, header.MappedAddress, entries.front(), entries, pointers, sceFunctions.empty(), sectionReachable[i], [&](std::uint64_t count) { report(6000 + 2500 * count / std::max<std::uint64_t>(scanBytes, 1)); }, inTable);
            for (const auto address : graph->ReachableVaddrs()) {
                if (!decodedInstructions.insert(address).second) continue;
                instructions.insert(address);
                const auto offset = address - header.MappedAddress;
                const auto info = decoder.DecodeInstruction(text.data() + offset, text.size() - offset);
                addAddressTakenRoot(address, text.data() + offset, info);
                if (info.HasBranchTarget && !info.HasRipRelativeDisp) addRoot(static_cast<std::uint64_t>(static_cast<std::int64_t>(address + info.Length) + info.BranchDisp));
            }
        }
    } while (roots.size() != previousRoots);
    report(8500);
    std::uint64_t checked = 0;
    std::uint64_t previousEnd = 0;
    const Domain::ProgramHeader* cachedHeader = nullptr;
    const auto findCodeHeader = [&](std::uint64_t addr) -> const Domain::ProgramHeader* {
        if (cachedHeader != nullptr && cachedHeader->Type == 1 && (cachedHeader->Flags & 1) != 0 && addr >= cachedHeader->MappedAddress && addr - cachedHeader->MappedAddress < cachedHeader->FileSize)
            return cachedHeader;
        for (const auto& header : headers) {
            if (header.Type == 1 && (header.Flags & 1) != 0 && addr >= header.MappedAddress && addr - header.MappedAddress < header.FileSize) {
                cachedHeader = &header;
                return cachedHeader;
            }
        }
        return nullptr;
    };
    for (const auto address : instructions) {
        if (progress && checked % 4096 == 0) report(8500 + 1499 * checked / instructions.size());
        ++checked;
        if (address < previousEnd) {
            std::uint64_t spanEnd = previousEnd;
            try {
                if (const auto* header = findCodeHeader(address)) {
                    const auto offset = address - header->MappedAddress;
                    const auto length = decoder.DecodeInstruction(bytes.data() + header->Offset + offset, header->FileSize - offset).Length;
                    if (length != 0 && length <= header->FileSize - offset)
                        spanEnd = std::max(spanEnd, address + length);
                }
            } catch (const Codegen::CodegenException&) {
            }
            bool spanHasFsPrefix = false;
            if (const auto* header = findCodeHeader(address)) {
                const auto base = header->Offset + address - header->MappedAddress;
                for (auto probe = base; probe < base + (spanEnd - address) && probe < bytes.size(); ++probe) {
                    if (bytes[probe] == 0x64) {
                        spanHasFsPrefix = true;
                        break;
                    }
                }
            }
            if (!spanHasFsPrefix) {
                const auto previous = instructions.lower_bound(address);
                if (previous != instructions.begin()) {
                    const auto previousStart = *std::prev(previous);
                    if (const auto* header = findCodeHeader(previousStart)) {
                        const auto base = header->Offset + previousStart - header->MappedAddress;
                        if (decoder.DecodeInstruction(bytes.data() + base, bytes.size() - base).SegmentPrefix == 0x64) spanHasFsPrefix = true;
                    }
                }
            }
            if (spanHasFsPrefix) throw Domain::RelinkerException("Code analysis: overlapping instruction boundaries", address);
            previousEnd = spanEnd;
            continue;
        }
        if (const auto* header = findCodeHeader(address)) {
            const auto offset = address - header->MappedAddress;
            previousEnd = address + decoder.DecodeInstruction(bytes.data() + header->Offset + offset, header->FileSize - offset).Length;
        }
    }
    report(10000);
    std::erase_if(dataTargets, [&](std::uint64_t address) { return roots.contains(address) || inTable(address); });
    if (!dataTargets.empty() && message) {
        std::ostringstream text;
        text << "Took " << std::dec << dataTargets.size() << " pointer target(s) outside the unwind functions as data, the first at 0x" << std::hex << *dataTargets.begin() << '\n';
        message(text.str());
    }
    if (entryRoots) {
        *entryRoots = std::move(roots);
        entryRoots->insert(dataTargets.begin(), dataTargets.end());
    }
    return instructions;
}

}
