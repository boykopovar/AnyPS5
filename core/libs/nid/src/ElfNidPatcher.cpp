#include <nid/ElfPatcher.hpp>
#include <nid/NidResolver.hpp>
#include <nid/NidPatcherUtils.hpp>
#include <nid/NidCompute.hpp>
#include <algorithm>
#include <unordered_map>
#include <utility>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace Nid {

namespace {


void RequireElfRange(const std::vector<std::uint8_t>& elf, std::uint64_t offset, std::uint64_t size, const char* table) {
    if (offset > elf.size() || size > elf.size() - offset)
        throw std::runtime_error(std::string("invalid ELF ") + table + " range");
}

std::uint32_t GnuHash(const std::string& name) {
    std::uint32_t h = 5381u;
    for (const unsigned char c : name)
        h = h * 33u + c;
    return h;
}

std::uint32_t SysvHash(const std::string& name) {
    std::uint32_t hash = 0u;
    for (const unsigned char byte : name) {
        hash = (hash << 4u) + byte;
        const auto high = hash & 0xf0000000u;
        hash ^= high >> 24u;
        hash &= ~high;
    }
    return hash;
}

void ValidateSysvHash(const std::vector<std::uint8_t>& elf, std::size_t offset, std::size_t size, std::size_t symCount) {
    using namespace Internal;
    RequireElfRange(elf, offset, size, "hash");
    if (size < 8u) throw std::runtime_error("invalid ELF hash header");
    const auto buckets = Read<std::uint32_t>(elf, offset);
    const auto chains = Read<std::uint32_t>(elf, offset + 4u);
    if (!buckets || chains != symCount || 8u + (std::uint64_t{buckets} + chains) * 4u > size)
        throw std::runtime_error("invalid ELF hash geometry");
}

void RebuildSysvHash(std::vector<std::uint8_t>& elf, std::size_t offset, const std::vector<std::string>& symbolNames) {
    using namespace Internal;
    const auto bucketCount = Read<std::uint32_t>(elf, offset);
    std::vector<std::uint32_t> buckets(bucketCount, 0u);
    std::vector<std::uint32_t> chains(symbolNames.size(), 0u);
    for (std::size_t index = 1u; index < symbolNames.size(); ++index) {
        if (symbolNames[index].empty()) continue;
        const auto bucket = SysvHash(symbolNames[index]) % bucketCount;
        chains[index] = buckets[bucket];
        buckets[bucket] = static_cast<std::uint32_t>(index);
    }
    for (std::size_t index = 0u; index < buckets.size(); ++index)
        Write(elf, offset + 8u + index * 4u, buckets[index]);
    for (std::size_t index = 0u; index < chains.size(); ++index)
        Write(elf, offset + 8u + buckets.size() * 4u + index * 4u, chains[index]);
}


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

void ValidateGnuHash(const std::vector<std::uint8_t>& elf, std::size_t offset, std::size_t size, std::size_t symCount) {
    if (offset > elf.size() || size > elf.size() - offset || size < 16u)
        throw std::runtime_error("invalid gnu.hash section range");
    const auto layout = ReadGnuHashLayout(elf, offset);
    if (layout.SymOffset > symCount || !layout.BloomSize || (layout.BloomSize & (layout.BloomSize - 1u)) ||
        layout.BloomShift >= 32u || (!layout.NBuckets && layout.SymOffset != symCount))
        throw std::runtime_error("invalid gnu.hash geometry");
    const std::uint64_t required = 16u + std::uint64_t{layout.BloomSize} * 8u +
        std::uint64_t{layout.NBuckets} * 4u + (symCount - layout.SymOffset) * 4u;
    if (required > size) throw std::runtime_error("gnu.hash section too small to rebuild in place");
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

void RemapDynamicNeededOffsets(
    std::vector<std::uint8_t>& elf,
    const Elf64_Ehdr& ehdr,
    const std::vector<std::pair<std::uint32_t, std::uint32_t>>& oldToNewOffset
) {
    using namespace Internal;

    for (std::uint16_t i = 0u; i < ehdr.e_shnum; ++i) {
        const std::size_t shOffset = static_cast<std::size_t>(ehdr.e_shoff) + i * sizeof(Elf64_Shdr);
        const auto shdr = Read<Elf64_Shdr>(elf, shOffset);
        if (shdr.sh_type != kShtDynamic) continue;

        const std::size_t dynOffset = static_cast<std::size_t>(shdr.sh_offset);
        const std::size_t dynCount = static_cast<std::size_t>(shdr.sh_size) / sizeof(Elf64_Dyn);

        for (std::size_t d = 0u; d < dynCount; ++d) {
            const std::size_t entryOffset = dynOffset + d * sizeof(Elf64_Dyn);
            const auto dyn = Read<Elf64_Dyn>(elf, entryOffset);
            if (!IsDynamicNameTag(dyn.d_tag)) continue;

            const auto oldNameOffset = static_cast<std::uint32_t>(dyn.d_val);
            const auto mapped = std::find_if(
                oldToNewOffset.begin(), oldToNewOffset.end(),
                [&](const auto& entry) { return entry.first == oldNameOffset; }
            );
            if (mapped == oldToNewOffset.end()) continue;

            Write(elf, entryOffset + offsetof(Elf64_Dyn, d_val), static_cast<std::uint64_t>(mapped->second));
        }
    }
}

struct VersionNameField {
    std::size_t Offset;
    std::uint32_t Name;
};

std::vector<VersionNameField> ReadVersionNameFields(const std::vector<std::uint8_t>& elf, const Elf64_Ehdr& ehdr) {
    using namespace Internal;
    std::vector<VersionNameField> fields;
    for (std::uint16_t i = 0u; i < ehdr.e_shnum; ++i) {
        const auto section = Read<Elf64_Shdr>(elf, static_cast<std::size_t>(ehdr.e_shoff) + i * sizeof(Elf64_Shdr));
        if (section.sh_type != kShtGnuVerneed || section.sh_size == 0u) continue;
        if (section.sh_offset > elf.size() || section.sh_size > elf.size() - section.sh_offset)
            throw std::runtime_error("invalid .gnu.version_r section range");
        const auto location = [&](std::uint64_t relative, std::size_t size) {
            if (relative > section.sh_size || size > section.sh_size - relative)
                throw std::runtime_error(".gnu.version_r record outside section");
            return static_cast<std::size_t>(section.sh_offset + relative);
        };
        const auto advance = [&](std::uint64_t current, std::uint32_t step, std::size_t size) {
            if (step < size || step > section.sh_size - current)
                throw std::runtime_error("invalid .gnu.version_r record link");
            return current + step;
        };
        std::uint64_t current = 0u;
        while (true) {
            const auto offset = location(current, sizeof(Elf64_Verneed));
            const auto need = Read<Elf64_Verneed>(elf, offset);
            if (need.vn_version != 1u) throw std::runtime_error("unsupported .gnu.version_r structure version");
            fields.push_back({offset + offsetof(Elf64_Verneed, vn_file), need.vn_file});
            if (need.vn_cnt != 0u) {
                auto auxiliary = advance(current, need.vn_aux, sizeof(Elf64_Verneed));
                for (std::uint16_t index = 0u; index < need.vn_cnt; ++index) {
                    const auto auxOffset = location(auxiliary, sizeof(Elf64_Vernaux));
                    const auto aux = Read<Elf64_Vernaux>(elf, auxOffset);
                    fields.push_back({auxOffset + offsetof(Elf64_Vernaux, vna_name), aux.vna_name});
                    if (index + 1u == need.vn_cnt) {
                        if (aux.vna_next != 0u) throw std::runtime_error(".gnu.version_r auxiliary count mismatch");
                    } else {
                        auxiliary = advance(auxiliary, aux.vna_next, sizeof(Elf64_Vernaux));
                    }
                }
            }
            if (need.vn_next == 0u) break;
            current = advance(current, need.vn_next, sizeof(Elf64_Verneed));
        }
    }
    return fields;
}

void RemapVerneedOffsets(
    std::vector<std::uint8_t>& elf,
    const Elf64_Ehdr& ehdr,
    const std::vector<std::pair<std::uint32_t, std::uint32_t>>& oldToNewOffset
) {
    using namespace Internal;
    for (const auto& field : ReadVersionNameFields(elf, ehdr)) {
        const auto mapped = std::find_if(oldToNewOffset.begin(), oldToNewOffset.end(),
            [&](const auto& entry) { return entry.first == field.Name; });
        if (mapped != oldToNewOffset.end()) Write(elf, field.Offset, mapped->second);
    }
}

std::vector<std::uint32_t> CollectDynamicNameOffsets(const std::vector<std::uint8_t>& elf, const Elf64_Ehdr& ehdr) {
    using namespace Internal;

    std::vector<std::uint32_t> offsets;

    for (std::uint16_t i = 0u; i < ehdr.e_shnum; ++i) {
        const std::size_t shOffset = static_cast<std::size_t>(ehdr.e_shoff) + i * sizeof(Elf64_Shdr);
        const auto shdr = Read<Elf64_Shdr>(elf, shOffset);
        if (shdr.sh_type != kShtDynamic) continue;

        const std::size_t dynOffset = static_cast<std::size_t>(shdr.sh_offset);
        const std::size_t dynCount = static_cast<std::size_t>(shdr.sh_size) / sizeof(Elf64_Dyn);

        for (std::size_t d = 0u; d < dynCount; ++d) {
            const auto dyn = Read<Elf64_Dyn>(elf, dynOffset + d * sizeof(Elf64_Dyn));
            if (dyn.d_tag == kDtNull) break;
            if (!IsDynamicNameTag(dyn.d_tag)) continue;
            offsets.push_back(static_cast<std::uint32_t>(dyn.d_val));
        }
    }

    return offsets;
}

std::vector<std::uint32_t> CollectVerneedNameOffsets(const std::vector<std::uint8_t>& elf, const Elf64_Ehdr& ehdr) {
    std::vector<std::uint32_t> offsets;
    for (const auto& field : ReadVersionNameFields(elf, ehdr)) offsets.push_back(field.Name);
    return offsets;
}

}

void ElfNidPatcher::PatchNids(std::vector<std::uint8_t>& elf, const std::string& libraryName, const std::unordered_set<std::string>& excludedExports) const {
    using namespace Internal;

    if (elf.size() < sizeof(Elf64_Ehdr)) throw std::runtime_error("file too small");
    if (elf[4] != 2) throw std::runtime_error("not ELF64");

    const auto ehdr = Read<Elf64_Ehdr>(elf, 0u);
    if (ehdr.e_ident[5] != 1 || ehdr.e_shentsize != sizeof(Elf64_Shdr))
        throw std::runtime_error("unsupported ELF byte order or section header size");
    RequireElfRange(elf, ehdr.e_shoff, std::uint64_t{ehdr.e_shnum} * sizeof(Elf64_Shdr), "section table");

    std::size_t dynSymOffset = 0u, dynSymSize = 0u;
    std::size_t gnuHashOffset = 0u, gnuHashSize = 0u;
    std::size_t sysvHashOffset = 0u, sysvHashSize = 0u;
    std::uint32_t sysvHashLink = 0u;
    std::size_t versymOffset = 0u;
    std::uint32_t dynSymSectionIndex = 0u;
    std::uint32_t gnuHashSymOffset = 0u;
    for (std::uint16_t i = 0u; i < ehdr.e_shnum; ++i) {
        const std::size_t shOffset = static_cast<std::size_t>(ehdr.e_shoff) + i * sizeof(Elf64_Shdr);
        const auto shdr = Read<Elf64_Shdr>(elf, shOffset);
        if (shdr.sh_type == kShtDynsym) {
            if (shdr.sh_entsize != sizeof(Elf64_Sym) || shdr.sh_size % sizeof(Elf64_Sym) || shdr.sh_link >= ehdr.e_shnum)
                throw std::runtime_error("invalid ELF dynsym layout");
            RequireElfRange(elf, shdr.sh_offset, shdr.sh_size, "dynsym");
            dynSymOffset = static_cast<std::size_t>(shdr.sh_offset);
            dynSymSize = static_cast<std::size_t>(shdr.sh_size);
            dynSymSectionIndex = i;
        }
        if (shdr.sh_type == kShtGnuHash) {
            gnuHashOffset = static_cast<std::size_t>(shdr.sh_offset);
            gnuHashSize = static_cast<std::size_t>(shdr.sh_size);
        }
        if (shdr.sh_type == kShtHash) {
            sysvHashOffset = static_cast<std::size_t>(shdr.sh_offset);
            sysvHashSize = static_cast<std::size_t>(shdr.sh_size);
            sysvHashLink = shdr.sh_link;
        }
        if (shdr.sh_type == kShtGnuVersym)
            versymOffset = static_cast<std::size_t>(shdr.sh_offset);
        if (shdr.sh_type == kShtGnuVerdef)
            throw std::runtime_error("unsupported .gnu.version_d section: symbol version definitions cannot be preserved");
    }

    if (dynSymOffset == 0u) throw std::runtime_error("no .dynsym section");

    const std::size_t symCount = dynSymSize / sizeof(Elf64_Sym);
    if (symCount == 0u) throw std::runtime_error(".dynsym is empty");
    if (sysvHashOffset != 0u) {
        if (sysvHashLink != dynSymSectionIndex) throw std::runtime_error("invalid ELF hash symbol table link");
        ValidateSysvHash(elf, sysvHashOffset, sysvHashSize, symCount);
    }
    if (gnuHashOffset != 0u) {
        ValidateGnuHash(elf, gnuHashOffset, gnuHashSize, symCount);
        gnuHashSymOffset = Read<std::uint32_t>(elf, gnuHashOffset + 4u);
    }

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
        if (shdr.sh_type != 3) throw std::runtime_error("invalid ELF dynstr section type");
        RequireElfRange(elf, shdr.sh_offset, shdr.sh_size, "dynstr");
        dynStrOffset = static_cast<std::size_t>(shdr.sh_offset);
        dynStrSize = static_cast<std::size_t>(shdr.sh_size);
    }

