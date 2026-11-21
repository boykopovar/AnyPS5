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
extern "C" int APS5_VABI pipe_nid_postfix(int* descriptors) {
    return pipe2_nid_postfix(descriptors, 0);
}

static int PathError(const char* path, std::string& copy) {
    if (const int error = CopyGuestPath_nid_no_patch(path, copy)) return error;
    return copy.empty() ? GUEST_ENOENT : 0;
}

#ifdef _WIN32
bool DescriptorIsOpen_nid_no_patch(int descriptor) {
    return descriptor >= 0 && WithoutParameterHandler([descriptor] {
        const auto handle = ::_get_osfhandle(descriptor);
        if (handle == -1) return -1;
        DWORD flags = 0;
        return handle == -2 || GetHandleInformation(reinterpret_cast<HANDLE>(handle), &flags) ? 0 : -1;
    }) == 0;
}
#else
bool DescriptorIsOpen_nid_no_patch(int descriptor) {
    return descriptor >= 0 && ::fcntl(descriptor, F_GETFD) >= 0;
}
#endif

static std::filesystem::path WithoutTrailingSeparator(const std::filesystem::path& native) {
    return native.has_filename() ? native : native.parent_path();
}

static bool HasNonDirectoryPrefix(const std::filesystem::path& native) {
    std::error_code error;
    for (auto prefix = WithoutTrailingSeparator(native).parent_path(); prefix.has_relative_path(); prefix = prefix.parent_path()) {
        const auto link = LinkPrefixState_nid_no_patch(prefix);
        if (link != LinkPrefix_nid_no_patch::Resolves) return link == LinkPrefix_nid_no_patch::BlockedByFile;
        const auto status = std::filesystem::status(prefix, error);
        if (std::filesystem::exists(status)) return !std::filesystem::is_directory(status);
    }
    return false;
}

int File::DuplicateFrom(int fd, int start) {
#ifdef _WIN32
    GuestResourceLimit openFileLimit{};
    if (getrlimit_nid_postfix(8, &openFileLimit) == 0 && start >= openFileLimit.rlim_cur) return PosixFailure(GUEST_EINVAL);
    std::vector<int> below;
    int duplicate;
    while ((duplicate = dup_nid_postfix(fd)) >= 0 && duplicate < start) below.push_back(duplicate);
    const int error = *__error_nid_postfix();
    for (const int descriptor : below) close_nid_postfix(descriptor);
    return duplicate < 0 ? PosixFailure(error) : duplicate;
#else
    const int duplicate = ::fcntl(fd, F_DUPFD, start);
    if (duplicate < 0) return PosixFailure(SceErrorFromErrno(errno) & 0xffff);
    File::DuplicateDirectoryDescriptor(fd, duplicate);
    if (File::IsRandomDevice(fd)) File::RememberRandomDevice(duplicate);
    File::ShareDescriptor(fd, duplicate);
    return duplicate;
#endif
}

static int LookupError(const std::filesystem::path& native) {
#ifdef _WIN32
    std::error_code pathError;
    const auto verbatim = NativeVerbatimPath_nid_no_patch(native, pathError);
    const auto& checkNative = pathError ? native : verbatim;
    const auto named = checkNative.has_filename() ? checkNative : WithoutTrailingSeparator(checkNative);
    const DWORD attributes = GetFileAttributesW(named.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        FileStat target{};
        if (!File::TryFillFileStat(named, &target)) return SceErrorFromErrno(errno) & 0xffff;
        return checkNative.has_filename() || (target.st_mode & 0170000) == 0040000 ? 0 : GUEST_ENOTDIR;
    }
    const auto& statusNative = checkNative;
#else
    const auto& statusNative = native;
#endif
    std::error_code error;
    const auto status = std::filesystem::status(statusNative, error);
    if (std::filesystem::exists(status)) return statusNative.has_filename() || std::filesystem::is_directory(status) ? 0 : GUEST_ENOTDIR;
    if (status.type() != std::filesystem::file_type::not_found) return SceErrorFromErrno(error.default_error_condition().value()) & 0xffff;
    const bool fileWithSeparator = !statusNative.has_filename() && std::filesystem::exists(WithoutTrailingSeparator(statusNative), error);
    return fileWithSeparator || HasNonDirectoryPrefix(statusNative) ? GUEST_ENOTDIR : GUEST_ENOENT;
}

static constexpr std::pair<std::uint64_t, const char*> GUEST_FILE_FLAGS[] = {
    {0x00000001, "UF_NODUMP"}, {0x00000002, "UF_IMMUTABLE"}, {0x00000004, "UF_APPEND"}, {0x00000008, "UF_OPAQUE"},
    {0x00000010, "UF_NOUNLINK"}, {0x00000080, "UF_SYSTEM"}, {0x00000100, "UF_SPARSE"}, {0x00000200, "UF_OFFLINE"},
    {0x00000400, "UF_REPARSE"}, {0x00000800, "UF_ARCHIVE"}, {0x00001000, "UF_READONLY"}, {0x00008000, "UF_HIDDEN"},
    {0x00010000, "SF_ARCHIVED"}, {0x00020000, "SF_IMMUTABLE"}, {0x00040000, "SF_APPEND"}, {0x00100000, "SF_NOUNLINK"},
    {0x00200000, "SF_SNAPSHOT"}};
static constexpr std::uint64_t GUEST_SF_SETTABLE = 0xffff0000;
static constexpr std::uint64_t GUEST_UNSUPPORTED_USER_FLAGS[] = {0x00000001, 0x00000008};

static std::string FileFlagNames(std::uint64_t flags) {
    std::string names;
    for (const auto& [flag, name] : GUEST_FILE_FLAGS) {
        if ((flags & flag) == 0) continue;
        if (!names.empty()) names += '|';
        names += name;
    }
    return names;
}

static int SetFileFlags(const char* function, const std::string& target, std::uint64_t flags) {
    std::uint64_t known = 0;
    for (const auto& entry : GUEST_FILE_FLAGS) known |= entry.first;
    if ((flags & ~known) != 0) return GUEST_EOPNOTSUPP;
    if ((flags & GUEST_SF_SETTABLE) != 0) return GUEST_EPERM;
    for (const std::uint64_t unsupported : GUEST_UNSUPPORTED_USER_FLAGS)
        if (flags == unsupported) return GUEST_EOPNOTSUPP;
    if (flags != 0)
        throw std::runtime_error(std::string(function) + ": setting " + FileFlagNames(flags) + " on " + target + " is not supported, the host keeps no BSD file flags");
    return 0;
}


int APS5_VABI chmod_nid_postfix(const char* path, int mode) {
    std::string copy;
    if (const int error = PathError(path, copy)) return PosixFailure(error);
    path = copy.c_str();
    std::filesystem::path native;
    if (const int error = ResolvePathError_nid_no_patch(path, native)) return PosixFailure(error);
    if (NativeChmod(native, mode & 07777) != 0) return PosixResult(SceErrorFromErrno(errno == ENOENT ? NotFoundError_nid_no_patch(native) : errno));
    return 0;
}

int ChmodNative_nid_no_patch(const std::filesystem::path& native, int mode) {
    return NativeChmod(native, mode);
}

int ChangeFileFlags_nid_no_patch(const char* function, const char* path, std::uint64_t flags, bool follow) {
    std::string copy;
    if (const int error = PathError(path, copy)) return PosixFailure(error);
    path = copy.c_str();
    std::filesystem::path native;
    if (const int error = ResolvePathError_nid_no_patch(path, native)) return PosixFailure(error);
    FileStat linkStatus{};
    const bool link = !follow && native.has_filename() && File::FillLinkStat(native, &linkStatus) && (linkStatus.st_mode & 0170000) == 0120000;
    if (const int error = link ? 0 : LookupError(native)) return PosixFailure(error);
    if (const int error = SetFileFlags(function, path, flags)) return PosixFailure(error);
    return 0;
}

int APS5_VABI chflags_nid_postfix(const char* path, std::uint64_t flags) {
    return ChangeFileFlags_nid_no_patch("chflags", path, flags, true);
}

int APS5_VABI fchflags_nid_postfix(int d, std::uint64_t flags) {
    if (File::IsVirtualDirectoryDescriptor(d)) return PosixFailure(GUEST_EISDIR);
    if (GuestSockets::IsOpen(d) || IsKqueueDescriptor_nid_no_patch(d) || File::IsSharedMemoryDescriptor_nid_no_patch(d)) return PosixFailure(GUEST_EINVAL);
    File::RefuseArchivedFile(d, "fchflags");
    if (!File::IsVnode(d)) {
        if (!DescriptorIsOpen_nid_no_patch(d)) return PosixFailure(GUEST_EBADF);
        if (NativeIsPipe(d)) return PosixFailure(GUEST_EINVAL);
        throw std::runtime_error("fchflags: descriptor " + std::to_string(d) + " is neither a file, a directory, a pipe, a socket nor shared memory");
    }
    if (const int error = SetFileFlags("fchflags", "descriptor " + std::to_string(d), flags)) return PosixFailure(error);
    return 0;
}

int APS5_VABI nmount_nid_postfix(const KernelIovec* iov, unsigned int niov, int flags) {
    (void)iov;
    (void)niov;
    (void)flags;
    NotImplemented_nid_no_patch("nmount: mounting file systems");
    return -1;
}

int APS5_VABI close_nid_postfix(int d) {
    if (File::IsHostStream(d)) return PosixFailure(GUEST_EPERM);
    int socketError = 0;
    if (KernelCloseSocket_nid_no_patch(d, &socketError)) {
        if (socketError == 0) return 0;
        *__error_nid_postfix() = socketError;
        return -1;
    }
    if (GuestSockets::IsOpen(d)) return GuestSockets::Close(d);
    EqueueDescriptorClosed(d);
#ifdef _WIN32
    const auto positioned = File::LockPositionedTransfer(d);
#endif
    File::ForgetClosedDescriptor(d);
#ifdef _WIN32
    const int shared = File::CloseSharedMemoryDescriptor_nid_no_patch(d);
    if (shared != -2) return shared;
    return WithoutParameterHandler([d] { return ::_close(d); });
#else
    return ::close(d);
#endif
}

