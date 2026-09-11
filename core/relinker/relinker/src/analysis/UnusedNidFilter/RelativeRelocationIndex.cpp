#include <relinker/analysis/UnusedNidFilter/IRelativeRelocationIndex.hpp>
#include <unordered_map>
#include <cstring>

namespace Relinker::UnusedNidFilter {

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

std::uint64_t vaddrToFileOffset(const std::vector<LoadSegment>& loads, VirtualAddress va) {
    for (const auto& seg : loads) {
        if (va >= seg.vaddr && va < seg.vaddr + seg.fileSize)
            return seg.fileOffset + (va - seg.vaddr);
    }
    throw RelinkerException("Cannot translate virtual address to file offset", va);
}

constexpr std::uint32_t R_X86_64_RELATIVE = 8;
constexpr std::size_t RelaEntSize = 24;

void collectRelativeEntries(
    const std::vector<std::uint8_t>& elfBytes,
    const std::vector<LoadSegment>& loads,
    VirtualAddress tableVaddr,
    std::uint64_t tableSize,
    std::unordered_map<VirtualAddress, VirtualAddress>& out
) {
    if (tableVaddr == 0 || tableSize == 0) return;
    std::uint64_t tableFileOff = vaddrToFileOffset(loads, tableVaddr);

    for (std::uint64_t off = 0; off + RelaEntSize <= tableSize; off += RelaEntSize) {
        std::size_t pos = static_cast<std::size_t>(tableFileOff + off);
        if (pos + RelaEntSize > elfBytes.size())
            throw RelinkerException("Relocation entry out of bounds", pos);

        std::uint64_t rOffset = read64(elfBytes, pos);
        std::uint64_t rInfo = read64(elfBytes, pos + 8);
        std::int64_t rAddend = static_cast<std::int64_t>(read64(elfBytes, pos + 16));
        std::uint32_t relType = static_cast<std::uint32_t>(rInfo & 0xffffffff);

        if (relType != R_X86_64_RELATIVE) continue;

        out[static_cast<VirtualAddress>(rOffset)] = static_cast<VirtualAddress>(rAddend);
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

    std::vector<LoadSegment> loads;
    for (std::uint16_t i = 0; i < phCount; ++i) {
        std::size_t phPos = static_cast<std::size_t>(phOff) + i * phEntSize;
        if (phPos + 56 > elfBytes.size()) throw RelinkerException("Program header out of bounds");
        std::uint32_t type = read32(elfBytes, phPos);
        constexpr std::uint32_t PT_LOAD = 1;
        if (type != PT_LOAD) continue;
        LoadSegment seg;
        seg.fileOffset = read64(elfBytes, phPos + 8);
        seg.vaddr = read64(elfBytes, phPos + 16);
        seg.fileSize = read64(elfBytes, phPos + 32);
        loads.push_back(seg);
    }

    std::unordered_map<VirtualAddress, VirtualAddress> targets;

    for (std::uint16_t i = 0; i < phCount; ++i) {
        std::size_t phPos = static_cast<std::size_t>(phOff) + i * phEntSize;
        if (phPos + 56 > elfBytes.size()) throw RelinkerException("Program header out of bounds");

        std::uint32_t type = read32(elfBytes, phPos);
        constexpr std::uint32_t PT_DYNAMIC = 2;
        if (type != PT_DYNAMIC) continue;

        std::uint64_t segOff = read64(elfBytes, phPos + 8);
        std::uint64_t segSz = read64(elfBytes, phPos + 32);
        if (segOff + segSz > elfBytes.size()) throw RelinkerException("PT_DYNAMIC segment out of bounds");

        constexpr std::int64_t DT_NULL = 0;
        constexpr std::int64_t DT_RELA = 7;
        constexpr std::int64_t DT_RELASZ = 8;
        constexpr std::int64_t DT_JMPREL = 0x17;
        constexpr std::int64_t DT_PLTRELSZ = 2;
        constexpr std::int64_t DT_OS_RELA = 0x6100002f;
        constexpr std::int64_t DT_OS_RELASZ = 0x61000031;
        constexpr std::int64_t DT_OS_JMPREL = 0x61000029;
        constexpr std::int64_t DT_OS_PLTRELSZ = 0x6100002d;

        VirtualAddress relaVa = 0;
        std::uint64_t relaSz = 0;
        VirtualAddress jmprelVa = 0;
        std::uint64_t pltrelsz = 0;

        for (std::uint64_t off = 0; off + 16 <= segSz; off += 16) {
            std::size_t pos = static_cast<std::size_t>(segOff + off);
            std::int64_t tag = static_cast<std::int64_t>(read64(elfBytes, pos));
            std::uint64_t val = read64(elfBytes, pos + 8);

            if (tag == DT_NULL) break;
            if (tag == DT_RELA || tag == DT_OS_RELA) relaVa = val;
            if (tag == DT_RELASZ || tag == DT_OS_RELASZ) relaSz = val;
            if (tag == DT_JMPREL || tag == DT_OS_JMPREL) jmprelVa = val;
            if (tag == DT_PLTRELSZ || tag == DT_OS_PLTRELSZ) pltrelsz = val;
        }

        collectRelativeEntries(elfBytes, loads, relaVa, relaSz, targets);
        collectRelativeEntries(elfBytes, loads, jmprelVa, pltrelsz, targets);

        break;
    }

    return std::make_unique<RelativeRelocationIndex>(std::move(targets));
}

}
