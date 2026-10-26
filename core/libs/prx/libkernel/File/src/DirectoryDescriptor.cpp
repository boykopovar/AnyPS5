#include "prx/libc/include/GuestFileNumber.hpp"
#include "prx/libkernel/File/include/DirectoryDescriptor.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/File/include/NativeStat.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libc/include/AppArchive.hpp"
#include "prx/libc/include/FilesystemError.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/General.hpp"
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

constexpr std::uint8_t GuestUnknownType = 0;
constexpr std::uint8_t GuestDirectoryType = 4;
constexpr std::uint8_t GuestRegularType = 8;
constexpr std::uint8_t GuestSymbolicLinkType = 10;

struct DirectoryEntry {
    std::string name;
    std::uint32_t fileNumber;
    std::uint8_t type;
};

class DirectoryEntryNameTooLong : public std::runtime_error {
public:
    DirectoryEntryNameTooLong() : std::runtime_error("Directory entry name exceeds the guest 255-byte limit") {}
};

struct DirectoryState {
    std::mutex mutex;
    std::filesystem::path path;
    bool virtualNamespace = false;
    bool nativeBacked = false;
    std::vector<DirectoryEntry> entries;
    std::size_t cursor = 0;
    bool loaded = false;
};

std::mutex g_mutex;
std::map<int, std::shared_ptr<DirectoryState>> g_directories;
std::atomic<std::size_t> g_directoryCount{0};

int DirectoryError(const std::error_code& code) {
#ifdef _WIN32
    if (code.category() == std::system_category())
        return FilesystemError(std::error_code(WindowsOpenError_nid_no_patch(static_cast<unsigned long>(code.value())), std::generic_category()));
#endif
    return FilesystemError(code);
}

std::shared_ptr<DirectoryState> Find(int fd) {
    if (g_directoryCount.load(std::memory_order_relaxed) == 0) return nullptr;
    std::lock_guard lock(g_mutex);
    const auto found = g_directories.find(fd);
    return found == g_directories.end() ? nullptr : found->second;
}

#ifndef _WIN32
std::uint8_t DirectoryEntryType(const std::filesystem::file_status& status) {
    switch (status.type()) {
    case std::filesystem::file_type::regular: return GuestRegularType;
    case std::filesystem::file_type::directory: return GuestDirectoryType;
    case std::filesystem::file_type::symlink: return GuestSymbolicLinkType;
    case std::filesystem::file_type::block: return 6;
    case std::filesystem::file_type::character: return 2;
    case std::filesystem::file_type::fifo: return 1;
    case std::filesystem::file_type::socket: return 12;
    default: return 0;
    }
}
#else
bool InsideApp0(const std::filesystem::path& directory) {
    const auto root = ResolveDiskPath_nid_no_patch("/app0");
    const auto handle = CreateFileW(root.c_str(), FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) return false;
    const int descriptor = ::_open_osfhandle(reinterpret_cast<std::intptr_t>(handle), _O_RDONLY | _O_BINARY);
    if (descriptor < 0) {
        CloseHandle(handle);
        return false;
    }
    const auto app0 = File::WindowsDescriptorPath_nid_no_patch(descriptor);
    ::_close(descriptor);
    if (!app0) return false;
    auto entry = directory.begin();
    for (const auto& component : *app0) {
        if (entry == directory.end()) return false;
        const auto& left = entry->native();
        const auto& right = component.native();
        if (CompareStringOrdinal(left.c_str(), static_cast<int>(left.size()),
                right.c_str(), static_cast<int>(right.size()), TRUE) != CSTR_EQUAL) return false;
        ++entry;
    }
    return true;
}

std::uint8_t EntryType(const std::filesystem::path& path, bool followLinks) {
    FileStat status{};
    if (!File::FillLinkStat(path, &status))
        return GuestUnknownType;
    if (followLinks && (status.st_mode & 0170000) == 0120000) {
        FileStat target{};
        const int savedErrno = errno;
        const bool found = File::TryFillFileStat(path, &target);
        errno = savedErrno;
        if (found && ((target.st_mode & 0170000) == 0040000 || (target.st_mode & 0170000) == 0100000))
            status = target;
    }
    switch (status.st_mode & 0170000) {
        case 0040000: return GuestDirectoryType;
        case 0100000: return GuestRegularType;
        case 0120000: return GuestSymbolicLinkType;
        default: NotImplemented_nid_no_patch("getdents of a Windows file type other than a directory, regular file or symbolic link");
    }
}
#endif

void Add(DirectoryState& state, const std::string& name, std::uint64_t fileNumber, std::uint8_t type) {
    if (name.size() > 255) throw DirectoryEntryNameTooLong();
    state.entries.push_back({name, GuestFileNumber_nid_no_patch(fileNumber), type});
}

