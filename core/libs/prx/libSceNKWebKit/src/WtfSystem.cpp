#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <string>
#include <stdexcept>
#ifndef _WIN32
#include <unistd.h>
#endif

#include <cstddef>
#include <cstdint>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

namespace NKWebKit {
void FillRandomBytes(void* buffer, std::size_t bytes);
}  // namespace NKWebKit

namespace {
bool g_configPermanentlyFrozen = false;
}  // namespace

extern "C" {
int APS5_VABI _ZN3WTF22numberOfProcessorCoresEv() {
    unsigned int count = std::thread::hardware_concurrency();
    if (count == 0) throw std::runtime_error("numberOfProcessorCores: the host reports no processor count");
    return static_cast<int>(count);
}

std::uint64_t APS5_VABI _ZN3WTF7ramSizeEv() {
#ifdef _WIN32
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    if (GlobalMemoryStatusEx(&status)) {
        return status.ullTotalPhys;
    }
#else
    const long pages = sysconf(_SC_PHYS_PAGES);
    const long pageBytes = sysconf(_SC_PAGE_SIZE);
    if (pages > 0 && pageBytes > 0) return static_cast<std::uint64_t>(pages) * static_cast<std::uint64_t>(pageBytes);
#endif
    throw std::runtime_error("ramSize: the host reports no physical memory size");
}

void APS5_VABI _ZN3WTF29cryptographicallyRandomValuesEPvm(void* buffer, std::size_t length) {
    NKWebKit::FillRandomBytes(buffer, length);
}

std::size_t APS5_VABI _ZN3WTF8pageSizeEv() {
    return 4096;
}

void APS5_VABI _ZN3WTF6Config17permanentlyFreezeEv() {
    g_configPermanentlyFrozen = true;
}

int APS5_VABI _ZN3WTF18constantTimeMemcmpEPKvS1_m(const void* voidA, const void* voidB, std::size_t length) {
    const auto* a = static_cast<const std::uint8_t*>(voidA);
    const auto* b = static_cast<const std::uint8_t*>(voidB);
    std::uint8_t result = 0;
    for (std::size_t i = 0; i < length; ++i) {
        result |= static_cast<std::uint8_t>(a[i] ^ b[i]);
    }
    return result;
}

}

extern "C" {

int APS5_VABI _ZN3WTF14FileSystemImpl8openFileERKNS_6StringENS0_12FileOpenModeENS0_20FileAccessPermissionEb(const NKWebKit::WtfAbi::String& path, int mode, bool permission, bool failIfFileExists) {
    const auto representation = _ZNK3WTF6String4utf8ENS_14ConversionModeE(&path, 0);
    if (!representation.buffer) return -1;
    int flags = SCE_KERNEL_O_RDONLY;
    switch (mode) {
    case 1: flags = SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_TRUNC; break;
    case 2: flags = SCE_KERNEL_O_RDWR | SCE_KERNEL_O_CREAT; break;
    }
    if (failIfFileExists) flags |= SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL;
    return open_nid_postfix(reinterpret_cast<const char*>(representation.buffer + 1), flags, permission ? 0666 : 0600);
}

void APS5_VABI _ZN3WTF14FileSystemImpl9closeFileERi(int& handle) {
    if (handle != -1) {
        close_nid_postfix(handle);
        handle = -1;
    }
}

long long APS5_VABI _ZN3WTF14FileSystemImpl8seekFileEixNS0_14FileSeekOriginE(int handle, long long offset, int origin) {
    const int whence = origin == 1 ? 1 : origin == 2 ? 2 : 0;
    return lseek_nid_postfix(handle, offset, whence);
}

bool APS5_VABI _ZN3WTF14FileSystemImpl12truncateFileEix(int handle, long long offset) {
    return ftruncate_nid_postfix(handle, offset) == 0;
}

int APS5_VABI _ZN3WTF14FileSystemImpl15openAndLockFileERKNS_6StringENS0_12FileOpenModeENS_9OptionSetINS0_12FileLockModeEEE(const NKWebKit::WtfAbi::String& path, int mode, unsigned lockMode) {
    return _ZN3WTF14FileSystemImpl8openFileERKNS_6StringENS0_12FileOpenModeENS0_20FileAccessPermissionEb(path, mode, true, false);
}

void APS5_VABI _ZN3WTF14FileSystemImpl18unlockAndCloseFileEi(int handle) {
    _ZN3WTF14FileSystemImpl9closeFileERi(handle);
}

}

