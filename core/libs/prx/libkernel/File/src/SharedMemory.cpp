#include "prx/libc/include/General.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/File/include/SharedMemory.hpp"
#include "prx/libc/include/CreationMask.hpp"
#include "prx/libkernel/File/include/File.hpp"

#include <cerrno>
#include <algorithm>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <string>
#include <cstdint>
#include <limits>
#include <optional>
#include <atomic>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#include <filesystem>
#include <random>
#else
#include "nid/Sha1.hpp"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <unistd.h>
#include <cstdio>
#endif

extern "C" int* APS5_VABI __error_nid_postfix();

namespace {

bool IsHostSharedMemoryDescriptor(int descriptor) {
#ifdef __linux__
    char descriptorPath[64];
    std::snprintf(descriptorPath, sizeof(descriptorPath), "/proc/self/fd/%d", descriptor);
    char objectPath[4096];
    const auto length = ::readlink(descriptorPath, objectPath, sizeof(objectPath) - 1);
    if (length < 0) return false;
    objectPath[length] = '\0';
    return std::string_view(objectPath).starts_with("/dev/shm/anyps5-shm-") ||
        std::string_view(objectPath).starts_with("/memfd:anyps5 shm_open");
#else
    (void)descriptor;
    return false;
#endif
}

class HostShmLock {
public:
    HostShmLock() {
#ifdef __linux__
        descriptor = ::open("/dev/shm", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (descriptor < 0 || ::flock(descriptor, LOCK_EX) != 0) error = errno;
#endif
    }

    ~HostShmLock() {
#ifdef __linux__
        const int savedError = errno;
        if (descriptor >= 0) {
            if (!error) ::flock(descriptor, LOCK_UN);
            ::close(descriptor);
        }
        errno = savedError;
#endif
    }

    int Error() const { return error; }

private:
    int descriptor = -1;
    int error = 0;
};

constexpr int GuestNoMemory = 12;
constexpr int GuestInvalid = 22;
constexpr int GuestTooManyFilesInSystem = 23;
constexpr int GuestTooManyFiles = 24;
constexpr int GuestCloseOnExec = 0x00100000;
constexpr int AcceptedFlags = SCE_KERNEL_O_ACCMODE | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL | SCE_KERNEL_O_TRUNC | GuestCloseOnExec;

const char* const anonymousObject = reinterpret_cast<const char*>(1);

int Fail(int error) {
    *__error_nid_postfix() = error;
    return -1;
}

int GuestError(int error) {
    switch (error) {
    case EMFILE:
        return GuestTooManyFiles;
    case ENFILE:
        return GuestTooManyFilesInSystem;
    case ENOMEM:
        return GuestNoMemory;
    default:
        return 0;
    }
}

#ifndef _WIN32
int NamedGuestError(int error) {
    switch (error) {
    case ENOENT:
        return 2;
    case EINTR:
        return 4;
    case EACCES:
        return 13;
    case EFAULT:
        return 14;
    case EEXIST:
        return 17;
    case EINVAL:
        return GuestInvalid;
    case ENOSPC:
        return 28;
    case EROFS:
        return 30;
    case ENAMETOOLONG:
        return 63;
    default:
        return GuestError(error);
    }
}

std::string NamedHostPath(const char* path, int& error) {
    std::string copy;
    if (const int copyError = CopyGuestPath_nid_no_patch(path, copy)) { error = copyError; return {}; }
    if (copy.empty() || copy[0] != '/') { error = EINVAL; return {}; }
    const auto digest = Nid::Sha1(std::vector<std::uint8_t>(copy.begin(), copy.end()));
    std::string hostName = "/anyps5-shm-";
    for (const auto byte : digest) {
        hostName += "0123456789abcdef"[byte >> 4];
        hostName += "0123456789abcdef"[byte & 15];
    }
    error = 0;
    return hostName;
}

int OpenNamedFile(const char* path, int flags, int mode) {
    int validationError = 0;
    const auto hostName = NamedHostPath(path, validationError);
    if (validationError != 0) return Fail(NamedGuestError(validationError) ? NamedGuestError(validationError) : GuestInvalid);
    HostShmLock namespaceLock;
    if (namespaceLock.Error() != 0) {
        errno = namespaceLock.Error();
        if (const int guest = NamedGuestError(errno)) return Fail(guest);
        throw std::runtime_error("shm_open: locking the host shared-memory namespace failed, errno=" + std::to_string(errno));
    }
    int hostFlags = (flags & SCE_KERNEL_O_ACCMODE) == SCE_KERNEL_O_RDWR ? O_RDWR : O_RDONLY;
    if (flags & SCE_KERNEL_O_CREAT) hostFlags |= O_CREAT;
    if (flags & SCE_KERNEL_O_EXCL) hostFlags |= O_EXCL;
    if ((flags & SCE_KERNEL_O_TRUNC) && (flags & SCE_KERNEL_O_ACCMODE) == SCE_KERNEL_O_RDWR) hostFlags |= O_TRUNC;
    const auto permissions = (mode & 0777) & ~File::GetCreationMask_nid_no_patch();
    bool created = false;
    int descriptor;
    if (flags & SCE_KERNEL_O_CREAT) {
        descriptor = ::shm_open(hostName.c_str(), hostFlags | O_CLOEXEC | O_EXCL, permissions);
        created = descriptor >= 0;
        if (descriptor < 0 && errno == EEXIST && !(flags & SCE_KERNEL_O_EXCL)) {
            descriptor = ::shm_open(hostName.c_str(), (hostFlags & ~(O_CREAT | O_EXCL)) | O_CLOEXEC, 0);
        }
    } else {
        descriptor = ::shm_open(hostName.c_str(), hostFlags | O_CLOEXEC, 0);
    }
    if (descriptor >= 0) {
        if (!created || ::fchmod(descriptor, permissions) == 0) return descriptor;
        const int error = errno;
        ::close(descriptor);
        ::shm_unlink(hostName.c_str());
        if (const int guest = NamedGuestError(error)) return Fail(guest);
        throw std::runtime_error("shm_open: setting guest creation permissions failed, errno=" + std::to_string(error));
    }
    const int error = errno;
    if (const int guest = NamedGuestError(error)) return Fail(guest);
    throw std::runtime_error("shm_open: opening the host named object failed, errno=" + std::to_string(error));
}
#endif

int CreateAnonymousFile() {
#ifdef _WIN32
    static std::atomic<unsigned> sequence{0};
    const auto directory = std::filesystem::temp_directory_path();
    const auto seed = std::to_string(std::random_device{}());
    for (int attempt = 0; attempt < 64; ++attempt) {
        const auto path = directory / ("anyps5-shm-" + seed + "-" + std::to_string(sequence++));
        int descriptor = -1;
        const int error = _wsopen_s(&descriptor, path.c_str(),
                                    _O_CREAT | _O_EXCL | _O_RDWR | _O_BINARY | _O_TEMPORARY | _O_SHORT_LIVED | _O_NOINHERIT,
                                    _SH_DENYNO, _S_IREAD | _S_IWRITE);
        if (error == 0) return descriptor;
        if (error != EEXIST) {
            errno = error;
            return -1;
        }
    }
    errno = EEXIST;
    return -1;
#else
    const int descriptor = memfd_create("anyps5 shm_open", MFD_CLOEXEC);
    return descriptor;
#endif
}

}

namespace File {

bool IsSharedMemoryDescriptor_nid_no_patch(int descriptor) {
    return IsHostSharedMemoryDescriptor(descriptor);
}

namespace {

class SharedMemoryLock {
public:
    explicit SharedMemoryLock(int value) {
#ifdef __linux__
        struct stat originalStatus{};
        if (::fstat(value, &originalStatus) != 0) { shared = true; error = errno; return; }
#endif
        if (!IsHostSharedMemoryDescriptor(value)) {
#ifdef __linux__
            struct stat currentStatus{};
            if (::fstat(value, &currentStatus) != 0) { shared = true; error = errno; return; }
            if (currentStatus.st_dev != originalStatus.st_dev || currentStatus.st_ino != originalStatus.st_ino) {
                shared = true;
                error = EBADF;
            }
#endif
            return;
        }
        shared = true;
        hostLock.emplace();
        if (hostLock->Error() != 0) { error = hostLock->Error(); return; }
        descriptor = Duplicate(value);
        if (descriptor < 0) { error = errno; return; }
        if (!IsHostSharedMemoryDescriptor(descriptor)) { error = EBADF; return; }
        if (!Refresh()) { error = errno; return; }
#ifdef __linux__
        if (status.st_dev != originalStatus.st_dev || status.st_ino != originalStatus.st_ino) error = EBADF;
#endif
    }

