#include <nid/PePatcher.hpp>
#include <nid/NidCompute.hpp>
#include "NidPatcherUtils.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace Nid {

namespace {

struct PeDataDirectory {
    std::uint32_t VirtualAddress;
    std::uint32_t Size;
};

struct PeExportDirectory {
    std::uint32_t Characteristics;
    std::uint32_t TimeDateStamp;
    std::uint16_t MajorVersion;
    std::uint16_t MinorVersion;
    std::uint32_t Name;
    std::uint32_t Base;
    std::uint32_t NumberOfFunctions;
    std::uint32_t NumberOfNames;
    std::uint32_t AddressOfFunctions;
    std::uint32_t AddressOfNames;
    std::uint32_t AddressOfNameOrdinals;
};

struct PeSectionHeader {
    std::uint8_t Name[8];
    std::uint32_t VirtualSize;
    std::uint32_t VirtualAddress;
    std::uint32_t SizeOfRawData;
    std::uint32_t PointerToRawData;
    std::uint32_t PointerToRelocations;
    std::uint32_t PointerToLinenumbers;
    std::uint16_t NumberOfRelocations;
    std::uint16_t NumberOfLinenumbers;
    std::uint32_t Characteristics;
};

std::size_t _findSectionOffsetByRva(const std::vector<std::uint8_t>& pe, std::uint32_t rva, std::uint32_t peHeaderOffset, std::uint16_t numberOfSections, std::uint32_t sizeOfOptionalHeader) {
    const std::size_t sectionTableOffset = static_cast<std::size_t>(peHeaderOffset) + 4u + 20u + sizeOfOptionalHeader;
    for (std::uint16_t i = 0u; i < numberOfSections; ++i) {
        const std::size_t sectionOffset = sectionTableOffset + i * sizeof(PeSectionHeader);
        if (sectionOffset + sizeof(PeSectionHeader) > pe.size()) throw std::runtime_error("section header out of bounds");
        const auto section = Internal::Read<PeSectionHeader>(pe, sectionOffset);
        const std::uint32_t effectiveSize = section.VirtualSize != 0u ? section.VirtualSize : section.SizeOfRawData;
        if (rva >= section.VirtualAddress && rva < section.VirtualAddress + effectiveSize)
            return sectionOffset;
    }
    throw std::runtime_error("rva not mapped to any section");
}

std::size_t _rvaToOffset(const std::vector<std::uint8_t>& pe, std::uint32_t rva, std::uint32_t peHeaderOffset, std::uint16_t numberOfSections, std::uint32_t sizeOfOptionalHeader) {
    const std::size_t sectionOffset = _findSectionOffsetByRva(pe, rva, peHeaderOffset, numberOfSections, sizeOfOptionalHeader);
    const auto section = Internal::Read<PeSectionHeader>(pe, sectionOffset);
    return static_cast<std::size_t>(section.PointerToRawData) + (rva - section.VirtualAddress);
}

}

