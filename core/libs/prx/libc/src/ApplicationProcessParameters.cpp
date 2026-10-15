#include "prx/libc/include/ApplicationHeap.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__)
#include "prx/libc/include/specifics/linux/ElfTypes.hpp"
#endif

const void* ApplicationProcessParameters_nid_no_patch() {
#ifdef _WIN32
    const auto* image = reinterpret_cast<const std::byte*>(GetModuleHandleW(nullptr));
    if (image == nullptr) throw std::runtime_error("application heap: main image is unavailable");
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0) throw std::runtime_error("application heap: invalid DOS header");
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(image + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) throw std::runtime_error("application heap: invalid PE header");
    const auto* sections = IMAGE_FIRST_SECTION(nt);
    const auto imageSize = nt->OptionalHeader.SizeOfImage;
    const void* result = nullptr;
    for (unsigned index = 0; index < nt->FileHeader.NumberOfSections; ++index) {
        const auto& section = sections[index];
        if (std::memcmp(section.Name, ".procpar", 8) != 0) continue;
        if (result != nullptr) throw std::runtime_error("application heap: duplicate process metadata");
        if (section.Misc.VirtualSize != 8 || section.VirtualAddress > imageSize || 8 > imageSize - section.VirtualAddress) throw std::runtime_error("application heap: invalid process metadata section");
        std::uint32_t address;
        std::uint32_t size;
        std::memcpy(&address, image + section.VirtualAddress, sizeof(address));
        std::memcpy(&size, image + section.VirtualAddress + 4, sizeof(size));
        if (size < 0x40 || address > imageSize || size > imageSize - address) throw std::runtime_error("application heap: process parameters exceed the image");
        result = image + address;
    }
    if (result == nullptr) throw std::runtime_error("application heap: process metadata is missing; relink the executable");
    return result;
#elif defined(__linux__) || defined(__APPLE__)
    struct Search {
        const void* parameters = nullptr;
        bool invalid = false;
    } search;
    dl_iterate_phdr([](dl_phdr_info* image, std::size_t, void* data) {
        auto& result = *static_cast<Search*>(data);
        if (image->dlpi_name != nullptr && image->dlpi_name[0] != '\0') return 0;
        for (std::uint16_t index = 0; index < image->dlpi_phnum; ++index) {
            const auto& header = image->dlpi_phdr[index];
            if (header.p_type != 0x61000001) continue;
            if (result.parameters != nullptr || header.p_filesz < 0x40) {
                result.invalid = true;
                return 1;
            }
            bool mapped = false;
            for (std::uint16_t loadIndex = 0; loadIndex < image->dlpi_phnum; ++loadIndex) {
                const auto& load = image->dlpi_phdr[loadIndex];
                if (load.p_type == PT_LOAD && header.p_vaddr >= load.p_vaddr && header.p_vaddr - load.p_vaddr <= load.p_memsz && header.p_filesz <= load.p_memsz - (header.p_vaddr - load.p_vaddr)) mapped = true;
            }
            if (!mapped) {
                result.invalid = true;
                return 1;
            }
            result.parameters = reinterpret_cast<const void*>(image->dlpi_addr + header.p_vaddr);
        }
        return 1;
    }, &search);
    if (search.invalid || search.parameters == nullptr) throw std::runtime_error("application heap: invalid or missing process parameters");
    return search.parameters;
#else
    throw std::runtime_error("application heap: unsupported executable format");
#endif
}
