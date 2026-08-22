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

struct Elf64_Rela {
    std::uint64_t r_offset;
    std::uint64_t r_info;
    std::int64_t r_addend;
};

constexpr std::uint32_t kShtDynsym = 11u;
constexpr std::uint32_t kShtGnuHash = 0x6ffffff6u;
constexpr std::uint32_t kShtRela = 4u;
constexpr std::uint8_t kStbLocal = 0u;
constexpr std::uint16_t kShnUndef = 0u;

std::uint32_t GnuHash(const std::string& name) {
    std::uint32_t h = 5381u;
    for (const unsigned char c : name)
        h = h * 33u + c;
    return h;
}

struct GnuHashLayout {
    std::uint32_t NBuckets;
    std::uint32_t SymOffset;
    std::uint32_t BloomSize;
    std::uint32_t BloomShift;
    std::size_t BloomOffset;
    std::size_t BucketsOffset;
    std::size_t ChainOffset;
};

GnuHashLayout ReadGnuHashLayout(const std::vector<std::uint8_t>& elf, std::size_t sectionOffset) {
    using namespace Internal;

    GnuHashLayout layout{};
    layout.NBuckets = Read<std::uint32_t>(elf, sectionOffset + 0u);
    layout.SymOffset = Read<std::uint32_t>(elf, sectionOffset + 4u);
    layout.BloomSize = Read<std::uint32_t>(elf, sectionOffset + 8u);
    layout.BloomShift = Read<std::uint32_t>(elf, sectionOffset + 12u);
    layout.BloomOffset = sectionOffset + 16u;
    layout.BucketsOffset = layout.BloomOffset + static_cast<std::size_t>(layout.BloomSize) * 8u;
    layout.ChainOffset = layout.BucketsOffset + static_cast<std::size_t>(layout.NBuckets) * 4u;
    return layout;
}

void ReorderDynSymForGnuHash(
    std::vector<std::uint8_t>& elf,
    std::size_t dynSymOffset,
    std::size_t symCount,
    std::uint32_t symOffset,
    std::uint32_t bucketCount,
    std::vector<std::string>& symbolNames,
    std::vector<std::uint32_t>& oldToNewIndex
) {
    using namespace Internal;

    std::vector<std::uint32_t> movableIndices;
    for (std::uint32_t i = symOffset; i < symCount; ++i)
        movableIndices.push_back(i);

    std::stable_sort(
        movableIndices.begin(), movableIndices.end(),
        [&](const std::uint32_t lhs, const std::uint32_t rhs) {
            return GnuHash(symbolNames[lhs]) % bucketCount < GnuHash(symbolNames[rhs]) % bucketCount;
        }
    );

    std::vector<Elf64_Sym> reorderedSyms(symCount);
    std::vector<std::string> reorderedNames(symCount);

    for (std::uint32_t i = 0u; i < symOffset; ++i) {
        reorderedSyms[i] = Read<Elf64_Sym>(elf, dynSymOffset + i * sizeof(Elf64_Sym));
        reorderedNames[i] = symbolNames[i];
        oldToNewIndex[i] = i;
    }

    for (std::size_t position = 0u; position < movableIndices.size(); ++position) {
        const std::uint32_t oldIndex = movableIndices[position];
        const std::uint32_t newIndex = symOffset + static_cast<std::uint32_t>(position);
        reorderedSyms[newIndex] = Read<Elf64_Sym>(elf, dynSymOffset + oldIndex * sizeof(Elf64_Sym));
        reorderedNames[newIndex] = symbolNames[oldIndex];
        oldToNewIndex[oldIndex] = newIndex;
    }

    for (std::size_t i = 0u; i < symCount; ++i)
        Write(elf, dynSymOffset + i * sizeof(Elf64_Sym), reorderedSyms[i]);

    symbolNames = std::move(reorderedNames);
}

