#include <nid/ExportExclusions.hpp>
#include <nid/ElfPatcher.hpp>
#include <nid/PeNidPatcher.hpp>
#include <nid/MachONidPatcher.hpp>
#include <nid/NidPatcherUtils.hpp>
#include <bit>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace Nid {

namespace {

void RequireRange(const std::vector<std::uint8_t>& binary, std::uint64_t offset, std::uint64_t size) {
    if (offset > binary.size() || size > binary.size() - offset) throw std::runtime_error("export reference range out of bounds");
}

template<typename TValue>
TValue ReadValue(const std::vector<std::uint8_t>& binary, std::uint64_t offset) {
    RequireRange(binary, offset, sizeof(TValue));
    TValue value;
    std::memcpy(&value, binary.data() + offset, sizeof(value));
    return value;
}

std::string ReadName(const std::vector<std::uint8_t>& binary, std::uint64_t offset, std::uint64_t size) {
    RequireRange(binary, offset, size);
    if (size == 0) throw std::runtime_error("empty export reference name range");
    const auto start = reinterpret_cast<const char*>(binary.data() + offset);
    const auto end = static_cast<const char*>(std::memchr(start, 0, size));
    if (!end || end == start) throw std::runtime_error("invalid export reference name");
    return {start, end};
}

std::unordered_set<std::string> ReadPeExports(const std::vector<std::uint8_t>& binary) {
    const std::uint64_t header = ReadValue<std::uint32_t>(binary, 0x3c);
    if (ReadValue<std::uint32_t>(binary, header) != 0x4550) throw std::runtime_error("invalid reference PE signature");
    const auto sectionCount = ReadValue<std::uint16_t>(binary, header + 6);
    const auto optionalSize = ReadValue<std::uint16_t>(binary, header + 20);
    const auto optional = header + 24;
    RequireRange(binary, optional, optionalSize);
    const auto magic = ReadValue<std::uint16_t>(binary, optional);
    if (magic != 0x20b && magic != 0x10b) throw std::runtime_error("unsupported reference PE optional header");
    const std::uint64_t directoryOffset = magic == 0x20b ? 112 : 96;
    if (optionalSize < directoryOffset + sizeof(PeDataDirectory)) throw std::runtime_error("missing reference PE export directory");
    if (ReadValue<std::uint32_t>(binary, optional + directoryOffset - 4) == 0) throw std::runtime_error("missing reference PE data directories");
    const auto directory = ReadValue<PeDataDirectory>(binary, optional + directoryOffset);
    if (!directory.VirtualAddress || directory.Size < sizeof(PeExportDirectory)) throw std::runtime_error("missing reference PE exports");
    const auto sections = optional + optionalSize;
    RequireRange(binary, sections, std::uint64_t{sectionCount} * sizeof(PeSectionHeader));
    const auto mapRva = [&](std::uint32_t rva, std::uint64_t size) -> std::uint64_t {
        for (std::uint16_t index = 0; index < sectionCount; ++index) {
            const auto section = ReadValue<PeSectionHeader>(binary, sections + std::uint64_t{index} * sizeof(PeSectionHeader));
            if (rva < section.VirtualAddress) continue;
            const std::uint64_t relative = rva - section.VirtualAddress;
            if (relative >= section.SizeOfRawData || size > section.SizeOfRawData - relative) continue;
            const auto offset = std::uint64_t{section.PointerToRawData} + relative;
            RequireRange(binary, offset, size);
            return offset;
        }
        throw std::runtime_error("unmapped reference PE export RVA");
    };
    const auto table = ReadValue<PeExportDirectory>(binary, mapRva(directory.VirtualAddress, sizeof(PeExportDirectory)));
    if (!table.NumberOfNames) throw std::runtime_error("reference PE has no named exports");
    const auto names = mapRva(table.AddressOfNames, std::uint64_t{table.NumberOfNames} * 4);
    std::unordered_set<std::string> result;
    for (std::uint32_t index = 0; index < table.NumberOfNames; ++index) {
        const auto rva = ReadValue<std::uint32_t>(binary, names + std::uint64_t{index} * 4);
        const auto offset = mapRva(rva, 1);
        auto name = ReadName(binary, offset, binary.size() - offset);
        mapRva(rva, name.size() + 1);
        if (!result.insert(name).second) throw std::runtime_error("duplicate reference export: " + name);
    }
    return result;
}

std::unordered_set<std::string> ReadElfExports(const std::vector<std::uint8_t>& binary) {
    const auto header = ReadValue<Elf64_Ehdr>(binary, 0);
    if (header.e_ident[4] != 2 || header.e_ident[5] != 1 || header.e_shentsize != sizeof(Elf64_Shdr)) throw std::runtime_error("unsupported reference ELF layout");
    RequireRange(binary, header.e_shoff, std::uint64_t{header.e_shnum} * sizeof(Elf64_Shdr));
    std::unordered_set<std::string> result;
    for (std::uint16_t index = 0; index < header.e_shnum; ++index) {
        const auto section = ReadValue<Elf64_Shdr>(binary, header.e_shoff + std::uint64_t{index} * sizeof(Elf64_Shdr));
        if (section.sh_type != kShtDynsym) continue;
        if (section.sh_entsize != sizeof(Elf64_Sym) || section.sh_size % sizeof(Elf64_Sym) || section.sh_link >= header.e_shnum) throw std::runtime_error("invalid reference ELF symbol table");
        RequireRange(binary, section.sh_offset, section.sh_size);
        const auto strings = ReadValue<Elf64_Shdr>(binary, header.e_shoff + std::uint64_t{section.sh_link} * sizeof(Elf64_Shdr));
        if (strings.sh_type != 3) throw std::runtime_error("invalid reference ELF string table");
        RequireRange(binary, strings.sh_offset, strings.sh_size);
        for (std::uint64_t offset = 0; offset < section.sh_size; offset += sizeof(Elf64_Sym)) {
            const auto symbol = ReadValue<Elf64_Sym>(binary, section.sh_offset + offset);
            const auto binding = symbol.st_info >> 4;
            const auto visibility = symbol.st_other & 3;
            if (symbol.st_shndx == kShnUndef || (binding != 1 && binding != 2 && binding != 10) || (visibility != 0 && visibility != 3)) continue;
            if (symbol.st_name >= strings.sh_size) throw std::runtime_error("invalid reference ELF export name");
            result.insert(ReadName(binary, strings.sh_offset + symbol.st_name, strings.sh_size - symbol.st_name));
        }
    }
    if (result.empty()) throw std::runtime_error("reference ELF has no exports");
    return result;
}

}

