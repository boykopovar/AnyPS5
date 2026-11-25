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
constexpr int FtwMount = 0x02;
constexpr int FtwDepth = 0x04;
constexpr int FtwChdir = 0x08;
constexpr int FtwF = 0;
constexpr int FtwD = 1;
constexpr int FtwDnr = 2;
constexpr int FtwDp = 3;
constexpr int FtwNs = 4;
constexpr int FtwSl = 5;
constexpr int FtwSln = 6;
constexpr int GuestEnoent = 2;
constexpr int GuestEinval = 22;
constexpr std::size_t GuestPathMax = 1024;
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
    bool mount;
    bool changeDirectory;
    bool stopped;
    std::uint32_t rootDevice;
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

int Visit(Walk& walk, const std::string& path, const std::string& access, const std::string& parent, int base, int level) {
    const bool follow = !walk.physical || level == 0;
    FileStat status{};
    const bool found = follow ? stat_nid_postfix(access.c_str(), &status) == 0 : lstat_nid_postfix(access.c_str(), &status) == 0;
    if (!found) {
        FileStat link{};
        if (follow && lstat_nid_postfix(access.c_str(), &link) == 0 && IsLink(link))
            return Invoke(walk, path, link, FtwSln, base, level);
        return Invoke(walk, path, FileStat{}, FtwNs, base, level);
    }
    if (level == 0) walk.rootDevice = status.st_dev;
    if (IsLink(status)) return Invoke(walk, path, status, FtwSl, base, level);
    if (!IsDirectory(status)) return Invoke(walk, path, status, FtwF, base, level);
    for (const Identity& ancestor : walk.ancestors)
        if (ancestor.device == status.st_dev && ancestor.inode == status.st_ino) return 0;
    if (walk.mount && status.st_dev != walk.rootDevice) return Invoke(walk, path, status, walk.depth ? FtwDp : FtwD, base, level);
    if (!walk.depth) {
        const int before = Invoke(walk, path, status, FtwD, base, level);
        if (before != 0) return before;
    }
    void* handle = OpenDirectory_nid_no_patch(access.c_str());
    if (handle == nullptr) return Invoke(walk, path, status, FtwDnr, base, level);
    std::vector<std::string> names;
    while (GuestDirectoryEntry* entry = ReadDirectory_nid_no_patch(handle)) names.emplace_back(entry->name, entry->nameLength);
    CloseDirectory_nid_no_patch(handle);
    if (walk.changeDirectory && chdir_nid_postfix(access.c_str()) != 0) {
        const int refused = *__error_nid_postfix();
        for (const std::string& name : names) {
            if (name == "." || name == "..") continue;
            const std::string child = Join(path, name);
            const int unread = Invoke(walk, child, FileStat{}, FtwNs, static_cast<int>(child.size() - name.size()), level + 1);
            if (unread != 0) return unread;
        }
        *__error_nid_postfix() = refused;
        return -1;
    }
    walk.ancestors.push_back(Identity{status.st_dev, status.st_ino});
    int result = 0;
    for (const std::string& name : names) {
        if (name == "." || name == "..") continue;
        const std::string child = Join(path, name);
        result = Visit(walk, child, Join(access, name), access, static_cast<int>(child.size() - name.size()), level + 1);
        if (result != 0 || walk.stopped) break;
    }
    walk.ancestors.pop_back();
    if (walk.changeDirectory && !walk.stopped && chdir_nid_postfix(parent.c_str()) != 0) walk.stopped = true;
    if (result == 0 && !walk.stopped && walk.depth) result = Invoke(walk, path, status, FtwDp, base, level);
    return result;
}

}

extern "C" int APS5_VABI nftw_nid_postfix(const char* path, int (APS5_VABI *fn)(const char*, const void*, int, void*), int nopenfd, int flags) {
    if (path == nullptr || fn == nullptr) throw std::invalid_argument("nftw: null path or function");
    if (nopenfd < 1) {
        *__error_nid_postfix() = GuestEinval;
        return -1;
    }
    if (flags & ~(FtwPhys | FtwMount | FtwDepth | FtwChdir)) NotImplemented_nid_no_patch(__func__);
    std::string copy;
    if (const int error = CopyGuestPath_nid_no_patch(path, copy)) {
        *__error_nid_postfix() = error;
        return -1;
    }
    if (copy.empty()) {
        *__error_nid_postfix() = GuestEnoent;
        return -1;
    }
    Walk walk{fn, (flags & FtwPhys) != 0, (flags & FtwDepth) != 0, (flags & FtwMount) != 0, false, false, 0, {}};
    std::string start;
    if ((flags & FtwChdir) != 0 && walk.physical) {
        char current[GuestPathMax];
        if (getcwd_nid_postfix(current, sizeof(current)) != nullptr) {
            start = current;
            walk.changeDirectory = true;
        }
    }
    const std::string access = walk.changeDirectory && copy.front() != '/' ? Join(start, copy) : copy;
    int result = Visit(walk, copy, access, start, BasenameOffset(copy), 0);
    if (walk.changeDirectory) {
        const int saved = *__error_nid_postfix();
        if (chdir_nid_postfix(start.c_str()) != 0) {
            if (result == 0) result = -1;
        } else {
            *__error_nid_postfix() = saved;
        }
    }
    return result;
}
