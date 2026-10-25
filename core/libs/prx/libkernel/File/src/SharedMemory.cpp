#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/File/include/SharedMemory.hpp"
#include "prx/libc/include/CreationMask.hpp"
#include "prx/libkernel/File/include/File.hpp"

#include <cerrno>
#include <algorithm>
#include <string_view>
#include <vector>
#include <stdexcept>
#include <string>
#include <cstdint>
#include <limits>
#include <optional>
#include <atomic>
#include <mutex>
#include <unordered_map>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <sys/stat.h>
#include <filesystem>
#include <random>
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>
#include "nid/Sha1.hpp"
extern "C" _invalid_parameter_handler _set_thread_local_invalid_parameter_handler(_invalid_parameter_handler);
#else
#include "nid/Sha1.hpp"
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/uio.h>
#include <unistd.h>
#include <cstdio>
#endif

namespace {

#ifdef _WIN32
std::mutex sharedDescriptorAccessMutex;
std::unordered_map<int, bool> sharedDescriptorAccess;

const std::wstring& WindowsNamedRoot() {
    static const std::wstring root = [] {
        const auto directory = std::filesystem::temp_directory_path();
        std::vector<wchar_t> pathBuffer(32768);
        const HANDLE handle = ::CreateFileW(directory.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            throw std::system_error(static_cast<int>(::GetLastError()), std::system_category(), "opening the named shared-memory temporary root");
        const DWORD length = ::GetFinalPathNameByHandleW(handle, pathBuffer.data(), static_cast<DWORD>(pathBuffer.size()),
            FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
        const DWORD error = length == 0 ? ::GetLastError() : length >= pathBuffer.size() ? ERROR_FILENAME_EXCED_RANGE : ERROR_SUCCESS;
        ::CloseHandle(handle);
        if (error != ERROR_SUCCESS)
            throw std::system_error(static_cast<int>(error), std::system_category(), "resolving the named shared-memory temporary root");
        return (std::filesystem::path(std::wstring(pathBuffer.data(), length)) / L"AnyPS5" / L"shm").wstring();
    }();
    return root;
}

void IgnoreDescriptorParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, std::uintptr_t) {}
#endif

bool IsHostSharedMemoryDescriptor(int descriptor) {
#ifdef __linux__
    char descriptorPath[64];
    std::snprintf(descriptorPath, sizeof(descriptorPath), "/proc/self/fd/%d", descriptor);
    char objectPath[4096];
    const auto length = ::readlink(descriptorPath, objectPath, sizeof(objectPath) - 1);
    if (length < 0) return false;
    objectPath[length] = '\0';
    return std::string_view(objectPath).starts_with("/dev/shm/anyps5-shm-") ||
        std::string_view(objectPath).starts_with("/memfd:anyps5 shm_open");
#elif defined(_WIN32)
    {
        std::lock_guard lock(sharedDescriptorAccessMutex);
        if (!sharedDescriptorAccess.contains(descriptor)) return false;
    }
    const auto previous = _set_thread_local_invalid_parameter_handler(IgnoreDescriptorParameter);
    const auto handleValue = ::_get_osfhandle(descriptor);
    _set_thread_local_invalid_parameter_handler(previous);
    if (handleValue == -1) return false;
    const auto handle = reinterpret_cast<HANDLE>(handleValue);
    std::vector<wchar_t> pathBuffer(32768);
    const DWORD length = ::GetFinalPathNameByHandleW(handle, pathBuffer.data(), static_cast<DWORD>(pathBuffer.size()), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (length == 0 || length >= pathBuffer.size()) return false;
    std::wstring objectPath(pathBuffer.data(), length);
    std::wstring_view root;
    try {
        root = WindowsNamedRoot();
    } catch (...) {
        return false;
    }
    if (objectPath.size() <= root.size() || objectPath[root.size()] != L'\\') return false;
    if (::CompareStringOrdinal(objectPath.data(), static_cast<int>(root.size()), root.data(), static_cast<int>(root.size()), TRUE) != CSTR_EQUAL) return false;
    const std::wstring_view relative(objectPath.data() + root.size() + 1, objectPath.size() - root.size() - 1);
    if (relative.find_first_of(L"\\/") != std::wstring_view::npos) return false;
    const auto validHex = [](wchar_t character) {
        return (character >= L'0' && character <= L'9') || (character >= L'a' && character <= L'f');
    };
    if (relative.size() < 44 || !std::all_of(relative.begin(), relative.begin() + 40, validHex)) return false;
    if (relative.substr(40) == L".dat") return true;
    constexpr std::wstring_view marker = L"-unlinked-";
    if (!relative.substr(40).starts_with(marker) || !relative.ends_with(L".dat")) return false;
    const auto suffix = relative.substr(40 + marker.size(), relative.size() - 40 - marker.size() - 4);
    const auto separator = suffix.find(L'-');
    if (separator == std::wstring_view::npos || separator == 0 || separator + 1 == suffix.size()) return false;
    const auto decimal = [](std::wstring_view value) {
        return std::all_of(value.begin(), value.end(), [](wchar_t character) { return character >= L'0' && character <= L'9'; });
    };
    return decimal(suffix.substr(0, separator)) && decimal(suffix.substr(separator + 1));
#else
    (void)descriptor;
    return false;
#endif
}

class HostShmLock {
public:
    HostShmLock() {
#ifdef __linux__
        descriptor = ::open("/dev/shm", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (descriptor < 0 || ::flock(descriptor, LOCK_EX) != 0) error = errno;
#endif
    }

    ~HostShmLock() {
#ifdef __linux__
        const int savedError = errno;
        if (descriptor >= 0) {
            if (!error) ::flock(descriptor, LOCK_UN);
            ::close(descriptor);
        }
        errno = savedError;
#endif
    }

    int Error() const { return error; }

private:
    int descriptor = -1;
    int error = 0;
};

constexpr int GuestNoMemory = 12;
constexpr int GuestInvalid = 22;
constexpr int GuestTooManyFilesInSystem = 23;
constexpr int GuestTooManyFiles = 24;
constexpr int GuestCloseOnExec = 0x00100000;
constexpr int AcceptedFlags = SCE_KERNEL_O_ACCMODE | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_EXCL | SCE_KERNEL_O_TRUNC | GuestCloseOnExec;

const char* const anonymousObject = reinterpret_cast<const char*>(1);

int Fail(int error) {
    *__error_nid_postfix() = error;
    return -1;
}

int GuestError(int error) {
    switch (error) {
    case EMFILE:
        return GuestTooManyFiles;
    case ENFILE:
        return GuestTooManyFilesInSystem;
    case ENOMEM:
        return GuestNoMemory;
    default:
        return 0;
    }
}

int NamedGuestError(int error) {
    switch (error) {
    case ENOENT:
        return 2;
    case EINTR:
        return 4;
    case EACCES:
        return 13;
    case EFAULT:
        return 14;
    case EEXIST:
        return 17;
    case EINVAL:
        return GuestInvalid;
    case ENOSPC:
        return 28;
    case EROFS:
        return 30;
    case ENAMETOOLONG:
        return 63;
    default:
        return GuestError(error);
    }
}

std::string NamedHostName(const char* path, int& error) {
    if (path == nullptr) { error = EFAULT; return {}; }
    std::size_t length = 0;
    while (length < 1024 && path[length] != '\0') ++length;
    if (length == 1024) { error = ENAMETOOLONG; return {}; }
    if (length == 0 || path[0] != '/') { error = EINVAL; return {}; }
    const auto digest = Nid::Sha1(std::vector<std::uint8_t>(path, path + length));
    std::string hostName;
#ifndef _WIN32
    hostName = "/anyps5-shm-";
#endif
    for (const auto byte : digest) {
        hostName += "0123456789abcdef"[byte >> 4];
        hostName += "0123456789abcdef"[byte & 15];
    }
    error = 0;
    return hostName;
}

#ifdef _WIN32
int WindowsNamedError(DWORD error) {
    switch (error) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND: return ENOENT;
    case ERROR_ACCESS_DENIED:
    case ERROR_SHARING_VIOLATION: return EACCES;
    case ERROR_FILE_EXISTS:
    case ERROR_ALREADY_EXISTS: return EEXIST;
    case ERROR_INVALID_NAME: return EINVAL;
    case ERROR_TOO_MANY_OPEN_FILES: return EMFILE;
    case ERROR_DISK_FULL: return ENOSPC;
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY: return ENOMEM;
    default: return 0;
    }
}

std::filesystem::path WindowsNamedPath(const std::string& name) {
    return std::filesystem::path(WindowsNamedRoot()) / std::filesystem::path(name + ".dat");
}

void EnsurePrivateWindowsDirectory(const std::filesystem::path& path, bool objectDirectory = false) {
    const std::wstring ownerRights = objectDirectory ? L"0x001701ff" : L"FA";
    const std::wstring policy = L"D:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)(A;OICI;" + ownerRights + L";;;OW)";
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!::ConvertStringSecurityDescriptorToSecurityDescriptorW(
            policy.c_str(), SDDL_REVISION_1, &descriptor, nullptr))
        throw std::system_error(static_cast<int>(::GetLastError()), std::system_category(), "creating the named shared-memory access policy");
    PACL dacl = nullptr;
    BOOL present = FALSE;
    BOOL defaulted = FALSE;
    if (!::GetSecurityDescriptorDacl(descriptor, &present, &dacl, &defaulted) || !present || dacl == nullptr) {
        const DWORD error = ::GetLastError();
        ::LocalFree(descriptor);
        throw std::system_error(static_cast<int>(error == ERROR_SUCCESS ? ERROR_INVALID_SECURITY_DESCR : error),
                                std::system_category(), "reading the named shared-memory access policy");
    }

    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), descriptor, FALSE};
    if (!::CreateDirectoryW(path.c_str(), &security) && ::GetLastError() != ERROR_ALREADY_EXISTS) {
        const DWORD error = ::GetLastError();
        ::LocalFree(descriptor);
        throw std::system_error(static_cast<int>(error), std::system_category(), "creating the named shared-memory directory");
    }
    const DWORD attributes = ::GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
        (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        const DWORD error = attributes == INVALID_FILE_ATTRIBUTES ? ::GetLastError() : ERROR_ACCESS_DENIED;
        ::LocalFree(descriptor);
        throw std::system_error(static_cast<int>(error), std::system_category(), "validating the named shared-memory directory");
    }
    const DWORD result = ::SetNamedSecurityInfoW(const_cast<LPWSTR>(path.c_str()), SE_FILE_OBJECT,
                                                  DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
                                                  nullptr, nullptr, dacl, nullptr);
    ::LocalFree(descriptor);
    if (result != ERROR_SUCCESS) throw std::system_error(static_cast<int>(result), std::system_category(), "securing the named shared-memory directory");
}

