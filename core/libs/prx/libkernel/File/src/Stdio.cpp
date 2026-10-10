#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <limits>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/File/include/NativeStat.hpp"
#include "prx/libkernel/File/include/DirectoryDescriptor.hpp"
#include "prx/libkernel/File/include/FileLock.hpp"
#include "prx/libkernel/File/include/RandomDevice.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Socket/include/SocketRuntime.hpp"
#include <cerrno>
#include <cstring>
#include <cstdarg>
#include <deque>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

struct KernelIovec {
    void* base;
    std::size_t length;
};

static constexpr int KERNEL_IOV_MAX = 1024;

struct KernelStatfs {
    std::uint32_t f_version;
    std::uint32_t f_type;
    std::uint64_t f_flags;
    std::uint64_t f_bsize;
    std::uint64_t f_iosize;
    std::uint64_t f_blocks;
    std::uint64_t f_bfree;
    std::int64_t f_bavail;
    std::uint64_t f_files;
    std::int64_t f_ffree;
    std::uint64_t f_syncwrites;
    std::uint64_t f_asyncwrites;
    std::uint64_t f_syncreads;
    std::uint64_t f_asyncreads;
    std::uint64_t f_spare[10];
    std::uint32_t f_namemax;
    std::uint32_t f_owner;
    std::int32_t f_fsid[2];
    char f_charspare[80];
    char f_fstypename[16];
    char f_mntfromname[88];
    char f_mntonname[88];
};
static_assert(sizeof(KernelStatfs) == 472);
static_assert(offsetof(KernelStatfs, f_bavail) == 48);
static_assert(offsetof(KernelStatfs, f_namemax) == 184);
static_assert(offsetof(KernelStatfs, f_fstypename) == 280);
static_assert(offsetof(KernelStatfs, f_mntonname) == 384);