void FixupRelocationSymbolIndices(
    std::vector<std::uint8_t>& elf,
    const Elf64_Ehdr& ehdr,
    std::uint32_t dynSymSectionIndex,
    const std::vector<std::uint32_t>& oldToNewIndex
) {
    using namespace Internal;

    for (std::uint16_t i = 0u; i < ehdr.e_shnum; ++i) {
        const std::size_t shOffset = static_cast<std::size_t>(ehdr.e_shoff) + i * sizeof(Elf64_Shdr);
        const auto shdr = Read<Elf64_Shdr>(elf, shOffset);
        if (shdr.sh_type != kShtRela) continue;
        if (shdr.sh_link != dynSymSectionIndex) continue;

        const std::size_t relaOffset = static_cast<std::size_t>(shdr.sh_offset);
        const std::size_t relaCount = static_cast<std::size_t>(shdr.sh_size) / sizeof(Elf64_Rela);

        for (std::size_t r = 0u; r < relaCount; ++r) {
            const std::size_t entryOffset = relaOffset + r * sizeof(Elf64_Rela);
            const auto rela = Read<Elf64_Rela>(elf, entryOffset);

            const auto oldSymIndex = static_cast<std::uint32_t>(rela.r_info >> 32u);
            if (oldSymIndex >= oldToNewIndex.size()) continue;

            const std::uint32_t newSymIndex = oldToNewIndex[oldSymIndex];
            const auto relType = static_cast<std::uint32_t>(rela.r_info & 0xffffffffu);
            const std::uint64_t newInfo = (static_cast<std::uint64_t>(newSymIndex) << 32u) | relType;

            Write(elf, entryOffset + offsetof(Elf64_Rela, r_info), newInfo);
        }
    }
}

void RebuildGnuHash(
    std::vector<std::uint8_t>& elf,
    std::size_t sectionOffset,
    std::size_t sectionSize,
    std::size_t symCount,
    const std::vector<std::string>& symbolNames
) {
    using namespace Internal;

    const GnuHashLayout layout = ReadGnuHashLayout(elf, sectionOffset);

    const std::size_t chainCount = symCount - layout.SymOffset;
    const std::size_t requiredSize = layout.ChainOffset - sectionOffset + chainCount * 4u;
    if (requiredSize > sectionSize)
        throw std::runtime_error("gnu.hash section too small to rebuild in place");

    std::vector<std::uint32_t> hashes(symCount, 0u);
    for (std::size_t i = layout.SymOffset; i < symCount; ++i)
        hashes[i] = GnuHash(symbolNames[i]);

    std::vector<std::uint64_t> bloom(layout.BloomSize, 0u);
    for (std::size_t i = layout.SymOffset; i < symCount; ++i) {
        const std::uint32_t h = hashes[i];
        const std::uint32_t wordIndex = (h / 64u) % layout.BloomSize;
        const std::uint32_t bit1 = h % 64u;
        const std::uint32_t bit2 = (h >> layout.BloomShift) % 64u;
        bloom[wordIndex] |= (std::uint64_t{1} << bit1);
        bloom[wordIndex] |= (std::uint64_t{1} << bit2);
    }

    std::vector<std::uint32_t> buckets(layout.NBuckets, 0u);
    std::vector<std::uint32_t> chain(chainCount, 0u);

    for (std::size_t i = layout.SymOffset; i < symCount; ++i) {
        const std::uint32_t bucketIndex = hashes[i] % layout.NBuckets;
        if (buckets[bucketIndex] == 0u)
            buckets[bucketIndex] = static_cast<std::uint32_t>(i);
    }

    for (std::size_t i = layout.SymOffset; i < symCount; ++i) {
        const std::uint32_t bucketIndex = hashes[i] % layout.NBuckets;
        const bool isLastInBucket = (i + 1u == symCount) || (hashes[i + 1u] % layout.NBuckets != bucketIndex);
        chain[i - layout.SymOffset] = isLastInBucket ? (hashes[i] | 1u) : (hashes[i] & ~std::uint32_t{1});
    }

    for (std::uint32_t bloomIndex = 0u; bloomIndex < layout.BloomSize; ++bloomIndex)
        Write(elf, layout.BloomOffset + bloomIndex * 8u, bloom[bloomIndex]);

    for (std::uint32_t bucketIndex = 0u; bucketIndex < layout.NBuckets; ++bucketIndex)
        Write(elf, layout.BucketsOffset + bucketIndex * 4u, buckets[bucketIndex]);

    for (std::size_t i = 0u; i < chainCount; ++i)
        Write(elf, layout.ChainOffset + i * 4u, chain[i]);

    if (requiredSize < sectionSize)
        std::memset(elf.data() + sectionOffset + requiredSize, 0, sectionSize - requiredSize);
}

}

