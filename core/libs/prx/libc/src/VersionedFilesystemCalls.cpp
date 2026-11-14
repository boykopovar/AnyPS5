#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/FilesystemForwards.hpp"
#include "prx/libc/include/General.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
#include <stdexcept>
#include <string>

namespace {

constexpr std::uint32_t KernelErrnoBase = 0x80020000u;
constexpr std::uint32_t KernelExists = 0x80020011u;
constexpr std::uint32_t KernelNoSuchFile = 0x80020002u;
constexpr std::uint64_t NotFoundCodes = 0x2000000000140001ull;
constexpr int OutOfMemory = 12;
constexpr int NotSupported = 45;
constexpr int NoSystemCall = 78;
constexpr int TypeNotFound = -1;
constexpr int TypeNone = 0;
constexpr int TypeUnknown = 8;
constexpr int TypesByMode[] = {6, 5, 8, 2, 8, 4, 8, 1, 8, 3, 8, 7};
constexpr std::uint16_t DefaultDirectoryMode = 0777;
constexpr std::uint16_t CopiedPermissions = 06777;
constexpr int CopyTargetFlags = 0x601;
constexpr std::uint16_t CopyTargetMode = 0777;
constexpr std::size_t CopyBlock = 0x10000;
constexpr std::int64_t MicrosecondsPerSecond = 1000000;
constexpr std::int64_t NanosecondsPerMicrosecond = 1000;

std::atomic<const FilesystemForwards_nid_no_patch*> registeredForwards{nullptr};

const FilesystemForwards_nid_no_patch& Kernel(const char* function) {
    const auto* forwards = registeredForwards.load(std::memory_order_acquire);
    if (!forwards) throw std::runtime_error(std::string(function) + ": libkernel filesystem calls are not registered");
    return *forwards;
}

int ErrorOf(int result) {
    const auto code = static_cast<std::uint32_t>(result);
    return (code & 0xffff0000u) == KernelErrnoBase ? static_cast<int>(code & 0xffffu) : 1;
}

int StatusOf(int result) {
    return result == 0 ? 0 : ErrorOf(result);
}

bool NotFound(int result) {
    const auto index = static_cast<std::uint32_t>(result) - KernelNoSuchFile;
    return index <= 0x3du && ((NotFoundCodes >> index) & 1u) != 0;
}

int TypeOf(std::uint16_t mode) {
    const auto index = static_cast<std::uint32_t>(mode) - 0x1000u;
    return index >= 0xc000u ? TypeUnknown : TypesByMode[index >> 12];
}

int StatusType(const char* function, const char* path, int* perms, int* error) {
    FileStat status{};
    const int result = Kernel(function).stat(path, &status);
    int type = TypeNone;
    if (NotFound(result)) {
        type = TypeNotFound;
    } else if (result == 0) {
        if (perms) *perms = status.st_mode & 0xfff;
        type = TypeOf(status.st_mode);
    }
    if (error) *error = StatusOf(result);
    return type;
}

class CopyDescriptor {
    const FilesystemForwards_nid_no_patch& kernel;
    int descriptor;
public:
    CopyDescriptor(const FilesystemForwards_nid_no_patch& calls, int value) : kernel(calls), descriptor(value) {}
    ~CopyDescriptor() {
        if (descriptor < 0) return;
        try { Close(); } catch (...) {}
    }
    CopyDescriptor(const CopyDescriptor&) = delete;
    CopyDescriptor& operator=(const CopyDescriptor&) = delete;
    void Close() {
        const int value = descriptor;
        descriptor = -1;
        kernel.close(value);
    }
};

int CopyContents(const FilesystemForwards_nid_no_patch& kernel, int source, int target, std::byte* buffer) {
    for (;;) {
        const auto count = kernel.read(source, buffer, CopyBlock);
        if (count == 0) {
            FileStat status{};
            int result = kernel.fstat(source, &status);
            if (result == 0) result = kernel.fchmod(target, static_cast<std::uint16_t>(status.st_mode & CopiedPermissions));
            return StatusOf(result);
        }
        if (count < 0) return StatusOf(static_cast<int>(count));
        for (std::int64_t done = 0; done < count;) {
            const auto written = kernel.write(target, buffer + done, static_cast<std::size_t>(count - done));
            if (written < 0) return StatusOf(static_cast<int>(written));
            done += written;
        }
    }
}

}

