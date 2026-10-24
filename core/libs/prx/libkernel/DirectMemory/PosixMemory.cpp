#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "DirectMemory.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libkernel/File/include/ArchivedFile.hpp"
#include "prx/libkernel/File/include/SharedMemory.hpp"
#include <algorithm>
#include <limits>
#include <cerrno>
#include <new>
#include <stdexcept>
#include <system_error>
#if defined(__linux__) || defined(__APPLE__)
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#elif defined(_WIN32)
#include "prx/libc/include/WindowsDescriptor.hpp"
#include "prx/libkernel/File/include/DirectoryDescriptor.hpp"
#include "prx/libkernel/File/include/RandomDevice.hpp"
#endif

namespace {
// FreeBSD/PS5 ABI values, independent of the host's errno and mmap constants.
constexpr int GuestInvalid = 22;
constexpr int GuestNoMemory = 12;
constexpr int GuestBadDescriptor = 9;
constexpr int GuestAccess = 13;
constexpr int GuestFault = 14;
constexpr int GuestBusy = 16;
constexpr int GuestIoError = 5;
constexpr int GuestNotSupported = 45;
constexpr int GuestShared = 0x1;
constexpr int GuestPrivate = 0x2;
constexpr int GuestAnonymous = 0x1000;
constexpr int GuestMapFixed = 0x10;
constexpr int GuestMapExclusive = 0x4000;
constexpr int GuestMapNoOverwrite = 0x80;
constexpr int GuestMapNoCoalesce = 0x400000;
constexpr int GuestSyncAsync = 0x1;
constexpr int GuestSyncInvalidate = 0x2;
constexpr int GuestAdviceCore = 9;
constexpr int GuestAdviceProtect = 10;
constexpr int GuestLockCurrent = 0x1;
constexpr int GuestLockFuture = 0x2;
constexpr int GuestDefinedProtection = 0x3F7;
constexpr std::uintptr_t GuestUserAddressEnd = 0x800000000000;
constexpr int GuestAlignedSuper = 0x1000000;
constexpr std::size_t SuperpageSize = std::size_t{1} << 21;

#if defined(__linux__) || defined(__APPLE__)
class ScopedDescriptor {
public:
    explicit ScopedDescriptor(int value) noexcept : descriptor(fcntl(value, F_DUPFD_CLOEXEC, 0)), error(descriptor < 0 ? errno : 0) {}
    ~ScopedDescriptor() {
        const int savedError = errno;
        if (descriptor >= 0) close(descriptor);
        errno = savedError;
    }
    ScopedDescriptor(const ScopedDescriptor&) = delete;
    ScopedDescriptor& operator=(const ScopedDescriptor&) = delete;
    int Get() const noexcept { return descriptor; }
    int Error() const noexcept { return error; }
private:
    int descriptor = -1;
    int error = 0;
};

bool RegularFile(int descriptor) {
    struct stat status{};
    return ::fstat(descriptor, &status) == 0 && S_ISREG(status.st_mode);
}
#endif

bool RoundLength(std::size_t length, std::size_t& rounded) {
    constexpr auto mask = PS5_PAGE_SIZE - 1;
    if (length == 0 || length > std::numeric_limits<std::size_t>::max() - mask)
        return false;
    rounded = (length + mask) & ~mask;
    return true;
}

void SetError(int error) {
    *__error_nid_postfix() = error;
}

bool Mapped(std::uintptr_t address) {
    VirtualQueryInfo info{};
    return sceKernelVirtualQuery(reinterpret_cast<const void*>(address), 0, &info, sizeof(info)) == 0;
}

#ifdef _WIN32
int DescriptorAccess(HANDLE handle, int descriptor) noexcept {
    try {
        return WindowsDescriptorAccess_nid_no_patch(handle, descriptor, "mmap");
    } catch (const std::exception&) {
        return -1;
    }
}

template <typename Failed>
void* MapPrivateFileCopy(void* address, std::size_t rounded, int protection, int placement, std::size_t alignment, HANDLE handle,
                         int descriptor, std::int64_t offset, int accessMode, Failed failed) noexcept {
    if (accessMode == 1) return failed(GuestNotSupported);
    LARGE_INTEGER fileBytes{};
    if (!GetFileSizeEx(handle, &fileBytes)) return failed(GuestBadDescriptor);
    const auto size = static_cast<std::uint64_t>(fileBytes.QuadPart);
    const auto start = static_cast<std::uint64_t>(offset);
    if (start + rounded > ((size + PS5_PAGE_SIZE - 1) & ~static_cast<std::uint64_t>(PS5_PAGE_SIZE - 1))) return failed(GuestNotSupported);
    void* mapped = address;
    try {
        if (DoMapAnon(&mapped, rounded, 3, placement, alignment, false) != 0) return failed(GuestNoMemory);
    } catch (const std::invalid_argument&) {
        return failed(GuestInvalid);
    } catch (const std::exception&) {
        return failed(GuestNoMemory);
    }
    const auto copied = std::min<std::uint64_t>(rounded, size - start);
    for (std::uint64_t done = 0; done < copied;) {
        const auto read = sceKernelPread(descriptor, static_cast<unsigned char*>(mapped) + done, static_cast<std::size_t>(copied - done), static_cast<std::int64_t>(start + done));
        if (read <= 0) {
            munmap_nid_postfix(mapped, rounded);

            return failed(read < 0 ? static_cast<int>(read & 0xffff) : GuestIoError);
        }
        done += static_cast<std::uint64_t>(read);
    }
    if (protection != 3 && mprotect_nid_postfix(mapped, rounded, protection) != 0) {
        const int error = *__error_nid_postfix();
        munmap_nid_postfix(mapped, rounded);

        return failed(error);
    }
    return mapped;
}
#endif
}

