#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <algorithm>
#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include <sys/stat.h>
#include <cerrno>
#include <ctime>
#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/AppMetadata/include/AppMetadata.hpp"
#include "prx/libc/include/FilesystemError.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "prx/libkernel/File/include/DirectoryDescriptor.hpp"
#include "prx/libkernel/File/include/DescriptorControl.hpp"
#include "prx/libc/include/CreationMask.hpp"
#include "prx/libkernel/File/include/FileFlags.hpp"
#include "prx/libkernel/File/include/SharedMemory.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#include <sys/stat.h>
#include "prx/libkernel/File/include/WindowsSymbolicLink.hpp"
#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2
#endif
#endif

namespace {
#ifdef _WIN32
extern "C" _invalid_parameter_handler _set_thread_local_invalid_parameter_handler(_invalid_parameter_handler);
static void IgnoreInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, std::uintptr_t) {}
static int GuestFstat(int fd, bool* withoutVnode = nullptr) {
    const auto previous = _set_thread_local_invalid_parameter_handler(IgnoreInvalidParameter);
    struct _stat64 status{};
    const int result = ::_fstat64(fd, &status);
    _set_thread_local_invalid_parameter_handler(previous);
    if (result == 0 && withoutVnode != nullptr) *withoutVnode = (status.st_mode & _S_IFMT) == _S_IFIFO;
    return result;
}
#endif

constexpr int GuestEnoent = 2;
constexpr int GuestEperm = 1;
constexpr int GuestEsrch = 3;
constexpr int GuestEio = 5;
constexpr int GuestEbadf = 9;
constexpr int GuestEfault = 14;
constexpr int GuestEnotdir = 20;
constexpr int GuestEisdir = 21;
constexpr int GuestEinval = 22;
constexpr int GuestEexist = 17;
constexpr int GuestExdev = 18;
constexpr int GuestEdeadlk = 11;
constexpr int GuestEopnotsupp = 45;
constexpr int GuestEagain = 35;
std::atomic<int> g_gameCpumode{1};
constexpr int GuestEloop = 62;
constexpr int GuestEnametoolong = 63;
constexpr int GuestEnotempty = 66;
constexpr int GuestEoverflow = 84;
constexpr int GuestAtFdcwd = -100;
constexpr int GuestAtRemovedir = 0x800;
constexpr std::size_t GuestPathMax = 1024;
constexpr std::size_t GuestNameMax = 255;

constexpr int PcLinkMax = 1;
constexpr int PcNameMax = 4;
constexpr int PcPathMax = 5;
constexpr int PcPipeBuf = 6;
constexpr int PcChownRestricted = 7;
constexpr int PcNoTrunc = 8;
constexpr int PcAllocSizeMin = 10;
constexpr int PcFileSizeBits = 12;
constexpr int PcRecIncrXferSize = 14;
constexpr int PcRecMaxXferSize = 15;
constexpr int PcRecMinXferSize = 16;
constexpr int PcRecXferAlign = 17;
constexpr int PcSymlinkMax = 18;
constexpr int PcMinHoleSize = 21;
constexpr int PcAsyncIo = 53;
constexpr std::int64_t PosixAsynchronousIo = 200112;
constexpr int PcPrioIo = 54;
constexpr int PcSyncIo = 55;
constexpr int PcAclExtended = 59;
constexpr int PcAclPathMax = 60;
constexpr int PcMacPresent = 63;
constexpr int PcAclNfs4 = 64;
constexpr int GuestAtSymlinkNofollow = 0x0200;
constexpr int GuestAtSymlinkFollow = 0x0400;

int GuestFileError(int error) {
    switch (error) {
    case ENOENT: return GuestEnoent;
    case EACCES: return 13;
    case ENOTDIR: return 20;
    case EINVAL: return GuestEinval;
    case EPERM: return GuestEperm;
    case EROFS: return 30;
    case ENAMETOOLONG: return GuestEnametoolong;
    case EIO: return GuestEio;
#ifdef ELOOP
    case ELOOP: return GuestEloop;
#endif
    default: return GuestEio;
    }
}

std::int64_t Fail(int error) {
    *__error_nid_postfix() = error;
    return -1;
}

constexpr int KenvGet = 0;
constexpr int KenvSet = 1;
constexpr int KenvUnset = 2;
constexpr int KenvDump = 3;
constexpr std::size_t KenvNameLength = 128;
constexpr std::size_t KenvValueLength = 128;
constexpr std::size_t KenvEntries = 512;

struct KernelEnvironment {
    std::mutex mutex;
    std::vector<std::string> entries;
};

KernelEnvironment& KernelEnv() {
    static auto* environment = new KernelEnvironment;
    return *environment;
}

std::vector<std::string>::iterator FindKernelEnv(std::vector<std::string>& entries, const std::string& name) {
    return std::find_if(entries.begin(), entries.end(), [&](const std::string& entry) {
        return entry.size() > name.size() && entry.compare(0, name.size(), name) == 0 && entry[name.size()] == '=';
    });
}

int CopyKernelEnvString(const char* source, std::size_t limit, std::string& destination) {
    if (const int error = CopyGuestPath_nid_no_patch(source, destination)) return error;
    return destination.size() + 1 > limit ? GuestEnametoolong : 0;
}

std::int64_t PathConfigurationValue(int name) {
    switch (name) {
    case PcLinkMax: return 32767;
    case PcNameMax: return static_cast<std::int64_t>(GuestNameMax);
    case PcPathMax: return static_cast<std::int64_t>(GuestPathMax);
    case PcPipeBuf: return 512;
    case PcChownRestricted: return 1;
    case PcNoTrunc: return 1;
    case PcFileSizeBits: return 64;
    case PcRecMaxXferSize: return -1;
    case PcSymlinkMax: return static_cast<std::int64_t>(GuestPathMax);
    case PcPrioIo: return 0;
    case PcSyncIo: return 0;
    case PcAllocSizeMin: return 65536;
    case PcRecIncrXferSize: return 65536;
    case PcRecMinXferSize: return 65536;
    case PcRecXferAlign: return 16384;
    case PcMinHoleSize: return 65536;
    case PcAclExtended: return 0;
    case PcAclPathMax: return 3;
    case PcMacPresent: return 0;
    case PcAclNfs4: return 0;
    default: return Fail(GuestEinval);
    }
}

int CopyPath(const char*& path, std::string& copy) {
    if (const int error = CopyGuestPath_nid_no_patch(path, copy)) return error;
    path = copy.c_str();
    return copy.empty() ? GuestEnoent : 0;
}

int GuestError(const std::error_code& error) {
    const auto condition = error.default_error_condition();
    if (condition == std::errc::resource_unavailable_try_again) return GuestEagain;
    if (condition == std::errc::resource_deadlock_would_occur) return GuestEdeadlk;
    if (condition == std::errc::too_many_symbolic_link_levels) return GuestEloop;
    if (condition == std::errc::filename_too_long) return GuestEnametoolong;
    if (condition == std::errc::directory_not_empty) return GuestEnotempty;
    if (condition == std::errc::value_too_large) return GuestEoverflow;
    const int value = condition.value();
    return value > 0 && value <= 34 ? value : GuestEio;
}

std::int64_t CopyLinkTarget(const std::string& target, char* buffer, std::size_t size) {
    const auto count = std::min(size, target.size());
    if (count != 0 && buffer == nullptr) return Fail(GuestEfault);
    if (count != 0) std::memcpy(buffer, target.data(), count);
    return static_cast<std::int64_t>(count);
}