int APS5_VABI _close_nid_postfix(int descriptor) {
    return close_nid_postfix(descriptor);
}

}

namespace {
std::atomic<int> g_hostStandardError{-1};
std::atomic<int> g_hostStandardOutput{-1};

void PreserveHostStream(int standard, std::atomic<int>& slot) {
    if (slot.load() >= 0) return;
    const int saved = standard == 1 ? KeepHostStandardOutput_nid_no_patch() : KeepHostStandardError_nid_no_patch();
    if (saved < 0) throw std::runtime_error(standard == 1 ? "dup2: cannot keep the host standard output" : "dup2: cannot keep the host standard error");
    slot.store(saved);
}

void PreserveHostStandardError() { PreserveHostStream(2, g_hostStandardError); }
void PreserveHostStandardOutput() { PreserveHostStream(1, g_hostStandardOutput); }
}

int File::HostStandardError() {
    return g_hostStandardError.load();
}

extern "C" {

int APS5_VABI dup_nid_postfix(int d) {
    if (GuestSockets::IsOpen(d)) return GuestSockets::Duplicate(d);
    if (d < 0) return PosixFailure(GUEST_EBADF);
    if (IsKqueueDescriptor_nid_no_patch(d)) throw std::runtime_error(std::string(__func__) + ": duplicating a kqueue descriptor is not supported");
    if (IsKsemDescriptor(d)) throw std::runtime_error(std::string(__func__) + ": duplicating a ksem id is not supported");
#ifdef _WIN32
    const int duplicate = WithoutParameterHandler([d] { return ::_dup(d); });
    if (duplicate >= 0) File::DuplicateSharedMemoryDescriptor_nid_no_patch(d, duplicate);
#else
    const int duplicate = ::dup(d);
#endif
    if (duplicate < 0) return PosixFailure(SceErrorFromErrno(errno) & 0xffff);
    File::DuplicateDirectoryDescriptor(d, duplicate);
    if (File::IsRandomDevice(d)) File::RememberRandomDevice(duplicate);
    File::ShareDescriptor(d, duplicate);
    DuplicateJitSharedMemory(d, duplicate);
    return duplicate;
}

int APS5_VABI sceKernelRdup(int pid, int fd) {
    if (pid != 0 && pid != getpid_nid_postfix())
        NotImplemented_nid_no_patch("sceKernelRdup: a pid other than the caller's and 0");
    if (fd < 0) return SCE_KERNEL_ERROR_EBADF;
    const int duplicate = dup_nid_postfix(fd);
    return duplicate < 0 ? SceErrorFromErrno(*__error_nid_postfix()) : duplicate;
}

int APS5_VABI dup2_nid_postfix(int from, int to) {
    if (from < 0 || to < 0) return PosixFailure(GUEST_EBADF);
    if (GuestSockets::IsOpen(from)) {
        if (from != to && to <= 2)
            throw std::runtime_error(std::string(__func__) + ": moving a guest socket onto standard descriptor " + std::to_string(to) + " is not supported");
        if (GuestSockets::DuplicateTo(from, to) < 0) return -1;
        if (from == to) return to;
        File::ForgetRandomDevice(to);
        File::ForgetArchivedFile(to);
        File::ForgetDirectoryDescriptor(to);
#ifdef _WIN32
        File::ForgetFileLock(to);
#endif
        return to;
    }
    const bool sourceIsRandom = File::IsRandomDevice(from);
    if (from != to && IsKqueueDescriptor_nid_no_patch(from)) throw std::runtime_error(std::string(__func__) + ": duplicating a kqueue descriptor is not supported");
    if (from != to && IsKsemDescriptor(from)) throw std::runtime_error(std::string(__func__) + ": duplicating a ksem id is not supported");
#ifdef _WIN32
    if (WithoutParameterHandler([from] { return ::_get_osfhandle(from) == -1 ? -1 : 0; }) != 0) return PosixFailure(GUEST_EBADF);
    if (from == to) return to;
    if (to == 1) PreserveHostStandardOutput();
    if (to == 2) PreserveHostStandardError();
    EqueueDescriptorClosed(to);
    auto positioned = File::LockPositionedTransfer(to);
    auto* const standard = HostStandardHandle_nid_no_patch(to);
    int duplicated = File::ReplaceSharedMemoryDescriptor_nid_no_patch(from, to);
    if (duplicated == -2) duplicated = WithoutParameterHandler([from, to] { return ::_dup2(from, to); });
    KeepStandardHandle_nid_no_patch(to, standard);
    if (duplicated != 0) return PosixFailure(SceErrorFromErrno(errno) & 0xffff);
    GuestSockets::Forget(to);
    File::ForgetDirectoryDescriptor(to);
    File::DuplicateDirectoryDescriptor(from, to);
    File::ForgetFileLock(to);
    File::ForgetRandomDevice(to);
    File::ForgetSharedMemoryDescriptor_nid_no_patch(to);
    File::DuplicateSharedMemoryDescriptor_nid_no_patch(from, to);
    if (sourceIsRandom) File::RememberRandomDevice(to);
    positioned.unlock();
    File::ShareDescriptor(from, to);
    DuplicateJitSharedMemory(from, to);
    return to;
#else
    if (from == to) {
        if (::fcntl(from, F_GETFL) < 0) return PosixFailure(GUEST_EBADF);
        return to;
    }
    if (to == 1) PreserveHostStandardOutput();
    if (to == 2) PreserveHostStandardError();
    const int result = ::dup2(from, to);
    if (result < 0) return PosixFailure(SceErrorFromErrno(errno) & 0xffff);
    File::ForgetDirectoryDescriptor(to);
    File::DuplicateDirectoryDescriptor(from, to);
    EqueueDescriptorClosed(to);
    GuestSockets::Forget(to);
    File::ForgetRandomDevice(to);
    if (sourceIsRandom) File::RememberRandomDevice(to);
    File::ShareDescriptor(from, to);
    DuplicateJitSharedMemory(from, to);
    return result;
#endif
}

#ifdef __linux__
static std::mutex g_flockTransitions;

static bool HoldsSharedFlock(int descriptor) {
    std::FILE* info = std::fopen(("/proc/self/fdinfo/" + std::to_string(descriptor)).c_str(), "re");
    if (info == nullptr) throw std::runtime_error("flock: cannot read the lock state of fd=" + std::to_string(descriptor) + ", errno=" + std::to_string(errno));
    char line[256];
    bool shared = false;
    while (std::fgets(line, sizeof(line), info) != nullptr)
        if (std::strncmp(line, "lock:", 5) == 0 && std::strstr(line, " FLOCK ") != nullptr && std::strstr(line, " READ ") != nullptr) shared = true;
    std::fclose(info);
    return shared;
}
#endif

int APS5_VABI flock_nid_postfix(int d, int operation) {
    if (File::IsVirtualDirectoryDescriptor(d)) return PosixFailure(GUEST_EISDIR);
    if (GuestSockets::IsOpen(d) || IsKqueueDescriptor_nid_no_patch(d)) return PosixFailure(GUEST_EOPNOTSUPP);
    File::RefuseArchivedFile(d, "flock");
#ifdef _WIN32
    const auto handle = reinterpret_cast<HANDLE>(WithoutParameterHandler([d] { return ::_get_osfhandle(d); }));
    if (handle == INVALID_HANDLE_VALUE) return PosixFailure(GUEST_EBADF);
    if (::GetFileType(handle) == FILE_TYPE_PIPE) return PosixFailure(GUEST_EOPNOTSUPP);
#else
    struct stat status;
    if (::fstat(d, &status) != 0) return PosixFailure(GUEST_EBADF);
    if (S_ISFIFO(status.st_mode)) return PosixFailure(GUEST_EOPNOTSUPP);
#endif
    const int type = operation & 8 ? 8 : operation & 2 ? 2 : operation & 1 ? 1 : 0;
    if (type == 0) return PosixFailure(GUEST_EBADF);
#ifdef __linux__
    std::unique_lock serial(g_flockTransitions, std::defer_lock);
    if ((operation & 4) || type == 8) serial.lock();
    const bool keepShared = type == 2 && (operation & 4) && HoldsSharedFlock(d);
#endif
    if (NativeFlock(d, type | (operation & 4)) != 0) {
#ifdef _WIN32
        const auto error = ::GetLastError();
        if ((operation & 4) && error == ERROR_LOCK_VIOLATION) return PosixFailure(GUEST_EWOULDBLOCK);
        throw std::runtime_error(std::string(__func__) + ": flock failed, fd=" + std::to_string(d) + ", error=" + std::to_string(error));
#else
        const int error = errno;
        if ((operation & 4) && error == EWOULDBLOCK) {
#ifdef __linux__
            if (keepShared && ::flock(d, LOCK_SH | LOCK_NB) != 0)
                throw std::runtime_error(std::string(__func__) + ": cannot take back the shared lock the host dropped in a failed upgrade, fd=" + std::to_string(d) + ", errno=" + std::to_string(errno));
#endif
            return PosixFailure(GUEST_EWOULDBLOCK);
        }
        throw std::runtime_error(std::string(__func__) + ": flock failed, fd=" + std::to_string(d) + ", errno=" + std::to_string(error));
#endif
    }
    return 0;
}

int APS5_VABI _fstatfs_nid_postfix(int d, KernelStatfs* buf) {
    if (File::IsVirtualDirectoryDescriptor(d)) return PosixFailure(GUEST_EOPNOTSUPP);
    if (GuestSockets::IsOpen(d) || IsKqueueDescriptor_nid_no_patch(d)) return PosixFailure(GUEST_EINVAL);
    if (d < 0) return PosixFailure(GUEST_EBADF);
    File::RefuseArchivedFile(d, "fstatfs");
    KernelStatfs result{};
    result.f_version = KERNEL_STATFS_VERSION;
    if (const int error = NativeFstatfs(d, &result)) return PosixFailure(error);
    if (!GuestCopyOut_nid_no_patch(buf, &result, sizeof(result))) return PosixFailure(GUEST_EFAULT);
    return 0;
}


int PosixStatfs_nid_no_patch(const char* path, KernelStatfs* buf) {
    std::string copy;
    if (const int error = PathError(path, copy)) return PosixFailure(error);
    path = copy.c_str();
    std::filesystem::path native;
    if (const int error = ResolvePathError_nid_no_patch(path, native)) return PosixFailure(error);
    AppArchiveRefuseOnly_nid_no_patch(native, path, "statfs");
#ifdef _WIN32
    std::error_code error;
    const auto status = std::filesystem::status(native, error);
    if (const int failure = PathLookupFailure(error)) return PosixResult(failure);
    if (!std::filesystem::exists(status)) return PosixFailure(NotFoundError_nid_no_patch(native));
#endif
    KernelStatfs result{};
    result.f_version = KERNEL_STATFS_VERSION;
    if (const int error = NativeStatfs(native, &result)) return PosixFailure(error);
    return GuestCopyOut_nid_no_patch(buf, &result, sizeof(result)) ? 0 : PosixFailure(GUEST_EFAULT);
}

int64_t APS5_VABI fstat_nid_disambig1_nid_postfix(int d, FileStat* sb) {
    if (sb == nullptr) {
        FileStat ignored{};
        if (fstat_nid_disambig1_nid_postfix(d, &ignored) != 0) return -1;
        return PosixFailure(GUEST_EFAULT);
    }
    FileStat result{};
    if (IsKqueueDescriptor_nid_no_patch(d)) {
        result.st_mode = 0010000;
        if (!GuestCopyOut_nid_no_patch(sb, &result, sizeof(result))) return PosixFailure(GUEST_EFAULT);
        return 0;
    }
    if (GuestSockets::IsOpen(d)) {
        if (const int error = GuestSockets::Stat(d, &result)) return PosixFailure(error);
    } else if (!File::FillFileStatFromDescriptor(d, &result)) {
        return PosixFailure(SceErrorFromErrno(errno) & 0xffff);
    }
    if (!GuestCopyOut_nid_no_patch(sb, &result, sizeof(result))) return PosixFailure(GUEST_EFAULT);
    return 0;
}

int64_t APS5_VABI _fstat_nid_postfix(int d, FileStat* sb) {
    return fstat_nid_disambig1_nid_postfix(d, sb);
}

int APS5_VABI ftruncate_nid_postfix(int d, int64_t length) {
    if (File::IsDirectoryDescriptor(d)) return PosixFailure(GUEST_EINVAL);
    if (length < 0 || GuestSockets::IsOpen(d)) {
        return PosixFailure(GUEST_EINVAL);
    }
    if (File::IsArchivedFile(d)) return PosixFailure(GUEST_EINVAL);
#ifdef _WIN32
    int error = NativeFtruncate(d, length);
    if (error != 0) {
        if (error == EACCES) error = GUEST_EINVAL;
        return PosixFailure(SceErrorFromErrno(error) & 0xffff);
    }
#else
    if (NativeFtruncate(d, length) != 0) {
        return PosixFailure(SceErrorFromErrno(errno) & 0xffff);
    }
#endif
    return 0;
}

int APS5_VABI sceKernelFtruncate(int d, int64_t length) {
    return ftruncate_nid_postfix(d, length) == 0 ? 0 : SceErrorFromErrno(*__error_nid_postfix());
}

int64_t APS5_VABI lseek_nid_postfix(int d, int64_t offset, int whence) {
    if (!GuestSockets::IsOpen(d) && !DescriptorIsOpen_nid_no_patch(d)) return PosixFailure(GUEST_EBADF);
    if (whence < 0 || whence > 2) return PosixFailure(GUEST_EINVAL);
    return PosixResult(sceKernelLseek(d, offset, whence));
}

int APS5_VABI mkdir_nid_postfix(const char* path, uint16_t mode) {
    std::string copy;
    if (const int error = PathError(path, copy)) return PosixFailure(error);
    path = copy.c_str();
    std::filesystem::path native;
    if (const int error = ResolvePathError_nid_no_patch(path, native)) return PosixFailure(error);
    if (NativeMkdir(native, mode) != 0) {
        const int nativeError = errno;
#ifdef _WIN32
        if (File::WindowsSymbolicLinkLoop(WithoutTrailingSeparator(native).parent_path())) return PosixFailure(GUEST_ELOOP);
#endif
        const bool blockedByFile = (nativeError == ENOENT || nativeError == EINVAL) && HasNonDirectoryPrefix(native);
        return PosixResult(SceErrorFromErrno(blockedByFile ? GUEST_ENOTDIR : nativeError));
    }
    RecordWrittenPath_nid_no_patch(native);
    return 0;
}

int APS5_VABI open_nid_postfix(const char* path, int flags, int mode) {
    if (File::InvalidOpenFlags(flags)) return PosixFailure(GUEST_EINVAL);
    std::string copy;
    if (const int error = PathError(path, copy)) return PosixFailure(error);
    path = copy.c_str();
    return PosixResult(sceKernelOpen(path, flags, static_cast<std::uint16_t>(mode)));
}

int APS5_VABI creat_nid_postfix(const char* path, int mode) {
    return open_nid_postfix(path, SCE_KERNEL_O_WRONLY | SCE_KERNEL_O_CREAT | SCE_KERNEL_O_TRUNC, mode);
}

int APS5_VABI _open_nid_postfix(const char* path, int flags, ...) {
    int mode = 0;
    if (flags & SCE_KERNEL_O_CREAT) {
#ifdef _WIN32
        __builtin_sysv_va_list arguments;
        __builtin_sysv_va_start(arguments, flags);
        mode = __builtin_va_arg(arguments, int);
        __builtin_sysv_va_end(arguments);
#else
        std::va_list arguments;
        va_start(arguments, flags);
        mode = va_arg(arguments, int);
        va_end(arguments);
#endif
    }
    return open_nid_postfix(path, flags, mode);
}

int64_t APS5_VABI pread_nid_postfix(int d, void* buf, size_t nbytes, int64_t offset) {
    if (File::IsDirectoryDescriptor(d)) return PosixFailure(GUEST_EISDIR);
    if (ExceedsTransferLimit(nbytes)) return PosixFailure(GUEST_EINVAL);
    if (const int error = PositionedArgumentError(d, false, buf == nullptr && nbytes != 0, offset)) return PosixFailure(error);
    if (FailPosixPositionedGuestSocket(d)) return -1;
    const GuestArena::HostWrite destination(buf, nbytes);
    if (!destination.Open()) return PosixFailure(GUEST_EFAULT);
    if (File::ReadRandomDevice(d, buf, nbytes)) return static_cast<int64_t>(nbytes);
    if (const auto archived = File::ReadArchivedFile(d, buf, nbytes, &offset)) return *archived < 0 ? PosixFailure(SceErrorFromErrno(static_cast<int>(-*archived)) & 0xffff) : *archived;
    char empty = 0;
    const auto result = NativePread(d, buf == nullptr ? &empty : buf, nbytes, offset);
    return result < 0 ? PosixFailure(SceErrorFromErrno(errno) & 0xffff) : result;
}

int64_t APS5_VABI pwrite_nid_disambig1_nid_postfix(int d, const void* buf, size_t nbytes, int64_t offset) {
    return PosixPwrite_nid_no_patch(d, buf, nbytes, offset);
}

std::int64_t PosixPwrite_nid_no_patch(int d, const void* buf, std::size_t nbytes, std::int64_t offset) {
    if (File::IsVirtualDirectoryDescriptor(d)) return PosixFailure(GUEST_EISDIR);
    if (ExceedsTransferLimit(nbytes)) return PosixFailure(GUEST_EINVAL);
    if (const int error = PositionedArgumentError(d, true, buf == nullptr && nbytes != 0, offset)) return PosixFailure(error);
    if (FailPosixPositionedGuestSocket(d)) return -1;
    const File::WriteSync sync(d);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(buf, nbytes);
    char empty = 0;
    const auto result = NativePwrite(d, buf == nullptr ? &empty : buf, nbytes, offset);
    return result < 0 ? PosixFailure(SceErrorFromErrno(errno) & 0xffff) : result;
}

int64_t APS5_VABI read_nid_postfix(int d, void* buf, uint64_t nbytes) {
    if (ExceedsTransferLimit(nbytes)) return PosixFailure(GUEST_EINVAL);
    if (GuestSockets::IsOpen(d)) return GuestSockets::Read(d, buf, static_cast<size_t>(nbytes));
    if (const int error = TransferArgumentError(d, buf == nullptr && nbytes != 0)) return PosixFailure(error);
    char emptyBuffer = 0;
    return PosixResult(sceKernelRead(d, buf == nullptr ? &emptyBuffer : buf, static_cast<size_t>(nbytes)));
}

std::int64_t APS5_VABI _read_nid_postfix(int descriptor, void* buffer, std::size_t count) {
    if (ExceedsTransferLimit(count)) return PosixFailure(GUEST_EINVAL);
    if (GuestSockets::IsOpen(descriptor)) return GuestSockets::Read(descriptor, buffer, count);
    if (const int error = TransferArgumentError(descriptor, buffer == nullptr && count != 0)) return PosixFailure(error);
    char emptyBuffer = 0;
    return PosixResult(sceKernelRead(descriptor, buffer == nullptr ? &emptyBuffer : buffer, count));
}

int64_t APS5_VABI write_nid_postfix(int d, const char* str, int64_t size) {
    if (size < 0) return PosixFailure(GUEST_EINVAL);
    if (GuestSockets::IsOpen(d)) return GuestSockets::Write(d, str, static_cast<std::size_t>(size));
    if (const int error = TransferArgumentError(d, str == nullptr && size != 0)) return PosixFailure(error);
    const char emptyBuffer = 0;
    return PosixResult(sceKernelWrite(d, str == nullptr ? &emptyBuffer : str, static_cast<size_t>(size)));
}

std::int64_t APS5_VABI _write_nid_postfix(int descriptor, const void* buffer, std::size_t count) {
    if (ExceedsTransferLimit(count)) return PosixFailure(GUEST_EINVAL);
    if (GuestSockets::IsOpen(descriptor)) return GuestSockets::Write(descriptor, buffer, count);
    if (const int error = TransferArgumentError(descriptor, buffer == nullptr && count != 0)) return PosixFailure(error);
    const char emptyBuffer = 0;
    return PosixResult(sceKernelWrite(descriptor, buffer == nullptr ? &emptyBuffer : buffer, count));
}

const bool g_libcWriteHookRegistered = (LibcSetGuestWriteHook_nid_no_patch(&_write_nid_postfix), true);

int APS5_VABI stat_nid_postfix(const char* path, FileStat* sb) {
    if (sb == nullptr) return PosixFailure(GUEST_EFAULT);
    std::string copy;
    if (const int error = PathError(path, copy)) return PosixFailure(error);
    path = copy.c_str();
    return PosixResult(sceKernelStat(path, sb));
}

int APS5_VABI lstat_nid_postfix(const char* path, FileStat* sb) {
    if (sb == nullptr) return PosixFailure(GUEST_EFAULT);
    std::string copy;
    if (const int error = PathError(path, copy)) return PosixFailure(error);
    path = copy.c_str();
    std::filesystem::path native;
    if (const int error = ResolvePathError_nid_no_patch(path, native, true)) return PosixFailure(error);
    FileStat result{};
    if (!File::FillLinkStat(native, &result)) return PosixResult(SceErrorFromErrno(errno == ENOENT ? NotFoundError_nid_no_patch(native) : errno));
    if (!GuestCopyOut_nid_no_patch(sb, &result, sizeof(result))) return PosixFailure(GUEST_EFAULT);
    return 0;
}

int APS5_VABI unlink_nid_postfix(const char* path) {
    std::string copy;
    if (const int error = PathError(path, copy)) return PosixFailure(error);
    path = copy.c_str();
    return PosixResult(sceKernelUnlink(path));
}

int APS5_VABI sceKernelCheckReachability(const char* path) {
    if (path == nullptr) return SCE_KERNEL_ERROR_EINVAL;
    std::string copy;
    if (const int error = CopyGuestPath_nid_no_patch(path, copy)) return SceKernelError(error);
    path = copy.c_str();
    if (copy.empty()) return SCE_KERNEL_ERROR_ENOENT;
    if (std::strlen(path) > 255) return SCE_KERNEL_ERROR_ENAMETOOLONG;
    std::filesystem::path native;
    if (const int error = ResolvePathError_nid_no_patch(path, native, true, true)) return SceKernelError(error);
    if (AppArchiveOnly_nid_no_patch(native)) return 0;
#ifdef _WIN32
    std::error_code pathError;
    const auto verbatim = NativeVerbatimPath_nid_no_patch(native, pathError);
    const auto& checkNative = pathError ? native : verbatim;
#else
    const auto& checkNative = native;
#endif
    std::error_code error;
    return std::filesystem::exists(checkNative, error) ? 0 : SCE_KERNEL_ERROR_ENOENT;
}

int APS5_VABI sceKernelFstat(int d, FileStat* sb) {
    if (sb == nullptr) {
        FileStat ignored{};
        if (const int error = sceKernelFstat(d, &ignored)) return error;
        return SceErrorFromErrno(GUEST_EFAULT);
    }
    FileStat result{};
    if (IsKqueueDescriptor_nid_no_patch(d)) {
        result.st_mode = 0010000;
        if (!GuestCopyOut_nid_no_patch(sb, &result, sizeof(result))) return SceErrorFromErrno(GUEST_EFAULT);
        return 0;
    }
    if (GuestSockets::IsOpen(d)) {
        const int error = GuestSockets::Stat(d, &result);
        if (error != 0) return SceErrorFromErrno(error);
    } else if (!File::FillFileStatFromDescriptor(d, &result)) {
        return SceErrorFromErrno(errno);
    }
    if (!GuestCopyOut_nid_no_patch(sb, &result, sizeof(result))) return SceErrorFromErrno(GUEST_EFAULT);
    return 0;
}

int APS5_VABI sceKernelFsync(int fd) {
    if (File::IsVirtualDirectoryDescriptor(fd)) return SceErrorFromErrno(GUEST_EISDIR);
    if (GuestSockets::IsOpen(fd) || IsKqueueDescriptor_nid_no_patch(fd)) return SceErrorFromErrno(GUEST_EINVAL);
    File::RefuseArchivedFile(fd, "fsync");
#ifdef _WIN32
    if (NativeSync(fd) != 0) return SceErrorFromErrno(errno);
#else
    if (::fsync(fd) != 0) return SceErrorFromErrno(errno);
#endif
    return 0;
}

int APS5_VABI sceKernelFdatasync(int fd) {
    if (File::IsVirtualDirectoryDescriptor(fd)) return SceErrorFromErrno(GUEST_EISDIR);
    if (GuestSockets::IsOpen(fd) || IsKqueueDescriptor_nid_no_patch(fd)) return SceErrorFromErrno(GUEST_EINVAL);
    File::RefuseArchivedFile(fd, "fdatasync");
#ifdef _WIN32
    if (NativeSync(fd) != 0) return SceErrorFromErrno(errno);
#elif defined(__APPLE__)
    if (::fsync(fd) != 0) return SceErrorFromErrno(errno);
#else
    if (::fdatasync(fd) != 0) return SceErrorFromErrno(errno);
#endif
    return 0;
}

int APS5_VABI sceKernelWriteThrottlingStatus(std::uint64_t* status) {
    if (status == nullptr) return SceErrorFromErrno(GUEST_EINVAL);
    const std::uint64_t values[4]{std::numeric_limits<std::uint32_t>::max(), 0, 0, 0};
    if (!GuestCopyOut_nid_no_patch(status, values, sizeof(values))) return SceErrorFromErrno(GUEST_EFAULT);
    return 0;
}

#ifdef _WIN32

int APS5_VABI sceKernelGetdirentries(int fd, char* buf, int nbytes, int64_t* basep) {
    if (GuestSockets::IsOpen(fd) || File::IsArchivedFile(fd)) return SceErrorFromErrno(GUEST_EINVAL);
    if (buf == nullptr) return SceErrorFromErrno(GUEST_EFAULT);
    if (nbytes <= 0) return SceErrorFromErrno(GUEST_EINVAL);
    if (!File::DirectoryDescriptorPath(fd)) {
        if (WithoutParameterHandler([fd] { return ::_get_osfhandle(fd); }) == -1) return SceErrorFromErrno(GUEST_EBADF);
        return SceErrorFromErrno(GUEST_EINVAL);
    }
    return File::ReadDirectoryDescriptor(fd, buf, nbytes, basep);
}

int APS5_VABI sceKernelGetdents(int fd, char* buf, int nbytes) {
    return sceKernelGetdirentries(fd, buf, nbytes, nullptr);
}

#else

static bool InsideApp0(int fd) {
    const int savedErrno = errno;
    const bool inside = [fd] {
        const auto directory = File::DirectoryDescriptorPath(fd);
        if (!directory) return false;
        std::error_code error;
        const auto app0 = std::filesystem::weakly_canonical(ResolveDiskPath_nid_no_patch("/app0"), error);
        if (error || app0.empty()) return false;
        const auto relative = directory->lexically_relative(app0);
        return !relative.empty() && *relative.begin() != "..";
    }();
    errno = savedErrno;
    return inside;
}

#ifdef __APPLE__
static unsigned char GuestEntryType(int fd, const struct dirent& entry, bool followLinks) {
    if (entry.d_type != DT_LNK && entry.d_type != DT_UNKNOWN) return entry.d_type;
    if (entry.d_type == DT_LNK && !followLinks) return DT_LNK;
    const int savedErrno = errno;
    struct stat target{};
    const bool found = ::fstatat(fd, entry.d_name, &target, followLinks ? 0 : AT_SYMLINK_NOFOLLOW) == 0;
    errno = savedErrno;
    if (!found) return entry.d_type;
    if (S_ISDIR(target.st_mode)) return DT_DIR;
    if (S_ISREG(target.st_mode)) return DT_REG;
    if (S_ISLNK(target.st_mode)) return DT_LNK;
    return entry.d_type;
}

int APS5_VABI sceKernelGetdirentries(int fd, char* buf, int nbytes, int64_t* basep) {
    if (GuestSockets::IsOpen(fd) || File::IsArchivedFile(fd)) return SceErrorFromErrno(GUEST_EINVAL);
    constexpr std::size_t GuestHeaderBytes = 8;
    constexpr std::size_t GuestMaxName = 255;
    if (buf == nullptr) return SceErrorFromErrno(GUEST_EFAULT);
    if (nbytes <= 0) return SceErrorFromErrno(GUEST_EINVAL);
    if (File::IsDirectoryDescriptor(fd)) return File::ReadDirectoryDescriptor(fd, buf, nbytes, basep);
    struct stat status{};
    if (::fstat(fd, &status) != 0) return SceErrorFromErrno(errno);
    if (!S_ISDIR(status.st_mode)) return SceErrorFromErrno(GUEST_EINVAL);
    const off_t base = ::lseek(fd, 0, SEEK_CUR);
    if (base < 0) return SceErrorFromErrno(errno);
    if (::lseek(fd, 0, SEEK_SET) < 0) return SceErrorFromErrno(errno);
    std::vector<char> listing;
    for (;;) {
        constexpr std::size_t Chunk = 64 * 1024;
        const auto start = listing.size();
        listing.resize(start + Chunk);
        off_t position = 0;
        const auto read = ::__getdirentries64(fd, listing.data() + start, Chunk, &position);
        if (read < 0) {
            const int error = errno;
            ::lseek(fd, base, SEEK_SET);
            return SceErrorFromErrno(error);
        }
        listing.resize(start + static_cast<std::size_t>(read));
        if (read == 0) break;
    }
    const bool followLinks = InsideApp0(fd);
    std::string staged;
    std::size_t written = 0;
    const auto finish = [&](off_t resumeAt, int result) {
        const auto guestBase = static_cast<int64_t>(base);
        if ((written != 0 && !GuestCopyOut_nid_no_patch(buf, staged.data(), written)) ||
            (basep != nullptr && !GuestCopyOut_nid_no_patch(basep, &guestBase, sizeof(guestBase)))) {
            ::lseek(fd, base, SEEK_SET);
            return SceErrorFromErrno(GUEST_EFAULT);
        }
        if (::lseek(fd, resumeAt, SEEK_SET) < 0) return SceErrorFromErrno(errno);
        return result;
    };
    off_t index = 0;
    for (std::size_t offset = 0; offset < listing.size(); ++index) {
        const auto* entry = reinterpret_cast<const struct dirent*>(listing.data() + offset);
        if (entry->d_reclen == 0) break;
        offset += entry->d_reclen;
        if (index < base) continue;
        const auto nameLength = std::strlen(entry->d_name);
        const auto record = (GuestHeaderBytes + nameLength + 1 + 3) & ~std::size_t{3};
        if (nameLength > GuestMaxName || written + record > static_cast<std::size_t>(nbytes)) {
            if (written != 0) return finish(index, static_cast<int>(written));
            return finish(index, nameLength > GuestMaxName ? static_cast<int>(0x80020000u | GUEST_ENAMETOOLONG) : SceErrorFromErrno(GUEST_EINVAL));
        }
        staged.resize(written + record);
        char* out = staged.data() + written;
        const auto fileNumber = static_cast<std::uint32_t>(entry->d_ino);
        const auto recordLength = static_cast<std::uint16_t>(record);
        const auto nameBytes = static_cast<std::uint8_t>(nameLength);
        std::memcpy(out, &fileNumber, sizeof(fileNumber));
        std::memcpy(out + 4, &recordLength, sizeof(recordLength));
        out[6] = static_cast<char>(GuestEntryType(fd, *entry, followLinks));
        out[7] = static_cast<char>(nameBytes);
        std::memcpy(out + GuestHeaderBytes, entry->d_name, nameLength);
        written += record;
    }
    return finish(index, static_cast<int>(written));
}
#else
static unsigned char GuestEntryType(int fd, const struct dirent64& entry, bool followLinks) {
    if (entry.d_type != DT_LNK && entry.d_type != DT_UNKNOWN) return entry.d_type;
    if (entry.d_type == DT_LNK && !followLinks) return DT_LNK;
    const int savedErrno = errno;
    struct stat target{};
    const bool found = ::fstatat(fd, entry.d_name, &target, followLinks ? 0 : AT_SYMLINK_NOFOLLOW) == 0;
    errno = savedErrno;
    if (!found) return entry.d_type;
    if (S_ISDIR(target.st_mode)) return DT_DIR;
    if (S_ISREG(target.st_mode)) return DT_REG;
    if (S_ISLNK(target.st_mode)) return DT_LNK;
    return entry.d_type;
}

int APS5_VABI sceKernelGetdirentries(int fd, char* buf, int nbytes, int64_t* basep) {
    if (GuestSockets::IsOpen(fd) || File::IsArchivedFile(fd)) return SceErrorFromErrno(GUEST_EINVAL);
    constexpr std::size_t GuestHeaderBytes = 8;
    constexpr std::size_t GuestMaxName = 255;
    if (buf == nullptr) return SceErrorFromErrno(GUEST_EFAULT);
    if (nbytes <= 0) return SceErrorFromErrno(GUEST_EINVAL);
    if (File::IsDirectoryDescriptor(fd)) {
        return File::ReadDirectoryDescriptor(fd, buf, nbytes, basep);
    }
    struct stat status{};
    if (::fstat(fd, &status) != 0) return SceErrorFromErrno(errno);
    if (!S_ISDIR(status.st_mode)) return SceErrorFromErrno(GUEST_EINVAL);
    const off_t base = ::lseek(fd, 0, SEEK_CUR);
    if (base < 0) return SceErrorFromErrno(errno);
    constexpr std::size_t MaxHostChunk = 64 * 1024;
    std::vector<char> host(std::min(std::max(static_cast<std::size_t>(nbytes), std::size_t{4096}), MaxHostChunk));
#ifdef __EMSCRIPTEN__
    const auto read = ::getdents(fd, reinterpret_cast<struct dirent*>(host.data()), host.size());
#else
    const auto read = ::syscall(SYS_getdents64, fd, host.data(), host.size());
#endif
    if (read < 0) return SceErrorFromErrno(errno);
    off_t resume = base;
    const bool followLinks = InsideApp0(fd);
    std::string staged;
    std::size_t written = 0;
    const auto finish = [&](off_t resumeAt, int result) {
        const auto guestBase = static_cast<int64_t>(base);
        if ((written != 0 && !GuestCopyOut_nid_no_patch(buf, staged.data(), written)) ||
            (basep != nullptr && !GuestCopyOut_nid_no_patch(basep, &guestBase, sizeof(guestBase)))) {
            ::lseek(fd, base, SEEK_SET);
            return SceErrorFromErrno(GUEST_EFAULT);
        }
        if (::lseek(fd, resumeAt, SEEK_SET) < 0) return SceErrorFromErrno(errno);
        return result;
    };
    for (long offset = 0; offset < read;) {
        const auto* entry = reinterpret_cast<const struct dirent64*>(host.data() + offset);
        const auto nameLength = std::strlen(entry->d_name);
        const auto record = (GuestHeaderBytes + nameLength + 1 + 3) & ~std::size_t{3};
        if (nameLength > GuestMaxName || written + record > static_cast<std::size_t>(nbytes)) {
            if (written != 0) return finish(resume, static_cast<int>(written));
            return finish(resume, nameLength > GuestMaxName ? static_cast<int>(0x80020000u | GUEST_ENAMETOOLONG) : SceErrorFromErrno(GUEST_EINVAL));
        }
        staged.resize(written + record);
        char* out = staged.data() + written;
        const auto fileNumber = static_cast<std::uint32_t>(entry->d_ino);
        const auto recordLength = static_cast<std::uint16_t>(record);
        const auto nameBytes = static_cast<std::uint8_t>(nameLength);
        std::memcpy(out, &fileNumber, sizeof(fileNumber));
        std::memcpy(out + 4, &recordLength, sizeof(recordLength));
        out[6] = static_cast<char>(GuestEntryType(fd, *entry, followLinks));
        out[7] = static_cast<char>(nameBytes);
        std::memcpy(out + GuestHeaderBytes, entry->d_name, nameLength);
        written += record;
        resume = entry->d_off;
        offset += entry->d_reclen;
    }
    return finish(resume, static_cast<int>(written));
}
#endif

int APS5_VABI sceKernelGetdents(int fd, char* buf, int nbytes) {
    return sceKernelGetdirentries(fd, buf, nbytes, nullptr);
}

#endif

int APS5_VABI getdirentries_nid_postfix(int fd, char* buf, int nbytes, int64_t* basep) {
    return PosixResult(sceKernelGetdirentries(fd, buf, nbytes, basep));
}

int APS5_VABI _getdirentries_nid_postfix(int fd, char* buf, int nbytes, int64_t* basep) {
    return getdirentries_nid_postfix(fd, buf, nbytes, basep);
}

int APS5_VABI getdents_nid_postfix(int fd, char* buf, int nbytes) {
    return PosixResult(sceKernelGetdents(fd, buf, nbytes));
}

int APS5_VABI sceKernelMkdir(const char* path, uint16_t mode) {
    (void)mode;
    std::string copy;
    if (const int error = CopyGuestPath_nid_no_patch(path, copy)) return SceKernelError(error);
    path = copy.c_str();
    if (copy.empty()) return SceErrorFromErrno(GUEST_ENOENT);
    std::filesystem::path native;
    if (const int error = ResolvePathError_nid_no_patch(path, native)) return SceKernelError(error);
    while (!native.has_filename() && native.has_relative_path()) native = native.parent_path();
#ifdef _WIN32
    std::error_code pathError;
    const auto verbatimNative = NativeVerbatimPath_nid_no_patch(native, pathError, WindowsMaxDirectoryPathLength);
    const auto& targetNative = pathError ? native : verbatimNative;
#else
    const auto& targetNative = native;
#endif
    std::error_code error;
    if (std::filesystem::exists(targetNative, error)) return SceErrorFromErrno(GUEST_EEXIST);
    if (const int failure = PathLookupFailure(error)) return failure;
    if (!std::filesystem::is_directory(WithoutTrailingSeparator(targetNative).parent_path(), error)) {
#ifdef _WIN32
        if (File::WindowsSymbolicLinkLoop(WithoutTrailingSeparator(targetNative).parent_path())) return SceKernelError(GUEST_ELOOP);
#endif
        return SceErrorFromErrno(HasNonDirectoryPrefix(targetNative) ? GUEST_ENOTDIR : GUEST_ENOENT);
    }
    if (!std::filesystem::create_directory(targetNative, error)) return SceErrorFromErrno(error.value() ? error.value() : GUEST_EIO);
    RecordWrittenPath_nid_no_patch(native);
    return 0;
}

static int CopyIovecs(const KernelIovec* iov, int iovcnt, std::vector<KernelIovec>& vectors) {
    if (iovcnt < 0 || iovcnt > KERNEL_IOV_MAX) return SceErrorFromErrno(GUEST_EINVAL);
    if (iovcnt == 0) return 0;
    const auto bytes = static_cast<std::size_t>(iovcnt) * sizeof(KernelIovec);
    const auto address = reinterpret_cast<std::uintptr_t>(iov);
    if (address == 0 || bytes > UINTPTR_MAX - address) return SceErrorFromErrno(GUEST_EFAULT);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(iov, bytes);
    vectors.resize(static_cast<std::size_t>(iovcnt));
#ifdef _WIN32
    SIZE_T copied = 0;
    if (!ReadProcessMemory(GetCurrentProcess(), iov, vectors.data(), bytes, &copied) || copied != bytes) return SceErrorFromErrno(GUEST_EFAULT);
#elif defined(__linux__)
    iovec local{vectors.data(), bytes};
    iovec remote{const_cast<KernelIovec*>(iov), bytes};
    const int saved = errno;
    const auto copied = ::process_vm_readv(::getpid(), &local, 1, &remote, 1, 0);
    if (copied < 0 && errno != EFAULT) throw std::runtime_error("iovec: process_vm_readv failed with errno " + std::to_string(errno));
    errno = saved;
    if (copied < 0 || static_cast<std::size_t>(copied) != bytes) return SceErrorFromErrno(GUEST_EFAULT);
#elif defined(__APPLE__)
    mach_vm_size_t copied = 0;
    if (::mach_vm_read_overwrite(::mach_task_self(), reinterpret_cast<mach_vm_address_t>(iov), bytes,
            reinterpret_cast<mach_vm_address_t>(vectors.data()), &copied) != KERN_SUCCESS || copied != bytes) return SceErrorFromErrno(GUEST_EFAULT);
#else
    throw std::runtime_error("iovec: checked guest copies are not implemented on this host");
#endif
    std::size_t total = 0;
    for (const auto& vector : vectors) {
        if (vector.length > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max()) - total) return SceErrorFromErrno(GUEST_EINVAL);
        total += vector.length;
    }
    return 0;
}

