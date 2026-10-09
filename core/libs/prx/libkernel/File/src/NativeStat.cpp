#include "prx/libkernel/File/include/NativeStat.hpp"
#include "prx/libkernel/File/include/DirectoryDescriptor.hpp"

#include <cerrno>
#include <cstdint>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#include <sys/stat.h>
#include <sys/types.h>
using NativeStat = struct __stat64;
static int DoStat(const std::filesystem::path& p, NativeStat* st) {
    return _wstat64(p.wstring().c_str(), st);
}
static int DoFstat(int fd, NativeStat* st) {
    if (const auto directory = File::DirectoryDescriptorPath(fd)) return DoStat(*directory, st);
    return _fstat64(fd, st);
}
static bool CopyHandleIdentity(HANDLE handle, FileStat* sb) {
    BY_HANDLE_FILE_INFORMATION info{};
    if (!::GetFileInformationByHandle(handle, &info)) {
        errno = EIO;
        return false;
    }
    const auto index = (static_cast<std::uint64_t>(info.nFileIndexHigh) << 32) | info.nFileIndexLow;
    sb->st_dev = static_cast<std::uint32_t>(info.dwVolumeSerialNumber);
    sb->st_ino = static_cast<std::uint32_t>(index ^ (index >> 32));
    sb->st_nlink = static_cast<std::uint16_t>(info.nNumberOfLinks > 0xffff ? 0xffff : info.nNumberOfLinks);
    return true;
}
static bool CopyPathIdentity(const std::filesystem::path& p, FileStat* sb) {
    const auto handle = ::CreateFileW(p.wstring().c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        errno = EIO;
        return false;
    }
    const bool copied = CopyHandleIdentity(handle, sb);
    ::CloseHandle(handle);
    return copied;
}
static bool CopyDescriptorIdentity(int fd, FileStat* sb) {
    if (const auto directory = File::DirectoryDescriptorPath(fd)) return CopyPathIdentity(*directory, sb);
    const auto handle = reinterpret_cast<HANDLE>(::_get_osfhandle(fd));
    if (handle == INVALID_HANDLE_VALUE) {
        errno = EBADF;
        return false;
    }
    if (::GetFileType(handle) != FILE_TYPE_DISK) return true;
    return CopyHandleIdentity(handle, sb);
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
static bool CopyPathIdentity(const std::filesystem::path&, FileStat*) {
    return true;
}
static bool CopyDescriptorIdentity(int, FileStat*) {
    return true;
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
    if (!CopyPathIdentity(nativePath, sb)) {
        throw std::runtime_error(std::string("FillFileStat: file identity failed for ") + nativePath.string());
    }
}

void FillFileStat(int nativeDescriptor, FileStat* sb) {
    NativeStat st{};
    if (DoFstat(nativeDescriptor, &st) != 0) {
        throw std::runtime_error(std::string("FillFileStat: fstat failed for fd ") + std::to_string(nativeDescriptor));
    }
    CopyNativeStat(st, sb);
    if (!CopyDescriptorIdentity(nativeDescriptor, sb)) {
        throw std::runtime_error(std::string("FillFileStat: file identity failed for fd ") + std::to_string(nativeDescriptor));
    }
}

bool FillFileStatFromDescriptor(int fd, FileStat* sb) {
    NativeStat st{};
    if (DoFstat(fd, &st) != 0) return false;
    CopyNativeStat(st, sb);
    return CopyDescriptorIdentity(fd, sb);
}

}