    if (dynStrOffset == 0u) throw std::runtime_error("no .dynstr section");

    const std::vector<std::uint8_t> origDynStr(
        elf.begin() + static_cast<std::ptrdiff_t>(dynStrOffset),
        elf.begin() + static_cast<std::ptrdiff_t>(dynStrOffset + dynStrSize)
    );

    std::vector<std::string> exportedNames;
    for (std::size_t i = 1u; i < symCount; ++i) {
        const std::size_t symOffset = dynSymOffset + i * sizeof(Elf64_Sym);
        const auto sym = Read<Elf64_Sym>(elf, symOffset);
        if (sym.st_name == 0u) continue;
        const std::uint8_t binding = sym.st_info >> 4u;
        if (binding == kStbLocal || sym.st_shndx == kShnUndef) continue;
        const std::string symName = ReadCStr(origDynStr, sym.st_name);
        if (symName.empty()) continue;
        exportedNames.push_back(symName);
    }

    const auto nidMap = ResolveNids(exportedNames, libraryName, excludedExports);

    struct SymbolNameUse {
        std::size_t symIndex;
        std::uint32_t oldNameOffset;
        std::string newValue;
    };

    std::vector<SymbolNameUse> uses;
    std::unordered_map<std::string, std::pair<std::uint64_t, std::uint16_t>> patchedExports;
    for (std::size_t i = 1u; i < symCount; ++i) {
        const std::size_t symOffset = dynSymOffset + i * sizeof(Elf64_Sym);
        const auto sym = Read<Elf64_Sym>(elf, symOffset);
        if (sym.st_name == 0u) continue;

        const std::uint8_t binding = sym.st_info >> 4u;
        const bool isPatchable = binding != kStbLocal && sym.st_shndx != kShnUndef;

        const std::string symName = ReadCStr(origDynStr, sym.st_name);
        if (symName.empty()) continue;

        std::string newValue = symName;
        if (isPatchable) {
            const auto it = nidMap.find(symName);
            if (it != nidMap.end())
                newValue = it->second;
            const auto [existing, inserted] = patchedExports.try_emplace(newValue, sym.st_value, sym.st_shndx);
            if (!inserted && existing->second != std::pair{sym.st_value, sym.st_shndx})
                throw std::runtime_error("duplicate patched export name: " + newValue);
        } else if (binding != kStbLocal && sym.st_shndx == kShnUndef) {
            const bool isUnversioned = versymOffset == 0u ||
                (Read<std::uint16_t>(elf, versymOffset + i * sizeof(std::uint16_t)) <= 1u);
            const bool hasNidPostfix = symName.size() >= kNidPostfixLen &&
                symName.compare(symName.size() - kNidPostfixLen, kNidPostfixLen, kNidPostfix) == 0;
            const bool hasScePrefix = symName.size() >= 3u &&
                std::tolower(static_cast<unsigned char>(symName[0])) == 's' &&
                std::tolower(static_cast<unsigned char>(symName[1])) == 'c' &&
                std::tolower(static_cast<unsigned char>(symName[2])) == 'e';
            if (isUnversioned && (hasNidPostfix || hasScePrefix))
                newValue = ResolveOneName(symName);
        }

        uses.push_back(SymbolNameUse{i, sym.st_name, newValue});
    }

