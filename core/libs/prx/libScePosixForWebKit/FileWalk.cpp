#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestDirectory.hpp"
#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr int FtwF = 0;
constexpr int FtwD = 1;
constexpr int FtwDnr = 2;
constexpr int FtwNs = 4;
constexpr int GuestEinval = 22;
constexpr int GuestEloop = 62;
constexpr std::uint16_t TypeMask = 0170000;
constexpr std::uint16_t TypeDirectory = 0040000;

using Callback = int (APS5_VABI *)(const char*, const void*, int);

struct Identity {
    std::uint32_t device;
    std::uint32_t inode;
};

struct Walk {
    Callback callback;
    std::vector<Identity> ancestors;
};

bool IsDirectory(const FileStat& status) {
    return (status.st_mode & TypeMask) == TypeDirectory;
}

int Invoke(Walk& walk, const std::string& path, const FileStat& status, int type) {
    return walk.callback(path.c_str(), &status, type);
}

std::string Join(const std::string& path, const std::string& name) {
    if (path.empty()) return name;
    if (path.back() == '/') return path + name;
    return path + "/" + name;
}

int Visit(Walk& walk, const std::string& path) {
    FileStat status{};
    if (stat_nid_postfix(path.c_str(), &status) != 0) return Invoke(walk, path, FileStat{}, FtwNs);
    if (!IsDirectory(status)) return Invoke(walk, path, status, FtwF);
    for (const Identity& ancestor : walk.ancestors)
        if (ancestor.device == status.st_dev && ancestor.inode == status.st_ino) {
            *__error_nid_postfix() = GuestEloop;
            return -1;
        }
    void* handle = opendir_nid_postfix(path.c_str());
    if (handle == nullptr) return Invoke(walk, path, status, FtwDnr);
    std::vector<std::string> names;
    while (GuestDirectoryEntry* entry = readdir_nid_postfix(handle)) names.emplace_back(entry->name, entry->nameLength);
    closedir_nid_postfix(handle);
    const int before = Invoke(walk, path, status, FtwD);
    if (before != 0) return before;
    walk.ancestors.push_back(Identity{status.st_dev, status.st_ino});
    int result = 0;
    for (const std::string& name : names) {
        if (name == "." || name == "..") continue;
        result = Visit(walk, Join(path, name));
        if (result != 0) break;
    }
    walk.ancestors.pop_back();
    return result;
}

}

extern "C" int APS5_VABI ftw_nid_postfix(const char* path, int (APS5_VABI *fn)(const char*, const void*, int), int nfds) {
    if (path == nullptr || fn == nullptr) throw std::invalid_argument("ftw: null path or function");
    if (nfds < 1) {
        *__error_nid_postfix() = GuestEinval;
        return -1;
    }
    std::string copy;
    if (const int error = CopyGuestPath_nid_no_patch(path, copy)) {
        *__error_nid_postfix() = error;
        return -1;
    }
    Walk walk{fn, {}};
    return Visit(walk, copy);
}