void ElfNidPatcher::PatchNids(std::vector<std::uint8_t>& elf, const std::string& libraryName) const {
    using namespace Internal;

    if (elf.size() < sizeof(Elf64_Ehdr)) throw std::runtime_error("file too small");
    if (elf[4] != 2) throw std::runtime_error("not ELF64");

    const auto ehdr = Read<Elf64_Ehdr>(elf, 0u);

    std::size_t dynSymOffset = 0u, dynSymSize = 0u;
    std::size_t gnuHashOffset = 0u, gnuHashSize = 0u;
    std::uint32_t dynSymSectionIndex = 0u;
    std::uint32_t gnuHashSymOffset = 0u;
    for (std::uint16_t i = 0u; i < ehdr.e_shnum; ++i) {
        const std::size_t shOffset = static_cast<std::size_t>(ehdr.e_shoff) + i * sizeof(Elf64_Shdr);
        const auto shdr = Read<Elf64_Shdr>(elf, shOffset);
        if (shdr.sh_type == kShtDynsym) {
            dynSymOffset = static_cast<std::size_t>(shdr.sh_offset);
            dynSymSize = static_cast<std::size_t>(shdr.sh_size);
            dynSymSectionIndex = i;
        }
        if (shdr.sh_type == kShtGnuHash) {
            gnuHashOffset = static_cast<std::size_t>(shdr.sh_offset);
            gnuHashSize = static_cast<std::size_t>(shdr.sh_size);
            gnuHashSymOffset = Read<std::uint32_t>(elf, gnuHashOffset + 4u);
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

    std::vector<std::string> newSymbolNames(symCount);
    for (std::size_t i = 1u; i < symCount; ++i) {
        const std::size_t symOffset = dynSymOffset + i * sizeof(Elf64_Sym);
        const auto sym = Read<Elf64_Sym>(elf, symOffset);
        if (sym.st_name == 0u) continue;

        const auto mapped = std::find_if(
            oldToNewOffset.begin(), oldToNewOffset.end(),
            [&](const auto& entry) { return entry.first == sym.st_name; }
        );
        if (mapped == oldToNewOffset.end()) continue;

        newSymbolNames[i] = ReadCStr(newDynStr, mapped->second);
        Write(elf, symOffset + offsetof(Elf64_Sym, st_name), mapped->second);
    }

    std::memcpy(elf.data() + dynStrOffset, newDynStr.data(), newDynStr.size());

    if (gnuHashOffset != 0u) {
        const auto bucketCount = Read<std::uint32_t>(elf, gnuHashOffset);
        std::vector<std::uint32_t> oldToNewIndex(symCount);
        ReorderDynSymForGnuHash(elf, dynSymOffset, symCount, gnuHashSymOffset, bucketCount, newSymbolNames, oldToNewIndex);
        FixupRelocationSymbolIndices(elf, ehdr, dynSymSectionIndex, oldToNewIndex);
        RebuildGnuHash(elf, gnuHashOffset, gnuHashSize, symCount, newSymbolNames);
    }
}

}
