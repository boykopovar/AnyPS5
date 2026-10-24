#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <limits>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/FilesystemError.hpp"
#include "prx/libc/include/RenameDot.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/SharedMemory.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/File/include/NativeStat.hpp"
#include "prx/libkernel/File/include/KernelStatfs.hpp"
#include "prx/libkernel/File/include/DirectoryDescriptor.hpp"
#include "prx/libkernel/File/include/DescriptorControl.hpp"
#include "prx/libkernel/File/include/FileLock.hpp"
#include "prx/libkernel/File/include/RandomDevice.hpp"
#include "prx/libkernel/File/include/HostStreams.hpp"
#include "prx/libkernel/File/include/ArchivedFile.hpp"
#include "prx/libc/include/AppArchive.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "prx/libkernel/Socket/include/SocketRuntime.hpp"
#include "prx/libkernel/Socket/include/SocketPoll.hpp"
#include <atomic>
#include <cerrno>
#include <cstring>
#include <cstdarg>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#ifdef __APPLE__
#include <mach/mach.h>
#include <mach/mach_vm.h>
#endif

struct KernelIovec {
    void* base;
    std::size_t length;
};
static_assert(sizeof(KernelIovec) == sizeof(GuestSockets::Iovec));
static_assert(sizeof(KernelIovec) == sizeof(AppArchiveBuffer) && offsetof(KernelIovec, length) == offsetof(AppArchiveBuffer, Length));

static constexpr int KERNEL_IOV_MAX = 1024;

static constexpr std::uint32_t KERNEL_STATFS_VERSION = 0x20030518;
static constexpr std::uint64_t KERNEL_MNT_RDONLY = 0x1;
static constexpr std::uint64_t KERNEL_MNT_NOEXEC = 0x4;
static constexpr std::uint64_t KERNEL_MNT_NOSUID = 0x8;

