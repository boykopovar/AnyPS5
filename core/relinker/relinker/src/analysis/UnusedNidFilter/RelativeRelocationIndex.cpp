#include <relinker/analysis/UnusedNidFilter/IRelativeRelocationIndex.hpp>
#include <relinker/parsing/ElfReader.hpp>
#include <elfpatcher/general/ElfConstants.hpp>
#include <unordered_map>
#include <cstring>
#include <limits>
#include <optional>

namespace Relinker::UnusedNidFilter {

using namespace Elfpatcher;

namespace {

std::uint16_t read16(const std::vector<std::uint8_t>& b, std::size_t off) {
    std::uint16_t v = 0;
    std::memcpy(&v, b.data() + off, 2);
    return v;
}

std::uint32_t read32(const std::vector<std::uint8_t>& b, std::size_t off) {
    std::uint32_t v = 0;
    std::memcpy(&v, b.data() + off, 4);
    return v;
}

std::uint64_t read64(const std::vector<std::uint8_t>& b, std::size_t off) {
    std::uint64_t v = 0;
    std::memcpy(&v, b.data() + off, 8);
    return v;
}

struct LoadSegment {
    VirtualAddress vaddr;
    std::uint64_t fileOffset;
    std::uint64_t fileSize;
};

std::uint64_t vaddrToFileOffset(
    const std::vector<LoadSegment>& loads,
    VirtualAddress va,
    std::uint64_t size
) {
    for (const auto& seg : loads) {
        if (va >= seg.vaddr) {
            std::uint64_t relative = va - seg.vaddr;
            if (relative < seg.fileSize) {
                if (size > seg.fileSize - relative)
                    throw RelinkerException("Relocation table out of bounds", va);
                if (relative > std::numeric_limits<std::uint64_t>::max() - seg.fileOffset)
                    throw RelinkerException("Relocation table file offset overflow", va);
                return seg.fileOffset + relative;
            }
        }
    }
    throw RelinkerException("Cannot translate virtual address to file offset", va);
}

constexpr std::size_t RelaEntSize = 24;

void collectRelativeEntries(
    const std::vector<std::uint8_t>& elfBytes,
    const std::vector<LoadSegment>& loads,
    std::uint64_t tableAddress,
    std::uint64_t tableSize,
    bool sceTable,
    std::uint64_t sceTableBase,
    std::unordered_map<VirtualAddress, VirtualAddress>& out
) {
    if (tableSize == 0 && !sceTable) return;
    if (tableSize % RelaEntSize != 0)
        throw RelinkerException("Invalid relocation table size", tableSize);

    if (sceTable && tableAddress > std::numeric_limits<std::uint64_t>::max() - sceTableBase)
        throw RelinkerException("Relocation table file offset overflow", tableAddress);
    std::uint64_t tableFileOff = sceTable ? sceTableBase + tableAddress : vaddrToFileOffset(loads, tableAddress, tableSize);
    if (tableFileOff > elfBytes.size())
        throw RelinkerException("Relocation table out of bounds", tableFileOff);
    if (tableSize > elfBytes.size() - tableFileOff)
        throw RelinkerException("Relocation table out of bounds", tableFileOff);

    for (std::uint64_t off = 0; off < tableSize; off += RelaEntSize) {
        std::size_t pos = static_cast<std::size_t>(tableFileOff + off);
        if (pos > elfBytes.size() || RelaEntSize > elfBytes.size() - pos)
            throw RelinkerException("Relocation entry out of bounds", pos);

        std::uint64_t rOffset = read64(elfBytes, pos);
        std::uint64_t rInfo = read64(elfBytes, pos + 8);
        std::int64_t rAddend = static_cast<std::int64_t>(read64(elfBytes, pos + 16));
        std::uint32_t relType = static_cast<std::uint32_t>(rInfo & 0xffffffff);

        if (relType != R_X86_64_RELATIVE) continue;

        if (rAddend < 0)
            throw RelinkerException("Relative relocation target address is negative", rOffset);

        auto [it, inserted] = out.emplace(static_cast<VirtualAddress>(rOffset), static_cast<VirtualAddress>(rAddend));
        if (!inserted && it->second != static_cast<VirtualAddress>(rAddend))
            throw RelinkerException("Conflicting relative relocation targets for slot", rOffset);
    }
}

}

class RelativeRelocationIndex : public IRelativeRelocationIndex {
public:
    explicit RelativeRelocationIndex(std::unordered_map<VirtualAddress, VirtualAddress> targets)
        : _targets(std::move(targets)) {}

