#include <nid/ElfPatcher.hpp>
#include <nid/NidCompute.hpp>
#include "nid/NidPatcherUtils.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace Nid {

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

constexpr std::uint32_t kShtDynsym = 11u;
constexpr std::uint8_t kStbLocal = 0u;
constexpr std::uint16_t kShnUndef = 0u;

}

void ElfNidPatcher::PatchNids(std::vector<std::uint8_t>& elf, const std::string& libraryName) const {
    using namespace Internal;

    if (elf.size() < sizeof(Elf64_Ehdr)) throw std::runtime_error("file too small");
    if (elf[4] != 2) throw std::runtime_error("not ELF64");

    const auto ehdr = Read<Elf64_Ehdr>(elf, 0u);

    std::size_t dynSymOffset = 0u, dynSymSize = 0u;
    for (std::uint16_t i = 0u; i < ehdr.e_shnum; ++i) {
        const std::size_t shOffset = static_cast<std::size_t>(ehdr.e_shoff) + i * sizeof(Elf64_Shdr);
        const auto shdr = Read<Elf64_Shdr>(elf, shOffset);
        if (shdr.sh_type == kShtDynsym) {
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
            const auto shdr = Read<Elf64_Shdr>(elf, shOffset);
            if (shdr.sh_type == kShtDynsym) return static_cast<std::size_t>(shdr.sh_link);
        }
        throw std::runtime_error("cannot find dynsym sh_link");
    }();

    std::size_t dynStrOffset = 0u, dynStrSize = 0u;
    {
        const std::size_t shOffset = static_cast<std::size_t>(ehdr.e_shoff) + dynStrSectionLink * sizeof(Elf64_Shdr);
        const auto shdr = Read<Elf64_Shdr>(elf, shOffset);
        dynStrOffset = static_cast<std::size_t>(shdr.sh_offset);
        dynStrSize = static_cast<std::size_t>(shdr.sh_size);
    }

    if (dynStrOffset == 0u) throw std::runtime_error("no .dynstr section");

    const std::vector<std::uint8_t> origDynStr(
        elf.begin() + static_cast<std::ptrdiff_t>(dynStrOffset),
        elf.begin() + static_cast<std::ptrdiff_t>(dynStrOffset + dynStrSize)
    );

    struct SymbolNameUse {
        std::size_t symIndex;
        std::uint32_t oldNameOffset;
        std::string newValue;
    };

    std::vector<SymbolNameUse> uses;
    for (std::size_t i = 1u; i < symCount; ++i) {
        const std::size_t symOffset = dynSymOffset + i * sizeof(Elf64_Sym);
        const auto sym = Read<Elf64_Sym>(elf, symOffset);

        if (sym.st_name == 0u) continue;

        const std::uint8_t binding = sym.st_info >> 4u;
        const bool isPatchable = binding != kStbLocal && sym.st_shndx != kShnUndef;

        const std::string symName = ReadCStr(origDynStr, sym.st_name);
        if (symName.empty()) continue;

        std::string newValue = symName;
        if (isPatchable) newValue = ComputeNid(StripNidPostfix(symName), libraryName);

        uses.push_back(SymbolNameUse{i, sym.st_name, newValue});
    }

    std::vector<std::uint8_t> newDynStr;
    newDynStr.push_back(0u);

    std::vector<std::pair<std::uint32_t, std::uint32_t>> oldToNewOffset;
    for (const auto& use : uses) {
        const auto existing = std::find_if(
            oldToNewOffset.begin(), oldToNewOffset.end(),
            [&](const auto& entry) { return entry.first == use.oldNameOffset; }
        );
        if (existing != oldToNewOffset.end()) continue;

        const auto newOffset = static_cast<std::uint32_t>(newDynStr.size());
        newDynStr.insert(newDynStr.end(), use.newValue.begin(), use.newValue.end());
        newDynStr.push_back(0u);
        oldToNewOffset.emplace_back(use.oldNameOffset, newOffset);
    }

    if (newDynStr.size() > dynStrSize) {
        throw std::runtime_error(
            "rebuilt .dynstr (" + std::to_string(newDynStr.size()) + " bytes) exceeds original section size (" +
            std::to_string(dynStrSize) + " bytes) - relinking with a larger section is required"
        );
    }
    newDynStr.resize(dynStrSize, 0u);

    for (std::size_t i = 1u; i < symCount; ++i) {
        const std::size_t symOffset = dynSymOffset + i * sizeof(Elf64_Sym);
        const auto sym = Read<Elf64_Sym>(elf, symOffset);
        if (sym.st_name == 0u) continue;

        const auto mapped = std::find_if(
            oldToNewOffset.begin(), oldToNewOffset.end(),
            [&](const auto& entry) { return entry.first == sym.st_name; }
        );
        if (mapped == oldToNewOffset.end()) continue;

        Write(elf, symOffset + offsetof(Elf64_Sym, st_name), mapped->second);
    }

    std::memcpy(elf.data() + dynStrOffset, newDynStr.data(), newDynStr.size());
}

}