void Load(DirectoryState& state, int fd) {
    if (state.loaded) return;
    state.entries.clear();
    const auto directoryPath = state.nativeBacked ? File::DirectoryDescriptorPath(fd).value_or(state.path) : state.path;
    AppArchiveNode node{};
    const bool hasArchiveDirectory = AppArchiveLookup_nid_no_patch(directoryPath, &node) && node.Directory;
    std::error_code hostError;
    const auto hostPathStatus = std::filesystem::status(directoryPath, hostError);
    if (hostError && hostError != std::errc::no_such_file_or_directory)
        throw std::system_error(hostError, "stat directory descriptor path");
    const bool hostPathExists = !hostError && std::filesystem::exists(hostPathStatus);
    const bool hasHostDirectory = hostPathExists && std::filesystem::is_directory(hostPathStatus);
    FileStat directoryStatus{};
    if (hasHostDirectory && !File::TryFillFileStat(directoryPath, &directoryStatus))
        throw std::system_error(errno, std::generic_category(), "stat directory descriptor identity");
    if (!hasHostDirectory && !hasArchiveDirectory) throw std::system_error(ENOTDIR, std::generic_category(), "directory descriptor path");
    std::uint64_t currentNumber = hasArchiveDirectory ? static_cast<std::uint64_t>(node.Handle) + 1 : 0;
    if (hasHostDirectory) currentNumber = static_cast<std::uint64_t>(directoryStatus.st_ino);
    Add(state, ".", currentNumber, GuestDirectoryType);
    std::uint64_t parentNumber = 0;
    FileStat parentStatus{};
    const bool hasHostParent = File::TryFillFileStat(directoryPath.parent_path(), &parentStatus);
    if (hasHostParent) parentNumber = static_cast<std::uint64_t>(parentStatus.st_ino);
    AppArchiveNode parentNode{};
    if (!hasHostParent && AppArchiveLookup_nid_no_patch(directoryPath.parent_path(), &parentNode) && parentNode.Directory)
        parentNumber = static_cast<std::uint64_t>(parentNode.Handle) + 1;
    Add(state, "..", parentNumber, GuestDirectoryType);
    if (hasHostDirectory) {
        std::error_code error;
#ifdef _WIN32
        const int savedErrno = errno;
        const bool followLinks = InsideApp0(directoryPath);
        errno = savedErrno;
#endif
        for (std::filesystem::directory_iterator it(directoryPath, error), end; it != end; it.increment(error)) {
            if (error) throw std::system_error(error, "read directory descriptor");
            const auto utf8Name = it->path().filename().u8string();
            const std::string name(reinterpret_cast<const char*>(utf8Name.data()), utf8Name.size());
            FileStat entryStatus{};
            if (!File::FillLinkStat(it->path(), &entryStatus)) throw std::system_error(errno, std::generic_category(), "stat directory entry");
#ifdef _WIN32
            Add(state, name, static_cast<std::uint64_t>(entryStatus.st_ino), EntryType(it->path(), followLinks));
#else
            Add(state, name, static_cast<std::uint64_t>(entryStatus.st_ino), DirectoryEntryType(it->symlink_status(error)));
#endif
            if (error) throw std::system_error(error, "stat directory entry");
        }
        if (error) throw std::system_error(error, "read directory descriptor");
    }
    if (hasArchiveDirectory) {
        std::vector<std::string> hostNames;
        hostNames.reserve(state.entries.size());
        for (const auto& entry : state.entries) hostNames.push_back(entry.name);
        for (const auto& entry : AppArchiveListUnshadowedDirectory_nid_no_patch(directoryPath, hostNames)) {
            Add(state, entry.Name, static_cast<std::uint64_t>(entry.Handle) + 1, entry.Directory ? GuestDirectoryType : GuestRegularType);
        }
    }
    state.loaded = true;
}

#ifdef _WIN32
int OpenPlaceholder(const std::filesystem::path& path) {
    std::error_code error;
    if (std::filesystem::is_directory(path, error)) {
        SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
        const auto handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            &security, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            errno = WindowsOpenError_nid_no_patch(GetLastError());
            return -1;
        }
        const int fd = ::_open_osfhandle(reinterpret_cast<std::intptr_t>(handle), _O_RDONLY | _O_BINARY);
        if (fd < 0) CloseHandle(handle);
        return fd;
    }
    return ::_open("NUL", _O_RDONLY | _O_BINARY);
}
#else
int OpenPlaceholder(const std::filesystem::path& path) {
    if (AppArchiveHasDirectory_nid_no_patch(path)) return ::open("/dev/null", O_RDONLY | O_CLOEXEC);
    return ::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
}
#endif

