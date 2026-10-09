#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/File/include/NativeStat.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/DirectoryDescriptor.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "SceTypes.hpp"

#include <cerrno>
#include <limits>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
extern "C" _invalid_parameter_handler _set_thread_local_invalid_parameter_handler(_invalid_parameter_handler);
static void IgnoreInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, std::uintptr_t) {}
static int NativeOpen(const std::filesystem::path& p, int nativeFlags, std::uint16_t mode) {
    return ::_wopen(p.wstring().c_str(), nativeFlags, static_cast<int>(mode));
}
static std::int64_t NativeLseek(int fd, std::int64_t offset, int whence) {
    const auto previous = _set_thread_local_invalid_parameter_handler(IgnoreInvalidParameter);
    const auto result = ::_lseeki64(fd, offset, whence);
    _set_thread_local_invalid_parameter_handler(previous);
    return result;
}
static int NativeRead(int fd, void* buf, std::size_t n) {
    if (n > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("sceKernelRead: nbytes exceeds platform limit");
    }
    char empty = 0;
    if (buf == nullptr) buf = &empty;
    const auto previous = _set_thread_local_invalid_parameter_handler(IgnoreInvalidParameter);
    const int result = ::_read(fd, buf, static_cast<unsigned int>(n));
    _set_thread_local_invalid_parameter_handler(previous);
    return result;
}
static int NativeWrite(int fd, const void* buf, std::size_t n) {
    if (n > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        throw std::runtime_error("sceKernelWrite: nbytes exceeds platform limit");
    }
    char empty = 0;
    if (buf == nullptr) buf = &empty;
    const auto previous = _set_thread_local_invalid_parameter_handler(IgnoreInvalidParameter);
    const int result = ::_write(fd, buf, static_cast<unsigned int>(n));
    _set_thread_local_invalid_parameter_handler(previous);
    return result;
}
static int NativeClose(int fd) {
    const auto previous = _set_thread_local_invalid_parameter_handler(IgnoreInvalidParameter);
    const int result = ::_close(fd);
    _set_thread_local_invalid_parameter_handler(previous);
    return result;
}
static int NativeUnlink(const std::filesystem::path& p) {
    return ::_wunlink(p.wstring().c_str());
}
static int MapFlags(int sceFlags) {
    int f = 0;
    const int acc = sceFlags & SCE_KERNEL_O_ACCMODE;
    if (acc == SCE_KERNEL_O_RDONLY) f |= _O_RDONLY;
    else if (acc == SCE_KERNEL_O_WRONLY) f |= _O_WRONLY;
    else if (acc == SCE_KERNEL_O_RDWR) f |= _O_RDWR;
    else throw std::invalid_argument("sceKernelOpen: invalid access mode");
    if (sceFlags & SCE_KERNEL_O_APPEND) f |= _O_APPEND;
    if (sceFlags & SCE_KERNEL_O_CREAT) f |= _O_CREAT;
    if (sceFlags & SCE_KERNEL_O_TRUNC) f |= _O_TRUNC;
    if (sceFlags & SCE_KERNEL_O_EXCL) f |= _O_EXCL;
    f |= _O_BINARY;
    return f;
}
#else
#include <fcntl.h>
#include <unistd.h>
static int NativeOpen(const std::filesystem::path& p, int nativeFlags, std::uint16_t mode) {
    return ::open(p.c_str(), nativeFlags, static_cast<mode_t>(mode));
}
static std::int64_t NativeLseek(int fd, std::int64_t offset, int whence) {
    return ::lseek(fd, static_cast<off_t>(offset), whence);
}
static std::int64_t NativeRead(int fd, void* buf, std::size_t n) {
    return ::read(fd, buf, n);
}
static std::int64_t NativeWrite(int fd, const void* buf, std::size_t n) {
    return ::write(fd, buf, n);
}
static int NativeClose(int fd) { return ::close(fd); }
static int NativeUnlink(const std::filesystem::path& p) {
    return ::unlink(p.c_str());
}
static int MapFlags(int sceFlags) {
    int f = 0;
    const int acc = sceFlags & SCE_KERNEL_O_ACCMODE;
    if (acc == SCE_KERNEL_O_RDONLY) f |= O_RDONLY;
    else if (acc == SCE_KERNEL_O_WRONLY) f |= O_WRONLY;
    else if (acc == SCE_KERNEL_O_RDWR) f |= O_RDWR;
    else throw std::invalid_argument("sceKernelOpen: invalid access mode");
    if (sceFlags & SCE_KERNEL_O_APPEND) f |= O_APPEND;
    if (sceFlags & SCE_KERNEL_O_CREAT) f |= O_CREAT;
    if (sceFlags & SCE_KERNEL_O_TRUNC) f |= O_TRUNC;
    if (sceFlags & SCE_KERNEL_O_EXCL) f |= O_EXCL;
    if (sceFlags & SCE_KERNEL_O_SYNC) f |= O_SYNC;
    if (sceFlags & SCE_KERNEL_O_DIRECTORY) f |= O_DIRECTORY;
    return f;
}
#endif

