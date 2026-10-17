#include "prx/libkernel/File/include/DirectoryDescriptor.hpp"
#include "prx/libkernel/File/include/File.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"

#include <cerrno>
#include <string>

#ifdef _WIN32

#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <cstdint>
#include <atomic>
#include <cstring>
#include <limits>
#include <fcntl.h>
#include <io.h>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr std::uint8_t GuestDirectoryType = 4;
constexpr std::uint8_t GuestRegularType = 8;

struct DirectoryState {
    std::mutex mutex;
    std::filesystem::path path;
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

void Load(DirectoryState& state) {
    if (state.loaded) return;
    state.loaded = true;
    state.entries.emplace_back(".", GuestDirectoryType);
    state.entries.emplace_back("..", GuestDirectoryType);
    std::error_code error;
    for (const auto& entry : std::filesystem::directory_iterator(state.path, error)) {
        std::error_code typeError;
        state.entries.emplace_back(entry.path().filename().string(), entry.is_directory(typeError) ? GuestDirectoryType : GuestRegularType);
    }
}

}

namespace File {

int OpenDirectoryDescriptor(const std::filesystem::path& path) {
    const int fd = ::_open("NUL", _O_RDONLY | _O_BINARY);
    if (fd < 0) return -1;
    std::lock_guard lock(g_mutex);
    const auto state = std::make_shared<DirectoryState>();
    state->path = path;
    g_directories[fd] = state;
    g_directoryCount = g_directories.size();
    return fd;
}

std::optional<std::filesystem::path> DirectoryDescriptorPath(int fd) {
    const auto state = Find(fd);
    if (!state) return std::nullopt;
    return state->path;
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
    if (whence == SeekEnd) Load(state);
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
    Load(state);
    state.cursor = static_cast<std::size_t>(target);
    return target;
}

int ReadDirectoryDescriptor(int fd, char* buf, int nbytes) {
    const auto found = Find(fd);
    if (!found) return SCE_KERNEL_ERROR_ENOTDIR;
    auto& state = *found;
    std::lock_guard lock(state.mutex);
    Load(state);
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
        *error = ENOTDIR;
        return nullptr;
    }
    struct PathTag {};
    auto& path = HostThreadLocal<std::string, PathTag>();
    const auto utf8Path = directory->u8string();
    path.assign(reinterpret_cast<const char*>(utf8Path.data()), utf8Path.size());
    *error = 0;
    return path.c_str();
}