    ~SharedMemoryLock() {
        if (descriptor >= 0) {
            const int savedError = errno;
#ifdef _WIN32
            ::_close(descriptor);
#else
            ::close(descriptor);
#endif
            errno = savedError;
        }
    }

    bool IsShared() const { return shared; }
    int Error() const { return error; }
    int Descriptor() const { return descriptor; }
    bool Refresh() {
#ifdef _WIN32
        struct _stat64 nativeStatus{};
        if (::_fstat64(descriptor, &nativeStatus) != 0) return false;
        status.st_size = nativeStatus.st_size;
        return true;
#else
        return ::fstat(descriptor, &status) == 0;
#endif
    }
    struct stat status{};

private:
    static int Duplicate(int value) {
#ifdef _WIN32
        return ::_dup(value);
#else
        return ::fcntl(value, F_DUPFD_CLOEXEC, 0);
#endif
    }

    int descriptor = -1;
    std::optional<HostShmLock> hostLock;
    bool shared = false;
    int error = 0;
};
}

std::int64_t WriteSharedMemory_nid_no_patch(int descriptor, const void* buffer, std::size_t size) {
    SharedMemoryLock guard(descriptor);
    if (!guard.IsShared()) return -2;
    if (guard.Error() != 0) { errno = guard.Error(); return -1; }
    if (!guard.Refresh()) return -1;
    const auto fd = guard.Descriptor();
#ifndef _WIN32
    const int access = ::fcntl(fd, F_GETFL);
    if (access < 0) return -1;
    if ((access & O_ACCMODE) == O_RDONLY) { errno = EBADF; return -1; }
#endif
    const auto& status = guard.status;
#ifdef _WIN32
    const auto position = ::_lseeki64(fd, 0, SEEK_CUR);
#else
    const auto position = ::lseek(fd, 0, SEEK_CUR);
#endif
    if (position < 0) return -1;
    if (position >= status.st_size) return 0;
    const auto count = std::min<std::uint64_t>(size, static_cast<std::uint64_t>(status.st_size - position));
#ifdef _WIN32
    const auto result = ::_write(fd, buffer, static_cast<unsigned int>(std::min<std::uint64_t>(count, std::numeric_limits<unsigned int>::max())));
#else
    const auto result = ::write(fd, buffer, static_cast<std::size_t>(count));
#endif
    return static_cast<std::int64_t>(result);
}

std::int64_t PwriteSharedMemory_nid_no_patch(int descriptor, const void* buffer, std::size_t size, std::int64_t offset) {
    SharedMemoryLock guard(descriptor);
    if (!guard.IsShared()) return -2;
    if (guard.Error() != 0) { errno = guard.Error(); return -1; }
    if (!guard.Refresh()) return -1;
    const auto fd = guard.Descriptor();
#ifndef _WIN32
    const int access = ::fcntl(fd, F_GETFL);
    if (access < 0) return -1;
    if ((access & O_ACCMODE) == O_RDONLY) { errno = EBADF; return -1; }
#endif
    const auto& status = guard.status;
    if (offset < 0) { errno = EINVAL; return -1; }
    if (offset >= status.st_size) return 0;
    const auto count = std::min<std::uint64_t>(size, static_cast<std::uint64_t>(status.st_size - offset));
#ifdef _WIN32
    const auto result = NativePositioned_nid_no_patch(fd, const_cast<void*>(buffer), static_cast<std::size_t>(count), offset, true);
#else
    const auto result = ::pwrite(fd, buffer, static_cast<std::size_t>(count), static_cast<off_t>(offset));
#endif
    return static_cast<std::int64_t>(result);
}

std::int64_t SeekSharedMemory_nid_no_patch(int descriptor, std::int64_t offset, int whence) {
    SharedMemoryLock guard(descriptor);
    if (!guard.IsShared()) return -2;
    if (guard.Error() != 0) { errno = guard.Error(); return -1; }
    if (!guard.Refresh()) return -1;
    const auto fd = guard.Descriptor();
    const auto& status = guard.status;
    std::int64_t base = 0;
    if (whence == SEEK_CUR) {
#ifdef _WIN32
        base = ::_lseeki64(fd, 0, SEEK_CUR);
#else
        base = ::lseek(fd, 0, SEEK_CUR);
#endif
        if (base < 0) return -1;
    } else if (whence == SEEK_END) {
        base = static_cast<std::int64_t>(status.st_size);
    } else if (whence != SEEK_SET) {
        errno = EINVAL;
        return -1;
    }
    if ((offset > 0 && base > std::numeric_limits<std::int64_t>::max() - offset) ||
        (offset < 0 && base < std::numeric_limits<std::int64_t>::min() - offset)) {
        errno = EOVERFLOW;
        return -1;
    }
    const auto target = base + offset;
    if (target < 0 || target > status.st_size) {
        errno = EINVAL;
        return -1;
    }
#ifdef _WIN32
    return ::_lseeki64(fd, target, SEEK_SET);
#else
    return ::lseek(fd, static_cast<off_t>(target), SEEK_SET);
#endif
}

#ifndef _WIN32
static std::int64_t TransferSharedMemoryVectors(int descriptor, const struct iovec* vectors, int count,
                                                std::int64_t offset, bool positioned) {
    SharedMemoryLock guard(descriptor);
    if (!guard.IsShared()) return -2;
    if (guard.Error() != 0) { errno = guard.Error(); return -1; }
    const auto fd = guard.Descriptor();
    const int access = ::fcntl(fd, F_GETFL);
    if (access < 0) return -1;
    if ((access & O_ACCMODE) == O_RDONLY) { errno = EBADF; return -1; }
    if (positioned && offset < 0) { errno = EINVAL; return -1; }
    struct stat status{};
    if (::fstat(fd, &status) != 0) return -1;
    std::uint64_t total = 0;
    constexpr auto maxTransfer = static_cast<std::uint64_t>(std::numeric_limits<ssize_t>::max());
    for (int i = 0; i < count; ++i) {
        if (vectors[i].iov_len > maxTransfer - total) { errno = EINVAL; return -1; }
        total += vectors[i].iov_len;
    }
    std::uint64_t available = 0;
    if (positioned) {
        if (offset < status.st_size) available = static_cast<std::uint64_t>(status.st_size - offset);
        if (total > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max() - offset)) { errno = EINVAL; return -1; }
    } else {
        const auto position = ::lseek(fd, 0, SEEK_CUR);
        if (position < 0) return -1;
        if (position < status.st_size) available = static_cast<std::uint64_t>(status.st_size - position);
    }
    std::vector<struct iovec> bounded;
    bounded.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count && available > 0; ++i) {
        const auto length = static_cast<std::size_t>(std::min<std::uint64_t>(vectors[i].iov_len, available));
        if (length != 0) bounded.push_back({vectors[i].iov_base, length});
        available -= length;
    }
    if (bounded.empty()) return 0;
    return positioned ? ::pwritev(fd, bounded.data(), static_cast<int>(bounded.size()), static_cast<off_t>(offset))
                      : ::writev(fd, bounded.data(), static_cast<int>(bounded.size()));
}