#ifdef _WIN32
#include <windows.h>
#include "prx/libkernel/File/include/WindowsSymbolicLink.hpp"
#include <io.h>
#include <cstdio>
#include <fcntl.h>
#include <direct.h>
#include <sys/stat.h>
#include "prx/libkernel/File/include/WindowsFileTime.hpp"
#include "prx/libc/include/WindowsDescriptor.hpp"
extern "C" _invalid_parameter_handler _set_thread_local_invalid_parameter_handler(_invalid_parameter_handler);
static void IgnoreDescriptorParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, std::uintptr_t) {}
template <typename TCall> static auto WithoutParameterHandler(TCall call) {
    const auto previous = _set_thread_local_invalid_parameter_handler(IgnoreDescriptorParameter);
    const auto result = call();
    _set_thread_local_invalid_parameter_handler(previous);
    return result;
}
static int NativeRmdir(const std::filesystem::path& path) {
    return ::_wrmdir(path.wstring().c_str());
}
static bool RemoveEmptyDirectory(const std::filesystem::path& path) {
    struct _stat64 status{};
    const bool unlocked = ::_wstat64(path.c_str(), &status) == 0 && !(status.st_mode & _S_IWRITE) &&
        ::_wchmod(path.c_str(), _S_IREAD | _S_IWRITE) == 0;
    std::error_code error;
    if (std::filesystem::remove(path, error)) return true;
    if (unlocked) ::_wchmod(path.c_str(), _S_IREAD);
    return false;
}
static int NativeMkdir(const std::filesystem::path& path, std::uint16_t mode) {
    (void)mode;
    return ::_wmkdir(path.wstring().c_str());
}
static int NativeChmod(const std::filesystem::path& path, int mode) {
    RecordWrittenPath_nid_no_patch(path);
    return ::_wchmod(path.wstring().c_str(), mode);
}
std::optional<std::filesystem::path> File::WindowsDescriptorPath_nid_no_patch(int descriptor) {
    const auto handle = reinterpret_cast<HANDLE>(::_get_osfhandle(descriptor));
    if (handle == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        return std::nullopt;
    }
    std::wstring path(MAX_PATH, L'\0');
    auto length = ::GetFinalPathNameByHandleW(handle, path.data(), static_cast<DWORD>(path.size()), FILE_NAME_NORMALIZED);
    if (length >= path.size()) {
        path.resize(length);
        length = ::GetFinalPathNameByHandleW(handle, path.data(), length, FILE_NAME_NORMALIZED);
    }
    if (length == 0) {
        errno = WindowsOpenError_nid_no_patch(GetLastError());
        return std::nullopt;
    }
    if (length >= path.size()) {
        errno = EINVAL;
        return std::nullopt;
    }
    path.resize(length);
    if (path.starts_with(LR"(\\?\UNC\)")) path.replace(0, 8, LR"(\\)");
    else if (path.starts_with(LR"(\\?\)") && path.size() > 6 && path[5] == L':' && path[6] == L'\\') path.erase(0, 4);
    return path;
}
static int NativeFchmod(int descriptor, int mode) {
    const auto path = File::WindowsDescriptorPath_nid_no_patch(descriptor);
    return path ? NativeChmod(*path, mode) : -1;
}
static int TruncateThroughWriter(HANDLE handle, std::int64_t length) {
    const auto writer = ReOpenFile(handle, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0);
    if (writer == INVALID_HANDLE_VALUE) return WindowsOpenError_nid_no_patch(GetLastError());
    FILE_END_OF_FILE_INFO end{};
    end.EndOfFile.QuadPart = length;
    const bool truncated = SetFileInformationByHandle(writer, FileEndOfFileInfo, &end, sizeof(end));
    const DWORD error = GetLastError();
    CloseHandle(writer);
    return truncated ? 0 : WindowsOpenError_nid_no_patch(error);
}
static int NativeFtruncate(int descriptor, std::int64_t length) {
    const int result = File::TruncateSharedMemory_nid_no_patch(descriptor, length);
    if (result != -2) return result;
    if (File::IsDirectoryDescriptor(descriptor)) return EINVAL;
    const auto positioned = File::LockPositionedTransfer(descriptor);
    const auto handle = reinterpret_cast<HANDLE>(WithoutParameterHandler([descriptor] { return ::_get_osfhandle(descriptor); }));
    if (handle == INVALID_HANDLE_VALUE) return EBADF;
    if (::GetFileType(handle) != FILE_TYPE_DISK) return EINVAL;
    if (WindowsHandleOnlyAppends_nid_no_patch(WindowsHandleAccess_nid_no_patch(handle, descriptor, "ftruncate"))) return TruncateThroughWriter(handle, length);
    FILE_END_OF_FILE_INFO end{};
    end.EndOfFile.QuadPart = length;
    if (::SetFileInformationByHandle(handle, FileEndOfFileInfo, &end, sizeof(end))) return 0;
    switch (::GetLastError()) {
        case ERROR_ACCESS_DENIED: return EACCES;
        case ERROR_DISK_FULL:
        case ERROR_HANDLE_DISK_FULL: return ENOSPC;
        case ERROR_FILE_TOO_LARGE:
        case ERROR_INVALID_PARAMETER: return EFBIG;
        default: return EINVAL;
    }
}
static int NativeTruncate(const std::filesystem::path& path, std::int64_t length) {
    const int descriptor = ::_wopen(path.c_str(), _O_WRONLY | _O_BINARY);
    if (descriptor < 0) return errno;
    const int error = NativeFtruncate(descriptor, length);
    ::_close(descriptor);
    return error;
}
static int SetTimes(HANDLE handle, const KernelTimeval* times) {
    FILETIME access{}, modified{};
    if (times == nullptr) {
        GetSystemTimePreciseAsFileTime(&access);
        modified = access;
    } else if (!File::WindowsFileTime::Encode(times[0], access) || !File::WindowsFileTime::Encode(times[1], modified)) {
        const FILETIME unchanged{0xFFFFFFFF, 0xFFFFFFFF};
        SetFileTime(handle, nullptr, &unchanged, &unchanged);
        errno = EINVAL;
        return -1;
    }
    FILETIME creation{};
    if (!GetFileTime(handle, &creation, nullptr, nullptr)) return File::WindowsFileTime::Failure(GetLastError());
    const auto birth = CompareFileTime(&modified, &creation) < 0 ? &modified : nullptr;
    return SetFileTime(handle, birth, &access, &modified) ? 0 : File::WindowsFileTime::Failure(GetLastError());
}
static int NativeUtimes(const std::filesystem::path& path, const KernelTimeval* times, bool followLink = true) {
    const auto handle = CreateFileW(path.c_str(), FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | (followLink ? 0 : FILE_FLAG_OPEN_REPARSE_POINT), nullptr);
    if (handle == INVALID_HANDLE_VALUE) return File::WindowsFileTime::Failure(GetLastError());
    const int result = SetTimes(handle, times);
    CloseHandle(handle);
    if (result == 0) RecordWrittenPath_nid_no_patch(path);
    return result;
}
static int NativeFutimes(int descriptor, const KernelTimeval* times) {
    if (const auto directory = File::DirectoryDescriptorPath(descriptor)) return NativeUtimes(*directory, times);
    const auto original = reinterpret_cast<HANDLE>(::_get_osfhandle(descriptor));
    if (original == INVALID_HANDLE_VALUE) { errno = EBADF; return -1; }
    if (GetFileType(original) != FILE_TYPE_DISK) { errno = EINVAL; return -1; }
    const auto handle = ReOpenFile(original, FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0);
    if (handle == INVALID_HANDLE_VALUE) return File::WindowsFileTime::Failure(GetLastError());
    const int result = SetTimes(handle, times);
    CloseHandle(handle);
    if (result == 0) {
        if (const auto path = File::WindowsDescriptorPath_nid_no_patch(descriptor)) RecordWrittenPath_nid_no_patch(*path);
    }
    return result;
}
static int NativeSync(int descriptor) {
    constexpr DWORD share = FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
    HANDLE handle = INVALID_HANDLE_VALUE;
    if (const auto directory = File::DirectoryDescriptorPath(descriptor)) {
        handle = CreateFileW(directory->c_str(), GENERIC_WRITE, share, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    } else {
        const int previous = errno;
        const auto pipe = reinterpret_cast<HANDLE>(WithoutParameterHandler([descriptor] { return ::_get_osfhandle(descriptor); }));
        if (pipe != INVALID_HANDLE_VALUE && GetFileType(pipe) == FILE_TYPE_PIPE) { errno = EINVAL; return -1; }
        if (::_commit(descriptor) == 0) return 0;
        const auto original = reinterpret_cast<HANDLE>(::_get_osfhandle(descriptor));
        if (original == INVALID_HANDLE_VALUE || GetFileType(original) != FILE_TYPE_DISK) { errno = EBADF; return -1; }
        handle = ReOpenFile(original, GENERIC_WRITE, share, 0);
        errno = previous;
    }
    if (handle == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_ACCESS_DENIED ? 0 : File::WindowsFileTime::Failure(GetLastError());
    const int result = FlushFileBuffers(handle) ? 0 : File::WindowsFileTime::Failure(GetLastError());
    CloseHandle(handle);
    return result;
}
static int NativeFlock(int descriptor, int operation) {
    return File::Flock(descriptor, operation);
}
struct PositionedLock {
    std::mutex mutex;
};
std::mutex g_positionedTableLock;
std::unordered_map<int, std::shared_ptr<PositionedLock>> g_positionedLocks;
std::unique_lock<std::mutex> File::LockPositionedTransfer(int descriptor) {
    std::shared_ptr<PositionedLock> entry;
    {
        std::lock_guard table(g_positionedTableLock);
        auto& slot = g_positionedLocks[descriptor];
        if (!slot) slot = std::make_shared<PositionedLock>();
        entry = slot;
    }
    return std::unique_lock<std::mutex>(entry->mutex);
}
void File::SharePositionedTransfer(int from, int to) {
    std::lock_guard table(g_positionedTableLock);
    auto& source = g_positionedLocks[from];
    if (!source) source = std::make_shared<PositionedLock>();
    g_positionedLocks[to] = source;
}
static std::int64_t NativePositionedTransfer(int descriptor, void* buf, std::size_t nbytes, std::int64_t offset, bool write) {
    if (nbytes > static_cast<std::size_t>(std::numeric_limits<DWORD>::max())) {
        throw std::runtime_error("NativePositioned: nbytes exceeds platform limit");
    }
    if (write) {
        const auto result = File::PwriteSharedMemory_nid_no_patch(descriptor, buf, nbytes, offset);
        if (result != -2) return result;
    }
    const auto handle = reinterpret_cast<HANDLE>(WithoutParameterHandler([descriptor] { return ::_get_osfhandle(descriptor); }));
    if (handle == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        return -1;
    }
    if (File::IsDirectoryDescriptor(descriptor)) {
        errno = write ? EBADF : EISDIR;
        return -1;
    }
    LARGE_INTEGER position{};
    if (::GetFileType(handle) != FILE_TYPE_DISK || !::SetFilePointerEx(handle, LARGE_INTEGER{}, &position, FILE_CURRENT)) {
        errno = ESPIPE;
        return -1;
    }
    const auto at = write && File::Appends(descriptor) ? ~std::uint64_t{} : static_cast<std::uint64_t>(offset);
    OVERLAPPED overlapped{};
    overlapped.Offset = static_cast<DWORD>(at);
    overlapped.OffsetHigh = static_cast<DWORD>(at >> 32u);
    DWORD done = 0;
    const BOOL ok = write ? ::WriteFile(handle, buf, static_cast<DWORD>(nbytes), &done, &overlapped)
                          : ::ReadFile(handle, buf, static_cast<DWORD>(nbytes), &done, &overlapped);
    const DWORD error = ok ? ERROR_SUCCESS : ::GetLastError();
    if (!::SetFilePointerEx(handle, position, nullptr, FILE_BEGIN)) {
        throw std::runtime_error("NativePositioned: cannot restore the file offset");
    }
    if (!ok) {
        if (!write && error == ERROR_HANDLE_EOF) return 0;
        errno = error == ERROR_ACCESS_DENIED ? EBADF : EIO;
        return -1;
    }
    return done;
}
std::int64_t NativePositioned_nid_no_patch(int descriptor, void* buf, std::size_t nbytes, std::int64_t offset, bool write) {
    const auto positioned = File::LockPositionedTransfer(descriptor);
    return NativePositionedTransfer(descriptor, buf, nbytes, offset, write);
}
std::int64_t File::WriteAtEnd(int descriptor, const void* buf, std::size_t nbytes) {
    if (nbytes > static_cast<std::size_t>(std::numeric_limits<DWORD>::max())) {
        throw std::runtime_error("WriteAtEnd: nbytes exceeds platform limit");
    }
    const auto handle = reinterpret_cast<HANDLE>(WithoutParameterHandler([descriptor] { return ::_get_osfhandle(descriptor); }));
    if (handle == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        return -1;
    }
    if (::GetFileType(handle) != FILE_TYPE_DISK) return -2;
    OVERLAPPED end{};
    end.Offset = 0xFFFFFFFF;
    end.OffsetHigh = 0xFFFFFFFF;
    DWORD done = 0;
    if (!::WriteFile(handle, buf, static_cast<DWORD>(nbytes), &done, &end)) {
        const DWORD error = ::GetLastError();
        errno = error == ERROR_ACCESS_DENIED ? EBADF : error == ERROR_DISK_FULL || error == ERROR_HANDLE_DISK_FULL ? ENOSPC : EIO;
        return -1;
    }
    return done;
}
static std::int64_t NativeTransferBuffer(int descriptor, void* buf, std::size_t nbytes, bool write) {
    if (nbytes > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("NativeTransfer: nbytes exceeds platform limit");
    }
    if (write) {
        const auto result = File::WriteSharedMemory_nid_no_patch(descriptor, buf, nbytes);
        if (result != -2) return result;
        if (File::Appends(descriptor)) {
            const auto appended = File::WriteAtEnd(descriptor, buf, nbytes);
            if (appended != -2) return appended;
        }
    }
    return write ? ::_write(descriptor, buf, static_cast<unsigned int>(nbytes)) : ::_read(descriptor, buf, static_cast<unsigned int>(nbytes));
}
static std::int64_t NativeTransfer(int descriptor, void* buf, std::size_t nbytes, bool write) {
    const auto positioned = File::LockPositionedTransfer(descriptor);
    return NativeTransferBuffer(descriptor, buf, nbytes, write);
}
static bool NativeIsDisk(int descriptor) {
    const auto handle = reinterpret_cast<HANDLE>(::_get_osfhandle(descriptor));
    return handle != INVALID_HANDLE_VALUE && ::GetFileType(handle) == FILE_TYPE_DISK;
}
static bool NativePipeHasData(int descriptor) {
    const auto handle = reinterpret_cast<HANDLE>(::_get_osfhandle(descriptor));
    DWORD available = 0;
    return handle != INVALID_HANDLE_VALUE && ::GetFileType(handle) == FILE_TYPE_PIPE
        && ::PeekNamedPipe(handle, nullptr, 0, nullptr, &available, nullptr) && available != 0;
}
static std::int64_t NativePread(int descriptor, void* buf, std::size_t nbytes, std::int64_t offset) {
    return NativePositioned_nid_no_patch(descriptor, buf, nbytes, offset, false);
}
static std::int64_t NativePwrite(int descriptor, const void* buf, std::size_t nbytes, std::int64_t offset) {
    return NativePositioned_nid_no_patch(descriptor, const_cast<void*>(buf), nbytes, offset, true);
}
#else
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <dirent.h>
#include <sys/syscall.h>
#include <sys/uio.h>
static int NativeRmdir(const std::filesystem::path& path) {
    return ::rmdir(path.c_str());
}
static bool RemoveEmptyDirectory(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::remove(path, error);
}
static int NativeMkdir(const std::filesystem::path& path, std::uint16_t mode) {
    return ::mkdir(path.c_str(), static_cast<mode_t>(mode));
}
static int NativeChmod(const std::filesystem::path& path, int mode) {
    return ::chmod(path.c_str(), static_cast<mode_t>(mode));
}
static int NativeFchmod(int descriptor, int mode) {
    return ::fchmod(descriptor, static_cast<mode_t>(mode));
}
static int NativeFtruncate(int descriptor, std::int64_t length) {
    const int result = File::TruncateSharedMemory_nid_no_patch(descriptor, length);
    return result == -2 ? ::ftruncate(descriptor, static_cast<off_t>(length)) : result;
}
static int NativeTruncate(const std::filesystem::path& path, std::int64_t length) {
    return ::truncate(path.c_str(), static_cast<off_t>(length)) == 0 ? 0 : errno;
}
static int NativeUtimes(const std::filesystem::path& path, const KernelTimeval* times, bool followLink = true) {
    if (times == nullptr) return followLink ? ::utimes(path.c_str(), nullptr) : ::lutimes(path.c_str(), nullptr);
    struct timeval values[2]{{static_cast<time_t>(times[0].tv_sec), static_cast<suseconds_t>(times[0].tv_usec)},
        {static_cast<time_t>(times[1].tv_sec), static_cast<suseconds_t>(times[1].tv_usec)}};
    return followLink ? ::utimes(path.c_str(), values) : ::lutimes(path.c_str(), values);
}
static int NativeFutimes(int descriptor, const KernelTimeval* times) {
    if (times == nullptr) return ::futimes(descriptor, nullptr);
    struct timeval values[2]{{static_cast<time_t>(times[0].tv_sec), static_cast<suseconds_t>(times[0].tv_usec)},
        {static_cast<time_t>(times[1].tv_sec), static_cast<suseconds_t>(times[1].tv_usec)}};
    return ::futimes(descriptor, values);
}
static int NativeFlock(int descriptor, int operation) {
    return ::flock(descriptor, operation);
}
static std::int64_t NativePread(int descriptor, void* buf, std::size_t nbytes, std::int64_t offset) {
    return static_cast<std::int64_t>(::pread(descriptor, buf, nbytes, static_cast<off_t>(offset)));
}
static std::int64_t NativePwrite(int descriptor, const void* buf, std::size_t nbytes, std::int64_t offset) {
    const auto result = File::PwriteSharedMemory_nid_no_patch(descriptor, buf, nbytes, offset);
    return result == -2 ? static_cast<std::int64_t>(::pwrite(descriptor, buf, nbytes, static_cast<off_t>(offset))) : result;
}
static_assert(sizeof(KernelIovec) == sizeof(struct iovec));
static_assert(offsetof(KernelIovec, base) == offsetof(struct iovec, iov_base));
static_assert(offsetof(KernelIovec, length) == offsetof(struct iovec, iov_len));
static const struct iovec* NativeIovecs(const KernelIovec* iov) {
    return reinterpret_cast<const struct iovec*>(iov);
}
#endif

static constexpr int GUEST_EPERM = 1;
static constexpr int GUEST_ENOENT = 2;
static constexpr int GUEST_EIO = 5;
static constexpr int GUEST_EBADF = 9;
static constexpr int GUEST_EFAULT = 14;
static constexpr int GUEST_EEXIST = 17;
static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_ESPIPE = 29;
static constexpr int GUEST_ENAMETOOLONG = 63;
static constexpr int GUEST_ENOTDIR = 20;
static constexpr int GUEST_EISDIR = 21;
static constexpr int GUEST_EWOULDBLOCK = 35;
static constexpr int GUEST_EOPNOTSUPP = 45;
static constexpr int GUEST_ENOTEMPTY = 66;
static constexpr int GUEST_EDEADLK = 11;
static constexpr int GUEST_ELOOP = 62;
static constexpr int GUEST_EDQUOT = 69;
static constexpr int GUEST_EOVERFLOW = 84;

static int SceErrorFromErrno(int error);

static std::int64_t PositionedGuestSocketError(int descriptor) {
    return GuestSockets::IsOpen(descriptor) ? SceErrorFromErrno(GUEST_ESPIPE) : 0;
}

static bool FailPosixPositionedGuestSocket(int descriptor) {
    if (!GuestSockets::IsOpen(descriptor)) return false;
    errno = GUEST_ESPIPE;
    return true;
}

static bool IsSeekableDescriptor(int descriptor) {
    if (GuestSockets::IsOpen(descriptor)) return false;
    return File::IsVnode(descriptor) || File::IsCharacterDevice(descriptor) || File::IsRandomDevice(descriptor);
}

static int PositionedArgumentError(int descriptor, bool write, bool nullBuffer, std::int64_t offset) {
    if (IsKqueueDescriptor(descriptor)) return GUEST_ESPIPE;
    if (!nullBuffer && offset >= 0) return 0;
    if (!GuestSockets::IsOpen(descriptor) && !DescriptorIsOpen_nid_no_patch(descriptor)) return GUEST_EBADF;
    if (!IsSeekableDescriptor(descriptor)) return GUEST_ESPIPE;
    if (!File::GrantsAccess(descriptor, write)) return GUEST_EBADF;
    return offset < 0 ? GUEST_EINVAL : GUEST_EFAULT;
}

static int TransferArgumentError(int descriptor, bool nullBuffer) {
    if (!nullBuffer) return 0;
    return DescriptorIsOpen_nid_no_patch(descriptor) ? GUEST_EFAULT : GUEST_EBADF;
}

static int SceErrorFromErrno(int error) {
    int guest = error > 0 && error <= 34 ? error : error == GUEST_ENOTEMPTY ? error : GUEST_EIO;
    if (error == EAGAIN) guest = GUEST_EWOULDBLOCK;
#ifdef EDEADLK
    else if (error == EDEADLK) guest = GUEST_EDEADLK;
#endif
    else if (error == ENAMETOOLONG) guest = GUEST_ENAMETOOLONG;
    else if (error == ENOTEMPTY) guest = GUEST_ENOTEMPTY;
#ifdef ELOOP
    else if (error == ELOOP) guest = GUEST_ELOOP;
#endif
#ifdef EDQUOT
    else if (error == EDQUOT) guest = GUEST_EDQUOT;
#endif
#ifdef EOVERFLOW
    else if (error == EOVERFLOW) guest = GUEST_EOVERFLOW;
#endif
    return static_cast<int>(0x80020000u | static_cast<unsigned>(guest));
}

static int PathLookupFailure(const std::error_code& error) {
    const bool lookup = error == std::errc::filename_too_long || error == std::errc::too_many_symbolic_link_levels;
    return lookup ? SceErrorFromErrno(error.default_error_condition().value()) : 0;
}

extern "C" int* APS5_VABI __error_nid_postfix();

static int PosixFailure(int error) {
    *__error_nid_postfix() = error;
    return -1;
}

static bool ExceedsTransferLimit(std::uint64_t bytes) {
    return bytes > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
}

static int PosixResult(int result) {
    return result < 0 ? PosixFailure(result & 0xffff) : result;
}

static std::int64_t PosixResult(std::int64_t result) {
    return result < 0 ? PosixFailure(static_cast<int>(result) & 0xffff) : result;
}

static int DeviceFilesystem(KernelStatfs* fs) {
    fs->f_bsize = 512;
    fs->f_iosize = 512;
    fs->f_blocks = 2;
    fs->f_namemax = 255;
    return 0;
}

#ifdef _WIN32
static int NativeFstatfs(int descriptor, KernelStatfs* fs) {
    if (!File::IsDirectoryDescriptor(descriptor)) {
        const auto handle = reinterpret_cast<HANDLE>(::_get_osfhandle(descriptor));
        if (handle == INVALID_HANDLE_VALUE) return GUEST_EBADF;
        const DWORD type = ::GetFileType(handle);
        if (type == FILE_TYPE_PIPE) return GUEST_EINVAL;
        if (type == FILE_TYPE_CHAR) return DeviceFilesystem(fs);
        if (type != FILE_TYPE_DISK)
            throw std::runtime_error("_fstatfs: descriptor " + std::to_string(descriptor) + " is not a file on a Windows volume");
    }
    const auto path = File::WindowsDescriptorPath_nid_no_patch(descriptor);
    if (!path) throw std::runtime_error("_fstatfs: no path for descriptor " + std::to_string(descriptor) + ", errno=" + std::to_string(errno));
    std::wstring root(path->native().size() + 2, L'\0');
    if (!::GetVolumePathNameW(path->c_str(), root.data(), static_cast<DWORD>(root.size())))
        throw std::runtime_error("_fstatfs: GetVolumePathNameW failed for " + path->string() + ", error=" + std::to_string(::GetLastError()));
    DWORD sectorsPerCluster = 0, bytesPerSector = 0, freeClusters = 0, totalClusters = 0;
    ULARGE_INTEGER available{}, total{}, totalFree{};
    DWORD nameMax = 0, volumeFlags = 0;
    if (!::GetDiskFreeSpaceW(root.c_str(), &sectorsPerCluster, &bytesPerSector, &freeClusters, &totalClusters) ||
        !::GetDiskFreeSpaceExW(root.c_str(), &available, &total, &totalFree) ||
        !::GetVolumeInformationW(root.c_str(), nullptr, 0, nullptr, &nameMax, &volumeFlags, nullptr, 0))
        throw std::runtime_error("_fstatfs: volume query failed for " + path->string() + ", error=" + std::to_string(::GetLastError()));
    const std::uint64_t cluster = static_cast<std::uint64_t>(sectorsPerCluster) * bytesPerSector;
    if (cluster == 0) throw std::runtime_error("_fstatfs: volume of " + path->string() + " reports no cluster size");
    fs->f_flags = (volumeFlags & FILE_READ_ONLY_VOLUME) ? KERNEL_MNT_RDONLY : 0;
    fs->f_bsize = cluster;
    fs->f_iosize = cluster;
    fs->f_blocks = total.QuadPart / cluster;
    fs->f_bfree = totalFree.QuadPart / cluster;
    fs->f_bavail = static_cast<std::int64_t>(available.QuadPart / cluster);
    fs->f_namemax = nameMax;
    return 0;
}

static bool NativeIsPipe(int descriptor) {
    const auto handle = reinterpret_cast<HANDLE>(::_get_osfhandle(descriptor));
    return handle != INVALID_HANDLE_VALUE && ::GetFileType(handle) == FILE_TYPE_PIPE;
}
#else
#include <sys/statvfs.h>
#include <sys/vfs.h>
static bool NativeIsPipe(int descriptor) {
    constexpr long PipeFilesystem = 0x50495045;
    struct stat file{};
    struct statfs kind{};
    return ::fstat(descriptor, &file) == 0 && S_ISFIFO(file.st_mode) && ::fstatfs(descriptor, &kind) == 0
        && static_cast<long>(kind.f_type) == PipeFilesystem;
}

static int NativeFstatfs(int descriptor, KernelStatfs* fs) {
    struct stat file{};
    if (::fstat(descriptor, &file) != 0) return SceErrorFromErrno(errno) & 0xffff;
    if (S_ISSOCK(file.st_mode) && (::fcntl(descriptor, F_GETFL) & O_PATH) == 0) return GUEST_EINVAL;
    if (NativeIsPipe(descriptor)) return GUEST_EINVAL;
    if (S_ISCHR(file.st_mode)) return DeviceFilesystem(fs);
    struct statvfs host{};
    if (::fstatvfs(descriptor, &host) != 0) return SceErrorFromErrno(errno) & 0xffff;
    if (host.f_flag & ST_RDONLY) fs->f_flags |= KERNEL_MNT_RDONLY;
    if (host.f_flag & ST_NOSUID) fs->f_flags |= KERNEL_MNT_NOSUID;
    if (host.f_flag & ST_NOEXEC) fs->f_flags |= KERNEL_MNT_NOEXEC;
    fs->f_bsize = host.f_frsize != 0 ? host.f_frsize : host.f_bsize;
    fs->f_iosize = host.f_bsize;
    fs->f_blocks = host.f_blocks;
    fs->f_bfree = host.f_bfree;
    fs->f_bavail = static_cast<std::int64_t>(host.f_bavail);
    fs->f_files = host.f_files;
    fs->f_ffree = static_cast<std::int64_t>(host.f_favail);
    fs->f_namemax = static_cast<std::uint32_t>(host.f_namemax);
    return 0;
}
#endif

extern "C" int APS5_VABI pipe_nid_postfix(int* descriptors) {
    if (!descriptors) return PosixFailure(GUEST_EFAULT);
    const GuestArena::HostWrite destination(descriptors, 2 * sizeof(int));
    if (!destination.Open()) return PosixFailure(GUEST_EFAULT);
    int native[2];
#ifdef _WIN32
    const int result = ::_pipe(native, 65536, _O_BINARY);
#else
    const int result = ::pipe(native);
#endif
    if (result != 0) return PosixFailure(SceErrorFromErrno(errno) & 0xffff);
    File::RememberPipe(native[0]);
    File::RememberPipe(native[1]);
    std::memcpy(descriptors, native, sizeof(native));
    return 0;
}

static int PathError(const char* path) {
    if (path == nullptr) return GUEST_EFAULT;
    return *path == '\0' ? GUEST_ENOENT : 0;
}

#ifdef _WIN32
bool DescriptorIsOpen_nid_no_patch(int descriptor) {
    return descriptor >= 0 && WithoutParameterHandler([descriptor] { return ::_get_osfhandle(descriptor) == -1 ? -1 : 0; }) == 0;
}
#else
bool DescriptorIsOpen_nid_no_patch(int descriptor) {
    return descriptor >= 0 && ::fcntl(descriptor, F_GETFD) >= 0;
}
#endif

static std::filesystem::path WithoutTrailingSeparator(const std::filesystem::path& native) {
    return native.has_filename() ? native : native.parent_path();
}

static bool HasNonDirectoryPrefix(const std::filesystem::path& native) {
    std::error_code error;
    for (auto prefix = WithoutTrailingSeparator(native).parent_path(); prefix.has_relative_path(); prefix = prefix.parent_path()) {
        const auto status = std::filesystem::status(prefix, error);
        if (std::filesystem::exists(status)) return !std::filesystem::is_directory(status);
    }
    return false;
}

extern "C" int APS5_VABI dup_nid_postfix(int d);
extern "C" int APS5_VABI close_nid_postfix(int d);

int File::DuplicateFrom(int fd, int start) {
#ifdef _WIN32
    std::vector<int> below;
    int duplicate;
    while ((duplicate = dup_nid_postfix(fd)) >= 0 && duplicate < start) below.push_back(duplicate);
    const int error = *__error_nid_postfix();
    for (const int descriptor : below) close_nid_postfix(descriptor);
    return duplicate < 0 ? PosixFailure(error) : duplicate;
#else
    const int duplicate = ::fcntl(fd, F_DUPFD, start);
    if (duplicate < 0) return PosixFailure(SceErrorFromErrno(errno) & 0xffff);
    if (File::IsRandomDevice(fd)) File::RememberRandomDevice(duplicate);
    File::ShareDescriptor(fd, duplicate);
    return duplicate;
#endif
}

static int LookupError(const std::filesystem::path& native) {
    std::error_code error;
    const auto status = std::filesystem::status(native, error);
    if (std::filesystem::exists(status)) return native.has_filename() || std::filesystem::is_directory(status) ? 0 : GUEST_ENOTDIR;
    if (status.type() != std::filesystem::file_type::not_found) return SceErrorFromErrno(error.default_error_condition().value()) & 0xffff;
    const bool fileWithSeparator = !native.has_filename() && std::filesystem::exists(WithoutTrailingSeparator(native), error);
    return fileWithSeparator || HasNonDirectoryPrefix(native) ? GUEST_ENOTDIR : GUEST_ENOENT;
}

static constexpr std::pair<std::uint64_t, const char*> GUEST_FILE_FLAGS[] = {
    {0x00000001, "UF_NODUMP"}, {0x00000002, "UF_IMMUTABLE"}, {0x00000004, "UF_APPEND"}, {0x00000008, "UF_OPAQUE"},
    {0x00000010, "UF_NOUNLINK"}, {0x00000080, "UF_SYSTEM"}, {0x00000100, "UF_SPARSE"}, {0x00000200, "UF_OFFLINE"},
    {0x00000400, "UF_REPARSE"}, {0x00000800, "UF_ARCHIVE"}, {0x00001000, "UF_READONLY"}, {0x00008000, "UF_HIDDEN"},
    {0x00010000, "SF_ARCHIVED"}, {0x00020000, "SF_IMMUTABLE"}, {0x00040000, "SF_APPEND"}, {0x00100000, "SF_NOUNLINK"},
    {0x00200000, "SF_SNAPSHOT"}};
static constexpr std::uint64_t GUEST_SF_SNAPSHOT = 0x00200000;

static std::string FileFlagNames(std::uint64_t flags) {
    std::string names;
    for (const auto& [flag, name] : GUEST_FILE_FLAGS) {
        if ((flags & flag) == 0) continue;
        if (!names.empty()) names += '|';
        names += name;
    }
    return names;
}

static int SetFileFlags(const char* function, const std::string& target, std::uint64_t flags) {
    std::uint64_t known = 0;
    for (const auto& entry : GUEST_FILE_FLAGS) known |= entry.first;
    if ((flags & ~known) != 0) return GUEST_EOPNOTSUPP;
    if ((flags & GUEST_SF_SNAPSHOT) != 0) return GUEST_EPERM;
    if (flags != 0)
        throw std::runtime_error(std::string(function) + ": setting " + FileFlagNames(flags) + " on " + target + " is not supported, the host keeps no BSD file flags");
    return 0;
}

extern "C" {

int APS5_VABI chmod_nid_postfix(const char* path, int mode) {
    if (const int error = PathError(path)) return PosixFailure(error);
    std::filesystem::path native;
    if (const int error = ResolvePathError_nid_no_patch(path, native)) return PosixFailure(error);
    if (NativeChmod(native, mode & 07777) != 0) return PosixResult(SceErrorFromErrno(errno == ENOENT ? NotFoundError_nid_no_patch(native) : errno));
    return 0;
}

int APS5_VABI chflags_nid_postfix(const char* path, std::uint64_t flags) {
    if (const int error = PathError(path)) return PosixFailure(error);
    std::filesystem::path native;
    if (const int error = ResolvePathError_nid_no_patch(path, native)) return PosixFailure(error);
    if (const int error = LookupError(native)) return PosixFailure(error);
    if (const int error = SetFileFlags("chflags", path, flags)) return PosixFailure(error);
    return 0;
}

int APS5_VABI fchflags_nid_postfix(int d, std::uint64_t flags) {
    if (GuestSockets::IsOpen(d) || IsKqueueDescriptor(d) || File::IsSharedMemoryDescriptor_nid_no_patch(d)) return PosixFailure(GUEST_EINVAL);
    File::RefuseArchivedFile(d, "fchflags");
    if (!File::IsVnode(d)) {
        if (!DescriptorIsOpen_nid_no_patch(d)) return PosixFailure(GUEST_EBADF);
        if (NativeIsPipe(d)) return PosixFailure(GUEST_EINVAL);
        throw std::runtime_error("fchflags: descriptor " + std::to_string(d) + " is neither a file, a directory, a pipe, a socket nor shared memory");
    }
    if (const int error = SetFileFlags("fchflags", "descriptor " + std::to_string(d), flags)) return PosixFailure(error);
    return 0;
}

int APS5_VABI nmount_nid_postfix(const KernelIovec* iov, unsigned int niov, int flags) {
    (void)iov;
    (void)niov;
    (void)flags;
    NotImplemented_nid_no_patch("nmount: mounting file systems");
    return -1;
}

int APS5_VABI close_nid_postfix(int d) {
    if (File::IsHostStream(d)) return PosixFailure(GUEST_EPERM);
    int socketError = 0;
    if (KernelCloseSocket_nid_no_patch(d, &socketError)) {
        if (socketError == 0) return 0;
        *__error_nid_postfix() = socketError;
        return -1;
    }
    if (GuestSockets::IsOpen(d)) return GuestSockets::Close(d);
#ifdef _WIN32
    const auto positioned = File::LockPositionedTransfer(d);
#endif
    EqueueDescriptorClosed(d);
    File::ForgetRandomDevice(d);
    File::ForgetDescriptor(d);
    ForgetJitSharedMemory(d);
#ifdef _WIN32
    File::ForgetDirectoryDescriptor(d);
    File::ForgetFileLock(d);
    return WithoutParameterHandler([d] { return ::_close(d); });
#else
    return ::close(d);
#endif
}

int APS5_VABI _close_nid_postfix(int descriptor) {
    return close_nid_postfix(descriptor);
}

}

namespace {
std::atomic<int> g_hostStandardError{-1};
std::atomic<int> g_hostStandardOutput{-1};

void PreserveHostStream(int standard, std::atomic<int>& slot) {
    if (slot.load() >= 0) return;
    const int saved = standard == 1 ? KeepHostStandardOutput_nid_no_patch() : KeepHostStandardError_nid_no_patch();
    if (saved < 0) throw std::runtime_error(standard == 1 ? "dup2: cannot keep the host standard output" : "dup2: cannot keep the host standard error");
    slot.store(saved);
}

void PreserveHostStandardError() { PreserveHostStream(2, g_hostStandardError); }
void PreserveHostStandardOutput() { PreserveHostStream(1, g_hostStandardOutput); }
}

int File::HostStandardError() {
    return g_hostStandardError.load();
}

extern "C" {

int APS5_VABI dup_nid_postfix(int d) {
    if (GuestSockets::IsOpen(d)) return GuestSockets::Duplicate(d);
    if (d < 0) return PosixFailure(GUEST_EBADF);
    if (IsKqueueDescriptor(d)) throw std::runtime_error(std::string(__func__) + ": duplicating a kqueue descriptor is not supported");
#ifdef _WIN32
    const int duplicate = WithoutParameterHandler([d] { return ::_dup(d); });
    if (duplicate >= 0) File::DuplicateDirectoryDescriptor(d, duplicate);
#else
    const int duplicate = ::dup(d);
#endif
    if (duplicate < 0) return PosixFailure(SceErrorFromErrno(errno) & 0xffff);
    if (File::IsRandomDevice(d)) File::RememberRandomDevice(duplicate);
    File::ShareDescriptor(d, duplicate);
    DuplicateJitSharedMemory(d, duplicate);
    return duplicate;
}

int APS5_VABI dup2_nid_postfix(int from, int to) {
    if (from < 0 || to < 0) return PosixFailure(GUEST_EBADF);
    if (GuestSockets::IsOpen(from)) {
        if (from != to && to <= 2)
            throw std::runtime_error(std::string(__func__) + ": moving a guest socket onto standard descriptor " + std::to_string(to) + " is not supported");
        if (GuestSockets::DuplicateTo(from, to) < 0) return -1;
        if (from == to) return to;
        File::ForgetRandomDevice(to);
        File::ForgetArchivedFile(to);
#ifdef _WIN32
        File::ForgetDirectoryDescriptor(to);
        File::ForgetFileLock(to);
#endif
        return to;
    }
    const bool sourceIsRandom = File::IsRandomDevice(from);
    if (from != to && IsKqueueDescriptor(from)) throw std::runtime_error(std::string(__func__) + ": duplicating a kqueue descriptor is not supported");
#ifdef _WIN32
    if (WithoutParameterHandler([from] { return ::_get_osfhandle(from) == -1 ? -1 : 0; }) != 0) return PosixFailure(GUEST_EBADF);
    if (from == to) return to;
    if (to == 1) PreserveHostStandardOutput();
    if (to == 2) PreserveHostStandardError();
    auto* const standard = HostStandardHandle_nid_no_patch(to);
    const int duplicated = WithoutParameterHandler([from, to] { return ::_dup2(from, to); });
    KeepStandardHandle_nid_no_patch(to, standard);
    if (duplicated != 0) return PosixFailure(SceErrorFromErrno(errno) & 0xffff);
    EqueueDescriptorClosed(to);
    GuestSockets::Forget(to);
    File::ForgetDirectoryDescriptor(to);
    File::DuplicateDirectoryDescriptor(from, to);
    File::ForgetFileLock(to);
    File::ForgetRandomDevice(to);
    if (sourceIsRandom) File::RememberRandomDevice(to);
    File::ShareDescriptor(from, to);
    DuplicateJitSharedMemory(from, to);
    return to;
#else
    if (from == to) {
        if (::fcntl(from, F_GETFL) < 0) return PosixFailure(GUEST_EBADF);
        return to;
    }
    if (to == 1) PreserveHostStandardOutput();
    if (to == 2) PreserveHostStandardError();
    const int result = ::dup2(from, to);
    if (result < 0) return PosixFailure(SceErrorFromErrno(errno) & 0xffff);
    EqueueDescriptorClosed(to);
    GuestSockets::Forget(to);
    File::ForgetRandomDevice(to);
    if (sourceIsRandom) File::RememberRandomDevice(to);
    File::ShareDescriptor(from, to);
    DuplicateJitSharedMemory(from, to);
    return result;
#endif
}

int APS5_VABI flock_nid_postfix(int d, int operation) {
    if (GuestSockets::IsOpen(d)) return PosixFailure(GUEST_EOPNOTSUPP);
    File::RefuseArchivedFile(d, "flock");
#ifdef _WIN32
    const auto handle = reinterpret_cast<HANDLE>(WithoutParameterHandler([d] { return ::_get_osfhandle(d); }));
    if (handle == INVALID_HANDLE_VALUE) return PosixFailure(GUEST_EBADF);
    if (::GetFileType(handle) == FILE_TYPE_PIPE) return PosixFailure(GUEST_EOPNOTSUPP);
#else
    struct stat status;
    if (::fstat(d, &status) != 0) return PosixFailure(GUEST_EBADF);
    if (S_ISFIFO(status.st_mode)) return PosixFailure(GUEST_EOPNOTSUPP);
#endif
    const int type = operation & 8 ? 8 : operation & 2 ? 2 : operation & 1 ? 1 : 0;
    if (type == 0) return PosixFailure(GUEST_EBADF);
#ifdef _WIN32
    if (File::IsDirectoryDescriptor(d)) return 0;
#endif
    if (NativeFlock(d, type | (operation & 4)) != 0) {
#ifdef _WIN32
        const auto error = ::GetLastError();
        if ((operation & 4) && error == ERROR_LOCK_VIOLATION) return PosixFailure(GUEST_EWOULDBLOCK);
        throw std::runtime_error(std::string(__func__) + ": flock failed, fd=" + std::to_string(d) + ", error=" + std::to_string(error));
#else
        const int error = errno;
        if ((operation & 4) && error == EWOULDBLOCK) return PosixFailure(GUEST_EWOULDBLOCK);
        throw std::runtime_error(std::string(__func__) + ": flock failed, fd=" + std::to_string(d) + ", errno=" + std::to_string(error));
#endif
    }
    return 0;
}

int APS5_VABI _fstatfs_nid_postfix(int d, KernelStatfs* buf) {
    if (GuestSockets::IsOpen(d) || IsKqueueDescriptor(d)) return PosixFailure(GUEST_EINVAL);
    if (d < 0) return PosixFailure(GUEST_EBADF);
    File::RefuseArchivedFile(d, "fstatfs");
    KernelStatfs result{};
    result.f_version = KERNEL_STATFS_VERSION;
    if (const int error = NativeFstatfs(d, &result)) return PosixFailure(error);
    if (buf == nullptr) return PosixFailure(GUEST_EFAULT);
    const GuestArena::HostWrite destination(buf, sizeof(result));
    if (!destination.Open()) return PosixFailure(GUEST_EFAULT);
    std::memcpy(buf, &result, sizeof(result));
    return 0;
}

int64_t APS5_VABI fstat_nid_disambig1_nid_postfix(int d, FileStat* sb) {
    if (sb == nullptr) {
        FileStat ignored{};
        if (fstat_nid_disambig1_nid_postfix(d, &ignored) != 0) return -1;
        return PosixFailure(GUEST_EFAULT);
    }
    if (GuestSockets::IsOpen(d)) {
        if (const int error = GuestSockets::Stat(d, sb)) return PosixFailure(error);
        return 0;
    }
    if (!File::FillFileStatFromDescriptor(d, sb)) return PosixFailure(SceErrorFromErrno(errno) & 0xffff);
    return 0;
}

int64_t APS5_VABI _fstat_nid_postfix(int d, FileStat* sb) {
    return fstat_nid_disambig1_nid_postfix(d, sb);
}

int APS5_VABI ftruncate_nid_postfix(int d, int64_t length) {
    if (length < 0 || GuestSockets::IsOpen(d)) {
        return PosixFailure(GUEST_EINVAL);
    }
    File::RefuseArchivedFile(d, "ftruncate");
#ifdef _WIN32
    int error = NativeFtruncate(d, length);
    if (error != 0) {
        if (error == EACCES) error = GUEST_EINVAL;
        return PosixFailure(SceErrorFromErrno(error) & 0xffff);
    }
#else
    if (NativeFtruncate(d, length) != 0) {
        return PosixFailure(SceErrorFromErrno(errno) & 0xffff);
    }
#endif
    return 0;
}

int APS5_VABI sceKernelFtruncate(int d, int64_t length) {
    return ftruncate_nid_postfix(d, length) == 0 ? 0 : SceErrorFromErrno(*__error_nid_postfix());
}

int64_t APS5_VABI lseek_nid_postfix(int d, int64_t offset, int whence) {
    if (!GuestSockets::IsOpen(d) && !DescriptorIsOpen_nid_no_patch(d)) return PosixFailure(GUEST_EBADF);
    if (whence < 0 || whence > 2) return PosixFailure(GUEST_EINVAL);
    return PosixResult(sceKernelLseek(d, offset, whence));
}

int APS5_VABI mkdir_nid_postfix(const char* path, uint16_t mode) {
    if (const int error = PathError(path)) return PosixFailure(error);
    std::filesystem::path native;
    if (const int error = ResolvePathError_nid_no_patch(path, native)) return PosixFailure(error);
    if (NativeMkdir(native, mode) != 0) {
        const int nativeError = errno;
#ifdef _WIN32
        if (File::WindowsSymbolicLinkLoop(WithoutTrailingSeparator(native).parent_path())) return PosixFailure(GUEST_ELOOP);
#endif
        const bool blockedByFile = (nativeError == ENOENT || nativeError == EINVAL) && HasNonDirectoryPrefix(native);
        return PosixResult(SceErrorFromErrno(blockedByFile ? GUEST_ENOTDIR : nativeError));
    }
    RecordWrittenPath_nid_no_patch(native);
    return 0;
}

int APS5_VABI open_nid_postfix(const char* path, int flags, int mode) {
    if (File::InvalidOpenFlags(flags)) return PosixFailure(GUEST_EINVAL);
    if (const int error = PathError(path)) return PosixFailure(error);
    return PosixResult(sceKernelOpen(path, flags, static_cast<std::uint16_t>(mode)));
}

int APS5_VABI creat_nid_postfix(const char* path, int mode) {
    return open_nid_postfix(path, SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_TRUNC, mode);
}

int APS5_VABI _open_nid_postfix(const char* path, int flags, ...) {
    int mode = 0;
    if (flags & SCE_KERNEL_O_CREAT) {
#ifdef _WIN32
        __builtin_sysv_va_list arguments;
        __builtin_sysv_va_start(arguments, flags);
        mode = __builtin_va_arg(arguments, int);
        __builtin_sysv_va_end(arguments);
#else
        std::va_list arguments;
        va_start(arguments, flags);
        mode = va_arg(arguments, int);
        va_end(arguments);
#endif
    }
    return open_nid_postfix(path, flags, mode);
}

int64_t APS5_VABI pread_nid_postfix(int d, void* buf, size_t nbytes, int64_t offset) {
    if (ExceedsTransferLimit(nbytes)) return PosixFailure(GUEST_EINVAL);
    if (const int error = PositionedArgumentError(d, false, buf == nullptr && nbytes != 0, offset)) return PosixFailure(error);
    if (FailPosixPositionedGuestSocket(d)) return -1;
    const GuestArena::HostWrite destination(buf, nbytes);
    if (!destination.Open()) return PosixFailure(GUEST_EFAULT);
    if (File::ReadRandomDevice(d, buf, nbytes)) return static_cast<int64_t>(nbytes);
    if (const auto archived = File::ReadArchivedFile(d, buf, nbytes, &offset)) return *archived;
    char empty = 0;
    const auto result = NativePread(d, buf == nullptr ? &empty : buf, nbytes, offset);
    return result < 0 ? PosixFailure(SceErrorFromErrno(errno) & 0xffff) : result;
}

int64_t APS5_VABI pwrite_nid_disambig1_nid_postfix(int d, const void* buf, size_t nbytes, int64_t offset) {
    if (ExceedsTransferLimit(nbytes)) return PosixFailure(GUEST_EINVAL);
    if (const int error = PositionedArgumentError(d, true, buf == nullptr && nbytes != 0, offset)) return PosixFailure(error);
    if (FailPosixPositionedGuestSocket(d)) return -1;
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(buf, nbytes);
    char empty = 0;
    const auto result = NativePwrite(d, buf == nullptr ? &empty : buf, nbytes, offset);
    return result < 0 ? PosixFailure(SceErrorFromErrno(errno) & 0xffff) : result;
}

int64_t APS5_VABI read_nid_postfix(int d, void* buf, uint64_t nbytes) {
    if (ExceedsTransferLimit(nbytes)) return PosixFailure(GUEST_EINVAL);
    if (GuestSockets::IsOpen(d)) return GuestSockets::Read(d, buf, static_cast<size_t>(nbytes));
    if (const int error = TransferArgumentError(d, buf == nullptr && nbytes != 0)) return PosixFailure(error);
    char emptyBuffer = 0;
    return PosixResult(sceKernelRead(d, buf == nullptr ? &emptyBuffer : buf, static_cast<size_t>(nbytes)));
}

std::int64_t APS5_VABI _read_nid_postfix(int descriptor, void* buffer, std::size_t count) {
    if (ExceedsTransferLimit(count)) return PosixFailure(GUEST_EINVAL);
    if (GuestSockets::IsOpen(descriptor)) return GuestSockets::Read(descriptor, buffer, count);
    if (const int error = TransferArgumentError(descriptor, buffer == nullptr && count != 0)) return PosixFailure(error);
    char emptyBuffer = 0;
    return PosixResult(sceKernelRead(descriptor, buffer == nullptr ? &emptyBuffer : buffer, count));
}

int64_t APS5_VABI write_nid_postfix(int d, const char* str, int64_t size) {
    if (size < 0) return PosixFailure(GUEST_EINVAL);
    if (GuestSockets::IsOpen(d)) return GuestSockets::Write(d, str, static_cast<std::size_t>(size));
    if (const int error = TransferArgumentError(d, str == nullptr && size != 0)) return PosixFailure(error);
    const char emptyBuffer = 0;
    return PosixResult(sceKernelWrite(d, str == nullptr ? &emptyBuffer : str, static_cast<size_t>(size)));
}

std::int64_t APS5_VABI _write_nid_postfix(int descriptor, const void* buffer, std::size_t count) {
    if (ExceedsTransferLimit(count)) return PosixFailure(GUEST_EINVAL);
    if (GuestSockets::IsOpen(descriptor)) return GuestSockets::Write(descriptor, buffer, count);
    if (const int error = TransferArgumentError(descriptor, buffer == nullptr && count != 0)) return PosixFailure(error);
    const char emptyBuffer = 0;
    return PosixResult(sceKernelWrite(descriptor, buffer == nullptr ? &emptyBuffer : buffer, count));
}

int APS5_VABI stat_nid_postfix(const char* path, FileStat* sb) {
    if (sb == nullptr) return PosixFailure(GUEST_EFAULT);
    if (const int error = PathError(path)) return PosixFailure(error);
    return PosixResult(sceKernelStat(path, sb));
}

int APS5_VABI lstat_nid_postfix(const char* path, FileStat* sb) {
    if (sb == nullptr) return PosixFailure(GUEST_EFAULT);
    if (const int error = PathError(path)) return PosixFailure(error);
    std::filesystem::path native;
    if (const int error = ResolvePathError_nid_no_patch(path, native)) return PosixFailure(error);
    if (!File::FillLinkStat(native, sb)) return PosixResult(SceErrorFromErrno(errno == ENOENT ? NotFoundError_nid_no_patch(native) : errno));
    return 0;
}

int APS5_VABI unlink_nid_postfix(const char* path) {
    if (const int error = PathError(path)) return PosixFailure(error);
    return PosixResult(sceKernelUnlink(path));
}

int APS5_VABI sceKernelCheckReachability(const char* path) {
    if (path == nullptr) return SCE_KERNEL_ERROR_EINVAL;
    if (*path == '\0') return SCE_KERNEL_ERROR_ENOENT;
    if (std::strlen(path) > 255) return SCE_KERNEL_ERROR_ENAMETOOLONG;
    std::filesystem::path native;
    if (const int error = ResolvePathError_nid_no_patch(path, native, true, true)) return SceKernelError(error);
    if (AppArchiveOnly_nid_no_patch(native)) return 0;
    std::error_code error;
    return std::filesystem::exists(native, error) ? 0 : SCE_KERNEL_ERROR_ENOENT;
}

int APS5_VABI sceKernelFstat(int d, FileStat* sb) {
    if (sb == nullptr) {
        FileStat ignored{};
        if (const int error = sceKernelFstat(d, &ignored)) return error;
        return SceErrorFromErrno(GUEST_EFAULT);
    }
    if (GuestSockets::IsOpen(d)) {
        const int error = GuestSockets::Stat(d, sb);
        return error == 0 ? 0 : SceErrorFromErrno(error);
    }
    if (!File::FillFileStatFromDescriptor(d, sb)) return SceErrorFromErrno(errno);
    return 0;
}

int APS5_VABI sceKernelFsync(int fd) {
    if (GuestSockets::IsOpen(fd)) return SceErrorFromErrno(GUEST_EINVAL);
    File::RefuseArchivedFile(fd, "fsync");
#ifdef _WIN32
    if (NativeSync(fd) != 0) return SceErrorFromErrno(errno);
#else
    if (::fsync(fd) != 0) return SceErrorFromErrno(errno);
#endif
    return 0;
}

int APS5_VABI sceKernelFdatasync(int fd) {
    if (GuestSockets::IsOpen(fd)) return SceErrorFromErrno(GUEST_EINVAL);
    File::RefuseArchivedFile(fd, "fdatasync");
#ifdef _WIN32
    if (NativeSync(fd) != 0) return SceErrorFromErrno(errno);
#else
    if (::fdatasync(fd) != 0) return SceErrorFromErrno(errno);
#endif
    return 0;
}

int APS5_VABI sceKernelWriteThrottlingStatus(std::uint64_t* status) {
    if (status == nullptr) return SceErrorFromErrno(GUEST_EFAULT);
    status[0] = std::numeric_limits<std::uint32_t>::max();
    status[1] = 0;
    status[2] = 0;
    status[3] = 0;
    return 0;
}

#ifdef _WIN32

int APS5_VABI sceKernelGetdirentries(int fd, char* buf, int nbytes, int64_t* basep) {
    if (GuestSockets::IsOpen(fd)) return SceErrorFromErrno(GUEST_EINVAL);
    File::RefuseArchivedFile(fd, "getdirentries");
    if (buf == nullptr) return SceErrorFromErrno(GUEST_EFAULT);
    if (nbytes <= 0) return SceErrorFromErrno(GUEST_EINVAL);
    if (!File::DirectoryDescriptorPath(fd)) {
        if (WithoutParameterHandler([fd] { return ::_get_osfhandle(fd); }) == -1) return SceErrorFromErrno(GUEST_EBADF);
        return SceErrorFromErrno(GUEST_EINVAL);
    }
    const GuestArena::HostWrite destination(buf, static_cast<std::size_t>(nbytes));
    const GuestArena::HostWrite baseDestination(basep, basep == nullptr ? 0 : sizeof(*basep));
    if (!destination.Open() || !baseDestination.Open()) return SceErrorFromErrno(GUEST_EFAULT);
    return File::ReadDirectoryDescriptor(fd, buf, nbytes, basep);
}

int APS5_VABI sceKernelGetdents(int fd, char* buf, int nbytes) {
    return sceKernelGetdirentries(fd, buf, nbytes, nullptr);
}

#else

static bool InsideApp0(int fd) {
    const int savedErrno = errno;
    const bool inside = [fd] {
        const auto directory = File::DirectoryDescriptorPath(fd);
        if (!directory) return false;
        std::error_code error;
        const auto app0 = std::filesystem::weakly_canonical(ResolveDiskPath_nid_no_patch("/app0"), error);
        if (error || app0.empty()) return false;
        const auto relative = directory->lexically_relative(app0);
        return !relative.empty() && *relative.begin() != "..";
    }();
    errno = savedErrno;
    return inside;
}

static unsigned char GuestEntryType(int fd, const struct dirent64& entry, bool followLinks) {
    if (entry.d_type != DT_LNK && entry.d_type != DT_UNKNOWN) return entry.d_type;
    if (entry.d_type == DT_LNK && !followLinks) return DT_LNK;
    const int savedErrno = errno;
    struct stat target{};
    const bool found = ::fstatat(fd, entry.d_name, &target, followLinks ? 0 : AT_SYMLINK_NOFOLLOW) == 0;
    errno = savedErrno;
    if (!found) return entry.d_type;
    if (S_ISDIR(target.st_mode)) return DT_DIR;
    if (S_ISREG(target.st_mode)) return DT_REG;
    if (S_ISLNK(target.st_mode)) return DT_LNK;
    return entry.d_type;
}

int APS5_VABI sceKernelGetdirentries(int fd, char* buf, int nbytes, int64_t* basep) {
    if (GuestSockets::IsOpen(fd)) return SceErrorFromErrno(GUEST_EINVAL);
    File::RefuseArchivedFile(fd, "getdirentries");
    constexpr std::size_t GuestHeaderBytes = 8;
    constexpr std::size_t GuestMaxName = 255;
    if (buf == nullptr) return SceErrorFromErrno(GUEST_EFAULT);
    if (nbytes <= 0) return SceErrorFromErrno(GUEST_EINVAL);
    struct stat status{};
    if (::fstat(fd, &status) != 0) return SceErrorFromErrno(errno);
    if (!S_ISDIR(status.st_mode)) return SceErrorFromErrno(GUEST_EINVAL);
    const GuestArena::HostWrite destination(buf, static_cast<std::size_t>(nbytes));
    const GuestArena::HostWrite baseDestination(basep, basep == nullptr ? 0 : sizeof(*basep));
    if (!destination.Open() || !baseDestination.Open()) return SceErrorFromErrno(GUEST_EFAULT);
    const off_t base = ::lseek(fd, 0, SEEK_CUR);
    if (base < 0) return SceErrorFromErrno(errno);
    if (basep != nullptr) *basep = static_cast<int64_t>(base);
    std::vector<char> host(static_cast<std::size_t>(nbytes) < 4096 ? 4096 : static_cast<std::size_t>(nbytes));
    const auto read = ::syscall(SYS_getdents64, fd, host.data(), host.size());
    if (read < 0) return SceErrorFromErrno(errno);
    std::size_t written = 0;
    off_t resume = base;
    const bool followLinks = InsideApp0(fd);
    for (long offset = 0; offset < read;) {
        const auto* entry = reinterpret_cast<const struct dirent64*>(host.data() + offset);
        const auto nameLength = std::strlen(entry->d_name);
        const auto record = (GuestHeaderBytes + nameLength + 1 + 3) & ~std::size_t{3};
        if (nameLength > GuestMaxName || written + record > static_cast<std::size_t>(nbytes)) {
            if (::lseek(fd, resume, SEEK_SET) < 0) return SceErrorFromErrno(errno);
            if (written != 0) return static_cast<int>(written);
            return nameLength > GuestMaxName ? static_cast<int>(0x80020000u | GUEST_ENAMETOOLONG) : SceErrorFromErrno(GUEST_EINVAL);
        }
        char* out = buf + written;
        std::memset(out, 0, record);
        const auto fileNumber = static_cast<std::uint32_t>(entry->d_ino);
        const auto recordLength = static_cast<std::uint16_t>(record);
        const auto nameBytes = static_cast<std::uint8_t>(nameLength);
        std::memcpy(out, &fileNumber, sizeof(fileNumber));
        std::memcpy(out + 4, &recordLength, sizeof(recordLength));
        out[6] = static_cast<char>(GuestEntryType(fd, *entry, followLinks));
        out[7] = static_cast<char>(nameBytes);
        std::memcpy(out + GuestHeaderBytes, entry->d_name, nameLength);
        written += record;
        resume = entry->d_off;
        offset += entry->d_reclen;
    }
    if (::lseek(fd, resume, SEEK_SET) < 0) return SceErrorFromErrno(errno);
    return static_cast<int>(written);
}

int APS5_VABI sceKernelGetdents(int fd, char* buf, int nbytes) {
    return sceKernelGetdirentries(fd, buf, nbytes, nullptr);
}

#endif

int APS5_VABI getdirentries_nid_postfix(int fd, char* buf, int nbytes, int64_t* basep) {
    return PosixResult(sceKernelGetdirentries(fd, buf, nbytes, basep));
}

int APS5_VABI _getdirentries_nid_postfix(int fd, char* buf, int nbytes, int64_t* basep) {
    return getdirentries_nid_postfix(fd, buf, nbytes, basep);
}

int APS5_VABI getdents_nid_postfix(int fd, char* buf, int nbytes) {
    return PosixResult(sceKernelGetdents(fd, buf, nbytes));
}

int APS5_VABI sceKernelMkdir(const char* path, uint16_t mode) {
    (void)mode;
    if (!path) return SceErrorFromErrno(GUEST_EFAULT);
    if (!*path) return SceErrorFromErrno(GUEST_ENOENT);
    std::filesystem::path native;
    if (const int error = ResolvePathError_nid_no_patch(path, native)) return SceKernelError(error);
    while (!native.has_filename() && native.has_relative_path()) native = native.parent_path();
    std::error_code error;
    if (std::filesystem::exists(native, error)) return SceErrorFromErrno(GUEST_EEXIST);
    if (const int failure = PathLookupFailure(error)) return failure;
    if (!std::filesystem::is_directory(WithoutTrailingSeparator(native).parent_path(), error)) {
#ifdef _WIN32
        if (File::WindowsSymbolicLinkLoop(WithoutTrailingSeparator(native).parent_path())) return SceKernelError(GUEST_ELOOP);
#endif
        return SceErrorFromErrno(HasNonDirectoryPrefix(native) ? GUEST_ENOTDIR : GUEST_ENOENT);
    }
    if (!std::filesystem::create_directory(native, error)) return SceErrorFromErrno(error.value() ? error.value() : GUEST_EIO);
    RecordWrittenPath_nid_no_patch(native);
    return 0;
}

static int CopyIovecs(const KernelIovec* iov, int iovcnt, std::vector<KernelIovec>& vectors) {
    if (iovcnt < 0 || iovcnt > KERNEL_IOV_MAX) return SceErrorFromErrno(GUEST_EINVAL);
    if (iovcnt == 0) return 0;
    const auto bytes = static_cast<std::size_t>(iovcnt) * sizeof(KernelIovec);
    const auto address = reinterpret_cast<std::uintptr_t>(iov);
    if (address == 0 || bytes > UINTPTR_MAX - address) return SceErrorFromErrno(GUEST_EFAULT);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(iov, bytes);
    vectors.resize(static_cast<std::size_t>(iovcnt));
#ifdef _WIN32
    SIZE_T copied = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), iov, vectors.data(), bytes, &copied) || copied != bytes) return SceErrorFromErrno(GUEST_EFAULT);
#elif defined(__linux__)
    iovec local{vectors.data(), bytes};
    iovec remote{const_cast<KernelIovec*>(iov), bytes};
    const auto copied = ::process_vm_readv(::getpid(), &local, 1, &remote, 1, 0);
    if (copied < 0 && errno != EFAULT) throw std::runtime_error("iovec: process_vm_readv failed with errno " + std::to_string(errno));
    if (copied < 0 || static_cast<std::size_t>(copied) != bytes) return SceErrorFromErrno(GUEST_EFAULT);
#elif defined(__APPLE__)
    mach_vm_size_t copied = 0;
    if (::mach_vm_read_overwrite(::mach_task_self(), reinterpret_cast<mach_vm_address_t>(iov), bytes,
            reinterpret_cast<mach_vm_address_t>(vectors.data()), &copied) != KERN_SUCCESS || copied != bytes) return SceErrorFromErrno(GUEST_EFAULT);
#else
    throw std::runtime_error("iovec: checked guest copies are not implemented on this host");
#endif
    std::size_t total = 0;
    for (const auto& vector : vectors) {
        if (vector.length > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()) - total) return SceErrorFromErrno(GUEST_EINVAL);
        total += vector.length;
    }
    return 0;
}