static std::int64_t ReadRandomIovecs(int d, const KernelIovec* iov, int iovcnt) {
    std::int64_t total = 0;
    for (int i = 0; i < iovcnt; ++i) {
        File::ReadRandomDevice(d, iov[i].base, iov[i].length);
        total += static_cast<std::int64_t>(iov[i].length);
    }
    return total;
}

static bool OpenIovecs(const KernelIovec* iov, int iovcnt, std::deque<GuestArena::HostWrite>& destinations) {
    for (int i = 0; i < iovcnt; ++i) {
        if (!destinations.emplace_back(iov[i].base, iov[i].length).Open()) return false;
    }
    return true;
}

static std::int64_t KqueueIovecsResult(const KernelIovec* iov, int iovcnt, bool write) {
    const bool empty = std::none_of(iov, iov + iovcnt, [](const KernelIovec& vector) { return vector.length != 0; });
    return write || !empty ? SCE_KERNEL_ERROR_EOPNOTSUPP : 0;
}

static void PrepareWriteIovecs(const KernelIovec* iov, int iovcnt) {
    for (int i = 0; i < iovcnt; ++i) GuestArena::GuestArenaPrepareHostRead_nid_no_patch(iov[i].base, iov[i].length);
}

#ifdef _WIN32

int64_t APS5_VABI sceKernelPread(int d, void* buf, size_t nbytes, int64_t offset) {
    if (File::IsDirectoryDescriptor(d)) return SceErrorFromErrno(GUEST_EISDIR);
    if (const int error = PositionedArgumentError(d, false, buf == nullptr && nbytes != 0, offset)) return SceErrorFromErrno(error);
    if (const auto error = PositionedGuestSocketError(d); error != 0) return error;
    const GuestArena::HostWrite destination(buf, nbytes);
    if (!destination.Open()) return SceErrorFromErrno(GUEST_EFAULT);
    if (File::ReadRandomDevice(d, buf, nbytes)) return static_cast<int64_t>(nbytes);
    if (const auto archived = File::ReadArchivedFile(d, buf, nbytes, &offset)) return *archived < 0 ? SceErrorFromErrno(static_cast<int>(-*archived)) : *archived;
    char empty = 0;
    const auto result = NativePread(d, nbytes == 0 && buf == nullptr ? &empty : buf, nbytes, offset);
    return result < 0 ? SceErrorFromErrno(errno) : result;
}