    std::vector<std::pair<std::uint32_t, std::string>> names;
    const auto addName = [&](std::uint32_t oldOffset, std::string value) {
        const auto existing = std::find_if(
            names.begin(), names.end(),
            [&](const auto& entry) { return entry.first == oldOffset && entry.second == value; }
        );
        if (existing == names.end()) names.emplace_back(oldOffset, std::move(value));
    };
    for (const auto& use : uses) addName(use.oldNameOffset, use.newValue);

    std::vector<std::uint32_t> preservedOffsets = CollectDynamicNameOffsets(elf, ehdr);
    const std::vector<std::uint32_t> verneedOffsets = CollectVerneedNameOffsets(elf, ehdr);
    preservedOffsets.insert(preservedOffsets.end(), verneedOffsets.begin(), verneedOffsets.end());
    for (const auto oldOffset : preservedOffsets) addName(oldOffset, ReadCStr(origDynStr, oldOffset));

    std::vector<std::string> values;
    for (const auto& name : names) values.push_back(name.second);
    std::sort(values.begin(), values.end(), [](const std::string& left, const std::string& right) {
        return std::lexicographical_compare(right.rbegin(), right.rend(), left.rbegin(), left.rend());
    });
    values.erase(std::unique(values.begin(), values.end()), values.end());