std::string NormalizeExportName(const std::string& name) {
    if (name.ends_with(Internal::kNidPostfix)) return name.substr(0, name.size() - Internal::kNidPostfixLen);
    return name;
}

std::unordered_set<std::string> ReadExportExclusions(const std::string& path) {
    if constexpr (std::endian::native != std::endian::little) throw std::runtime_error("export references require a little-endian host");
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) throw std::runtime_error("cannot open export reference: " + path);
    const auto size = input.tellg();
    if (size <= 0) throw std::runtime_error("empty or unreadable export reference: " + path);
    std::vector<std::uint8_t> binary(static_cast<std::size_t>(size));
    input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(binary.data()), static_cast<std::streamsize>(binary.size()))) throw std::runtime_error("cannot read export reference: " + path);
    std::unordered_set<std::string> exports;
    if (binary.size() >= 2 && binary[0] == 'M' && binary[1] == 'Z') {
        exports = ReadPeExports(binary);
    } else if (binary.size() >= 4 && binary[0] == 0x7f && binary[1] == 'E' && binary[2] == 'L' && binary[3] == 'F') {
        exports = ReadElfExports(binary);
    } else if (binary.size() >= 4 && binary[0] == 0xcf && binary[1] == 0xfa && binary[2] == 0xed && binary[3] == 0xfe) {
        for (const auto& entry : ReadMachOExports(binary)) {
            if (entry.Name.size() < 2 || entry.Name[0] != '_') throw std::runtime_error("invalid reference Mach-O export: " + entry.Name);
            if (!exports.insert(entry.Name.substr(1)).second) throw std::runtime_error("duplicate reference export: " + entry.Name);
        }
        if (exports.empty()) throw std::runtime_error("reference Mach-O has no exports");
    } else {
        throw std::runtime_error("unrecognized export reference format: " + path);
    }
    std::unordered_set<std::string> result;
    result.reserve(exports.size());
    for (const auto& name : exports) {
        auto normalized = NormalizeExportName(name);
        if (normalized.empty()) throw std::runtime_error("empty normalized export reference name");
        result.insert(std::move(normalized));
    }
    return result;
}

}
