#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/File/include/NativeStat.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/FilesystemError.hpp"
#include "prx/libc/include/RenameDot.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libkernel/DirectMemory/DirectMemory.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/SharedMemory.hpp"
#include "prx/libkernel/File/include/DirectoryDescriptor.hpp"
#include "prx/libkernel/File/include/DescriptorControl.hpp"
#include "prx/libkernel/File/include/FileLock.hpp"
#include "prx/libkernel/Socket/include/SocketRuntime.hpp"
#include "prx/libkernel/File/include/RandomDevice.hpp"
#include "prx/libkernel/File/include/HostStreams.hpp"
#include "prx/libkernel/File/include/ArchivedFile.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "SceTypes.hpp"

#include <cerrno>
#include <cstdarg>
#include <limits>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#include <windows.h>
#include "prx/libkernel/File/include/WindowsSymbolicLink.hpp"
static int NativeOpen(const std::filesystem::path& p, int nativeFlags, std::uint16_t mode) {
    const int access = nativeFlags & (_O_RDONLY | _O_WRONLY | _O_RDWR);
    const DWORD desired = access == _O_RDONLY ? GENERIC_READ : access == _O_WRONLY ? GENERIC_WRITE : GENERIC_READ | GENERIC_WRITE;
    DWORD disposition = OPEN_EXISTING;
    if (nativeFlags & _O_CREAT) disposition = (nativeFlags & _O_EXCL) ? CREATE_NEW : (nativeFlags & _O_TRUNC) ? CREATE_ALWAYS : OPEN_ALWAYS;
    else if (nativeFlags & _O_TRUNC) disposition = TRUNCATE_EXISTING;
    const DWORD attributes = (nativeFlags & _O_CREAT) && !(mode & _S_IWRITE) ? FILE_ATTRIBUTE_READONLY : FILE_ATTRIBUTE_NORMAL;
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    const auto handle = CreateFileW(p.c_str(), desired, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, &security, disposition, attributes, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        errno = WindowsOpenError_nid_no_patch(GetLastError());
        return -1;
    }
    const int descriptor = ::_open_osfhandle(reinterpret_cast<std::intptr_t>(handle), nativeFlags & (_O_APPEND | _O_RDONLY | _O_BINARY));
    if (descriptor < 0) CloseHandle(handle);
    return descriptor;
}
static std::int64_t NativeLseek(int fd, std::int64_t offset, int whence) {
    const auto positioned = File::LockPositionedTransfer(fd);
    const auto result = File::SeekSharedMemory_nid_no_patch(fd, offset, whence);
    if (result != -2) return result;
    const auto position = ::_lseeki64(fd, offset, whence);
    if (position < 0 || GetFileType(reinterpret_cast<HANDLE>(::_get_osfhandle(fd))) != FILE_TYPE_PIPE) return position;
    errno = ESPIPE;
    return -1;
}
static int NativeRead(int fd, void* buf, std::size_t n) {
    if (n > static_cast<std::size_t>(std::numeric_limits<unsigned int>::max())) {
        throw std::runtime_error("sceKernelRead: nbytes exceeds platform limit");
    }
    if (File::IsDirectoryDescriptor(fd)) {
        errno = EISDIR;
        return -1;
    }
    const auto positioned = File::LockPositionedTransfer(fd);
    const int result = ::_read(fd, buf, static_cast<unsigned int>(n));
    if (result < 0 && _doserrno == ERROR_NO_DATA) errno = EAGAIN;
    return result;
}
static int NativeWrite(int fd, const void* buf, std::size_t n) {
    if (n > static_cast<std::size_t>(std::numeric_limits<unsigned int>::max())) {
        throw std::runtime_error("sceKernelWrite: nbytes exceeds platform limit");
    }
    const auto positioned = File::LockPositionedTransfer(fd);
    const auto shared = File::WriteSharedMemory_nid_no_patch(fd, buf, n);
    if (shared != -2) return static_cast<int>(shared);
    if (File::Appends(fd)) {
        const auto appended = File::WriteAtEnd(fd, buf, n);
        if (appended != -2) return static_cast<int>(appended);
    }
    const int result = ::_write(fd, buf, static_cast<unsigned int>(n));
    if (result < 0 && errno == ENOSPC && _doserrno == 0 && ::GetFileType(reinterpret_cast<HANDLE>(::_get_osfhandle(fd))) == FILE_TYPE_PIPE) errno = EAGAIN;
    return result;
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
    const auto path = p.wstring();
    struct _stat64 status{};
    const bool unlocked = ::_wstat64(path.c_str(), &status) == 0 && (status.st_mode & _S_IFMT) == _S_IFREG &&
        !(status.st_mode & _S_IWRITE) && ::_wchmod(path.c_str(), _S_IREAD | _S_IWRITE) == 0;
    if (::_wunlink(path.c_str()) == 0) return 0;
    const int error = errno;
    if (unlocked) ::_wchmod(path.c_str(), _S_IREAD);
    errno = error;
    return -1;
}
static std::filesystem::path NativeMetadataPath(const std::filesystem::path& path, std::error_code& error) {
    const auto& input = path.native();
    if (input.starts_with(L"\\\\?\\") || input.starts_with(L"\\\\.\\")) return path;
    auto absolute = std::filesystem::absolute(path, error);
    if (error) return {};
    absolute = absolute.lexically_normal().make_preferred();
    const auto& native = absolute.native();
    if (native.size() < MAX_PATH || native.starts_with(L"\\\\?\\") || native.starts_with(L"\\\\.\\")) return absolute;
    return native.starts_with(L"\\\\") ? std::filesystem::path(L"\\\\?\\UNC\\" + native.substr(2))
                                            : std::filesystem::path(L"\\\\?\\" + native);
}
static bool NativeSymbolicLinkLoop(const std::filesystem::path& path) {
    return File::WindowsSymbolicLinkLoop(path);
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
    const auto result = File::SeekSharedMemory_nid_no_patch(fd, offset, whence);
    return result == -2 ? ::lseek(fd, static_cast<off_t>(offset), whence) : result;
}
static std::int64_t NativeRead(int fd, void* buf, std::size_t n) {
    return ::read(fd, buf, n);
}
static std::int64_t NativeWrite(int fd, const void* buf, std::size_t n) {
    const auto result = File::WriteSharedMemory_nid_no_patch(fd, buf, n);
    return result == -2 ? ::write(fd, buf, n) : result;
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
extern "C" int APS5_VABI fcntl_nid_postfix(int descriptor, int command, ...);

static int SceErrorFromErrno(int error) {
    const int guest = error > 0 && error <= 34 ? error : FilesystemError(std::error_code(error, std::generic_category()));
    return static_cast<int>(0x80020000u | static_cast<unsigned>(guest));
}

static int SceStatErrorFromErrno(int error) {
#ifdef ELOOP
    if (error == ELOOP) return SCE_KERNEL_ERROR_ELOOP;
#endif
    return SceErrorFromErrno(error);
}

static int SceIoErrorFromErrno(int error) {
    int guest = SceErrorFromErrno(error) & 0xffff;
    if (error == EAGAIN) guest = 35;
#ifdef EDEADLK
    else if (error == EDEADLK) guest = 11;
#endif
#ifdef EDQUOT
    else if (error == EDQUOT) guest = 69;
#endif
#ifdef EOVERFLOW
    else if (error == EOVERFLOW) guest = 84;
#endif
    return static_cast<int>(0x80020000u | static_cast<unsigned>(guest));
}

static int OpenNative(const std::filesystem::path& native, int flags, std::uint16_t mode) {
    if (const auto archived = File::OpenArchivedFile(native, flags)) {
        if (*archived < 0) return SceErrorFromErrno(-*archived);
        File::RememberDescriptor(*archived, flags);
        return *archived;
    }
    File::RefuseMergedDirectory(native);
#ifdef _WIN32
    if ((flags & (SCE_KERNEL_O_DIRECTORY | SCE_KERNEL_O_CREAT)) == SCE_KERNEL_O_DIRECTORY) {
        std::error_code error;
        const auto status = std::filesystem::status(native, error);
        if (std::filesystem::exists(status) && !std::filesystem::is_directory(status)) return SceErrorFromErrno(ENOTDIR);
    }
#endif
    if ((flags & SCE_KERNEL_O_NOFOLLOW) != 0 && (flags & (SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL)) != (SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL)) {
        constexpr int GuestEmlink = 31;
        std::error_code error;
        if (std::filesystem::is_symlink(native, error)) return SceErrorFromErrno(GuestEmlink);
#ifdef _WIN32
        const DWORD attributes = GetFileAttributesW(native.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
            std::string junctionTarget;
            if (File::WindowsSymbolicLink::ReadJunction(native, junctionTarget, "open")) return SceErrorFromErrno(GuestEmlink);
        }
#endif
    }
    int fd = NativeOpen(native, MapFlags(flags), mode);
#ifdef _WIN32
    if (fd < 0 && (errno != ENOENT || !native.has_filename())) {
        const int openError = errno;
        auto directory = native;
        while (!directory.has_filename() && directory.has_relative_path()) directory = directory.parent_path();
        std::error_code error;
        if (std::filesystem::is_directory(directory, error)) {
            if ((flags & (SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL)) == (SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL)) errno = EEXIST;
            else if ((flags & SCE_KERNEL_O_ACCMODE) != SCE_KERNEL_O_RDONLY || (flags & (SCE_KERNEL_O_CREAT | SCE_KERNEL_O_TRUNC))) errno = EISDIR;
            else fd = File::OpenDirectoryDescriptor(directory);
        } else {
            errno = openError;
        }
    }
#endif
#ifdef _WIN32
    if (fd < 0 && (errno == ENOENT || errno == ENOTDIR)) {
        std::error_code metadataError;
        const auto metadataPath = NativeMetadataPath(native, metadataError);
        if (!metadataError && NativeSymbolicLinkLoop(metadataPath)) return SCE_KERNEL_ERROR_ELOOP;
    }
#endif
    if (fd < 0) {
        return SceErrorFromErrno(errno == ENOENT ? NotFoundError_nid_no_patch(native) : errno);
    }
    if ((flags & SCE_KERNEL_O_ACCMODE) != SCE_KERNEL_O_RDONLY || (flags & (SCE_KERNEL_O_CREAT | SCE_KERNEL_O_TRUNC)))
        RecordWrittenPath_nid_no_patch(native);
    File::RememberDescriptor(fd, flags);
    return fd;
}

extern "C" {

int APS5_VABI sceKernelOpen(const char* path, int flags, std::uint16_t mode) {
    if (File::InvalidOpenFlags(flags)) return SCE_KERNEL_ERROR_EINVAL;
    if (path != nullptr && *path == '\0') return SCE_KERNEL_ERROR_ENOENT;
    APS5_LOG_OUT("path=%s flags=0x%X nativeFlags=0x%X mode=0%o", path != nullptr ? path : "(null)", flags, MapFlags(flags), mode);
    if (path != nullptr && File::IsRandomDevicePath(path)) {
        if ((flags & SCE_KERNEL_O_ACCMODE) != SCE_KERNEL_O_RDONLY) throw std::runtime_error(std::string(__func__) + ": writing to " + path + " is not implemented");
        const int fd = File::OpenRandomDevice();
        if (fd < 0) return SceErrorFromErrno(errno);
        File::RememberDescriptor(fd, flags);
        return fd;
    }
    std::filesystem::path native;
    const bool module = (flags & (SCE_KERNEL_O_ACCMODE | SCE_KERNEL_O_CREAT)) == SCE_KERNEL_O_RDONLY;
    if (const int error = ResolvePathError_nid_no_patch(path, native, module, true)) {
        const int result = SceKernelError(error);
        APS5_LOG_ERR("open failed path=%s resolveError=%d result=0x%08X", path != nullptr ? path : "<null>", error, static_cast<unsigned>(result));
        return result;
    }
    const int result = OpenNative(native, flags, mode);
    if (result < 0) {
        const auto resolved = native.u8string();
        APS5_LOG_ERR("open failed path=%s resolvedPath=%s result=0x%08X", path != nullptr ? path : "<null>", reinterpret_cast<const char*>(resolved.c_str()), static_cast<unsigned>(result));
    }
    return result;
}

int OpenInDirectory_nid_no_patch(int directory, const char* path, int flags, std::uint16_t mode) {
    if (File::InvalidOpenFlags(flags)) return SCE_KERNEL_ERROR_EINVAL;
    int error = 0;
    const char* base = GetDirectoryDescriptorPath_nid_no_patch(directory, &error);
    if (base == nullptr) return SceErrorFromErrno(error);
    return OpenNative(std::filesystem::path(reinterpret_cast<const char8_t*>(base)) / std::filesystem::path(reinterpret_cast<const char8_t*>(path)), flags, mode);
}

int APS5_VABI sceKernelClose(int d) {
    if (File::IsHostStream(d)) return SCE_KERNEL_ERROR_EPERM;
    if (GuestSockets::IsOpen(d)) return GuestSockets::Close(d) == 0 ? 0 : SceKernelError(*__error_nid_postfix());
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
#endif
    if (NativeClose(d) != 0) {
        if (errno == EBADF) return SCE_KERNEL_ERROR_EBADF;
        throw std::runtime_error(std::string(__func__) + ": close failed, fd=" + std::to_string(d) + ", errno=" + std::to_string(errno));
    }
    return 0;
}

std::int64_t APS5_VABI sceKernelRead(int d, void* buf, std::size_t nbytes) {
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
    else if (const auto archived = File::ReadArchivedFile(d, buf, nbytes, nullptr)) return *archived;
    auto n = destination.Open() ? NativeRead(d, buf, nbytes) : -1;
    if (n < 0) return SceIoErrorFromErrno(errno);
    return static_cast<std::int64_t>(n);
}

std::int64_t APS5_VABI sceKernelWrite(int d, const void* buf, std::size_t nbytes) {
    if (GuestSockets::IsOpen(d)) {
        const auto n = GuestSockets::Write(d, buf, nbytes);
        return n < 0 ? SceKernelError(*__error_nid_postfix()) : n;
    }
    if (buf == nullptr) {
        if (nbytes != 0) return SceIoErrorFromErrno(DescriptorIsOpen_nid_no_patch(d) ? EFAULT : EBADF);
        const char empty = 0;
        return sceKernelWrite(d, &empty, 0);
    }
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
    if (GuestSockets::IsOpen(d)) return SCE_KERNEL_ERROR_ESPIPE;
    if (whence < 0 || whence > 2) {
        throw std::invalid_argument(std::string(__func__) + ": invalid whence=" + std::to_string(whence));
    }
    if (const auto archived = File::SeekArchivedFile(d, offset, whence)) return *archived < 0 ? SceIoErrorFromErrno(-*archived) : *archived;
#ifdef _WIN32
    if (const auto directory = File::SeekDirectoryDescriptor(d, offset, whence)) {
        if (*directory < 0) return SceIoErrorFromErrno(errno);
        return *directory;
    }
#endif
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
        if (sb == nullptr) return SceErrorFromErrno(GuestEfault);
        *sb = archived;
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
    if (!File::TryFillFileStat(metadataPath, sb)) return SceStatErrorFromErrno(errno);
    return 0;
}

int APS5_VABI sceKernelStat(const char* path, FileStat* sb) {
    if (path == nullptr) return SceErrorFromErrno(GuestEfault);
    if (*path == '\0') return SceErrorFromErrno(2);
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
    if (!File::FillLinkStat(native, sb)) return SceErrorFromErrno(errno);
    return 0;
}

int APS5_VABI sceKernelUnlink(const char* path) {
    if (path == nullptr) return SceErrorFromErrno(GuestEfault);
    if (*path == '\0') return SceErrorFromErrno(2);
    if (const int dot = RenameDotError_nid_no_patch(path)) return SceKernelError(dot);
    std::filesystem::path native;
    if (const int error = ResolvePathError_nid_no_patch(path, native)) return SceKernelError(error);
    constexpr int GuestEperm = 1;
    std::error_code error;
    if (std::filesystem::is_directory(std::filesystem::symlink_status(native, error))) return SceErrorFromErrno(GuestEperm);
    if (NativeUnlink(native) != 0) {
#ifdef _WIN32
        if (NativeSymbolicLinkLoop(native.parent_path())) return SCE_KERNEL_ERROR_ELOOP;
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