extern "C" {

void* APS5_VABI mmap_nid_postfix(void* address, std::size_t length, int protection,
                                int flags, int descriptor, std::int64_t offset) noexcept {
    const auto failed = [](int error) -> void* {
        SetError(error);
        return reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1));
    };
    std::size_t rounded;
    if (!RoundLength(length, rounded) || (protection & ~7) != 0)
        return failed(GuestInvalid);
    if ((flags & (GuestMapExclusive | GuestMapFixed)) == GuestMapExclusive) return failed(GuestInvalid);
    if ((flags & (GuestShared | GuestPrivate)) == (GuestShared | GuestPrivate)) return failed(GuestInvalid);
    int mappingFlags = flags & ~GuestAlignedSuper;
    if ((mappingFlags & (GuestShared | GuestPrivate | GuestAnonymous)) == GuestAnonymous) mappingFlags |= GuestPrivate;
    const bool fixedAnonymous = (mappingFlags & ~(GuestMapFixed | GuestMapExclusive)) == (GuestPrivate | GuestAnonymous)
                                && (mappingFlags & GuestMapFixed) != 0;
    if (mappingFlags == (GuestPrivate | GuestAnonymous) || fixedAnonymous) {
        if (descriptor != -1 || offset != 0) return failed(GuestInvalid);
        const auto start = reinterpret_cast<std::uintptr_t>(address);
        if (fixedAnonymous && ((start & (PS5_PAGE_SIZE - 1)) != 0 ||
            rounded > std::numeric_limits<std::uintptr_t>::max() - start)) return failed(GuestInvalid);
#ifndef _WIN32
        if (fixedAnonymous && !GuestArena::GuestArenaContains_nid_postfix(address, rounded)) return failed(GuestInvalid);
#endif
        const bool exclusive = fixedAnonymous && (mappingFlags & GuestMapExclusive) != 0;
        const int placement = fixedAnonymous ? GuestMapFixed | (exclusive ? GuestMapNoOverwrite : 0) : 0;
        void* mapped = address;
        const auto alignment = !fixedAnonymous && (flags & GuestAlignedSuper) != 0 ? SuperpageSize : PS5_PAGE_SIZE;
        try {
            const auto result = DoMapAnon(&mapped, rounded, protection, placement, alignment, exclusive, fixedAnonymous);
            if (result != 0) return failed(result == SCE_KERNEL_ERROR_EINVAL ? GuestInvalid : GuestNoMemory);
            if (fixedAnonymous) ReleaseFlexibleMappingRange(start, rounded);
            return mapped;
        } catch (const std::invalid_argument&) {
            return failed(fixedAnonymous ? GuestInvalid : GuestNoMemory);
        } catch (const std::bad_alloc&) {
            return failed(GuestNoMemory);
        } catch (const std::exception&) {
            return failed(GuestNoMemory);
        }
    }
    const int kind = mappingFlags & (GuestShared | GuestPrivate);
    int placement = mappingFlags & (GuestMapFixed | GuestMapNoOverwrite | GuestMapNoCoalesce);
    if ((mappingFlags & GuestAnonymous) != 0 ||
        (kind != GuestShared && kind != GuestPrivate) ||
        (mappingFlags & ~(GuestShared | GuestPrivate | GuestMapFixed | GuestMapNoOverwrite | GuestMapNoCoalesce)) != 0)
        return failed(GuestNotSupported);
    if (descriptor < 0) return failed(GuestBadDescriptor);
    File::RefuseArchivedFile(descriptor, "mmap");
    if (offset < 0 || (static_cast<std::uint64_t>(offset) & (PS5_PAGE_SIZE - 1)) != 0 ||
        rounded > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()) ||
        static_cast<std::uint64_t>(offset) > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) - rounded)
        return failed(GuestInvalid);