int64_t APS5_VABI sceKernelPwrite(int d, const void* buf, size_t nbytes, int64_t offset) {
    if (File::IsVirtualDirectoryDescriptor(d)) return SceErrorFromErrno(GUEST_EISDIR);
    if (const int error = PositionedArgumentError(d, true, buf == nullptr && nbytes != 0, offset)) return SceErrorFromErrno(error);
    if (const auto error = PositionedGuestSocketError(d); error != 0) return error;
    const File::WriteSync sync(d);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(buf, nbytes);
    char empty = 0;
    const auto result = NativePwrite(d, nbytes == 0 && buf == nullptr ? &empty : buf, nbytes, offset);
    return result < 0 ? SceErrorFromErrno(errno) : result;
}

static std::int64_t TransferIovecs(int d, const KernelIovec* guestIov, int iovcnt, const std::int64_t* offset, bool write) {
    if (write ? File::IsVirtualDirectoryDescriptor(d) : File::IsDirectoryDescriptor(d)) return SceErrorFromErrno(GUEST_EISDIR);
    std::vector<KernelIovec> vectors;
    if (const int error = CopyIovecs(guestIov, iovcnt, vectors)) return error;
    const auto* iov = vectors.data();
    if (offset != nullptr) {
        if (const int error = PositionedArgumentError(d, write, false, *offset)) return SceErrorFromErrno(error);
    }
    if (offset == nullptr && IsKqueueDescriptor_nid_no_patch(d)) return KqueueIovecsResult(iov, iovcnt, write);
    std::size_t total = 0;
    for (int i = 0; i < iovcnt; ++i) {
        if (iov[i].base == nullptr && iov[i].length != 0) return SceErrorFromErrno(GUEST_EFAULT);
        total += iov[i].length;
    }
    if (offset != nullptr && *offset >= 0 && total > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max() - *offset)) return SceErrorFromErrno(GUEST_EINVAL);
    if (offset != nullptr) {
        if (const auto error = PositionedGuestSocketError(d); error != 0) return error;
    }
    std::deque<GuestArena::HostWrite> destinations;
    if (!write && !OpenIovecs(iov, iovcnt, destinations)) return SceErrorFromErrno(GUEST_EFAULT);
    if (offset == nullptr && GuestSockets::IsOpen(d)) {
        const auto result = write ? GuestSockets::Writev(d, reinterpret_cast<const GuestSockets::Iovec*>(iov), iovcnt)
                                  : GuestSockets::Readv(d, reinterpret_cast<const GuestSockets::Iovec*>(iov), iovcnt);
        return result < 0 ? SceKernelError(*__error_nid_postfix()) : result;
    }
    const File::WriteSync sync(write ? d : -1);
    if (write) PrepareWriteIovecs(iov, iovcnt);
    if (!write && File::IsRandomDevice(d)) return ReadRandomIovecs(d, iov, iovcnt);
    if (!write) {
        if (const auto archived = File::ReadArchivedIovecs(d, iov, iovcnt, offset)) return *archived < 0 ? SceErrorFromErrno(static_cast<int>(-*archived)) : *archived;
    }
    const auto positioned = File::LockPositionedTransfer(d);
    if (total == 0) {
        char none = 0;
        const auto result = offset != nullptr ? NativePositionedTransfer(d, &none, 0, *offset, write) : NativeTransferBuffer(d, &none, 0, write);
        return result < 0 ? SceErrorFromErrno(errno) : 0;
    }
    if (!write && offset == nullptr && File::IsDirectoryDescriptor(d)) return SceErrorFromErrno(GUEST_EISDIR);
    if (write && iovcnt > 1 && total <= static_cast<std::size_t>(std::numeric_limits<int>::max()) && File::Appends(d)) {
        std::vector<char> gathered(total);
        std::size_t gatheredOffset = 0;
        for (int i = 0; i < iovcnt; ++i) {
            if (iov[i].length == 0) continue;
            SIZE_T copied = 0;
            if (!ReadProcessMemory(GetCurrentProcess(), iov[i].base, gathered.data() + gatheredOffset, iov[i].length, &copied) || copied != iov[i].length) {
                return SceErrorFromErrno(GUEST_EFAULT);
            }
            gatheredOffset += iov[i].length;
        }
        const auto result = offset != nullptr ? NativePositionedTransfer(d, gathered.data(), gathered.size(), *offset, true)
                                              : NativeTransferBuffer(d, gathered.data(), gathered.size(), true);
        return result < 0 ? SceErrorFromErrno(errno) : result;
    }
    const bool whole = write || offset != nullptr || NativeIsDisk(d);
    std::int64_t done = 0;
    for (int i = 0; i < iovcnt; ++i) {
        auto* base = static_cast<char*>(iov[i].base);
        for (std::size_t position = 0; position < iov[i].length;) {
            const auto chunk = std::min<std::size_t>(iov[i].length - position, std::numeric_limits<int>::max());
            const auto result = offset != nullptr ? NativePositionedTransfer(d, base + position, chunk, *offset + done, write) : NativeTransferBuffer(d, base + position, chunk, write);
            if (result < 0) return done != 0 ? done : SceErrorFromErrno(errno);
            done += result;
            position += static_cast<std::size_t>(result);
            if (static_cast<std::size_t>(result) < chunk || (!whole && !NativePipeHasData(d))) return done;
        }
    }
    return done;
}

