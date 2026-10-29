#include <relinker/analysis/UnusedNidFilter.hpp>
#include <relinker/analysis/UnusedNidFilter/StrictReachability.hpp>
#include <relinker/analysis/UnusedNidFilter/EhFrameReader.hpp>
#include <relinker/parsing/ElfReader.hpp>
#include <cstring>
#include <sstream>
#include <limits>
#include <optional>

namespace Relinker {

namespace {

class StrictImage {
public:
    explicit StrictImage(const std::vector<std::uint8_t>& bytes) : bytes(bytes), reader(bytes) {
        if (bytes.size() < 64 || bytes[4] != 2 || bytes[5] != 1 || bytes[6] != 1)
            throw RelinkerException("Strict filter: expected a little-endian ELF64 image");
        const auto header = reader.ReadHeader();
        if (header.Machine != 62 || header.ProgramHeaderEntrySize != 56)
            throw RelinkerException("Strict filter: unsupported ELF machine or program header format");
        requireRange(header.ProgramHeaderOffset, static_cast<std::uint64_t>(header.ProgramHeaderCount) * 56);
        headers = reader.ReadProgramHeaders();
        for (const auto& segment : headers) {
            requireRange(segment.Offset, segment.FileSize);
            if (segment.MemorySize > std::numeric_limits<VirtualAddress>::max() - segment.MappedAddress)
                throw RelinkerException("Strict filter: segment address overflow", segment.MappedAddress);
            if (segment.Type == 0x61000000) {
                if (hasSceDynlibData) throw RelinkerException("Strict filter: multiple PT_SCE_DYNLIBDATA segments", segment.Offset);
                hasSceDynlibData = true;
                sceTableBase = segment.Offset;
            }
            if (segment.Type == 1 && segment.FileSize > segment.MemorySize)
                throw RelinkerException("Strict filter: invalid load segment size", segment.Offset);
            if (segment.Type != 2) continue;
            if (!tags.empty()) throw RelinkerException("Strict filter: multiple dynamic segments");
            bool terminated = false;
            for (std::uint64_t offset = 0; segment.FileSize - offset >= 16; offset += 16) {
                const auto tag = read<std::int64_t>(segment.Offset + offset);
                if (tag == 0) {
                    terminated = true;
                    break;
                }
                if (tag == 1 || (tag >= 0x61000000 && tag != 0x6100002F && tag != 0x61000031 && tag != 0x61000033 && tag != 0x61000039 && tag != 0x6100003B && tag != 0x6100003F)) continue;
                if (!tags.emplace(tag, read<std::uint64_t>(segment.Offset + offset + 8)).second)
                    throw RelinkerException("Strict filter: duplicate dynamic tag", segment.Offset + offset);
            }
            if (!terminated) throw RelinkerException("Strict filter: unterminated dynamic segment", segment.Offset);
        }
    }