#if defined(__linux__) || defined(__APPLE__)
    const ScopedDescriptor pinnedDescriptor(descriptor);
    if (pinnedDescriptor.Get() < 0) return failed(pinnedDescriptor.Error());
    const int mappingDescriptor = pinnedDescriptor.Get();
    const int descriptorFlags = fcntl(mappingDescriptor, F_GETFL);
    if (descriptorFlags < 0) return failed(errno);
    const int accessMode = descriptorFlags & O_ACCMODE;
    if ((protection & 1) != 0 && accessMode == O_WRONLY) return failed(GuestAccess);
    if (kind == GuestShared && (protection & 2) != 0 && accessMode == O_RDONLY) return failed(GuestAccess);
    if (!File::IsSharedMemoryDescriptor_nid_no_patch(mappingDescriptor) && !RegularFile(mappingDescriptor)) return failed(GuestNotSupported);
#elif defined(_WIN32)
    const int mappingDescriptor = descriptor;
    if (!File::IsSharedMemoryDescriptor_nid_no_patch(mappingDescriptor)) {
        const HANDLE handle = WindowsDescriptorHandle_nid_no_patch(mappingDescriptor);
        if (handle == INVALID_HANDLE_VALUE) return failed(GuestBadDescriptor);
        if (File::DirectoryDescriptorPath(mappingDescriptor) || File::IsRandomDevice(mappingDescriptor) || GetFileType(handle) != FILE_TYPE_DISK) return failed(GuestNotSupported);
        const int accessMode = DescriptorAccess(handle, mappingDescriptor);
        if (accessMode < 0) return failed(GuestBadDescriptor);
        if ((protection & 1) != 0 && accessMode == 1) return failed(GuestAccess);
        if (kind == GuestShared && (protection & 2) != 0 && accessMode == 0) return failed(GuestAccess);
        if (kind == GuestPrivate) {
            const std::size_t privateAlignment = (flags & GuestAlignedSuper) != 0 ? SuperpageSize : PS5_PAGE_SIZE;
            return MapPrivateFileCopy(address, rounded, protection, placement, privateAlignment, handle, mappingDescriptor, offset, accessMode, failed);
        }
    }
#else
    const int mappingDescriptor = descriptor;
    if (!File::IsSharedMemoryDescriptor_nid_no_patch(mappingDescriptor)) return failed(GuestNotSupported);
#endif
    const std::size_t alignment = (flags & GuestAlignedSuper) != 0 ? SuperpageSize : PS5_PAGE_SIZE;
    void* mapped = nullptr;
    try {
        mapped = address;
        const auto result = DoMapFileBacked_nid_no_patch(&mapped, rounded, protection, kind | placement, mappingDescriptor, static_cast<std::uint64_t>(offset), alignment);
        if (result != 0) return failed(result == SCE_KERNEL_ERROR_EINVAL ? GuestInvalid : GuestNoMemory);
        if ((placement & GuestMapFixed) != 0) ReleaseFlexibleMappingRange(reinterpret_cast<std::uintptr_t>(mapped), rounded);
        return mapped;
    } catch (const std::invalid_argument&) {
        return failed(GuestInvalid);
    } catch (const std::system_error& error) {
        const int hostError = error.code().value();
        if (hostError == ENOTSUP || hostError == EOPNOTSUPP) return failed(GuestNotSupported);
        if (hostError == EACCES || hostError == EPERM) return failed(GuestAccess);
        if (hostError == EBADF) return failed(GuestBadDescriptor);
        return failed(hostError > 0 && hostError <= 34 ? hostError : GuestIoError);
    } catch (const std::bad_alloc&) {
        return failed(GuestNoMemory);
    } catch (const std::exception&) {
        return failed(GuestNoMemory);
    }
}

int APS5_VABI munmap_nid_postfix(void* address, std::size_t length) noexcept {
    const auto failed = [](int error) {
        SetError(error);
        return -1;
    };
    std::size_t rounded;
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    if (!RoundLength(length, rounded) || start == 0 ||
        (start & (PS5_PAGE_SIZE - 1)) != 0 ||
        rounded > std::numeric_limits<std::uintptr_t>::max() - start)
        return failed(GuestInvalid);
    try {
        if (sceKernelMunmap(start, rounded) != 0) return failed(GuestInvalid);
        return 0;
    } catch (const std::bad_alloc&) {
        return failed(GuestNoMemory);
    } catch (const std::exception&) {
        // The allocation tracker rejects foreign, pinned, and noncontiguous ranges.
        return failed(GuestInvalid);
    }
}