int64_t APS5_VABI sceKernelReadv(int d, const KernelIovec* iov, int iovcnt) {
    if (File::IsDirectoryDescriptor(d)) return SceErrorFromErrno(GUEST_EISDIR);
    return TransferIovecs(d, iov, iovcnt, nullptr, false);
}

int64_t APS5_VABI sceKernelWritev(int d, const KernelIovec* iov, int iovcnt) {
    if (File::IsVirtualDirectoryDescriptor(d)) return SceErrorFromErrno(GUEST_EISDIR);
    return TransferIovecs(d, iov, iovcnt, nullptr, true);
}

int64_t APS5_VABI sceKernelPreadv(int d, const KernelIovec* iov, int iovcnt, int64_t offset) {
    if (File::IsDirectoryDescriptor(d)) return SceErrorFromErrno(GUEST_EISDIR);
    return TransferIovecs(d, iov, iovcnt, &offset, false);
}

int64_t APS5_VABI sceKernelPwritev(int d, const KernelIovec* iov, int iovcnt, int64_t offset) {
    if (File::IsVirtualDirectoryDescriptor(d)) return SceErrorFromErrno(GUEST_EISDIR);
    return TransferIovecs(d, iov, iovcnt, &offset, true);
}

#else

int64_t APS5_VABI sceKernelPread(int d, void* buf, size_t nbytes, int64_t offset) {
    if (File::IsDirectoryDescriptor(d)) return SceErrorFromErrno(GUEST_EISDIR);
    if (const int error = PositionedArgumentError(d, false, buf == nullptr && nbytes != 0, offset)) return SceErrorFromErrno(error);
    if (const auto error = PositionedGuestSocketError(d); error != 0) return error;
    const GuestArena::HostWrite destination(buf, nbytes);
    if (!destination.Open()) return SceErrorFromErrno(GUEST_EFAULT);
    if (File::ReadRandomDevice(d, buf, nbytes)) return static_cast<int64_t>(nbytes);
    if (const auto archived = File::ReadArchivedFile(d, buf, nbytes, &offset)) return *archived < 0 ? SceErrorFromErrno(static_cast<int>(-*archived)) : *archived;
    const auto result = NativePread(d, buf, nbytes, offset);
    return result < 0 ? SceErrorFromErrno(errno) : result;
}

