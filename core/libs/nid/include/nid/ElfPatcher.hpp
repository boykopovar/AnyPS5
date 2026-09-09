#ifndef NID_ELFPATCHER_HPP
#define NID_ELFPATCHER_HPP

#include <nid/IBinaryPatcher.hpp>

namespace Nid {

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

struct Elf64_Dyn {
    std::int64_t d_tag;
    std::uint64_t d_val;
};

struct Elf64_Verneed {
    std::uint16_t vn_version;
    std::uint16_t vn_cnt;
    std::uint32_t vn_file;
    std::uint32_t vn_aux;
    std::uint32_t vn_next;
};

struct Elf64_Vernaux {
    std::uint32_t vna_hash;
    std::uint16_t vna_flags;
    std::uint16_t vna_other;
    std::uint32_t vna_name;
    std::uint32_t vna_next;
};

struct GnuHashLayout {
    std::uint32_t NBuckets;
    std::uint32_t SymOffset;
    std::uint32_t BloomSize;
    std::uint32_t BloomShift;
    std::size_t BloomOffset;
    std::size_t BucketsOffset;
    std::size_t ChainOffset;
};

constexpr std::int64_t kDtNeeded = 1;
constexpr std::int64_t kDtSoname = 14;
constexpr std::int64_t kDtNull = 0;
constexpr std::uint32_t kShtDynamic = 6u;
constexpr std::uint32_t kShtGnuVerneed = 0x6ffffffeu;
constexpr std::uint32_t kShtDynsym = 11u;
constexpr std::uint32_t kShtGnuHash = 0x6ffffff6u;
constexpr std::uint32_t kShtRela = 4u;
constexpr std::uint32_t kShtGnuVersym = 0x6fffffffu;
constexpr std::uint8_t kStbLocal = 0u;
constexpr std::uint16_t kShnUndef = 0u;

class ElfNidPatcher final : public IBinaryPatcher {
public:
    void PatchNids(std::vector<std::uint8_t>& binary, const std::string& libraryName) const override;
};

}

#endif
