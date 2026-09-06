#ifndef PRX_LIBC_INCLUDE_SPECIFICS_LINUX_ELFTYPES_HPP
#define PRX_LIBC_INCLUDE_SPECIFICS_LINUX_ELFTYPES_HPP

#include <cstdint>
#include <cstddef>

struct Elf64_Phdr {
    std::uint32_t p_type;
    std::uint32_t p_flags;
    std::uint64_t p_offset;
    std::uint64_t p_vaddr;
    std::uint64_t p_paddr;
    std::uint64_t p_filesz;
    std::uint64_t p_memsz;
    std::uint64_t p_align;
};

struct dl_phdr_info {
    std::uintptr_t dlpi_addr;
    const char* dlpi_name;
    const Elf64_Phdr* dlpi_phdr;
    std::uint16_t dlpi_phnum;
};

static constexpr std::uint32_t PT_LOAD = 1;
static constexpr std::uint32_t PT_GNU_EH_FRAME = 0x6474e550;
static constexpr std::uint32_t PF_X = 1;
static constexpr std::uint32_t PF_W = 2;

extern "C" int dl_iterate_phdr(int (*callback)(dl_phdr_info*, std::size_t, void*), void* data);

#endif