DWORD SetWindowsNamedPermissions(HANDLE handle, int mode) {
    std::wstring policy = L"D:P(A;OICI;FA;;;SY)(A;;WD;;;OW)";
    if ((mode & 0400) != 0) policy += L"(A;;GRWD;;;OW)";
    if ((mode & 0200) != 0) policy += L"(A;;GWSDWD;;;OW)";
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    if (!::ConvertStringSecurityDescriptorToSecurityDescriptorW(policy.c_str(), SDDL_REVISION_1, &descriptor, nullptr))
        return ::GetLastError();
    PACL dacl = nullptr;
    BOOL present = FALSE;
    BOOL defaulted = FALSE;
    if (!::GetSecurityDescriptorDacl(descriptor, &present, &dacl, &defaulted) || !present || dacl == nullptr) {
        const DWORD error = ::GetLastError();
        ::LocalFree(descriptor);
        return error == ERROR_SUCCESS ? ERROR_INVALID_SECURITY_DESCR : error;
    }
    const DWORD result = ::SetSecurityInfo(handle, SE_FILE_OBJECT,
        DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION, nullptr, nullptr, dacl, nullptr);
    ::LocalFree(descriptor);
    return result;
}

void CleanupUnlinkedWindowsObjects(const std::filesystem::path& objectPath, const std::string& name) {
    const auto pattern = objectPath.parent_path() / (std::wstring(name.begin(), name.end()) + L"-unlinked-*.dat");
    WIN32_FIND_DATAW data{};
    const HANDLE search = ::FindFirstFileW(pattern.c_str(), &data);
    if (search == INVALID_HANDLE_VALUE) return;
    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            const auto orphanPath = objectPath.parent_path() / data.cFileName;
            HANDLE orphan = ::CreateFileW(orphanPath.c_str(), DELETE, 0, nullptr, OPEN_EXISTING,
                FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
            if (orphan != INVALID_HANDLE_VALUE) {
                FILE_DISPOSITION_INFO deletion{TRUE};
                ::SetFileInformationByHandle(orphan, FileDispositionInfo, &deletion, sizeof(deletion));
                ::CloseHandle(orphan);
            }
        }
    } while (::FindNextFileW(search, &data));
    ::FindClose(search);
}

