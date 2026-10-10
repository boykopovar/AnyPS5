#pragma once

#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace RelinkerUnitTests {

using Bytes = std::vector<std::uint8_t>;

inline constexpr std::uint32_t PtLoad = 1;
inline constexpr std::uint32_t PtDynamic = 2;
inline constexpr std::uint32_t PtTls = 7;
inline constexpr std::uint32_t PtSceDynlibData = 0x61000000;
inline constexpr std::uint32_t PfExecute = 1;
inline constexpr std::uint32_t PfWrite = 2;
inline constexpr std::uint32_t PfRead = 4;
inline constexpr std::size_t ElfHeaderSize = 64;
inline constexpr std::size_t ProgramHeaderSize = 56;
inline constexpr std::size_t SectionHeaderSize = 64;
inline constexpr std::size_t DynamicEntrySize = 16;
inline constexpr std::size_t SymbolSize = 24;

template<typename TValue>
void Write(Bytes& bytes, const std::size_t offset, const TValue value) {
    if (offset > bytes.size() || sizeof(value) > bytes.size() - offset) throw std::runtime_error("ELF image write is out of bounds");
    std::memcpy(bytes.data() + offset, &value, sizeof(value));
}

template<typename TValue>
TValue Read(const Bytes& bytes, const std::size_t offset) {
    TValue value{};
    if (offset > bytes.size() || sizeof(value) > bytes.size() - offset) throw std::runtime_error("ELF image read is out of bounds");
    std::memcpy(&value, bytes.data() + offset, sizeof(value));
    return value;
}

inline void Place(Bytes& bytes, const std::size_t offset, const Bytes& data) {
    if (offset > bytes.size() || data.size() > bytes.size() - offset) throw std::runtime_error("ELF image placement is out of bounds");
    std::memcpy(bytes.data() + offset, data.data(), data.size());
}

struct ProgramHeaderSpec {
    std::uint32_t Type;
    std::uint32_t Flags;
    std::uint64_t Offset;
    std::uint64_t Address;
    std::uint64_t FileSize;
    std::uint64_t MemorySize;
    std::uint64_t Alignment;
};

struct SectionHeaderSpec {
    std::uint32_t Name;
    std::uint32_t Type;
    std::uint64_t Offset;
    std::uint64_t Size;
};

struct DynamicEntry {
    std::uint64_t Tag;
    std::uint64_t Value;
};

struct SymbolSpec {
    std::uint32_t Name;
    std::uint8_t Info;
    std::uint8_t Visibility;
    std::uint16_t Section;
    std::uint64_t Value;
    std::uint64_t Size;
};

inline Bytes StringTable(const std::initializer_list<std::string_view> strings) {
    Bytes table{0};
    for (const auto text : strings) {
        table.insert(table.end(), text.begin(), text.end());
        table.push_back(0);
    }
    return table;
}

inline Bytes EncodeDynamic(const std::vector<DynamicEntry>& entries) {
    Bytes data(entries.size() * DynamicEntrySize);
    for (std::size_t index = 0; index < entries.size(); ++index) {
        Write<std::uint64_t>(data, index * DynamicEntrySize, entries[index].Tag);
        Write<std::uint64_t>(data, index * DynamicEntrySize + 8, entries[index].Value);
    }
    return data;
}

inline Bytes EncodeSymbols(const std::vector<SymbolSpec>& symbols) {
    Bytes data(symbols.size() * SymbolSize);
    for (std::size_t index = 0; index < symbols.size(); ++index) {
        const auto base = index * SymbolSize;
        Write<std::uint32_t>(data, base, symbols[index].Name);
        Write<std::uint8_t>(data, base + 4, symbols[index].Info);
        Write<std::uint8_t>(data, base + 5, symbols[index].Visibility);
        Write<std::uint16_t>(data, base + 6, symbols[index].Section);
        Write<std::uint64_t>(data, base + 8, symbols[index].Value);
        Write<std::uint64_t>(data, base + 16, symbols[index].Size);
    }
    return data;
}

