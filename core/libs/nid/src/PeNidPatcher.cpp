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
#include <unordered_map>
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

void PeNidPatcher::PatchNids(std::vector<std::uint8_t>& pe, const std::string& libraryName, const std::unordered_set<std::string>& excludedExports, const PatchOptions& options) const {
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
    for (std::uint16_t i = 0; !options.executable && i < numberOfSections; ++i) {
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

    const std::size_t fixedOptionalSize = dataDirectoryOffset - optionalHeaderOffset;
    if (sizeOfOptionalHeader < fixedOptionalSize) throw std::runtime_error("truncated optional header");
    std::size_t directoryCount = 0u;
    if (sizeOfOptionalHeader >= fixedOptionalSize + sizeof(std::uint32_t)) {
        const auto declared = Read<std::uint32_t>(pe, optionalHeaderOffset + fixedOptionalSize - sizeof(std::uint32_t));
        directoryCount = std::min<std::size_t>(declared, (sizeOfOptionalHeader - fixedOptionalSize) / sizeof(PeDataDirectory));
    }

    if (!options.executable) {
        if (directoryCount < 1u) throw std::runtime_error("no export directory");
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

        const auto nidMap = ResolveNids(names, libraryName, excludedExports);

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

        std::uint32_t rawLimit = ((edataSection.SizeOfRawData + fileAlignment - 1u) / fileAlignment) * fileAlignment;
        const auto dllName = ReadCStr(pe, RvaToOffset(pe, exportTable.Name, peHeaderOffset, numberOfSections, sizeOfOptionalHeader));
        const bool repackDllName = exportTable.Name >= edataSection.VirtualAddress &&
            exportTable.Name - edataSection.VirtualAddress < rawLimit;
        if (repackDllName) stringsRegionStart = std::min(stringsRegionStart, exportTable.Name - edataSection.VirtualAddress);
        struct Forwarder {
            std::size_t EntryOffset;
            std::string Target;
        };
        std::vector<Forwarder> forwarders;
        std::vector<std::uint32_t> directRvas;
        const auto functionsArrayOffset = RvaToOffset(pe, exportTable.AddressOfFunctions, peHeaderOffset, numberOfSections, sizeOfOptionalHeader);
        for (std::uint32_t index = 0u; index < exportTable.NumberOfFunctions; ++index) {
            const auto entryOffset = functionsArrayOffset + index * sizeof(std::uint32_t);
            const auto rva = Read<std::uint32_t>(pe, entryOffset);
            if (rva < exportDir.VirtualAddress || std::uint64_t{rva} - exportDir.VirtualAddress >= exportDir.Size) {
                directRvas.push_back(rva);
                continue;
            }
            const auto target = ReadCStr(pe, RvaToOffset(pe, rva, peHeaderOffset, numberOfSections, sizeOfOptionalHeader));
            if (target.size() + 1u > exportDir.Size - (rva - exportDir.VirtualAddress))
                throw std::runtime_error("forwarder string outside export directory");
            forwarders.push_back({entryOffset, target});
            stringsRegionStart = std::min(stringsRegionStart, rva - edataSection.VirtualAddress);
        }
        for (const auto rva : directRvas) {
            if (rva >= edataSection.VirtualAddress && rva - edataSection.VirtualAddress >= stringsRegionStart)
                rawLimit = std::min(rawLimit, rva - edataSection.VirtualAddress);
        }
        const auto reserveTable = [&](std::uint32_t rva, std::uint64_t size) {
            if (rva < edataSection.VirtualAddress || rva - edataSection.VirtualAddress >= rawLimit) return;
            const auto end = std::uint64_t{rva - edataSection.VirtualAddress} + size;
            if (end > rawLimit) throw std::runtime_error("export table outside edata section");
            stringsRegionStart = std::max(stringsRegionStart, static_cast<std::uint32_t>(end));
        };
        reserveTable(exportDir.VirtualAddress, sizeof(PeExportDirectory));
        reserveTable(exportTable.AddressOfFunctions, std::uint64_t{exportTable.NumberOfFunctions} * 4u);
        reserveTable(exportTable.AddressOfNames, std::uint64_t{exportTable.NumberOfNames} * 4u);
        reserveTable(exportTable.AddressOfNameOrdinals, std::uint64_t{exportTable.NumberOfNames} * 2u);

        std::vector<std::string> finalNames(exportTable.NumberOfNames);
        for (std::uint32_t i = 0u; i < exportTable.NumberOfNames; ++i) {
            const auto it = nidMap.find(names[i]);
            if (it == nidMap.end()) throw std::runtime_error("symbol not in nid map: " + names[i]);
            finalNames[i] = it->second;
        }

        std::vector<std::uint8_t> strings;
        std::unordered_map<std::string, std::uint32_t> stringRvas;
        const auto storeString = [&](const std::string& name) {
            const auto existing = stringRvas.find(name);
            if (existing != stringRvas.end()) return existing->second;
            const auto rva = edataSection.VirtualAddress + stringsRegionStart + static_cast<std::uint32_t>(strings.size());
            stringRvas.emplace(name, rva);
            strings.insert(strings.end(), name.begin(), name.end());
            strings.push_back(0u);
            return rva;
        };
        std::vector<std::uint32_t> forwarderRvas;
        for (const auto& forwarder : forwarders) forwarderRvas.push_back(storeString(forwarder.Target));
        if (!forwarders.empty() && (std::uint64_t{edataSection.VirtualAddress} + stringsRegionStart < exportDir.VirtualAddress ||
            std::uint64_t{edataSection.VirtualAddress} + stringsRegionStart + strings.size() > std::uint64_t{exportDir.VirtualAddress} + exportDir.Size))
            throw std::runtime_error("not enough export directory space to repack forwarders");
        const auto dllNameRva = repackDllName ? storeString(dllName) : exportTable.Name;
        std::vector<std::uint32_t> finalNameRvas;
        for (const auto& name : finalNames) finalNameRvas.push_back(storeString(name));
        const auto requiredSize = std::uint64_t{stringsRegionStart} + strings.size();
        if (requiredSize > rawLimit)
            throw std::runtime_error("not enough raw space in edata section to repack export names");
        const auto directoryEnd = std::max(std::uint64_t{exportDir.VirtualAddress} + exportDir.Size,
            std::uint64_t{edataSection.VirtualAddress} + requiredSize);
        for (const auto rva : directRvas) {
            if (rva != 0u && rva >= exportDir.VirtualAddress && std::uint64_t{rva} < directoryEnd)
                throw std::runtime_error("repacked export directory would cover a direct export");
        }
        const auto directorySize = directoryEnd - exportDir.VirtualAddress;
        if (directorySize > std::numeric_limits<std::uint32_t>::max()) throw std::runtime_error("export directory too large");
        const auto stringFileOffset = static_cast<std::size_t>(edataSection.PointerToRawData) + stringsRegionStart;
        if (stringFileOffset > pe.size() || strings.size() > pe.size() - stringFileOffset)
            throw std::runtime_error("repack write out of file bounds");
        std::memcpy(pe.data() + stringFileOffset, strings.data(), strings.size());
        for (std::size_t index = 0u; index < finalNameRvas.size(); ++index)
            Write(pe, namesArrayOffset + index * 4u, finalNameRvas[index]);
        for (std::size_t index = 0u; index < forwarders.size(); ++index)
            Write(pe, forwarders[index].EntryOffset, forwarderRvas[index]);
        Write(pe, exportDirOffset + offsetof(PeExportDirectory, Name), dllNameRva);
        Write(pe, dataDirectoryOffset + offsetof(PeDataDirectory, Size), static_cast<std::uint32_t>(directorySize));

        const std::uint32_t newVirtualSize = std::max(edataSection.VirtualSize, static_cast<std::uint32_t>(requiredSize));
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

    }

    constexpr std::size_t kImportDirIndex = 1u;
    if (directoryCount < kImportDirIndex + 1u)
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

    struct ImportUse {
        std::size_t NameOffset;
        std::size_t LookupOffset;
        std::size_t AddressOffset;
        std::string Name;
    };
    std::vector<ImportUse> imports;
    std::vector<std::string> importNames;
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
                    const auto addressOffset = RvaToOffset(pe, desc.FirstThunk, peHeaderOffset, numberOfSections, sizeOfOptionalHeader) + thunkOffset - RvaToOffset(pe, thunkRva, peHeaderOffset, numberOfSections, sizeOfOptionalHeader);
                    imports.push_back({hintNameOffset + 2u, thunkOffset, addressOffset, funcName});
                    importNames.push_back(funcName);
                }
            }

            thunkOffset += thunkEntrySize;
        }

        importDescOffset += sizeof(PeImportDescriptor);
    }
    const auto importMap = ResolveImportNids(importNames, excludedExports, options);
    const auto fitsInPlace = [&](const ImportUse& item) {
        return !options.executable && importMap.at(item.Name).size() <= item.Name.size();
    };
    const bool growsImports = options.executable || std::any_of(imports.begin(), imports.end(), [&](const ImportUse& item) {
        return !fitsInPlace(item) && importMap.at(item.Name) != item.Name;
    });
    for (const auto index : {4u, 11u, 13u}) {
        if (index >= directoryCount) continue;
        const auto directory = Read<PeDataDirectory>(pe, dataDirectoryOffset + index * sizeof(PeDataDirectory));
        if (growsImports && (directory.VirtualAddress || directory.Size))
            throw std::runtime_error("signed, bound or delay-loaded PE imports are not supported when import names move to a new section");
    }
    for (const auto& item : imports) {
        if (!fitsInPlace(item)) continue;
        const auto& name = importMap.at(item.Name);
        std::memcpy(pe.data() + item.NameOffset, name.c_str(), name.size() + 1);
    }
    std::vector<std::uint8_t> strings;
    std::unordered_map<std::string, std::uint32_t> stringOffsets;
    for (const auto& item : imports) {
        const auto& name = importMap.at(item.Name);
        if (name == item.Name || fitsInPlace(item) || stringOffsets.contains(name)) continue;
        if (strings.size() & 1u) strings.push_back(0);
        if (strings.size() > 0xffffffffu - name.size() - 3u) throw std::runtime_error("PE import string table overflow");
        stringOffsets.emplace(name, static_cast<std::uint32_t>(strings.size()));
        strings.insert(strings.end(), {0, 0});
        strings.insert(strings.end(), name.begin(), name.end());
        strings.push_back(0);
    }
    if (strings.empty()) return;
    const auto fileAlignment = Read<std::uint32_t>(pe, optionalHeaderOffset + 36u);
    const auto sectionAlignment = Read<std::uint32_t>(pe, optionalHeaderOffset + 32u);
    const auto align = [](std::uint64_t value, std::uint32_t alignment) {
        if (!alignment || (alignment & (alignment - 1u))) throw std::runtime_error("invalid PE alignment");
        const auto result = (value + alignment - 1u) & ~std::uint64_t{alignment - 1u};
        if (result > 0xffffffffu) throw std::runtime_error("PE section overflow");
        return static_cast<std::uint32_t>(result);
    };
    const auto newHeader = sectionTableOffset + numberOfSections * sizeof(PeSectionHeader);
    auto headerLimit = std::size_t{Read<std::uint32_t>(pe, optionalHeaderOffset + 60u)};
    std::uint64_t virtualEnd = headerLimit;
    for (std::uint16_t index = 0; index < numberOfSections; ++index) {
        const auto section = Read<PeSectionHeader>(pe, sectionTableOffset + index * sizeof(PeSectionHeader));
        if (section.PointerToRawData) headerLimit = std::min(headerLimit, std::size_t{section.PointerToRawData});
        virtualEnd = std::max(virtualEnd, std::uint64_t{section.VirtualAddress} + std::max(section.VirtualSize, section.SizeOfRawData));
    }
    if (numberOfSections == 0xffffu || newHeader + sizeof(PeSectionHeader) > headerLimit || headerLimit > pe.size()) throw std::runtime_error("no room for PE import section header");
    PeSectionHeader section{};
    const char sectionName[] = ".nidimp";
    std::memcpy(section.Name, sectionName, sizeof(sectionName));
    section.VirtualSize = static_cast<std::uint32_t>(strings.size());
    section.VirtualAddress = align(virtualEnd, sectionAlignment);
    section.PointerToRawData = align(pe.size(), fileAlignment);
    section.SizeOfRawData = align(strings.size(), fileAlignment);
    section.Characteristics = 0x40000040u;
    const auto imageSize = align(std::uint64_t{section.VirtualAddress} + section.VirtualSize, sectionAlignment);
    const auto rawEnd = std::uint64_t{section.PointerToRawData} + section.SizeOfRawData;
    if (rawEnd > 0xffffffffu) throw std::runtime_error("PE file size overflow");
    pe.resize(static_cast<std::size_t>(rawEnd), 0);
    std::copy(strings.begin(), strings.end(), pe.begin() + section.PointerToRawData);
    Write(pe, newHeader, section);
    Write(pe, coffHeaderOffset + 2u, static_cast<std::uint16_t>(numberOfSections + 1u));
    Write(pe, optionalHeaderOffset + 56u, imageSize);
    Write(pe, optionalHeaderOffset + 64u, std::uint32_t{0});
    for (const auto& item : imports) {
        const auto& name = importMap.at(item.Name);
        if (name == item.Name || fitsInPlace(item)) continue;
        const auto rva = section.VirtualAddress + stringOffsets.at(name);
        for (const auto offset : {item.LookupOffset, item.AddressOffset}) {
            if (magic == 0x20bu) Write(pe, offset, std::uint64_t{rva});
            else Write(pe, offset, rva);
        }
    }

}

}
