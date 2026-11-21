extern "C" {
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/File/include/NativeStat.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/FilesystemError.hpp"
#include "prx/libc/include/RenameDot.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/AppArchive.hpp"
#include "prx/libc/include/FileStream.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/SharedMemory.hpp"
#include "prx/libkernel/File/include/DirectoryDescriptor.hpp"
#include "prx/libkernel/File/include/DescriptorControl.hpp"
#include "prx/libkernel/File/include/FileLock.hpp"
#include "prx/libkernel/Socket/include/SocketRuntime.hpp"
#include "prx/libkernel/Socket/include/SocketPoll.hpp"
#include "prx/libkernel/File/include/RandomDevice.hpp"
#include "prx/libkernel/File/include/HostStreams.hpp"
#include "prx/libkernel/File/include/ArchivedFile.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "SceTypes.hpp"

#include <cerrno>
#include <cstdarg>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <windows.h>
#include "prx/libc/include/WindowsDescriptor.hpp"
#include "prx/libc/include/WindowsVerbatimPath.hpp"
#include "prx/libkernel/File/include/WindowsSymbolicLink.hpp"
std::int64_t APS5_VABI sceKernelRead(int d, void* buf, std::size_t nbytes) {
    if (File::IsDirectoryDescriptor(d)) return SceErrorFromErrno(EISDIR);
    if (IsKqueueDescriptor_nid_no_patch(d)) return nbytes == 0 ? 0 : SCE_KERNEL_ERROR_EOPNOTSUPP;
    if (GuestSockets::IsOpen(d)) {
        const auto n = GuestSockets::Read(d, buf, nbytes);
        return n < 0 ? SceKernelError(*__error_nid_postfix()) : n;
    }
    if (buf == nullptr) {
        if (nbytes != 0) return SceIoErrorFromErrno(DescriptorIsOpen_nid_no_patch(d) ? EFAULT : EBADF);
        char empty = 0;
        return sceKernelRead(d, &empty, 0);
    }
    const GuestArena::HostWrite destination(buf, nbytes);
    if (!destination.Open()) errno = EFAULT;
    else if (File::ReadRandomDevice(d, buf, nbytes)) return static_cast<std::int64_t>(nbytes);
    else if (const auto archived = File::ReadArchivedFile(d, buf, nbytes, nullptr)) return *archived < 0 ? SceIoErrorFromErrno(static_cast<int>(-*archived)) : *archived;
    auto n = destination.Open() ? NativeRead(d, buf, nbytes) : -1;
    if (n < 0) return SceIoErrorFromErrno(errno);
    return static_cast<std::int64_t>(n);
}

std::int64_t APS5_VABI sceKernelWrite(int d, const void* buf, std::size_t nbytes) {
    if (File::IsVirtualDirectoryDescriptor(d)) return SceErrorFromErrno(EISDIR);
    if (IsKqueueDescriptor_nid_no_patch(d)) return SCE_KERNEL_ERROR_EOPNOTSUPP;
    if (GuestSockets::IsOpen(d)) {
        const auto n = GuestSockets::Write(d, buf, nbytes);
        return n < 0 ? SceKernelError(*__error_nid_postfix()) : n;
    }
    if (buf == nullptr) {
        if (nbytes != 0) return SceIoErrorFromErrno(DescriptorIsOpen_nid_no_patch(d) ? EFAULT : EBADF);
        const char empty = 0;
        return sceKernelWrite(d, &empty, 0);
    }
    const File::WriteSync sync(d);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(buf, nbytes);
    auto n = NativeWrite(d, buf, nbytes);
    if (n < 0) return SceIoErrorFromErrno(errno);
    return static_cast<std::int64_t>(n);
}

static bool SeekOverflows(int d, std::int64_t offset, int whence) {
    const std::int64_t current = NativeLseek(d, 0, 1);
    if (current < 0) return false;
    std::int64_t base = current;
    if (whence == 2) {
        base = NativeLseek(d, 0, 2);
        NativeLseek(d, current, 0);
        if (base < 0) return false;
    }
    return base > std::numeric_limits<std::int64_t>::max() - offset;
}

std::int64_t APS5_VABI sceKernelLseek(int d, std::int64_t offset, int whence) {
    if (GuestSockets::IsOpen(d) || IsKqueueDescriptor_nid_no_patch(d)) return SCE_KERNEL_ERROR_ESPIPE;
    if (whence < 0 || whence > 2) {
        throw std::invalid_argument(std::string(__func__) + ": invalid whence=" + std::to_string(whence));
    }
    if (const auto archived = File::SeekArchivedFile(d, offset, whence)) return *archived < 0 ? SceIoErrorFromErrno(-*archived) : *archived;
    if (const auto directory = File::SeekDirectoryDescriptor(d, offset, whence)) {
        if (*directory < 0) return SceIoErrorFromErrno(errno);
        return *directory;
    }
    std::int64_t result = NativeLseek(d, offset, whence);
    if (result < 0) {
        const int error = errno;
        if (error == EINVAL && offset > 0 && (whence == 1 || whence == 2) && SeekOverflows(d, offset, whence)) return SceIoErrorFromErrno(EOVERFLOW);
        return SceIoErrorFromErrno(error);
    }
    return result;
}

constexpr int GuestEfault = 14;

static int StatNative(const std::filesystem::path& native, bool directory, FileStat* sb) {
    std::error_code error;
    constexpr int GuestEnotdir = 20;
    FileStat archived{};
    if (File::FillArchivedStat(native, &archived)) {
        if (directory && (archived.st_mode & 0170000) != 0040000) return SceErrorFromErrno(GuestEnotdir);
        if (sb == nullptr || !GuestCopyOut_nid_no_patch(sb, &archived, sizeof(archived))) return SceErrorFromErrno(GuestEfault);
        return 0;
    }
    auto metadataPath = native;
#ifdef _WIN32
    metadataPath = NativeMetadataPath(native, error);
    if (error) return SceErrorFromErrno(FilesystemError(error));
#endif
    const auto status = std::filesystem::status(metadataPath, error);
    if (error == std::errc::not_a_directory) return SceErrorFromErrno(GuestEnotdir);
    if (error == std::errc::filename_too_long || error == std::errc::too_many_symbolic_link_levels) return SceKernelError(FilesystemError(error));
    if (!std::filesystem::exists(status)) {
#ifdef _WIN32
        if (NativeSymbolicLinkLoop(metadataPath)) return SCE_KERNEL_ERROR_ELOOP;
#endif
        return SceErrorFromErrno(NotFoundError_nid_no_patch(native));
    }
    if (directory && !std::filesystem::is_directory(status)) return SceErrorFromErrno(GuestEnotdir);
    if (sb == nullptr) return SceErrorFromErrno(GuestEfault);
    FileStat result{};
    if (!File::TryFillFileStat(metadataPath, &result)) return SceStatErrorFromErrno(errno);
    if (!GuestCopyOut_nid_no_patch(sb, &result, sizeof(result))) return SceErrorFromErrno(GuestEfault);
    return 0;
}

int APS5_VABI sceKernelStat(const char* path, FileStat* sb) {
    std::string copy;
    if (const int error = CopyGuestPath_nid_no_patch(path, copy)) return SceKernelError(error);
    path = copy.c_str();
    if (copy.empty()) return SceErrorFromErrno(2);
    std::filesystem::path native;
    if (const int error = ResolvePathError_nid_no_patch(path, native, true, true)) return SceKernelError(error);
    return StatNative(native, std::string_view(path).back() == '/', sb);
}

int StatInDirectory_nid_no_patch(int directory, const char* path, FileStat* sb, bool follow) {
    int error = 0;
    const char* base = GetDirectoryDescriptorPath_nid_no_patch(directory, &error);
    if (base == nullptr) return SceErrorFromErrno(error);
    const auto native = std::filesystem::path(reinterpret_cast<const char8_t*>(base)) / std::filesystem::path(reinterpret_cast<const char8_t*>(path));
    if (follow) return StatNative(native, std::string_view(path).back() == '/', sb);
    if (sb == nullptr) return SceErrorFromErrno(GuestEfault);
    FileStat result{};
    if (!File::FillLinkStat(native, &result)) return SceErrorFromErrno(errno);
    if (!GuestCopyOut_nid_no_patch(sb, &result, sizeof(result))) return SceErrorFromErrno(GuestEfault);
    return 0;
}

int APS5_VABI sceKernelUnlink(const char* path) {
    std::string copy;
    if (const int error = CopyGuestPath_nid_no_patch(path, copy)) return SceKernelError(error);
    path = copy.c_str();
    if (copy.empty()) return SceErrorFromErrno(2);
    if (const int dot = RenameDotError_nid_no_patch(path)) return SceKernelError(dot);
    std::filesystem::path native;
    if (const int error = ResolvePathError_nid_no_patch(path, native)) return SceKernelError(error);
#ifdef _WIN32
    std::error_code unlinkPathError;
    const auto metadataUnlinkPath = NativeMetadataPath(native, unlinkPathError);
    const auto& unlinkNative = unlinkPathError ? native : metadataUnlinkPath;
#else
    const auto& unlinkNative = native;
#endif
    constexpr int GuestEperm = 1;
    std::error_code error;
    bool directory = std::filesystem::is_directory(std::filesystem::symlink_status(unlinkNative, error));
#ifdef _WIN32
    constexpr DWORD DirectoryLink = FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT;
    const DWORD linkAttributes = GetFileAttributesW(unlinkNative.c_str());
    if (linkAttributes != INVALID_FILE_ATTRIBUTES && (linkAttributes & DirectoryLink) == DirectoryLink) directory = true;
#endif
#ifdef _WIN32
    if (unlinkNative.has_filename() && unlinkNative.filename().native().find_first_of(L"*?<>\"") == std::wstring::npos) {
        WIN32_FIND_DATAW found{};
        if (const HANDLE find = FindFirstFileW(unlinkNative.c_str(), &found); find != INVALID_HANDLE_VALUE) {
            FindClose(find);
            constexpr DWORD DirectoryLink = FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY;
            if ((found.dwFileAttributes & DirectoryLink) == DirectoryLink && found.dwReserved0 == IO_REPARSE_TAG_SYMLINK) {
                if (!RemoveDirectoryW(unlinkNative.c_str())) return SceErrorFromErrno(GetLastError() == ERROR_ACCESS_DENIED ? 13 : 5);
                RecordWrittenPath_nid_no_patch(native);
                return 0;
            }
        }
    }
#endif
    if (directory) {
#ifdef _WIN32
        const DWORD attributes = GetFileAttributesW(unlinkNative.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            std::string junctionTarget;
            if (File::WindowsSymbolicLink::ReadJunction(unlinkNative, junctionTarget, "unlink")) {
                if (RemoveDirectoryW(unlinkNative.c_str()) == 0) return SceErrorFromErrno(GuestEperm);
                RecordWrittenPath_nid_no_patch(native);
                return 0;
            }
        }
#endif
        return SceErrorFromErrno(GuestEperm);
    }
    if (NativeUnlink(unlinkNative) != 0) {
#ifdef _WIN32
        if (NativeSymbolicLinkLoop(unlinkNative.parent_path())) return SCE_KERNEL_ERROR_ELOOP;
#endif
        return SceErrorFromErrno(errno == ENOENT ? NotFoundError_nid_no_patch(native) : errno);
    }
    RecordWrittenPath_nid_no_patch(native);
    return 0;
}

int APS5_VABI sceKernelFcntl(int d, int command, ...) {
    const auto type = File::FcntlArgumentOf(command);
    std::intptr_t argument = 0;
    if (type != File::FcntlArgument::None) {
#ifdef _WIN32
        __builtin_sysv_va_list arguments;
        __builtin_sysv_va_start(arguments, command);
        argument = type == File::FcntlArgument::Int ? __builtin_va_arg(arguments, int)
                                                    : reinterpret_cast<std::intptr_t>(__builtin_va_arg(arguments, void*));
        __builtin_sysv_va_end(arguments);
#else
        std::va_list arguments;
        va_start(arguments, command);
        argument = type == File::FcntlArgument::Int ? va_arg(arguments, int)
                                                    : reinterpret_cast<std::intptr_t>(va_arg(arguments, void*));
        va_end(arguments);
#endif
    }
    int result;
    if (GuestSockets::IsOpen(d)) {
        if (type == File::FcntlArgument::Int) result = fcntl_nid_postfix(d, command, static_cast<int>(argument));
        else if (type == File::FcntlArgument::Pointer) result = fcntl_nid_postfix(d, command, reinterpret_cast<void*>(argument));
        else result = fcntl_nid_postfix(d, command);
    } else {
        result = File::Fcntl(d, command, argument);
    }
    return result < 0 ? SceKernelError(*__error_nid_postfix()) : result;
}

}