void ReapUnlinkedWindowsObject(const std::filesystem::path& orphanPath) {
    HANDLE orphan = ::CreateFileW(orphanPath.c_str(), DELETE, 0, nullptr, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (orphan == INVALID_HANDLE_VALUE) return;
    FILE_DISPOSITION_INFO deletion{TRUE};
    ::SetFileInformationByHandle(orphan, FileDispositionInfo, &deletion, sizeof(deletion));
    ::CloseHandle(orphan);
}

class WindowsNamedLock {
public:
    explicit WindowsNamedLock(const std::string& name) {
        Initialize(WindowsNamedPath(name).wstring());
    }

    explicit WindowsNamedLock(HANDLE object) {
        BY_HANDLE_FILE_INFORMATION status{};
        if (!::GetFileInformationByHandle(object, &status)) { error = ::GetLastError(); return; }
        Initialize(L"object-" + std::to_wstring(status.dwVolumeSerialNumber) + L"-" +
                   std::to_wstring(status.nFileIndexHigh) + L"-" + std::to_wstring(status.nFileIndexLow));
    }

    ~WindowsNamedLock() {
        if (locked) ::ReleaseMutex(handle);
        if (handle != nullptr) ::CloseHandle(handle);
    }

    DWORD Error() const { return error; }

private:
    void Initialize(const std::wstring& identity) {
        const auto bytes = std::vector<std::uint8_t>(reinterpret_cast<const std::uint8_t*>(identity.data()),
                                                     reinterpret_cast<const std::uint8_t*>(identity.data() + identity.size()));
        const auto digest = Nid::Sha1(bytes);
        std::wstring lockName = L"Global\\AnyPS5NamedShm-";
        for (const auto byte : digest) {
            lockName += L"0123456789abcdef"[byte >> 4];
            lockName += L"0123456789abcdef"[byte & 15];
        }
        handle = ::CreateMutexW(nullptr, FALSE, lockName.c_str());
        if (handle == nullptr) { error = ::GetLastError(); return; }
        const DWORD result = ::WaitForSingleObject(handle, INFINITE);
        if (result == WAIT_OBJECT_0 || result == WAIT_ABANDONED) locked = true;
        else error = result == WAIT_FAILED ? ::GetLastError() : ERROR_GEN_FAILURE;
    }
    HANDLE handle = nullptr;
    DWORD error = ERROR_SUCCESS;
    bool locked = false;
};

int OpenNamedFile(const char* path, int flags, int mode) {
    const int savedError = errno;
    int validationError = 0;
    const auto hostName = NamedHostName(path, validationError);
    if (validationError != 0) return Fail(NamedGuestError(validationError) ? NamedGuestError(validationError) : GuestInvalid);

    WindowsNamedLock namespaceLock(hostName);
    if (namespaceLock.Error() != ERROR_SUCCESS) {
        if (const int guest = WindowsNamedError(namespaceLock.Error())) return Fail(NamedGuestError(guest));
        throw std::runtime_error("shm_open: locking the host named-object namespace failed, error=" + std::to_string(namespaceLock.Error()));
    }

    std::filesystem::path objectPath;
    try {
        objectPath = WindowsNamedPath(hostName);
        EnsurePrivateWindowsDirectory(objectPath.parent_path().parent_path());
        EnsurePrivateWindowsDirectory(objectPath.parent_path(), true);
        CleanupUnlinkedWindowsObjects(objectPath, hostName);
    } catch (const std::system_error& error) {
        const auto nativeError = static_cast<DWORD>(error.code().value());
        if (const int guest = WindowsNamedError(nativeError)) return Fail(NamedGuestError(guest));
        throw std::runtime_error("shm_open: preparing the host named-object namespace failed: " + std::string(error.what()));
    }

    const bool readWrite = (flags & SCE_KERNEL_O_ACCMODE) == SCE_KERNEL_O_RDWR;
    const DWORD access = GENERIC_READ | (readWrite ? GENERIC_WRITE : 0) | WRITE_DAC;
    const bool create = (flags & SCE_KERNEL_O_CREAT) != 0;
    const DWORD creationAccess = access | (create ? WRITE_DAC | DELETE : 0);
    ::SetLastError(ERROR_SUCCESS);
    HANDLE handle = ::CreateFileW(objectPath.c_str(), creationAccess, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                  nullptr, create ? CREATE_NEW : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    DWORD createResult = ::GetLastError();
    bool created = handle != INVALID_HANDLE_VALUE && create;
    if (handle == INVALID_HANDLE_VALUE && create && createResult == ERROR_FILE_EXISTS && !(flags & SCE_KERNEL_O_EXCL)) {
        handle = ::CreateFileW(objectPath.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        createResult = ::GetLastError();
    }
    if (handle == INVALID_HANDLE_VALUE) {
        const DWORD nativeError = createResult;
        if (const int guest = WindowsNamedError(nativeError)) return Fail(NamedGuestError(guest));
        throw std::runtime_error("shm_open: opening the host named object failed, error=" + std::to_string(nativeError));
    }

    BY_HANDLE_FILE_INFORMATION status{};
    if (!::GetFileInformationByHandle(handle, &status) || (status.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        const DWORD lastError = ::GetLastError();
        const DWORD nativeError = lastError == ERROR_SUCCESS ? ERROR_INVALID_NAME : lastError;
        if (created) {
            FILE_DISPOSITION_INFO deletion{TRUE};
            ::SetFileInformationByHandle(handle, FileDispositionInfo, &deletion, sizeof(deletion));
        }
        ::CloseHandle(handle);
        if (const int guest = WindowsNamedError(nativeError)) return Fail(NamedGuestError(guest));
        throw std::runtime_error("shm_open: refusing a reparse-point named object, error=" + std::to_string(nativeError));
    }

    if (created) {
        const int permissions = (mode & 0777) & ~File::GetCreationMask_nid_no_patch();
        const DWORD nativeError = SetWindowsNamedPermissions(handle, permissions);
        if (nativeError != ERROR_SUCCESS) {
            FILE_DISPOSITION_INFO deletion{TRUE};
            ::SetFileInformationByHandle(handle, FileDispositionInfo, &deletion, sizeof(deletion));
            ::CloseHandle(handle);
            if (const int guest = WindowsNamedError(nativeError)) return Fail(NamedGuestError(guest));
            throw std::runtime_error("shm_open: setting guest creation permissions failed, error=" + std::to_string(nativeError));
        }
    }

    if ((flags & SCE_KERNEL_O_TRUNC) && readWrite) {
        WindowsNamedLock objectLock(handle);
        if (objectLock.Error() != ERROR_SUCCESS) {
            const DWORD nativeError = objectLock.Error();
            ::CloseHandle(handle);
            if (const int guest = WindowsNamedError(nativeError)) return Fail(NamedGuestError(guest));
            throw std::runtime_error("shm_open: locking the host named object for truncation failed, error=" + std::to_string(nativeError));
        }
        LARGE_INTEGER zero{};
        if (!::SetFilePointerEx(handle, zero, nullptr, FILE_BEGIN) || !::SetEndOfFile(handle)) {
            const DWORD nativeError = ::GetLastError();
            ::CloseHandle(handle);
            if (const int guest = WindowsNamedError(nativeError)) return Fail(NamedGuestError(guest));
            throw std::runtime_error("shm_open: truncating the host named object failed, error=" + std::to_string(nativeError));
        }
    }

    const int nativeFlags = _O_BINARY | (readWrite ? _O_RDWR : _O_RDONLY);
    const int descriptor = ::_open_osfhandle(reinterpret_cast<std::intptr_t>(handle), nativeFlags);
    if (descriptor >= 0) {
        File::TrackSharedMemoryDescriptor_nid_no_patch(descriptor, readWrite);
        errno = savedError;
        return descriptor;
    }
    const int nativeError = errno;
    ::CloseHandle(handle);
    if (const int guest = GuestError(nativeError)) return Fail(guest);
    throw std::runtime_error("shm_open: converting the host named object to a descriptor failed, errno=" + std::to_string(nativeError));
}

int UnlinkNamedFile(const char* path) {
    const int savedError = errno;
    int validationError = 0;
    const auto hostName = NamedHostName(path, validationError);
    if (validationError != 0) return Fail(NamedGuestError(validationError) ? NamedGuestError(validationError) : GuestInvalid);
    WindowsNamedLock namespaceLock(hostName);
    if (namespaceLock.Error() != ERROR_SUCCESS) {
        if (const int guest = WindowsNamedError(namespaceLock.Error())) return Fail(NamedGuestError(guest));
        throw std::runtime_error("shm_unlink: locking the host named-object namespace failed, error=" + std::to_string(namespaceLock.Error()));
    }

    std::filesystem::path objectPath;
    try {
        objectPath = WindowsNamedPath(hostName);
    } catch (const std::filesystem::filesystem_error& error) {
        const auto nativeError = static_cast<DWORD>(error.code().value());
        if (const int guest = WindowsNamedError(nativeError)) return Fail(NamedGuestError(guest));
        throw std::runtime_error("shm_unlink: locating the host named object failed: " + std::string(error.what()));
    }

    HANDLE objectHandle = ::CreateFileW(objectPath.c_str(), FILE_READ_ATTRIBUTES | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
    if (objectHandle == INVALID_HANDLE_VALUE) {
        const DWORD nativeError = ::GetLastError();
        if (const int guest = WindowsNamedError(nativeError)) return Fail(NamedGuestError(guest));
        throw std::runtime_error("shm_unlink: opening the host named object failed, error=" + std::to_string(nativeError));
    }
    BY_HANDLE_FILE_INFORMATION status{};
    if (!::GetFileInformationByHandle(objectHandle, &status) ||
        (status.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0) {
        const DWORD lastError = ::GetLastError();
        const DWORD nativeError = lastError == ERROR_SUCCESS ? ERROR_INVALID_NAME : lastError;
        ::CloseHandle(objectHandle);
        if (const int guest = WindowsNamedError(nativeError)) return Fail(NamedGuestError(guest));
        throw std::runtime_error("shm_unlink: refusing an invalid host named object, error=" + std::to_string(nativeError));
    }

    static std::atomic<unsigned> sequence{0};
    for (unsigned attempt = 0; attempt < 64; ++attempt) {
        const auto orphanPath = objectPath.parent_path() /
            (objectPath.stem().wstring() + L"-unlinked-" + std::to_wstring(::GetCurrentProcessId()) +
             L"-" + std::to_wstring(sequence.fetch_add(1, std::memory_order_relaxed)) + objectPath.extension().wstring());
        if (!::MoveFileExW(objectPath.c_str(), orphanPath.c_str(), MOVEFILE_WRITE_THROUGH)) {
            const DWORD nativeError = ::GetLastError();
            if (nativeError == ERROR_ALREADY_EXISTS || nativeError == ERROR_FILE_EXISTS) continue;
            ::CloseHandle(objectHandle);
            if (const int guest = WindowsNamedError(nativeError)) return Fail(NamedGuestError(guest));
            throw std::runtime_error("shm_unlink: detaching the host named object failed, error=" + std::to_string(nativeError));
        }
        ::CloseHandle(objectHandle);
        ReapUnlinkedWindowsObject(orphanPath);
        errno = savedError;
        return 0;
    }
    ::CloseHandle(objectHandle);
    return Fail(NamedGuestError(EEXIST));
}

int SetSharedMemoryModeInternal(int descriptor, int mode) {
    if (!IsHostSharedMemoryDescriptor(descriptor)) return -2;
    const auto rawHandle = ::_get_osfhandle(descriptor);
    if (rawHandle == -1) { errno = EBADF; return -1; }
    const auto handle = reinterpret_cast<HANDLE>(rawHandle);
    WindowsNamedLock objectLock(handle);
    if (objectLock.Error() != ERROR_SUCCESS) {
        const int error = WindowsNamedError(objectLock.Error());
        errno = error ? error : EIO;
        return -1;
    }
    const DWORD nativeError = SetWindowsNamedPermissions(handle, mode & 0777);
    if (nativeError == ERROR_SUCCESS) return 0;
    const int error = WindowsNamedError(nativeError);
    errno = error ? error : EACCES;
    return -1;
}

std::filesystem::path SharedMemoryDescriptorPath(int descriptor) {
    const auto rawHandle = ::_get_osfhandle(descriptor);
    std::filesystem::path finalPath;
    if (rawHandle != -1) {
        std::vector<wchar_t> path(32768);
        const DWORD length = ::GetFinalPathNameByHandleW(reinterpret_cast<HANDLE>(rawHandle), path.data(),
            static_cast<DWORD>(path.size()), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
        if (length > 4 && length < path.size()) {
            std::wstring value(path.data(), length);
            finalPath = std::filesystem::path(std::move(value));
        }
    }
    return finalPath;
}

void ReapSharedMemoryDescriptorPath(const std::filesystem::path& path) {
    if (!path.empty() && path.filename().wstring().find(L"-unlinked-") != std::wstring::npos)
        ReapUnlinkedWindowsObject(path);
}

int CloseSharedMemoryDescriptorInternal(int descriptor) {
    if (!IsHostSharedMemoryDescriptor(descriptor)) return -2;
    const int savedError = errno;
    const auto finalPath = SharedMemoryDescriptorPath(descriptor);
    int result;
    {
        std::lock_guard lock(sharedDescriptorAccessMutex);
        result = ::_close(descriptor);
        if (result == 0) sharedDescriptorAccess.erase(descriptor);
    }
    if (result == 0) ReapSharedMemoryDescriptorPath(finalPath);
    if (result == 0) errno = savedError;
    return result;
}

int ReplaceSharedMemoryDescriptorInternal(int source, int destination) {
    if (!IsHostSharedMemoryDescriptor(destination)) return -2;
    const int savedError = errno;
    const auto finalPath = SharedMemoryDescriptorPath(destination);
    const auto previous = _set_thread_local_invalid_parameter_handler(IgnoreDescriptorParameter);
    const int result = ::_dup2(source, destination);
    _set_thread_local_invalid_parameter_handler(previous);
    if (result == 0) {
        ReapSharedMemoryDescriptorPath(finalPath);
        errno = savedError;
    }
    return result;
}
#else
int OpenNamedFile(const char* path, int flags, int mode) {
    int validationError = 0;
    const auto hostName = NamedHostName(path, validationError);
    if (validationError != 0) return Fail(NamedGuestError(validationError) ? NamedGuestError(validationError) : GuestInvalid);
    HostShmLock namespaceLock;
    if (namespaceLock.Error() != 0) {
        errno = namespaceLock.Error();
        if (const int guest = NamedGuestError(errno)) return Fail(guest);
        throw std::runtime_error("shm_open: locking the host shared-memory namespace failed, errno=" + std::to_string(errno));
    }
    int hostFlags = (flags & SCE_KERNEL_O_ACCMODE) == SCE_KERNEL_O_RDWR ? O_RDWR : O_RDONLY;
    if (flags & SCE_KERNEL_O_CREAT) hostFlags |= O_CREAT;
    if (flags & SCE_KERNEL_O_EXCL) hostFlags |= O_EXCL;
    if ((flags & SCE_KERNEL_O_TRUNC) && (flags & SCE_KERNEL_O_ACCMODE) == SCE_KERNEL_O_RDWR) hostFlags |= O_TRUNC;
    const auto permissions = (mode & 0777) & ~File::GetCreationMask_nid_no_patch();
    bool created = false;
    int descriptor;
    if (flags & SCE_KERNEL_O_CREAT) {
        descriptor = ::shm_open(hostName.c_str(), hostFlags | O_CLOEXEC | O_EXCL, permissions);
        created = descriptor >= 0;
        if (descriptor < 0 && errno == EEXIST && !(flags & SCE_KERNEL_O_EXCL)) {
            descriptor = ::shm_open(hostName.c_str(), (hostFlags & ~(O_CREAT | O_EXCL)) | O_CLOEXEC, 0);
        }
    } else {
        descriptor = ::shm_open(hostName.c_str(), hostFlags | O_CLOEXEC, 0);
    }
    if (descriptor >= 0) {
        if (!created || ::fchmod(descriptor, permissions) == 0) return descriptor;
        const int error = errno;
        ::close(descriptor);
        ::shm_unlink(hostName.c_str());
        if (const int guest = NamedGuestError(error)) return Fail(guest);
        throw std::runtime_error("shm_open: setting guest creation permissions failed, errno=" + std::to_string(error));
    }
    const int error = errno;
    if (const int guest = NamedGuestError(error)) return Fail(guest);
    throw std::runtime_error("shm_open: opening the host named object failed, errno=" + std::to_string(error));
}
#endif

int CreateAnonymousFile() {
#ifdef _WIN32
    static std::atomic<unsigned> sequence{0};
    const auto directory = std::filesystem::temp_directory_path();
    const auto seed = std::to_string(std::random_device{}());
    for (int attempt = 0; attempt < 64; ++attempt) {
        const auto path = directory / ("anyps5-shm-" + seed + "-" + std::to_string(sequence++));
        int descriptor = -1;
        const int error = _wsopen_s(&descriptor, path.c_str(),
                                    _O_CREAT | _O_EXCL | _O_RDWR | _O_BINARY | _O_TEMPORARY | _O_SHORT_LIVED | _O_NOINHERIT,
                                    _SH_DENYNO, _S_IREAD | _S_IWRITE);
        if (error == 0) return descriptor;
        if (error != EEXIST) {
            errno = error;
            return -1;
        }
    }
    errno = EEXIST;
    return -1;
#else
    const int descriptor = memfd_create("anyps5 shm_open", MFD_CLOEXEC);
    return descriptor;
#endif
}

}

namespace File {

bool IsSharedMemoryDescriptor_nid_no_patch(int descriptor) {
    return IsHostSharedMemoryDescriptor(descriptor);
}

#ifdef _WIN32
int SetSharedMemoryMode_nid_no_patch(int descriptor, int mode) {
    return SetSharedMemoryModeInternal(descriptor, mode);
}

int CloseSharedMemoryDescriptor_nid_no_patch(int descriptor) {
    return CloseSharedMemoryDescriptorInternal(descriptor);
}

int ReplaceSharedMemoryDescriptor_nid_no_patch(int source, int destination) {
    return ReplaceSharedMemoryDescriptorInternal(source, destination);
}

int SharedMemoryDescriptorWritable_nid_no_patch(int descriptor) {
    if (!IsHostSharedMemoryDescriptor(descriptor)) return -2;
    std::lock_guard lock(sharedDescriptorAccessMutex);
    const auto found = sharedDescriptorAccess.find(descriptor);
    return found != sharedDescriptorAccess.end() && found->second ? 1 : 0;
}

void TrackSharedMemoryDescriptor_nid_no_patch(int descriptor, bool writable) {
    std::lock_guard lock(sharedDescriptorAccessMutex);
    sharedDescriptorAccess[descriptor] = writable;
}

void ForgetSharedMemoryDescriptor_nid_no_patch(int descriptor) {
    std::lock_guard lock(sharedDescriptorAccessMutex);
    sharedDescriptorAccess.erase(descriptor);
}

void DuplicateSharedMemoryDescriptor_nid_no_patch(int source, int destination) {
    std::lock_guard lock(sharedDescriptorAccessMutex);
    sharedDescriptorAccess.erase(destination);
    const auto found = sharedDescriptorAccess.find(source);
    if (found != sharedDescriptorAccess.end()) sharedDescriptorAccess[destination] = found->second;
}
#endif

namespace {

class SharedMemoryLock {
public:
    explicit SharedMemoryLock(int value) {
#ifdef __linux__
        struct stat originalStatus{};
        if (::fstat(value, &originalStatus) != 0) { shared = true; error = errno; return; }
#elif defined(_WIN32)
        if (!IsHostSharedMemoryDescriptor(value)) return;
        const auto originalValue = ::_get_osfhandle(value);
        if (originalValue == -1) return;
        BY_HANDLE_FILE_INFORMATION originalStatus{};
        if (!::GetFileInformationByHandle(reinterpret_cast<HANDLE>(originalValue), &originalStatus)) {
            shared = true;
            error = EBADF;
            return;
        }
        windowsObjectLock = std::make_unique<WindowsNamedLock>(reinterpret_cast<HANDLE>(originalValue));
        if (windowsObjectLock->Error() != ERROR_SUCCESS) { shared = true; error = WindowsNamedError(windowsObjectLock->Error()); if (!error) error = EIO; return; }
#endif
        if (!IsHostSharedMemoryDescriptor(value)) {
#ifdef __linux__
            struct stat currentStatus{};
            if (::fstat(value, &currentStatus) != 0) { shared = true; error = errno; return; }
            if (currentStatus.st_dev != originalStatus.st_dev || currentStatus.st_ino != originalStatus.st_ino) {
                shared = true;
                error = EBADF;
            }
#endif
            return;
        }
        shared = true;
        hostLock.emplace();
        if (hostLock->Error() != 0) { error = hostLock->Error(); return; }
        descriptor = Duplicate(value);
        if (descriptor < 0) { error = errno; return; }
#ifdef _WIN32
        DuplicateSharedMemoryDescriptor_nid_no_patch(value, descriptor);
#endif
        if (!IsHostSharedMemoryDescriptor(descriptor)) { error = EBADF; return; }
        if (!Refresh()) { error = errno; return; }
#ifdef __linux__
        if (status.st_dev != originalStatus.st_dev || status.st_ino != originalStatus.st_ino) error = EBADF;
#elif defined(_WIN32)
        BY_HANDLE_FILE_INFORMATION duplicatedStatus{};
        const auto duplicatedValue = ::_get_osfhandle(descriptor);
        if (duplicatedValue == -1 || !::GetFileInformationByHandle(reinterpret_cast<HANDLE>(duplicatedValue), &duplicatedStatus) ||
            duplicatedStatus.dwVolumeSerialNumber != originalStatus.dwVolumeSerialNumber ||
            duplicatedStatus.nFileIndexHigh != originalStatus.nFileIndexHigh ||
            duplicatedStatus.nFileIndexLow != originalStatus.nFileIndexLow) error = EBADF;
#endif
    }

    ~SharedMemoryLock() {
        if (descriptor >= 0) {
            const int savedError = errno;
#ifdef _WIN32
            CloseSharedMemoryDescriptor_nid_no_patch(descriptor);
#else
            ::close(descriptor);
#endif
            errno = savedError;
        }
    }

    bool IsShared() const { return shared; }
    int Error() const { return error; }
    int Descriptor() const { return descriptor; }
    bool Refresh() {
#ifdef _WIN32
        struct _stat64 nativeStatus{};
        if (::_fstat64(descriptor, &nativeStatus) != 0) return false;
        status.st_size = nativeStatus.st_size;
        return true;
#else
        return ::fstat(descriptor, &status) == 0;
#endif
    }
    struct stat status{};

private:
    static int Duplicate(int value) {
#ifdef _WIN32
        return ::_dup(value);
#else
        return ::fcntl(value, F_DUPFD_CLOEXEC, 0);
#endif
    }

    int descriptor = -1;
    std::optional<HostShmLock> hostLock;
#ifdef _WIN32
    std::unique_ptr<WindowsNamedLock> windowsObjectLock;
#endif
    bool shared = false;
    int error = 0;
};
}

std::int64_t WriteSharedMemory_nid_no_patch(int descriptor, const void* buffer, std::size_t size) {
    SharedMemoryLock guard(descriptor);
    if (!guard.IsShared()) return -2;
    if (guard.Error() != 0) { errno = guard.Error(); return -1; }
    if (!guard.Refresh()) return -1;
    const auto fd = guard.Descriptor();
#ifdef _WIN32
    if (SharedMemoryDescriptorWritable_nid_no_patch(fd) == 0) { errno = EBADF; return -1; }
#else
    const int access = ::fcntl(fd, F_GETFL);
    if (access < 0) return -1;
    if ((access & O_ACCMODE) == O_RDONLY) { errno = EBADF; return -1; }
#endif
    const auto& status = guard.status;
#ifdef _WIN32
    const auto position = ::_lseeki64(fd, 0, SEEK_CUR);
#else
    const auto position = ::lseek(fd, 0, SEEK_CUR);
#endif
    if (position < 0) return -1;
    if (position >= status.st_size) return 0;
    const auto count = std::min<std::uint64_t>(size, static_cast<std::uint64_t>(status.st_size - position));
#ifdef _WIN32
    const auto result = ::_write(fd, buffer, static_cast<unsigned int>(std::min<std::uint64_t>(count, std::numeric_limits<unsigned int>::max())));
#else
    const auto result = ::write(fd, buffer, static_cast<std::size_t>(count));
#endif
    return static_cast<std::int64_t>(result);
}

std::int64_t PwriteSharedMemory_nid_no_patch(int descriptor, const void* buffer, std::size_t size, std::int64_t offset) {
    SharedMemoryLock guard(descriptor);
    if (!guard.IsShared()) return -2;
    if (guard.Error() != 0) { errno = guard.Error(); return -1; }
    if (!guard.Refresh()) return -1;
    const auto fd = guard.Descriptor();
#ifdef _WIN32
    if (SharedMemoryDescriptorWritable_nid_no_patch(fd) == 0) { errno = EBADF; return -1; }
#else
    const int access = ::fcntl(fd, F_GETFL);
    if (access < 0) return -1;
    if ((access & O_ACCMODE) == O_RDONLY) { errno = EBADF; return -1; }
#endif
    const auto& status = guard.status;
    if (offset < 0) { errno = EINVAL; return -1; }
    if (offset >= status.st_size) return 0;
    const auto count = std::min<std::uint64_t>(size, static_cast<std::uint64_t>(status.st_size - offset));
#ifdef _WIN32
    const auto handle = reinterpret_cast<HANDLE>(::_get_osfhandle(fd));
    if (handle == INVALID_HANDLE_VALUE || ::GetFileType(handle) != FILE_TYPE_DISK) { errno = EBADF; return -1; }
    LARGE_INTEGER previous{};
    if (!::SetFilePointerEx(handle, LARGE_INTEGER{}, &previous, FILE_CURRENT)) { errno = EIO; return -1; }
    OVERLAPPED overlapped{};
    overlapped.Offset = static_cast<DWORD>(offset);
    overlapped.OffsetHigh = static_cast<DWORD>(static_cast<std::uint64_t>(offset) >> 32u);
    DWORD written = 0;
    const BOOL succeeded = ::WriteFile(handle, buffer, static_cast<DWORD>(std::min<std::uint64_t>(count, std::numeric_limits<DWORD>::max())), &written, &overlapped);
    const DWORD writeError = succeeded ? ERROR_SUCCESS : ::GetLastError();
    if (!::SetFilePointerEx(handle, previous, nullptr, FILE_BEGIN)) throw std::runtime_error("pwrite: restoring the host file position failed");
    if (!succeeded) { errno = writeError == ERROR_ACCESS_DENIED ? EBADF : EIO; return -1; }
    const auto result = written;
#else
    const auto result = ::pwrite(fd, buffer, static_cast<std::size_t>(count), static_cast<off_t>(offset));
#endif
    return static_cast<std::int64_t>(result);
}

std::int64_t SeekSharedMemory_nid_no_patch(int descriptor, std::int64_t offset, int whence) {
    SharedMemoryLock guard(descriptor);
    if (!guard.IsShared()) return -2;
    if (guard.Error() != 0) { errno = guard.Error(); return -1; }
    if (!guard.Refresh()) return -1;
    const auto fd = guard.Descriptor();
    const auto& status = guard.status;
    std::int64_t base = 0;
    if (whence == SEEK_CUR) {
#ifdef _WIN32
        base = ::_lseeki64(fd, 0, SEEK_CUR);
#else
        base = ::lseek(fd, 0, SEEK_CUR);
#endif
        if (base < 0) return -1;
    } else if (whence == SEEK_END) {
        base = static_cast<std::int64_t>(status.st_size);
    } else if (whence != SEEK_SET) {
        errno = EINVAL;
        return -1;
    }
    if ((offset > 0 && base > std::numeric_limits<std::int64_t>::max() - offset) ||
        (offset < 0 && base < std::numeric_limits<std::int64_t>::min() - offset)) {
        errno = EOVERFLOW;
        return -1;
    }
    const auto target = base + offset;
    if (target < 0 || target > status.st_size) {
        errno = EINVAL;
        return -1;
    }
#ifdef _WIN32
    return ::_lseeki64(fd, target, SEEK_SET);
#else
    return ::lseek(fd, static_cast<off_t>(target), SEEK_SET);
#endif
}

#ifndef _WIN32
static std::int64_t TransferSharedMemoryVectors(int descriptor, const struct iovec* vectors, int count,
                                                std::int64_t offset, bool positioned) {
    SharedMemoryLock guard(descriptor);
    if (!guard.IsShared()) return -2;
    if (guard.Error() != 0) { errno = guard.Error(); return -1; }
    const auto fd = guard.Descriptor();
    const int access = ::fcntl(fd, F_GETFL);
    if (access < 0) return -1;
    if ((access & O_ACCMODE) == O_RDONLY) { errno = EBADF; return -1; }
    if (positioned && offset < 0) { errno = EINVAL; return -1; }
    struct stat status{};
    if (::fstat(fd, &status) != 0) return -1;
    std::uint64_t total = 0;
    constexpr auto maxTransfer = static_cast<std::uint64_t>(std::numeric_limits<ssize_t>::max());
    for (int i = 0; i < count; ++i) {
        if (vectors[i].iov_len > maxTransfer - total) { errno = EINVAL; return -1; }
        total += vectors[i].iov_len;
    }
    std::uint64_t available = 0;
    if (positioned) {
        if (offset < status.st_size) available = static_cast<std::uint64_t>(status.st_size - offset);
        if (total > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max() - offset)) { errno = EINVAL; return -1; }
    } else {
        const auto position = ::lseek(fd, 0, SEEK_CUR);
        if (position < 0) return -1;
        if (position < status.st_size) available = static_cast<std::uint64_t>(status.st_size - position);
    }
    std::vector<struct iovec> bounded;
    bounded.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count && available > 0; ++i) {
        const auto length = static_cast<std::size_t>(std::min<std::uint64_t>(vectors[i].iov_len, available));
        if (length != 0) bounded.push_back({vectors[i].iov_base, length});
        available -= length;
    }
    if (bounded.empty()) return 0;
    return positioned ? ::pwritev(fd, bounded.data(), static_cast<int>(bounded.size()), static_cast<off_t>(offset))
                      : ::writev(fd, bounded.data(), static_cast<int>(bounded.size()));
}

std::int64_t WritevSharedMemory_nid_no_patch(int descriptor, const struct iovec* vectors, int count) {
    return TransferSharedMemoryVectors(descriptor, vectors, count, 0, false);
}

std::int64_t PwritevSharedMemory_nid_no_patch(int descriptor, const struct iovec* vectors, int count, std::int64_t offset) {
    return TransferSharedMemoryVectors(descriptor, vectors, count, offset, true);
}
#endif

int TruncateSharedMemory_nid_no_patch(int descriptor, std::int64_t length) {
#ifdef _WIN32
    const auto positioned = LockPositionedTransfer(descriptor);
#endif
    SharedMemoryLock guard(descriptor);
    if (!guard.IsShared()) return -2;
    if (guard.Error() != 0) {
        errno = guard.Error();
#ifdef _WIN32
        return errno;
#else
        return -1;
#endif
    }
    const auto fd = guard.Descriptor();
#ifdef _WIN32
    const int result = static_cast<int>(::_chsize_s(fd, length));
#else
    const int result = ::ftruncate(fd, static_cast<off_t>(length));
#endif
    return result;
}

}

extern "C" int APS5_VABI shm_open_nid_postfix(const char* path, int flags, int mode) {
    const int accessMode = flags & SCE_KERNEL_O_ACCMODE;
    if (accessMode != SCE_KERNEL_O_RDONLY && accessMode != SCE_KERNEL_O_RDWR) return Fail(GuestInvalid);
    if ((flags & ~AcceptedFlags) != 0) return Fail(GuestInvalid);
    if (path != anonymousObject) {
        std::string copy;
        if (const int error = CopyGuestPath_nid_no_patch(path, copy)) return Fail(error);
        path = copy.c_str();
#ifdef _WIN32
        return OpenNamedFile(path, flags, mode);
#else
        return OpenNamedFile(path, flags, mode);
#endif
    }
    if (accessMode == SCE_KERNEL_O_RDONLY) return Fail(GuestInvalid);
    const int descriptor = CreateAnonymousFile();
    if (descriptor >= 0) return descriptor;
    const int error = errno;
    if (const int guest = GuestError(error)) return Fail(guest);
    throw std::runtime_error("shm_open: creating the host anonymous file failed, errno=" + std::to_string(error));
}

extern "C" int APS5_VABI shm_unlink_nid_postfix(const char* path) {
    std::string copy;
    if (const int error = CopyGuestPath_nid_no_patch(path, copy)) return Fail(error);
    path = copy.c_str();
#ifdef _WIN32
    return UnlinkNamedFile(path);
#else
    int validationError = 0;
    const auto hostName = NamedHostName(path, validationError);
    if (validationError != 0) return Fail(NamedGuestError(validationError) ? NamedGuestError(validationError) : GuestInvalid);
    HostShmLock namespaceLock;
    if (namespaceLock.Error() != 0) {
        errno = namespaceLock.Error();
        if (const int guest = NamedGuestError(errno)) return Fail(guest);
        throw std::runtime_error("shm_unlink: locking the host shared-memory namespace failed, errno=" + std::to_string(errno));
    }
    const auto objectPath = "/dev/shm" + hostName;
    if (::faccessat(AT_FDCWD, objectPath.c_str(), R_OK | W_OK, AT_EACCESS) != 0) {
        const int error = errno;
        if (const int guest = NamedGuestError(error)) return Fail(guest);
        throw std::runtime_error("shm_unlink: checking shared-memory permissions failed, errno=" + std::to_string(error));
    }
    if (::shm_unlink(hostName.c_str()) == 0) return 0;
    const int error = errno;
    if (const int guest = NamedGuestError(error)) return Fail(guest);
    throw std::runtime_error("shm_unlink: removing the host named object failed, errno=" + std::to_string(error));
#endif
}