class ElfImageBuilder {
public:
    explicit ElfImageBuilder(const std::size_t fileSize)
        : bytes(fileSize, 0)
    {
    }

    ElfImageBuilder& Type(const std::uint16_t value) {
        type = value;
        return *this;
    }

    ElfImageBuilder& Entry(const std::uint64_t value) {
        entry = value;
        return *this;
    }

    ElfImageBuilder& AddProgramHeader(const ProgramHeaderSpec& header) {
        programHeaders.push_back(header);
        return *this;
    }

    ElfImageBuilder& AddSectionHeader(const SectionHeaderSpec& header) {
        sectionHeaders.push_back(header);
        return *this;
    }

    ElfImageBuilder& SectionHeaderTable(const std::uint64_t offset, const std::uint16_t stringIndex) {
        sectionHeaderOffset = offset;
        sectionStringIndex = stringIndex;
        return *this;
    }

    ElfImageBuilder& Data(const std::size_t offset, const Bytes& data) {
        Place(bytes, offset, data);
        return *this;
    }

    Bytes Build() const {
        auto image = bytes;
        const std::uint8_t ident[] = {0x7f, 'E', 'L', 'F', 2, 1, 1};
        std::memcpy(image.data(), ident, sizeof(ident));
        Write<std::uint16_t>(image, 0x10, type);
        Write<std::uint16_t>(image, 0x12, 62);
        Write<std::uint32_t>(image, 0x14, 1);
        Write<std::uint64_t>(image, 0x18, entry);
        Write<std::uint64_t>(image, 0x20, ElfHeaderSize);
        Write<std::uint64_t>(image, 0x28, sectionHeaders.empty() ? 0 : sectionHeaderOffset);
        Write<std::uint16_t>(image, 0x34, ElfHeaderSize);
        Write<std::uint16_t>(image, 0x36, ProgramHeaderSize);
        Write<std::uint16_t>(image, 0x38, static_cast<std::uint16_t>(programHeaders.size()));
        Write<std::uint16_t>(image, 0x3a, SectionHeaderSize);
        Write<std::uint16_t>(image, 0x3c, static_cast<std::uint16_t>(sectionHeaders.size()));
        Write<std::uint16_t>(image, 0x3e, sectionStringIndex);
        for (std::size_t index = 0; index < programHeaders.size(); ++index) {
            const auto& header = programHeaders[index];
            const auto base = ElfHeaderSize + index * ProgramHeaderSize;
            Write<std::uint32_t>(image, base, header.Type);
            Write<std::uint32_t>(image, base + 0x04, header.Flags);
            Write<std::uint64_t>(image, base + 0x08, header.Offset);
            Write<std::uint64_t>(image, base + 0x10, header.Address);
            Write<std::uint64_t>(image, base + 0x18, header.Address);
            Write<std::uint64_t>(image, base + 0x20, header.FileSize);
            Write<std::uint64_t>(image, base + 0x28, header.MemorySize);
            Write<std::uint64_t>(image, base + 0x30, header.Alignment);
        }
        for (std::size_t index = 0; index < sectionHeaders.size(); ++index) {
            const auto& header = sectionHeaders[index];
            const auto base = sectionHeaderOffset + index * SectionHeaderSize;
            Write<std::uint32_t>(image, base, header.Name);
            Write<std::uint32_t>(image, base + 0x04, header.Type);
            Write<std::uint64_t>(image, base + 0x18, header.Offset);
            Write<std::uint64_t>(image, base + 0x20, header.Size);
        }
        return image;
    }

private:
    Bytes bytes;
    std::vector<ProgramHeaderSpec> programHeaders;
    std::vector<SectionHeaderSpec> sectionHeaders;
    std::uint64_t entry = 0;
    std::uint64_t sectionHeaderOffset = 0;
    std::uint16_t sectionStringIndex = 0;
    std::uint16_t type = 3;
};

} // namespace RelinkerUnitTests