    std::vector<std::uint8_t> newDynStr;
    newDynStr.push_back(0u);
    std::unordered_map<std::string, std::uint32_t> valueOffsets;
    const std::string* previous = nullptr;
    std::uint32_t previousOffset = 0u;
    for (const auto& value : values) {
        if (previous && previous->size() >= value.size() &&
            previous->compare(previous->size() - value.size(), value.size(), value) == 0) {
            valueOffsets.emplace(value, previousOffset + static_cast<std::uint32_t>(previous->size() - value.size()));
            continue;
        }
        previousOffset = static_cast<std::uint32_t>(newDynStr.size());
        previous = &value;
        valueOffsets.emplace(value, previousOffset);
        newDynStr.insert(newDynStr.end(), value.begin(), value.end());
        newDynStr.push_back(0u);
    }

    std::vector<std::pair<std::uint32_t, std::uint32_t>> preservedToNewOffset;
    for (const auto offset : preservedOffsets)
        preservedToNewOffset.emplace_back(offset, valueOffsets.at(ReadCStr(origDynStr, offset)));

    if (newDynStr.size() > dynStrSize) {
        throw std::runtime_error(
            "rebuilt .dynstr (" + std::to_string(newDynStr.size()) + " bytes) exceeds original section size (" +
            std::to_string(dynStrSize) + " bytes) - relinking with a larger section is required"
        );
    }
    newDynStr.resize(dynStrSize, 0u);