int64_t APS5_VABI sceKernelPwrite(int d, const void* buf, size_t nbytes, int64_t offset) {
    if (File::IsVirtualDirectoryDescriptor(d)) return SceErrorFromErrno(GUEST_EISDIR);
    if (const int error = PositionedArgumentError(d, true, buf == nullptr && nbytes != 0, offset)) return SceErrorFromErrno(error);
    if (const auto error = PositionedGuestSocketError(d); error != 0) return error;
    const File::WriteSync sync(d);
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(buf, nbytes);
    const auto result = NativePwrite(d, buf, nbytes, offset);
    return result < 0 ? SceErrorFromErrno(errno) : result;
}

#ifdef __APPLE__
static std::int64_t EmptyIovecsResult(int d, bool write, bool positioned) {
    const int flags = ::fcntl(d, F_GETFL);
    if (flags < 0) return SceErrorFromErrno(errno);
    if ((flags & O_ACCMODE) == (write ? O_RDONLY : O_WRONLY)) return SceErrorFromErrno(EBADF);
    if (positioned && ::lseek(d, 0, SEEK_CUR) < 0) return SceErrorFromErrno(errno);
    return 0;
}
#endif

int64_t APS5_VABI sceKernelReadv(int d, const KernelIovec* guestIov, int iovcnt) {
    if (File::IsDirectoryDescriptor(d)) return SceErrorFromErrno(GUEST_EISDIR);
    std::vector<KernelIovec> vectors;
    if (const int error = CopyIovecs(guestIov, iovcnt, vectors)) return error;
    const auto* iov = vectors.data();
    if (IsKqueueDescriptor_nid_no_patch(d)) return KqueueIovecsResult(iov, iovcnt, false);
    std::deque<GuestArena::HostWrite> destinations;
    if (!OpenIovecs(iov, iovcnt, destinations)) return SceErrorFromErrno(GUEST_EFAULT);
    if (GuestSockets::IsOpen(d)) {
        const auto result = GuestSockets::Readv(d, reinterpret_cast<const GuestSockets::Iovec*>(iov), iovcnt);
        return result < 0 ? SceKernelError(*__error_nid_postfix()) : result;
    }
    if (File::IsRandomDevice(d)) return ReadRandomIovecs(d, iov, iovcnt);
    if (const auto archived = File::ReadArchivedIovecs(d, iov, iovcnt, nullptr)) return *archived < 0 ? SceErrorFromErrno(static_cast<int>(-*archived)) : *archived;
#ifdef __APPLE__
    if (iovcnt == 0) return EmptyIovecsResult(d, false, false);
#endif
    const auto parts = File::TransferInParts(d, NativeIovecs(iov), iovcnt, nullptr, false);
    const auto result = parts == -2 ? static_cast<std::int64_t>(::readv(d, NativeIovecs(iov), iovcnt)) : parts;
    return result < 0 ? SceErrorFromErrno(errno) : result;
}

