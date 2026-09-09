#include <nid/PeNidPatcher.hpp>
#include <nid/NidResolver.hpp>
#include <nid/NidPatcherUtils.hpp>
#include <nid/NidCompute.hpp>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace Nid {

namespace {

std::size_t FindSectionOffsetByRva(const std::vector<std::uint8_t>& pe, std::uint32_t rva, std::uint32_t peHeaderOffset, std::uint16_t numberOfSections, std::uint32_t sizeOfOptionalHeader) {
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

std::size_t RvaToOffset(const std::vector<std::uint8_t>& pe, std::uint32_t rva, std::uint32_t peHeaderOffset, std::uint16_t numberOfSections, std::uint32_t sizeOfOptionalHeader) {
    const std::size_t sectionOffset = FindSectionOffsetByRva(pe, rva, peHeaderOffset, numberOfSections, sizeOfOptionalHeader);
    const auto section = Internal::Read<PeSectionHeader>(pe, sectionOffset);
    return static_cast<std::size_t>(section.PointerToRawData) + (rva - section.VirtualAddress);
}

}

void PeNidPatcher::PatchNids(std::vector<std::uint8_t>& pe, const std::string& libraryName) const {
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

    const std::size_t exportDirOffset = RvaToOffset(pe, exportDir.VirtualAddress, peHeaderOffset, numberOfSections, sizeOfOptionalHeader);
    const auto exportTable = Read<PeExportDirectory>(pe, exportDirOffset);

    if (exportTable.NumberOfNames == 0u) throw std::runtime_error("no exported names");

    const std::size_t namesArrayOffset = RvaToOffset(pe, exportTable.AddressOfNames, peHeaderOffset, numberOfSections, sizeOfOptionalHeader);

    std::vector<std::uint32_t> nameRvas(exportTable.NumberOfNames);
    std::vector<std::string> names(exportTable.NumberOfNames);
    for (std::uint32_t i = 0u; i < exportTable.NumberOfNames; ++i) {
        const auto nameRva = Read<std::uint32_t>(pe, namesArrayOffset + i * 4u);
        const std::size_t nameOffset = RvaToOffset(pe, nameRva, peHeaderOffset, numberOfSections, sizeOfOptionalHeader);
        const std::string name = ReadCStr(pe, nameOffset);
        if (name.empty()) throw std::runtime_error("empty exported name");
        nameRvas[i] = nameRva;
        names[i] = name;
    }

    const auto nidMap = ResolveNids(names, libraryName);

    const std::size_t edataSectionOffset = FindSectionOffsetByRva(pe, exportDir.VirtualAddress, peHeaderOffset, numberOfSections, sizeOfOptionalHeader);
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
        const auto it = nidMap.find(names[i]);
        if (it == nidMap.end()) throw std::runtime_error("symbol not in nid map: " + names[i]);
        const std::string& nid = it->second;

        const std::size_t oldNameOffset = RvaToOffset(pe, nameRvas[i], peHeaderOffset, numberOfSections, sizeOfOptionalHeader);

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

    constexpr std::size_t kImportDirIndex = 1u;
    if (dataDirectoryOffset + (kImportDirIndex + 1u) * sizeof(PeDataDirectory) > pe.size())
        return;

    const auto importDir = Read<PeDataDirectory>(pe, dataDirectoryOffset + kImportDirIndex * sizeof(PeDataDirectory));
    if (importDir.VirtualAddress == 0u || importDir.Size == 0u)
        return;

    struct PeImportDescriptor {
        std::uint32_t OriginalFirstThunk;
        std::uint32_t TimeDateStamp;
        std::uint32_t ForwarderChain;
        std::uint32_t Name;
        std::uint32_t FirstThunk;
    };

    struct PeImportByName {
        std::uint16_t Hint;
        char Name[1];
    };

    std::size_t importDescOffset = RvaToOffset(pe, importDir.VirtualAddress, peHeaderOffset, numberOfSections, sizeOfOptionalHeader);

    while (true) {
        if (importDescOffset + sizeof(PeImportDescriptor) > pe.size())
            throw std::runtime_error("import descriptor out of bounds");

        const auto desc = Read<PeImportDescriptor>(pe, importDescOffset);
        if (desc.OriginalFirstThunk == 0u && desc.FirstThunk == 0u)
            break;

        const std::uint32_t thunkRva = desc.OriginalFirstThunk != 0u ? desc.OriginalFirstThunk : desc.FirstThunk;
        std::size_t thunkOffset = RvaToOffset(pe, thunkRva, peHeaderOffset, numberOfSections, sizeOfOptionalHeader);

        const bool is64 = magic == 0x20bu;
        const std::size_t thunkEntrySize = is64 ? 8u : 4u;
        const std::uint64_t ordinalFlag = is64 ? (std::uint64_t{1} << 63u) : (std::uint64_t{1} << 31u);

        while (true) {
            if (thunkOffset + thunkEntrySize > pe.size())
                throw std::runtime_error("thunk entry out of bounds");

            const std::uint64_t thunk = is64
                ? Read<std::uint64_t>(pe, thunkOffset)
                : static_cast<std::uint64_t>(Read<std::uint32_t>(pe, thunkOffset));

            if (thunk == 0u)
                break;

            if (!(thunk & ordinalFlag)) {
                const auto hintNameRva = static_cast<std::uint32_t>(thunk & 0x7fffffffffffffffu);
                const std::size_t hintNameOffset = RvaToOffset(pe, hintNameRva, peHeaderOffset, numberOfSections, sizeOfOptionalHeader);
                if (hintNameOffset + 2u >= pe.size())
                    throw std::runtime_error("import by name out of bounds");

                const std::string funcName = ReadCStr(pe, hintNameOffset + 2u);
                if (!funcName.empty()) {
                    const bool hasNidPostfix = funcName.size() >= kNidPostfixLen &&
                        funcName.compare(funcName.size() - kNidPostfixLen, kNidPostfixLen, kNidPostfix) == 0;
                    const bool hasScePrefix = funcName.size() >= 3u &&
                        std::tolower(static_cast<unsigned char>(funcName[0])) == 's' &&
                        std::tolower(static_cast<unsigned char>(funcName[1])) == 'c' &&
                        std::tolower(static_cast<unsigned char>(funcName[2])) == 'e';
                    if (hasNidPostfix || hasScePrefix) {
                        const std::string nid = ResolveOneName(funcName);
                        if (nid.size() > funcName.size())
                            throw std::runtime_error("import NID longer than original name: " + funcName);
                        std::memcpy(pe.data() + hintNameOffset + 2u, nid.data(), nid.size());
                        pe[hintNameOffset + 2u + nid.size()] = 0u;
                    }
                }
            }

            thunkOffset += thunkEntrySize;
        }

        importDescOffset += sizeof(PeImportDescriptor);
    }
}

}