bool HasNonDirectoryPrefix(const std::filesystem::path& native) {
    std::error_code error;
    for (auto prefix = native.parent_path(); prefix.has_relative_path(); prefix = prefix.parent_path()) {
        const auto status = std::filesystem::status(prefix, error);
        if (std::filesystem::exists(status)) return !std::filesystem::is_directory(status);
    }
    return false;
}

void RequireGuestOwner(const char* call, std::uint32_t uid, std::uint32_t gid) {
    constexpr std::uint32_t Unchanged = 0xFFFFFFFFu;
    if (uid != 0 && uid != Unchanged) NotImplemented_nid_no_patch((std::string(call) + ": an owner other than the guest user").c_str());
    if (gid != 0 && gid != Unchanged) NotImplemented_nid_no_patch((std::string(call) + ": a group other than the guest group").c_str());
}

bool IsSymbolicLink(const std::filesystem::path& native) {
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesW(native.wstring().c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
    std::error_code error;
    return std::filesystem::is_symlink(std::filesystem::symlink_status(native, error));
#endif
}

std::int64_t ReadNativeLink(const std::filesystem::path& native, char* buffer, std::size_t size) {
#ifdef _WIN32
    const DWORD attributes = GetFileAttributesW(native.wstring().c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        std::string target;
        if (!File::WindowsSymbolicLink::Read(native, target, "readlink") && !File::WindowsSymbolicLink::ReadJunction(native, target, "readlink"))
            return Fail(GuestEinval);
        return CopyLinkTarget(target, buffer, size);
    }
#endif
    std::error_code error;
    const auto status = std::filesystem::symlink_status(native, error);
    if (error)
        return Fail(error == std::errc::no_such_file_or_directory && HasNonDirectoryPrefix(native) ? GuestEnotdir : GuestError(error));
    if (status.type() == std::filesystem::file_type::not_found)
        return Fail(HasNonDirectoryPrefix(native) ? GuestEnotdir : GuestEnoent);
    if (!std::filesystem::is_symlink(status)) return Fail(GuestEinval);
    const auto target = std::filesystem::read_symlink(native, error).generic_string();
    if (error) return Fail(GuestError(error));
    return CopyLinkTarget(target, buffer, size);
}

bool HasLongComponent(std::string_view path) {
    for (std::size_t start = 0; start < path.size();) {
        const auto end = std::min(path.find('/', start), path.size());
        if (end - start > GuestNameMax) {
            if (start == 0) return true;
            std::filesystem::path prefix;
            if (ResolvePathError_nid_no_patch(std::string(path.substr(0, start)).c_str(), prefix)) return false;
            std::error_code error;
            return std::filesystem::is_directory(prefix, error);
        }
        start = end + 1;
    }
    return false;
}

bool IsDotOrDotDot(std::string_view path) {
    while (!path.empty() && (path.back() == '/' || path.back() == '\\')) path.remove_suffix(1);
    const auto separator = path.find_last_of("/\\");
    const auto component = path.substr(separator == std::string_view::npos ? 0 : separator + 1);
    return component == "." || component == "..";
}

int RenamePath(const std::filesystem::path& source, const std::filesystem::path& destination) {
    namespace fs = std::filesystem;
    std::error_code error;
    const auto sourceStatus = fs::symlink_status(source, error);
    if (error && error != std::errc::no_such_file_or_directory) return FilesystemError(error);
    if (sourceStatus.type() == fs::file_type::not_found)
        return HasNonDirectoryPrefix(source) ? GuestEnotdir : GuestEnoent;

    const auto parent = destination.parent_path().empty() ? fs::path(".") : destination.parent_path();
    error.clear();
    const auto parentStatus = fs::status(parent, error);
    if (error) return FilesystemError(error);
    if (!fs::exists(parentStatus))
        return HasNonDirectoryPrefix(destination) ? GuestEnotdir : GuestEnoent;
    if (!fs::is_directory(parentStatus)) return GuestEnotdir;

    error.clear();
    const auto destinationStatus = fs::symlink_status(destination, error);
    if (error && error != std::errc::no_such_file_or_directory) return FilesystemError(error);
    const bool destinationExists = destinationStatus.type() != fs::file_type::not_found;
    const bool sourceIsDirectory = fs::is_directory(sourceStatus);
    const bool destinationIsDirectory = destinationExists && fs::is_directory(destinationStatus);
    bool sameFile = false;
    if (destinationExists) {
        error.clear();
        sameFile = fs::equivalent(source, destination, error);
        if (error == std::errc::no_such_file_or_directory) error.clear();
        if (error) return FilesystemError(error);
        if (!sameFile && sourceIsDirectory && !destinationIsDirectory) return GuestEnotdir;
        if (!sameFile && !sourceIsDirectory && destinationIsDirectory) return GuestEisdir;
    }
    if (sourceIsDirectory && !sameFile) {
        error.clear();
        const auto canonicalSource = fs::weakly_canonical(source, error);
        if (error) return FilesystemError(error);
        const auto canonicalDestination = fs::weakly_canonical(destination, error);
        if (error) return FilesystemError(error);
        const auto relative = canonicalDestination.lexically_relative(canonicalSource);
        if (!relative.empty() && *relative.begin() != "..") return GuestEinval;
    }
    if (destinationExists && !sameFile && destinationIsDirectory) {
        error.clear();
        if (!fs::is_empty(destination, error)) {
            return error ? FilesystemError(error) : GuestEnotempty;
        }
    }
#ifdef _WIN32
    if (!sameFile && sourceIsDirectory && destinationIsDirectory) {
        error.clear();
        if (!fs::remove(destination, error)) return error ? FilesystemError(error) : GuestEio;
    }
#endif

    error.clear();
    fs::rename(source, destination, error);
    return error ? FilesystemError(error) : 0;
}
}