#ifdef _WIN32
int OpenStreamDirectory(const wchar_t* path) {
    return File::OpenDirectoryDescriptor(path);
}

struct StreamRegistration {
    StreamRegistration() { LibcRegisterDirectoryStreams_nid_no_patch(&OpenStreamDirectory, &File::ForgetDirectoryDescriptor); }
    ~StreamRegistration() { LibcRegisterDirectoryStreams_nid_no_patch(nullptr, nullptr); }
    StreamRegistration(const StreamRegistration&) = delete;
    StreamRegistration& operator=(const StreamRegistration&) = delete;
};

const StreamRegistration g_streamRegistration;
#endif

}

namespace File {

int OpenDirectoryDescriptor(const std::filesystem::path& path) {
    auto state = std::make_shared<DirectoryState>();
    state->path = path;
    std::error_code statusError;
    const bool hostDirectory = std::filesystem::is_directory(path, statusError);
    AppArchiveNode archiveNode{};
    const bool archiveDirectory = AppArchiveLookup_nid_no_patch(path, &archiveNode) && archiveNode.Directory;
    state->virtualNamespace = archiveDirectory;
#ifdef _WIN32
    state->nativeBacked = hostDirectory;
#else
    state->nativeBacked = hostDirectory && !AppArchiveHasDirectory_nid_no_patch(path);
#endif
    const int fd = OpenPlaceholder(path);
    if (fd < 0) return -1;
    try {
        std::lock_guard lock(g_mutex);
        g_directories[fd] = state;
        g_directoryCount.store(g_directories.size(), std::memory_order_relaxed);
    } catch (...) {
#ifdef _WIN32
        ::_close(fd);
#else
        ::close(fd);
#endif
        throw;
    }
    return fd;
}

bool IsDirectoryDescriptor(int fd) {
    return Find(fd) != nullptr;
}

bool IsVirtualDirectoryDescriptor(int fd) {
    const auto state = Find(fd);
    return state && state->virtualNamespace;
}

bool IsNativeDirectoryDescriptor(int fd) {
    const auto state = Find(fd);
    return state && state->nativeBacked;
}

std::optional<std::filesystem::path> DirectoryDescriptorPath(int fd) {
    if (const auto state = Find(fd)) {
        if (!state->nativeBacked) return state->path;
#ifdef _WIN32
        return WindowsDescriptorPath_nid_no_patch(fd);
#else
        if (fd < 0) return std::nullopt;
        std::error_code error;
        const auto link = std::filesystem::path("/proc/self/fd") / std::to_string(fd);
        const auto target = std::filesystem::read_symlink(link, error);
        if (error) return std::nullopt;
        return target;
#endif
    }
#ifdef _WIN32
    return std::nullopt;
#else
    if (fd < 0) return std::nullopt;
    std::error_code error;
    const auto link = std::filesystem::path("/proc/self/fd") / std::to_string(fd);
    const auto target = std::filesystem::read_symlink(link, error);
    if (error) return std::nullopt;
    if (!std::filesystem::is_directory(target, error)) return std::nullopt;
    return target;
#endif
}

void ForgetDirectoryDescriptor(int fd) {
    if (g_directoryCount.load(std::memory_order_relaxed) == 0) return;
    std::lock_guard lock(g_mutex);
    g_directories.erase(fd);
    g_directoryCount.store(g_directories.size(), std::memory_order_relaxed);
}

void DuplicateDirectoryDescriptor(int from, int to) {
    if (g_directoryCount.load(std::memory_order_relaxed) == 0) return;
    std::lock_guard lock(g_mutex);
    const auto found = g_directories.find(from);
    if (found == g_directories.end()) g_directories.erase(to);
    else g_directories[to] = found->second;
    g_directoryCount.store(g_directories.size(), std::memory_order_relaxed);
}

std::optional<std::int64_t> SeekDirectoryDescriptor(int fd, std::int64_t offset, int whence) {
    const auto state = Find(fd);
    if (!state) return std::nullopt;
    std::lock_guard lock(state->mutex);
    constexpr int SeekSet = 0;
    constexpr int SeekCurrent = 1;
    constexpr int SeekEnd = 2;
    if (whence != SeekSet && whence != SeekCurrent && whence != SeekEnd) {
        errno = EINVAL;
        return std::int64_t{-1};
    }
    if (whence == SeekEnd) {
        try { Load(*state, fd); }
        catch (const std::system_error& error) { errno = DirectoryError(error.code()); return std::int64_t{-1}; }
        catch (const std::exception&) { errno = EIO; return std::int64_t{-1}; }
    }
    const auto cursor = static_cast<std::int64_t>(whence == SeekEnd ? state->entries.size() : state->cursor);
    if (whence == SeekCurrent && offset == 0) return cursor;
    if ((whence == SeekCurrent || whence == SeekEnd) && offset > std::numeric_limits<std::int64_t>::max() - cursor) {
        errno = EOVERFLOW;
        return std::int64_t{-1};
    }
    const std::int64_t target = whence == SeekSet ? offset : cursor + offset;
    if (target < 0) {
        errno = EINVAL;
        return std::int64_t{-1};
    }
    if (target == 0) {
        state->entries.clear();
        state->loaded = false;
        state->cursor = 0;
        return std::int64_t{0};
    }
    try { Load(*state, fd); }
    catch (const std::system_error& error) { errno = DirectoryError(error.code()); return std::int64_t{-1}; }
    state->cursor = static_cast<std::size_t>(target);
    return target;
}

int ReadDirectoryDescriptor(int fd, char* buf, int nbytes, std::int64_t* base) {
    const auto state = Find(fd);
    if (!state) return SCE_KERNEL_ERROR_ENOTDIR;
    if (buf == nullptr) return SCE_KERNEL_ERROR_EFAULT;
    if (nbytes <= 0) return SCE_KERNEL_ERROR_EINVAL;
    std::lock_guard lock(state->mutex);
    try { Load(*state, fd); }
    catch (const std::system_error& error) {
        return SceKernelError(DirectoryError(error.code()));
    }
    catch (const DirectoryEntryNameTooLong&) { throw; }
    catch (const std::exception&) { return SCE_KERNEL_ERROR_EIO; }
    if (base != nullptr) *base = static_cast<std::int64_t>(state->cursor);
    std::size_t used = 0;
    while (state->cursor < state->entries.size()) {
        const auto& entry = state->entries[state->cursor];
        const std::size_t record = (8 + entry.name.size() + 1 + 3) & ~std::size_t{3};
        if (used + record > static_cast<std::size_t>(nbytes)) {
            if (used == 0) return SCE_KERNEL_ERROR_EINVAL;
            break;
        }
        char* out = buf + used;
        std::memset(out, 0, record);
        const auto recordLength = static_cast<std::uint16_t>(record);
        const auto nameLength = static_cast<std::uint8_t>(entry.name.size());
        std::memcpy(out, &entry.fileNumber, sizeof(entry.fileNumber));
        std::memcpy(out + 4, &recordLength, sizeof(recordLength));
        out[6] = static_cast<char>(entry.type);
        out[7] = static_cast<char>(nameLength);
        std::memcpy(out + 8, entry.name.c_str(), entry.name.size() + 1);
        used += record;
        ++state->cursor;
    }
    return static_cast<int>(used);
}

}