namespace {
constexpr unsigned GuestFileTypeMask = 0170000;
constexpr unsigned GuestDirectoryType = 0040000;
constexpr int GuestNoEntry = 2;
constexpr int GuestAlreadyExists = 17;

std::string FileSystemPath(const NKWebKit::WtfAbi::String& path) {
    const auto representation = _ZNK3WTF6String4utf8ENS_14ConversionModeE(&path, 0);
    return representation.buffer ? reinterpret_cast<const char*>(representation.buffer + 1) : "";
}

bool MakeDirectories(const std::string& path) {
    FileStat status{};
    if (stat_nid_postfix(path.c_str(), &status) == 0) return (status.st_mode & GuestFileTypeMask) == GuestDirectoryType;
    if (*__error_nid_postfix() != GuestNoEntry) return false;
    const auto last = path.find_last_not_of('/');
    if (last == std::string::npos) return false;
    const auto separator = path.rfind('/', last);
    if (separator != std::string::npos && !MakeDirectories(path.substr(0, separator ? separator : 1))) return false;
    if (mkdir_nid_postfix(path.c_str(), 0777) == 0) return true;
    return *__error_nid_postfix() == GuestAlreadyExists && stat_nid_postfix(path.c_str(), &status) == 0 &&
        (status.st_mode & GuestFileTypeMask) == GuestDirectoryType;
}
}  // namespace

extern "C" {

bool APS5_VABI _ZN3WTF14FileSystemImpl10fileExistsERKNS_6StringE(const NKWebKit::WtfAbi::String& path) {
    const auto name = FileSystemPath(path);
    FileStat status{};
    return stat_nid_postfix(name.c_str(), &status) == 0;
}

bool APS5_VABI _ZN3WTF14FileSystemImpl10deleteFileERKNS_6StringE(const NKWebKit::WtfAbi::String& path) {
    const auto name = FileSystemPath(path);
    FileStat status{};
    return lstat_nid_postfix(name.c_str(), &status) == 0 && (status.st_mode & GuestFileTypeMask) != GuestDirectoryType &&
        unlink_nid_postfix(name.c_str()) == 0;
}

bool APS5_VABI _ZN3WTF14FileSystemImpl8moveFileERKNS_6StringES3_(const NKWebKit::WtfAbi::String& from, const NKWebKit::WtfAbi::String& to) {
    const auto source = FileSystemPath(from);
    const auto destination = FileSystemPath(to);
    return RenameGuestPath_nid_no_patch(source.c_str(), destination.c_str()) != -1;
}

bool APS5_VABI _ZN3WTF14FileSystemImpl18makeAllDirectoriesERKNS_6StringE(const NKWebKit::WtfAbi::String& path) {
    return MakeDirectories(FileSystemPath(path));
}

bool APS5_VABI _ZN3WTF14FileSystemImpl20deleteEmptyDirectoryERKNS_6StringE(const NKWebKit::WtfAbi::String& path) {
    const auto name = FileSystemPath(path);
    FileStat status{};
    return lstat_nid_postfix(name.c_str(), &status) == 0 && (status.st_mode & GuestFileTypeMask) == GuestDirectoryType &&
        rmdir_nid_postfix(name.c_str()) == 0;
}

NKWebKit::WtfAbi::String APS5_VABI _ZN3WTF14FileSystemImpl8realPathERKNS_6StringE(const NKWebKit::WtfAbi::String& path) {
    const auto name = FileSystemPath(path);
    std::array<char, 1024> resolved{};
    const auto* result = realpath_nid_postfix(name.c_str(), resolved.data());
    return result ? _ZN3WTF6String8fromUTF8EPKh(reinterpret_cast<const unsigned char*>(result)) : path;
}

}