static std::int64_t ReadRandomIovecs(int d, const KernelIovec* iov, int iovcnt) {
    std::int64_t total = 0;
    for (int i = 0; i < iovcnt; ++i) {
        File::ReadRandomDevice(d, iov[i].base, iov[i].length);
        total += static_cast<std::int64_t>(iov[i].length);
    }
    return total;
}

static bool OpenIovecs(const KernelIovec* iov, int iovcnt, std::deque<GuestArena::HostWrite>& destinations) {
    for (int i = 0; i < iovcnt; ++i) {
        if (!destinations.emplace_back(iov[i].base, iov[i].length).Open()) return false;
    }
    return true;
}

static void PrepareWriteIovecs(const KernelIovec* iov, int iovcnt) {
    for (int i = 0; i < iovcnt; ++i) GuestArena::GuestArenaPrepareHostRead_nid_no_patch(iov[i].base, iov[i].length);
}

#ifdef _WIN32

int64_t APS5_VABI sceKernelPread(int d, void* buf, size_t nbytes, int64_t offset) {
    if (const int error = PositionedArgumentError(d, false, buf == nullptr && nbytes != 0, offset)) return SceErrorFromErrno(error);
    if (const auto error = PositionedGuestSocketError(d); error != 0) return error;
    const GuestArena::HostWrite destination(buf, nbytes);
    if (!destination.Open()) return SceErrorFromErrno(GUEST_EFAULT);
    if (File::ReadRandomDevice(d, buf, nbytes)) return static_cast<int64_t>(nbytes);
    if (const auto archived = File::ReadArchivedFile(d, buf, nbytes, &offset)) return *archived;
    char empty = 0;
    const auto result = NativePread(d, nbytes == 0 && buf == nullptr ? &empty : buf, nbytes, offset);
    return result < 0 ? SceErrorFromErrno(errno) : result;
}

