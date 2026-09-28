#include <cstdint>
#include <cstddef>
#include <limits>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/File/include/NativeStat.hpp"
#include "prx/libkernel/Socket/include/SocketRuntime.hpp"
#include <cerrno>
#include <cstring>
#include <cstdarg>
#include <filesystem>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <direct.h>
#include <sys/stat.h>
static int NativeRmdir(const std::filesystem::path& path) {
    return ::_wrmdir(path.wstring().c_str());
}
static int NativeMkdir(const std::filesystem::path& path, std::uint16_t mode) {
    (void)mode;
    return ::_wmkdir(path.wstring().c_str());
}
static int NativeChmod(const std::filesystem::path& path, int mode) {
    return ::_wchmod(path.wstring().c_str(), mode);
}
static int NativeFtruncate(int descriptor, std::int64_t length) {
    return static_cast<int>(::_chsize_s(descriptor, length));
}
static int NativeFlock(int descriptor, int operation) {
    HANDLE handle = reinterpret_cast<HANDLE>(::_get_osfhandle(descriptor));
    if (handle == INVALID_HANDLE_VALUE) {
        return -1;
    }
    OVERLAPPED overlapped{};
    if (operation & 8) {
        return ::UnlockFileEx(handle, 0, MAXDWORD, MAXDWORD, &overlapped) ? 0 : -1;
    }
    DWORD flags = 0;
    if (operation & 2) flags |= LOCKFILE_EXCLUSIVE_LOCK;
    if (operation & 4) flags |= LOCKFILE_FAIL_IMMEDIATELY;
    return ::LockFileEx(handle, flags, 0, MAXDWORD, MAXDWORD, &overlapped) ? 0 : -1;
}
static std::int64_t NativePread(int descriptor, void* buf, std::size_t nbytes, std::int64_t offset) {
    if (nbytes > static_cast<std::size_t>(std::numeric_limits<unsigned int>::max())) {
        throw std::runtime_error("NativePread: nbytes exceeds platform limit");
    }
    std::int64_t saved = ::_lseeki64(descriptor, 0, SEEK_CUR);
    if (saved < 0) {
        return -1;
    }
    if (::_lseeki64(descriptor, offset, SEEK_SET) < 0) {
        return -1;
    }
    int n = ::_read(descriptor, buf, static_cast<unsigned int>(nbytes));
    ::_lseeki64(descriptor, saved, SEEK_SET);
    return n;
}
static std::int64_t NativePwrite(int descriptor, const void* buf, std::size_t nbytes, std::int64_t offset) {
    if (nbytes > static_cast<std::size_t>(std::numeric_limits<unsigned int>::max())) {
        throw std::runtime_error("NativePwrite: nbytes exceeds platform limit");
    }
    std::int64_t saved = ::_lseeki64(descriptor, 0, SEEK_CUR);
    if (saved < 0) {
        return -1;
    }
    if (::_lseeki64(descriptor, offset, SEEK_SET) < 0) {
        return -1;
    }
    int n = ::_write(descriptor, buf, static_cast<unsigned int>(nbytes));
    ::_lseeki64(descriptor, saved, SEEK_SET);
    return n;
}
#else
#include <unistd.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <dirent.h>
#include <sys/syscall.h>
#include <vector>
static int NativeRmdir(const std::filesystem::path& path) {
    return ::rmdir(path.c_str());
}
static int NativeMkdir(const std::filesystem::path& path, std::uint16_t mode) {
    return ::mkdir(path.c_str(), static_cast<mode_t>(mode));
}
static int NativeChmod(const std::filesystem::path& path, int mode) {
    return ::chmod(path.c_str(), static_cast<mode_t>(mode));
}
static int NativeFtruncate(int descriptor, std::int64_t length) {
    return ::ftruncate(descriptor, static_cast<off_t>(length));
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
#endif

static constexpr int GUEST_ENOENT = 2;
static constexpr int GUEST_EIO = 5;
static constexpr int GUEST_EFAULT = 14;
static constexpr int GUEST_EEXIST = 17;
static constexpr int GUEST_EINVAL = 22;
static constexpr int GUEST_ENAMETOOLONG = 63;
static constexpr int GUEST_ENOTDIR = 20;
static constexpr int GUEST_ENOTEMPTY = 66;

static int SceErrorFromErrno(int error) {
    return static_cast<int>(0x80020000u | static_cast<unsigned>(error > 0 && error <= 34 ? error : error == GUEST_ENOTEMPTY ? error : GUEST_EIO));
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
#ifdef _WIN32
    return _close(d);
#else
    return ::close(d);
#endif
}

int APS5_VABI _close_nid_postfix(int descriptor) {
    return close_nid_postfix(descriptor);
}

int APS5_VABI flock_nid_postfix(int d, int operation) {
    if (NativeFlock(d, operation) != 0) {
#ifdef _WIN32
        throw std::runtime_error(std::string(__func__) + ": flock failed, fd=" + std::to_string(d) + ", error=" + std::to_string(::GetLastError()));
#else
        throw std::runtime_error(std::string(__func__) + ": flock failed, fd=" + std::to_string(d) + ", errno=" + std::to_string(errno));
#endif
    }
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
        APS5_INVALID_ARG_EX;
    }
#ifdef _WIN32
    int error = NativeFtruncate(d, length);
    if (error != 0) {
        throw std::runtime_error(std::string(__func__) + ": ftruncate failed, fd=" + std::to_string(d) + ", error=" + std::to_string(error));
    }
#else
    if (NativeFtruncate(d, length) != 0) {
        throw std::runtime_error(std::string(__func__) + ": ftruncate failed, fd=" + std::to_string(d) + ", errno=" + std::to_string(errno));
    }
#endif
    return 0;
}

