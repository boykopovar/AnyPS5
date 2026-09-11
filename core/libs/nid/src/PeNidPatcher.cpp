#include <nid/PeNidPatcher.hpp>
#include <nid/NidResolver.hpp>
#include <nid/NidPatcherUtils.hpp>
#include <nid/NidCompute.hpp>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
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

    const std::size_t sectionTableOffset = coffHeaderOffset + 20u + sizeOfOptionalHeader;
    const auto symbolTable = Read<std::uint32_t>(pe, coffHeaderOffset + 8u);
    const auto symbolCount = Read<std::uint32_t>(pe, coffHeaderOffset + 12u);
    for (std::uint16_t i = 0; i < numberOfSections; ++i) {
        const auto offset = sectionTableOffset + i * sizeof(PeSectionHeader);
        auto section = Read<PeSectionHeader>(pe, offset);
        if (pe[offset] != '/' || !symbolTable) continue;
        const std::string reference(reinterpret_cast<const char*>(pe.data() + offset), 8);
        const auto stringOffset = std::stoul(reference.substr(1));
        const auto name = ReadCStr(pe, static_cast<std::size_t>(symbolTable) + static_cast<std::size_t>(symbolCount) * 18 + stringOffset);
        if (name == ".eh_frame") {
            const char shortName[8] = ".ehfram";
            std::memcpy(pe.data() + offset, shortName, 8);
        }
    }

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
    const std::size_t ordinalsArrayOffset = RvaToOffset(pe, exportTable.AddressOfNameOrdinals, peHeaderOffset, numberOfSections, sizeOfOptionalHeader);

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

    const auto fileAlignment = Read<std::uint32_t>(pe, optionalHeaderOffset + 36u);
    if (fileAlignment == 0u) throw std::runtime_error("invalid file alignment");

    std::uint32_t stringsRegionStart = std::numeric_limits<std::uint32_t>::max();
    for (std::uint32_t i = 0u; i < exportTable.NumberOfNames; ++i) {
        const std::uint32_t rvaFromSection = nameRvas[i] - edataSection.VirtualAddress;
        if (rvaFromSection < stringsRegionStart) stringsRegionStart = rvaFromSection;
    }
    if (stringsRegionStart == std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("could not determine strings region start");

    const std::uint32_t rawLimit = ((edataSection.SizeOfRawData + fileAlignment - 1u) / fileAlignment) * fileAlignment;

    std::vector<std::string> finalNames(exportTable.NumberOfNames);
    for (std::uint32_t i = 0u; i < exportTable.NumberOfNames; ++i) {
        const auto it = nidMap.find(names[i]);
        if (it == nidMap.end()) throw std::runtime_error("symbol not in nid map: " + names[i]);
        finalNames[i] = it->second;
    }

    std::uint32_t requiredSize = stringsRegionStart;
    for (std::uint32_t i = 0u; i < exportTable.NumberOfNames; ++i)
        requiredSize += static_cast<std::uint32_t>(finalNames[i].size()) + 1u;

    if (requiredSize > rawLimit)
        throw std::runtime_error("not enough raw space in edata section to repack export names");

    std::uint32_t writeOffset = stringsRegionStart;
    for (std::uint32_t i = 0u; i < exportTable.NumberOfNames; ++i) {
        const std::uint32_t newNameRva = edataSection.VirtualAddress + writeOffset;
        const std::size_t newNameFileOffset = static_cast<std::size_t>(edataSection.PointerToRawData) + writeOffset;

        if (newNameFileOffset + finalNames[i].size() + 1u > pe.size())
            throw std::runtime_error("repack write out of file bounds");

        std::memcpy(pe.data() + newNameFileOffset, finalNames[i].data(), finalNames[i].size());
        pe[newNameFileOffset + finalNames[i].size()] = 0u;
        Write(pe, namesArrayOffset + i * 4u, newNameRva);
        writeOffset += static_cast<std::uint32_t>(finalNames[i].size()) + 1u;
    }

    const std::uint32_t newVirtualSize = std::max(edataSection.VirtualSize, writeOffset);
    auto patchedSection = edataSection;
    patchedSection.VirtualSize = newVirtualSize;
    Write(pe, edataSectionOffset, patchedSection);

    struct ExportName {
        std::string Name;
        std::uint32_t Rva;
        std::uint16_t Ordinal;
    };

    std::vector<ExportName> exportNames;
    exportNames.reserve(exportTable.NumberOfNames);
    for (std::size_t i = 0; i < exportTable.NumberOfNames; ++i) {
        const auto nameRva = Read<std::uint32_t>(pe, namesArrayOffset + i * sizeof(std::uint32_t));
        const auto ordinal = Read<std::uint16_t>(pe, ordinalsArrayOffset + i * sizeof(std::uint16_t));
        if (ordinal >= exportTable.NumberOfFunctions)
            throw std::runtime_error("export name ordinal out of bounds");
        const auto nameOffset = RvaToOffset(pe, nameRva, peHeaderOffset, numberOfSections, sizeOfOptionalHeader);
        exportNames.push_back({ReadCStr(pe, nameOffset), nameRva, ordinal});
    }
    std::sort(exportNames.begin(), exportNames.end(), [](const ExportName& left, const ExportName& right) { return left.Name < right.Name; });
    for (std::size_t i = 0; i < exportNames.size(); ++i) {
        if (i != 0 && exportNames[i - 1].Name == exportNames[i].Name)
            throw std::runtime_error("duplicate patched export name: " + exportNames[i].Name);
        Write(pe, namesArrayOffset + i * sizeof(std::uint32_t), exportNames[i].Rva);
        Write(pe, ordinalsArrayOffset + i * sizeof(std::uint16_t), exportNames[i].Ordinal);
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