int64_t APS5_VABI sceKernelPwrite(int d, const void* buf, size_t nbytes, int64_t offset) {
    if (const int error = PositionedArgumentError(d, true, buf == nullptr && nbytes != 0, offset)) return SceErrorFromErrno(error);
    if (const auto error = PositionedGuestSocketError(d); error != 0) return error;
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(buf, nbytes);
    char empty = 0;
    const auto result = NativePwrite(d, nbytes == 0 && buf == nullptr ? &empty : buf, nbytes, offset);
    return result < 0 ? SceErrorFromErrno(errno) : result;
}

static std::int64_t TransferIovecs(int d, const KernelIovec* guestIov, int iovcnt, const std::int64_t* offset, bool write) {
    std::vector<KernelIovec> vectors;
    if (const int error = CopyIovecs(guestIov, iovcnt, vectors)) return error;
    const auto* iov = vectors.data();
    if (offset != nullptr) {
        if (const int error = PositionedArgumentError(d, write, false, *offset)) return SceErrorFromErrno(error);
    }
    std::size_t total = 0;
    for (int i = 0; i < iovcnt; ++i) {
        if (iov[i].base == nullptr && iov[i].length != 0) return SceErrorFromErrno(GUEST_EFAULT);
        total += iov[i].length;
    }
    if (offset != nullptr && total > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max() - *offset)) return SceErrorFromErrno(GUEST_EINVAL);
    if (offset != nullptr) {
        if (const auto error = PositionedGuestSocketError(d); error != 0) return error;
    }
    std::deque<GuestArena::HostWrite> destinations;
    if (!write && !OpenIovecs(iov, iovcnt, destinations)) return SceErrorFromErrno(GUEST_EFAULT);
    if (offset == nullptr && GuestSockets::IsOpen(d)) {
        const auto result = write ? GuestSockets::Writev(d, reinterpret_cast<const GuestSockets::Iovec*>(iov), iovcnt)
                                  : GuestSockets::Readv(d, reinterpret_cast<const GuestSockets::Iovec*>(iov), iovcnt);
        return result < 0 ? SceKernelError(*__error_nid_postfix()) : result;
    }
    if (write) PrepareWriteIovecs(iov, iovcnt);
    if (!write && File::IsRandomDevice(d)) return ReadRandomIovecs(d, iov, iovcnt);
    if (!write) {
        if (const auto archived = File::ReadArchivedIovecs(d, iov, iovcnt, offset)) return *archived;
    }
    const auto positioned = File::LockPositionedTransfer(d);
    if (total == 0) {
        char none = 0;
        const auto result = offset != nullptr ? NativePositionedTransfer(d, &none, 0, *offset, write) : NativeTransferBuffer(d, &none, 0, write);
        return result < 0 ? SceErrorFromErrno(errno) : 0;
    }
    if (!write && offset == nullptr && File::IsDirectoryDescriptor(d)) return SceErrorFromErrno(GUEST_EISDIR);
    if (write && iovcnt > 1 && total <= static_cast<std::size_t>(std::numeric_limits<int>::max()) && File::Appends(d)) {
        std::vector<char> gathered;
        gathered.reserve(total);
        for (int i = 0; i < iovcnt; ++i) {
            const auto* base = static_cast<const char*>(iov[i].base);
            gathered.insert(gathered.end(), base, base + iov[i].length);
        }
        const auto result = offset != nullptr ? NativePositionedTransfer(d, gathered.data(), gathered.size(), *offset, true)
                                              : NativeTransferBuffer(d, gathered.data(), gathered.size(), true);
        return result < 0 ? SceErrorFromErrno(errno) : result;
    }
    const bool whole = write || offset != nullptr || NativeIsDisk(d);
    std::int64_t done = 0;
    for (int i = 0; i < iovcnt; ++i) {
        auto* base = static_cast<char*>(iov[i].base);
        for (std::size_t position = 0; position < iov[i].length;) {
            const auto chunk = std::min<std::size_t>(iov[i].length - position, std::numeric_limits<int>::max());
            const auto result = offset != nullptr ? NativePositionedTransfer(d, base + position, chunk, *offset + done, write) : NativeTransferBuffer(d, base + position, chunk, write);
            if (result < 0) return done != 0 ? done : SceErrorFromErrno(errno);
            done += result;
            position += static_cast<std::size_t>(result);
            if (static_cast<std::size_t>(result) < chunk || (!whole && !NativePipeHasData(d))) return done;
        }
    }
    return done;
}

