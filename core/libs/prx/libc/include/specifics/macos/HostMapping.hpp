#ifndef PRX_LIBC_INCLUDE_SPECIFICS_MACOS_HOSTMAPPING_HPP
#define PRX_LIBC_INCLUDE_SPECIFICS_MACOS_HOSTMAPPING_HPP

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <libproc.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <optional>
#include <stdexcept>
#include <string>
#include <sys/proc_info.h>
#include <unistd.h>

namespace HostMapping {

struct Mapping {
    std::uint64_t start;
    std::uint64_t end;
    std::string path;
    bool readable = false;
    bool writable = false;
    bool executable = false;
    bool shared = false;
    std::uint64_t inode = 0;
};

inline std::optional<Mapping> CoveringOrNext(std::uint64_t address, const char* caller) {
    proc_regionwithpathinfo info{};
    errno = 0;
    const int bytes = proc_pidinfo(getpid(), PROC_PIDREGIONPATHINFO, address, &info, sizeof(info));
    if (bytes == 0 && errno == EINVAL) return std::nullopt;
    if (bytes != static_cast<int>(sizeof(info))) throw std::runtime_error(std::string(caller) + ": PROC_PIDREGIONPATHINFO failed: " + std::strerror(errno));
    const auto& region = info.prp_prinfo;
    const auto mode = region.pri_share_mode;
    return Mapping{region.pri_address, region.pri_address + region.pri_size, info.prp_vip.vip_path, (region.pri_protection & VM_PROT_READ) != 0,
        (region.pri_protection & VM_PROT_WRITE) != 0, (region.pri_protection & VM_PROT_EXECUTE) != 0,
        mode == SM_SHARED || mode == SM_TRUESHARED || mode == SM_SHARED_ALIASED, info.prp_vip.vip_vi.vi_stat.vst_ino};
}

inline std::optional<Mapping> Find(std::uint64_t address, const char* caller) {
    auto mapping = CoveringOrNext(address, caller);
    if (!mapping || mapping->start > address) return std::nullopt;
    return mapping;
}

template <typename Visit>
void Walk(std::uint64_t begin, std::uint64_t end, const char* caller, Visit&& visit) {
    for (auto cursor = begin; cursor < end;) {
        const auto mapping = CoveringOrNext(cursor, caller);
        if (!mapping || mapping->start >= end) return;
        if (mapping->end <= cursor) throw std::runtime_error(std::string(caller) + ": PROC_PIDREGIONPATHINFO went backwards");
        visit(*mapping);
        cursor = mapping->end;
    }
}

inline bool Readable(std::uint64_t address, const char* caller) {
    char byte = 0;
    mach_vm_size_t read = 0;
    const kern_return_t result = mach_vm_read_overwrite(mach_task_self(), address, 1, reinterpret_cast<mach_vm_address_t>(&byte), &read);
    if (result == KERN_SUCCESS && read == 1) return true;
    if (result == KERN_INVALID_ADDRESS || result == KERN_PROTECTION_FAILURE || result == KERN_MEMORY_ERROR) return false;
    throw std::runtime_error(std::string(caller) + ": mach_vm_read_overwrite failed with " + std::to_string(result));
}

inline bool PlaceNoReplace(std::uint64_t address, std::uint64_t bytes) {
    mach_vm_address_t wanted = address;
    return mach_vm_allocate(mach_task_self(), &wanted, bytes, VM_FLAGS_FIXED) == KERN_SUCCESS;
}

}

#endif