extern "C" {

void RegisterFilesystemForwards_nid_no_patch(const FilesystemForwards_nid_no_patch* forwards) {
    registeredForwards.store(forwards, std::memory_order_release);
}

int APS5_VABI _ZNSt8__sce_v210filesystem6_LstatEPKcPNS0_5permsEPi_nid_postfix(const char* path, int* perms, int* error) {
    return StatusType(__func__, path, perms, error);
}

int APS5_VABI _ZNSt8__sce_v210filesystem6_LstatEPKcPNS0_5permsE_nid_postfix(const char* path, int* perms) {
    return StatusType(__func__, path, perms, nullptr);
}

std::uint64_t APS5_VABI _ZNSt8__sce_v210filesystem10_File_sizeEPKcPi_nid_postfix(const char* path, int* error) {
    FileStat status{};
    const int result = Kernel(__func__).stat(path, &status);
    if (result != 0 && error) *error = result;
    return result != 0 ? UINT64_MAX : static_cast<std::uint64_t>(status.st_size);
}

std::uint64_t APS5_VABI _ZNSt8__sce_v210filesystem10_File_sizeEPKc_nid_postfix(const char* path) {
    FileStat status{};
    return Kernel(__func__).stat(path, &status) != 0 ? UINT64_MAX : static_cast<std::uint64_t>(status.st_size);
}

std::int64_t APS5_VABI _ZNSt8__sce_v210filesystem16_Last_write_timeEPKc_nid_postfix(const char* path) {
    FileStat status{};
    return Kernel(__func__).posixStat(path, &status) != 0 ? -1 : status.st_mtim.tv_sec;
}

int APS5_VABI _ZNSt8__sce_v210filesystem16_Last_write_timeEPKcPlS3__nid_postfix(const char* path, std::int64_t* seconds, std::int64_t* nanoseconds) {
    FileStat status{};
    const int result = Kernel(__func__).stat(path, &status);
    if (result != 0) return ErrorOf(result);
    *seconds = status.st_mtim.tv_sec;
    *nanoseconds = status.st_mtim.tv_nsec;
    return 0;
}

bool APS5_VABI _ZNSt8__sce_v210filesystem20_Set_last_write_timeEPKcl_nid_postfix(const char* path, std::int64_t microseconds) {
    const auto& kernel = Kernel(__func__);
    FileStat status{};
    if (kernel.stat(path, &status) != 0) return false;
    std::int64_t seconds = microseconds / MicrosecondsPerSecond;
    std::int64_t remainder = microseconds - seconds * MicrosecondsPerSecond;
    if (remainder < 0) {
        --seconds;
        remainder += MicrosecondsPerSecond;
    }
    const KernelTimeval times[2] = {{status.st_atim.tv_sec, status.st_atim.tv_nsec / NanosecondsPerMicrosecond}, {seconds, remainder}};
    return kernel.utimes(path, times) == 0;
}

int APS5_VABI _ZNSt8__sce_v210filesystem9_Make_dirEPKcS2__nid_postfix(const char* path, const char* existing) {
    const auto& kernel = Kernel(__func__);
    std::uint16_t mode = DefaultDirectoryMode;
    if (existing) {
        FileStat status{};
        const int result = kernel.stat(existing, &status);
        if (result != 0) return -ErrorOf(result);
        mode = status.st_mode;
    }
    const int result = kernel.mkdir(path, mode);
    if (result == 0) return 1;
    if (static_cast<std::uint32_t>(result) == KernelExists) return 0;
    return -ErrorOf(result);
}

bool APS5_VABI _ZNSt8__sce_v210filesystem11_Remove_dirEPKc_nid_postfix(const char* path) {
    return Kernel(__func__).posixRmdir(path) == 0;
}

int APS5_VABI _ZNSt8__sce_v210filesystem7_UnlinkEPKc_nid_postfix(const char* path) {
    return StatusOf(Kernel(__func__).unlink(path));
}

int APS5_VABI _ZNSt8__sce_v210filesystem7_RenameEPKcS2__nid_postfix(const char* from, const char* to) {
    return StatusOf(Kernel(__func__).rename(from, to));
}

int APS5_VABI _ZNSt8__sce_v210filesystem7_ResizeEPKcm_nid_postfix(const char* path, std::uint64_t size) {
    return StatusOf(Kernel(__func__).truncate(path, static_cast<std::int64_t>(size)));
}

int APS5_VABI _ZNSt8__sce_v210filesystem6_ChmodEPKcNS0_5permsE_nid_postfix(const char* path, int perms) {
    return StatusOf(Kernel(__func__).chmod(path, static_cast<std::uint16_t>(perms)));
}

int APS5_VABI _ZNSt8__sce_v210filesystem10_Copy_fileEPKcS2__nid_postfix(const char* from, const char* to) {
    const auto& kernel = Kernel(__func__);
    const int source = kernel.open(from, 0, 0);
    if (source < 0) return ErrorOf(source);
    CopyDescriptor sourceOwner(kernel, source);
    const int target = kernel.open(to, CopyTargetFlags, CopyTargetMode);
    if (target < 0) {
        sourceOwner.Close();
        return ErrorOf(target);
    }
    CopyDescriptor targetOwner(kernel, target);
    const std::unique_ptr<std::byte[]> buffer(new (std::nothrow) std::byte[CopyBlock]);
    const int result = buffer ? CopyContents(kernel, source, target, buffer.get()) : OutOfMemory;
    targetOwner.Close();
    sourceOwner.Close();
    return result;
}

int APS5_VABI _ZNSt8__sce_v210filesystem8_StatvfsEPKcRNS0_10space_infoE_nid_postfix(const char*, std::uint64_t* space) {
    space[0] = UINT64_MAX;
    space[1] = UINT64_MAX;
    space[2] = UINT64_MAX;
    return NoSystemCall;
}

int APS5_VABI _ZNSt8__sce_v210filesystem13_Current_set2EPKc_nid_postfix(const char* path) {
    return std::strncmp(path, "/", 2) == 0 ? 0 : NotSupported;
}

}