int APS5_VABI mprotect_nid_postfix(void* address, std::size_t length, int protection) noexcept {
    const auto failed = [](int error) {
        SetError(error);
        return -1;
    };
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    if (length == 0) return 0;
    if (start == 0 || length > std::numeric_limits<std::uintptr_t>::max() - start)
        return failed(GuestInvalid);
    try {
        if (DoMprotect(address, length, protection & GuestDefinedProtection) != 0) return failed(GuestInvalid);
        return 0;
    } catch (const std::bad_alloc&) {
        return failed(GuestNoMemory);
    } catch (const std::system_error& error) {
        const int hostError = error.code().value();
        return failed(hostError == EACCES || hostError == EPERM ? GuestAccess : GuestInvalid);
    } catch (const std::exception&) {
        return failed(GuestInvalid);
    }
}

int APS5_VABI mlock_nid_postfix(const void* address, std::size_t length) {
    const int result = sceKernelMlock_nid_postfix(const_cast<void*>(address), length);
    if (result == 0) return 0;
    SetError(result & 0xffff);
    return -1;
}

int APS5_VABI munlock_nid_postfix(const void* address, std::size_t length) {
    const int result = sceKernelMunlock_nid_postfix(const_cast<void*>(address), length);
    if (result == 0) return 0;
    SetError(result & 0xffff);
    return -1;
}

int APS5_VABI mlockall_nid_postfix(int flags) {
    if (flags == 0 || (flags & ~(GuestLockCurrent | GuestLockFuture)) != 0) {
        SetError(GuestInvalid);
        return -1;
    }
    if ((flags & GuestLockFuture) != 0) throw std::runtime_error("mlockall: MCL_FUTURE is not implemented: later mappings are not locked");
    throw std::runtime_error("mlockall: MCL_CURRENT is not implemented: the process also holds host stacks, heaps, modules and driver mappings");
}

int APS5_VABI munlockall_nid_postfix() {
    const int result = sceKernelMunlockall_nid_postfix();
    if (result == 0) return 0;
    SetError(result & 0xffff);
    return -1;
}

int APS5_VABI msync_nid_postfix(void* address, std::size_t length, int flags) {
    const auto failed = [](int error) {
        SetError(error);
        return -1;
    };
    constexpr std::uintptr_t mask = PS5_PAGE_SIZE - 1;
    const auto first = reinterpret_cast<std::uintptr_t>(address);
    const auto start = first & ~mask;
    const std::uintptr_t size = (length + (first - start) + mask) & ~mask;
    const auto end = start + size;
    if (end < start) return failed(GuestInvalid);
    if ((flags & (GuestSyncAsync | GuestSyncInvalidate)) == (GuestSyncAsync | GuestSyncInvalidate)) return failed(GuestInvalid);
    if (start == end ? !Mapped(start) : !GuestRangeMapped(start, end)) return failed(GuestNoMemory);
    if ((flags & GuestSyncInvalidate) != 0) return failed(GuestBusy);
    return 0;
}

int APS5_VABI madvise_nid_postfix(void* address, std::size_t length, int advice) {
    if (advice == GuestAdviceProtect) return 0;
    const auto start = reinterpret_cast<std::uintptr_t>(address);
    if (advice < 0 || advice > GuestAdviceCore || start > GuestUserAddressEnd || length > GuestUserAddressEnd - start) {
        SetError(GuestInvalid);
        return -1;
    }
    return 0;
}

int APS5_VABI mincore_nid_postfix(const void* address, std::size_t length, char* vec) {
    const auto failed = [](int error) {
        SetError(error);
        return -1;
    };
    constexpr std::uintptr_t mask = PS5_PAGE_SIZE - 1;
    const auto first = reinterpret_cast<std::uintptr_t>(address) & ~mask;
    if (length > std::numeric_limits<std::uintptr_t>::max() - mask) return failed(GuestNoMemory);
    const auto size = (length + mask) & ~mask;
    if (size > std::numeric_limits<std::uintptr_t>::max() - first) return failed(GuestNoMemory);
    const auto end = first + size;
    if (size == 0 ? !Mapped(first) : !GuestRangeMapped(first, end)) return failed(GuestNoMemory);
    if (size == 0) return 0;
    if (vec == nullptr) return failed(GuestFault);
    for (auto cursor = first; cursor < end;) {
        VirtualQueryInfo info{};
        sceKernelVirtualQuery(reinterpret_cast<const void*>(cursor), 0, &info, sizeof(info));

        const auto extentEnd = std::min<std::uintptr_t>(info.end, end);
        std::fill_n(vec + (cursor - first) / PS5_PAGE_SIZE, (extentEnd - cursor) / PS5_PAGE_SIZE, static_cast<char>(info.is_committed != 0 ? 1 : 0));
        cursor = extentEnd;
    }
    return 0;
}

}
