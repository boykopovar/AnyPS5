extern "C" {
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <limits>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/FilesystemError.hpp"
#include "prx/libc/include/RenameDot.hpp"
#include "prx/libc/include/WindowsVerbatimPath.hpp"
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
#include "prx/libkernel/Semaphore/include/Ksem.hpp"
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
#ifdef __APPLE__
extern "C" ssize_t __getdirentries64(int fd, void* buffer, size_t bytes, off_t* base);
#endif
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
std::int64_t File::TransferInParts(int descriptor, const iovec* iov, int iovcnt, const std::int64_t* offset, bool write) {
    constexpr std::uint64_t singleCallBytes = 0x7ffff000;
    constexpr std::size_t partBytes = std::size_t{1} << 30;
    constexpr auto maxBytes = static_cast<std::uint64_t>(std::numeric_limits<ssize_t>::max());
    std::uint64_t total = 0;
    for (int i = 0; i < iovcnt; ++i) {
        if (iov[i].iov_len > maxBytes - total) return -2;
        total += iov[i].iov_len;
    }
    if (total <= singleCallBytes) return -2;
    if (offset != nullptr && (*offset < 0 || total > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max() - *offset))) return -2;
    struct stat status{};
    if (::fstat(descriptor, &status) != 0 || !S_ISREG(status.st_mode)) return -2;
    std::vector<iovec> part;
    std::int64_t done = 0;
    int index = 0;
    std::size_t skip = 0;
    while (static_cast<std::uint64_t>(done) < total) {
        part.clear();
        std::size_t bytes = 0;
        while (index < iovcnt && bytes < partBytes) {
            const auto take = std::min(iov[index].iov_len - skip, partBytes - bytes);
            part.push_back({static_cast<char*>(iov[index].iov_base) + skip, take});
            bytes += take;
            skip += take;
            if (skip == iov[index].iov_len) {
                ++index;
                skip = 0;
            }
        }
        const auto count = static_cast<int>(part.size());
        const auto at = static_cast<off_t>(offset != nullptr ? *offset + done : 0);
        const auto moved = static_cast<std::int64_t>(offset == nullptr ? (write ? ::writev(descriptor, part.data(), count) : ::readv(descriptor, part.data(), count))
                                                                       : (write ? ::pwritev(descriptor, part.data(), count, at) : ::preadv(descriptor, part.data(), count, at)));
        if (moved < 0) return done != 0 ? done : -1;
        done += moved;
        if (static_cast<std::size_t>(moved) < bytes) break;
    }
    return done;
}
static std::int64_t NativePread(int descriptor, void* buf, std::size_t nbytes, std::int64_t offset) {
    const iovec whole{buf, nbytes};
    const auto parts = File::TransferInParts(descriptor, &whole, 1, &offset, false);
    return parts == -2 ? static_cast<std::int64_t>(::pread(descriptor, buf, nbytes, static_cast<off_t>(offset))) : parts;
}
#ifdef __APPLE__
std::int64_t File::WritevAtEnd(int descriptor, const iovec* iov, int iovcnt) {
    struct stat status{};
    if (!File::Appends(descriptor) || ::fstat(descriptor, &status) != 0 || !S_ISREG(status.st_mode)) return -2;
    const off_t position = ::lseek(descriptor, 0, SEEK_CUR);
    if (position < 0) return -1;
    const auto result = static_cast<std::int64_t>(::writev(descriptor, iov, iovcnt));
    const int error = errno;
    if (::lseek(descriptor, position, SEEK_SET) < 0) throw std::runtime_error("WritevAtEnd: cannot restore the file offset");
    errno = error;
    return result;
}
#endif
static std::int64_t NativePwrite(int descriptor, const void* buf, std::size_t nbytes, std::int64_t offset) {
    const auto result = File::PwriteSharedMemory_nid_no_patch(descriptor, buf, nbytes, offset);
    if (result != -2) return result;
    const iovec whole{const_cast<void*>(buf), nbytes};
#ifdef __APPLE__
    if (const auto appended = File::WritevAtEnd(descriptor, &whole, 1); appended != -2) return appended;
#endif
    const auto parts = File::TransferInParts(descriptor, &whole, 1, &offset, true);
    return parts == -2 ? static_cast<std::int64_t>(::pwrite(descriptor, buf, nbytes, static_cast<off_t>(offset))) : parts;
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

static bool IsPipeDescriptor(int descriptor) {
#ifdef _WIN32
    if (GuestSockets::IsOpen(descriptor)) return false;
    const auto handle = reinterpret_cast<HANDLE>(WithoutParameterHandler([descriptor] { return ::_get_osfhandle(descriptor); }));
    return handle != INVALID_HANDLE_VALUE && ::GetFileType(handle) == FILE_TYPE_PIPE;
#else
    struct stat status;
    return ::fstat(descriptor, &status) == 0 && S_ISFIFO(status.st_mode);
#endif
}

static bool IsSeekableDescriptor(int descriptor) {
    if (GuestSockets::IsOpen(descriptor)) return false;
    return File::IsVnode(descriptor) || File::IsCharacterDevice(descriptor) || File::IsRandomDevice(descriptor);
}

static int PositionedArgumentError(int descriptor, bool write, bool nullBuffer, std::int64_t offset) {
    if (IsKqueueDescriptor_nid_no_patch(descriptor)) return GUEST_ESPIPE;
    if (!nullBuffer && offset >= 0) return 0;
    if (!GuestSockets::IsOpen(descriptor) && !DescriptorIsOpen_nid_no_patch(descriptor)) return GUEST_EBADF;
    if (!IsSeekableDescriptor(descriptor)) return GUEST_ESPIPE;
    if (!File::GrantsAccess(descriptor, write)) return GUEST_EBADF;
    if (offset < 0 && !File::IsRandomDevice(descriptor)) return GUEST_EINVAL;
    return nullBuffer ? GUEST_EFAULT : 0;
}

static int TransferArgumentError(int descriptor, bool nullBuffer) {
    if (!nullBuffer) return 0;
    return (GuestSockets::IsOpen(descriptor) || DescriptorIsOpen_nid_no_patch(descriptor)) ? GUEST_EFAULT : GUEST_EBADF;
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
static std::wstring VolumeLookupPath(const std::filesystem::path& path) {
    const HANDLE handle = ::CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return path.native();
    std::wstring final(MAX_PATH, L'\0');
    auto length = ::GetFinalPathNameByHandleW(handle, final.data(), static_cast<DWORD>(final.size()), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (length >= final.size()) {
        final.resize(length);
        length = ::GetFinalPathNameByHandleW(handle, final.data(), length, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    }
    ::CloseHandle(handle);
    if (length == 0 || length >= final.size()) return path.native();
    final.resize(length);
    return final;
}

static int NativeStatfs(const std::filesystem::path& path, KernelStatfs* fs) {
    const auto lookup = VolumeLookupPath(path);
    std::wstring root(lookup.size() + 2, L'\0');
    if (!::GetVolumePathNameW(lookup.c_str(), root.data(), static_cast<DWORD>(root.size())))
        throw std::runtime_error("_fstatfs: GetVolumePathNameW failed for " + path.string() + ", error=" + std::to_string(::GetLastError()));
    DWORD sectorsPerCluster = 0, bytesPerSector = 0, freeClusters = 0, totalClusters = 0;
    ULARGE_INTEGER available{}, total{}, totalFree{};
    DWORD nameMax = 0, volumeFlags = 0;
    if (!::GetDiskFreeSpaceW(root.c_str(), &sectorsPerCluster, &bytesPerSector, &freeClusters, &totalClusters) ||
        !::GetDiskFreeSpaceExW(root.c_str(), &available, &total, &totalFree) ||
        !::GetVolumeInformationW(root.c_str(), nullptr, 0, nullptr, &nameMax, &volumeFlags, nullptr, 0))
        throw std::runtime_error("_fstatfs: volume query failed for " + path.string() + ", error=" + std::to_string(::GetLastError()));
    const std::uint64_t cluster = static_cast<std::uint64_t>(sectorsPerCluster) * bytesPerSector;
    if (cluster == 0) throw std::runtime_error("_fstatfs: volume of " + path.string() + " reports no cluster size");
    fs->f_flags = (volumeFlags & FILE_READ_ONLY_VOLUME) ? KERNEL_MNT_RDONLY : 0;
    fs->f_bsize = cluster;
    fs->f_iosize = cluster;
    fs->f_blocks = total.QuadPart / cluster;
    fs->f_bfree = totalFree.QuadPart / cluster;
    fs->f_bavail = static_cast<std::int64_t>(available.QuadPart / cluster);
    fs->f_namemax = nameMax;
    return 0;
}

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
    return NativeStatfs(*path, fs);
}

static bool NativeIsPipe(int descriptor) {
    const auto handle = reinterpret_cast<HANDLE>(::_get_osfhandle(descriptor));
    return handle != INVALID_HANDLE_VALUE && ::GetFileType(handle) == FILE_TYPE_PIPE;
}
#else
#include <sys/statvfs.h>
#ifdef __APPLE__
#include <sys/mount.h>
#else
#include <sys/vfs.h>
#endif
static bool NativeIsPipe(int descriptor) {
#ifdef __APPLE__
    struct stat file{};
    struct statfs kind{};
    return ::fstat(descriptor, &file) == 0 && S_ISFIFO(file.st_mode) && ::fstatfs(descriptor, &kind) != 0;
#else
    constexpr long PipeFilesystem = 0x50495045;
    struct stat file{};
    struct statfs kind{};
    return ::fstat(descriptor, &file) == 0 && S_ISFIFO(file.st_mode) && ::fstatfs(descriptor, &kind) == 0
        && static_cast<long>(kind.f_type) == PipeFilesystem;
#endif
}

static int FillStatfs(const struct statvfs& host, bool noexec, KernelStatfs* fs);

static int NativeFstatfs(int descriptor, KernelStatfs* fs) {
    struct stat file{};
    if (::fstat(descriptor, &file) != 0) return SceErrorFromErrno(errno) & 0xffff;
#ifdef __APPLE__
    if (S_ISSOCK(file.st_mode)) return GUEST_EINVAL;
    if (NativeIsPipe(descriptor)) return GUEST_EINVAL;
#else
    if (S_ISSOCK(file.st_mode) && (::fcntl(descriptor, F_GETFL) & O_PATH) == 0) return GUEST_EINVAL;
    if (NativeIsPipe(descriptor)) return GUEST_EINVAL;
#endif
    if (S_ISCHR(file.st_mode)) return DeviceFilesystem(fs);
    struct statvfs host{};
    if (::fstatvfs(descriptor, &host) != 0) return SceErrorFromErrno(errno) & 0xffff;
#ifdef __APPLE__
    struct statfs mount{};
    const bool noexec = ::fstatfs(descriptor, &mount) == 0 && (mount.f_flags & MNT_NOEXEC);
#else
    const bool noexec = (host.f_flag & ST_NOEXEC) != 0;
#endif
    return FillStatfs(host, noexec, fs);
}

static int FillStatfs(const struct statvfs& host, bool noexec, KernelStatfs* fs) {
    if (host.f_flag & ST_RDONLY) fs->f_flags |= KERNEL_MNT_RDONLY;
    if (host.f_flag & ST_NOSUID) fs->f_flags |= KERNEL_MNT_NOSUID;
    if (noexec) fs->f_flags |= KERNEL_MNT_NOEXEC;
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

static int NativeStatfs(const std::filesystem::path& path, KernelStatfs* fs) {
    struct statvfs host{};
    if (::statvfs(path.c_str(), &host) != 0) return SceErrorFromErrno(errno == ENOENT ? NotFoundError_nid_no_patch(path) : errno) & 0xffff;
#ifdef __APPLE__
    struct statfs mount{};
    const bool noexec = ::statfs(path.c_str(), &mount) == 0 && (mount.f_flags & MNT_NOEXEC);
#else
    const bool noexec = (host.f_flag & ST_NOEXEC) != 0;
#endif
    return FillStatfs(host, noexec, fs);
}


#endif

extern "C" int APS5_VABI pipe2_nid_postfix(int* descriptors, int flags) {
    if ((flags & ~(SCE_KERNEL_O_NONBLOCK | SCE_KERNEL_O_CLOEXEC)) != 0) return PosixFailure(GUEST_EINVAL);
    int native[2];
#ifdef _WIN32
    int mode = _O_BINARY;
    if ((flags & SCE_KERNEL_O_CLOEXEC) != 0) mode |= _O_NOINHERIT;
    const int result = ::_pipe(native, 65536, mode);
#elif defined(__APPLE__)
    int result = ::pipe(native);
    if (result == 0) {
        for (const int descriptor : native) {
            if (((flags & SCE_KERNEL_O_CLOEXEC) != 0 && ::fcntl(descriptor, F_SETFD, FD_CLOEXEC) != 0) ||
                ((flags & SCE_KERNEL_O_NONBLOCK) != 0 && ::fcntl(descriptor, F_SETFL, ::fcntl(descriptor, F_GETFL) | O_NONBLOCK) != 0)) {
                const int saved = errno;
                ::close(native[0]);
                ::close(native[1]);
                errno = saved;
                result = -1;
                break;
            }
        }
    }
#else
    int hostFlags = 0;
#ifdef O_CLOEXEC
    if ((flags & SCE_KERNEL_O_CLOEXEC) != 0) hostFlags |= O_CLOEXEC;
#endif
#ifdef O_NONBLOCK
    if ((flags & SCE_KERNEL_O_NONBLOCK) != 0) hostFlags |= O_NONBLOCK;
#endif
    const int result = ::pipe2(native, hostFlags);
#endif
    if (result != 0) return PosixFailure(SceErrorFromErrno(errno) & 0xffff);
#ifdef _WIN32
    if ((flags & SCE_KERNEL_O_NONBLOCK) != 0) {
        DWORD pipeMode = PIPE_NOWAIT;
        ::SetNamedPipeHandleState(WindowsDescriptorHandle_nid_no_patch(native[0]), &pipeMode, nullptr, nullptr);
        ::SetNamedPipeHandleState(WindowsDescriptorHandle_nid_no_patch(native[1]), &pipeMode, nullptr, nullptr);
    }
#endif
    File::RememberPipe(native[0], flags);
    File::RememberPipe(native[1], flags);
    if (!GuestCopyOut_nid_no_patch(descriptors, native, sizeof(native))) {
        close_nid_postfix(native[0]);
        close_nid_postfix(native[1]);
        return PosixFailure(GUEST_EFAULT);
    }
    return 0;
}

}
