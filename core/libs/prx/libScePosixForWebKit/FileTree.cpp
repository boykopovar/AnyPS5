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

constexpr int FtwPhys = 0x01;
constexpr int FtwDepth = 0x04;
constexpr int FtwF = 0;
constexpr int FtwD = 1;
constexpr int FtwDnr = 2;
constexpr int FtwDp = 3;
constexpr int FtwNs = 4;
constexpr int FtwSl = 5;
constexpr int FtwSln = 6;
constexpr int GuestEinval = 22;
constexpr std::uint16_t TypeMask = 0170000;
constexpr std::uint16_t TypeDirectory = 0040000;
constexpr std::uint16_t TypeLink = 0120000;

struct GuestFtw {
    int base;
    int level;
};

using Callback = int (APS5_VABI *)(const char*, const void*, int, void*);

struct Identity {
    std::uint32_t device;
    std::uint32_t inode;
};

struct Walk {
    Callback callback;
    bool physical;
    bool depth;
    std::vector<Identity> ancestors;
};

bool IsLink(const FileStat& status) {
    return (status.st_mode & TypeMask) == TypeLink;
}

bool IsDirectory(const FileStat& status) {
    return (status.st_mode & TypeMask) == TypeDirectory;
}

int Invoke(Walk& walk, const std::string& path, const FileStat& status, int type, int base, int level) {
    GuestFtw info{base, level};
    return walk.callback(path.c_str(), &status, type, &info);
}

std::string Join(const std::string& path, const std::string& name) {
    if (path.empty()) return name;
    if (path.back() == '/') return path + name;
    return path + "/" + name;
}

int BasenameOffset(const std::string& path) {
    const auto slash = path.rfind('/');
    return slash == std::string::npos ? 0 : static_cast<int>(slash + 1);
}

int Visit(Walk& walk, const std::string& path, int base, int level) {
    FileStat status{};
    const bool found = walk.physical ? lstat_nid_postfix(path.c_str(), &status) == 0 : stat_nid_postfix(path.c_str(), &status) == 0;
    if (!found) {
        FileStat link{};
        if (!walk.physical && lstat_nid_postfix(path.c_str(), &link) == 0 && IsLink(link))
            return Invoke(walk, path, link, FtwSln, base, level);
        return Invoke(walk, path, FileStat{}, FtwNs, base, level);
    }
    if (IsLink(status)) return Invoke(walk, path, status, FtwSl, base, level);
    if (!IsDirectory(status)) return Invoke(walk, path, status, FtwF, base, level);
    for (const Identity& ancestor : walk.ancestors)
        if (ancestor.device == status.st_dev && ancestor.inode == status.st_ino) return 0;
    if (!walk.depth) {
        const int before = Invoke(walk, path, status, FtwD, base, level);
        if (before != 0) return before;
    }
    void* handle = opendir_nid_postfix(path.c_str());
    if (handle == nullptr) return Invoke(walk, path, status, FtwDnr, base, level);
    std::vector<std::string> names;
    while (GuestDirectoryEntry* entry = readdir_nid_postfix(handle)) names.emplace_back(entry->name, entry->nameLength);
    closedir_nid_postfix(handle);
    walk.ancestors.push_back(Identity{status.st_dev, status.st_ino});
    int result = 0;
    for (const std::string& name : names) {
        if (name == "." || name == "..") continue;
        const std::string child = Join(path, name);
        result = Visit(walk, child, static_cast<int>(child.size() - name.size()), level + 1);
        if (result != 0) break;
    }
    walk.ancestors.pop_back();
    if (result == 0 && walk.depth) result = Invoke(walk, path, status, FtwDp, base, level);
    return result;
}

}

extern "C" int APS5_VABI nftw_nid_postfix(const char* path, int (APS5_VABI *fn)(const char*, const void*, int, void*), int nopenfd, int flags) {
    if (path == nullptr || fn == nullptr) throw std::invalid_argument("nftw: null path or function");
    if (nopenfd < 1) {
        *__error_nid_postfix() = GuestEinval;
        return -1;
    }
    if (flags & ~(FtwPhys | FtwDepth)) NotImplemented_nid_no_patch(__func__);
    std::string copy;
    if (const int error = CopyGuestPath_nid_no_patch(path, copy)) {
        *__error_nid_postfix() = error;
        return -1;
    }
    Walk walk{fn, (flags & FtwPhys) != 0, (flags & FtwDepth) != 0, {}};
    return Visit(walk, copy, BasenameOffset(copy), 0);
}
