#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>

static void Require(bool condition, const std::source_location& location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "module address match check failed at line %u\n", location.line());
        std::abort();
    }
}

static void* Address(std::uintptr_t value) {
    return reinterpret_cast<void*>(value);
}

static void CheckQueriedModule() {
    alignas(16) static std::array<std::uint64_t, 8> data{};
    const auto code = reinterpret_cast<std::uintptr_t>(&CheckQueriedModule);
    for (const int flags : {1, 2}) {
        ModuleInfoEx module{};
        module.st_size = sizeof(module);
        Require(sceKernelGetModuleInfoFromAddr(code, flags, &module) == 0);
        Require(module.st_size == 0);
        Require(module.segment_count > 0 && module.segment_count <= std::size(module.segments));
        Require(module.name[16] != 0);
        std::array<unsigned char, sizeof(module)> before{};
        std::memcpy(before.data(), &module, sizeof(module));
        Require(__elf_phdr_match_addr_nid_postfix(&module, Address(code)) == 1);
        Require(__elf_phdr_match_addr_nid_postfix(&module, data.data()) == 0);
        Require(std::memcmp(before.data(), &module, sizeof(module)) == 0);
    }
}

static void CheckSegments() {
    ModuleInfoEx module{};
    module.segment_count = 4;
    module.segments[0] = {0x401000, 0x1000, 1 | 2};
    module.segments[1] = {0x402000, 0x1000, 1};
    module.segments[2] = {0x403000, 0x1000, 1 | 4};
    module.segments[3] = {0x408000, 0x100, 1 | 2 | 4};
    for (const std::uint64_t size : {std::uint64_t{0}, std::uint64_t{sizeof(module)}, std::numeric_limits<std::uint64_t>::max()}) {
        module.st_size = size;
        Require(__elf_phdr_match_addr_nid_postfix(&module, Address(0x403800)) == 1);
        Require(__elf_phdr_match_addr_nid_postfix(&module, Address(0x401800)) == 0);
        Require(__elf_phdr_match_addr_nid_postfix(&module, Address(0x402800)) == 0);
        Require(__elf_phdr_match_addr_nid_postfix(&module, Address(0x403000)) == 1);
        Require(__elf_phdr_match_addr_nid_postfix(&module, Address(0x402fff)) == 0);
        Require(__elf_phdr_match_addr_nid_postfix(&module, Address(0x403ff8)) == 1);
        Require(__elf_phdr_match_addr_nid_postfix(&module, Address(0x403ff9)) == 0);
        Require(__elf_phdr_match_addr_nid_postfix(&module, Address(0x404000)) == 0);
        Require(__elf_phdr_match_addr_nid_postfix(&module, Address(0x400800)) == 0);
        Require(__elf_phdr_match_addr_nid_postfix(&module, Address(0x3800)) == 0);
        Require(__elf_phdr_match_addr_nid_postfix(&module, Address(0x408010)) == 1);
    }
    module.segment_count = 0;
    Require(__elf_phdr_match_addr_nid_postfix(&module, Address(0x403800)) == 0);
    module.segment_count = std::numeric_limits<std::uint32_t>::max();
    Require(__elf_phdr_match_addr_nid_postfix(&module, Address(0x408010)) == 1);
    Require(__elf_phdr_match_addr_nid_postfix(&module, Address(0x409000)) == 0);
    module.segment_count = 1;
    for (const std::uint32_t size : {0u, 7u, 8u}) {
        module.segments[0] = {0x401000, size, 4};
        Require(__elf_phdr_match_addr_nid_postfix(&module, Address(0x401000)) == (size == 8 ? 1 : 0));
    }
    module.segments[0] = {std::numeric_limits<std::uintptr_t>::max() - 15, 16, 4};
    Require(__elf_phdr_match_addr_nid_postfix(&module, Address(std::numeric_limits<std::uintptr_t>::max() - 7)) == 1);
    Require(__elf_phdr_match_addr_nid_postfix(&module, Address(std::numeric_limits<std::uintptr_t>::max() - 6)) == 0);
    Require(__elf_phdr_match_addr_nid_postfix(&module, nullptr) == 0);
}

int main() {
    CheckQueriedModule();
    CheckSegments();
    bool threw = false;
    try {
        __elf_phdr_match_addr_nid_postfix(nullptr, nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
    std::puts("module query-to-match and segment checks passed");
}
