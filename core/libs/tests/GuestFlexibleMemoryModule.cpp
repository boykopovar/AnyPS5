#ifdef __APPLE__
#include "prx/libc/include/specifics/linux/ElfTypes.hpp"
#include <dlfcn.h>
#include <cstring>
#endif

extern "C" {
unsigned char guestFlexibleMemoryModuleData[512u * 1024u * 1024u] = {};
}

#ifdef __APPLE__
namespace {

[[gnu::section("__APS5DATA,__meta"), gnu::used]] alignas(8) std::byte metadata[0x60 + sizeof(Elf64_Phdr)]{};

[[maybe_unused]] const bool described = [] {
    Dl_info image{};
    if (dladdr(guestFlexibleMemoryModuleData, &image) == 0) return false;
    const std::uint64_t loadBias = reinterpret_cast<std::uintptr_t>(image.dli_fbase);
    Elf64_Phdr header{};
    header.p_type = PT_LOAD;
    header.p_flags = PF_R | PF_W;
    header.p_offset = reinterpret_cast<std::uintptr_t>(guestFlexibleMemoryModuleData) - loadBias;
    header.p_vaddr = header.p_offset;
    header.p_paddr = header.p_offset;
    header.p_memsz = sizeof(guestFlexibleMemoryModuleData);
    const std::uint64_t version = 2;
    const std::uint64_t count = 1;
    std::memcpy(metadata + 0x08, &loadBias, sizeof(loadBias));
    std::memcpy(metadata + 0x58, &count, sizeof(count));
    std::memcpy(metadata + 0x60, &header, sizeof(header));
    std::memcpy(metadata, &version, sizeof(version));
    return true;
}();

}
#endif