extern "C" {

int APS5_VABI sceKernelGetCompiledSdkVersion(std::int32_t* version) {
    if (version == nullptr) return SCE_KERNEL_ERROR_EINVAL;
    static const auto* parameters = static_cast<const std::byte*>(ApplicationProcessParameters_nid_no_patch());
    if (!GuestCopyOut_nid_no_patch(version, parameters + 0x10, sizeof(*version))) return SCE_KERNEL_ERROR_EFAULT;
    return 0;
}

int APS5_VABI fork_nid_postfix(void) {
    NotImplemented_nid_no_patch("fork: child processes");
    return -1;
}

int APS5_VABI setsid_nid_postfix(void) {
    return static_cast<int>(Fail(GuestEperm));
}

int APS5_VABI profil_nid_postfix(void* samples, std::size_t size, std::size_t offset, unsigned int scale) {
    (void)samples;
    (void)size;
    (void)offset;
    constexpr unsigned int MaxScale = 1u << 16;
    if (scale > MaxScale) return static_cast<int>(Fail(GuestEinval));
    if (scale != 0) NotImplemented_nid_no_patch("profil: the profiling clock");
    return 0;
}

std::uint16_t APS5_VABI umask_nid_postfix(std::uint16_t mask) {
    return File::ExchangeCreationMask_nid_no_patch(mask);
}

int APS5_VABI sceKernelGetOpenPsIdForSystem(void* id) {
    if (id == nullptr) return GuestEinval;
    return sceKernelGetOpenPsId(id);
}

int APS5_VABI sceKernelGetCpumodeGame(int* mode) {
    if (mode == nullptr) throw std::runtime_error("sceKernelGetCpumodeGame: a null output faults on the console");
    *mode = g_gameCpumode.load(std::memory_order_relaxed);
    return 0;
}

int APS5_VABI sceKernelSetCpumodeGame(int mode) {
    if (mode < 0 || mode > 7) NotImplemented_nid_no_patch("sceKernelSetCpumodeGame with a mode outside 0 to 7");
    if (mode > 1) return SCE_KERNEL_ERROR_EINVAL;
    g_gameCpumode.store(mode, std::memory_order_relaxed);
    return 0;
}

int APS5_VABI sceKernelIsCEX(void) {
    return 1;
}

int APS5_VABI sceKernelIsDevKit(void) {
    return 0;
}

int APS5_VABI sceKernelIsGenuineCEX(void) {
    return 1;
}

int APS5_VABI sceKernelIsGenuineDevKit(void) {
    return 0;
}

int APS5_VABI sceKernelIsGenuineTestKit(void) {
    return 0;
}

int APS5_VABI sceKernelIsTestKit(void) {
    return 0;
}

int APS5_VABI sceKernelIsProspero(void) {
    return 1;
}

int APS5_VABI sceKernelIsKratos(void) {
    return 0;
}

bool IsPs4Process() {
    try {
        const auto titleId = GetAppTitleId_nid_postfix();
        const std::string_view id(titleId.value, strnlen(titleId.value, sizeof(titleId.value)));
        return id.starts_with("CUSA");
    } catch (const std::runtime_error&) {
        return false;
    } catch (const std::length_error&) {
        return false;
    }
}

int APS5_VABI sceKernelIsPs4Process(void) {
    return IsPs4Process() ? 1 : 0;
}

int APS5_VABI get_authinfo_nid_postfix(int pid, KernelAuthInfo* info) {
    if (info == nullptr) return static_cast<int>(Fail(GuestEperm));
    if (pid != 0 && pid != getpid_nid_postfix()) return static_cast<int>(Fail(GuestEsrch));
    KernelAuthInfo local{};
    local.capabilities[0] = 0x2000000000000000;
    if (!GuestCopyOut_nid_no_patch(info, &local, sizeof(local))) return static_cast<int>(Fail(GuestEfault));
    return 0;
}

int APS5_VABI setlogin_nid_postfix(const char* name) {
    constexpr std::size_t MaxLoginName = 33;
    std::string copy;
    const int copied = CopyGuestString_nid_no_patch(name, MaxLoginName, copy);
    if (copied == GuestEnametoolong) return static_cast<int>(Fail(GuestEinval));
    if (copied != 0) return static_cast<int>(Fail(copied));
    if (const int error = SetLogin_nid_no_patch(copy.c_str())) return static_cast<int>(Fail(error));
    return 0;
}

int APS5_VABI fstatfs_nid_postfix(int d, KernelStatfs* buf) {
    return _fstatfs_nid_postfix(d, buf);
}

int APS5_VABI kenv_nid_postfix(int what, const char* name, char* value, int len) {
    auto& environment = KernelEnv();
    if (what == KenvDump) {
        std::size_t capacity = len > 0 ? static_cast<std::size_t>(len) : 0;
        if (capacity > KenvEntries * (KenvNameLength + KenvValueLength + 2)) capacity = KenvEntries * (KenvNameLength + KenvValueLength + 2);
        const bool copy = len > 0 && value != nullptr;
        std::string buffer;
        std::size_t needed = 0;
        {
            const std::lock_guard lock(environment.mutex);
            for (const auto& entry : environment.entries) {
                const std::size_t size = entry.size() + 1;
                needed += size;
                if (copy) {
                    const std::size_t taken = std::min(size, capacity - buffer.size());
                    buffer.append(entry.c_str(), taken);
                }
            }
        }
        if (copy && !GuestCopyOut_nid_no_patch(value, buffer.data(), buffer.size())) return static_cast<int>(Fail(GuestEfault));
        return buffer.size() == needed ? 0 : static_cast<int>(needed);
    }
    std::string key;
    if (const int error = CopyKernelEnvString(name, KenvNameLength + 1, key)) return static_cast<int>(Fail(error));
    const std::lock_guard lock(environment.mutex);
    auto& entries = environment.entries;
    switch (what) {
    case KenvGet: {
        const auto found = FindKernelEnv(entries, key);
        if (found == entries.end()) return static_cast<int>(Fail(GuestEnoent));
        const std::size_t size = found->size() - key.size();
        const std::size_t limit = static_cast<std::size_t>(static_cast<std::int64_t>(len));
        const std::size_t taken = std::min(size, limit);
        if (!GuestCopyOut_nid_no_patch(value, found->c_str() + key.size() + 1, taken)) return static_cast<int>(Fail(GuestEfault));
        return static_cast<int>(taken);
    }
    case KenvSet: {
        if (len == 0) return static_cast<int>(Fail(GuestEinval));
        std::string text;
        if (const int error = CopyKernelEnvString(value, std::min<std::size_t>(len, KenvValueLength + 1), text)) return static_cast<int>(Fail(error));
        const auto found = FindKernelEnv(entries, key);
        if (found != entries.end()) *found = key + "=" + text;
        else if (entries.size() < KenvEntries) entries.push_back(key + "=" + text);
        return 0;
    }
    case KenvUnset: {
        const auto found = FindKernelEnv(entries, key);
        if (found == entries.end()) return static_cast<int>(Fail(GuestEnoent));
        entries.erase(found);
        return 0;
    }
    default: return static_cast<int>(Fail(GuestEinval));
    }
}

int GetModuleInfoExByHandle_nid_no_patch(KernelModule handle, ModuleInfoEx* info);

int APS5_VABI sceKernelGetModuleInfoInternal(KernelModule handle, ModuleInfoEx* info) {
    return GetModuleInfoExByHandle_nid_no_patch(handle, info);
}

int APS5_VABI sceKernelGetModuleListInternal(KernelModule* handles, std::size_t count, std::size_t* actual) {
    return sceKernelGetModuleList(handles, count, actual);
}

KernelModule APS5_VABI sceKernelLoadStartModuleForSysmodule(const char* moduleFileName, std::size_t args, const void* argp, std::uint32_t flags, const KernelLoadModuleOpt* opt, int* res) {
    return sceKernelLoadStartModule(moduleFileName, args, argp, flags, opt, res);
}

int APS5_VABI sceKernelIsModuleLoaded(const char* name) {
    if (!name) throw std::invalid_argument("sceKernelIsModuleLoaded: null name");
    KernelModule handles[0x300];
    std::size_t count = 0;
    if (const int result = sceKernelGetModuleListInternal(handles, std::size(handles), &count)) return result;
    for (std::size_t i = 0; i < count; ++i) {
        ModuleInfo info{};
        info.st_size = sizeof(info);
        if (sceKernelGetModuleInfo(handles[i], &info) == 0 && std::strncmp(info.name, name, sizeof(info.name)) == 0) return 0;
    }
    return SCE_KERNEL_ERROR_ESRCH;
}

int APS5_VABI fstat_nid_postfix(int d, FileStat* sb) {
    return static_cast<int>(_fstat_nid_postfix(d, sb));
}

std::int64_t APS5_VABI readlink_nid_postfix(const char* path, char* buffer, std::size_t size) {
    if (size > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max())) return Fail(GuestEinval);
    std::string copy;
    if (const int error = CopyPath(path, copy)) return Fail(error);
    std::filesystem::path native;
    if (const int error = ResolvePathError_nid_no_patch(path, native)) return Fail(error);
    return ReadNativeLink(native, buffer, size);
}

