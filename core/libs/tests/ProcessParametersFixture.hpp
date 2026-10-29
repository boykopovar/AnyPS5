#ifndef CORE_LIBS_TESTS_PROCESSPARAMETERSFIXTURE_HPP
#define CORE_LIBS_TESTS_PROCESSPARAMETERSFIXTURE_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include "prx/libc/include/specifics/linux/ElfTypes.hpp"
#include <sys/mman.h>
#include <unistd.h>
#elif defined(__APPLE__)
#include "prx/libc/include/specifics/linux/ElfTypes.hpp"
#include <array>
#endif

namespace ProcessParametersFixture {
namespace {

#ifdef _WIN32
[[gnu::section(".proctst"), gnu::used]] alignas(4) std::uint32_t metadata[2]{};
#elif defined(__APPLE__)
[[gnu::section("__APS5DATA,__meta"), gnu::used]] alignas(8) std::array<std::byte, 0x60 + 2 * sizeof(Elf64_Phdr)> metadata{};
#endif

bool Install(std::byte* process, std::size_t size) {
#ifdef _WIN32
    auto* image = reinterpret_cast<std::byte*>(GetModuleHandleW(nullptr));
    if (image == nullptr) return false;
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(image + dos->e_lfanew);
    auto* sections = IMAGE_FIRST_SECTION(nt);
    IMAGE_SECTION_HEADER* target = nullptr;
    for (unsigned index = 0; index < nt->FileHeader.NumberOfSections; ++index) {
        if (std::memcmp(sections[index].Name, ".proctst", 8) == 0) {
            target = &sections[index];
            break;
        }
    }
    if (target == nullptr) return false;
    metadata[0] = static_cast<std::uint32_t>(process - image);
    metadata[1] = static_cast<std::uint32_t>(size);
    DWORD oldProtect = 0;
    if (VirtualProtect(target, sizeof(*target), PAGE_READWRITE, &oldProtect) == 0) return false;
    std::memcpy(target->Name, ".procpar", 8);
    target->Misc.VirtualSize = sizeof(metadata);
    DWORD restored = 0;
    VirtualProtect(target, sizeof(*target), oldProtect, &restored);
    return true;
#elif defined(__linux__)
    struct Request {
        std::byte* process;
        std::size_t size;
        bool installed;
    } request{process, size, false};
    dl_iterate_phdr([](dl_phdr_info* image, std::size_t, void* data) {
        auto& request = *static_cast<Request*>(data);
        if (image->dlpi_name != nullptr && image->dlpi_name[0] != '\0') return 0;
        for (std::uint16_t index = 0; index < image->dlpi_phnum; ++index) {
            const auto& header = image->dlpi_phdr[index];
            if (header.p_type == PT_LOAD || header.p_type == PT_GNU_EH_FRAME || header.p_type < 0x60000000u) continue;
            auto* mutableHeader = const_cast<Elf64_Phdr*>(&header);
            const auto pageSize = static_cast<std::uintptr_t>(sysconf(_SC_PAGESIZE));
            const auto pageStart = reinterpret_cast<std::uintptr_t>(mutableHeader) & ~(pageSize - 1);
            const auto pageEnd = (reinterpret_cast<std::uintptr_t>(mutableHeader + 1) + pageSize - 1) & ~(pageSize - 1);
            int protection = PROT_READ;
            for (std::uint16_t loadIndex = 0; loadIndex < image->dlpi_phnum; ++loadIndex) {
                const auto& load = image->dlpi_phdr[loadIndex];
                const auto start = image->dlpi_addr + load.p_vaddr;
                if (load.p_type == PT_LOAD && pageStart >= (start & ~(pageSize - 1)) && pageStart < start + load.p_memsz && (load.p_flags & PF_X)) protection |= PROT_EXEC;
            }
            if (mprotect(reinterpret_cast<void*>(pageStart), pageEnd - pageStart, protection | PROT_WRITE) != 0) return 1;
            mutableHeader->p_type = 0x61000001;
            mutableHeader->p_vaddr = reinterpret_cast<std::uintptr_t>(request.process) - image->dlpi_addr;
            mutableHeader->p_paddr = mutableHeader->p_vaddr;
            mutableHeader->p_filesz = request.size;
            mutableHeader->p_memsz = request.size;
            request.installed = mprotect(reinterpret_cast<void*>(pageStart), pageEnd - pageStart, protection) == 0;
            return 1;
        }
        return 1;
    }, &request);
    return request.installed;
#elif defined(__APPLE__)
    const std::uint64_t version = 2;
    const std::uint64_t loadBias = 0;
    const std::uint64_t count = 2;
    std::memcpy(metadata.data(), &version, sizeof(version));
    std::memcpy(metadata.data() + 0x08, &loadBias, sizeof(loadBias));
    std::memcpy(metadata.data() + 0x58, &count, sizeof(count));
    Elf64_Phdr headers[2]{};
    headers[0].p_type = PT_LOAD;
    headers[1].p_type = 0x61000001;
    for (auto& header : headers) {
        header.p_vaddr = reinterpret_cast<std::uintptr_t>(process);
        header.p_paddr = header.p_vaddr;
        header.p_filesz = size;
        header.p_memsz = size;
    }
    std::memcpy(metadata.data() + 0x60, headers, sizeof(headers));
    return true;
#else
    return false;
#endif
}

}
}

#endif