int64_t APS5_VABI sceKernelWritev(int d, const KernelIovec* guestIov, int iovcnt) {
    if (File::IsVirtualDirectoryDescriptor(d)) return SceErrorFromErrno(GUEST_EISDIR);
    std::vector<KernelIovec> vectors;
    if (const int error = CopyIovecs(guestIov, iovcnt, vectors)) return error;
    const auto* iov = vectors.data();
    if (IsKqueueDescriptor_nid_no_patch(d)) return KqueueIovecsResult(iov, iovcnt, true);
    if (GuestSockets::IsOpen(d)) {
        const auto result = GuestSockets::Writev(d, reinterpret_cast<const GuestSockets::Iovec*>(iov), iovcnt);
        return result < 0 ? SceKernelError(*__error_nid_postfix()) : result;
    }
    const File::WriteSync sync(d);
    PrepareWriteIovecs(iov, iovcnt);
    const auto sharedResult = File::WritevSharedMemory_nid_no_patch(d, NativeIovecs(iov), iovcnt);
    if (sharedResult != -2) return sharedResult < 0 ? SceErrorFromErrno(errno) : sharedResult;
#ifdef __APPLE__
    if (iovcnt == 0) return EmptyIovecsResult(d, true, false);
#endif
    const auto parts = File::TransferInParts(d, NativeIovecs(iov), iovcnt, nullptr, true);
    const auto result = parts == -2 ? static_cast<std::int64_t>(::writev(d, NativeIovecs(iov), iovcnt)) : parts;
    return result < 0 ? SceErrorFromErrno(errno) : result;
}

int64_t APS5_VABI sceKernelPreadv(int d, const KernelIovec* guestIov, int iovcnt, int64_t offset) {
    if (File::IsDirectoryDescriptor(d)) return SceErrorFromErrno(GUEST_EISDIR);
    std::vector<KernelIovec> vectors;
    if (const int error = CopyIovecs(guestIov, iovcnt, vectors)) return error;
    const auto* iov = vectors.data();
    if (const int error = PositionedArgumentError(d, false, false, offset)) return SceErrorFromErrno(error);
    if (const auto error = PositionedGuestSocketError(d); error != 0) return error;
    std::deque<GuestArena::HostWrite> destinations;
    if (!OpenIovecs(iov, iovcnt, destinations)) return SceErrorFromErrno(GUEST_EFAULT);
    if (File::IsRandomDevice(d)) return ReadRandomIovecs(d, iov, iovcnt);
    if (const auto archived = File::ReadArchivedIovecs(d, iov, iovcnt, &offset)) return *archived < 0 ? SceErrorFromErrno(static_cast<int>(-*archived)) : *archived;
#ifdef __APPLE__
    if (iovcnt == 0) return EmptyIovecsResult(d, false, true);
#endif
    const auto parts = File::TransferInParts(d, NativeIovecs(iov), iovcnt, &offset, false);
    const auto result = parts == -2 ? static_cast<std::int64_t>(::preadv(d, NativeIovecs(iov), iovcnt, static_cast<off_t>(offset))) : parts;
    return result < 0 ? SceErrorFromErrno(errno) : result;
}

int64_t APS5_VABI sceKernelPwritev(int d, const KernelIovec* guestIov, int iovcnt, int64_t offset) {
    if (File::IsVirtualDirectoryDescriptor(d)) return SceErrorFromErrno(GUEST_EISDIR);
    std::vector<KernelIovec> vectors;
    if (const int error = CopyIovecs(guestIov, iovcnt, vectors)) return error;
    const auto* iov = vectors.data();
    if (const int error = PositionedArgumentError(d, true, false, offset)) return SceErrorFromErrno(error);
    if (const auto error = PositionedGuestSocketError(d); error != 0) return error;
    const File::WriteSync sync(d);
    PrepareWriteIovecs(iov, iovcnt);
    const auto sharedResult = File::PwritevSharedMemory_nid_no_patch(d, NativeIovecs(iov), iovcnt, offset);
    if (sharedResult != -2) return sharedResult < 0 ? SceErrorFromErrno(errno) : sharedResult;
#ifdef __APPLE__
    if (iovcnt == 0) return EmptyIovecsResult(d, true, true);
    if (const auto appended = File::WritevAtEnd(d, NativeIovecs(iov), iovcnt); appended != -2) return appended < 0 ? SceErrorFromErrno(errno) : appended;
#endif
    const auto parts = File::TransferInParts(d, NativeIovecs(iov), iovcnt, &offset, true);
    const auto result = parts == -2 ? static_cast<std::int64_t>(::pwritev(d, NativeIovecs(iov), iovcnt, static_cast<off_t>(offset))) : parts;
    return result < 0 ? SceErrorFromErrno(errno) : result;
}

#endif

int64_t APS5_VABI preadv_nid_postfix(int d, const KernelIovec* iov, int iovcnt, int64_t offset) {
    const auto result = sceKernelPreadv(d, iov, iovcnt, offset);
    return result < 0 ? PosixFailure(static_cast<int>(result) & 0xffff) : result;
}

int64_t APS5_VABI pwritev_nid_postfix(int d, const KernelIovec* iov, int iovcnt, int64_t offset) {
    const auto result = sceKernelPwritev(d, iov, iovcnt, offset);
    return result < 0 ? PosixFailure(static_cast<int>(result) & 0xffff) : result;
}

int APS5_VABI sceKernelRename(const char* from, const char* to) {
    if (!from) return SceErrorFromErrno(GUEST_EFAULT);
    const int result = RenameGuestPath_nid_no_patch(from, to);
    return result == 0 ? 0 : SceKernelError(errno);
}

int APS5_VABI sceKernelRmdir(const char* path) {
    std::string copy;
    if (const int error = CopyGuestPath_nid_no_patch(path, copy)) return SceKernelError(error);
    path = copy.c_str();
    if (copy.empty()) return SceErrorFromErrno(GUEST_ENOENT);
    if (const int dot = RenameDotError_nid_no_patch(path)) return SceKernelError(dot);
    std::filesystem::path native;
    if (const int error = ResolvePathError_nid_no_patch(path, native)) return SceKernelError(error);
#ifdef _WIN32
    std::error_code pathError;
    const auto verbatimNative = NativeVerbatimPath_nid_no_patch(native, pathError, WindowsMaxDirectoryPathLength);
    const auto& targetNative = pathError ? native : verbatimNative;
#else
    const auto& targetNative = native;
#endif
    FileStat linkStatus{};
    if (targetNative.has_filename() && File::FillLinkStat(targetNative, &linkStatus) && (linkStatus.st_mode & 0170000) == 0120000) return SceErrorFromErrno(GUEST_ENOTDIR);
    std::error_code error;
    const auto status = std::filesystem::status(targetNative, error);
    if (const int failure = PathLookupFailure(error)) return failure;
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesW(targetNative.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        std::string junctionTarget;
        if (File::WindowsSymbolicLink::ReadJunction(targetNative, junctionTarget, "rmdir")) return SceErrorFromErrno(GUEST_ENOTDIR);
    }
#endif
    if (!std::filesystem::is_directory(status)) {
#ifdef _WIN32
        if (File::WindowsSymbolicLinkLoop(WithoutTrailingSeparator(targetNative).parent_path())) return SceKernelError(GUEST_ELOOP);
#endif
        const bool blockedByFile = HasNonDirectoryPrefix(targetNative) || std::filesystem::exists(WithoutTrailingSeparator(targetNative), error);
        return SceErrorFromErrno(blockedByFile ? GUEST_ENOTDIR : GUEST_ENOENT);
    }
#ifdef _WIN32
    if (!IsDirectoryEmpty(targetNative)) return SceErrorFromErrno(GUEST_ENOTEMPTY);
#else
    if (!std::filesystem::is_empty(targetNative, error)) return SceErrorFromErrno(GUEST_ENOTEMPTY);
#endif
    if (!RemoveEmptyDirectory(targetNative)) return SceErrorFromErrno(GUEST_EIO);
    RecordWrittenPath_nid_no_patch(native);
    return 0;
}