std::int64_t WritevSharedMemory_nid_no_patch(int descriptor, const struct iovec* vectors, int count) {
    return TransferSharedMemoryVectors(descriptor, vectors, count, 0, false);
}

std::int64_t PwritevSharedMemory_nid_no_patch(int descriptor, const struct iovec* vectors, int count, std::int64_t offset) {
    return TransferSharedMemoryVectors(descriptor, vectors, count, offset, true);
}
#endif

int TruncateSharedMemory_nid_no_patch(int descriptor, std::int64_t length) {
    SharedMemoryLock guard(descriptor);
    if (!guard.IsShared()) return -2;
    if (guard.Error() != 0) { errno = guard.Error(); return -1; }
    const auto fd = guard.Descriptor();
#ifdef _WIN32
    const int result = static_cast<int>(::_chsize_s(fd, length));
#else
    const int result = ::ftruncate(fd, static_cast<off_t>(length));
#endif
    return result;
}

}

extern "C" int APS5_VABI shm_open_nid_postfix(const char* path, int flags, int mode) {
    const int accessMode = flags & SCE_KERNEL_O_ACCMODE;
    if (accessMode != SCE_KERNEL_O_RDONLY && accessMode != SCE_KERNEL_O_RDWR) return Fail(GuestInvalid);
    if ((flags & ~AcceptedFlags) != 0) return Fail(GuestInvalid);
    if (path != anonymousObject) {
        std::string copy;
        if (const int error = CopyGuestPath_nid_no_patch(path, copy)) return Fail(error);
        path = copy.c_str();
#ifdef _WIN32
        NotImplemented_nid_no_patch("shm_open of a named object");
#else
        return OpenNamedFile(path, flags, mode);
#endif
    }
    if (accessMode == SCE_KERNEL_O_RDONLY) return Fail(GuestInvalid);
    const int descriptor = CreateAnonymousFile();
    if (descriptor >= 0) return descriptor;
    const int error = errno;
    if (const int guest = GuestError(error)) return Fail(guest);
    throw std::runtime_error("shm_open: creating the host anonymous file failed, errno=" + std::to_string(error));
}