std::int64_t APS5_VABI readlinkat_nid_postfix(int fd, const char* path, char* buffer, std::size_t size) {
    if (size > static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max())) return Fail(GuestEinval);
    std::string copy;
    if (const int error = CopyPath(path, copy)) return Fail(error);
    if (fd == GuestAtFdcwd || *path == '/') return readlink_nid_postfix(path, buffer, size);
    int error = 0;
    const char* base = GetDirectoryDescriptorPath_nid_no_patch(fd, &error);
    if (base == nullptr) return Fail(error);
    return ReadNativeLink(std::filesystem::path(reinterpret_cast<const char8_t*>(base)) /
                          std::filesystem::path(reinterpret_cast<const char8_t*>(path)), buffer, size);
}

std::int64_t APS5_VABI pathconf_nid_postfix(const char* path, int name) {
    std::string copy;
    if (const int error = CopyPath(path, copy)) return Fail(error);
    if (std::strlen(path) >= GuestPathMax || HasLongComponent(path)) return Fail(GuestEnametoolong);
    std::filesystem::path native;
    if (const int error = ResolvePathError_nid_no_patch(path, native)) return Fail(error);
    std::error_code error;
    const auto status = std::filesystem::status(native, error);
    if (error) return Fail(error == std::errc::no_such_file_or_directory && HasNonDirectoryPrefix(native) ? GuestEnotdir : GuestError(error));
    if (!std::filesystem::exists(status)) return Fail(HasNonDirectoryPrefix(native) ? GuestEnotdir : GuestEnoent);
    return PathConfigurationValue(name);
}

std::int64_t APS5_VABI _fpathconf_nid_postfix(int fd, int name) {
    if (fd < 0) return Fail(GuestEbadf);
    bool withoutVnode = false;
#ifdef _WIN32
    if (GuestFstat(fd, &withoutVnode) != 0) return Fail(GuestEbadf);
#else
    struct stat status{};
    if (::fstat(fd, &status) != 0) return Fail(GuestEbadf);
    withoutVnode = S_ISFIFO(status.st_mode) || S_ISSOCK(status.st_mode);
#endif
    if (name == PcAsyncIo) return PosixAsynchronousIo;
    if (withoutVnode) return name == PcPipeBuf ? 512 : Fail(GuestEinval);
    if (IsKqueueDescriptor_nid_no_patch(fd) || File::IsSharedMemoryDescriptor_nid_no_patch(fd)) return Fail(GuestEopnotsupp);
    return PathConfigurationValue(name);
}

std::int64_t APS5_VABI fpathconf_nid_postfix(int fd, int name) {
    return _fpathconf_nid_postfix(fd, name);
}