    std::vector<std::string> newSymbolNames(symCount);
    for (const auto& use : uses) {
        newSymbolNames[use.symIndex] = use.newValue;
        Write(elf, dynSymOffset + use.symIndex * sizeof(Elf64_Sym) + offsetof(Elf64_Sym, st_name), valueOffsets.at(use.newValue));
    }

    std::memcpy(elf.data() + dynStrOffset, newDynStr.data(), newDynStr.size());

    RemapDynamicNeededOffsets(elf, ehdr, preservedToNewOffset);
    RemapVerneedOffsets(elf, ehdr, preservedToNewOffset);

    if (gnuHashOffset != 0u) {
        const auto bucketCount = Read<std::uint32_t>(elf, gnuHashOffset);
        std::vector<std::uint32_t> oldToNewIndex(symCount);
        ReorderDynSymForGnuHash(elf, dynSymOffset, symCount, gnuHashSymOffset, bucketCount, newSymbolNames, oldToNewIndex);
        FixupRelocationSymbolIndices(elf, ehdr, dynSymSectionIndex, oldToNewIndex);
        RebuildGnuHash(elf, gnuHashOffset, gnuHashSize, symCount, newSymbolNames);
    }
    if (sysvHashOffset != 0u) RebuildSysvHash(elf, sysvHashOffset, newSymbolNames);
}

}
