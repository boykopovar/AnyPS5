#include "prx/libkernel/File/include/DirectoryDescriptor.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"

#include <cerrno>
#include <string>

#ifdef _WIN32

#include "prx/libc/include/FileStream.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/File/include/NativeStat.hpp"

#include <cerrno>
#include <cstdint>
#include <atomic>
#include <cstring>
#include <limits>
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <system_error>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr std::uint8_t GuestUnknownType = 0;
constexpr std::uint8_t GuestDirectoryType = 4;
constexpr std::uint8_t GuestRegularType = 8;
constexpr std::uint8_t GuestSymbolicLinkType = 10;

struct DirectoryState {
    std::mutex mutex;
    std::vector<std::pair<std::string, std::uint8_t>> entries;
    std::size_t cursor = 0;
    bool loaded = false;
};

std::mutex g_mutex;
std::map<int, std::shared_ptr<DirectoryState>> g_directories;
std::atomic<std::size_t> g_directoryCount{0};

std::shared_ptr<DirectoryState> Find(int fd) {
    std::lock_guard lock(g_mutex);
    const auto found = g_directories.find(fd);
    return found == g_directories.end() ? nullptr : found->second;
}

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

void Load(DirectoryState& state, int fd) {
    if (state.loaded) return;
    const auto path = File::WindowsDescriptorPath_nid_no_patch(fd);
    if (!path) throw std::system_error(errno, std::generic_category(), "directory descriptor path");
    const int savedErrno = errno;
    const bool followLinks = InsideApp0(*path);
    errno = savedErrno;
    std::vector<std::pair<std::string, std::uint8_t>> entries{{".", GuestDirectoryType}, {"..", GuestDirectoryType}};
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(*path, error)) {
        entries.emplace_back(entry.path().filename().string(), EntryType(entry.path(), followLinks));
    }
    state.entries = std::move(entries);
    state.loaded = true;
}

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

}

namespace File {

int OpenDirectoryDescriptor(const std::filesystem::path& path) {
    const auto state = std::make_shared<DirectoryState>();
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    const auto handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        &security, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        errno = WindowsOpenError_nid_no_patch(GetLastError());
        return -1;
    }
    const int fd = ::_open_osfhandle(reinterpret_cast<std::intptr_t>(handle), _O_RDONLY | _O_BINARY);
    if (fd < 0) {
        CloseHandle(handle);
        return -1;
    }
    try {
        std::lock_guard lock(g_mutex);
        g_directories[fd] = state;
        g_directoryCount = g_directories.size();
    } catch (...) {
        ::_close(fd);
        throw;
    }
    return fd;
}

bool IsDirectoryDescriptor(int fd) {
    return g_directoryCount != 0 && Find(fd) != nullptr;
}

std::optional<std::filesystem::path> DirectoryDescriptorPath(int fd) {
    if (!IsDirectoryDescriptor(fd)) return std::nullopt;
    return WindowsDescriptorPath_nid_no_patch(fd);
}

void ForgetDirectoryDescriptor(int fd) {
    if (g_directoryCount == 0) return;
    std::lock_guard lock(g_mutex);
    g_directories.erase(fd);
    g_directoryCount = g_directories.size();
}

void DuplicateDirectoryDescriptor(int from, int to) {
    if (g_directoryCount == 0) return;
    std::lock_guard lock(g_mutex);
    const auto found = g_directories.find(from);
    if (found == g_directories.end()) return;
    g_directories[to] = found->second;
    g_directoryCount = g_directories.size();
}

std::optional<std::int64_t> SeekDirectoryDescriptor(int fd, std::int64_t offset, int whence) {
    constexpr int SeekSet = 0;
    constexpr int SeekCurrent = 1;
    constexpr int SeekEnd = 2;
    if (g_directoryCount == 0) return std::nullopt;
    const auto found = Find(fd);
    if (!found) return std::nullopt;
    auto& state = *found;
    std::lock_guard lock(state.mutex);
    if (whence != SeekSet && whence != SeekCurrent && whence != SeekEnd) {
        errno = EINVAL;
        return std::int64_t{-1};
    }
    if (whence == SeekEnd) Load(state, fd);
    const auto cursor = static_cast<std::int64_t>(whence == SeekEnd ? state.entries.size() : state.cursor);
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
        state.entries.clear();
        state.loaded = false;
        state.cursor = 0;
        return std::int64_t{0};
    }
    Load(state, fd);
    state.cursor = static_cast<std::size_t>(target);
    return target;
}

int ReadDirectoryDescriptor(int fd, char* buf, int nbytes, std::int64_t* base) {
    const auto found = Find(fd);
    if (!found) return SCE_KERNEL_ERROR_ENOTDIR;
    auto& state = *found;
    std::lock_guard lock(state.mutex);
    Load(state, fd);
    if (base != nullptr) *base = static_cast<std::int64_t>(state.cursor);
    std::size_t used = 0;
    while (state.cursor < state.entries.size()) {
        const auto& [name, type] = state.entries[state.cursor];
        if (name.size() > 255) {
            throw std::runtime_error("Directory entry name exceeds the guest 255-byte limit");
        }
        const std::size_t record = (8 + name.size() + 1 + 3) & ~std::size_t{3};
        if (used + record > static_cast<std::size_t>(nbytes)) {
            if (used == 0) return SCE_KERNEL_ERROR_EINVAL;
            break;
        }
        char* out = buf + used;
        std::memset(out, 0, record);
        const auto fileNumber = static_cast<std::uint32_t>(state.cursor + 1);
        const auto recordLength = static_cast<std::uint16_t>(record);
        std::memcpy(out, &fileNumber, sizeof(fileNumber));
        std::memcpy(out + 4, &recordLength, sizeof(recordLength));
        out[6] = static_cast<char>(type);
        out[7] = static_cast<char>(name.size());
        std::memcpy(out + 8, name.c_str(), name.size() + 1);
        used += record;
        ++state.cursor;
    }
    return static_cast<int>(used);
}

}

#else

namespace File {

std::optional<std::filesystem::path> DirectoryDescriptorPath(int fd) {
    if (fd < 0) return std::nullopt;
    std::error_code error;
    const auto link = std::filesystem::path("/proc/self/fd") / std::to_string(fd);
    const auto target = std::filesystem::read_symlink(link, error);
    if (error) return std::nullopt;
    if (!std::filesystem::is_directory(target, error)) return std::nullopt;
    return target;
}

}

#endif

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
#ifdef _WIN32
    const auto position = File::SeekDirectoryDescriptor(fd, 0, 1);
    return position && *position > 0 ? static_cast<std::uint64_t>(*position) : 0;
#else
    (void)fd;
    return 0;
#endif
}