static int SceErrorFromErrno(int error) {
    switch (error) {
        case EPERM: return SCE_KERNEL_ERROR_EPERM;
        case ENOENT: return SCE_KERNEL_ERROR_ENOENT;
        case ESRCH: return SCE_KERNEL_ERROR_ESRCH;
        case EINTR: return SCE_KERNEL_ERROR_EINTR;
        case EIO: return SCE_KERNEL_ERROR_EIO;
        case ENXIO: return SCE_KERNEL_ERROR_ENXIO;
        case E2BIG: return SCE_KERNEL_ERROR_E2BIG;
        case ENOEXEC: return SCE_KERNEL_ERROR_ENOEXEC;
        case EBADF: return SCE_KERNEL_ERROR_EBADF;
        case ECHILD: return SCE_KERNEL_ERROR_ECHILD;
        case EDEADLK: return SCE_KERNEL_ERROR_EDEADLK;
        case EBUSY: return SCE_KERNEL_ERROR_EBUSY;
        case EXDEV: return SCE_KERNEL_ERROR_EXDEV;
        case ENODEV: return SCE_KERNEL_ERROR_ENODEV;
        case EACCES: return SCE_KERNEL_ERROR_EACCES;
        case EFAULT: return SCE_KERNEL_ERROR_EFAULT;
        case EEXIST: return SCE_KERNEL_ERROR_EEXIST;
        case ENOTDIR: return SCE_KERNEL_ERROR_ENOTDIR;
        case EISDIR: return SCE_KERNEL_ERROR_EISDIR;
        case EINVAL: return SCE_KERNEL_ERROR_EINVAL;
        case ENFILE: return SCE_KERNEL_ERROR_ENFILE;
        case EMFILE: return SCE_KERNEL_ERROR_EMFILE;
        case ENOTTY: return SCE_KERNEL_ERROR_ENOTTY;
        case ETXTBSY: return SCE_KERNEL_ERROR_ETXTBSY;
        case ENOSPC: return SCE_KERNEL_ERROR_ENOSPC;
        case ESPIPE: return SCE_KERNEL_ERROR_ESPIPE;
        case EROFS: return SCE_KERNEL_ERROR_EROFS;
        case EFBIG: return SCE_KERNEL_ERROR_EFBIG;
        case EMLINK: return SCE_KERNEL_ERROR_EMLINK;
        case EPIPE: return SCE_KERNEL_ERROR_EPIPE;
        case EDOM: return SCE_KERNEL_ERROR_EDOM;
        case ERANGE: return SCE_KERNEL_ERROR_ERANGE;
        case EOVERFLOW: return SCE_KERNEL_ERROR_EOVERFLOW;
        case EAGAIN: return SCE_KERNEL_ERROR_EAGAIN;
        case ENOMEM: return SCE_KERNEL_ERROR_ENOMEM;
        case ENOSYS: return SCE_KERNEL_ERROR_ENOSYS;
        case ENAMETOOLONG: return SCE_KERNEL_ERROR_ENAMETOOLONG;
        case ENOTEMPTY: return SCE_KERNEL_ERROR_ENOTEMPTY;
        case ELOOP: return SCE_KERNEL_ERROR_ELOOP;
        default: return SCE_KERNEL_ERROR_EIO;
    }
}