int64_t APS5_VABI sceKernelReadv(int d, const KernelIovec* iov, int iovcnt) {
    return TransferIovecs(d, iov, iovcnt, nullptr, false);
}

int64_t APS5_VABI sceKernelWritev(int d, const KernelIovec* iov, int iovcnt) {
    return TransferIovecs(d, iov, iovcnt, nullptr, true);
}

int64_t APS5_VABI sceKernelPreadv(int d, const KernelIovec* iov, int iovcnt, int64_t offset) {
    return TransferIovecs(d, iov, iovcnt, &offset, false);
}

int64_t APS5_VABI sceKernelPwritev(int d, const KernelIovec* iov, int iovcnt, int64_t offset) {
    return TransferIovecs(d, iov, iovcnt, &offset, true);
}

#else

int64_t APS5_VABI sceKernelPread(int d, void* buf, size_t nbytes, int64_t offset) {
    if (const int error = PositionedArgumentError(d, false, buf == nullptr && nbytes != 0, offset)) return SceErrorFromErrno(error);
    if (const auto error = PositionedGuestSocketError(d); error != 0) return error;
    const GuestArena::HostWrite destination(buf, nbytes);
    if (!destination.Open()) return SceErrorFromErrno(GUEST_EFAULT);
    if (File::ReadRandomDevice(d, buf, nbytes)) return static_cast<int64_t>(nbytes);
    if (const auto archived = File::ReadArchivedFile(d, buf, nbytes, &offset)) return *archived;
    const auto result = NativePread(d, buf, nbytes, offset);
    return result < 0 ? SceErrorFromErrno(errno) : result;
}