extern "C" const char* GetDirectoryDescriptorPath_nid_no_patch(int fd, int* error) {
    if (error == nullptr) return nullptr;
    if (!DescriptorIsOpen_nid_no_patch(fd)) {
        *error = EBADF;
        return nullptr;
    }
    const auto directory = File::DirectoryDescriptorPath(fd);
    if (!directory) {
#ifdef _WIN32
        *error = File::IsDirectoryDescriptor(fd) ? errno : ENOTDIR;
#else
        *error = ENOTDIR;
#endif
        return nullptr;
    }
    struct PathTag {};
    auto& path = HostThreadLocal<std::string, PathTag>();
    const auto utf8Path = directory->u8string();
    path.assign(reinterpret_cast<const char*>(utf8Path.data()), utf8Path.size());
    *error = 0;
    return path.c_str();
}

extern "C" std::uint64_t GetDirectoryDescriptorPosition_nid_no_patch(int fd) {
    const auto position = File::SeekDirectoryDescriptor(fd, 0, 1);
    return position && *position > 0 ? static_cast<std::uint64_t>(*position) : 0;
}

extern "C" int IsDirectoryDescriptor_nid_no_patch(int fd) {
    return File::IsDirectoryDescriptor(fd) ? 1 : 0;
}

extern "C" int ReadDirectoryDescriptor_nid_no_patch(int fd, char* buf, int nbytes) {
    return File::ReadDirectoryDescriptor(fd, buf, nbytes);
}

extern "C" std::int64_t SeekDirectoryDescriptor_nid_no_patch(int fd, std::int64_t offset, int whence) {
    const auto position = File::SeekDirectoryDescriptor(fd, offset, whence);
    return position ? *position : -1;
}

extern "C" int APS5_VABI ReadDirectoryDescriptorCallback_nid_no_patch(int fd, char* buf, int nbytes) {
    return File::ReadDirectoryDescriptor(fd, buf, nbytes);
}

extern "C" std::int64_t APS5_VABI SeekDirectoryDescriptorCallback_nid_no_patch(int fd, std::int64_t offset, int whence) {
    const auto position = File::SeekDirectoryDescriptor(fd, offset, whence);
    return position ? *position : -1;
}