extern "C" {

int APS5_VABI sceKernelOpen(const char* path, int flags, std::uint16_t mode) {
    APS5_LOG_OUT("path=%s flags=0x%X nativeFlags=0x%X mode=0%o", path, flags, MapFlags(flags), mode);
    auto native = ResolvePath_nid_no_patch(path);
    int fd = NativeOpen(native, MapFlags(flags), mode);
#ifdef _WIN32
    if (fd < 0 && errno != ENOENT) {
        std::error_code error;
        if (std::filesystem::is_directory(native, error)) fd = File::OpenDirectoryDescriptor(native);
    }
#endif
    if (fd < 0) {
        return SceErrorFromErrno(errno);
    }
    if ((flags & SCE_KERNEL_O_ACCMODE) != SCE_KERNEL_O_RDONLY || (flags & (SCE_KERNEL_O_CREAT | SCE_KERNEL_O_TRUNC)))
        RecordWrittenPath_nid_no_patch(native);
    return fd;
}

int APS5_VABI sceKernelClose(int d) {
#ifdef _WIN32
    File::ForgetDirectoryDescriptor(d);
#endif
    if (NativeClose(d) != 0) {
        if (errno == EBADF) return SCE_KERNEL_ERROR_EBADF;
        throw std::runtime_error(std::string(__func__) + ": close failed, fd=" + std::to_string(d) + ", errno=" + std::to_string(errno));
    }
    return 0;
}

std::int64_t APS5_VABI sceKernelRead(int d, void* buf, std::size_t nbytes) {
    if (buf == nullptr && nbytes != 0) return SceErrorFromErrno(EFAULT);
    char empty = 0;
    if (buf == nullptr) buf = &empty;
    const GuestArena::HostWrite destination(buf, nbytes);
    if (nbytes != 0 && !destination.Open()) return SceErrorFromErrno(EFAULT);
    const auto result = NativeRead(d, buf, nbytes);
    return result < 0 ? SceErrorFromErrno(errno) : static_cast<std::int64_t>(result);
}

std::int64_t APS5_VABI sceKernelWrite(int d, const void* buf, std::size_t nbytes) {
    if (buf == nullptr && nbytes != 0) return SceErrorFromErrno(EFAULT);
    char empty = 0;
    if (buf == nullptr) buf = &empty;
    const auto result = NativeWrite(d, buf, nbytes);
    return result < 0 ? SceErrorFromErrno(errno) : static_cast<std::int64_t>(result);
}

std::int64_t APS5_VABI sceKernelLseek(int d, std::int64_t offset, int whence) {
    if (whence < 0 || whence > 4) return SceErrorFromErrno(EINVAL);
    if (whence == 3 || whence == 4) NotImplemented_nid_no_patch(__func__);
    std::int64_t result = NativeLseek(d, offset, whence);
    return result < 0 ? SceErrorFromErrno(errno) : result;
}

int APS5_VABI sceKernelStat(const char* path, FileStat* sb) {
    if (path == nullptr) {
        throw std::invalid_argument(std::string(__func__) + ": path is null");
    }
    if (sb == nullptr) {
        throw std::invalid_argument(std::string(__func__) + ": sb is null");
    }
    const auto native = ResolvePath_nid_no_patch(path);
    std::error_code error;
    if (!std::filesystem::exists(native, error)) {
        return SceErrorFromErrno(2);
    }
    File::FillFileStat(native, sb);
    return 0;
}

int APS5_VABI sceKernelUnlink(const char* path) {
    if (path == nullptr) {
        throw std::invalid_argument(std::string(__func__) + ": path is null");
    }
    auto native = ResolvePath_nid_no_patch(path);
    if (NativeUnlink(native) != 0) {
        return SceErrorFromErrno(errno);
    }
    RecordWrittenPath_nid_no_patch(native);
    return 0;
}

int APS5_VABI sceKernelFcntl() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