int APS5_VABI rmdir_nid_postfix(const char* path) {
    std::string copy;
    if (const int error = PathError(path, copy)) return PosixFailure(error);
    path = copy.c_str();
    return PosixResult(sceKernelRmdir(path));
}

}

extern "C" {

int APS5_VABI sceKernelChmod_nid_postfix(const char* path, std::uint16_t mode) {
    std::string copy;
    if (const int error = CopyGuestPath_nid_no_patch(path, copy)) return SceKernelError(error);
    path = copy.c_str();
    if (copy.empty()) return SceErrorFromErrno(GUEST_ENOENT);
    std::filesystem::path native;
    if (const int error = ResolvePathError_nid_no_patch(path, native)) return SceKernelError(error);
    if (NativeChmod(native, mode & 07777) != 0) return SceErrorFromErrno(errno);
    return 0;
}

int APS5_VABI sceKernelFchmod(int d, std::uint16_t mode) {
    if (File::IsVirtualDirectoryDescriptor(d)) return SceErrorFromErrno(GUEST_EISDIR);
    if (GuestSockets::IsOpen(d) || IsKqueueDescriptor_nid_no_patch(d)) return SceErrorFromErrno(GUEST_EINVAL);
    if (IsPipeDescriptor(d)) return SceErrorFromErrno(GUEST_EINVAL);
    File::RefuseArchivedFile(d, "fchmod");
#ifdef _WIN32
    const int sharedMemoryResult = File::SetSharedMemoryMode_nid_no_patch(d, mode);
    if (sharedMemoryResult != -2) return sharedMemoryResult == 0 ? 0 : SceErrorFromErrno(errno);
#endif
    if (NativeFchmod(d, mode & 07777) != 0) return SceErrorFromErrno(errno);
    return 0;
}

int APS5_VABI fchmod_nid_postfix(int d, int mode) {
    return PosixResult(sceKernelFchmod(d, static_cast<std::uint16_t>(mode)));
}

static bool InvalidTimes(const KernelTimeval* times) {
    return times != nullptr && (times[0].tv_usec < 0 || times[0].tv_usec >= 1000000 || times[1].tv_usec < 0 || times[1].tv_usec >= 1000000);
}

static bool CopyInTimes(const KernelTimeval*& times, KernelTimeval (&local)[2]) {
    if (times == nullptr) return true;
    if (!GuestCopyIn_nid_no_patch(local, times, sizeof(local))) return false;
    times = local;
    return true;
}

int APS5_VABI sceKernelTruncate_nid_postfix(const char* path, std::int64_t length) {
    if (length < 0) return SceErrorFromErrno(GUEST_EINVAL);
    std::string copy;
    if (const int error = CopyGuestPath_nid_no_patch(path, copy)) return SceKernelError(error);
    path = copy.c_str();
    if (copy.empty()) return SceErrorFromErrno(GUEST_ENOENT);
    std::filesystem::path native;
    if (const int error = ResolvePathError_nid_no_patch(path, native)) return SceKernelError(error);
    std::error_code error;
    const auto verbatim = NativeVerbatimPath_nid_no_patch(native, error);
    const auto& target = error ? native : verbatim;
    const auto status = std::filesystem::status(target, error);
    if (const int failure = PathLookupFailure(error)) return failure;
    if (!std::filesystem::exists(status)) {
#ifdef _WIN32
        if (File::WindowsSymbolicLinkLoop(target)) return SceKernelError(GUEST_ELOOP);
#endif
        return SceErrorFromErrno(NotFoundError_nid_no_patch(native));
    }
    if (std::filesystem::is_directory(status)) return SceErrorFromErrno(GUEST_EISDIR);
    if (const int failure = NativeTruncate(target, length)) {
#ifdef _WIN32
        if (File::WindowsSymbolicLinkLoop(target)) return SceKernelError(GUEST_ELOOP);
#endif
        return SceErrorFromErrno(failure);
    }
    RecordWrittenPath_nid_no_patch(native);
    return 0;
}

int APS5_VABI truncate_nid_postfix(const char* path, std::int64_t length) {
    if (length < 0) return PosixFailure(GUEST_EINVAL);
    std::string copy;
    if (const int error = PathError(path, copy)) return PosixFailure(error);
    path = copy.c_str();
    return PosixResult(sceKernelTruncate_nid_postfix(path, length));
}

int APS5_VABI sceKernelUtimes_nid_postfix(const char* path, const KernelTimeval* times) {
    KernelTimeval local[2];
    if (!CopyInTimes(times, local)) return SCE_KERNEL_ERROR_EFAULT;
    if (InvalidTimes(times)) return SCE_KERNEL_ERROR_EINVAL;
    std::string copy;
    if (const int error = CopyGuestPath_nid_no_patch(path, copy)) return SceKernelError(error);
    path = copy.c_str();
    if (copy.empty()) return SceErrorFromErrno(GUEST_ENOENT);
    std::filesystem::path native;
    if (const int error = ResolvePathError_nid_no_patch(path, native)) return SceKernelError(error);
    if (NativeUtimes(native, times) != 0) return SceErrorFromErrno(errno == ENOENT ? NotFoundError_nid_no_patch(native) : errno);
    return 0;
}

int APS5_VABI utimes_nid_postfix(const char* path, const KernelTimeval* times) {
    KernelTimeval local[2];
    if (!CopyInTimes(times, local)) return PosixFailure(GUEST_EFAULT);
    if (InvalidTimes(times)) return PosixFailure(GUEST_EINVAL);
    std::string copy;
    if (const int error = PathError(path, copy)) return PosixFailure(error);
    path = copy.c_str();
    return PosixResult(sceKernelUtimes_nid_postfix(path, times));
}

int PosixUtimesAt_nid_no_patch(int directory, const char* path, const KernelTimeval* times) {
    KernelTimeval local[2];
    if (!CopyInTimes(times, local)) return PosixFailure(GUEST_EFAULT);
    if (InvalidTimes(times)) return PosixFailure(GUEST_EINVAL);
    std::string copy;
    if (const int error = PathError(path, copy)) return PosixFailure(error);
    path = copy.c_str();
    if (directory == -100 || *path == '/') return utimes_nid_postfix(path, times);
    int error = 0;
    const char* base = GetDirectoryDescriptorPath_nid_no_patch(directory, &error);
    if (!base) return PosixFailure(error);
    const auto native = std::filesystem::path(base) / std::filesystem::path(reinterpret_cast<const char8_t*>(path));
    if (NativeUtimes(native, times) != 0) return PosixFailure(SceErrorFromErrno(errno == ENOENT ? NotFoundError_nid_no_patch(native) : errno) & 0xffff);
    return 0;
}

int APS5_VABI lutimes_nid_postfix(const char* path, const KernelTimeval* times) {
    KernelTimeval local[2];
    if (!CopyInTimes(times, local)) return PosixFailure(GUEST_EFAULT);
    if (InvalidTimes(times)) return PosixFailure(GUEST_EINVAL);
    std::string copy;
    if (const int error = PathError(path, copy)) return PosixFailure(error);
    path = copy.c_str();
    std::filesystem::path native;
    if (const int error = ResolvePathError_nid_no_patch(path, native)) return PosixFailure(error);
    if (NativeUtimes(native, times, false) != 0) return PosixResult(SceErrorFromErrno(errno == ENOENT ? NotFoundError_nid_no_patch(native) : errno));
    return 0;
}

int APS5_VABI sceKernelFutimes(int d, const KernelTimeval* times) {
    KernelTimeval local[2];
    if (!CopyInTimes(times, local)) return SCE_KERNEL_ERROR_EFAULT;
    if (File::IsVirtualDirectoryDescriptor(d)) return SceErrorFromErrno(GUEST_EISDIR);
    if (GuestSockets::IsOpen(d)) return SceErrorFromErrno(GUEST_EINVAL);
    File::RefuseArchivedFile(d, "futimes");
    if (times != nullptr) {
        for (int i = 0; i < 2; ++i) {
            if (times[i].tv_usec < 0 || times[i].tv_usec >= 1000000) return SceErrorFromErrno(GUEST_EINVAL);
        }
    }
    if (NativeFutimes(d, times) != 0) return SceErrorFromErrno(errno);
    return 0;
}

int APS5_VABI futimes_nid_postfix(int d, const KernelTimeval* times) {
    return PosixResult(sceKernelFutimes(d, times));
}

int APS5_VABI fsync_nid_postfix(int fd) {
    return PosixResult(sceKernelFsync(fd));
}

int APS5_VABI fdatasync_nid_postfix(int fd) {
    return PosixResult(sceKernelFdatasync(fd));
}

int64_t APS5_VABI readv_nid_postfix(int d, const KernelIovec* iov, int iovcnt) {
    const auto result = sceKernelReadv(d, iov, iovcnt);
    return result < 0 ? PosixFailure(static_cast<int>(result) & 0xffff) : result;
}

int64_t APS5_VABI writev_nid_postfix(int d, const KernelIovec* iov, int iovcnt) {
    const auto result = sceKernelWritev(d, iov, iovcnt);
    return result < 0 ? PosixFailure(static_cast<int>(result) & 0xffff) : result;
}

int64_t APS5_VABI _readv_nid_postfix(int d, const KernelIovec* iov, int iovcnt) {
    return readv_nid_postfix(d, iov, iovcnt);
}

int64_t APS5_VABI _writev_nid_postfix(int d, const KernelIovec* iov, int iovcnt) {
    return writev_nid_postfix(d, iov, iovcnt);
}

}
