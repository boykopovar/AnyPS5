#ifndef ELFPATCHER_DOMAIN_ELFCONSTANTS_HPP
#define ELFPATCHER_DOMAIN_ELFCONSTANTS_HPP

#include <cstdint>

namespace Elfpatcher {

inline constexpr std::size_t kEhdrOsAbiOffset = 7;
inline constexpr std::size_t kEhdrAbiVersionOffset = 8;
inline constexpr std::size_t kEhdrTypeOffset = 0x10;
inline constexpr std::size_t kEhdrEntryOffset = 24;
inline constexpr std::size_t kEhdrPhOffOffset = 32;
inline constexpr std::size_t kEhdrShOffOffset = 40;
inline constexpr std::size_t kEhdrPhEntSizeOffset = 54;
inline constexpr std::size_t kEhdrPhNumOffset = 56;
inline constexpr std::size_t kEhdrShEntSizeOffset = 58;
inline constexpr std::size_t kEhdrShNumOffset = 60;
inline constexpr std::size_t kEhdrShStrNdxOffset = 62;

inline constexpr std::size_t kShdrNameOffset = 0;
inline constexpr std::size_t kShdrTypeOffset = 4;
inline constexpr std::size_t kShdrFlagsOffset = 8;
inline constexpr std::size_t kShdrAddrOffset = 16;
inline constexpr std::size_t kShdrFileOffsetOffset = 24;
inline constexpr std::size_t kShdrSizeOffset = 32;
inline constexpr std::size_t kShdrLinkOffset = 40;
inline constexpr std::size_t kShdrInfoOffset = 44;
inline constexpr std::size_t kShdrAlignOffset = 48;
inline constexpr std::size_t kShdrEntSizeOffset = 56;
inline constexpr std::size_t kShdrEntrySize = 64;

inline constexpr std::size_t kPhdrTypeOffset = 0;
inline constexpr std::size_t kPhdrFlagsOffset = 4;
inline constexpr std::size_t kPhdrOffsetOffset = 8;
inline constexpr std::size_t kPhdrVaddrOffset = 16;
inline constexpr std::size_t kPhdrPaddrOffset = 24;
inline constexpr std::size_t kPhdrFileSizeOffset = 32;
inline constexpr std::size_t kPhdrMemSizeOffset = 40;
inline constexpr std::size_t kPhdrAlignOffset = 48;

inline constexpr std::size_t kDynEntrySize = 16;

inline constexpr std::uint64_t kSymEntrySize = 24;

inline constexpr std::uint64_t kRelaEntrySize = 24;

inline constexpr std::size_t kDynStrAlignment = 24;
inline constexpr std::size_t kDynSymAlignment = 24;
inline constexpr std::size_t kRelaAlignment = 24;
inline constexpr std::size_t kRelaPltAlignment = 8;
inline constexpr std::size_t kGotAlignment = 8;
inline constexpr std::size_t kGotReservedSlots = 3;

inline constexpr std::uint64_t kDefaultLoadAlignment = 0x1000;
inline constexpr std::uint64_t kPhdrHeaderAlignment = 8;
inline constexpr std::uint64_t kDynamicHeaderAlignment = 8;
inline constexpr std::uint64_t kInterpHeaderAlignment = 1;
inline constexpr std::uint16_t kSyntheticProgramHeaderCount = 4;

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

inline constexpr std::uint32_t PT_LOOS = 0x61000000;
inline constexpr std::uint32_t PT_HIOS = 0x6fffffff;

inline constexpr std::int64_t DT_NULL = 0;
inline constexpr std::int64_t DT_NEEDED = 1;
inline constexpr std::int64_t DT_PLTRELSZ = 2;
inline constexpr std::int64_t DT_PLTGOT = 3;
inline constexpr std::int64_t DT_STRTAB = 5;
inline constexpr std::int64_t DT_SYMTAB = 6;
inline constexpr std::int64_t DT_RELA = 7;
inline constexpr std::int64_t DT_RELASZ = 8;
inline constexpr std::int64_t DT_RELAENT = 9;
inline constexpr std::int64_t DT_STRSZ = 10;
inline constexpr std::int64_t DT_SYMENT = 11;
inline constexpr std::int64_t DT_PLTREL = 20;
inline constexpr std::int64_t DT_JMPREL = 23;
inline constexpr std::int64_t DT_RUNPATH = 29;

inline constexpr std::uint32_t SHT_NULL = 0;
inline constexpr std::uint32_t SHT_PROGBITS = 1;
inline constexpr std::uint32_t SHT_STRTAB = 3;
inline constexpr std::uint32_t SHT_DYNAMIC = 6;
inline constexpr std::uint32_t SHT_DYNSYM = 11;
inline constexpr std::uint32_t SHF_WRITE = 0x1;
inline constexpr std::uint32_t SHF_ALLOC = 0x2;
inline constexpr std::uint32_t SHF_EXECINSTR = 0x4;

inline constexpr char kShStrTabBlob[] = "\0.shstrtab\0.dynstr\0.dynsym\0.dynamic\0.text";
inline constexpr std::uint32_t kShStrTabNameOffset = 1;
inline constexpr std::uint32_t kDynStrNameOffset = 11;
inline constexpr std::uint32_t kDynSymNameOffset = 19;
inline constexpr std::uint32_t kDynamicNameOffset = 27;
inline constexpr std::uint32_t kTextNameOffset = 36;
inline constexpr std::size_t kSectionHeaderCount = 6;
inline constexpr std::uint32_t kShStrTabIndex = 1;
inline constexpr std::uint64_t kNoSpecialAlignment = 1;

inline constexpr std::uint32_t kDynSymLinkToStrTab = 2;
inline constexpr std::uint32_t kDynSymInfoFirstGlobal = 1;
inline constexpr std::uint64_t kDynSymAlign = 8;
inline constexpr std::uint32_t kDynamicLinkToStrTab = 2;

inline constexpr std::uint8_t kStubOpPopRax = 0x58;
inline constexpr std::uint8_t kStubOpMovRbxRsp[] = {0x48, 0x89, 0xe3};
inline constexpr std::uint8_t kStubOpSubRsp0x30[] = {0x48, 0x83, 0xec, 0x30};
inline constexpr std::uint8_t kStubOpAndRsp0xf0[] = {0x48, 0x83, 0xe4, 0xf0};
inline constexpr std::uint8_t kStubOpMovDwordPtrRsp[] = {0x89, 0x04, 0x24};
inline constexpr std::uint8_t kStubOpMovQwordPtrRsp8Rbx[] = {0x48, 0x89, 0x5c, 0x24, 0x08};
inline constexpr std::uint8_t kStubOpMovRdiRsp[] = {0x48, 0x89, 0xe7};
inline constexpr std::uint8_t kStubOpXorRsiRsi[] = {0x48, 0x31, 0xf6};
inline constexpr std::uint8_t kStubOpJmpRel32 = 0xe9;
inline constexpr std::size_t kStubJmpInstructionSize = 5;

}

#endif
