#include <elfpatcher/windows/WindowsRelocationBuilder.hpp>
#include <io/BufferUtils.hpp>
#include <map>

namespace Elfpatcher::Windows {

PeRelocations WindowsRelocationBuilder::Apply(WindowsLoadImage& image, const Domain::SysVDynamicSection& dynamicSection) const {
    if (dynamicSection.DynSymData.empty() || dynamicSection.DynSymData.size() % 24 != 0)
        throw Domain::RelinkerException("Invalid ELF dynamic symbol table size");
    PeRelocations result;
    std::map<std::uint64_t, std::uint64_t> ranges;
    const auto applyTable = [&](const std::vector<std::uint8_t>& table, const bool plt) {
        if (table.size() % 24 != 0)
            throw Domain::RelinkerException("Invalid ELF RELA table size");
        for (std::size_t offset = 0; offset < table.size(); offset += 24) {
            const auto target = Io::ReadU64(table, offset);
            const auto info = Io::ReadU64(table, offset + 8);
            const auto addend = Io::ReadU64(table, offset + 16);
            const auto type = static_cast<std::uint32_t>(info);
            const auto symbol = info >> 32;
            if ((plt && type != 7) || (type != 1 && type != 6 && type != 7 && type != 8))
                throw Domain::RelinkerException("Unsupported Windows relocation type " + std::to_string(type), target);
            const auto rva = image.GetRva(target, 8);
            const auto next = ranges.lower_bound(target);
            if ((next != ranges.end() && next->first < target + 8) || (next != ranges.begin() && std::prev(next)->second > target))
                throw Domain::RelinkerException("Overlapping ELF relocation targets", target);
            ranges.emplace(target, target + 8);
            if (type == 8) {
                if (symbol != 0)
                    throw Domain::RelinkerException("RELATIVE relocation has a symbol index", target);
                image.WritePointer(target, image.GetRelocatedAddress(addend));
                result.BaseRelocations.push_back(rva);
                continue;
            }
            if (symbol == 0 || symbol >= dynamicSection.DynSymData.size() / 24)
                throw Domain::RelinkerException("Invalid import symbol index", target);
            const auto symbolOffset = static_cast<std::size_t>(symbol) * 24;
            if (Io::ReadU16(dynamicSection.DynSymData, symbolOffset + 6) != 0)
                throw Domain::RelinkerException("Defined symbols require explicit relocation support", target);
            const auto name = ReadString(dynamicSection.DynStrData, Io::ReadU32(dynamicSection.DynSymData, symbolOffset));
            if (name.empty())
                throw Domain::RelinkerException("Empty import symbol name", target);
            if (type != 1 && addend != 0)
                throw Domain::RelinkerException("GLOB_DAT/JUMP_SLOT relocation has a nonzero addend", target);
            image.RequireWritable(target, 8);
            image.WritePointer(target, 0);
            result.Imports.push_back({name, rva, addend});
        }
    };
    applyTable(dynamicSection.RelaData, false);
    applyTable(dynamicSection.RelaPltData, true);
    return result;
}

std::vector<std::uint8_t> WindowsRelocationBuilder::BuildBaseRelocations(const std::vector<std::uint32_t>& targets) const {
    std::map<std::uint32_t, std::vector<std::uint16_t>> pages;
    for (const auto target : targets)
        pages[target & ~0xfffu].push_back(static_cast<std::uint16_t>(0xa000u | (target & 0xfffu)));
    std::vector<std::uint8_t> result;
    for (const auto& [page, entries] : pages) {
        Io::AppendU32(result, page);
        Io::AppendU32(result, CheckedRva(8 + (entries.size() + entries.size() % 2) * 2));
        for (const auto entry : entries)
            Io::AppendU16(result, entry);
        if (entries.size() % 2 != 0)
            Io::AppendU16(result, 0);
    }
    return result;
}

}