static constexpr std::uint32_t KERNEL_STATFS_VERSION = 0x20030518;
static constexpr std::uint64_t KERNEL_MNT_RDONLY = 0x1;
static constexpr std::uint64_t KERNEL_MNT_NOEXEC = 0x4;
static constexpr std::uint64_t KERNEL_MNT_NOSUID = 0x8;

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <direct.h>
#include <sys/stat.h>
#include "prx/libkernel/File/include/WindowsFileTime.hpp"
static int NativeRmdir(const std::filesystem::path& path) {
    return ::_wrmdir(path.wstring().c_str());
}
static int NativeMkdir(const std::filesystem::path& path, std::uint16_t mode) {
    (void)mode;
    return ::_wmkdir(path.wstring().c_str());
}
static int NativeChmod(const std::filesystem::path& path, int mode) {
    RecordWrittenPath_nid_no_patch(path);
    return ::_wchmod(path.wstring().c_str(), mode);
}
static std::optional<std::filesystem::path> NativeDescriptorPath(int descriptor) {
    if (auto directory = File::DirectoryDescriptorPath(descriptor)) return directory;
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
    if (length == 0 || length >= path.size()) {
        errno = EINVAL;
        return std::nullopt;
    }
    path.resize(length);
    return path;
}
static int NativeFchmod(int descriptor, int mode) {
    const auto path = NativeDescriptorPath(descriptor);
    return path ? NativeChmod(*path, mode) : -1;
}
static int NativeFtruncate(int descriptor, std::int64_t length) {
    return static_cast<int>(::_chsize_s(descriptor, length));
}
static int SetTimes(HANDLE handle, const KernelTimeval* times) {
    FILETIME access{}, modified{};
    if (times == nullptr) {
        GetSystemTimePreciseAsFileTime(&access);
        modified = access;
    } else if (!File::WindowsFileTime::Encode(times[0], access) || !File::WindowsFileTime::Encode(times[1], modified)) {
        errno = EINVAL;
        return -1;
    }
    return SetFileTime(handle, nullptr, &access, &modified) ? 0 : File::WindowsFileTime::Failure(GetLastError());
}
static int NativeUtimes(const std::filesystem::path& path, const KernelTimeval* times) {
    const auto handle = CreateFileW(path.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
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
    const auto handle = ReOpenFile(original, FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0);
    if (handle == INVALID_HANDLE_VALUE) return File::WindowsFileTime::Failure(GetLastError());
    const int result = SetTimes(handle, times);
    CloseHandle(handle);
    if (result == 0) {
        if (const auto path = NativeDescriptorPath(descriptor)) RecordWrittenPath_nid_no_patch(*path);
    }
    return result;
}
static int NativeFlock(int descriptor, int operation) {
    return File::Flock(descriptor, operation);
}
std::int64_t NativePositioned_nid_no_patch(int descriptor, void* buf, std::size_t nbytes, std::int64_t offset, bool write) {
    if (nbytes > static_cast<std::size_t>(std::numeric_limits<DWORD>::max())) {
        throw std::runtime_error("NativePositioned: nbytes exceeds platform limit");
    }
    const auto handle = reinterpret_cast<HANDLE>(::_get_osfhandle(descriptor));
    if (handle == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        return -1;
    }
    LARGE_INTEGER position{};
    if (::GetFileType(handle) != FILE_TYPE_DISK || !::SetFilePointerEx(handle, LARGE_INTEGER{}, &position, FILE_CURRENT)) {
        errno = ESPIPE;
        return -1;
    }
    OVERLAPPED overlapped{};
    overlapped.Offset = static_cast<DWORD>(offset);
    overlapped.OffsetHigh = static_cast<DWORD>(static_cast<std::uint64_t>(offset) >> 32u);
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
static std::int64_t NativeTransfer(int descriptor, void* buf, std::size_t nbytes, bool write) {
    if (nbytes > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("NativeTransfer: nbytes exceeds platform limit");
    }
    return write ? ::_write(descriptor, buf, static_cast<unsigned int>(nbytes)) : ::_read(descriptor, buf, static_cast<unsigned int>(nbytes));
}
static bool NativeIsDisk(int descriptor) {
    const auto handle = reinterpret_cast<HANDLE>(::_get_osfhandle(descriptor));
    return handle != INVALID_HANDLE_VALUE && ::GetFileType(handle) == FILE_TYPE_DISK;
}
static std::int64_t NativePread(int descriptor, void* buf, std::size_t nbytes, std::int64_t offset) {
    return NativePositioned_nid_no_patch(descriptor, buf, nbytes, offset, false);
}
static std::int64_t NativePwrite(int descriptor, const void* buf, std::size_t nbytes, std::int64_t offset) {
    return NativePositioned_nid_no_patch(descriptor, const_cast<void*>(buf), nbytes, offset, true);
}
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/time.h>
#include <dirent.h>
#include <sys/syscall.h>
#include <sys/uio.h>
static int NativeRmdir(const std::filesystem::path& path) {
    return ::rmdir(path.c_str());
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
    return ::ftruncate(descriptor, static_cast<off_t>(length));
}
static int NativeUtimes(const std::filesystem::path& path, const KernelTimeval* times) {
    if (times == nullptr) return ::utimes(path.c_str(), nullptr);
    struct timeval values[2]{{static_cast<time_t>(times[0].tv_sec), static_cast<suseconds_t>(times[0].tv_usec)},
        {static_cast<time_t>(times[1].tv_sec), static_cast<suseconds_t>(times[1].tv_usec)}};
    return ::utimes(path.c_str(), values);
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
    return static_cast<std::int64_t>(::pwrite(descriptor, buf, nbytes, static_cast<off_t>(offset)));
}
static_assert(sizeof(KernelIovec) == sizeof(struct iovec));
static_assert(offsetof(KernelIovec, base) == offsetof(struct iovec, iov_base));
static_assert(offsetof(KernelIovec, length) == offsetof(struct iovec, iov_len));
static const struct iovec* NativeIovecs(const KernelIovec* iov) {
    return reinterpret_cast<const struct iovec*>(iov);
}
#endif

static constexpr int GUEST_ENOENT = 2;
static constexpr int GUEST_EIO = 5;
static constexpr int GUEST_EBADF = 9;
static constexpr int GUEST_EFAULT = 14;
static constexpr int GUEST_EEXIST = 17;
static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_ENAMETOOLONG = 63;
static constexpr int GUEST_ENOTDIR = 20;
static constexpr int GUEST_EWOULDBLOCK = 35;
static constexpr int GUEST_ENOTEMPTY = 66;

static int SceErrorFromErrno(int error) {
    return static_cast<int>(0x80020000u | static_cast<unsigned>(error > 0 && error <= 34 ? error : error == GUEST_ENOTEMPTY ? error : GUEST_EIO));
}

extern "C" int* APS5_VABI __error_nid_postfix();

static int PosixFailure(int error) {
    *__error_nid_postfix() = error;
    return -1;
}

static int PosixResult(int result) {
    return result < 0 ? PosixFailure(result & 0xffff) : result;
}

#ifdef _WIN32
static int NativeFstatfs(int descriptor, KernelStatfs* fs) {
    if (!File::DirectoryDescriptorPath(descriptor)) {
        const auto handle = reinterpret_cast<HANDLE>(::_get_osfhandle(descriptor));
        if (handle == INVALID_HANDLE_VALUE) return GUEST_EBADF;
        const DWORD type = ::GetFileType(handle);
        if (type == FILE_TYPE_PIPE) return GUEST_EINVAL;
        if (type != FILE_TYPE_DISK)
            throw std::runtime_error("_fstatfs: descriptor " + std::to_string(descriptor) + " is not a file on a Windows volume");
    }
    const auto path = NativeDescriptorPath(descriptor);
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
#else
#include <sys/statvfs.h>
#include <sys/vfs.h>
static int NativeFstatfs(int descriptor, KernelStatfs* fs) {
    constexpr long PipeFilesystem = 0x50495045;
    struct stat file{};
    if (::fstat(descriptor, &file) != 0) return SceErrorFromErrno(errno) & 0xffff;
    if (S_ISSOCK(file.st_mode)) return GUEST_EINVAL;
    if (S_ISFIFO(file.st_mode)) {
        struct statfs kind{};
        if (::fstatfs(descriptor, &kind) == 0 && static_cast<long>(kind.f_type) == PipeFilesystem) return GUEST_EINVAL;
    }
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
    const int result = ::_pipe(native, 4096, _O_BINARY);
#else
    const int result = ::pipe(native);
#endif
    if (result != 0) return PosixFailure(SceErrorFromErrno(errno) & 0xffff);
    std::memcpy(descriptors, native, sizeof(native));
    return 0;
}

static int PathError(const char* path) {
    if (path == nullptr) return GUEST_EFAULT;
    return *path == '\0' ? GUEST_ENOENT : 0;
}

extern "C" {

int APS5_VABI chmod_nid_postfix(const char* path, int mode) {
    if (path == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    auto native = ResolvePath_nid_no_patch(path);
    if (NativeChmod(native, mode) != 0) {
        throw std::runtime_error(std::string(__func__) + ": chmod failed for " + native.string() + ", errno=" + std::to_string(errno));
    }
    return 0;
}

int APS5_VABI close_nid_postfix(int d) {
    if (d >= GuestSockets::FirstDescriptor) return GuestSockets::Close(d);
    File::ForgetRandomDevice(d);
#ifdef _WIN32
    File::ForgetDirectoryDescriptor(d);
    File::ForgetFileLock(d);
    return _close(d);
#else
    return ::close(d);
#endif
}

int APS5_VABI _close_nid_postfix(int descriptor) {
    return close_nid_postfix(descriptor);
}

int APS5_VABI flock_nid_postfix(int d, int operation) {
    const int type = operation & 8 ? 8 : operation & 2 ? 2 : operation & 1 ? 1 : 0;
    if (type == 0) return PosixFailure(GUEST_EBADF);
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
    if (d >= GuestSockets::FirstDescriptor) return PosixFailure(GuestSockets::IsOpen(d) ? GUEST_EINVAL : GUEST_EBADF);
    if (d < 0) return PosixFailure(GUEST_EBADF);
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
        APS5_INVALID_ARG_EX;
    }
    File::FillFileStat(d, sb);
    return 0;
}

int APS5_VABI ftruncate_nid_postfix(int d, int64_t length) {
    if (length < 0) {
        return PosixFailure(GUEST_EINVAL);
    }
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
    return static_cast<int64_t>(sceKernelLseek(d, offset, whence));
}

int APS5_VABI mkdir_nid_postfix(const char* path, uint16_t mode) {
    if (const int error = PathError(path)) return PosixFailure(error);
    auto native = ResolvePath_nid_no_patch(path);
    if (NativeMkdir(native, mode) != 0) {
        return PosixResult(SceErrorFromErrno(errno));
    }
    RecordWrittenPath_nid_no_patch(native);
    return 0;
}

int APS5_VABI open_nid_postfix(const char* path, int flags, int mode) {
    if (const int error = PathError(path)) return PosixFailure(error);
    return PosixResult(sceKernelOpen(path, flags, static_cast<std::uint16_t>(mode)));
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
    if (buf == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    if (offset < 0) {
        APS5_INVALID_ARG_EX;
    }
    const GuestArena::HostWrite destination(buf, nbytes);
    if (!destination.Open()) errno = EFAULT;
    else if (File::ReadRandomDevice(d, buf, nbytes)) return static_cast<int64_t>(nbytes);
    auto n = destination.Open() ? NativePread(d, buf, nbytes, offset) : -1;
    if (n < 0) {
        throw std::runtime_error(std::string(__func__) + ": pread failed, fd=" + std::to_string(d) + ", errno=" + std::to_string(errno));
    }
    return n;
}

int64_t APS5_VABI pwrite_nid_disambig1_nid_postfix(int d, const void* buf, size_t nbytes, int64_t offset) {
    if (buf == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    if (offset < 0) {
        APS5_INVALID_ARG_EX;
    }
    auto n = NativePwrite(d, buf, nbytes, offset);
    if (n < 0) {
        throw std::runtime_error(std::string(__func__) + ": pwrite failed, fd=" + std::to_string(d) + ", errno=" + std::to_string(errno));
    }
    return n;
}

int64_t APS5_VABI read_nid_postfix(int d, void* buf, uint64_t nbytes) {
    if (d >= GuestSockets::FirstDescriptor) return GuestSockets::Read(d, buf, static_cast<size_t>(nbytes));
    return sceKernelRead(d, buf, static_cast<size_t>(nbytes));
}

std::int64_t APS5_VABI _read_nid_postfix(int descriptor, void* buffer, std::size_t count) {
    if (descriptor >= GuestSockets::FirstDescriptor) return GuestSockets::Read(descriptor, buffer, count);
    return sceKernelRead(descriptor, buffer, count);
}

int64_t APS5_VABI write_nid_postfix(int d, const char* str, int64_t size) {
    if (size < 0) {
        APS5_INVALID_ARG_EX;
    }
    if (d >= GuestSockets::FirstDescriptor) return GuestSockets::Write(d, str, static_cast<std::size_t>(size));
    return sceKernelWrite(d, str, static_cast<size_t>(size));
}

std::int64_t APS5_VABI _write_nid_postfix(int descriptor, const void* buffer, std::size_t count) {
    if (descriptor >= GuestSockets::FirstDescriptor) return GuestSockets::Write(descriptor, buffer, count);
    return sceKernelWrite(descriptor, buffer, count);
}

int APS5_VABI stat_nid_postfix(const char* path, FileStat* sb) {
    if (sb == nullptr) return PosixFailure(GUEST_EFAULT);
    if (const int error = PathError(path)) return PosixFailure(error);
    return PosixResult(sceKernelStat(path, sb));
}

int APS5_VABI lstat_nid_postfix(const char* path, FileStat* sb) {
    if (sb == nullptr) return PosixFailure(GUEST_EFAULT);
    if (const int error = PathError(path)) return PosixFailure(error);
    if (!File::FillLinkStat(ResolvePath_nid_no_patch(path), sb)) return PosixResult(SceErrorFromErrno(errno));
    return 0;
}

int APS5_VABI unlink_nid_postfix(const char* path) {
    if (const int error = PathError(path)) return PosixFailure(error);
    return PosixResult(sceKernelUnlink(path));
}

int APS5_VABI sceKernelCheckReachability(const char* path) {
    if (path == nullptr) return SCE_KERNEL_ERROR_EINVAL;
    if (std::strlen(path) > 255) return SCE_KERNEL_ERROR_ENAMETOOLONG;
    std::error_code error;
    return std::filesystem::exists(ResolvePath_nid_no_patch(path), error) ? 0 : SCE_KERNEL_ERROR_ENOENT;
}

int APS5_VABI sceKernelFstat(int d, FileStat* sb) {
    if (sb == nullptr) throw std::invalid_argument("sceKernelFstat: sb is null");
    if (!File::FillFileStatFromDescriptor(d, sb)) return SceErrorFromErrno(errno);
    return 0;
}

int APS5_VABI sceKernelFsync(int fd) {
#ifdef _WIN32
 return ::_commit(fd);
#else
 return ::fsync(fd);
#endif
}

int APS5_VABI sceKernelWriteThrottlingStatus(std::uint64_t* status) {
    if (status == nullptr) throw std::invalid_argument("sceKernelWriteThrottlingStatus: status is null");
    status[0] = std::numeric_limits<std::uint32_t>::max();
    status[1] = 0;
    status[2] = 0;
    status[3] = 0;
    return 0;
}

#ifdef _WIN32

int APS5_VABI sceKernelGetdirentries(int fd, char* buf, int nbytes, int64_t* basep) {
    if (buf == nullptr) return SceErrorFromErrno(GUEST_EFAULT);
    if (nbytes <= 0) return SceErrorFromErrno(GUEST_EINVAL);
    if (basep != nullptr) *basep = 0;
    return File::ReadDirectoryDescriptor(fd, buf, nbytes);
}

int APS5_VABI sceKernelGetdents(int fd, char* buf, int nbytes) {
    return sceKernelGetdirentries(fd, buf, nbytes, nullptr);
}

#else

int APS5_VABI sceKernelGetdirentries(int fd, char* buf, int nbytes, int64_t* basep) {
    constexpr std::size_t GuestHeaderBytes = 8;
    constexpr std::size_t GuestMaxName = 255;
    if (buf == nullptr) return SceErrorFromErrno(GUEST_EFAULT);
    if (nbytes <= 0) return SceErrorFromErrno(GUEST_EINVAL);
    const off_t base = ::lseek(fd, 0, SEEK_CUR);
    if (base < 0) return SceErrorFromErrno(errno);
    if (basep != nullptr) *basep = static_cast<int64_t>(base);
    std::vector<char> host(static_cast<std::size_t>(nbytes) < 4096 ? 4096 : static_cast<std::size_t>(nbytes));
    const auto read = ::syscall(SYS_getdents64, fd, host.data(), host.size());
    if (read < 0) return SceErrorFromErrno(errno);
    std::size_t written = 0;
    off_t resume = base;
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
        out[6] = static_cast<char>(entry->d_type);
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

int APS5_VABI getdents_nid_postfix(int fd, char* buf, int nbytes) {
    return PosixResult(sceKernelGetdents(fd, buf, nbytes));
}

int APS5_VABI sceKernelMkdir(const char* path, uint16_t mode) {
    (void)mode;
    if (path == nullptr) throw std::invalid_argument("sceKernelMkdir: path is null");
    const auto native = ResolvePath_nid_no_patch(path);
    std::error_code error;
    if (std::filesystem::exists(native, error)) return SceErrorFromErrno(GUEST_EEXIST);
    if (!std::filesystem::exists(native.parent_path(), error)) return SceErrorFromErrno(GUEST_ENOENT);
    if (!std::filesystem::create_directory(native, error)) return SceErrorFromErrno(error.value() ? error.value() : GUEST_EIO);
    RecordWrittenPath_nid_no_patch(native);
    return 0;
}

static int CheckIovecs(const KernelIovec* iov, int iovcnt) {
    if (iovcnt < 0 || iovcnt > KERNEL_IOV_MAX) return SceErrorFromErrno(GUEST_EINVAL);
    if (iov == nullptr && iovcnt != 0) return SceErrorFromErrno(GUEST_EFAULT);
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

#ifdef _WIN32

int64_t APS5_VABI sceKernelPread(int d, void* buf, size_t nbytes, int64_t offset) {
    if (buf == nullptr && nbytes != 0) return SceErrorFromErrno(GUEST_EFAULT);
    if (offset < 0) return SceErrorFromErrno(GUEST_EINVAL);
    const GuestArena::HostWrite destination(buf, nbytes);
    if (!destination.Open()) return SceErrorFromErrno(GUEST_EFAULT);
    if (File::ReadRandomDevice(d, buf, nbytes)) return static_cast<int64_t>(nbytes);
    char empty = 0;
    const auto result = NativePread(d, nbytes == 0 && buf == nullptr ? &empty : buf, nbytes, offset);
    return result < 0 ? SceErrorFromErrno(errno) : result;
}

int64_t APS5_VABI sceKernelPwrite(int d, const void* buf, size_t nbytes, int64_t offset) {
    if (buf == nullptr && nbytes != 0) return SceErrorFromErrno(GUEST_EFAULT);
    if (offset < 0) return SceErrorFromErrno(GUEST_EINVAL);
    char empty = 0;
    const auto result = NativePwrite(d, nbytes == 0 && buf == nullptr ? &empty : buf, nbytes, offset);
    return result < 0 ? SceErrorFromErrno(errno) : result;
}

static std::int64_t TransferIovecs(int d, const KernelIovec* iov, int iovcnt, const std::int64_t* offset, bool write) {
    if (const int error = CheckIovecs(iov, iovcnt)) return error;
    if (offset != nullptr && *offset < 0) return SceErrorFromErrno(GUEST_EINVAL);
    std::size_t total = 0;
    for (int i = 0; i < iovcnt; ++i) {
        if (iov[i].base == nullptr && iov[i].length != 0) return SceErrorFromErrno(GUEST_EFAULT);
        if (iov[i].length > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()) - total) return SceErrorFromErrno(GUEST_EINVAL);
        total += iov[i].length;
    }
    if (offset != nullptr && total > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max() - *offset)) return SceErrorFromErrno(GUEST_EINVAL);
    std::deque<GuestArena::HostWrite> destinations;
    if (!write && !OpenIovecs(iov, iovcnt, destinations)) return SceErrorFromErrno(GUEST_EFAULT);
    if (!write && File::IsRandomDevice(d)) return ReadRandomIovecs(d, iov, iovcnt);
    if (total == 0) {
        char none = 0;
        const auto result = offset != nullptr ? NativePositioned_nid_no_patch(d, &none, 0, *offset, write) : NativeTransfer(d, &none, 0, write);
        return result < 0 ? SceErrorFromErrno(errno) : 0;
    }
    const bool whole = write || offset != nullptr || NativeIsDisk(d);
    std::int64_t done = 0;
    for (int i = 0; i < iovcnt; ++i) {
        auto* base = static_cast<char*>(iov[i].base);
        for (std::size_t position = 0; position < iov[i].length;) {
            const auto chunk = std::min<std::size_t>(iov[i].length - position, std::numeric_limits<int>::max());
            const auto result = offset != nullptr ? NativePositioned_nid_no_patch(d, base + position, chunk, *offset + done, write) : NativeTransfer(d, base + position, chunk, write);
            if (result < 0) return done != 0 ? done : SceErrorFromErrno(errno);
            done += result;
            position += static_cast<std::size_t>(result);
            if (static_cast<std::size_t>(result) < chunk || !whole) return done;
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
    if (buf == nullptr && nbytes != 0) return SceErrorFromErrno(GUEST_EFAULT);
    if (offset < 0) return SceErrorFromErrno(GUEST_EINVAL);
    const GuestArena::HostWrite destination(buf, nbytes);
    if (!destination.Open()) return SceErrorFromErrno(GUEST_EFAULT);
    if (File::ReadRandomDevice(d, buf, nbytes)) return static_cast<int64_t>(nbytes);
    const auto result = NativePread(d, buf, nbytes, offset);
    return result < 0 ? SceErrorFromErrno(errno) : result;
}

int64_t APS5_VABI sceKernelPwrite(int d, const void* buf, size_t nbytes, int64_t offset) {
    if (buf == nullptr && nbytes != 0) return SceErrorFromErrno(GUEST_EFAULT);
    if (offset < 0) return SceErrorFromErrno(GUEST_EINVAL);
    const auto result = NativePwrite(d, buf, nbytes, offset);
    return result < 0 ? SceErrorFromErrno(errno) : result;
}

int64_t APS5_VABI sceKernelReadv(int d, const KernelIovec* iov, int iovcnt) {
    if (const int error = CheckIovecs(iov, iovcnt)) return error;
    std::deque<GuestArena::HostWrite> destinations;
    if (!OpenIovecs(iov, iovcnt, destinations)) return SceErrorFromErrno(GUEST_EFAULT);
    if (File::IsRandomDevice(d)) return ReadRandomIovecs(d, iov, iovcnt);
    const auto result = static_cast<std::int64_t>(::readv(d, NativeIovecs(iov), iovcnt));
    return result < 0 ? SceErrorFromErrno(errno) : result;
}

int64_t APS5_VABI sceKernelWritev(int d, const KernelIovec* iov, int iovcnt) {
    if (const int error = CheckIovecs(iov, iovcnt)) return error;
    const auto result = static_cast<std::int64_t>(::writev(d, NativeIovecs(iov), iovcnt));
    return result < 0 ? SceErrorFromErrno(errno) : result;
}

int64_t APS5_VABI sceKernelPreadv(int d, const KernelIovec* iov, int iovcnt, int64_t offset) {
    if (const int error = CheckIovecs(iov, iovcnt)) return error;
    if (offset < 0) return SceErrorFromErrno(GUEST_EINVAL);
    std::deque<GuestArena::HostWrite> destinations;
    if (!OpenIovecs(iov, iovcnt, destinations)) return SceErrorFromErrno(GUEST_EFAULT);
    if (File::IsRandomDevice(d)) return ReadRandomIovecs(d, iov, iovcnt);
    const auto result = static_cast<std::int64_t>(::preadv(d, NativeIovecs(iov), iovcnt, static_cast<off_t>(offset)));
    return result < 0 ? SceErrorFromErrno(errno) : result;
}

int64_t APS5_VABI sceKernelPwritev(int d, const KernelIovec* iov, int iovcnt, int64_t offset) {
    if (const int error = CheckIovecs(iov, iovcnt)) return error;
    if (offset < 0) return SceErrorFromErrno(GUEST_EINVAL);
    const auto result = static_cast<std::int64_t>(::pwritev(d, NativeIovecs(iov), iovcnt, static_cast<off_t>(offset)));
    return result < 0 ? SceErrorFromErrno(errno) : result;
}

#endif

int APS5_VABI sceKernelRename(const char* from, const char* to) {
    if (from == nullptr || to == nullptr) throw std::invalid_argument("sceKernelRename: path is null");
    const auto source = ResolvePath_nid_no_patch(from);
    std::error_code error;
    if (!std::filesystem::exists(source, error)) return SceErrorFromErrno(GUEST_ENOENT);
    const auto destination = ResolvePath_nid_no_patch(to);
    std::filesystem::rename(source, destination, error);
    if (error) return SceErrorFromErrno(GUEST_EIO);
    RecordWrittenPath_nid_no_patch(source);
    RecordWrittenPath_nid_no_patch(destination);
    return 0;
}

int APS5_VABI sceKernelRmdir(const char* path) {
    if (path == nullptr) throw std::invalid_argument("sceKernelRmdir: path is null");
    const auto native = ResolvePath_nid_no_patch(path);
    std::error_code error;
    if (!std::filesystem::is_directory(native, error)) return SceErrorFromErrno(std::filesystem::exists(native, error) ? GUEST_ENOTDIR : GUEST_ENOENT);
    if (!std::filesystem::is_empty(native, error)) return SceErrorFromErrno(GUEST_ENOTEMPTY);
    if (!std::filesystem::remove(native, error)) return SceErrorFromErrno(GUEST_EIO);
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
    return chmod_nid_postfix(path, mode);
}

int APS5_VABI sceKernelFchmod(int d, std::uint16_t mode) {
    if (d >= GuestSockets::FirstDescriptor) return SceErrorFromErrno(GuestSockets::IsOpen(d) ? GUEST_EINVAL : GUEST_EBADF);
    if (NativeFchmod(d, mode & 07777) != 0) return SceErrorFromErrno(errno);
    return 0;
}

int APS5_VABI fchmod_nid_postfix(int d, int mode) {
    return PosixResult(sceKernelFchmod(d, static_cast<std::uint16_t>(mode)));
}

int APS5_VABI sceKernelTruncate_nid_postfix(const char* path, std::int64_t length) {
    if (path == nullptr) throw std::invalid_argument("sceKernelTruncate: path is null");
    if (length < 0) return SceErrorFromErrno(GUEST_EINVAL);
    const auto native = ResolvePath_nid_no_patch(path);
    std::error_code error;
    if (!std::filesystem::exists(native, error)) return SceErrorFromErrno(GUEST_ENOENT);
    std::filesystem::resize_file(native, static_cast<std::uintmax_t>(length), error);
    if (error) return SceErrorFromErrno(GUEST_EIO);
    RecordWrittenPath_nid_no_patch(native);
    return 0;
}

int APS5_VABI sceKernelUtimes_nid_postfix(const char* path, const KernelTimeval* times) {
    if (path == nullptr) throw std::invalid_argument("sceKernelUtimes: path is null");
    if (times != nullptr) {
        for (int i = 0; i < 2; ++i) {
            if (times[i].tv_usec < 0 || times[i].tv_usec >= 1000000) return SCE_KERNEL_ERROR_EINVAL;
        }
    }
    const auto native = ResolvePath_nid_no_patch(path);
    if (NativeUtimes(native, times) != 0) return SceErrorFromErrno(errno);
    return 0;
}

int APS5_VABI utimes_nid_postfix(const char* path, const KernelTimeval* times) {
    if (const int error = PathError(path)) return PosixFailure(error);
    return PosixResult(sceKernelUtimes_nid_postfix(path, times));
}

int APS5_VABI futimes_nid_postfix(int d, const KernelTimeval* times) {
    if (d >= GuestSockets::FirstDescriptor) return PosixFailure(GuestSockets::IsOpen(d) ? GUEST_EINVAL : GUEST_EBADF);
    if (times != nullptr) {
        for (int i = 0; i < 2; ++i) {
            if (times[i].tv_usec < 0 || times[i].tv_usec >= 1000000) return PosixFailure(GUEST_EINVAL);
        }
    }
    if (NativeFutimes(d, times) != 0) return PosixResult(SceErrorFromErrno(errno));
    return 0;
}

int APS5_VABI fsync_nid_postfix(int fd) {
    if (sceKernelFsync(fd) != 0) return PosixFailure(errno);
    return 0;
}

int APS5_VABI fdatasync_nid_postfix(int fd) {
    if (fd >= GuestSockets::FirstDescriptor) return PosixFailure(GuestSockets::IsOpen(fd) ? GUEST_EINVAL : GUEST_EBADF);
#ifdef _WIN32
    if (::_commit(fd) != 0) return PosixResult(SceErrorFromErrno(errno));
#else
    if (::fdatasync(fd) != 0) return PosixResult(SceErrorFromErrno(errno));
#endif
    return 0;
}

}