extern "C" int APS5_VABI shm_unlink_nid_postfix(const char* path) {
    std::string copy;
    if (const int error = CopyGuestPath_nid_no_patch(path, copy)) return Fail(error);
    path = copy.c_str();
#ifdef _WIN32
    (void)path;
    NotImplemented_nid_no_patch("shm_unlink of a named object");
    return 0;
#else
    int validationError = 0;
    const auto hostName = NamedHostPath(path, validationError);
    if (validationError != 0) return Fail(NamedGuestError(validationError) ? NamedGuestError(validationError) : GuestInvalid);
    HostShmLock namespaceLock;
    if (namespaceLock.Error() != 0) {
        errno = namespaceLock.Error();
        if (const int guest = NamedGuestError(errno)) return Fail(guest);
        throw std::runtime_error("shm_unlink: locking the host shared-memory namespace failed, errno=" + std::to_string(errno));
    }
    const auto objectPath = "/dev/shm" + hostName;
    if (::faccessat(AT_FDCWD, objectPath.c_str(), R_OK | W_OK, AT_EACCESS) != 0) {
        const int error = errno;
        if (const int guest = NamedGuestError(error)) return Fail(guest);
        throw std::runtime_error("shm_unlink: checking shared-memory permissions failed, errno=" + std::to_string(error));
    }
    if (::shm_unlink(hostName.c_str()) == 0) return 0;
    const int error = errno;
    if (const int guest = NamedGuestError(error)) return Fail(guest);
    throw std::runtime_error("shm_unlink: removing the host named object failed, errno=" + std::to_string(error));
#endif
}
