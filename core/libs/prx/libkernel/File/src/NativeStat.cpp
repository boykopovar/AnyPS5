#include "prx/libkernel/File/include/NativeStat.hpp"
#include "prx/libkernel/File/include/DirectoryDescriptor.hpp"
#include "prx/libc/include/General.hpp"

#include <stdexcept>
#include <string>

#ifdef _WIN32
#include "prx/libkernel/File/include/WindowsFileTime.hpp"
#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>
struct NativeStat : __stat64 {
    FILE_BASIC_INFO times{};
    bool hasTimes = false;
};
static int ReadTimes(HANDLE handle, NativeStat* st) {
    if (GetFileType(handle) != FILE_TYPE_DISK) return 0;
    if (!GetFileInformationByHandleEx(handle, FileBasicInfo, &st->times, sizeof(st->times)))
        return File::WindowsFileTime::Failure(GetLastError());
    st->hasTimes = true;
    return 0;
}
static int DoStat(const std::filesystem::path& p, NativeStat* st) {
    if (_wstat64(p.c_str(), st) != 0) return -1;
    const auto handle = CreateFileW(p.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return File::WindowsFileTime::Failure(GetLastError());
    const int result = ReadTimes(handle, st);
    CloseHandle(handle);
    return result;
}
static int DoFstat(int fd, NativeStat* st) {
    if (const auto directory = File::DirectoryDescriptorPath(fd)) return DoStat(*directory, st);
    if (_fstat64(fd, st) != 0) return -1;
    return ReadTimes(reinterpret_cast<HANDLE>(::_get_osfhandle(fd)), st);
}
static int DoLstat(const std::filesystem::path& p, NativeStat* st) {
    std::error_code error;
    if (std::filesystem::is_symlink(std::filesystem::symlink_status(p, error))) throw std::runtime_error("lstat of a Windows symbolic link is not supported");
    return DoStat(p, st);
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
#elif defined(__linux__)
    sb->st_birthtim = sb->st_ctim;
#else
    sb->st_birthtim.tv_sec = static_cast<std::int64_t>(st.st_birthtim.tv_sec);
    sb->st_birthtim.tv_nsec = static_cast<std::int64_t>(st.st_birthtim.tv_nsec);
#endif
#endif
}

namespace File {

void FillFileStat(const std::filesystem::path& nativePath, FileStat* sb) {
    NativeStat st{};
    if (DoStat(nativePath, &st) != 0) {
        throw std::runtime_error(std::string("FillFileStat: stat failed for ") + nativePath.string());
    }
    CopyNativeStat(st, sb);
}

void FillFileStat(int nativeDescriptor, FileStat* sb) {
    NativeStat st{};
    if (DoFstat(nativeDescriptor, &st) != 0) {
        throw std::runtime_error(std::string("FillFileStat: fstat failed for fd ") + std::to_string(nativeDescriptor));
    }
    CopyNativeStat(st, sb);
}

bool FillFileStatFromDescriptor(int fd, FileStat* sb) {
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
