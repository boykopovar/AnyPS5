#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/File/include/NativeStat.hpp"
#include "prx/libkernel/File/include/Path.hpp"
#include "prx/libkernel/File/include/File.hpp"
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
static int NativeClose(int fd) { return ::_close(fd); }
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

extern "C" {

int APS5_VABI sceKernelOpen(const char* path, int flags, std::uint16_t mode) {
    if (path == nullptr) {
        throw std::invalid_argument("sceKernelOpen: path is null");
    }
    auto native = File::ResolvePath(path);
    int fd = NativeOpen(native, MapFlags(flags), mode);
    if (fd < 0) {
        throw std::runtime_error("sceKernelOpen: failed to open " + native.string() + ", errno=" + std::to_string(errno));
    }
    return fd;
}

int APS5_VABI sceKernelClose(int d) {
    if (NativeClose(d) != 0) {
        throw std::runtime_error("sceKernelClose: close failed, fd=" + std::to_string(d) + ", errno=" + std::to_string(errno));
    }
    return 0;
}

std::int64_t APS5_VABI sceKernelRead(int d, void* buf, std::size_t nbytes) {
    if (buf == nullptr) {
        throw std::invalid_argument("sceKernelRead: buf is null");
    }
    auto n = NativeRead(d, buf, nbytes);
    if (n < 0) {
        throw std::runtime_error("sceKernelRead: read failed, fd=" + std::to_string(d) + ", errno=" + std::to_string(errno));
    }
    return static_cast<std::int64_t>(n);
}

std::int64_t APS5_VABI sceKernelWrite(int d, const void* buf, std::size_t nbytes) {
    if (buf == nullptr) {
        throw std::invalid_argument("sceKernelWrite: buf is null");
    }
    auto n = NativeWrite(d, buf, nbytes);
    if (n < 0) {
        throw std::runtime_error("sceKernelWrite: write failed, fd=" + std::to_string(d) + ", errno=" + std::to_string(errno));
    }
    return static_cast<std::int64_t>(n);
}

int APS5_VABI sceKernelLseek(int d, std::int64_t offset, int whence) {
    if (whence < 0 || whence > 2) {
        throw std::invalid_argument("sceKernelLseek: invalid whence=" + std::to_string(whence));
    }
    std::int64_t result = NativeLseek(d, offset, whence);
    if (result < 0) {
        throw std::runtime_error("sceKernelLseek: lseek failed, fd=" + std::to_string(d) + ", errno=" + std::to_string(errno));
    }
    if (result > static_cast<std::int64_t>(std::numeric_limits<int>::max())) {
        throw std::overflow_error("sceKernelLseek: result " + std::to_string(result) + " overflows int return type");
    }
    return static_cast<int>(result);
}

int APS5_VABI sceKernelStat(const char* path, FileStat* sb) {
    if (path == nullptr) {
        throw std::invalid_argument("sceKernelStat: path is null");
    }
    if (sb == nullptr) {
        throw std::invalid_argument("sceKernelStat: sb is null");
    }
    File::FillFileStat(File::ResolvePath(path), sb);
    return 0;
}

int APS5_VABI sceKernelUnlink(const char* path) {
    if (path == nullptr) {
        throw std::invalid_argument("sceKernelUnlink: path is null");
    }
    auto native = File::ResolvePath(path);
    if (NativeUnlink(native) != 0) {
        throw std::runtime_error("sceKernelUnlink: unlink failed for " + native.string() + ", errno=" + std::to_string(errno));
    }
    return 0;
}

}
