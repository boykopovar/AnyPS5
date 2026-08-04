#ifndef ELFPATCHER_DOMAIN_ELFCONSTANTS_HPP
#define ELFPATCHER_DOMAIN_ELFCONSTANTS_HPP

#include <cstdint>

namespace Elfpatcher {

inline constexpr std::uint32_t PT_LOAD = 1;
inline constexpr std::uint32_t PT_DYNAMIC = 2;
inline constexpr std::uint32_t PT_INTERP = 3;
inline constexpr std::uint32_t PT_NOTE = 4;
inline constexpr std::uint32_t PT_PHDR = 6;
inline constexpr std::uint32_t PT_GNU_RELRO = 0x6474e552;
inline constexpr std::uint32_t PF_X = 0x1;
inline constexpr std::uint32_t PF_W = 0x2;
inline constexpr std::uint32_t PF_R = 0x4;
inline constexpr std::uint32_t ET_DYN = 3;
inline constexpr std::uint32_t PT_SCE_DYNLIBDATA = 0x61000000;
inline constexpr std::uint32_t PT_OS_PROCPARAM = 0x61000001;
inline constexpr std::uint32_t PT_OS_RELRO = 0x61000010;

inline constexpr std::int64_t DT_NULL = 0;
inline constexpr std::int64_t DT_NEEDED = 1;
inline constexpr std::int64_t DT_PLTRELSZ = 2;
inline constexpr std::int64_t DT_STRTAB = 5;
inline constexpr std::int64_t DT_SYMTAB = 6;
inline constexpr std::int64_t DT_RELA = 7;
inline constexpr std::int64_t DT_RELASZ = 8;
inline constexpr std::int64_t DT_RELAENT = 9;
inline constexpr std::int64_t DT_STRSZ = 10;
inline constexpr std::int64_t DT_SYMENT = 11;
inline constexpr std::int64_t DT_PLTREL = 20;
inline constexpr std::int64_t DT_JMPREL = 23;

inline constexpr std::uint32_t SHT_NULL = 0;
inline constexpr std::uint32_t SHT_PROGBITS = 1;
inline constexpr std::uint32_t SHT_STRTAB = 3;
inline constexpr std::uint32_t SHT_DYNAMIC = 6;
inline constexpr std::uint32_t SHT_DYNSYM = 11;
inline constexpr std::uint32_t SHF_WRITE = 0x1;
inline constexpr std::uint32_t SHF_ALLOC = 0x2;
inline constexpr std::uint32_t SHF_EXECINSTR = 0x4;

}

#endif
