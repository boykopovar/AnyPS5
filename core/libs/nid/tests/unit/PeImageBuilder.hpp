#pragma once

#include <nid/NidPatcherUtils.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace NidTests {

struct PeExport {
    std::string Name;
    std::uint16_t Ordinal;
};

struct PeImport {
    std::string Name;
    bool ByOrdinal = false;
};

struct PeImageLayout {
    static constexpr std::uint32_t PeHeader = 0x80;
    static constexpr std::uint32_t Coff = PeHeader + 4;
    static constexpr std::uint32_t Optional = Coff + 20;
    static constexpr std::uint32_t SectionRaw = 0x200;
    static constexpr std::uint32_t SectionRva = 0x1000;
    static constexpr std::uint32_t ExportDirectory = 0x00;
    static constexpr std::uint32_t Functions = 0x28;
    static constexpr std::uint32_t ExportStrings = 0x80;
    static constexpr std::uint32_t ImportDescriptors = 0x140;
    static constexpr std::uint32_t ImportThunks = 0x170;
    static constexpr std::uint32_t ImportNames = 0x1A0;
    static constexpr std::uint32_t SymbolTable = 0x400;
};

class PeImageBuilder {
public:
    std::vector<PeExport> Exports;
    std::vector<PeImport> Imports;
    std::uint16_t Magic = 0x20b;
    std::uint32_t FileAlignment = 0x200;
    std::uint32_t RawSize = 0x200;
    std::uint32_t VirtualSize = 0x200;
    bool EhFrameSection = false;

    static std::uint32_t DataDirectories(std::uint16_t magic) {
        return PeImageLayout::Optional + (magic == 0x20b ? 112u : 96u);
    }

    static std::uint32_t OptionalHeaderSize(std::uint16_t magic) {
        return (magic == 0x20b ? 112u : 96u) + 16u * 8u;
    }

    static std::uint32_t SectionTable(std::uint16_t magic) {
        return PeImageLayout::Optional + OptionalHeaderSize(magic);
    }

    static std::uint32_t NamesArray(std::size_t exportCount) {
        return PeImageLayout::Functions + static_cast<std::uint32_t>(exportCount) * 4u;
    }

    static std::uint32_t OrdinalsArray(std::size_t exportCount) {
        return NamesArray(exportCount) + static_cast<std::uint32_t>(exportCount) * 4u;
    }

    static std::size_t FileOffset(std::uint32_t sectionOffset) {
        return PeImageLayout::SectionRaw + sectionOffset;
    }

    std::vector<std::uint8_t> Build() const {
        using Nid::Internal::Write;
        std::vector<std::uint8_t> image(PeImageLayout::SymbolTable + 0x40, 0);
        image[0] = 'M';
        image[1] = 'Z';
        Write<std::uint32_t>(image, 0x3c, PeImageLayout::PeHeader);
        std::memcpy(image.data() + PeImageLayout::PeHeader, "PE\0\0", 4);
        const std::uint16_t sectionCount = EhFrameSection ? 2 : 1;
        Write<std::uint16_t>(image, PeImageLayout::Coff, 0x8664);
        Write<std::uint16_t>(image, PeImageLayout::Coff + 2, sectionCount);
        Write<std::uint32_t>(image, PeImageLayout::Coff + 8, EhFrameSection ? PeImageLayout::SymbolTable : 0u);
        Write<std::uint16_t>(image, PeImageLayout::Coff + 16, static_cast<std::uint16_t>(OptionalHeaderSize(Magic)));
        Write<std::uint16_t>(image, PeImageLayout::Optional, Magic);
        Write<std::uint32_t>(image, PeImageLayout::Optional + 36, FileAlignment);
        Write<std::uint32_t>(image, DataDirectories(Magic) - 4, 16u);
        Write<std::uint32_t>(image, DataDirectories(Magic), PeImageLayout::SectionRva + PeImageLayout::ExportDirectory);
        Write<std::uint32_t>(image, DataDirectories(Magic) + 4, 40u);
        if (!Imports.empty()) {
            Write<std::uint32_t>(image, DataDirectories(Magic) + 8, PeImageLayout::SectionRva + PeImageLayout::ImportDescriptors);
            Write<std::uint32_t>(image, DataDirectories(Magic) + 12, 40u);
        }
        WriteSection(image, SectionTable(Magic), ".edata", VirtualSize, PeImageLayout::SectionRva, RawSize, PeImageLayout::SectionRaw);
        if (EhFrameSection) {
            WriteSection(image, SectionTable(Magic) + 40, "/4", 0x10, 0x8000, 0x10, PeImageLayout::SymbolTable + 0x20);
            const char longName[] = ".eh_frame";
            std::memcpy(image.data() + PeImageLayout::SymbolTable + 4, longName, sizeof(longName));
        }
        WriteExports(image);
        WriteImports(image);
        return image;
    }

private:
    static void WriteSection(std::vector<std::uint8_t>& image, std::uint32_t offset, const char* name, std::uint32_t virtualSize,
                             std::uint32_t virtualAddress, std::uint32_t rawSize, std::uint32_t rawPointer) {
        using Nid::Internal::Write;
        std::memcpy(image.data() + offset, name, std::min<std::size_t>(std::strlen(name), 8));
        Write<std::uint32_t>(image, offset + 8, virtualSize);
        Write<std::uint32_t>(image, offset + 12, virtualAddress);
        Write<std::uint32_t>(image, offset + 16, rawSize);
        Write<std::uint32_t>(image, offset + 20, rawPointer);
    }