int64_t APS5_VABI sceKernelPwrite(int d, const void* buf, size_t nbytes, int64_t offset) {
    if (const int error = PositionedArgumentError(d, true, buf == nullptr && nbytes != 0, offset)) return SceErrorFromErrno(error);
    if (const auto error = PositionedGuestSocketError(d); error != 0) return error;
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(buf, nbytes);
    const auto result = NativePwrite(d, buf, nbytes, offset);
    return result < 0 ? SceErrorFromErrno(errno) : result;
}

int64_t APS5_VABI sceKernelReadv(int d, const KernelIovec* guestIov, int iovcnt) {
    std::vector<KernelIovec> vectors;
    if (const int error = CopyIovecs(guestIov, iovcnt, vectors)) return error;
    const auto* iov = vectors.data();
    std::deque<GuestArena::HostWrite> destinations;
    if (!OpenIovecs(iov, iovcnt, destinations)) return SceErrorFromErrno(GUEST_EFAULT);
    if (GuestSockets::IsOpen(d)) {
        const auto result = GuestSockets::Readv(d, reinterpret_cast<const GuestSockets::Iovec*>(iov), iovcnt);
        return result < 0 ? SceKernelError(*__error_nid_postfix()) : result;
    }
    if (File::IsRandomDevice(d)) return ReadRandomIovecs(d, iov, iovcnt);
    if (const auto archived = File::ReadArchivedIovecs(d, iov, iovcnt, nullptr)) return *archived;
    const auto result = static_cast<std::int64_t>(::readv(d, NativeIovecs(iov), iovcnt));
    return result < 0 ? SceErrorFromErrno(errno) : result;
}

