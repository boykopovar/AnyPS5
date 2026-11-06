#include "prx/libc/include/ApplicationHeap.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <array>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__) || defined(__APPLE__) || defined(__EMSCRIPTEN__)
#include "prx/libc/include/specifics/linux/ElfTypes.hpp"
#include <dlfcn.h>
#endif

bool ApplicationGuestHeapApi_nid_no_patch(void** api) {
#ifdef __linux__
    static const auto guest = [] {
        constexpr std::array<const char*, 10> names{
            "gQX+4GDQjpM#guest", "tIhsqj0qsFE#guest", "2X5agFjKxMc#guest", "Y7aJ1uydPMo#guest",
            "Ujf3KzMvRmI#guest", "OGybVuPAhAY#guest", "cVSk9y8URbc#guest", "CC-BLMBu9-I#guest",
            "KuOuD58hqn4#guest", "NDcSfcYZRC8#guest"
        };
        std::array<void*, 10> result{};
        bool found = false;
        for (std::size_t index = 0; index < names.size(); ++index) {
            result[index] = ::dlsym(RTLD_DEFAULT, names[index]);
            found |= result[index] != nullptr;
        }
        if (!found) return result;
        if (result[0] == nullptr) throw std::runtime_error("application heap: incomplete guest allocator API");
        Dl_info owner{};
        if (!::dladdr(result[0], &owner) || owner.dli_fbase == nullptr || owner.dli_fname == nullptr)
            throw std::runtime_error("application heap: cannot identify guest allocator owner");
        for (std::size_t index = 1; index < names.size(); ++index) {
            Dl_info info{};
            if (result[index] == nullptr || !::dladdr(result[index], &info) || info.dli_fbase != owner.dli_fbase)
                throw std::runtime_error("application heap: incomplete or mixed guest allocator API");
        }
        if (::dlopen(owner.dli_fname, RTLD_LAZY | RTLD_NOLOAD) == nullptr)
            throw std::runtime_error("application heap: cannot retain guest allocator owner");
        return result;
    }();
    if (guest[0] == nullptr) return false;
    std::memcpy(api, guest.data(), sizeof(guest));
    return true;
#else
    (void)api;
    return false;
#endif
}

const void* ApplicationEmptyProcessParameters_nid_no_patch() {
    alignas(16) static const std::byte empty[0x40] = {};
    return empty;
}

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
    bool found = false;
    for (unsigned index = 0; index < nt->FileHeader.NumberOfSections; ++index) {
        const auto& section = sections[index];
        if (std::memcmp(section.Name, ".procpar", 8) != 0) continue;
        if (found) throw std::runtime_error("application heap: duplicate process metadata");
        found = true;
        if (section.Misc.VirtualSize != 8 || section.VirtualAddress > imageSize || 8 > imageSize - section.VirtualAddress) throw std::runtime_error("application heap: invalid process metadata section");
        std::uint32_t address;
        std::uint32_t size;
        std::memcpy(&address, image + section.VirtualAddress, sizeof(address));
        std::memcpy(&size, image + section.VirtualAddress + 4, sizeof(size));
        if (address == 0 && size == 0) {
            result = ApplicationEmptyProcessParameters_nid_no_patch();
            continue;
        }
        if (size < 0x40 || address > imageSize || size > imageSize - address) throw std::runtime_error("application heap: process parameters exceed the image");
        result = image + address;
    }
    if (!found) throw std::runtime_error("application heap: process metadata is missing; relink the executable");
    return result;
#elif defined(__linux__) || defined(__APPLE__) || defined(__EMSCRIPTEN__)
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
