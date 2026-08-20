#include <nid/NidCompute.hpp>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <limits>

namespace {

struct Elf64_Ehdr {
    std::uint8_t e_ident[16];
    std::uint16_t e_type;
    std::uint16_t e_machine;
    std::uint32_t e_version;
    std::uint64_t e_entry;
    std::uint64_t e_phoff;
    std::uint64_t e_shoff;
    std::uint32_t e_flags;
    std::uint16_t e_ehsize;
    std::uint16_t e_phentsize;
    std::uint16_t e_phnum;
    std::uint16_t e_shentsize;
    std::uint16_t e_shnum;
    std::uint16_t e_shstrndx;
};

struct Elf64_Shdr {
    std::uint32_t sh_name;
    std::uint32_t sh_type;
    std::uint64_t sh_flags;
    std::uint64_t sh_addr;
    std::uint64_t sh_offset;
    std::uint64_t sh_size;
    std::uint32_t sh_link;
    std::uint32_t sh_info;
    std::uint64_t sh_addralign;
    std::uint64_t sh_entsize;
};

struct Elf64_Sym {
    std::uint32_t st_name;
    std::uint8_t st_info;
    std::uint8_t st_other;
    std::uint16_t st_shndx;
    std::uint64_t st_value;
    std::uint64_t st_size;
};

constexpr std::uint32_t SHT_DYNSYM = 11u;
constexpr std::uint8_t STB_LOCAL = 0u;
constexpr char kNidPostfix[] = "_nid_postfix";
constexpr std::size_t kNidPostfixLen = sizeof(kNidPostfix) - 1u;

std::string _stripNidPostfix(const std::string& name) {
    if (name.size() < kNidPostfixLen) return name;
    if (name.compare(name.size() - kNidPostfixLen, kNidPostfixLen, kNidPostfix) != 0) return name;
    return name.substr(0u, name.size() - kNidPostfixLen);
}

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

template<typename T>
T _read(const std::vector<std::uint8_t>& buf, std::size_t offset) {
    if (offset + sizeof(T) > buf.size()) throw std::runtime_error("read out of bounds");
    T v;
    std::memcpy(&v, buf.data() + offset, sizeof(T));
    return v;
}

template<typename T>
void _write(std::vector<std::uint8_t>& buf, std::size_t offset, const T& v) {
    if (offset + sizeof(T) > buf.size()) throw std::runtime_error("write out of bounds");
    std::memcpy(buf.data() + offset, &v, sizeof(T));
}

std::string _readCStr(const std::vector<std::uint8_t>& buf, std::size_t offset) {
    if (offset >= buf.size()) throw std::runtime_error("cstr offset out of bounds");
    std::string s;
    while (offset < buf.size() && buf[offset] != 0u)
        s.push_back(static_cast<char>(buf[offset++]));
    return s;
}

std::vector<std::uint8_t> _readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open: " + path);
    return {std::istreambuf_iterator<char>(f), {}};
}