#ifdef _WIN32
static int FollowLinkPath(const std::filesystem::path& source, std::filesystem::path& target) {
    target = source;
    const DWORD attributes = GetFileAttributesW(source.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0) return 0;
    const HANDLE handle = CreateFileW(source.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        const DWORD code = GetLastError();
        if (code == ERROR_FILE_NOT_FOUND || code == ERROR_PATH_NOT_FOUND) return GuestEnoent;
        if (code == ERROR_CANT_RESOLVE_FILENAME) return GuestEloop;
        throw std::system_error(static_cast<int>(code), std::system_category(), "link: CreateFileW");
    }
    std::wstring name(MAX_PATH, L'\0');
    DWORD length = GetFinalPathNameByHandleW(handle, name.data(), static_cast<DWORD>(name.size()), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    if (length >= name.size()) {
        name.resize(length);
        length = GetFinalPathNameByHandleW(handle, name.data(), static_cast<DWORD>(name.size()), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
    }
    const DWORD code = GetLastError();
    CloseHandle(handle);
    if (length == 0 || length >= name.size())
        throw std::system_error(static_cast<int>(code), std::system_category(), "link: GetFinalPathNameByHandleW");
    name.resize(length);
    target = std::filesystem::path(name);
    return 0;
}
#endif

static int LinkSourceError(const std::filesystem::path& source) {
    std::error_code error;
    const auto status = std::filesystem::status(source, error);
    if (!std::filesystem::exists(status)) {
#ifdef _WIN32
        std::filesystem::path target;
        if (const int followError = FollowLinkPath(source, target)) return followError;
#endif
        return HasNonDirectoryPrefix(source) ? GuestEnotdir : GuestEnoent;
    }
    return std::filesystem::is_directory(status) ? GuestEperm : 0;
}

static int LinkDestinationError(const std::filesystem::path& destination) {
    std::error_code error;
    if (std::filesystem::exists(std::filesystem::symlink_status(destination, error))) return GuestEexist;
    if (!std::filesystem::is_directory(destination.parent_path(), error)) return HasNonDirectoryPrefix(destination) ? GuestEnotdir : GuestEnoent;
    return 0;
}

static int LinkNative(const std::filesystem::path& source, const std::filesystem::path& destination) {
    if (const int destinationError = LinkDestinationError(destination)) return static_cast<int>(Fail(destinationError));
    std::error_code error;
#ifdef _WIN32
    std::filesystem::path target;
    if (const int followError = FollowLinkPath(source, target)) return static_cast<int>(Fail(followError));
    if (std::filesystem::is_directory(target, error)) return static_cast<int>(Fail(GuestEperm));
#else
    const auto target = std::filesystem::is_symlink(source, error) ? std::filesystem::canonical(source, error) : source;
#endif
    if (!error) std::filesystem::create_hard_link(target, destination, error);
    if (error == std::errc::cross_device_link) return static_cast<int>(Fail(GuestExdev));
    if (error) throw std::system_error(error, "link");
    RecordWrittenPath_nid_no_patch(destination);
    return 0;
}

static int LinkSymbolicLinkItself(const std::filesystem::path& source, const std::filesystem::path& destination) {
    if (const int error = LinkDestinationError(destination)) return static_cast<int>(Fail(error));
#ifdef _WIN32
    if ((GetFileAttributesW(source.c_str()) & FILE_ATTRIBUTE_DIRECTORY) != 0)
        NotImplemented_nid_no_patch("linkat: a hard link to a directory symbolic link itself on Windows");
    if (!CreateHardLinkW(destination.c_str(), source.c_str(), nullptr)) {
        const DWORD error = GetLastError();
        if (error == ERROR_NOT_SAME_DEVICE) return static_cast<int>(Fail(GuestExdev));
        throw std::system_error(static_cast<int>(error), std::system_category(), "linkat");
    }
#else
    if (::linkat(AT_FDCWD, source.c_str(), AT_FDCWD, destination.c_str(), 0) != 0) {
        if (errno == EXDEV) return static_cast<int>(Fail(GuestExdev));
        throw std::system_error(errno, std::generic_category(), "linkat");
    }
#endif
    RecordWrittenPath_nid_no_patch(destination);
    return 0;
}

int APS5_VABI link_nid_postfix(const char* existing, const char* created) {
    std::string existingCopy, createdCopy;
    if (const int error = CopyPath(existing, existingCopy)) return static_cast<int>(Fail(error));
    std::filesystem::path source, destination;
    if (const int error = ResolvePathError_nid_no_patch(existing, source)) return static_cast<int>(Fail(error));
    if (const int error = LinkSourceError(source)) return static_cast<int>(Fail(error));
    if (const int error = CopyPath(created, createdCopy)) return static_cast<int>(Fail(error));
    if (const int error = ResolvePathError_nid_no_patch(created, destination)) return static_cast<int>(Fail(error));
    return LinkNative(source, destination);
}

static int ResolveAtPath(int fd, const char* path, std::filesystem::path& native) {
    if (fd == GuestAtFdcwd || *path == '/') return ResolvePathError_nid_no_patch(path, native);
    if (std::strlen(path) >= GuestPathMax || HasLongComponent(path)) return GuestEnametoolong;
    int error = 0;
    const char* base = GetDirectoryDescriptorPath_nid_no_patch(fd, &error);
    if (base == nullptr) return error;
    native = std::filesystem::path(reinterpret_cast<const char8_t*>(base)) / std::filesystem::path(reinterpret_cast<const char8_t*>(path));
    return 0;
}

int APS5_VABI linkat_nid_postfix(int fd1, const char* path1, int fd2, const char* path2, int flag) {
    if (flag & ~GuestAtSymlinkFollow) return static_cast<int>(Fail(GuestEinval));
    std::string copy1, copy2;
    if (const int error = CopyPath(path1, copy1)) return static_cast<int>(Fail(error));
    std::filesystem::path source, destination;
    if (const int error = ResolveAtPath(fd1, path1, source)) return static_cast<int>(Fail(error));
    const bool linkItself = flag == 0 && IsSymbolicLink(source);
    if (!linkItself)
        if (const int error = LinkSourceError(source)) return static_cast<int>(Fail(error));
    if (const int error = CopyPath(path2, copy2)) return static_cast<int>(Fail(error));
    if (const int error = ResolveAtPath(fd2, path2, destination)) return static_cast<int>(Fail(error));
    if (linkItself) return LinkSymbolicLinkItself(source, destination);
    return LinkNative(source, destination);
}

int APS5_VABI fchdir_nid_postfix(int fd) {
    int error = 0;
    const char* path = GetDirectoryDescriptorPath_nid_no_patch(fd, &error);
    if (path == nullptr && error == GuestEnotdir && DescriptorHasNoVnode_nid_no_patch(fd)) return static_cast<int>(Fail(GuestEinval));
    if (path == nullptr) return static_cast<int>(Fail(error));
#ifndef _WIN32
    if (::faccessat(fd, ".", X_OK, AT_EACCESS) != 0) return static_cast<int>(Fail(errno));
#endif
    error = SetWorkingDirectory_nid_no_patch(path);
    if (error != 0) return static_cast<int>(Fail(error));
    return 0;
}

int SetTimeOfDay_nid_no_patch(const KernelTimeval* tv, const KernelTimezone* tz);

int APS5_VABI settimeofday_nid_postfix(const KernelTimeval* tv, const KernelTimezone* tz) {
    return SetTimeOfDay_nid_no_patch(tv, tz);
}

int APS5_VABI chroot_nid_postfix(const char* path) {
    std::string copy;
    if (const int error = CopyPath(path, copy)) return static_cast<int>(Fail(error));
    if (std::strlen(path) >= GuestPathMax || HasLongComponent(path)) return static_cast<int>(Fail(GuestEnametoolong));
    std::filesystem::path native;
    if (const int error = ResolvePathError_nid_no_patch(path, native)) return static_cast<int>(Fail(error));
    std::error_code error;
    const auto status = std::filesystem::status(native, error);
    if (status.type() == std::filesystem::file_type::not_found) return static_cast<int>(Fail(HasNonDirectoryPrefix(native) ? GuestEnotdir : GuestEnoent));
    if (error) return static_cast<int>(Fail(GuestError(error)));
    if (!std::filesystem::is_directory(status)) return static_cast<int>(Fail(GuestEnotdir));
    const bool root = std::filesystem::equivalent(native, ResolvePath_nid_no_patch("/"), error);
    if (error) return static_cast<int>(Fail(GuestError(error)));
    if (root) return 0;
    throw std::runtime_error(std::string("chroot: changing the root directory to ") + path + " is not supported, the guest path resolver keeps a fixed root");
}

int APS5_VABI kldfind_nid_postfix(const char* file) {
    std::string copy;
    if (const int error = CopyPath(file, copy)) return static_cast<int>(Fail(error));
    return static_cast<int>(Fail(GuestEnoent));
}

int APS5_VABI kldfirstmod_nid_postfix(int fileid) {
    return static_cast<int>(Fail(GuestEnoent));
}

int APS5_VABI kldnext_nid_postfix(int fileid) {
    if (fileid < 0) return static_cast<int>(Fail(GuestEnoent));
    if (fileid == 0) return 0;
    return static_cast<int>(Fail(GuestEnoent));
}

int APS5_VABI kldunload_nid_postfix(int fileid) {
    (void)fileid;
    return static_cast<int>(Fail(GuestEperm));
}

int APS5_VABI kldstat_nid_postfix(int fileid, void* stat) {
    struct FileStatus {
        std::int32_t version;
        char name[GuestPathMax];
        std::int32_t refs;
        std::int32_t id;
        std::uint64_t address;
        std::uint64_t size;
        char pathname[GuestPathMax];
    };
    constexpr auto Version1 = static_cast<std::int32_t>(offsetof(FileStatus, pathname));
    constexpr auto Version2 = static_cast<std::int32_t>(sizeof(FileStatus));
    static_assert(Version1 == 1056 && Version2 == 2080);
    std::int32_t version = 0;
    if (!GuestCopyIn_nid_no_patch(&version, stat, sizeof(version))) return static_cast<int>(Fail(GuestEfault));
    if (version != Version1 && version != Version2) return static_cast<int>(Fail(GuestEinval));
    (void)fileid;
    return static_cast<int>(Fail(GuestEnoent));
}

int APS5_VABI kldsym_nid_postfix(int fileid, int cmd, void* data) {
    struct SymbolLookup {
        std::int32_t version;
        const char* name;
        std::uint64_t value;
        std::uint64_t size;
    };
    static_assert(sizeof(SymbolLookup) == 32);
    constexpr int LookupCommand = 1;
    SymbolLookup lookup{};
    if (!GuestCopyIn_nid_no_patch(&lookup, data, sizeof(lookup))) return static_cast<int>(Fail(GuestEfault));
    if (lookup.version != static_cast<std::int32_t>(sizeof(lookup)) || cmd != LookupCommand) return static_cast<int>(Fail(GuestEinval));
    std::string name;
    if (const int error = CopyGuestPath_nid_no_patch(lookup.name, name)) return static_cast<int>(Fail(error));
    if (fileid != 0) return static_cast<int>(Fail(GuestEnoent));
    throw std::runtime_error("kldsym: looking up the symbol " + name + " in every kernel module is not supported");
}

int APS5_VABI symlink_nid_postfix(const char* target, const char* created) {
    std::string targetCopy, createdCopy;
    if (const int error = CopyPath(target, targetCopy)) return static_cast<int>(Fail(error));
    if (const int error = CopyPath(created, createdCopy)) return static_cast<int>(Fail(error));
    const std::filesystem::path contents(target);
    if (contents.has_root_name() || contents.has_root_directory()) {
        NotImplemented_nid_no_patch("symlink: absolute targets");
        return -1;
    }
    for (const auto& part : contents) {
        if (part == "..") {
            NotImplemented_nid_no_patch("symlink: parent-directory targets");
            return -1;
        }
    }
    std::filesystem::path native;
    if (const int error = ResolvePathError_nid_no_patch(created, native)) return static_cast<int>(Fail(error));
    std::error_code error;
#ifdef _WIN32
    if (GetFileAttributesW(native.wstring().c_str()) != INVALID_FILE_ATTRIBUTES) return static_cast<int>(Fail(GuestEexist));
#else
    if (std::filesystem::exists(std::filesystem::symlink_status(native, error))) return static_cast<int>(Fail(GuestEexist));
#endif
    if (!std::filesystem::is_directory(native.parent_path(), error)) return static_cast<int>(Fail(HasNonDirectoryPrefix(native) ? GuestEnotdir : GuestEnoent));
    const bool directory = std::filesystem::is_directory(native.parent_path() / contents, error);
#ifdef _WIN32
    DWORD flags = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
    if (directory) flags |= SYMBOLIC_LINK_FLAG_DIRECTORY;
    if (!CreateSymbolicLinkW(native.wstring().c_str(), std::filesystem::path(contents).make_preferred().wstring().c_str(), flags)) {
        const DWORD code = GetLastError();
        if (code == ERROR_PRIVILEGE_NOT_HELD) return static_cast<int>(Fail(GuestEperm));
        if (code == ERROR_ALREADY_EXISTS || code == ERROR_FILE_EXISTS) return static_cast<int>(Fail(GuestEexist));
        if (code == ERROR_PATH_NOT_FOUND) return static_cast<int>(Fail(GuestEnoent));
        throw std::system_error(static_cast<int>(code), std::system_category(), "symlink");
    }
#else
    if (directory)
        std::filesystem::create_directory_symlink(contents, native, error);
    else
        std::filesystem::create_symlink(contents, native, error);
    if (error) throw std::system_error(error, "symlink");
#endif
    RecordWrittenPath_nid_no_patch(native);
    return 0;
}

int APS5_VABI openat_nid_postfix(int fd, const char* path, int flags, ...) {
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
    if (File::InvalidOpenFlags(flags)) return static_cast<int>(Fail(GuestEinval));
    std::string copy;
    if (const int error = CopyPath(path, copy)) return static_cast<int>(Fail(error));
    if (fd == GuestAtFdcwd || *path == '/') return open_nid_postfix(path, flags, mode);
    const int result = OpenInDirectory_nid_no_patch(fd, path, flags, static_cast<std::uint16_t>(mode));
    if (result < 0) return static_cast<int>(Fail(result & 0xffff));
    return result;
}

int APS5_VABI _openat_nid_postfix(int fd, const char* path, int flags, ...) {
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
    return openat_nid_postfix(fd, path, flags, mode);
}


int APS5_VABI mkdirat_nid_postfix(int fd, const char* path, std::uint16_t mode) {
    std::string copy;
    if (const int error = CopyPath(path, copy)) return static_cast<int>(Fail(error));
    if (fd == GuestAtFdcwd || *path == '/') return mkdir_nid_postfix(path, mode);
    if (std::strlen(path) >= GuestPathMax || HasLongComponent(path)) return static_cast<int>(Fail(GuestEnametoolong));
    int error = 0;
    const char* base = GetDirectoryDescriptorPath_nid_no_patch(fd, &error);
    if (base == nullptr) return static_cast<int>(Fail(error));
    auto native = std::filesystem::path(reinterpret_cast<const char8_t*>(base)) /
                  std::filesystem::path(reinterpret_cast<const char8_t*>(path));
    while (!native.has_filename() && native.has_relative_path()) native = native.parent_path();
#ifdef _WIN32
    if (!CreateDirectoryW(native.wstring().c_str(), nullptr)) {
        const DWORD code = GetLastError();
        if (code == ERROR_ALREADY_EXISTS || code == ERROR_FILE_EXISTS) return static_cast<int>(Fail(GuestEexist));
        if (code == ERROR_ACCESS_DENIED) return static_cast<int>(Fail(GuestEperm));
        if (code == ERROR_FILENAME_EXCED_RANGE) return static_cast<int>(Fail(GuestEnametoolong));
        if (code == ERROR_CANT_RESOLVE_FILENAME) return static_cast<int>(Fail(GuestEloop));
        if (code == ERROR_PATH_NOT_FOUND) {
            if (HasNonDirectoryPrefix(native)) return static_cast<int>(Fail(GuestEnotdir));
            return static_cast<int>(Fail(GuestEnoent));
        }
        return static_cast<int>(Fail(GuestEio));
    }
#else
    if (::mkdir(native.c_str(), static_cast<mode_t>(mode & 0777)) != 0) {
        const int nativeError = errno;
        if ((nativeError == ENOENT || nativeError == ENOTDIR) && HasNonDirectoryPrefix(native))
            return static_cast<int>(Fail(GuestEnotdir));
        return static_cast<int>(Fail(GuestError(std::error_code(nativeError, std::generic_category()))));
    }
#endif
    RecordWrittenPath_nid_no_patch(native);
    return 0;
}

int APS5_VABI fchmodat_nid_postfix(int fd, const char* path, int mode, int flag) {
    if (flag != 0 && flag != GuestAtSymlinkNofollow) return static_cast<int>(Fail(GuestEinval));
    std::string copy;
    if (const int error = CopyPath(path, copy)) return static_cast<int>(Fail(error));
    std::filesystem::path native;
    if (fd == GuestAtFdcwd || *path == '/') {
        if (const int error = ResolvePathError_nid_no_patch(path, native)) return static_cast<int>(Fail(error));
    } else {
        int error = 0;
        const char* base = GetDirectoryDescriptorPath_nid_no_patch(fd, &error);
        if (base == nullptr) return static_cast<int>(Fail(error));
        native = std::filesystem::path(reinterpret_cast<const char8_t*>(base)) / std::filesystem::path(reinterpret_cast<const char8_t*>(path));
    }
    if (flag == GuestAtSymlinkNofollow) {
#ifdef _WIN32
        const DWORD attributes = GetFileAttributesW(native.wstring().c_str());
        const bool symbolic = attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
        std::error_code status;
        const bool symbolic = std::filesystem::is_symlink(std::filesystem::symlink_status(native, status));
#endif
        if (symbolic) {
#ifdef __APPLE__
        if (::fchmodat(AT_FDCWD, native.c_str(), static_cast<mode_t>(mode & 07777), AT_SYMLINK_NOFOLLOW) != 0) {
            return static_cast<int>(Fail(GuestFileError(errno)));
        }
#endif
        RecordWrittenPath_nid_no_patch(native);
        return 0;
    }
    }
    if (ChmodNative_nid_no_patch(native, mode & 07777) != 0) return static_cast<int>(Fail(errno == ENOENT && HasNonDirectoryPrefix(native) ? GuestEnotdir : GuestFileError(errno)));
    RecordWrittenPath_nid_no_patch(native);
    return 0;
}

int APS5_VABI lchmod_nid_postfix(const char* path, std::uint16_t mode) {
    std::string copy;
    if (path != nullptr) {
        if (const int error = CopyPath(path, copy)) return static_cast<int>(Fail(error));
    }
#ifndef __APPLE__
    std::filesystem::path native;
    if (path != nullptr && ResolvePathError_nid_no_patch(path, native) == 0) {
#ifdef _WIN32
        const DWORD attributes = GetFileAttributesW(native.wstring().c_str());
        const bool symbolic = attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
        std::error_code status;
        const bool symbolic = std::filesystem::is_symlink(std::filesystem::symlink_status(native, status));
#endif
        if (symbolic) NotImplemented_nid_no_patch("lchmod: the mode of a symbolic link on a host that cannot change it");
    }
#endif
    return fchmodat_nid_postfix(GuestAtFdcwd, path, mode, GuestAtSymlinkNofollow);
}

int APS5_VABI fchown_nid_postfix(int fd, std::uint32_t uid, std::uint32_t gid) {
#ifdef _WIN32
    if (GuestFstat(fd) != 0) return static_cast<int>(Fail(GuestEbadf));
#else
    struct stat status{};
    if (::fstat(fd, &status) != 0) return static_cast<int>(Fail(GuestEbadf));
#endif
    RequireGuestOwner("fchown", uid, gid);
    return 0;
}

int APS5_VABI sched_rr_get_interval_nid_postfix(int pid, KernelTimespec* interval) {
    constexpr int GuestEsrch = 3;
    const int self = getpid_nid_postfix();
    if (pid != 0 && pid != self) return static_cast<int>(Fail(GuestEsrch));
    const KernelTimespec quantum{0, 94000000};
    if (interval == nullptr || !GuestCopyOut_nid_no_patch(interval, &quantum, sizeof(quantum))) return static_cast<int>(Fail(GuestEfault));
    return 0;
}

int APS5_VABI chown_nid_postfix(const char* path, std::uint32_t uid, std::uint32_t gid) {
    FileStat status{};
    if (stat_nid_postfix(path, &status) != 0) return -1;
    RequireGuestOwner("chown", uid, gid);
    return 0;
}

int APS5_VABI lchown_nid_postfix(const char* path, std::uint32_t uid, std::uint32_t gid) {
    FileStat status{};
    if (lstat_nid_postfix(path, &status) != 0) return -1;
    RequireGuestOwner("lchown", uid, gid);
    return 0;
}

int APS5_VABI fchownat_nid_postfix(int fd, const char* path, std::uint32_t uid, std::uint32_t gid, int flag) {
    if (flag & ~GuestAtSymlinkNofollow) return static_cast<int>(Fail(GuestEinval));
    FileStat status{};
    if (fstatat_nid_postfix(fd, path, &status, flag) != 0) return -1;
    RequireGuestOwner("fchownat", uid, gid);
    return 0;
}

int APS5_VABI fstatat_nid_postfix(int fd, const char* path, FileStat* sb, int flag) {
    if (flag & ~GuestAtSymlinkNofollow) return static_cast<int>(Fail(GuestEinval));
    std::string copy;
    if (const int error = CopyPath(path, copy)) return static_cast<int>(Fail(error));
    if (fd == GuestAtFdcwd || *path == '/') return flag ? lstat_nid_postfix(path, sb) : stat_nid_postfix(path, sb);
    const int result = StatInDirectory_nid_no_patch(fd, path, sb, flag == 0);
    if (result < 0) return static_cast<int>(Fail(result & 0xffff));
    return 0;
}

int ChangeFileFlags_nid_no_patch(const char* function, const char* path, std::uint64_t flags, bool follow);

int APS5_VABI lchflags_nid_postfix(const char* path, std::uint64_t flags) {
    return ChangeFileFlags_nid_no_patch("lchflags", path, flags, false);
}

int APS5_VABI unlinkat_nid_postfix(int fd, const char* path, int flag) {
    if (flag & ~GuestAtRemovedir) return static_cast<int>(Fail(GuestEinval));
    std::string copy;
    if (const int error = CopyPath(path, copy)) return static_cast<int>(Fail(error));
    if (fd == GuestAtFdcwd || *path == '/') return (flag & GuestAtRemovedir) ? rmdir_nid_postfix(path) : unlink_nid_postfix(path);
    if (std::strlen(path) >= GuestPathMax || HasLongComponent(path)) return static_cast<int>(Fail(GuestEnametoolong));
    int error = 0;
    const char* base = GetDirectoryDescriptorPath_nid_no_patch(fd, &error);
    if (base == nullptr) return static_cast<int>(Fail(error));
    auto native = std::filesystem::path(reinterpret_cast<const char8_t*>(base)) /
                  std::filesystem::path(reinterpret_cast<const char8_t*>(path));
    const bool trailingSlash = !native.has_filename() && native.has_relative_path();
    while (!native.has_filename() && native.has_relative_path()) native = native.parent_path();
    std::error_code statusError;
    const auto symlinkStatus = std::filesystem::symlink_status(native, statusError);
    if (IsDotOrDotDot(path)) return static_cast<int>(Fail(GuestEinval));
    if (flag & GuestAtRemovedir) {
        if (std::filesystem::is_symlink(symlinkStatus)) return static_cast<int>(Fail(GuestEnotdir));
#ifdef _WIN32
        const DWORD attributes = GetFileAttributesW(native.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            std::string junctionTarget;
            if (File::WindowsSymbolicLink::ReadJunction(native, junctionTarget, "unlinkat")) return static_cast<int>(Fail(GuestEnotdir));
        }
#endif
        if (!std::filesystem::is_directory(symlinkStatus)) {
            if (HasNonDirectoryPrefix(native) || std::filesystem::exists(symlinkStatus))
                return static_cast<int>(Fail(GuestEnotdir));
            return static_cast<int>(Fail(GuestEnoent));
        }
        if (!std::filesystem::is_empty(native, statusError)) return static_cast<int>(Fail(GuestEnotempty));
#ifdef _WIN32
        struct _stat64 status{};
        const bool unlocked = ::_wstat64(native.c_str(), &status) == 0 && !(status.st_mode & _S_IWRITE) &&
            ::_wchmod(native.c_str(), _S_IREAD | _S_IWRITE) == 0;
        if (!std::filesystem::remove(native, statusError)) {
            if (unlocked) ::_wchmod(native.c_str(), _S_IREAD);
            return static_cast<int>(Fail(GuestEio));
        }
#else
        if (::rmdir(native.c_str()) != 0) {
            const int nativeError = errno;
            if ((nativeError == ENOENT || nativeError == ENOTDIR) && HasNonDirectoryPrefix(native))
                return static_cast<int>(Fail(GuestEnotdir));
            return static_cast<int>(Fail(GuestError(std::error_code(nativeError, std::generic_category()))));
        }
#endif
    } else {
        if (trailingSlash) {
            if (std::filesystem::is_directory(symlinkStatus)) return static_cast<int>(Fail(GuestEperm));
            if (std::filesystem::exists(symlinkStatus) || HasNonDirectoryPrefix(native))
                return static_cast<int>(Fail(GuestEnotdir));
            return static_cast<int>(Fail(GuestEnoent));
        }
#ifdef _WIN32
        const DWORD attributes = GetFileAttributesW(native.c_str());
        if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 && (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            std::string junctionTarget;
            if (File::WindowsSymbolicLink::ReadJunction(native, junctionTarget, "unlinkat")) {
                if (RemoveDirectoryW(native.c_str()) == 0) return static_cast<int>(Fail(GuestEperm));
                RecordWrittenPath_nid_no_patch(native);
                return 0;
            }
        }
#endif
        if (std::filesystem::is_directory(symlinkStatus)) return static_cast<int>(Fail(GuestEperm));
#ifdef _WIN32
        struct _stat64 status{};
        const bool unlocked = ::_wstat64(native.c_str(), &status) == 0 && (status.st_mode & _S_IFMT) == _S_IFREG &&
            !(status.st_mode & _S_IWRITE) && ::_wchmod(native.c_str(), _S_IREAD | _S_IWRITE) == 0;
        if (::_wunlink(native.c_str()) != 0) {
            const int nativeError = errno;
            if (unlocked) ::_wchmod(native.c_str(), _S_IREAD);
            if (HasNonDirectoryPrefix(native)) return static_cast<int>(Fail(GuestEnotdir));
            return static_cast<int>(Fail(GuestError(std::error_code(nativeError, std::generic_category()))));
        }
#else
        if (::unlink(native.c_str()) != 0) {
            const int nativeError = errno;
            if ((nativeError == ENOENT || nativeError == ENOTDIR) && HasNonDirectoryPrefix(native))
                return static_cast<int>(Fail(GuestEnotdir));
            return static_cast<int>(Fail(GuestError(std::error_code(nativeError, std::generic_category()))));
        }
#endif
    }
    RecordWrittenPath_nid_no_patch(native);
    return 0;
}

int APS5_VABI scePthreadCondattrGetclock(const PthreadCondattr* attr, KernelClockid* clockId) {
    if (!attr || !*attr) return SCE_KERNEL_ERROR_EINVAL;
    if (!clockId) throw std::runtime_error("scePthreadCondattrGetclock: null clock id");
    *clockId = (*attr)->_clockid;
    return 0;
}

int APS5_VABI renameat_nid_postfix(int oldfd, const char* from, int newfd, const char* to) {
    std::string fromCopy, toCopy;
    if (const int error = CopyPath(from, fromCopy)) return static_cast<int>(Fail(error));
    if ((oldfd == GuestAtFdcwd || *from == '/') && newfd == GuestAtFdcwd)
        return RenameGuestPath_nid_no_patch(from, to);
    if (std::strlen(from) >= GuestPathMax || HasLongComponent(from))
        return static_cast<int>(Fail(GuestEnametoolong));
    if (IsDotOrDotDot(from)) return static_cast<int>(Fail(GuestEinval));

    std::filesystem::path source;
    if (oldfd == GuestAtFdcwd || *from == '/') {
        if (const int error = ResolvePathError_nid_no_patch(from, source)) return static_cast<int>(Fail(error));
    } else {
        int error = 0;
        const char* base = GetDirectoryDescriptorPath_nid_no_patch(oldfd, &error);
        if (base == nullptr) return static_cast<int>(Fail(error));
        source = std::filesystem::path(reinterpret_cast<const char8_t*>(base)) /
                 std::filesystem::path(reinterpret_cast<const char8_t*>(from));
    }
    const std::string_view fromView(from);
    const bool oldTrailingSlash = !fromView.empty() && fromView.back() == '/';
    while (!source.has_filename() && source.has_relative_path()) source = source.parent_path();
    std::error_code statusError;
    const auto sourceStatus = std::filesystem::symlink_status(source, statusError);
    if (statusError && statusError != std::errc::no_such_file_or_directory)
        return static_cast<int>(Fail(FilesystemError(statusError)));
    if (sourceStatus.type() == std::filesystem::file_type::not_found)
        return static_cast<int>(Fail(HasNonDirectoryPrefix(source) ? GuestEnotdir : GuestEnoent));
    if (oldTrailingSlash && !std::filesystem::is_directory(sourceStatus)) return static_cast<int>(Fail(GuestEnotdir));

    if (const int error = CopyPath(to, toCopy)) return static_cast<int>(Fail(error));
    if (std::strlen(to) >= GuestPathMax || HasLongComponent(to)) return static_cast<int>(Fail(GuestEnametoolong));
    if (IsDotOrDotDot(to)) return static_cast<int>(Fail(GuestEinval));

    std::filesystem::path destination;
    if (newfd == GuestAtFdcwd || *to == '/') {
        if (const int error = ResolvePathError_nid_no_patch(to, destination)) return static_cast<int>(Fail(error));
    } else {
        int error = 0;
        const char* base = GetDirectoryDescriptorPath_nid_no_patch(newfd, &error);
        if (base == nullptr) return static_cast<int>(Fail(error));
        destination = std::filesystem::path(reinterpret_cast<const char8_t*>(base)) /
                      std::filesystem::path(reinterpret_cast<const char8_t*>(to));
    }

    const std::string_view toView(to);
    const bool newTrailingSlash = !toView.empty() && toView.back() == '/';

    while (!destination.has_filename() && destination.has_relative_path()) destination = destination.parent_path();

    if (newTrailingSlash && sourceStatus.type() != std::filesystem::file_type::not_found &&
        !std::filesystem::is_directory(sourceStatus))
        return static_cast<int>(Fail(GuestEnotdir));

    const int error = RenamePath(source, destination);
    if (error) return static_cast<int>(Fail(error));
    RecordWrittenPath_nid_no_patch(source);
    RecordWrittenPath_nid_no_patch(destination);
    return 0;
}

bool SetDefaultStackSize_nid_no_patch(std::size_t size);

int APS5_VABI pthread_set_defaultstacksize_np_nid_postfix(std::size_t size) {
    return SetDefaultStackSize_nid_no_patch(size) ? 0 : GuestEinval;
}

int APS5_VABI scePthreadSetDefaultstacksize(std::size_t size) {
    return SetDefaultStackSize_nid_no_patch(size) ? 0 : SceKernelError(GuestEinval);
}

void APS5_VABI scePthreadSetName(Pthread thread, const char* name) {
    pthread_set_name_np_nid_postfix(thread, name);

}

int APS5_VABI sceKernelTitleWorkaroundIsEnabled(const KernelTitleWorkaround* workaround, std::int32_t bit, std::int32_t* enabled) {
    if (!workaround || !enabled) return static_cast<int>(0x8002000E);
    if (static_cast<std::uint32_t>(bit) > 0x7F) return static_cast<int>(0x80020016);
    KernelTitleWorkaround localWorkaround{};
    const int saved = errno;
    if (!GuestCopyIn_nid_no_patch(&localWorkaround, workaround, sizeof(localWorkaround))) {
        errno = saved;
        return static_cast<int>(0x8002000E);
    }
    const std::int32_t value = static_cast<std::int32_t>((localWorkaround.ids[bit >> 6] >> (bit & 0x3F)) & 1);
    const bool copied = GuestCopyOut_nid_no_patch(enabled, &value, sizeof(value));
    errno = saved;
    if (!copied) return static_cast<int>(0x8002000E);
    return 0;
}

int APS5_VABI sceKernelTitleWorkdaroundIsEnabled(const KernelTitleWorkaround* workaround, std::int32_t bit, std::int32_t* enabled) {
    return sceKernelTitleWorkaroundIsEnabled(workaround, bit, enabled);
}

int APS5_VABI sceKernelGetProcessType(int pid) {
    if (pid != getpid_nid_postfix() && pid != -1 && pid != 0) return static_cast<int>(0x80020003);
    return 0;
}

}