int64_t APS5_VABI sceKernelWritev(int d, const KernelIovec* guestIov, int iovcnt) {
    std::vector<KernelIovec> vectors;
    if (const int error = CopyIovecs(guestIov, iovcnt, vectors)) return error;
    const auto* iov = vectors.data();
    if (GuestSockets::IsOpen(d)) {
        const auto result = GuestSockets::Writev(d, reinterpret_cast<const GuestSockets::Iovec*>(iov), iovcnt);
        return result < 0 ? SceKernelError(*__error_nid_postfix()) : result;
    }
    PrepareWriteIovecs(iov, iovcnt);
    const auto sharedResult = File::WritevSharedMemory_nid_no_patch(d, NativeIovecs(iov), iovcnt);
    if (sharedResult != -2) return sharedResult < 0 ? SceErrorFromErrno(errno) : sharedResult;
    const auto result = static_cast<std::int64_t>(::writev(d, NativeIovecs(iov), iovcnt));
    return result < 0 ? SceErrorFromErrno(errno) : result;
}

int64_t APS5_VABI sceKernelPreadv(int d, const KernelIovec* guestIov, int iovcnt, int64_t offset) {
    std::vector<KernelIovec> vectors;
    if (const int error = CopyIovecs(guestIov, iovcnt, vectors)) return error;
    const auto* iov = vectors.data();
    if (const int error = PositionedArgumentError(d, false, false, offset)) return SceErrorFromErrno(error);
    if (const auto error = PositionedGuestSocketError(d); error != 0) return error;
    std::deque<GuestArena::HostWrite> destinations;
    if (!OpenIovecs(iov, iovcnt, destinations)) return SceErrorFromErrno(GUEST_EFAULT);
    if (File::IsRandomDevice(d)) return ReadRandomIovecs(d, iov, iovcnt);
    if (const auto archived = File::ReadArchivedIovecs(d, iov, iovcnt, &offset)) return *archived;
    const auto result = static_cast<std::int64_t>(::preadv(d, NativeIovecs(iov), iovcnt, static_cast<off_t>(offset)));
    return result < 0 ? SceErrorFromErrno(errno) : result;
}

