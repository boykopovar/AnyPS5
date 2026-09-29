#include "prx/libkernel/File/include/DirectoryDescriptor.hpp"

#ifdef _WIN32

#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <io.h>
#include <map>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr std::uint8_t GuestDirectoryType = 4;
constexpr std::uint8_t GuestRegularType = 8;

struct DirectoryState {
    std::filesystem::path path;
    std::vector<std::pair<std::string, std::uint8_t>> entries;
    std::size_t cursor = 0;
    bool loaded = false;
};

std::mutex g_mutex;
std::map<int, DirectoryState> g_directories;

}

namespace File {

int OpenDirectoryDescriptor(const std::filesystem::path& path) {
    const int fd = ::_open("NUL", _O_RDONLY | _O_BINARY);
    if (fd < 0) return -1;
    std::lock_guard lock(g_mutex);
    g_directories[fd] = DirectoryState{path};
    return fd;
}

std::optional<std::filesystem::path> DirectoryDescriptorPath(int fd) {
    std::lock_guard lock(g_mutex);
    const auto found = g_directories.find(fd);
    if (found == g_directories.end()) return std::nullopt;
    return found->second.path;
}

void ForgetDirectoryDescriptor(int fd) {
    std::lock_guard lock(g_mutex);
    g_directories.erase(fd);
}

int ReadDirectoryDescriptor(int fd, char* buf, int nbytes) {
    std::lock_guard lock(g_mutex);
    const auto found = g_directories.find(fd);
    if (found == g_directories.end()) return -1;
    auto& state = found->second;
    if (!state.loaded) {
        state.loaded = true;
        state.entries.emplace_back(".", GuestDirectoryType);
        state.entries.emplace_back("..", GuestDirectoryType);
        std::error_code error;
        for (const auto& entry : std::filesystem::directory_iterator(state.path, error)) {
            std::error_code typeError;
            state.entries.emplace_back(entry.path().filename().string(), entry.is_directory(typeError) ? GuestDirectoryType : GuestRegularType);
        }
    }
    std::size_t used = 0;
    while (state.cursor < state.entries.size()) {
        const auto& [name, type] = state.entries[state.cursor];
        const std::size_t record = (8 + name.size() + 1 + 3) & ~std::size_t{3};
        if (used + record > static_cast<std::size_t>(nbytes)) break;
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
