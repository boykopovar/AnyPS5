#include "prx/libkernel/File/include/NativeStat.hpp"
#include "prx/libkernel/File/include/ArchivedFile.hpp"
#include "prx/libkernel/File/include/DirectoryDescriptor.hpp"
#include "prx/libc/include/General.hpp"

#include <algorithm>
#include <cerrno>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include "prx/libkernel/File/include/WindowsFileTime.hpp"
#include "prx/libkernel/File/include/WindowsSymbolicLink.hpp"
#include "prx/libc/include/WindowsDescriptor.hpp"
#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>
constexpr unsigned short SymbolicLinkMode = 0120777;
struct NativeStat : __stat64 {
    FILE_BASIC_INFO times{};
    BY_HANDLE_FILE_INFORMATION identity{};
    bool hasTimes = false;
    bool hasIdentity = false;
    bool isSymbolicLink = false;
};
static int ReadHandleInfo(HANDLE handle, NativeStat* st) {
    if (GetFileType(handle) != FILE_TYPE_DISK) return 0;
    if (!GetFileInformationByHandleEx(handle, FileBasicInfo, &st->times, sizeof(st->times)))
        return File::WindowsFileTime::Failure(GetLastError());
    st->hasTimes = true;
    st->hasIdentity = GetFileInformationByHandle(handle, &st->identity) != 0;
    return 0;
}
static int DoStat(const std::filesystem::path& p, NativeStat* st) {
    if (_wstat64(p.c_str(), st) != 0) {
        if (_doserrno == ERROR_CANT_RESOLVE_FILENAME) File::WindowsFileTime::Failure(_doserrno);
        return -1;
    }
    const auto handle = CreateFileW(p.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return File::WindowsFileTime::Failure(GetLastError());
    const int result = ReadHandleInfo(handle, st);
    CloseHandle(handle);
    return result;
}
static int DoFstat(int fd, NativeStat* st) {
    if (WindowsDescriptorHandle_nid_no_patch(fd) == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        return -1;
    }
    if (_fstat64(fd, st) != 0) return -1;
    return ReadHandleInfo(reinterpret_cast<HANDLE>(::_get_osfhandle(fd)), st);
}
static int DoLstat(const std::filesystem::path& p, NativeStat* st) {
    const auto attributes = GetFileAttributesW(p.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) return DoStat(p, st);
    if (!File::WindowsSymbolicLink::IsNameSurrogate(p, "lstat")) return DoStat(p, st);
    std::string target;
    const bool link = File::WindowsSymbolicLink::Read(p, target, "lstat") || File::WindowsSymbolicLink::ReadJunction(p, target, "lstat");
    const auto handle = CreateFileW(p.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return File::WindowsFileTime::Failure(GetLastError());
    const int result = ReadHandleInfo(handle, st);
    CloseHandle(handle);
    if (result != 0) return result;
    if (!link) {
        st->st_mode = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ? (_S_IFDIR | 0111) : _S_IFREG;
        st->st_mode |= (attributes & FILE_ATTRIBUTE_READONLY) != 0 ? 0444 : 0666;
        return 0;
    }
    st->st_mode = SymbolicLinkMode;
    st->st_size = static_cast<__int64>(target.size());
    st->isSymbolicLink = true;
    return 0;
}
#else
#include <sys/stat.h>
using NativeStat = struct stat;
static int DoStat(const std::filesystem::path& p, NativeStat* st) {
    return ::stat(p.c_str(), st);
}
static int DoFstat(int fd, NativeStat* st) {
    return ::fstat(fd, st);
}
static int DoLstat(const std::filesystem::path& p, NativeStat* st) {
    return ::lstat(p.c_str(), st);
}
#endif

static void CopyNativeStat(const NativeStat& st, FileStat* sb) {
    *sb = FileStat{};
    sb->st_mode = static_cast<std::uint16_t>(st.st_mode);
    sb->st_size = static_cast<std::int64_t>(st.st_size);
#ifdef _WIN32
    if (st.hasIdentity && !st.isSymbolicLink) sb->st_size = static_cast<std::int64_t>((static_cast<std::uint64_t>(st.identity.nFileSizeHigh) << 32u) | st.identity.nFileSizeLow);
    sb->st_dev = static_cast<std::uint32_t>(st.st_dev);
    sb->st_ino = static_cast<std::uint32_t>(st.st_ino);
    sb->st_nlink = static_cast<std::uint16_t>(st.st_nlink);
    sb->st_uid = 0;
    sb->st_gid = 0;
    sb->st_rdev = static_cast<std::uint32_t>(st.st_rdev);
    sb->st_blksize = 512;
    sb->st_blocks = (sb->st_size + 511LL) / 512LL;
    sb->st_atim.tv_sec = static_cast<std::int64_t>(st.st_atime);
    sb->st_atim.tv_nsec = 0;
    sb->st_mtim.tv_sec = static_cast<std::int64_t>(st.st_mtime);
    sb->st_mtim.tv_nsec = 0;
    sb->st_ctim.tv_sec = static_cast<std::int64_t>(st.st_ctime);
    sb->st_ctim.tv_nsec = 0;
    sb->st_birthtim.tv_sec = static_cast<std::int64_t>(st.st_ctime);
    sb->st_birthtim.tv_nsec = 0;
    if (st.hasTimes) {
        sb->st_atim = File::WindowsFileTime::Decode(st.times.LastAccessTime.QuadPart);
        sb->st_mtim = File::WindowsFileTime::Decode(st.times.LastWriteTime.QuadPart);
        sb->st_ctim = File::WindowsFileTime::Decode(st.times.ChangeTime.QuadPart);
        sb->st_birthtim = File::WindowsFileTime::Decode(st.times.CreationTime.QuadPart);
    }
    if (st.hasIdentity) {
        sb->st_dev = st.identity.dwVolumeSerialNumber;
        sb->st_ino = WindowsFileNumber_nid_no_patch(st.identity);
        sb->st_nlink = static_cast<std::uint16_t>(std::min<DWORD>(st.identity.nNumberOfLinks, 0xffff));
    }
#else
    sb->st_dev = static_cast<std::uint32_t>(st.st_dev);
    sb->st_ino = static_cast<std::uint32_t>(st.st_ino);
    sb->st_nlink = static_cast<std::uint16_t>(st.st_nlink);
    sb->st_uid = st.st_uid;
    sb->st_gid = st.st_gid;
    sb->st_rdev = static_cast<std::uint32_t>(st.st_rdev);
    sb->st_blksize = static_cast<std::uint32_t>(st.st_blksize);
    sb->st_blocks = static_cast<std::int64_t>(st.st_blocks);
    sb->st_atim.tv_sec = static_cast<std::int64_t>(st.st_atim.tv_sec);
    sb->st_atim.tv_nsec = static_cast<std::int64_t>(st.st_atim.tv_nsec);
    sb->st_mtim.tv_sec = static_cast<std::int64_t>(st.st_mtim.tv_sec);
    sb->st_mtim.tv_nsec = static_cast<std::int64_t>(st.st_mtim.tv_nsec);
    sb->st_ctim.tv_sec = static_cast<std::int64_t>(st.st_ctim.tv_sec);
    sb->st_ctim.tv_nsec = static_cast<std::int64_t>(st.st_ctim.tv_nsec);
#if defined(__APPLE__)
    sb->st_birthtim.tv_sec = static_cast<std::int64_t>(st.st_birthtimespec.tv_sec);
    sb->st_birthtim.tv_nsec = static_cast<std::int64_t>(st.st_birthtimespec.tv_nsec);
#elif defined(__linux__) || defined(__EMSCRIPTEN__)
    sb->st_birthtim = sb->st_ctim;
#else
    sb->st_birthtim.tv_sec = static_cast<std::int64_t>(st.st_birthtim.tv_sec);
    sb->st_birthtim.tv_nsec = static_cast<std::int64_t>(st.st_birthtim.tv_nsec);
#endif
#endif
}

namespace File {

void FillFileStat(const std::filesystem::path& nativePath, FileStat* sb) {
    if (!TryFillFileStat(nativePath, sb)) {
        throw std::runtime_error(std::string("FillFileStat: stat failed for ") + nativePath.string());
    }
}

bool TryFillFileStat(const std::filesystem::path& nativePath, FileStat* sb) {
    NativeStat st{};
    if (DoStat(nativePath, &st) != 0) return false;
    CopyNativeStat(st, sb);
    return true;
}

void FillFileStat(int nativeDescriptor, FileStat* sb) {
    NativeStat st{};
    if (DoFstat(nativeDescriptor, &st) != 0) {
        throw std::runtime_error(std::string("FillFileStat: fstat failed for fd ") + std::to_string(nativeDescriptor));
    }
    CopyNativeStat(st, sb);
}

bool FillFileStatFromDescriptor(int fd, FileStat* sb) {
    if (FillArchivedStatFromDescriptor(fd, sb)) return true;
    if (File::IsDirectoryDescriptor(fd) && !File::IsNativeDirectoryDescriptor(fd)) {
        const auto directory = DirectoryDescriptorPath(fd);
        if (!directory) { errno = ENOENT; return false; }
        if (FillArchivedStat(*directory, sb)) return true;
        if (TryFillFileStat(*directory, sb)) return true;
        errno = ENOENT;
        return false;
    }
    NativeStat st{};
    if (DoFstat(fd, &st) != 0) return false;
    CopyNativeStat(st, sb);
    return true;
}

bool FillLinkStat(const std::filesystem::path& nativePath, FileStat* sb) {
    NativeStat st{};
    if (DoLstat(nativePath, &st) != 0) return false;
    CopyNativeStat(st, sb);
    return true;
}

}
