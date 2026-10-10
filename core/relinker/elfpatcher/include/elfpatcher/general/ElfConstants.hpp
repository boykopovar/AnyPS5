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
inline constexpr std::uint16_t kSyntheticProgramHeaderCount = 5;

inline constexpr std::uint32_t PT_LOAD = 1;
inline constexpr std::uint32_t PT_DYNAMIC = 2;
inline constexpr std::uint32_t PT_INTERP = 3;
inline constexpr std::uint32_t PT_NOTE = 4;
inline constexpr std::uint32_t PT_PHDR = 6;
inline constexpr std::uint32_t PT_GNU_EH_FRAME = 0x6474e550;
inline constexpr std::uint32_t PT_GNU_STACK = 0x6474e551;
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

inline constexpr std::int64_t DT_DEBUG = 21;
inline constexpr std::int64_t DT_NULL = 0;
inline constexpr std::int64_t DT_NEEDED = 1;
inline constexpr std::int64_t DT_PLTRELSZ = 2;
inline constexpr std::int64_t DT_PLTGOT = 3;
inline constexpr std::int64_t DT_HASH = 4;
inline constexpr std::int64_t DT_STRTAB = 5;
inline constexpr std::int64_t DT_SYMTAB = 6;
inline constexpr std::int64_t DT_RELA = 7;
inline constexpr std::int64_t DT_RELASZ = 8;
inline constexpr std::int64_t DT_RELAENT = 9;
inline constexpr std::int64_t DT_STRSZ = 10;
inline constexpr std::int64_t DT_SYMENT = 11;
inline constexpr std::int64_t DT_PLTREL = 20;
inline constexpr std::int64_t DT_JMPREL = 23;
inline constexpr std::int64_t DT_INIT = 12;
inline constexpr std::int64_t DT_FINI = 13;
inline constexpr std::int64_t DT_INIT_ARRAY = 25;
inline constexpr std::int64_t DT_FINI_ARRAY = 26;
inline constexpr std::int64_t DT_INIT_ARRAYSZ = 27;
inline constexpr std::int64_t DT_FINI_ARRAYSZ = 28;
inline constexpr std::int64_t DT_RUNPATH = 29;
inline constexpr std::int64_t DT_FLAGS = 30;
inline constexpr std::int64_t DT_PREINIT_ARRAY = 32;
inline constexpr std::int64_t DT_PREINIT_ARRAYSZ = 33;
inline constexpr std::uint64_t DF_BIND_NOW = 0x8;

inline constexpr std::int64_t DT_OS_INIT = 0x6000000c;
inline constexpr std::int64_t DT_OS_FINI = 0x6000000d;
inline constexpr std::int64_t DT_OS_INIT_ARRAY = 0x60000019;
inline constexpr std::int64_t DT_OS_FINI_ARRAY = 0x6000001a;
inline constexpr std::int64_t DT_OS_INIT_ARRAYSZ = 0x6000001b;
inline constexpr std::int64_t DT_OS_FINI_ARRAYSZ = 0x6000001c;
inline constexpr std::int64_t DT_OS_PREINIT_ARRAY = 0x60000020;
inline constexpr std::int64_t DT_OS_PREINIT_ARRAYSZ = 0x60000021;
inline constexpr std::int64_t DT_OS_PLTGOT = 0x61000027;
inline constexpr std::int64_t DT_OS_JMPREL = 0x61000029;
inline constexpr std::int64_t DT_OS_PLTREL = 0x6100002b;
inline constexpr std::int64_t DT_OS_PLTRELSZ = 0x6100002d;
inline constexpr std::int64_t DT_OS_RELA = 0x6100002f;
inline constexpr std::int64_t DT_OS_RELASZ = 0x61000031;
inline constexpr std::int64_t DT_OS_RELAENT = 0x61000033;
inline constexpr std::int64_t DT_OS_STRTAB = 0x61000035;
inline constexpr std::int64_t DT_OS_STRSZ = 0x61000037;
inline constexpr std::int64_t DT_OS_SYMTAB = 0x61000039;
inline constexpr std::int64_t DT_OS_SYMENT = 0x6100003b;
inline constexpr std::int64_t DT_OS_SYMTABSZ = 0x6100003f;

inline constexpr std::uint32_t R_X86_64_NONE = 0;
inline constexpr std::uint32_t R_X86_64_64 = 1;
inline constexpr std::uint32_t R_X86_64_PC32 = 2;
inline constexpr std::uint32_t R_X86_64_GOT32 = 3;
inline constexpr std::uint32_t R_X86_64_PLT32 = 4;
inline constexpr std::uint32_t R_X86_64_COPY = 5;
inline constexpr std::uint32_t R_X86_64_GLOB_DAT = 6;
inline constexpr std::uint32_t R_X86_64_JUMP_SLOT = 7;
inline constexpr std::uint32_t R_X86_64_RELATIVE = 8;
inline constexpr std::uint32_t R_X86_64_GOTPCREL = 9;
inline constexpr std::uint32_t R_X86_64_32 = 10;
inline constexpr std::uint32_t R_X86_64_32S = 11;
inline constexpr std::uint32_t R_X86_64_GOTPCRELX = 41;
inline constexpr std::uint32_t R_X86_64_REX_GOTPCRELX = 42;

inline constexpr std::uint8_t STB_GLOBAL = 1;
inline constexpr std::uint8_t STB_WEAK = 2;
inline constexpr std::uint8_t STT_FUNC = 2;
inline constexpr std::uint8_t STV_DEFAULT = 0;
inline constexpr std::uint16_t SHN_UNDEF = 0;

inline constexpr std::uint32_t SHT_NULL = 0;
inline constexpr std::uint32_t SHT_PROGBITS = 1;
inline constexpr std::uint32_t SHT_SYMTAB = 2;
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

inline constexpr std::uint8_t kStubOpAndRsp0xf0[] = {0x48, 0x83, 0xe4, 0xf0};
inline constexpr std::uint8_t kStubOpMovRdiRsp[] = {0x48, 0x89, 0xe7};
inline constexpr std::uint8_t kStubOpXorRsiRsi[] = {0x48, 0x31, 0xf6};
inline constexpr std::uint8_t kStubOpPushZero[] = {0x6a, 0x00};
inline constexpr std::uint8_t kStubOpMovRbpRsp[] = {0x48, 0x89, 0xe5};
inline constexpr std::uint8_t kStubOpCallRel32 = 0xe8;
inline constexpr std::size_t kStubCallInstructionSize = 5;
inline constexpr std::uint8_t kStubOpUd2[] = {0x0f, 0x0b};

}

#endif
