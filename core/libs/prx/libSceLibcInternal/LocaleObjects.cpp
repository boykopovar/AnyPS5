#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports2.hpp"
#include "prx/libSceLibcInternal/include/RuntimeInternals.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <mach/mach_vm.h>
#else
#include <fstream>
#include <string>
#endif

namespace {

std::atomic<GuestLocale::Implementation*> g_globalLocale{nullptr};

static_assert(offsetof(GuestLocale::Implementation, base) == 0);

struct ErrnoGuard {
    int saved = errno;
    ~ErrnoGuard() { errno = saved; }
};

struct GuestWriteScope {
    ErrnoGuard savedErrno;
    GuestArena::HostWrite write;
    bool ok = false;

    GuestWriteScope(void* address, std::size_t size) : write(address, size) {
        ok = address != nullptr && write.Open() && GuestWritable_nid_no_patch(address, size);
    }
};

template <typename T>
bool CopyIn(const void* source, T& out) {
    if (source == nullptr) return false;
    const ErrnoGuard savedErrno;
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(source, sizeof(T));
    return GuestReadable_nid_no_patch(source, sizeof(T)) &&
        GuestCopyIn_nid_no_patch(&out, source, sizeof(T));
}

template <typename T>
bool CopyOut(void* destination, const T& value) {
    const GuestWriteScope scope(destination, sizeof(T));
    return scope.ok && GuestCopyOut_nid_no_patch(destination, &value, sizeof(T));
}

bool IsExecutableAddress(const void* address) {
    if (address == nullptr) return false;
    const ErrnoGuard savedErrno;
#ifdef _WIN32
    MEMORY_BASIC_INFORMATION info{};
    if (VirtualQuery(address, &info, sizeof(info)) != sizeof(info)) return false;
    if (info.State != MEM_COMMIT) return false;
    const DWORD protect = info.Protect & 0xffU;
    return protect == PAGE_EXECUTE || protect == PAGE_EXECUTE_READ ||
        protect == PAGE_EXECUTE_READWRITE || protect == PAGE_EXECUTE_WRITECOPY;
#elif defined(__APPLE__)
    const mach_vm_address_t query = reinterpret_cast<mach_vm_address_t>(address);
    mach_vm_address_t region = query;
    mach_vm_size_t size = 0;
    vm_region_basic_info_data_64_t info{};
    mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t object = MACH_PORT_NULL;
    if (mach_vm_region(mach_task_self(), &region, &size, VM_REGION_BASIC_INFO_64,
            reinterpret_cast<vm_region_info_t>(&info), &count, &object) != KERN_SUCCESS) {
        return false;
    }
    if (object != MACH_PORT_NULL) mach_port_deallocate(mach_task_self(), object);
    return query >= region && (query - region) < size && (info.protection & VM_PROT_EXECUTE) != 0;
#else
    const std::uintptr_t target = reinterpret_cast<std::uintptr_t>(address);
    std::ifstream maps("/proc/self/maps");
    std::string line;
    while (std::getline(maps, line)) {
        const std::size_t dash = line.find('-');
        const std::size_t space = line.find(' ');
        if (dash == std::string::npos || space == std::string::npos || space <= dash + 1) continue;
        char* endFirst = nullptr;
        char* endSecond = nullptr;
        const unsigned long long start = std::strtoull(line.c_str(), &endFirst, 16);
        const unsigned long long end = std::strtoull(line.c_str() + dash + 1, &endSecond, 16);
        if (endFirst != line.c_str() + dash || endSecond != line.c_str() + space) continue;
        if (target < start || target >= end) continue;
        return space + 3 < line.size() && line[space + 3] == 'x';
    }
    return false;
#endif
}

} // namespace