    UnusedNidFilter::StrictReachabilityInput Build(const std::vector<NidReference>& references, const std::vector<std::uint8_t>& text, VirtualAddress textVaddr) const {
        UnusedNidFilter::StrictReachabilityInput input{text, textVaddr, {reader.ReadHeader().EntryPoint}, {}, {}, {}, {}};
        std::size_t codeSegments = 0;
        for (const auto& segment : headers) {
            if (segment.Type == 1 && (segment.Flags & 1) != 0) {
                ++codeSegments;
                if (segment.MappedAddress != textVaddr || segment.FileSize != text.size() || segment.MemorySize != segment.FileSize || (segment.Flags & 2) != 0)
                    throw RelinkerException("Strict filter: unsupported executable segment layout", segment.MappedAddress);
            }
        }
        if (codeSegments != 1) throw RelinkerException("Strict filter: requires exactly one immutable code segment");
        for (const auto& reference : references) {
            if (reference.RelocationTypeValue != 1 && reference.RelocationTypeValue != 6 && reference.RelocationTypeValue != 7)
                throw RelinkerException("Strict filter: unsupported imported relocation", reference.RelocationAddress);
            if (!input.ImportSlots.insert(reference.RelocationAddress).second)
                throw RelinkerException("Strict filter: duplicate imported slot", reference.RelocationAddress);
        }

        const auto rela = table(7, 0x6100002F);
        const auto relaSize = value(8, 0x61000031);
        if (rela.has_value() != relaSize.has_value()) throw RelinkerException("Strict filter: incomplete relocation table metadata");
        if (rela) {
            if (value(9, 0x61000033) != 24 || *relaSize % 24 != 0)
                throw RelinkerException("Strict filter: invalid RELA entry size");
            requireRange(*rela, *relaSize);
            for (std::uint64_t offset = 0; offset < *relaSize; offset += 24) {
                const auto slot = read<std::uint64_t>(*rela + offset);
                const auto info = read<std::uint64_t>(*rela + offset + 8);
                if (static_cast<std::uint32_t>(info) != 8) continue;
                if (const auto symbol = info >> 32; symbol != 0) {
                    const auto symbols = table(6, 0x61000039);
                    const auto size = tags.find(0x6100003F);
                    if (!symbols || size == tags.end() || symbol >= size->second / 24 || (read<std::uint8_t>(*symbols + symbol * 24 + 4) & 15) != 3 || read<std::uint64_t>(*symbols + symbol * 24 + 8) != 0)
                        throw RelinkerException("Strict filter: relative relocation has a symbol", slot);
                }
                const auto target = read<std::uint64_t>(*rela + offset + 16);
                const auto [recorded, inserted] = input.Pointers.emplace(slot, target);
                if (input.ImportSlots.contains(slot) || (!inserted && recorded->second != target))
                    throw RelinkerException("Strict filter: overlapping pointer relocations", slot);

            }
        }

        for (const auto tag : {12, 13}) {
            if (const auto entry = value(tag, 0x60000000 + tag); entry && *entry != 0) input.Entries.push_back(*entry);
        }
        for (const auto& [addressTag, sizeTag] : {std::pair{25, 27}, std::pair{26, 28}, std::pair{32, 33}}) {
            const auto array = value(addressTag, 0x60000000 + addressTag);
            const auto size = value(sizeTag, 0x60000000 + sizeTag);
            if (size && !array) throw RelinkerException("Strict filter: incomplete constructor array metadata");
            if (!size || *size == 0) continue;
            if (*size % 8 != 0) throw RelinkerException("Strict filter: invalid constructor array size", *array);
            for (std::uint64_t offset = 0; offset < *size; offset += 8) {
                const auto slot = *array + offset;
                const auto position = input.Pointers.find(slot);
                const auto entry = position == input.Pointers.end() ? read<std::uint64_t>(fileOffset(slot, 8)) : position->second;
                if (entry != 0 && entry != std::numeric_limits<std::uint64_t>::max()) input.Entries.push_back(entry);
            }
        }

        const auto symbols = table(6, 0x61000039);
        if (symbols) {
            if (value(11, 0x6100003B) != 24) throw RelinkerException("Strict filter: invalid symbol entry size");
            std::uint64_t count = 0;
            if (const auto size = tags.find(0x6100003F); size != tags.end()) {
                if (size->second % 24 != 0) throw RelinkerException("Strict filter: invalid symbol table size");
                count = size->second / 24;
            } else if (const auto hash = tags.find(4); hash != tags.end()) {
                count = read<std::uint32_t>(fileOffset(hash->second, 8) + 4);
            } else {
                throw RelinkerException("Strict filter: cannot establish dynamic symbol table bounds");
            }
            if (count > bytes.size() / 24) throw RelinkerException("Strict filter: symbol count exceeds image size");
            requireRange(*symbols, count * 24);
            for (std::uint64_t index = 0; index < count; ++index) {
                const auto position = *symbols + index * 24;
                const auto section = read<std::uint16_t>(position + 6);
                const auto info = read<std::uint8_t>(position + 4);
                const auto type = info & 15;
                if (type == 10) throw RelinkerException("Strict filter: IFUNC resolver is not modeled", position);
                if (section == 0 || (info >> 4) == 0 || type != 2) continue;
                input.Entries.push_back(read<std::uint64_t>(position + 8));
            }
        }
        input.Functions = UnusedNidFilter::ReadExceptionFunctions(bytes, headers, input.Pointers, input.ImportSlots);
        for (const auto& segment : headers) {
            if (segment.Type != 1 || (segment.Flags & 1) != 0 || (segment.Flags & 6) == 0 || segment.FileSize == 0) continue;
            input.Data.push_back({segment.MappedAddress, reader.ReadSegment(segment)});
        }
        return input;
    }

private:
    const std::vector<std::uint8_t>& bytes;
    ElfReader reader;
    std::vector<ProgramHeader> headers;
    std::map<std::int64_t, std::uint64_t> tags;
    std::uint64_t sceTableBase = 0;
    bool hasSceDynlibData = false;