void PePatcher::PatchNids(std::vector<std::uint8_t>& pe, const std::string& libraryName) const {
    using namespace Internal;

    if (pe.size() < 0x40) throw std::runtime_error("file too small");

    const auto peHeaderOffset = Read<std::uint32_t>(pe, 0x3cu);
    if (static_cast<std::size_t>(peHeaderOffset) + 4u > pe.size()) throw std::runtime_error("invalid pe header offset");
    if (pe[peHeaderOffset] != 'P' || pe[peHeaderOffset + 1u] != 'E' || pe[peHeaderOffset + 2u] != 0u || pe[peHeaderOffset + 3u] != 0u)
        throw std::runtime_error("not a PE file");

    const std::size_t coffHeaderOffset = static_cast<std::size_t>(peHeaderOffset) + 4u;
    const auto numberOfSections = Read<std::uint16_t>(pe, coffHeaderOffset + 2u);
    const auto sizeOfOptionalHeader = Read<std::uint16_t>(pe, coffHeaderOffset + 16u);
    if (sizeOfOptionalHeader == 0u) throw std::runtime_error("no optional header");

    const std::size_t optionalHeaderOffset = coffHeaderOffset + 20u;
    const auto magic = Read<std::uint16_t>(pe, optionalHeaderOffset);

    std::size_t dataDirectoryOffset;
    if (magic == 0x20bu) {
        dataDirectoryOffset = optionalHeaderOffset + 112u;
    } else if (magic == 0x10bu) {
        dataDirectoryOffset = optionalHeaderOffset + 96u;
    } else {
        throw std::runtime_error("unsupported optional header magic");
    }

    const auto exportDir = Read<PeDataDirectory>(pe, dataDirectoryOffset);
    if (exportDir.VirtualAddress == 0u) throw std::runtime_error("no export directory");

    const std::size_t exportDirOffset = _rvaToOffset(pe, exportDir.VirtualAddress, peHeaderOffset, numberOfSections, sizeOfOptionalHeader);
    const auto exportTable = Read<PeExportDirectory>(pe, exportDirOffset);

    if (exportTable.NumberOfNames == 0u) throw std::runtime_error("no exported names");

    const std::size_t namesArrayOffset = _rvaToOffset(pe, exportTable.AddressOfNames, peHeaderOffset, numberOfSections, sizeOfOptionalHeader);

    std::vector<std::uint32_t> nameRvas(exportTable.NumberOfNames);
    std::vector<std::string> names(exportTable.NumberOfNames);
    for (std::uint32_t i = 0u; i < exportTable.NumberOfNames; ++i) {
        const auto nameRva = Read<std::uint32_t>(pe, namesArrayOffset + i * 4u);
        const std::size_t nameOffset = _rvaToOffset(pe, nameRva, peHeaderOffset, numberOfSections, sizeOfOptionalHeader);
        const std::string name = ReadCStr(pe, nameOffset);
        if (name.empty()) throw std::runtime_error("empty exported name");
        nameRvas[i] = nameRva;
        names[i] = name;
    }

    const std::size_t edataSectionOffset = _findSectionOffsetByRva(pe, exportDir.VirtualAddress, peHeaderOffset, numberOfSections, sizeOfOptionalHeader);
    const auto edataSection = Read<PeSectionHeader>(pe, edataSectionOffset);

    const auto sectionAlignment = Read<std::uint32_t>(pe, optionalHeaderOffset + 32u);
    const auto fileAlignment = Read<std::uint32_t>(pe, optionalHeaderOffset + 36u);
    if (sectionAlignment == 0u || fileAlignment == 0u) throw std::runtime_error("invalid section/file alignment");

    const std::uint32_t rawLimit = ((edataSection.SizeOfRawData + fileAlignment - 1u) / fileAlignment) * fileAlignment;
    const std::uint32_t virtualLimit = ((edataSection.VirtualSize + sectionAlignment - 1u) / sectionAlignment) * sectionAlignment;

    std::uint32_t usedEnd = 0u;
    for (std::uint32_t i = 0u; i < exportTable.NumberOfNames; ++i)
        usedEnd = std::max(usedEnd, nameRvas[i] - edataSection.VirtualAddress + static_cast<std::uint32_t>(names[i].size()) + 1u);

    for (std::uint32_t i = 0u; i < exportTable.NumberOfNames; ++i) {
        const std::string nid = ComputeNid(StripNidPostfix(names[i]), libraryName);
        const std::size_t oldNameOffset = _rvaToOffset(pe, nameRvas[i], peHeaderOffset, numberOfSections, sizeOfOptionalHeader);

        if (nid.size() <= names[i].size()) {
            std::memcpy(pe.data() + oldNameOffset, nid.data(), nid.size());
            pe[oldNameOffset + nid.size()] = 0u;
            for (std::size_t j = nid.size() + 1u; j < names[i].size() + 1u; ++j)
                pe[oldNameOffset + j] = 0u;
            continue;
        }

        const std::uint32_t newSize = static_cast<std::uint32_t>(nid.size()) + 1u;
        if (usedEnd + newSize > rawLimit || edataSection.VirtualAddress + usedEnd + newSize > edataSection.VirtualAddress + virtualLimit)
            throw std::runtime_error("no free space left in edata section to relocate export name");

        const std::uint32_t newNameRva = edataSection.VirtualAddress + usedEnd;
        const std::size_t newNameOffset = static_cast<std::size_t>(edataSection.PointerToRawData) + usedEnd;

        std::memcpy(pe.data() + newNameOffset, nid.data(), nid.size());
        pe[newNameOffset + nid.size()] = 0u;
        Write(pe, namesArrayOffset + i * 4u, newNameRva);

        usedEnd += newSize;
    }
}

}
