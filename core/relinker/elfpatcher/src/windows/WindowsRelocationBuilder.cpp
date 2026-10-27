#include <elfpatcher/windows/WindowsRelocationBuilder.hpp>
#include <elfpatcher/general/ElfConstants.hpp>
#include <io/BufferUtils.hpp>
#include <algorithm>
#include <bit>
#include <limits>
#include <map>
#include <string>
#include <utility>

namespace Elfpatcher::Windows {

PeRelocations WindowsRelocationBuilder::Apply(WindowsLoadImage& image, const Domain::SysVDynamicSection& dynamicSection,
                                              const std::vector<Domain::ProgramHeader>& headers) const {
    if (dynamicSection.DynSymData.empty() || dynamicSection.DynSymData.size() % 24 != 0)
        throw Domain::RelinkerException("Invalid ELF dynamic symbol table size");
    PeRelocations result;
    std::map<std::uint64_t, std::uint64_t> ranges;
    std::map<std::uint64_t, std::pair<std::uint64_t, std::uint64_t>> entries;
    const auto applyTable = [&](const std::vector<std::uint8_t>& table, const bool plt) {
        if (table.size() % 24 != 0)
            throw Domain::RelinkerException("Invalid ELF RELA table size");
        for (std::size_t offset = 0; offset < table.size(); offset += 24) {
            const auto target = Io::ReadU64(table, offset);
            const auto info = Io::ReadU64(table, offset + 8);
            const auto addend = Io::ReadU64(table, offset + 16);
            const auto type = static_cast<std::uint32_t>(info);
            const auto symbol = info >> 32;
            if ((plt && type != 7) || (type != 1 && type != 6 && type != 7 && type != 8 && type != 16 && type != 17 && type != 18))
                throw Domain::RelinkerException("Relocation type " + std::to_string(type) + " is not supported for this target", target);
            if (target > std::numeric_limits<std::uint64_t>::max() - 8)
                throw Domain::RelinkerException("Relocation target address overflow", target);
            const auto rva = image.GetRva(target, 8);
            const auto applied = entries.find(target);
            if (applied != entries.end() && applied->second == std::pair{info, addend}) continue;
            const auto next = ranges.lower_bound(target);
            if ((next != ranges.end() && next->first < target + 8) || (next != ranges.begin() && std::prev(next)->second > target))
                throw Domain::RelinkerException("Overlapping ELF relocation targets", target);
            ranges.emplace(target, target + 8);
            entries.emplace(target, std::pair{info, addend});
            if (type == 8) {
                if (symbol != 0)
                    throw Domain::RelinkerException("RELATIVE relocation has a symbol index", target);
                image.WritePointer(target, image.GetRelocatedAddress(addend));
                result.BaseRelocations.push_back(rva);
                continue;
            }
            if (type == 16 || type == 17 || type == 18) {
                if (symbol != 0)
                    throw Domain::RelinkerException("Imported TLS relocation " + std::to_string(type) + " is not supported for this target", target);
                if (type == 16)
                    throw Domain::RelinkerException("Executable DTPMOD64 relocation is not supported for this target", target);
                const auto tls = std::find_if(headers.begin(), headers.end(), [](const auto& header) { return header.Type == 7; });
                if (tls == headers.end() || tls->MemorySize == 0)
                    throw Domain::RelinkerException("TLS relocation without a non-empty PT_TLS segment", target);
                const auto alignment = std::max<std::uint64_t>(tls->Alignment, 1);
                if (!std::has_single_bit(alignment) || tls->MemorySize > 0x7fff0000u)
                    throw Domain::RelinkerException("Invalid or unsupported ELF TLS layout", tls->Offset);
                image.RequireWritable(target, 8);
                image.WritePointer(target, type == 17 ? addend : addend - Io::AlignUp64(tls->MemorySize, alignment));
                continue;
            }
            if (symbol == 0 || symbol >= dynamicSection.DynSymData.size() / 24)
                throw Domain::RelinkerException("Invalid import symbol index", target);
            const auto symbolOffset = static_cast<std::size_t>(symbol) * 24;
            if (Io::ReadU16(dynamicSection.DynSymData, symbolOffset + 6) != 0)
                throw Domain::RelinkerException("Defined symbols require explicit relocation support", target);
            const auto name = ReadString(dynamicSection.DynStrData, Io::ReadU32(dynamicSection.DynSymData, symbolOffset));
            if (name.empty() && dynamicSection.DynSymData[symbolOffset + 4] >> 4 != STB_WEAK)
                throw Domain::RelinkerException("Empty import symbol name", target);
            if (type != 1 && addend != 0)
                throw Domain::RelinkerException("GLOB_DAT/JUMP_SLOT relocation has a nonzero addend", target);
            image.RequireWritable(target, 8);
            if (name.empty()) {
                image.WritePointer(target, addend);
                continue;
            }
            image.WritePointer(target, 0);
            result.Imports.push_back({name, rva, addend});
            result.Imports.back().Weak = (dynamicSection.DynSymData[symbolOffset + 4] >> 4) == STB_WEAK;
            result.Imports.back().Function = type == 7 || (type == 6 && dynamicSection.FunctionImports.contains(target));
            if (const auto module = dynamicSection.ImportModules.find(target); module != dynamicSection.ImportModules.end())
                result.Imports.back().Library = module->second;
        }
    };
    applyTable(dynamicSection.RelaData, false);
    applyTable(dynamicSection.RelaPltData, true);
    return result;
}

std::vector<std::uint8_t> WindowsRelocationBuilder::BuildBaseRelocations(const std::vector<std::uint32_t>& targets) const {
    std::map<std::uint32_t, std::vector<std::uint16_t>> pages;
    for (const auto target : targets) {
        auto& entries = pages[target & ~0xfffu];
        const auto entry = static_cast<std::uint16_t>(0xa000u | (target & 0xfffu));
        if (std::find(entries.begin(), entries.end(), entry) == entries.end())
            entries.push_back(entry);
    }
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