extern "C" {

void APS5_VABI sceLibcInternalLocinfoBaseConstructor_nid_no_patch(void* self, const char* localeName) {
    _ZNSt8_LocinfoC1EPKc_nid_postfix(static_cast<GuestLocale::LocinfoStorage*>(self), localeName);
}
APS5_EXPORT("pN02FS5SPgg", sceLibcInternalLocinfoBaseConstructor_nid_no_patch);

void APS5_VABI sceLibcInternalLocinfoBaseDestructor_nid_no_patch(void* self) {
    _ZNSt8_LocinfoD1Ev_nid_postfix(static_cast<GuestLocale::LocinfoStorage*>(self));
}
APS5_EXPORT("YXVCU6PdgZk", sceLibcInternalLocinfoBaseDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalLocimpBaseCopyConstructor_nid_no_patch(void* self, const void* source) {
    _ZNSt6locale7_LocimpC1ERKS0__nid_postfix(static_cast<GuestLocale::Implementation*>(self), static_cast<const GuestLocale::Implementation*>(source));
}
APS5_EXPORT("dsJKehuajH4", sceLibcInternalLocimpBaseCopyConstructor_nid_no_patch);

GuestLocaleFacade* APS5_VABI _ZNSt6locale6globalERKS__nid_postfix(GuestLocaleFacade* result, const GuestLocaleFacade* locale) {
    GuestLocaleFacade localeCopy{};
    if (!CopyIn(locale, localeCopy) || localeCopy.implementation == nullptr) {
        throw std::invalid_argument("locale::global: null locale");
    }
    GuestLocale::Implementation implCopy{};
    if (!CopyIn(localeCopy.implementation, implCopy) || implCopy.base.vtable == nullptr) {
        throw std::invalid_argument("locale::global: unreadable locale implementation");
    }
    GuestLocale::FacetVtable vtableCopy{};
    if (!CopyIn(implCopy.base.vtable, vtableCopy) ||
        vtableCopy.retain == nullptr || vtableCopy.release == nullptr || vtableCopy.deleteObject == nullptr ||
        !IsExecutableAddress(reinterpret_cast<const void*>(vtableCopy.retain)) ||
        !IsExecutableAddress(reinterpret_cast<const void*>(vtableCopy.release)) ||
        !IsExecutableAddress(reinterpret_cast<const void*>(vtableCopy.deleteObject))) {
        throw std::invalid_argument("locale::global: invalid locale vtable");
    }
    const GuestWriteScope resultWrite(result, sizeof(*result));
    if (!resultWrite.ok) {
        throw std::invalid_argument("locale::global: unwritable result");
    }
    const GuestLocaleFacade swapped{LibcSwapGlobalLocale_nid_no_patch(localeCopy.implementation)};
    if (!GuestCopyOut_nid_no_patch(result, &swapped, sizeof(swapped))) {
        throw std::invalid_argument("locale::global: unwritable result");
    }
    return result;
}

void APS5_VABI _ZNSt6locale7_LocimpC2Eb_nid_postfix(GuestLocale::Implementation* self, bool transparent) {
    const GuestLocale::Implementation value{{LibcLocimpVtable_nid_no_patch(), 1, 0}, nullptr, 0, 0, transparent, "*"};
    if (!CopyOut(self, value)) {
        throw std::invalid_argument("_Locimp: null object");
    }
}

void APS5_VABI _ZNSt8_LocinfoC1EiPKc_nid_postfix(GuestLocale::LocinfoStorage* self, int category, const char* localeName) {
    if (self == nullptr) {
        throw std::invalid_argument("_Locinfo: null object");
    }
    if (category < 0 || (static_cast<unsigned int>(category) & ~0x3fU) != 0) {
        throw std::invalid_argument("_Locinfo: invalid locale category");
    }
    _ZNSt8_LocinfoC1EPKc_nid_postfix(self, localeName);
}

void* APS5_VABI _Getgloballocale_nid_postfix() {
    return g_globalLocale.load(std::memory_order_acquire);
}

void APS5_VABI _Setgloballocale_nid_postfix(void* pointer) {
    g_globalLocale.store(static_cast<GuestLocale::Implementation*>(pointer), std::memory_order_release);
}

void APS5_VABI _Deletegloballocale_nid_postfix(void* pointer) {
    GuestLocale::Implementation* locale = nullptr;
    if (!CopyIn(pointer, locale)) {
        throw std::invalid_argument("_Deletegloballocale: null locale reference");
    }
    if (locale == nullptr) return;
    GuestLocale::Facet baseCopy{};
    if (!CopyIn(locale, baseCopy) || baseCopy.vtable == nullptr) {
        throw std::invalid_argument("_Deletegloballocale: unreadable locale object");
    }
    GuestLocale::FacetVtable vtableCopy{};
    if (!CopyIn(baseCopy.vtable, vtableCopy) || vtableCopy.release == nullptr ||
        !IsExecutableAddress(reinterpret_cast<const void*>(vtableCopy.release))) {
        throw std::invalid_argument("_Deletegloballocale: invalid locale vtable");
    }
    auto* released = vtableCopy.release(reinterpret_cast<GuestLocale::Facet*>(locale));
    if (released == nullptr) return;
    GuestLocale::Facet releasedCopy{};
    if (!CopyIn(released, releasedCopy) || releasedCopy.vtable == nullptr) {
        throw std::invalid_argument("_Deletegloballocale: unreadable released locale");
    }
    GuestLocale::FacetVtable releasedVtable{};
    if (!CopyIn(releasedCopy.vtable, releasedVtable) || releasedVtable.deleteObject == nullptr ||
        !IsExecutableAddress(reinterpret_cast<const void*>(releasedVtable.deleteObject))) {
        throw std::invalid_argument("_Deletegloballocale: invalid released locale vtable");
    }
    releasedVtable.deleteObject(released);
}

}
