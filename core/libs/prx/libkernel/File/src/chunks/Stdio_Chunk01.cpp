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
static int NativeRmdir(const std::filesystem::path& path) {
    std::error_code error;
    const auto verbatim = NativeVerbatimPath_nid_no_patch(path, error, WindowsMaxDirectoryPathLength);
    const auto& target = error ? path : verbatim;
    if (::RemoveDirectoryW(target.c_str())) return 0;
    errno = WindowsOpenError_nid_no_patch(GetLastError());
    return -1;
}
static bool IsDirectoryEmpty(const std::filesystem::path& path) {
    if (path.empty()) return true;
    const auto search = (path.native().back() == L'\\' || path.native().back() == L'/') ? (path.native() + L"*") : (path.native() + L"\\*");
    WIN32_FIND_DATAW data{};
    const HANDLE handle = ::FindFirstFileW(search.c_str(), &data);
    if (handle == INVALID_HANDLE_VALUE) return true;
    bool empty = true;
    do {
        const auto* name = data.cFileName;
        if (name[0] == L'.' && (name[1] == L'\0' || (name[1] == L'.' && name[2] == L'\0'))) continue;
        empty = false;
        break;
    } while (::FindNextFileW(handle, &data));
    ::FindClose(handle);
    return empty;
}
static bool RemoveEmptyDirectory(const std::filesystem::path& path) {
    std::error_code pathError;
    const auto verbatim = NativeVerbatimPath_nid_no_patch(path, pathError, WindowsMaxDirectoryPathLength);
    const auto& target = pathError ? path : verbatim;
    const DWORD attributes = ::GetFileAttributesW(target.c_str());
    const bool unlocked = attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_READONLY) &&
        ::SetFileAttributesW(target.c_str(), attributes & ~FILE_ATTRIBUTE_READONLY);
    std::error_code error;
    const bool removed = ::RemoveDirectoryW(target.c_str()) != 0 || std::filesystem::remove(target, error);
    if (!removed && unlocked) ::SetFileAttributesW(target.c_str(), attributes);
    return removed;
}
static int NativeMkdir(const std::filesystem::path& path, std::uint16_t mode) {
    (void)mode;
    std::error_code error;
    const auto verbatim = NativeVerbatimPath_nid_no_patch(path, error, WindowsMaxDirectoryPathLength);
    const auto& target = error ? path : verbatim;
    if (::CreateDirectoryW(target.c_str(), nullptr)) return 0;
    errno = WindowsOpenError_nid_no_patch(GetLastError());
    return -1;
}
static std::optional<std::filesystem::path> HandlePath(HANDLE handle) {
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
std::optional<std::filesystem::path> File::WindowsDescriptorPath_nid_no_patch(int descriptor) {
    const auto handle = WindowsDescriptorHandle_nid_no_patch(descriptor);
    if (handle == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        return std::nullopt;
    }
    return HandlePath(handle);
}
static int NativeChmod(const std::filesystem::path& path, int mode) {
    std::error_code pathError;
    const auto verbatim = NativeVerbatimPath_nid_no_patch(path, pathError);
    const auto& checkPath = pathError ? path : verbatim;
    const DWORD attributes = GetFileAttributesW(checkPath.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) {
        RecordWrittenPath_nid_no_patch(path);
        return ::_wchmod(checkPath.wstring().c_str(), mode);
    }
    const HANDLE handle = CreateFileW(checkPath.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        errno = WindowsOpenError_nid_no_patch(GetLastError());
        return -1;
    }
    const auto target = HandlePath(handle);
    CloseHandle(handle);
    if (!target) return -1;
    RecordWrittenPath_nid_no_patch(*target);
    const auto verbatimTarget = NativeVerbatimPath_nid_no_patch(*target, pathError);
    const auto& finalTarget = pathError ? *target : verbatimTarget;
    return ::_wchmod(finalTarget.wstring().c_str(), mode);
}
static int NativeFchmod(int descriptor, int mode) {
    const auto path = File::WindowsDescriptorPath_nid_no_patch(descriptor);
    return path ? NativeChmod(*path, mode) : -1;
}
static bool PrepareSparseExtension(HANDLE handle, std::int64_t length) {
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(handle, &size)) return true;
    if (length <= size.QuadPart) return true;
    DWORD flags = 0;
    if (!GetVolumeInformationByHandleW(handle, nullptr, 0, nullptr, nullptr, &flags, nullptr, 0)) return true;
    if ((flags & FILE_SUPPORTS_SPARSE_FILES) == 0) return true;
    DWORD returned = 0;
    return DeviceIoControl(handle, FSCTL_SET_SPARSE, nullptr, 0, nullptr, 0, &returned, nullptr);
}
static int TruncateThroughWriter(HANDLE handle, std::int64_t length) {
    const auto writer = ReOpenFile(handle, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0);
    if (writer == INVALID_HANDLE_VALUE) return WindowsOpenError_nid_no_patch(GetLastError());
    FILE_END_OF_FILE_INFO end{};
    end.EndOfFile.QuadPart = length;
    const bool truncated = PrepareSparseExtension(writer, length) && SetFileInformationByHandle(writer, FileEndOfFileInfo, &end, sizeof(end));
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
    if (PrepareSparseExtension(handle, length) && ::SetFileInformationByHandle(handle, FileEndOfFileInfo, &end, sizeof(end))) return 0;
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
    std::error_code error;
    const auto verbatim = NativeVerbatimPath_nid_no_patch(path, error);
    const auto& target = error ? path : verbatim;
    const int descriptor = ::_wopen(target.c_str(), _O_WRONLY | _O_BINARY);
    if (descriptor < 0) return errno;
    const int err = NativeFtruncate(descriptor, length);
    ::_close(descriptor);
    return err;
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
    std::error_code error;
    const auto verbatim = NativeVerbatimPath_nid_no_patch(path, error);
    const auto& target = error ? path : verbatim;
    const auto handle = CreateFileW(target.c_str(), FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | (followLink ? 0 : FILE_FLAG_OPEN_REPARSE_POINT), nullptr);
    if (handle == INVALID_HANDLE_VALUE) return File::WindowsFileTime::Failure(GetLastError());
    const int result = SetTimes(handle, times);
    CloseHandle(handle);
    if (result == 0) RecordWrittenPath_nid_no_patch(path);
    return result;
}
static int NativeFutimes(int descriptor, const KernelTimeval* times) {
    if (const auto directory = File::DirectoryDescriptorPath(descriptor)) return NativeUtimes(*directory, times);

    const auto original = WindowsDescriptorHandle_nid_no_patch(descriptor);
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
        if (pipe == INVALID_HANDLE_VALUE) { errno = EBADF; return -1; }
        if (GetFileType(pipe) == FILE_TYPE_PIPE) { errno = EINVAL; return -1; }
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
File::PositionedTransferLock File::LockPositionedTransfer(int descriptor) {
    std::shared_ptr<PositionedLock> entry;
    {
        std::lock_guard table(g_positionedTableLock);
        auto& slot = g_positionedLocks[descriptor];
        if (!slot) slot = std::make_shared<PositionedLock>();
        entry = slot;
    }
    return PositionedTransferLock(std::shared_ptr<std::mutex>(entry, &entry->mutex));
}
void File::SharePositionedTransfer(int from, int to) {
    std::lock_guard table(g_positionedTableLock);
    auto& source = g_positionedLocks[from];
    if (!source) source = std::make_shared<PositionedLock>();
    g_positionedLocks[to] = source;
}
bool File::LimitSharedMemoryWrite_nid_no_patch(int descriptor, std::size_t& size, std::int64_t offset) {
    if (offset < 0) { errno = EINVAL; return false; }
    const auto length = WithoutParameterHandler([descriptor] { return ::_filelengthi64(descriptor); });
    if (length < 0) return false;
    size = offset >= length ? 0 : static_cast<std::size_t>(std::min<std::uint64_t>(size, static_cast<std::uint64_t>(length - offset)));
    return true;
}

static std::int64_t NativePositionedTransfer(int descriptor, void* buf, std::size_t nbytes, std::int64_t offset, bool write) {
    if (nbytes > static_cast<std::size_t>(std::numeric_limits<DWORD>::max())) {
        throw std::runtime_error("NativePositioned: nbytes exceeds platform limit");
    }
    if (write) {
        if (File::HasFixedSizeWrites(descriptor)) {
            if (!File::LimitSharedMemoryWrite_nid_no_patch(descriptor, nbytes, offset)) return -1;
            if (nbytes == 0) return 0;
        }
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
        if (File::HasFixedSizeWrites(descriptor)) {
            const auto position = WithoutParameterHandler([descriptor] { return ::_lseeki64(descriptor, 0, SEEK_CUR); });
            if (position < 0 || !File::LimitSharedMemoryWrite_nid_no_patch(descriptor, nbytes, position)) return -1;
            if (nbytes == 0) return 0;
        }
        const auto result = File::WriteSharedMemory_nid_no_patch(descriptor, buf, nbytes);
        if (result != -2) return result;
        if (File::Appends(descriptor)) {
            const auto appended = File::WriteAtEnd(descriptor, buf, nbytes);
            if (appended != -2) return appended;
        }
    }
    if (WindowsDescriptorHandle_nid_no_patch(descriptor) == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        return -1;
    }
    if (!write) return ::_read(descriptor, buf, static_cast<unsigned int>(nbytes));
    const int result = ::_write(descriptor, buf, static_cast<unsigned int>(nbytes));
    if (result < 0 && (_doserrno == ERROR_NO_DATA || _doserrno == ERROR_BROKEN_PIPE) && ::GetFileType(reinterpret_cast<HANDLE>(::_get_osfhandle(descriptor))) == FILE_TYPE_PIPE) errno = EPIPE;
    return result;
}
static std::int64_t NativeTransfer(int descriptor, void* buf, std::size_t nbytes, bool write) {
    const auto positioned = File::LockPositionedTransfer(descriptor);
    return NativeTransferBuffer(descriptor, buf, nbytes, write);
}
static bool NativeIsDisk(int descriptor) {
    const auto handle = WindowsDescriptorHandle_nid_no_patch(descriptor);
    return handle != INVALID_HANDLE_VALUE && ::GetFileType(handle) == FILE_TYPE_DISK;
}
static bool NativePipeHasData(int descriptor) {
    const auto handle = reinterpret_cast<HANDLE>(::_get_osfhandle(descriptor));
    DWORD available = 0;
    return handle != INVALID_HANDLE_VALUE && ::GetFileType(handle) == FILE_TYPE_PIPE
        && ::PeekNamedPipe(handle, nullptr, 0, nullptr, &available, nullptr) && available != 0;
}
}