    void requireRange(std::uint64_t offset, std::uint64_t size) const {
        if (offset > bytes.size() || size > bytes.size() - offset) throw RelinkerException("Strict filter: file range out of bounds", offset);
    }

    template<typename TValue>
    TValue read(std::uint64_t offset) const {
        requireRange(offset, sizeof(TValue));
        TValue result;
        std::memcpy(&result, bytes.data() + offset, sizeof(result));
        return result;
    }

    std::uint64_t fileOffset(VirtualAddress address, std::uint64_t size) const {
        for (const auto& segment : headers)
            if (segment.Type == 1 && address >= segment.MappedAddress && address - segment.MappedAddress <= segment.FileSize && size <= segment.FileSize - (address - segment.MappedAddress))
                return segment.Offset + address - segment.MappedAddress;
        throw RelinkerException("Strict filter: virtual range is not file-backed", address);
    }

    std::optional<std::uint64_t> value(std::int64_t standardTag, std::int64_t osTag) const {
        const auto standard = tags.find(standardTag);
        const auto os = tags.find(osTag);
        if (standard != tags.end() && os != tags.end()) throw RelinkerException("Strict filter: conflicting standard and OS dynamic tags");
        if (standard != tags.end()) return standard->second;
        if (os != tags.end()) return os->second;
        return std::nullopt;
    }

    std::optional<std::uint64_t> table(std::int64_t standardTag, std::int64_t osTag) const {
        const auto address = value(standardTag, osTag);
        if (!address) return std::nullopt;
        if (!tags.contains(osTag)) return fileOffset(*address, 0);
        if (*address > std::numeric_limits<std::uint64_t>::max() - sceTableBase)
            throw RelinkerException("Strict filter: SCE table file offset overflow", *address);
        return sceTableBase + *address;
    }
};

class StrictNidFilter : public IUnusedNidFilter {
    Domain::IProgress& progress;

public:
    explicit StrictNidFilter(Domain::IProgress& progress) : progress(progress) {}
    std::vector<NidReference> Filter(const std::vector<NidReference>& nidRefs, const std::vector<std::uint8_t>& elfBytes, const std::vector<std::uint8_t>& textSection, VirtualAddress textVAddr, const std::vector<VirtualAddress>&) override {
        const StrictImage image(elfBytes);
        const auto input = image.Build(nidRefs, textSection, textVAddr);
        const auto analysis = UnusedNidFilter::AnalyzeStrictReachability(input);
        std::vector<NidReference> result;
        for (const auto& reference : nidRefs) {
            if (analysis.ImportSlots.contains(reference.RelocationAddress)) result.push_back(reference);
        }
        { std::ostringstream message; message << "Strict reachability: " << analysis.Instructions.size() << " instructions; conservative indirect transfers=" << analysis.IndirectTransfers << "\n"; progress.Message(message.str()); }
        { std::ostringstream message; message << "Function graph: " << analysis.LiveRegions << "/" << analysis.TotalRegions << " live regions; address-taken roots=" << analysis.AddressTakenRoots << "; unwind functions=" << input.Functions.size() << "\n"; progress.Message(message.str()); }
        return result;
    }
};

}

std::shared_ptr<IUnusedNidFilter> MakeStrictUnusedNidFilter(Domain::IProgress& progress) {
    return std::make_shared<StrictNidFilter>(progress);
}

}
