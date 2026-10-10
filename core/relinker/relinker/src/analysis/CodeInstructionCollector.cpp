#include <relinker/analysis/CodeInstructionCollector.hpp>
#include <relinker/analysis/UnusedNidFilter/IControlFlowGraph.hpp>
#include <relinker/analysis/UnusedNidFilter/EhFrameReader.hpp>
#include <codegen/x86/X64InstructionDecoder.hpp>
#include <codegen/CodegenException.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <bit>
#include <iterator>
#include <limits>
#include <map>
#include <span>
#include <string>
#include <unordered_set>

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
};

}

std::set<Domain::VirtualAddress> CodeInstructionCollector::Collect(const std::vector<std::uint8_t>& bytes, const std::vector<Domain::ProgramHeader>& headers) const {
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
    const auto isCode = [&](std::uint64_t address) {
        for (const auto& header : headers) if (header.Type == 1 && (header.Flags & 1) != 0 && address >= header.MappedAddress && address - header.MappedAddress < header.FileSize) return true;
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
            if (tag == 1 || (tag >= 0x61000000 && tag != 0x6100003f && (tag < 0x61000027 || tag > 0x6100003b))) continue;
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
    std::set<std::uint64_t> roots;
    std::map<std::uint64_t, std::uint64_t> functions;
    const auto addRoot = [&](std::uint64_t address) { if (isCode(address)) roots.insert(address); };
    const auto addAddressTakenRoot = [&](std::uint64_t address, const std::uint8_t* code, const Codegen::DecodedInstructionInfo& info) {
        if (!info.HasRipRelativeDisp || code[info.OpcodeOffset] != 0x8d || !(info.RexPrefix & 8)) return;
        const auto displacement = std::bit_cast<std::int32_t>(Io::ReadU32(bytes, fileOffset(address, info.Length) + info.RipRelativeDispOffset));
        addRoot(address + info.Length + static_cast<std::int64_t>(displacement));
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
    for (const auto tag : {12u, 13u}) if (const auto address = value(tag, 0x60000000 + tag); address && *address != 0) addRoot(*address);
    const auto symbolBytes = value(0x6100003f, 0xffffffffffffffffull);
    std::uint64_t symbolCount = 0;
    if (symbolBytes) {
        if (*symbolBytes % 24 != 0) throw Domain::RelinkerException("Code analysis: invalid symbol table size");
        symbolCount = *symbolBytes / 24;
    } else if (tags.contains(4)) symbolCount = Io::ReadU32(bytes, fileOffset(tags.at(4), 8) + 4);
    std::uint64_t symbols = 0;
    if (symbolCount != 0) {
        if (value(11, 0x6100003b) != 24) throw Domain::RelinkerException("Code analysis: invalid symbol entry size");
        symbols = table(6, 0x61000039, symbolCount * 24);
        for (std::uint64_t index = 0; index < symbolCount; ++index) {
            const auto offset = symbols + index * 24;
            if ((bytes[offset + 4] & 15) == 2 && Io::ReadU16(bytes, offset + 6) != 0) {
                const auto address = Io::ReadU64(bytes, offset + 8);
                addRoot(address);
                addFunction(address, Io::ReadU64(bytes, offset + 16), true);
            }
        }
    }
    CodePointers pointers;
    std::set<std::uint64_t> importSlots;
    const auto readRelocations = [&](std::uint64_t addressTag, std::uint64_t sceAddressTag, std::uint64_t sizeTag, std::uint64_t sceSizeTag) {
        const auto size = value(sizeTag, sceSizeTag);
        if (!size) return;
        if (*size % 24 != 0) throw Domain::RelinkerException("Code analysis: invalid relocation table size");
        if (*size == 0) return;
        const auto start = table(addressTag, sceAddressTag, *size);
        for (std::uint64_t offset = start; offset < start + *size; offset += 24) {
            const auto slot = Io::ReadU64(bytes, offset);
            const auto info = Io::ReadU64(bytes, offset + 8);
            const auto type = static_cast<std::uint32_t>(info);
            const auto index = info >> 32;
            const auto addend = Io::ReadU64(bytes, offset + 16);
            if (type == 8) {
                if (index != 0 || !pointers.Values.emplace(slot, addend).second) throw Domain::RelinkerException("Code analysis: invalid relative relocation", slot);
                addRoot(addend);
            } else if (type == 1 || type == 6 || type == 7) {
                if (index == 0 || index >= symbolCount) throw Domain::RelinkerException("Code analysis: invalid relocation symbol", slot);
                const auto symbol = symbols + index * 24;
                if (Io::ReadU16(bytes, symbol + 6) == 0) importSlots.insert(slot);
                else {
                    const auto target = Io::ReadU64(bytes, symbol + 8) + addend;
                    if (!pointers.Values.emplace(slot, target).second) throw Domain::RelinkerException("Code analysis: duplicate pointer relocation", slot);
                    addRoot(target);
                }
            }
        }
    };
    readRelocations(7, 0x6100002f, 8, 0x61000031);
    readRelocations(23, 0x61000029, 2, 0x6100002d);
    for (const auto& [addressTag, sizeTag] : {std::pair{25u, 27u}, std::pair{26u, 28u}, std::pair{32u, 33u}}) {
        const auto address = value(addressTag, 0x60000000 + addressTag);
        const auto size = value(sizeTag, 0x60000000 + sizeTag);
        if (address.has_value() != size.has_value()) throw Domain::RelinkerException("Code analysis: incomplete initializer array");
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
    if (roots.empty()) throw Domain::RelinkerException("Code analysis: no code entry points");
    std::set<std::uint64_t> instructions;
    const Codegen::X64InstructionDecoder decoder;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> skipped;
    std::set<std::uint64_t> staticTargets;
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
            instructions.insert(address);
            addAddressTakenRoot(address, bytes.data() + offset + address - begin, info);
            if (info.HasBranchTarget && !info.HasRipRelativeDisp) {
                const auto target = address + info.Length + static_cast<std::uint64_t>(info.BranchDisp);
                staticTargets.insert(target);
                addRoot(target);
            }
            address += info.Length;
        }
    }
    for (const auto& header : headers) {
        if (header.Type != 1 || (header.Flags & 1) == 0 || header.FileSize < 16) continue;
        range(header.Offset, header.FileSize);
        const auto* text = bytes.data() + header.Offset;
        const auto size = static_cast<std::size_t>(header.FileSize);
        const auto paddedStart = [&](std::size_t address) { return address % 16 == 0 && address >= 2 && text[address - 1] == 0xcc && text[address - 2] == 0xcc; };
        const auto nextPaddedStart = [&](std::size_t address) {
            address = (address + 16) & ~std::size_t{15};
            while (address < size && !paddedStart(address)) address += 16;
            return address;
        };
        bool afterTransfer = false;
        for (std::size_t position = nextPaddedStart(0); position < size;) {
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
            const auto opcode = position + info.OpcodeOffset;
            if (info.SegmentPrefix == 0x64 && opcode + 2 < next && (text[opcode + 1] & 0xc7) == 0x04 && text[opcode + 2] == 0x25) addRoot(header.MappedAddress + position);
            afterTransfer = info.FlowKind != Codegen::ControlFlowKind::Sequential && info.FlowKind != Codegen::ControlFlowKind::ConditionalBranch;
            position = next;
        }
    }
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
    std::vector<std::unordered_set<Domain::VirtualAddress>> sectionReachable(headers.size());
    do {
        previousRoots = roots.size();
        for (std::size_t i = 0; i < headers.size(); ++i) {
            const auto& header = headers[i];
            if (header.Type != 1 || (header.Flags & 1) == 0 || header.FileSize == 0) continue;
            range(header.Offset, header.FileSize);
            std::vector<std::uint64_t> entries;
            for (const auto root : roots) if (root >= header.MappedAddress && root - header.MappedAddress < header.FileSize) entries.push_back(root);
            if (entries.empty()) continue;
            const std::span<const std::uint8_t> text(bytes.data() + header.Offset, header.FileSize);
            const auto graph = UnusedNidFilter::BuildControlFlowGraph(text, header.MappedAddress, entries.front(), entries, pointers, true, sectionReachable[i]);
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
    std::uint64_t previousEnd = 0;
    for (const auto address : instructions) {
        if (address < previousEnd) {
            std::uint64_t spanEnd = previousEnd;
            try {
                for (const auto& header : headers) {
                    if (header.Type != 1 || (header.Flags & 1) == 0 || address < header.MappedAddress || address - header.MappedAddress >= header.FileSize) continue;
                    const auto offset = address - header.MappedAddress;
                    const auto length = decoder.DecodeInstruction(bytes.data() + header.Offset + offset, header.FileSize - offset).Length;
                    if (length == 0 || length > header.FileSize - offset) break;
                    spanEnd = std::max(spanEnd, address + length);
                    break;
                }
            } catch (const Codegen::CodegenException&) {
            }
            bool spanHasFsPrefix = false;
            for (const auto& header : headers) {
                if (header.Type != 1 || (header.Flags & 1) == 0 || address < header.MappedAddress || address - header.MappedAddress >= header.FileSize) continue;
                const auto base = header.Offset + address - header.MappedAddress;
                for (auto probe = base; probe < base + (spanEnd - address) && probe < bytes.size(); ++probe) {
                    if (bytes[probe] == 0x64) {
                        spanHasFsPrefix = true;
                        break;
                    }
                }
                break;
            }
            if (!spanHasFsPrefix) {
                const auto previous = instructions.lower_bound(address);
                if (previous != instructions.begin()) {
                    const auto previousStart = *std::prev(previous);
                    for (const auto& header : headers) {
                        if (header.Type != 1 || (header.Flags & 1) == 0 || previousStart < header.MappedAddress || previousStart - header.MappedAddress >= header.FileSize) continue;
                        const auto base = header.Offset + previousStart - header.MappedAddress;
                        if (decoder.DecodeInstruction(bytes.data() + base, bytes.size() - base).SegmentPrefix == 0x64) spanHasFsPrefix = true;
                        break;
                    }
                }
            }
            if (spanHasFsPrefix) throw Domain::RelinkerException("Code analysis: overlapping instruction boundaries", address);
            previousEnd = spanEnd;
            continue;
        }
        for (const auto& header : headers) {
            if (header.Type != 1 || (header.Flags & 1) == 0 || address < header.MappedAddress || address - header.MappedAddress >= header.FileSize) continue;
            const auto offset = address - header.MappedAddress;
            previousEnd = address + decoder.DecodeInstruction(bytes.data() + header.Offset + offset, header.FileSize - offset).Length;
            break;
        }
    }
    return instructions;
}

}