    std::optional<VirtualAddress> TargetOfSlot(VirtualAddress slotVaddr) const override {
        auto it = _targets.find(slotVaddr);
        if (it == _targets.end()) return std::nullopt;
        return it->second;
    }

    std::vector<VirtualAddress> Targets() const override {
        std::vector<VirtualAddress> result;
        result.reserve(_targets.size());
        for (const auto& entry : _targets) result.push_back(entry.second);
        return result;
    }

private:
    std::unordered_map<VirtualAddress, VirtualAddress> _targets;
};

std::unique_ptr<IRelativeRelocationIndex> BuildRelativeRelocationIndex(
    const std::vector<std::uint8_t>& elfBytes
) {
    if (elfBytes.size() < 64) throw RelinkerException("ELF too small for header");
    if (elfBytes[0] != 0x7f || elfBytes[1] != 'E' ||
        elfBytes[2] != 'L' || elfBytes[3] != 'F') {
        throw RelinkerException("Not an ELF file");
    }
    if (elfBytes[4] != 2) throw RelinkerException("Only ELF64 supported");

    std::uint64_t phOff = read64(elfBytes, 32);
    std::uint16_t phEntSize = read16(elfBytes, 54);
    std::uint16_t phCount = read16(elfBytes, 56);

    if (phEntSize < 56) throw RelinkerException("ELF program header entry too small");
    if (phOff > elfBytes.size() || phCount > (elfBytes.size() - phOff) / phEntSize)
        throw RelinkerException("Program header out of bounds");

    std::vector<LoadSegment> loads;
    std::uint64_t sceTableBase = 0;
    bool hasSceDynlibData = false;
    for (std::uint16_t i = 0; i < phCount; ++i) {
        std::size_t phPos = static_cast<std::size_t>(phOff) + i * phEntSize;
        if (phPos + 56 > elfBytes.size()) throw RelinkerException("Program header out of bounds");
        std::uint32_t type = read32(elfBytes, phPos);
        if (type == PT_SCE_DYNLIBDATA) {
            if (hasSceDynlibData) throw RelinkerException("Multiple PT_SCE_DYNLIBDATA segments", phPos);
            hasSceDynlibData = true;
            sceTableBase = read64(elfBytes, phPos + 8);
        }
    }
    for (const auto& header : ElfReader(elfBytes).ReadProgramHeaders()) {
        if (header.Type != PT_LOAD) continue;
        loads.push_back({header.MappedAddress, header.Offset, header.FileSize});
    }

    std::unordered_map<VirtualAddress, VirtualAddress> targets;
    bool hasDynamic = false;

    for (std::uint16_t i = 0; i < phCount; ++i) {
        std::size_t phPos = static_cast<std::size_t>(phOff) + i * phEntSize;
        if (phPos + 56 > elfBytes.size()) throw RelinkerException("Program header out of bounds");

        std::uint32_t type = read32(elfBytes, phPos);
        if (type != PT_DYNAMIC) continue;
        if (hasDynamic) throw RelinkerException("Multiple PT_DYNAMIC segments", phPos);
        hasDynamic = true;

        std::uint64_t segOff = read64(elfBytes, phPos + 8);
        std::uint64_t segSz = read64(elfBytes, phPos + 32);
        if (segOff > elfBytes.size() || segSz > elfBytes.size() - segOff) throw RelinkerException("PT_DYNAMIC segment out of bounds");
        if (segSz % 16 != 0) throw RelinkerException("Invalid dynamic segment size", segOff);

        std::optional<VirtualAddress> relaVa;
        std::optional<std::uint64_t> relaSz;
        std::optional<VirtualAddress> jmprelVa;
        std::optional<std::uint64_t> pltrelsz;
        bool hasSysvRela = false;
        bool hasOsRela = false;
        bool hasSysvRelaSz = false;
        bool hasOsRelaSz = false;
        bool hasSysvJmprel = false;
        bool hasOsJmprel = false;
        bool hasSysvPltrelsz = false;
        bool hasOsPltrelsz = false;
        bool terminated = false;

        for (std::uint64_t off = 0; off <= segSz && segSz - off >= 16; off += 16) {
            std::size_t pos = static_cast<std::size_t>(segOff + off);
            std::int64_t tag = static_cast<std::int64_t>(read64(elfBytes, pos));
            std::uint64_t val = read64(elfBytes, pos + 8);

            if (tag == DT_NULL) {
                terminated = true;
                break;
            }
            if (tag == DT_RELA) {
                if (hasOsRela) throw RelinkerException("Both DT_OS_ and DT_ variants present for DT_RELA");
                if (hasSysvRela && relaVa.value() != val) throw RelinkerException("Conflicting duplicate DT_RELA tags");
                hasSysvRela = true;
                relaVa = val;
            }
            if (tag == DT_OS_RELA) {
                if (hasSysvRela) throw RelinkerException("Both DT_OS_ and DT_ variants present for DT_RELA");
                if (hasOsRela && relaVa.value() != val) throw RelinkerException("Conflicting duplicate DT_OS_RELA tags");
                hasOsRela = true;
                relaVa = val;
            }
            if (tag == DT_RELASZ) {
                if (hasOsRelaSz) throw RelinkerException("Both DT_OS_ and DT_ variants present for DT_RELASZ");
                if (hasSysvRelaSz && relaSz.value() != val) throw RelinkerException("Conflicting duplicate DT_RELASZ tags");
                hasSysvRelaSz = true;
                relaSz = val;
            }
            if (tag == DT_OS_RELASZ) {
                if (hasSysvRelaSz) throw RelinkerException("Both DT_OS_ and DT_ variants present for DT_RELASZ");
                if (hasOsRelaSz && relaSz.value() != val) throw RelinkerException("Conflicting duplicate DT_OS_RELASZ tags");
                hasOsRelaSz = true;
                relaSz = val;
            }
            if (tag == DT_JMPREL) {
                if (hasOsJmprel) throw RelinkerException("Both DT_OS_ and DT_ variants present for DT_JMPREL");
                if (hasSysvJmprel && jmprelVa.value() != val) throw RelinkerException("Conflicting duplicate DT_JMPREL tags");
                hasSysvJmprel = true;
                jmprelVa = val;
            }
            if (tag == DT_OS_JMPREL) {
                if (hasSysvJmprel) throw RelinkerException("Both DT_OS_ and DT_ variants present for DT_JMPREL");
                if (hasOsJmprel && jmprelVa.value() != val) throw RelinkerException("Conflicting duplicate DT_OS_JMPREL tags");
                hasOsJmprel = true;
                jmprelVa = val;
            }
            if (tag == DT_PLTRELSZ) {
                if (hasOsPltrelsz) throw RelinkerException("Both DT_OS_ and DT_ variants present for DT_PLTRELSZ");
                if (hasSysvPltrelsz && pltrelsz.value() != val) throw RelinkerException("Conflicting duplicate DT_PLTRELSZ tags");
                hasSysvPltrelsz = true;
                pltrelsz = val;
            }
            if (tag == DT_OS_PLTRELSZ) {
                if (hasSysvPltrelsz) throw RelinkerException("Both DT_OS_ and DT_ variants present for DT_PLTRELSZ");
                if (hasOsPltrelsz && pltrelsz.value() != val) throw RelinkerException("Conflicting duplicate DT_OS_PLTRELSZ tags");
                hasOsPltrelsz = true;
                pltrelsz = val;
            }
        }

        if (!terminated) throw RelinkerException("Unterminated dynamic segment", segOff);

        if ((hasSysvRela && hasOsRelaSz) || (hasOsRela && hasSysvRelaSz))
            throw RelinkerException("Mismatched DT_RELA and DT_RELASZ tag variants");
        if (relaVa.has_value() != relaSz.has_value())
            throw RelinkerException("Incomplete relocation table metadata");

        if ((hasSysvJmprel && hasOsPltrelsz) || (hasOsJmprel && hasSysvPltrelsz))
            throw RelinkerException("Mismatched DT_JMPREL and DT_PLTRELSZ tag variants");
        if (jmprelVa.has_value() != pltrelsz.has_value())
            throw RelinkerException("Incomplete PLT relocation table metadata");

        if (relaVa.has_value())
            collectRelativeEntries(elfBytes, loads, *relaVa, *relaSz, hasOsRela, sceTableBase, targets);
        if (jmprelVa.has_value())
            collectRelativeEntries(elfBytes, loads, *jmprelVa, *pltrelsz, hasOsJmprel, sceTableBase, targets);
    }

    return std::make_unique<RelativeRelocationIndex>(std::move(targets));
}

}