int64_t APS5_VABI lseek_nid_postfix(int d, int64_t offset, int whence) {
    return static_cast<int64_t>(sceKernelLseek(d, offset, whence));
}

int APS5_VABI mkdir_nid_postfix(const char* path, uint16_t mode) {
    if (path == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    auto native = ResolvePath_nid_no_patch(path);
    if (NativeMkdir(native, mode) != 0) {
        throw std::runtime_error(std::string(__func__) + ": mkdir failed for " + native.string() + ", errno=" + std::to_string(errno));
    }
    return 0;
}

int APS5_VABI open_nid_postfix(const char* path, int flags, int mode) {
    return sceKernelOpen(path, flags, static_cast<std::uint16_t>(mode));
}

int APS5_VABI _open_nid_postfix(const char* path, int flags, ...) {
    std::uint16_t mode = 0;
    if (flags & SCE_KERNEL_O_CREAT) {
#ifdef _WIN32
        __builtin_sysv_va_list arguments;
        __builtin_sysv_va_start(arguments, flags);
        mode = static_cast<std::uint16_t>(__builtin_va_arg(arguments, int));
        __builtin_sysv_va_end(arguments);
#else
        std::va_list arguments;
        va_start(arguments, flags);
        mode = static_cast<std::uint16_t>(va_arg(arguments, int));
        va_end(arguments);
#endif
    }
    return sceKernelOpen(path, flags, mode);
}

int64_t APS5_VABI pread_nid_postfix(int d, void* buf, size_t nbytes, int64_t offset) {
    if (buf == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    if (offset < 0) {
        APS5_INVALID_ARG_EX;
    }
    auto n = NativePread(d, buf, nbytes, offset);
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
    return sceKernelRead(d, buf, static_cast<size_t>(nbytes));
}

std::int64_t APS5_VABI _read_nid_postfix(int descriptor, void* buffer, std::size_t count) {
    return sceKernelRead(descriptor, buffer, count);
}

int64_t APS5_VABI write_nid_postfix(int d, const char* str, int64_t size) {
    if (size < 0) {
        APS5_INVALID_ARG_EX;
    }
    return sceKernelWrite(d, str, static_cast<size_t>(size));
}

std::int64_t APS5_VABI _write_nid_postfix(int descriptor, const void* buffer, std::size_t count) {
    return sceKernelWrite(descriptor, buffer, count);
}

int APS5_VABI stat_nid_postfix(const char* path, FileStat* sb) {
    return sceKernelStat(path, sb);
}

int APS5_VABI unlink_nid_postfix(const char* path) {
    return sceKernelUnlink(path);
}

int APS5_VABI sceKernelCheckReachability(const char* path) {
 (void)path;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelFstat(int d, FileStat* sb) {
    if (sb == nullptr) throw std::invalid_argument("sceKernelFstat: sb is null");
    if (!File::FillFileStatFromDescriptor(d, sb)) return SceErrorFromErrno(errno);
    return 0;
}

int APS5_VABI sceKernelFsync(int fd) {
 (void)fd;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

#ifdef _WIN32

int APS5_VABI sceKernelGetdents(int fd, char* buf, int nbytes) {
 (void)fd;
 (void)buf;
 (void)nbytes;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelGetdirentries(int fd, char* buf, int nbytes, int64_t* basep) {
 (void)fd;
 (void)buf;
 (void)nbytes;
 (void)basep;
 NotImplemented_nid_no_patch(__func__);
 return 0;
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

int APS5_VABI sceKernelMkdir(const char* path, uint16_t mode) {
    (void)mode;
    if (path == nullptr) throw std::invalid_argument("sceKernelMkdir: path is null");
    const auto native = ResolvePath_nid_no_patch(path);
    std::error_code error;
    if (std::filesystem::exists(native, error)) return SceErrorFromErrno(GUEST_EEXIST);
    if (!std::filesystem::exists(native.parent_path(), error)) return SceErrorFromErrno(GUEST_ENOENT);
    if (!std::filesystem::create_directory(native, error)) return SceErrorFromErrno(error.value() ? error.value() : GUEST_EIO);
    return 0;
}

#ifdef _WIN32

int64_t APS5_VABI sceKernelPread(int d, void* buf, size_t nbytes, int64_t offset) {
 (void)d;
 (void)buf;
 (void)nbytes;
 (void)offset;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int64_t APS5_VABI sceKernelPwrite(int d, const void* buf, size_t nbytes, int64_t offset) {
 (void)d;
 (void)buf;
 (void)nbytes;
 (void)offset;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

#else

int64_t APS5_VABI sceKernelPread(int d, void* buf, size_t nbytes, int64_t offset) {
    if (buf == nullptr && nbytes != 0) return SceErrorFromErrno(GUEST_EFAULT);
    if (offset < 0) return SceErrorFromErrno(GUEST_EINVAL);
    const auto result = NativePread(d, buf, nbytes, offset);
    return result < 0 ? SceErrorFromErrno(errno) : result;
}

int64_t APS5_VABI sceKernelPwrite(int d, const void* buf, size_t nbytes, int64_t offset) {
    if (buf == nullptr && nbytes != 0) return SceErrorFromErrno(GUEST_EFAULT);
    if (offset < 0) return SceErrorFromErrno(GUEST_EINVAL);
    const auto result = NativePwrite(d, buf, nbytes, offset);
    return result < 0 ? SceErrorFromErrno(errno) : result;
}

#endif

int APS5_VABI sceKernelRename(const char* from, const char* to) {
    if (from == nullptr || to == nullptr) throw std::invalid_argument("sceKernelRename: path is null");
    const auto source = ResolvePath_nid_no_patch(from);
    std::error_code error;
    if (!std::filesystem::exists(source, error)) return SceErrorFromErrno(GUEST_ENOENT);
    std::filesystem::rename(source, ResolvePath_nid_no_patch(to), error);
    if (error) return SceErrorFromErrno(GUEST_EIO);
    return 0;
}

int APS5_VABI sceKernelRmdir(const char* path) {
    if (path == nullptr) throw std::invalid_argument("sceKernelRmdir: path is null");
    const auto native = ResolvePath_nid_no_patch(path);
    std::error_code error;
    if (!std::filesystem::is_directory(native, error)) return SceErrorFromErrno(std::filesystem::exists(native, error) ? GUEST_ENOTDIR : GUEST_ENOENT);
    if (!std::filesystem::is_empty(native, error)) return SceErrorFromErrno(GUEST_ENOTEMPTY);
    if (!std::filesystem::remove(native, error)) return SceErrorFromErrno(GUEST_EIO);
    return 0;
}

int APS5_VABI rmdir_nid_postfix(const char* path) {
    return sceKernelRmdir(path);
}

}

extern "C" {

int APS5_VABI sceKernelChmod_nid_postfix(const char* path, std::uint16_t mode) {
    (void)path;
    (void)mode;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelTruncate_nid_postfix(const char* path, std::int64_t length) {
    (void)path;
    (void)length;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceKernelUtimes_nid_postfix(const char* path, const KernelTimeval* times) {
    (void)path;
    (void)times;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
