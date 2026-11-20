#include "prx/libkernel_sys/include/AdditionalExports.hpp"
#include "prx/libc/include/General.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <string>

#if defined(__linux__)
#include <dlfcn.h>
#include <link.h>

#include <array>
#include <map>
#include <mutex>
#endif

namespace {

#if defined(__linux__)
using CfiCheck = int (APS5_VABI*)(std::uint64_t, void*, void*);

constexpr std::uintptr_t PageShift = 12;
constexpr std::array<const char*, 4> CfiCheckNames{"__cfi_check", "iSB5fJWVqIU", "__cfi_check#guest", "iSB5fJWVqIU#guest"};

struct Image {
    bool found = false;
    std::uintptr_t base = 0;
    std::string name;
    unsigned long long adds = 0;
    unsigned long long subs = 0;
};

struct Search {
    std::uintptr_t page = 0;
    Image image;
};

int FindImage(dl_phdr_info* info, std::size_t size, void* data) {
    auto& search = *static_cast<Search*>(data);
    if (size >= offsetof(dl_phdr_info, dlpi_subs) + sizeof(info->dlpi_subs)) {
        search.image.adds = info->dlpi_adds;
        search.image.subs = info->dlpi_subs;
    }
    for (std::uint16_t index = 0; index < info->dlpi_phnum; ++index) {
        const auto& header = info->dlpi_phdr[index];
        if (header.p_type != PT_LOAD || header.p_memsz == 0) continue;
        const auto begin = static_cast<std::uintptr_t>(info->dlpi_addr + header.p_vaddr);
        const auto end = begin + static_cast<std::uintptr_t>(header.p_memsz);
        if (search.page < (begin >> PageShift) || search.page > ((end - 1) >> PageShift)) continue;
        search.image.found = true;
        search.image.base = static_cast<std::uintptr_t>(info->dlpi_addr);
        search.image.name = info->dlpi_name != nullptr ? info->dlpi_name : "";
        return 1;
    }
    return 0;
}

bool OwnedBy(const void* symbol, std::uintptr_t base) {
    Dl_info info{};
    link_map* map = nullptr;
    return ::dladdr1(symbol, &info, reinterpret_cast<void**>(&map), RTLD_DL_LINKMAP) != 0 && map != nullptr && map->l_addr == base;
}

std::uintptr_t ExportedCfiCheck(const Image& image) {
    void* handle = ::dlopen(image.name.empty() ? nullptr : image.name.c_str(), RTLD_LAZY | RTLD_NOLOAD);
    if (handle == nullptr) return 0;
    std::uintptr_t found = 0;
    for (const char* name : CfiCheckNames) {
        void* symbol = ::dlsym(handle, name);
        if (symbol != nullptr && OwnedBy(symbol, image.base)) {
            found = reinterpret_cast<std::uintptr_t>(symbol);
            break;
        }
    }
    ::dlclose(handle);
    return found;
}

std::uintptr_t CachedCfiCheck(const Image& image) {
    static std::mutex mutex;
    static std::map<std::uintptr_t, std::uintptr_t> checks;
    static unsigned long long adds = 0;
    static unsigned long long subs = 0;
    {
        std::lock_guard lock(mutex);
        if (image.adds != adds || image.subs != subs) {
            checks.clear();
            adds = image.adds;
            subs = image.subs;
        }
        const auto cached = checks.find(image.base);
        if (cached != checks.end()) return cached->second;
    }
    const auto check = ExportedCfiCheck(image);
    std::lock_guard lock(mutex);
    if (image.adds == adds && image.subs == subs) checks.emplace(image.base, check);
    return check;
}

void SlowPath(const char* caller, std::uint64_t callSiteTypeId, void* target, void* diagData) {
    const auto address = reinterpret_cast<std::uintptr_t>(target);
    Search search{address >> PageShift, {}};
    ::dl_iterate_phdr(FindImage, &search);
    const auto check = search.image.found ? CachedCfiCheck(search.image) : 0;
    const bool invalid = !search.image.found || (check != 0 && (address >> PageShift) < (check >> PageShift));
    if (invalid) {
        if (diagData != nullptr) {
            __ubsan_handle_cfi_check_fail_nid_postfix(static_cast<Ubsan::CfiCheckFailData*>(diagData), address, 0);
            return;
        }
        char message[128];
        std::snprintf(message, sizeof(message), "%s: CFI: invalid memory region for a check target: %p", caller, target);
        throw std::runtime_error(message);
    }
    if (check == 0) return;
    reinterpret_cast<CfiCheck>(check)(callSiteTypeId, target, diagData);
}
#endif

}

extern "C" {

void APS5_VABI __cfi_slowpath_nid_postfix(std::uint64_t callSiteTypeId, void* target) {
#if defined(__linux__)
    SlowPath(__func__, callSiteTypeId, target, nullptr);
#else
    static_cast<void>(callSiteTypeId);
    static_cast<void>(target);
    NotImplemented_nid_no_patch(__func__);
#endif
}

void APS5_VABI __cfi_slowpath_diag_nid_postfix(std::uint64_t callSiteTypeId, void* target, void* diagData) {
#if defined(__linux__)
    SlowPath(__func__, callSiteTypeId, target, diagData);
#else
    static_cast<void>(callSiteTypeId);
    static_cast<void>(target);
    static_cast<void>(diagData);
    NotImplemented_nid_no_patch(__func__);
#endif
}

}
