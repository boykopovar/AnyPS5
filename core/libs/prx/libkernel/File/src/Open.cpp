#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/File/include/NativeStat.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/DirectoryDescriptor.hpp"
#include "prx/libkernel/File/include/FileLock.hpp"
#include "prx/libkernel/Socket/include/SocketRuntime.hpp"
#include "prx/libkernel/File/include/RandomDevice.hpp"
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
static int NativeOpen(const std::filesystem::path& p, int nativeFlags, std::uint16_t mode) {
    return ::_wopen(p.wstring().c_str(), nativeFlags, static_cast<int>(mode));
}
static std::int64_t NativeLseek(int fd, std::int64_t offset, int whence) {
    return ::_lseeki64(fd, offset, whence);
}
static int NativeRead(int fd, void* buf, std::size_t n) {
    if (n > static_cast<std::size_t>(std::numeric_limits<unsigned int>::max())) {
        throw std::runtime_error("sceKernelRead: nbytes exceeds platform limit");
    }
    return ::_read(fd, buf, static_cast<unsigned int>(n));
}
static int NativeWrite(int fd, const void* buf, std::size_t n) {
    if (n > static_cast<std::size_t>(std::numeric_limits<unsigned int>::max())) {
        throw std::runtime_error("sceKernelWrite: nbytes exceeds platform limit");
    }
    return ::_write(fd, buf, static_cast<unsigned int>(n));
}
extern "C" _invalid_parameter_handler _set_thread_local_invalid_parameter_handler(_invalid_parameter_handler);
static void IgnoreInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, std::uintptr_t) {}
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

extern "C" int* APS5_VABI __error_nid_postfix();

static int SceErrorFromErrno(int error) {
    constexpr int GuestEio = 5;
    const int guest = error > 0 && error <= 34 ? error : GuestEio;
    return static_cast<int>(0x80020000u | static_cast<unsigned>(guest));
}

extern "C" {

int APS5_VABI sceKernelOpen(const char* path, int flags, std::uint16_t mode) {
    APS5_LOG_OUT("path=%s flags=0x%X nativeFlags=0x%X mode=0%o", path, flags, MapFlags(flags), mode);
    if (path != nullptr && File::IsRandomDevicePath(path)) {
        if ((flags & SCE_KERNEL_O_ACCMODE) != SCE_KERNEL_O_RDONLY) throw std::runtime_error(std::string(__func__) + ": writing to " + path + " is not implemented");
        const int fd = File::OpenRandomDevice();
        return fd < 0 ? SceErrorFromErrno(errno) : fd;
    }
    auto native = ResolvePath_nid_no_patch(path);
    int fd = NativeOpen(native, MapFlags(flags), mode);
#ifdef _WIN32
    if (fd < 0 && errno != ENOENT) {
        std::error_code error;
        if (std::filesystem::is_directory(native, error)) {
            if ((flags & (SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL)) == (SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL)) errno = EEXIST;
            else if ((flags & SCE_KERNEL_O_ACCMODE) != SCE_KERNEL_O_RDONLY || (flags & (SCE_KERNEL_O_CREAT | SCE_KERNEL_O_TRUNC))) errno = EISDIR;
            else fd = File::OpenDirectoryDescriptor(native);
        }
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
    File::ForgetRandomDevice(d);
#ifdef _WIN32
    File::ForgetDirectoryDescriptor(d);
    File::ForgetFileLock(d);
#endif
    if (NativeClose(d) != 0) {
        if (errno == EBADF) return SCE_KERNEL_ERROR_EBADF;
        throw std::runtime_error(std::string(__func__) + ": close failed, fd=" + std::to_string(d) + ", errno=" + std::to_string(errno));
    }
    return 0;
}

std::int64_t APS5_VABI sceKernelRead(int d, void* buf, std::size_t nbytes) {
    if (d >= GuestSockets::FirstDescriptor) {
        const auto n = GuestSockets::Read(d, buf, nbytes);
        return n < 0 ? SceKernelError(*__error_nid_postfix()) : n;
    }
    if (buf == nullptr) {
        throw std::invalid_argument(std::string(__func__) + ": buf is null");
    }
    const GuestArena::HostWrite destination(buf, nbytes);
    if (!destination.Open()) errno = EFAULT;
    else if (File::ReadRandomDevice(d, buf, nbytes)) return static_cast<std::int64_t>(nbytes);
    auto n = destination.Open() ? NativeRead(d, buf, nbytes) : -1;
    if (n < 0) {
        throw std::runtime_error(std::string(__func__) + ": read failed, fd=" + std::to_string(d) + ", errno=" + std::to_string(errno));
    }
    return static_cast<std::int64_t>(n);
}

std::int64_t APS5_VABI sceKernelWrite(int d, const void* buf, std::size_t nbytes) {
    if (d >= GuestSockets::FirstDescriptor) {
        const auto n = GuestSockets::Write(d, buf, nbytes);
        return n < 0 ? SceKernelError(*__error_nid_postfix()) : n;
    }
    if (buf == nullptr) {
        throw std::invalid_argument(std::string(__func__) + ": buf is null");
    }
    auto n = NativeWrite(d, buf, nbytes);
    if (n < 0) {
        throw std::runtime_error(std::string(__func__) + ": write failed, fd=" + std::to_string(d) + ", errno=" + std::to_string(errno));
    }
    return static_cast<std::int64_t>(n);
}

std::int64_t APS5_VABI sceKernelLseek(int d, std::int64_t offset, int whence) {
    if (whence < 0 || whence > 2) {
        throw std::invalid_argument(std::string(__func__) + ": invalid whence=" + std::to_string(whence));
    }
#ifdef _WIN32
    if (const auto directory = File::SeekDirectoryDescriptor(d, offset, whence)) {
        if (*directory < 0) {
            throw std::runtime_error(std::string(__func__) + ": lseek failed, fd=" + std::to_string(d) + ", errno=" + std::to_string(errno));
        }
        return *directory;
    }
#endif
    std::int64_t result = NativeLseek(d, offset, whence);
    if (result < 0) {
        throw std::runtime_error(std::string(__func__) + ": lseek failed, fd=" + std::to_string(d) + ", errno=" + std::to_string(errno));
    }
    return result;
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
    constexpr int GuestEnotdir = 20;
    const auto status = std::filesystem::status(native, error);
    if (error == std::errc::not_a_directory) return SceErrorFromErrno(GuestEnotdir);
    if (!std::filesystem::exists(status)) {
        return SceErrorFromErrno(2);
    }
    const std::string_view guestPath(path);
    if (!guestPath.empty() && guestPath.back() == '/' && !std::filesystem::is_directory(status)) return SceErrorFromErrno(GuestEnotdir);
    File::FillFileStat(native, sb);
    return 0;
}

int APS5_VABI sceKernelUnlink(const char* path) {
    if (path == nullptr) {
        throw std::invalid_argument(std::string(__func__) + ": path is null");
    }
    auto native = ResolvePath_nid_no_patch(path);
    constexpr int GuestEperm = 1;
    std::error_code error;
    if (std::filesystem::is_directory(std::filesystem::symlink_status(native, error))) return SceErrorFromErrno(GuestEperm);
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
