#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/specifics/linux/ElfTypes.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>

extern "C" {
int APS5_VABI __elf_phdr_match_addr_nid_postfix(dl_phdr_info*, void*);
}

static void Require(bool condition) {
    if (!condition) std::abort();
}

static void* Address(std::uintptr_t value) {
    return reinterpret_cast<void*>(value);
}

int main() {
    const std::array<Elf64_Phdr, 4> headers{{
        {PT_LOAD, PF_W, 0, 0x1000, 0, 0x1000, 0x1000, 0x1000},
        {PT_GNU_EH_FRAME, PF_X, 0, 0x2000, 0, 0x1000, 0x1000, 4},
        {PT_LOAD, PF_X, 0, 0x3000, 0, 0x1000, 0x1000, 0x1000},
        {PT_LOAD, PF_X | PF_W, 0, 0x8000, 0, 0x100, 0x100, 0x1000},
    }};
    dl_phdr_info info{0x400000, "guest", headers.data(), static_cast<std::uint16_t>(headers.size())};

    Require(__elf_phdr_match_addr_nid_postfix(&info, Address(0x403800)) == 1);
    Require(__elf_phdr_match_addr_nid_postfix(&info, Address(0x401800)) == 0);
    Require(__elf_phdr_match_addr_nid_postfix(&info, Address(0x402800)) == 0);

    Require(__elf_phdr_match_addr_nid_postfix(&info, Address(0x403000)) == 1);
    Require(__elf_phdr_match_addr_nid_postfix(&info, Address(0x402fff)) == 0);
    Require(__elf_phdr_match_addr_nid_postfix(&info, Address(0x404000 - 9)) == 1);
    Require(__elf_phdr_match_addr_nid_postfix(&info, Address(0x404000 - 8)) == 0);
    Require(__elf_phdr_match_addr_nid_postfix(&info, Address(0x404000)) == 0);

    Require(__elf_phdr_match_addr_nid_postfix(&info, Address(0x400800)) == 0);
    Require(__elf_phdr_match_addr_nid_postfix(&info, Address(0x3800)) == 0);
    Require(__elf_phdr_match_addr_nid_postfix(&info, Address(0x408010)) == 1);

    info.dlpi_phnum = 0;
    Require(__elf_phdr_match_addr_nid_postfix(&info, Address(0x403800)) == 0);

    ModuleInfoEx module{};
    module.st_size = sizeof(module);
    module.segment_count = 3;
    module.segments[0] = {0x401000, 0x1000, 1 | 2};
    module.segments[1] = {0x403000, 0x1000, 1 | 4};
    module.segments[2] = {0x408000, 0x100, 1 | 2 | 4};

    Require(__elf_phdr_match_addr_nid_postfix(reinterpret_cast<dl_phdr_info*>(&module), Address(0x403000)) == 1);
    Require(__elf_phdr_match_addr_nid_postfix(reinterpret_cast<dl_phdr_info*>(&module), Address(0x403ff8)) == 1);
    Require(__elf_phdr_match_addr_nid_postfix(reinterpret_cast<dl_phdr_info*>(&module), Address(0x404000)) == 0);
    Require(__elf_phdr_match_addr_nid_postfix(reinterpret_cast<dl_phdr_info*>(&module), Address(0x401800)) == 0);
    Require(__elf_phdr_match_addr_nid_postfix(reinterpret_cast<dl_phdr_info*>(&module), Address(0x408010)) == 1);
}