    void WriteExports(std::vector<std::uint8_t>& image) const {
        using Nid::Internal::Write;
        const auto count = static_cast<std::uint32_t>(Exports.size());
        const auto directory = FileOffset(PeImageLayout::ExportDirectory);
        Write<std::uint32_t>(image, directory + 16, 1u);
        Write<std::uint32_t>(image, directory + 20, count);
        Write<std::uint32_t>(image, directory + 24, count);
        Write<std::uint32_t>(image, directory + 28, PeImageLayout::SectionRva + PeImageLayout::Functions);
        Write<std::uint32_t>(image, directory + 32, PeImageLayout::SectionRva + NamesArray(count));
        Write<std::uint32_t>(image, directory + 36, PeImageLayout::SectionRva + OrdinalsArray(count));
        std::uint32_t stringOffset = PeImageLayout::ExportStrings;
        for (std::uint32_t index = 0; index < count; ++index) {
            Write<std::uint32_t>(image, FileOffset(PeImageLayout::Functions + index * 4u), 0x2000u + index);
            Write<std::uint32_t>(image, FileOffset(NamesArray(count) + index * 4u), PeImageLayout::SectionRva + stringOffset);
            Write<std::uint16_t>(image, FileOffset(OrdinalsArray(count) + index * 2u), Exports[index].Ordinal);
            std::memcpy(image.data() + FileOffset(stringOffset), Exports[index].Name.data(), Exports[index].Name.size());
            stringOffset += static_cast<std::uint32_t>(Exports[index].Name.size()) + 1u;
        }
    }

    void WriteImports(std::vector<std::uint8_t>& image) const {
        using Nid::Internal::Write;
        if (Imports.empty()) return;
        const auto descriptor = FileOffset(PeImageLayout::ImportDescriptors);
        Write<std::uint32_t>(image, descriptor, PeImageLayout::SectionRva + PeImageLayout::ImportThunks);
        Write<std::uint32_t>(image, descriptor + 16, PeImageLayout::SectionRva + PeImageLayout::ImportThunks);
        const bool wide = Magic == 0x20b;
        const std::uint32_t thunkSize = wide ? 8u : 4u;
        std::uint32_t nameOffset = PeImageLayout::ImportNames;
        for (std::uint32_t index = 0; index < Imports.size(); ++index) {
            const auto thunk = FileOffset(PeImageLayout::ImportThunks + index * thunkSize);
            const std::uint64_t value = Imports[index].ByOrdinal
                ? (wide ? (std::uint64_t{1} << 63u) : (std::uint64_t{1} << 31u)) | (index + 1u)
                : std::uint64_t{PeImageLayout::SectionRva + nameOffset};
            if (wide) {
                Write<std::uint64_t>(image, thunk, value);
            } else {
                Write<std::uint32_t>(image, thunk, static_cast<std::uint32_t>(value));
            }
            if (Imports[index].ByOrdinal) continue;
            std::memcpy(image.data() + FileOffset(nameOffset + 2u), Imports[index].Name.data(), Imports[index].Name.size());
            nameOffset += static_cast<std::uint32_t>(Imports[index].Name.size()) + 3u;
        }
    }
};

inline std::vector<std::string> ExportNames(const std::vector<std::uint8_t>& image, std::size_t exportCount) {
    std::vector<std::string> names;
    for (std::size_t index = 0; index < exportCount; ++index) {
        const auto rva = Nid::Internal::Read<std::uint32_t>(image, PeImageBuilder::FileOffset(PeImageBuilder::NamesArray(exportCount)) + index * 4u);
        names.push_back(Nid::Internal::ReadCStr(image, PeImageLayout::SectionRaw + (rva - PeImageLayout::SectionRva)));
    }
    return names;
}

inline std::vector<std::uint16_t> ExportOrdinals(const std::vector<std::uint8_t>& image, std::size_t exportCount) {
    std::vector<std::uint16_t> ordinals;
    for (std::size_t index = 0; index < exportCount; ++index) {
        ordinals.push_back(Nid::Internal::Read<std::uint16_t>(image, PeImageBuilder::FileOffset(PeImageBuilder::OrdinalsArray(exportCount)) + index * 2u));
    }
    return ordinals;
}

} // namespace NidTests