void _writeFile(const std::string& path, const std::vector<std::uint8_t>& data) {
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot write: " + path);
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

enum class BinaryFormat { Elf64, Pe };

BinaryFormat _detectFormat(const std::vector<std::uint8_t>& buf) {
    if (buf.size() >= 4 && buf[0] == 0x7f && buf[1] == 'E' && buf[2] == 'L' && buf[3] == 'F') return BinaryFormat::Elf64;
    if (buf.size() >= 2 && buf[0] == 'M' && buf[1] == 'Z') return BinaryFormat::Pe;
    throw std::runtime_error("unrecognized binary format");
}

void _patchNidsElf(std::vector<std::uint8_t>& elf, const std::string& libraryName) {
    if (elf.size() < sizeof(Elf64_Ehdr)) throw std::runtime_error("file too small");
    if (elf[4] != 2) throw std::runtime_error("not ELF64");

    const auto ehdr = _read<Elf64_Ehdr>(elf, 0u);

    std::size_t dynSymOffset = 0u, dynSymSize = 0u;
    std::size_t dynStrOffset = 0u, dynStrSize = 0u;

    for (std::uint16_t i = 0u; i < ehdr.e_shnum; ++i) {
        const std::size_t shOffset = static_cast<std::size_t>(ehdr.e_shoff) + i * sizeof(Elf64_Shdr);
        const auto shdr = _read<Elf64_Shdr>(elf, shOffset);
        if (shdr.sh_type == SHT_DYNSYM) {
            dynSymOffset = static_cast<std::size_t>(shdr.sh_offset);
            dynSymSize = static_cast<std::size_t>(shdr.sh_size);
        }
    }

    if (dynSymOffset == 0u) throw std::runtime_error("no .dynsym section");

    const std::size_t symCount = dynSymSize / sizeof(Elf64_Sym);
    if (symCount == 0u) throw std::runtime_error(".dynsym is empty");

    const std::size_t dynStrSectionLink = [&]() -> std::size_t {
        for (std::uint16_t i = 0u; i < ehdr.e_shnum; ++i) {
            const std::size_t shOffset = static_cast<std::size_t>(ehdr.e_shoff) + i * sizeof(Elf64_Shdr);
            const auto shdr = _read<Elf64_Shdr>(elf, shOffset);
            if (shdr.sh_type == SHT_DYNSYM) return static_cast<std::size_t>(shdr.sh_link);
        }
        throw std::runtime_error("cannot find dynsym sh_link");
    }();

    {
        const std::size_t shOffset = static_cast<std::size_t>(ehdr.e_shoff) + dynStrSectionLink * sizeof(Elf64_Shdr);
        const auto shdr = _read<Elf64_Shdr>(elf, shOffset);
        dynStrOffset = static_cast<std::size_t>(shdr.sh_offset);
        dynStrSize = static_cast<std::size_t>(shdr.sh_size);
    }

    if (dynStrOffset == 0u) throw std::runtime_error("no .dynstr section");

    constexpr std::uint32_t SHT_NOBITS = 8u;
    std::size_t dynStrAvailable = elf.size() - dynStrOffset;
    for (std::uint16_t i = 0u; i < ehdr.e_shnum; ++i) {
        const std::size_t shOffset = static_cast<std::size_t>(ehdr.e_shoff) + i * sizeof(Elf64_Shdr);
        const auto shdr = _read<Elf64_Shdr>(elf, shOffset);
        if (shdr.sh_type == SHT_NOBITS) continue;
        const auto otherOffset = static_cast<std::size_t>(shdr.sh_offset);
        if (otherOffset > dynStrOffset) dynStrAvailable = std::min(dynStrAvailable, otherOffset - dynStrOffset);
    }

    std::vector<std::uint8_t> newDynStr(elf.begin() + static_cast<std::ptrdiff_t>(dynStrOffset),
                                        elf.begin() + static_cast<std::ptrdiff_t>(dynStrOffset + dynStrSize));

    std::vector<std::pair<std::uint32_t, std::uint32_t>> nameRemap;

    for (std::size_t i = 1u; i < symCount; ++i) {
        const std::size_t symOffset = dynSymOffset + i * sizeof(Elf64_Sym);
        auto sym = _read<Elf64_Sym>(elf, symOffset);

        const std::uint8_t binding = sym.st_info >> 4u;
        if (binding == STB_LOCAL) continue;
        if (sym.st_name == 0u) continue;

        const std::string symName = _readCStr(newDynStr, sym.st_name);
        if (symName.empty()) continue;

        const std::string nid = Nid::ComputeNid(_stripNidPostfix(symName), libraryName);

        const auto existingIt = [&]() {
            for (const auto& [orig, newIdx] : nameRemap)
                if (orig == sym.st_name) return newIdx;
            return std::numeric_limits<std::uint32_t>::max();
        }();

        if (existingIt != std::numeric_limits<std::uint32_t>::max()) {
            sym.st_name = existingIt;
            _write(elf, symOffset, sym);
            continue;
        }

        const auto newNameOffset = static_cast<std::uint32_t>(newDynStr.size());
        for (char c : nid) newDynStr.push_back(static_cast<std::uint8_t>(c));
        newDynStr.push_back(0u);

        nameRemap.emplace_back(sym.st_name, newNameOffset);
        sym.st_name = newNameOffset;
        _write(elf, symOffset, sym);
    }

    if (newDynStr.size() > dynStrAvailable) throw std::runtime_error("new .dynstr exceeds available padding - cannot patch in place");

    std::memcpy(elf.data() + dynStrOffset, newDynStr.data(), newDynStr.size());
    for (std::size_t i = newDynStr.size(); i < dynStrAvailable; ++i)
        elf[dynStrOffset + i] = 0u;

    if (newDynStr.size() > dynStrSize) {
        const std::size_t dynStrShOffset = static_cast<std::size_t>(ehdr.e_shoff) + dynStrSectionLink * sizeof(Elf64_Shdr);
        auto dynStrShdr = _read<Elf64_Shdr>(elf, dynStrShOffset);
        dynStrShdr.sh_size = static_cast<std::uint64_t>(newDynStr.size());
        _write(elf, dynStrShOffset, dynStrShdr);
    }
}

std::size_t _peSectionTableOffset(std::uint32_t peHeaderOffset, std::uint32_t sizeOfOptionalHeader) {
    return static_cast<std::size_t>(peHeaderOffset) + 4u + 20u + sizeOfOptionalHeader;
}

std::size_t _peFindSectionOffsetByRva(const std::vector<std::uint8_t>& pe, std::uint32_t rva, std::uint32_t peHeaderOffset, std::uint16_t numberOfSections, std::uint32_t sizeOfOptionalHeader) {
    const std::size_t sectionTableOffset = _peSectionTableOffset(peHeaderOffset, sizeOfOptionalHeader);
    for (std::uint16_t i = 0u; i < numberOfSections; ++i) {
        const std::size_t sectionOffset = sectionTableOffset + i * sizeof(PeSectionHeader);
        if (sectionOffset + sizeof(PeSectionHeader) > pe.size()) throw std::runtime_error("section header out of bounds");
        const auto section = _read<PeSectionHeader>(pe, sectionOffset);
        const std::uint32_t effectiveSize = section.VirtualSize != 0u ? section.VirtualSize : section.SizeOfRawData;
        if (rva >= section.VirtualAddress && rva < section.VirtualAddress + effectiveSize)
            return sectionOffset;
    }
    throw std::runtime_error("rva not mapped to any section");
}

std::size_t _peRvaToOffset(const std::vector<std::uint8_t>& pe, std::uint32_t rva, std::uint32_t peHeaderOffset, std::uint16_t numberOfSections, std::uint32_t sizeOfOptionalHeader) {
    const std::size_t sectionOffset = _peFindSectionOffsetByRva(pe, rva, peHeaderOffset, numberOfSections, sizeOfOptionalHeader);
    const auto section = _read<PeSectionHeader>(pe, sectionOffset);
    return static_cast<std::size_t>(section.PointerToRawData) + (rva - section.VirtualAddress);
}

void _patchNidsPe(std::vector<std::uint8_t>& pe, const std::string& libraryName) {
    if (pe.size() < 0x40) throw std::runtime_error("file too small");

    const auto peHeaderOffset = _read<std::uint32_t>(pe, 0x3cu);
    if (static_cast<std::size_t>(peHeaderOffset) + 4u > pe.size()) throw std::runtime_error("invalid pe header offset");
    if (pe[peHeaderOffset] != 'P' || pe[peHeaderOffset + 1u] != 'E' || pe[peHeaderOffset + 2u] != 0u || pe[peHeaderOffset + 3u] != 0u)
        throw std::runtime_error("not a PE file");

    const std::size_t coffHeaderOffset = static_cast<std::size_t>(peHeaderOffset) + 4u;
    const auto numberOfSections = _read<std::uint16_t>(pe, coffHeaderOffset + 2u);
    const auto sizeOfOptionalHeader = _read<std::uint16_t>(pe, coffHeaderOffset + 16u);
    if (sizeOfOptionalHeader == 0u) throw std::runtime_error("no optional header");

    const std::size_t optionalHeaderOffset = coffHeaderOffset + 20u;
    const auto magic = _read<std::uint16_t>(pe, optionalHeaderOffset);

    std::size_t dataDirectoryOffset;
    if (magic == 0x20bu) {
        dataDirectoryOffset = optionalHeaderOffset + 112u;
    } else if (magic == 0x10bu) {
        dataDirectoryOffset = optionalHeaderOffset + 96u;
    } else {
        throw std::runtime_error("unsupported optional header magic");
    }

    const auto exportDir = _read<PeDataDirectory>(pe, dataDirectoryOffset);
    if (exportDir.VirtualAddress == 0u) throw std::runtime_error("no export directory");

    const std::size_t exportDirOffset = _peRvaToOffset(pe, exportDir.VirtualAddress, peHeaderOffset, numberOfSections, sizeOfOptionalHeader);
    const auto exportTable = _read<PeExportDirectory>(pe, exportDirOffset);

    if (exportTable.NumberOfNames == 0u) throw std::runtime_error("no exported names");

    const std::size_t namesArrayOffset = _peRvaToOffset(pe, exportTable.AddressOfNames, peHeaderOffset, numberOfSections, sizeOfOptionalHeader);

    std::vector<std::uint32_t> nameRvas(exportTable.NumberOfNames);
    std::vector<std::string> names(exportTable.NumberOfNames);
    for (std::uint32_t i = 0u; i < exportTable.NumberOfNames; ++i) {
        const auto nameRva = _read<std::uint32_t>(pe, namesArrayOffset + i * 4u);
        const std::size_t nameOffset = _peRvaToOffset(pe, nameRva, peHeaderOffset, numberOfSections, sizeOfOptionalHeader);
        const std::string name = _readCStr(pe, nameOffset);
        if (name.empty()) throw std::runtime_error("empty exported name");
        nameRvas[i] = nameRva;
        names[i] = name;
    }

    const std::size_t edataSectionOffset = _peFindSectionOffsetByRva(pe, exportDir.VirtualAddress, peHeaderOffset, numberOfSections, sizeOfOptionalHeader);
    const auto edataSection = _read<PeSectionHeader>(pe, edataSectionOffset);

    const auto sectionAlignment = _read<std::uint32_t>(pe, optionalHeaderOffset + 32u);
    const auto fileAlignment = _read<std::uint32_t>(pe, optionalHeaderOffset + 36u);
    if (sectionAlignment == 0u || fileAlignment == 0u) throw std::runtime_error("invalid section/file alignment");

    const std::uint32_t rawLimit = ((edataSection.SizeOfRawData + fileAlignment - 1u) / fileAlignment) * fileAlignment;
    const std::uint32_t virtualLimit = ((edataSection.VirtualSize + sectionAlignment - 1u) / sectionAlignment) * sectionAlignment;

    std::uint32_t usedEnd = 0u;
    for (std::uint32_t i = 0u; i < exportTable.NumberOfNames; ++i)
        usedEnd = std::max(usedEnd, nameRvas[i] - edataSection.VirtualAddress + static_cast<std::uint32_t>(names[i].size()) + 1u);

    for (std::uint32_t i = 0u; i < exportTable.NumberOfNames; ++i) {
        const std::string nid = Nid::ComputeNid(_stripNidPostfix(names[i]), libraryName);
        const std::size_t oldNameOffset = _peRvaToOffset(pe, nameRvas[i], peHeaderOffset, numberOfSections, sizeOfOptionalHeader);

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
        _write(pe, namesArrayOffset + i * 4u, newNameRva);

        usedEnd += newSize;
    }
}

void _patchNids(std::vector<std::uint8_t>& binary, const std::string& libraryName) {
    switch (_detectFormat(binary)) {
        case BinaryFormat::Elf64: _patchNidsElf(binary, libraryName); return;
        case BinaryFormat::Pe: _patchNidsPe(binary, libraryName); return;
    }
    throw std::runtime_error("unreachable");
}

}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: nid_patcher <library_name> <file.so> [file2.so ...]\n";
        return 1;
    }

    const std::string libraryName = argv[1];

    for (int i = 2; i < argc; ++i) {
        const std::string path = argv[i];
        try {
            auto binary = _readFile(path);
            _patchNids(binary, libraryName);
            _writeFile(path, binary);
            std::cout << "OK: " << path << "\n";
        } catch (const std::exception& e) {
            std::cerr << "FAIL: " << path << ": " << e.what() << "\n";
            return 2;
        }
    }

    return 0;
}
