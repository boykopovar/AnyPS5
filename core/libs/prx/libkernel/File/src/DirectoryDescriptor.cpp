#include "prx/libkernel/File/include/DirectoryDescriptor.hpp"
#include "prx/libkernel/KernelErrors.hpp"

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
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr std::uint8_t GuestDirectoryType = 4;
constexpr std::uint8_t GuestRegularType = 8;
constexpr int GuestEoverflow = 84;

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

std::optional<std::int64_t> SeekDirectoryDescriptor(int fd, std::int64_t offset, int whence) {
    constexpr int SeekSet = 0;
    constexpr int SeekCurrent = 1;
    if (g_directoryCount == 0) return std::nullopt;
    const auto found = Find(fd);
    if (!found) return std::nullopt;
    auto& state = *found;
    std::lock_guard lock(state.mutex);
    if (whence != SeekSet && whence != SeekCurrent) NotImplemented_nid_no_patch("lseek from the end of a directory descriptor");
    const auto cursor = static_cast<std::int64_t>(state.cursor);
    if (whence == SeekCurrent && offset == 0) return cursor;
    if (whence == SeekCurrent && offset > std::numeric_limits<std::int64_t>::max() - cursor) {
        errno = GuestEoverflow;
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

#endif