int64_t APS5_VABI sceKernelPwritev(int d, const KernelIovec* guestIov, int iovcnt, int64_t offset) {
    std::vector<KernelIovec> vectors;
    if (const int error = CopyIovecs(guestIov, iovcnt, vectors)) return error;
    const auto* iov = vectors.data();
    if (const int error = PositionedArgumentError(d, true, false, offset)) return SceErrorFromErrno(error);
    if (const auto error = PositionedGuestSocketError(d); error != 0) return error;
    PrepareWriteIovecs(iov, iovcnt);
    const auto sharedResult = File::PwritevSharedMemory_nid_no_patch(d, NativeIovecs(iov), iovcnt, offset);
    if (sharedResult != -2) return sharedResult < 0 ? SceErrorFromErrno(errno) : sharedResult;
    const auto result = static_cast<std::int64_t>(::pwritev(d, NativeIovecs(iov), iovcnt, static_cast<off_t>(offset)));
    return result < 0 ? SceErrorFromErrno(errno) : result;
}

#endif

int64_t APS5_VABI preadv_nid_postfix(int d, const KernelIovec* iov, int iovcnt, int64_t offset) {
    const auto result = sceKernelPreadv(d, iov, iovcnt, offset);
    return result < 0 ? PosixFailure(static_cast<int>(result) & 0xffff) : result;
}

int64_t APS5_VABI pwritev_nid_postfix(int d, const KernelIovec* iov, int iovcnt, int64_t offset) {
    const auto result = sceKernelPwritev(d, iov, iovcnt, offset);
    return result < 0 ? PosixFailure(static_cast<int>(result) & 0xffff) : result;
}

int APS5_VABI sceKernelRename(const char* from, const char* to) {
    if (!from) return SceErrorFromErrno(GUEST_EFAULT);
    const int result = RenameGuestPath_nid_no_patch(from, to);
    return result == 0 ? 0 : SceKernelError(errno);
    if (const int dot = RenameDotError_nid_no_patch(from)) return SceKernelError(dot);
    if (const int dot = RenameDotError_nid_no_patch(to)) return SceKernelError(dot);
}

int APS5_VABI sceKernelRmdir(const char* path) {
    if (!path) return SceErrorFromErrno(GUEST_EFAULT);
    if (*path == '\0') return SceErrorFromErrno(GUEST_ENOENT);
    if (const int dot = RenameDotError_nid_no_patch(path)) return SceKernelError(dot);
    std::filesystem::path native;
    if (const int error = ResolvePathError_nid_no_patch(path, native)) return SceKernelError(error);
    std::error_code error;
    if (std::filesystem::is_symlink(native, error)) return SceErrorFromErrno(GUEST_ENOTDIR);
    if (const int failure = PathLookupFailure(error)) return failure;
    if (!std::filesystem::is_directory(native, error)) {
#ifdef _WIN32
        if (File::WindowsSymbolicLinkLoop(WithoutTrailingSeparator(native).parent_path())) return SceKernelError(GUEST_ELOOP);
#endif
        const bool blockedByFile = HasNonDirectoryPrefix(native) || std::filesystem::exists(WithoutTrailingSeparator(native), error);
        return SceErrorFromErrno(blockedByFile ? GUEST_ENOTDIR : GUEST_ENOENT);
    }
    if (!std::filesystem::is_empty(native, error)) return SceErrorFromErrno(GUEST_ENOTEMPTY);
    if (!RemoveEmptyDirectory(native)) return SceErrorFromErrno(GUEST_EIO);
    RecordWrittenPath_nid_no_patch(native);
    return 0;
}

int APS5_VABI rmdir_nid_postfix(const char* path) {
    if (const int error = PathError(path)) return PosixFailure(error);
    return PosixResult(sceKernelRmdir(path));
}

}

extern "C" {

int APS5_VABI sceKernelChmod_nid_postfix(const char* path, std::uint16_t mode) {
    if (!path) return SceErrorFromErrno(GUEST_EFAULT);
    if (!*path) return SceErrorFromErrno(GUEST_ENOENT);
    std::filesystem::path native;
    if (const int error = ResolvePathError_nid_no_patch(path, native)) return SceKernelError(error);
    if (NativeChmod(native, mode & 07777) != 0) return SceErrorFromErrno(errno);
    return 0;
}

int APS5_VABI sceKernelFchmod(int d, std::uint16_t mode) {
    if (GuestSockets::IsOpen(d)) return SceErrorFromErrno(GUEST_EINVAL);
    File::RefuseArchivedFile(d, "fchmod");
    if (NativeFchmod(d, mode & 07777) != 0) return SceErrorFromErrno(errno);
    return 0;
}

int APS5_VABI fchmod_nid_postfix(int d, int mode) {
    return PosixResult(sceKernelFchmod(d, static_cast<std::uint16_t>(mode)));
}

static bool InvalidTimes(const KernelTimeval* times) {
    return times != nullptr && (times[0].tv_usec < 0 || times[0].tv_usec >= 1000000 || times[1].tv_usec < 0 || times[1].tv_usec >= 1000000);
}

int APS5_VABI sceKernelTruncate_nid_postfix(const char* path, std::int64_t length) {
    if (length < 0) return SceErrorFromErrno(GUEST_EINVAL);
    if (!path) return SceErrorFromErrno(GUEST_EFAULT);
    if (*path == '\0') return SceErrorFromErrno(GUEST_ENOENT);
    std::filesystem::path native;
    if (const int error = ResolvePathError_nid_no_patch(path, native)) return SceKernelError(error);
    std::error_code error;
    const auto status = std::filesystem::status(native, error);
    if (const int failure = PathLookupFailure(error)) return failure;
    if (!std::filesystem::exists(status)) return SceErrorFromErrno(NotFoundError_nid_no_patch(native));
    if (std::filesystem::is_directory(status)) return SceErrorFromErrno(GUEST_EISDIR);
    if (const int failure = NativeTruncate(native, length)) {
#ifdef _WIN32
        if (File::WindowsSymbolicLinkLoop(native)) return SceKernelError(GUEST_ELOOP);
#endif
        return SceErrorFromErrno(failure);
    }
    RecordWrittenPath_nid_no_patch(native);
    return 0;
}

int APS5_VABI truncate_nid_postfix(const char* path, std::int64_t length) {
    if (length < 0) return PosixFailure(GUEST_EINVAL);
    if (const int error = PathError(path)) return PosixFailure(error);
    return PosixResult(sceKernelTruncate_nid_postfix(path, length));
}

int APS5_VABI sceKernelUtimes_nid_postfix(const char* path, const KernelTimeval* times) {
    if (InvalidTimes(times)) return SCE_KERNEL_ERROR_EINVAL;
    if (!path) return SceErrorFromErrno(GUEST_EFAULT);
    if (*path == '\0') return SceErrorFromErrno(GUEST_ENOENT);
    std::filesystem::path native;
    if (const int error = ResolvePathError_nid_no_patch(path, native)) return SceKernelError(error);
    if (NativeUtimes(native, times) != 0) return SceErrorFromErrno(errno == ENOENT ? NotFoundError_nid_no_patch(native) : errno);
    return 0;
}

int APS5_VABI utimes_nid_postfix(const char* path, const KernelTimeval* times) {
    if (InvalidTimes(times)) return PosixFailure(GUEST_EINVAL);
    if (const int error = PathError(path)) return PosixFailure(error);
    return PosixResult(sceKernelUtimes_nid_postfix(path, times));
}

int APS5_VABI lutimes_nid_postfix(const char* path, const KernelTimeval* times) {
    if (InvalidTimes(times)) return PosixFailure(GUEST_EINVAL);
    if (const int error = PathError(path)) return PosixFailure(error);
    std::filesystem::path native;
    if (const int error = ResolvePathError_nid_no_patch(path, native)) return PosixFailure(error);
    if (NativeUtimes(native, times, false) != 0) return PosixResult(SceErrorFromErrno(errno));
    return 0;
}

int APS5_VABI sceKernelFutimes(int d, const KernelTimeval* times) {
    if (GuestSockets::IsOpen(d)) return SceErrorFromErrno(GUEST_EINVAL);
    File::RefuseArchivedFile(d, "futimes");
    if (times != nullptr) {
        for (int i = 0; i < 2; ++i) {
            if (times[i].tv_usec < 0 || times[i].tv_usec >= 1000000) return SceErrorFromErrno(GUEST_EINVAL);
        }
    }
    if (NativeFutimes(d, times) != 0) return SceErrorFromErrno(errno);
    return 0;
}

int APS5_VABI futimes_nid_postfix(int d, const KernelTimeval* times) {
    return PosixResult(sceKernelFutimes(d, times));
}

int APS5_VABI fsync_nid_postfix(int fd) {
    return PosixResult(sceKernelFsync(fd));
}

int APS5_VABI fdatasync_nid_postfix(int fd) {
    return PosixResult(sceKernelFdatasync(fd));
}

int64_t APS5_VABI readv_nid_postfix(int d, const KernelIovec* iov, int iovcnt) {
    const auto result = sceKernelReadv(d, iov, iovcnt);
    return result < 0 ? PosixFailure(static_cast<int>(result) & 0xffff) : result;
}

int64_t APS5_VABI writev_nid_postfix(int d, const KernelIovec* iov, int iovcnt) {
    const auto result = sceKernelWritev(d, iov, iovcnt);
    return result < 0 ? PosixFailure(static_cast<int>(result) & 0xffff) : result;
}

int64_t APS5_VABI _readv_nid_postfix(int d, const KernelIovec* iov, int iovcnt) {
    return readv_nid_postfix(d, iov, iovcnt);
}

int64_t APS5_VABI _writev_nid_postfix(int d, const KernelIovec* iov, int iovcnt) {
    return writev_nid_postfix(d, iov, iovcnt);
}

}
